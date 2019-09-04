/*
 * Copyright (c) 2019 BITMAIN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/mfd/syscon.h>
#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/string.h>
#include <linux/log2.h>
#include "clk.h"

/*
 * @hw:		handle between common and hardware-specific interfaces
 * @reg:	register containing divider
 * @shift:	shift to the divider bit field
 * @width:	width of the divider bit field
 * @initial_val:initial value of the divider
 * @table:	the div table that the divider supports
 * @lock:	register lock
 */
struct bm_clk_divider {
	struct clk_hw	hw;
	void __iomem	*reg;
	u8		shift;
	u8		width;
	u8		flags;
	u32		initial_val;
	const struct clk_div_table *table;
	spinlock_t	*lock;
};

#define to_bm_clk_divider(_hw)	\
	container_of(_hw, struct bm_clk_divider, hw)

static const char * const clk_mode[] = {
	"normal", "fast", "safe", "bypass", NULL
};

static inline int bm_pll_enable(struct regmap *map,
					struct bm_pll_clock *pll, bool en)
{
#ifdef CONFIG_ARCH_BM1684
	unsigned int value;
	unsigned long enter;
	unsigned int id;

	/*
	 * when set pll divider, the id order:
	 * mpll tpll fpll vpll dpll0 dpll1(top + offset(0xe4, 0xe8, ...))
	 * if en/dis-able the pll, driver polling the pll state, sthe id order:
	 * mpll tpll vpll fpll dpll0 dpll1(bit offset(0x0, 0x1, ...))
	 */
	if (pll->id == 2)
		id = 3;
	else if (pll->id == 3)
		id = 2;
	else
		id = pll->id;

	if (en) {
		/* wait pll lock */
		enter = jiffies;
		regmap_read(map, pll->status_offset, &value);
		while (!((value >> (PLL_STAT_LOCK_OFFSET + id)) & 0x1)) {
			regmap_read(map, pll->status_offset, &value);
			if (time_after(jiffies, enter + HZ / 1000))
				goto out;
		}
		/* wait pll updating */
		enter = jiffies;
		regmap_read(map, pll->status_offset, &value);
		while (((value >> id) & 0x1)) {
			regmap_read(map, pll->status_offset, &value);
			if (time_after(jiffies, enter + HZ / 1000))
				goto out;
		}
		/* enable pll */
		regmap_read(map, pll->enable_offset, &value);
		regmap_write(map, pll->enable_offset, value | (1 << id));
	} else {
		/* disable pll */
		regmap_read(map, pll->enable_offset, &value);
		regmap_write(map, pll->enable_offset, value & (~(1 << id)));
	}

	return 0;
out:
	return 1;
#elif defined(CONFIG_ARCH_BM1880)
	unsigned int value;
	unsigned long enter;
	unsigned int id;

	/* wait pll updating */
	id = 4 - pll->id;
	enter = jiffies;
	regmap_read(map, pll->status_offset, &value);
	while (((value >> id) & 0x1)) {
		regmap_read(map, pll->status_offset, &value);
		if (time_after(jiffies, enter + HZ / 1000))
			goto out;
	}
	return 0;
out:
	return 1;
#else
	return 0;
#endif
}

static inline int bm_pll_write(struct regmap *map, int id, int value)
{
	return regmap_write(map, PLL_CTRL_OFFSET + (id << 2), value);
}

static inline int bm_pll_read(struct regmap *map, int id, unsigned int *pvalue)
{
	return regmap_read(map, PLL_CTRL_OFFSET + (id << 2), pvalue);
}

static unsigned int _get_table_div(const struct clk_div_table *table,
				   unsigned int val)
{
	const struct clk_div_table *clkt;

	for (clkt = table; clkt->div; clkt++)
		if (clkt->val == val)
			return clkt->div;
	return 0;
}

static unsigned int _get_div(const struct clk_div_table *table,
			     unsigned int val, unsigned long flags, u8 width)
{
	if (flags & CLK_DIVIDER_ONE_BASED)
		return val;
	if (flags & CLK_DIVIDER_POWER_OF_TWO)
		return 1 << val;
	if (flags & CLK_DIVIDER_MAX_AT_ZERO)
		return val ? val : div_mask(width) + 1;
	if (table)
		return _get_table_div(table, val);
	return val + 1;
}

static unsigned long bm_clk_divider_recalc_rate(struct clk_hw *hw,
						unsigned long parent_rate)
{
	struct bm_clk_divider *divider = to_bm_clk_divider(hw);
	unsigned int val;

	val = clk_readl(divider->reg) >> divider->shift;
	val &= div_mask(divider->width);

#ifdef CONFIG_ARCH_BM1880
	/* if select divide factor from initial value */
	if (!(clk_readl(divider->reg) & BIT(3)))
		val = divider->initial_val;
#endif

	return divider_recalc_rate(hw, parent_rate, val, divider->table,
				   divider->flags);
}

static long bm_clk_divider_round_rate(struct clk_hw *hw, unsigned long rate,
				      unsigned long *prate)
{
	int bestdiv;
	struct bm_clk_divider *divider = to_bm_clk_divider(hw);

	/* if read only, just return current value */
	if (divider->flags & CLK_DIVIDER_READ_ONLY) {
		bestdiv = clk_readl(divider->reg) >> divider->shift;
		bestdiv &= div_mask(divider->width);
		bestdiv = _get_div(divider->table, bestdiv, divider->flags,
				   divider->width);
		return DIV_ROUND_UP_ULL((u64)*prate, bestdiv);
	}

	return divider_round_rate(hw, rate, prate, divider->table,
				  divider->width, divider->flags);
}

static int bm_clk_divider_set_rate(struct clk_hw *hw, unsigned long rate,
				   unsigned long parent_rate)
{
	unsigned int value;
	unsigned int val;
	unsigned long flags = 0;
	struct bm_clk_divider *divider = to_bm_clk_divider(hw);

	value = divider_get_val(rate, parent_rate, divider->table,
				divider->width, divider->flags);

	if (divider->lock)
		spin_lock_irqsave(divider->lock, flags);
	else
		__acquire(divider->lock);

	/* div assert */
	val = clk_readl(divider->reg);
	val &= ~0x1;
	clk_writel(val, divider->reg);

	if (divider->flags & CLK_DIVIDER_HIWORD_MASK) {
		val = div_mask(divider->width) << (divider->shift + 16);
	} else {
		val = clk_readl(divider->reg);
		val &= ~(div_mask(divider->width) << divider->shift);
	}

	val |= value << divider->shift;
	clk_writel(val, divider->reg);

	if (!(divider->flags & CLK_DIVIDER_READ_ONLY))
		val |= 1 << 3;

	/* de-assert */
	val |= 1;
	clk_writel(val, divider->reg);
	if (divider->lock)
		spin_unlock_irqrestore(divider->lock, flags);
	else
		__release(divider->lock);

	return 0;
}

/* Below array is the total combination lists of POSTDIV1 and POSTDIV2
 * for example:
 * postdiv1_2[0] = {1, 1, 1}
 *           ==> div1 = 1, div2 = 1 , div1 * div2 = 1
 * postdiv1_2[22] = {6, 7, 42}
 *           ==> div1 = 6, div2 = 7 , div1 * div2 = 42
 *
 * And POSTDIV_RESULT_INDEX point to 3rd element in the array
 */
#define	POSTDIV_RESULT_INDEX	2
int postdiv1_2[][3] = {
	{2, 4,  8}, {3, 3,  9}, {2, 5, 10}, {2, 6, 12},
	{2, 7, 14}, {3, 5, 15}, {4, 4, 16}, {3, 6, 18},
	{4, 5, 20}, {3, 7, 21}, {4, 6, 24}, {5, 5, 25},
	{4, 7, 28}, {5, 6, 30}, {5, 7, 35}, {6, 6, 36},
	{6, 7, 42}, {7, 7, 49}
};

static inline unsigned long abs_diff(unsigned long a, unsigned long b)
{
	return (a > b) ? (a - b) : (b - a);
}

/*
 * @reg_value: current register value
 * @parent_rate: parent frequency
 *
 * This function is used to calculate below "rate" in equation
 * rate = (parent_rate/REFDIV) x FBDIV/POSTDIV1/POSTDIV2
 *      = (parent_rate x FBDIV) / (REFDIV x POSTDIV1 x POSTDIV2)
 */
static unsigned long __pll_recalc_rate(unsigned int reg_value,
				      unsigned long parent_rate)
{
	unsigned int fbdiv, refdiv;
	unsigned int postdiv1, postdiv2;
	u64 rate, numerator, denominator;

	fbdiv = (reg_value >> 16) & 0xfff;
	refdiv = reg_value & 0x1f;
	postdiv1 = (reg_value >> 8) & 0x7;
	postdiv2 = (reg_value >> 12) & 0x7;

	numerator = parent_rate * fbdiv;
	denominator = refdiv * postdiv1 * postdiv2;
	do_div(numerator, denominator);
	rate = numerator;

	return rate;
}

/*
 * @reg_value: current register value
 * @rate: request rate
 * @prate: parent rate
 * @pctrl_table: use to save div1/div2/fbdiv/refdiv
 *
 * We use below equation to get POSTDIV1 and POSTDIV2
 * POSTDIV = (parent_rate/REFDIV) x FBDIV/input_rate
 * above POSTDIV = POSTDIV1*POSTDIV2
 */
static int __pll_get_postdiv_1_2(unsigned long rate, unsigned long prate,
				 unsigned int fbdiv, unsigned int refdiv, unsigned int *postdiv1,
				 unsigned int *postdiv2)
{
	int index = 0;
	int ret = 0;
	u64 tmp0;

	/* calculate (parent_rate/refdiv)
	 * and result save to prate
	 */
	tmp0 = prate;
	do_div(tmp0, refdiv);

	/* calcuate ((parent_rate/REFDIV) x FBDIV)
	 * and result save to prate
	 */
	tmp0 *= fbdiv;

	/* calcuate (((parent_rate/REFDIV) x FBDIV)/input_rate)
	 * and result save to prate
	 * here *prate is (POSTDIV1*POSTDIV2)
	 */
	do_div(tmp0, rate);

	/* calculate div1 and div2 value */
	if (tmp0 <= 7) {
		/* (div1 * div2) <= 7, no need to use array search */
		*postdiv1 = tmp0;
		*postdiv2 = 1;
	} else {
		/* (div1 * div2) > 7, use array search */
		for (index = 0; index < ARRAY_SIZE(postdiv1_2); index++) {
			if (tmp0 > postdiv1_2[index][POSTDIV_RESULT_INDEX]) {
				continue;
			} else {
				/* found it */
				break;
			}
		}
		if (index < ARRAY_SIZE(postdiv1_2)) {
			*postdiv1 = postdiv1_2[index][0];
			*postdiv2 = postdiv1_2[index][1];
		} else {
			pr_debug("%s out of postdiv array range!\n", __func__);
			ret = -ESPIPE;
		}
	}

	return ret;
}

static int __get_pll_ctl_setting(struct bm_pll_ctrl *best,
			unsigned long req_rate, unsigned long parent_rate)
{
	int ret;
	unsigned int fbdiv, refdiv, fref, postdiv1, postdiv2;
	unsigned long tmp = 0, foutvco;

	fref = parent_rate;

	for (refdiv = REFDIV_MIN; refdiv < REFDIV_MAX + 1; refdiv++) {
		for (fbdiv = FBDIV_MIN; fbdiv < FBDIV_MAX + 1; fbdiv++) {
			foutvco = fref * fbdiv / refdiv;
			/* check fpostdiv pfd */
			if (foutvco < PLL_FREQ_MIN || foutvco > PLL_FREQ_MAX
					|| (fref / refdiv) < 10)
				continue;

			ret = __pll_get_postdiv_1_2(req_rate, fref, fbdiv,
					refdiv, &postdiv1, &postdiv2);
			if (ret)
				continue;

			tmp = foutvco / (postdiv1 * postdiv2);
			if (abs_diff(tmp, req_rate) < abs_diff(best->freq, req_rate)) {
				best->freq = tmp;
				best->refdiv = refdiv;
				best->fbdiv = fbdiv;
				best->postdiv1 = postdiv1;
				best->postdiv2 = postdiv2;
				if (tmp == req_rate)
					return 0;
			}
			continue;
		}
	}

	return 0;
}

/*
 * @hw: ccf use to hook get bm_pll_clock
 * @parent_rate: parent rate
 *
 * The is function will be called through clk_get_rate
 * and return current rate after decoding reg value
 */
static unsigned long bm_clk_pll_recalc_rate(struct clk_hw *hw,
					    unsigned long parent_rate)
{
	unsigned int value;
	unsigned long rate;
	struct bm_pll_clock *bm_pll = to_bm_pll_clk(hw);

	bm_pll_read(bm_pll->syscon_top, bm_pll->id, &value);
	rate = __pll_recalc_rate(value, parent_rate);
	return rate;
}

static long bm_clk_pll_round_rate(struct clk_hw *hw,
				  unsigned long req_rate, unsigned long *prate)
{
	unsigned int value;
	struct bm_pll_ctrl pctrl_table;
	struct bm_pll_clock *bm_pll = to_bm_pll_clk(hw);
	long proper_rate;

	memset(&pctrl_table, 0, sizeof(struct bm_pll_ctrl));

	/* use current setting to get fbdiv, refdiv
	 * then combine with prate, and req_rate to
	 * get postdiv1 and postdiv2
	 */
	bm_pll_read(bm_pll->syscon_top, bm_pll->id, &value);
	__get_pll_ctl_setting(&pctrl_table, req_rate, *prate);
	if (!pctrl_table.freq) {
		proper_rate = 0;
		goto out;
	}

	value = TOP_PLL_CTRL(pctrl_table.fbdiv, pctrl_table.postdiv1,
			     pctrl_table.postdiv2, pctrl_table.refdiv);
	proper_rate = (long)__pll_recalc_rate(value, *prate);

out:
	return proper_rate;
}

static int bm_clk_pll_determine_rate(struct clk_hw *hw,
				     struct clk_rate_request *req)
{
	req->rate = bm_clk_pll_round_rate(hw, min(req->rate, req->max_rate),
					  &req->best_parent_rate);
	return 0;
}

static int bm_clk_pll_set_rate(struct clk_hw *hw, unsigned long rate,
			       unsigned long parent_rate)
{
	unsigned long flags;
	unsigned int value;
	int ret = 0;
	struct bm_pll_ctrl pctrl_table;
	struct bm_pll_clock *bm_pll = to_bm_pll_clk(hw);

	memset(&pctrl_table, 0, sizeof(struct bm_pll_ctrl));
	spin_lock_irqsave(bm_pll->lock, flags);
	if (bm_pll_enable(bm_pll->syscon_top, bm_pll, 0)) {
		pr_warn("Can't disable pll(%s), status error\n", bm_pll->name);
		goto out;
	}
	bm_pll_read(bm_pll->syscon_top, bm_pll->id, &value);
	__get_pll_ctl_setting(&pctrl_table, rate, parent_rate);
	if (!pctrl_table.freq) {
		pr_warn("%s: Can't find a proper pll setting\n", bm_pll->name);
		goto out;
	}

	value = TOP_PLL_CTRL(pctrl_table.fbdiv, pctrl_table.postdiv1,
			     pctrl_table.postdiv2, pctrl_table.refdiv);

	/* write the value to top register */
	bm_pll_write(bm_pll->syscon_top, bm_pll->id, value);
	bm_pll_enable(bm_pll->syscon_top, bm_pll, 1);
out:
	spin_unlock_irqrestore(bm_pll->lock, flags);
	return ret;
}

const struct clk_ops bm_clk_divider_ops = {
	.recalc_rate = bm_clk_divider_recalc_rate,
	.round_rate = bm_clk_divider_round_rate,
	.set_rate = bm_clk_divider_set_rate,
};

const struct clk_ops bm_clk_divider_ro_ops = {
	.recalc_rate = bm_clk_divider_recalc_rate,
	.round_rate = bm_clk_divider_round_rate,
};

const struct clk_ops bm_clk_pll_ops = {
	.recalc_rate = bm_clk_pll_recalc_rate,
	.round_rate = bm_clk_pll_round_rate,
	.determine_rate = bm_clk_pll_determine_rate,
	.set_rate = bm_clk_pll_set_rate,
};

const struct clk_ops bm_clk_pll_ro_ops = {
	.recalc_rate = bm_clk_pll_recalc_rate,
	.round_rate = bm_clk_pll_round_rate,
};

static int set_default_div_clk_rates(struct device_node *node, int mode)
{
	struct of_phandle_args clkspec;
	struct property *prop;
	const __be32 *cur;
	int rc, index = 0;
	struct clk *clk;
	u32 rate;
	char name[32];

	memset(name, 0, 32);
	sprintf(name, "assigned-clock-rates-%s", clk_mode[mode]);
	of_property_for_each_u32(node, name, prop, cur, rate) {
		if (rate) {
			rc = of_parse_phandle_with_args(node, "assigned-clocks",
					"#clock-cells", index, &clkspec);
			if (rc < 0) {
				/* skip empty (null) phandles */
				if (rc == -ENOENT)
					continue;
				else
					return rc;
			}

			clk = of_clk_get_from_provider(&clkspec);
			if (IS_ERR(clk)) {
				pr_warn("clk: couldn't get clock %d for %s\n",
					index, node->full_name);
				return PTR_ERR(clk);
			}

			rc = clk_set_rate(clk, rate);
			if (rc < 0)
				pr_err("clk: couldn't set %s clk rate to %d (%d), current rate: %ld\n",
					   __clk_get_name(clk), rate, rc,
					   clk_get_rate(clk));
			clk_put(clk);
		}
		index++;
	}
	return 0;
}

static struct clk *__register_divider_clks(struct device *dev, const char *name,
					   const char *parent_name,
					   unsigned long flags,
					   void __iomem *reg, u8 shift,
					   u8 width, u32 initial_val,
					   u8 clk_divider_flags,
					   const struct clk_div_table *table,
					   spinlock_t *lock)
{
	struct bm_clk_divider *div;
	struct clk_hw *hw;
	struct clk_init_data init;
	int ret;

	if (clk_divider_flags & CLK_DIVIDER_HIWORD_MASK) {
		if (width + shift > 16) {
			pr_warn("divider value exceeds LOWORD field\n");
			return ERR_PTR(-EINVAL);
		}
	}

	/* allocate the divider */
	div = kzalloc(sizeof(*div), GFP_KERNEL);
	if (!div)
		return ERR_PTR(-ENOMEM);

	init.name = name;
	if (clk_divider_flags & CLK_DIVIDER_READ_ONLY)
		init.ops = &bm_clk_divider_ro_ops;
	else
		init.ops = &bm_clk_divider_ops;
	init.flags = flags | CLK_IS_BASIC;
	init.parent_names = (parent_name ? &parent_name : NULL);
	init.num_parents = (parent_name ? 1 : 0);

	/* struct bm_clk_divider assignments */
	div->reg = reg;
	div->shift = shift;
	div->width = width;
	div->flags = clk_divider_flags;
	div->lock = lock;
	div->hw.init = &init;
	div->table = table;
	div->initial_val = initial_val;

	/* register the clock */
	hw = &div->hw;
	ret = clk_hw_register(dev, hw);
	if (ret) {
		kfree(div);
		hw = ERR_PTR(ret);
		return ERR_PTR(-EBUSY);
	}

	return hw->clk;
}

static inline int register_provider_clks
(struct device_node *node, struct bm_clk_data *clk_data, int clk_num)
{
	return of_clk_add_provider(node, of_clk_src_onecell_get,
				   &clk_data->clk_data);
}

static int register_gate_clks(struct device *dev, struct bm_clk_data *clk_data)
{
	struct clk *clk;
	const struct bm_clk_table *table = clk_data->table;
	const struct bm_gate_clock *gate_clks = table->gate_clks;
	void __iomem *base = clk_data->base;
	int clk_num = table->gate_clks_num;
	int i;

	for (i = 0; i < clk_num; i++) {
		clk = clk_register_gate(
			dev, gate_clks[i].name, gate_clks[i].parent_name,
			gate_clks[i].flags, base + gate_clks[i].offset,
			gate_clks[i].bit_idx, gate_clks[i].gate_flags,
			&clk_data->lock);
		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s\n", __func__,
			       gate_clks[i].name);
			goto err;
		}

		if (gate_clks[i].alias)
			clk_register_clkdev(clk, gate_clks[i].alias, NULL);

		clk_data->clk_data.clks[gate_clks[i].id] = clk;
	}

	return 0;

err:
	while (i--)
		clk_unregister_gate(clk_data->clk_data.clks[gate_clks[i].id]);

	return PTR_ERR(clk);
}

static int register_divider_clks(struct device *dev,
				 struct bm_clk_data *clk_data)
{
	struct clk *clk;
	const struct bm_clk_table *table = clk_data->table;
	const struct bm_divider_clock *div_clks = table->div_clks;
	void __iomem *base = clk_data->base;
	int clk_num = table->div_clks_num;
	int i, val;

	for (i = 0; i < clk_num; i++) {
		clk = __register_divider_clks(
			NULL, div_clks[i].name, div_clks[i].parent_name,
			div_clks[i].flags, base + div_clks[i].offset,
			div_clks[i].shift, div_clks[i].width,
			div_clks[i].initial_val,
			(div_clks[i].initial_sel & BM_CLK_USE_INIT_VAL) ?
				div_clks[i].div_flags | CLK_DIVIDER_READ_ONLY :
				div_clks[i].div_flags,
			div_clks[i].table, &clk_data->lock);
		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s\n", __func__,
			       div_clks[i].name);
			goto err;
		}

		clk_data->clk_data.clks[div_clks[i].id] = clk;

		if (div_clks[i].initial_sel == BM_CLK_USE_REG_VAL) {
			regmap_read(clk_data->syscon_top, div_clks[i].offset,
				    &val);

			/*
			 * set a default divider factor,
			 * clk driver should not select divider clock as the
			 * clock source, before set the divider by right process
			 * (assert div, set div factor, de assert div).
			 */
			regmap_write(clk_data->syscon_top, div_clks[i].offset,
				     val | 1 << 16);
		}
	}

	return 0;

err:
	while (i--)
		clk_unregister_divider(clk_data->clk_data.clks[div_clks[i].id]);

	return PTR_ERR(clk);
}

static int register_mux_clks(struct device *dev, struct bm_clk_data *clk_data)
{
	struct clk *clk;
	const struct bm_clk_table *table = clk_data->table;
	const struct bm_mux_clock *mux_clks = table->mux_clks;
	void __iomem *base = clk_data->base;
	int clk_num = table->mux_clks_num;
	int i;

	for (i = 0; i < clk_num; i++) {
		u32 mask = BIT(mux_clks[i].width) - 1;

		clk = clk_register_mux_table(
			dev, mux_clks[i].name, mux_clks[i].parent_names,
			mux_clks[i].num_parents, mux_clks[i].flags,
			base + mux_clks[i].offset, mux_clks[i].shift, mask,
			mux_clks[i].mux_flags, mux_clks[i].table,
			&clk_data->lock);
		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s\n", __func__,
			       mux_clks[i].name);
			goto err;
		}

		clk_data->clk_data.clks[mux_clks[i].id] = clk;
	}

	return 0;

err:
	while (i--)
		clk_unregister_mux(clk_data->clk_data.clks[mux_clks[i].id]);

	return PTR_ERR(clk);
}

/* pll clock init */
int bm_register_pll_clks(struct device_node *node,
			 struct bm_clk_data *clk_data, unsigned int id)
{
	struct clk *clk = NULL;
	struct bm_pll_clock *pll_clks;
	int i, ret = 0;
	const struct clk_ops *local_ops;

	pll_clks = (struct bm_pll_clock *)clk_data->table->pll_clks;
	for (i = 0; i < clk_data->table->pll_clks_num; i++) {
		if (id == pll_clks[i].id) {
			/* have to assigne pll_clks.syscon_top first
			 * since clk_register_composite will need it
			 * to calculate current rate.
			 */
			pll_clks[i].syscon_top = clk_data->syscon_top;
			pll_clks[i].lock = &clk_data->lock;
			if (pll_clks[i].ini_flags & BM_CLK_RO)
				local_ops = &bm_clk_pll_ro_ops;
			else
				local_ops = &bm_clk_pll_ops;
			clk = clk_register_composite(
				NULL, pll_clks[i].name, &pll_clks[i].parent_name,
				1, NULL, NULL, &pll_clks[i].hw, local_ops,
				NULL, NULL, pll_clks[i].flags);

			if (IS_ERR(clk)) {
				pr_err("%s: failed to register clock %s\n", __func__,
				       pll_clks[i].name);
				ret = -EINVAL;
				goto out;
			}
			ret = of_clk_add_provider(node, of_clk_src_simple_get, clk);
			if (ret)
				clk_unregister(clk);
		} else {
			continue;
		}
	}

out:
	return ret;
}

/* mux clk init */
int bm_register_mux_clks(struct device_node *node, struct bm_clk_data *clk_data)
{
	int ret;
	int count;
	struct clk **clk_table;

	count = clk_data->table->mux_clks_num + clk_data->table->gate_clks_num;
	clk_table = kcalloc(count, sizeof(*clk_table), GFP_KERNEL);
	if (!clk_table)
		return -ENOMEM;

	clk_data->clk_data.clks = clk_table;
	clk_data->clk_data.clk_num = count;

	ret = register_mux_clks(NULL, clk_data);
	if (ret)
		goto err;

	ret = register_gate_clks(NULL, clk_data);
	if (ret)
		goto err;

	ret = register_provider_clks(node, clk_data, count);
	if (ret)
		goto err;

	return 0;
err:
	kfree(clk_table);
	return ret;
}

/* pll divider init */
int bm_register_div_clks(struct device_node *node, struct bm_clk_data *clk_data)
{
	int ret;
	int count;
	int clk_boot_mode;

	struct clk **clk_table;

	count = clk_data->table->div_clks_num + clk_data->table->gate_clks_num;
	clk_table = kcalloc(count, sizeof(*clk_table), GFP_KERNEL);
	if (!clk_table)
		return -ENOMEM;

	clk_data->clk_data.clks = clk_table;
	clk_data->clk_data.clk_num = count;

	ret = register_divider_clks(NULL, clk_data);
	if (ret)
		goto err;

	ret = register_gate_clks(NULL, clk_data);
	if (ret)
		goto err;

	ret = register_provider_clks(node, clk_data, count);
	if (ret)
		goto err;

	ret = regmap_read(clk_data->syscon_top, CLK_MODE, &clk_boot_mode);
	if (ret < 0)
		goto err;

	ret = set_default_div_clk_rates(node, clk_boot_mode & CLK_MODE_MASK);
	if (ret < 0)
		goto err;

	return 0;
err:
	kfree(clk_table);
	pr_err("%s error %d\n", __func__, ret);
	return ret;
}

/* Below weak define is used in case there is no
 * other bitmain clk driver defined.
 * And that will cause compile error.
 */
struct bm_clk_table pll_clk_tables __weak;
struct bm_clk_table div_clk_tables[1] __weak;
struct bm_clk_table mux_clk_tables[1] __weak;
unsigned int size_div_clk_tables __weak = ARRAY_SIZE(div_clk_tables);
unsigned int size_mux_clk_tables __weak = ARRAY_SIZE(mux_clk_tables);

const struct of_device_id bm_clk_match_ids_tables[] = {
	{
		.compatible = "bitmain, pll-clock",
		.data = &pll_clk_tables,
	},
	{
		.compatible = "bitmain, pll-child-clock",
		.data = div_clk_tables,
	},
	{
		.compatible = "bitmain, pll-mux-clock",
		.data = mux_clk_tables,
	},
	{}
};

static void __init bm_clk_init(struct device_node *node)
{
	struct device_node *np_top;
	struct bm_clk_data *clk_data = NULL;
	const struct bm_clk_table *dev_data;
	static struct regmap *syscon;
	static void __iomem *base;
	int i, ret = 0;
	unsigned int id;
	const struct of_device_id *match = NULL;

	clk_data = kzalloc(sizeof(*clk_data), GFP_KERNEL);
	if (!clk_data) {
		ret = -ENOMEM;
		goto out;
	}
	match = of_match_node(bm_clk_match_ids_tables, node);
	if (match) {
		dev_data = (struct bm_clk_table *)match->data;
	} else {
		pr_err("%s did't match node data\n", __func__);
		ret = -ENODEV;
		goto no_match_data;
	}

	spin_lock_init(&clk_data->lock);
	if (of_device_is_compatible(node, "bitmain, pll-clock")) {
		np_top = of_parse_phandle(node, "subctrl-syscon", 0);
		if (!np_top) {
			pr_err("%s can't get subctrl-syscon node\n",
			       __func__);
			ret = -EINVAL;
			goto no_match_data;
		}

		if (!dev_data->pll_clks_num) {
			ret = -EINVAL;
			goto no_match_data;
		}

		syscon = syscon_node_to_regmap(np_top);
		if (IS_ERR_OR_NULL(syscon)) {
			pr_err("%s cannot get regmap\n", __func__);
			ret = -ENODEV;
			goto no_match_data;
		}

		base = of_iomap(np_top, 0);
		clk_data->table = dev_data;
		clk_data->base = base;
		clk_data->syscon_top = syscon;

		if (of_property_read_u32(node, "id", &id)) {
			pr_err("%s cannot get pll id for %s\n",
			       __func__, node->full_name);
			ret = -ENODEV;
			goto no_match_data;
		}
		ret = bm_register_pll_clks(node, clk_data, id);
	}

	if (of_device_is_compatible(node, "bitmain, pll-child-clock")) {
		ret = of_property_read_u32(node, "id", &id);
		if (ret) {
			pr_err("not assigned id for %s\n", node->full_name);
			ret = -ENODEV;
			goto no_match_data;
		}

		/* Below brute-force to check dts property "id"
		 * whether match id of array
		 */
		for (i = 0; i < size_div_clk_tables; i++) {
			if (id == dev_data[i].id)
				break; /* found */
		}
		clk_data->table = &dev_data[i];
		clk_data->base = base;
		clk_data->syscon_top = syscon;
		ret = bm_register_div_clks(node, clk_data);
	}

	if (of_device_is_compatible(node, "bitmain, pll-mux-clock")) {
		ret = of_property_read_u32(node, "id", &id);
		if (ret) {
			pr_err("not assigned id for %s\n", node->full_name);
			ret = -ENODEV;
			goto no_match_data;
		}

		/* Below brute-force to check dts property "id"
		 * whether match id of array
		 */
		for (i = 0; i < size_div_clk_tables; i++) {
			if (id == dev_data[i].id)
				break; /* found */
		}
		clk_data->table = &dev_data[i];
		clk_data->base = base;
		clk_data->syscon_top = syscon;
		ret = bm_register_mux_clks(node, clk_data);
	}

	if (!ret)
		return;

no_match_data:
	kfree(clk_data);

out:
	pr_err("%s failed error number %d\n", __func__, ret);
}

CLK_OF_DECLARE(bm_clk_pll, "bitmain, pll-clock", bm_clk_init);
CLK_OF_DECLARE(bm_clk_pll_child, "bitmain, pll-child-clock", bm_clk_init);
CLK_OF_DECLARE(bm_clk_pll_mux, "bitmain, pll-mux-clock", bm_clk_init);
