#ifdef ENV_BMTEST
#include <common.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include "system_common.h"
#elif defined(ENV_EMU)
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "emu/command.h"
#else
#include <linux/types.h>
#include <linux/delay.h>
#endif  // ENU_BMTEST

#include "vip_common.h"
#include "scaler.h"
#include "disp_timing.h"
#include "vip_sys_reg.h"
#include "scaler_reg.h"
#include "reg.h"
#include "cmdq.h"

/****************************************************************************
 * Global parameters
 ****************************************************************************/
static struct sclr_top_cfg g_top_cfg;
static struct sclr_core_cfg g_sc_cfg[SCL_MAX_INST];
static struct sclr_gop_cfg g_gop_cfg[SCL_MAX_INST];
static struct sclr_img_cfg g_img_cfg[2];
static struct sclr_cir_cfg g_cir_cfg[SCL_MAX_INST];
static struct sclr_border_cfg g_bd_cfg[SCL_MAX_INST];
static struct sclr_odma_cfg g_odma_cfg[SCL_MAX_INST];
static struct sclr_disp_cfg g_disp_cfg;
static struct sclr_disp_timing disp_timing;
static uintptr_t reg_base;

/****************************************************************************
 * Initial info
 ****************************************************************************/
#define DEFINE_CSC_COEF0(a, b, c) \
		.coef[0][0] = a, .coef[0][1] = b, .coef[0][2] = c,
#define DEFINE_CSC_COEF1(a, b, c) \
		.coef[1][0] = a, .coef[1][1] = b, .coef[1][2] = c,
#define DEFINE_CSC_COEF2(a, b, c) \
		.coef[2][0] = a, .coef[2][1] = b, .coef[2][2] = c,
static struct sclr_csc_cfg csc_mtrx[SCL_CSC_MAX] = {
	// none
	{
		DEFINE_CSC_COEF0(BIT(10),	0,		0)
		DEFINE_CSC_COEF1(0,		BIT(10),	0)
		DEFINE_CSC_COEF2(0,		0,		BIT(10))
		.sub[0] = 0,   .sub[1] = 0,   .sub[2] = 0,
		.add[0] = 0,   .add[1] = 0,   .add[2] = 0
	},
	// yuv2rgb
	// 601 Limited
	//  R = Y + 1.402* Pr                           //
	//  G = Y - 0.344 * Pb  - 0.792* Pr             //
	//  B = Y + 1.772 * Pb                          //
	{
		DEFINE_CSC_COEF0(BIT(10),	0,		1436)
		DEFINE_CSC_COEF1(BIT(10),	BIT(13) | 352,	BIT(13) | 731)
		DEFINE_CSC_COEF2(BIT(10),	1815,		0)
		.sub[0] = 0,   .sub[1] = 128, .sub[2] = 128,
		.add[0] = 0,   .add[1] = 0,   .add[2] = 0
	},
	// 601 Full
	//  R = 1.164 *(Y - 16) + 1.596 *(Cr - 128)                     //
	//  G = 1.164 *(Y - 16) - 0.392 *(Cb - 128) - 0.812 *(Cr - 128) //
	//  B = 1.164 *(Y - 16) + 2.016 *(Cb - 128)                     //
	{
		DEFINE_CSC_COEF0(1192,	0,		1634)
		DEFINE_CSC_COEF1(1192,	BIT(13) | 401,	BIT(13) | 833)
		DEFINE_CSC_COEF2(1192,	2065,		0)
		.sub[0] = 16,  .sub[1] = 128, .sub[2] = 128,
		.add[0] = 0,   .add[1] = 0,   .add[2] = 0
	},
	// 709 Limited
	// R = Y + 1.540(Cr – 128)
	// G = Y - 0.183(Cb – 128) – 0.459(Cr – 128)
	// B = Y + 1.816(Cb – 128)
	{
		DEFINE_CSC_COEF0(BIT(10),	0,		1577)
		DEFINE_CSC_COEF1(BIT(10),	BIT(13) | 187,	BIT(13) | 470)
		DEFINE_CSC_COEF2(BIT(10),	1860,		0)
		.sub[0] = 0,   .sub[1] = 128, .sub[2] = 128,
		.add[0] = 0,   .add[1] = 0,   .add[2] = 0
	},
	// 709 Full
	//  R = 1.164 *(Y - 16) + 1.792 *(Cr - 128)                     //
	//  G = 1.164 *(Y - 16) - 0.213 *(Cb - 128) - 0.534 *(Cr - 128) //
	//  B = 1.164 *(Y - 16) + 2.114 *(Cb - 128)                     //
	{
		DEFINE_CSC_COEF0(1192,	0,		1836)
		DEFINE_CSC_COEF1(1192,	BIT(13) | 218,	BIT(13) | 547)
		DEFINE_CSC_COEF2(1192,	2166,		0)
		.sub[0] = 16,  .sub[1] = 128, .sub[2] = 128,
		.add[0] = 0,   .add[1] = 0,   .add[2] = 0
	},
	// rgb2yuv
	// 601 Limited
	//  Y = 0.299 * R + 0.587 * G + 0.114 * B       //
	// Pb =-0.169 * R - 0.331 * G + 0.500 * B       //
	// Pr = 0.500 * R - 0.439 * G - 0.081 * B       //
	{
		DEFINE_CSC_COEF0(306,		601,		117)
		DEFINE_CSC_COEF1(BIT(13)|173,	BIT(13)|339,	512)
		DEFINE_CSC_COEF2(512,		BIT(13)|429,	BIT(13)|83)
		.sub[0] = 0,   .sub[1] = 0,   .sub[2] = 0,
		.add[0] = 0,   .add[1] = 128, .add[2] = 128
	},
	// 601 Full
	//  Y = 16  + 0.257 * R + 0.504 * g + 0.098 * b //
	// Cb = 128 - 0.148 * R - 0.291 * g + 0.439 * b //
	// Cr = 128 + 0.439 * R - 0.368 * g - 0.071 * b //
	{
		DEFINE_CSC_COEF0(263,		516,		100)
		DEFINE_CSC_COEF1(BIT(13)|152,	BIT(13)|298,	450)
		DEFINE_CSC_COEF2(450,		BIT(13)|377,	BIT(13)|73)
		.sub[0] = 0,   .sub[1] = 0,   .sub[2] = 0,
		.add[0] = 16,  .add[1] = 128, .add[2] = 128
	},
	// 709 Limited
	//   Y =       0.2126   0.7152   0.0722
	//  Cb = 128 - 0.1146  -0.3854   0.5000
	//  Cr = 128 + 0.5000  -0.4542  -0.0468
	{
		DEFINE_CSC_COEF0(218,		732,		74)
		DEFINE_CSC_COEF1(BIT(13)|117,	BIT(13)|395,	512)
		DEFINE_CSC_COEF2(512,		BIT(13)|465,	BIT(13)|48)
		.sub[0] = 0,   .sub[1] = 0,   .sub[2] = 0,
		.add[0] = 0,   .add[1] = 128, .add[2] = 128
	},
	// 709 Full
	//  Y = 16  + 0.183 * R + 0.614 * g + 0.062 * b //
	// Cb = 128 - 0.101 * R - 0.339 * g + 0.439 * b //
	// Cr = 128 + 0.439 * R - 0.399 * g - 0.040 * b //
	{
		DEFINE_CSC_COEF0(187,		629,		63)
		DEFINE_CSC_COEF1(BIT(13)|103,	BIT(13)|347,	450)
		DEFINE_CSC_COEF2(450,		BIT(13)|408,	BIT(13)|41)
		.sub[0] = 0,   .sub[1] = 0,   .sub[2] = 0,
		.add[0] = 16,  .add[1] = 128, .add[2] = 128
	},
};

static int scl_coef[128][4] = {
	{-4,    1024,   4,  0},
	{-8,    1024,   8,  0},
	{-12,   1024,   12, 0},
	{-16,   1024,   16, 0},
	{-19,   1023,   20, 0},
	{-23,   1023,   25, -1},
	{-26,   1022,   29, -1},
	{-30,   1022,   33, -1},
	{-34,   1022,   37, -1},
	{-37,   1021,   42, -2},
	{-40,   1020,   46, -2},
	{-44,   1020,   50, -2},
	{-47,   1019,   55, -3},
	{-50,   1018,   59, -3},
	{-53,   1017,   63, -3},
	{-56,   1016,   68, -4},
	{-59,   1015,   72, -4},
	{-62,   1014,   77, -5},
	{-65,   1013,   81, -5},
	{-68,   1012,   86, -6},
	{-71,   1011,   90, -6},
	{-74,   1010,   95, -7},
	{-76,   1008,   100,    -8},
	{-79,   1007,   104,    -8},
	{-81,   1005,   109,    -9},
	{-84,   1004,   113,    -9},
	{-86,   1002,   118,    -10},
	{-89,   1001,   123,    -11},
	{-91,   999,    128,    -12},
	{-94,   998,    132,    -12},
	{-96,   996,    137,    -13},
	{-98,   994,    142,    -14},
	{-100,  992,    147,    -15},
	{-102,  990,    152,    -16},
	{-104,  988,    157,    -17},
	{-106,  986,    161,    -17},
	{-108,  984,    166,    -18},
	{-110,  982,    171,    -19},
	{-112,  980,    176,    -20},
	{-114,  978,    181,    -21},
	{-116,  976,    186,    -22},
	{-117,  973,    191,    -23},
	{-119,  971,    196,    -24},
	{-121,  969,    201,    -25},
	{-122,  966,    206,    -26},
	{-124,  964,    211,    -27},
	{-125,  961,    216,    -28},
	{-127,  959,    221,    -29},
	{-128,  956,    226,    -30},
	{-130,  954,    231,    -31},
	{-131,  951,    237,    -33},
	{-132,  948,    242,    -34},
	{-133,  945,    247,    -35},
	{-134,  942,    252,    -36},
	{-136,  940,    257,    -37},
	{-137,  937,    262,    -38},
	{-138,  934,    267,    -39},
	{-139,  931,    273,    -41},
	{-140,  928,    278,    -42},
	{-141,  925,    283,    -43},
	{-142,  922,    288,    -44},
	{-142,  918,    294,    -46},
	{-143,  915,    299,    -47},
	{-144,  912,    304,    -48},
	{-145,  909,    309,    -49},
	{-145,  905,    315,    -51},
	{-146,  902,    320,    -52},
	{-147,  899,    325,    -53},
	{-147,  895,    330,    -54},
	{-148,  892,    336,    -56},
	{-148,  888,    341,    -57},
	{-149,  885,    346,    -58},
	{-149,  881,    352,    -60},
	{-150,  878,    357,    -61},
	{-150,  874,    362,    -62},
	{-150,  870,    367,    -63},
	{-151,  867,    373,    -65},
	{-151,  863,    378,    -66},
	{-151,  859,    383,    -67},
	{-151,  855,    389,    -69},
	{-151,  851,    394,    -70},
	{-152,  848,    399,    -71},
	{-152,  844,    405,    -73},
	{-152,  840,    410,    -74},
	{-152,  836,    415,    -75},
	{-152,  832,    421,    -77},
	{-152,  828,    426,    -78},
	{-152,  824,    431,    -79},
	{-151,  819,    437,    -81},
	{-151,  815,    442,    -82},
	{-151,  811,    447,    -83},
	{-151,  807,    453,    -85},
	{-151,  803,    458,    -86},
	{-151,  799,    463,    -87},
	{-150,  794,    469,    -89},
	{-150,  790,    474,    -90},
	{-150,  786,    479,    -91},
	{-149,  781,    485,    -93},
	{-149,  777,    490,    -94},
	{-149,  773,    495,    -95},
	{-148,  768,    501,    -97},
	{-148,  764,    506,    -98},
	{-147,  759,    511,    -99},
	{-147,  755,    516,    -100},
	{-146,  750,    522,    -102},
	{-146,  746,    527,    -103},
	{-145,  741,    532,    -104},
	{-144,  736,    537,    -105},
	{-144,  732,    543,    -107},
	{-143,  727,    548,    -108},
	{-142,  722,    553,    -109},
	{-142,  718,    558,    -110},
	{-141,  713,    563,    -111},
	{-140,  708,    569,    -113},
	{-140,  704,    574,    -114},
	{-139,  699,    579,    -115},
	{-138,  694,    584,    -116},
	{-137,  689,    589,    -117},
	{-136,  684,    594,    -118},
	{-135,  679,    600,    -120},
	{-135,  675,    605,    -121},
	{-134,  670,    610,    -122},
	{-133,  665,    615,    -123},
	{-132,  660,    620,    -124},
	{-131,  655,    625,    -125},
	{-130,  650,    630,    -126},
	{-129,  645,    635,    -127},
	{-128,  640,    640,    -128},
};

/****************************************************************************
 * Interfaces
 ****************************************************************************/
void sclr_set_base_addr(void *base)
{
	reg_base = (uintptr_t)base;
}

/**
 * sclr_top_set_cfg - set scl-top's configurations.
 *
 * @param cfg: scl-top's settings.
 */
void sclr_top_set_cfg(struct sclr_top_cfg *cfg)
{
	u32 sc_top = 0;
	u32 img_ctrl = 0;
	union vip_sys_clk clk;

	_reg_write(reg_base + REG_SCL_TOP_CFG0, cfg->ip_trig_src << 3);

	clk = vip_get_clk();
	clk.b.sc_top = 1;
	vip_set_clk(clk);

	if (cfg->sclr_enable[0])
		sc_top |= BIT(0);
	if (cfg->sclr_enable[1])
		sc_top |= BIT(1);
	if (cfg->sclr_enable[2])
		sc_top |= BIT(2);
	if (cfg->sclr_enable[3])
		sc_top |= BIT(3);
	if (cfg->disp_enable)
		sc_top |= BIT(4);
	if (cfg->disp_from_sc)
		sc_top |= BIT(8);
	if (cfg->sclr_d_src)
		sc_top |= BIT(9);
	if (cfg->img_in_d_trig_src)
		img_ctrl |= BIT(8);
	if (cfg->img_in_v_trig_src)
		img_ctrl |= BIT(9);

	_reg_write(reg_base + REG_SCL_TOP_CFG1, sc_top);
	_reg_write(reg_base + REG_SCL_TOP_IMG_CTRL, img_ctrl);

	sclr_top_reg_done();
	sclr_top_reg_force_up();
}

/**
 * sclr_top_get_cfg - get scl_top's cfg
 *
 * @return: scl_top's cfg
 */
struct sclr_top_cfg *sclr_top_get_cfg(void)
{
	return &g_top_cfg;
}

/**
 * sclr_top_reg_done - to mark all sc-reg valid for update.
 *
 */
void sclr_top_reg_done(void)
{
	_reg_write_mask(reg_base + REG_SCL_TOP_CFG0, 0, 1);
}

/**
 * sclr_top_reg_force_up - trigger reg update by sw.
 *
 */
void sclr_top_reg_force_up(void)
{
	_reg_write_mask(reg_base + REG_SCL_TOP_SHD, 0x00ff, 0xff);
}

u8 sclr_top_pg_late_get_bus(void)
{
	return (_reg_read(reg_base + REG_SCL_TOP_PG) >> 8) & 0xff;
}

void sclr_top_pg_late_clr(void)
{
	_reg_write_mask(reg_base + REG_SCL_TOP_PG, 0x0f0000, 0x80000);
}

/**
 * sclr_init - setup sclr's scaling coef
 *
 * @param inst: (0~3), the instance of scaler which want to be configured.
 */
void sclr_init(u8 inst)
{
	u8 i = 0;

	if (inst >= SCL_MAX_INST) {
		pr_err("[bm-vip][sc] %s: no enough sclr-instance ", __func__);
		pr_cont("for the requirement(%d)\n", inst);
		return;
	}

	for (i = 0; i < 128; ++i) {
		_reg_write(reg_base + REG_SCL_COEF1(inst),
			   (scl_coef[i][1] << 16) |
			   (scl_coef[i][0] & 0x0fff));
		_reg_write(reg_base + REG_SCL_COEF2(inst),
			   (scl_coef[i][3] << 16) |
			   (scl_coef[i][2] & 0x0fff));
		_reg_write(reg_base + REG_SCL_COEF0(inst), (0x5 << 8) | i);
	}

	// enable cb_mode of scaling, since it only works on fac > 4
	// enable mirror
	_reg_write(reg_base + REG_SCL_SC_CFG(inst), 0x13);
}

/**
 * sclr_set_cfg - set scl's configurations.
 *
 * @param inst: (0~3), the instance of scaler which want to be configured.
 * @param sc_bypass: if bypass scaling-engine.
 * @param gop_bypass: if bypass gop-engine.
 * @param dbg_en: if bypass debug.
 * @param cir_bypass: if bypass circle-engine.
 * @param odma_bypass: if bypass odma-engine.
 */
void sclr_set_cfg(u8 inst, bool sc_bypass, bool gop_bypass, bool dbg_en,
		  bool cir_bypass, bool odma_bypass)
{
	u32 tmp = 0;

	if (sc_bypass)
		tmp |= 0x02;
	if (gop_bypass)
		tmp |= 0x04;
	if (dbg_en)
		tmp |= 0x10;
	if (cir_bypass)
		tmp |= 0x20;
	if (odma_bypass)
		tmp |= 0x40;

	_reg_write(reg_base + REG_SCL_CFG(inst), tmp);

	g_sc_cfg[inst].sc_bypass = sc_bypass;
	g_sc_cfg[inst].gop_bypass = gop_bypass;
	g_sc_cfg[inst].dbg_en = dbg_en;
	g_sc_cfg[inst].cir_bypass = cir_bypass;
	g_sc_cfg[inst].odma_bypass = odma_bypass;
}

/**
 * sclr_get_cfg - get scl_core's cfg
 *
 * @param inst: 0~3
 * @return: scl_core's cfg
 */
struct sclr_core_cfg *sclr_get_cfg(u8 inst)
{
	if (inst >= SCL_MAX_INST)
		return NULL;
	return &g_sc_cfg[inst];
}

/**
 * sclr_reg_shadow_sel - control the read reg-bank.
 *
 * @param read_shadow: true(shadow); false(working)
 */
void sclr_reg_shadow_sel(u8 inst, bool read_shadow)
{
	_reg_write(reg_base + REG_SCL_SHD(inst), (read_shadow ? 0x0 : 0x2));
}

/**
 * sclr_reg_force_up - trigger reg update by sw.
 *
 * @param inst: 0~3
 */
void sclr_reg_force_up(u8 inst)
{
	_reg_write_mask(reg_base + REG_SCL_TOP_SHD, BIT(3+inst), BIT(3+inst));
}

struct sclr_status sclr_get_status(u8 inst)
{
	u32 tmp = _reg_read(reg_base + REG_SCL_STATUS(inst));

	return *(struct sclr_status *)&tmp;
}

/**
 * sclr_set_input_size - update scl's input-size.
 *
 * @param inst: 0~3
 * @param src_rect: size of input-src
 */
void sclr_set_input_size(u8 inst, struct sclr_size src_rect)
{
	_reg_write(reg_base + REG_SCL_SRC_SIZE(inst), ((src_rect.h - 1) << 12)
		   | (src_rect.w - 1));

	g_sc_cfg[inst].sc.src = src_rect;
}

/**
 * sclr_set_crop - update scl's crop-size/position.
 *
 * @param inst: 0~3
 * @param crop_rect: to specify pos/size of crop
 */
void sclr_set_crop(u8 inst, struct sclr_rect crop_rect)
{
	_reg_write(reg_base + REG_SCL_CROP_OFFSET(inst),
		   (crop_rect.y << 12) | crop_rect.x);
	_reg_write(reg_base + REG_SCL_CROP_SIZE(inst),
		   ((crop_rect.h - 1) << 16) | (crop_rect.w - 1));

	g_sc_cfg[inst].sc.crop = crop_rect;
}

/**
 * sclr_set_output_size - update scl's output-size.
 *
 * @param inst: 0~3
 * @param src_rect: size of output-src
 */
void sclr_set_output_size(u8 inst, struct sclr_size size)
{
	_reg_write(reg_base + REG_SCL_OUT_SIZE(inst),
		   ((size.h - 1) << 16) | (size.w - 1));

	g_sc_cfg[inst].sc.dst = size;

	if (inst == 0) {
		struct sclr_rect rect;

		rect.x = 0;
		rect.y = 0;
		rect.w = size.w;
		rect.h = size.h;
		sclr_disp_set_rect(rect);
	}
}

/**
 * sclr_set_scale - update scl's scaling settings by current crop/dst values.
 *
 * @param inst: 0~3
 */
void sclr_set_scale(u8 inst)
{
	u32 tmp = 0;

	if (g_sc_cfg[inst].sc.src.w == 0 || g_sc_cfg[inst].sc.src.h == 0)
		return;
	if (g_sc_cfg[inst].sc.crop.w == 0 || g_sc_cfg[inst].sc.crop.h == 0)
		return;
	if (g_sc_cfg[inst].sc.dst.w == 0 || g_sc_cfg[inst].sc.dst.h == 0)
		return;

	tmp = ((g_sc_cfg[inst].sc.crop.w - 1) << 13) /
	      (g_sc_cfg[inst].sc.dst.w - 1);
	_reg_write_mask(reg_base + REG_SCL_SC_H_CFG(inst), 0x03ffff00,
			tmp << 8);
	tmp = ((g_sc_cfg[inst].sc.crop.h - 1) << 13) /
	      (g_sc_cfg[inst].sc.dst.h - 1);
	_reg_write_mask(reg_base + REG_SCL_SC_V_CFG(inst), 0x03ffff00,
			tmp << 8);
}

union sclr_intr sclr_get_intr_mask(void)
{
	union sclr_intr intr_mask;

	intr_mask.raw = _reg_read(reg_base + REG_SCL_TOP_INTR_MASK);
	return intr_mask;
}

/**
 * sclr_set_intr_mask - sclr's interrupt mask. Only enable ones will be
 *			integrated to vip_subsys.  check 'union sclr_intr'
 *			for each bit mask.
 *
 * @param intr_mask: On/Off ctrl of the interrupt.
 */
void sclr_set_intr_mask(union sclr_intr intr_mask)
{
	_reg_write(reg_base + REG_SCL_TOP_INTR_MASK, intr_mask.raw);
}

/**
 * sclr_intr_ctrl - sclr's interrupt on(1)/off(0)
 *                  check 'union sclr_intr' for each bit mask
 *
 * @param intr_mask: On/Off ctrl of the interrupt.
 */
void sclr_intr_ctrl(union sclr_intr intr_mask)
{
	_reg_write(reg_base + REG_SCL_TOP_INTR_STATUS, intr_mask.raw << 16);
}

/**
 * sclr_intr_clr - clear sclr's interrupt
 *                 check 'union sclr_intr' for each bit mask
 *
 * @param intr_mask: On/Off ctrl of the interrupt.
 */
void sclr_intr_clr(union sclr_intr intr_mask)
{
	_reg_write_mask(reg_base + REG_SCL_TOP_INTR_STATUS, 0x0000ffff,
			intr_mask.raw);
}

/**
 * sclr_intr_status - sclr's interrupt status
 *                    check 'union sclr_intr' for each bit mask
 *
 * @return: The interrupt's status
 */
union sclr_intr sclr_intr_status(void)
{
	union sclr_intr status;

	status.raw = (_reg_read(reg_base + REG_SCL_TOP_INTR_STATUS) & 0xffff);
	return status;
}

/****************************************************************************
 * SCALER IMG
 ****************************************************************************/
/**
 * sclr_img_set_cfg - set img's configurations.
 *
 * @param inst: (0~1), the instance of img-in which want to be configured.
 * @param cfg: img's settings.
 */
void sclr_img_set_cfg(u8 inst, struct sclr_img_cfg *cfg)
{
	if (inst == SCL_IMG_D) {
		u32 tmp = (cfg->src == SCL_INPUT_D_IMG_V) ? BIT(9) : 0;

		_reg_write_mask(reg_base + REG_SCL_TOP_CFG1, BIT(9), tmp);
	}

	_reg_write(reg_base + REG_SCL_IMG_CFG(inst),
		   (cfg->burst << 8) | (cfg->fmt << 4) | cfg->src);

	if (cfg->src == SCL_INPUT_D_MEM)
		sclr_img_set_mem(inst, &cfg->mem);

	sclr_img_set_csc(inst, &csc_mtrx[cfg->csc]);
	g_img_cfg[inst] = *cfg;
}

/**
 * sclr_img_get_cfg - get scl_img's cfg
 *
 * @param inst: 0~1
 * @return: scl_img's cfg
 */
struct sclr_img_cfg *sclr_img_get_cfg(u8 inst)
{
	if (inst >= SCL_IMG_MAX)
		return NULL;
	return &g_img_cfg[inst];
}

/**
 * sclr_img_reg_shadow_sel - control scl_img's the read reg-bank.
 *
 * @param read_shadow: true(shadow); false(working)
 */
void sclr_img_reg_shadow_sel(u8 inst, bool read_shadow)
{
	_reg_write(reg_base + REG_SCL_IMG_SHD(inst), (read_shadow ? 0x0 : 0x4));
}

/**
 * sclr_img_reg_shadow_mask - reg won't be update by sw/hw until unmask.
 *
 * @param mask: true(mask); false(unmask)
 * @return: mask status before this modification.
 */
bool sclr_img_reg_shadow_mask(u8 inst, bool mask)
{
	bool is_masked = (_reg_read(reg_base + REG_SCL_IMG_SHD(inst)) & BIT(1));

	if (is_masked != mask)
		_reg_write_mask(reg_base + REG_SCL_IMG_SHD(inst), BIT(1),
				(mask ? 0x0 : BIT(1)));

	return is_masked;
}

/**
 * sclr_img_reg_force_up - trigger reg update by sw.
 *
 * @param inst: 0~1
 */
void sclr_img_reg_force_up(u8 inst)
{
	_reg_write_mask(reg_base + REG_SCL_TOP_SHD, BIT(1+inst), BIT(1+inst));
}

void sclr_img_start(u8 inst)
{
	inst = (~inst) & 0x01;
	_reg_write_mask(reg_base + REG_SCL_TOP_IMG_CTRL, 0x00000003, BIT(inst));
}

/**
 * sclr_img_set_fmt - set scl_img's input data format
 *
 * @param inst: 0~1
 * @param fmt: scl_img's input format
 */
void sclr_img_set_fmt(u8 inst, enum sclr_format fmt)
{
	_reg_write_mask(reg_base + REG_SCL_IMG_CFG(inst), 0x000000f0, fmt << 4);

	g_img_cfg[inst].fmt = fmt;
}

/**
 * sclr_img_set_mem - setup img's mem settings. Only work if img from mem.
 *
 * @param mem: mem settings for img
 */
void sclr_img_set_mem(u8 inst, struct sclr_mem *mem)
{
	_reg_write(reg_base + REG_SCL_IMG_OFFSET(inst),
		   (mem->start_y << 16) | mem->start_x);
	_reg_write(reg_base + REG_SCL_IMG_SIZE(inst),
		   ((mem->height - 1) << 16) | (mem->width - 1));
	_reg_write(reg_base + REG_SCL_IMG_PITCH_Y(inst), mem->pitch_y);
	_reg_write(reg_base + REG_SCL_IMG_PITCH_C(inst), mem->pitch_c);

	sclr_img_set_addr(inst, mem->addr0, mem->addr1, mem->addr2);

	g_img_cfg[inst].mem = *mem;
}

/**
 * sclr_img_set_addr - setup img's mem address. Only work if img from mem.
 *
 * @param addr0: address of planar0
 * @param addr1: address of planar1
 * @param addr2: address of planar2
 */
void sclr_img_set_addr(u8 inst, u64 addr0, u64 addr1, u64 addr2)
{
	_reg_write(reg_base + REG_SCL_IMG_ADDR0_L(inst), addr0);
	_reg_write(reg_base + REG_SCL_IMG_ADDR0_H(inst), addr0 >> 32);
	_reg_write(reg_base + REG_SCL_IMG_ADDR1_L(inst), addr1);
	_reg_write(reg_base + REG_SCL_IMG_ADDR1_H(inst), addr1 >> 32);
	_reg_write(reg_base + REG_SCL_IMG_ADDR2_L(inst), addr2);
	_reg_write(reg_base + REG_SCL_IMG_ADDR2_H(inst), addr2 >> 32);

	g_img_cfg[inst].mem.addr0 = addr0;
	g_img_cfg[inst].mem.addr1 = addr1;
	g_img_cfg[inst].mem.addr2 = addr2;
}

/**
 * sclr_img_set_csc - configure scl-img's input CSC's coefficient/offset
 *
 * @param inst: (0~1)
 * @param cfg: The settings for CSC
 */
void sclr_img_set_csc(u8 inst, struct sclr_csc_cfg *cfg)
{
	_reg_write(reg_base + REG_SCL_IMG_CSC_COEF0(inst),
		   (cfg->coef[0][1] << 16) | cfg->coef[0][0]);
	_reg_write(reg_base + REG_SCL_IMG_CSC_COEF1(inst),
		   cfg->coef[0][2]);
	_reg_write(reg_base + REG_SCL_IMG_CSC_COEF2(inst),
		   (cfg->coef[1][1] << 16) | cfg->coef[1][0]);
	_reg_write(reg_base + REG_SCL_IMG_CSC_COEF3(inst), cfg->coef[1][2]);
	_reg_write(reg_base + REG_SCL_IMG_CSC_COEF4(inst),
		   (cfg->coef[2][1] << 16) | cfg->coef[2][0]);
	_reg_write(reg_base + REG_SCL_IMG_CSC_COEF5(inst), cfg->coef[2][2]);
	_reg_write(reg_base + REG_SCL_IMG_CSC_SUB(inst),
		   (cfg->sub[2] << 16) | (cfg->sub[1] << 8) | cfg->sub[0]);
	_reg_write(reg_base + REG_SCL_IMG_CSC_ADD(inst),
		   (cfg->add[2] << 16) | (cfg->add[1] << 8) | cfg->add[0]);
}

union sclr_img_dbg_status sclr_img_get_dbg_status(u8 inst, bool clr)
{
	union sclr_img_dbg_status status;

	status.raw = _reg_read(reg_base + REG_SCL_IMG_DBG(inst));

	if (clr) {
		status.b.err_fwr_clr = 1;
		status.b.err_erd_clr = 1;
		status.b.ip_clr = 1;
		status.b.ip_int_clr = 1;
		_reg_write(reg_base + REG_SCL_IMG_DBG(inst), status.raw);
	}

	return status;
}

/****************************************************************************
 * SCALER CIRCLE
 ****************************************************************************/
/**
 * sclr_cir_set_cfg - configure scl-circle
 *
 * @param inst: (0~3), the instance of scaler which want to be configured.
 * @param cfg: cir's settings.
 */
void sclr_cir_set_cfg(u8 inst, struct sclr_cir_cfg *cfg)
{
	if (cfg->mode == SCL_CIR_DISABLE) {
		_reg_write_mask(reg_base + REG_SCL_CFG(inst), 0x20, 0x20);
	} else {
		_reg_write(reg_base + REG_SCL_CIR_CFG(inst),
			   (cfg->line_width << 8) | cfg->mode);
		_reg_write(reg_base + REG_SCL_CIR_CENTER_X(inst),
			   cfg->center.x);
		_reg_write(reg_base + REG_SCL_CIR_CENTER_Y(inst),
			   cfg->center.y);
		_reg_write(reg_base + REG_SCL_CIR_RADIUS(inst), cfg->radius);
		_reg_write(reg_base + REG_SCL_CIR_SIZE(inst),
			   ((cfg->rect.h - 1) << 16) | (cfg->rect.w - 1));
		_reg_write(reg_base + REG_SCL_CIR_OFFSET(inst),
			   (cfg->rect.y << 16) | cfg->rect.x);
		_reg_write(reg_base + REG_SCL_CIR_COLOR(inst),
			   (cfg->color_b << 16) | (cfg->color_g << 8) |
			   cfg->color_r);

		_reg_write_mask(reg_base + REG_SCL_CFG(inst), 0x20, 0x00);
	}

	g_cir_cfg[inst] = *cfg;
}

/****************************************************************************
 * SCALER BORDER
 ****************************************************************************/
/**
 * sclr_odma_set_cfg - configure scaler's odma
 *
 * @param inst: (0~3), the instance of scaler which want to be configured.
 * @param cfg: new border config
 */
void sclr_border_set_cfg(u8 inst, struct sclr_border_cfg *cfg)
{
	if ((g_sc_cfg[inst].sc.dst.w + cfg->start.x) >
	    g_odma_cfg[inst].mem.width) {
		pr_err("[bm-vip][sc] %s: sc_width(%d) + offset(%d)", __func__,
		       g_sc_cfg[inst].sc.dst.w, cfg->start.x);
		pr_cont(" > odma_width(%d)\n", g_odma_cfg[inst].mem.width);
		cfg->start.x = g_odma_cfg[inst].mem.width -
			       g_sc_cfg[inst].sc.dst.w;
	}

	if ((g_sc_cfg[inst].sc.dst.h + cfg->start.y) >
	    g_odma_cfg[inst].mem.height) {
		pr_err("[bm-vip][sc] %s: sc_height(%d) + offset(%d)", __func__,
		       g_sc_cfg[inst].sc.dst.h, cfg->start.y);
		pr_cont(" > odma_height(%d)\n", g_odma_cfg[inst].mem.height);
		cfg->start.y = g_odma_cfg[inst].mem.height -
			       g_sc_cfg[inst].sc.dst.h;
	}

	_reg_write(reg_base + REG_SCL_BORDER_CFG(inst), cfg->cfg.raw);
	_reg_write(reg_base + REG_SCL_BORDER_OFFSET(inst),
		   (cfg->start.y << 16) | cfg->start.x);

	g_bd_cfg[inst] = *cfg;
}

/**
 * sclr_bordera_get_cfg - get scl_border's cfg
 *
 * @param inst: 0~3
 * @return: scl_border's cfg
 */
struct sclr_border_cfg *sclr_border_get_cfg(u8 inst)
{
	if (inst >= SCL_MAX_INST)
		return NULL;
	return &g_bd_cfg[inst];
}

/****************************************************************************
 * SCALER ODMA
 ****************************************************************************/
/**
 * sclr_odma_set_cfg - configure scaler's odma
 *
 * @param inst: (0~3), the instance of scaler which want to be configured.
 * @param burst: dma's burst length
 * @param fmt: dma's format
 */
void sclr_odma_set_cfg(u8 inst, struct sclr_odma_cfg *cfg)
{
	_reg_write(reg_base + REG_SCL_ODMA_CFG(inst),
		   (cfg->flip << 16) |  (cfg->fmt << 8) | cfg->burst);

	sclr_odma_set_mem(inst, &cfg->mem);

	g_odma_cfg[inst] = *cfg;
}

/**
 * sclr_odma_get_cfg - get scl_odma's cfg
 *
 * @param inst: 0~3
 * @return: scl_odma's cfg
 */
struct sclr_odma_cfg *sclr_odma_get_cfg(u8 inst)
{
	if (inst >= SCL_MAX_INST)
		return NULL;
	return &g_odma_cfg[inst];
}

/**
 * sclr_odma_set_fmt - set scl_odma's output data format
 *
 * @param inst: 0~3
 * @param fmt: scl_odma's output format
 */
void sclr_odma_set_fmt(u8 inst, enum sclr_format fmt)
{
	_reg_write_mask(reg_base + REG_SCL_ODMA_CFG(inst), 0x0000ff00,
			fmt << 8);

	g_odma_cfg[inst].fmt = fmt;
}

/**
 * sclr_odma_set_mem - setup odma's mem settings.
 *
 * @param mem: mem settings for odma
 */
void sclr_odma_set_mem(u8 inst, struct sclr_mem *mem)
{
	_reg_write(reg_base + REG_SCL_ODMA_OFFSET_X(inst), mem->start_x);
	_reg_write(reg_base + REG_SCL_ODMA_OFFSET_Y(inst), mem->start_y);
	_reg_write(reg_base + REG_SCL_ODMA_WIDTH(inst), mem->width - 1);
	_reg_write(reg_base + REG_SCL_ODMA_HEIGHT(inst), mem->height - 1);
	_reg_write(reg_base + REG_SCL_ODMA_PITCH_Y(inst), mem->pitch_y);
	_reg_write(reg_base + REG_SCL_ODMA_PITCH_C(inst), mem->pitch_c);

	sclr_odma_set_addr(inst, mem->addr0, mem->addr1, mem->addr2);

	g_odma_cfg[inst].mem = *mem;
}

/**
 * sclr_odma_set_addr - setup odma's mem address.
 *
 * @param addr0: address of planar0
 * @param addr1: address of planar1
 * @param addr2: address of planar2
 */
void sclr_odma_set_addr(u8 inst, u64 addr0, u64 addr1, u64 addr2)
{
	_reg_write(reg_base + REG_SCL_ODMA_ADDR0_L(inst), addr0);
	_reg_write(reg_base + REG_SCL_ODMA_ADDR0_H(inst), addr0 >> 32);
	_reg_write(reg_base + REG_SCL_ODMA_ADDR1_L(inst), addr1);
	_reg_write(reg_base + REG_SCL_ODMA_ADDR1_H(inst), addr1 >> 32);
	_reg_write(reg_base + REG_SCL_ODMA_ADDR2_L(inst), addr2);
	_reg_write(reg_base + REG_SCL_ODMA_ADDR2_H(inst), addr2 >> 32);
}

union sclr_odma_dbg_status sclr_odma_get_dbg_status(u8 inst)
{
	union sclr_odma_dbg_status status;

	status.raw = _reg_read(reg_base + REG_SCL_ODMA_DBG(inst));

	return status;
}

/**
 * sclr_set_out_mode - Control sclr output's mode. CSC/Quantization/HSV
 *
 * @param inst: (0~3), the instance of scaler which want to be configured.
 * @param mode: csc/quant/hsv/none
 */
void sclr_set_out_mode(u8 inst, enum sclr_out_mode mode)
{
	u32 tmp = 0;

	switch (mode) {
	case SCL_OUT_CSC:
		tmp = BIT(0);
		break;
	case SCL_OUT_QUANT:
		tmp = BIT(1) | BIT(0);
		break;
	case SCL_OUT_HSV:
		tmp = BIT(4);
		break;
	case SCL_OUT_DISABLE:
		break;
	}
	_reg_write_mask(reg_base + REG_SCL_CSC_EN(inst), 0x00000013, tmp);

	g_odma_cfg[inst].mode = mode;
}

/**
 * sclr_set_csc - configure scl's output CSC's coefficient/offset
 *
 * @param inst: (0~3), the instance of scaler which want to be configured.
 * @param cfg: The settings for CSC
 */
void sclr_set_csc(u8 inst, struct sclr_csc_cfg *cfg)
{
	_reg_write(reg_base + REG_SCL_CSC_COEF0(inst),
		   (cfg->coef[0][1] << 16) | cfg->coef[0][0]);
	_reg_write(reg_base + REG_SCL_CSC_COEF1(inst),
		   (cfg->coef[1][0] << 16) | cfg->coef[0][2]);
	_reg_write(reg_base + REG_SCL_CSC_COEF2(inst),
		   (cfg->coef[1][2] << 16) | cfg->coef[1][1]);
	_reg_write(reg_base + REG_SCL_CSC_COEF3(inst),
		   (cfg->coef[2][1] << 16) | cfg->coef[2][0]);
	_reg_write(reg_base + REG_SCL_CSC_COEF4(inst), cfg->coef[2][2]);

	_reg_write(reg_base + REG_SCL_CSC_OFFSET(inst),
		   (cfg->add[2] << 16) | (cfg->add[1] << 8) | cfg->add[0]);
	_reg_write(reg_base + REG_SCL_CSC_FRAC0(inst), 0);
	_reg_write(reg_base + REG_SCL_CSC_FRAC1(inst), 0);
}

/**
 * sclr_set_quant - set quantization by csc module
 *
 * @param inst: (0~3), the instance of scaler which want to be configured.
 * @param cfg: The settings of quantization
 */
void sclr_set_quant(u8 inst, struct sclr_quant_cfg *cfg)
{
	_reg_write(reg_base + REG_SCL_CSC_COEF0(inst), cfg->sc[0]);
	_reg_write(reg_base + REG_SCL_CSC_COEF2(inst), cfg->sc[1]);
	_reg_write(reg_base + REG_SCL_CSC_COEF4(inst), cfg->sc[2]);

	_reg_write(reg_base + REG_SCL_CSC_OFFSET(inst),
		   (cfg->sub[2] << 16) | (cfg->sub[1] << 8) | cfg->sub[0]);
	_reg_write(reg_base + REG_SCL_CSC_FRAC0(inst),
		   (cfg->sub_frac[1] << 16) | cfg->sub_frac[0]);
	_reg_write(reg_base + REG_SCL_CSC_FRAC1(inst), cfg->sub_frac[2]);
}

/**
 * sclr_set_quant_drop_mode - set drop-mode of quantization by csc module
 *
 * @param inst: (0~3), the instance of scaler which want to be configured.
 * @param cfg: The drop-mode for rounding
 */
void sclr_set_quant_drop_mode(u8 inst, enum sclr_quant_rounding mode)
{
	_reg_write_mask(reg_base + REG_SCL_CSC_EN(inst), 0x000c, mode << 2);
}

/**
 * sclr_get_csc - get current CSC's coefficient/offset
 *
 * @param inst: (0~3), the instance of scaler which want to be configured.
 * @param cfg: The settings of CSC
 */
void sclr_get_csc(u8 inst, struct sclr_csc_cfg *cfg)
{
	u32 tmp;

	tmp = _reg_read(reg_base + REG_SCL_CSC_COEF0(inst));
	cfg->coef[0][0] = tmp;
	cfg->coef[0][1] = tmp >> 16;
	tmp = _reg_read(reg_base + REG_SCL_CSC_COEF1(inst));
	cfg->coef[0][2] = tmp;
	cfg->coef[1][0] = tmp >> 16;
	tmp = _reg_read(reg_base + REG_SCL_CSC_COEF2(inst));
	cfg->coef[1][1] = tmp;
	cfg->coef[1][2] = tmp >> 16;
	tmp = _reg_read(reg_base + REG_SCL_CSC_COEF3(inst));
	cfg->coef[2][0] = tmp;
	cfg->coef[2][1] = tmp >> 16;
	tmp = _reg_read(reg_base + REG_SCL_CSC_COEF4(inst));
	cfg->coef[2][2] = tmp;

	tmp = _reg_read(reg_base + REG_SCL_CSC_OFFSET(inst));
	cfg->add[0] = tmp;
	cfg->add[1] = tmp >> 8;
	cfg->add[2] = tmp >> 16;
	memset(cfg->sub, 0, sizeof(cfg->sub));
}

/****************************************************************************
 * SCALER GOP
 ****************************************************************************/
/**
 * sclr_gop_set_cfg - configure gop
 *
 * @param inst: (0~4), the instance of gop which want to be configured.
 *		0~3 is on scl, 4 is on disp.
 * @param cfg: gop's settings
 */
void sclr_gop_set_cfg(u8 inst, struct sclr_gop_cfg *cfg)
{
	if (inst < SCL_MAX_INST) {
		_reg_write(reg_base + REG_SCL_GOP_CFG(inst), cfg->raw & 0xffff);
		_reg_write(reg_base + REG_SCL_GOP_FONTCOLOR(inst),
			   (cfg->font_fg_color << 16) |
			   cfg->font_bg_color);
		if (cfg->b.colorkey_en)
			_reg_write(reg_base + REG_SCL_GOP_COLORKEY(inst),
				   cfg->colorkey);

		g_gop_cfg[inst] = *cfg;
	} else {
		_reg_write(reg_base + REG_SCL_DISP_GOP_CFG, cfg->raw & 0xffff);
		_reg_write(reg_base + REG_SCL_DISP_GOP_FONTCOLOR,
			   (cfg->font_fg_color << 16) |
			   cfg->font_bg_color);
		if (cfg->b.colorkey_en)
			_reg_write(reg_base + REG_SCL_DISP_GOP_COLORKEY,
				   cfg->colorkey);

		g_disp_cfg.gop_cfg = *cfg;
	}
}

/**
 * sclr_gop_get_cfg - get gop's configurations.
 *
 * @param inst: (0~4), the instance of gop which want to be configured.
 *		0~3 is on scl, 4 is on disp.
 */
struct sclr_gop_cfg *sclr_gop_get_cfg(u8 inst)
{
	if (inst < SCL_MAX_INST)
		return &g_gop_cfg[inst];
	else if (inst == SCL_MAX_INST)
		return &g_disp_cfg.gop_cfg;

	return NULL;
}

/**
 * sclr_gop_setup LUT - setup gop's Look-up table
 *
 * @param inst: (0~4), the instance of gop which want to be configured.
 *		0~3 is on scl, 4 is on disp.
 * @param data: start address of LUT-table. There should be 256 instances.
 */
void sclr_gop_setup_LUT(u8 inst, u16 *data)
{
	u16 i = 0;

	if (inst < SCL_MAX_INST) {
		for (i = 0; i < 256; ++i) {
			_reg_write(reg_base + REG_SCL_GOP_LUT0(inst),
				   (i << 16) | *(data + i));
			_reg_write(reg_base + REG_SCL_GOP_LUT1(inst), ~BIT(16));
			_reg_write(reg_base + REG_SCL_GOP_LUT1(inst), BIT(16));
		}
	} else {
		for (i = 0; i < 256; ++i) {
			_reg_write(reg_base + REG_SCL_DISP_GOP_LUT0,
				   (i << 16) | *(data + i));
			_reg_write(reg_base + REG_SCL_DISP_GOP_LUT1, ~BIT(16));
			_reg_write(reg_base + REG_SCL_DISP_GOP_LUT1, BIT(16));
		}
	}
}

/**
 * sclr_gop_ow_set_cfg - set gop's osd-window configurations.
 *
 * @param inst: (0~4), the instance of gop which want to be configured.
 *		0~3 is on scl, 4 is on disp.
 * @param ow_inst: (0~7), the instance of ow which want to be configured.
 * @param cfg: ow's settings.
 */
void sclr_gop_ow_set_cfg(u8 inst, u8 ow_inst, struct sclr_gop_ow_cfg *cfg)
{
	static u8 reg_map_fmt[SCL_GOP_FMT_MAX] = {0, 0x4, 0x5, 0x8, 0xc};

	if (inst < SCL_MAX_INST) {
		_reg_write(reg_base + REG_SCL_GOP_FMT(inst, ow_inst),
			   reg_map_fmt[cfg->fmt]);
		_reg_write(reg_base + REG_SCL_GOP_H_RANGE(inst, ow_inst),
			   (cfg->end.x << 16) | cfg->start.x);
		_reg_write(reg_base + REG_SCL_GOP_V_RANGE(inst, ow_inst),
			   (cfg->end.y << 16) | cfg->start.y);
		_reg_write(reg_base + REG_SCL_GOP_ADDR_L(inst, ow_inst),
			   cfg->addr);
		_reg_write(reg_base + REG_SCL_GOP_ADDR_H(inst, ow_inst),
			   cfg->addr >> 32);
		_reg_write(reg_base + REG_SCL_GOP_PITCH(inst, ow_inst),
			   cfg->pitch);
		_reg_write(reg_base + REG_SCL_GOP_SIZE(inst, ow_inst),
			   (cfg->mem_size.h << 16) | cfg->mem_size.w);

		g_gop_cfg[inst].ow_cfg[ow_inst] = *cfg;
	} else {
		_reg_write(reg_base + REG_SCL_DISP_GOP_FMT(ow_inst),
			   reg_map_fmt[cfg->fmt]);
		_reg_write(reg_base + REG_SCL_DISP_GOP_H_RANGE(ow_inst),
			   (cfg->end.x << 16) | cfg->start.x);
		_reg_write(reg_base + REG_SCL_DISP_GOP_V_RANGE(ow_inst),
			   (cfg->end.y << 16) | cfg->start.y);
		_reg_write(reg_base + REG_SCL_DISP_GOP_ADDR_L(ow_inst),
			   cfg->addr);
		_reg_write(reg_base + REG_SCL_DISP_GOP_ADDR_H(ow_inst),
			   cfg->addr >> 32);
		_reg_write(reg_base + REG_SCL_DISP_GOP_PITCH(ow_inst),
			   cfg->pitch);
		_reg_write(reg_base + REG_SCL_DISP_GOP_SIZE(ow_inst),
			   (cfg->mem_size.h << 16) | cfg->mem_size.w);

		g_disp_cfg.gop_cfg.ow_cfg[ow_inst] = *cfg;
	}
}

/****************************************************************************
 * SCALER DISP
 ****************************************************************************/
/**
 * sclr_disp_reg_shadow_sel - control the read reg-bank.
 *
 * @param read_shadow: true(shadow); false(working)
 */
void sclr_disp_reg_shadow_sel(bool read_shadow)
{
	_reg_write_mask(reg_base + REG_SCL_DISP_CFG, BIT(18),
			(read_shadow ? 0x0 : BIT(18)));
}

/**
 * sclr_disp_reg_shadow_mask - reg won't be update by sw/hw until unmask.
 *
 * @param mask: true(mask); false(unmask)
 * @return: mask status before thsi modification.
 */
bool sclr_disp_reg_shadow_mask(bool mask)
{
	bool is_masked = (_reg_read(reg_base + REG_SCL_DISP_CFG) & BIT(17));

	if (is_masked != mask)
		_reg_write_mask(reg_base + REG_SCL_DISP_CFG, BIT(17),
				(mask ? 0x0 : BIT(17)));

	return is_masked;
}

/**
 * sclr_disp_tgen - enable timing-generator on disp.
 *
 * @param enable: AKA.
 * @return: tgen's enable status before change.
 */
bool sclr_disp_tgen_enable(bool enable)
{
	bool is_enable = (_reg_read(reg_base + REG_SCL_DISP_CFG) & 0x80);

	if (is_enable != enable) {
		_reg_write_mask(reg_base + REG_SCL_DISP_CFG, 0x0080,
				enable ? 0x80 : 0x00);
		g_disp_cfg.tgen_en = enable;
	}

	return is_enable;
}

/**
 * sclr_disp_set_cfg - set disp's configurations.
 *
 * @param cfg: disp's settings.
 */
void sclr_disp_set_cfg(struct sclr_disp_cfg *cfg)
{
	u32 tmp = 0;
	bool is_enable = false;

	tmp |= cfg->disp_from_sc;
	tmp |= (cfg->fmt << 12);
	if (cfg->sync_ext)
		tmp |= BIT(4);
	if (cfg->tgen_en)
		tmp |= BIT(7);
	if (cfg->dw1_en)
		tmp |= BIT(8);
	if (cfg->dw2_en)
		tmp |= BIT(9);

	is_enable = sclr_disp_reg_shadow_mask(false);
	if (!cfg->disp_from_sc) {
		sclr_disp_set_mem(&cfg->mem);
		_reg_write_mask(reg_base + REG_SCL_DISP_PITCH_Y, 0xf0000000,
				cfg->burst << 28);

		sclr_disp_set_in_csc(cfg->in_csc);
		if (g_odma_cfg[0].csc != SCL_CSC_NONE)
			sclr_set_out_mode(0, SCL_OUT_CSC);
	} else {
		// csc only needed if disp from dram
		sclr_set_out_mode(0, SCL_OUT_DISABLE);
	}

	sclr_disp_set_out_csc(cfg->out_csc);

	_reg_write_mask(reg_base + REG_SCL_DISP_CFG, 0x0000ff9f, tmp);

	if (is_enable)
		sclr_disp_reg_shadow_mask(true);

	g_disp_cfg = *cfg;
}

/**
 * sclr_disp_get_cfg - get scl_disp's cfg
 *
 * @return: scl_disp's cfg
 */
struct sclr_disp_cfg *sclr_disp_get_cfg(void)
{
	return &g_disp_cfg;
}

/**
 * sclr_disp_set_timing - modify disp's timing-generator.
 *
 * @param timing: new timing of disp.
 */
void sclr_disp_set_timing(struct sclr_disp_timing *timing)
{
	u32 tmp = 0;
	bool is_enable = sclr_disp_tgen_enable(false);

	if (timing->vsync_pol)
		tmp |= 0x20;
	if (timing->hsync_pol)
		tmp |= 0x40;

	_reg_write_mask(reg_base + REG_SCL_DISP_CFG, 0x0060, tmp);
	_reg_write(reg_base + REG_SCL_DISP_TOTAL,
		   (timing->htotal << 16) | timing->vtotal);
	_reg_write(reg_base + REG_SCL_DISP_VSYNC,
		   (timing->vsync_end << 16) | timing->vsync_start);
	_reg_write(reg_base + REG_SCL_DISP_VFDE,
		   (timing->vfde_end << 16) | timing->vfde_start);
	_reg_write(reg_base + REG_SCL_DISP_VMDE,
		   (timing->vmde_end << 16) | timing->vmde_start);
	_reg_write(reg_base + REG_SCL_DISP_HSYNC,
		   (timing->hsync_end << 16) | timing->hsync_start);
	_reg_write(reg_base + REG_SCL_DISP_HFDE,
		   (timing->hfde_end << 16) | timing->hfde_start);
	_reg_write(reg_base + REG_SCL_DISP_HMDE,
		   (timing->hmde_end << 16) | timing->hmde_start);

	if (is_enable)
		sclr_disp_tgen_enable(true);
	disp_timing = *timing;
}

/**
 * sclr_disp_set_rect - setup rect(me) of disp
 *
 * @param rect: the pos/size of me, which should fit with disp's input.
 */
int sclr_disp_set_rect(struct sclr_rect rect)
{
	bool is_enable = sclr_disp_reg_shadow_mask(false);

	if ((rect.y > disp_timing.vfde_end) ||
	    (rect.x > disp_timing.hfde_end) ||
	    ((disp_timing.vfde_start + rect.y + rect.h - 1) >
	     disp_timing.vfde_end) ||
	    ((disp_timing.hfde_start + rect.x + rect.w - 1) >
	     disp_timing.hfde_end)) {
		pr_err("[bm-vip][sc] %s: me's pos(%d, %d) size(%d, %d) ",
		       __func__, rect.x, rect.y, rect.w, rect.h);
		pr_cont(" out of range(%d, %d).\n",
			disp_timing.hfde_end, disp_timing.vfde_end);
		return -EINVAL;
	}

	disp_timing.vmde_start = rect.y + disp_timing.vfde_start;
	disp_timing.hmde_start = rect.x + disp_timing.hfde_start;
	disp_timing.vmde_end = disp_timing.vmde_start + rect.h - 1;
	disp_timing.hmde_end = disp_timing.hmde_start + rect.w - 1;

	_reg_write(reg_base + REG_SCL_DISP_HMDE,
		   (disp_timing.hmde_end << 16) | disp_timing.hmde_start);
	_reg_write(reg_base + REG_SCL_DISP_VMDE,
		   (disp_timing.vmde_end << 16) | disp_timing.vmde_start);

	if (is_enable)
		sclr_disp_reg_shadow_mask(true);

	return 0;
}

/**
 * sclr_disp_set_mem - setup disp's mem settings. Only work if disp from mem.
 *
 * @param mem: mem settings for disp
 */
void sclr_disp_set_mem(struct sclr_mem *mem)
{
	bool is_enable = sclr_disp_reg_shadow_mask(false);

	_reg_write(reg_base + REG_SCL_DISP_OFFSET,
		   (mem->start_y << 16) | mem->start_x);
	_reg_write(reg_base + REG_SCL_DISP_SIZE,
		   ((mem->height - 1) << 16) | (mem->width - 1));
	_reg_write_mask(reg_base + REG_SCL_DISP_PITCH_Y, 0x00ffffff,
			mem->pitch_y);
	_reg_write(reg_base + REG_SCL_DISP_PITCH_C, mem->pitch_c);

	sclr_disp_set_addr(mem->addr0, mem->addr1, mem->addr2);
	if (is_enable)
		sclr_disp_reg_shadow_mask(true);

	g_disp_cfg.mem = *mem;
}

/**
 * sclr_disp_set_addr - setup disp's mem address. Only work if disp from mem.
 *
 * @param addr0: address of planar0
 * @param addr1: address of planar1
 * @param addr2: address of planar2
 */
void sclr_disp_set_addr(u64 addr0, u64 addr1, u64 addr2)
{
	_reg_write(reg_base + REG_SCL_DISP_ADDR0_L, addr0);
	_reg_write(reg_base + REG_SCL_DISP_ADDR0_H, addr0 >> 32);
	_reg_write(reg_base + REG_SCL_DISP_ADDR1_L, addr1);
	_reg_write(reg_base + REG_SCL_DISP_ADDR1_H, addr1 >> 32);
	_reg_write(reg_base + REG_SCL_DISP_ADDR2_L, addr2);
	_reg_write(reg_base + REG_SCL_DISP_ADDR2_H, addr2 >> 32);

	g_disp_cfg.mem.addr0 = addr0;
	g_disp_cfg.mem.addr1 = addr1;
	g_disp_cfg.mem.addr2 = addr2;
}

/**
 * sclr_disp_set_in_csc - setup disp's csc on input. Only work if disp from mem.
 *
 * @param csc: csc settings
 */
void sclr_disp_set_in_csc(enum sclr_csc csc)
{
	if (csc == SCL_CSC_NONE) {
		_reg_write(reg_base + REG_SCL_DISP_IN_CSC0, 0);
	} else if (csc < SCL_CSC_MAX) {
		struct sclr_csc_cfg *cfg = &csc_mtrx[csc];

		_reg_write(reg_base + REG_SCL_DISP_IN_CSC0, BIT(31) |
			   (cfg->coef[0][1] << 16) | (cfg->coef[0][0]));
		_reg_write(reg_base + REG_SCL_DISP_IN_CSC1,
			   (cfg->coef[1][0] << 16) | (cfg->coef[0][2]));
		_reg_write(reg_base + REG_SCL_DISP_IN_CSC2,
			   (cfg->coef[1][2] << 16) | (cfg->coef[1][1]));
		_reg_write(reg_base + REG_SCL_DISP_IN_CSC3,
			   (cfg->coef[2][1] << 16) | (cfg->coef[2][0]));
		_reg_write(reg_base + REG_SCL_DISP_IN_CSC4, (cfg->coef[2][2]));
		_reg_write(reg_base + REG_SCL_DISP_IN_CSC_SUB,
			   (cfg->sub[2] << 16) | (cfg->sub[1] << 8) |
			   cfg->sub[0]);
		_reg_write(reg_base + REG_SCL_DISP_IN_CSC_ADD,
			   (cfg->add[2] << 16) | (cfg->add[1] << 8) |
			   cfg->add[0]);
	}

	g_disp_cfg.in_csc = csc;
}

/**
 * sclr_disp_set_out_csc - setup disp's csc on output.
 *
 * @param csc: csc settings
 */
void sclr_disp_set_out_csc(enum sclr_csc csc)
{
	if (csc == SCL_CSC_NONE) {
		_reg_write(reg_base + REG_SCL_DISP_OUT_CSC0, 0);
	} else if (csc < SCL_CSC_MAX) {
		struct sclr_csc_cfg *cfg = &csc_mtrx[csc];

		_reg_write(reg_base + REG_SCL_DISP_OUT_CSC0, BIT(31) |
			   (cfg->coef[0][1] << 16) | (cfg->coef[0][0]));
		_reg_write(reg_base + REG_SCL_DISP_OUT_CSC1,
			   (cfg->coef[1][0] << 16) | (cfg->coef[0][2]));
		_reg_write(reg_base + REG_SCL_DISP_OUT_CSC2,
			   (cfg->coef[1][2] << 16) | (cfg->coef[1][1]));
		_reg_write(reg_base + REG_SCL_DISP_OUT_CSC3,
			   (cfg->coef[2][1] << 16) | (cfg->coef[2][0]));
		_reg_write(reg_base + REG_SCL_DISP_OUT_CSC4, (cfg->coef[2][2]));
		_reg_write(reg_base + REG_SCL_DISP_OUT_CSC_SUB,
			   (cfg->sub[2] << 16) | (cfg->sub[1] << 8) |
			   cfg->sub[0]);
		_reg_write(reg_base + REG_SCL_DISP_OUT_CSC_ADD,
			   (cfg->add[2] << 16) | (cfg->add[1] << 8) |
			   cfg->add[0]);
	}

	g_disp_cfg.out_csc = csc;
}

/**
 * sclr_disp_set_pattern - setup disp's pattern generator.
 *
 * @param type: type of pattern
 * @param color: color of pattern. Only for Gradient/FULL type.
 */
void sclr_disp_set_pattern(enum sclr_disp_pat_type type,
			   enum sclr_disp_pat_color color)
{
	switch (type) {
	case SCL_PAT_TYPE_OFF:
		_reg_write_mask(reg_base + REG_SCL_DISP_PAT_CFG, 0x36, 0);
		break;

	case SCL_PAT_TYPE_FIX:
		_reg_write_mask(reg_base + REG_SCL_DISP_PAT_CFG, 0x36, 0x20);
		break;

	case SCL_PAT_TYPE_SNOW:
		_reg_write_mask(reg_base + REG_SCL_DISP_PAT_CFG, 0x36, 0x10);
		break;

	case SCL_PAT_TYPE_AUTO:
		_reg_write_mask(reg_base + REG_SCL_DISP_PAT_CFG, 0xff0036,
				0x780006);
		break;

	case SCL_PAT_TYPE_V_GRAD:
	case SCL_PAT_TYPE_H_GRAD:
	case SCL_PAT_TYPE_FULL:
		_reg_write_mask(reg_base + REG_SCL_DISP_PAT_CFG, 0x1f000036,
				(type << 27) | (color << 24) | 0x0002);
		break;
	default:
		pr_err("%s - unacceptiable pattern-type(%d)\n", __func__, type);
		break;
	}
}

/**
 * sclr_disp_set_frame_bgcolro - setup disp frame(area outside mde)'s
 *				 background color.
 *
 * @param r: 10bit red value
 * @param g: 10bit green value
 * @param b: 10bit blue value
 */
void sclr_disp_set_frame_bgcolor(u16 r, u16 g, u16 b)
{
	_reg_write_mask(reg_base + REG_SCL_DISP_PAT_COLOR1, 0x0fff0000,
			r << 16);
	_reg_write(reg_base + REG_SCL_DISP_PAT_COLOR2, b << 16 | g);
}

/**
 * sclr_disp_set_window_bgcolro - setup disp window's background color.
 *
 * @param r: 10bit red value
 * @param g: 10bit green value
 * @param b: 10bit blue value
 */
void sclr_disp_set_window_bgcolor(u16 r, u16 g, u16 b)
{
	_reg_write(reg_base + REG_SCL_DISP_PAT_COLOR3, g << 16 | r);
	_reg_write_mask(reg_base + REG_SCL_DISP_PAT_COLOR4, 0x0fff, b);
}

union sclr_disp_dbg_status sclr_disp_get_dbg_status(bool clr)
{
	union sclr_disp_dbg_status status;

	status.raw = _reg_read(reg_base + REG_SCL_DISP_DBG);

	if (clr) {
		status.b.err_fwr_clr = 1;
		status.b.err_erd_clr = 1;
		status.b.bw_fail_clr = 1;
		_reg_write(reg_base + REG_SCL_DISP_DBG, status.raw);
	}

	return status;
}

void sclr_lvdstx_set(union sclr_lvdstx cfg)
{
	_reg_write(reg_base + REG_SCL_TOP_LVDSTX, cfg.raw);
}

void sclr_lvdstx_get(union sclr_lvdstx *cfg)
{
	cfg->raw = _reg_read(reg_base + REG_SCL_TOP_LVDSTX);
}

void sclr_bt_set(union sclr_bt_enc enc, union sclr_bt_sync_code sync)
{
	_reg_write(reg_base + REG_SCL_TOP_BT_ENC, enc.raw);
	_reg_write(reg_base + REG_SCL_TOP_BT_SYNC_CODE, sync.raw);
}

void sclr_bt_get(union sclr_bt_enc *enc, union sclr_bt_sync_code *sync)
{
	enc->raw = _reg_read(reg_base + REG_SCL_TOP_BT_ENC);
	sync->raw = _reg_read(reg_base + REG_SCL_TOP_BT_SYNC_CODE);
}

/****************************************************************************
 * SCALER CTRL
 ****************************************************************************/
/**
 * sclr_ctrl_init - setup all sc instances.
 *
 */
void sclr_ctrl_init(void)
{
	union sclr_intr intr_mask;
	union vip_sys_intr vip_intr_mask;
	bool disp_from_sc = false;
	unsigned int i = 0;

	memset(&intr_mask, 0, sizeof(intr_mask));

	// init variables
	memset(&g_top_cfg, 0, sizeof(g_top_cfg));
	memset(&g_sc_cfg, 0, sizeof(g_sc_cfg));
	memset(&g_bd_cfg, 0, sizeof(g_bd_cfg));
	memset(&g_cir_cfg, 0, sizeof(g_cir_cfg));
	memset(&g_img_cfg, 0, sizeof(g_img_cfg));
	memset(&g_odma_cfg, 0, sizeof(g_odma_cfg));
	memset(&g_disp_cfg, 0, sizeof(g_disp_cfg));
	memset(&disp_timing, 0, sizeof(disp_timing));
	memset(&g_gop_cfg, 0, sizeof(g_gop_cfg));

	for (i = 0; i < SCL_MAX_INST; ++i) {
		g_odma_cfg[i].flip = SCL_FLIP_NO;
		g_odma_cfg[i].burst = true;
	}

	g_img_cfg[0].burst = 3;
	g_img_cfg[1].burst = 3;
	g_top_cfg.ip_trig_src = true;
	g_top_cfg.sclr_enable[0] = false;
	g_top_cfg.sclr_enable[1] = false;
	g_top_cfg.sclr_enable[2] = false;
	g_top_cfg.sclr_enable[3] = false;
	g_top_cfg.disp_enable = false;
	g_top_cfg.disp_from_sc = disp_from_sc;
	g_top_cfg.img_in_d_trig_src = 0;
	g_top_cfg.img_in_v_trig_src = 0;
	g_disp_cfg.disp_from_sc = disp_from_sc;
	g_disp_cfg.sync_ext = false;
	g_disp_cfg.tgen_en = false;
	g_disp_cfg.fmt = SCL_FMT_RGB_PLANAR;
	g_disp_cfg.in_csc = SCL_CSC_NONE;
	g_disp_cfg.out_csc = SCL_CSC_NONE;
	g_disp_cfg.burst = 4;

	// init hw
	sclr_top_set_cfg(&g_top_cfg);

	intr_mask.b.img_in_d_frame_end = true;
	intr_mask.b.img_in_v_frame_end = true;
	intr_mask.b.scl0_frame_end = true;
	intr_mask.b.scl1_frame_end = true;
	intr_mask.b.scl2_frame_end = true;
	intr_mask.b.scl3_frame_end = true;
	intr_mask.b.prog_too_late = true;
	intr_mask.b.cmdq = true;
	intr_mask.b.disp_frame_end = true;
	sclr_set_intr_mask(intr_mask);

	sclr_img_reg_shadow_sel(0, false);
	sclr_img_reg_shadow_sel(1, false);
	sclr_disp_reg_shadow_sel(false);
	for (i = 0; i < SCL_MAX_INST; ++i) {
		sclr_reg_shadow_sel(i, false);
		sclr_init(i);
		sclr_set_cfg(i, false, false, false, true, false);
		sclr_reg_force_up(i);
	}

	sclr_disp_tgen_enable(false);
	sclr_disp_set_cfg(&g_disp_cfg);
#ifdef ENV_BMTEST
	// TODO: for FPGA TTL
	sclr_disp_set_timing(&panels[SCL_PANEL_TTL]);
	//_reg_write(reg_base + REG_SCL_DISP_DUMMY, 0xFFFD);
#endif

	sclr_top_reg_done();
	sclr_top_reg_force_up();
	sclr_top_pg_late_clr();

	vip_intr_mask = vip_get_intr_mask();
	vip_intr_mask.b.sc = 1;
	vip_set_intr_mask(vip_intr_mask);
}

/**
 * sclr_ctrl_set_scale - setup scaling
 *
 * @param inst: (0~3), the instance of sc
 * @param cfg: scaling settings, include in/crop/out size.
 */
void sclr_ctrl_set_scale(u8 inst, struct sclr_scale_cfg *cfg)
{
	if (inst >= SCL_MAX_INST) {
		pr_err("[bm-vip][sc] %s: no enough sclr-instance ", __func__);
		pr_cont("for the requirement(%d)\n", inst);
		return;
	}
	if (memcmp(cfg, &g_sc_cfg[inst].sc, sizeof(*cfg)) == 0)
		return;

	sclr_set_input_size(inst, cfg->src);

	// if crop invalid, use src-size
	if (cfg->crop.w + cfg->crop.x > cfg->src.w) {
		cfg->crop.x = 0;
		cfg->crop.w = cfg->src.w;
	}
	if (cfg->crop.h + cfg->crop.y > cfg->src.h) {
		cfg->crop.y = 0;
		cfg->crop.h = cfg->src.h;
	}
	sclr_set_crop(inst, cfg->crop);
	sclr_set_output_size(inst, cfg->dst);
	sclr_set_scale(inst);
}

/**
 * sclr_ctrl_set_input - setup input
 *
 * @param inst: (0~1), the instance of img_in
 * @param input: input mux
 * @param fmt: color format
 * @param csc: csc which used to convert from yuv to rgb.
 * @return: 0 if success
 */
int sclr_ctrl_set_input(enum sclr_img_in inst, union sclr_input input,
			enum sclr_format fmt, enum sclr_csc csc)
{
	int ret = 0;

	// always transfer to rgb for sc
	if ((inst >= SCL_IMG_MAX) || (csc >= SCL_CSC_601_LIMIT_RGB2YUV))
		return -EINVAL;

	switch (inst) {
	case SCL_IMG_D:
		g_img_cfg[inst].src = input.d;
		break;

	case SCL_IMG_V:
		g_img_cfg[inst].src = input.v;
		break;

	default:
		break;
	};

	g_img_cfg[inst].fmt = fmt;
	sclr_img_set_cfg(inst, &g_img_cfg[inst]);

	g_img_cfg[inst].csc = csc;
	sclr_img_set_csc(inst, &csc_mtrx[csc]);

	return ret;
}

/**
 * sclr_ctrl_set_output - setup output of sc
 *
 * @param inst: (0~3), the instance of sc
 * @param mode: output mode (csc/quant/hsv/disable)
 * @param fmt: color format
 * @param param: pamaeters, if mode = csc then enum sclr_csc , if mode = quant
 *               then scl_quant_cfg, o/w NULL
 * @return: 0 if success
 */
int sclr_ctrl_set_output(u8 inst, enum sclr_out_mode mode,
			 enum sclr_format fmt, void *param)
{
	if (inst >= SCL_MAX_INST)
		return -EINVAL;

	if (mode == SCL_OUT_CSC) {
		enum sclr_csc csc = *(enum sclr_csc *)param;

		// Use yuv for csc
		// sc's data is always rgb
		if ((!param) || (!IS_YUV_FMT(fmt)) ||
		    ((csc < SCL_CSC_601_LIMIT_RGB2YUV) &&
		     (csc != SCL_CSC_NONE)))
			return -EINVAL;

		sclr_set_out_mode(inst, mode);
		sclr_odma_set_fmt(inst, fmt);

		g_odma_cfg[inst].csc = csc;
		sclr_set_csc(inst, &csc_mtrx[csc]);
	} else {
		// Use rgb for quant/hsv
		if (IS_YUV_FMT(fmt))
			return -EINVAL;

		sclr_set_out_mode(inst, mode);
		sclr_odma_set_fmt(inst, fmt);

		g_odma_cfg[inst].csc = SCL_CSC_NONE;
		sclr_set_csc(inst, &csc_mtrx[SCL_CSC_NONE]);
		if (mode == SCL_OUT_QUANT) {
			struct sclr_quant_cfg *cfg = param;

			if (!param)
				return -EINVAL;

			sclr_set_quant(inst, cfg);
		}
	}

	return 0;
}

/**
 * sclr_ctrl_set_disp_src - setup input-src of disp.
 *
 * @param disp_from_sc: true(from sc_0); false(from mem)
 * @return: 0 if success
 */
int sclr_ctrl_set_disp_src(bool disp_from_sc)
{
	g_top_cfg.disp_from_sc = disp_from_sc;
	g_top_cfg.img_in_d_trig_src = disp_from_sc;
	g_disp_cfg.disp_from_sc = disp_from_sc;

	sclr_top_set_cfg(&g_top_cfg);
	sclr_disp_set_cfg(&g_disp_cfg);
	sclr_set_cfg(0, g_sc_cfg[0].sc_bypass, g_sc_cfg[0].gop_bypass,
		     g_sc_cfg[0].dbg_en, g_sc_cfg[0].cir_bypass,
		     disp_from_sc);

	return 0;
}

static void _map_ctrl_to_img(struct sclr_ctrl_cfg *cfg,
			     struct sclr_img_cfg *img_cfg)
{
	img_cfg->burst = g_img_cfg[cfg->img_inst].burst;
	img_cfg->fmt = cfg->src_fmt;
	img_cfg->csc = cfg->src_csc;
	img_cfg->src = (cfg->img_inst == SCL_IMG_D) ?
		       cfg->input.d : cfg->input.v;
	img_cfg->mem.addr0 = cfg->src_addr0;
	img_cfg->mem.addr1 = cfg->src_addr1;
	img_cfg->mem.addr2 = cfg->src_addr2;
	img_cfg->mem.start_x = 0;
	img_cfg->mem.start_y = 0;
	img_cfg->mem.width = cfg->src.w;
	img_cfg->mem.height = cfg->src.h;
	img_cfg->mem.pitch_y = IS_PACKED_FMT(cfg->src_fmt)
				? VIP_ALIGN(3 * cfg->src.w)
				: VIP_ALIGN(cfg->src.w);
	img_cfg->mem.pitch_c = IS_YUV_FMT(cfg->src_fmt)
				? VIP_ALIGN(cfg->src.w >> 1)
				: VIP_ALIGN(cfg->src.w);
}

static void _map_ctrl_to_sc(struct sclr_ctrl_cfg *cfg,
			    struct sclr_core_cfg *sc_cfg, u8 inst)
{
	sc_cfg->sc_bypass = false;
	sc_cfg->gop_bypass = false;
	sc_cfg->dbg_en = false;
	sc_cfg->cir_bypass = true;
	sc_cfg->odma_bypass = false;
	sc_cfg->sc.src = cfg->src;
	sc_cfg->sc.crop = cfg->src_crop;
	sc_cfg->sc.dst = cfg->dst[inst].window;
}

static void _map_ctrl_to_odma(struct sclr_ctrl_cfg *cfg,
			      struct sclr_odma_cfg *odma_cfg, u8 inst)
{
	odma_cfg->flip = SCL_FLIP_NO;
	odma_cfg->burst = g_odma_cfg[inst].burst;
	odma_cfg->fmt = cfg->dst[inst].fmt;
	odma_cfg->csc = cfg->dst[inst].csc;
	odma_cfg->mem.addr0 = cfg->dst[inst].addr0;
	odma_cfg->mem.addr1 = cfg->dst[inst].addr1;
	odma_cfg->mem.addr2 = cfg->dst[inst].addr2;
	odma_cfg->mem.start_x = cfg->dst[inst].offset.x;
	odma_cfg->mem.start_y = cfg->dst[inst].offset.y;
	odma_cfg->mem.width = cfg->dst[inst].window.w;
	odma_cfg->mem.height = cfg->dst[inst].window.h;
	odma_cfg->mem.pitch_y = IS_PACKED_FMT(cfg->dst[inst].fmt) ?
				VIP_ALIGN(3 * cfg->dst[inst].frame.w) :
				VIP_ALIGN(cfg->dst[inst].frame.w);
	odma_cfg->mem.pitch_c = IS_YUV_FMT(cfg->dst[inst].fmt) ?
				VIP_ALIGN(cfg->dst[inst].frame.w >> 1) :
				VIP_ALIGN(cfg->dst[inst].frame.w);
}

int _map_img_to_cmdq(struct sclr_ctrl_cfg *ctrl_cfg, union cmdq_set *cmd_start,
		     u16 cmd_idx)
{
	struct sclr_img_cfg img_cfg;
	struct sclr_csc_cfg *cfg;
	u8 img_inst = ctrl_cfg->img_inst;

	if (img_inst >= SCL_IMG_MAX)
		return cmd_idx;

	_map_ctrl_to_img(ctrl_cfg, &img_cfg);
	cfg = &csc_mtrx[img_cfg.csc];

	cmdQ_set_package(&cmd_start[cmd_idx++].reg, REG_SCL_IMG_CFG(img_inst),
			 (img_cfg.burst << 8) | (img_cfg.fmt << 4) |
			 img_cfg.src);
	cmdQ_set_package(&cmd_start[cmd_idx++].reg,
			 REG_SCL_IMG_OFFSET(img_inst),
			 (img_cfg.mem.start_y << 16) | img_cfg.mem.start_x);
	cmdQ_set_package(&cmd_start[cmd_idx++].reg, REG_SCL_IMG_SIZE(img_inst),
			 ((img_cfg.mem.height - 1) << 16) |
			 (img_cfg.mem.width - 1));
	cmdQ_set_package(&cmd_start[cmd_idx++].reg,
			 REG_SCL_IMG_PITCH_Y(img_inst), img_cfg.mem.pitch_y);
	cmdQ_set_package(&cmd_start[cmd_idx++].reg,
			 REG_SCL_IMG_PITCH_C(img_inst), img_cfg.mem.pitch_c);
	cmdQ_set_package(&cmd_start[cmd_idx++].reg,
			 REG_SCL_IMG_ADDR0_L(img_inst), img_cfg.mem.addr0);
	cmdQ_set_package(&cmd_start[cmd_idx++].reg,
			 REG_SCL_IMG_ADDR0_H(img_inst),
			 img_cfg.mem.addr0 >> 32);
	cmdQ_set_package(&cmd_start[cmd_idx++].reg,
			 REG_SCL_IMG_ADDR1_L(img_inst), img_cfg.mem.addr1);
	cmdQ_set_package(&cmd_start[cmd_idx++].reg,
			 REG_SCL_IMG_ADDR1_H(img_inst),
			 img_cfg.mem.addr1 >> 32);
	cmdQ_set_package(&cmd_start[cmd_idx++].reg,
			 REG_SCL_IMG_ADDR2_L(img_inst), img_cfg.mem.addr2);
	cmdQ_set_package(&cmd_start[cmd_idx++].reg,
			 REG_SCL_IMG_ADDR2_H(img_inst),
			 img_cfg.mem.addr2 >> 32);

	cmdQ_set_package(&cmd_start[cmd_idx++].reg,
			 REG_SCL_IMG_CSC_COEF0(img_inst),
			 (cfg->coef[0][1] << 16) | cfg->coef[0][0]);
	cmdQ_set_package(&cmd_start[cmd_idx++].reg,
			 REG_SCL_IMG_CSC_COEF1(img_inst), cfg->coef[0][2]);
	cmdQ_set_package(&cmd_start[cmd_idx++].reg,
			 REG_SCL_IMG_CSC_COEF2(img_inst),
			 (cfg->coef[1][1] << 16) | cfg->coef[1][0]);
	cmdQ_set_package(&cmd_start[cmd_idx++].reg,
			 REG_SCL_IMG_CSC_COEF3(img_inst), cfg->coef[1][2]);
	cmdQ_set_package(&cmd_start[cmd_idx++].reg,
			 REG_SCL_IMG_CSC_COEF4(img_inst),
			 (cfg->coef[2][1] << 16) | cfg->coef[2][0]);
	cmdQ_set_package(&cmd_start[cmd_idx++].reg,
			 REG_SCL_IMG_CSC_COEF5(img_inst), cfg->coef[2][2]);
	cmdQ_set_package(&cmd_start[cmd_idx++].reg,
			 REG_SCL_IMG_CSC_SUB(img_inst),
			 (cfg->sub[2] << 16) | (cfg->sub[1] << 8)
			  | cfg->sub[0]);
	cmdQ_set_package(&cmd_start[cmd_idx++].reg,
			 REG_SCL_IMG_CSC_ADD(img_inst),
			 (cfg->add[2] << 16) | (cfg->add[1] << 8)
			  | cfg->add[0]);

	return cmd_idx;
}

int _map_sc_to_cmdq(struct sclr_ctrl_cfg *ctrl_cfg, u8 j,
		    union cmdq_set *cmd_start, u16 cmd_idx)
{
	u32 tmp = 0;
	u8 inst = ctrl_cfg->dst[j].inst;
	struct sclr_core_cfg sc_cfg;
	struct sclr_odma_cfg odma_cfg;

	if (inst >= SCL_MAX_INST)
		return cmd_idx;

	_map_ctrl_to_sc(ctrl_cfg, &sc_cfg, j);
	_map_ctrl_to_odma(ctrl_cfg, &odma_cfg, j);

	// sc core
	tmp = 0;
	if (sc_cfg.sc_bypass)
		tmp |= 0x02;
	if (sc_cfg.gop_bypass)
		tmp |= 0x04;
	if (sc_cfg.dbg_en)
		tmp |= 0x10;
	if (sc_cfg.cir_bypass)
		tmp |= 0x20;
	if (sc_cfg.odma_bypass)
		tmp |= 0x40;

	cmdQ_set_package(&cmd_start[cmd_idx++].reg, REG_SCL_CFG(inst), tmp);
	cmdQ_set_package(&cmd_start[cmd_idx++].reg, REG_SCL_SRC_SIZE(inst),
			 ((sc_cfg.sc.src.h - 1) << 12) | (sc_cfg.sc.src.w - 1));
	cmdQ_set_package(&cmd_start[cmd_idx++].reg, REG_SCL_CROP_OFFSET(inst),
			 (sc_cfg.sc.crop.y << 12) | sc_cfg.sc.crop.x);
	cmdQ_set_package(&cmd_start[cmd_idx++].reg, REG_SCL_CROP_SIZE(inst),
			 ((sc_cfg.sc.crop.h - 1) << 16)
			  | (sc_cfg.sc.crop.w - 1));
	cmdQ_set_package(&cmd_start[cmd_idx++].reg, REG_SCL_OUT_SIZE(inst),
			 ((sc_cfg.sc.dst.h - 1) << 16) | (sc_cfg.sc.dst.w - 1));
	tmp = ((sc_cfg.sc.crop.w - 1) << 13) / (sc_cfg.sc.dst.w - 1);
	cmdQ_set_package(&cmd_start[cmd_idx++].reg, REG_SCL_SC_H_CFG(inst),
			 tmp << 8);
	tmp = ((sc_cfg.sc.crop.h - 1) << 13) / (sc_cfg.sc.dst.h - 1);
	cmdQ_set_package(&cmd_start[cmd_idx++].reg, REG_SCL_SC_V_CFG(inst),
			 tmp << 8);

	// sc odma
	cmdQ_set_package(&cmd_start[cmd_idx++].reg, REG_SCL_ODMA_CFG(inst),
			 (odma_cfg.flip << 16) |  (odma_cfg.fmt << 8) |
			 odma_cfg.burst);
	cmdQ_set_package(&cmd_start[cmd_idx++].reg, REG_SCL_ODMA_OFFSET_X(inst),
			 odma_cfg.mem.start_x);
	cmdQ_set_package(&cmd_start[cmd_idx++].reg, REG_SCL_ODMA_OFFSET_Y(inst),
			 odma_cfg.mem.start_y);
	cmdQ_set_package(&cmd_start[cmd_idx++].reg, REG_SCL_ODMA_WIDTH(inst),
			 odma_cfg.mem.width - 1);
	cmdQ_set_package(&cmd_start[cmd_idx++].reg, REG_SCL_ODMA_HEIGHT(inst),
			 odma_cfg.mem.height - 1);
	cmdQ_set_package(&cmd_start[cmd_idx++].reg, REG_SCL_ODMA_PITCH_Y(inst),
			 odma_cfg.mem.pitch_y);
	cmdQ_set_package(&cmd_start[cmd_idx++].reg, REG_SCL_ODMA_PITCH_C(inst),
			 odma_cfg.mem.pitch_c);
	cmdQ_set_package(&cmd_start[cmd_idx++].reg, REG_SCL_ODMA_ADDR0_L(inst),
			 odma_cfg.mem.addr0);
	cmdQ_set_package(&cmd_start[cmd_idx++].reg, REG_SCL_ODMA_ADDR0_H(inst),
			 odma_cfg.mem.addr0 >> 32);
	cmdQ_set_package(&cmd_start[cmd_idx++].reg, REG_SCL_ODMA_ADDR1_L(inst),
			 odma_cfg.mem.addr1);
	cmdQ_set_package(&cmd_start[cmd_idx++].reg, REG_SCL_ODMA_ADDR1_H(inst),
			 odma_cfg.mem.addr1 >> 32);
	cmdQ_set_package(&cmd_start[cmd_idx++].reg, REG_SCL_ODMA_ADDR2_L(inst),
			 odma_cfg.mem.addr2);
	cmdQ_set_package(&cmd_start[cmd_idx++].reg, REG_SCL_ODMA_ADDR2_H(inst),
			 odma_cfg.mem.addr2 >> 32);

	// sc csc
	if (odma_cfg.csc == SCL_CSC_NONE) {
		cmdQ_set_package(&cmd_start[cmd_idx++].reg,
				 REG_SCL_CSC_EN(inst), 0);
	} else {
		struct sclr_csc_cfg *cfg = &csc_mtrx[odma_cfg.csc];

		cmdQ_set_package(&cmd_start[cmd_idx++].reg,
				 REG_SCL_CSC_EN(inst), 1);

		cmdQ_set_package(&cmd_start[cmd_idx++].reg,
				 REG_SCL_CSC_COEF0(inst),
				 (cfg->coef[0][1] << 16) | cfg->coef[0][0]);
		cmdQ_set_package(&cmd_start[cmd_idx++].reg,
				 REG_SCL_CSC_COEF1(inst),
				 (cfg->coef[1][0] << 16) | cfg->coef[0][2]);
		cmdQ_set_package(&cmd_start[cmd_idx++].reg,
				 REG_SCL_CSC_COEF2(inst),
				 (cfg->coef[1][2] << 16) | cfg->coef[1][1]);
		cmdQ_set_package(&cmd_start[cmd_idx++].reg,
				 REG_SCL_CSC_COEF3(inst),
				 (cfg->coef[2][1] << 16) | cfg->coef[2][0]);
		cmdQ_set_package(&cmd_start[cmd_idx++].reg,
				 REG_SCL_CSC_COEF4(inst), cfg->coef[2][2]);

		cmdQ_set_package(&cmd_start[cmd_idx++].reg,
				 REG_SCL_CSC_OFFSET(inst),
				 (cfg->add[2] << 16) | (cfg->add[1] << 8) |
				 cfg->add[0]);
	}

	return cmd_idx;
}
/**
 * sclr_engine_cmdq - trigger sc  by cmdq
 *
 * @param cfgs: settings for cmdq
 * @param cnt: the number of settings
 * @param cmdq_addr: memory-address to put cmdq
 */
void sclr_engine_cmdq(struct sclr_ctrl_cfg *cfgs, u8 cnt, u64 cmdq_addr)
{
	u8 i = 0, j = 0;
	u16 cmd_idx = 0;
	union cmdq_set *cmd_start = (union cmdq_set *)cmdq_addr;
	u32 tmp = 0, inst_enable = 0;

	memset((void *)cmdq_addr, 0, sizeof(u32)*0x100);

	_reg_write(reg_base + REG_SCL_TOP_CMDQ_START, 0x18000);
	_reg_write(reg_base + REG_SCL_TOP_CMDQ_STOP, 0x18000);

	for (i = 0; i < cnt; ++i) {
		struct sclr_ctrl_cfg *cfg = (cfgs + i);
		u8 img_inst = cfg->img_inst;

		cmd_idx = _map_img_to_cmdq(cfg, cmd_start, cmd_idx);

		for (j = 0; j < SCL_MAX_INST; ++j)
			cmd_idx = _map_sc_to_cmdq(cfg, j, cmd_start, cmd_idx);

		img_inst = (~img_inst) & 0x01;
		// modify & clear sc-cmdq's stop flag
		tmp = 0x10000 | BIT(img_inst+1);
		inst_enable = 0;
		for (j = 0; j < SCL_MAX_INST; ++j) {
			u8 inst = cfg->dst[j].inst;

			if (inst < SCL_MAX_INST) {
				tmp |= BIT(inst + 3);
				inst_enable |= BIT(inst);
			}
		}
		cmdQ_set_package(&cmd_start[cmd_idx++].reg,
				 REG_SCL_TOP_CMDQ_STOP, tmp);

		tmp = _reg_read(reg_base + REG_SCL_TOP_CFG1);
		tmp &= ~0x1f;
		tmp |= inst_enable;
		cmdQ_set_package(&cmd_start[cmd_idx++].reg, REG_SCL_TOP_CFG1,
				 tmp);

		// start sclr
		cmdQ_set_package(&cmd_start[cmd_idx++].reg,
				 REG_SCL_TOP_IMG_CTRL, BIT(img_inst));

		// wait end-inter(1)
		cmdQ_set_wait(&cmd_start[cmd_idx++], false, 1, 0);
	}
	// clear intr
	cmdQ_set_package(&cmd_start[cmd_idx++].reg, REG_SCL_TOP_INTR_STATUS,
			 0x1000ffff);

	cmd_start[cmd_idx-1].reg.intr_end = 1;
	cmd_start[cmd_idx-1].reg.intr_last = 1;

	cmdQ_intr_ctrl(REG_SCL_CMDQ_BASE, 0x02);
	cmdQ_engine(REG_SCL_CMDQ_BASE, (uintptr_t)cmdq_addr,
		    REG_SCL_TOP_BASE >> 22, true, false, cmd_idx);
}
