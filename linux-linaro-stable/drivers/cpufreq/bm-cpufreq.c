/* drivers/cpufreq/bm-cpufreq.c
 *
 * Bitmain architecture cpufreq driver
 *
 * Copyright (C) 2019 , Bitmain Inc.
 * Copyright (c) 2019 , The Linux Foundation. All rights reserved.
 * Author: xiao hui <hui.xiao@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License , as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/suspend.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>

static DEFINE_PER_CPU(struct cpufreq_frequency_table *, freq_table);

static int bm_cpufreq_target(struct cpufreq_policy *policy,
				unsigned int target_freq,
				unsigned int relation)
{
	return 0;
}

static int bm_cpufreq_verify(struct cpufreq_policy *policy)
{
	cpufreq_verify_within_limits(policy, policy->cpuinfo.min_freq,
			policy->cpuinfo.max_freq);
	return 0;
}

static unsigned int bm_cpufreq_get_freq(unsigned int cpu)
{
	int freq;
	struct cpufreq_frequency_table *ftbl;

	freq = bm_get_cpufreq() * 1000;
	if (!freq) {
		ftbl = per_cpu(freq_table, cpu);
		freq = ftbl[0].frequency;
	}
	return freq;
}

static int bm_cpufreq_init(struct cpufreq_policy *policy)
{
	int cur_freq;
	int ret = 0;
	struct cpufreq_frequency_table *table =
			per_cpu(freq_table, policy->cpu);
	int cpu;

	ret = cpufreq_table_validate_and_show(policy, table);
	if (ret) {
		pr_err("cpufreq: failed to get policy min/max\n");
		return ret;
	}

	cur_freq = bm_cpufreq_get_freq(cpu);

	pr_debug("cpufreq: cpu%d init at %d switching to %d\n",
			policy->cpu, cur_freq);
	policy->cur = cur_freq;

	return 0;
}

static struct freq_attr *bm_freq_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL,
};

static struct cpufreq_driver bm_cpufreq_driver = {
	/* lps calculations are handled here. */
	.flags		= CPUFREQ_STICKY | CPUFREQ_NEED_INITIAL_FREQ_CHECK,
	.init		= bm_cpufreq_init,
	.verify		= bm_cpufreq_verify,
	.target		= bm_cpufreq_target,
	.get		= bm_cpufreq_get_freq,
	.name		= "bm",
	.attr		= bm_freq_attr,
};

/* Parse list of usable CPU frequencies. */
static struct cpufreq_frequency_table *cpufreq_parse_dt(struct device *dev,
						char *tbl_name, int cpu)
{
	int ret, nf, i, j;
	u32 *data;
	struct cpufreq_frequency_table *ftbl;

	if (!of_find_property(dev->of_node, tbl_name, &nf)) {
		ftbl = ERR_PTR(-EINVAL);
		goto out2;
	}

	nf /= sizeof(*data);

	if (nf == 0) {
		ftbl = ERR_PTR(-EINVAL);
		goto out2;
	}

	data = devm_kzalloc(dev, nf * sizeof(*data), GFP_KERNEL);
	if (!data) {
		ftbl = ERR_PTR(-ENOMEM);
		goto out2;
	}

	ret = of_property_read_u32_array(dev->of_node, tbl_name, data, nf);
	if (ret) {
		ftbl = ERR_PTR(ret);
		goto out1;
	}

	ftbl = devm_kzalloc(dev, (nf + 1) * sizeof(*ftbl), GFP_KERNEL);
	if (!ftbl) {
		ftbl = ERR_PTR(-ENOMEM);
		goto out1;
	}

	j = 0;
	for (i = 0; i < nf; i++) {
		ftbl[j].driver_data = j;
		ftbl[j].frequency = data[i];
		j++;
	}

	ftbl[j].driver_data = j;
	ftbl[j].frequency = CPUFREQ_TABLE_END;

out1:
	devm_kfree(dev, data);
out2:
	return ftbl;

}

static int bm_cpufreq_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct cpufreq_frequency_table *ftbl;
	int ret = -ENODEV;
	int cpu;

	/* Parse commong cpufreq table for all CPUs */
	ftbl = cpufreq_parse_dt(dev, "bitmain,cpufreq-table", 0);
	if (!IS_ERR(ftbl)) {
		for_each_possible_cpu(cpu)
			per_cpu(freq_table, cpu) = ftbl;
		ret = cpufreq_register_driver(&bm_cpufreq_driver);
	}

	return ret;
}

static const struct of_device_id bm_cpufreq_match_table[] = {
	{ .compatible = "bitmain,bm-cpufreq" },
	{}
};

static struct platform_driver bm_cpufreq_plat_driver = {
	.probe = bm_cpufreq_probe,
	.driver = {
		.name = "bm-cpufreq",
		.of_match_table = bm_cpufreq_match_table,
	},
};

static int __init bm_cpufreq_register(void)
{
	int rc;

	rc = platform_driver_register(&bm_cpufreq_plat_driver);
	if (rc < 0) {
		pr_err("bm_cpufreq_register error:%d\n", rc);
		return rc;
	}

	return 0;
}

subsys_initcall(bm_cpufreq_register);

MODULE_DESCRIPTION("cpufreq for bitmain soc platform");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("hui.xiao@bitmain.com>");

