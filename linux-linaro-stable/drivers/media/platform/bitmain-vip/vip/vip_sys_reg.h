#ifndef _VIP_SYS_REG_H_
#define _VIP_SYS_REG_H_

#if defined(ENV_BMTEST) || defined(ENV_EMU)
#define REG_VIP_SYS_BASE 0x0A0C8000
#else
#define REG_VIP_SYS_BASE 0
#endif

#define BIT_VIP_IMG_CLK(x) (2+x)    // 2 instance
#define BIT_VIP_SCL_CLK(x) (5+x)    // 4 instance
#define BIT_VIP_DISP_CLK(x) (9)
#define BIT_VIP_DWA_CLK 14
#define BIT_VIP_ROT_CLK 15

#define BIT_VIP_IMG_RESET(x) (2+x)  // 2 instance
#define BIT_VIP_SCL_TOP_RESET(x) 4
#define BIT_VIP_SCL_RESET(x) (5+x)  // 4 instance
#define BIT_VIP_DWA_RESET 14
#define BIT_VIP_ROT_RESET 14

#define BIT_VIP_DWA_INTR 24
#define BIT_VIP_ROT_INTR 28

#define REG_VIP_RESETS (REG_VIP_SYS_BASE + 0x00)
//#define REG_VIP_ENABLE (REG_VIP_SYS_BASE + 0x04)
#define REG_VIP_INT_STS (REG_VIP_SYS_BASE + 0x08)
#define REG_VIP_INT_EN (REG_VIP_SYS_BASE + 0x0c)
#define REG_VIP_CLK_LP (REG_VIP_SYS_BASE + 0x14)
#define REG_VIP_CLK_CTRL0 (REG_VIP_SYS_BASE + 0x18)
#define REG_VIP_CLK_CTRL1 (REG_VIP_SYS_BASE + 0x1c)

#endif
