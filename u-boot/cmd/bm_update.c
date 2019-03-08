#include <stdlib.h>
#include <common.h>
#include <config.h>
#include <command.h>
#include <fs.h>
#include <part.h>
#include <vsprintf.h>
#include <u-boot/md5.h>
#include <image-sparse.h>
#include <bm_emmc_burn.h>
#include <div64.h>
#include <linux/math64.h>

#define BM_PACK_HEADER_MAGIC	"bm_pack"
#define BM_PACK_MAX_ITEMS	(8)
#define BM_PACK_START_ADDR	BM_UPDATE_FW_START_ADDR
#define BM_PACK_NAME	"bm_update.img"

struct bm_pack_item {
	char name[16];
	uint64_t offset;
	uint64_t size;
	uint8_t reserved[8];
	uint8_t md5[16];
};

struct bm_pack_header {
	char magic[15];
	uint8_t item_num;
	uint32_t blksz;
	struct bm_pack_item item[BM_PACK_MAX_ITEMS];
	uint8_t reserved[36];
	uint32_t hdr_crc;
};

#define DEBUG
#ifdef DEBUG
#define pr_debug(fmt, ...) \
	printf(fmt, ##__VA_ARGS__)
#else
#define pr_debug(fmt, ...)
#endif

void bm_write_sparse_image(
		struct sparse_storage *info, const char *part_name,
		void *data, unsigned int sz)
{
	lbaint_t blk;
	lbaint_t blkcnt;
	lbaint_t blks;
	uint32_t bytes_written = 0;
	unsigned int chunk;
	unsigned int offset;
	unsigned int chunk_data_sz;
	uint32_t *fill_buf = NULL;
	uint32_t fill_val;
	sparse_header_t *sparse_header;
	chunk_header_t *chunk_header;
	uint32_t total_blocks = 0;
	int fill_buf_num_blks;
	int i;
	int j;

	fill_buf_num_blks = BM_UPDATE_FW_FILLBUF_SIZE / info->blksz;

	/* Read and skip over sparse image header */
	sparse_header = (sparse_header_t *)data;

	data += sparse_header->file_hdr_sz;
	if (sparse_header->file_hdr_sz > sizeof(sparse_header_t)) {
		/*
		 * Skip the remaining bytes in a header that is longer than
		 * we expected.
		 */
		data += (sparse_header->file_hdr_sz - sizeof(sparse_header_t));
	}

	debug("=== Sparse Image Header ===\n");
	debug("magic: 0x%x\n", sparse_header->magic);
	debug("major_version: 0x%x\n", sparse_header->major_version);
	debug("minor_version: 0x%x\n", sparse_header->minor_version);
	debug("file_hdr_sz: %d\n", sparse_header->file_hdr_sz);
	debug("chunk_hdr_sz: %d\n", sparse_header->chunk_hdr_sz);
	debug("blk_sz: %d\n", sparse_header->blk_sz);
	debug("total_blks: %d\n", sparse_header->total_blks);
	debug("total_chunks: %d\n", sparse_header->total_chunks);

	/*
	 * Verify that the sparse block size is a multiple of our
	 * storage backend block size
	 */
	div_u64_rem(sparse_header->blk_sz, info->blksz, &offset);
	if (offset) {
		printf("%s: Sparse image block size issue [%u]\n",
		       __func__, sparse_header->blk_sz);
		printf("sparse image block size issue");
		return;
	}

	puts("Flashing Sparse Image\n");

	/* Start processing chunks */
	blk = info->start;
	for (chunk = 0; chunk < sparse_header->total_chunks; chunk++) {
		/* Read and skip over chunk header */
		chunk_header = (chunk_header_t *)data;
		data += sizeof(chunk_header_t);

		if (chunk_header->chunk_type != CHUNK_TYPE_RAW) {
			debug("=== Chunk Header ===\n");
			debug("chunk_type: 0x%x\n", chunk_header->chunk_type);
			debug("chunk_data_sz: 0x%x\n", chunk_header->chunk_sz);
			debug("total_size: 0x%x\n", chunk_header->total_sz);
		}

		if (sparse_header->chunk_hdr_sz > sizeof(chunk_header_t)) {
			/*
			 * Skip the remaining bytes in a header that is longer
			 * than we expected.
			 */
			data += (sparse_header->chunk_hdr_sz -
				 sizeof(chunk_header_t));
		}

		chunk_data_sz = sparse_header->blk_sz * chunk_header->chunk_sz;
		blkcnt = chunk_data_sz / info->blksz;
		switch (chunk_header->chunk_type) {
		case CHUNK_TYPE_RAW:
			if (chunk_header->total_sz !=
			    (sparse_header->chunk_hdr_sz + chunk_data_sz)) {
				printf(
					"Bogus chunk size for chunk type Raw");
				return;
			}

			if (blk + blkcnt > info->start + info->size) {
				printf(
				    "%s: Request would exceed partition size!\n",
				    __func__);
				printf(
				    "Request would exceed partition size!");
				return;
			}

			blks = info->write(info, blk, blkcnt, data);
			/* blks might be > blkcnt (eg. NAND bad-blocks) */
			if (blks < blkcnt) {
				printf("%s: %s" LBAFU " [" LBAFU "]\n",
				       __func__, "Write failed, block #",
				       blk, blks);
				printf(
					      "flash write failure");
				return;
			}
			blk += blks;
			bytes_written += blkcnt * info->blksz;
			total_blocks += chunk_header->chunk_sz;
			data += chunk_data_sz;
			break;

		case CHUNK_TYPE_FILL:
			if (chunk_header->total_sz !=
			    (sparse_header->chunk_hdr_sz + sizeof(uint32_t))) {
				printf(
					"Bogus chunk size for chunk type FILL");
				return;
			}

			fill_buf = (uint32_t *)
				   memalign(ARCH_DMA_MINALIGN,
					    ROUNDUP(
						info->blksz * fill_buf_num_blks,
						ARCH_DMA_MINALIGN));
			if (!fill_buf) {
				printf(
					"Malloc failed for: CHUNK_TYPE_FILL");
				return;
			}

			fill_val = *(uint32_t *)data;
			data = (char *)data + sizeof(uint32_t);

			for (i = 0;
			     i < (info->blksz * fill_buf_num_blks /
				  sizeof(fill_val));
			     i++)
				fill_buf[i] = fill_val;

			if (blk + blkcnt > info->start + info->size) {
				printf(
				    "%s: Request would exceed partition size!\n",
				    __func__);
				printf(
				    "Request would exceed partition size!");
				return;
			}

			for (i = 0; i < blkcnt;) {
				j = blkcnt - i;
				if (j > fill_buf_num_blks)
					j = fill_buf_num_blks;
				blks = info->write(info, blk, j, fill_buf);
				/* blks might be > j (eg. NAND bad-blocks) */
				if (blks < j) {
					printf("%s: %s " LBAFU " [%d]\n",
					       __func__,
					       "Write failed, block #",
					       blk, j);
					printf(
						      "flash write failure");
					free(fill_buf);
					return;
				}
				blk += blks;
				i += j;
			}
			bytes_written += blkcnt * info->blksz;
			total_blocks += chunk_data_sz / sparse_header->blk_sz;
			free(fill_buf);
			break;

		case CHUNK_TYPE_DONT_CARE:
			blk += info->reserve(info, blk, blkcnt);
			total_blocks += chunk_header->chunk_sz;
			break;

		case CHUNK_TYPE_CRC32:
			if (chunk_header->total_sz !=
			    sparse_header->chunk_hdr_sz) {
				printf(
					"Bogus chunk size for chunk type Dont Care");
				return;
			}
			total_blocks += chunk_header->chunk_sz;
			data += chunk_data_sz;
			break;

		default:
			printf("%s: Unknown chunk type: %x\n", __func__,
			       chunk_header->chunk_type);
			printf("Unknown chunk type");
			return;
		}
	}

	debug("Wrote %d blocks, expected to write %d blocks\n",
	      total_blocks, sparse_header->total_blks);
	printf("........ wrote %u bytes to '%s'\n", bytes_written, part_name);

	if (total_blocks != sparse_header->total_blks)
		printf("sparse image write failure");
}

static lbaint_t bm_mmc_sparse_write(struct sparse_storage *info,
		lbaint_t blk, lbaint_t blkcnt, const void *buffer)
{
	struct blk_desc *dev_desc = info->priv;

	return blk_dwrite(dev_desc, blk, blkcnt, buffer);
}

static lbaint_t bm_mmc_sparse_reserve(struct sparse_storage *info,
		lbaint_t blk, lbaint_t blkcnt)
{
	return blkcnt;
}

static int do_bm_update(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	char cmd[128];
	int i, ret, err;
	struct bm_pack_header *hdr_ptr;
	struct bm_pack_item *item_ptr;
	struct blk_desc *desc, *boot0_desc;
	disk_partition_t info;
	const char *image_name = BM_PACK_NAME;
	ulong start_addr = BM_PACK_START_ADDR;
	uint64_t item_size = 0;
	uint64_t item_addr = 0;
	u32 crc;
	loff_t image_size = 0;

	if (argc > 1) {
		start_addr = (ulong)simple_strtoul(argv[1], NULL, 16);
		printf("start_addr:0x%lx\n", start_addr);
	} else {
		for (i = 1; i < 6; ++i) {
			sprintf(cmd, "1:%d", i);
			printf("Init mmc %s ...\n", cmd);
			if (fs_set_blk_dev("mmc", cmd, FS_TYPE_FAT)) {
				continue;
			}

			if (fs_read(image_name, start_addr, 0, 0, &image_size)) {
				continue;
			} else {
				break;
			}
		}
		if (i >= 6) {
			printf("Can't find %s in your SD card partitions\n", image_name);
			return CMD_RET_FAILURE;
		}
	}

	hdr_ptr = (struct bm_pack_header *)start_addr;
	if (strncmp(hdr_ptr->magic, BM_PACK_HEADER_MAGIC, sizeof(hdr_ptr->magic))) {
		printf("Package header MAGIC err!!!\n");
		return CMD_RET_FAILURE;
	}

	printf("\n");
	printf("header->magic:%s\n", hdr_ptr->magic);
	printf("header->item_num:0x%x\n", hdr_ptr->item_num);
	printf("header->blksz:0x%x\n", hdr_ptr->blksz);
	printf("header->hdr_crc32:0x%x\n", hdr_ptr->hdr_crc);
	printf("\n");

	printf("Check package header CRC...\n");
	crc = crc32(0, (unsigned char *)hdr_ptr, sizeof(struct bm_pack_header) - sizeof(hdr_ptr->hdr_crc));
	printf("calc crc32: 0x%x\n", crc);

	if (crc != hdr_ptr->hdr_crc) {
		printf("CRC check failed!\n");
		return CMD_RET_FAILURE;
	}
	printf("crc compare done!\n");

	if (hdr_ptr->item_num > BM_PACK_MAX_ITEMS) {
		printf("item num %d exceed the max item number %d!\n", hdr_ptr->item_num, BM_PACK_MAX_ITEMS);
		return CMD_RET_FAILURE;
	}

	ret = blk_get_device_by_str("mmc", "0", &desc);
	if (ret < 0) {
		printf("get mmc 0 device failed!\n");
		return CMD_RET_FAILURE;
	}

	for (i = 0; i < hdr_ptr->item_num; ++i) {
		uint8_t md5[16] = {0};
		item_ptr = &hdr_ptr->item[i];
		item_size = item_ptr->size;
		item_addr = start_addr + item_ptr->offset;

		printf("\titem->name:%s\n", item_ptr->name);
		printf("\titem->size:0x%llx\n", item_ptr->size);
		printf("\titem->offset:0x%llx\n", item_ptr->offset);
		printf("\titem->md5:");
		for (int j = 0; j < sizeof(item_ptr->md5); ++j) {
			printf("%x", item_ptr->md5[j]);
		}
		printf("\n");
		printf("\n");

		printf("%s start...\n", item_ptr->name);
		md5_wd((void *)item_addr, item_size, md5, CHUNKSZ_MD5);

		if (memcmp(item_ptr->md5, md5, sizeof(item_ptr->md5))) {
			printf("item %s's md5 value verified failed!!!\n", item_ptr->name);
			return CMD_RET_FAILURE;
		}

		if(!strncmp(item_ptr->name, "FIP", sizeof(item_ptr->name))) {
			ret = blk_get_device_by_str("mmc", "0.1", &boot0_desc);
			if (ret < 0) {
				printf("get mmc 0 device failed!\n");
				return CMD_RET_FAILURE;
			}

			err = blk_dwrite(boot0_desc, 0, (item_size + desc->blksz - 1) / desc->blksz, (void *)item_addr);
			if (err < 0) {
				printf("Burn %s partition failed!\r\n", item_ptr->name);
				return CMD_RET_FAILURE;
			}

			ret = blk_get_device_by_str("mmc", "0", &desc);
			if (ret < 0) {
				printf("get mmc 0 device failed!\n");
				return CMD_RET_FAILURE;
			}

			printf("done...\n");
			continue;
		}

		if(!strncmp(item_ptr->name, "GPT", sizeof(item_ptr->name))) {
			printf("GPT: blks:%llu, item_addr:0x%llx\n", (item_size + desc->blksz - 1) / desc->blksz, item_addr);
			err = blk_dwrite(desc, 0, (item_size + desc->blksz - 1) / desc->blksz, (void *)item_addr);
			if (err < 0) {
				printf("Burn %s partition failed!\r\n", item_ptr->name);
				return CMD_RET_FAILURE;
			}

			printf("done...\n");
			continue;
		}

		ret = part_get_info_by_name(desc, item_ptr->name, &info);
		if (ret < 0) {
			printf("emmc gpt do not have partition named %s!!!\n", item_ptr->name);
			return CMD_RET_FAILURE;
		}

		if (item_ptr->size > (info.size * info.blksz)) {
			printf("item %s's size above the partition size!!!\n", item_ptr->name);
			return CMD_RET_FAILURE;
		}

		if (is_sparse_image((void *)item_addr)) {
			struct sparse_storage sparse;

			sparse.blksz = 512;
			sparse.start = info.start;
			sparse.size = info.size;
			sparse.write = bm_mmc_sparse_write;
			sparse.reserve = bm_mmc_sparse_reserve;

			printf("Flashing sparse image at offset " LBAFU "\n",
					sparse.start);

			sparse.priv = desc;
			bm_write_sparse_image(&sparse, item_ptr->name, (void *)item_addr,
					      item_size);
		} else {
			err = blk_dwrite(desc, info.start, (item_size + info.blksz - 1) / info.blksz, (void *)item_addr);
			if (err < 0) {
				printf("Burn %s partition failed!\r\n", item_ptr->name);
				return CMD_RET_FAILURE;
			}
		}
		printf("done...\n");

	}

	printf("\nUpdate done!\n");

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	bm_update,	2,	0,	do_bm_update,
	"bootloader control block command",
	"bm_bcb <interface> <dev> <varname>\n"
);

