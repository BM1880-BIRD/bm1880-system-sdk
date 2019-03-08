/*
 * Copyright (c) 2015-2016, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __BM_PRIVATE_H
#define __BM_PRIVATE_H

enum {
	FIP_SRC_I2C_DWLD = 0x0,
	FIP_SRC_UART = 0x1,
	FIP_SRC_NAND = 0x2,
	FIP_SRC_SPI_FLASH = 0x3,
	FIP_SRC_EMMC = 0x4,
	FIP_SRC_SDFAT = 0x5,
	FIP_SRC_USB = 0x6,
	FIP_SRC_MEMMAP = 0x7,
};

enum { /* patch table index in SPI flash */
	PT_USER_CONF = 0,
	PT_SDIO_PHY,
	PT_RESERVED_2,
	PT_RESERVED_3,
	PT_RESERVED_4,
};

void plat_bm_io_setup(void);
void plat_bm_ddr_init(void);
uint32_t  plat_bm_efuse_read(uint32_t address);
uint32_t plat_bm_gpio_read(uint32_t mask);
void plat_bm_unleash_arm926(void);
void plat_bm_spi_flash_patch_table(int index);
void plat_bm_set_pinmux(int io_type);
void plat_init_user_conf(void);
int plat_bm_clock_init(void);
int plat_bm_sd_get_clk(void);
int plat_bm_emmc_get_clk(void);
void plat_bm_init_uart(void);

#endif /*__BM_PRIVATE_H*/
