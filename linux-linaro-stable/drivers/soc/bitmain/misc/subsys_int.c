#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <soc/bitmain/subsys_int.h>

#define DMA_OFFSET 0

enum {
	UART0,
	UART1,
	UART2,
	UART3
};

u32 __iomem *subsys_int_reg;
struct subsys_int {
	void __iomem *membase;
	unsigned char i2s_int_offset;
	unsigned char i2s_int_mask;
	unsigned char uart_int_offset;
	unsigned char uart_int_mask;
};

static struct subsys_int *sysint;

void bm_enable_i2s_int(int port, bool en)
{
	u32 v = readl(subsys_int_reg);
	unsigned int value = sysint->i2s_int_mask << (sysint->i2s_int_offset +
		(__fls(sysint->i2s_int_mask) + 1) * port);

	writel(en ? (v | value) : (v & ~value), subsys_int_reg);
}

void bm_enable_uart_dma_int(int port, bool en)
{
	u32 v = readl(subsys_int_reg);
	unsigned int value = sysint->uart_int_mask << (sysint->uart_int_offset +
		(__fls(sysint->uart_int_mask) + 1) * port);

	v |= (1 << DMA_OFFSET);
	writel(en ? (v | value) : (v & ~value),	subsys_int_reg);
}

void bm_enable_uart_int(int port, bool en)
{
	u32 v = readl(subsys_int_reg);
	unsigned int value = sysint->uart_int_mask << (sysint->uart_int_offset +
		(__fls(sysint->uart_int_mask) + 1) * port);

	writel(en ? (v | value) : (v & ~value), subsys_int_reg);
}

void bm_enable_sysdma_int(bool en)
{
	u32 v = readl(subsys_int_reg);
	unsigned int value = (1 << DMA_OFFSET);

	writel(en ? (v | value) : (v & ~value), subsys_int_reg);
}

static int subsys_int_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	u32 val;
	int err;

	sysint = kzalloc(sizeof(*sysint), GFP_KERNEL);

	err = device_property_read_u32(dev, "regbase", &val);
	if (!err) {
		subsys_int_reg = ioremap(val, 0x8);
		if (!subsys_int_reg)
			return -ENOMEM;
		sysint->membase = subsys_int_reg;
	}
	err = device_property_read_u32(dev, "uart-int-offset", &val);
	if (!err)
		sysint->uart_int_offset = val;

	err = device_property_read_u32(dev, "uart-int-mask", &val);
	if (!err)
		sysint->uart_int_mask = val;

	err = device_property_read_u32(dev, "i2s-int-offset", &val);
	if (!err)
		sysint->i2s_int_offset = val;

	err = device_property_read_u32(dev, "i2s-int-mask", &val);
	if (!err)
		sysint->i2s_int_mask = val;
	bm_enable_uart_int(UART0, true);

	return 0;
}

static const struct of_device_id subsys_int_match[] = {
	{
		.compatible = "bitmain,subsys-int-en",
	},
	{},
};

static struct platform_driver subsys_int_driver = {
	.driver = {
		.name = "bm-subsys-int",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(subsys_int_match),
	},
	.probe = subsys_int_probe,
};

static int __init subsys_int_init(void)
{
	return platform_driver_register(&subsys_int_driver);
}

arch_initcall(subsys_int_init);
