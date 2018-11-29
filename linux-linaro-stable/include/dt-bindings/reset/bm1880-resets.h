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

#ifndef _DT_BINDINGS_RST_BM1880_H_
#define _DT_BINDINGS_RST_BM1880_H_

#define RST_MAIN_AP		0
#define RST_SECOND_AP		1
#define RST_DDR			2
#define RST_VIDEO		3
#define RST_JPEG		4
#define RST_VPP			5
#define RST_GDMA		6
#define RST_AXI_SRAM		7
#define RST_TPU			8
#define RST_USB			9
#define RST_ETH0		10
#define RST_ETH1		11
#define RST_NAND		12
#define RST_EMMC		13
#define RST_SD			14
#define RST_SDMA		15
#define RST_I2S0		16
#define RST_I2S1		17
#define RST_UART0_1_CLK		18
#define RST_UART0_1_ACLK	19
#define RST_UART2_3_CLK		20
#define RST_UART2_3_ACLK	21
#define RST_MINER		22
#define RST_I2C0		23
#define RST_I2C1		24
#define RST_I2C2		25
#define RST_I2C3		26
#define RST_I2C4		27
#define RST_PWM0		28
#define RST_PWM1		29
#define RST_PWM2		30
#define RST_PWM3		31
#define RST_SPI			32
#define RST_GPIO0		33
#define RST_GPIO1		34
#define RST_GPIO2		35
#define RST_EFUSE		36
#define RST_WDT			37
#define RST_AHB_ROM		38
#define RST_SPIC		39

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



#endif /* _DT_BINDINGS_RST_BM1880_H_ */
