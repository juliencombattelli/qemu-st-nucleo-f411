/*
 * STM32F411 SoC
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

/* Based on stm32f405_soc.c */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "exec/address-spaces.h"
#include "sysemu/sysemu.h"
#include "hw/arm/stm32f411_soc.h"
#include "hw/qdev-clock.h"
#include "hw/misc/unimp.h"

#define RCC_ADDR 0x40023800
#define SYSCFG_ADDR 0x40013800
#define FLASH_R_ADDR 0x40023C00
static const uint32_t usart_addr[] = {
    0x40011000, // USART1
    0x40004400, // USART2
    0x40011400, // USART6
};
/* At the moment only Timer 2 to 5 are modelled */
static const uint32_t timer_addr[] = {
    0x40000000, // TIM2
    0x40000400, // TIM3
    0x40000800, // TIM4
    0x40000C00, // TIM5
    // TIM1,9,10,11 are not supported for now
};
static const uint32_t adc_addr[] = {
    0x40012000, // ADC1
};
static const uint32_t spi_addr[] = {
    0x40003800, // SPI2
    0x40003C00, // SPI3
    0x40013000, // SPI1
    0x40013400, // SPI4
    0x40015000, // SPI5
};
#define EXTI_ADDR 0x40013C00

#define RCC_IRQ 5
#define SYSCFG_IRQ 71
#define FLASH_R_IRQ 4
static const int usart_irq[] = {
    37, // USART1
    38, // USART2
    71, // USART6
};
static const int timer_irq[] = {
    28, // TIM2
    29, // TIM3
    30, // TIM4
    50, // TIM5
};
#define ADC_IRQ 18
static const int spi_irq[] = {
    35, // SPI1
    36, // SPI2
    51, // SPI3
    84, // SPI4
    85, // SPI5
};
static const int exti_irq[] = {
    6,  // EXTI0
    7,  // EXTI1
    8,  // EXTI2
    9,  // EXTI3
    10, // EXTI4
    23, // EXTI9_5
    23, // EXTI9_5
    23, // EXTI9_5
    23, // EXTI9_5
    23, // EXTI9_5
    40, // EXTI15_10
    40, // EXTI15_10
    40, // EXTI15_10
    40, // EXTI15_10
    40, // EXTI15_10
    40, // EXTI15_10
};

static void stm32f411_soc_initfn(Object *obj)
{
    STM32F411State *s = STM32F411_SOC(obj);
    int i;

    object_initialize_child(obj, "armv7m", &s->armv7m, TYPE_ARMV7M);

    object_initialize_child(obj, "rcc", &s->rcc, TYPE_STM32F4XX_RCC);

    object_initialize_child(obj, "syscfg", &s->syscfg, TYPE_STM32F4XX_SYSCFG);

    object_initialize_child(obj, "flash_r", &s->flash_r, TYPE_STM32F4XX_FLASH);

    for (i = 0; i < STM_NUM_USARTS; i++)
    {
        object_initialize_child(obj, "usart[*]", &s->usart[i],
                                TYPE_STM32F2XX_USART);
    }

    for (i = 0; i < STM_NUM_TIMERS; i++)
    {
        object_initialize_child(obj, "timer[*]", &s->timer[i],
                                TYPE_STM32F2XX_TIMER);
    }

    for (i = 0; i < STM_NUM_ADCS; i++)
    {
        object_initialize_child(obj, "adc[*]", &s->adc[i], TYPE_STM32F2XX_ADC);
    }

    for (i = 0; i < STM_NUM_SPIS; i++)
    {
        object_initialize_child(obj, "spi[*]", &s->spi[i], TYPE_STM32F2XX_SPI);
    }

    object_initialize_child(obj, "exti", &s->exti, TYPE_STM32F4XX_EXTI);

    s->sysclk = qdev_init_clock_in(DEVICE(s), "sysclk", NULL, NULL, 0);
    s->refclk = qdev_init_clock_in(DEVICE(s), "refclk", NULL, NULL, 0);
}

static void stm32f411_soc_realize(DeviceState *dev_soc, Error **errp)
{
    STM32F411State *s = STM32F411_SOC(dev_soc);
    MemoryRegion *system_memory = get_system_memory();
    DeviceState *dev, *armv7m;
    SysBusDevice *busdev;
    Error *err = NULL;
    int i;

    /*
     * We use s->refclk internally and only define it with qdev_init_clock_in()
     * so it is correctly parented and not leaked on an init/deinit; it is not
     * intended as an externally exposed clock.
     */
    if (clock_has_source(s->refclk))
    {
        error_setg(errp, "refclk clock must not be wired up by the board code");
        return;
    }

    if (!clock_has_source(s->sysclk))
    {
        error_setg(errp, "sysclk clock must be wired up by the board code");
        return;
    }

    /*
     * TODO: ideally we should model the SoC RCC and its ability to
     * change the sysclk frequency and define different sysclk sources.
     */

    /* The refclk always runs at frequency HCLK / 8 */
    clock_set_mul_div(s->refclk, 8, 1);
    clock_set_source(s->refclk, s->sysclk);

    memory_region_init_rom(&s->flash, OBJECT(dev_soc), "STM32F411.flash",
                           FLASH_SIZE, &err);
    if (err != NULL)
    {
        error_propagate(errp, err);
        return;
    }
    memory_region_init_alias(&s->flash_alias, OBJECT(dev_soc),
                             "STM32F411.flash.alias", &s->flash, 0,
                             FLASH_SIZE);

    memory_region_add_subregion(system_memory, FLASH_BASE_ADDRESS, &s->flash);
    memory_region_add_subregion(system_memory, 0, &s->flash_alias);

    memory_region_init_ram(&s->sram, NULL, "STM32F411.sram", SRAM_SIZE,
                           &err);
    if (err != NULL)
    {
        error_propagate(errp, err);
        return;
    }
    memory_region_add_subregion(system_memory, SRAM_BASE_ADDRESS, &s->sram);

    armv7m = DEVICE(&s->armv7m);
    qdev_prop_set_uint32(armv7m, "num-irq", 100);
    qdev_prop_set_string(armv7m, "cpu-type", s->cpu_type);
    qdev_prop_set_bit(armv7m, "enable-bitband", true);
    qdev_connect_clock_in(armv7m, "cpuclk", s->sysclk);
    qdev_connect_clock_in(armv7m, "refclk", s->refclk);
    object_property_set_link(OBJECT(&s->armv7m), "memory",
                             OBJECT(system_memory), &error_abort);
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->armv7m), errp))
    {
        return;
    }

    /*  Reset and Clock controller */
    dev = DEVICE(&s->rcc);
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->rcc), errp))
    {
        return;
    }
    busdev = SYS_BUS_DEVICE(dev);
    sysbus_mmio_map(busdev, 0, RCC_ADDR);
    sysbus_connect_irq(busdev, 0, qdev_get_gpio_in(armv7m, RCC_IRQ));

    /* System configuration controller */
    dev = DEVICE(&s->syscfg);
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->syscfg), errp))
    {
        return;
    }
    busdev = SYS_BUS_DEVICE(dev);
    sysbus_mmio_map(busdev, 0, SYSCFG_ADDR);
    sysbus_connect_irq(busdev, 0, qdev_get_gpio_in(armv7m, SYSCFG_IRQ));

    /* Flash controller */
    dev = DEVICE(&s->flash_r);
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->flash_r), errp))
    {
        return;
    }
    busdev = SYS_BUS_DEVICE(dev);
    sysbus_mmio_map(busdev, 0, FLASH_R_ADDR);
    sysbus_connect_irq(busdev, 0, qdev_get_gpio_in(armv7m, FLASH_R_IRQ));

    /* Attach UART (uses USART registers) and USART controllers */
    for (i = 0; i < STM_NUM_USARTS; i++)
    {
        dev = DEVICE(&(s->usart[i]));
        qdev_prop_set_chr(dev, "chardev", serial_hd(i));
        if (!sysbus_realize(SYS_BUS_DEVICE(&s->usart[i]), errp))
        {
            return;
        }
        busdev = SYS_BUS_DEVICE(dev);
        sysbus_mmio_map(busdev, 0, usart_addr[i]);
        sysbus_connect_irq(busdev, 0, qdev_get_gpio_in(armv7m, usart_irq[i]));
    }

    /* Timer 2 to 5 */
    for (i = 0; i < STM_NUM_TIMERS; i++)
    {
        dev = DEVICE(&(s->timer[i]));
        qdev_prop_set_uint64(dev, "clock-frequency", 1000000000);
        if (!sysbus_realize(SYS_BUS_DEVICE(&s->timer[i]), errp))
        {
            return;
        }
        busdev = SYS_BUS_DEVICE(dev);
        sysbus_mmio_map(busdev, 0, timer_addr[i]);
        sysbus_connect_irq(busdev, 0, qdev_get_gpio_in(armv7m, timer_irq[i]));
    }

    /* ADC device, the IRQs are ORed together */
    if (!object_initialize_child_with_props(OBJECT(s), "adc-orirq",
                                            &s->adc_irqs, sizeof(s->adc_irqs),
                                            TYPE_OR_IRQ, errp, NULL))
    {
        return;
    }
    object_property_set_int(OBJECT(&s->adc_irqs), "num-lines", STM_NUM_ADCS,
                            &error_abort);
    if (!qdev_realize(DEVICE(&s->adc_irqs), NULL, errp))
    {
        return;
    }
    qdev_connect_gpio_out(DEVICE(&s->adc_irqs), 0,
                          qdev_get_gpio_in(armv7m, ADC_IRQ));

    for (i = 0; i < STM_NUM_ADCS; i++)
    {
        dev = DEVICE(&(s->adc[i]));
        if (!sysbus_realize(SYS_BUS_DEVICE(&s->adc[i]), errp))
        {
            return;
        }
        busdev = SYS_BUS_DEVICE(dev);
        sysbus_mmio_map(busdev, 0, adc_addr[i]);
        sysbus_connect_irq(busdev, 0,
                           qdev_get_gpio_in(DEVICE(&s->adc_irqs), i));
    }

    /* SPI devices */
    for (i = 0; i < STM_NUM_SPIS; i++)
    {
        dev = DEVICE(&(s->spi[i]));
        if (!sysbus_realize(SYS_BUS_DEVICE(&s->spi[i]), errp))
        {
            return;
        }
        busdev = SYS_BUS_DEVICE(dev);
        sysbus_mmio_map(busdev, 0, spi_addr[i]);
        sysbus_connect_irq(busdev, 0, qdev_get_gpio_in(armv7m, spi_irq[i]));
    }

    /* EXTI device */
    dev = DEVICE(&s->exti);
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->exti), errp))
    {
        return;
    }
    busdev = SYS_BUS_DEVICE(dev);
    sysbus_mmio_map(busdev, 0, EXTI_ADDR);
    for (i = 0; i < 16; i++)
    {
        sysbus_connect_irq(busdev, i, qdev_get_gpio_in(armv7m, exti_irq[i]));
    }
    for (i = 0; i < 16; i++)
    {
        qdev_connect_gpio_out(DEVICE(&s->syscfg), i, qdev_get_gpio_in(dev, i));
    }

    // TODO update with unimplemented devices for stm32f411
    create_unimplemented_device("timer[7]", 0x40001400, 0x400);
    create_unimplemented_device("timer[12]", 0x40001800, 0x400);
    create_unimplemented_device("timer[6]", 0x40001000, 0x400);
    create_unimplemented_device("timer[13]", 0x40001C00, 0x400);
    create_unimplemented_device("timer[14]", 0x40002000, 0x400);
    create_unimplemented_device("RTC and BKP", 0x40002800, 0x400);
    create_unimplemented_device("WWDG", 0x40002C00, 0x400);
    create_unimplemented_device("IWDG", 0x40003000, 0x400);
    create_unimplemented_device("I2S2ext", 0x40003000, 0x400);
    create_unimplemented_device("I2S3ext", 0x40004000, 0x400);
    create_unimplemented_device("I2C1", 0x40005400, 0x400);
    create_unimplemented_device("I2C2", 0x40005800, 0x400);
    create_unimplemented_device("I2C3", 0x40005C00, 0x400);
    create_unimplemented_device("CAN1", 0x40006400, 0x400);
    create_unimplemented_device("CAN2", 0x40006800, 0x400);
    create_unimplemented_device("PWR", 0x40007000, 0x400);
    create_unimplemented_device("DAC", 0x40007400, 0x400);
    create_unimplemented_device("timer[1]", 0x40010000, 0x400);
    create_unimplemented_device("timer[8]", 0x40010400, 0x400);
    create_unimplemented_device("SDIO", 0x40012C00, 0x400);
    create_unimplemented_device("timer[9]", 0x40014000, 0x400);
    create_unimplemented_device("timer[10]", 0x40014400, 0x400);
    create_unimplemented_device("timer[11]", 0x40014800, 0x400);
    create_unimplemented_device("GPIOA", 0x40020000, 0x400);
    create_unimplemented_device("GPIOB", 0x40020400, 0x400);
    create_unimplemented_device("GPIOC", 0x40020800, 0x400);
    create_unimplemented_device("GPIOD", 0x40020C00, 0x400);
    create_unimplemented_device("GPIOE", 0x40021000, 0x400);
    create_unimplemented_device("GPIOF", 0x40021400, 0x400);
    create_unimplemented_device("GPIOG", 0x40021800, 0x400);
    create_unimplemented_device("GPIOH", 0x40021C00, 0x400);
    create_unimplemented_device("GPIOI", 0x40022000, 0x400);
    create_unimplemented_device("CRC", 0x40023000, 0x400);
    create_unimplemented_device("BKPSRAM", 0x40024000, 0x400);
    create_unimplemented_device("DMA1", 0x40026000, 0x400);
    create_unimplemented_device("DMA2", 0x40026400, 0x400);
    create_unimplemented_device("Ethernet", 0x40028000, 0x1400);
    create_unimplemented_device("USB OTG HS", 0x40040000, 0x30000);
    create_unimplemented_device("USB OTG FS", 0x50000000, 0x31000);
    create_unimplemented_device("DCMI", 0x50050000, 0x400);
    create_unimplemented_device("RNG", 0x50060800, 0x400);
}

static Property stm32f411_soc_properties[] = {
    DEFINE_PROP_STRING("cpu-type", STM32F411State, cpu_type),
    DEFINE_PROP_END_OF_LIST(),
};

static void stm32f411_soc_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = stm32f411_soc_realize;
    device_class_set_props(dc, stm32f411_soc_properties);
    /* No vmstate or reset required: device has no internal state */
}

static const TypeInfo stm32f411_soc_info = {
    .name = TYPE_STM32F411_SOC,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(STM32F411State),
    .instance_init = stm32f411_soc_initfn,
    .class_init = stm32f411_soc_class_init,
};

static void stm32f411_soc_types(void)
{
    type_register_static(&stm32f411_soc_info);
}

type_init(stm32f411_soc_types)
