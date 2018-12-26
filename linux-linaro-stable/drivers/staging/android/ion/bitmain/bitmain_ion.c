/*
 * Bitmain SoC ion dirver
 *
 * Copyright (c) 2018 Bitmain Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#define pr_fmt(fmt) "Ion: " fmt

#include <linux/err.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/cma.h>
#include <linux/dma-contiguous.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <asm/cacheflush.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/genalloc.h>

#include "bitmain_ion.h"
#include "../../uapi/ion_bitmain.h"

struct ion_carveout_heap { // workaround to get private info of heap
	struct ion_heap heap;
	struct gen_pool *pool;
	phys_addr_t base;
};

struct ion_of_heap {
	const char *compat;
	int heap_id;
	int type;
	const char *name;
	int align;
};

#define PLATFORM_HEAP(_compat, _id, _type, _name) \
{ \
	.compat = _compat, \
	.heap_id = _id, \
	.type = _type, \
	.name = _name, \
	.align = PAGE_SIZE, \
}

struct ion_platform_data {
	int nr;
	struct ion_platform_heap *heaps;
};

struct bm_ion_dev {
	struct ion_heap	**heaps;
	struct ion_device *idev;
	struct ion_platform_data *plat_data;
};

static struct ion_of_heap bm_ion_heap_list[] = {
	// heap id in ion_platform_heap is not used, id in ion_heap is assigned in ion_device_add_heap
	PLATFORM_HEAP("bitmain,carveout", 0, ION_HEAP_TYPE_CARVEOUT, "carveout"),
	{}
};

static int ion_parse_dt_heap_common(struct device_node *heap_node,
				    struct ion_platform_heap *heap,
				    struct ion_of_heap *compatible)
{
	int i;

	for (i = 0; compatible[i].name; i++) {
		if (of_device_is_compatible(heap_node, compatible[i].compat))
			break;
	}

	if (!compatible[i].name)
		return -ENODEV;

	heap->id = compatible[i].heap_id;
	heap->type = compatible[i].type;
	heap->name = compatible[i].name;
	heap->align = compatible[i].align;

	/* Some kind of callback function pointer? */

	pr_info("%s: id %d type %d name %s align %llx\n", __func__,
		heap->id, heap->type, heap->name, heap->align);
	return 0;
}

static int ion_setup_heap_common(struct platform_device *parent,
				 struct device_node *heap_node,
				 struct ion_platform_heap *heap)
{
	int ret = 0;

	switch (heap->type) {
	case ION_HEAP_TYPE_CARVEOUT:
	case ION_HEAP_TYPE_CHUNK:
		if (heap->base && heap->size)
			return 0;

		ret = of_reserved_mem_device_init(heap->priv);
		break;
	default:
		break;
	}

	return ret;
}

struct ion_platform_data *ion_parse_dt(struct platform_device *pdev,
				       struct ion_of_heap *compatible)
{
	int num_heaps, ret;
	const struct device_node *dt_node = pdev->dev.of_node;
	struct device_node *node;
	struct ion_platform_heap *heaps;
	struct ion_platform_data *data;
	int i = 0;

	num_heaps = of_get_available_child_count(dt_node);

	if (!num_heaps)
		return ERR_PTR(-EINVAL);

	heaps = devm_kzalloc(&pdev->dev,
			     sizeof(struct ion_platform_heap) * num_heaps,
			     GFP_KERNEL);
	if (!heaps)
		return ERR_PTR(-ENOMEM);

	data = devm_kzalloc(&pdev->dev, sizeof(struct ion_platform_data),
			    GFP_KERNEL);
	if (!data)
		return ERR_PTR(-ENOMEM);

	for_each_available_child_of_node(dt_node, node) {
		struct platform_device *heap_pdev;

		ret = ion_parse_dt_heap_common(node, &heaps[i], compatible);
		if (ret)
			return ERR_PTR(ret);

		heap_pdev = of_platform_device_create(node, heaps[i].name,
						      &pdev->dev);
		if (!heap_pdev)
			return ERR_PTR(-ENOMEM);
		heap_pdev->dev.platform_data = &heaps[i];

		heaps[i].priv = &heap_pdev->dev;

		ret = ion_setup_heap_common(pdev, node, &heaps[i]);
		if (ret)
			goto out_err;
		i++;
	}

	data->heaps = heaps;
	data->nr = num_heaps;
	return data;

out_err:
	for ( ; i >= 0; i--)
		if (heaps[i].priv)
			of_device_unregister(to_platform_device(heaps[i].priv));

	return ERR_PTR(ret);
}

void ion_destroy_platform_data(struct ion_platform_data *data)
{
	int i;

	for (i = 0; i < data->nr; i++)
		if (data->heaps[i].priv)
			of_device_unregister(to_platform_device(
				data->heaps[i].priv));
}

static int bitmain_get_heap_info(struct ion_device *dev, struct bitmain_heap_info *info)
{
	struct ion_heap *heap;

	if (info->id >= dev->heap_cnt)
		return -EFAULT;

	info->total_size = 0;
	info->avail_size = 0;

	plist_for_each_entry(heap, &dev->heaps, node) {
		if (heap->id == info->id) {
			switch (heap->type) {
			case ION_HEAP_TYPE_CARVEOUT:
			{
				struct ion_carveout_heap *carveout_heap;

				carveout_heap = container_of(heap, struct ion_carveout_heap, heap);
				info->total_size = gen_pool_size(carveout_heap->pool);
				info->avail_size = gen_pool_avail(carveout_heap->pool);
				break;
			}
			default:
				break;
			}
			return 0;
		}
	}

	return -1;
}

long bitmain_ion_ioctl(struct ion_device *dev, unsigned int cmd, unsigned long arg)
{
	long ret = 0;

	switch (cmd) {
	case ION_IOC_BITMAIN_FLUSH_RANGE:
	{
		struct bitmain_cache_range data;

		if (copy_from_user(&data, (void __user *)arg, sizeof(data)))
			return -EFAULT;

		if (IS_ERR(data.start)) {
			pr_err("flush fault addr %p, size %zu!\n",
				data.start, data.size);
			return -EFAULT;
		}
		pr_debug("flush addr %p, size %zu\n", data.start, data.size);

		__flush_dcache_area(data.start, data.size);
		break;
	}
	case ION_IOC_BITMAIN_GET_HEAP_INFO:
	{
		struct bitmain_heap_info data;

		if (copy_from_user(&data, (void __user *)arg, sizeof(data)))
			return -EFAULT;

		if (bitmain_get_heap_info(dev, &data) == 0) {
			if (copy_to_user((void __user *)arg, &data, sizeof(data)))
				return -EFAULT;
		} else {
			return -EINVAL;
		}
		break;
	}
	default:
		return -ENOTTY;
	}

	return ret;
}

static struct mutex debugfs_mutex;

static int bm_ion_debug_heap_show(struct seq_file *s, void *unused)
{
	struct bm_ion_dev *ipdev = s->private;
	struct ion_device *dev = ipdev->idev;
	struct ion_heap *heap;
	struct rb_node *n;

	mutex_lock(&debugfs_mutex);
	seq_puts(s, "Summary:\n");
	plist_for_each_entry(heap, &dev->heaps, node) {
		switch (heap->type) {
		case ION_HEAP_TYPE_CARVEOUT:
		{
			struct ion_carveout_heap *carveout_heap;
			size_t total_size;
			size_t alloc_size;
			int usage_rate;

			carveout_heap = container_of(heap, struct ion_carveout_heap, heap);
			total_size = gen_pool_size(carveout_heap->pool);
			alloc_size = total_size - gen_pool_avail(carveout_heap->pool);
			usage_rate = alloc_size * 100 / total_size;
			if ((alloc_size * 100) % total_size)
				usage_rate += 1;
			seq_printf(s, "[%d] %s heap size:%ld bytes, used:%ld bytes, usage rate:%d%%\n",
				   heap->id, heap->name, total_size, alloc_size, usage_rate);
			break;
		}
		default:
			break;
		}
	}

	seq_printf(s, "\nDetails:\n%16s %16s %16s %16s\n", "heap_id", "alloc_buf_size", "phy_addr", "kmap_cnt");
	for (n = rb_first(&dev->buffers); n; n = rb_next(n)) {
		struct ion_buffer *buffer = rb_entry(n, struct ion_buffer, node);

		seq_printf(s, "%16d %16zu %16llx %16d\n",
			   buffer->heap->id, buffer->size, buffer->paddr, buffer->kmap_cnt);
	}
	seq_puts(s, "\n");
	mutex_unlock(&debugfs_mutex);

	return 0;
}

static int bm_ion_debug_heap_open(struct inode *inode, struct file *file)
{
	return single_open(file, bm_ion_debug_heap_show, inode->i_private);
}

static const struct file_operations debug_heap_fops = {
	.open = bm_ion_debug_heap_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static void bm_ion_create_debug_info(struct bm_ion_dev *ipdev, struct ion_heap *heap)
{
	struct dentry *debug_file;
	struct ion_device *dev = heap->dev;
	char debug_heap_name[64];

	mutex_init(&debugfs_mutex);
	snprintf(debug_heap_name, 64, "bm_%s_heap_dump", heap->name);
	debug_file = debugfs_create_file(
		debug_heap_name, 0644, dev->debug_root, ipdev,
		&debug_heap_fops);
	if (!debug_file) {
		char buf[256], *path;

		path = dentry_path(dev->debug_root, buf, 256);
		pr_err("Failed to create heap debugfs at %s/%s\n", path, debug_heap_name);
	}
}

static int bitmain_ion_probe(struct platform_device *pdev)
{
	struct bm_ion_dev *ipdev;
	int i;

	ipdev = devm_kzalloc(&pdev->dev, sizeof(*ipdev), GFP_KERNEL);
	if (!ipdev)
		return -ENOMEM;

	platform_set_drvdata(pdev, ipdev);

	ipdev->plat_data = ion_parse_dt(pdev, bm_ion_heap_list);
	if (IS_ERR(ipdev->plat_data))
		return PTR_ERR(ipdev->plat_data);

	ipdev->heaps = devm_kzalloc(&pdev->dev,
				sizeof(struct ion_heap *) * ipdev->plat_data->nr,
				GFP_KERNEL);
	if (!ipdev->heaps) {
		ion_destroy_platform_data(ipdev->plat_data);
		return -ENOMEM;
	}

	for (i = 0; i < ipdev->plat_data->nr; i++) {
		if (ipdev->plat_data->heaps[i].type ==  ION_HEAP_TYPE_CARVEOUT) {
			ipdev->heaps[i] = ion_carveout_heap_create(&ipdev->plat_data->heaps[i]);
			if (IS_ERR(ipdev->heaps[i])) {
				kfree(ipdev->heaps);
				ion_destroy_platform_data(ipdev->plat_data);
				return PTR_ERR(ipdev->heaps[i]);
			}
			pr_info("add heap %d, type %d, base 0x%llx, size 0x%lx\n",
				ipdev->heaps[i]->id, ipdev->heaps[i]->type,
				ipdev->plat_data->heaps[i].base,
				ipdev->plat_data->heaps[i].size);
			ion_device_add_heap(ipdev->heaps[i]);

			if (!ipdev->idev)
				ipdev->idev = ipdev->heaps[i]->dev;
			if (!ipdev->heaps[i]->dev->custom_ioctl)
				ipdev->heaps[i]->dev->custom_ioctl = bitmain_ion_ioctl;

			/* create debugfs to show heap/buffer info */
			bm_ion_create_debug_info(ipdev, ipdev->heaps[i]);
		}
	}
	return 0;
}

static int bitmain_ion_remove(struct platform_device *pdev)
{
	struct bm_ion_dev *ipdev;

	ipdev = platform_get_drvdata(pdev);
	kfree(ipdev->heaps);
	ion_destroy_platform_data(ipdev->plat_data);

	return 0;
}

static const struct of_device_id bitmain_ion_match_table[] = {
	{.compatible = "bitmain,bitmain-ion"},
	{},
};

static struct platform_driver bitmain_ion_driver = {
	.probe = bitmain_ion_probe,
	.remove = bitmain_ion_remove,
	.driver = {
		.name = "ion-bitmain",
		.of_match_table = bitmain_ion_match_table,
	},
};

static int __init bitmain_ion_init(void)
{
	return platform_driver_register(&bitmain_ion_driver);
}

subsys_initcall(bitmain_ion_init);

#ifdef CONFIG_OF_RESERVED_MEM
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>

static int rmem_ion_device_init(struct reserved_mem *rmem, struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct ion_platform_heap *heap = pdev->dev.platform_data;

	heap->base = rmem->base;
	heap->size = rmem->size;
	pr_info("%s: heap %s base %pa size %pa dev %p\n", __func__,
		heap->name, &rmem->base, &rmem->size, dev);
	return 0;
}

static void rmem_ion_device_release(struct reserved_mem *rmem,
				    struct device *dev)
{
}

static const struct reserved_mem_ops rmem_dma_ops = {
	.device_init	= rmem_ion_device_init,
	.device_release	= rmem_ion_device_release,
};

static int __init rmem_ion_setup(struct reserved_mem *rmem)
{
	phys_addr_t size = rmem->size;

	size = size / 1024 / 1024;

	pr_info("Ion memory setup at %pa size %ld MiB\n",
		&rmem->base, (unsigned long)size);
	rmem->ops = &rmem_dma_ops;
	return 0;
}

RESERVEDMEM_OF_DECLARE(ion, "ion-region", rmem_ion_setup);
#endif

