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

// TODO rename into stm32f411_rcc.h since this is specific to F411 cores

#ifndef HW_STM32F4XX_RCC_H
#define HW_STM32F4XX_RCC_H

#include "hw/sysbus.h"
#include "qom/object.h"

#define RCC_CR 0x00
#define RCC_PLLCFGR 0x04
#define RCC_CFGR 0x08
#define RCC_CIR 0x0C
#define RCC_AHB1RSTR 0x10
#define RCC_AHB2RSTR 0x14
#define RCC_APB1RSTR 0x20
#define RCC_APB2RSTR 0x24
#define RCC_AHB1ENR 0x30
#define RCC_AHB2ENR 0x34
#define RCC_APB1ENR 0x40
#define RCC_APB2ENR 0x44
#define RCC_AHB1LPENR 0x50
#define RCC_AHB2LPENR 0x54
#define RCC_APB1LPENR 0x60
#define RCC_APB2LPENR 0x64
#define RCC_BDCR 0x70
#define RCC_CSR 0x74
#define RCC_SSCGR 0x80
#define RCC_PLLI2SCFGR 0x84
#define RCC_DCKCFGR 0x8C

#define TYPE_STM32F4XX_RCC "stm32f4xx-rcc"
OBJECT_DECLARE_SIMPLE_TYPE(STM32F4xxRccState, STM32F4XX_RCC)

struct STM32F4xxRccState
{
    SysBusDevice parent_obj;

    MemoryRegion mmio;

    uint32_t rcc_cr;           /*!< RCC clock control register,                                  Address offset: 0x00 */
    uint32_t rcc_pllcfgr;      /*!< RCC PLL configuration register,                              Address offset: 0x04 */
    uint32_t rcc_cfgr;         /*!< RCC clock configuration register,                            Address offset: 0x08 */
    uint32_t rcc_cir;          /*!< RCC clock interrupt register,                                Address offset: 0x0C */
    uint32_t rcc_ahb1rstr;     /*!< RCC AHB1 peripheral reset register,                          Address offset: 0x10 */
    uint32_t rcc_ahb2rstr;     /*!< RCC AHB2 peripheral reset register,                          Address offset: 0x14 */
    uint32_t rcc_reserved0[2]; /*!< Reserved, 0x1C                                                                    */
    uint32_t rcc_apb1rstr;     /*!< RCC APB1 peripheral reset register,                          Address offset: 0x20 */
    uint32_t rcc_apb2rstr;     /*!< RCC APB2 peripheral reset register,                          Address offset: 0x24 */
    uint32_t rcc_reserved1[2]; /*!< Reserved, 0x28-0x2C                                                               */
    uint32_t rcc_ahb1enr;      /*!< RCC AHB1 peripheral clock register,                          Address offset: 0x30 */
    uint32_t rcc_ahb2enr;      /*!< RCC AHB2 peripheral clock register,                          Address offset: 0x34 */
    uint32_t rcc_reserved2[2]; /*!< Reserved, 0x3C                                                                    */
    uint32_t rcc_apb1enr;      /*!< RCC APB1 peripheral clock enable register,                   Address offset: 0x40 */
    uint32_t rcc_apb2enr;      /*!< RCC APB2 peripheral clock enable register,                   Address offset: 0x44 */
    uint32_t rcc_reserved3[2]; /*!< Reserved, 0x48-0x4C                                                               */
    uint32_t rcc_ahb1lpenr;    /*!< RCC AHB1 peripheral clock enable in low power mode register, Address offset: 0x50 */
    uint32_t rcc_ahb2lpenr;    /*!< RCC AHB2 peripheral clock enable in low power mode register, Address offset: 0x54 */
    uint32_t rcc_reserved4[2]; /*!< Reserved, 0x5C                                                                    */
    uint32_t rcc_apb1lpenr;    /*!< RCC APB1 peripheral clock enable in low power mode register, Address offset: 0x60 */
    uint32_t rcc_apb2lpenr;    /*!< RCC APB2 peripheral clock enable in low power mode register, Address offset: 0x64 */
    uint32_t rcc_reserved5[2]; /*!< Reserved, 0x68-0x6C                                                               */
    uint32_t rcc_bdcr;         /*!< RCC Backup domain control register,                          Address offset: 0x70 */
    uint32_t rcc_csr;          /*!< RCC clock control & status register,                         Address offset: 0x74 */
    uint32_t rcc_reserved6[2]; /*!< Reserved, 0x78-0x7C                                                               */
    uint32_t rcc_sscgr;        /*!< RCC spread spectrum clock generation register,               Address offset: 0x80 */
    uint32_t rcc_plli2scfgr;   /*!< RCC PLLI2S configuration register,                           Address offset: 0x84 */
    uint32_t rcc_reserved7[1]; /*!< Reserved, 0x88                                                                    */
    uint32_t rcc_dckcfgr;      /*!< RCC Dedicated Clocks configuration register,                 Address offset: 0x8C */

    qemu_irq irq;
};

#endif
