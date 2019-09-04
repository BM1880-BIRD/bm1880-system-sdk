/*
 * OMAP USB HOST xHCI Controller
 *
 * (C) Copyright 2013
 * Texas Instruments, <www.ti.com>
 *
 * Author: Dan Murphy <dmurphy@ti.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <usb.h>
#include <errno.h>

#include <dm.h>
#include <fdtdec.h>
#include <libfdt.h>
#include <asm/types.h>

#include <asm/arch-armv8/mmio.h>
#include "xhci.h"
#include "xhci-bitmain.h"

extern int onboard_hub_reset(void);

/* Declare global data pointer */

struct bm_xhci_platdata {
	fdt_addr_t hcd_base;
};

static struct bm_xhci bitmain;

static int bitmain_core_init(struct bm_xhci *cdns)
{
	u32	reg;

	reg = xhci_readl(&cdns->regs->OTGVERSION);

	if ((OTGVERSION_TYPE__OTGVERSION__READ(reg)) != OTG_HW_VERSION) {
		printf("this is not a Bitmain USB3 OTG Core\n");
		return -ENODEV;
	}
	cdns->otg_version = OTGVERSION_TYPE__OTGVERSION__READ(reg);

	reg = xhci_readl(&cdns->regs->OTGSTS);
	if (OTGSTS_TYPE__OTG_NRDY__READ(reg) != 0) {
		printf("Bitmain USB3 OTG device not ready\n");
		return -ENODEV;
	}

	printf("%s otg_version:0x%x\n", __func__, cdns->otg_version);

	return 0;
}

void cdns_set_mode(struct bm_xhci *cdns, u32 mode)
{
	u32 reg;

	switch (mode) {
	case CDNS_ROLE_GADGET:
		xhci_writel(&cdns->regs->OTGCMD,
			    OTGCMD_TYPE__DEV_BUS_REQ__MASK
			    | OTGCMD_TYPE__OTG_DIS__MASK);
		break;
	case CDNS_ROLE_HOST:
		xhci_writel(&cdns->regs->OTGCMD,
			    OTGCMD_TYPE__HOST_BUS_REQ__MASK
			    | OTGCMD_TYPE__OTG_DIS__MASK);
		break;
	case CDNS_ROLE_OTG:
		reg = xhci_readl(&cdns->regs->OTGCTRL1);
		reg = OTGCTRL1_TYPE__IDPULLUP__SET(reg);
		xhci_writel(&cdns->regs->OTGCTRL1, reg);
		/*
		 * wait until valid ID (ID_VALUE) can be sampled (50ms)
		 */
#ifdef BOARD_PALLADIUM
		udelay(50);
#else
		mdelay(50);
#endif
		/*
		 * OTG mode is initialized later
		 */
		break;
	default:
		printf("Unsupported mode of operation %d\n", mode);
		return;
	}

	/* Shall we force it to usb2.0 mode */
	if (cdns->ss_disable)
		xhci_writel(&cdns->regs->OTGCMD, xhci_readl(&cdns->regs->OTGCMD) |
				OTGCMD_TYPE__SS_HOST_DISABLED_SET__MASK);
	cdns->current_mode = mode;
}

static int bitmain_core_init_mode(struct bm_xhci *cdns)
{
	/* Remap usb address of 32~39bit for accessing DDR */
	mmio_write_32(REG_TOP_DDR_ADDR_MODE,
		      (1 << DAMR_REG_USB_REMAP_ADDR_39_32_OFFSET));

	mmio_write_32(TOP_BASE + REG_TOP_SOFT_RST,
		      mmio_read_32(TOP_BASE + REG_TOP_SOFT_RST) &
		      (~BIT_TOP_SOFT_RST_USB));
	udelay(50);
	xhci_readl((void *)REG_TOP_USB_CTRSTS);
	mmio_write_32(REG_TOP_USB_CTRSTS,
		      UCR_MODE_STRAP_HOST |
		      (1 << UCR_PORT_OVER_CURRENT_ACTIVE_OFFSET));
	xhci_readl((void *)REG_TOP_USB_CTRSTS);
	mmio_write_32(TOP_BASE + REG_TOP_SOFT_RST,
		      mmio_read_32(TOP_BASE + REG_TOP_SOFT_RST) |
		      BIT_TOP_SOFT_RST_USB);
	udelay(50);
	mmio_write_32(REG_TOP_USB_PHY_CTRL,
		      mmio_read_32(REG_TOP_USB_PHY_CTRL) |
		      BIT_TOP_USB_PHY_CTRL_EXTVBUS);

	xhci_readl((void *)REG_TOP_USB_PHY_CTRL);
	xhci_readl((void *)REG_TOP_USB_CTRSTS);

	cdns_set_mode(cdns, CDNS_ROLE_HOST);

	return 0;
}

static int bitmain_xhci_core_init(struct bm_xhci *cdns,
				  struct udevice *dev)
{
	int ret = 0;

	ret = bitmain_core_init(cdns);
	if (ret) {
		debug("%s:failed to initialize core\n", __func__);
		return ret;
	}

	bitmain_core_init_mode(cdns);

	/*
	 * Reduce reset time value for hub port reset
	 * open in palladium
	 */
#ifdef BOARD_PALLADIUM
	xhci_writel((void *)(USB_HOST_BASE + 0x8138),
		    (xhci_readl((void *)(USB_HOST_BASE + 0x8138)) & ~0x7fff)
		    | 0x100);
#endif
	/*
	 * scratch_pad_en
	 * Command Manager: Enables scratch pad function
	 */
	xhci_writel((void *)(USB_HOST_BASE + 0x80d4),
		    (xhci_readl((void *)(USB_HOST_BASE + 0x80d4)) &
		    ~0x100));
	/*
	 * disable BW calculation.
	 * xhci_writel((void *)(USB_HOST_BASE + 0x809c),
	 *	       (xhci_readl((void *)(USB_HOST_BASE + 0x809c)) &
	 *	       ~0x00400000));
	 */

	return ret;
}

static void bitmain_xhci_core_exit(struct bm_xhci *cdsn)
{
}

int xhci_hcd_init(int index, struct xhci_hccr **hccr, struct xhci_hcor **hcor)
{
	struct bm_xhci *ctx = &bitmain;
	int ret = 0;

	ctx->hcd = (struct xhci_hccr *)USB_HOST_BASE;
	ctx->regs = (void *)USB_BASE;

	ret = bitmain_xhci_core_init(ctx, NULL);
	if (ret < 0) {
		puts("Failed to initialize xhci\n");
		return ret;
	}

	*hccr = (struct xhci_hccr *)(ctx->hcd);
	*hcor = (struct xhci_hcor *)((uintptr_t)*hccr
			+ HC_LENGTH(xhci_readl(&(*hccr)->cr_capbase)));

	debug("bitmain-xhci: init hccr %lx and hcor %lx hc_length %ld\n",
	      (uintptr_t)*hccr, (uintptr_t)*hcor,
	      (uintptr_t)HC_LENGTH(xhci_readl(&(*hccr)->cr_capbase)));

	return ret;
}

void xhci_hcd_stop(int index)
{
	struct bm_xhci *ctx = &bitmain;

	bitmain_xhci_core_exit(ctx);
}

#if defined(CONFIG_DM_USB)

static int xhci_usb_ofdata_to_platdata(struct udevice *dev)
{
	struct bm_xhci_platdata *plat = dev_get_platdata(dev);

	/*
	 * Get the base address for XHCI controller from the device node
	 */
	plat->hcd_base = devfdt_get_addr(dev);
	if (plat->hcd_base == FDT_ADDR_T_NONE) {
		debug("Can't get the XHCI register base address\n");
		return -ENXIO;
	}

#if defined(CONFIG_DM_USB) && defined(CONFIG_DM_REGULATOR)
	/* Vbus regulator */
	ret = device_get_supply_regulator(dev, "vbus-supply",
					  &plat->vbus_supply);
	if (ret)
		debug("Can't get vbus supply\n");
#endif

	return 0;
}

__weak int onboard_hub_reset(void)
{
	return 0;
}

static int xhci_usb_probe(struct udevice *dev)
{
	struct bm_xhci_platdata *plat = dev_get_platdata(dev);
	struct bm_xhci *ctx = &bitmain;
	struct xhci_hcor *hcor;
	struct xhci_hccr *hccr;
	int ret;

	if (!ctx || !plat) {
		printf("dev is NULL\n");
		return -1;
	}

	ctx->ss_disable = dev_read_bool(dev, "ss_disable");
	ctx->ext_hub = dev_read_bool(dev, "ext-hub");
	if (ctx->ext_hub)
		onboard_hub_reset();
	ctx->hcd = (struct xhci_hccr *)plat->hcd_base;
	ctx->regs = (void *)USB_BASE;
	ret = bitmain_xhci_core_init(ctx, dev);
	if (ret) {
		debug("XHCI: failed to initialize controller\n");
		return ret;
	}
	hccr = (struct xhci_hccr *)plat->hcd_base;
	hcor = (struct xhci_hcor *)((uint64_t)ctx->hcd +
				HC_LENGTH(xhci_readl(&ctx->hcd->cr_capbase)));

	printf("[XHCI] hcd = 0x%llx, regs = 0x%llx, ss_disable = %d\n",
	       (u64)ctx->hcd, (u64)ctx->regs, ctx->ss_disable);
#if defined(CONFIG_DM_USB) && defined(CONFIG_DM_REGULATOR)
	ret = regulator_set_enable(plat->vbus_supply, true);
	if (ret)
		debug("XHCI: Failed to enable vbus supply\n");
#endif

	return xhci_register(dev, hccr, hcor);
}

static int xhci_usb_remove(struct udevice *dev)
{
	struct bm_xhci *ctx = dev_get_priv(dev);
	int ret;

	ret = xhci_deregister(dev);
	if (ret)
		return ret;
	bitmain_xhci_core_exit(ctx);

#if defined(CONFIG_DM_USB) && defined(CONFIG_DM_REGULATOR)
	ret = regulator_set_enable(plat->vbus_supply, false);
	if (ret)
		debug("XHCI: Failed to disable vbus supply\n");
#endif

	return 0;
}

static const struct udevice_id xhci_usb_ids[] = {
	{ .compatible = "bitmain,xhci-platform" },
	{ }
};

U_BOOT_DRIVER(usb_xhci) = {
	.name	= "xhci_bitmain",
	.id	= UCLASS_USB,
	.of_match = xhci_usb_ids,
	.ofdata_to_platdata = xhci_usb_ofdata_to_platdata,
	.probe = xhci_usb_probe,
	.remove = xhci_usb_remove,
	.ops	= &xhci_usb_ops,
	.bind	= dm_scan_fdt_dev,
	.platdata_auto_alloc_size = sizeof(struct bm_xhci_platdata),
	.priv_auto_alloc_size = sizeof(struct xhci_ctrl),
	.flags	= DM_FLAG_ALLOC_PRIV_DMA,
};

#endif
