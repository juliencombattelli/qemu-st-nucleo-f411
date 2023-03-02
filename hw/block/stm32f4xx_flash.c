/*
 * STM32F4xx Flash memory interface
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
#include "hw/block/stm32f4xx_flash.h"

static void stm32f4xx_flash_reset(DeviceState *dev)
{
    STM32F4xxFlashState *s = STM32F4XX_FLASH(dev);

    s->flash_acr = 0x00000000;
    s->flash_keyr = 0x00000000;
    s->flash_optkeyr = 0x00000000;
    s->flash_sr = 0x00000000;
    s->flash_cr = 0x80000000;
    s->flash_optcr = 0x0FFFAAED;
    s->flash_optcr1 = 0x0FFF0000;
}

static void stm32f4xx_flash_set_irq(void *opaque, int irq, int level)
{
    STM32F4xxFlashState *s = opaque;

    trace_stm32f4xx_flash_set_irq(irq, level);

    // if (level)
    // {
    //     const uint32_t irq_enable_bit = BIT(irq + RCC_CIR_ENABLE_BIT_OFFSET);
    //     if ((irq == RCC_IRQ_CSS) || (s->rcc_cir & irq_enable_bit))
    //     {
    //         s->rcc_cir |= irq;
    //     }
    // }

    qemu_irq_pulse(s->irq);
}

static uint64_t stm32f4xx_flash_read(void *opaque, hwaddr addr,
                                   unsigned int size)
{
    STM32F4xxFlashState *s = opaque;

    trace_stm32f4xx_flash_read(addr);

    switch (addr)
    {
    case FLASH_ACR:
        return s->flash_acr;
    case FLASH_KEYR:
        return s->flash_keyr;
    case FLASH_OPTKEYR:
        return s->flash_optkeyr;
    case FLASH_SR:
        return s->flash_sr;
    case FLASH_CR:
        return s->flash_sr;
    case FLASH_OPTCR:
        return s->flash_optcr;
    case FLASH_OPTCR1:
        return s->flash_optcr1;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "STM32F4XX_flash_read: Bad offset %x\n", (int)addr);
    }
    return 0;
}

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

/**
 * Perform additional operations on some bits depending on the value of other bits of the register value
 */
// static uint32_t handle_rcc_cr_write(uint32_t rcc_cr)
// {
//     rcc_cr = set_or_clear_if(rcc_cr, BIT(1), rcc_cr & BIT(0)); /* Set or clear HSIRDY depending on HSION */
//     rcc_cr = set_or_clear_if(rcc_cr, BIT(17), rcc_cr & BIT(16)); /* Set or clear HSERDY depending on HSEON */
//     rcc_cr = set_or_clear_if(rcc_cr, BIT(25), rcc_cr & BIT(24)); /* Set or clear PLLRDY depending on PLLON */
//     rcc_cr = set_or_clear_if(rcc_cr, BIT(27), rcc_cr & BIT(26)); /* Set or clear PLLI2SRDY depending on PLLI2SON */
//     return rcc_cr;
// }

static void stm32f4xx_flash_write(void *opaque, hwaddr addr,
                                uint64_t val64, unsigned int size)
{
    STM32F4xxFlashState *s = opaque;
    uint32_t value = (uint32_t)val64;

    trace_stm32f4xx_flash_write(addr, value);

    switch (addr)
    {
    case FLASH_ACR:
        s->flash_acr = value;
        return;
    case FLASH_KEYR:
        s->flash_keyr = value;
        return;
    case FLASH_OPTKEYR:
        s->flash_optkeyr = value;
        return;
    case FLASH_SR:
        s->flash_sr = value;
        return;
    case FLASH_CR:
        s->flash_cr = value;
        return;
    case FLASH_OPTCR:
        s->flash_optcr = value;
        return;
    case FLASH_OPTCR1:
        s->flash_optcr1 = value;
        return;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "STM32F4XX_flash_write: Bad offset %x\n", (int)addr);
    }
}

static const MemoryRegionOps stm32f4xx_flash_ops = {
    .read = stm32f4xx_flash_read,
    .write = stm32f4xx_flash_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void stm32f4xx_flash_init(Object *obj)
{
    STM32F4xxFlashState *s = STM32F4XX_FLASH(obj);

    sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->irq);

    memory_region_init_io(&s->mmio, obj, &stm32f4xx_flash_ops, s,
                          TYPE_STM32F4XX_FLASH, 0x400);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->mmio);

    qdev_init_gpio_in(DEVICE(obj), stm32f4xx_flash_set_irq, 1);
}

static const VMStateDescription vmstate_stm32f4xx_flash = {
    .name = TYPE_STM32F4XX_FLASH,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){
        VMSTATE_UINT32(flash_acr, STM32F4xxFlashState),
        VMSTATE_UINT32(flash_keyr, STM32F4xxFlashState),
        VMSTATE_UINT32(flash_optkeyr, STM32F4xxFlashState),
        VMSTATE_UINT32(flash_sr, STM32F4xxFlashState),
        VMSTATE_UINT32(flash_cr, STM32F4xxFlashState),
        VMSTATE_UINT32(flash_optcr, STM32F4xxFlashState),
        VMSTATE_UINT32(flash_optcr1, STM32F4xxFlashState),
        VMSTATE_END_OF_LIST()}};

static void stm32f4xx_flash_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = stm32f4xx_flash_reset;
    dc->vmsd = &vmstate_stm32f4xx_flash;
}

static const TypeInfo stm32f4xx_flash_info = {
    .name = TYPE_STM32F4XX_FLASH,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(STM32F4xxFlashState),
    .instance_init = stm32f4xx_flash_init,
    .class_init = stm32f4xx_flash_class_init,
};

static void stm32f4xx_flash_register_types(void)
{
    type_register_static(&stm32f4xx_flash_info);
}

type_init(stm32f4xx_flash_register_types)
