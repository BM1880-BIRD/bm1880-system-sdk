#include <common.h>
#include <command.h>
#include <asm/io.h>
#include <mapmem.h>
#include <linux/input.h>
#include <linux/mtd/mtd.h>
#include <linux/compat.h>
#include <linux/math64.h>
#include <mmc.h>
#include <fat.h>
#include <fs.h>
#include <blk.h>
#include <libfdt.h>
#include <fdt.h>
#include <fdtdec.h>
#include <linux/ctype.h>
#include <image-sparse.h>
#include <bm_cdump.h>

/* bm1682 memory dump to 2 parts:
 *	address		size
 *	0x100080000	0x1551000	code
 *	0x100000000	0xb0000000	all memory
 *
 *
 *	elf header program:
 *	header:	0x0 - 0x1000
 *	note:	0x1000 - 0x2000		// don't support yet
 *	code:	0x2000 - 0x1553000
 *	mem:	0x1553000 - end
 *
 */
static int fill_dump_mem(struct crashdump_mem *cdump_mem)
{
	int dump_mem_field_cnt;

	/* fill code mem and sys mem field */
	cdump_mem[0].paddr = CODE_PHY_ADDR;
	cdump_mem[0].size = CODE_REGION_SIZE;
	cdump_mem[0].offset = ELF_HEADER_SIZE + ELF_NOTE_SIZE;
	cdump_mem[0].type = CDUMP_CODE;
	cdump_mem[0].vaddr = CODE_VM_ADDR;

	cdump_mem[1].paddr = MEM_PHY_ADDR;
	cdump_mem[1].size = MEM_REGION_SIZE;	//      total 2.75G system memory
	cdump_mem[1].offset =
	    ELF_HEADER_SIZE + ELF_NOTE_SIZE + CODE_REGION_SIZE;
	cdump_mem[1].type = CDUMP_MEM;
	cdump_mem[1].vaddr = MEM_VM_ADDR;

	//Fix to 2 parts. TODO: if need dump more mem, can enlarge the count.
	dump_mem_field_cnt = 2;

	return dump_mem_field_cnt;
}

static void fill_core_hdr(struct pt_regs *regs,
			  struct crashdump_mem *sysmem, int mem_num,
			  char *bufp, int nphdr, int dataoff)
{
	struct elf_phdr *nhdr, *phdr;
	struct elfhdr *elf;
#if SETUP_NOTE
	struct memelfnote notes[3];
#endif
	off_t offset = 0;
	int i;

	/* setup ELF header */
	elf = (struct elfhdr *)bufp;
	bufp += sizeof(struct elfhdr);
	offset += sizeof(struct elfhdr);
	memcpy(elf->e_ident, ELFMAG, SELFMAG);
	elf->e_ident[EI_CLASS] = ELFCLASS64;
	elf->e_ident[EI_DATA] = ELF_DATA;
	elf->e_ident[EI_VERSION] = EV_CURRENT;
	elf->e_ident[EI_OSABI] = ELFOSABI_LINUX;
	memset(elf->e_ident + EI_PAD, 0, EI_NIDENT - EI_PAD);
	elf->e_type = ET_CORE;
	elf->e_machine = EM_ARM;
	elf->e_version = EV_CURRENT;
	elf->e_entry = 0;
	elf->e_phoff = sizeof(struct elfhdr);
	elf->e_shoff = 0;
	elf->e_flags = 0;
	elf->e_ehsize = sizeof(struct elfhdr);
	elf->e_phentsize = sizeof(struct elf_phdr);
	elf->e_phnum = nphdr;
	elf->e_shentsize = 0;
	elf->e_shnum = 0;
	elf->e_shstrndx = 0;

	/* setup ELF PT_NOTE program header */
	nhdr = (struct elf_phdr *)bufp;
	bufp += sizeof(struct elf_phdr);
	offset += sizeof(struct elf_phdr);
	nhdr->p_type = PT_NOTE;
	nhdr->p_offset = 0;
	nhdr->p_vaddr = 0;
	nhdr->p_paddr = 0;
	nhdr->p_filesz = 0;
	nhdr->p_memsz = 0;
	nhdr->p_flags = 0;
	nhdr->p_align = 0;

	/* setup ELF PT_LOAD program header for every area */
	for (i = 0; i < mem_num; i++) {
		phdr = (struct elf_phdr *)bufp;
		bufp += sizeof(struct elf_phdr);
		offset += sizeof(struct elf_phdr);

		phdr->p_type = PT_LOAD;
		phdr->p_flags = PF_R | PF_W | PF_X;
		phdr->p_offset = sysmem[i].offset;
		phdr->p_vaddr = sysmem[i].vaddr;
		phdr->p_paddr = sysmem[i].paddr;
		phdr->p_memsz = sysmem[i].size;
		phdr->p_filesz = sysmem[i].size;
		phdr->p_align = 0;
		dataoff += sysmem[i].size;
	}
#if SETUP_NOTE
	/*
	 * Set up the notes in similar form to SVR4 core dumps made
	 * with info from their /proc.
	 */
	nhdr->p_offset = offset;

	/* set up the process status */
	notes[0].name = CORE_STR;
	notes[0].type = NT_PRSTATUS;
	notes[0].datasz = sizeof(struct elf_prstatus);
	notes[0].data = &prstatus;

	memset(&prstatus, 0, sizeof(struct elf_prstatus));
	//fill_prstatus(&prstatus, current, 0);
	//if (regs)
	//      memcpy(&prstatus.pr_reg, regs, sizeof(*regs));
	//else
	//      crash_setup_regs((struct pt_regs *)&prstatus.pr_reg, NULL);

	nhdr->p_filesz = notesize(&notes[0]);
	bufp = storenote(&notes[0], bufp);

	/* set up the process info */
	notes[1].name = CORE_STR;
	notes[1].type = NT_PRPSINFO;
	notes[1].datasz = sizeof(struct elf_prpsinfo);
	notes[1].data = &prpsinfo;

	memset(&prpsinfo, 0, sizeof(struct elf_prpsinfo));
	//fill_psinfo(&prpsinfo, current, current->mm);

	strcpy(prpsinfo.pr_fname, "vmlinux");
	//strncpy(prpsinfo.pr_psargs, saved_command_line, ELF_PRARGSZ);

	nhdr->p_filesz += notesize(&notes[1]);
	bufp = storenote(&notes[1], bufp);

	/* set up the task structure */
	notes[2].name = CORE_STR;
	notes[2].type = NT_TASKSTRUCT;
	notes[2].datasz = sizeof(struct task_struct);
	notes[2].data = current;

	printf("%s: data size is %d, data addr is %p", __func__, notes[2].datasz, notes[2].data);

	nhdr->p_filesz += notesize(&notes[2]);
	bufp = storenote(&notes[2], bufp);
#endif
}

static size_t get_elfhdr_size(int nphdr)
{
	size_t elfhdr_len;

	elfhdr_len = sizeof(struct elfhdr) + (nphdr + 1) * sizeof(struct elf_phdr);
#if SETUP_NOTE
	elfhdr_len += ((sizeof(struct elf_note)) +
		roundup(sizeof(CORE_STR), 4)) * 3 +
		roundup(sizeof(struct elf_prstatus), 4) +
		roundup(sizeof(struct elf_prpsinfo), 4) +
		roundup(sizeof(struct task_struct), 4);
#endif
	elfhdr_len = PAGE_ALIGN(elfhdr_len);	//why?

	return elfhdr_len;
}

static int build_elf_header(struct crashdump_mem *cdump_mem)
{
	loff_t write_cnt;
	int cdump_mem_cnt;
	int core_hdr_size;
	const char *image_name = BM_CDUMP_HEADER_NAME;
	char *hdr_buf;

	cdump_mem_cnt = fill_dump_mem(cdump_mem);
	core_hdr_size = get_elfhdr_size(cdump_mem_cnt);

	/* Note field is not support, but we need alloc the memory in file */
	hdr_buf = malloc(ELF_HEADER_SIZE + ELF_NOTE_SIZE);
	if (!hdr_buf) {
		printf("malloc mem failed!\n");
		return -1;
	}
	memset(hdr_buf, 0, ELF_HEADER_SIZE + ELF_NOTE_SIZE);

	fill_core_hdr(NULL, cdump_mem, cdump_mem_cnt, hdr_buf, cdump_mem_cnt + 1, core_hdr_size);

	/* write core hdr to image file */
	if (file_fat_write
			(image_name, (void *)hdr_buf, 0,
			ELF_HEADER_SIZE + ELF_NOTE_SIZE, &write_cnt)) {
		printf("write file failed!\n");
		return -1;
	}

	printf("write data:%llx to file %s.\n", write_cnt, image_name);

	return 0;
}

int do_cdump(void)
{
	char cmd[128];
	char image_name[15];
	loff_t memory_size = 0;
	u32 i, j, dev, part;
	u32 max_bank = 0;
	struct blk_desc *dev_desc;
	struct crashdump_mem cdump_mem[MAX_NUM_DUMP_MEM_FIELD];
	disk_partition_t info;

	for (i = 0; i < 8; i++) {
		sprintf(cmd, "1:%d", i);
		printf("\nInit mmc %s ...\n", cmd);
		part = blk_get_device_part_str("mmc", cmd, &dev_desc, &info, 1);
		if (part < 0)
			continue;

		printf("find partition size:%lx, partition type:%d\n", info.size * info.blksz, dev_desc->part_type);

		dev = dev_desc->devnum;
		if (fat_set_blk_dev(dev_desc, &info) != 0) {
			printf("Unable to use %d:%d for fatinfo.\n", dev, part);
			continue;
		}

		if (info.size * info.blksz < MEM_REGION_SIZE) {
			printf("partition type or size invalid!\n");
			continue;
		} else {
			break;
		}
	}

	/* write image header */
	build_elf_header(cdump_mem);

	/* write core data to file */
	for (i = 0; i < 1; i++) {
		sprintf(image_name, "%s-%d", BM_CDUMP_FILE_NAME, i);
		if (file_fat_write
			(image_name, (void *)cdump_mem[i].paddr, 0,
			cdump_mem[i].size, &memory_size)) {
			printf("write file failed!\n");
			return -1;
		}

		printf("write data:%llx to file %s.\n", memory_size, image_name);
	}

	/* write all ddr mem to file, split to 256M per bank */
	max_bank = MEM_REGION_SIZE / BANK_SIZE;
	if (MEM_REGION_SIZE % BANK_SIZE)
		max_bank++;

	printf("size:%x spilt to %d banks (size: %x)\n", MEM_REGION_SIZE, max_bank, BANK_SIZE);

	for (j = 0; j < max_bank; j++) {
		sprintf(image_name, "%s-%d", BM_CDUMP_FILE_NAME, j + i);
		if (file_fat_write
			(image_name, (void *)cdump_mem[i].paddr + BANK_SIZE * j, 0,
			BANK_SIZE, &memory_size)) {
			printf("write file failed!\n");
			return -1;
		}
		printf("write data:%llx to file %s done.\n", memory_size, image_name);
	}

	printf("crash memory dump finished!!\n");

	return 0;
}

static int do_bm_crash_dump(cmd_tbl_t *cmdtp, int flag, int argc,
			    char *const argv[])
{
	if (argc > 2) {
		printf("usage: cdump\n");
		return -1;
	}
	return do_cdump();
}

U_BOOT_CMD(cdump, 2, 0, do_bm_crash_dump,
	   "dump memories after system crash and reboot", "crash dump");
