#include <common.h>
#include <dm.h>
#include <errno.h>
#include <reset-uclass.h>
#include <asm/io.h>
#include <linux/io.h>

#define MAX_RESETS	32

struct bm_reset_priv {
	void __iomem *regs;
};

static int bm_reset_assert(struct reset_ctl *rst)
{
	struct bm_reset_priv *priv = dev_get_priv(rst->dev);

	debug("[hq] %s, 0x%p 0x%lx\n", __func__, priv->regs, BIT(rst->id));

	writel(readl(priv->regs) & (~BIT(rst->id)), priv->regs);
	mdelay(1);

	return 0;
}

static int bm_reset_deassert(struct reset_ctl *rst)
{
	struct bm_reset_priv *priv = dev_get_priv(rst->dev);

	debug("[hq] %s, 0x%p 0x%lx\n", __func__, priv->regs, BIT(rst->id));
	writel(readl(priv->regs) | BIT(rst->id), priv->regs);
	mdelay(1);

	return 0;
}

static int bm_reset_free(struct reset_ctl *rst)
{
	return 0;
}

static int bm_reset_request(struct reset_ctl *rst)
{
	if (rst->id >= MAX_RESETS)
		return -EINVAL;

	return bm_reset_assert(rst);
}

struct reset_ops bm_reset_reset_ops = {
	.free = bm_reset_free,
	.request = bm_reset_request,
	.rst_assert = bm_reset_assert,
	.rst_deassert = bm_reset_deassert,
};

static const struct udevice_id bm_reset_ids[] = {
	{ .compatible = "bitmain,bm-reset" },
	{ /* sentinel */ }
};

static int bm_reset_probe(struct udevice *dev)
{
	struct bm_reset_priv *priv = dev_get_priv(dev);
	fdt_addr_t addr;
	fdt_size_t size;

	addr = devfdt_get_addr_size_index(dev, 0, &size);
	if (addr == FDT_ADDR_T_NONE)
		return -EINVAL;

	priv->regs = ioremap(addr, size);

	return 0;
}

U_BOOT_DRIVER(bm_reset) = {
	.name = "bm-reset",
	.id = UCLASS_RESET,
	.of_match = bm_reset_ids,
	.ops = &bm_reset_reset_ops,
	.probe = bm_reset_probe,
	.priv_auto_alloc_size = sizeof(struct bm_reset_priv),
};
