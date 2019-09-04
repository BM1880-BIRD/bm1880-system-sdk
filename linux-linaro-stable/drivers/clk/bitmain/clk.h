#ifndef __BITMAIN_CLOCK__
#define __BITMAIN_CLOCK__

#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/device.h>
#include <linux/clkdev.h>

#include <dt-bindings/clock/bitmain.h>

#define KHZ 1000L
#define MHZ (KHZ * KHZ)

#define BM_CLK_USE_INIT_VAL		BIT(0) /* use default value */
#define BM_CLK_USE_REG_VAL		BIT(1) /* use reg divider value */
#define BM_CLK_RO			BIT(2) /* use reg divider value */

#define CLK_PLL	BIT(0)
#define CLK_MUX	BIT(1)

#define PLL_CTRL_OFFSET	0xE8
#define PLL_STAT_LOCK_OFFSET	0x8
#define CLK_MODE	0x4
#define CLK_MODE_MASK	0x3

#define REFDIV_MIN 1
#define REFDIV_MAX 64
#define FBDIV_MIN 16
#define FBDIV_MAX 321

#define PLL_FREQ_MIN (16 * MHZ)
#define PLL_FREQ_MAX (3200 * MHZ)

#define div_mask(width) ((1 << (width)) - 1)
#define TOP_PLL_CTRL(fbdiv, p1, p2, refdiv) \
	(((fbdiv & 0xfff) << 16) | ((p2 & 0x7) << 12) | ((p1 & 0x7) << 8) | (refdiv & 0x1f))

struct bm_pll_ctrl {
	unsigned int mode;
	unsigned long freq;

	unsigned int fbdiv;
	unsigned int postdiv1;
	unsigned int postdiv2;
	unsigned int refdiv;
};

struct bm_pll_clock {
	unsigned int id;
	char *name;
	const char *parent_name;
	unsigned long flags;
	struct clk_hw	hw;
	struct regmap *syscon_top;

	/* Below lock used to protect PLL top register during write */
	spinlock_t *lock;
	u32 ini_flags;

	u32 status_offset;
	u32 enable_offset;

	struct bm_pll_ctrl pctrl_table[4];
};

#define to_bm_pll_clk(_hw) container_of(_hw, struct bm_pll_clock, hw)

struct bm_divider_clock {
	unsigned int id;
	const char *name;
	const char *parent_name;
	unsigned long flags;
	unsigned long offset;
	u8 shift;
	u8 width;
	u8 div_flags;
	u32 initial_sel;
	u32 initial_val;
	struct clk_div_table *table;
};

struct bm_mux_clock {
	unsigned int id;
	const char *name;
	const char *const *parent_names;
	u8 num_parents;
	unsigned long flags;
	unsigned long offset;
	u8 shift;
	u8 width;
	u8 mux_flags;
	u32 *table;
};

struct bm_gate_clock {
	unsigned int id;
	const char *name;
	const char *parent_name;
	unsigned long flags;
	unsigned long offset;
	u8 bit_idx;
	u8 gate_flags;
	const char *alias;
};

struct bm_clk_table {
	u32 id;
	u32 pll_clks_num;
	u32 div_clks_num;
	u32 gate_clks_num;
	u32 mux_clks_num;

	const struct bm_pll_clock *pll_clks;
	const struct bm_divider_clock *div_clks;
	const struct bm_gate_clock *gate_clks;
	const struct bm_mux_clock *mux_clks;
};

struct bm_clk_data {
	void __iomem *base;
	spinlock_t lock;
	struct regmap *syscon_top;
	struct clk_onecell_data	clk_data;
	const struct bm_clk_table *table;
};

int bm_register_mux_clks
(struct device_node *node, struct bm_clk_data *clk_data);
int bm_register_div_clks
(struct device_node *node, struct bm_clk_data *clk_data);
int bm_register_pll_clks
(struct device_node *node, struct bm_clk_data *clk_data, unsigned int id);

extern struct bm_clk_table pll_clk_tables;
extern struct bm_clk_table div_clk_tables[];
extern struct bm_clk_table mux_clk_tables[];

extern unsigned int size_div_clk_tables;
extern unsigned int size_mux_clk_tables;
#endif
