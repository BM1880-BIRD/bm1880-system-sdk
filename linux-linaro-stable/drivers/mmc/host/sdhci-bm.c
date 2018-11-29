/*
 * drivers/mmc/host/sdhci-bm.c - BitMain SDHCI Platform driver
 *
 * Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/delay.h>
#include <linux/mmc/mmc.h>
#include <linux/slab.h>
#include <linux/reset.h>
#include "sdhci-pltfm.h"
#include "sdhci-bm.h"

#define DRIVER_NAME "bm"

#define BM_SDHCI_VENDOR_OFFSET		0x500
#define BM_SDHCI_VENDOR_MSHC_CTRL_R (BM_SDHCI_VENDOR_OFFSET + 0x8)
#define BM_SDHCI_VENDOR_A_CTRL_R (BM_SDHCI_VENDOR_OFFSET + 0x40)
#define BM_SDHCI_VENDOR_A_STAT_R (BM_SDHCI_VENDOR_OFFSET + 0x44)

static void bm_sdhci_set_tap(struct sdhci_host *host, unsigned int tap)
{
	sdhci_writel(host, 0x0, BM_SDHCI_VENDOR_MSHC_CTRL_R);
	sdhci_writel(host, 0x18, BM_SDHCI_VENDOR_A_CTRL_R);
	sdhci_writel(host, tap, BM_SDHCI_VENDOR_A_STAT_R);
}

static void bm_sdhci_bm1880_set_tap(struct sdhci_host *host, unsigned int tap)
{
	pr_debug("bm_sdhci_bm1880_set_tap %d\n", tap);

	sdhci_writel(host, tap, BM_SDHCI_VENDOR_A_STAT_R);
}

static void bm_sdhci_bm_1880_reset_after_tuning_pass(struct sdhci_host *host)
{
	pr_debug("tuning pass\n");

	/* Clear BUF_RD_READY intr */
	sdhci_writew(host, sdhci_readw(host, SDHCI_NORMAL_INT_STATUS) & (~(0x1<<5)),
		     SDHCI_NORMAL_INT_STATUS);

	/* Set SW_RST_R.SW_RST_DAT = 1 to clear buffered tuning block */
	sdhci_writeb(host, sdhci_readb(host, SW_RST_R) | (0x1<<2), SW_RST_R);

	/* Set SW_RST_R.SW_RST_CMD = 1	*/
	sdhci_writeb(host, sdhci_readb(host, SW_RST_R) | (0x1<<1), SW_RST_R);

	while (sdhci_readb(host, SW_RST_R) & 0x3)
	;
}

#define TAP_MAX_VALUE_BM1880 7
static int sdhci_bm1880_execute_tuning(struct sdhci_host *host, u32 opcode)
{
	unsigned int min, max;
	uint32_t reg = 0;

	sdhci_writel(host, 0x0, BM_SDHCI_VENDOR_MSHC_CTRL_R); // ??

	reg = sdhci_readw(host, SDHCI_ERR_INT_STATUS);
	pr_debug("%s : SDHCI_ERR_INT_STATUS 0x%x\n", mmc_hostname(host->mmc), reg);

	reg = sdhci_readw(host, SDHCI_HOST_CTRL2_R);
	pr_debug("%s : host ctrl2 0x%x\n", mmc_hostname(host->mmc), reg);
	/* Set Host_CTRL2_R.SAMPLE_CLK_SEL=0 */
	sdhci_writew(host, sdhci_readw(host, SDHCI_HOST_CTRL2_R) & (~(0x1<<7)), SDHCI_HOST_CTRL2_R);
	sdhci_writew(host, sdhci_readw(host, SDHCI_HOST_CTRL2_R) & (~(0x3<<4)), SDHCI_HOST_CTRL2_R);

	reg = sdhci_readw(host, SDHCI_HOST_CTRL2_R);
	pr_debug("%s : host ctrl2 0x%x\n", mmc_hostname(host->mmc), reg);

	/* Set ATR_CTRL_R.SW_TNE_EN=1 */
	reg = sdhci_readl(host, BM_SDHCI_VENDOR_A_CTRL_R);
	pr_debug("%s : A ctrl 0x%x\n", mmc_hostname(host->mmc), reg);
	sdhci_writel(host, sdhci_readl(host, BM_SDHCI_VENDOR_A_CTRL_R) | (0x1<<4), BM_SDHCI_VENDOR_A_CTRL_R);
	reg = sdhci_readl(host, BM_SDHCI_VENDOR_A_CTRL_R);
	pr_debug("%s : A ctrl 0x%x\n", mmc_hostname(host->mmc), reg);

	/*
	 * Start search for minimum tap value at 10, as smaller values are
	 * may wrongly be reported as working but fail at higher speeds,
	 * according to the TRM.
	 */
	min = 0;
	while (min <= TAP_MAX_VALUE_BM1880) {
		bm_sdhci_bm1880_set_tap(host, min);
		if (!mmc_send_tuning(host->mmc, opcode, NULL)) {
			pr_info("%s : min %d, pass\n", mmc_hostname(host->mmc), min);
			bm_sdhci_bm_1880_reset_after_tuning_pass(host);
			break;
		}

		min++;
	}

	WARN_ON(min > TAP_MAX_VALUE_BM1880);

	/* Find the maximum tap value that still passes. */
	max = min + 1;
	while (max <= TAP_MAX_VALUE_BM1880) {
		bm_sdhci_bm1880_set_tap(host, max);
		if (mmc_send_tuning(host->mmc, opcode, NULL)) {
			pr_info("%s : max %d, fail\n", mmc_hostname(host->mmc), max);
			max--;
			break;
		}

		bm_sdhci_bm_1880_reset_after_tuning_pass(host);
		max++;
	}

	if (max > TAP_MAX_VALUE_BM1880)
		max = TAP_MAX_VALUE_BM1880;

	pr_info("%s : max %d, min %d\n", mmc_hostname(host->mmc), max, min);

	/* The TRM states the ideal tap value is at center in the passing range. */
	bm_sdhci_bm1880_set_tap(host, min + ((max - min) / 2));

	pr_info("%s min:%d max:%d\n", mmc_hostname(host->mmc), min, max);

	return mmc_send_tuning(host->mmc, opcode, NULL);
}

static int sdhci_bm_execute_software_tuning(struct sdhci_host *host, u32 opcode)
{
	unsigned int min, max;

	/*
	 * Start search for minimum tap value at 10, as smaller values are
	 * may wrongly be reported as working but fail at higher speeds,
	 * according to the TRM.
	 */
	min = 0;
	while (min < 128) {
		bm_sdhci_set_tap(host, min);
		if (!mmc_send_tuning(host->mmc, opcode, NULL))
			break;
		min++;
	}

	/* Find the maximum tap value that still passes. */
	max = min + 1;
	while (max < 128) {
		bm_sdhci_set_tap(host, max);
		if (mmc_send_tuning(host->mmc, opcode, NULL)) {
			max--;
			break;
		}
		max++;
	}

	/* The TRM states the ideal tap value is at 75% in the passing range. */
	bm_sdhci_set_tap(host, min + ((max - min) * 3 / 4));

	return mmc_send_tuning(host->mmc, opcode, NULL);
}

static int sdhci_bm_select_drive_strength(struct sdhci_host *host,
		struct mmc_card *card, unsigned int max_dtr, int host_drv,
		int card_drv, int *drv_type)
{
	uint32_t reg;
	int driver_type;

	pr_info(" max_dtr %d, host_drv %d, card_drv %d, drv_type %d\n",
		max_dtr, host_drv, card_drv, *drv_type);

	driver_type = MMC_SET_DRIVER_TYPE_A;
	*drv_type = MMC_SET_DRIVER_TYPE_A;

	reg = (1 << PHY_CNFG_PHY_PWRGOOD) | (0xe << PHY_CNFG_PAD_SP) |
		(0xe << PHY_CNFG_PAD_SN) | (1 << PHY_CNFG_PHY_RSTN);
	sdhci_writel(host, reg, SDHCI_P_PHY_CNFG);

	return driver_type;
}

static void sdhci_bm_set_uhs_signaling(struct sdhci_host *host, unsigned int uhs)
{
	struct mmc_host *mmc = host->mmc;
	u16 ctrl_2;

	ctrl_2 = sdhci_readw(host, SDHCI_HOST_CONTROL2);
	/* Select Bus Speed Mode for host */
	ctrl_2 &= ~SDHCI_CTRL_UHS_MASK;
	switch (uhs) {
	case MMC_TIMING_UHS_SDR12:
		ctrl_2 |= SDHCI_CTRL_UHS_SDR12;
		break;
	case MMC_TIMING_UHS_SDR25:
		ctrl_2 |= SDHCI_CTRL_UHS_SDR25;
		break;
	case MMC_TIMING_UHS_SDR50:
		ctrl_2 |= SDHCI_CTRL_UHS_SDR50;
		break;
	case MMC_TIMING_MMC_HS200:
	case MMC_TIMING_UHS_SDR104:
		ctrl_2 |= SDHCI_CTRL_UHS_SDR104;
		break;
	case MMC_TIMING_UHS_DDR50:
	case MMC_TIMING_MMC_DDR52:
		ctrl_2 |= SDHCI_CTRL_UHS_DDR50;
		break;
	}

	/*
	 * When clock frequency is less than 100MHz, the feedback clock must be
	 * provided and DLL must not be used so that tuning can be skipped. To
	 * provide feedback clock, the mode selection can be any value less
	 * than 3'b011 in bits [2:0] of HOST CONTROL2 register.
	 */
	if (host->clock <= 100000000 &&
	    (uhs == MMC_TIMING_MMC_HS400 ||
	     uhs == MMC_TIMING_MMC_HS200 ||
	     uhs == MMC_TIMING_UHS_SDR104))
		ctrl_2 &= ~SDHCI_CTRL_UHS_MASK;

	dev_dbg(mmc_dev(mmc), "%s: clock=%u uhs=%u ctrl_2=0x%x\n",
		mmc_hostname(host->mmc), host->clock, uhs, ctrl_2);
	sdhci_writew(host, ctrl_2, SDHCI_HOST_CONTROL2);
}

static unsigned int bm_sdhci_get_min_clock(struct sdhci_host *host)
{
	return 200 * 1000;
}

static unsigned int bm_sdhci_get_max_clock(struct sdhci_host *host)
{
	return 50 * 1000 * 1000;
}

static unsigned int bm1880_sdhci_get_emmc_max_clock(struct sdhci_host *host)
{
	uint32_t clk = 187 * 1000 * 1000;

	pr_debug(DRIVER_NAME ":%s : %d\n", __func__, clk);
	return clk;
}

static unsigned int bm1880_sdhci_get_sd_max_clock(struct sdhci_host *host)
{
	return 100 * 1000 * 1000;
}

static void bm_sdhci_hw_reset(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host;
	struct sdhci_bm_host *bm_host;

	pltfm_host = sdhci_priv(host);
	bm_host = sdhci_pltfm_priv(pltfm_host);

	reset_control_assert(bm_host->reset);
	udelay(10);
	reset_control_deassert(bm_host->reset);
}

void bm_sdhci_reset(struct sdhci_host *host, u8 mask)
{
	sdhci_reset(host, mask);
	if (mask & SDHCI_RESET_ALL)
		bm_sdhci_phy_init(host);
}

void bm1880_sd_reset(struct sdhci_host *host, u8 mask)
{
	sdhci_reset(host, mask);

	if (mask & SDHCI_RESET_ALL) {

		// revert tx
		sdhci_writeb(host, 0x10, SDHCI_P_SDCLKDL_DC);
		// revert rx
		bm_sdhci_set_tap(host, 0x00);
	}
}

void bm1880_emmc_reset(struct sdhci_host *host, u8 mask)
{
	sdhci_reset(host, mask);
	if (mask & SDHCI_RESET_ALL) {
		// revert tx
		sdhci_writeb(host, 0x10, SDHCI_P_SDCLKDL_DC);
		// revert rx
		bm_sdhci_set_tap(host, 0x00);
	}
}

void bm_pldm_sdhci_reset(struct sdhci_host *host, u8 mask)
{
	bm_sdhci_hw_reset(host);
	sdhci_reset(host, mask);

	if (mask & SDHCI_RESET_ALL)
		bm_sdhci_phy_init(host);
}

int bm_sdhci_phy_init(struct sdhci_host *host)
{
	// Asset reset of phy
	sdhci_writel(host, sdhci_readl(host, SDHCI_P_PHY_CNFG) & ~(1 << PHY_CNFG_PHY_RSTN), SDHCI_P_PHY_CNFG);

	// Set PAD_SN PAD_SP
	sdhci_writel(host, (1 << PHY_CNFG_PHY_PWRGOOD) | (0x9 << PHY_CNFG_PAD_SP) | (0x8 << PHY_CNFG_PAD_SN), SDHCI_P_PHY_CNFG);

	// Set CMDPAD
	sdhci_writew(host, (0x2 << PAD_CNFG_RXSEL) | (1 << PAD_CNFG_WEAKPULL_EN) |
			(0x3 << PAD_CNFG_TXSLEW_CTRL_P) | (0x2 << PAD_CNFG_TXSLEW_CTRL_N), SDHCI_P_CMDPAD_CNFG);

	// Set DATAPAD
	sdhci_writew(host, (0x2 << PAD_CNFG_RXSEL) | (1 << PAD_CNFG_WEAKPULL_EN) |
			(0x3 << PAD_CNFG_TXSLEW_CTRL_P) | (0x2 << PAD_CNFG_TXSLEW_CTRL_N), SDHCI_P_DATPAD_CNFG);

	// Set CLKPAD
	sdhci_writew(host, (0x2 << PAD_CNFG_RXSEL) | (0x3 << PAD_CNFG_TXSLEW_CTRL_P) | (0x2 << PAD_CNFG_TXSLEW_CTRL_N), SDHCI_P_CLKPAD_CNFG);

	// Set STB_PAD
	sdhci_writew(host, (0x2 << PAD_CNFG_RXSEL) | (0x2 << PAD_CNFG_WEAKPULL_EN) |
			(0x3 << PAD_CNFG_TXSLEW_CTRL_P) | (0x2 << PAD_CNFG_TXSLEW_CTRL_N), SDHCI_P_STBPAD_CNFG);

	// Set RSTPAD
	sdhci_writew(host, (0x2 << PAD_CNFG_RXSEL) | (1 << PAD_CNFG_WEAKPULL_EN) |
			(0x3 << PAD_CNFG_TXSLEW_CTRL_P) | (0x2 << PAD_CNFG_TXSLEW_CTRL_N), SDHCI_P_RSTNPAD_CNFG);

	// Set SDCLKDL_CNFG, EXTDLY_EN = 1, fix delay
	sdhci_writeb(host, (1 << SDCLKDL_CNFG_EXTDLY_EN), SDHCI_P_SDCLKDL_CNFG);

	// Add 10 * 70ps = 0.7ns for output delay
	sdhci_writeb(host, 10, SDHCI_P_SDCLKDL_DC);

	//if (host->index == 1) {
	//	 Set SMPLDL_CNFG, Bypass
	sdhci_writeb(host, (1 << SMPLDL_CNFG_BYPASS_EN), SDHCI_P_SMPLDL_CNFG);
	//}
	//else {
	//	Set SMPLDL_CNFG, INPSEL_CNFG = 0x2
	//sdhci_writeb(host, (0x2 << SMPLDL_CNFG_INPSEL_CNFG), SDHCI_P_SMPLDL_CNFG);
	//}

	// Set ATDL_CNFG, tuning clk not use for init
	sdhci_writeb(host, (2 << ATDL_CNFG_INPSEL_CNFG), SDHCI_P_ATDL_CNFG);

	// deasset reset of phy
	sdhci_writel(host, sdhci_readl(host, SDHCI_P_PHY_CNFG) | (1 << PHY_CNFG_PHY_RSTN), SDHCI_P_PHY_CNFG);

	return 0;
}

void bm1880_sdhci_set_clock(struct sdhci_host *host, unsigned int clock)
{
	sdhci_set_clock(host, clock);

	if (clock == 0)
		// forward tx
		sdhci_writeb(host, 0x0, SDHCI_P_SDCLKDL_DC);
	else
		// revert tx
		sdhci_writeb(host, 0x10, SDHCI_P_SDCLKDL_DC);
}

/* ------------- bm palludium sdcard --------------- */
static const struct sdhci_ops sdhci_bm_pldm_sd_ops= {
	.reset = bm_pldm_sdhci_reset,
	.hw_reset = bm_sdhci_hw_reset,
	.set_clock = bm1880_sdhci_set_clock,
	.set_bus_width = sdhci_set_bus_width,
	.set_uhs_signaling = sdhci_bm_set_uhs_signaling,
	.get_max_clock = bm_sdhci_get_max_clock,
	.get_min_clock = bm_sdhci_get_min_clock,
};

static const struct sdhci_pltfm_data sdhci_bm_pldm_sd_pdata = {
	.ops = &sdhci_bm_pldm_sd_ops,
	.quirks = SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN | SDHCI_QUIRK_INVERTED_WRITE_PROTECT,
	.quirks2 = SDHCI_QUIRK2_NO_1_8_V,
};

#define BM1880_MS_CONTROL_REG_0	(0x50010400)
#define BM1880_MS_CONTROL_GROUP0_V1_8	(0x1)
/* TODO: ioremap way need repaired */
void bm1880_emmc_voltage_switch(struct sdhci_host *host)
{
	void __iomem *ioaddr;

	/* https://info.bitmain.vip:8443/display/BM1880/Boot+Mode?focusedCommentId=31204319#comment-31204319 */
	ioaddr = ioremap_nocache(BM1880_MS_CONTROL_REG_0, 4);
	writel_relaxed(readl_relaxed(ioaddr) | BM1880_MS_CONTROL_GROUP0_V1_8, ioaddr);
	iounmap(ioaddr);
	usleep_range(1000, 1500);
}

/* TODO: need repaired */
void bm1880_sd_voltage_switch(struct sdhci_host *host)
{

}

int bm_platform_execute_tuning(struct sdhci_host *host, u32 opcode)
{
	u16 ctrl;
	int tuning_loop_counter = 0;
	int err = 0;
	unsigned long flags;
	unsigned int tuning_count = 0;
	bool hs400_tuning;
	int hit = 0;

	pr_info(DRIVER_NAME ":%s\n", __func__);
	spin_lock_irqsave(&host->lock, flags);

	hs400_tuning = host->flags & SDHCI_HS400_TUNING;
	host->flags &= ~SDHCI_HS400_TUNING;

	if (host->tuning_mode == SDHCI_TUNING_MODE_1)
		tuning_count = host->tuning_count;

	switch (host->timing) {
	/* HS400 tuning is done in HS200 mode */
	case MMC_TIMING_MMC_HS400:
		err = -EINVAL;
		goto out_unlock;

	case MMC_TIMING_MMC_HS200:
		/*
		 * Periodic re-tuning for HS400 is not expected to be needed, so
		 * disable it here.
		 */
		if (hs400_tuning)
			tuning_count = 0;
		break;

	case MMC_TIMING_UHS_SDR104:
	case MMC_TIMING_UHS_DDR50:
		break;

	case MMC_TIMING_UHS_SDR50:
		if (host->flags & SDHCI_SDR50_NEEDS_TUNING)
			break;
		/* FALLTHROUGH */

	default:
		goto out_unlock;
	}

	ctrl = sdhci_readw(host, SDHCI_HOST_CONTROL2);
	ctrl |= SDHCI_CTRL_EXEC_TUNING;

	sdhci_writel(host, SDHCI_INT_DATA_AVAIL, SDHCI_INT_ENABLE);
	sdhci_writel(host, SDHCI_INT_DATA_AVAIL, SDHCI_SIGNAL_ENABLE);

	sdhci_writew(host, 0x704b | (0x3<<4) | (0x1<<3), SDHCI_HOST_CONTROL2);/*drv_strength | 1.8v*/

	sdhci_writel(host, 0, SDHCI_DMA_ADDRESS);/*sdmasa*/
	sdhci_writel(host, 0, SDHCI_MSHC_CTRL);

	sdhci_writel(host, 0x18, SDHCI_AT_CTRL);

	sdhci_writew(host, 0x0, SDHCI_BLOCK_COUNT);
	sdhci_writew(host, 0x1040, SDHCI_BLOCK_SIZE);
	sdhci_writew(host, SDHCI_TRNS_READ, SDHCI_TRANSFER_MODE);

	do {
		struct mmc_command cmd = {0};
		struct mmc_request mrq = {NULL};

		cmd.opcode = opcode;
		cmd.arg = 0;
		cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;
		cmd.retries = 0;
		cmd.data = NULL;
		cmd.mrq = &mrq;
		cmd.error = 0;

		sdhci_writel(host, tuning_loop_counter, SDHCI_AT_STAT);
		mrq.cmd = &cmd;
		sdhci_send_command(host, &cmd);

		host->cmd = NULL;
		sdhci_del_timer(host, &mrq);
		spin_unlock_irqrestore(&host->lock, flags);

		/* Wait for Buffer Read Ready interrupt */
		wait_event_timeout(host->buf_ready_int,
					(host->tuning_done == 1),
					msecs_to_jiffies(10));

		spin_lock_irqsave(&host->lock, flags);
		if (host->tuning_done == 1) {
			u16 stat;

			stat = sdhci_readw(host, SDHCI_ERR_INT_STATUS) & 0x3F;
			if (stat == 0)
				hit = tuning_loop_counter;
		}

		host->tuning_done = 0;
		tuning_loop_counter++;
		sdhci_writeb(host, 0xFF, SDHCI_INT_STATUS);
		sdhci_writeb(host, 0xFF, SDHCI_ERR_INT_STATUS);
		sdhci_writeb(host, SDHCI_RESET_CMD | SDHCI_RESET_DATA, SDHCI_SOFTWARE_RESET);
	} while (tuning_loop_counter < MAX_TUNING_STEP);

	if (tuning_loop_counter >= MAX_TUNING_STEP) {
		ctrl &= ~(SDHCI_CTRL_TUNED_CLK | SDHCI_CTRL_EXEC_TUNING);
		sdhci_writew(host, ctrl, SDHCI_HOST_CONTROL2);
	}

	sdhci_writel(host, 0, SDHCI_AT_CTRL);
	sdhci_writeb(host, 0xFF, SDHCI_INT_STATUS);/*clear normal int*/
	sdhci_writeb(host, 0xFF, SDHCI_ERR_INT_STATUS);/*clear error int*/
	sdhci_writel(host, sdhci_readl(host, SDHCI_AT_CTRL) | (0x1<<4), SDHCI_AT_CTRL);/*en sw_tuning_en bit*/
	sdhci_writel(host, (sdhci_readl(host, SDHCI_AT_STAT) & (~0xFF)) | hit, SDHCI_AT_STAT);/*center_ph_code*/
	sdhci_writel(host, sdhci_readl(host, SDHCI_AT_CTRL) & (~(0x1<<4)), SDHCI_AT_CTRL);/*dis sw_tuning_en bit*/
	sdhci_writeb(host, SDHCI_RESET_CMD | SDHCI_RESET_DATA, SDHCI_SOFTWARE_RESET);

	if (tuning_count)
		err = 0;

	host->mmc->retune_period = err ? 0 : tuning_count;

	sdhci_writel(host, host->ier, SDHCI_INT_ENABLE);
	sdhci_writel(host, host->ier, SDHCI_SIGNAL_ENABLE);
out_unlock:
	spin_unlock_irqrestore(&host->lock, flags);
	return err;
}

/* ------------- bm palludium emmc --------------- */
static const struct sdhci_ops sdhci_bm_pldm_emmc_ops= {
	.reset = sdhci_reset,
	.set_clock = sdhci_set_clock,
	.set_bus_width = sdhci_set_bus_width,
	.set_uhs_signaling = sdhci_bm_set_uhs_signaling,
	.get_max_clock = bm_sdhci_get_max_clock,
	.get_min_clock = bm_sdhci_get_min_clock,
	.platform_execute_tuning = bm_platform_execute_tuning,
};

static const struct sdhci_pltfm_data sdhci_bm_pldm_emmc_pdata = {
	.ops = &sdhci_bm_pldm_emmc_ops,
	.quirks = SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN,
};

/* ------------ bm asic ------------ */
static const struct sdhci_ops sdhci_bm_ops= {
	.reset = bm_sdhci_reset,
	.set_clock = sdhci_set_clock,
	.set_bus_width = sdhci_set_bus_width,
	.set_uhs_signaling = sdhci_bm_set_uhs_signaling,
	.platform_execute_tuning = sdhci_bm_execute_software_tuning,
	.select_drive_strength = sdhci_bm_select_drive_strength,
};

static const struct sdhci_pltfm_data sdhci_bm_emmc_pdata = {
	.ops = &sdhci_bm_ops,
	.quirks = SDHCI_QUIRK_INVERTED_WRITE_PROTECT,
#if defined(CONFIG_ARCH_BM1682_BOX) || defined(CONFIG_ARCH_BM1682_HDS)
	.quirks2 = SDHCI_QUIRK2_PRESET_VALUE_BROKEN | SDHCI_QUIRK2_NO_1_8_V,
#else
	.quirks2 = SDHCI_QUIRK2_PRESET_VALUE_BROKEN,
#endif
};

static const struct sdhci_pltfm_data sdhci_bm_sd_pdata = {
	.ops = &sdhci_bm_ops,
	.quirks = SDHCI_QUIRK_INVERTED_WRITE_PROTECT,
	.quirks2 = SDHCI_QUIRK2_PRESET_VALUE_BROKEN | SDHCI_QUIRK2_NO_1_8_V,
};

static const struct sdhci_ops sdhci_bm1880_emmc_ops = {
	.reset = bm1880_emmc_reset,
	.set_clock = sdhci_set_clock,
	.set_bus_width = sdhci_set_bus_width,
	.get_max_clock = bm1880_sdhci_get_emmc_max_clock,
	.voltage_switch = bm1880_emmc_voltage_switch,
	.set_uhs_signaling = sdhci_bm_set_uhs_signaling,
	.platform_execute_tuning = sdhci_bm1880_execute_tuning,
	.select_drive_strength = sdhci_bm_select_drive_strength,
};

static const struct sdhci_ops sdhci_bm1880_sd_ops = {
	.reset = bm1880_sd_reset,
	.set_clock = sdhci_set_clock,
	.set_bus_width = sdhci_set_bus_width,
	.get_max_clock = bm1880_sdhci_get_sd_max_clock,
	.voltage_switch = bm1880_sd_voltage_switch,
	.set_uhs_signaling = sdhci_bm_set_uhs_signaling,
	.platform_execute_tuning = sdhci_bm1880_execute_tuning,
	.select_drive_strength = sdhci_bm_select_drive_strength,
};

static const struct sdhci_pltfm_data sdhci_bm1880_emmc_pdata = {
	.ops = &sdhci_bm1880_emmc_ops,
	.quirks = SDHCI_QUIRK_INVERTED_WRITE_PROTECT | SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN,
	.quirks2 = SDHCI_QUIRK2_PRESET_VALUE_BROKEN | SDHCI_QUIRK2_SUPPORT_DISABLE_CLK,
};

static const struct sdhci_pltfm_data sdhci_bm1880_sd_pdata = {
	.ops = &sdhci_bm1880_sd_ops,
	.quirks = SDHCI_QUIRK_INVERTED_WRITE_PROTECT | SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN,
	.quirks2 = SDHCI_QUIRK2_PRESET_VALUE_BROKEN | SDHCI_QUIRK2_NO_1_8_V,
};

static const struct of_device_id sdhci_bm_dt_match[] = {
	{.compatible = "bitmain,bm-pldm-sdcard", .data = &sdhci_bm_pldm_sd_pdata},
	{.compatible = "bitmain,bm-pldm-emmc", .data = &sdhci_bm_pldm_emmc_pdata},
	{.compatible = "bitmain,bm-emmc", .data = &sdhci_bm_emmc_pdata},
	{.compatible = "bitmain,bm-sd", .data = &sdhci_bm_sd_pdata},
	{.compatible = "bitmain,bm1880-emmc", .data = &sdhci_bm1880_emmc_pdata},
	{.compatible = "bitmain,bm1880-sd", .data = &sdhci_bm1880_sd_pdata},
	{ /* sentinel */ }
};

static void disable_mmc_clk(struct sdhci_bm_host *bm_host)
{
	struct mmc_host *mmc = bm_host->mmc;

	pr_warn("%s: disable_mmc_clk\n", mmc_hostname(mmc));
	reset_control_assert(bm_host->clk_rst_axi_emmc_ctrl);
	reset_control_assert(bm_host->clk_rst_emmc_ctrl);
	reset_control_assert(bm_host->clk_rst_100k_emmc_ctrl);
}

static int check_mmc_device_presence(struct sdhci_bm_host *bm_host)
{
	u32 pstate_reg = 0;
	struct mmc_host *mmc = bm_host->mmc;
	int ret = 0;

	pr_warn("%s: Check_mmc_device_presence:\n", mmc_hostname(mmc));

	pstate_reg = readl_relaxed(bm_host->core_mem + SDHCI_PRESENT_STATE);

	if (!(pstate_reg & SDHCI_CARD_PRESENT)) {
		// card not detected
		pr_warn("%s: device not present\n", mmc_hostname(mmc));
		disable_mmc_clk(bm_host);
		ret = -1;
	} else {
		pr_warn("%s: device is present\n", mmc_hostname(mmc));
		ret = 0;
	}

	return ret;
}

static int get_emmc_clk_control(struct sdhci_bm_host *bm_host)
{
	int ret;
	struct mmc_host *mmc = bm_host->mmc;

	pr_warn("%s: get_emmc_clk_control\n", mmc_hostname(mmc));

	bm_host->clk_rst_axi_emmc_ctrl = devm_reset_control_get(&bm_host->pdev->dev, "axi_emmc");
	if (IS_ERR(bm_host->clk_rst_axi_emmc_ctrl)) {
		ret = PTR_ERR(bm_host->clk_rst_axi_emmc_ctrl);
		dev_err(&bm_host->pdev->dev, "failed to retrieve axi_emmc clk reset");
		return ret;
	}

	bm_host->clk_rst_emmc_ctrl = devm_reset_control_get(&bm_host->pdev->dev, "emmc");
	if (IS_ERR(bm_host->clk_rst_emmc_ctrl)) {
		ret = PTR_ERR(bm_host->clk_rst_emmc_ctrl);
		dev_err(&bm_host->pdev->dev, "failed to retrieve emmc clk reset");
		return ret;
	}

	bm_host->clk_rst_100k_emmc_ctrl = devm_reset_control_get(&bm_host->pdev->dev, "100k_emmc");
	if (IS_ERR(bm_host->clk_rst_100k_emmc_ctrl)) {
		ret = PTR_ERR(bm_host->clk_rst_100k_emmc_ctrl);
		dev_err(&bm_host->pdev->dev, "failed to retrieve 100k_emmc clk reset");
		return ret;
	}

	return 0;
}

MODULE_DEVICE_TABLE(of, sdhci_bm_dt_match);

static int sdhci_bm_probe(struct platform_device *pdev)
{
	struct sdhci_host *host;
	struct sdhci_pltfm_host *pltfm_host;
	struct sdhci_bm_host *bm_host;
	const struct of_device_id *match;
	const struct sdhci_pltfm_data *pdata;
	int ret;

	pr_info(DRIVER_NAME ":%s\n", __func__);

	match = of_match_device(sdhci_bm_dt_match, &pdev->dev);
	if (!match)
		return -EINVAL;

	pdata = match->data;

	host = sdhci_pltfm_init(pdev, pdata, sizeof(*bm_host));
	if (IS_ERR(host))
		return PTR_ERR(host);

	pltfm_host = sdhci_priv(host);
	bm_host = sdhci_pltfm_priv(pltfm_host);
	bm_host->mmc = host->mmc;
	bm_host->pdev = pdev;
	bm_host->core_mem = host->ioaddr;

	ret = mmc_of_parse(host->mmc);
	if (ret)
		goto pltfm_free;

	sdhci_get_of_property(pdev);

	if (host->quirks2 & SDHCI_QUIRK2_SUPPORT_DISABLE_CLK &&
	    !(host->quirks & SDHCI_QUIRK_BROKEN_CARD_DETECTION)
	) {
		get_emmc_clk_control(bm_host);
		ret = check_mmc_device_presence(bm_host);

		if (ret)
			goto err_add_host;
	}

	if (pdata->ops->hw_reset) {
		bm_host->reset = devm_reset_control_get(&pdev->dev, "sdio");
		if (IS_ERR(bm_host->reset)) {
			ret = PTR_ERR(bm_host->reset);
			goto pltfm_free;
		}
	}


	ret = sdhci_add_host(host);
	if (ret)
		goto err_add_host;

	return 0;

err_add_host:
pltfm_free:
	sdhci_pltfm_free(pdev);
	return ret;
}

static int sdhci_bm_remove(struct platform_device *pdev)
{
	struct sdhci_host *host = platform_get_drvdata(pdev);
	int dead = (readl_relaxed(host->ioaddr + SDHCI_INT_STATUS) == 0xffffffff);

	sdhci_remove_host(host, dead);
	sdhci_pltfm_free(pdev);
	return 0;
}

static struct platform_driver sdhci_bm_driver = {
	.probe = sdhci_bm_probe,
	.remove = sdhci_bm_remove,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = sdhci_bm_dt_match,
	},
};

module_platform_driver(sdhci_bm_driver);
MODULE_DESCRIPTION("BitMain Secure Digital Host Controller Interface driver");
MODULE_LICENSE("GPL v2");
