/*
 * STM32F4XX RCC
 *
 * Copyright (c) 2023 Julien Combattelli <julien.combattelli@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "trace.h"
#include "hw/irq.h"
#include "migration/vmstate.h"
#include "hw/misc/stm32f4xx_rcc.h"

typedef enum
{
    RCC_IRQ_LSI_READY,    /* LSI ready interrupt */
    RCC_IRQ_LSE_READY,    /* LSE ready interrupt */
    RCC_IRQ_HSI_READY,    /* HSI ready interrupt */
    RCC_IRQ_HSE_READY,    /* HSE ready interrupt */
    RCC_IRQ_PLL_READY,    /* Main PLL(PLL) ready interrupt */
    RCC_IRQ_PLLI2S_READY, /* PLLI2S ready interrupt */
    RCC_IRQ_CSS,          /* Clock security system interrupt (always enabled) */
    RCC_IRQ_COUNT
} RccIrqEvents;

#define RCC_CIR_ENABLE_BIT_OFFSET 7
#define RCC_CIR_CLEAR_BIT_OFFSET 16

static void stm32f4xx_rcc_reset(DeviceState *dev)
{
    STM32F4xxRccState *s = STM32F4XX_RCC(dev);

    /* Leave reserved registers uninitialized */

    s->rcc_cr = 0x0000FF81; // bits[15:8] are HSI calibration value, TODO use real on-board value
    s->rcc_pllcfgr = 0x24003010;
    s->rcc_cfgr = 0x00000000;
    s->rcc_cir = 0x00000000;
    s->rcc_ahb1rstr = 0x00000000;
    s->rcc_ahb2rstr = 0x00000000;
    s->rcc_apb1rstr = 0x00000000;
    s->rcc_apb2rstr = 0x00000000;
    s->rcc_ahb1enr = 0x00000000;
    s->rcc_ahb2enr = 0x00000000;
    s->rcc_apb1enr = 0x00000000;
    s->rcc_apb2enr = 0x00000000;
    s->rcc_ahb1lpenr = 0x0061900F;
    s->rcc_ahb2lpenr = 0x00000080;
    s->rcc_apb1lpenr = 0x10E2C80F;
    s->rcc_apb2lpenr = 0x00077930;
    s->rcc_bdcr = 0x00000000;
    s->rcc_csr = 0x0E000000;
    s->rcc_sscgr = 0x00000000;
    s->rcc_plli2scfgr = 0x24003000;
    s->rcc_dckcfgr = 0x00000000;
}

static void stm32f4xx_rcc_set_irq(void *opaque, int irq, int level)
{
    STM32F4xxRccState *s = opaque;

    trace_stm32f4xx_rcc_set_irq(irq, level);

    g_assert(irq < RCC_IRQ_COUNT);

    if (level)
    {
        const uint32_t irq_enable_bit = BIT(irq + RCC_CIR_ENABLE_BIT_OFFSET);
        if ((irq == RCC_IRQ_CSS) || (s->rcc_cir & irq_enable_bit))
        {
            s->rcc_cir |= irq;
        }
    }

    qemu_irq_pulse(s->irq);
}

static uint64_t stm32f4xx_rcc_read(void *opaque, hwaddr addr,
                                   unsigned int size)
{
    STM32F4xxRccState *s = opaque;

    trace_stm32f4xx_rcc_read(addr);

    switch (addr)
    {
    case RCC_CR:
        return s->rcc_cr;
    case RCC_PLLCFGR:
        return s->rcc_pllcfgr;
    case RCC_CFGR:
        return s->rcc_cfgr;
    case RCC_CIR:
        return s->rcc_cir;
    case RCC_AHB1RSTR:
        return s->rcc_ahb1rstr;
    case RCC_AHB2RSTR:
        return s->rcc_ahb2rstr;
    case RCC_APB1RSTR:
        return s->rcc_apb1rstr;
    case RCC_APB2RSTR:
        return s->rcc_apb2rstr;
    case RCC_AHB1ENR:
        return s->rcc_ahb1enr;
    case RCC_AHB2ENR:
        return s->rcc_ahb2enr;
    case RCC_APB1ENR:
        return s->rcc_apb1enr;
    case RCC_APB2ENR:
        return s->rcc_apb2enr;
    case RCC_AHB1LPENR:
        return s->rcc_ahb1lpenr;
    case RCC_AHB2LPENR:
        return s->rcc_ahb2lpenr;
    case RCC_APB1LPENR:
        return s->rcc_apb1lpenr;
    case RCC_APB2LPENR:
        return s->rcc_apb2lpenr;
    case RCC_BDCR:
        return s->rcc_bdcr;
    case RCC_CSR:
        return s->rcc_csr;
    case RCC_SSCGR:
        return s->rcc_sscgr;
    case RCC_PLLI2SCFGR:
        return s->rcc_plli2scfgr;
    case RCC_DCKCFGR:
        return s->rcc_dckcfgr;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "STM32F4XX_rcc_read: Bad offset %x\n", (int)addr);
    }
    return 0;
}

/**
 * Set value with bits from mask if condition is true, clear otherwise.
 */
static inline uint32_t set_or_clear_if(uint32_t value, uint32_t mask, uint32_t condition)
{
    if (condition != 0)
    {
        value |= mask;
    }
    else
    {
        value &= ~mask;
    }
    return value;
}

static uint32_t handle_rcc_cr_write(uint32_t rcc_cr)
{
    rcc_cr = set_or_clear_if(rcc_cr, BIT(1), rcc_cr & BIT(0)); /* Set or clear HSIRDY depending on HSION */
    rcc_cr = set_or_clear_if(rcc_cr, BIT(17), rcc_cr & BIT(16)); /* Set or clear HSERDY depending on HSEON */
    rcc_cr = set_or_clear_if(rcc_cr, BIT(25), rcc_cr & BIT(24)); /* Set or clear PLLRDY depending on PLLON */
    rcc_cr = set_or_clear_if(rcc_cr, BIT(27), rcc_cr & BIT(26)); /* Set or clear PLLI2SRDY depending on PLLI2SON */
    return rcc_cr;
}

static uint32_t handle_rcc_cfgr_write(uint32_t rcc_cfgr)
{
    // Update the clock status (bits[3:2]) with the selected clock (bits[1:0])
    const uint32_t sysclk_switch_status_offset = 2;
    const uint32_t sysclk_switch_status_bits = 0x3 << sysclk_switch_status_offset;
    const uint32_t sysclk_switch_bits = 0x3;
    const uint32_t sysclk_switch = rcc_cfgr & sysclk_switch_bits;
    return (rcc_cfgr & ~sysclk_switch_status_bits) | (sysclk_switch << sysclk_switch_status_offset);
}

static void stm32f4xx_rcc_write(void *opaque, hwaddr addr,
                                uint64_t val64, unsigned int size)
{
    STM32F4xxRccState *s = opaque;
    uint32_t value = (uint32_t)val64;

    trace_stm32f4xx_rcc_write(addr, value);

    switch (addr)
    {
    case RCC_CR:
        s->rcc_cr = handle_rcc_cr_write(value);
        return;
    case RCC_PLLCFGR:
        s->rcc_pllcfgr = value;
        return;
    case RCC_CFGR:
        s->rcc_cfgr = handle_rcc_cfgr_write(value);
        return;
    case RCC_CIR:
        s->rcc_cir = value; // TODO handle reset of flag  bits when clear bits are set
        return;
    case RCC_AHB1RSTR:
        s->rcc_ahb1rstr = value;
        return;
    case RCC_AHB2RSTR:
        s->rcc_ahb2rstr = value;
        return;
    case RCC_APB1RSTR:
        s->rcc_apb1rstr = value;
        return;
    case RCC_APB2RSTR:
        s->rcc_apb2rstr = value;
        return;
    case RCC_AHB1ENR:
        s->rcc_ahb1enr = value;
        return;
    case RCC_AHB2ENR:
        s->rcc_ahb2enr = value;
        return;
    case RCC_APB1ENR:
        s->rcc_apb1enr = value;
        return;
    case RCC_APB2ENR:
        s->rcc_apb2enr = value;
        return;
    case RCC_AHB1LPENR:
        s->rcc_ahb1lpenr = value;
        return;
    case RCC_AHB2LPENR:
        s->rcc_ahb2lpenr = value;
        return;
    case RCC_APB1LPENR:
        s->rcc_apb1lpenr = value;
        return;
    case RCC_APB2LPENR:
        s->rcc_apb2lpenr = value;
        return;
    case RCC_BDCR:
        s->rcc_bdcr = value;
        return;
    case RCC_CSR:
        s->rcc_csr = value;
        return;
    case RCC_SSCGR:
        s->rcc_sscgr = value;
        return;
    case RCC_PLLI2SCFGR:
        s->rcc_plli2scfgr = value;
        return;
    case RCC_DCKCFGR:
        s->rcc_dckcfgr = value;
        return;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "STM32F4XX_rcc_write: Bad offset %x\n", (int)addr);
    }
}

static const MemoryRegionOps stm32f4xx_rcc_ops = {
    .read = stm32f4xx_rcc_read,
    .write = stm32f4xx_rcc_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void stm32f4xx_rcc_init(Object *obj)
{
    STM32F4xxRccState *s = STM32F4XX_RCC(obj);

    sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->irq);

    memory_region_init_io(&s->mmio, obj, &stm32f4xx_rcc_ops, s,
                          TYPE_STM32F4XX_RCC, 0x400);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->mmio);

    qdev_init_gpio_in(DEVICE(obj), stm32f4xx_rcc_set_irq,
                      RCC_IRQ_COUNT);
}

static const VMStateDescription vmstate_stm32f4xx_rcc = {
    .name = TYPE_STM32F4XX_RCC,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){
        VMSTATE_UINT32(rcc_cr, STM32F4xxRccState),
        VMSTATE_UINT32(rcc_pllcfgr, STM32F4xxRccState),
        VMSTATE_UINT32(rcc_cfgr, STM32F4xxRccState),
        VMSTATE_UINT32(rcc_cir, STM32F4xxRccState),
        VMSTATE_UINT32(rcc_ahb1rstr, STM32F4xxRccState),
        VMSTATE_UINT32(rcc_ahb2rstr, STM32F4xxRccState),
        VMSTATE_UINT32(rcc_apb1rstr, STM32F4xxRccState),
        VMSTATE_UINT32(rcc_apb2rstr, STM32F4xxRccState),
        VMSTATE_UINT32(rcc_ahb1enr, STM32F4xxRccState),
        VMSTATE_UINT32(rcc_ahb2enr, STM32F4xxRccState),
        VMSTATE_UINT32(rcc_apb1enr, STM32F4xxRccState),
        VMSTATE_UINT32(rcc_apb2enr, STM32F4xxRccState),
        VMSTATE_UINT32(rcc_ahb1lpenr, STM32F4xxRccState),
        VMSTATE_UINT32(rcc_ahb2lpenr, STM32F4xxRccState),
        VMSTATE_UINT32(rcc_apb1lpenr, STM32F4xxRccState),
        VMSTATE_UINT32(rcc_apb2lpenr, STM32F4xxRccState),
        VMSTATE_UINT32(rcc_bdcr, STM32F4xxRccState),
        VMSTATE_UINT32(rcc_csr, STM32F4xxRccState),
        VMSTATE_UINT32(rcc_sscgr, STM32F4xxRccState),
        VMSTATE_UINT32(rcc_plli2scfgr, STM32F4xxRccState),
        VMSTATE_UINT32(rcc_dckcfgr, STM32F4xxRccState),
        VMSTATE_END_OF_LIST()}};

static void stm32f4xx_rcc_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = stm32f4xx_rcc_reset;
    dc->vmsd = &vmstate_stm32f4xx_rcc;
}

static const TypeInfo stm32f4xx_rcc_info = {
    .name = TYPE_STM32F4XX_RCC,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(STM32F4xxRccState),
    .instance_init = stm32f4xx_rcc_init,
    .class_init = stm32f4xx_rcc_class_init,
};

static void stm32f4xx_rcc_register_types(void)
{
    type_register_static(&stm32f4xx_rcc_info);
}

type_init(stm32f4xx_rcc_register_types)
