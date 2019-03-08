/*
 * dwmac-bitmain.c - Bitmain DWMAC specific glue layer
 *
 * Copyright (c) 2018 Bitmain Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/stmmac.h>
#include <linux/module.h>
#include <linux/phy.h>
#include <linux/platform_device.h>
#include <linux/of_net.h>
#include <linux/of_gpio.h>
#include <linux/io.h>

#include "stmmac_platform.h"

static u64 bm_dma_mask = DMA_BIT_MASK(40);

static int bm_eth_reset_phy(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	int phy_reset_gpio;

	if (!np)
		return 0;

	phy_reset_gpio = of_get_named_gpio(np, "phy-reset-gpios", 0);

	if (phy_reset_gpio < 0)
		return 0;

	if (gpio_request(phy_reset_gpio, "eth-phy-reset"))
		return 0;

	/* RESET_PU */
	gpio_direction_output(phy_reset_gpio, 0);
	mdelay(50);

	gpio_direction_output(phy_reset_gpio, 1);
	/* RC charging time */
	mdelay(6);

	return 0;
}

static int bm_dwmac_probe(struct platform_device *pdev)
{
	struct plat_stmmacenet_data *plat_dat;
	struct stmmac_resources stmmac_res;
	int ret;

	pdev->dev.dma_mask = &bm_dma_mask;
	pdev->dev.coherent_dma_mask = bm_dma_mask;

	bm_eth_reset_phy(pdev);

	ret = stmmac_get_platform_resources(pdev, &stmmac_res);
	if (ret)
		return ret;

	plat_dat = stmmac_probe_config_dt(pdev, &stmmac_res.mac);
	if (IS_ERR(plat_dat))
		return PTR_ERR(plat_dat);

	ret = stmmac_dvr_probe(&pdev->dev, plat_dat, &stmmac_res);
	if (ret)
		goto err_remove_config_dt;

	return 0;

err_remove_config_dt:
	stmmac_remove_config_dt(pdev, plat_dat);

	return ret;
}

static const struct of_device_id bm_dwmac_match[] = {
	{ .compatible = "bitmain,ethernet" },
	{ }
};
MODULE_DEVICE_TABLE(of, bm_dwmac_match);

static struct platform_driver bm_dwmac_driver = {
	.probe  = bm_dwmac_probe,
	.remove = stmmac_pltfr_remove,
	.driver = {
		.name           = "bm-dwmac",
		.pm		= &stmmac_pltfr_pm_ops,
		.of_match_table = bm_dwmac_match,
	},
};
module_platform_driver(bm_dwmac_driver);

MODULE_AUTHOR("Wei Huang<wei.huang01@bitmain.com>");
MODULE_DESCRIPTION("Bitmain DWMAC specific glue layer");
MODULE_LICENSE("GPL");
