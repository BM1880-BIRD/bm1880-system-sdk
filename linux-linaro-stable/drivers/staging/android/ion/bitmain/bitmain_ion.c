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
#include <linux/uaccess.h>
#include <asm/cacheflush.h>
#include <asm/pgtable.h>
#include <linux/mm.h>
#include <linux/swap.h>

#include "../ion.h"
#include "../../uapi/ion_bitmain.h"

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

// heap id in ion_platform_heap is not used, id in ion_heap is assigned in ion_device_add_heap
static struct ion_of_heap bm_ion_heap_list[] = {
	PLATFORM_HEAP("bitmain,carveout_vpp", 0,
		      ION_HEAP_TYPE_CARVEOUT, "vpp"),
	PLATFORM_HEAP("bitmain,cma_vpp", 0,
		      ION_HEAP_TYPE_DMA, "vpp"),
	PLATFORM_HEAP("bitmain,carveout_npu", 0,
		      ION_HEAP_TYPE_CARVEOUT, "npu"),
	PLATFORM_HEAP("bitmain,carveout", 0,
		      ION_HEAP_TYPE_CARVEOUT, "carveout"),
	PLATFORM_HEAP("bitmain,cma", 0,
		      ION_HEAP_TYPE_DMA, "cma"),
	PLATFORM_HEAP("bitmain,sys_contig", 0,
		      ION_HEAP_TYPE_SYSTEM_CONTIG, "sys_contig"),
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
	case ION_HEAP_TYPE_DMA:
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
			continue;

		heap_pdev = of_platform_device_create(node, heaps[i].name,
						      &pdev->dev);
		if (!heap_pdev)
			continue;
		heap_pdev->dev.platform_data = &heaps[i];

		heaps[i].priv = &heap_pdev->dev;

		ret = ion_setup_heap_common(pdev, node, &heaps[i]);
		if (ret) {
			of_device_unregister(to_platform_device(heaps[i].priv));
			continue;
		}
		i++;
	}

	data->heaps = heaps;
	data->nr = num_heaps;
	return data;
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
			case ION_HEAP_TYPE_DMA:
			case ION_HEAP_TYPE_CARVEOUT:
			case ION_HEAP_TYPE_CHUNK:
			{
				info->total_size = heap->total_size;
				info->avail_size =
					heap->total_size - heap->num_of_alloc_bytes;
				break;
			}
			case ION_HEAP_TYPE_SYSTEM:
			case ION_HEAP_TYPE_SYSTEM_CONTIG:
			{
				info->total_size =
					get_num_physpages() << PAGE_SHIFT;
				info->avail_size =
					nr_free_pages() << PAGE_SHIFT;
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

u64 get_user_pa(u64 user_addr)
{
	pgd_t *pgd; //= (pgd_t*)per_cpu(current_pgd, smp_processor_id());
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	struct mm_struct *mm;
	u64 pa;

	if (!find_vma(current->mm, user_addr)) {
		pr_err("no va %llx\n", user_addr);
		goto exit;
	}

	mm = current->mm;
	if (!mm) {
		pr_err("no vm_mm\n");
		goto exit;
	}

	pgd = pgd_offset(mm, user_addr);
	pr_debug("[%08llx] *pgd=%016llx", user_addr, pgd_val(*pgd));
	if (pgd_none(*pgd) || pgd_bad(*pgd))
		goto exit;

	pud = pud_offset(pgd, user_addr);
	pr_debug(", *pud=%016llx", pud_val(*pud));
	if (pud_none(*pud) || pud_bad(*pud))
		goto exit;

	pmd = pmd_offset(pud, user_addr);
	pr_debug(", *pmd=%016llx", pmd_val(*pmd));
	if (pmd_none(*pmd) || pmd_bad(*pmd))
		goto exit;

	pte = pte_offset_map(pmd, user_addr);
	pr_debug(", *pte=%016llx", pte_val(*pte));
	if (!pte_present(*pte))
		goto exit;

	pa = ((pte_val(*pte) & PHYS_MASK) & PAGE_MASK) |
						(user_addr & ~PAGE_MASK);

	return pa;

exit:
	pr_err("failed to get pa\n");
	return 0;
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

		__flush_cache_user_range(data.start, ((u64)data.start) + data.size);
		break;
	}
	case ION_IOC_BITMAIN_INVALIDATE_RANGE:
	{
		struct bitmain_cache_range data;
		unsigned long  va, pa;

		pr_debug("ion invalidate:%p, %u\n", data.start, data.size);
		if (copy_from_user(&data, (void __user *)arg, sizeof(data)))
			return -EFAULT;

		if (IS_ERR(data.start)) {
			pr_err(" addr %p, size %u!\n", data.start, data.size);
			return -EFAULT;
		}
		pa = get_user_pa((u64)data.start);
		if (!pa) {
			pr_err("pa is 0\n");
			break;
		}
		va = (unsigned long)phys_to_virt(pa);
		pr_debug("IonInv  va:%llx, pa:%llx\n", va, pa);
		__inval_cache_range(va, va + data.size);
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

static int bitmain_ion_probe(struct platform_device *pdev)
{
	struct bm_ion_dev *ipdev;
	int i, er_count = 0;
	int ret = 0;

	ipdev = devm_kzalloc(&pdev->dev, sizeof(*ipdev), GFP_KERNEL);
	if (!ipdev) {
		ret = -ENOMEM;
		goto out;
	}
	platform_set_drvdata(pdev, ipdev);

	ipdev->plat_data = ion_parse_dt(pdev, bm_ion_heap_list);
	if (IS_ERR(ipdev->plat_data)) {
		ret = PTR_ERR(ipdev->plat_data);
		goto ipdev_free;
	}
	ipdev->heaps = devm_kzalloc(&pdev->dev,
				sizeof(struct ion_heap *) * ipdev->plat_data->nr,
				GFP_KERNEL);
	if (!ipdev->heaps) {
		ion_destroy_platform_data(ipdev->plat_data);
		ret = -ENOMEM;
		goto ipdev_free;
	}

	for (i = 0, er_count = 0; i < ipdev->plat_data->nr; i++) {
		/* Here we use local variable
		 * to shorten name of parmater
		 * pass to other functions
		 */
		struct device *dev = ipdev->plat_data->heaps[i].priv;
		struct ion_platform_heap *pl_heap = &ipdev->plat_data->heaps[i];
		u32 chunk_size;

		switch (pl_heap->type) {
		case ION_HEAP_TYPE_CARVEOUT:
			ipdev->heaps[i] = ion_carveout_heap_create(pl_heap);
			goto show_heap;
		case ION_HEAP_TYPE_DMA:
			ipdev->heaps[i] = ion_cma_heap_create(pl_heap);
			goto show_heap;
		case ION_HEAP_TYPE_CHUNK:
			ret = of_property_read_u32(dev->of_node,
						   "chunk-size", &chunk_size);
			if (ret) {
				WARN(1, "missing dts property \"chunk-size\"\n");
				ipdev->heaps[i] = ERR_PTR(-EINVAL);
			} else {
				ipdev->heaps[i] =
					ion_chunk_heap_create(pl_heap, chunk_size);
			}
			goto show_heap;
		default:
			continue;
		}
show_heap:
		if (IS_ERR(ipdev->heaps[i])) {
			er_count++;
			dev_err(dev, "ion %s create fail\n", pl_heap->name);
			continue;
		}

		ion_device_add_heap(ipdev->heaps[i]);
		if (!ipdev->idev) {
			ipdev->idev = ipdev->heaps[i]->dev;
			ipdev->idev->custom_ioctl = bitmain_ion_ioctl;
		}
		dev_info(dev, "[ion] add heap id %d, type %d, base 0x%llx, size 0x%lx\n",
			 ipdev->heaps[i]->id, ipdev->heaps[i]->type,
			 pl_heap->base,
			 pl_heap->size);
	}

	/* all heaps are created fail */
	if (ipdev->plat_data->nr == er_count) {
		ret = -ENODEV;
		goto ipdev_heaps_free;
	} else {
		/* any heap is created */
		goto out;
	}
ipdev_heaps_free:
	ion_destroy_platform_data(ipdev->plat_data);
	devm_kfree(&pdev->dev, ipdev->heaps);

ipdev_free:
	devm_kfree(&pdev->dev, ipdev);

out:
	return ret;
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

RESERVEDMEM_OF_DECLARE(vpp, "vpp-region", rmem_ion_setup);
RESERVEDMEM_OF_DECLARE(npu, "npu-region", rmem_ion_setup);
RESERVEDMEM_OF_DECLARE(ion, "ion-region", rmem_ion_setup);
#endif

