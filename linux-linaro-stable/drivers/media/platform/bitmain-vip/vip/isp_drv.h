#ifndef _ISP_DRV_H_
#define _ISP_DRV_H_


#define _OFST(_BLK_T, _REG)       ((uint64_t)&(((struct _BLK_T *)0)->_REG))

#define ISP_RD_REG(_BA, _BLK_T, _REG) \
	(_reg_read(_BA+_OFST(_BLK_T, _REG)))

#define ISP_RD_BITS(_BA, _BLK_T, _REG, _FLD) \
	({\
		typeof(((struct _BLK_T *)0)->_REG) _r;\
		_r.raw = _reg_read(_BA+_OFST(_BLK_T, _REG));\
		_r.bits._FLD;\
	})

#define ISP_WR_REG(_BA, _BLK_T, _REG, _V) \
	(_reg_write((_BA+_OFST(_BLK_T, _REG)), _V))

#define ISP_WR_BITS(_BA, _BLK_T, _REG, _FLD, _V) \
	do {\
		typeof(((struct _BLK_T *)0)->_REG) _r;\
		_r.raw = _reg_read(_BA+_OFST(_BLK_T, _REG));\
		_r.bits._FLD = _V;\
		_reg_write((_BA+_OFST(_BLK_T, _REG)), _r.raw);\
	} while (0)

#define ISP_WO_BITS(_BA, _BLK_T, _REG, _FLD, _V) \
	do {\
		typeof(((struct _BLK_T *)0)->_REG) _r;\
		_r.raw = 0;\
		_r.bits._FLD = _V;\
		_reg_write((_BA+_OFST(_BLK_T, _REG)), _r.raw);\
	} while (0)



enum ISP_RGB_PROB_OUT {
	ISP_RGB_PROB_OUT_CFA = 0,
	ISP_RGB_PROB_OUT_RGBEE,
	ISP_RGB_PROB_OUT_CCM,
	ISP_RGB_PROB_OUT_GMA,
	ISP_RGB_PROB_OUT_DHZ,
	ISP_RGB_PROB_OUT_HSV,
	ISP_RGB_PROB_OUT_RGBDITHER,
	ISP_RGB_PROB_OUT_CSC,
	ISP_RGB_PROB_OUT_MAX,
};

/*
 * To indicate the 1st two pixel in the bayer_raw.
 */
enum ISP_BAYER_TYPE {
	ISP_BAYER_TYPE_BG = 0,
	ISP_BAYER_TYPE_GB,
	ISP_BAYER_TYPE_GR,
	ISP_BAYER_TYPE_RG,
	ISP_BAYER_TYPE_MAX,
};

enum ISP_BNR_OUT {
	ISP_BNR_OUT_BYPASS = 0,
	ISP_BNR_OUT_B_DELAY,
	ISP_BNR_OUT_FACTOR,
	ISP_BNR_OUT_B_NL,
	ISP_BNR_OUT_RESV_0,
	ISP_BNR_OUT_RESV_1,
	ISP_BNR_OUT_RESV_2,
	ISP_BNR_OUT_RESV_3,
	ISP_BNR_OUT_B_OUT,
	ISP_BNR_OUT_INTENSITY,
	ISP_BNR_OUT_DELTA,
	ISP_BNR_OUT_NOT_SM,
	ISP_BNR_OUT_FLAG_V,
	ISP_BNR_OUT_FLAG_H,
	ISP_BNR_OUT_FLAG_D45,
	ISP_BNR_OUT_FLAG_D135,
	ISP_BNR_OUT_MAX,
};

enum ISP_YNR_OUT {
	ISP_YNR_OUT_BYPASS = 0,
	ISP_YNR_OUT_Y_DELAY,
	ISP_YNR_OUT_FACTOR,
	ISP_YNR_OUT_ALPHA,
	ISP_YNR_OUT_Y_BF,
	ISP_YNR_OUT_Y_NL,
	ISP_YNR_OUT_RESV_0,
	ISP_YNR_OUT_RESV_1,
	ISP_YNR_OUT_Y_OUT,
	ISP_YNR_OUT_INTENSITY,
	ISP_YNR_OUT_DELTA,
	ISP_YNR_OUT_NOT_SM,
	ISP_YNR_OUT_FLAG_V,
	ISP_YNR_OUT_FLAG_H,
	ISP_YNR_OUT_FLAG_D45,
	ISP_YNR_OUT_FLAG_D135,
	ISP_YNR_OUT_MAX,
};

enum ISP_FS_OUT {
	ISP_FS_OUT_FS = 0,
	ISP_FS_OUT_LONG,
	ISP_FS_OUT_SHORT,
	ISP_FS_OUT_SHORT_EX,
	ISP_FS_OUT_MAX,
};

struct isp_param {
	uint32_t img_width;
	uint32_t img_height;
	uint32_t img_plane;
	uint32_t img_format;
	uint32_t img_stride;
};

enum ISPCQ_ID_T {
	ISPCQ_ID_PRERAW0 = 0,
	ISPCQ_ID_PRERAW1,
	ISPCQ_ID_POSTRAW,
	ISPCQ_ID_MAX
};

enum ISPCQ_OP_MODE {
	ISPCQ_OP_SINGLE_CMDSET,
	ISPCQ_OP_ADMA,
};

struct ispcq_config {
	uint32_t op_mode;
	uint32_t intr_en;

	enum ISPCQ_ID_T cq_id;
	union {
		uint64_t adma_table_pa;
		uint64_t cmdset_pa;
	};
	uint32_t cmdset_size;
};

struct isp_vblock_info {
	uint32_t block_id;
	uint32_t block_size;
	uint64_t reg_base;
};

struct isp_dma_cfg {
	uint16_t width;
	uint16_t height;
	uint16_t stride;
	uint16_t format;
};


/*
 * @img_width: width of image
 * @img_height: height of image
 * @pyhs_regs: index by enum ISP_BLK_ID_T, always phys reg
 * @vreg_bases: index by enum ISP_BLK_ID_T
 * @vreg_bases_pa: index by enum ISP_BLK_ID_T
 *
 * @rgb_color_mode: bayer_raw type
 *
 * @cam_id: preraw(0,1)
 * @is_offline_preraw: preraw src offline(from dram)
 * @is_offline_postraw: postraw src offline(from dram)
 */
struct isp_ctx {
	struct isp_param	inparm;
	uint32_t		img_width;
	uint32_t		img_height;

	uint64_t		*phys_regs;
	uintptr_t		*vreg_bases;
	uintptr_t		*vreg_bases_pa;
	uintptr_t		adma_table[ISPCQ_ID_MAX];
	uintptr_t		adma_table_pa[ISPCQ_ID_MAX];

	enum ISP_BAYER_TYPE	rgb_color_mode;
	uint8_t			csibdg_patgen_en;
	uint8_t			sensor_bitdepth;

	uint8_t			cam_id;
	uint32_t		is_yuv_sensor       : 1;
	uint32_t		is_hdr_on           : 1;
	uint32_t		is_offline_preraw   : 1;
	uint32_t		is_offline_postraw  : 1;
	uint32_t		is_offline_scaler   : 1;
	uint32_t		is_preraw0_done     : 1;
	uint32_t		is_preraw1_done     : 1;
	uint32_t		is_postraw_done     : 1;

	uint32_t		vreg_page_idx;
};

void isp_set_base_addr(void *base);

/**
 * isp_init - setup isp
 *
 * @param :
 */
void isp_init(struct isp_ctx *ctx);

/**
 * isp_uninit - clear isp setting
 *
 * @param :
 */
void isp_uninit(struct isp_ctx *ctx);


/**
 * isp_reset - do reset. This can be activated only if dma stop to avoid
 * hang fabric.
 *
 */
void isp_reset(struct isp_ctx *ctx);

/**
 * isp_config - configuration isp.
 *
 */
void isp_config(struct isp_ctx *ctx, struct isp_param *param);

/**
 * isp_stream_on - start/stop isp stream.
 *
 * @param on: 1 for stream start, 0 for stream stop
 */
void isp_streaming(struct isp_ctx *ctx, uint32_t on);


uint64_t *isp_get_phys_reg_bases(void);

int isp_get_vblock_info(struct isp_vblock_info **pinfo, uint32_t *nblocks,
			enum ISPCQ_ID_T cq_group);

int ispcq_init_cmdset(char *cmdset_ba, int size, uint64_t reg_base);
int ispcq_set_end_cmdset(char *cmdset_ba, int size);
int ispcq_init_adma_table(char *adma_tb, int num_cmdset);
int ispcq_add_descriptor(char *adma_tb, int index, uint64_t cmdset_addr,
			 uint32_t cmdset_size);
uint64_t ispcq_get_desc_addr(char *adma_tb, int index);
int ispcq_set_link_desc(char *adma_tb, int index,
			uint64_t target_desc_addr, int is_link);
int ispcq_set_end_desc(char *adma_tb, int index, int is_end);
int ispcq_engine_config(uint64_t *phys_regs, struct ispcq_config *cfg);
int ispcq_engine_start(uint64_t *phys_regs, enum ISPCQ_ID_T id);

int ispblk_rgbmap_config(struct isp_ctx *ctx, int map_id);
int ispblk_lmap_config(struct isp_ctx *ctx, int map_id);
int ispblk_dpc_config(struct isp_ctx *ctx, union REG_ISP_DPC_2 reg2);
int ispblk_blc_set_offset(struct isp_ctx *ctx, int blc_id, uint16_t roffset,
			  uint16_t groffset, uint16_t gboffset,
			  uint16_t boffset);
int ispblk_blc_set_gain(struct isp_ctx *ctx, int blc_id, uint16_t rgain,
			uint16_t grgain, uint16_t gbgain, uint16_t bgain);
int ispblk_blc_enable(struct isp_ctx *ctx, int blc_id, bool en, bool bypass);
int ispblk_wbg_config(struct isp_ctx *ctx, int wbg_id, uint16_t rgain,
		      uint16_t ggain, uint16_t bgain);
int ispblk_wbg_enable(struct isp_ctx *ctx, int wbg_id, bool enable,
		      bool bypass);
int ispblk_fusion_config(struct isp_ctx *ctx, bool enable, bool bypass,
			 bool mc_enable, enum ISP_FS_OUT out_sel);
void ispblk_ltm_d_lut(struct isp_ctx *ctx, uint8_t sel, uint16_t *data);
void ispblk_ltm_b_lut(struct isp_ctx *ctx, uint8_t sel, uint16_t *data);
void ispblk_ltm_g_lut(struct isp_ctx *ctx, uint8_t sel, uint16_t *data);
void ispblk_ltm_config(struct isp_ctx *ctx, bool en, bool dehn_en, bool dlce_en,
		       bool behn_en, bool blce_en, uint8_t hblk_size,
		       uint8_t vblk_size);
int ispblk_bnr_config(struct isp_ctx *ctx, enum ISP_BNR_OUT out_sel,
		      bool lsc_en, uint8_t ns_gain, uint8_t str);

int ispblk_rgb_config(struct isp_ctx *ctx);
void ispblk_rgb_prob_set_pos(struct isp_ctx *ctx, enum ISP_RGB_PROB_OUT out,
			     uint16_t x, uint16_t y);
void ispblk_rgb_prob_get_values(struct isp_ctx *ctx, uint16_t *r, uint16_t *g,
				uint16_t *b);
int ispblk_cfa_config(struct isp_ctx *ctx);
int ispblk_rgbee_config(struct isp_ctx *ctx, bool en, uint16_t osgain,
			uint16_t usgain);
int ispblk_rgbee_config(struct isp_ctx *ctx, bool en, uint16_t osgain,
			uint16_t usgain);
int ispblk_gamma_config(struct isp_ctx *ctx, uint8_t sel, uint16_t *data);
int ispblk_gamma_enable(struct isp_ctx *ctx, bool enable, uint8_t sel);
int ispblk_rgbdither_config(struct isp_ctx *ctx, bool en, bool mod_en,
			    bool histidx_en, bool fmnum_en);
int ispblk_dhz_config(struct isp_ctx *ctx, bool en, uint8_t str,
		      uint16_t th_smooth);
int ispblk_hsv_config(struct isp_ctx *ctx, uint8_t sel, uint8_t type,
		      uint16_t *lut);
int ispblk_hsv_enable(struct isp_ctx *ctx, bool en, uint8_t sel, bool hsgain_en,
		      bool hvgain_en, bool htune_en, bool stune_en);
int ispblk_csc_config(struct isp_ctx *ctx);

int ispblk_yuvdither_config(struct isp_ctx *ctx, uint8_t sel, bool en,
			    bool mod_en, bool histidx_en, bool fmnum_en);
int ispblk_444_422_config(struct isp_ctx *ctx);
int ispblk_ynr_config(struct isp_ctx *ctx, enum ISP_YNR_OUT out_sel,
		      uint8_t ns_gain, uint8_t str);
int ispblk_cnr_config(struct isp_ctx *ctx, bool en, bool pfc_en,
		      uint8_t str_mode);
int ispblk_ee_config(struct isp_ctx *ctx, bool enable, bool bypass);
int ispblk_ycur_config(struct isp_ctx *ctx, uint8_t sel, uint16_t *data);
int ispblk_ycur_enable(struct isp_ctx *ctx, bool enable, uint8_t sel);

int ispblk_preraw_config(struct isp_ctx *ctx);
int ispblk_rawtop_config(struct isp_ctx *ctx);
int ispblk_yuvtop_config(struct isp_ctx *ctx);
int ispblk_isptop_config(struct isp_ctx *ctx);
int ispblk_csibdg_config(struct isp_ctx *ctx);
int ispblk_dma_config(struct isp_ctx *ctx, int dmaid, uint64_t buf_addr);
void ispblk_dma_setaddr(struct isp_ctx *ctx, uint32_t dmaid, uint64_t buf_addr);
int ispblk_dma_enable(struct isp_ctx *ctx, uint32_t dmaid, uint32_t on);
int ispblk_dma_dbg_st(struct isp_ctx *ctx, uint32_t dmaid, uint32_t bus_sel);

void isp_pchk_config(struct isp_ctx *ctx, uint8_t en_mask);
void isp_pchk_chk_status(struct isp_ctx *ctx, uint8_t en_mask,
			 uint32_t intr_status);

#endif // _ISP_DRV_H_
