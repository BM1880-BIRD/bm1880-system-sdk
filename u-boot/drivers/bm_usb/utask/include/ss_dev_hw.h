/******************************************************************************
 * Copyright (C) 2014-2015 Cadence Design Systems, Inc.
 * All rights reserved worldwide.
 ******************************************************************************
 * ss_dev_hw.h
 * Super speed hardware header file
 *
 *****************************************************************************/

#ifndef SS_DEV_HW_H
#define	SS_DEV_HW_H

#ifdef __cplusplus
extern "C" {
#endif

#if 0
static char *const reg_name[] = {
	"USBR_CONF",
	"USBR_STS",
	"USBR_CMD",
	"USBR_ITPN",
	"USBR_LPM",
	"USBR_IEN",
	"USBR_ISTS",
	"USBR_EP_SEL",
	"USBR_EP_TRADDR",
	"USBR_EP_CFG",
	"USBR_EP_CMD",
	"USBR_EP_STS",
	"USBR_EP_STS_SID",
	"USBR_EP_STS_EN",
	"USBR_DRBL",
	"USBR_EP_IEN",
	"USBR_EP_ISTS",
	"USBR_SER0",
	"USBR_SER1"
};
#endif

#ifdef IMPLEMENT_SFR_64
#define USBR_REGISTER(REG_OFFSET_) ((USBR_BASE + (REG_OFFSET_)) << 1)
#else
#define USBR_REGISTER(REG_OFFSET_) ((uint32_t *)(USBR_BASE + (REG_OFFSET_)))
#endif

#define U32_BIT(BIT_INDEX_) \
	((uint32_t)(1ul << (BIT_INDEX_)))

#define U32_MASK(OFFSET_, WIDTH_) \
	((uint32_t)(~(((WIDTH_) < 32 ? 0xFFFFFFFFul : 0x00ul) << (WIDTH_)) << (OFFSET_)))

struct ssReg {
	uint32_t USBR_CONF;
	uint32_t USBR_STS;
	uint32_t USBR_CMD;
	uint32_t USBR_ITPN;
	uint32_t USBR_LPM;
	uint32_t USBR_IEN;
	uint32_t USBR_ISTS;
	uint32_t USBR_EP_SEL;
	uint32_t USBR_EP_TRADDR;
	uint32_t USBR_EP_CFG;
	uint32_t USBR_EP_CMD;
	uint32_t USBR_EP_STS;
	uint32_t USBR_EP_STS_SID;
	uint32_t USBR_EP_STS_EN;
	uint32_t USBR_DRBL;
	uint32_t USBR_EP_IEN;
	uint32_t USBR_EP_ISTS;
	uint32_t USBR_SER0;
	uint32_t USBR_SER1;
};

/*-----------------------------------------------------------------------------*/
/* USB_CONF - Global Configuration Register*/
/*-----------------------------------------------------------------------------*/

/* Flags*/
#define USBRF_CFGRST           U32_BIT(0)
#define USBRF_CFGSET           U32_BIT(1)
#define USBRF_USBDIS           U32_BIT(3)
#define USBRF_HSDIS            U32_BIT(4)
#define USBRF_LENDIAN          U32_BIT(5)
#define USBRF_BENDIAN          U32_BIT(6)
#define USBRF_SWRST            U32_BIT(7)
#define USBRF_DSING            U32_BIT(8)
#define USBRF_DMULT            U32_BIT(9)
#define USBRF_DMAOFFEN         U32_BIT(10)
#define USBRF_DMAOFFDS         U32_BIT(11)
#define USBRF_FORCEFSCLR       U32_BIT(12)
#define USBRF_FORCEFSSET       U32_BIT(13)
#define USBRF_DEVEN            U32_BIT(14)
#define USBRF_DEVDS            U32_BIT(15)
#define USBRF_L1EN             U32_BIT(16)
#define USBRF_L1DS             U32_BIT(17)
#define USBRF_CLK2OFFEN        U32_BIT(18)
#define USBRF_CLK2OFFDS        U32_BIT(19)
#define USBRF_LGO_L0           U32_BIT(20)
#define USBRF_CLK3OFFEN        U32_BIT(21)
#define USBRF_CLK3OFFDS        U32_BIT(22)
#define USBRF_U1EN             U32_BIT(24)
#define USBRF_U1DS             U32_BIT(25)
#define USBRF_U2EN             U32_BIT(26)
#define USBRF_U2DS             U32_BIT(27)
#define USBRF_LGO_U0           U32_BIT(28)
#define USBRF_LGO_U1           U32_BIT(29)
#define USBRF_LGO_U2           U32_BIT(30)

/*-----------------------------------------------------------------------------*/
/* USB_CMD - Global USB Command Register*/
/*-----------------------------------------------------------------------------*/

/* Flags*/
#define USBRF_SET_ADDR         U32_BIT(0)
#define USBRF_DEV_NOTIFY       U32_BIT(8)
#define USBRF_TESTMODE_ENABLE  U32_BIT(9)
#define USBRF_SEND_LTM         U32_BIT(12)
#define USBRF_SEND_FUNC_WAKEUP U32_BIT(8)

/* Masks*/
#define USBRM_FADDR            U32_MASK(1, 7)
#define USBRM_TESTMODE_SEL     U32_MASK(10, 2)
#define USBRM_DEV_NOTIFY_INT   U32_MASK(16, 8)
#define USBRM_DEV_NOTIFY_BELT  U32_MASK(16, 12)

/* Values*/
#define USBRV_TM_TEST_J        0ul
#define USBRV_TM_TEST_K        1ul
#define USBRV_TM_SE0_NAK       2ul
#define USBRV_TM_TEST_PACKET   3ul

/* Defines*/
#define USBRD_SET_TESTMODE(TESTMODE_) \
	((((TESTMODE_) << 10) & (USBRM_TESTMODE_SEL)) | USBRF_TESTMODE_ENABLE)

#define USBRD_GET_TESTMODE(USBR_CMD_) \
	((uint32_t)(((USBR_CMD_) & (USBRM_TESTMODE_SEL))) >> 10)

#define USBRD_SET_BELT(BELT_) \
	((((BELT_) << 16) & (USBRM_DEV_NOTIFY_BELT)) | USBRF_SEND_LTM)

#define USBRD_GET_BELT(USBR_CMD_) \
	((uint32_t)(((USBR_CMD_) & (USBRM_DEV_NOTIFY_BELT))) >> 16)

#define USBRD_SET_FUNC_WAKEUP(FW_) \
	((((FW_) << 16) & (USBRM_DEV_NOTIFY_INT)) | (USBRF_SEND_FUNC_WAKEUP))

#define USBRD_GET_FUNC_WAKEUP(USBR_CMD_) \
	((uint32_t)(((USBR_CMD_) & (USBRM_DEV_NOTIFY_INT))) >> 16)

/*-----------------------------------------------------------------------------*/
/* USB_STS - Global Status Register*/
/*-----------------------------------------------------------------------------*/

/* Flags*/
#define USBRF_CFGSTS           U32_BIT(0)
#define USBRF_MEM_OV           U32_BIT(1)
#define USBRF_USBCONS          U32_BIT(2)
#define USBRF_DTRANS           U32_BIT(3)
#define USBRF_ENDIAN           U32_BIT(7)
#define USBRF_CLK2OFF          U32_BIT(8)
#define USBRF_CLK3OFF          U32_BIT(9)
#define USBRF_DEVS             U32_BIT(14)
#define USBRF_ADDRESSED        U32_BIT(15)
#define USBRF_L1ENS            U32_BIT(16)
#define USBRF_VBUSS            U32_BIT(17)
#define USBRF_HSCONS           U32_BIT(20)
#define USBRF_FORCEFSS         U32_BIT(21)
#define USBRF_U1ENS            U32_BIT(24)
#define USBRF_U2ENS            U32_BIT(25)
#define USBRF_DMAOFF           U32_BIT(30)

/* Masks*/
#define USBRM_USBSPEED         U32_MASK(4, 3)
#define USBRM_LPMST            U32_MASK(18, 2)
#define USBRM_LST              U32_MASK(26, 4)

/* Values*/
#define USBRV_UNDEFINED_SPEED      0ul
#define USBRV_LOW_SPEED            1ul
#define USBRV_FULL_SPEED           2ul
#define USBRV_HIGH_SPEED           3ul
#define USBRV_SUPER_SPEED          4ul

#define USBRV_LST_U0               0ul
#define USBRV_LST_U1               1ul
#define USBRV_LST_U2               2ul
#define USBRV_LST_U3               3ul
#define USBRV_LST_DISABLED         4ul
#define USBRV_LST_RX_DETECT        5ul
#define USBRV_LST_INACTIVE         6ul
#define USBRV_LST_POLLING          7ul
#define USBRV_LST_RECOVERY         8ul
#define USBRV_LST_HOT_RESET        9ul
#define USBRV_LST_COMPILANCE_MODE  10ul
#define USBRV_LST_LOOPBACK         11ul

/* Defines*/
/* Macros for registers; register USB_STS is read only*/
#if 0
#define USBRD_USBSPEED(SPEED_) ((SPEED_) << 4 & USBRM_USB_SPEED)
#define USBRD_SPEED_UNDEFINED USBRD_USB_SPEED(USBV_UNDEFINED_SPEED)
#define USBRD_SPEED_LOW USBRD_USB_SPEED(USBV_LOW_SPEED)
#define USBRD_SPEED_FULL USBRD_USB_SPEED(USBV_FULL_SPEED)
#define USBRD_SPEED_HIGH USBRD_USB_SPEED(USBV_HIGH_SPEED)
#define USBRD_SPEED_SUPER USBRD_USB_SPEED(USBV_SUPER_SPEED)
#define USBRD_FADDR(FUNCTION_ADDR_) ((FADDR_) << 8 & USBRM_FADDR)
#define USBRD_LST(LINK_STATE_) ((LINK_STATE_) << 26 & USBRM_LST)
#define USBRD_LST_U0 (USBV_LST_U0 << 26 & USBRM_LST)
#define USBRD_LST_U1 (USBV_LST_U1 << 26 & USBRM_LST)
#define USBRD_LST_U2 (USBV_LST_U2 << 26 & USBRM_LST)
#define USBRD_LST_U3 (USBV_LST_U3 << 26 & USBRM_LST)
#define USBRD_LST_DISABLED (USBV_LST_DISABLED << 26 & USBRM_LST)
#define USBRD_LST_RX_DETECT (USBV_LST_RX_DETECT << 26 & USBRM_LST)
#define USBRD_LST_INACTIVE (USBV_LST_INACTIVE << 26 & USBRM_LST)
#define USBRD_LST_POLLING (USBV_LST_POLLING << 26 & USBRM_LST)
#define USBRD_LST_RECOVERY (USBV_LST_RECOVERY << 26 & USBRM_LST)
#define USBRD_LST_HOT_RESET (USBV_LST_HOT_RESET << 26 & USBRM_LST)
#define USBRD_LST_COMPILANCE_MODE (USBV_LST_COMPILANCE_MODE << 26 & USBRM_LST)
#define USBRD_LST_LOOPBACK (USBV_LST_LOOPBACK << 26 & USBRM_LST)
#endif

#define USBRD_GET_USBSPEED(STS_REG_VAL_) \
	((uint32_t)((STS_REG_VAL_) & (USBRM_USBSPEED)) >> 4)
#define USBRD_GET_FADDR(STS_REG_VAL_) \
	(uint32_t)(((STS_REG_VAL_) & (USBRM_FADDR)) >> 8)
#define USBRD_GET_LST(STS_REG_VAL_) \
	((uint32_t)((STS_REG_VAL_) & (USBRM_LST)) >> 26)
#define USBRD_GET_CLK3OFF(STS_REG_VAL_) \
	((bool)(((STS_REG_VAL_) & (USBRF_CLK3OFF)) >> 9))

/*-----------------------------------------------------------------------------*/
/* USBR_LPM - Low Power Mode Register*/
/*-----------------------------------------------------------------------------*/
#define USBRF_BRW              U32_BIT(4)
#define USBRM_HIRD             U32_MASK(0, 4)

/* Macros for soft*/

/*-----------------------------------------------------------------------------*/
/* USB_EP_SEL - Endpoint Select Register*/
/*-----------------------------------------------------------------------------*/

/* Masks*/
#define USBRM_EP_NUM          U32_MASK(0, 4)
#define USBRM_EP_DIR          U32_MASK(7, 1)
#define USBRM_EP_ADDR         (USBRM_EP_DIR | USBRM_EP_NUM)

/* Defines*/
#define USBRV_EP_IN           1ul
#define USBRV_EP_OUT          0ul

#define USBRD_EP_IN           0x80ul
#define USBRD_EP_OUT          0x00ul
#define USBRD_EP_DIR(EP_DIR_VAL_) (((EP_DIR_VAL_) << 7) & USBRM_EP_DIR)
#define USBRD_EP_NUM(EP_NUM_VAL_) ((EP_NUM_VAL_) & USBRM_EP_NUM)
#define USBRD_EP_ADDR(EP_NUM_VAL_, EP_DIR_VAL_) \
	(USBRD_EP_NUM(EP_NUM_VAL_) | USBRD_EP_DIR(EP_DIR_VAL_))

#define USBRD_GET_EP_DIR(EP_SEL_REG_VAL_) \
	((uint32_t)((EP_SEL_REG_VAL_) & USBRM_EP_DIR) >> 7)
#define USBRD_GET_EP_NUM(EP_SEL_REG_VAL_) \
	((uint32_t)(EP_SEL_REG_VAL_) & USBRM_EP_NUM)
#define USBRD_GET_EP_ADDR(EP_SEL_REG_VAL_) \
	((uint32_t)(EP_SEL_REG_VAL_) & USBRM_EP_ADDR)

/*-----------------------------------------------------------------------------*/
/* USB_EP_TRADDR - Endpoint transfer ring address Register*/
/*-----------------------------------------------------------------------------*/

/* Masks*/
#define USBRM_EP_TRADDR       U32_MASK(0, 32)

/*-----------------------------------------------------------------------------*/
/* USB_EP_CFG - Endpoint Configuration Register*/
/*-----------------------------------------------------------------------------*/

/* Flags*/
#define USBRF_EP_ENABLE        U32_BIT(0)
#define USBRF_EP_STREAM_EN     U32_BIT(3)
#define USBRF_EP_TDL_CHK       U32_BIT(4)
#define USBRF_EP_SID_CHK       U32_BIT(5)
#define USBRF_EP_ENDIAN        U32_BIT(7)

/* Values*/
#define USBRV_EP_DISABLED      0ul
#define USBRV_EP_ENABLED       1ul

#define USBRV_EP_CONTROL       0ul
#define USBRV_EP_ISOCHRONOUS   1ul
#define USBRV_EP_BULK          2ul
#define USBRV_EP_INTERRUPT     3ul

#define USBRV_EP_LITTLE_ENDIAN 0ul
#define USBRV_EP_BIG_ENDIAN    1ul

/* Masks*/
#define USBRM_EP_ENABLE        U32_MASK(0, 1)
#define USBRM_EP_TYPE          U32_MASK(1, 2)
#define USBRM_EP_STREAM_EN     U32_MASK(3, 1)
#define USBRM_EP_TDL_CHK       U32_MASK(4, 1)
#define USBRM_EP_SID_CHK       U32_MASK(5, 1)
#define USBRM_EP_ENDIAN        U32_MASK(7, 1)
#define USBRM_EP_MAXBURST      U32_MASK(8, 4)
#define USBRM_EP_MULT          U32_MASK(14, 2)
#define USBRM_EP_MAXPKSIZE     U32_MASK(16, 11)
#define USBRM_EP_BUFFERING     U32_MASK(27, 5)

/* Defines*/
#define USBRD_EP_ENABLE(EP_ENABLE_VAL_) \
	((EP_ENABLE_VAL_) & USBRM_EP_ENABLE)
#define USBRD_EP_ENABLED USBRD_EP_ENABLE(USBRV_EP_ENABLED)
#define USBRD_EP_DISABLED USBRD_EP_ENABLE(USBRV_EP_DISABLED)
#define USBRD_EP_TYPE(EP_TYPE_VAL_)\
	(((EP_TYPE_VAL_) << 1) & USBRM_EP_TYPE)
#define USBRD_EP_CONTROL USBRD_EP_TYPE(USBRV_EP_CONTROL)
#define USBRD_EP_ISOCHRONOUS USBRD_EP_TYPE(USBRV_EP_ISOCHRONOUS)
#define USBRD_EP_BULK USBRD_EP_TYPE(USBRV_EP_BULK)
#define USBRD_EP_INTERRUPT USBRD_EP_TYPE(USBRV_EP_INTERRUPT)
#define USBRD_EP_ENDIAN(ENDIAN_VAL_)\
	(((ENDIAN_VAL_) << 7) & USBRM_EP_ENDIAN)
#define USBRD_EP_LITTLE_ENDIAN USBRD_EP_ENDIAN(USBRV_EP_LITTLE_ENDIAN)
#define USBRD_EP_BIG_ENDIAN USBRD_EP_ENDIAN(USBRV_EP_BIG_ENDIAN)
#define USBRD_EP_MAXBURST(MAX_BURST_VAL_)\
	(((MAX_BURST_VAL_) << 8) & USBRM_EP_MAXBURST)
#define USBRD_EP_MULT(MULT_VAL_)\
	(((MULT_VAL_) << 14) & USBRM_EP_MULT)
#define USBRD_EP_MAXPKSIZE(MAX_PK_SIZE_VAL_)\
	((((uint32_t)MAX_PK_SIZE_VAL_) << 16) & USBRM_EP_MAXPKSIZE)
#define USBRD_EP_BUFFERING(BUFFERING_VAL_)\
	((((uint32_t)BUFFERING_VAL_) << 27) & USBRM_EP_BUFFERING)

#define USBRD_GET_EP_TYPE(EP_CFG_REG_VAL_) \
	(((EP_CFG_REG_VAL_) & USBRM_EP_TYPE) >> 1)
#define USBRD_GET_EP_ENDIAN(EP_CFG_REG_VAL_) \
	(((EP_CFG_REG_VAL_) & USBRM_EP_ENDIAN) >> 7)
#define USBRD_GET_EP_MAXBURST(EP_CFG_REG_VAL_) \
	(((EP_CFG_REG_VAL_) & USBRM_EP_MAXBURST) >> 8)
#define USBRD_GET_EP_MULT(EP_CFG_REG_VAL_) \
	(((EP_CFG_REG_VAL_) & USBRM_EP_MULT) >> 14)
#define USBRD_GET_EP_MAXPKSIZE(EP_CFG_REG_VAL_) \
	(((EP_CFG_REG_VAL_) & USBRM_EP_MAXPKSIZE) >> 16)
#define USBRD_GET_EP_BUFFERING(EP_CFG_REG_VAL_) \
	(((EP_CFG_REG_VAL_) & USBRM_EP_BUFFERING) >> 27)

/*-----------------------------------------------------------------------------*/
/* USB_EP_CMD - Endpoint Command Register*/
/*-----------------------------------------------------------------------------*/

/* Flags*/
#define USBRF_EP_EPRST         U32_BIT(0)
#define USBRF_EP_SSTALL        U32_BIT(1)
#define USBRF_EP_CSTALL        U32_BIT(2)
#define USBRF_EP_ERDY          U32_BIT(3)
#define USBRF_EP_REQ_CMPL      U32_BIT(5)
#define USBRF_EP_DRDY          U32_BIT(6)
#define USBRF_EP_DFLUSH        U32_BIT(7)
#define USBRF_EP_STDL          U32_BIT(8)

/* Masks*/
#define USBRM_EP_TDL           U32_MASK(9, 7)
#define USBRM_EP_ERDY_SID      U32_MASK(16, 16)

/* Defines*/
#define USBRD_EP_ERDY_SID(ERDY_SID_VAL_) \
	(((uint32_t)(ERDY_SID_VAL_) << 16) & USBRM_EP_ERDY_SID)
#define USBRD_GET_EP_ERDY_SID(EP_CMD_REG_VAL_) \
	((uint32_t)((EP_CMD_REG_VAL_) & USBRM_EP_ERDY_SID) >> 16)
#define USBRD_EP_TDL(TDL_VAL_) \
	(((TDL_VAL_) << 9) & USBRM_EP_TDL)
#define USBRD_GET_EP_TDL(EP_CMD_REG_VAL_) \
	((uint32_t)((EP_CMD_REG_VAL_) & USBRM_EP_TDL) >> 9)

/*-----------------------------------------------------------------------------*/
/* USB_EP_STS - Endpoint Status Register*/
/*-----------------------------------------------------------------------------*/

/* Masks*/
#define USBRF_EP_SETUP         U32_BIT(0)
#define USBRF_EP_STALL         U32_BIT(1)
#define USBRF_EP_IOC           U32_BIT(2)
#define USBRF_EP_ISP           U32_BIT(3)
#define USBRF_EP_DESCMIS       U32_BIT(4)
#define USBRF_EP_STREAMR       U32_BIT(5)
#define USBRF_EP_MD_EXIT       U32_BIT(6)
#define USBRF_EP_TRBERR        U32_BIT(7)
#define USBRF_EP_NRDY          U32_BIT(8)
#define USBRF_EP_DBUSY         U32_BIT(9)
#define USBRF_EP_BUFFEMPTY     U32_BIT(10)
#define USBRF_EP_CCS           U32_BIT(11)
#define USBRF_EP_PRIME         U32_BIT(12)
#define USBRF_EP_SIDERR        U32_BIT(13)
#define USBRF_EP_OUTSMM        U32_BIT(14)
#define USBRF_EP_ISOERR        U32_BIT(15)
#define USBRF_EP_HOSTPP        U32_BIT(16)
#define USBRF_EP_IOT           U32_BIT(19)
#define USBRF_EP_OUTQ_VAL      U32_BIT(28)

#define USBRF_EP_UNKNOWN_INT \
	(!(USBRF_EP_SETUP | USBRF_EP_STALL | USBRF_EP_IOC | USBRF_EP_ISP \
	| USBRF_EP_DESCMIS | USBRF_EP_TRBERR | USBRF_EP_NRDY | USBRF_EP_BUFFEMPTY \
	| USBRF_EP_PRIME | USBRF_EP_SIDERR | USBRF_EP_OUTSMM | USBRF_EP_ISOERR \
	| USBRF_EP_STREAMR | USBRF_EP_MD_EXIT))

/* Masks*/
#define USBRM_EP_SPSMST        U32_MASK(17, 2)
#define USBRM_EP_OUTQ_NO       U32_MASK(24, 4)

#define USBRD_GET_EP_SPSMST(EP_STS_REG_VAL_) \
	((uint8_t)((EP_STS_REG_VAL_) & USBRM_EP_SPSMST) >> 17)
#define USBRD_GET_EP_OUTQ_NO(EP_STS_REG_VAL_) \
	((uint32_t)((EP_STS_REG_VAL_) & USBRM_EP_SPSMST) >> 17)
#define USBRD_GET_HOSTPP(EP_STS_REG_VAL_) \
	((bool)(((EP_STS_REG_VAL_) & USBRF_EP_HOSTPP) >> 16))

/*-----------------------------------------------------------------------------*/
/* EP_STS_SID - Endpoint Status Register (Stream ID)*/
/*-----------------------------------------------------------------------------*/

/* Masks*/
#define USBRM_EP_SID           U32_MASK(0, 16)

/* Defines*/
#define USBRD_EP_SID(SID_VAL_) ((uint32_t)(SID_VAL_) & USBRM_EP_SID)

#define USBRD_GET_EP_SID(EP_STS_REG_VAL_) \
	((uint16_t)((uint32_t)((EP_STS_REG_VAL_) & USBRM_EP_SID)))

/*-----------------------------------------------------------------------------*/
/* EP_STS_EN - Endpoint Status Register Enable*/
/*-----------------------------------------------------------------------------*/

/* Flags*/
#define USBRF_EP_SETUPEN        U32_BIT(0)
#define USBRF_EP_DESCMISEN      U32_BIT(4)
#define USBRF_EP_STREAMREN      U32_BIT(5)
#define USBRF_EP_MD_EXITEN      U32_BIT(6)
#define USBRF_EP_TRBERREN       U32_BIT(7)
#define USBRF_EP_NRDYEN         U32_BIT(8)
#define USBRF_EP_PRIMEEN        U32_BIT(12)
#define USBRF_EP_SIDERREN       U32_BIT(13)
#define USBRF_EP_OUTSMMEN       U32_BIT(14)
#define USBRF_EP_ISOERREN       U32_BIT(15)
#define USBRF_EP_IOTEN          U32_BIT(19)

/*-----------------------------------------------------------------------------*/
/* DRBL - Doorbell Register*/
/*-----------------------------------------------------------------------------*/

/* Defines*/
#define USBRD_DRBL(EP_NUM_, EP_DIR_) \
	(1ul << (EP_NUM_) << ((EP_DIR_) ? 16 : 0))

/*-----------------------------------------------------------------------------*/
/* USB_IEN - USB Interrupt Enable Register*/
/*-----------------------------------------------------------------------------*/

/* Flags*/
#define USBRF_CONIEN            U32_BIT(0)
#define USBRF_DISIEN            U32_BIT(1)
#define USBRF_UWRESIEN          U32_BIT(2)
#define USBRF_UHRESIEN          U32_BIT(3)
#define USBRF_U3ENTIEN          U32_BIT(4)
#define USBRF_U3EXTIEN          U32_BIT(5)
#define USBRF_U2ENTIEN          U32_BIT(6)
#define USBRF_U2EXTIEN          U32_BIT(7)
#define USBRF_U1ENTIEN          U32_BIT(8)
#define USBRF_U1EXTIEN          U32_BIT(9)
#define USBRF_ITPIEN            U32_BIT(10)
#define USBRF_WAKEIEN           U32_BIT(11)
#define USBRF_CON2IEN           U32_BIT(16)
#define USBRF_DIS2IEN           U32_BIT(17)
#define USBRF_UHSFSRESIEN       U32_BIT(18)
#define USBRF_L2ENTIEN          U32_BIT(20)
#define USBRF_L2EXTIEN          U32_BIT(21)
#define USBRF_L1ENTIEN          U32_BIT(24)
#define USBRF_L1EXTIEN          U32_BIT(25)

/*-----------------------------------------------------------------------------*/
/* USB_ISTS - USB Interrupt Status Register*/
/*-----------------------------------------------------------------------------*/

/* Flags*/
#define USBRF_CONI              U32_BIT(0)
#define USBRF_DISI              U32_BIT(1)
#define USBRF_UWRESI            U32_BIT(2)
#define USBRF_UHRESI            U32_BIT(3)
#define USBRF_U3ENTI            U32_BIT(4)
#define USBRF_U3EXTI            U32_BIT(5)
#define USBRF_U2ENTI            U32_BIT(6)
#define USBRF_U2EXTI            U32_BIT(7)
#define USBRF_U1ENTI            U32_BIT(8)
#define USBRF_U1EXTI            U32_BIT(9)
#define USBRF_ITPI              U32_BIT(10)
#define USBRF_WAKEI             U32_BIT(11)
#define USBRF_CON2I             U32_BIT(16)
#define USBRF_DIS2I             U32_BIT(17)
#define USBRF_UHSFSRESI         U32_BIT(18)
#define USBRF_L2ENTI            U32_BIT(20)
#define USBRF_L2EXTI            U32_BIT(21)
#define USBRF_L1ENTI            U32_BIT(24)
#define USBRF_L1EXTI            U32_BIT(25)

#define USBRF_UNKNOWN_INT \
	(!(USBRF_CONI | USBRF_DISI \
	| USBRF_UWRESI | USBRF_UHRESI \
	| USBRF_U3ENTI | USBRF_U3EXTI \
	| USBRF_U2ENTI | USBRF_U2EXTI \
	| USBRF_U1ENTI | USBRF_U1EXTI \
	| USBRF_ITPI   | USBRF_CON2I \
	| USBRF_WAKEI \
	| USBRF_L2ENTI | USBRF_L2EXTI \
	| USBRF_L1ENTI | USBRF_L1EXTI \
	| USBRF_DIS2I  | USBRF_UHSFSRESI))

/*-----------------------------------------------------------------------------*/
/* EP_IEN - Endpoints Interrupt Enable Register*/
/*-----------------------------------------------------------------------------*/

/* Defines*/
#define USBRD_EP_INTERRUPT_EN(EP_NUM_, EP_DIR_) \
	(1ul << (EP_NUM_) << ((EP_DIR_) ? 16 : 0))

/*-----------------------------------------------------------------------------*/
/* EP_ISTS - Endpoints Interrupt Status Register*/
/*-----------------------------------------------------------------------------*/

/* Defines*/
#define USBRD_GET_EP_INTERRUPT_STS(EP_ISTS_REG_, EP_NUM_, EP_DIR_) \
	(((1ul << (EP_NUM_) << ((EP_DIR_) ? 16 : 0)) & (EP_ISTS_REG_)) ? 1u : 0u)

/* Defines for software*/

/* Defines*/
#define USBD_EPID_FROM_EPADDR(EPADDR_) \
	((((EPADDR_) >> 7) & 0x01) | (((EPADDR_) << 1) & 0x1E))

#define USBD_EPADDR_FROM_EPID(EPID_) \
	((((EPID_) >> 1) & 0x0F) | (((EPID_) << 7) & 0x80))

#define USBD_GET_EPID(EPNUM_, EPDIR_) \
	((((EPNUM_) << 1) & 0x1E) | ((EPDIR_) & 0x01))

#define USBD_EPNUM_FROM_EPID(EPID_) \
	((((EPID_) >> 1) & 0x0F))

#define USBD_EPDIR_FROM_EPID(EPID_) \
	((EPID_) & 0x01)

#define USBD_EPNUM_FROM_EPADDR(EPADDR_) \
	((EPADDR_) & 0x0F)

#define USBD_EPDIR_FROM_EPADDR(EPADDR_) \
	((EPADDR_) >> 7 & 0x01)

#ifdef __cplusplus
}
#endif

#endif	/* SS_DEV_HW_H */

