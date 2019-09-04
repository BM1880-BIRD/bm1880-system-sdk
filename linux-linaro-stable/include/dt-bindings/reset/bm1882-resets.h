/*
 * Bitmain SoCs Reset definitions
 *
 * Copyright (c) 2018 Bitmain Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _DT_BINDINGS_RST_BM1882_H_
#define _DT_BINDINGS_RST_BM1882_H_

/* Reset Registers0 */
#define RST_MAIN_AP		0
#define RST_SECOND_AP	1
#define RST_DDR			2
#define RST_H264C		3
#define RST_JPEG		4
#define RST_H265C		5
#define RST_VIP         6
#define RST_TDMA		7
#define RST_TPU			8
#define RST_TPU_FAB		9
#define RST_TSM         10
#define RST_USB         11
#define RST_ETH0		12
#define RST_ETH1		13
#define RST_NAND		14
#define RST_EMMC		15
#define RST_SD0         16
#define RST_SD1         17
#define RST_SDMA		18
#define RST_I2S0        19
#define RST_I2S1		20
#define RST_I2S2        21
#define RST_I2S3		22
#define RST_UART0		23
#define RST_UART1		24
#define RST_UART2		25
#define RST_UART3		26
#define RST_I2C0		27
#define RST_I2C1		28
#define RST_I2C2		29
#define RST_I2C3		30
#define RST_I2C4		31

/* Reset Register1 */
#define RST_PWM0		0
#define RST_PWM1        1
#define RST_PWM2		2
#define RST_PWM3		3
#define RST_PWM4		4
#define RST_PWM5		5
#define RST_PWM6		6
#define RST_PWM7		7
#define RST_SPI0		8
#define RST_SPI1		9
#define RST_SPI2		10
#define RST_SPI3        11
#define RST_GPIO0		12
#define RST_GPIO1		13
#define RST_GPIO2		14
#define RST_EFUSE		15
#define RST_WDT         16
#define RST_AHB_ROM		17
#define RST_SPIC		18
#define RST_TEMPSEN     19
#define RST_SARADC		20
#define RST_PCIE0       21
#define RST_PCIE1		22
#define RST_PCIE2		23
#define RST_PCIE3		24
#define RST_M31_PHY0	25
#define RST_M31_PHY1	26

#if 0
#define CLK_RST_A53			0
#define CLK_RST_50M_A53		1
#define CLK_RST_AHB_ROM		2
#define CLK_RST_AXI_SRAM	3
#define CLK_RST_DDR_AXI		4
#define CLK_RST_EFUSE		5
#define CLK_RST_APB_EFUSE	6
#define CLK_RST_AXI_EMMC	7
#define CLK_RST_EMMC		8
#define CLK_RST_100K_EMMC	9
#define CLK_RST_AXI_SD		10
#define CLK_RST_SD			11
#define CLK_RST_100K_SD		12
#define CLK_RST_500M_ETH0	13
#define CLK_RST_AXI_ETH0	14
#define CLK_RST_500M_ETH1	15
#define CLK_RST_AXI_ETH1	16
#define CLK_RST_AXI_GDMA	17
#define CLK_RST_APB_GPIO	18
#define CLK_RST_APB_GPIO_INTR	19
#define CLK_RST_GPIO_DB		20
#define CLK_RST_AXI_MINER	21
#define CLK_RST_AHB_SF		22
#define CLK_RST_SDMA_AXI	23
#define CLK_RST_SDMA_AUD	24
#define CLK_RST_APB_I2C		25
#define CLK_RST_APB_WDT		26
#define CLK_RST_APB_JPEG	27
#define CLK_RST_JPEG_AXI	28
#define CLK_RST_AXI_NF		29
#define CLK_RST_APB_NF		30
#define CLK_RST_NF			31
#define CLK_RST_APB_PWM		32
#define CLK_RST_RV			33
#define CLK_RST_APB_SPI		34
#define CLK_RST_TPU_AXI		35
#define CLK_RST_UART_500M	36
#define CLK_RST_APB_UART	37
#define CLK_RST_APB_I2S		38
#define CLK_RST_AXI_USB		39
#define CLK_RST_APB_USB		40
#define CLK_RST_125M_USB	41
#define CLK_RST_33K_USB		42
#define CLK_RST_12M_USB		43
#define CLK_RST_APB_VIDEO	44
#define CLK_RST_VIDEO_AXI	45
#define CLK_RST_VPP_AXI		46
#define CLK_RST_APB_VPP		47
#define CLK_RST_AXI1		48
#define CLK_RST_AXI2		49
#define CLK_RST_AXI3		50
#define CLK_RST_AXI4		51
#define CLK_RST_AXI5		52
#define CLK_RST_AXI6		53
#endif


#endif /* _DT_BINDINGS_RST_BM1880_H_ */
