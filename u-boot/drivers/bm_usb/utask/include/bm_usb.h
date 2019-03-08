/*
 * Copyright (c) 2015-2016, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __BM_USB_H
#define __BM_USB_H

#define BCD_USB_SS                      0x0310  /* 3.00 version USB*/
#define ID_VENDOR                       0x0559  /* Cadence*/
#define ID_PRODUCT                      0x1001  /* example bulk product*/
#define BCD_DEVICE_SS                   0x0100  /* 0.1*/
#define BCD_USB_HS_ONLY                 0x0201  /* 2.01  Only HS with BOS descriptor */
#define BCD_USB_HS                      0x0210  /* 2.10*/
#define BCD_DEVICE_HS                   0x0200  /* 2.00*/

#define USB_MANUFACTURER_STRING         "BITMAIN"
#define USB_PRODUCT_STRING              "USB Com Port"
#define USB_SERIAL_NUMBER_STRING        "123456789ABC" /* should 12 chars long*/

#define BULK_EP_IN 0x81
#define BULK_EP_OUT 0x01
#define BULK_EP_NOTIFY 0x82
#define HEADER_SIZE 8

#define SS_PERIPH_DISABLED_SET BIT(21)
#define HOST_BUS_DROP          BIT(9)
#define DEV_BUS_REQ            (1)
#define INTR_SPI_BASE          32
#define USB_DEV_INTR0          115
/*#define USB_DEV_INTR1          116*/
/*#define USB_IRQ_MODE*/

enum BM_USB_TOKEN {
	BM_USB_NONE = 0,
	BM_USB_INFO,
	BM_USB_VERBOSE,
	BM_USB_JUMP,
	BM_USB_BREAK,
	BM_USB_KEEP_DL,
	BM_USB_PRG_CMD,
	BM_USB_RESET_ARM,
	BM_USB_TEST_THERMAL_SENSOR,
	BM_USB_TEST_EMMC,
	BM_USB_TEST_GET_RESULT,

	BM_USB_RUNTIME = 0x80,
	BM_USB_S2D = 0x81,
	BM_USB_D2S = 0x82

};

typedef void func(void);

#define USB_BUF_BASE      0x08001000
/*#define USB_BUF_BASE      0x0401E000*/
#define PD_SIZE           4096
#define BUF_SIZE          1024
#define EP0_SIZE          512
#define TRB_SIZE          256
#define CB_SIZE           128
#define RSP_SIZE          16

#define PD_ADDR USB_BUF_BASE
#define BLK_BUF_ADDR  (PD_ADDR + PD_SIZE)       /* 1024*/
#define CMD_BUF_ADDR  (BLK_BUF_ADDR + BUF_SIZE) /* 1024*/
#define TRB_BUF_ADDR  (CMD_BUF_ADDR + BUF_SIZE) /* 256*/
#define CB0_BUF_ADDR  (TRB_BUF_ADDR + TRB_SIZE) /* 128*/
#define CB1_BUF_ADDR  (CB0_BUF_ADDR + CB_SIZE)  /* 128*/
#define CB2_BUF_ADDR  (CB1_BUF_ADDR + CB_SIZE)  /* 128*/
#define EP0_BUF_ADDR  (CB2_BUF_ADDR + CB_SIZE)  /* 512*/
#define RSP_BUF_ADDR  (EP0_BUF_ADDR + EP0_SIZE) /* 16*/
#define ACM_BUF_ADDR  (RSP_BUF_ADDR + RSP_SIZE) /* 128*/

int bm_usb_polling(void);
#endif
