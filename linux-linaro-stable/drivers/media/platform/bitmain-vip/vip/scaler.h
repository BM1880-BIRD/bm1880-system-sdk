#ifndef _BMD_SCL_H_
#define _BMD_SCL_H_

#define SCL_INTR_SCL_NUM 139
#define SCL_MAX_INST 4
#define SCL_MIN_WIDTH  32
#define SCL_MIN_HEIGHT 32
#define SCL_MAX_WIDTH  2688
#define SCL_MAX_HEIGHT 2688

#define IS_YUV_FMT(x) \
	((x == SCL_FMT_YUV420) || (x == SCL_FMT_YUV422) || \
	 (x == SCL_FMT_Y_ONLY))
#define IS_PACKED_FMT(x) \
	((x == SCL_FMT_RGB_PACKED) || (x == SCL_FMT_BGR_PACKED))

struct sclr_size {
	u16 w;
	u16 h;
};

struct sclr_point {
	u16 x;
	u16 y;
};

struct sclr_rect {
	u16 x;
	u16 y;
	u16 w;
	u16 h;
};

struct sclr_status {
	u8 crop_idle : 1;
	u8 hscale_idle : 1;
	u8 vscale_idle : 1;
	u8 gop_idle : 1;
	u8 csc_idle : 1;
	u8 wdma_idle : 1;
	u8 rsv : 2;
};

enum sclr_img_in {
	SCL_IMG_V,  // for video-encoder
	SCL_IMG_D,  // for display
	SCL_IMG_MAX,
};

enum sclr_input_d {
	SCL_INPUT_D_ISP,
	SCL_INPUT_D_MAP,
	SCL_INPUT_D_MEM,
	SCL_INPUT_D_IMG_V,
	SCL_INPUT_D_MAX,
};

enum sclr_input_v {
	SCL_INPUT_V_ISP,
	SCL_INPUT_V_DWA,
	SCL_INPUT_V_MEM,
	SCL_INPUT_V_MAX,
};

union sclr_input {
	enum sclr_input_d d;
	enum sclr_input_v v;
};

enum sclr_cir_mode {
	SCL_CIR_EMPTY,
	SCL_CIR_SHAPE = 2,
	SCL_CIR_LINE,
	SCL_CIR_DISABLE,
	SCL_CIR_MAX,
};

struct sclr_csc_cfg {
	u16 coef[3][3];
	u8 sub[3];
	u8 add[3];
};

struct sclr_quant_cfg {
	u16 sc[3];
	u8 sub[3];
	u16 sub_frac[3];
};

union sclr_intr {
	struct {
		u16 disp_frame_start : 1;
		u16 disp_frame_end : 1;
		u16 img_in_d_frame_start : 1;
		u16 img_in_d_frame_end : 1;
		u16 img_in_v_frame_start : 1;
		u16 img_in_v_frame_end : 1;
		u16 scl0_frame_end : 1;
		u16 scl1_frame_end : 1;
		u16 scl2_frame_end : 1;
		u16 scl3_frame_end : 1;
		u16 prog_too_late : 1;
		u16 map_cv_frame_end : 1;
		u16 cmdq : 1;
		u16 cmdq_start : 1;
		u16 cmdq_end : 1;
	} b;
	u16 raw;
};

enum sclr_format {
	SCL_FMT_YUV420,
	SCL_FMT_YUV422,
	SCL_FMT_RGB_PLANAR,
	SCL_FMT_RGB_PACKED, // B lsb
	SCL_FMT_BGR_PACKED, // R lsb
	SCL_FMT_Y_ONLY,
	SCL_FMT_MAX
};

enum sclr_out_mode {
	SCL_OUT_CSC,
	SCL_OUT_QUANT,
	SCL_OUT_HSV,
	SCL_OUT_DISABLE
};

enum sclr_csc {
	SCL_CSC_NONE,
	SCL_CSC_601_LIMIT_YUV2RGB,
	SCL_CSC_601_FULL_YUV2RGB,
	SCL_CSC_709_LIMIT_YUV2RGB,
	SCL_CSC_709_FULL_YUV2RGB,
	SCL_CSC_601_LIMIT_RGB2YUV,
	SCL_CSC_601_FULL_RGB2YUV,
	SCL_CSC_709_LIMIT_RGB2YUV,
	SCL_CSC_709_FULL_RGB2YUV,
	SCL_CSC_MAX,
};

enum sclr_quant_rounding {
	SCL_QUANT_ROUNDING_TO_EVEN = 0,
	SCL_QUANT_ROUNDING_AWAY_FROM_ZERO = 1,
	SCL_QUANT_ROUNDING_TRUNCATE = 2,
	SCL_QUANT_ROUNDING_MAX,
};

enum sclr_flip_mode {
	SCL_FLIP_NO,
	SCL_FLIP_HFLIP,
	SCL_FLIP_VFLIP,
	SCL_FLIP_HVFLIP,
	SCL_FLIP_MAX
};

enum sclr_gop_format {
	SCL_GOP_FMT_ARGB8888,
	SCL_GOP_FMT_ARGB4444,
	SCL_GOP_FMT_ARGB1555,
	SCL_GOP_FMT_256LUT,
	SCL_GOP_FMT_FONT,
	SCL_GOP_FMT_MAX
};

struct sclr_top_cfg {
	bool ip_trig_src;    // 0(IMG_V), 1(IMG_D)
	bool sclr_enable[4];
	bool disp_enable;
	bool disp_from_sc;   // 0(DRAM), 1(SCL_D)
	bool sclr_d_src;     // 0(IMG_D), 1(IMG_V)
	bool img_in_d_trig_src;  // 0(SW), 1(disp_tgen_vs)
	bool img_in_v_trig_src;  // 0(SW), 1(disp_tgen_vs)
};

struct sclr_mem {
	u64 addr0;
	u64 addr1;
	u64 addr2;
	u16 pitch_y;
	u16 pitch_c;
	u16 start_x;
	u16 start_y;
	u16 width;
	u16 height;
};

struct sclr_img_cfg {
	u8 src;      // 0(ISP), 1(map_cnv if img_d/dwa if img_v), ow(DRAM)
	enum sclr_csc csc;
	u8 burst;       // 0~15
	enum sclr_format fmt;
	struct sclr_mem mem;
};

union sclr_img_dbg_status {
	struct {
		u32 err_fwr_y   : 1;
		u32 err_fwr_u   : 1;
		u32 err_fwr_v   : 1;
		u32 err_fwr_clr : 1;
		u32 err_erd_y   : 1;
		u32 err_erd_u   : 1;
		u32 err_erd_v   : 1;
		u32 err_erd_clr : 1;
		u32 lb_full_y   : 1;
		u32 lb_full_u   : 1;
		u32 lb_full_v   : 1;
		u32 resv1       : 1;
		u32 lb_empty_y  : 1;
		u32 lb_empty_u  : 1;
		u32 lb_empty_v  : 1;
		u32 resv2       : 1;
		u32 ip_idle     : 1;
		u32 ip_int      : 1;
		u32 ip_clr      : 1;
		u32 ip_int_clr  : 1;
		u32 resv        : 13;
	} b;
	u32 raw;
};

struct sclr_cir_cfg {
	enum sclr_cir_mode mode;
	u8 line_width;
	struct sclr_point center;
	u16 radius;
	struct sclr_rect rect;
	u8 color_r;
	u8 color_g;
	u8 color_b;
};

struct sclr_border_cfg {
	union {
		struct {
			u32 bd_color_r	: 8;
			u32 bd_color_g	: 8;
			u32 bd_color_b	: 8;
			u32 resv	: 7;
			u32 enable	: 1;
		} b;
		u32 raw;
	} cfg;
	struct sclr_point start;
};

struct sclr_gop_ow_cfg {
	enum sclr_gop_format fmt;
	struct sclr_point start;
	struct sclr_point end;
	u64 addr;
	u16 pitch;
	struct sclr_size mem_size;
	struct sclr_size img_size;
};

struct sclr_gop_cfg {
	union {
		struct {
			u16 ow0_en : 1;
			u16 ow1_en : 1;
			u16 ow2_en : 1;
			u16 ow3_en : 1;
			u16 ow4_en : 1;
			u16 ow5_en : 1;
			u16 ow6_en : 1;
			u16 ow7_en : 1;
			u16 hscl_en: 1;
			u16 vscl_en: 1;
			u16 colorkey_en : 1;
			u16 resv   : 1;
			u16 burst  : 4;
		} b;
		u16 raw;
	};
	u32 colorkey;       // RGB888
	u16 font_fg_color;  // ARGB4444
	u16 font_bg_color;  // ARGB4444
	struct sclr_gop_ow_cfg ow_cfg[8];
};

struct sclr_odma_cfg {
	enum sclr_flip_mode flip;
	enum sclr_out_mode mode;
	enum sclr_csc csc;
	bool burst;     // burst(0: 8, 1:16)
	enum sclr_format fmt;
	struct sclr_mem mem;
};

union sclr_odma_dbg_status {
	struct {
		u32 axi_status  : 4;
		u32 v_buf_empty : 1;
		u32 v_buf_full  : 1;
		u32 u_buf_empty : 1;
		u32 u_buf_full  : 1;
		u32 y_buf_empty : 1;
		u32 y_buf_full  : 1;
		u32 v_axi_active: 1;
		u32 u_axi_active: 1;
		u32 y_axi_active: 1;
		u32 axi_active  : 1;
		u32 resv        : 18;
	} b;
	u32 raw;
};

struct sclr_rot_cfg {
	bool burst;     // burst(0: 8, 1:16)
	enum sclr_format fmt;
	u64 src_addr;
	u64 dst_addr;
	u32 src_pitch;
	u32 dst_pitch;
	struct sclr_point src_offset;
	struct sclr_point dst_offset;
	struct sclr_size size;
};

struct sclr_scale_cfg {
	struct sclr_size src;
	struct sclr_rect crop;
	struct sclr_size dst;
};

struct sclr_core_cfg {
	bool sc_bypass;
	bool gop_bypass;
	bool dbg_en;
	bool cir_bypass;
	bool odma_bypass;
	struct sclr_scale_cfg sc;
};

enum sclr_disp_pat_color {
	SCL_PAT_COLOR_WHITE,
	SCL_PAT_COLOR_RED,
	SCL_PAT_COLOR_GREEN,
	SCL_PAT_COLOR_BLUE,
	SCL_PAT_COLOR_CYAN,
	SCL_PAT_COLOR_MAGENTA,
	SCL_PAT_COLOR_YELLOW,
	SCL_PAT_COLOR_BAR,
	SCL_PAT_COLOR_MAX
};

enum sclr_disp_pat_type {
	SCL_PAT_TYPE_FULL,
	SCL_PAT_TYPE_H_GRAD,
	SCL_PAT_TYPE_V_GRAD,
	SCL_PAT_TYPE_AUTO,
	SCL_PAT_TYPE_SNOW,
	SCL_PAT_TYPE_FIX,
	SCL_PAT_TYPE_OFF,
	SCL_PAT_TYPE_MAX
};

struct sclr_disp_cfg {
	bool disp_from_sc;  // 0(DRAM), 1(scaler_d)
	bool sync_ext;
	bool tgen_en;
	bool dw1_en;
	bool dw2_en;
	enum sclr_format fmt;
	enum sclr_csc in_csc;
	enum sclr_csc out_csc;
	u8 burst;       // 0~15
	struct sclr_mem mem;
	struct sclr_gop_cfg gop_cfg;
};

struct sclr_disp_timing {
	bool vsync_pol;
	bool hsync_pol;
	u16 vtotal;
	u16 htotal;
	u16 vsync_start;
	u16 vsync_end;
	u16 vfde_start;
	u16 vfde_end;
	u16 vmde_start;
	u16 vmde_end;
	u16 hsync_start;
	u16 hsync_end;
	u16 hfde_start;
	u16 hfde_end;
	u16 hmde_start;
	u16 hmde_end;
};

union sclr_disp_dbg_status {
	struct {
		u32 bw_fail     : 1;
		u32 bw_fail_clr : 1;
		u32 resv0       : 2;
		u32 err_fwr_y   : 1;
		u32 err_fwr_u   : 1;
		u32 err_fwr_v   : 1;
		u32 err_fwr_clr : 1;
		u32 err_erd_y   : 1;
		u32 err_erd_u   : 1;
		u32 err_erd_v   : 1;
		u32 err_erd_clr : 1;
		u32 lb_full_y   : 1;
		u32 lb_full_u   : 1;
		u32 lb_full_v   : 1;
		u32 resv1       : 1;
		u32 lb_empty_y  : 1;
		u32 lb_empty_u  : 1;
		u32 lb_empty_v  : 1;
		u32 resv2       : 13;
	} b;
	u32 raw;
};

struct sclr_ctrl_cfg {
	enum sclr_img_in img_inst;
	union sclr_input input;
	enum sclr_format src_fmt;
	enum sclr_csc src_csc;
	struct sclr_size src;
	struct sclr_rect src_crop;
	u64 src_addr0;
	u64 src_addr1;
	u64 src_addr2;

	struct {
		u8 inst;
		enum sclr_format fmt;
		enum sclr_csc csc;
		struct sclr_size frame;
		struct sclr_size window;
		struct sclr_point offset;
		u64 addr0;
		u64 addr1;
		u64 addr2;
	} dst[4];
};

/**
 * @ out_bit: 0(6-bit), 1(8-bit), others(10-bit)
 * @ vesa_mode: 0(JEIDA), 1(VESA)
 * @ dual_ch: dual link
 * @ vs_out_en: vs output enable
 * @ hs_out_en: hs output enable
 * @ hs_blk_en: vertical blanking hs output enable
 * @ ml_swap: lvdstx hs data msb/lsb swap
 * @ ctrl_rev: serializer 0(msb first), 1(lsb first)
 * @ oe_swap: lvdstx even/odd link swap
 * @ en: lvdstx enable
 */
union sclr_lvdstx {
	struct {
		u32 out_bit	: 2;
		u32 vesa_mode	: 1;
		u32 dual_ch	: 1;
		u32 vs_out_en	: 1;
		u32 hs_out_en	: 1;
		u32 hs_blk_en	: 1;
		u32 resv_1	: 1;
		u32 ml_swap	: 1;
		u32 ctrl_rev	: 1;
		u32 oe_swap	: 1;
		u32 en		: 1;
		u32 resv	: 20;
	} b;
	u32 raw;
};

/**
 * @fmt_sel: [0] clk select
 *		0: bt clock 2x of disp clock
 *		1: bt clock 2x of disp clock
 *	     [1] sync signal index
 *		0: with sync pattern
 *		1: without sync pattern
 * @hde_gate: gate output hde with vde
 * @data_seq: fmt_sel[0] = 0
 *		00: Cb0Y0Cr0Y1
 *		01: Cr0Y0Cb0Y1
 *		10: Y0Cb0Y1Cr0
 *		11: Y0Cr0Y1Cb0
 *	      fmt_sel[0] = 1
 *		0: Cb0Cr0
 *		1: Cr0Cb0
 * @clk_inv: clock rising edge at middle of data
 * @vs_inv: vs low active
 * @hs_inv: hs low active
 */
union sclr_bt_enc {
	struct {
		u32 fmt_sel	: 2;
		u32 resv_1	: 1;
		u32 hde_gate	: 1;
		u32 data_seq	: 2;
		u32 resv_2	: 2;
		u32 clk_inv	: 1;
		u32 hs_inv	: 1;
		u32 vs_inv	: 1;
	} b;
	u32 raw;
};

/**
 * @ sav_vld: sync pattern for start of valid data
 * @ sav_blk: sync pattern for start of blanking data
 * @ eav_vld: sync pattern for end of valid data
 * @ eav_blk: sync pattern for end of blanking data
 */
union sclr_bt_sync_code {
	struct {
		u8 sav_vld;
		u8 sav_blk;
		u8 eav_vld;
		u8 eav_blk;
	} b;
	u32 raw;
};

enum sclr_vo_sel {
	SCLR_VO_SEL_DISABLE,
	SCLR_VO_SEL_BT,
	SCLR_VO_SEL_I80,
	SCLR_VO_SEL_SW,
	SCLR_VO_SEL_MAX,
};

void sclr_set_base_addr(void *base);
void sclr_top_set_cfg(struct sclr_top_cfg *cfg);
struct sclr_top_cfg *sclr_top_get_cfg(void);
void sclr_top_reg_done(void);
void sclr_top_reg_force_up(void);
u8 sclr_top_pg_late_get_bus(void);
void sclr_top_pg_late_clr(void);
void sclr_reg_force_up(u8 inst);
void sclr_set_cfg(u8 inst, bool sc_bypass, bool gop_bypass, bool dbg_en,
		  bool cir_bypass, bool odma_bypass);
struct sclr_core_cfg *sclr_get_cfg(u8 inst);
void sclr_set_input_size(u8 inst, struct sclr_size src_rect);
void sclr_set_crop(u8 inst, struct sclr_rect crop_rect);
void sclr_set_output_size(u8 inst, struct sclr_size rect);
void sclr_set_scale(u8 inst);
struct sclr_status sclr_get_status(u8 inst);
void sclr_img_set_cfg(u8 inst, struct sclr_img_cfg *cfg);
struct sclr_img_cfg *sclr_img_get_cfg(u8 inst);
void sclr_img_reg_force_up(u8 inst);
void sclr_img_start(u8 inst);
void sclr_img_set_fmt(u8 inst, enum sclr_format fmt);
void sclr_img_set_mem(u8 inst, struct sclr_mem *mem);
void sclr_img_set_addr(u8 inst, u64 addr0, u64 addr1, u64 addr2);
void sclr_img_set_csc(u8 inst, struct sclr_csc_cfg *cfg);
union sclr_img_dbg_status sclr_img_get_dbg_status(u8 inst, bool clr);
void sclr_cir_set_cfg(u8 inst, struct sclr_cir_cfg *cfg);
void sclr_odma_set_cfg(u8 inst, struct sclr_odma_cfg *cfg);
struct sclr_odma_cfg *sclr_odma_get_cfg(u8 inst);
void sclr_odma_set_fmt(u8 inst, enum sclr_format fmt);
void sclr_odma_set_mem(u8 inst, struct sclr_mem *mem);
void sclr_odma_set_addr(u8 inst, u64 addr0, u64 addr1, u64 addr2);
union sclr_odma_dbg_status sclr_odma_get_dbg_status(u8 inst);
void sclr_set_out_mode(u8 inst, enum sclr_out_mode mode);
void sclr_set_quant(u8 inst, struct sclr_quant_cfg *cfg);
void sclr_set_quant_drop_mode(u8 inst, enum sclr_quant_rounding mode);
void sclr_border_set_cfg(u8 inst, struct sclr_border_cfg *cfg);
struct sclr_border_cfg *sclr_border_get_cfg(u8 inst);
void sclr_set_csc(u8 inst, struct sclr_csc_cfg *cfg);
void sclr_get_csc(u8 inst, struct sclr_csc_cfg *cfg);
void sclr_intr_ctrl(union sclr_intr intr_mask);
union sclr_intr sclr_get_intr_mask(void);
void sclr_set_intr_mask(union sclr_intr intr_mask);
void sclr_intr_clr(union sclr_intr intr_mask);
union sclr_intr sclr_intr_status(void);
void sclr_disp_set_cfg(struct sclr_disp_cfg *cfg);
struct sclr_disp_cfg *sclr_disp_get_cfg(void);
void sclr_disp_set_timing(struct sclr_disp_timing *timing);
int sclr_disp_set_rect(struct sclr_rect rect);
void sclr_disp_set_mem(struct sclr_mem *mem);
void sclr_disp_set_addr(u64 addr0, u64 addr1, u64 addr2);
void sclr_disp_set_in_csc(enum sclr_csc csc);
void sclr_disp_set_out_csc(enum sclr_csc csc);
void sclr_disp_set_pattern(enum sclr_disp_pat_type type,
			   enum sclr_disp_pat_color color);
void sclr_disp_set_frame_bgcolor(u16 r, u16 g, u16 b);
void sclr_disp_set_window_bgcolor(u16 r, u16 g, u16 b);
bool sclr_disp_tgen_enable(bool enable);
union sclr_disp_dbg_status sclr_disp_get_dbg_status(bool clr);
void sclr_lvdstx_set(union sclr_lvdstx cfg);
void sclr_lvdstx_get(union sclr_lvdstx *cfg);
void sclr_bt_set(union sclr_bt_enc enc, union sclr_bt_sync_code sync);
void sclr_bt_get(union sclr_bt_enc *enc, union sclr_bt_sync_code *sync);

void sclr_gop_set_cfg(u8 inst, struct sclr_gop_cfg *cfg);
struct sclr_gop_cfg *sclr_gop_get_cfg(u8 inst);
void sclr_gop_ow_set_cfg(u8 inst, u8 ow_inst, struct sclr_gop_ow_cfg *cfg);
void sclr_gop_setup_LUT(u8 inst, u16 *data);

void sclr_ctrl_init(void);
void sclr_ctrl_set_scale(u8 inst, struct sclr_scale_cfg *cfg);
int sclr_ctrl_set_input(enum sclr_img_in inst, union sclr_input input,
			enum sclr_format fmt, enum sclr_csc csc);
int sclr_ctrl_set_output(u8 inst, enum sclr_out_mode mode,
			 enum sclr_format fmt, void *param);
int sclr_ctrl_set_disp_src(bool disp_from_sc);
void sclr_engine_cmdq(struct sclr_ctrl_cfg *cfg, u8 cnt, u64 cmdq_addr);

#endif  //_BMD_SCL_H_
