/*
 *
 * file combine ion_query/ion_alloc to kernel drivers
 *
 * Copyright (C) 2019 Bitmain, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/uaccess.h>

#include "../ion.h"
#include "bitmain_ion_alloc.h"

int bm_ion_alloc(enum ion_heap_type type, size_t len, bool mmap_cache)
{
	struct ion_heap_query query;
	int ret = 0, index;
	unsigned int heap_id;
	struct ion_heap_data *heap_data;
	struct ion_buffer *buf;
	mm_segment_t old_fs = get_fs();

	memset(&query, 0, sizeof(struct ion_heap_query));
	query.cnt = HEAP_QUERY_CNT;
	heap_data = vzalloc(sizeof(*heap_data) * HEAP_QUERY_CNT);
	query.heaps = (unsigned long int)heap_data;
	if (!query.heaps)
		return -ENOMEM;

	pr_debug("%s: len %zu looking for type %d and mmap it as %s\n",
		 __func__, len, type,
		 (mmap_cache) ? "cacheable" : "un-cacheable");

	set_fs(KERNEL_DS);
	ret = ion_query_heaps(&query);
	set_fs(old_fs);
	if (ret != 0)
		return ret;

	heap_id = HEAP_QUERY_CNT + 1;
	/* here only return the 1st match
	 * heap id that user requests for
	 */
	for (index = 0; index < query.cnt; index++) {
		if (heap_data[index].type == type) {
			heap_id = heap_data[index].heap_id;
			break;
		}
	}
	vfree(heap_data);
	return ion_alloc(len, 1 << heap_id,
			 ((mmap_cache) ? 1 : 0), &buf);
}
EXPORT_SYMBOL(bm_ion_alloc);

void bm_ion_free(int fd)
{
	ion_free(fd);
}
EXPORT_SYMBOL(bm_ion_free);
