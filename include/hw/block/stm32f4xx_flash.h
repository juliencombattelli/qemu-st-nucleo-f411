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

#ifndef HW_STM32F4XX_FLASH_H
#define HW_STM32F4XX_FLASH_H

#include "hw/sysbus.h"
#include "qom/object.h"

#define TYPE_STM32F4XX_FLASH "stm32f4xx-flash-r"
OBJECT_DECLARE_SIMPLE_TYPE(STM32F4xxFlashState, STM32F4XX_FLASH)

#define FLASH_ACR 0x00
#define FLASH_KEYR 0x04
#define FLASH_OPTKEYR 0x08
#define FLASH_SR 0x0C
#define FLASH_CR 0x10
#define FLASH_OPTCR 0x14
#define FLASH_OPTCR1 0x18

struct STM32F4xxFlashState
{
    SysBusDevice parent_obj;

    MemoryRegion mmio;

    uint32_t flash_acr;      /*!< FLASH access control register,   Address offset: 0x00 */
    uint32_t flash_keyr;     /*!< FLASH key register,              Address offset: 0x04 */
    uint32_t flash_optkeyr;  /*!< FLASH option key register,       Address offset: 0x08 */
    uint32_t flash_sr;       /*!< FLASH status register,           Address offset: 0x0C */
    uint32_t flash_cr;       /*!< FLASH control register,          Address offset: 0x10 */
    uint32_t flash_optcr;    /*!< FLASH option control register ,  Address offset: 0x14 */
    uint32_t flash_optcr1;   /*!< FLASH option control register 1, Address offset: 0x18 */

    qemu_irq irq;
};

#endif
