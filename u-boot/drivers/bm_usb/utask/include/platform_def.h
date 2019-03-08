/*
 * Copyright (c) 2015-2016, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __PLATFORM_DEF_H__
#define __PLATFORM_DEF_H__

#define BUILD_A53_VERSION

#define PLATFORM_STACK_SIZE 0x1000

#define DISABLE_DCACHE

/* When write fip.bin to NAND, inject error to 1st fip.bin copy */
#define INJECT_1ST_FIP_CRC_ERROR	0

#define HPNFC_USE_PIO_MODE 1

/* For riscv, image size might bigger than TPU size, load them to DDR */
#if defined(BUILD_RISCV_VERSION)
#define USE_TPU_AS_BUFFER	0 /* 1 for TPU, 0 for DDR*/
#elif defined(BUILD_A53_VERSION)
#define USE_TPU_AS_BUFFER	1 /* 1 for TPU, 0 for DDR*/
#else
#error "Err buffer setting"
#endif

#ifdef SUBTYPE_PALLADIUM
#define USE_FAST_TIMING_SETTING		1
#else
#define USE_FAST_TIMING_SETTING		0
#endif

/* #ifdef BUILD_RISCV_VERSION*/
#define RISCV_BL1_IMAGE_MAX_SIZE	(64 * 1024) /* 64KB*/
#define RISCV_BL2_IMAGE_MAX_SIZE	(16 * 1024 * 1024) /* 10MB*/
#define LOAD_RISCV_BL1_IMG_OFFSET	0
#define LOAD_RISCV_BL2_IMG_OFFSET	RISCV_BL1_IMAGE_MAX_SIZE
/* #endif*/

#define TPU_SRAM_ADDRESS	(0x08003000)	/* for USB dl, first 0x3000 is resvered*/
#define DDR_RAM_ADDRESS		(0x100000000)

#define MAX_FIP_SZ (512 * 1024) /* MAX fip.bin size is 512 KB*/

#define RISCV_IMG_READ_BACK_OFFSET (RISCV_BL1_IMAGE_MAX_SIZE + RISCV_BL2_IMAGE_MAX_SIZE)

#define PLATFORM_MAX_CPUS_PER_CLUSTER	2
#define PLATFORM_CLUSTER_COUNT			1
#define PLATFORM_CLUSTER0_CORE_COUNT	PLATFORM_MAX_CPUS_PER_CLUSTER
#define PLATFORM_CORE_COUNT				PLATFORM_CLUSTER0_CORE_COUNT

#define BM_PRIMARY_CPU			0

#define PLAT_NUM_PWR_DOMAINS		(PLATFORM_CLUSTER_COUNT + \
					PLATFORM_CORE_COUNT + 1)
#define PLAT_MAX_PWR_LVL		MPIDR_AFFLVL1

#define PLAT_MAX_RET_STATE		1
#define PLAT_MAX_OFF_STATE		2

/* Local power state for power domains in Run state. */
#define PLAT_LOCAL_STATE_RUN		0
/* Local power state for retention. Valid only for CPU power domains */
#define PLAT_LOCAL_STATE_RET		1
/*
 * Local power state for OFF/power-down. Valid for CPU and cluster power
 * domains.
 */
#define PLAT_LOCAL_STATE_OFF		2

/*
 * Macros used to parse state information from State-ID if it is using the
 * recommended encoding for State-ID.
 */
#define PLAT_LOCAL_PSTATE_WIDTH		4
#define PLAT_LOCAL_PSTATE_MASK		((1 << PLAT_LOCAL_PSTATE_WIDTH) - 1)

/*
 * Some data must be aligned on the biggest cache line size in the platform.
 * This is known only to the platform as it might have a combination of
 * integrated and external caches.
 */
#define CACHE_WRITEBACK_SHIFT		6
#define CACHE_WRITEBACK_GRANULE		BIT(CACHE_WRITEBACK_SHIFT)

#define PLAT_PHY_ADDR_SPACE_SIZE	BIT_ULL(36)
#define PLAT_VIRT_ADDR_SPACE_SIZE	BIT_ULL(36)
#define MAX_MMAP_REGIONS		8
#define MAX_XLAT_TABLES			3 /* varies when memory layout changes, FIXME*/
#define MAX_IO_DEVICES			8 /* FIP, MEMMAP, eMMC, SD card, SPI flash, NAND and I2C/USB download*/
#define MAX_IO_HANDLES			2 /* FIP and one of [MEMMAP, eMMC, SPI flash, I2C/USB download]*/
#define MAX_IO_BLOCK_DEVICES		2 /* eMMC or SPI flash*/

/*
 * Partition memory into secure ROM, non-secure SRAM, secure SRAM.
 * the "SRAM" region is NPS SRAM, 128KB in total.
 * all sizes need to be page aligned for page table requirement.
 */
#define NS_DRAM0_BASE			0x100000000
#define NS_DRAM0_SIZE			0x10000000 /* 256MB*/

#define SEC_SRAM_BASE			0x04000000
#define SEC_SRAM_SIZE			0x00020000 /* 128KB*/
#define SRAM_FLGA_ADDR1         (SEC_SRAM_BASE + 0x384)
#define SRAM_FLGA_ADDR2         (SEC_SRAM_BASE + 0x388)

#define NS_IMAGE_OFFSET			0x108000000

#ifdef BL1_IN_SPI_FLASH
#define SEC_ROM_BASE			0x06000000
#define SEC_ROM_SIZE			0x00020000 /* 128KB*/
#else
#define SEC_ROM_BASE			0x07000000
#define SEC_ROM_SIZE			0x00020000 /* 128KB*/
#endif

#define BM_FLASH0_BASE			0x110000000
#define BM_FLASH0_SIZE			0x00080000 /* 512KB*/

/*
 * ARM-TF lives in SRAM, partition it here
 */
#define SHARED_RAM_BASE			SEC_SRAM_BASE
#define SHARED_RAM_SIZE			0x00001000 /* 4KB*/

#define PLAT_BM_TRUSTED_MAILBOX_BASE	SHARED_RAM_BASE
#define PLAT_BM_TRUSTED_MAILBOX_SIZE	(8 + PLAT_BM_HOLD_SIZE)
#define PLAT_BM_HOLD_BASE		(PLAT_BM_TRUSTED_MAILBOX_BASE + 8)
#define PLAT_BM_HOLD_SIZE		(PLATFORM_CORE_COUNT * \
					  PLAT_BM_HOLD_ENTRY_SIZE)
#define PLAT_BM_HOLD_ENTRY_SIZE	8
#define PLAT_BM_HOLD_STATE_WAIT	0
#define PLAT_BM_HOLD_STATE_GO	1

#define PLAT_COMMON_INFO	(SHARED_RAM_BASE + 0x400 - 0x80) /* last 128B of the 1st 1KB AXI-SRAM*/

#define BL_RAM_BASE			(SHARED_RAM_BASE + SHARED_RAM_SIZE)
#define BL_RAM_SIZE			(SEC_SRAM_SIZE - SHARED_RAM_SIZE)

/* block IO buffer's start address and size must be block size aligned*/
#define BM_EMMC_BUF_BASE		BL_RAM_BASE
#define BM_EMMC_BUF_SIZE		0x800 /* 2KB*/
#define BM_SPI_BUF_BASE			(BM_EMMC_BUF_BASE + BM_EMMC_BUF_SIZE)
#define BM_SPI_BUF_SIZE			0x800 /* 2KB*/

/*
 * BL1 specific defines.
 *
 * BL1 RW data is relocated from ROM to RAM at runtime so we need 2 sets of
 * addresses.
 * Put BL1 RW at the top of the Secure SRAM. BL1_RW_BASE is calculated using
 * the current BL1 RW debug size plus a little space for growth.
 */
#define BL1_RO_BASE			SEC_ROM_BASE
#define BL1_RO_LIMIT			(SEC_ROM_BASE + SEC_ROM_SIZE)
#define BL1_RW_BASE			(BL1_RW_LIMIT - 0x6000) /* 24KB*/
#define BL1_RW_LIMIT			(BL_RAM_BASE + BL_RAM_SIZE)

/*
 * BL2 specific defines.
 *
 * Put BL2 just below BL1 RW, and reserve a 8KB at the base of BL RAM for
 * large buffers.
 */
#define BL2_BASE			(BL_RAM_BASE + 0x2000) /* 8KB*/
#define BL2_LIMIT			BL1_RW_BASE

/*
 * BL3-1 specific defines.
 *
 * Put BL3-1 at the base of DRAM. BL31_BASE is calculated using the
 * current BL3-1 debug size plus a little space for growth.
 */
#define BL31_BASE			NS_DRAM0_BASE
#define BL31_LIMIT			(NS_DRAM0_BASE + 0x20000) /* 128KB*/
#define BL31_PROGBITS_LIMIT		(NS_DRAM0_BASE + 0x10000) /* 64KB*/

/*
 * FIP binary defines.
 */
#define PLAT_BM_FIP_BASE	BM_FLASH0_BASE
#define PLAT_BM_FIP_MAX_SIZE	BM_FLASH0_SIZE

/*
 * SPI flash offset defines.
 */
#define SPIF_BL1_SIZE		(128 * 1024)
#define SPIF_PATCH_USERCONF_SIZE	(128)
#define SPIF_PATCH_TABLE1_SIZE	(32 * 1024 - 128)
#define SPIF_PATCH_TABLE2_SIZE	(32 * 1024)
#define SPIF_PATCH_TABLE3_SIZE	(32 * 1024)
#define SPIF_PATCH_TABLE4_SIZE	(32 * 1024)

#define SPIF_BL1		(0)
#define SPIF_PATCH_USERCONF	(SPIF_BL1 + SPIF_BL1_SIZE)
#define SPIF_PATCH_TABLE1	(SPIF_PATCH_USERCONF + SPIF_PATCH_USERCONF_SIZE)
#define SPIF_PATCH_TABLE2	(SPIF_PATCH_TABLE1 + SPIF_PATCH_TABLE1_SIZE)
#define SPIF_PATCH_TABLE3	(SPIF_PATCH_TABLE2 + SPIF_PATCH_TABLE2_SIZE)
#define SPIF_PATCH_TABLE4	(SPIF_PATCH_TABLE3 + SPIF_PATCH_TABLE3_SIZE)
#define SPIF_FIP		(SPIF_PATCH_TABLE4 + SPIF_PATCH_TABLE4_SIZE)

/*
 * device register defines.
 */
#define DEVICE0_BASE			0x00000000 /* ITCM + DTCM*/
#define DEVICE0_SIZE			0x03000000 /* 48MB*/
#define DEVICE1_BASE			0x50000000 /* peripheral registers*/
#define DEVICE1_SIZE			0x30000000
#define DEVICE2_BASE			0x06000000 /* SPI flash*/
#define DEVICE2_SIZE			0x00100000 /* 1MB*/

#define SPIF_BASE			0x06000000
#define EMMC_BASE			0x50100000
#define SDIO_BASE			0x50101000
#define DDR_CTRL0			0x58020000
#define TOP_BASE			0x50010000
#define PINMUX_BASE			(TOP_BASE + 0x400)
#define CLKGEN_BASE			(TOP_BASE + 0x800)
#define RST_BASE			(TOP_BASE + 0xC00)
#define I2C0_BASE			0x5001A000
#define I2C1_BASE			0x5001C000
#define I2C2_BASE			0x5001E000
#define I2C3_BASE			0x50020000
#define UART0_BASE			0x58018000
#define WATCHDOG_BASE       0x50026000
#define GPIO_BASE			0x50027000
#define EFUSE_BASE			0x50028000
#define NFC_BASE			0x500C0000
#define NFC_SDMA_BASE       0x500C3000
#define USB_BASE            0x50080000
#define USB_DEV_BASE        0x500A0000

/*
 * TOP registers.
 */
#define REG_TOP_CHIPID		(TOP_BASE + 0x0)
#define REG_TOP_CONF_INFO	(TOP_BASE + 0x4)
#define REG_TOP_SOFT_RST		0xC00
#define REG_TOP_IIC1_CLK_DIV	0x80
#define REG_TOP_IIC2_CLK_DIV	0x84
#define REG_TOP_USB_CTLSTS	0x38
#define REG_TOP_DDR_ADDR_MODE	(TOP_BASE + 0x64)

#define BIT_TOP_SOFT_RST_A53        (1)
#define BIT_TOP_SOFT_RST_RISCV      BIT(1)
#define BIT_TOP_SOFT_RST_EMMC		BIT(13)
#define BIT_TOP_SOFT_RST_SDIO		BIT(14)
#define BIT_TOP_SOFT_RST_IIC		BIT(23)
#define BIT_TOP_SOFT_RST_NFC        BIT(29)
#define BIT_TOP_SOFT_RST_USB        BIT(9)

#define BIT_TOP_USB_CTLSTS_MODE_MASK    7U
#define BIT_TOP_USB_CTLSTS_MODE_DEV	4U

#define PINMUX_SPIF			0x0
#define PINMUX_EMMC			0x1
#define PINMUX_NAND			0x2
#define PINMUX_SDIO			0x3
#define PINMUX_I2C0			0x4
#define PINMUX_I2C1			0x5
#define PINMUX_I2C2			0x6
#define PINMUX_I2C3			0x7
#define PINMUX_I2C4			0x8
#define PINMUX_PWM0			0x9
#define PINMUX_PWM1			0xA
#define PINMUX_PWM2			0xB
#define PINMUX_PWM3			0xC
#define PINMUX_UART0		0xD
#define PINMUX_UART1		0xE
#define PINMUX_UART2		0xF
#define PINMUX_UART3		0x10
#define PINMUX_UART4		0x11
#define PINMUX_UART5		0x12
#define PINMUX_UART6		0x13
#define PINMUX_UART7		0x14
#define PINMUX_UART8		0x15
#define PINMUX_UART9		0x16
#define PINMUX_UART10		0x17
#define PINMUX_UART11		0x18
#define PINMUX_GPIO0		0x19
#define PINMUX_GPIO1		0x1A
#define PINMUX_GPIO2		0x1B
#define PINMUX_GPIO3		0x1C
#define PINMUX_GPIO4		0x1D
#define PINMUX_GPIO5		0x1E
#define PINMUX_GPIO6		0x1F
#define PINMUX_GPIO7		0x20
#define PINMUX_GPIO8		0x21
#define PINMUX_GPIO9		0x22

/*
 * GIC definitions.
 */
#define PLAT_ARM_GICD_BASE		0x50001000
#define PLAT_ARM_GICC_BASE		0x50002000

#define BM_IRQ_SEC_SGI_0		8
#define BM_IRQ_SEC_SGI_1		9
#define BM_IRQ_SEC_SGI_2		10
#define BM_IRQ_SEC_SGI_3		11
#define BM_IRQ_SEC_SGI_4		12
#define BM_IRQ_SEC_SGI_5		13
#define BM_IRQ_SEC_SGI_6		14
#define BM_IRQ_SEC_SGI_7		15
#define BM_IRQ_SEC_PHY_TIMER		29

#define PLAT_ARM_G1S_IRQS		BM_IRQ_SEC_PHY_TIMER, \
					BM_IRQ_SEC_SGI_1, \
					BM_IRQ_SEC_SGI_2, \
					BM_IRQ_SEC_SGI_3, \
					BM_IRQ_SEC_SGI_4, \
					BM_IRQ_SEC_SGI_5, \
					BM_IRQ_SEC_SGI_7
#define PLAT_ARM_G0_IRQS		BM_IRQ_SEC_SGI_0, \
					BM_IRQ_SEC_SGI_6

/*
 * clock definitions.
 * to get more close to wall time, on FPGA we need 10MHz, and on Palladium
 * we need 50KHz. but this will make too many timer interrupts in kenrel,
 * which makes kernel barely unable to boot. 10MHz on Palladium and 500MHz
 * on FPGA seem to be still a slow but acceptable choice. we cat achieve
 * different timer setting for kernel by adding clock-frequency node at timer
 * in device tree.
 */

#ifdef SUBTYPE_FPGA
#define UART0_CLK_IN_HZ			25000000
#define SYS_COUNTER_FREQ_IN_TICKS	25000000	/*origin: 10000000*/
#else
#define UART0_CLK_IN_HZ			153600
#define SYS_COUNTER_FREQ_IN_TICKS	50000
#endif

/*
 * UART console
 */
#define PLAT_BM_BOOT_UART_BASE		UART0_BASE
#define PLAT_BM_BOOT_UART_CLK_IN_HZ	UART0_CLK_IN_HZ

#define PLAT_BM_CRASH_UART_BASE		UART0_BASE
#define PLAT_BM_CRASH_UART_CLK_IN_HZ	UART0_CLK_IN_HZ

#ifdef THIS_IS_FPGA
#define PLAT_BM_CONSOLE_BAUDRATE	115200
#else
#define PLAT_BM_CONSOLE_BAUDRATE	9600
/* #define PLAT_BM_CONSOLE_BAUDRATE	(UART0_CLK_IN_HZ / 16)  make divisor=1*/
#endif

/*
 * GPIO definitions
 */
#define BIT_MASK_GPIO_I2C_ADDR		0x0
#define BIT_MASK_GPIO_UART_CLI		0x1
#define BIT_MASK_GPIO_EFUSE_PATCH	0x2
#define BIT_MASK_GPIO_SPIF_PATCH	0x3
#define BIT_MASK_GPIO_DISABLE_MMU	0x4
#define BIT_MASK_GPIO_BOOT_SEL		0x5

/*
 * eFuse definitions
 */
#define REG_EFUSE_MODE			0x0
#define REG_EFUSE_ADR			0x4
#define REG_EFUSE_RD_DATA		0xc
#define EFUSE_NUM_ADDRESS_BITS	7
#define EFUSE_MAGIC_BEGIN		0x0b0b0b0b
#define EFUSE_MAGIC_END			0x0e0e0e0e
#define SPIFP_MAGIC_BEGIN		0x1b1b1b1b
#define SPIFP_MAGIC_END			0x1e1e1e1e
#define USERCONF_MAGIC_BEGIN	0x2b2b2b2b
#define USERCONF_MAGIC_END		0x2e2e2e2e

/*
 * USB registers.
 */
#define REG_OTG_CMD			0x00U
#define BIT_OTG_CMD_DEV_BUS_REQ		BIT(0)
#define REG_OTG_STS			0x04U
#define BIT_OTG_STS_DEV_RDY		BIT(27)
#endif /* __PLATFORM_DEF_H__ */
