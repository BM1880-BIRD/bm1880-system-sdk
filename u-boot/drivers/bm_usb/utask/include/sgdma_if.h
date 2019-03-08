/**********************************************************************
 * Copyright (C) 2014-2016 Cadence Design Systems, Inc.
 * All rights reserved worldwide.
 ***********************************************************************
 * sgdma_if.h
 * Header file for DMA Scatter/Gather Driver for USB Controllers
 *
 ***********************************************************************/
#ifndef _SGDMA_INT_H
#define _SGDMA_INT_H

#include "cusb_dma_if.h"

#ifndef ListHead
#define ListHead struct CUSBDMA_ListHead
#endif

#include "sduc_list.h"

/** This value should be module 8*/
#define NUM_OF_TRB 16

/**
 * Number of TRBs that will be allocated for DMA component. This trb will
 * be dynamically managed by DMA driver
 */
#define TRB_POOL_SIZE (sizeof(struct DmaTrb) * (NUM_OF_TRB))

/**
 * Number of channel supported by DMA driver
 */
#define MAX_DMA_CHANNELS 8

/*
 * Number of byte need for holding information about TRBs allocated from TRB pool
 **/
#define TRB_MAP_SIZE ((NUM_OF_TRB + (sizeof(uint8_t) * 8) - 1) / (sizeof(uint8_t) * 8))

/**
 * Number of element for holding information about TRB chain
 * We assume that for every channel for IN and OUT direction we can
 * allocate average one TRBS chain.
 */
#define TAB_SIZE_OF_DMA_CHAIN MAX_DMA_CHANNELS * 2

/*Describe One trb*/
struct DmaTrb {
	uint32_t dmaAddr;
	uint32_t dmaSize;
	uint32_t ctrl;
};

struct DmaController;
struct DmaChannel;
struct DmaTrbChainDesc;

struct DmaChannel {
	struct DmaController *controller;
	uint16_t wMaxPacketSize;
	uint8_t hwUsbEppNum; /*USB EP number. It can be different then channel number */
	uint8_t isDirTx;
	/*Maximum size for Transfer Descriptor*/
	uint32_t maxTdLen;
	/*Maximum size for single Transfer Request Block */
	uint32_t maxTrbLen;
	enum CUSBDMA_Status status;

	/*TRB management*/
	void *priv; /*for private use*/
	/*For DMULT mode firmware must wait for TRBERR interrupt. After TRBERR firmware can finish transfer (ADMA stop)
	 * this filed hold rasising interrupt before TRBERR (ISP and IOC)
	 */
	uint32_t dmultGuard;
	uint8_t dmultEnabled; /*field holds dmult configuration for channel*/
	uint8_t numOfTrbChain;
	ListHead trbChainDescList;
	/** Field hold information about number of bytes received or sent in last
	 * transfer
	 */
	uint32_t lastTransferLength;
	uint8_t isIsoc;
};

/*Describe packet for one isochronous interval*/
struct DmaTrbFrameDesc {
	/**
	 * For IN (RX) isochronous transfer this field holds information about received length of packet,
	 * and for OUT (TX) direction this field inicate length of submiting frame
	 */
	uint32_t length;
	/**
	 * For IN (RX) direction this field indicates initial address in buffer where received packet should
	 * be placed and for OUT (TX) direction this field indicates starting address in buffer for sending
	 * packet
	 */
	uint32_t offset;
};

struct DmaTrbChainDesc {
	uint8_t reserved;
	struct DmaChannel *channel;
	struct DmaTrb *trbPool;
	uint32_t trbDMAAddr;
	uint32_t len;
	uint32_t dwStartAddress;
	uint32_t actualLen;
	uint8_t isoError;
	/** Finish transfer and inform USB driver when IOC event has been detected.
	 *  This filed is used for DMULT mode. By default in DMULT mode DMA finish
	 *  transfer on TRBERR event, but when firmware link many TRB Chain Descriptors
	 *  for one channel then last bad TRB descriptor from Chain Descriptor will be
	 *  removed, and transfer should be finished on IOC event
	 */
	uint8_t lastTrbIsLink;
	uint32_t mapSize; /*Number of elements divided by 8 reserved in TRBs map*/
	uint32_t numOfTrbs; /*Number of TRB in chain without LINK and bad TRB*/
	uint32_t start; /* index of oldest element              */
	uint32_t end; /* index at which to write new element  */
	struct DmaTrbFrameDesc *framesDesc; /*used only for ISOC transfer in Host mode*/
	ListHead chainNode;
};

enum DmaMode {
	DMA_MODE_GLOBAL_DMULT,
	DMA_MODE_GLOBAL_DSINGL,
	DMA_MODE_CHANNEL_INDIVIDUAL,
};

struct DmaController {
	struct DmaRegs *regs;
	struct CUSBDMA_OBJ *drv;
	struct CUSBDMA_Config cfg;
	struct CUSBDMA_Callbacks callback;
	struct DmaChannel rx[MAX_DMA_CHANNELS];
	struct DmaChannel tx[MAX_DMA_CHANNELS];
	/*DMA mode for SGDMA*/
	enum DmaMode dmaMode;
	uint8_t isHostCtrlMode;
	void *parent;  /*parent private data*/
	void *trbPoolAddr;
	uintptr_t trbDMAPoolAddr;
	uint8_t trbChainFreeMap[TRB_MAP_SIZE];
	struct DmaTrbChainDesc trbChainDesc[TAB_SIZE_OF_DMA_CHAIN];
};

#endif

