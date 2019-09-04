/*
 * (C) Copyright 2013
 * David Feng <fenghua@phytium.com.cn>
 * Sharma Bhupesh <bhupesh.sharma@freescale.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <dm.h>
#include <malloc.h>
#include <errno.h>
#include <netdev.h>
#include <asm/io.h>
#include <linux/compiler.h>
#include <dm/platform_data/serial_pl01x.h>
#include <asm/armv8/mmu.h>
#include <asm/arch-armv8/mmio.h>
#include "bm1882_reg.h"

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_PL011_SERIAL
static const struct pl01x_serial_platdata serial_platdata = {
	.base = V2M_UART0,
	.type = TYPE_PL011,
	.clock = CONFIG_PL011_CLOCK,
};

U_BOOT_DEVICE(vexpress_serials) = {
	.name = "serial_pl01x",
	.platdata = &serial_platdata,
};
#endif

static struct mm_region vexpress64_mem_map[] = {
	{
		.virt = 0x0UL,
		.phys = 0x0UL,
		.size = 0x80000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		.virt = PHYS_SDRAM_1,
		.phys = PHYS_SDRAM_1,
		.size = PHYS_SDRAM_1_SIZE,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE
#ifdef BM_UPDATE_FW_START_ADDR
	}, {
		.virt = BM_UPDATE_FW_START_ADDR,
		.phys = BM_UPDATE_FW_START_ADDR,
		/*
		 * this area is for bmtest under uboot. -- added by Xun Li
		 * [0x110000000, 0x190000000] size = 2G
		 */
		.size = BM_UPDATE_FW_SIZE,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE
#else
	}, {
		/*
		 * be aware we'll need 256MB more other than PHYS_SDRAM_1_SIZE for the fake flash area
		 * of itb file during ram boot, and MMC's DMA buffer (BM_UPDATE_ALIGNED_BUFFER).
		 * so either cover it here or in video's region.
		 * also be carefull with BM_SPIF_BUFFER_ADDR and BM_UPDATE_FW_START_ADDR...
		 */
		.virt = PHYS_SDRAM_1 + PHYS_SDRAM_1_SIZE,
		.phys = PHYS_SDRAM_1 + PHYS_SDRAM_1_SIZE,
		.size = 0x10000000,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE
#endif
	}, {
		/* List terminator */
		0,
	}
};

struct mm_region *mem_map = vexpress64_mem_map;

static void pinmux_config(int io_type)
{
	switch (io_type) {
	case PINMUX_SPIF:
		break;
	case PINMUX_EMMC:
	mmio_clrsetbits_32(PINMUX_BASE + 0x20, (0x7 << 0), (0x0 << 0)); /* CLK */
	mmio_clrsetbits_32(PINMUX_BASE + 0x24, (0x7 << 0), (0x0 << 0)); /* RSTN */
	mmio_clrsetbits_32(PINMUX_BASE + 0x28, (0x7 << 0), (0x0 << 0)); /* CMD */
	mmio_clrsetbits_32(PINMUX_BASE + 0x2c, (0x7 << 0), (0x0 << 0)); /* D1 */
	mmio_clrsetbits_32(PINMUX_BASE + 0x30, (0x7 << 0), (0x0 << 0)); /* D0 */
	mmio_clrsetbits_32(PINMUX_BASE + 0x34, (0x7 << 0), (0x0 << 0)); /* D2 */
	mmio_clrsetbits_32(PINMUX_BASE + 0x38, (0x7 << 0), (0x0 << 0)); /* D3 */
		break;
	case PINMUX_SDIO:
		break;
	case PINMUX_RMII0:
	/* Set pinmux for Ethernet 0 */
	mmio_clrsetbits_32(PINMUX_BASE + 0x6C, (0x7 << 0), (0x3 << 0)); /* RGMII0_IRQ */
	mmio_clrsetbits_32(PINMUX_BASE + 0x68, (0x7 << 0), (0x0 << 0)); /* RGMII0_MDCK */
	mmio_clrsetbits_32(PINMUX_BASE + 0x64, (0x7 << 0), (0x0 << 0)); /* RGMII0_MDIO */
	mmio_clrsetbits_32(PINMUX_BASE + 0x60, (0x7 << 0), (0x0 << 0)); /* RGMII0_REFCLKO */
	mmio_clrsetbits_32(PINMUX_BASE + 0x78, (0x7 << 0), (0x0 << 0)); /* RGMII0_RXDV */
	mmio_clrsetbits_32(PINMUX_BASE + 0x70, (0x7 << 0), (0x0 << 0)); /* RGMII0_RXD0 */
	mmio_clrsetbits_32(PINMUX_BASE + 0x74, (0x7 << 0), (0x0 << 0)); /* RGMII0_RXD1 */
	mmio_clrsetbits_32(PINMUX_BASE + 0x54, (0x7 << 0), (0x0 << 0)); /* RGMII0_TXEN */
	mmio_clrsetbits_32(PINMUX_BASE + 0x5C, (0x7 << 0), (0x0 << 0)); /* RGMII0_TXD0 */
	mmio_clrsetbits_32(PINMUX_BASE + 0x58, (0x7 << 0), (0x0 << 0)); /* RGMII0_TXD1 */
		break;
	case PINMUX_UART0:
		break;
	default:
		break;
	}
}

int board_init(void)
{
#if defined(CONFIG_TARGET_BITMAIN_BM1882_FPGA)
	writel(0x000000C1, 0x03000034); /* Set eth0 RGMII, eth1 RMII clk resource and interface type*/
#elif defined(CONFIG_TARGET_BITMAIN_BM1882_PALLADIUM)
	writel(0x0000001C, 0x03000034); /* Set eth0 RMII, eth1 RGMII clk resource and interface type*/
#endif
	pinmux_config(PINMUX_RMII0);

	pinmux_config(PINMUX_EMMC);

	return 0;
}

int dram_init(void)
{
	gd->ram_size = PHYS_SDRAM_1_SIZE;
	return 0;
}

int dram_init_banksize(void)
{
	gd->bd->bi_dram[0].start = PHYS_SDRAM_1;
	gd->bd->bi_dram[0].size = PHYS_SDRAM_1_SIZE;

	return 0;
}

/*
 * Board specific reset that is system reset.
 */
void reset_cpu(ulong addr)
{
}

/*
 * Board specific ethernet initialization routine.
 */
int board_eth_init(bd_t *bis)
{
	int rc = 0;
#ifdef CONFIG_SMC91111
	rc = smc91111_initialize(0, CONFIG_SMC91111_BASE);
#endif
#ifdef CONFIG_SMC911X
	rc = smc911x_initialize(0, CONFIG_SMC911X_BASE);
#endif
	return rc;
}

