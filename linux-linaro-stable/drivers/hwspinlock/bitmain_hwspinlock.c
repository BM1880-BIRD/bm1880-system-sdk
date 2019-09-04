/*
 * BITMAIN hardware spinlock driver
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/hwspinlock.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/pm_runtime.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>

#include "hwspinlock_internal.h"

static int bm_hwspinlock_trylock(struct hwspinlock *lock)
{
	int ret;
	u32 status;
	struct regmap_field *field = lock->priv;

	ret = regmap_field_read(field, &status);
	if (ret)
		return ret;

	return !!status;
}

static void bm_hwspinlock_unlock(struct hwspinlock *lock)
{
	int ret;
	struct regmap_field *field = lock->priv;

	ret = regmap_field_write(field, 0);
	if (ret)
		pr_err("%s: failed to unlock spinlock\n", __func__);
}

static void bm_hwspinlock_relax(struct hwspinlock *lock)
{
	ndelay(50);
}

static const struct hwspinlock_ops bm_hwspinlock_ops = {
	.trylock = bm_hwspinlock_trylock,
	.unlock = bm_hwspinlock_unlock,
	.relax = bm_hwspinlock_relax,
};

static int bm_hwspinlock_probe(struct platform_device *pdev)
{
	int num_locks, i, ret;
	unsigned int base, reg_width;
	struct hwspinlock_device *bank;
	struct device_node *syscon;
	struct reg_field field;
	struct regmap *regmap;

	if (!pdev->dev.of_node)
		return -ENODEV;

	syscon = of_parse_phandle(pdev->dev.of_node, "subctrl-syscon", 0);
	if (!syscon) {
		dev_err(&pdev->dev, "no subctrl-syscon property\n");
		return -ENODEV;
	}

	regmap = syscon_node_to_regmap(syscon);
	of_node_put(syscon);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	ret = of_property_read_u32_index(pdev->dev.of_node, "subctrl-syscon", 1, &base);
	if (ret < 0) {
		dev_err(&pdev->dev, "no offset in subctrl-syscon\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_index(pdev->dev.of_node, "subctrl-syscon", 2, &reg_width);
	if (ret < 0) {
		dev_err(&pdev->dev, "no reg_width subctrl-syscon\n");
		return -EINVAL;
	}

	ret = device_property_read_u32(&pdev->dev, "spinlock_numbers", &num_locks);
	if (ret)
		goto err;

	bank = devm_kzalloc(&pdev->dev, sizeof(*bank) + num_locks * sizeof(struct hwspinlock),
			    GFP_KERNEL);
	if (!bank) {
		ret = -ENOMEM;
		goto err;
	}

	pm_runtime_enable(&pdev->dev);

	for (i = 0; i < num_locks; i++) {
		field.reg = base + i * reg_width;
		field.lsb = 0;
		field.msb = 31;
		bank->lock[i].priv = devm_regmap_field_alloc(&pdev->dev, regmap, field);
	}

	ret = hwspin_lock_register(bank, &pdev->dev, &bm_hwspinlock_ops, 0, num_locks);
	if (ret) {
		pm_runtime_disable(&pdev->dev);
		goto err;
	}

	platform_set_drvdata(pdev, bank);

	return 0;

err:
	dev_err(&pdev->dev, "%s failed: %d\n", __func__, ret);
	return ret;
}

static int bm_hwspinlock_remove(struct platform_device *pdev)
{
	struct hwspinlock_device *bank = platform_get_drvdata(pdev);
	int ret;

	ret = hwspin_lock_unregister(bank);
	if (ret) {
		dev_err(&pdev->dev, "%s failed: %d\n", __func__, ret);
		return ret;
	}

	pm_runtime_disable(&pdev->dev);

	return 0;
}

static const struct of_device_id bm_hwpinlock_ids[] = {
	{
		.compatible = "bitmain,hwspinlock",
	},
	{},
};
MODULE_DEVICE_TABLE(of, bm_hwpinlock_ids);

static struct platform_driver bm_hwspinlock_driver = {
	.probe = bm_hwspinlock_probe,
	.remove = bm_hwspinlock_remove,
	.driver = {
			.name = "bitmain_hwspinlock",
			.of_match_table = of_match_ptr(bm_hwpinlock_ids),
	},
};

module_platform_driver(bm_hwspinlock_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("BITMAIN Hardware spinlock driver");
