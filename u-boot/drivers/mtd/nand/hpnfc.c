/*
 * NAND Flash Controller Device Driver
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <common.h>
#include <malloc.h>
#include <asm/io.h>
#include <linux/errno.h>
#include <nand.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/nand_ecc.h>
#include <asm/arch/hardware.h>
#include <inttypes.h>
#include <asm-generic/global_data.h>

#include "hpnfc.h"


MODULE_LICENSE("GPL");

//#define DBG_DUMP_HPNFC_REG

#define PART_1_REG_START	(0x0)
#define PART_1_REG_END		(0x154)

#define PART_2_REG_START	(0x400)
#define PART_2_REG_END		(0x44C)

#define PART_3_REG_START	(0x800)
#define PART_3_REG_END		(0x844)

#define PART_4_REG_START	(0x1000)
#define PART_4_REG_END		(0x102C)

#define HPNFC_MINIMUM_SPARE_SIZE            4
#define HPNFC_MAX_SPARE_SIZE_PER_SECTOR     32
#define HPNFC_BCH_MAX_NUM_CORR_CAPS         8
#define HPNFC_BCH_MAX_NUM_SECTOR_SIZES      2
/** this definition is used when no devices are discovered */
#define HPNFC_DEV_NUM                       1

static int reserved_blocks = 0;
static int limited_blocks = 0;
//static int init_erase = 0;
//static int maxchips = 0;
//static int disable_ddr = 0;

module_param(limited_blocks, int, S_IRUGO);
module_param(init_erase, int, S_IRUGO);
module_param(maxchips, int, S_IRUGO);
module_param(disable_ddr, int, S_IRUGO);

struct hpnfc_state_t g_hpnfc;
hpnfc_cdma_desc_t g_cdma_desc;

#define NUM_PARTITIONS (ARRAY_SIZE(mtd_partition_info))

static uint32_t sector_size = 512;
static uint32_t ecc_corr_cap = 8;

/* PHY configurations for nf_clk = 100MHz
 * phy ctrl, phy tsel, DQ timing, DQS timing, LPBK, dll master, dll slave*/
static uint32_t phy_timings_ddr[] = {
	0x0000,  0x00, 0x02, 0x00000004, 0x00200002, 0x01140004, 0x1f1f};
static uint32_t phy_timings_ddr2[] = {
	0x4000,  0x00, 0x02, 0x00000005, 0x00380000, 0x01140004, 0x1f1f};
static uint32_t phy_timings_toggle[] = {
	0x4000,  0x00, 0x02, 0x00000004, 0x00280001, 0x01140004, 0x1f1f};
static uint32_t phy_timings_async[] = {
	0x4040, 0x00, 0x02, 0x00100004, 0x1b << 19, 0x00800000, 0x0000};

extern void bbt_dump_buf(char *s, void *buf, int len);

static int wait_for_thread(struct hpnfc_state_t *hpnfc, int8_t thread);
static int hpnfc_wait_for_idle(struct hpnfc_state_t *hpnfc);
static int dma_read_data(struct hpnfc_state_t *hpnfc, void *buf, uint32_t size);

static struct hpnfc_state_t *mtd_to_hpnfc(struct mtd_info *mtd)
{
	struct hpnfc_state_t *hpnfc;
	struct nand_chip *nand = mtd_to_nand(mtd);
	hpnfc = container_of(nand, struct hpnfc_state_t, nand);

	return hpnfc;
}

#ifdef DBG_DUMP_HPNFC_REG
static void dump_nfc_reg(struct hpnfc_state_t *hpnfc, uint32_t start, uint32_t end)
{
	uint32_t reg;
	uint32_t i;

	for (i = start; i <= end; i += 4) {
		reg = readl(hpnfc->reg + i);
		printf("%s, reg: 0x%x = 0x%x\n", __func__, (uint32_t)(hpnfc->reg + i), reg);
	}
}
#endif
/* send set features command to nand flash memory device */
static int nf_mem_set_features(struct hpnfc_state_t *hpnfc, uint8_t feat_addr,
			       uint8_t feat_val, uint8_t mem, uint8_t thread,
			       uint8_t vol_id)
{
	uint32_t reg;
	int status;

	/* wait for thread ready */
	status = wait_for_thread(hpnfc, thread);
	if (status)
		return status;

	reg = 0;
	WRITE_FIELD(reg, HPNFC_CMD_REG1_FADDR, feat_addr);
	WRITE_FIELD(reg, HPNFC_CMD_REG1_BANK, mem);
	/* set feature address and bank number */
	writel(reg, hpnfc->reg + HPNFC_CMD_REG1);
	/* set feature - value*/
	writel(feat_val, hpnfc->reg + HPNFC_CMD_REG2);

	reg = 0;
	/* select PIO mode */
	WRITE_FIELD(reg, HPNFC_CMD_REG0_CT, HPNFC_CMD_REG0_CT_PIO);
	/* thread number */
	WRITE_FIELD(reg, HPNFC_CMD_REG0_TN, thread);
	/* volume ID */
	WRITE_FIELD(reg, HPNFC_CMD_REG0_VOL_ID, vol_id);
	/* disabled interrupt */
	WRITE_FIELD(reg, HPNFC_CMD_REG0_INT, 0);
	/* select set feature command */
	WRITE_FIELD(reg, HPNFC_CMD_REG0_PIO_CC, HPNFC_CMD_REG0_PIO_CC_SF);
	/* execute command */
	writel(reg, hpnfc->reg + HPNFC_CMD_REG0);

	return 0;
}


/* send reset command to nand flash memory device */
static int nf_mem_reset(struct hpnfc_state_t *hpnfc, uint8_t mem, uint8_t thread,
			uint8_t vol_id)
{
	uint32_t reg = 0;
	int status;

	/* wait for thread ready */
	status = wait_for_thread(hpnfc, thread);
	if (status)
		return status;

	WRITE_FIELD(reg, HPNFC_CMD_REG1_BANK, mem);
	writel(reg, hpnfc->reg + HPNFC_CMD_REG1);

	reg = 0;

	/* select PIO mode */
	WRITE_FIELD(reg, HPNFC_CMD_REG0_CT, HPNFC_CMD_REG0_CT_PIO);
	/* thread number */
	WRITE_FIELD(reg, HPNFC_CMD_REG0_TN, thread);
	/* volume ID */
	WRITE_FIELD(reg, HPNFC_CMD_REG0_VOL_ID, vol_id);
	/* disabled interrupt */
	WRITE_FIELD(reg, HPNFC_CMD_REG0_INT, 0);
	/* select reset command */
	WRITE_FIELD(reg, HPNFC_CMD_REG0_PIO_CC, HPNFC_CMD_REG0_PIO_CC_RST);
	/* execute command */
	writel(reg, hpnfc->reg + HPNFC_CMD_REG0);

	return 0;
}

/* function returns thread status */
static uint32_t hpnfc_get_thrd_status(struct hpnfc_state_t *hpnfc, uint8_t thread)
{
	uint32_t  reg;

	pr_debug("%s thread %d\n", __func__, thread);

	writel(thread, hpnfc->reg + HPNFC_CMD_STATUS_PTR);
	reg = readl(hpnfc->reg + HPNFC_CMD_STATUS);

	pr_debug("%s reg 0x%x\n", __func__, reg);

	return reg;
}


/* Wait until operation is finished  */
static int hpnfc_pio_check_finished(struct hpnfc_state_t *hpnfc, uint8_t thread)
{
	uint32_t  thrd_status;

//	uint32_t reg;
	uint timeout = 100;
	uint start;

	start = get_timer(0);
	/* wait for fail or complete status */
	do {
		thrd_status = hpnfc_get_thrd_status(hpnfc, thread);
		thrd_status &= (HPNFC_CDMA_CS_COMP_MASK | HPNFC_CDMA_CS_FAIL_MASK);

		if (get_timer(start) > timeout) {
			error("Timeout while waiting for PIO command finished\n");
			return -1;
		}

	} while ((thrd_status == 0));

	if (thrd_status & HPNFC_CDMA_CS_FAIL_MASK)
		return -2;
	if (thrd_status & HPNFC_CDMA_CS_COMP_MASK)
		return 0;

	return -3;
}


/* checks what is the best work mode */
#if 0
static void hpnfc_check_the_best_mode(hpnfc_state_t *hpnfc,
									  uint8_t *work_mode,
									  uint8_t *nf_timing_mode)
{
	uint32_t reg;
	*work_mode = HPNFC_WORK_MODE_ASYNC;
	*nf_timing_mode = 0;

	if (hpnfc->dev_type != HPNFC_DEV_PARAMS_0_DEV_TYPE_ONFI){
		return;
	}

	/* check if DDR is supported */
	reg = readl(hpnfc->reg + HPNFC_ONFI_TIME_MOD_0);
	reg = READ_FIELD(reg, HPNFC_ONFI_TIME_MOD_0_DDR);
	if (reg)
		*work_mode = HPNFC_WORK_MODE_NV_DDR;

	/* check if DDR2 is supported */
	reg = readl(hpnfc->reg + HPNFC_ONFI_TIME_MOD_1);
	reg = READ_FIELD(reg, HPNFC_ONFI_TIME_MOD_1_DDR2);
	if (reg)
		*work_mode = HPNFC_WORK_MODE_NV_DDR2;

	/* check if DDR is supported */
	reg = READ_FIELD(reg, HPNFC_ONFI_TIME_MOD_1_DDR3);
	if (reg)
		*work_mode = HPNFC_WORK_MODE_NV_DDR3;


	switch (*work_mode) {
	case HPNFC_WORK_MODE_NV_DDR:
		reg = readl(hpnfc->reg + HPNFC_ONFI_TIME_MOD_0);
		reg = READ_FIELD(reg, HPNFC_ONFI_TIME_MOD_0_DDR);
		break;
	case HPNFC_WORK_MODE_NV_DDR2:
	case HPNFC_WORK_MODE_TOGG:
		reg = readl(hpnfc->reg + HPNFC_ONFI_TIME_MOD_1);
		reg = READ_FIELD(reg, HPNFC_ONFI_TIME_MOD_1_DDR2);
		break;
	case HPNFC_WORK_MODE_NV_DDR3:
		reg = readl(hpnfc->reg + HPNFC_ONFI_TIME_MOD_1);
		reg = READ_FIELD(reg, HPNFC_ONFI_TIME_MOD_1_DDR3);
		break;
	case HPNFC_WORK_MODE_ASYNC:
	default:
		reg = readl(hpnfc->reg + HPNFC_ONFI_TIME_MOD_0);
		reg = READ_FIELD(reg, HPNFC_ONFI_TIME_MOD_0_SDR);
	}

	/* calculate from timing mode 1 */
	reg >>= 1;
	while (reg != 0) {
		reg >>= 1;
		*nf_timing_mode  += 1;
	}
}
#endif

/* set NAND flash memory device work mode */
static int nf_mem_set_work_mode(struct hpnfc_state_t *hpnfc, uint8_t work_mode,
				uint8_t timing_mode)
{
	uint8_t flash_work_mode = timing_mode;
	int i, status;

	switch (work_mode) {
	case HPNFC_WORK_MODE_NV_DDR:
		flash_work_mode |= (1 << 4);
		break;
	case HPNFC_WORK_MODE_NV_DDR2:
	case HPNFC_WORK_MODE_TOGG:
		flash_work_mode |= (2 << 4);
		break;
	case HPNFC_WORK_MODE_NV_DDR3:
		flash_work_mode |= (3 << 4);
		break;
	case HPNFC_WORK_MODE_ASYNC:
	default:
		break;
	}

	/* send SET FEATURES command */
	for (i=0; i < hpnfc->devnum; i++)
		nf_mem_set_features(hpnfc, 0x01, flash_work_mode | 5, i, i, 0);
	for (i=0; i < hpnfc->devnum; i++) {
		status = hpnfc_pio_check_finished(hpnfc, i);
		if (status)
			return status;
	}

	/* wait for controller IDLE */
	status = hpnfc_wait_for_idle(hpnfc);
	if (status)
		return status;

	return 0;
}

static void hpnfc_apply_phy_settings(hpnfc_state_t *hpnfc, uint32_t settings[])
{
	pr_debug("%s\n", __func__);

	writel(settings[0],hpnfc->reg + HPNFC_PHY_CTRL_REG);
	writel(settings[1],hpnfc->reg + HPNFC_PHY_TSEL_REG);
	writel(settings[2],hpnfc->reg + HPNFC_PHY_DQ_TIMING_REG);
	writel(settings[3],hpnfc->reg + HPNFC_PHY_DQS_TIMING_REG);
	writel(settings[4],hpnfc->reg + HPNFC_PHY_GATE_LPBK_CTRL_REG);
	writel(settings[5],hpnfc->reg + HPNFC_PHY_DLL_MASTER_CTRL_REG);
	writel(settings[6],hpnfc->reg + HPNFC_PHY_DLL_SLAVE_CTRL_REG);
}

/* Sets desired work mode to HPNFC controller and all NAND flash devices
 *  Each memory is processed in separate thread, starting from thread 0 */
static int hpnfc_set_work_mode(struct hpnfc_state_t *hpnfc, uint8_t work_mode,
			       uint8_t timing_mode)
{
	uint32_t reg;
	uint32_t dll_phy_ctrl;
	int i, status;

	pr_debug("%s\n", __func__);

	reg = (readl(hpnfc->reg + HPNFC_DEV_PARAMS_1)) & 0xFF;
	reg = READ_FIELD(reg, HPNFC_DEV_PARAMS_1_READID_5);
	if (reg == 0x01)
		/* exit if memory works in NV-DDR3 mode */
		return -EINVAL;

	/* set NF memory in known work mode by sending the reset command */
	reg = 0;
	/* select SDR mode in the controller */
	WRITE_FIELD(reg, HPNFC_COMMON_SETT_OPR_MODE,
				HPNFC_COMMON_SETT_OPR_MODE_SDR);
	writel(reg, hpnfc->reg + HPNFC_COMMON_SETT);

	/* select default timings */
	hpnfc_apply_phy_settings(hpnfc, phy_timings_async);

	/* send reset command to all nand flash memory devices*/
	for (i = 0; i < hpnfc->devnum; i++){
		status = nf_mem_reset(hpnfc, i, i, 0);
		if (status)
			return status;
	}

	/* wait until reset commands is finished*/
	for (i = 0; i < hpnfc->devnum; i++) {
		status = hpnfc_pio_check_finished(hpnfc, i);
		if (status)
			return status;
	}

	/* set NAND flash memory work mode */
	status = nf_mem_set_work_mode(hpnfc, work_mode, timing_mode);
	if (status)
		return status;

	/* set dll_rst_n in dll_phy_ctrl to 0 */
	dll_phy_ctrl = readl(hpnfc->reg + HPNFC_DLL_PHY_CTRL);
	dll_phy_ctrl &= ~HPNFC_DLL_PHY_CTRL_DLL_RST_N_MASK;
	writel(dll_phy_ctrl ,hpnfc->reg + HPNFC_DLL_PHY_CTRL );

	/* set value of other PHY registers according to PHY user guide
	 * currently all values for nf_clk = 100 MHz
	 * */
	switch (work_mode) {
	case HPNFC_WORK_MODE_NV_DDR:
		pr_debug("Switch to NV_DDR mode %d\n", timing_mode);
		hpnfc_apply_phy_settings(hpnfc, phy_timings_ddr);
		break;

	case HPNFC_WORK_MODE_NV_DDR2:
		pr_debug("Switch to NV_DDR2 mode %d\n", timing_mode);
		hpnfc_apply_phy_settings(hpnfc, phy_timings_ddr2);
		dll_phy_ctrl &= ~HPNFC_DLL_PHY_CTRL_EXTENDED_RD_MODE_MASK;
		break;

	case HPNFC_WORK_MODE_TOGG:
		pr_debug("Switch to toggle DDR mode\n");
		hpnfc_apply_phy_settings(hpnfc, phy_timings_toggle);
		break;

	case HPNFC_WORK_MODE_ASYNC:
	default:
		pr_debug("Switch to SDR mode %d\n", timing_mode);
		hpnfc_apply_phy_settings(hpnfc, phy_timings_async);

		reg = 0;

#if 0// faster timing setting
		WRITE_FIELD(reg, HPNFC_ASYNC_TOGGLE_TIMINGS_TRH, 1);
		WRITE_FIELD(reg, HPNFC_ASYNC_TOGGLE_TIMINGS_TRP, 2);
		WRITE_FIELD(reg, HPNFC_ASYNC_TOGGLE_TIMINGS_TWH, 1);
		WRITE_FIELD(reg, HPNFC_ASYNC_TOGGLE_TIMINGS_TWP, 2);
#else
		WRITE_FIELD(reg, HPNFC_ASYNC_TOGGLE_TIMINGS_TRH, 3);
		WRITE_FIELD(reg, HPNFC_ASYNC_TOGGLE_TIMINGS_TRP, 4);
		WRITE_FIELD(reg, HPNFC_ASYNC_TOGGLE_TIMINGS_TWH, 3);
		WRITE_FIELD(reg, HPNFC_ASYNC_TOGGLE_TIMINGS_TWP, 4);
#endif
		writel(reg, hpnfc->reg + HPNFC_ASYNC_TOGGLE_TIMINGS);

		dll_phy_ctrl |= HPNFC_DLL_PHY_CTRL_EXTENDED_RD_MODE_MASK;
		dll_phy_ctrl |= HPNFC_DLL_PHY_CTRL_EXTENDED_WR_MODE_MASK;
	}

	/* set HPNFC controller work mode */
	reg = readl(hpnfc->reg + HPNFC_COMMON_SETT);
	switch (work_mode) {
	case HPNFC_WORK_MODE_NV_DDR:
		WRITE_FIELD(reg, HPNFC_COMMON_SETT_OPR_MODE,
					HPNFC_COMMON_SETT_OPR_MODE_NV_DDR);
		break;
	case HPNFC_WORK_MODE_TOGG:
	case HPNFC_WORK_MODE_NV_DDR2:
	case HPNFC_WORK_MODE_NV_DDR3:
		WRITE_FIELD(reg, HPNFC_COMMON_SETT_OPR_MODE,
					HPNFC_COMMON_SETT_OPR_MODE_TOGGLE);
		break;

	case HPNFC_WORK_MODE_ASYNC:
	default:
		WRITE_FIELD(reg, HPNFC_COMMON_SETT_OPR_MODE,
					HPNFC_COMMON_SETT_OPR_MODE_SDR);
	}
	writel(reg, hpnfc->reg + HPNFC_COMMON_SETT);

	/* set dll_rst_n in dll_phy_ctrl to 1 */
	dll_phy_ctrl |= HPNFC_DLL_PHY_CTRL_DLL_RST_N_MASK;
	writel(dll_phy_ctrl, hpnfc->reg + HPNFC_DLL_PHY_CTRL);

	/* wait for controller IDLE */
	return hpnfc_wait_for_idle(hpnfc);
}

static int hpnfc_ecc_enable(struct hpnfc_state_t *hpnfc, bool enable)
{
	uint32_t reg;

	reg = readl(hpnfc->reg + HPNFC_ECC_CONFIG_0);

	if (enable)
		WRITE_FIELD(reg, HPNFC_ECC_CONFIG_0_CORR_STR, hpnfc->corr_cap);

	if (enable)
		reg |= HPNFC_ECC_CONFIG_0_ECC_EN_MASK | HPNFC_ECC_CONFIG_0_ERASE_DET_EN_MASK;
	else {
		reg &= ~HPNFC_ECC_CONFIG_0_ECC_EN_MASK;
		reg &= ~HPNFC_ECC_CONFIG_0_ERASE_DET_EN_MASK;
	}

	writel(reg, hpnfc->reg + HPNFC_ECC_CONFIG_0);
	writel(0, hpnfc->reg + HPNFC_ECC_CONFIG_1);

	return 0;
}

static void hpnfc_clear_interrupt(struct hpnfc_state_t *hpnfc,
				  hpnfc_irq_status_t *irq_status)
{
	writel(irq_status->status, hpnfc->reg + HPNFC_INTR_STATUS);
	writel(irq_status->trd_status, hpnfc->reg + HPNFC_TRD_COMP_INT_STATUS);
	writel(irq_status->trd_error, hpnfc->reg + HPNFC_TRD_ERR_INT_STATUS);
}

static void hpnfc_read_int_status(struct hpnfc_state_t *hpnfc,
				  hpnfc_irq_status_t *irq_status)
{
	irq_status->status = readl(hpnfc->reg + HPNFC_INTR_STATUS);
	irq_status->trd_status = readl(hpnfc->reg + HPNFC_TRD_COMP_INT_STATUS);
	irq_status->trd_error = readl(hpnfc->reg + HPNFC_TRD_ERR_INT_STATUS);
}

static inline uint32_t irq_detected(struct hpnfc_state_t *hpnfc,
				    hpnfc_irq_status_t *irq_status)
{
	hpnfc_read_int_status(hpnfc, irq_status);

	return irq_status->status || irq_status->trd_status
		|| irq_status->trd_error;
}

#define RF_CONTROLLER_REGS__RT_INTR_STATUS__SDMA_TRIGG__MASK        0x00200000U
#define RF_CONTROLLER_REGS__RT_INTR_EN__SDMA_ERR_EN__MASK           0x00400000U

/*
 * This is the interrupt service routine. It handles all interrupts
 * sent to this device.
 */
static irqreturn_t hpnfc_isr(int irq, void *dev_id)
{
	struct hpnfc_state_t *hpnfc = dev_id;
	hpnfc_irq_status_t irq_status;
	irqreturn_t result = IRQ_NONE;

//    uint32_t trdErr = readl(hpnfc->reg + HPNFC_TRD_ERR_INT_STATUS);
//    uint32_t trdTimeoutErr = readl(hpnfc->reg + HPNFC_TRD_TIMEOUT_INT_STATUS);
//    uint32_t trdCmpl = readl(hpnfc->reg + HPNFC_TRD_COMP_INT_STATUS);
    uint32_t intStat = readl(hpnfc->reg + HPNFC_INTR_STATUS);

//    printf("Interrupt status: thread complete status 0x%"PRIX32","
//            " thread error status %"PRIX32", thread error timeout status %"PRIX32
//            " status %"PRIX32"\n", trdCmpl, trdErr, trdTimeoutErr, intStat);

	if (irq_detected(hpnfc, &irq_status)) {
		/* handle interrupt */
		/* first acknowledge it */
		hpnfc_clear_interrupt(hpnfc, &irq_status);
		/* store the status in the device context for someone to read */
		hpnfc->irq_status.status |= irq_status.status;
		hpnfc->irq_status.trd_status |= irq_status.trd_status;
		hpnfc->irq_status.trd_error |= irq_status.trd_error;
		/* notify anyone who cares that it happened */
		//complete(&hpnfc->complete);
		/* tell the OS that we've handled this */
		result = IRQ_HANDLED;
	}

	if (intStat & RF_CONTROLLER_REGS__RT_INTR_STATUS__SDMA_TRIGG__MASK){
		/* clear slave DMA interrupt */
		writel(RF_CONTROLLER_REGS__RT_INTR_STATUS__SDMA_TRIGG__MASK, hpnfc->reg + HPNFC_INTR_STATUS);
		//HPNFC_Sdma_Isr(hpnfc);
	}
	return result;
}

static int wait_for_irq(struct hpnfc_state_t *hpnfc,
						 hpnfc_irq_status_t *irq_mask,
						 hpnfc_irq_status_t *irq_status)
{
	uint timeout = 100000;
	uint start;
	start = get_timer(0);

	do {
		hpnfc_isr(0, hpnfc);
		udelay(10);

		*irq_status = hpnfc->irq_status;

		if ((irq_status->status & irq_mask->status)
			|| (irq_status->trd_status & irq_mask->trd_status)
			|| (irq_status->trd_error & irq_mask->trd_error)
		   ) {
			hpnfc->irq_status.status &= ~irq_mask->status;
			hpnfc->irq_status.trd_status &= ~irq_mask->trd_status;
			hpnfc->irq_status.trd_error &= ~irq_mask->trd_error;

			/* our interrupt was detected */
			break;
		}

		/*
		 * these are not the interrupts you are looking for; need to wait again
		 */

		if (get_timer(start) > timeout) {
			error( "Timeout while waiting for wait_for_irq %d\n",
					hpnfc->chip_nr);
			return -ETIMEDOUT;
		}

	} while (1);

	return 0;
}

/*
 * We need to buffer some data for some of the NAND core routines.
 * The operations manage buffering that data.
 */
static void reset_buf(struct hpnfc_state_t *hpnfc)
{
	hpnfc->buf.head = hpnfc->buf.tail = 0;
	memset(&hpnfc->buf.buf[0], 0, 20);
}

static void write_byte_to_buf(struct hpnfc_state_t *hpnfc, uint8_t byte)
{
	hpnfc->buf.buf[hpnfc->buf.tail++] = byte;
}

static void write_dword_to_buf(struct hpnfc_state_t *hpnfc, uint32_t dword)
{
	memcpy(&hpnfc->buf.buf[hpnfc->buf.tail], &dword, 4);
	hpnfc->buf.tail += 4;
}

static uint8_t* get_buf_ptr(struct hpnfc_state_t *hpnfc)
{
	return &hpnfc->buf.buf[hpnfc->buf.tail];
}

static void increase_buff_ptr(struct hpnfc_state_t *hpnfc, uint32_t size)
{
	hpnfc->buf.tail += size;
}

/* wait until NAND flash device is ready */
int wait_for_rb_ready(struct hpnfc_state_t *hpnfc)
{
	uint32_t reg;
	uint timeout = 1000000;
	uint start;

	start = get_timer(0);

	do  {
		reg = readl(hpnfc->reg + HPNFC_RBN_SETTINGS);
		reg = (reg >> hpnfc->chip_nr) & 0x01;

		if (get_timer(start) > timeout) {
			error( "Timeout while waiting for flash device %d ready\n",
					hpnfc->chip_nr);
			return -ETIMEDOUT;
		}

	} while ((reg == 0));


	return 0;
}

static int wait_for_thread(struct hpnfc_state_t *hpnfc, int8_t thread)
{
	uint32_t reg;
	uint timeout = 10000;
	uint start;

	start = get_timer(0);

	do  {
		/* get busy status of all threads */
		reg = readl(hpnfc->reg + HPNFC_TRD_STATUS);
		/* mask all threads but selected */
		reg &= (1 << thread);

		if (get_timer(start) > timeout) {
			error("Timeout while waiting for wait_for_thread \n");
			return -1;
		}

	} while (reg);

	return 0;
}

static int hpnfc_wait_for_idle(struct hpnfc_state_t *hpnfc)
{
	uint32_t reg;
	uint timeout = 100000;
	uint start;

	start = get_timer(0);

	do  {
		reg = readl(hpnfc->reg + HPNFC_CTRL_STATUS);

		if (get_timer(start) > timeout) {
			error("Timeout while waiting for controller idle\n");
			return -ETIMEDOUT;
		}

	} while (reg & HPNFC_CTRL_STATUS_CTRL_BUSY_MASK);

	return 0;
}

/*  This function waits for device initialization */
static int wait_for_init_complete(struct hpnfc_state_t *hpnfc)
{
	uint32_t reg;
	uint timeout = 1000000;
	uint start;

	start = get_timer(0);

	dev_info(hpnfc->dev, "hpnfc->reg 0x%p\n", hpnfc->reg);

	do  {/* get ctrl status register */
		reg = readl(hpnfc->reg + HPNFC_CTRL_STATUS);

		if (get_timer(start) > timeout) {
			error( "Timeout while waiting for controller init complete\n");
			return -ETIMEDOUT;
		}

	} while (((reg & HPNFC_CTRL_STATUS_INIT_COMP_MASK) == 0));

	return 0;
}

/* execute generic command on HPNFC controller */
static int hpnfc_generic_cmd_send(struct hpnfc_state_t *hpnfc, uint8_t thread_nr,
				  uint64_t mini_ctrl_cmd, uint8_t use_intr)
{
	uint32_t reg = 0;
	uint8_t status;

	uint32_t mini_ctrl_cmd_l = mini_ctrl_cmd & 0xFFFFFFFF;
	uint32_t mini_ctrl_cmd_h = mini_ctrl_cmd >> 32;
//	printf("%s: 0 thread_nr %d\n", __func__, thread_nr);

	status = wait_for_thread(hpnfc, thread_nr);
	if (status)
		return status;
//	printf("%s: 1\n", __func__);

	writel(mini_ctrl_cmd_l, hpnfc->reg + HPNFC_CMD_REG2);
	writel(mini_ctrl_cmd_h, hpnfc->reg + HPNFC_CMD_REG3);

	/* select generic command */
	WRITE_FIELD(reg, HPNFC_CMD_REG0_CT, HPNFC_CMD_REG0_CT_GEN);
	/* thread number */
	WRITE_FIELD(reg, HPNFC_CMD_REG0_TN, thread_nr);
	if (use_intr)
		reg |= HPNFC_CMD_REG0_INT_MASK;

	/* issue command */
	writel(reg, hpnfc->reg + HPNFC_CMD_REG0);

	return 0;
}

/* preapre generic command on  HPNFC controller  */
static int hpnfc_generic_cmd_command(struct hpnfc_state_t *hpnfc, uint32_t command,
				     uint64_t addr, uint8_t use_intr)
{

	uint64_t mini_ctrl_cmd = 0;
	uint8_t thread_nr = hpnfc->chip_nr;
	int status;

	pr_debug("%s: 0, command %d thread_nr %d\n", __func__, command, thread_nr);

	switch(command){
	case HPNFC_GCMD_LAY_INSTR_RDPP:
		mini_ctrl_cmd |= HPNFC_GCMD_LAY_TWB_MASK;
		break;
	case HPNFC_GCMD_LAY_INSTR_RDID:
		mini_ctrl_cmd |= HPNFC_GCMD_LAY_TWB_MASK;
		break;
	default:
		break;
	}

	WRITE_FIELD64(mini_ctrl_cmd, HPNFC_GCMD_LAY_INSTR, command);

	WRITE_FIELD64(mini_ctrl_cmd, HPNFC_GCMD_LAY_CS, hpnfc->chip_nr);
	WRITE_FIELD64(mini_ctrl_cmd, HPNFC_GCMD_LAY_INPUT_ADDR0, addr);

	/* send command */
	status = hpnfc_generic_cmd_send(hpnfc, thread_nr, mini_ctrl_cmd, use_intr);
	if (status)
		return status;

   /* wait for thread ready*/
	status = wait_for_thread(hpnfc, thread_nr);
	if (status)
		return status;

	return 0;
}



/* preapre generic command used to data transfer on  HPNFC controller  */
static int hpnfc_generic_cmd_data(struct hpnfc_state_t *hpnfc,
				  generic_data_t *generic_data)
{
	uint64_t mini_ctrl_cmd = 0;
	uint8_t thread_nr = hpnfc->chip_nr;


	WRITE_FIELD64(mini_ctrl_cmd, HPNFC_GCMD_LAY_CS, hpnfc->chip_nr);
	WRITE_FIELD64(mini_ctrl_cmd, HPNFC_GCMD_LAY_INSTR,
				  HPNFC_GCMD_LAY_INSTR_DATA);
	WRITE_FIELD64(mini_ctrl_cmd, HPNFC_GCMD_DIR, generic_data->direction);

	if (generic_data->ecc_en)
		mini_ctrl_cmd |= HPNFC_GCMD_ECC_EN_MASK;

	if (generic_data->scr_en)
		mini_ctrl_cmd |= HPNFC_GCMD_SCR_EN_MASK;

	if (generic_data->erpg_en)
		mini_ctrl_cmd |= HPNFC_GCMD_ERPG_EN_MASK;

	WRITE_FIELD64(mini_ctrl_cmd, HPNFC_GCMD_SECT_SIZE,
				  (uint64_t)generic_data->sec_size);
	WRITE_FIELD64(mini_ctrl_cmd, HPNFC_GCMD_SECT_CNT,
				  (uint64_t)generic_data->sec_cnt);
	WRITE_FIELD64(mini_ctrl_cmd, HPNFC_GCMD_LAST_SIZE,
				  (uint64_t)generic_data->last_sec_size);
	WRITE_FIELD64(mini_ctrl_cmd, HPNFC_GCMD_CORR_CAP,
				  (uint64_t)generic_data->corr_cap);

	return hpnfc_generic_cmd_send(hpnfc, thread_nr, mini_ctrl_cmd,
								  generic_data->use_intr);
}

/* wait for data on slave dma interface */
static int hpnfc_wait_on_sdma_trigg(struct hpnfc_state_t *hpnfc,
				    uint8_t *out_sdma_trd, uint32_t *out_sdma_size)
{
	hpnfc_irq_status_t irq_mask, irq_status;

	irq_mask.trd_status = 0;
	irq_mask.trd_error = 0;
	irq_mask.status = HPNFC_INTR_STATUS_SDMA_TRIGG_MASK
		| HPNFC_INTR_STATUS_SDMA_ERR_MASK
		| HPNFC_INTR_STATUS_UNSUPP_CMD_MASK;

	wait_for_irq(hpnfc, &irq_mask, &irq_status);
	if (irq_status.status == 0){
		error("Timeout while waiting for SDMA\n");
		return -ETIMEDOUT;
	}

	if (irq_status.status & HPNFC_INTR_STATUS_SDMA_TRIGG_MASK){
		*out_sdma_size = readl(hpnfc->reg + HPNFC_SDMA_SIZE);
		*out_sdma_trd  = readl(hpnfc->reg + HPNFC_SDMA_TRD_NUM);
		*out_sdma_trd = READ_FIELD(*out_sdma_trd, HPNFC_SDMA_TRD_NUM_SDMA_TRD);
	}
	else {
		error("SDMA error - irq_status %x\n", irq_status.status);
		return -EIO;
	}

	return 0;
}

/* read data from slave DMA interface */
static int dma_read_data(struct hpnfc_state_t *hpnfc, void *buf, uint32_t size)
{
	int i;
	uint32_t *buf32 = buf;

	if (size & 3){
		return -1;
	}

	for(i = 0; i < size / hpnfc->bytesPerSdmaAccess; i++){
		*buf32 = readl(hpnfc->slave_dma);
		//printf("id buf i:%, val 0x%x\n", i, *buf32);
				buf32++;
	}

	return 0;
}

static void hpnfc_read_buf32(struct mtd_info *mtd, uint8_t *buf, int len)
{
	int status;
	uint8_t sdmatrd_num;
	uint32_t sdma_size;
	generic_data_t generic_data;
	struct hpnfc_state_t *hpnfc = mtd_to_hpnfc(mtd);

	memset(&generic_data, 0, sizeof(generic_data_t));
	generic_data.sec_cnt = 1;
	generic_data.last_sec_size = len;
	generic_data.direction = HPNFC_GCMD_DIR_READ;

	/* wait for finishing operation */
	status = wait_for_rb_ready(hpnfc);
	if (status)
		return;

	/* prepare controller to read data in generic mode */
	status = hpnfc_generic_cmd_data(hpnfc, &generic_data);
	if (status)
		return;

	/* wait until data is read on slave DMA */
	status = hpnfc_wait_on_sdma_trigg(hpnfc, &sdmatrd_num, &sdma_size);
	if (status)
		return;

	/* read data  */
	status = dma_read_data(hpnfc, buf, sdma_size);
	if (status)
		return;

	hpnfc->offset += len;
}


static void hpnfc_read_buf64(struct mtd_info *mtd, uint8_t *buf, int len)
{
	int status;
	uint8_t sdmatrd_num;
	uint32_t sdma_size;
	generic_data_t generic_data;
	uint32_t tmp[2];
	uint8_t sub_size = 4;
	int i = 0;
	struct hpnfc_state_t *hpnfc = mtd_to_hpnfc(mtd);

	memset(&generic_data, 0, sizeof(generic_data_t));
	generic_data.sec_cnt = 1;
	generic_data.last_sec_size = sub_size;
	generic_data.direction = HPNFC_GCMD_DIR_READ;

	/* read is made by 4 bytes because of platform limitation */
	while (len){
		/* send change column command */
		status = hpnfc_generic_cmd_command(hpnfc,
										   HPNFC_GCMD_LAY_INSTR_CHRC,
										   hpnfc->offset,
										   0);
		if (status)
			return;

		/* wait for finishing operation */
		status = wait_for_rb_ready(hpnfc);
		if (status)
			return;

		/* prepare controller to read data in generic mode */
		status = hpnfc_generic_cmd_data(hpnfc, &generic_data);
		if (status)
			return;

		/* wait until data is read on slave DMA */
		status = hpnfc_wait_on_sdma_trigg(hpnfc, &sdmatrd_num, &sdma_size);
		if (status)
			return;

		/* read data  */
		status = dma_read_data(hpnfc, tmp, sdma_size);
		if (status)
			return;

		if (len < sub_size)
			sub_size = len;

		memcpy(buf + i, &tmp[0], sub_size);

		len -= sub_size;
		hpnfc->offset += sub_size;
		i += sub_size;
	}
}

static void hpnfc_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
	struct hpnfc_state_t *hpnfc = mtd_to_hpnfc(mtd);
	if (hpnfc->bytesPerSdmaAccess == 8)
		hpnfc_read_buf64(mtd, buf, len);
	else
		hpnfc_read_buf32(mtd, buf, len);
}


/* read parameter page */
static int read_parameter_page64(struct hpnfc_state_t *hpnfc, uint32_t size)
{
	int status;
	uint8_t sdmatrd_num;
	uint32_t sdma_size;
	generic_data_t generic_data;
	uint32_t tmp[2];
	const uint8_t sub_size = 4;
	uint32_t offset = 0;

	memset(&generic_data, 0, sizeof(generic_data_t));
	generic_data.sec_cnt = 1;
	generic_data.last_sec_size = sub_size;
	generic_data.direction = HPNFC_GCMD_DIR_READ;

	/* execute read parameter page instruction */
	status = hpnfc_generic_cmd_command(hpnfc, HPNFC_GCMD_LAY_INSTR_RDPP, 0, 0);
	if (status)
		return status;

	/* wait for finishing operation */
	status = wait_for_rb_ready(hpnfc);
	if (status)
		return status;

	/* read is made by 4 bytes because of platform limitation */
	while (size){

		/* prepare controller to read data in generic mode */
		status = hpnfc_generic_cmd_data(hpnfc, &generic_data);
		if (status)
			return status;

		/* wait until data is read on slave DMA */
		status = hpnfc_wait_on_sdma_trigg(hpnfc, &sdmatrd_num, &sdma_size);
		if (status)
			return status;

		/* read data (part of parameter page) */
		status = dma_read_data(hpnfc, tmp, sdma_size);
		if (status)
			return status;

		write_dword_to_buf(hpnfc, tmp[0]);

		size -= sub_size;
		offset += sub_size;

		/* send change column command */
		status = hpnfc_generic_cmd_command(hpnfc,
										   HPNFC_GCMD_LAY_INSTR_CHRC, offset,
										   0);
		if (status)
			return status;

		/* wait for finishing operation */
		status = wait_for_rb_ready(hpnfc);
		if (status)
			return status;

	}

	return 0;
}

static int read_parameter_page32(struct hpnfc_state_t *hpnfc, uint32_t size)
{
	int status;
	uint8_t sdmatrd_num;
	uint32_t sdma_size;
	generic_data_t generic_data;
	uint8_t *buffer;

	memset(&generic_data, 0, sizeof(generic_data_t));
	generic_data.sec_cnt = 1;
	generic_data.last_sec_size = size;
	generic_data.direction = HPNFC_GCMD_DIR_READ;

	/* execute read parameter page instruction */
	status = hpnfc_generic_cmd_command(hpnfc, HPNFC_GCMD_LAY_INSTR_RDPP, 0, 0);
	if (status)
		return status;

	/* wait for finishing operation */
	status = wait_for_rb_ready(hpnfc);
	if (status)
		return status;

	/* prepare controller to read data in generic mode */
	status = hpnfc_generic_cmd_data(hpnfc, &generic_data);
	if (status)
		return status;

	/* wait until data is read on slave DMA */
	status = hpnfc_wait_on_sdma_trigg(hpnfc, &sdmatrd_num, &sdma_size);
	if (status)
		return status;

	buffer = get_buf_ptr(hpnfc);
	/* read data (part of parameter page) */
	status = dma_read_data(hpnfc, buffer, sdma_size);
	if (status)
		return status;

	increase_buff_ptr(hpnfc, sdma_size);

	return 0;
}

static int read_parameter_page(struct hpnfc_state_t *hpnfc, uint32_t size)
{    if (hpnfc->bytesPerSdmaAccess == 8)
		return read_parameter_page64(hpnfc, size);
	else
		return read_parameter_page32(hpnfc, size);
}


/* read id of memory */
static int nf_mem_read_id(struct hpnfc_state_t *hpnfc, uint8_t address, uint32_t size)
{
	int status;
	uint8_t sdmatrd_num;
	uint32_t sdma_size;
	generic_data_t generic_data;
	uint32_t tmp[2];

	memset(&generic_data, 0, sizeof(generic_data_t));
	generic_data.sec_cnt = 1;
	generic_data.last_sec_size = size;
	generic_data.direction = HPNFC_GCMD_DIR_READ;

	/* execute read ID instruction */
	status = hpnfc_generic_cmd_command(hpnfc, HPNFC_GCMD_LAY_INSTR_RDID,
									   address, 0);
	if (status)
		return status;

	/* wait for finishing operation */
	status = wait_for_rb_ready(hpnfc);
	if (status)
		return status;

	/* prepare controller to read data in generic mode */
	status = hpnfc_generic_cmd_data(hpnfc, &generic_data);
	if (status)
		return status;

	/* wait until data is read on slave DMA */
	status = hpnfc_wait_on_sdma_trigg(hpnfc, &sdmatrd_num, &sdma_size);
	if (status)
		return status;

	/* read data (flash id) */
	status = dma_read_data(hpnfc, tmp, sdma_size);
	if (status)
		return status;

	write_dword_to_buf(hpnfc, tmp[0]);

	return 0;
}

static void hpnfc_get_dma_data_width(struct hpnfc_state_t *hpnfc)
{
	uint32_t  reg;

	reg = readl(hpnfc->reg + HPNFC_CTRL_FEATURES);

	if (READ_FIELD(reg,  HPNFC_CTRL_FEATURES_DMA_DWITH_64))
		hpnfc->bytesPerSdmaAccess = 8;
	else
		hpnfc->bytesPerSdmaAccess = 4;
}

/* obtain information about connected  */
static int hpnfc_dev_info(struct hpnfc_state_t *hpnfc)
{
	uint32_t  reg;
	uint64_t size;

	reg = readl(hpnfc->reg + HPNFC_DEV_PARAMS_0);
	hpnfc->dev_type = READ_FIELD(reg, HPNFC_DEV_PARAMS_0_DEV_TYPE);

	switch (hpnfc->dev_type) {
	case HPNFC_DEV_PARAMS_0_DEV_TYPE_ONFI:
		printf("Detected ONFI device:\n");
		break;
	case HPNFC_DEV_PARAMS_0_DEV_TYPE_JEDEC:
		printf("Detected JEDEC device:\n");
		break;
	default:
		printf("Error: Device was not detected correctly.\n");
		return -1;
	}

	reg = readl(hpnfc->reg + HPNFC_MANUFACTURER_ID);
	printf("-- Manufacturer ID: 0x%x\n",
			 (unsigned int)READ_FIELD(reg, HPNFC_MANUFACTURER_ID_MID));
	printf("-- Device ID: 0x%x\n",
			 (unsigned int)READ_FIELD(reg, HPNFC_MANUFACTURER_ID_DID));
	reg =  readl(hpnfc->reg + HPNFC_DEV_REVISION);
	printf("-- Device revisions codeword: 0x%x\r\n",
			 reg & 0xffff );

	reg =  readl(hpnfc->reg + HPNFC_NF_DEV_AREAS);
	hpnfc->nand.mtd.oobsize = hpnfc->spare_size =
		READ_FIELD(reg, HPNFC_NF_DEV_AREAS_SPARE_SIZE);
	hpnfc->main_size = hpnfc->nand.mtd.writesize =
		READ_FIELD(reg, HPNFC_NF_DEV_AREAS_MAIN_SIZE);
	printf("-- Page main area size: %u\n", hpnfc->main_size);
	printf("-- Page spare area size: %u\n", hpnfc->spare_size);

	reg =  readl(hpnfc->reg + HPNFC_NF_DEV_LAYOUT);
	hpnfc->nand.mtd.erasesize = READ_FIELD(reg, HPNFC_NF_DEV_LAYOUT_PPB)
		* hpnfc->main_size;

	reg =  readl(hpnfc->reg + HPNFC_DEV_PARAMS_0);
	hpnfc->lun_count = READ_FIELD(reg, HPNFC_DEV_PARAMS_0_NO_OF_LUNS);

	reg =  readl(hpnfc->reg + HPNFC_DEV_BLOCKS_PER_LUN);
	hpnfc->blocks_per_lun = reg;

	if (hpnfc->nand.numchips > 0)
		hpnfc->devnum = hpnfc->nand.numchips;
	hpnfc->chip_nr = 0;

	if (limited_blocks){
		hpnfc->lun_count = 1;
		hpnfc->blocks_per_lun = limited_blocks;
	}

	if (reserved_blocks){
		hpnfc->blocks_per_lun -= reserved_blocks;
		printf( "hpnfc->blocks_per_lun: %d\n", hpnfc->blocks_per_lun);
	}

	hpnfc->nand.page_shift = ffs((unsigned)hpnfc->nand.mtd.writesize) - 1;
	hpnfc->nand.pagemask = (hpnfc->nand.chipsize >>
							hpnfc->nand.page_shift) - 1;
	hpnfc->nand.bbt_erase_shift = ffs((unsigned)hpnfc->nand.mtd.erasesize) - 1;
	hpnfc->nand.phys_erase_shift = ffs((unsigned)hpnfc->nand.mtd.erasesize) - 1;
	hpnfc->nand.chip_shift = ffs((unsigned)hpnfc->nand.chipsize) - 1;

	size = hpnfc->nand.mtd.erasesize;
	size *= hpnfc->blocks_per_lun;
	size *= hpnfc->lun_count;
	hpnfc->nand.chipsize = size;
	if (hpnfc->devnum == 0){
		hpnfc->devnum = HPNFC_DEV_NUM;

	}
	hpnfc->nand.mtd.size = size * hpnfc->devnum;

	return 0;
}

void DumpDescriptor(hpnfc_cdma_desc_t* cdmaDesc)
{
    uint32_t *desc32 = (uint32_t*)cdmaDesc;
    int32_t i;

    printf( "desc addr %p\n", cdmaDesc);
    /* do not print32_t dummy part of descriptor if exists */
    for (i = 0; i < 56 / 4; i += 2) {
        printf("%08"PRIx32": %08"PRIX32"_%08"PRIX32"\n", i * 4, desc32[i + 1], desc32[i]);
    }
    printf("%s", "\n");
}


/* preapre CDMA descriptor */
static void hpnfc_cdma_desc_prepare(hpnfc_cdma_desc_t* cdma_desc, char nf_mem,
									uint32_t flash_ptr, char* mem_ptr,
									uint16_t ctype, uint32_t page_cnt)
{
//	printf("%s 0 cdma_desc %p\n", __func__, cdma_desc);

	memset(cdma_desc, 0, sizeof(hpnfc_cdma_desc_t));

	/* set fields for one descriptor */
	cdma_desc->flash_pointer = (nf_mem << HPNFC_CDMA_CFPTR_MEM_SHIFT)
		+ flash_ptr;
	cdma_desc->command_flags |= HPNFC_CDMA_CF_DMA_MASTER;
	cdma_desc->command_flags  |= HPNFC_CDMA_CF_INT;

	cdma_desc->memory_pointer= (uintptr_t)mem_ptr;
	cdma_desc->status = 0;
	cdma_desc->sync_flag_pointer = 0;
	cdma_desc->sync_arguments = 0;

	cdma_desc->command_type = ctype | (page_cnt - 1);

//	DumpDescriptor(cdma_desc);
	flush_dcache_all();
 }

//static uint8_t hpnfc_check_desc_error(uint32_t desc_status)
//{
//	if (desc_status & HPNFC_CDMA_CS_ERP_MASK)
//		return HPNFC_STAT_ERASED;
//
//	if (desc_status & HPNFC_CDMA_CS_UNCE_MASK)
//		return HPNFC_STAT_ECC_UNCORR;
//
//
//	if (desc_status & HPNFC_CDMA_CS_ERR_MASK) {
//		error(":CDMA descriptor error flag detected.\n");
//		return HPNFC_STAT_FAIL;
//	}
//
//	if (READ_FIELD(desc_status, HPNFC_CDMA_CS_MAXERR)){
//		return HPNFC_STAT_ECC_CORR;
//	}
//
//	return HPNFC_STAT_FAIL;
//}

//static int hpnfc_wait_cdma_finish(hpnfc_cdma_desc_t* cdma_desc)
//{
//	hpnfc_cdma_desc_t *desc_ptr;
//	uint8_t status = HPNFC_STAT_BUSY;
//
//	desc_ptr = cdma_desc;
//
//	do {
//		printf("desc_ptr->status %d\n", desc_ptr->status);
//
//		if (desc_ptr->status & HPNFC_CDMA_CS_FAIL_MASK) {
//			status = hpnfc_check_desc_error(desc_ptr->status);
//			error("CDMA error %x\n", desc_ptr->status);
//			break;
//		}
//
//		if (desc_ptr->status & HPNFC_CDMA_CS_COMP_MASK) {
//			/* descriptor finished with no errors */
//			if (desc_ptr->command_flags & HPNFC_CDMA_CF_CONT) {
//				/* not last descriptor */
//				desc_ptr =
//					(hpnfc_cdma_desc_t*)(uintptr_t)desc_ptr->next_pointer;
//			} else {
//				/* last descriptor  */
//				status = HPNFC_STAT_OK;
//			}
//		}
//	} while (status == HPNFC_STAT_BUSY);
//
//	return status;
//}

extern gd_t *gd;


static int hpnfc_cdma_send(hpnfc_state_t *hpnfc, uint8_t thread)
{
	uint32_t reg = 0;
	int status;
	uint64_t org_dam_cdma_desc = hpnfc->dma_cdma_desc;

	/* wait for thread ready*/
	status = wait_for_thread(hpnfc, thread);
	if (status)
		return status;
	writel((uint32_t)org_dam_cdma_desc, hpnfc->reg + HPNFC_CMD_REG2);
	writel(((uint64_t)org_dam_cdma_desc)>>32 , hpnfc->reg + HPNFC_CMD_REG3);

	/* select CDMA mode */
	WRITE_FIELD(reg, HPNFC_CMD_REG0_CT, HPNFC_CMD_REG0_CT_CDMA);
	/* thread number */
	WRITE_FIELD(reg, HPNFC_CMD_REG0_TN, thread);
	/* issue command */

	writel(reg, hpnfc->reg + HPNFC_CMD_REG0);

#ifdef DBG_DUMP_HPNFC_REG
	dump_nfc_reg(hpnfc, PART_1_REG_START, PART_1_REG_END);
	dump_nfc_reg(hpnfc, PART_2_REG_START, PART_2_REG_END);
	dump_nfc_reg(hpnfc, PART_3_REG_START, PART_3_REG_END);
	dump_nfc_reg(hpnfc, PART_4_REG_START, PART_4_REG_END);
#endif

	return 0;
}

/* send SDMA command and wait for finish */
static uint32_t hpnfc_cdma_send_and_wait(hpnfc_state_t *hpnfc, uint8_t thread)
{
	int status;
	hpnfc_irq_status_t irq_mask, irq_status;

	status = hpnfc_cdma_send(hpnfc, thread);
	if (status)
		return status;

	irq_mask.trd_status = 1 << thread;
	irq_mask.trd_error = 1 << thread;
	irq_mask.status = HPNFC_INTR_STATUS_CDMA_TERR_MASK;
	wait_for_irq(hpnfc, &irq_mask, &irq_status);

	if ((irq_status.status == 0) && (irq_status.trd_status == 0)
		&& (irq_status.trd_error == 0)){
		error("CDMA command timeout\n");
		return -ETIMEDOUT;
	}
	if (irq_status.status & irq_mask.status){
		error("CDMA command failed\n");
		dump_stack();
		BUG();
		return -EIO;
	}

	return 0;
}

/* HPNFC hardware initialization */
static int hpnfc_hw_init(hpnfc_state_t *hpnfc)
{
	uint32_t reg;
	int status;

	status = wait_for_init_complete(hpnfc);
	if (status)
		return status;

	/* disable cache and multiplane */
	writel(0, hpnfc->reg + HPNFC_MULTIPLANE_CFG);
	writel(0, hpnfc->reg + HPNFC_CACHE_CFG);

	/* enable interrupts */
	reg = HPNFC_INTR_ENABLE_INTR_EN_MASK
		| HPNFC_INTR_ENABLE_CDMA_TERR_EN_MASK
		| HPNFC_INTR_ENABLE_DDMA_TERR_EN_MASK
		| HPNFC_INTR_ENABLE_UNSUPP_CMD_EN_MASK
		| HPNFC_INTR_ENABLE_SDMA_TRIGG_EN_MASK
		| HPNFC_INTR_ENABLE_SDMA_ERR_EN_MASK;
	writel(reg, hpnfc->reg + HPNFC_INTR_ENABLE);
	/* clear all interrupts */
	writel(0xFFFFFFFF, hpnfc->reg + HPNFC_INTR_STATUS);
	/* enable signaling thread error interrupts for all threads  */
	writel(0xFF, hpnfc->reg + HPNFC_TRD_ERR_INT_STATUS_EN);

	return 0;
}

/* function reads BCH configuration */
static int hpnfc_read_bch_cfg(struct hpnfc_state_t *hpnfc)
{
    uint32_t reg;
    reg = readl(hpnfc->reg + HPNFC_BCH_CFG_0);
    hpnfc->bch_cfg.corr_caps[0] = READ_FIELD(reg, HPNFC_BCH_CFG_0_CORR_CAP_0);
    hpnfc->bch_cfg.corr_caps[1] = READ_FIELD(reg, HPNFC_BCH_CFG_0_CORR_CAP_1);
    hpnfc->bch_cfg.corr_caps[2] = READ_FIELD(reg, HPNFC_BCH_CFG_0_CORR_CAP_2);
    hpnfc->bch_cfg.corr_caps[3] = READ_FIELD(reg, HPNFC_BCH_CFG_0_CORR_CAP_3);


    reg = readl(hpnfc->reg + HPNFC_BCH_CFG_1);
    hpnfc->bch_cfg.corr_caps[4] = READ_FIELD(reg, HPNFC_BCH_CFG_1_CORR_CAP_4);
    hpnfc->bch_cfg.corr_caps[5] = READ_FIELD(reg, HPNFC_BCH_CFG_1_CORR_CAP_5);
    hpnfc->bch_cfg.corr_caps[6] = READ_FIELD(reg, HPNFC_BCH_CFG_1_CORR_CAP_6);
    hpnfc->bch_cfg.corr_caps[7] = READ_FIELD(reg, HPNFC_BCH_CFG_1_CORR_CAP_7);

    reg = readl(hpnfc->reg + HPNFC_BCH_CFG_2);
    hpnfc->bch_cfg.sector_sizes[0] = READ_FIELD(reg, HPNFC_BCH_CFG_2_SECT_0);
    hpnfc->bch_cfg.sector_sizes[1] = READ_FIELD(reg, HPNFC_BCH_CFG_2_SECT_1);

    return 0;
}

/* calculate size of check bit size per one sector */
static int bch_calculate_ecc_size(struct hpnfc_state_t *hpnfc,
								  uint32_t *check_bit_size)
{
	uint8_t mult = 14;
	uint32_t corr_cap, tmp;
	uint32_t max_sector_size;
	int i;

	*check_bit_size = 0;

	pr_debug("hpnfc->sector_size %d\n", hpnfc->sector_size);

	for (i = 0; i < HPNFC_BCH_MAX_NUM_SECTOR_SIZES; i++){
		printf("hpnfc->bch_cfg.sector_sizes[i] %d\n", hpnfc->bch_cfg.sector_sizes[i]);
		if (hpnfc->sector_size == hpnfc->bch_cfg.sector_sizes[i]){
			break;
		}
	}

	if (i >= HPNFC_BCH_MAX_NUM_SECTOR_SIZES){
		error( "Wrong ECC configuration, ECC sector size:%u"
				"is not supported. List of supported sector sizes\n", hpnfc->sector_size);
		for (i = 0; i < HPNFC_BCH_MAX_NUM_SECTOR_SIZES; i++){
			if (hpnfc->bch_cfg.sector_sizes[i] == 0){
				break;
				error( "%u ", hpnfc->bch_cfg.sector_sizes[i]);
			}
		}
		return -1;
	}


	if (hpnfc->bch_cfg.sector_sizes[1] >hpnfc->bch_cfg.sector_sizes[0])
		max_sector_size = hpnfc->bch_cfg.sector_sizes[1];
	else
		max_sector_size = hpnfc->bch_cfg.sector_sizes[0];

	switch(max_sector_size){
	case 256:
		mult = 12;
		break;
	case 512:
		mult = 13;
		break;
	case 1024:
		mult = 14;
		break;
	case 2048:
		mult = 15;
		break;
	default:
		return -EINVAL;
	}

	printf("hpnfc->corr_cap %d\n", hpnfc->corr_cap);

	for (i = 0; i < HPNFC_BCH_MAX_NUM_CORR_CAPS; i++){

		printf("hpnfc->bch_cfg.corr_caps[i]) %d\n", hpnfc->bch_cfg.corr_caps[i]);


		if (hpnfc->corr_cap == hpnfc->bch_cfg.corr_caps[i]){
			corr_cap = hpnfc->corr_cap;
			break;
		}
	}

	if (i >= HPNFC_BCH_MAX_NUM_CORR_CAPS){
		error("Wrong ECC configuration, correction capability:%d"
			  " is not supported. List of supported corrections: \n", hpnfc->corr_cap);
		for (i = 0; i < HPNFC_BCH_MAX_NUM_CORR_CAPS; i++){
			if (hpnfc->bch_cfg.corr_caps[i] == 0)
				break;
			error("%d ", hpnfc->bch_cfg.corr_caps[i]);
		}
		return -1;
	}

	tmp = (mult * corr_cap) / 16;
	/* round up */
	if ((tmp * 16)< (mult * corr_cap)){
		tmp++;
	}

	/* check bit size per one sector */
	*check_bit_size = 2 * tmp;

	return 0;
}

#define TT_SPARE_AREA           1
#define TT_MAIN_SPARE_AREAS     2
#define TT_RAW_SPARE_AREA       3

/* preapre size of data to transfer */
static int hpnfc_prepare_data_size(hpnfc_state_t *hpnfc, int transfer_type)
{
	uint32_t reg = 0;
	uint32_t sec_size = 0, last_sec_size, offset, sec_cnt;
	uint32_t ecc_size = hpnfc->nand.ecc.bytes;

	if (transfer_type == TT_MAIN_SPARE_AREAS)
		pr_debug("%s transfer_type %d\n", __func__, transfer_type);

	if (hpnfc->curr_trans_type == transfer_type)
		return 0;

	switch (transfer_type) {
	case TT_SPARE_AREA:
		offset = hpnfc->main_size - hpnfc->sector_size;
		ecc_size = ecc_size * (offset / hpnfc->sector_size);
		offset = offset + ecc_size;
		sec_cnt = 1;
		last_sec_size = hpnfc->sector_size + hpnfc->usnused_spare_size;
		break;
	case TT_MAIN_SPARE_AREAS:
		offset = 0;
		sec_cnt = hpnfc->sector_count;
		last_sec_size = hpnfc->sector_size;
		sec_size = hpnfc->sector_size;
		pr_debug("sec_cnt %d\n", sec_cnt);
		pr_debug("hpnfc->sector_size %d\n", hpnfc->sector_size);
		pr_debug("hpnfc->usnused_spare_size %d\n", hpnfc->usnused_spare_size);
		break;
	case TT_RAW_SPARE_AREA:
		offset = hpnfc->main_size;
		sec_cnt = 1;
		last_sec_size = hpnfc->usnused_spare_size;
		break;
	default:
		error("Data size preparation failed\n");
		return -EINVAL;
	}

	reg = 0;
	WRITE_FIELD(reg, HPNFC_TRAN_CFG_0_OFFSET, offset);
	WRITE_FIELD(reg, HPNFC_TRAN_CFG_0_SEC_CNT, sec_cnt);
	if (transfer_type == TT_MAIN_SPARE_AREAS)
		pr_debug("trancfg0 0x%x\n", reg);
	writel(reg, hpnfc->reg + HPNFC_TRAN_CFG_0);

	reg = 0;
	WRITE_FIELD(reg, HPNFC_TRAN_CFG_1_LAST_SEC_SIZE, last_sec_size);
	WRITE_FIELD(reg, HPNFC_TRAN_CFG_1_SECTOR_SIZE, sec_size);
	if (transfer_type == TT_MAIN_SPARE_AREAS)
		pr_debug("trancfg1 0x%x\n", reg);
	writel(reg, hpnfc->reg + HPNFC_TRAN_CFG_1);

//	hpnfc->curr_trans_type = transfer_type;

	return 0;
}

/* write data to flash memory using CDMA command */
static int cdma_write_data(struct mtd_info *mtd, int page, int page_cnt, uint8_t *buf)
{
	struct hpnfc_state_t *hpnfc = mtd_to_hpnfc(mtd);
	hpnfc_cdma_desc_t *cdma_desc = hpnfc->cdma_desc;
	uint8_t thread_nr = hpnfc->chip_nr;
	int status = 0;

	pr_debug("%s page 0x%x, page_cnt %d, buf 0x%p\n", __func__, page, page_cnt, buf);

	hpnfc_ecc_enable(hpnfc, hpnfc->ecc_enabled);
	hpnfc_cdma_desc_prepare(cdma_desc, hpnfc->chip_nr, page, (void*)buf,
				HPNFC_CDMA_CT_WR, page_cnt);
	flush_dcache_all();

	status = hpnfc_cdma_send_and_wait(hpnfc, thread_nr);

	if (status)
		return status;

	flush_dcache_all();

	return 0;
}

/* get corrected ECC errors of last read operation */
static uint32_t get_ecc_count(hpnfc_state_t *hpnfc)
{
	return READ_FIELD(hpnfc->cdma_desc->status, HPNFC_CDMA_CS_MAXERR);
}

static uint8_t hpnfc_check_desc_error(uint32_t desc_status)
{
	if (desc_status & HPNFC_CDMA_CS_ERP_MASK)
		return HPNFC_STAT_ERASED;

	if (desc_status & HPNFC_CDMA_CS_UNCE_MASK)
		return HPNFC_STAT_ECC_UNCORR;

	if (desc_status & HPNFC_CDMA_CS_ERR_MASK) {
		pr_err("CDMA descriptor error flag detected.\n");
		return HPNFC_STAT_FAIL;
	}

	if (READ_FIELD(desc_status, HPNFC_CDMA_CS_MAXERR))
		return HPNFC_STAT_ECC_CORR;

	return HPNFC_STAT_FAIL;
}

static int hpnfc_wait_cdma_finish(struct hpnfc_cdma_desc_t *cdma_desc)
{
	struct hpnfc_cdma_desc_t *desc_ptr;
	uint8_t status = HPNFC_STAT_BUSY;

	desc_ptr = cdma_desc;

	do {
		if (desc_ptr->status & HPNFC_CDMA_CS_FAIL_MASK) {
			status = hpnfc_check_desc_error(desc_ptr->status);
			pr_info("CDMA error %x\n", desc_ptr->status);
			break;
		}
		if (desc_ptr->status & HPNFC_CDMA_CS_COMP_MASK) {
			/* descriptor finished with no errors */
			if (desc_ptr->command_flags & HPNFC_CDMA_CF_CONT) {
				/* not last descriptor */
				desc_ptr =
					(struct hpnfc_cdma_desc_t *)(uintptr_t)desc_ptr->next_pointer;
			} else {
				/* last descriptor  */
				status = HPNFC_STAT_OK;
			}
		}
	} while (status == HPNFC_STAT_BUSY);

	return status;
}

/* read data from flash memory using CDMA command */
static int cdma_read_data(struct mtd_info *mtd, int page, bool with_ecc,
			     uint32_t *ecc_err_count, int page_cnt, uint8_t *buf)
{
	struct hpnfc_state_t *hpnfc = mtd_to_hpnfc(mtd);
	hpnfc_cdma_desc_t *cdma_desc = hpnfc->cdma_desc;
	uint8_t thread_nr = hpnfc->chip_nr;
	int status;

	pr_debug("%s page 0x%x, page_cnt %d, buf 0x%p\n", __func__, page, page_cnt, buf);

	hpnfc_ecc_enable(hpnfc, with_ecc & hpnfc->ecc_enabled);
	hpnfc_cdma_desc_prepare(cdma_desc, hpnfc->chip_nr, page, (void*)buf, HPNFC_CDMA_CT_RD, page_cnt);
	flush_dcache_all();

	status = hpnfc_cdma_send_and_wait(hpnfc, thread_nr);

	hpnfc_ecc_enable(hpnfc, hpnfc->ecc_enabled);

	status = hpnfc_wait_cdma_finish(hpnfc->cdma_desc);

	if (status == HPNFC_STAT_ECC_CORR){
		*ecc_err_count = get_ecc_count(hpnfc);
	}
	return status;
}



/* reads OOB data from the device */
static int read_oob_data(struct mtd_info *mtd, uint8_t *buf, int page)
{
	struct hpnfc_state_t *hpnfc = mtd_to_hpnfc(mtd);
	int status = 0;

	pr_debug("%s, page 0x%x, r 0x%p\n", __func__, page, __builtin_return_address(0));

	hpnfc->page = page;

	status = hpnfc_prepare_data_size(hpnfc, TT_RAW_SPARE_AREA);
	if (status)
		return -EIO;

	status = cdma_read_data(mtd, page, 0, NULL, 1, buf);

	if (status)
		printf("read_oob_data status %d\n", status);

	switch(status){
	case HPNFC_STAT_ERASED:
		printf("erased block @page 0x%x\n", page);
		break;
	case HPNFC_STAT_ECC_UNCORR:
		status = hpnfc_prepare_data_size(hpnfc, TT_RAW_SPARE_AREA);
		if (status)
			return -EIO;

		status = cdma_read_data(mtd, page, 0, NULL, 1, buf);
		if (status)
			error( "read oob failed\n");

		break;
	case HPNFC_STAT_OK:
	case HPNFC_STAT_ECC_CORR:
		break;
	default:
		error("read oob failed\n");
		return -EIO;
	}

	return 0;
}

/*
 * this function examines buffers to see if they contain data that
 * indicate that the buffer is part of an erased region of flash.
 */
static bool is_erased(uint8_t *buf, int len)
{
	int i;

	for (i = 0; i < len; i++)
		if (buf[i] != 0xFF)
			return false;

	return true;
}


/*
 * writes a page. user specifies type, and this function handles the
 * configuration details.
 */
static int write_page(struct mtd_info *mtd, struct nand_chip *chip,
			const uint8_t *buf, int len_xfer)
{
	struct hpnfc_state_t *hpnfc = mtd_to_hpnfc(mtd);
	int status = 0;
	uint32_t page = hpnfc->page;

	status = hpnfc_prepare_data_size(hpnfc, TT_MAIN_SPARE_AREAS);

	if (status) {
		error("hpnfc_prepare_data_size failed\n");
		return -EIO;
	}

	return cdma_write_data(mtd, page, len_xfer >> chip->page_shift, (void *)buf);
}

/* NAND core entry points */

/*
 * this is the callback that the NAND core calls to write a page. Since
 * writing a page with ECC or without is similar, all the work is done
 * by write_page above.
 */
static int hpnfc_write_page(struct mtd_info *mtd, struct nand_chip *chip,
			    const uint8_t *buf, int data_len, int page)
{
	/*
	 * for regular page writes, we let HW handle all the ECC
	 * data written to the device.
	 */
	return write_page(mtd, chip, buf, data_len);
}

static int hpnfc_read_oob(struct mtd_info *mtd, struct nand_chip *chip,
			     int page)
{
	return read_oob_data(mtd, chip->oob_poi, page);
}

#define HPNFC_CMD_REG0_THREAD_WRITE(thread)     ((uint32_t)thread << 24)

#define HPNFC_PIO_CMD_REG0_THREAD_WRITE         HPNFC_CMD_REG0_THREAD_WRITE

#define HPNFC_PIO_CMD_REG0_CC_SHIFT             0
/* command code field mask */
#define HPNFC_PIO_CMD_REG0_CC_MASK              (0xFFFFuL << 0)
/* command code - read page */
#define HPNFC_PIO_CMD_REG0_CC_RD                (0x2200uL)
/* command code - write page */
#define HPNFC_PIO_CMD_REG0_CC_WR                (0x2100uL)
/* command code - copy back */
#define HPNFC_PIO_CMD_REG0_CC_CPB               (0x1200uL)
/* command code - reset */
#define HPNFC_PIO_CMD_REG0_CC_RST               (0x1100uL)
/* command code - set feature */
#define HPNFC_PIO_CMD_REG0_CC_SF                (0x0100uL)
/* command code - erase block */
#define HPNFC_PIO_CMD_REG0_CC_ER                (0x1000uL)
/* command interrupt shift */
#define HPNFC_PIO_CMD_REG0_INT_SHIFT             20
/* command interrupt mask */
#define HPNFC_PIO_CMD_REG0_INT_MASK              HPNFC_CMD_REG0_INT_MASK
/* select DMA master engine for this command */
#define HPNFC_PIO_CMD_REG0_DMA_MASTER_MASK       (1uL << 21)

#define HPNFC_PIO_CMD_REG0_VOL_ID_WRITE(vId)    ((vId & 0xF) << 16)

#define HPNFC_PIO_CMD_REG1_BANK_WRITE(bank)     ((bank & 3) << 24)
#define HPNFC_PIO_CMD_REG1_FADDR_WRITE(fAddr)   (fAddr & 0xFF)
#define HPNFC_PIO_CMD_REG1_ROW_ADRR_WRITE(row)  (row & 0xFF)

/* max supported thread count */
#define HPNFC_MAX_THREAD_COUNT          16
/* max supported command count for PIO and CMDA mode */
#define HPNFC_MAX_CMD_COUNT             0x100
/* max supported extended command count for PIO and CMDA mode */
#define HPNFC_MAX_EXT_CMD_COUNT         0x10000
#define HPNFC_MAX_CMD_COUNT_MASK        (HPNFC_MAX_CMD_COUNT - 1)
#define HPNFC_MAX_EXT_CMD_COUNT_MASK    (HPNFC_MAX_EXT_CMD_COUNT - 1)

static int hpnfc_read_page_raw(struct mtd_info *mtd, struct nand_chip *chip,
							   uint8_t *buf, int oob_required, int page)
{
	struct hpnfc_state_t *hpnfc = mtd_to_hpnfc(mtd);
	int status = 0;
	pr_debug("%s, oob_req %d, page 0x%x\n", __func__, oob_required, page);

	status = hpnfc_prepare_data_size(hpnfc, TT_MAIN_SPARE_AREAS);
	if(status){
		return -1;
	}

	status = cdma_read_data(mtd, page, 1, NULL, 1, buf);
	switch(status){
	case HPNFC_STAT_ERASED:
	case HPNFC_STAT_ECC_UNCORR:
		status = cdma_read_data(mtd, page, 0, NULL, 1, buf);
//		memcpy(buf, hpnfc->buf.buf, mtd->writesize);
//		memcpy(buf, hpnfc->buf.buf + mtd->writesize, mtd->oobsize);
		if (!is_erased(buf, hpnfc->nand.mtd.writesize + mtd->oobsize)){
			return -1;
		}
	case HPNFC_STAT_ECC_CORR:
	case HPNFC_STAT_OK:
//		memcpy(buf, hpnfc->buf.buf, mtd->writesize);
//		memcpy(buf, hpnfc->buf.buf + mtd->writesize, mtd->oobsize);
		break;
	default:
		error( "read raw page failed\n");
		return -1;
	}

	return 0;
}

static int hpnfc_read_page(struct mtd_info *mtd, struct nand_chip *chip,
				uint8_t *buf, int bytes, int page)
{
	struct hpnfc_state_t *hpnfc = mtd_to_hpnfc(mtd);
	int status = 0;
	uint32_t ecc_err_count = 0;
	int page_cnt = bytes >> chip->page_shift;

	WARN_ON(bytes & (sector_size - 1));

	if (0 == page_cnt)
		page_cnt = 1;

	pr_debug("%s, bytes %d, page 0x%x\n", __func__, bytes, page);

	status = hpnfc_prepare_data_size(hpnfc, TT_MAIN_SPARE_AREAS);

	if (status)
		return -1;

	status = cdma_read_data(mtd, page, 1, &ecc_err_count, page_cnt, buf);

	//printf("%s 1 status %d\n", __func__, status);

	switch (status) {
	case HPNFC_STAT_ERASED:
	case HPNFC_STAT_ECC_UNCORR:
		/* Read again without ECC */
		status = cdma_read_data(mtd, page, 0, NULL, page_cnt, buf);
		if (status){
			pr_err("read page failed\n");
		}
//		memcpy(buf, hpnfc->buf.buf, mtd->writesize);
//		if (oob_required)
//			memcpy(chip->oob_poi, hpnfc->buf.buf + mtd->writesize,
//				   mtd->oobsize);
		if (!is_erased(hpnfc->buf.buf, hpnfc->nand.mtd.writesize + mtd->oobsize)){
			hpnfc->nand.mtd.ecc_stats.failed++;
			ecc_err_count++;
		}
		break;
	case HPNFC_STAT_ECC_CORR:
		if (ecc_err_count)
			hpnfc->nand.mtd.ecc_stats.corrected += ecc_err_count;
		break;
	case HPNFC_STAT_OK:
//		memcpy(buf, hpnfc->buf.buf, mtd->writesize);
//		if (oob_required)
//			memcpy(chip->oob_poi, hpnfc->buf.buf + mtd->writesize,
//				   mtd->oobsize);
		break;
	default:
		pr_err("read page failed\n");
		return -1;
	}

	return ecc_err_count;
}

static int hpnfc_read_subpage(struct mtd_info *mtd, struct nand_chip *chip,
			      u32 data_offs, u32 readlen, u8 *buf, int page)
{
	struct hpnfc_state_t *hpnfc = mtd_to_hpnfc(mtd);
	int status = 0;

	pr_debug("subpage pg 0x%x, offs %d, l %d\n", page, data_offs, readlen);

	memset(buf, 0, sector_size);

	status = hpnfc_read_page(mtd, chip, hpnfc->buf.buf, hpnfc->main_size, page);

	if (status)
		pr_err("read error\n");

//	bbt_dump_buf("hpnfc_read_subpage ", hpnfc->buf.buf, sectorSize / 8);

	memcpy((void *)buf, (void *)&hpnfc->buf.buf[data_offs], readlen);

//	bbt_dump_buf("hpnfc_read_subpage", buf, readlen);

	return 0;
}

static uint8_t hpnfc_read_byte(struct mtd_info *mtd)
{
	struct hpnfc_state_t *hpnfc = mtd_to_hpnfc(mtd);
	uint8_t result = 0xff;

	if (hpnfc->buf.head < hpnfc->buf.tail){
		result = hpnfc->buf.buf[hpnfc->buf.head++];
	}

	return result;
}

static void hpnfc_select_chip(struct mtd_info *mtd, int chip)
{
	struct hpnfc_state_t *hpnfc = mtd_to_hpnfc(mtd);
	hpnfc->chip_nr = chip;
}

static int hpnfc_waitfunc(struct mtd_info *mtd, struct nand_chip *chip)
{
	return 0;
}

static int hpnfc_erase(struct mtd_info *mtd, int page)
{
	struct hpnfc_state_t *hpnfc = mtd_to_hpnfc(mtd);
	uint8_t thread_nr = hpnfc->chip_nr;
	int status;

	pr_debug("%s paget 0x%x\n", __func__, page);

#if 0
	// for testing , skip some block
	if (page == 0x4000 || page == 0x3000) {// in the 1st itbimage area
		error("skip page 0x%x\n", page);
		return 0;
	}
#endif

	hpnfc_cdma_desc_prepare(hpnfc->cdma_desc, hpnfc->chip_nr, page, NULL,
							HPNFC_CDMA_CT_ERASE, 1);

	flush_dcache_all();

	status = hpnfc_cdma_send_and_wait(hpnfc, thread_nr);
	if(status){
		error( "erase operation failed\n");
		return -EIO;
	}

	return 0;
}

static void hpnfc_cmdfunc(struct mtd_info *mtd, unsigned int cmd, int col,
						  int page)
{
	struct hpnfc_state_t *hpnfc = mtd_to_hpnfc(mtd);
	uint32_t reg;

	pr_debug("=> %s, r 0x%p\n", __func__, __builtin_return_address(0));
	pr_debug("cmd %x, col %d, page 0x%x, mtd %p\n", cmd, col, page, mtd);

	hpnfc->offset = 0;
	switch (cmd) {
	case NAND_CMD_PAGEPROG:
		break;
	case NAND_CMD_STATUS:
		reset_buf(hpnfc);
		reg = readl(hpnfc->reg + HPNFC_RBN_SETTINGS);
		if ((reg >> hpnfc->chip_nr) & 0x01)
			write_byte_to_buf(hpnfc, 0xE0);
		else
			write_byte_to_buf(hpnfc, 0x80);
		break;
	case NAND_CMD_READID:
		reset_buf(hpnfc);
		nf_mem_read_id(hpnfc, col, 4);
		break;
	case NAND_CMD_PARAM:
		reset_buf(hpnfc);
		read_parameter_page(hpnfc, 4096);
		break;
	case NAND_CMD_READ0:
	case NAND_CMD_SEQIN:
		hpnfc->page = page;
		break;
	case NAND_CMD_RESET:
		/* resets a specific device connected to the core */
		break;
	case NAND_CMD_READOOB:
		read_oob_data(mtd, hpnfc->buf.buf, page);
		break;
	case NAND_CMD_RNDOUT:
		hpnfc->offset = col;
		break;
	default:
		dev_warn(hpnfc->dev, "unsupported command received 0x%x\n", cmd);
		break;
	}
}
/* end NAND core entry points */

#define RF_CTRL_CONFIG__RT_ECC_CONFIG_0__ECC_ENABLE__MODIFY(dst, src) \
                    (dst) = ((dst) &\
                    ~0x00000001U) | ((uint32_t)(src) &\
                    0x00000001U)

#define RF_CTRL_CONFIG__RT_ECC_CONFIG_0__SCRAMBLER_EN__MODIFY(dst, src) \
                    (dst) = ((dst) &\
                    ~0x00000010U) | (((uint32_t)(src) <<\
                    4) & 0x00000010U)

#define RF_CTRL_CONFIG__RT_ECC_CONFIG_0__CORR_STR__MODIFY(dst, src) \
                    (dst) = ((dst) &\
                    ~0x00000700U) | (((uint32_t)(src) <<\
                    8) & 0x00000700U)


#define RF_MINICTRL_REGS__RT_SKIP_BYTES_OFFSET__SKIP_BYTES_OFFSET__WRITE(src) \
                    ((uint32_t)(src)\
                    & 0x00ffffffU)

#define RF_MINICTRL_REGS__RT_SKIP_BYTES_CONF__MARKER__WRITE(src) \
                    (((uint32_t)(src)\
                    << 16) & 0xffff0000U)

#define RF_MINICTRL_REGS__RT_SKIP_BYTES_CONF__SKIP_BYTES__WRITE(src) \
                    ((uint32_t)(src)\
                    & 0x000000ffU)


int32_t hpnfc_core_set_ecc_config(struct hpnfc_state_t *hpnfc,
				  struct hpnfc_ecc_configuration_t *eccConfig)
{
	uint32_t reg;
	int32_t status = 0;
	uint8_t corrCapIdx = 0;

	if (status) {
		error("WaitForIdle fail\n");
		return (status);
	}

	reg = readl(hpnfc->reg + HPNFC_ECC_CONFIG_0);

	RF_CTRL_CONFIG__RT_ECC_CONFIG_0__ECC_ENABLE__MODIFY(reg, eccConfig->ecc_enabled);
	RF_CTRL_CONFIG__RT_ECC_CONFIG_0__SCRAMBLER_EN__MODIFY(reg, eccConfig->scrambler_enabled);

	//status = GetEccCorrCapIdx(hpnfc, eccConfig->corrCap, &corrCapIdx);
	if (status) {
		pr_err("Wrong ECC corection capability settings.\n");
		return (status);
	}

	RF_CTRL_CONFIG__RT_ECC_CONFIG_0__CORR_STR__MODIFY(reg, corrCapIdx);

	writel(reg, hpnfc->reg + HPNFC_ECC_CONFIG_0);

	reg = RF_MINICTRL_REGS__RT_SKIP_BYTES_OFFSET__SKIP_BYTES_OFFSET__WRITE(eccConfig->skip_bytes_offset);
	writel(reg, hpnfc->reg + 0x1010);

	reg = RF_MINICTRL_REGS__RT_SKIP_BYTES_CONF__MARKER__WRITE(eccConfig->marker)
		| RF_MINICTRL_REGS__RT_SKIP_BYTES_CONF__SKIP_BYTES__WRITE(eccConfig->skip_bytes);
	writel(reg, hpnfc->reg + 0x100C);

	return (0);
}

struct hpnfc_nand_info {
	void __iomem	*nand_base;
	u8		end_cmd_pending;
	u8		end_cmd;
};

#define ONDIE_ECC_FEATURE_ADDR		0x90
#define ONDIE_ECC_FEATURE_ENABLE	0x08
#define TOP_REG_NFC_REMAP39_TO_32	(0x50010064)
#define MAX_PAGE_SIZE			(16 * 1024)

static int hpnfc_nand_init(struct nand_chip *nand_chip, int devnum)
{
	struct hpnfc_state_t *hpnfc;
	struct mtd_info *mtd;
	int err = -1;
	int ret, status;
	uint32_t ecc_per_sec_size;
	uint32_t ddr_val;

	/* Allow HPNFC to access DDR */
	ddr_val = readl(TOP_REG_NFC_REMAP39_TO_32);
	ddr_val |= 1 << 8;
	writel(ddr_val, TOP_REG_NFC_REMAP39_TO_32);
	ddr_val = readl(TOP_REG_NFC_REMAP39_TO_32);

	hpnfc = container_of(nand_chip, struct hpnfc_state_t, nand);
	hpnfc->buf.buf = memalign(ARCH_DMA_MINALIGN, MAX_PAGE_SIZE);

	if (!hpnfc->buf.buf) {
		error("%s: failed to allocate hpnfc->buf.buf\n", __func__);
		goto fail;
	}

//	printf("hpnfc init buf.buf 0x%p\n", hpnfc->buf.buf);

	hpnfc->cdma_desc = calloc(1, sizeof(hpnfc_cdma_desc_t));

	if (!hpnfc->cdma_desc) {
		error("%s: failed to allocate cdma_desc\n", __func__);
		goto fail;
	}
	hpnfc->dma_cdma_desc = (uint64_t) hpnfc->cdma_desc;

	hpnfc->reg = (void __iomem *) 0x500C0000;
	hpnfc->slave_dma = (void __iomem *) 0x500C3000;

	mtd = &nand_chip->mtd ;

	hpnfc_get_dma_data_width(hpnfc);

	ret = hpnfc_hw_init(hpnfc);
	if (ret) {
		error("hpnfc_hw_init fail\n");
		goto fail;
	}
	mtd->priv = nand_chip;

	hpnfc_set_work_mode(hpnfc, HPNFC_WORK_MODE_ASYNC, 0);

	nand_chip->cmdfunc = hpnfc_cmdfunc;
	nand_chip->select_chip = hpnfc_select_chip;
	nand_chip->read_byte = hpnfc_read_byte;
	nand_chip->waitfunc = hpnfc_waitfunc;
	nand_chip->chip_delay = 30;

	/* Buffer read/write routines */
	nand_chip->read_buf = hpnfc_read_buf;

	/* Don't access oob area by SW */
	nand_chip->bbt_options = NAND_BBT_USE_FLASH | NAND_BBT_NO_OOB;

	/* first scan to find the device and get the page size */
	if (nand_scan_ident(mtd, 1, NULL)) {
		error("%s: nand_scan_ident failed\n", __func__);
		goto fail;
	}

	/* Get info about memory parameters  */
	if (hpnfc_dev_info(hpnfc)) {
		error("HW controller dev info failed\n");
		ret = -ENXIO;
		goto fail;
	}

	/* Error correction */
	hpnfc->nand.ecc.mode = NAND_ECC_HW;
	hpnfc->sector_size = sector_size;
	hpnfc->corr_cap = ecc_corr_cap;

	hpnfc->sector_count = hpnfc->main_size / hpnfc->sector_size;

	pr_info("hpnfc->main_size %d\n", hpnfc->main_size);
	pr_info("hpnfc->sector_count %d\n", hpnfc->sector_count);
	pr_info("hpnfc->sector_size %d\n", hpnfc->sector_size);

	hpnfc_read_bch_cfg(hpnfc);
	status = bch_calculate_ecc_size(hpnfc, &ecc_per_sec_size);
	if (status) {
		printf("bch_calculate_ecc_size error %d\n", status);
		hpnfc->ecc_enabled = 0;
		hpnfc->corr_cap = 0;
		/* despite lack of support HW ECC, set info that ECC is supported.
		 * If  NAND_ECC_NONE is set then nand_scan_tail replaces page read
		 * write function pointers
		 */
		hpnfc->nand.ecc.mode = NAND_ECC_HW;
		hpnfc->sector_count = 1;
		hpnfc->sector_size = hpnfc->main_size;
		hpnfc->nand.ecc.strength = 2;
		goto fail;
	} else {
		pr_info("ECC enabled, corr cap: %d, sec  size %u, ecc_per_sec_size %d\n",
			hpnfc->corr_cap, hpnfc->sector_size, ecc_per_sec_size);
		hpnfc->ecc_enabled = 1;
		hpnfc->nand.ecc.strength = hpnfc->corr_cap;
	}

	hpnfc_ecc_enable(hpnfc, hpnfc->ecc_enabled);

	if ((hpnfc->sector_count * ecc_per_sec_size) >=
	    (hpnfc->spare_size - HPNFC_MINIMUM_SPARE_SIZE)) {
		/* to small spare area to hanlde such big ECC */
		ret = -EIO;
		return ret;
	}

	hpnfc->usnused_spare_size = hpnfc->spare_size - hpnfc->sector_count * ecc_per_sec_size;

	if (hpnfc->usnused_spare_size > HPNFC_MAX_SPARE_SIZE_PER_SECTOR)
		hpnfc->usnused_spare_size = HPNFC_MAX_SPARE_SIZE_PER_SECTOR;

	/* real spare size is hidden for MTD layer
	 * because hardware handle all ECC stuff
	 */

	hpnfc->nand.ecc.bytes = ecc_per_sec_size;

	/* override the default read operations */
	hpnfc->nand.ecc.size = hpnfc->sector_size;
	hpnfc->nand.ecc.read_page = hpnfc_read_page;
	hpnfc->nand.ecc.read_oob = hpnfc_read_oob;
	hpnfc->nand.ecc.read_page_raw = hpnfc_read_page_raw;
	hpnfc->nand.ecc.read_subpage = hpnfc_read_subpage;
	hpnfc->nand.ecc.write_page = hpnfc_write_page;
	hpnfc->nand.erase = hpnfc_erase;

	hpnfc->nand.options |= NAND_SUBPAGE_READ;

	dev_info(hpnfc->dev, "hpnfc->nand.mtd.writesize %u, hpnfc->nand.mtd.oobsize %u\n",
		 hpnfc->nand.mtd.writesize, hpnfc->nand.mtd.oobsize);
	dev_info(hpnfc->dev, "hpnfc->nand.mtd.erasesize 0x%x, hpnfc->nand.mtd.size 0x%llx\n",
		 hpnfc->nand.mtd.erasesize, hpnfc->nand.mtd.size);

	nand_chip->ecc.mode = NAND_ECC_HW;
	nand_chip->ecc.bytes = 0;

	/* Second phase scan */
	if (nand_scan_tail(mtd)) {
		error("%s: nand_scan_tail failed\n", __func__);
		goto fail;
	}
	if (nand_register(devnum, mtd))
		goto fail;
	return 0;
fail:
	return err;
}

void board_nand_init(void)
{
	struct nand_chip *nand = &g_hpnfc.nand;

	puts("board_nand_init\n");
	if (hpnfc_nand_init(nand, 0))
		error("HPNFC NAND init failed\n");
}

