/**********************************************************************
 *
 * usb_tty.c
 * ACM class application.
 *
 ***********************************************************************/
#include <stdlib.h>
#include <malloc.h>
#include <memalign.h>
#include <crc.h>
#include "include/bm_private.h"
#include "include/debug.h"
#include "include/bm_usb.h"
#include <asm/arch-armv8/mmio.h>
#include "include/platform_def.h"
#include "include/cps.h"
#include "include/usb_tty.h"
#include "include/ss_dev_hw.h"
#include "include/system_common.h"

#ifdef BUILD_ATF
extern uint16_t bm_usb_vid;
#else
uint16_t bm_usb_vid = 0x30B1;
#endif

static void bulkOutCmplMain(struct CUSBD_Ep *ep, struct CUSBD_Req *req);
static void sramOutReqS2D(uint64_t addr, uint32_t size);

#define GLOBAL_MEM_START_ADDR 0x100000000

/* Super Speed USB object configuration */
static struct CUSBD_Config config = {
	.regBase = USB_DEV_BASE, /* address where USB core is mapped */
	.epIN = {
		{.bufferingValue = 8} /* ep1in */
	},
	.epOUT = {
		{.bufferingValue = 8} /* ep1out */
	},
	.dmultEnabled = 0, /* set to 1 if scatter/gather DMA available */
	.dmaInterfaceWidth = CUSBD_DMA_64_WIDTH,
	.preciseBurstLength = CUSBD_PRECISE_BURST_0
};

/* variable declare */
static struct CUSBD_OBJ *drv; /* driver pointer */
static void *pD; /* private data pointer */
static uint8_t *bulkBuf, *cmdBuf, *ep0Buff;
static struct   CUSBD_Ep *epIn, *epOut;
static struct   CUSBD_Req *bulkInReq, *bulkOutReq, *ep0Req; /* request used by driver */
static uint8_t  configValue;
static uint8_t  configBreak;
static uint8_t  acm_configValue;
static uint8_t  current_speed = CH9_USB_SPEED_UNKNOWN;
static uint8_t  mem_alloc_cnt;
static uint32_t transfer_size;
static uint8_t  flagEnterDL;
struct f_acm *acm;

struct CUSBD_Dev *dev; /* auxiliary pointer used for testing purposes */

static uint8_t *pd_buf       = (uint8_t *)PD_ADDR;      /* 4096 */
static uint8_t *bulkBufAlloc = (uint8_t *)BLK_BUF_ADDR; /* 1024 */
static uint8_t *cmdBufAlloc  = (uint8_t *)CMD_BUF_ADDR; /* 1024 */
static uint8_t *trb_buf      = (uint8_t *)TRB_BUF_ADDR; /* 256  */
static uint8_t *cb0_buf      = (uint8_t *)CB0_BUF_ADDR; /* 128  */
static uint8_t *cb1_buf      = (uint8_t *)CB1_BUF_ADDR; /* 128  */
static uint8_t *cb2_buf      = (uint8_t *)CB2_BUF_ADDR; /* 128  */
static uint8_t *ep0BuffAlloc = (uint8_t *)EP0_BUF_ADDR; /* 512  */
static uint8_t *rsp_buf      = (uint8_t *)RSP_BUF_ADDR; /* 8    */
static uint8_t *acm_buf      = (uint8_t *)ACM_BUF_ADDR; /* 128  */

/* string will be filled then in initializing section */
static char vendorDesc[sizeof(USB_MANUFACTURER_STRING) * 2 + 2];
static char productDesc[sizeof(USB_PRODUCT_STRING) * 2 + 2];
static char serialDesc[sizeof(USB_SERIAL_NUMBER_STRING) * 2 + 2];

typedef void func(void);

/* Driver private data */
struct CUSBD_PrivateData_TTY {
	/* this "class must implement" CUSBD class, this class is an extension of CUSBD */
	struct CUSBD_Dev device;

	struct CUSBD_Config config; /* copy of user configuration */

	struct CUSBD_Callbacks callbacks; /* copy of user callback */
	struct CH9_UsbSetup *setupPacket;
	uintptr_t setupPacketDma;

	/* these variables reflect device state */
	uint16_t status_value; /* keeps status value */
	uint8_t u2_value; /* keeps U2 value */
	uint8_t u1_value; /* keeps U1 value */
	uint16_t suspend; /* keeps suspend value */
	uint16_t isoch_delay;

	struct ssReg *reg;
};

struct usb_msg_header {
	uint8_t token;
	uint8_t len_hi;
	uint8_t len_low;
	uint8_t addr4;
	uint8_t addr3;
	uint8_t addr2;
	uint8_t addr1;
	uint8_t addr0;
} __packed;

struct usb_msg {
	struct usb_msg_header header;
} __packed;

struct usb_msg_s2d {
	struct usb_msg_header header;
	size_t size;
} __packed;

struct usb_msg_d2s {
	struct usb_msg_header header;
	size_t size;
} __packed;

struct usb_rsp {
	uint8_t no_use0;
	uint8_t no_use1;
	uint8_t crc16_hi;
	uint8_t crc16_low;
	uint8_t no_use3;
	uint8_t no_use4;
	uint8_t token;
	uint8_t ack_index;
	uint8_t reserved[RSP_SIZE - 8];
} __packed;

struct sram_info {
	uint64_t sram_dest;
	uint32_t total_len;
	uint8_t reserved[4];
} packed;

static struct sram_info sram_info;
void print_buf_addr(void)
{
	INFO("pd_buf: %p\n", pd_buf);
	INFO("bulkBufAlloc: %p\n", bulkBufAlloc);
	INFO("cmdBufAlloc: %p\n", cmdBufAlloc);
	INFO("trb_buf: %p\n", trb_buf);
	INFO("cb0_buf: %p\n", cb0_buf);
	INFO("cb1_buf: %p\n", cb1_buf);
	INFO("cb2_buf: %p\n", cb2_buf);
	INFO("ep0BuffAlloc: %p\n", ep0BuffAlloc);
	INFO("rsp_buf: %p\n", rsp_buf);
	INFO("acm_buf: %p\n", acm_buf);
}

void __attribute__((optimize("O0")))set_config_break(uint8_t value)
{
	configBreak = value;
}

uint8_t __attribute__((optimize("O0")))get_config_break(void)
{
	return configBreak;
}

void __attribute__((optimize("O0")))set_acm_config(uint8_t value)
{
	acm_configValue = value;
}

uint8_t __attribute__((optimize("O0")))get_acm_config(void)
{
	return acm_configValue;
}

static int acm_mem_init(void)
{
	pd_buf = memalign(4096, PD_SIZE);
	memset(pd_buf, 0, PD_SIZE);
	bulkBufAlloc = memalign(32, BUF_SIZE);
	memset(bulkBufAlloc, 0, BUF_SIZE);
	cmdBufAlloc = memalign(32, BUF_SIZE);
	memset(cmdBufAlloc, 0, BUF_SIZE);
	trb_buf = memalign(32, TRB_SIZE);
	memset(trb_buf, 0, TRB_SIZE);
	cb0_buf = malloc(CB_SIZE);
	memset(cb0_buf, 0, CB_SIZE);
	cb1_buf = malloc(CB_SIZE);
	memset(cb1_buf, 0, CB_SIZE);
	cb2_buf = malloc(CB_SIZE);
	memset(cb2_buf, 0, CB_SIZE);
	ep0BuffAlloc = malloc(EP0_SIZE);
	memset(ep0BuffAlloc, 0, EP0_SIZE);
	rsp_buf = malloc(RSP_SIZE);
	memset(rsp_buf, 0, RSP_SIZE);
	acm_buf = malloc(128);
	memset(acm_buf, 0, 128);
	set_config_break(0);
	set_acm_config(0);
	current_speed = CH9_USB_SPEED_UNKNOWN;
	mem_alloc_cnt = 0;
	transfer_size = 0;
	flagEnterDL = 0;
	return 0;
}

static void acm_mem_release(void)
{
	free(pd_buf);
	free(bulkBufAlloc);
	free(cmdBufAlloc);
	free(trb_buf);
	free(cb0_buf);
	free(cb1_buf);
	free(cb2_buf);
	free(ep0BuffAlloc);
	free(rsp_buf);
	free(acm_buf);
}

static struct CUSBD_PrivateData_TTY *get_DevPrivateObj(void *pD)
{
	return (struct CUSBD_PrivateData_TTY *)pD;
}

/* interrupt handler */
void AcmIsr(void)
{
	drv->isr(pD);
}

static int getDescAcm(enum CH9_UsbSpeed speed, uint8_t *acmDesc)
{
	int i = 0;
	void *desc;
	int sum = 0;
	void*(*tab)[];

	switch (speed) {
	case CH9_USB_SPEED_FULL:
		tab = &descriptorsFs;
		break;
	case CH9_USB_SPEED_HIGH:
		tab = &descriptorsHs;
		break;
	case CH9_USB_SPEED_SUPER:
		tab = &descriptorsSs;
		break;
	default:
		return -1;
	}

	desc = (*tab)[i];

	while (desc) {
		int length = *(uint8_t *)desc;

		VERBOSE("acm get length %d\n", length);
		memcpy(&acmDesc[sum], desc, length);
		sum += length;
		desc = (*tab)[++i];
	}
	/* VERBOSE("acm get sum:%d\n", sum); */
	return sum;
}

static void clearReq(struct CUSBD_Req *req)
{
	memset(req, 0, sizeof(struct CUSBD_Req));
}

static void connect(void *pD)
{
	/* struct CUSBD_Dev *dev; */

	INFO("Application: connect\n");
}

static void disconnect(void *pD)
{
	set_acm_config(0);
	mem_alloc_cnt = 1;
	configValue = 0;
	NOTICE("Application: disconnect\n");
}

static void resume(void *pD)
{
	VERBOSE("Application: resume\n");
}

static void reqComplete(struct CUSBD_Ep *ep, struct CUSBD_Req *req)
{
	struct CUSBD_PrivateData_TTY *dev;

	dev = get_DevPrivateObj(pD);

	CPS_UncachedWrite32(&dev->reg->USBR_EP_STS, USBRF_EP_DESCMIS);

	VERBOSE("Request on endpoint completed\n");
	/* displayRequestinfo(req); */
}

static void getStatusComplete(struct CUSBD_Ep *ep, struct CUSBD_Req *req)
{
	struct CUSBD_PrivateData_TTY *dev;

	dev = get_DevPrivateObj(pD);

	CPS_UncachedWrite32(&dev->reg->USBR_EP_STS, USBRF_EP_DESCMIS);

	NOTICE("getStatusComplete on endpoint completed\n");
}

static void suspend(void *pD)
{
	VERBOSE("Application: suspend %c\n", ' ');
}

static void *requestMemAlloc(void *pD, uint32_t requireSize)
{
	void *ptr;
	/* VERBOSE("requestMemAlloc: size %d\n", requireSize); */
	if (mem_alloc_cnt == 0)
		ptr = cb0_buf;
	else if (mem_alloc_cnt == 1)
		ptr = cb1_buf;
	else
		ptr = cb2_buf;
	VERBOSE("requestMemAlloc: ptr %p, size %d, mem_alloc_cnt %d\n", ptr, requireSize, mem_alloc_cnt);
	mem_alloc_cnt++;
	return ptr;
}

static void requestMemFree(void *pD, void *usbRequest)
{
}

static void resetOutReq(void)
{
	VERBOSE("epOut->ops->reqQueue\n");
	bulkOutReq->length = transfer_size;
	bulkOutReq->buf = cmdBuf;
	bulkOutReq->dma = (uintptr_t)cmdBuf;
	bulkOutReq->complete = bulkOutCmplMain;
#ifndef DISABLE_DCACHE
	flush_cache(bulkOutReq->dma, bulkOutReq->length);
#endif
	epOut->ops->reqQueue(pD, epOut, bulkOutReq);
}

static void bulkResetOutReq(struct CUSBD_Ep *ep, struct CUSBD_Req *req)
{
	/* INFO("bulkReset%sReq complete\n", (ep->address == BULK_EP_IN)?"In":"Out"); */
	resetOutReq();
}

static void sendInReq(uint32_t length, uint8_t token, CUSBD_reqComplete complete, uint8_t *pRsp, uint8_t rspLen)
{
	uint16_t crc;
	static uint8_t ack_idx;
	struct usb_rsp *rsp = (struct usb_rsp *)rsp_buf;

	memset(rsp_buf, 0, RSP_SIZE);
	if (pRsp && rspLen > 0 && rspLen < 8)
		memcpy(rsp_buf + 8, pRsp, rspLen);
	crc = crc16_ccitt(0, cmdBuf, length);
	VERBOSE("CRC: %x\n", crc);
	rsp->crc16_hi = (crc >> 8) & 0xFF;
	rsp->crc16_low = crc & 0xFF;
	rsp->ack_index = ack_idx;
	rsp->token = token;
	ack_idx++;
	clearReq(bulkInReq);
	bulkInReq->length = RSP_SIZE;
	bulkInReq->buf = rsp_buf;
	bulkInReq->dma = (uintptr_t)rsp_buf;
	bulkInReq->complete = complete;
	VERBOSE("epIn->ops->reqQueue\n");
#ifndef DISABLE_DCACHE
	flush_cache(bulkInReq->dma, bulkInReq->length);
#endif
	epIn->ops->reqQueue(pD, epIn, bulkInReq);
}

static void bulkCmplEmpty(struct CUSBD_Ep *ep, struct CUSBD_Req *req)
{
	VERBOSE("bulkCmplEmpty\n");
}

static void resetOutReqS2D(uint64_t addr, size_t size, CUSBD_reqComplete complete)
{
	/* INFO("epOut->ops->reqQueue S2D, addr:0x%lx, size:0x%lx\n", addr, size); */
	bulkOutReq->length = size;
	bulkOutReq->buf = (uint8_t *)addr;
	bulkOutReq->dma = (uintptr_t)addr;
	bulkOutReq->complete = complete;
#ifndef DISABLE_DCACHE
	flush_cache(bulkOutReq->dma, bulkOutReq->length);
#endif
	epOut->ops->reqQueue(pD, epOut, bulkOutReq);
}

static void sramCompl(struct CUSBD_Ep *ep, struct CUSBD_Req *req)
{
	uint32_t left = sram_info.total_len -= req->length;
	uint64_t target = sram_info.sram_dest + req->length;
	/* INFO("sram copy data to 0x%lx, len = 0x%x\n", sram_info.sram_dest, req->length); */
	memcpy((void *)sram_info.sram_dest, (void *)req->buf, req->length);

	if (left == 0U)
		resetOutReq();
	else
		sramOutReqS2D(target, left);
}

static void sramOutReqS2D(uint64_t addr, uint32_t size)
{
	sram_info.total_len = size;
	sram_info.sram_dest = addr;

	bulkOutReq->length = (sram_info.total_len > BUF_SIZE) ? BUF_SIZE : sram_info.total_len;
	bulkOutReq->buf = bulkBuf;
	bulkOutReq->dma = (uintptr_t)bulkBuf;
	bulkOutReq->complete = sramCompl;
#ifndef DISABLE_DCACHE
	flush_cache(bulkOutReq->dma, bulkOutReq->length);
#endif
	epOut->ops->reqQueue(pD, epOut, bulkOutReq);
}

static void sendInReqD2S(uint64_t addr, size_t size, CUSBD_reqComplete complete)
{
	/* INFO("epIn->ops->reqQueue D2S\n"); */
	clearReq(bulkInReq);
	bulkInReq->length = size;
	bulkInReq->buf = (uint8_t *)addr;
	bulkInReq->dma = (uintptr_t)addr;
	bulkInReq->complete = complete;
#ifndef DISABLE_DCACHE
	flush_cache(bulkInReq->dma, bulkInReq->length);
#endif
	epIn->ops->reqQueue(pD, epIn, bulkInReq);
}

static void bulkOutCmplMain(struct CUSBD_Ep *ep, struct CUSBD_Req *req)
{
	uint64_t dest_addr = 0x0;
	uint32_t i = 0;
	uint16_t crc = 0;
	struct usb_msg *msg = (struct usb_msg *)req->buf;
	struct usb_msg_s2d *msg_s2d = (struct usb_msg_s2d *)req->buf;
	struct usb_msg_d2s *msg_d2s = (struct usb_msg_d2s *)req->buf;
	uint32_t length = ((uint32_t)msg->header.len_hi << 8) | msg->header.len_low;
	func *jump_fun;

	dest_addr = ((uint64_t)(msg->header.addr4) << 32) |
				((uint64_t)(msg->header.addr3) << 24) |
				((uint64_t)(msg->header.addr2) << 16) |
				((uint64_t)(msg->header.addr1) << 8) |
				((uint64_t)(msg->header.addr0));

	if (length == 0 && dest_addr == 0) {
		VERBOSE("buffer zero\n");
		resetOutReq();
		return;
	}
	/* dest_addr += GLOBAL_MEM_START_ADDR; */
	switch (msg->header.token) {
	case BM_USB_INFO:
		/* INFO("BM_USB_INFO\n"); */
		sendInReq(length, BM_USB_INFO, bulkResetOutReq, NULL, 0);
		return;
	case BM_USB_S2D:
		/* INFO("BM_USB_S2D, addr = 0x%lx, len = 0x%lx\n",dest_addr, msg_s2d->size); */
		sendInReq(length, BM_USB_S2D, bulkCmplEmpty, NULL, 0);
		if (dest_addr >= GLOBAL_MEM_START_ADDR)
			resetOutReqS2D(dest_addr, msg_s2d->size, bulkResetOutReq);
		else
			sramOutReqS2D(dest_addr, msg_s2d->size);
		return;
	case BM_USB_D2S:
		/* INFO("BM_USB_D2S\n"); */
		sendInReqD2S(dest_addr, msg_d2s->size, bulkResetOutReq);
		return;
	case BM_USB_NONE:
		INFO("BM_USB_NONE, addr = 0x%llx, len = 0x%x\n", dest_addr, length);
		memcpy((void *)dest_addr, cmdBuf + HEADER_SIZE, length - HEADER_SIZE);
		sendInReq(length, BM_USB_NONE, bulkResetOutReq, NULL, 0);
		return;
	case BM_USB_JUMP:
		jump_fun = (func *)dest_addr;
		NOTICE("BM_USB_JUMP to %llx\n", dest_addr);
		drv->stop(pD);
		NOTICE("stop USB port\n");
		jump_fun();
		NOTICE("BM_USB_JUMP back\n");
		resetOutReq();
		break;
	case BM_USB_RESET_ARM:
		NOTICE("BM_USB_RESET_ARM\n");
		break;
	case BM_USB_BREAK:
		INFO("BM_USB_BREAK\n");
		set_config_break(1);
		break;
	case BM_USB_KEEP_DL:
		NOTICE("BM_USB_KEEP_DL\n");
		crc = crc16_ccitt(0, cmdBuf, length);
		if (crc == 0xB353) {
			flagEnterDL = 1;
			NOTICE("flagEnterDL %d\n", flagEnterDL);
		} else {
			flagEnterDL = 0;
			NOTICE("MAGIC NUM NOT MATCH\n");
			NOTICE("flagEnterDL %d\n", flagEnterDL);
		}
		break;
	case BM_USB_PRG_CMD:
		NOTICE("BM_USB_PRG_CMD\n");
		for (i = 0; i < length; i++)
			NOTICE("cmdBuf[%d] = %x\n", i, cmdBuf[i]);
		break;
#if defined(EVB_TEST)
	case BM_USB_TEST_THERMAL_SENSOR:
		NOTICE("BM_USB_TEST_THERMAL_SENSOR\n");
		rsp_tmp.reserved[0] = (uint8_t)i2c_read_thermal_local();
		rsp_tmp.reserved[1] = (uint8_t)i2c_read_thermal_external();
		NOTICE("BM_USB_TEST_THERMAL_SENSOR done\n");
		sendInReq(length, BM_USB_TEST_THERMAL_SENSOR, bulkResetOutReq, rsp_tmp.reserved, 2);
		break;

	case BM_USB_TEST_EMMC:
		NOTICE("BM_USB_TEST_EMMC\n");
		plat_bm_set_pinmux(PINMUX_EMMC);
		bm_emmc_phy_init();
		bm_emmc_init();
		for (i = 0; i < 10; i++) {
			if (emmc_partition_ewr_test(EMMC_PARTITION_UDA, i) == -1) {
				rsp_tmp.reserved[0] = i;
				break;
			}
			INFO("eMMC ewr lba %d OK\n", i);
		}
		NOTICE("BM_USB_TEST_EMMC done");
		sendInReq(length, BM_USB_TEST_EMMC, bulkResetOutReq, rsp_tmp.reserved, 1);
		break;
#endif
	default:
		VERBOSE("token not defined:[%d]\n", msg->header.token);
		resetOutReq();
		break;
	}
}

/* functions aligns buffer to alignValue modulo address */

static uint8_t *alignBuff(void *notAllignedBuff, uint8_t alignValue)
{
	uint8_t offset = ((uintptr_t)notAllignedBuff) % alignValue;
	uint8_t *ret_address = &(((uint8_t *)notAllignedBuff)[alignValue - offset]);

	if (offset == 0)
		return (uint8_t *)notAllignedBuff;

	return ret_address;
}

/* ACM control ... data handling is delegated to tty library code.
 * The main task of this function is to activate and deactivate
 * that code based on device state; track parameters like line
 * speed, handshake state, and so on; and issue notifications.
 */

static void acm_complete_set_line_coding(struct CUSBD_Ep *ep, struct CUSBD_Req *req)
{
	struct CUSBD_PrivateData_TTY *dev;

	dev = get_DevPrivateObj(pD);
	struct usb_cdc_line_coding  *value = req->buf;
	/* VERBOSE("req buf : %d, %d, %d, %d\n", value->dwDTERate,
	 *	    value->bCharFormat,value->bParityType,value->bDataBits);
	 */
	acm->port_line_coding = *value;
	CPS_UncachedWrite32(&dev->reg->USBR_EP_STS, USBRF_EP_DESCMIS);
	VERBOSE("acm data transfer complete\n");
}

static void print_ep0_buf(uint32_t length)
{
	int i;

	for (i = 0; i < length; i++)
		VERBOSE("%02X ", ep0Buff[i]);
	VERBOSE(" %c\n", ' ');
}

static uint32_t setup(void *pD, struct CH9_UsbSetup *ctrl)
{
	/* get device reference */
	struct CUSBD_Dev *dev;
	int length = 0;
	uint16_t status_value[2];
	*(status_value + 0) = 0;
	*(status_value + 1) = 0;

	struct CH9_UsbDeviceDescriptor *devDesc;
	struct CH9_UsbEndpointDescriptor *endpointEpInDesc, *endpointEpOutDesc;

	ctrl->wIndex = le16ToCpu(ctrl->wIndex);
	ctrl->wLength = le16ToCpu(ctrl->wLength);
	ctrl->wValue = le16ToCpu(ctrl->wValue);

	drv->getDevInstance(pD, &dev);

	VERBOSE("Speed %d:\n", dev->speed);
	VERBOSE("bRequest: %02X\n", ctrl->bRequest);
	VERBOSE("bRequestType: %02X\n", ctrl->bmRequestType);
	VERBOSE("wIndex: %04X\n", ctrl->wIndex);
	VERBOSE("wValue: %04X\n", ctrl->wValue);
	VERBOSE("wLength: %04X\n", ctrl->wLength);

	ep0Req->buf = ep0Buff;
	ep0Req->dma = (uintptr_t)ep0Buff;
	ep0Req->complete = reqComplete;

	switch (dev->speed) {
	case CH9_USB_SPEED_FULL:
		endpointEpInDesc = &acm_fs_in_desc;
		endpointEpOutDesc = &acm_fs_out_desc;
		devDesc = &devHsDesc;
		break;

	case CH9_USB_SPEED_HIGH:
		endpointEpInDesc = &acm_hs_in_desc;
		endpointEpOutDesc = &acm_hs_out_desc;
		devDesc = &devHsDesc;
		break;

	case CH9_USB_SPEED_SUPER:
		endpointEpInDesc = &acm_ss_in_desc;
		endpointEpOutDesc = &acm_ss_out_desc;
		devDesc = &devSsDesc;
		break;
	default:
		VERBOSE("Unknown speed %d:\n", dev->speed);
		return 1;
	}

	switch (ctrl->bmRequestType & CH9_USB_REQ_TYPE_MASK) {
	case CH9_USB_REQ_TYPE_STANDARD:

		switch (ctrl->bRequest) {
		case CH9_USB_REQ_GET_DESCRIPTOR:
			VERBOSE("GET DESCRIPTOR %c\n", ' ');
			if ((ctrl->bmRequestType & CH9_REQ_RECIPIENT_MASK) == CH9_USB_REQ_RECIPIENT_INTERFACE) {
				switch (ctrl->wValue >> 8) {
				default:
					return -1;
				}
			} else if ((ctrl->bmRequestType & CH9_REQ_RECIPIENT_MASK) == CH9_USB_REQ_RECIPIENT_DEVICE) {
				switch (ctrl->wValue >> 8) {
				case CH9_USB_DT_DEVICE:
					length = CH9_USB_DS_DEVICE;
					if (bm_usb_vid != 0) {
						NOTICE("Patch VID %x\n", bm_usb_vid);
						devDesc->idVendor = cpuToLe16(bm_usb_vid);
					}
					memmove(ep0Buff, devDesc, 18);
					VERBOSE("DevDesc[0] = %d\n", devDesc->bLength);
					print_ep0_buf(length);
					break;

				case CH9_USB_DT_CONFIGURATION: {
					uint8_t *ptr = &ep0Buff[CH9_USB_DS_CONFIGURATION];
					uint16_t acmDescLen = (uint16_t)getDescAcm(dev->speed, ptr);

					length = le16ToCpu(acmDescLen + CH9_USB_DS_CONFIGURATION);
					ConfDesc.wTotalLength = cpuToLe16(length);
					memmove(ep0Buff, &ConfDesc, CH9_USB_DS_CONFIGURATION);
					print_ep0_buf(length);
					break;
				}

				case CH9_USB_DT_STRING: {
					uint8_t descIndex = (uint8_t)(ctrl->wValue & 0xFF);
					char *strDesc;

					VERBOSE("StringDesc %c\n", ' ');
					switch (descIndex) {
					case 0:
						strDesc = (char *)&languageDesc;
						length = strDesc[0];
						VERBOSE("language %c\n", ' ');
						break;

					case 1:
						strDesc = (char *)&vendorDesc;
						length = strDesc[0];
						VERBOSE("vendor %c\n", ' ');
						break;

					case 2:
						strDesc = (char *)&productDesc;
						length = strDesc[0];
						VERBOSE("product %c\n", ' ');
						break;

					case 3:
						strDesc = (char *)&serialDesc;
						length = strDesc[0];
						VERBOSE("serial %c\n", ' ');
						break;

					default:
						return -1;
					}
					memmove(ep0Buff, strDesc, length);
					break;
				}

				case CH9_USB_DT_BOS: {
					int offset = 0;

					length = le16ToCpu(bosDesc.wTotalLength);

					memmove(ep0Buff, &bosDesc, CH9_USB_DS_BOS);
					offset += CH9_USB_DS_BOS;
					/*Only USB3 should support CH9_USB_DS_DEVICE_CAPABILITY_30 descriptor*/
					if (dev->maxSpeed == CH9_USB_SPEED_SUPER ||
					    dev->maxSpeed == CH9_USB_SPEED_SUPER_PLUS) {
						memmove(&ep0Buff[offset], &capabilitySsDesc,
							CH9_USB_DS_DEVICE_CAPABILITY_30);
						offset += CH9_USB_DS_DEVICE_CAPABILITY_30;
					}
					memmove(&ep0Buff[offset], &capabilityExtDesc,
						CH9_USB_DS_DEVICE_CAPABILITY_20);
				}
				print_ep0_buf(length);
				VERBOSE("BosDesc %c\n", ' ');
				break;

				case CH9_USB_DT_DEVICE_QUALIFIER:
					length = CH9_USB_DS_DEVICE_QUALIFIER;
					memmove(ep0Buff, &qualifierDesc, length);
					break;

				case CH9_USB_DT_OTHER_SPEED_CONFIGURATION: {
					uint8_t *ptr = &ep0Buff[CH9_USB_DS_CONFIGURATION];
					uint16_t acmDescLen = (uint16_t)getDescAcm(dev->speed, ptr);

					length = le16ToCpu(acmDescLen + CH9_USB_DS_CONFIGURATION);
					ConfDesc.wTotalLength = cpuToLe16(length);
					memmove(ep0Buff, &ConfDesc, CH9_USB_DS_CONFIGURATION);
					print_ep0_buf(length);
					break;
				}

				default:
					return -1;

				}        /* switch */
			}        /* if */
			break;

		case CH9_USB_REQ_SET_CONFIGURATION: {
			struct CUSBD_Ep *ep;
			struct CUSBD_ListHead *list;

			VERBOSE("SET CONFIGURATION(%d)\n", le16ToCpu(ctrl->wValue));
			if (ctrl->wValue > 1)
				return -1;       /* no such configuration */
			/* unconfigure device */
			if (ctrl->wValue == 0) {
				configValue = 0;
				for (list = dev->epList.next; list != &dev->epList; list = list->next) {
					ep = (struct CUSBD_Ep *)list;
					if (ep->address == BULK_EP_IN || ep->address == BULK_EP_OUT)
						ep->ops->epDisable(pD, ep);
				}
				return 0;
			}

			/* device already configured */
			if (configValue == 1 && ctrl->wValue == 1)
				return 0;

			/* configure device */
			configValue = (uint8_t)ctrl->wValue;
			dev->config_state = 1;
			for (list = dev->epList.next; list != &dev->epList; list = list->next) {
				ep = (struct CUSBD_Ep *)list;
				if (ep->address == BULK_EP_IN) {
					ep->ops->epEnable(pD, ep, endpointEpInDesc);
					VERBOSE("enable EP IN\n");
					break;
				}
			}
			for (list = dev->epList.next; list != &dev->epList; list = list->next) {
				ep = (struct CUSBD_Ep *)list;
				if (ep->address == BULK_EP_OUT) {
					ep->ops->epEnable(pD, ep, endpointEpOutDesc);
					VERBOSE("enable EP OUT\n");
					break;
				}
			}
			/*Code control  Self powered feature of USB*/
			if (ConfDesc.bmAttributes & CH9_USB_CONFIG_SELF_POWERED) {
				if (drv->dSetSelfpowered)
					drv->dSetSelfpowered(pD);
			} else {
				if (drv->dClearSelfpowered)
					drv->dSetSelfpowered(pD);
			}
		}
		return 0;

		case CH9_USB_REQ_GET_CONFIGURATION:
			length = 1;
			memmove(ep0Buff, &configValue, length);
			/* VERBOSE("CH9_USB_REQ_GET_CONFIGURATION %c\n", ' '); */
			break;

		default:
			ep0Req->length = ctrl->wLength;
			/* memmove(ep0Buff, status_value, size); */
			/* memcpy((void*) dev->setupPacket, &dev->status_value, (size_t) size); */
			VERBOSE("length:%d, status_value:%x, %x\n", ep0Req->length, status_value[0], status_value[1]);
			ep0Req->buf = status_value;
			ep0Req->dma = (uintptr_t)status_value;
			ep0Req->complete = getStatusComplete;
#ifndef DISABLE_DCACHE
			flush_cache(ep0Req->dma, ep0Req->length);
#endif
			dev->ep0->ops->reqQueue(pD, dev->ep0, ep0Req);
			return 0;
		}
		break;

	case CH9_USB_REQ_TYPE_CLASS: {
		/* SET_LINE_CODING ... just read and save what the host sends */
		switch (ctrl->bRequest) {
		case USB_CDC_REQ_SET_LINE_CODING:
			length = ctrl->wLength;
			ep0Req->complete = acm_complete_set_line_coding;
			VERBOSE("USB_CDC_REQ_SET_LINE_CODING %d\n", length);
			set_acm_config(1);
			break;
		case USB_CDC_REQ_SET_CONTROL_LINE_STATE:
			acm->port_handshake_bits = ctrl->wValue;
			set_acm_config(1);
			VERBOSE("USB_CDC_REQ_SET_CONTROL_LINE_STATE %c\n", ' ');
			break;
		case USB_CDC_REQ_GET_LINE_CODING:
			length = ctrl->wLength;
			memmove(ep0Buff, &acm->port_line_coding, length);
			/* ep0Req->complete = acm_complete_get_line_coding; */
			VERBOSE("USB_CDC_REQ_GET_LINE_CODING %d\n", length);
			set_acm_config(1);
			break;
		}
		break;
	}
	}

	if (length > 0) {
		ep0Req->length = ctrl->wLength < length ? ctrl->wLength : length;
#ifndef DISABLE_DCACHE
		flush_cache(ep0Req->dma, ep0Req->length);
#endif
		dev->ep0->ops->reqQueue(pD, dev->ep0, ep0Req);
	}
	return 0;
}

static void get_unicode_string(char *target, const char *src)
{
	size_t src_len = strlen(src) * 2;
	int i;

	*target++ = src_len + 2;
	*target++ = CH9_USB_DT_STRING;

	if (src_len > 100)
		src_len = 100;
	for (i = 0; i < src_len; i += 2) {
		*target++ = *src++;
		*target++ = 0;
	}
}

static struct CUSBD_Callbacks callback = {
	disconnect, connect, setup, suspend, resume, requestMemAlloc, requestMemFree
};

int acm_app_init(void)
{
	/* CUSBD_ListHead *list; // used in for_each loop */

	/*  set unicode strings */
	get_unicode_string(vendorDesc, USB_MANUFACTURER_STRING);
	get_unicode_string(productDesc, USB_PRODUCT_STRING);
	get_unicode_string(serialDesc, USB_SERIAL_NUMBER_STRING);

	/*  align buffers to modulo8 address */
	ep0Buff = alignBuff(ep0BuffAlloc, 8);
	bulkBuf = alignBuff(bulkBufAlloc, 8);
	cmdBuf = alignBuff(cmdBufAlloc, 8);
	VERBOSE("bulkBuf %p bulkBufAlloc %p\n", bulkBuf, bulkBufAlloc);
	VERBOSE("cmdBuf %p cmdBufAlloc %p\n", cmdBuf, cmdBufAlloc);
	VERBOSE("ep0Buff %p ep0BuffAlloc %p\n", ep0Buff, ep0BuffAlloc);

	memset(ep0BuffAlloc, 0x00, EP0_SIZE);
	memset(bulkBufAlloc, 0x00, BUF_SIZE);
	memset(cmdBufAlloc,  0x00, BUF_SIZE);

	/* checking device and endpoints parameters correctness */
	/*  get CUSBD device instance exposed as Linux gadget device */
	drv->getDevInstance(pD, &dev);
	/* displayDeviceInfo(dev); */

	/* allocate request for ep0 */
	drv->reqAlloc(pD, dev->ep0, &ep0Req);

	/* Change descriptor for maxSpeed == HS only Device*/
	/* For USB2.0 we have to modified wTotalLength of BOS descriptor*/
	if (dev->maxSpeed < CH9_USB_SPEED_SUPER) {
		bosDesc.wTotalLength = cpuToLe16(CH9_USB_DS_BOS + CH9_USB_DS_DEVICE_CAPABILITY_20);
		bosDesc.bNumDeviceCaps = 1;
		devHsDesc.bcdUSB = cpuToLe16(BCD_USB_HS_ONLY);
	}

	/* acm init */
	acm = (struct f_acm *)acm_buf;
	acm->port_line_coding.dwDTERate = 921600;
	acm->port_line_coding.bCharFormat = USB_CDC_1_STOP_BITS;
	acm->port_line_coding.bParityType = USB_CDC_NO_PARITY;
	acm->port_line_coding.bDataBits = 8;
	acm->port_handshake_bits = 0;
	/* VERBOSE("acm size %X\n", sizeof(struct f_acm)); */
	return 0;
}

#ifdef BUILD_ATF
uint8_t usb_phy_det_connection(void)
{
	uint8_t phy_det_connected = 0;
	uint32_t bm_usb_phy_config = 0;

	bm_usb_phy_config  = plat_bm_gpio_read(BIT_MASK_GPIO_USB_PHY_DET_OFF);
	if (bm_usb_phy_config == 0) {
		phy_det_connected = 1;
		NOTICE("by pass USB phy detection\n");
	} else {
		mmio_clrbits_32(GPIO_BASE + 0x4, 1 << 4);
		phy_det_connected = (mmio_read_32(GPIO_BASE + 0x50) & (1 << 4)) >> 4;
		INFO("phy_det_connected %d\n", phy_det_connected);
	}

	return phy_det_connected;
}
#endif

#ifndef BUILD_ATF
uint32_t plat_bm_gpio_read(uint32_t mask)
{
	NOTICE("Overwrite fip_src to FIP_SRC_USB\n");
	return FIP_SRC_USB;
}
#endif

/* ACM entry */
int AcmApp(void)
{
	uint32_t res = 0; /* keeps result of operation on driver */
	struct CUSBD_SysReq sysReq;
	int fip_src = FIP_SRC_MEMMAP;
	uint32_t ts = 0;
	struct CUSBD_ListHead *list; /* used in for_each loop */
#ifdef BUILD_ATF
	uint8_t phy_det_connected = 0;

	phy_det_connected = usb_phy_det_connection();

	if (phy_det_connected == 0) {
		NOTICE("USB cable is not connected\n");
		return res;
	}
#endif
	acm_mem_init();
	print_buf_addr();
	fip_src = plat_bm_gpio_read(0);
	NOTICE("fip_src %d\n", fip_src);
	if (fip_src == FIP_SRC_USB)
		flagEnterDL = 1;
	else
		flagEnterDL = 0;
#ifdef USB_IRQ_MODE
	request_irq(USB_DEV_INTR0, AcmIsr, 0, NULL, NULL);
	request_irq(USB_DEV_INTR1, AcmIsr, 0, NULL, NULL);
#endif
	drv = CUSBD_GetInstance(); /* get driver handle */

	res = drv->probe(&config, &sysReq);
	if (res != 0)
		goto error;
	VERBOSE("Device memory requirement: %d bytes\n", sysReq.privDataSize);
#if USE_MALLOC
	/* allocate memory for device object */
	pD = (uint8_t *)malloc((size_t)(sysReq.privDataSize));
	if (!pD) {
		VERBOSE("Error line: %d", __LINE__);
		goto error;
	} else {
		VERBOSE("pD addr %p\n", pD);
	}

	config.trbAddr = malloc(sysReq.trbMemSize);
	/* config.trbAddr = 0x04000000; */
	if (!config.trbAddr) {
		VERBOSE("Error line: %d", __LINE__);
		goto error;
	} else {
		VERBOSE("config.trbAddr addr %p\n", config.trbAddr);
	}
#else
	pD = pd_buf;
	VERBOSE("alloc pD addr %p\n", pD);

	config.trbAddr = trb_buf;
	VERBOSE("alloc config.trbAddr addr %p\n", config.trbAddr);
#endif
	config.trbDmaAddr = (uintptr_t)config.trbAddr;
	VERBOSE("config.trbDmaAddr addr %lx\n", config.trbDmaAddr);
	res = drv->init(pD, &config, &callback);
	if (res != 0)
		goto error;

	drv->stop(pD);
	NOTICE("Stop ATF USB port\n");
	acm_app_init();

	VERBOSE("Initializing OK! %d\n", __LINE__);

	drv->start(pD);

	ts = get_timer(0);
	VERBOSE("ts: %u\n", get_timer(ts));

unconfigured:
	while (!get_acm_config()) {
#ifndef USB_IRQ_MODE
		AcmIsr();
#endif
		if (get_timer(ts) > 1000 && flagEnterDL == 0) {
			NOTICE("Enumeration failed\n");
			acm_mem_release();
			return 0;
		}
	}
	NOTICE("USB enumeration done\n");

	mem_alloc_cnt = 1;
	/* find IN endpoint */
	for (list = dev->epList.next; list != &dev->epList; list = list->next) {
		epIn = (struct CUSBD_Ep *)list;
		if (epIn->address == BULK_EP_IN) {
			drv->reqAlloc(pD, epIn, &bulkInReq);
			break;
		}
	}

	/* find OUT endpoint */

	for (list = dev->epList.next; list != &dev->epList; list = list->next) {
		epOut = (struct CUSBD_Ep *)list;
		if (epOut->address == BULK_EP_OUT) {
			drv->reqAlloc(pD, epOut, &bulkOutReq);
			break;
		}
	}

	current_speed = dev->speed;
	switch (current_speed) {
	case CH9_USB_SPEED_FULL:
		transfer_size = 64;
		break;
	case CH9_USB_SPEED_HIGH:
		transfer_size = 512;
		break;
	case CH9_USB_SPEED_SUPER:
		transfer_size = 1024;
		break;
	default:
		VERBOSE("Test error\n");
		acm_mem_release();
		return -1;
	}

	VERBOSE("OUT DATA TRANSFER size :%d\n", transfer_size);
	clearReq(bulkOutReq);
	bulkOutReq->buf = cmdBuf;
	bulkOutReq->dma = (uintptr_t)cmdBuf;
	bulkOutReq->complete = bulkOutCmplMain;
	bulkOutReq->length = transfer_size;
#ifndef DISABLE_DCACHE
	flush_cache(bulkOutReq->dma, bulkOutReq->length);
#endif
	VERBOSE("IN DATA TRANSFER\n");
	clearReq(bulkInReq);
	bulkInReq->buf = bulkBuf;
	bulkInReq->dma = (uintptr_t)bulkBuf;
	bulkInReq->complete = bulkResetOutReq;
	bulkInReq->length = transfer_size;
#ifndef DISABLE_DCACHE
	flush_cache(bulkInReq->dma, bulkInReq->length);
#endif
	epOut->ops->reqQueue(pD, epOut, bulkOutReq);
	NOTICE("connection speed: %d\n", dev->speed);
	ts = get_timer(0);
	VERBOSE("ts: %u\n", get_timer(ts));

	while (1) {
#ifndef USB_IRQ_MODE
		AcmIsr();
#endif
		if (!get_acm_config())
			goto unconfigured;
		if (get_config_break())
			break;
		if (flagEnterDL == 0) {
			if (get_timer(ts) > 1000) {
				NOTICE("wait data timeout\n");
				break;
			}
		}
	}
	NOTICE("Leave transfer loop\n");

	drv->stop(pD);
	drv->destroy(pD);
	acm_mem_release();
	NOTICE("USB stop\n");
	return 0;

error:
	ERROR("Error %u\n", res);
	return res;
}
