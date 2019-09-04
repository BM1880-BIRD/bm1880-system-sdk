#include <linux/err.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>
#ifdef CONFIG_COMMON_CLK_BITMAIN
#include <linux/clk.h>
#include <linux/clk-provider.h>
#else
#include <linux/io.h>
#include <linux/delay.h>
#endif

struct bm1880_cooling_device {
	struct thermal_cooling_device *cdev;
	struct mutex lock; /* lock to protect the content of this struct */
	unsigned long clock_state;
	unsigned long max_clock_state;
};

#ifdef CONFIG_COMMON_CLK_BITMAIN
#define MAX_MPLL_RATE	1150000000L
#define MIN_MPLL_RATE	(1150000000L/49L)
#define MAX_SPLL_RATE	775000000L
#define MIN_SPLL_RATE	(775000000L/49L)
static const struct {
	unsigned long mpll;
	unsigned long spll;
} pll_rate[] = {
	{MAX_MPLL_RATE, MAX_SPLL_RATE},
	{MAX_MPLL_RATE/2L, MAX_SPLL_RATE},
	{MAX_MPLL_RATE/2L, MAX_SPLL_RATE/2L},
	{MAX_MPLL_RATE/3L, MAX_SPLL_RATE/2L},
	{MAX_MPLL_RATE/3L, MAX_SPLL_RATE/3L},
	{MAX_MPLL_RATE/4L, MAX_SPLL_RATE/3L},
	{MAX_MPLL_RATE/4L, MAX_SPLL_RATE/4L},
	{MAX_MPLL_RATE/5L, MAX_SPLL_RATE/4L},
	{MAX_MPLL_RATE/5L, MAX_SPLL_RATE/5L},
	{MAX_MPLL_RATE/6L, MAX_SPLL_RATE/5L},
	{MAX_MPLL_RATE/6L, MAX_SPLL_RATE/6L},
	{MAX_MPLL_RATE/7L, MAX_SPLL_RATE/6L},
	{MAX_MPLL_RATE/7L, MAX_SPLL_RATE/7L},
	{MAX_MPLL_RATE/14L, MAX_SPLL_RATE/7L},
	{MAX_MPLL_RATE/14L, MAX_SPLL_RATE/14L},
	{MAX_MPLL_RATE/49L, MAX_SPLL_RATE/14L},
	{MAX_MPLL_RATE/49L, MAX_SPLL_RATE/49L},
};

/* TODO: need to be removed when bmnpu.ko no longer uses this function */
void bm1880_set_spll_divide(u32 div)
{
	struct clk *spll_clk = __clk_lookup("spll_clk");
	unsigned long rate;

	rate = (div == 1) ? MAX_SPLL_RATE : MIN_SPLL_RATE;
	rate = clk_round_rate(spll_clk, rate);
	clk_set_rate(spll_clk, rate);
}
EXPORT_SYMBOL(bm1880_set_spll_divide);
#else
/* mpll & spll div table */
static const struct {
	u8 mpll_div;
	u8 spll_div;
} pll_div[] = {
	/* MPLL first interleaved */
	{0x11, 0x11}, {0x12, 0x11}, {0x12, 0x12}, {0x13, 0x12}, {0x13, 0x13},
	{0x14, 0x13}, {0x14, 0x14}, {0x15, 0x14}, {0x15, 0x15}, {0x16, 0x15},
	{0x16, 0x16}, {0x17, 0x16}, {0x17, 0x17}, {0x27, 0x17}, {0x27, 0x27},
	{0x77, 0x27}, {0x77, 0x77},
};

static int pll_div_idx;

void bm1880_set_spll_divide(u32 div)
{
	u32 __iomem *spll_ctl;
	u32 spll_ctl_val;
	u32 postdiv;

	spll_ctl = ioremap_nocache(0x500100ec, 4);

	if (div == 1)
		postdiv = pll_div[pll_div_idx].spll_div;
	else
		postdiv = div + (div << 4);

	spll_ctl_val = ioread32(spll_ctl);
	spll_ctl_val &= ~(0x77 << 8);
	spll_ctl_val |= postdiv << 8;
	iowrite32(spll_ctl_val, spll_ctl);
	iounmap(spll_ctl);
}
EXPORT_SYMBOL(bm1880_set_spll_divide);

static void bm1880_update_mpll_ctl(void)
{
	u32 __iomem *mpll_ctl;
	u32 __iomem *clk_sel0;

	u32 mpll_ctl_val;
	u32 clk_sel0_val;

	mpll_ctl = ioremap_nocache(0x500100e8, 4);
	clk_sel0 = ioremap_nocache(0x50010820, 4);

	mpll_ctl_val = ioread32(mpll_ctl);
	mpll_ctl_val &= ~(0x77 << 8);
	mpll_ctl_val |= pll_div[pll_div_idx].mpll_div << 8;

	clk_sel0_val = ioread32(clk_sel0);

	/* select spll as a53 clock and fpll as axi1 clock */
	iowrite32(clk_sel0_val & 0xFFFFFFFA, clk_sel0);
	iowrite32(mpll_ctl_val, mpll_ctl);
	udelay(1000);
	/* select mpll as a53 and axi1 clock */
	iowrite32(clk_sel0_val, clk_sel0);

	iounmap(mpll_ctl);
	iounmap(clk_sel0);
}
#endif /* ! CONFIG_COMMON_CLK_BITMAIN */

/* cooling device thermal callback functions are defined below */

/**
 * bm1880_cooling_get_max_state - callback function to get the max cooling state.
 * @cdev: thermal cooling device pointer.
 * @state: fill this variable with the max cooling state.
 *
 * Callback for the thermal cooling device to return the
 * max cooling state.
 *
 * Return: 0 on success, an error code otherwise.
 */
static int bm1880_cooling_get_max_state(struct thermal_cooling_device *cdev,
					unsigned long *state)
{
	struct bm1880_cooling_device *bmcdev = cdev->devdata;

	mutex_lock(&bmcdev->lock);
	*state = bmcdev->max_clock_state;
	mutex_unlock(&bmcdev->lock);

	return 0;
}

/**
 * bm1880_cooling_get_cur_state - function to get the current cooling state.
 * @cdev: thermal cooling device pointer.
 * @state: fill this variable with the current cooling state.
 *
 * Callback for the thermal cooling device to return the current cooling
 * state.
 *
 * Return: 0 (success)
 */
static int bm1880_cooling_get_cur_state(struct thermal_cooling_device *cdev,
					unsigned long *state)
{
	struct bm1880_cooling_device *bmcdev = cdev->devdata;

	mutex_lock(&bmcdev->lock);
	*state = bmcdev->clock_state;
	mutex_unlock(&bmcdev->lock);

	return 0;
}

/**
 * bm1880_cooling_set_cur_state - function to set the current cooling state.
 * @cdev: thermal cooling device pointer.
 * @state: set this variable to the current cooling state.
 *
 * Callback for the thermal cooling device to change the bm1880 cooling
 * current cooling state.
 *
 * Return: 0 on success, an error code otherwise.
 */
static int bm1880_cooling_set_cur_state(struct thermal_cooling_device *cdev,
					unsigned long state)
{
	struct bm1880_cooling_device *bmcdev = cdev->devdata;

	mutex_lock(&bmcdev->lock);

	if (state <= bmcdev->max_clock_state) {
#ifdef CONFIG_COMMON_CLK_BITMAIN
		struct clk *mpll_clk = __clk_lookup("mpll_clk");
		struct clk *spll_clk = __clk_lookup("spll_clk");
		struct clk *mux_clk_a53 = __clk_lookup("mux_clk_a53");
		struct clk *mux_clk_axi1 = __clk_lookup("mux_clk_axi1");
		struct clk *div_0_clk_axi1 = __clk_lookup("div_0_clk_axi1");
		struct clk *div_1_clk_axi1 = __clk_lookup("div_1_clk_axi1");

		/* set max rate of mpll and spll */
		clk_set_max_rate(mpll_clk, pll_rate[state].mpll);
		clk_set_max_rate(spll_clk, pll_rate[state].spll);

		/* set mpll rate */
		clk_set_parent(mux_clk_a53, spll_clk);
		clk_set_parent(mux_clk_axi1, div_1_clk_axi1);
		clk_set_rate(mpll_clk, pll_rate[state].mpll);
		clk_set_parent(mux_clk_a53, mpll_clk);
		clk_set_parent(mux_clk_axi1, div_0_clk_axi1);

		bmcdev->clock_state = state;
#else
		pll_div_idx = bmcdev->clock_state;
		bm1880_update_mpll_ctl();
#endif
	}

	mutex_unlock(&bmcdev->lock);
	return 0;
}

/* Bind clock callbacks to thermal cooling device ops */
static struct thermal_cooling_device_ops const bm1880_cooling_ops = {
	.get_max_state = bm1880_cooling_get_max_state,
	.get_cur_state = bm1880_cooling_get_cur_state,
	.set_cur_state = bm1880_cooling_set_cur_state,
};

static struct bm1880_cooling_device *
bm1880_cooling_device_register(struct device *dev)
{
	struct thermal_cooling_device *cdev;
	struct bm1880_cooling_device *bmcdev = NULL;

	bmcdev = devm_kzalloc(dev, sizeof(*bmcdev), GFP_KERNEL);
	if (!bmcdev)
		return ERR_PTR(-ENOMEM);

	mutex_init(&bmcdev->lock);

	bmcdev->clock_state = 0;
#ifdef CONFIG_COMMON_CLK_BITMAIN
	bmcdev->max_clock_state = ARRAY_SIZE(pll_rate) - 1;
#else
	bmcdev->max_clock_state = ARRAY_SIZE(pll_div) - 1;
#endif
	cdev = thermal_of_cooling_device_register(dev->of_node,
			"bm1880_cooling", bmcdev,
			&bm1880_cooling_ops);
	if (IS_ERR(cdev))
		return ERR_PTR(-EINVAL);

	bmcdev->cdev = cdev;

	return bmcdev;
}

static void bm1880_cooling_device_unregister(struct bm1880_cooling_device
		*bmcdev)
{
	thermal_cooling_device_unregister(bmcdev->cdev);
}

static int bm1880_cooling_probe(struct platform_device *pdev)
{
	struct bm1880_cooling_device *bmcdev;

	bmcdev = bm1880_cooling_device_register(&pdev->dev);
	if (IS_ERR(bmcdev)) {
		int ret = PTR_ERR(bmcdev);

		if (ret != -EPROBE_DEFER)
			dev_err(&pdev->dev,
				"Failed to register cooling device %d\n",
				ret);

		return ret;
	}

	platform_set_drvdata(pdev, bmcdev);
	dev_info(&pdev->dev, "Cooling device registered: %s\n",	bmcdev->cdev->type);

	return 0;
}

static int bm1880_cooling_remove(struct platform_device *pdev)
{
	struct bm1880_cooling_device *bmcdev = platform_get_drvdata(pdev);

	if (!IS_ERR(bmcdev))
		bm1880_cooling_device_unregister(bmcdev);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id bm1880_cooling_match[] = {
	{ .compatible = "bitmain,bm1880-cooling" },
	{},
};
MODULE_DEVICE_TABLE(of, bm1880_cooling_match);
#endif

static struct platform_driver bm1880_cooling_driver = {
	.driver = {
		.name = "bm1880-cooling",
		.of_match_table = of_match_ptr(bm1880_cooling_match),
	},
	.probe = bm1880_cooling_probe,
	.remove = bm1880_cooling_remove,
};

module_platform_driver(bm1880_cooling_driver);

MODULE_AUTHOR("fisher.cheng@bitmain.com");
MODULE_DESCRIPTION("BM1880 cooling driver");
MODULE_LICENSE("GPL");
