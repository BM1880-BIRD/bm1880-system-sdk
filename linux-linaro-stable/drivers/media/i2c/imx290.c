/*
 * Driver for IMX290 CMOS Image Sensor from Sony
 */
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_graph.h>
#include <linux/slab.h>
#include <linux/videodev2.h>
#include <linux/debugfs.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-image-sizes.h>
#include <media/v4l2-mediabus.h>

/* [TODO] shall be move to uapi. */
#define BM_CAMERA_CID_BASE	(V4L2_CTRL_CLASS_CAMERA | 0x3000)

#define BM_CAMERA_CID_LEXP	(BM_CAMERA_CID_BASE + 0)
#define BM_CAMERA_CID_SEXP	(BM_CAMERA_CID_BASE + 1)

enum bm_cif_type {
	BM_CIF_TYPE_CSI = 0,
	BM_CIF_TYPE_SUBLVDS,
	BM_CIF_TYPE_HISPI,
	BM_CIF_TYPE_PARALLEL
};

enum bm_cif_ch {
	BM_CIF_CH_2_LANE = 0,
	BM_CIF_CH_4_LANE,
	BM_CIF_CH_8_LAN
};

enum bm_cif_bit_order {
	BM_CIF_BIT_ORDER_MSB = 0,
	BM_CIF_BIT_ORDER_LSB
};

struct bm_cif_info {
	enum bm_cif_type	type;
	enum bm_cif_ch		ch;
	enum bm_cif_bit_order	order;
};

#define BM_CIF_G_SENSOR_INFO	_IOR('R', 2, int)
#define BM_CIF_G_HDR_HBLANK	_IOR('R', 3, int)

/* IMX290 supported DATA INTERFACE */
#define IMX290_DATA_INTF_SUBLVDS	0
#define IMX290_DATA_INTF_MIPI		1

/* IMX290 supported geometry */
#define IMX290_TABLE_END		0xffff

/* In dB*256 */
#define IMX290_DIGITAL_GAIN_MIN		0
#define IMX290_DIGITAL_GAIN_MAX		240
#define IMX290_DIGITAL_GAIN_DEFAULT	42

#define IMX290_DIGITAL_EXPOSURE_DEFAULT	48

#define IMX290_DIGITAL_LONG_EXP_DEFAULT	768

#define IMX290_DIGITAL_SHORT_EXP_DEFAULT 6

/* [TODO] hardcore the PINMUX temporarily. */
#define  fmux_gpio_funcsel_CAM_PD0   0x194
#define  fmux_gpio_funcsel_CAM_PD0_OFFSET 0
#define  fmux_gpio_funcsel_CAM_PD0_MASK   0x7
#define  CAM_PD0__CAM_MCLK1 0

#define  fmux_gpio_funcsel_CAM_MCLK0   0x1a0
#define  fmux_gpio_funcsel_CAM_MCLK0_OFFSET 0
#define  fmux_gpio_funcsel_CAM_MCLK0_MASK   0x7
#define  CAM_MCLK0__CAM_MCLK0 0

#define  fmux_gpio_funcsel_IIC3_SCL   0x1a4
#define  fmux_gpio_funcsel_IIC3_SCL_OFFSET 0
#define  fmux_gpio_funcsel_IIC3_SCL_MASK   0x7
#define   IIC3_SCL__IIC3_SCL 0

#define  fmux_gpio_funcsel_IIC3_SDA   0x1b4
#define  fmux_gpio_funcsel_IIC3_SDA_OFFSET 0
#define  fmux_gpio_funcsel_IIC3_SDA_MASK   0x7
#define  IIC3_SDA__IIC3_SDA 0

#define PINMUX_BASE 0x03001000
#define PINMUX_MASK(PIN_NAME) fmux_gpio_funcsel_##PIN_NAME##_MASK
#define PINMUX_OFFSET(PIN_NAME) fmux_gpio_funcsel_##PIN_NAME##_OFFSET
#define PINMUX_VALUE(PIN_NAME, FUNC_NAME) PIN_NAME##__##FUNC_NAME
#define PINMUX_CONFIG(PIN_NAME, FUNC_NAME) \
	mmio_clrsetbits_32(PINMUX_BASE + fmux_gpio_funcsel_##PIN_NAME, \
			PINMUX_MASK(PIN_NAME) << PINMUX_OFFSET(PIN_NAME), \
			PINMUX_VALUE(PIN_NAME, FUNC_NAME))

static inline void mmio_clrsetbits_32(uintptr_t addr,
				      uint32_t clear,
				      uint32_t set)
{
	iowrite32((ioread32(ioremap(addr, 0x4)) & ~clear) | set,
		  ioremap(addr, 0x4));
}

static void config_pinmux(void)
{
	/* camera interface. */
	PINMUX_CONFIG(CAM_PD0, CAM_MCLK1);
	PINMUX_CONFIG(CAM_MCLK0, CAM_MCLK0);
	/* I2C3. */
	PINMUX_CONFIG(IIC3_SCL, IIC3_SCL);
	PINMUX_CONFIG(IIC3_SDA, IIC3_SDA);
}

static const s64 link_freq_menu_items[] = {
	297000000,
};

struct imx290_reg {
	u16 addr;
	u8 val;
};

struct imx290_mode {
	u32 width;
	u32 height;
	u32 max_fps;
	u32 hts_def;
	u32 vts_def;
	u32 h_fp;
	u32 h_bp;
	u32 v_fp;
	u32 v_bp;
	u16 exp_min;
	u16 exp_max;
	u16 rhs1;
	u16 lexp_min;
	u16 lexp_max;
	u16 sexp_min;
	u16 sexp_max;
	const struct imx290_reg *reg_list;
};

static int data_interface = IMX290_DATA_INTF_SUBLVDS;
module_param(data_interface, int, 0444);
MODULE_PARM_DESC(data_interface, "Sensor Date interface");

/* MCLK:37.125MHz  1280x720 30fps Sub-LVDS LANE2 */
static const struct imx290_reg imx290_init_tab_1280_720_30fps[] = {
	{0x3005, 0x00}, /* ADBIT 10bit */
	{0x3007, 0x10}, /* VREVERS, */
	{0x3009, 0x02},
	{0x300A, 0x3C}, /* BLKLEVEL */
	{0x300F, 0x00},
	{0x3010, 0x21},
	{0x3012, 0x64},
	{0x3014, 0x2A}, /* GAIN, 0x2A=>12.6dB TBD */
	{0x3016, 0x09},
	{0x3018, 0xEE}, /* VMAX[7:0] */
	{0x3019, 0x02}, /* VMAX[15:8] */
	{0x301A, 0x00}, /* VMAX[17:16]:=:0x301A[1:0] */
	{0x301C, 0xC8}, /* HMAX[7:0], TBD */
	{0x301D, 0x19}, /* HMAX[15:8] */
	{0x3020, 0x30}, /* SHS[7:0], TBD */
	{0x3021, 0x00}, /* SHS[15:8] */
	{0x3022, 0x00}, /* SHS[17:16] */
	{0x3046, 0xD0}, /* Lane number D-2, E-4, F-8 */
	{0x304B, 0x0A},
	{0x305C, 0x20}, /* INCKSEL1 */
	{0x305D, 0x00}, /* INCKSEL2 */
	{0x305E, 0x20}, /* INCKSEL3 */
	{0x305F, 0x01}, /* INCKSEL4 */
	{0x3070, 0x02},
	{0x3071, 0x11},
	{0x309B, 0x10},
	{0x309C, 0x22},
	{0x30A2, 0x02},
	{0x30A6, 0x20},
	{0x30A8, 0x20},
	{0x30AA, 0x20},
	{0x30AC, 0x20},
	{0x30B0, 0x43},
	/*new module check {0x310B, 0x00}*/
	{0x3119, 0x9E},
	{0x311C, 0x1E},
	{0x311E, 0x08},
	{0x3128, 0x05},
	{0x3129, 0x1D},
	/*new module check {0x3134, 0x0F}*/
	/*new module check {0x313B, 0x50}*/
	{0x313D, 0x83},
	{0x3150, 0x03},
	{0x315E, 0x1A},
	{0x3164, 0x1A},
	{0x317C, 0x12},
	{0x317E, 0x00},
	/*new module check {0x317F, 0x00}*/
	{0x31EC, 0x37},
	{0x32B8, 0x50},
	{0x32B9, 0x10},
	{0x32BA, 0x00},
	{0x32BB, 0x04},
	{0x32C8, 0x50},
	{0x32C9, 0x10},
	{0x32CA, 0x00},
	{0x32CB, 0x04},
	{0x332C, 0xD3},
	{0x332D, 0x10},
	{0x332E, 0x0D},
	{0x3358, 0x06},
	{0x3359, 0xE1},
	{0x335A, 0x11},
	{0x3360, 0x1E},
	{0x3361, 0x61},
	{0x3362, 0x10},

	{0x33B0, 0x50},
	/*new module check {0x33B1, 0x30}*/
	/*new module check*/{0x33B2, 0x1A},
	{0x33B3, 0x04},
	{0x3480, 0x49}, /* INCKSEL7 */
	{IMX290_TABLE_END, 0x00}
};

static const struct imx290_reg start[] = {
	{0x3000, 0x00},		/* standby disable */
	{0x3002, 0x00},		/* mode select streaming on */
	{IMX290_TABLE_END, 0x00}
};

static const struct imx290_reg stop[] = {
	{0x3000, 0x01},		/* mode select streaming off */
	{IMX290_TABLE_END, 0x00}
};

static const struct imx290_reg hdr_on[] = {
	{0x300C, 0x11},		/* WDMODE [0] ,WDSEL [5:4] */
	{0x3045, 0x03},		/* DOLSCDEN [0], DOLSYDINFOEN [1], HINFOEN [2]*/
	{0x3106, 0x11},		/* DOLHBFIXEN [7], DOLHBFIXEN [1:0]
				 * DOLHBFIXEN [5:4]
				 */
	/* [TODO] shutter related setting. shall be adjusted independently*/
#if 0
	{0x3020, 0x02},		/* SHS1[7:0] */
	{0x3021, 0x00},		/* SHS1[15:8] */
	{0x3022, 0x00},		/* SHS1[19:16] */
	{0x3024, 0xDB},		/* SHS2[7:0] */
	{0x3025, 0x02},		/* SHS2[15:8] */
	{0x3026, 0x00},		/* SHS2[19:16] */
#endif
	{0x3030, 0x09},		/* RHS1[7:0] */
	{0x3031, 0x00},		/* RHS1[15:8] */
	{0x3032, 0x00},		/* RHS1[19:16] */

	{IMX290_TABLE_END, 0x00}
};

static const struct imx290_reg hdr_off[] = {
	{0x300C, 0x00},		/* WDMODE [0] ,WDSEL [5:4] */
	{0x3045, 0x00},		/* DOLSCDEN [0], DOLSYDINFOEN [1], HINFOEN [2]*/
	{0x3106, 0x00},		/* DOLHBFIXEN [7], DOLHBFIXEN [1:0]
				 * DOLHBFIXEN [5:4]
				 */

	/* [TODO] shutter related setting. shall be adjusted independently*/
#if 0
	{0x3020, 0x8C},		/* SHS[7:0] */
	{0x3021, 0x01},		/* SHS[15:8] */
	{0x3022, 0x00},		/* SHS[19:16] */
#endif
	{0x3024, 0x00},		/* SHS2[7:0] */
	{0x3025, 0x00},		/* SHS2[15:8] */
	{0x3026, 0x00},		/* SHS2[19:16] */
	{0x3030, 0x00},		/* RHS1[7:0] */
	{0x3031, 0x00},		/* RHS1[15:8] */
	{0x3032, 0x00},		/* RHS1[19:16] */

	{IMX290_TABLE_END, 0x00}
};

enum {
	TEST_PATTERN_DISABLED,
	TEST_PATTERN_SOLID_BLACK,
	TEST_PATTERN_SOLID_WHITE,
	TEST_PATTERN_SEQ1,
	TEST_PATTERN_H_COLOR_BAR,
	TEST_PATTERN_V_COLOR_BAR,
	TEST_PATTERN_SEQ2,
	TEST_PATTERN_GRAD1,
	TEST_PATTERN_GRAD2,
	TEST_PATTERN_TOGGLE,
	TEST_PATTERN_MAX
};

static const char *const tp_qmenu[] = {
	"Disabled",
	"Solid Black",
	"Solid White",
	"sequence 1",
	"H Color Bar",
	"V Color Bar",
	"sequence 2",
	"gradation 1",
	"gradation 2",
	"toggle",
};

#define SIZEOF_I2C_TRANSBUF 32

struct imx290 {
	struct v4l2_subdev subdev;
	struct media_pad pad;
	struct v4l2_ctrl_handler ctrl_handler;
	struct clk *clk;
	int hflip;
	int vflip;
	u16 gain;	/* bits 11:0 */
	u16 exposure_time;
	u16 lexp_time;
	u16 sexp_time;
	u16 test_pattern_data1;
	u16 test_pattern_data2;
	u8  test_pattern;
	u8  raw_bit;
	u8  hdr;
	struct v4l2_ctrl *pixel_rate;
	struct v4l2_ctrl *exposure;
	struct v4l2_ctrl *long_exp;
	struct v4l2_ctrl *short_exp;
	const struct imx290_mode *cur_mode;
	struct dentry *debug_root;
};

static const struct imx290_mode supported_modes[] = {
	{
		.width = 1308,
		.height = 736,
		.max_fps = 30,
		.hts_def = 0x0A50,
		.vts_def = 0x02EE,
		.reg_list = imx290_init_tab_1280_720_30fps,
		.h_fp = 12,
		.h_bp = 16,
		.v_fp = 11,
		.v_bp = 5,
		.exp_min = 1,
		.exp_max = 748,
		.rhs1 = 9,
		.lexp_min = 1,
		.lexp_max = 1488,
		.sexp_min = 1,
		.sexp_max = 6,
	},
};

static struct imx290 *to_imx290(const struct i2c_client *client)
{
	return container_of(i2c_get_clientdata(client), struct imx290, subdev);
}

static int reg_write(struct i2c_client *client, const u16 addr, const u8 data)
{
	struct i2c_adapter *adap = client->adapter;
	struct i2c_msg msg;
	u8 tx[3];
	int ret;

	msg.addr = client->addr;
	msg.buf = tx;
	msg.len = 3;
	msg.flags = 0;
	tx[0] = addr >> 8;
	tx[1] = addr & 0xff;
	tx[2] = data;
	ret = i2c_transfer(adap, &msg, 1);
	mdelay(2);

	return ret == 1 ? 0 : -EIO;
}

static int reg_read(struct i2c_client *client, const u16 addr)
{
	u8 buf[2] = {addr >> 8, addr & 0xff};
	int ret;
	struct i2c_msg msgs[] = {
		{
			.addr  = client->addr,
			.flags = 0,
			.len   = 2,
			.buf   = buf,
		}, {
			.addr  = client->addr,
			.flags = I2C_M_RD,
			.len   = 1,
			.buf   = buf,
		},
	};

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret < 0) {
		dev_warn(&client->dev, "Reading register %x from %x failed\n",
			 addr, client->addr);
		return ret;
	}

	return buf[0];
}

static int reg_write_table(struct i2c_client *client,
			   const struct imx290_reg table[])
{
	const struct imx290_reg *reg;
	int ret;

	for (reg = table; reg->addr != IMX290_TABLE_END; reg++) {
		ret = reg_write(client, reg->addr, reg->val);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/* V4L2 subdev video operations */
static int imx290_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct imx290 *priv = to_imx290(client);
	u8 reg = 0x00;
	int ret;

	dev_dbg(&client->dev, "stream %s\n", enable?"enable":"disable");
	if (!enable)
		return reg_write_table(client, stop);

	ret = reg_write_table(client, priv->cur_mode->reg_list);
	if (ret)
		return ret;

	/* Handle flip/mirror */
	reg = reg_read(client, 0x3007);
	if (priv->hflip)
		reg |= 0x02;
	if (priv->vflip)
		reg |= 0x01;
	ret = reg_write(client, 0x3007, reg);
	if (ret)
		return ret;

	/* Handle test pattern */
	if (priv->test_pattern) {
		/* Seet black level offset to 000h */
		ret = reg_write(client, 0x300A, 0);
		ret |= reg_write(client, 0x300B, 0);
		ret |= reg_write(client, 0x300E, 0);
		ret |= reg_write(client, 0x300F, 0);
		ret |= reg_write(client, 0x3092,
				 priv->test_pattern_data1 & 0xff);
		ret |= reg_write(client, 0x3093,
				 (priv->test_pattern_data1 >> 8) & 0x0f);
		ret |= reg_write(client, 0x3094,
				 priv->test_pattern_data2 & 0xff);
		ret |= reg_write(client, 0x3095,
				 (priv->test_pattern_data2 >> 8) & 0x0f);
		ret |= reg_write(client, 0x308C, priv->test_pattern);
	} else {
		ret = reg_write(client, 0x308C, 0x00);
	}

	if (ret)
		return ret;

	return reg_write_table(client, start);
}

/* V4L2 subdev core operations */

static long imx290_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct bm_cif_info *info = arg;

	switch (cmd) {
	case BM_CIF_G_SENSOR_INFO:
		info->type = BM_CIF_TYPE_SUBLVDS;
		info->ch = BM_CIF_CH_2_LANE;
		info->order = BM_CIF_BIT_ORDER_MSB;
		break;
	default:
		return -ENOIOCTLCMD;
	}

	return 0;
}

static int imx290_s_power(struct v4l2_subdev *sd, int on)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	uint32_t tmp = 0x3000; /* two lanes. */
	/*struct imx290 *priv = to_imx290(client);*/

	dev_dbg(&client->dev, "power %s\n", on?"on":"off");
	if (on)	{
		dev_dbg(&client->dev, "imx290 power on\n");
		/* [TODO] FPGA ONLY: enable the display clk select for MCLK. */
		iowrite32((ioread32(ioremap(0x0a0c8018, 0x4)) | 0x02),
			  ioremap(0x0a0c8018, 0x4));
		/* [TODO] GPIO: sensor power on sequence. */
		iowrite32(tmp, ioremap(0x0A0880F8, 0x4));
		mdelay(5);
		iowrite32(tmp | 0x08, ioremap(0x0A0880F8, 0x4));
		mdelay(5);
		iowrite32(tmp | 0x18, ioremap(0x0A0880F8, 0x4));
		mdelay(5);
		iowrite32(tmp | 0x38, ioremap(0x0A0880F8, 0x4));
		mdelay(5);

		/* [TODO] ISP: PHY type select. */
		iowrite32(0x701, ioremap(0x0a0c2000, 0x4));
		/* [TODO] ISP: Config sublvds. */
		iowrite32(0x103, ioremap(0x0a0c2200, 0x4));
		iowrite32(0x11, ioremap(0x0a0c2230, 0x4));
		/* [TODO] FPGA ONLY: Enabe RX PHY and debug. */
		iowrite32(tmp | 0x03100039, ioremap(0x0A0880F8, 0x4));

		/*clk_prepare_enable(priv->clk);*/
	} else if (!on) {
		dev_dbg(&client->dev, "imx290 power off\n");
		/* [TODO] GPIO: sensor power on sequence. */
		iowrite32(0x38, ioremap(0x0A0880F8, 0x4));
		mdelay(5);
		iowrite32(0x18, ioremap(0x0A0880F8, 0x4));
		mdelay(5);
		iowrite32(0x08, ioremap(0x0A0880F8, 0x4));
		mdelay(5);
		iowrite32(0x00, ioremap(0x0A0880F8, 0x4));
		mdelay(5);
		/*clk_disable_unprepare(priv->clk);*/
	}

	return 0;
}

/* V4L2 ctrl operations */
static int imx290_s_ctrl_test_pattern(struct v4l2_ctrl *ctrl)
{
	struct imx290 *priv =
		container_of(ctrl->handler, struct imx290, ctrl_handler);
	struct i2c_client *client = v4l2_get_subdevdata(&priv->subdev);

	dev_dbg(&client->dev, "set test pattern %s\n",
		ctrl->val >= ARRAY_SIZE(tp_qmenu) ?
		"invalid" : tp_qmenu[ctrl->val]);

	switch (ctrl->val) {
	case TEST_PATTERN_DISABLED:
		priv->test_pattern = 0x0000;
		break;
	case TEST_PATTERN_SOLID_BLACK:
		priv->test_pattern = 0xC1;
		priv->test_pattern_data1 = 0x0001;
		priv->test_pattern_data2 = 0x0001;
		break;
	case TEST_PATTERN_SOLID_WHITE:
		priv->test_pattern = 0x00C1;
		if (priv->raw_bit == 12) {
			priv->test_pattern_data1 = 0x0ffe;
			priv->test_pattern_data2 = 0x0ffe;
		} else {
			priv->test_pattern_data1 = 0x03fe;
			priv->test_pattern_data2 = 0x03fe;
		}
		break;
	case TEST_PATTERN_SEQ1:
		priv->test_pattern = 0x11;
		break;
	case TEST_PATTERN_H_COLOR_BAR:
		if (priv->cur_mode->width < 1920)
			priv->test_pattern = 0x25;
		else
			priv->test_pattern = 0x21;
		break;
	case TEST_PATTERN_V_COLOR_BAR:
		if (priv->cur_mode->width < 1080)
			priv->test_pattern = 0x35;
		else
			priv->test_pattern = 0x31;
		break;
	case TEST_PATTERN_SEQ2:
		priv->test_pattern = 0x41;
		break;
	case TEST_PATTERN_GRAD1:
		priv->test_pattern = 0x51;
		break;
	case TEST_PATTERN_GRAD2:
		priv->test_pattern = 0x61;
		break;
	case TEST_PATTERN_TOGGLE:
		priv->test_pattern = 0x71;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int _imx290_s_ctrl_exposure(struct imx290 *priv, u16 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&priv->subdev);
	u32 shs1;
	int ret;

	priv->exposure_time = val;

	if (priv->exposure_time > priv->cur_mode->exp_max)
		priv->exposure_time = priv->cur_mode->exp_max;
	if (priv->exposure_time < priv->cur_mode->exp_min)
		priv->exposure_time = priv->cur_mode->exp_min;
	shs1 = priv->cur_mode->vts_def - priv->exposure_time - 1;

	ret = reg_write(client, 0x3020, shs1 & 0xff);
	ret |= reg_write(client, 0x3021, (shs1 >> 8) & 0xff);
	ret |= reg_write(client, 0x3022, (shs1 >> 16) & 0x03);

	return ret;
}

static int imx290_s_ctrl_exposure(struct v4l2_ctrl *ctrl)
{
	struct imx290 *priv =
		container_of(ctrl->handler, struct imx290, ctrl_handler);

	return _imx290_s_ctrl_exposure(priv, ctrl->val);
}

static int _imx290_s_ctrl_long_exposure(struct imx290 *priv, u16 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&priv->subdev);
	u32 shs2;
	int ret;

	priv->lexp_time = val;

	if (priv->lexp_time > priv->cur_mode->lexp_max)
		priv->lexp_time = priv->cur_mode->lexp_max;
	if (priv->lexp_time < priv->cur_mode->lexp_min)
		priv->lexp_time = priv->cur_mode->lexp_min;
	shs2 = (priv->cur_mode->vts_def<<1) - priv->lexp_time - 1;

	ret = reg_write(client, 0x3024, shs2 & 0xff);
	ret |= reg_write(client, 0x3024, (shs2 >> 8) & 0xff);
	ret |= reg_write(client, 0x3025, (shs2 >> 16) & 0x03);

	return ret;
}

static int imx290_s_ctrl_long_exposure(struct v4l2_ctrl *ctrl)
{
	struct imx290 *priv =
		container_of(ctrl->handler, struct imx290, ctrl_handler);

	return _imx290_s_ctrl_long_exposure(priv, ctrl->val);
}

static int _imx290_s_ctrl_short_exposure(struct imx290 *priv, u16 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(&priv->subdev);
	u32 shs2;
	int ret;

	priv->sexp_time = val;

	if (priv->sexp_time > priv->cur_mode->sexp_max)
		priv->sexp_time = priv->cur_mode->sexp_max;
	if (priv->sexp_time < priv->cur_mode->sexp_min)
		priv->sexp_time = priv->cur_mode->sexp_min;
	shs2 = priv->cur_mode->rhs1 - priv->sexp_time - 1;

	ret = reg_write(client, 0x3020, shs2 & 0xff);
	ret |= reg_write(client, 0x3021, (shs2 >> 8) & 0xff);
	ret |= reg_write(client, 0x3022, (shs2 >> 16) & 0x03);

	return ret;
}

static int imx290_s_ctrl_short_exposure(struct v4l2_ctrl *ctrl)
{
	struct imx290 *priv =
		container_of(ctrl->handler, struct imx290, ctrl_handler);

	return _imx290_s_ctrl_short_exposure(priv, ctrl->val);
}

static int imx290_s_ctrl_gain(struct v4l2_ctrl *ctrl)
{
	struct imx290 *priv =
		container_of(ctrl->handler, struct imx290, ctrl_handler);
	struct i2c_client *client = v4l2_get_subdevdata(&priv->subdev);
	u16 gain;

	/*
	 * Gain[dB] x 10/3 (0.3 dB step)
	 */

	gain = ctrl->val;
	if (gain > 240)
		gain = 240;
	priv->gain = gain;

	return reg_write(client, 0x3014, priv->gain);
}

static int imx290_s_ctrl_hdr(struct v4l2_ctrl *ctrl)
{
	struct imx290 *priv =
		container_of(ctrl->handler, struct imx290, ctrl_handler);
	struct i2c_client *client = v4l2_get_subdevdata(&priv->subdev);
	int ret = 0;

	if (priv->hdr != ctrl->val) {
		u8 reg;

		/* Enter standby mode. */
		reg = reg_read(client, 0x3000);
		if (reg == 0x0)
			ret = reg_write_table(client, stop);

		priv->hdr = ctrl->val;
		if (priv->hdr) {
			/* Configure DOL related setting*/
			ret |= reg_write_table(client, hdr_on);
			ret |= _imx290_s_ctrl_long_exposure(priv,
							    priv->lexp_time);
			ret |= _imx290_s_ctrl_short_exposure(priv,
							     priv->sexp_time);

		} else {
			/* Disable DOL setting. */
			ret |= reg_write_table(client, hdr_off);
			ret |= _imx290_s_ctrl_exposure(priv,
						       priv->exposure_time);
		}

		/* Leave standby mode. */
		if (reg == 0x0)
			ret |= reg_write_table(client, start);
	}

	return ret;
}

static int imx290_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct imx290 *priv = to_imx290(client);
	const struct imx290_mode *mode = priv->cur_mode;

	fi->interval.numerator = 10000;
	fi->interval.denominator = mode->max_fps * 10000;

	return 0;
}

static int imx290_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct imx290 *priv =
		container_of(ctrl->handler, struct imx290, ctrl_handler);
	struct i2c_client *client = v4l2_get_subdevdata(&priv->subdev);
	u8 reg;

	switch (ctrl->id) {
	case V4L2_CID_HFLIP:
		dev_dbg(&client->dev, "H Flip\n");
		priv->hflip = ctrl->val;
		break;
	case V4L2_CID_VFLIP:
		dev_dbg(&client->dev, "V Flip\n");
		priv->vflip = ctrl->val;
		break;
	case V4L2_CID_WIDE_DYNAMIC_RANGE:
		dev_dbg(&client->dev, "HDR %d\n", ctrl->val);
		return imx290_s_ctrl_hdr(ctrl);
	case V4L2_CID_GAIN:
		dev_dbg(&client->dev, "GAIN %d\n", ctrl->val);
		return imx290_s_ctrl_gain(ctrl);
	case V4L2_CID_EXPOSURE:
		dev_dbg(&client->dev, "Exposure %d\n", ctrl->val);
		return imx290_s_ctrl_exposure(ctrl);
	case V4L2_CID_TEST_PATTERN:
		dev_dbg(&client->dev, "Test Pattern %d\n", ctrl->val);
		return imx290_s_ctrl_test_pattern(ctrl);
	case BM_CAMERA_CID_LEXP:
		dev_dbg(&client->dev, "Long Exposure %d\n", ctrl->val);
		return imx290_s_ctrl_long_exposure(ctrl);
	case BM_CAMERA_CID_SEXP:
		dev_dbg(&client->dev, "Short Exposure %d\n", ctrl->val);
		return imx290_s_ctrl_short_exposure(ctrl);
	default:
		return -EINVAL;
	}
	/* If enabled, apply settings immediately */
	reg = reg_read(client, 0x3000);
	if (reg == 0x0)
		imx290_s_stream(&priv->subdev, 1);
	return 0;
}

static int imx290_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index != 0)
		return -EINVAL;
	code->code = MEDIA_BUS_FMT_SBGGR10_1X10;

	return 0;
}

static int imx290_get_reso_dist(const struct imx290_mode *mode,
				struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
	       abs(mode->height - framefmt->height);
}

static const struct imx290_mode *imx290_find_best_fit(
	struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	int i;

	for (i = 0; i < ARRAY_SIZE(supported_modes); i++) {
		dist = imx290_get_reso_dist(&supported_modes[i], framefmt);
		if (cur_best_fit_dist == -1 || dist < cur_best_fit_dist) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		}
	}

	return &supported_modes[cur_best_fit];
}

static int imx290_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct imx290 *priv = to_imx290(client);
	const struct imx290_mode *mode;
	s64 pixel_rate;
	u32 h, w;

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY)
		return 0;

	mode = imx290_find_best_fit(fmt);
	fmt->format.code = MEDIA_BUS_FMT_SRGGB10_1X10;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	priv->cur_mode = mode;
	w = mode->width - mode->h_fp - mode->h_bp;
	h = mode->height - mode->v_fp - mode->v_bp;
	pixel_rate = w * h * mode->max_fps;
	__v4l2_ctrl_modify_range(priv->pixel_rate, pixel_rate,
				 pixel_rate, 1, pixel_rate);

	__v4l2_ctrl_modify_range(priv->exposure, mode->exp_min,
				 mode->exp_max, 1, priv->exposure_time);
	return 0;
}

static int imx290_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct imx290 *priv = to_imx290(client);
	const struct imx290_mode *mode = priv->cur_mode;

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY)
		return 0;

	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.code = MEDIA_BUS_FMT_SBGGR10_1X10;
	fmt->format.field = V4L2_FIELD_NONE;

	cfg->try_crop.left = mode->h_fp;
	cfg->try_crop.top = mode->v_fp;
	cfg->try_crop.width = mode->width - mode->h_fp - mode->h_bp;
	cfg->try_crop.height = mode->height - mode->v_fp - mode->v_bp;

	return 0;
}

/* Various V4L2 operations tables */
static struct v4l2_subdev_video_ops imx290_subdev_video_ops = {
	.s_stream = imx290_s_stream,
	.g_frame_interval = imx290_g_frame_interval,
};

static struct v4l2_subdev_core_ops imx290_subdev_core_ops = {
	.s_power = imx290_s_power,
	.ioctl = imx290_ioctl,
};

static const struct v4l2_subdev_pad_ops imx290_subdev_pad_ops = {
	.enum_mbus_code = imx290_enum_mbus_code,
	.set_fmt = imx290_set_fmt,
	.get_fmt = imx290_get_fmt,
};

static struct v4l2_subdev_ops imx290_subdev_ops = {
	.core = &imx290_subdev_core_ops,
	.video = &imx290_subdev_video_ops,
	.pad = &imx290_subdev_pad_ops,
};

static const struct v4l2_ctrl_ops imx290_ctrl_ops = {
	.s_ctrl = imx290_s_ctrl,
};

static struct v4l2_ctrl_config imx290_ctrl_lexp = {
	.ops = &imx290_ctrl_ops,
	.id = BM_CAMERA_CID_LEXP,
	.name = "long exposure setup",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 0,
	.max = 0,
	.step = 1,
	.def = IMX290_DIGITAL_LONG_EXP_DEFAULT,
};

static struct v4l2_ctrl_config imx290_ctrl_sexp = {
	.ops = &imx290_ctrl_ops,
	.id = BM_CAMERA_CID_SEXP,
	.name = "short exposure setup",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 0,
	.max = 0,
	.step = 1,
	.def = IMX290_DIGITAL_SHORT_EXP_DEFAULT,
};

static int imx290_video_probe(struct i2c_client *client)
{
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	int ret;

	/* [TODO] pinmux hardcore.*/
	config_pinmux();

	ret = imx290_s_power(subdev, 1);
	if (ret < 0)
		return ret;

	ret = imx290_s_stream(subdev, 1);
	if (ret < 0)
		dev_err(&client->dev, "Failure to setup stream.\n");
	/* ret = reg_read(client, 0x3000);
	 * if (ret < 0) {
	 *	dev_err(&client->dev, "Failure to read standby reg\n");
	 * }
	 * imx290_s_power(subdev, 0);
	 */

	return ret;
}

static int imx290_ctrls_init(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct imx290 *priv = to_imx290(client);
	const struct imx290_mode *mode = priv->cur_mode;
	s64 pixel_rate;
	u32 w, h;
	int ret;

	v4l2_ctrl_handler_init(&priv->ctrl_handler, 10);
	v4l2_ctrl_new_std(&priv->ctrl_handler, &imx290_ctrl_ops,
			  V4L2_CID_HFLIP, 0, 1, 1, 0);
	if (priv->ctrl_handler.error) {
		dev_err(&client->dev, "Error %d adding controls hflip\n",
			priv->ctrl_handler.error);
		ret = priv->ctrl_handler.error;
		goto error;
	}
	v4l2_ctrl_new_std(&priv->ctrl_handler, &imx290_ctrl_ops,
			  V4L2_CID_VFLIP, 0, 1, 1, 0);
	if (priv->ctrl_handler.error) {
		dev_err(&client->dev, "Error %d adding controls vflip\n",
			priv->ctrl_handler.error);
		ret = priv->ctrl_handler.error;
		goto error;
	}
	v4l2_ctrl_new_std(&priv->ctrl_handler, &imx290_ctrl_ops,
			  V4L2_CID_WIDE_DYNAMIC_RANGE, 0, 1, 1, 0);
	if (priv->ctrl_handler.error) {
		dev_err(&client->dev, "Error %d adding controls wdr\n",
			priv->ctrl_handler.error);
		ret = priv->ctrl_handler.error;
		goto error;
	}

	v4l2_ctrl_new_std(&priv->ctrl_handler, &imx290_ctrl_ops,
			  V4L2_CID_GAIN,
			  IMX290_DIGITAL_GAIN_MIN,
			  IMX290_DIGITAL_GAIN_MAX, 1,
			  IMX290_DIGITAL_GAIN_DEFAULT);
	if (priv->ctrl_handler.error) {
		dev_err(&client->dev, "Error %d adding controls gain\n",
			priv->ctrl_handler.error);
		ret = priv->ctrl_handler.error;
		goto error;
	}
	/* exposure */
	priv->exposure = v4l2_ctrl_new_std(&priv->ctrl_handler,
					   &imx290_ctrl_ops,
					   V4L2_CID_EXPOSURE,
					   mode->exp_min,
					   mode->exp_max, 1,
					   IMX290_DIGITAL_EXPOSURE_DEFAULT);
	if (priv->ctrl_handler.error) {
		dev_err(&client->dev, "Error %d adding controls exp\n",
			priv->ctrl_handler.error);
		ret = priv->ctrl_handler.error;
		goto error;
	}
	imx290_ctrl_lexp.min = mode->lexp_min;
	imx290_ctrl_lexp.max = mode->lexp_max;

	priv->long_exp = v4l2_ctrl_new_custom(&priv->ctrl_handler,
					      &imx290_ctrl_lexp, NULL);
	if (priv->ctrl_handler.error) {
		dev_err(&client->dev, "Error %d adding controls lexp\n",
			priv->ctrl_handler.error);
		ret = priv->ctrl_handler.error;
		goto error;
	}

	imx290_ctrl_sexp.min = mode->sexp_min;
	imx290_ctrl_sexp.max = mode->sexp_max;

	priv->short_exp = v4l2_ctrl_new_custom(&priv->ctrl_handler,
					       &imx290_ctrl_sexp, NULL);
	if (priv->ctrl_handler.error) {
		dev_err(&client->dev, "Error %d adding controls sexp\n",
			priv->ctrl_handler.error);
		ret = priv->ctrl_handler.error;
		goto error;
	}
	/* freq */
	v4l2_ctrl_new_int_menu(&priv->ctrl_handler, NULL, V4L2_CID_LINK_FREQ,
			       0, 0, link_freq_menu_items);
	if (priv->ctrl_handler.error) {
		dev_err(&client->dev, "Error %d adding controls link freq\n",
			priv->ctrl_handler.error);
		ret = priv->ctrl_handler.error;
		goto error;
	}
	w = mode->width - mode->h_fp - mode->h_bp;
	h = mode->height - mode->v_fp - mode->v_bp;
	pixel_rate = w * h * mode->max_fps;
	priv->pixel_rate = v4l2_ctrl_new_std(&priv->ctrl_handler, NULL,
					     V4L2_CID_PIXEL_RATE,
					     0, pixel_rate, 1, pixel_rate);

	if (priv->ctrl_handler.error) {
		dev_err(&client->dev, "Error %d adding controls pixel rate\n",
			priv->ctrl_handler.error);
		ret = priv->ctrl_handler.error;
		goto error;
	}

	v4l2_ctrl_new_std_menu_items(&priv->ctrl_handler, &imx290_ctrl_ops,
				     V4L2_CID_TEST_PATTERN,
				     ARRAY_SIZE(tp_qmenu) - 1, 0, 0, tp_qmenu);

	priv->subdev.ctrl_handler = &priv->ctrl_handler;
	if (priv->ctrl_handler.error) {
		dev_err(&client->dev, "Error %d adding controls dbg\n",
			priv->ctrl_handler.error);
		ret = priv->ctrl_handler.error;
		goto error;
	}

	ret = v4l2_ctrl_handler_setup(&priv->ctrl_handler);
	if (ret < 0) {
		dev_err(&client->dev, "Error %d setting default controls\n",
			ret);
		goto error;
	}

	return 0;
error:
	v4l2_ctrl_handler_free(&priv->ctrl_handler);
	return ret;
}

static struct dentry *imx290_dbg_i2c;

static int imx290_dbgfs_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t imx290_dbgfs_write(struct file *file, const char __user *buf,
				  size_t count, loff_t *pos)
{
	struct i2c_client *client = file->private_data;
	char info[255], tmp[255];
	int rw = -1;
	u16 addr;
	u8 val;
	int i, s, ret;

	memset(info, 0, 255);
	memset(tmp, 0, 255);
	ret = copy_from_user(info, buf, count);
	if (ret < 0) {
		dev_err(&client->dev, "copy from user error %d\n", ret);
		return ret;
	}

	/* parse operation. */
	rw = (info[0] == 'r') ? 0 : ((info[0] == 'w') ? 1 : -1);

	if (rw < 0)
		return -EINVAL;

	if (info[1] != ' ')
		return -EINVAL;

	/* find next digit. */
	for (i = 2; i < 255; i++)
		if ((info[i] >= '0') && (info[i] <= '9'))
			break;
	if (i == 255)
		return -EINVAL;
	s = i;
	/* find next space*/
	for (i = s; i < 255; i++)
		if (info[i] == ' ' || info[i] == '\0')
			break;
	if (i == 255)
		return -EINVAL;
	/* parse address */
	memcpy(tmp, &info[s], i-s);
	tmp[i-s] = '\0';
	ret = kstrtou16(tmp, 0, &addr);
	if (ret < 0) {
		dev_err(&client->dev, "kstrtou16 error %d\n", ret);
		return ret;
	}

	/* read operation. */
	if (!rw) {
		val = (u8)reg_read(client, addr);
		dev_info(&client->dev, "0x%x = 0x%x\n", addr, val);
		return count;
	}

	/* write*/
	memset(tmp, 0, 255);
	s = i + 1;
	/* find next digit. */
	for (i = s; i < 255; i++)
		if ((info[i] >= '0') && (info[i] <= '9'))
			break;
	if (i == 255)
		return -EINVAL;
	s = i;
	/* find next space*/
	for (i = s; i < 255; i++)
		if (info[i] == ' ' || info[i] == '\0')
			break;
	if (i == 255)
		return -EINVAL;
	/* parse value */
	memcpy(tmp, &info[s], i-s);
	tmp[i-s] = '\0';
	ret = kstrtou8(tmp, 0, &val);
	if (ret < 0) {
		dev_err(&client->dev, "kstrtou8 error %d\n", ret);
		return ret;
	}

	/* write operation. */
	reg_write(client, addr, val);

	return count;
}

static const struct file_operations imx290_dbg_fops_i2c = {
	.open		= imx290_dbgfs_open,
	.write		= imx290_dbgfs_write,
#if 0
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
#endif
};

static int imx290_dbgfs_init(struct i2c_client *client)
{
	struct imx290 *priv = to_imx290(client);

	priv->debug_root = debugfs_create_dir("imx290", NULL);

	if (!priv->debug_root)
		return -ENOENT;

	imx290_dbg_i2c = debugfs_create_file("i2c", 0644,
					     priv->debug_root, client,
					     &imx290_dbg_fops_i2c);

	if (!imx290_dbg_i2c) {
		debugfs_remove(priv->debug_root);
		priv->debug_root = NULL;
		return -ENOENT;
	}

	return 0;
}

static void imx290_dbgfs_cleanup(struct i2c_client *client)
{
	struct imx290 *priv = to_imx290(client);

	debugfs_remove(imx290_dbg_i2c);
	debugfs_remove(priv->debug_root);
}

static int imx290_probe(struct i2c_client *client,
			const struct i2c_device_id *did)
{
	struct imx290 *priv;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	int ret;

	if (data_interface != IMX290_DATA_INTF_SUBLVDS)
		return -EINVAL;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_warn(&adapter->dev,
			 "I2C-Adapter doesn't support I2C_FUNC_SMBUS_BYTE\n");
		return -EIO;
	}
	priv = devm_kzalloc(&client->dev, sizeof(struct imx290), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
#if 0
	priv->clk = devm_clk_get(&client->dev, NULL);
	if (IS_ERR(priv->clk)) {
		dev_info(&client->dev, "Error %ld getting clock\n",
			 PTR_ERR(priv->clk));
		return -EPROBE_DEFER;
	}
#endif

	/* 1280*720 by default */
	priv->cur_mode = &supported_modes[0];
	priv->exposure_time = IMX290_DIGITAL_EXPOSURE_DEFAULT;
	priv->lexp_time = IMX290_DIGITAL_LONG_EXP_DEFAULT;
	priv->sexp_time = IMX290_DIGITAL_SHORT_EXP_DEFAULT;

	/* raw 10bit by default*/
	priv->raw_bit = 10;
	v4l2_i2c_subdev_init(&priv->subdev, client, &imx290_subdev_ops);
	ret = imx290_video_probe(client);
	if (ret < 0)
		return ret;
	ret = imx290_ctrls_init(&priv->subdev);
	if (ret < 0)
		return ret;

	priv->subdev.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
#if defined(CONFIG_MEDIA_CONTROLLER)
	priv->pad.flags = MEDIA_PAD_FL_SOURCE;
	priv->subdev.entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&priv->subdev.entity, 1, &priv->pad);
	if (ret < 0)
		return ret;
#endif
	ret = v4l2_async_register_subdev(&priv->subdev);
	if (ret < 0)
		return ret;

	imx290_dbgfs_init(client);
	return ret;
}

static int imx290_remove(struct i2c_client *client)
{
	struct imx290 *priv = to_imx290(client);
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	int ret;

	ret = imx290_s_stream(subdev, 0);
	if (ret < 0)
		dev_err(&client->dev, "Failure to setup stream.\n");

	ret = imx290_s_power(subdev, 0);
	if (ret < 0)
		return ret;

	v4l2_async_unregister_subdev(&priv->subdev);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&priv->subdev.entity);
#endif
	v4l2_ctrl_handler_free(&priv->ctrl_handler);

	imx290_dbgfs_cleanup(client);

	return 0;
}

static const struct i2c_device_id imx290_id[] = {
	{"imx290", 0},
	{}
};

static const struct of_device_id imx290_of_match[] = {
	{ .compatible = "sony,imx290" },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, imx290_of_match);

MODULE_DEVICE_TABLE(i2c, imx290_id);
static struct i2c_driver imx290_i2c_driver = {
	.driver = {
		.of_match_table = of_match_ptr(imx290_of_match),
		.name = "imx290",
	},
	.probe = imx290_probe,
	.remove = imx290_remove,
	.id_table = imx290_id,
};

module_i2c_driver(imx290_i2c_driver);
MODULE_DESCRIPTION("Sony IMX290 Camera driver");
MODULE_AUTHOR("Bitmain");
MODULE_LICENSE("GPL v2");
