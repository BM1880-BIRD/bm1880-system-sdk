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
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include "../ion.h"

#include "../../uapi/ion_bitmain.h"

static int bm_ion_debug_heap_show(struct seq_file *s, void *unused)
{
	struct ion_heap *heap = s->private;
	struct ion_device *dev = heap->dev;
	struct ion_buffer *pos, *n;
	size_t total_size = heap->total_size;
	size_t alloc_size = heap->num_of_alloc_bytes;
	int usage_rate = 0;

	seq_puts(s, "Summary:\n");
	if (heap->debug_show)
		heap->debug_show(heap, s, unused);

	usage_rate = alloc_size * 100 / total_size;
	if ((alloc_size * 100) % total_size)
		usage_rate += 1;

	seq_printf(s, "[%d] %s heap size:%lu bytes, used:%lu bytes",
		   heap->id, heap->name, total_size, alloc_size);

	seq_printf(s, "usage rate:%d%%, memory usage peak %lld bytes\n",
		   usage_rate, heap->alloc_bytes_wm);

	seq_printf(s, "\nDetails:\n%16s %16s %16s %16s\n",
		   "heap_id", "alloc_buf_size", "phy_addr", "kmap_cnt");
	mutex_lock(&dev->buffer_lock);
	rbtree_postorder_for_each_entry_safe(pos, n, &dev->buffers, node) {
		/* only heap id matches will show buffer info */
		if (heap->id == pos->heap->id)
			seq_printf(s, "%16d %16zu %16llx %16d\n",
				   pos->heap->id, pos->size,
				   pos->paddr, pos->kmap_cnt);
	}
	mutex_unlock(&dev->buffer_lock);
	seq_puts(s, "\n");

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

static int bm_debugfs_get(void *data, u64 *val)
{
	unsigned long *p = data;

	*val = *p;

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(bm_get_fops, bm_debugfs_get, NULL, "%llu\n");

static int bm_get_peak(void *data, u64 *val)
{
	struct ion_heap *heap = data;

	spin_lock(&heap->stat_lock);
	*val = heap->alloc_bytes_wm;
	spin_unlock(&heap->stat_lock);

	return 0;
}

static int bm_clear_peak(void *data, u64 val)
{
	struct ion_heap *heap = data;

	spin_lock(&heap->stat_lock);
	heap->alloc_bytes_wm = 0;
	spin_unlock(&heap->stat_lock);

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(heap_peak, bm_get_peak, bm_clear_peak, "%llu\n");

void bm_ion_create_debug_info(struct ion_heap *heap)
{
	struct dentry *debug_file;
	struct ion_device *dev = heap->dev;
	char debug_heap_name[64];

	snprintf(debug_heap_name, 64, "bm_%s_heap_dump", heap->name);

	heap->heap_dfs_root =
		debugfs_create_dir(debug_heap_name, dev->debug_root);
	if (!heap->heap_dfs_root) {
		pr_err("%s: failed to create debugfs root directory.\n",
		       debug_heap_name);
		return;
	}
	debug_file = debugfs_create_file(
		"summary", 0644, heap->heap_dfs_root, heap,
		&debug_heap_fops);
	if (!debug_file) {
		char buf[256], *path;

		path = dentry_path(dev->debug_root, buf, 256);
		pr_err("Failed to create heap debugfs at %s/%s\n",
		       path, debug_heap_name);
	}
	debug_file = debugfs_create_file("peak", 0755,
					 heap->heap_dfs_root, heap,
					 &heap_peak);
	if (!debug_file) {
		char buf[256], *path;

		path = dentry_path(heap->heap_dfs_root, buf, 256);
		pr_err("Failed to create clear_peak heap debugfs at %s/%s\n",
		       path, debug_heap_name);
	}

	debug_file = debugfs_create_file("alloc_mem", 0644,
					 heap->heap_dfs_root,
					 &heap->num_of_alloc_bytes,
					 &bm_get_fops);
	if (!debug_file) {
		char buf[256], *path;

		path = dentry_path(heap->heap_dfs_root, buf, 256);
		pr_err("Failed to create ava_mem heap debugfs at %s/%s\n",
		       path, debug_heap_name);
	}

	debug_file = debugfs_create_file("total_mem", 0644,
					 heap->heap_dfs_root, &heap->total_size,
					 &bm_get_fops);
	if (!debug_file) {
		char buf[256], *path;

		path = dentry_path(heap->heap_dfs_root, buf, 256);
		pr_err("Failed to create all_mem heap debugfs at %s/%s\n",
		       path, debug_heap_name);
	}

}
