#include <ce_inst.h>
#include <ce_cipher.h>
#include <ce_hash.h>

static struct ce_inst *inst;

int ce_enqueue_request(struct crypto_async_request *req)
{
	return ce_inst_enqueue_request(inst, req);
}

static int ce_probe(struct platform_device *dev)
{
	int err = -EINVAL;

	err = ce_inst_new(&inst, dev);
	if (err)
		return err;

	err = register_all_alg() | register_all_hash();
	if (err) {
		ce_err("register algrithm failed");
		goto err_nop;
	}
	ce_info("bitmain crypto engine prob success\n");
	return 0;
err_nop:
	return err;
}
static int ce_remove(struct platform_device *dev)
{
	ce_inst_free(platform_get_drvdata(dev));
	unregister_all_alg();
	unregister_all_hash();
	return 0;
}
static void ce_shutdown(struct platform_device *dev)
{
}
static int ce_suspend(struct device *dev)
{
	return 0;
}
static int ce_resume(struct device *dev)
{
	return 0;
}

static SIMPLE_DEV_PM_OPS(ce_pm, ce_suspend, ce_resume);
static const struct of_device_id ce_match[] = {
	{ .compatible = "bitmain,crypto-engine", },
	{ .compatible = "bitmain,spacc", },
	{ .compatible = "bitmain,ce", },
	{ },
};

static struct platform_driver ce_driver = {
	.probe		= ce_probe,
	.remove		= ce_remove,
	.shutdown	= ce_shutdown,
	.driver		= {
		.name		= "bce",
		.of_match_table	= ce_match,
		.pm		= &ce_pm,
	},
};

module_platform_driver(ce_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("<chao.wei@bitmain.com>");
MODULE_DESCRIPTION("Bitmain Crypto Engine");
MODULE_VERSION("0.1");
