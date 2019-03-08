/*
 * R/O (V)BM_EMMC_BURN 12/16/32 filesystem implementation by Marcus Sundberg
 *
 * 2002-07-28 - rjones@nexus-tech.net - ported to ppcboot v1.1.6
 * 2003-03-10 - kharris@nexus-tech.net - ported to u-boot
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef _BM_EMMC_BURN_H_
#define _BM_EMMC_BURN_H_

#define BM_BLOCK_SECTOR_SIZE               512
#define BM_SECTION_NUMBER_MAX              16
#define BM_PACKAGE_HEAD_MAGIC              "BM_PACK"

/*fw section structure*/
#define BM_SECTION_HEAD_MAGIC              "BM_SECT"
#define BM_SECTION_FW_FIP                  "fip"

typedef struct _bm_package_head {
  char package_magic[8]; //BM_PACK 7
  char version[4];
  int  number_sections;
  uint64_t  section_offset[BM_SECTION_NUMBER_MAX];
  uint64_t  length;
  int  reserved[9];
  int  crc32;
} __attribute__((packed)) BM_PACKAGE_HEAD ;/// 4*32 bytes

typedef struct _bm_section_head {
  char  section_magic[8]; //BM_SECT 7
  char  section_name[12];
  int lba_first;
  int lba_last;
  uint64_t   section_length;
  int   reserved[1];
}__attribute__((packed)) BM_SECTION_HEAD;///4*10 bytes

#endif /* _BM_EMMC_BURN_H_ */
