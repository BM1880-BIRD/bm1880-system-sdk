#include <linux/string.h>
#include <linux/log2.h>
#include "clk.h"

#define div_mask(width)	((1 << (width)) - 1)

static int bm_pll_write(struct regmap *map, int id, int value)
{
	if (id != MPLL_CLK)
		return 0;

	return regmap_write(map, PLL_CTRL_OFFSET + (id  << 2), value);
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
	struct clk_divider *divider = to_clk_divider(hw);
	unsigned int val;

	val = clk_readl(divider->reg) >> divider->shift;
	val &= div_mask(divider->width);

	return divider_recalc_rate(hw, parent_rate, val, divider->table,
				divider->flags);
}

static long bm_clk_divider_round_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long *prate)
{
	int bestdiv;
	struct clk_divider *divider = to_clk_divider(hw);

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
	struct clk_divider *divider = to_clk_divider(hw);

	value = divider_get_val(rate, parent_rate, divider->table,
				divider->width, divider->flags);
	if (divider->lock)
		spin_lock_irqsave(divider->lock, flags);
	else
		__acquire(divider->lock);

	/* div assert */
	val = clk_readl(divider->reg);
	val = ((val >> 1) << 1);
	clk_writel(val, divider->reg);

	if (divider->flags & CLK_DIVIDER_HIWORD_MASK) {
		val = div_mask(divider->width) << (divider->shift + 16);
	} else {
		val = clk_readl(divider->reg);
		val &= ~(div_mask(divider->width) << divider->shift);
	}

	val |= value << divider->shift;
	clk_writel(val, divider->reg);

	/* de-assert */
	val |= 1;
	clk_writel(val, divider->reg);

	if (divider->lock)
		spin_unlock_irqrestore(divider->lock, flags);
	else
		__release(divider->lock);

	return 0;
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

static struct clk *__register_divider_clks(struct device *dev, const char *name,
					   const char *parent_name,
					   unsigned long flags,
					   void __iomem *reg, u8 shift,
					   u8 width, u8 clk_divider_flags,
					   const struct clk_div_table *table,
					   spinlock_t *lock)
{
	struct clk_divider *div;
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

	/* struct clk_divider assignments */
	div->reg = reg;
	div->shift = shift;
	div->width = width;
	div->flags = clk_divider_flags;
	div->lock = lock;
	div->hw.init = &init;
	div->table = table;

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

static int register_provider_clks(struct device *dev, struct bm_clk_data *clk_data, int clk_num)
{
	struct device_node *np = dev->of_node;

	return of_clk_add_provider(np, of_clk_src_onecell_get, &clk_data->clk_data);
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
		clk = clk_register_gate(dev, gate_clks[i].name,
					gate_clks[i].parent_name,
					gate_clks[i].flags,
					base + gate_clks[i].offset,
					gate_clks[i].bit_idx,
					gate_clks[i].gate_flags,
					&clk_data->lock);
		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s\n",
				__func__, gate_clks[i].name);
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
		clk = __register_divider_clks(dev, div_clks[i].name,
					div_clks[i].parent_name,
					div_clks[i].flags,
					base + div_clks[i].offset,
					div_clks[i].shift, div_clks[i].width,
					(div_clks[i].initial_sel & CLK_USE_INIT_VAL) ?
					div_clks[i].div_flags | CLK_DIVIDER_READ_ONLY : div_clks[i].div_flags,
					div_clks[i].table,
					 &clk_data->lock);
		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s\n",
				__func__, div_clks[i].name);
			goto err;
		}

		clk_data->clk_data.clks[div_clks[i].id] = clk;

		if (div_clks[i].initial_sel == CLK_USE_REG_VAL) {
			regmap_read(clk_data->syscon_top, div_clks[i].offset, &val);
			regmap_write(clk_data->syscon_top, div_clks[i].offset, val | (1 << 3));
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

		clk = clk_register_mux_table(dev, mux_clks[i].name,
					     mux_clks[i].parent_names,
					     mux_clks[i].num_parents,
					     mux_clks[i].flags,
					     base + mux_clks[i].offset,
					     mux_clks[i].shift, mask,
					     mux_clks[i].mux_flags,
					     mux_clks[i].table,
					     &clk_data->lock);
		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s\n",
				__func__, mux_clks[i].name);
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

/* pll fixed clock init */
int bm_register_pll_fixed_clks(struct device *dev, struct bm_clk_data *clk_data)
{
	struct clk *clk;
	const struct bm_fixed_rate_clock *fixed_clks;
	const struct bm_pll_ctrl *pllctrl;
	int i;
	int ret;
	unsigned int value;

	fixed_clks = (struct bm_fixed_rate_clock *)clk_data->table->fixed_clks;

	for (i = 0; i < clk_data->table->fixed_clks_num; i++) {
		pllctrl = &fixed_clks[i].pctrl_table[clk_data->mode];
		value = TOP_PLL_CTRL(pllctrl->fbdiv, pllctrl->postdiv1,
					pllctrl->postdiv2, pllctrl->refdiv);
		clk = clk_register_fixed_rate(dev, fixed_clks[i].name,
					      fixed_clks[i].parent_name,
					      fixed_clks[i].flags,
					      fixed_clks[i].pctrl_table[clk_data->mode].freq);
		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s\n",
				__func__, fixed_clks[i].name);
			goto err;
		}
		clk_data->clk_data.clks[fixed_clks[i].id] = clk;

		bm_pll_write(clk_data->syscon_top, fixed_clks[i].id, value);
	}

	ret = register_provider_clks(dev, clk_data, clk_data->table->fixed_clks_num);
	if (ret) {
		pr_err("line %d register provider error\n", __LINE__);
		return -EINVAL;
	}

	return 0;

err:
	while (i--)
		clk_unregister_fixed_rate(clk_data->clk_data.clks[fixed_clks[i].id]);

	return PTR_ERR(clk);
}

/* mux clk init */
int bm_register_mux_clks(struct device *dev, struct bm_clk_data *clk_data)
{
	int ret;
	int count;
	struct clk **clk_table;

	count = clk_data->table->mux_clks_num + clk_data->table->gate_clks_num;
	clk_table = devm_kcalloc(dev, count, sizeof(*clk_table), GFP_KERNEL);
	if (!clk_table)
		return -ENOMEM;

	clk_data->clk_data.clks = clk_table;
	clk_data->clk_data.clk_num = count;

	ret = register_mux_clks(dev, clk_data);
	if (ret)
		goto err;

	ret = register_gate_clks(dev, clk_data);
	if (ret)
		goto err;

	ret = register_provider_clks(dev, clk_data, count);
	if (ret)
		goto err;

	return 0;
err:
	pr_err("%s error %d\n", __func__, ret);
	return ret;
}

/* pll divider init */
int bm_register_pll_clks(struct device *dev, struct bm_clk_data *clk_data)
{
	int ret;
	int count;
	struct clk **clk_table;

	count = clk_data->table->div_clks_num + clk_data->table->gate_clks_num;
	clk_table = devm_kcalloc(dev, count, sizeof(*clk_table), GFP_KERNEL);
	if (!clk_table)
		return -ENOMEM;

	clk_data->clk_data.clks = clk_table;
	clk_data->clk_data.clk_num = count;

	ret = register_divider_clks(dev, clk_data);
	if (ret)
		goto err;

	ret = register_gate_clks(dev, clk_data);
	if (ret)
		goto err;

	ret = register_provider_clks(dev, clk_data, count);
	if (ret)
		goto err;

	return 0;
err:
	pr_err("%s error ret %d\n", __func__, ret);
	return ret;
}
