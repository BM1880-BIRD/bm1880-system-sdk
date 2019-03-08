/*
 * Copyright (C) 2016 Socionext Inc.
 *   Author: Masahiro Yamada <yamada.masahiro@socionext.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/sizes.h>
#include <libfdt.h>
#include <reset.h>
#include <mmc.h>
#include <sdhci.h>

#ifdef DEBUG
#define pr_debug(fmt, ...) \
	printf(fmt, ##__VA_ARGS__)
#else
#define pr_debug(fmt, ...)
#endif


struct bm_sdhci_plat {
	struct mmc_config cfg;
	struct mmc mmc;
};

struct bm_sdhci_host {
	struct sdhci_host host;
	int has_phy;
	int is_64_addressing;
	uint32_t mmc_init_freq;
	uint32_t mmc_trans_freq;
	struct reset_ctl reset_ctl;
};

static int bm_ofdata_to_platdata(struct udevice *dev)
{
	struct bm_sdhci_host *bm_host = dev_get_priv(dev);
	struct sdhci_host *host = &bm_host->host;
	int node = dev_of_offset(dev);

	host->name = strdup(dev->name);
	host->ioaddr = (void *)devfdt_get_addr(dev);
	host->bus_width = fdtdec_get_int(gd->fdt_blob, node, "bus-width", 4);
	host->index = fdtdec_get_uint(gd->fdt_blob, node, "index", 0);
	host->max_clk = fdtdec_get_uint(gd->fdt_blob, node, "max-frequency", 0);
	bm_host->mmc_init_freq = fdtdec_get_uint(gd->fdt_blob, node, "mmc_init_freq", 200000);
	bm_host->mmc_trans_freq = fdtdec_get_uint(gd->fdt_blob, node, "mmc_trans_freq", 0);
	bm_host->is_64_addressing = fdtdec_get_bool(gd->fdt_blob, node, "64_addressing");
	bm_host->has_phy = fdtdec_get_bool(gd->fdt_blob, node, "has_phy");

	if (host->ioaddr == (void *)FDT_ADDR_T_NONE)
		return -EINVAL;

	return 0;
}

static int bm_sdhci_bind(struct udevice *dev)
{
	struct bm_sdhci_plat *plat = dev_get_platdata(dev);

	pr_debug("[hq] %s\n", __func__);
	return sdhci_bind(dev, &plat->mmc, &plat->cfg);
}

#ifndef CONFIG_TARGET_BITMAIN_BM1880
/* TODO */
static int bm_sdhci_phy_init(struct bm_sdhci_host *bm_host)
{
	struct sdhci_host *host = &bm_host->host;

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

	if (host->index == 1) {
		// Set SMPLDL_CNFG, Bypass
		sdhci_writeb(host, (1 << SMPLDL_CNFG_BYPASS_EN), SDHCI_P_SMPLDL_CNFG);
	}
	else {
		// Set SMPLDL_CNFG, INPSEL_CNFG = 0x2
		sdhci_writeb(host, (0x2 << SMPLDL_CNFG_INPSEL_CNFG), SDHCI_P_SMPLDL_CNFG);
	}

	// Set ATDL_CNFG, tuning clk not use for init
	sdhci_writeb(host, (2 << ATDL_CNFG_INPSEL_CNFG), SDHCI_P_ATDL_CNFG);

	// Deasset reset of phy
	sdhci_writel(host, sdhci_readl(host, SDHCI_P_PHY_CNFG) | (1 << PHY_CNFG_PHY_RSTN), SDHCI_P_PHY_CNFG);

	return 0;
}
#else
static int bm_sdhci_phy_init(struct bm_sdhci_host *bm_host)
{
	struct sdhci_host *host = &bm_host->host;

	// revert rx
	sdhci_writeb(host, 0, 0x508);
	sdhci_writeb(host, 0x18, 0x540);
	if (host->index == 1) {
		sdhci_writeb(host, 0x00, 0x544);
		// revert tx
		sdhci_writeb(host, 0x10, 0x31e);
	}
	else {
		sdhci_writeb(host, 0x00, 0x544);
		// revert tx
		sdhci_writeb(host, 0x10, 0x31e);
	}

	return 0;
}

#endif

int bm_get_cd(struct sdhci_host *host)
{
	u32 reg;

	reg = sdhci_readl(host, SDHCI_PRESENT_STATE);

	if (reg & SDHCI_CARD_PRESENT) {
		sdhci_writeb(host, sdhci_readb(host, SDHCI_POWER_CONTROL) | SDHCI_POWER_ON,
				SDHCI_POWER_CONTROL);
		return 1;
	} else {
		return 0;
	}
}

static const struct sdhci_ops bm_sdhci_ops = {
	.get_cd	= bm_get_cd,
};

static int bm_sdhci_probe(struct udevice *dev)
{
	struct mmc_uclass_priv *upriv = dev_get_uclass_priv(dev);
	struct bm_sdhci_plat *plat = dev_get_platdata(dev);
	struct bm_sdhci_host *bm_host = dev_get_priv(dev);
	struct sdhci_host *host = &bm_host->host;
	int ret;

	pr_debug("[hq] %s\n", __func__);

	ret = reset_get_by_name(dev, "sdhci", &bm_host->reset_ctl);
	if (ret) {
		debug("warning: reset_get_by_name failed\n");
	} else {
		// Try to solve 1.8 to 3.3v converter HW issue
		ret = reset_assert(&bm_host->reset_ctl);
		if (ret) {
			printf("%s failed assert reset\n", __func__);
			return ret;
		}

		ret = reset_deassert(&bm_host->reset_ctl);
		if (ret) {
			printf("%s failed deassert reset\n", __func__);
			return ret;
		}
	}

	ret = sdhci_setup_cfg(&plat->cfg, host, bm_host->mmc_trans_freq, bm_host->mmc_init_freq);

	if (ret)
		return ret;

	upriv->mmc = &plat->mmc;
	host->mmc = &plat->mmc;
	host->mmc->priv = host;
	if (host->index == 1) {
		host->ops = &bm_sdhci_ops;
	}

	ret = sdhci_probe(dev);

	if (bm_host->has_phy) {
		bm_sdhci_phy_init(bm_host);
	}

	if (bm_host->is_64_addressing) {
		sdhci_writew(host, sdhci_readw(host, SDHCI_HOST_CONTROL2)
				| SDHCI_HOST_VER4_ENABLE | SDHCI_HOST_ADDRESSING,
				SDHCI_HOST_CONTROL2);
	}

	sdhci_writew(host, sdhci_readw(host, sdhci_readw(host, SDHCI_VENDOR_SPECIFIC_AREA) +
			SDHCI_EMMC_CTRL_R) | 0x1<<0,
			sdhci_readw(host, SDHCI_VENDOR_SPECIFIC_AREA) + SDHCI_EMMC_CTRL_R);
	return ret;
}

static const struct udevice_id bm_sdhci_match[] = {
	{ .compatible = "bitmain,synopsys-sdhc" },
	{ /* sentinel */ }
};

U_BOOT_DRIVER(bm_sdhci) = {
	.name = "bm_sdhci",
	.id = UCLASS_MMC,
	.of_match = bm_sdhci_match,
	.ofdata_to_platdata = bm_ofdata_to_platdata,
	.bind = bm_sdhci_bind,
	.probe = bm_sdhci_probe,
	.priv_auto_alloc_size = sizeof(struct bm_sdhci_host),
	.platdata_auto_alloc_size = sizeof(struct bm_sdhci_plat),
	.ops = &sdhci_ops,
};
