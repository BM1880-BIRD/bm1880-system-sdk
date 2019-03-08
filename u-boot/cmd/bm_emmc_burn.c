#include <common.h>
#include <command.h>
#include <s_record.h>
#include <net.h>
#include <ata.h>
#include <asm/io.h>
#include <mapmem.h>
#include <part.h>
#include <fat.h>
#include <fs.h>
#include <blk.h>
#include <mmc.h>
#include <bm_emmc_burn.h>
#include <image-sparse.h>
#include <linux/compat.h>
#include <linux/math64.h>
#include <android_image.h>
#include <div64.h>

#define DEBUG
#ifdef DEBUG
#define pr_debug(fmt, ...) \
	printf(fmt, ##__VA_ARGS__)
#else
#define pr_debug(fmt, ...)
#endif

void write_sparse_image(
		struct sparse_storage *info, const char *part_name,
		void *data, unsigned sz)
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

	return;
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

static int do_bm_burn_emmc(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int i, err = -1;
	char cmd[128];
	BM_PACKAGE_HEAD* ppack_head = NULL;
	BM_SECTION_HEAD* psect_head = NULL;
	ulong start_addr = BM_UPDATE_FW_START_ADDR;
	ulong section_start, sect_ctx_start, sect_ctx_len;
	const char *image_name = BM_UPDATE_FW_NAME;
	u32 crc;
	loff_t image_size = 0;

	if (argc > 1) {
		start_addr = (ulong)simple_strtoul(argv[1], NULL, 16);
		printf("start_addr:0x%lx\n", start_addr);
	} else {
		for (i = 0; i < 6; ++i) {
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

	ppack_head = (BM_PACKAGE_HEAD*)start_addr;
	/* early check */
	if(strcmp(ppack_head->package_magic, BM_PACKAGE_HEAD_MAGIC) || (ppack_head->number_sections <= 0)
			|| (ppack_head->number_sections > BM_SECTION_NUMBER_MAX)) {
		printf("The header of package invalid!\r\n");
		return CMD_RET_FAILURE;
	}

	image_size = ppack_head->length;

	printf("Image version :       %s\n", ppack_head->version);
	printf("Image size :          %lld Byte(%lld MByte)\n", image_size, image_size / 1024 / 1024);
	printf("Image sections num :  %d\n", ppack_head->number_sections);
	printf("Image crc32 :         0x%x\n", ppack_head->crc32);

	printf("Check CRC...\n");
	crc = crc32(0, (unsigned char *)(start_addr + sizeof(BM_PACKAGE_HEAD)), image_size - sizeof(BM_PACKAGE_HEAD));
	printf("calc crc32: 0x%x\n", crc);

	if (crc != ppack_head->crc32) {
		printf("CRC check failed!\n");
		return CMD_RET_FAILURE;
	}
	printf("crc compare done!\n");

	for (i = 0; i < ppack_head->number_sections; ++i) {
		section_start = start_addr + ppack_head->section_offset[i];
		psect_head = (BM_SECTION_HEAD*)(section_start);

		sect_ctx_start = section_start + sizeof(BM_SECTION_HEAD);
		sect_ctx_len = (ulong)(psect_head->section_length) - (ulong)sizeof(BM_SECTION_HEAD);
		pr_debug("Section[%d] magic:       %s\n", i, psect_head->section_magic);
		pr_debug("Section[%d] name:        %s\n", i, psect_head->section_name);
		pr_debug("Section[%d] length:      %llu KByte\n", i, psect_head->section_length / 1024);
		pr_debug("Section[%d] lba_first:   0x%x\n", i, psect_head->lba_first);
		pr_debug("Section[%d] lba_last:    0x%x\n", i, psect_head->lba_last);

		printf("Init emmc device...\n");
		if(strcmp(psect_head->section_name, BM_SECTION_FW_FIP) == 0) {
			sprintf(cmd, "mmc dev 0 %d", 1);
		} else {
			sprintf(cmd, "mmc dev 0 %d", 0);
		}
		err = run_command(cmd, 0);
		if (err < 0) {
			printf("%s failed!\n", cmd);
			return CMD_RET_FAILURE;
		}

		printf("Start burn %s of size:%ld KByte into EMMC...\n", psect_head->section_name, sect_ctx_len / 1024);

		if (is_sparse_image((void *)sect_ctx_start)) {
			struct sparse_storage sparse;
			struct blk_desc *dev_desc;

			dev_desc = blk_get_dev("mmc", 0);
			if (!dev_desc || dev_desc->type == DEV_TYPE_UNKNOWN) {
				printf("invalid mmc device\n");
				return CMD_RET_FAILURE;
			}

			sparse.blksz = BM_BLOCK_SECTOR_SIZE;
			sparse.start = psect_head->lba_first;
			sparse.size = psect_head->lba_last;
			sparse.write = bm_mmc_sparse_write;
			sparse.reserve = bm_mmc_sparse_reserve;

			printf("Flashing sparse image at offset " LBAFU "\n",
					sparse.start);

			sparse.priv = dev_desc;
			write_sparse_image(&sparse, "rootfs", (void *)sect_ctx_start,
					sect_ctx_len);
		} else {
			sprintf(cmd, "mmc write 0x%lx %x# %lx", sect_ctx_start, psect_head->lba_first,
					sect_ctx_len / BM_BLOCK_SECTOR_SIZE + 1);
			pr_debug("%s\n", cmd);
			err = run_command(cmd, 0);
			if (err < 0) {
				printf("Burn %s into boot0 partition failed!\r\n", psect_head->section_name);
				return CMD_RET_FAILURE;
			}
		}
	}

	printf("\nUpdate done!\n");
	return 0;
}

U_BOOT_CMD(
	update,	2,	0,	do_bm_burn_emmc,
	"read update package file from SD card and burn into eMMC",
	"burn emmc"
);

