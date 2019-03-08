/**********************************************************************
 * Copyright (C) 2014-2017 Cadence Design Systems, Inc.
 * All rights reserved worldwide.
 ***********************************************************************
 * sduc_regs.h
 * Header file describing USB registers for SD USB controllers
 *
 ***********************************************************************/

#ifndef _SDUC_REGS_
#define _SDUC_REGS_

/** Decoding naming convenction
 *  UCRM - mask used for HOST and Device mode
 *  UCRO - offset used for HOST and Device mode
 *  UCRV - value used for HOST and Device mode
 *  UCDRM - mask can be used only for Device mode
 *  UCDRO - offset can be used only for Device mode
 *  UCDRV - value  can be used only for Device mode
 */
/*************************************************************************************************************/

/* Bit definitions for USB special function registers */
/* Register lpmctrll, lpmctrlh  */
#define UCRM_LPMCTRLLL_HIRD         0xF0 /*Host Initiated Resume Duration mask*/
#define UCRO_LPMCTRLLL_HIRD         0x04 /*Host Initiated Resume Duration offset*/
#define UCRM_LPMCTRLLH_BREMOTEWAKEUP 0x01 /*LPM bRemoteWakeup register*/
/*It reflects value of the lpmnyet bit located in the usbcs(1) register. Only peripheral*/
#define UCDRM_LPMCTRLLH_LPMNYET     0x80
/*  Register ep0cs(hcep0cs) */
#define UCDM_EP0CS_CHGSET   0x80 /* Setup Buffer contents was changed */
#define UCDM_EP0CS_DSTALL   0x10 /* Send stall in the data stage*/
#define UCDM_EP0CS_HSNAK    0x02 /* If 0 the controller send status stage */
#define UCDM_EP0CS_STALL    0x01 /* Endpoint 0 stall bit*/
#define UCM_EP0CS_RXBSY_MSK 0x08 /* endpoint busy bit for transmitting data */
#define UCM_EP0CS_TXBSY_MSK 0x04 /* endpoint busy bit receiving data */

/** bit definitions for outxcs, hcinxcs registers */
#define UCM_CS_ERR  0x01 /* Error bit */
#define UCM_CS_BUSY 0x02 /* Busy bit */
#define UCM_CS_NPAK 0x0C /* Npak bits mask */
#define UCO_CS_NPAK 0x02 /* Npak bits offset */
#define UCM_CS_AUTO 0x10 /**< Auto-OUT bit, device mode */

/**< definitions for outxcon, hcinxcon registers */
#define UCM_CON_TYPE_ISOC   0x04 /*Type1, type0 = "01" - isochronous endpoint*/
#define UCM_CON_TYPE_BULK   0x08 /*Type1, type0 = "10" - bulk endpoint*/
#define UCM_CON_TYPE_INT    0x0C /*Type1, type0 = "11" - interrupt endpoint*/
#define UCM_CON_ISOD_AUTO   0x30
#define UCM_CON_1_ISOD      0x00 /*hcinisod1, hcinisod0="00" - 1 ISO packet per microframe*/
#define UCM_CON_2_ISOD      0x10 /*hcinisod1, hcinisod0="01" - 2 ISO packets per microframe*/
#define UCM_CON_3_ISOD      0x20 /*hcinisod1, hcinisod0="10" - 3 ISO packets per microframe*/
#define UCDM_CON_STALL      0x40 /*endpoint stall bit.*/
#define UCM_CON_VAL         0x80 /*endpoint valid bit. Disable(0) or Enable(1) endpoint*/

#define UCM_CON_TYPE        0x0c /*mask for bits type1 and type2 */
#define UCM_CON_ISOD        0x30 /*mask for bits  isod1 nad isod2*/

#define UCM_CON_BUF_SINGLE  0x00
#define UCM_CON_BUF_DOUBLE  0x01
#define UCM_CON_BUF_TRIPLE  0x02
#define UCM_CON_BUF_QUAD    0x03
#define UCM_CON_BUF         0x03

/* bit definitions for usbirq, hcusbirq registers **/
#define UCM_USBIR_SOF       0x02 /* Start-of-frame */
#define UCM_USBIR_SUSP      0x08 /* USB suspend interrupt request*/
#define UCM_USBIR_URES      0x10 /* USB reset interrupt request*/
#define UCM_USBIR_HSPPED    0x20 /* USB high-speed mode interrupt request*/

#define UCDM_USBIR_SUDAV    0x01 /* SETUP data valid interrupt request */
#define UCDM_USBIR_SUTOK    0x04 /* SETUP token interrupt request*/
#define UCDM_USBIR_LPMIR    0x80 /* Link Power Management interrupt request*/
/* bit definitions for  usbien, hcusbien registers **/
#define UCM_USBIEN_SOFIE    0x02 /* Start-of-frame interrupt enable*/
#define UCM_USBIEN_URESIE   0x10 /* USB reset interrupt enable*/
#define UCM_USBIEN_HSPIE    0x20 /* USB high speed mode interrupt enable*/

#define UCDM_USBIEN_SUDAVIE 0x01 /* SETUP data valid interrupt enable*/
#define UCDM_USBIEN_SUTOKIE 0x04 /* SETUP token interrupt enable */
#define UCDM_USBIEN_SUSPIE  0x08 /* USB suspend interrupt enable */
#define UCDM_USBIEN_LPMIE   0x80 /* Link Power Management interrupt request */

/* bit definitions for endprst, hcendprst registers*/
#define UCM_ENDPRST_EP      0x0f /* Endpoint number mask */
#define UCM_ENDPRST_IO_TX   0x10 /*  Direction bit for transmitting data */
#define UCM_ENDPRST_TOGRST  0x20 /* Toggle reset bit */
#define UCM_ENDPRST_FIFORST 0x40 /* Fifo reset bit */
#define UCM_ENDPRST_TOGSETQ 0x80 /*  Read access: Data toggle value, Write access: Toggle set bit */

/* bit definitions for usbcs, hcusbcs registers **/
#define UCDM_USBCS_LPMNYET  0x02 /* Send NYET handshake for the LPM transaction*/
#define UCDM_USBCS_SLFPWR   0x04 /* Self powered device status bit*/
#define UCDM_USBCS_RWAKEN   0x08 /* Remote wakeup enable bit */
#define UCDM_USBCS_ENUM     0x08 /* Enumeration bit */
#define UCM_USBCS_SIGRSUME  0x20 /* Remote wakeup bit */
#define UCM_USBCS_DISCON    0x40 /* Software disconnect bit */
#define UCM_USBCS_WAKESRC   0x80 /* Wakeup source*/

/* bit definitions for clkgate, hcclkgate registers*/
#define UCH_CLKGATE_WUIDEN_MSK   0x01 /* Wakeup ID enable */
#define UCH_CLKGATE_WUDPEN_MSK   0x02 /* Wakeup D+ enable */
#define UCH_CLKGATE_WUVBUSEN_MSK 0x04 /* Wakeup VBUS enable */

/* bit definitions for fifoctrl, hcfifoctrl registers*/
#define UCM_FIFOCTRL_EP         0x0f /* Endpoint number mask */
#define UCM_FIFOCTRL_IO_TX      0x10 /* Direction bit for transmitting data */
#define UCM_FIFOCTRL_FIFOAUTO   0x20 /* FIFO auto bit */
#define UCM_FIFOCTRL_FIFOCMIT   0x40 /* FIFO commit bit */
#define UCM_FIFOCTRL_FIFOACC    0x80 /*  FIFO access bit */

/*Below macro describe speedctrl register field*/
#define UCM_SPEEDCTRL_LS        0x01 /*Host works in Low Speed*/
#define UCM_SPEEDCTRL_FS        0x02 /*Host works in Full Speed*/
#define UCM_SPEEDCTRL_HS        0x04 /*Host works in High Speed*/
#define UCM_SPEEDCTRL_HSDISABLE 0x80 /*If set to 1 then CUSB Controller works only in FS/LS*/

#define UCRM_WA1_CNT 0x0F /*PHY workaround 1 counter register*/

#define UCRM_ENDIAN_SFR_CS_LENDIAN 0x01 /*set Little Endian order for SFR*/
#define UCRM_ENDIAN_SFR_CS_BENDIAN 0x02 /*set Big endian order for SFR*/
#define UCRM_ENDIAN_SFR_CS_ENDIAN  0x80 /*Current endian order*/
/*---------------------------------------------------------------------------*/

struct UCRegs {
	uint8_t ep0Rxbc;        /*address 0x00*/
	uint8_t ep0Txbc;        /*address 0x01*/
	uint8_t ep0cs;          /*address 0x02*/
	int8_t reserved0;       /*address 0x03*/
	uint8_t lpmctrll;       /*address 0x04*/
	uint8_t lpmctrlh;       /*address 0x04*/
	uint8_t lpmclock;
	uint8_t ep0fifoctrl;
	struct ep {             /*address 0x08*/
		uint16_t rxbc;        /*outbc (hcinbc)*/
		uint8_t rxcon;
		uint8_t rxcs;
		uint16_t txbc;         /*inbc  (hcoutbc*/
		uint8_t txcon;
		uint8_t txcs;
	} ep[15];
	uint8_t reserved1[4];
	uint32_t fifodat[15];   /*address 0x84*/
	uint8_t reserved3[64];
	uint8_t ep0datatx[64]; /*address 0x100*/
	uint8_t ep0datarx[64]; /*address 0x140*/
	uint8_t setupdat[8];    /*address 0x180*/
	uint16_t txirq;         /*address 0x188*/
	uint16_t rxirq;         /*address 0x18A*/
	uint8_t usbirq;         /*address 0x18C*/
	uint8_t reserved4;
	uint16_t rxpngirq;      /*address 0x18E*/
	uint16_t txfullirq;     /*address 0x190*/
	uint16_t rxemptirq;     /*address 0x192*/
	uint16_t txien;         /*address 0x194*/
	uint16_t rxien;         /*address 0x196*/
	uint8_t usbien;         /*address 0x198*/
	uint8_t reserved6;
	uint16_t rxpngien;      /*address 0x19A*/
	uint16_t txfullien;     /*address 0x19C*/
	uint16_t rxemptien;     /*address 0x19E*/
	uint8_t usbivect;       /*address 0x1A0*/
	uint8_t fifoivect;      /*address 0x1A1*/
	uint8_t endprst;        /*address 0x1A2*/
	uint8_t usbcs;          /*address 0x1A3*/
	uint16_t frmnr;         /*address 0x1A4*/
	uint8_t fnaddr;         /*address 0x1A6*/
	uint8_t clkgate;        /*address 0x1A7*/
	uint8_t fifoctrl;       /*address 0x1A8*/
	uint8_t speedctrl;      /*address 0x1A9*/
	uint8_t reserved7[30];
	uint8_t traddr;         /*address 0x1C8*/
	uint8_t trwdata;        /*address 0x1C9*/
	uint8_t trrdata;        /*address 0x1CA*/
	uint8_t trctrl;         /*address 0x1CB*/
	uint16_t isoautoarm;    /*address 0x1CC*/
	uint8_t adpbc1ien;      /*address 0x1CE*/
	uint8_t adpbc2ien;      /*address 0x1CF*/
	uint8_t adpbcctr0;      /*address 0x1D0*/
	uint8_t adpbcctr1;      /*address 0x1D1*/
	uint8_t adpbcctr2;      /*address 0x1D2*/
	uint8_t adpbc1irq;      /*address 0x1D3*/
	uint8_t adpbc0status;   /*address 0x1D4*/
	uint8_t adpbc1status;   /*address 0x1D5*/
	uint8_t adpbc2status;   /*address 0x1D6*/
	uint8_t adpbc2irq;      /*address 0x1D7*/
	uint16_t isodctrl;      /*address 0x1D8*/
	uint8_t reserved11[2];
	uint16_t isoautodump;   /*address 0x1DC*/
	uint8_t reserved12[2];
	uint8_t ep0maxpack;     /*address 0x1E0*/
	uint8_t reserved13;
	uint16_t rxmaxpack[15]; /*address 0x1E2*/
	uint8_t reserved14[260];
	struct rxstaddr {       /*address 0x304*/
		uint16_t addr;
		uint16_t reserved;
	} rxstaddr[15];
	uint8_t reserved15[4];
	struct txstaddr {       /*address 0x344*/
		uint16_t addr;
		uint16_t reserved;
	} txstaddr[15];
	int8_t reserved16[64];
	/*The Microprocessor control*/
	uint8_t cpuctrl;         /*address 0x3C0*/
	int8_t reserved17[15];
	/*The debug counters and workarounds*/
	uint8_t debug_rx_bcl;    /*address 0x3D0*/
	uint8_t debug_rx_bch;    /*address 0x3D1*/
	uint8_t debug_rx_status; /*address 0x3D2*/
	uint8_t debug_irq;       /*address 0x3D3*/
	uint8_t debug_tx_bcl;    /*address 0x3D4*/
	uint8_t debug_tx_bch;    /*address 0x3D5*/
	uint8_t debug_tx_status; /*address 0x3D6*/
	uint8_t debug_ien;       /*address 0x3D7*/
	uint8_t phywa_en;        /*address 0x3D8*/
	/*endian*/
	uint8_t wa1_cnt;       /*address 0x3D9*/
	int8_t reserved18[2];  /*address 0x3DA*/
	uint8_t endian_sfr_cs; /*address 0x3DC*/
	int8_t reserved19[2];    /*address 0x3DD*/
	uint8_t endian_sfr_s;  /*address 0x3DF*/
	int8_t reserved20[2];    /*address 0x3E0*/
	uint16_t txmaxpack[15]; /*address 0x3E2*/
} __packed;

/*************************************************************/
#endif /*end _SDUC_REGS_*/

