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
	switch(io_type) {
		case PINMUX_UART0:
			mmio_setbits_32(PINMUX_BASE + 0xB0, 0x3<<20); /* UART0_TX */
			mmio_setbits_32(PINMUX_BASE + 0xB4, 0x3<<4); /* UART0_RX */
		break;
		case PINMUX_UART1:
			mmio_setbits_32(PINMUX_BASE + 0xB4, 0x3<<20); /* UART1_TX */
			mmio_setbits_32(PINMUX_BASE + 0xB8, 0x3<<4); /* UART1_RX */
		break;
		case PINMUX_I2C0:
			mmio_clrsetbits_32(PINMUX_BASE + 0x9C, 0x3<<20, 0x1<<20); /* IIC0_SDA */
			mmio_clrsetbits_32(PINMUX_BASE + 0xA0, 0x3<<4, 0x1<<4); /* IIC0_SCL */
		break;
		case PINMUX_SPIF:
			mmio_clrbits_32(PINMUX_BASE + 0x20, (0x3<<20)|(0x3<<4)); /* ND_DATA7,ND_DATA6 */
			mmio_clrbits_32(PINMUX_BASE + 0x30, 0x3<<4); /* ND_RB1 */
			mmio_clrbits_32(PINMUX_BASE + 0x34, (0x3<<20)|(0x3<<4)); /* ND_CE1,ND_CE0 */
			mmio_clrbits_32(PINMUX_BASE + 0x38, (0x3<<20)|(0x3<<4)); /* ND_ALE,ND_CLE */
		break;
		case PINMUX_EMMC:
			mmio_clrsetbits_32(PINMUX_BASE + 0x24, (0x3<<20)|(0x3<<4), (0x1<<20)|(0x1<<4)); /* ND_DATA5,ND_DATA4 */
			mmio_clrsetbits_32(PINMUX_BASE + 0x28, (0x3<<20)|(0x3<<4), (0x1<<20)|(0x1<<4)); /* ND_DATA3,ND_DATA2 */
			mmio_clrsetbits_32(PINMUX_BASE + 0x2C, (0x3<<20)|(0x3<<4), (0x1<<20)|(0x1<<4)); /* ND_DATA1,ND_DATA0 */
			mmio_clrsetbits_32(PINMUX_BASE + 0x30, 0x3<<20, 0x1<<20); /* ND_RB0 */
			mmio_clrsetbits_32(PINMUX_BASE + 0x3C, (0x3<<20)|(0x3<<4), (0x1<<20)|(0x1<<4)); /* ND_WEB,ND_REB */
			mmio_clrsetbits_32(PINMUX_BASE + 0x40, 0x3<<4, 0x1<<4); /* ND_WPB */
		break;
		case PINMUX_NAND:
			mmio_write_32(TOP_BASE + REG_TOP_SOFT_RST,
				mmio_read_32(TOP_BASE + REG_TOP_SOFT_RST) & (~BIT_TOP_SOFT_RST_NAND));
			udelay(10);
			mmio_clrsetbits_32(PINMUX_BASE + 0x20, (0x3<<20)|(0x3<<4), (0x2<<20)|(0x2<<4)); /* ND_DATA7,ND_DATA6 */
			mmio_clrsetbits_32(PINMUX_BASE + 0x24, (0x3<<20)|(0x3<<4), (0x2<<20)|(0x2<<4)); /* ND_DATA5,ND_DATA4 */
			mmio_clrsetbits_32(PINMUX_BASE + 0x28, (0x3<<20)|(0x3<<4), (0x2<<20)|(0x2<<4)); /* ND_DATA3,ND_DATA2 */
			mmio_clrsetbits_32(PINMUX_BASE + 0x2C, (0x3<<20)|(0x3<<4), (0x2<<20)|(0x2<<4)); /* ND_DATA1,ND_DATA0 */
			mmio_clrsetbits_32(PINMUX_BASE + 0x30, (0x3<<20)|(0x3<<4), (0x2<<20)|(0x2<<4)); /* ND_RB1,ND_RB0 */
			mmio_clrsetbits_32(PINMUX_BASE + 0x34, (0x3<<20)|(0x3<<4), (0x2<<20)|(0x2<<4)); /* ND_CE1,ND_CE0 */
			mmio_clrsetbits_32(PINMUX_BASE + 0x38, (0x3<<20)|(0x3<<4), (0x2<<20)|(0x2<<4)); /* ND_ALE,ND_CLE */
			mmio_clrsetbits_32(PINMUX_BASE + 0x3C, (0x3<<20)|(0x3<<4), (0x2<<20)|(0x2<<4)); /* ND_WEB,ND_REB */
			mmio_clrsetbits_32(PINMUX_BASE + 0x40, 0x3<<4, 0x2<<4); /* ND_WPB */
			udelay(10);
			mmio_write_32(TOP_BASE + REG_TOP_SOFT_RST,
				mmio_read_32(TOP_BASE + REG_TOP_SOFT_RST) | BIT_TOP_SOFT_RST_NAND);

		break;
		case PINMUX_SDIO:
			mmio_write_32(PINMUX_BASE + 0x40, 0x30000); /* CLK, PULLUP */
			mmio_write_32(PINMUX_BASE + 0x44, 0x30003); /* CMD, DATA0 */
			mmio_write_32(PINMUX_BASE + 0x48, 0x30003); /* DATA1, DATA2 */
			mmio_write_32(PINMUX_BASE + 0x4C, 0x3); /* DATA3 */
		break;
		default:
		break;
	}
}

#define TOP_ECLK_DIV     0x50010848
#define TOP_SCLK_DIV     0x50010850

static void bm_set_sdhci_base_clock(void)
{
	// set emmc_clk to 125Mhz
	writel(0xC0009, TOP_ECLK_DIV);

	// don't set sd to 187Mhz, this will cause sd access failure in kernel
	// default sd_clk is 100MHz
//	writel(0x80009, TOP_SCLK_DIV);
}

int board_init(void)
{
	pinmux_config(PINMUX_UART0);
#ifdef CONFIG_CMD_NAND
	pinmux_config(PINMUX_NAND);
#else
	pinmux_config(PINMUX_EMMC);
#endif
	pinmux_config(PINMUX_SDIO);

	bm_set_sdhci_base_clock();

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

/*
 * Board specific software root reset.
 */
#define TOP_CTRL_REG        0x50010008
#define WATCHDOG_BASE       0x50026000
void software_root_reset(void)
{
	mmio_write_32(TOP_CTRL_REG, 0x04);
	mmio_write_32(WATCHDOG_BASE + 0x04, 0x20);
	mmio_write_32(WATCHDOG_BASE + 0x0C, 0x76);
	mmio_write_32(WATCHDOG_BASE, 0x13);
	while (1)
		;
}
