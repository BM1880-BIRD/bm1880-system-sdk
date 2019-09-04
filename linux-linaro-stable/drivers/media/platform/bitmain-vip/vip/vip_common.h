#ifndef _COMMON_VIP_H_
#define _COMMON_VIP_H_

#include "cmdq.h"

#define BASE_OFFSET 0x000000000
#define GOP_FRAME_OFFSET 0x200000

#define MIN(a, b) (((a) < (b))?(a):(b))
#define MAX(a, b) (((a) > (b))?(a):(b))
#define CLIP(x, min, max) MAX(MIN(x, max), min)
#define VIP_ALIGN(x) (((x) + 0x1F) & ~0x1F)   // for 32byte alignment
#define UPPER(x, y) (((x) + ((1 << (y)) - 1)) >> (y))   // for alignment

#if defined(ENV_BMTEST) || defined(ENV_EMU)
#define ARRAY_SIZE(x)	(sizeof(x) / sizeof((x)[0]))
#define BIT(nr)         (UINT32_C(1) << (nr))
#endif

#define R_IDX 0
#define G_IDX 1
#define B_IDX 2

union vip_sys_reset {
	struct {
		u32 axi : 1;
		u32 isp_top : 1;
		u32 img_d : 1;
		u32 img_v : 1;
		u32 sc_top : 1;
		u32 sc_d : 1;
		u32 sc_v1 : 1;
		u32 sc_v2 : 1;
		u32 sc_v3 : 1;
		u32 disp : 1;
		u32 bt : 1;
		u32 dsi_mac : 1;
		u32 csi_mac0 : 1;
		u32 csi_mac1 : 1;
		u32 dwa : 1;
		u32 clk_div : 1;
		u32 rsv : 16;
	} b;
	u32 raw;
};

union vip_sys_clk {
	struct {
		u32 sc_top : 1;
		u32 isp_top : 1;
		u32 dwa : 1;
		u32 rot_top : 1;
		u32 vip_sys : 1;
		u32 rsv1 : 3;
		u32 csi_mac0 : 1;
		u32 csi_mac1 : 1;
		u32 rsv2 : 6;
		u32 auto_sc_top : 1;
		u32 auto_isp_top : 1;
		u32 auto_dwa : 1;
		u32 auto_rot_top : 1;
		u32 auto_vip_sys : 1;
		u32 rsv3 : 3;
		u32 auto_csi_mac0 : 1;
		u32 auto_csi_mac1 : 1;
	} b;
	u32 raw;
};

union vip_sys_intr {
	struct {
		u32 sc : 1;
		u32 rsv1 : 15;
		u32 isp : 1;
		u32 rsv2 : 7;
		u32 dwa : 1;
		u32 rsv3 : 3;
		u32 rot : 1;
		u32 csi_mac0 : 1;
		u32 csi_mac1 : 1;
	} b;
	u32 raw;
};

/********************************************************************
 *   APIs to replace bmtest's standard APIs
 ********************************************************************/
#if defined(ENV_BMTEST) || defined(ENV_EMU)
extern int cli_readline(char *prompt, char *cmdbuf);
#define scanf bm_scanf
#define uart_getc bm_getc
#undef uartlog
#define uartlog bm_uartlog
#define bm_get_num(str, var) \
	{ \
		uartlog(str); \
		scanf("%d", &tmp); \
		var = tmp; \
	}

void bm_scanf(const char *fmt, ...);
int bm_getc(void);
void bm_uartlog(const char *fmt, ...);
#endif
u8 generate_cmdset(union cmdq_set *set, u32 addr, u8 num, u32 pattern, u8 op,
		   bool is_end);
int reg_compare(u32 addr, u8 count, u32 pattern);

void vip_set_base_addr(void *base);
union vip_sys_clk vip_get_clk(void);
void vip_set_clk(union vip_sys_clk clk);
union vip_sys_reset vip_get_reset(void);
void vip_set_reset(union vip_sys_reset reset);
union vip_sys_intr vip_get_intr_status(void);
union vip_sys_intr vip_get_intr_mask(void);
void vip_set_intr_mask(union vip_sys_intr intr);

#endif
