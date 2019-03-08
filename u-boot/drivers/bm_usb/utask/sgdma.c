/**********************************************************************
 * Copyright (C) 2014-2017 Cadence Design Systems, Inc.
 * All rights reserved worldwide.
 ***********************************************************************
 * sgdma.c
 * DMA Scatter/Gather Driver for USB Controllers
 *
 * NOTE:
 * You shall provide your own DMA routines in case you're using
 * external DMA hardware module.
 ***********************************************************************/
/*#include <stdio.h>*/
#include <stdlib.h>
/*#include <stdint.h>*/
/*#include <string.h>*/
#include <memalign.h>
#include "include/cdn_errno.h"
#include "include/cusb_dma_if.h"
#include "include/sgdma_regs.h"
#include "include/sgdma_if.h"
#include "include/byteorder.h"
#include "include/cps.h"
#include "include/debug.h"

#define DBG_DMA_BASIC_MSG           0x00000100
#define DBG_DMA_VERBOSE_MSG         0x00000200
#define DBG_DMA_ERR_MSG             0x00000400
#define DBG_DMA_CHAIN_MSG           0x00000800
#define DBG_DMA_CHANNEL_USAGE_MSG   0x00001000
#define DBG_DMA_IRQ_MSG             0x00002000

/******************************************************************************
 * Private Driver functions and macros - list management
 *****************************************************************************/

static inline struct DmaTrbChainDesc *sgdmaListGetTrbChainDescEntry(ListHead *list)
{
	return (struct DmaTrbChainDesc *)((uintptr_t)list - (uintptr_t)&(((struct DmaTrbChainDesc *)0)->chainNode));
}

uint32_t divRoundUp(uint32_t divident, uint32_t divisor)
{
	return divisor ? ((divident + divisor - 1) / divisor) : 0;
}

void sgdmaFreeTrbChain(struct DmaController *pD, struct DmaTrbChainDesc *trbChainDesc)
{
	uint32_t i = 0;

	/*INFO("Free %p TRBs pool start %d end %d numOfTrbs %d"*/
	/*     " mapSize %d channel %p trbAddrStart %p trbDmaAddrStart %08x allocated\n",*/
	/*     trbChainDesc,*/
	/*     trbChainDesc->start,*/
	/*     trbChainDesc->end,*/
	/*     trbChainDesc->numOfTrbs,*/
	/*     trbChainDesc->mapSize,*/
	/*     trbChainDesc->channel,*/
	/*     trbChainDesc->trbPool,*/
	/*     trbChainDesc->trbDMAAddr);*/

	for (i = 0; i < trbChainDesc->mapSize; i++)
		pD->trbChainFreeMap[(trbChainDesc->start >> 3) + i] = 0x00;

	trbChainDesc->channel->numOfTrbChain--;
	trbChainDesc->reserved = 0;
	listDelete(&trbChainDesc->chainNode);
}

static uint32_t sgdmaAllocTrbChain(struct DmaController *pD, struct DmaChannel *channel,
				   uint32_t numOfTrbs, struct DmaTrbChainDesc **chain)
{
	uint32_t i = 0, j, count = 0, mapCount = 0;
	struct DmaTrbChainDesc *trbChainDesc = NULL;
	/* find free descriptors for TRB chain */
	for (i = 0; i < TAB_SIZE_OF_DMA_CHAIN; i++) {
		if (!pD->trbChainDesc[i].reserved) {
			trbChainDesc = &pD->trbChainDesc[i];
			break;
		}
	}

	*chain = NULL;

	if (!trbChainDesc) {
		ERROR("No free TRB Chain Descriptor%s\n", "");
		return -ENOMEM;
	}

	VERBOSE("TRB_MAP_SIZE %d, trbChainDesc idx: %d addr %p selected\n", TRB_MAP_SIZE, i, trbChainDesc);

	/** Two additional TRB for LINK TRb and bad TRB, and in order to simplify
	 * algorittm firmware round number of TRBs to module 8
	 */
	mapCount = (numOfTrbs + 2 + 7) >> 3;
	/*Firmware round up reserved number of TRBS to modulo 8*/
	for (i = 0; i < TRB_MAP_SIZE; i++) {
		if (pD->trbChainFreeMap[i] == 0x0)
			count++;
		else
			count = 0;

		if (count == mapCount)
			break;
	}

	if (count != mapCount) {
		VERBOSE("No free TRBs%s\n", "");
		return -ENOMEM;
	}

	trbChainDesc->reserved = 1;
	trbChainDesc->channel = channel;
	trbChainDesc->actualLen = 0;
	trbChainDesc->mapSize = mapCount;
	trbChainDesc->numOfTrbs  = numOfTrbs;
	trbChainDesc->start = (i + 1 - mapCount) << 3;
	trbChainDesc->end = trbChainDesc->start;
	trbChainDesc->trbPool = ((struct DmaTrb *)pD->trbPoolAddr) + trbChainDesc->start;
	trbChainDesc->trbDMAAddr = (uint32_t)(uintptr_t)((struct DmaTrb *)pD->trbDMAPoolAddr + trbChainDesc->start);
	trbChainDesc->isoError = 0;

	/*INFO("TRBs pool start %d end %d numOfTrbs %d"*/
	/*     " mapSize %d channel %p trbAddrStart %p trbDmaAddrStart %lx allocated\n",*/
	/*     trbChainDesc->start,*/
	/*     trbChainDesc->end,*/
	/*     trbChainDesc->numOfTrbs*/
	/*     trbChainDesc->mapSize,*/
	/*     trbChainDesc->channel,*/
	/*     trbChainDesc->trbPool,*/
	/*     trbChainDesc->trbDMAAddr);*/

	/* Chain Descriptor work as FIFO, first was added first will be armed
	 * and serviced
	 */
	listAddTail(&trbChainDesc->chainNode, &channel->trbChainDescList);

	channel->numOfTrbChain++;
	for (j = 0; j < count; j++)
		pD->trbChainFreeMap[i - j] = 0xFF;
	*chain = trbChainDesc;
	return 0;
}

static void sgdmShowTrbChain(struct DmaTrbChainDesc *trbChainDesc)
{
	uint16_t i = 0;

	VERBOSE("idx    | trb size | trb addr |FLAG: NORMAL LINK CHAIN  ALL %s\n", " ");

	for (i = 0; i < trbChainDesc->numOfTrbs + 2; i++) {
		VERBOSE("%02d     | %08x | %08x |         %d      %d     %d    %08x\n",
			i,
			trbChainDesc->trbPool[i].dmaSize & TD_SIZE_MASK,
			trbChainDesc->trbPool[i].dmaAddr,
			(trbChainDesc->trbPool[i].ctrl & TD_TYPE_NORMAL) ? 1 : 0,
			(trbChainDesc->trbPool[i].ctrl & TD_TYPE_LINK) ? 1 : 0,
			(trbChainDesc->trbPool[i].ctrl & TDF_CHAIN_BIT) ? 1 : 0,
			trbChainDesc->trbPool[i].ctrl);
	}
}

/******************************************************************************
 * Private Driver functions for SGDMA
 *****************************************************************************/
/**
 * Function fill in TRBs in trbChainDesc
 */
static uint32_t sgdmaNewTdHw(struct DmaController *pD, struct DmaTrbChainDesc *trbChainDesc)
{
	uint32_t i = 0;
	uint32_t tmp = 0;

	uint32_t startAddress = trbChainDesc->dwStartAddress;
	struct DmaChannel *channel = trbChainDesc->channel;
	uint32_t dataSize = channel->maxTrbLen;

	if (trbChainDesc->numOfTrbs > 1) {
		for (i = 0; i < (trbChainDesc->numOfTrbs - 1); i++) {
			if (channel->dmultEnabled) {
				/*for isochronous transfer in host mode each TRB shoudl be build separately becaouse
				 *each packet can have different size
				 */
				if (trbChainDesc->framesDesc && pD->isHostCtrlMode && channel->isIsoc) {
					BUILD_NORMAL_TRB_NO_IOC(trbChainDesc->trbPool[i],
								trbChainDesc->dwStartAddress +
									trbChainDesc->framesDesc[i].offset,
								trbChainDesc->framesDesc[i].length,
								0);
					continue;
				} else {
					BUILD_NORMAL_TRB_NO_IOC(trbChainDesc->trbPool[i], startAddress, dataSize, 0);
				}
			} else {
				BUILD_NORMAL_TRB_NO_IOC_CHAIN(trbChainDesc->trbPool[i], startAddress, dataSize, 0);
			}

			startAddress += dataSize;
			tmp += dataSize;
		}
	}

	/*Only for ISOC and Host mode*/
	if (trbChainDesc->framesDesc && pD->isHostCtrlMode && channel->isIsoc) {
		BUILD_NORMAL_TRB(trbChainDesc->trbPool[i],
				 trbChainDesc->dwStartAddress + trbChainDesc->framesDesc[i].offset,
				 trbChainDesc->framesDesc[i].length,
				 0);
	} else {
		BUILD_NORMAL_TRB(trbChainDesc->trbPool[i], startAddress, trbChainDesc->len - tmp, 0);
	}
	trbChainDesc->lastTrbIsLink = 0;
	if (channel->dmultEnabled) {
		if (channel->isIsoc) {
			/*In Isochronouse transfer DMA should have always prepared TRB for next data.
			 * In order to ensure that firmware should loop the last trb chain. If firmware
			 * does'n have time to prepare next set of trbs then will be used again the last trb chain.
			 */
			trbChainDesc->lastTrbIsLink = 1;
			BUILD_LINK_TRB(trbChainDesc->trbPool[i + 1], trbChainDesc->trbDMAAddr);
		} else {
			BUILD_EMPTY_TRB(trbChainDesc->trbPool[i + 1]);
		}
#ifndef DISABLE_DCACHE
		flush_cache((uintptr_t)&trbChainDesc->trbPool[0], (i + 2) * sizeof(struct DmaTrb));
#endif
	} else {
#ifndef DISABLE_DCACHE
		flush_cache((uintptr_t)&trbChainDesc->trbPool[0], (i + 1) * sizeof(struct DmaTrb));
#endif
	}

	/*  sgdmShowTrbChain(trbChainDesc);*/
	return 0;
}

/**
 * Function starts DMA transfer
 */
static void sgdmaArmTd(struct DmaController *pD, struct DmaTrbChainDesc *trbChainDesc)
{
	uint32_t epStsUpdate;
	struct DmaChannel *channel = trbChainDesc->channel;

	/* initialize USB controller for data transfer*/
	CPS_UncachedWrite32(&pD->regs->ep_sel, (uint32_t)(channel->isDirTx | channel->hwUsbEppNum));

	epStsUpdate = CPS_UncachedRead32(&pD->regs->ep_sts);

	/*If DMA is not started than firmware can simple prepare and arm transfer*/
	if (channel->status == CUSBDMA_STATUS_FREE) {
		uint32_t cfg = 0;

		VERBOSE("Set DORBEL ChNr-chain-Dir-EpNr-traddr: 0x%p-0x%p-%s-%d-0x%x\n",
			channel,
			trbChainDesc,
			(channel->isDirTx) ? "Tx" : "Rx",
			channel->hwUsbEppNum,
			trbChainDesc->trbDMAAddr);
		CPS_UncachedWrite32(&pD->regs->ep_sts, DMARF_EP_TRBERR);
		CPS_UncachedWrite32(&pD->regs->traddr, (uint32_t)trbChainDesc->trbDMAAddr);
		/* CPS_UncachedWrite32(&pD->regs->drbl, 1);  markmarkmark*/
		/*Before starting DMA firmware clear old TRBERROR interrupt that could not be handled */
		CPS_UncachedWrite32(&pD->regs->ep_sts, DMARF_EP_TRBERR);

		cfg = CPS_UncachedRead32(&pD->regs->ep_cfg);
		CPS_UncachedWrite32(&pD->regs->ep_cmd, DMARF_EP_DRDY);
		CPS_UncachedWrite32(&pD->regs->ep_sts, DMARF_EP_TRBERR);
		if (!(cfg & DMARV_EP_ENABLED)) {
			cfg |= DMARV_EP_ENABLED;
			CPS_UncachedWrite32(&pD->regs->ep_cfg, (uint32_t)cfg);
		}

	} else {
		struct DmaTrbChainDesc *startedChain;
		/*Firmware has to replace last TRB from last transfer to link to next transfer*/

		/**
		 * Check if we have at least two trbChainDesc in list. It should be always
		 * meet in this place
		 **/
		if (channel->trbChainDescList.prev != channel->trbChainDescList.next) {
			struct DmaTrb *lastPrevTrb;
			/*get one before last trbChainDesc  */
			startedChain =  sgdmaListGetTrbChainDescEntry(channel->trbChainDescList.prev->prev);
			/*Last TRB in chain*/
			lastPrevTrb = &startedChain->trbPool[startedChain->numOfTrbs];

			/*If previous transfer hasn't finished in the meantime then we link new TRB Chain, else we start
			 * transfer again.
			 */
			startedChain->lastTrbIsLink = 1;
			BUILD_LINK_TRB((*lastPrevTrb), trbChainDesc->trbDMAAddr);
			sgdmShowTrbChain(startedChain);
			epStsUpdate = CPS_UncachedRead32(&pD->regs->ep_cmd);
			if (!(epStsUpdate & DMARF_EP_DRDY)) {
				CPS_UncachedWrite32(&pD->regs->traddr, (uint32_t)trbChainDesc->trbDMAAddr);
				CPS_UncachedWrite32(&pD->regs->ep_cmd, DMARF_EP_DRDY);
				CPS_UncachedWrite32(&pD->regs->ep_sts, DMARF_EP_TRBERR);
				/* CPS_UncachedWrite32(&pD->regs->drbl, 1);  markmarkmark*/
			}
		}
	}
}

/**
 * Function free trbChainDesc and inform upper layer that transfer has been finished
 */
static uint32_t sgdmaUpdateDescBuffer(struct DmaController *pD, struct DmaTrbChainDesc *trbChainDesc, uint16_t status)
{
	uint32_t i;
	uint32_t temp;
	struct DmaChannel *channel = trbChainDesc->channel;

	if (channel->isIsoc) {
		if (trbChainDesc->lastTrbIsLink) {
			if (channel->trbChainDescList.prev == channel->trbChainDescList.next) {
				/*New trb chain descriptor isn't ready
				 * and only one TRB chain is linked,
				 * so firmware will use this one and not
				 * inform user application about finishing
				 */
				return 0;
			}
		}
	}

	for (i = 0; i < trbChainDesc->numOfTrbs; i++) {
		uint32_t size = le32ToCpu((uint32_t)trbChainDesc->trbPool[i].dmaSize) & TD_SIZE_MASK;
		/* get transfer descriptor from ring*/
		if (trbChainDesc->framesDesc) {
			if (trbChainDesc->isoError)
				trbChainDesc->framesDesc[i].length  = 0;
			else
				trbChainDesc->framesDesc[i].length = size;
		}

		trbChainDesc->actualLen +=  size;
		/** Firmware can clear all TRB from TRB Chain link except link TRB in DMULT mode
		 * and when trbChainDesc->lastTrbIsLink is set. When firmware does that, hardware
		 * after raising IOC interrupt can not make it download link TRB and in this case
		 * we get TRBERR interrupt instead continue transfer.
		 */
		/*trbChainDesc->trbPool[i].ctrl = 0;*/
		/*trbChainDesc->trbPool[i].dmaAddr = 0;*/
		/*trbChainDesc->trbPool[i].dmaSize = 0;*/
	}

	if (trbChainDesc->isoError)
		trbChainDesc->actualLen = 0;

	temp = CPS_UncachedRead32(&pD->regs->ep_sel);

	/*This value can by read in complete callback function*/
	channel->lastTransferLength = trbChainDesc->actualLen;

	VERBOSE("Complete callback channel-actualLen %p-%d\n", channel, trbChainDesc->actualLen);

	if (!trbChainDesc->lastTrbIsLink) {
		if (trbChainDesc->chainNode.next) {
			sgdmaFreeTrbChain(pD, trbChainDesc);
			trbChainDesc = NULL;
		}
	}
	if (channel->trbChainDescList.prev == &channel->trbChainDescList)
		channel->status = CUSBDMA_STATUS_FREE;

	if (pD->callback.complete)
		pD->callback.complete(pD->parent, channel->hwUsbEppNum, channel->isDirTx);
	/*In case DMULT mode and when last TRB is Link TRB and trbChainDesc->lastTrbIsLink is set firmware
	 * can free TRB Chain after complete function in order to avoid rewrite Link TRB in complete function
	 */
	if (trbChainDesc && trbChainDesc->lastTrbIsLink) {
		if (trbChainDesc->chainNode.next)
			sgdmaFreeTrbChain(pD, trbChainDesc);
	}
	CPS_UncachedWrite32(&pD->regs->ep_sel, temp);
	return 0;
}

/*****************************************************************************
 * Public Driver function
 *****************************************************************************/
static uint32_t sgdmaProbe(struct CUSBDMA_Config *config, struct CUSBDMA_SysReq *sysReq)
{
	if (!config || !sysReq)
		return -EINVAL;

	VERBOSE("Required private size %lu\n", sizeof(struct DmaController));

	sysReq->trbMemSize = TRB_POOL_SIZE;
	sysReq->privDataSize = sizeof(struct DmaController);
	return 0;
}

static uint32_t sgdmaInit(void *pD, const struct CUSBDMA_Config *config, struct CUSBDMA_Callbacks *callbacks)
{
	struct DmaController *ctrl = (struct DmaController *)pD;

	if (!pD || !config || !callbacks)
		return -EINVAL;

	if (!config->trbAddr || !config->trbDmaAddr)
		return -EINVAL;

	VERBOSE("Initialization SGDMA component %s\n", " ");
	memset((void *)ctrl, 0, sizeof(struct DmaController));
	memset((void *)config->trbAddr, 0, TRB_POOL_SIZE);

	ctrl->trbDMAPoolAddr = config->trbDmaAddr;
	ctrl->trbPoolAddr = config->trbAddr;

	ctrl->regs = (struct DmaRegs *)config->regBase;
	ctrl->cfg = *config;
	ctrl->drv = CUSBDMA_GetInstance();
	ctrl->callback = *callbacks;
	ctrl->isHostCtrlMode = 0;
	return 0;
}

static void sgdmaDestroy(void *pD)
{
	struct DmaController *ctrl = (struct DmaController *)pD;
	struct DmaChannel *channel;
	uint8_t i = 0;

	VERBOSE("Destroying SGDMA component %s\n", " ");
	if (!pD)
		return;

	for (i = 0; i < 8; i++) {
		channel = &ctrl->rx[i];
		if (channel->status == CUSBDMA_STATUS_UNKNOWN)
			ctrl->drv->channelRelease(pD, channel);

		channel = &ctrl->tx[i];
		if (channel->status == CUSBDMA_STATUS_UNKNOWN)
			ctrl->drv->channelRelease(pD, channel);
	}
}

static uint32_t sgdmaStart(void *pD)
{
	struct DmaController *ctrl = (struct DmaController *)pD;
	uint8_t i = 0;

	if (!pD)
		return -EINVAL;

	VERBOSE("Starting SGDMA component %s\n", " ");
	ctrl->dmaMode = DMA_MODE_CHANNEL_INDIVIDUAL;
	/*if all bits all set for dmaModeRx and dmaModeTx fields then
	 * firmware set globally DMULT mode
	 * elsewhere DMA mode will be set individually for each channel in
	 * sgdmaChannelAlloc function
	 */
	if ((ctrl->cfg.dmaModeRx & ctrl->cfg.dmaModeTx) == 0xFFFF) {
		CPS_UncachedWrite32(&ctrl->regs->conf, DMARF_DMULT);
		ctrl->dmaMode = DMA_MODE_GLOBAL_DMULT;

	} else if ((ctrl->cfg.dmaModeRx | ctrl->cfg.dmaModeTx) == 0x0000) {
		CPS_UncachedWrite32(&ctrl->regs->conf, DMARF_DSING);
		ctrl->dmaMode = DMA_MODE_GLOBAL_DSINGL;
	}

	for (i = 0; i < MAX_DMA_CHANNELS; i++) {
		if (ctrl->cfg.dmaModeRx & (1 << i)) {
			ctrl->rx[i].dmultEnabled = 1;
			ctrl->rx[i].maxTrbLen = TD_DMULT_MAX_TRB_DATA_SIZE;
			ctrl->rx[i].maxTdLen = TD_DMULT_MAX_TD_DATA_SIZE;
		} else {
			ctrl->rx[i].dmultEnabled = 0;
			ctrl->rx[i].maxTrbLen = TD_SING_MAX_TRB_DATA_SIZE;
			ctrl->rx[i].maxTdLen = TD_SING_MAX_TD_DATA_SIZE;
		}

		ctrl->rx[i].lastTransferLength = 0;

		if (ctrl->cfg.dmaModeTx & (1 << i)) {
			ctrl->tx[i].dmultEnabled = 1;
			ctrl->tx[i].maxTrbLen = TD_DMULT_MAX_TRB_DATA_SIZE;
			ctrl->tx[i].maxTdLen = TD_DMULT_MAX_TD_DATA_SIZE;
		} else {
			ctrl->tx[i].dmultEnabled = 0;
			ctrl->tx[i].maxTrbLen = TD_SING_MAX_TRB_DATA_SIZE;
			ctrl->tx[i].maxTdLen = TD_SING_MAX_TD_DATA_SIZE;
		}
		ctrl->tx[i].lastTransferLength = 0;
	}

	return 0;
}

static uint32_t sgdmaStop(void *pD)
{
	if (!pD)
		return -EINVAL;

	VERBOSE("Stopping SGDMA component %s\n", " ");
	return 0;
}

static void *sgdmaChannelAlloc(void *pD, uint8_t isDirTx, uint8_t hwEpNum, uint8_t isIsoc)
{
	struct DmaController *ctrl = NULL;
	struct DmaRegs *regs = NULL;
	struct DmaChannel *channel = NULL;
	uint32_t dma_ien;
	uint32_t cfg = 0;
	uint16_t dmaMode = 0;

	if (!pD || (hwEpNum >= MAX_DMA_CHANNELS))
		return NULL;

	ctrl = (struct DmaController *)pD;
	regs = ctrl->regs;

	dma_ien = CPS_UncachedRead32(&regs->ep_ien);
	if (isDirTx) {
		if (ctrl->tx[hwEpNum].status >= CUSBDMA_STATUS_FREE) {
			ERROR("Channel 0x%p dedicated for Ep %d is already reserved\n",
			      &ctrl->tx[hwEpNum], hwEpNum);
			return NULL;
		}
		channel = &ctrl->tx[hwEpNum];
		channel->isDirTx = 0x80;
		dma_ien |= (0x01 << (hwEpNum + 16));
		dmaMode = ctrl->cfg.dmaModeTx;
	} else {
		if (ctrl->rx[hwEpNum].status >= CUSBDMA_STATUS_FREE) {
			ERROR("Channel 0x%p dedicated for Ep %d is already reserved\n",
			      &ctrl->rx[hwEpNum], hwEpNum);
			return NULL;
		}
		channel = &ctrl->rx[hwEpNum];
		channel->isDirTx = 0x00;
		dma_ien |= (0x01 << hwEpNum);
		dmaMode = ctrl->cfg.dmaModeRx;
	}

	channel->isIsoc = 0;
	if (isIsoc)
		channel->isIsoc = 1;

	VERBOSE("Channel 0x%p has been allocated for ep %d\n", channel, hwEpNum);
	listInit(&channel->trbChainDescList);
	channel->numOfTrbChain = 0;
	channel->controller = ctrl;
	channel->dmultGuard = 0;
	channel->status = CUSBDMA_STATUS_FREE;
	channel->hwUsbEppNum = hwEpNum;

	CPS_UncachedWrite32(&regs->ep_sel, (uint32_t)hwEpNum | channel->isDirTx);
	cfg = CPS_UncachedRead32(&regs->ep_cfg);

	if (ctrl->dmaMode == DMA_MODE_CHANNEL_INDIVIDUAL) {
		if (dmaMode & (1 << hwEpNum))
			cfg |= DMARV_EP_DMULT;
		else
			cfg |= DMARV_EP_DSING;
	}

	CPS_UncachedWrite32(&regs->ep_sts,
			    DMARF_EP_IOC |
			    DMARF_EP_ISP |
			    DMARF_EP_TRBERR
			    /*| DMARF_EP_ISOERR*/
			    );
	CPS_UncachedWrite32(&regs->ep_cfg, (uint32_t)DMARV_EP_ENABLED | cfg);
	CPS_UncachedWrite32(&regs->ep_ien, dma_ien);
	CPS_UncachedWrite32(&regs->ep_sts_en,
			    DMARF_EP_TRBERR
			    /*| DMARF_EP_ISOERR| DMARF_EP_DESCMIS | DMARF_EP_OUTSMM*/
			    );

	return channel;
}

/*Release channel from USB endpoint */
static uint32_t sgdmaChannelRelease(void *pD, void *channel)
{
	struct DmaController *ctrl = (struct DmaController *)pD;
	struct DmaChannel *dmaChannel = (struct DmaChannel *)channel;
	uint32_t dmaIen;

	VERBOSE("Dma channel release ch: %p\n", channel);
	if (!channel || !pD)
		return -EINVAL;

	/*Disable endpoint and interrupt for DMA. Probably it should be in dmaChannelAbort*/
	dmaIen = CPS_UncachedRead32(&ctrl->regs->ep_ien);
	if (dmaChannel->isDirTx)
		dmaIen &= ~(0x01 << (dmaChannel->hwUsbEppNum + 16));
	else
		dmaIen &= ~(0x01 << dmaChannel->hwUsbEppNum);

	CPS_UncachedWrite32(&ctrl->regs->ep_sel,
			    (uint32_t)dmaChannel->hwUsbEppNum | dmaChannel->isDirTx);
	CPS_UncachedWrite32(&ctrl->regs->ep_cfg, (uint32_t)0);

	CPS_UncachedWrite32(&ctrl->regs->ep_ien, dmaIen);
	CPS_UncachedWrite32(&ctrl->regs->ep_sts_en, 0x0);
	CPS_UncachedWrite32(&ctrl->regs->ep_cmd, DMARF_EP_EPRST);

	CPS_UncachedWrite32(&ctrl->regs->ep_sts,
			    DMARF_EP_IOC | DMARF_EP_ISP | DMARF_EP_TRBERR | DMARF_EP_ISOERR);

	if (dmaChannel->status >= CUSBDMA_STATUS_BUSY) {
		dmaChannel->status = CUSBDMA_STATUS_ABORT;
		ctrl->drv->channelAbort(ctrl, channel);
	} else {
		ctrl->drv->channelAbort(pD, channel);
	}
	while (dmaChannel->trbChainDescList.next != &dmaChannel->trbChainDescList)
		sgdmaFreeTrbChain(ctrl, sgdmaListGetTrbChainDescEntry(dmaChannel->trbChainDescList.next));

	VERBOSE("Channel 0x%p has been released\n", channel);

	dmaChannel->status = CUSBDMA_STATUS_UNKNOWN;
	return 0;
}

static uint32_t sgdmaChannelProgram(void *pD,
				    void *channel,
				    uint16_t packetSize,
				    uintptr_t dmaAddr,
				    uint32_t len,
				    void *framesDesc,
				    uint32_t framesNumber)
{
	struct DmaChannel *dmaChannel;
	struct DmaTrbChainDesc *trbChainDesc;
	struct DmaController *ctrl = (struct DmaController *)pD;
	uint8_t retval;
	uint32_t numOfTrbs;

	if (!pD || !channel)
		return -EINVAL;

	dmaChannel = (struct DmaChannel *)channel;

	VERBOSE("DMA prepare for channel: 0x%p ep%d-%s packetSize %d, dmaAddr 0x%lx length %d\n",
		dmaChannel, dmaChannel->hwUsbEppNum, dmaChannel->isDirTx ? "Tx" : "Rx",
		packetSize, dmaAddr, len);

	if (framesDesc && ctrl->isHostCtrlMode && dmaChannel->isIsoc)
		numOfTrbs = framesNumber;
	else
		numOfTrbs = divRoundUp(len, dmaChannel->maxTrbLen);

	retval = sgdmaAllocTrbChain(pD, channel, numOfTrbs, &trbChainDesc);
	if (retval)
		return retval;

	trbChainDesc->dwStartAddress = dmaAddr;
	trbChainDesc->len = len;
	trbChainDesc->framesDesc = framesDesc;

	dmaChannel->wMaxPacketSize = packetSize;

	sgdmaNewTdHw(pD, trbChainDesc);
	dmaChannel->lastTransferLength = 0;
	sgdmaArmTd(pD, trbChainDesc);

	dmaChannel->status = CUSBDMA_STATUS_BUSY;

	return 0;
}

/**
 * Function aborts DMA transmition
 */
static uint32_t sgdmaChannelAbort(void *pD, void *channel)
{
	struct DmaController *ctrl = (struct DmaController *)pD;
	struct DmaChannel *dmaChannel = (struct DmaChannel *)channel;
	uint32_t cfg = 0;

	if (!pD || !channel)
		return -EINVAL;

	VERBOSE("Channel 0x%p has been aborted\n", channel);

	/*Disable DMA*/
	CPS_UncachedWrite32(&ctrl->regs->ep_sel,
			    (uint32_t)(dmaChannel->isDirTx | dmaChannel->hwUsbEppNum));
	cfg = CPS_UncachedRead32(&ctrl->regs->ep_cfg);
	cfg &= ~DMARV_EP_ENABLED;
	CPS_UncachedWrite32(&ctrl->regs->ep_cfg, (uint32_t)cfg);
	/*Clear all interrupts for this channel*/
	CPS_UncachedWrite32(&ctrl->regs->ep_sts, 0xFFFFFFFF);

	while (dmaChannel->trbChainDescList.next != &dmaChannel->trbChainDescList)
		sgdmaFreeTrbChain(ctrl, sgdmaListGetTrbChainDescEntry(dmaChannel->trbChainDescList.next));

	if (dmaChannel->status != CUSBDMA_STATUS_UNKNOWN)
		dmaChannel->status = CUSBDMA_STATUS_FREE;
	return 0;
}

/**
 * Function is called by USB Host controller when error interrupt occurs
 */
static void sgdmaErrIsr(void *pD, uint8_t irqNr, uint8_t isDirTx)
{
	struct DmaController *ctrl = (struct DmaController *)pD;
	struct DmaChannel *channel = NULL;

	if (!pD)
		return;

	if (isDirTx) /*transmit data*/
		channel = &ctrl->tx[irqNr];
	else
		channel = &ctrl->rx[irqNr];

	if (channel->status >= CUSBDMA_STATUS_BUSY)
		channel->status = CUSBDMA_STATUS_ABORT;

	if (ctrl->callback.complete)
		ctrl->callback.complete(ctrl->parent,
					channel->hwUsbEppNum,
					channel->isDirTx);
}

/*
 * this function should be called when system indicates interrupt for DMA.
 */
static void sgdmaIsr(void *privateData)
{
	struct DmaController *ctrl = NULL;
	struct DmaRegs *regs = NULL;
	uint32_t epSts, epIsts;
	struct DmaChannel *channel = NULL;
	struct DmaTrbChainDesc *trbChainDesc = NULL;
	/*  UdcEp *udcEp = NULL;*/
	int i;

	if (!privateData)
		return;

	ctrl = (struct DmaController *)privateData;
	regs = ctrl->regs;

	/* Check interrupts for all endpoints*/
	epIsts = CPS_UncachedRead32(&regs->ep_ists);

	if (epIsts == 0)
		return;

	VERBOSE("DMAIRQ ep_ists SFR: %x\n", epIsts);
	/* check all endpoints interrupts*/
	for (i = 0; i < 32; i++) {
		uint8_t isDirTx;
		uint8_t epNum;

		if (!(epIsts & (1 << i)))
			continue;

		isDirTx = i > 15 ? DMARD_EP_TX : 0u;
		epNum = isDirTx ? i - 16 : i;

		CPS_UncachedWrite32(&regs->ep_sel, epNum | isDirTx);

		epSts = CPS_UncachedRead32(&regs->ep_sts);

		if (isDirTx) /*transmit data*/
			channel = &ctrl->tx[epNum];
		else
			channel = &ctrl->rx[epNum];

		VERBOSE("DMA IRQ epSts %x epNum: %d epDir: %s\n",
			epSts, epNum, (isDirTx ? "TX" : "RX"));

		if (epSts & DMARF_EP_TRBERR) {
			ERROR("DMAIRQ: TRBERR %s\n", "");
			CPS_UncachedWrite32(&regs->ep_sts, DMARF_EP_TRBERR);
		}

		if ((epSts & DMARF_EP_IOC) ||
		    (epSts & DMARF_EP_ISP) ||
		    (channel->dmultGuard & DMARF_EP_IOC) ||
		    (channel->dmultGuard & DMARF_EP_ISP)) {
retransmit:
			trbChainDesc = sgdmaListGetTrbChainDescEntry(channel->trbChainDescList.next);
			CPS_UncachedWrite32(&regs->ep_sts, DMARF_EP_IOC | DMARF_EP_ISP);

			/*Only For DMULT Transfer*/
			if (!(epSts & DMARF_EP_TRBERR) && channel->dmultEnabled && !trbChainDesc->lastTrbIsLink) {
				VERBOSE("DMAIRQ: IOC & ISP wait for TRBERROR %s\n", " ");

				/*For EP OUT in Device mode firmware can't wait for TRB ERROR*/
				if (!ctrl->isHostCtrlMode && !channel->isDirTx) {
					uint32_t cfg;

					channel->dmultGuard = 0;
					CPS_UncachedWrite32(&regs->ep_sts, DMARF_EP_TRBERR);
					/*disable DMA for this endpoint to avoid interrupt from TRB ERROR*/

					cfg = CPS_UncachedRead32(&regs->ep_cfg);
					cfg &= ~DMARV_EP_ENABLED;
					CPS_UncachedWrite32(&regs->ep_cfg, (uint32_t)cfg);
					sgdmaUpdateDescBuffer(privateData, trbChainDesc, 0);

					break;
				}

				channel->dmultGuard = epSts;
				break;
			}
			channel->dmultGuard = 0;
			sgdmaUpdateDescBuffer(privateData, trbChainDesc, 0);
		}

		/* stop program if critical error: Data overflow*/
		/*assert(!(ep_sts & USBRF_EP_OUTSMM));*/
		if (epSts & DMARF_EP_OUTSMM) {
			CPS_UncachedWrite32(&regs->ep_sts, DMARF_EP_OUTSMM);
			VERBOSE("DMAIRQ: OUTSMM%s\n", "");
		}

		if (epSts & DMARF_EP_DESCMIS) {
			CPS_UncachedWrite32(&regs->ep_sts, DMARF_EP_DESCMIS);
			VERBOSE("DMAIRQ: DESCMIS%s\n", "");
		}

		if (epSts & DMARF_EP_ISOERR) {
			CPS_UncachedWrite32(&regs->ep_sts, DMARF_EP_ISOERR);
			trbChainDesc = sgdmaListGetTrbChainDescEntry(channel->trbChainDescList.next);
			trbChainDesc->isoError = 1;
			VERBOSE("DMAIRQ: ISOERR%s\n", "");
			goto retransmit;
		}
	}
}

static enum CUSBDMA_Status sgdmaGetChannelStatus(void *pD, void *channel)
{
	struct DmaChannel *ch = (struct DmaChannel *)channel;
	struct DmaController *ctrl = (struct DmaController *)pD;

	if (!pD || !channel)
		return -EINVAL;

	/* If DMA has been armed and has not finished yet then
	 * update status to *_ARMED. This procedure allows to detect
	 * that DMA HW is in progress.
	 */
	if (ch->status >= CUSBDMA_STATUS_BUSY) {
		uint32_t dorbel, status;

		CPS_UncachedWrite32(&ctrl->regs->ep_sel,
				    (uint32_t)(ch->isDirTx | ch->hwUsbEppNum));

		dorbel = CPS_UncachedRead32(&ctrl->regs->ep_cmd);
		status = CPS_UncachedRead32(&ctrl->regs->ep_sts);

		if ((dorbel & DMARF_EP_DRDY) || (status & DMARF_EP_DBUSY))
			ch->status = CUSBDMA_STATUS_ARMED;
		else
			ch->status = CUSBDMA_STATUS_BUSY;
	}

	return ch->status;
}

static uint32_t sgdmaGetActualLength(void *pD, void *channel)
{
	struct DmaChannel *ch = (struct DmaChannel *)channel;

	if (!pD || !ch)
		return -EINVAL;

	return ch->lastTransferLength;
}

static uint32_t sgdmaSetMaxLength(void *pD, void *channel, uint32_t size)
{
	struct DmaChannel *ch = (struct DmaChannel *)channel;

	if (!pD || !ch)
		return -EINVAL;

	if (ch->dmultEnabled)
		ch->maxTrbLen = size;
	else
		ch->maxTrbLen  = (size > TD_SING_MAX_TRB_DATA_SIZE) ? TD_SING_MAX_TRB_DATA_SIZE : size;
	return 0;
}

static uint32_t sgdmaGetMaxLength(void *pD, void *channel)
{
	struct DmaChannel *ch = (struct DmaChannel *)channel;

	if (!pD || !ch)
		return -EINVAL;

	return ch->maxTdLen;
}

static void sgdmaSetParentPriv(void *pD, void *parent)
{
	struct DmaController *ctrl = (struct DmaController *)pD;

	if (!pD)
		return;

	ctrl->parent = parent;
}

static void sgdmaDmaReset(void *pD)
{
	struct DmaController *ctrl = (struct DmaController *)pD;
	uint32_t reg = 0;

	if (!pD)
		return;

	ctrl->isHostCtrlMode = 0;

	reg = CPS_UncachedRead32(&ctrl->regs->conf);
	reg |= DMARF_RESET;
	CPS_UncachedWrite32(&ctrl->regs->conf, reg);
}

static void sgdmaSetHostMode(void *pD)
{
	struct DmaController *ctrl = (struct DmaController *)pD;

	if (!pD)
		return;

	ctrl->isHostCtrlMode = 1;
}

struct CUSBDMA_OBJ sgdmaDrv = {
	.probe = sgdmaProbe,
	.init = sgdmaInit,
	.destroy = sgdmaDestroy,
	.isr = sgdmaIsr,
	.start = sgdmaStart,
	.stop = sgdmaStop,
	.errIsr = sgdmaErrIsr,
	.channelAlloc = sgdmaChannelAlloc,
	.channelRelease = sgdmaChannelRelease,
	.channelProgram = sgdmaChannelProgram,
	.channelAbort = sgdmaChannelAbort,
	.getChannelStatus = sgdmaGetChannelStatus,
	.getActualLength = sgdmaGetActualLength,
	.getMaxLength = sgdmaGetMaxLength,
	.setMaxLength = sgdmaSetMaxLength,
	.setParentPriv = sgdmaSetParentPriv,
	.controllerReset = sgdmaDmaReset,
	.setHostMode = sgdmaSetHostMode,
};

extern struct CUSBDMA_OBJ *CUSBDMA_GetInstance(void)
{
	return &sgdmaDrv;
}
