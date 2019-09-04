#ifndef _BM_VIP_H_
#define _BM_VIP_H_

#include <linux/version.h>
#include <linux/videodev2.h>
#include <linux/pm_qos.h>
#include <linux/iommu.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fh.h>

#include "vip/scaler.h"
#include "vip/inc/isp_reg.h"
#include "vip/isp_drv.h"

#define BM_VIP_DRV_NAME "bm_vip_driver"
#define BM_VIP_DVC_NAME "bm_vip_1880v2"

#define DEVICE_FROM_DTS 1

#define ISP_DEVICE_IDX  0
#define DWA_DEVICE_IDX  2
#define IMG_DEVICE_IDX  3   // 3,4
#define SC_DEVICE_IDX   5   // 5~8
#define DISP_DEVICE_IDX 9

#define VIP_MAX_PLANES 3
#define ISP_NUM_INPUTS 2

// TODO: move fmt to v4l2-fourcc.h
// 24  RGB-8-8-8 planar
#define V4L2_PIX_FMT_RGBM   v4l2_fourcc('R', 'G', 'B', 'M')
/* HSV formats */
#define V4L2_PIX_FMT_HSV24  v4l2_fourcc('H', 'S', 'V', '3')
#define V4L2_PIX_FMT_HSV32  v4l2_fourcc('H', 'S', 'V', '4')

// TODO: move ctrls to v4l2-controls.h
#define V4L2_CID_DV_VIP_BASE                    (V4L2_CTRL_CLASS_DV | 0x2000)
#define V4L2_CID_DV_VIP_DWA_BASE                (V4L2_CID_DV_VIP_BASE | 0x000)
#define V4L2_CID_DV_VIP_DWA_SET_MESH            (V4L2_CID_DV_VIP_BASE | 0x000)
#define V4L2_CID_DV_VIP_IMG_BASE                (V4L2_CID_DV_VIP_BASE | 0x100)
#define V4L2_CID_DV_VIP_IMG_INTR                (V4L2_CID_DV_VIP_IMG_BASE + 0)
#define V4L2_CID_DV_VIP_SC_BASE                 (V4L2_CID_DV_VIP_BASE | 0x200)
#define V4L2_CID_DV_VIP_SC_SET_SC_D_SRC_TO_V    (V4L2_CID_DV_VIP_SC_BASE + 0)
#define V4L2_CID_DV_VIP_SC_SET_FLIP             (V4L2_CID_DV_VIP_SC_BASE + 1)
#define V4L2_CID_DV_VIP_SC_SET_COLORSPACE       (V4L2_CID_DV_VIP_SC_BASE + 2)
#define V4L2_CID_DV_VIP_DISP_BASE               (V4L2_CID_DV_VIP_BASE | 0x300)
#define V4L2_CID_DV_VIP_DISP_VB_DONE            (V4L2_CID_DV_VIP_DISP_BASE + 0)
#define V4L2_CID_DV_VIP_DISP_INTR               (V4L2_CID_DV_VIP_DISP_BASE + 1)
#define V4L2_CID_DV_VIP_DISP_OUT_CSC            (V4L2_CID_DV_VIP_DISP_BASE + 2)
#define V4L2_CID_DV_VIP_DISP_PATTERN            (V4L2_CID_DV_VIP_DISP_BASE + 3)
#define V4L2_CID_DV_VIP_DISP_FRAME_BGCOLOR      (V4L2_CID_DV_VIP_DISP_BASE + 4)
#define V4L2_CID_DV_VIP_DISP_WINDOW_BGCOLOR     (V4L2_CID_DV_VIP_DISP_BASE + 5)
#define V4L2_CID_DV_VIP_DISP_ONLINE             (V4L2_CID_DV_VIP_DISP_BASE + 6)
#define V4L2_CID_DV_VIP_ISP_BASE                (V4L2_CID_DV_VIP_BASE | 0x400)
#define V4L2_CID_DV_VIP_ISP_PATTERN             (V4L2_CID_DV_VIP_ISP_BASE + 0)
#define V4L2_CID_DV_VIP_ISP_ONLINE              (V4L2_CID_DV_VIP_ISP_BASE + 1)

#define vip_fill_rect_from_v4l2(vip_rect, v4l2_rect) \
	do { \
		vip_rect.x = v4l2_rect.left; \
		vip_rect.y = v4l2_rect.top; \
		vip_rect.w = v4l2_rect.width; \
		vip_rect.h = v4l2_rect.height; \
	} while (0)


// TODO: move to videodev2.h
enum v4l2_hsv_encoding {
	/* Hue mapped to 0 - 179 */
	V4L2_HSV_ENC_180        = 128,
	/* Hue mapped to 0-255 */
	V4L2_HSV_ENC_256        = 129,
};

enum bm_vip_irq {
	BM_VIP_IRQ_SC = 0,
	BM_VIP_IRQ_DWA,
	BM_VIP_IRQ_ISP,
	BM_VIP_IRQ_MAX,
};

enum bm_img_type {
	BM_VIP_IMG_D = 0,
	BM_VIP_IMG_V,
	BM_VIP_IMG_MAX,
};

enum bm_img_2_sc_bounding {
	BM_VIP_IMG_2_SC_NONE = 0x00,
	BM_VIP_IMG_2_SC_D = 0x01,
	BM_VIP_IMG_2_SC_V = 0x02,
	BM_VIP_IMG_2_SC_ALL = 0x03,
};

enum bm_input_type {
	BM_VIP_INPUT_ISP = 0,
	BM_VIP_INPUT_DWA,
	BM_VIP_INPUT_MEM,
	BM_VIP_INPUT_MAX,
};

enum bm_sc_type {
	BM_VIP_SC_D = 0,
	BM_VIP_SC_V0,
	BM_VIP_SC_V1,
	BM_VIP_SC_V2,
	BM_VIP_SC_MAX,
};

enum bm_disp_intf {
	BM_VIP_DISP_INTF_DSI = 0,
	BM_VIP_DISP_INTF_BT,
	BM_VIP_DISP_INTF_RGB,
	BM_VIP_DISP_INTF_LVDS,
	BM_VIP_DISP_INTF_MAX,
};

enum bm_vip_pattern {
	BM_VIP_PAT_OFF = 0,
	BM_VIP_PAT_SNOW,
	BM_VIP_PAT_AUTO,
	BM_VIP_PAT_RED,
	BM_VIP_PAT_GREEN,
	BM_VIP_PAT_BLUE,
	BM_VIP_PAT_COLORBAR,
	BM_VIP_PAT_GRAY_GRAD_H,
	BM_VIP_PAT_GRAY_GRAD_V,
	BM_VIP_PAT_MAX,
};

/* struct bm_vip_fmt
 * @fourcc: v4l2 fourcc code.
 * @fmt: sclr driver's relative format.
 * @buffers: number of buffers.
 * @bit_depth: number of bits per pixel per plane.
 * @plane_sub_h: plane horizontal subsample.
 * @plane_sub_v: plane vertical subsample.
 */
struct bm_vip_fmt {
	u32 fourcc;          /* v4l2 format id */
	enum sclr_format fmt;
	u8  buffers;
	u32 bit_depth[VIP_MAX_PLANES];
	u8 plane_sub_h;
	u8 plane_sub_v;
};

/* buffer for one video frame */
struct bm_vip_buffer {
	/* common v4l buffer stuff -- must be first */
	struct vb2_v4l2_buffer          vb;
	struct list_head                list;
};

/**
 * @vdev: video_device instance
 * @opened: if device opened
 * @vb_q: vb2_queue
 * @rdy_queue: list of bm_vip_buffer in the vb_q
 * @rdy_lock: spinlock for rdy_queue
 * @num_rdy: number of buffer in rdy_queue
 * @seq_count: the number of vb output from queue
 * @vid_caps: capabilities of the device
 * @fmt: format
 * @colorspace: V4L2 colorspace
 * @bytesperline: bytesperline of each plane
 */
#define DEFINE_BASE_VDEV_PARAMS \
	struct video_device             vdev;       \
	atomic_t                        opened;     \
	struct vb2_queue                vb_q;       \
	struct list_head                rdy_queue;  \
	spinlock_t               rdy_lock;   \
	u8                              num_rdy;    \
	u32                             seq_count;  \
	u32                             vid_caps;   \
	const struct bm_vip_fmt         *fmt;       \
	u32                             colorspace; \
	u32                             bytesperline[VIP_MAX_PLANES]

struct bm_base_vdev {
	DEFINE_BASE_VDEV_PARAMS;
};

struct bm_sensor_info {
	struct v4l2_async_subdev asd;
	struct v4l2_subdev *subdev;
};

struct bm_isp_vdev {
	DEFINE_BASE_VDEV_PARAMS;

	// private data
	u32                             frame_number;
	struct v4l2_frmsize_discrete    src_size;

	struct bm_sensor_info		sensor[ISP_NUM_INPUTS];
	u32				num_sd;
	u32                             preraw_sof_count[ISP_NUM_INPUTS];
	u32                             preraw_frame_number[ISP_NUM_INPUTS];
	u32                             postraw_frame_number;
	struct v4l2_dv_timings          dv_timings;
	bool                            online;
	struct isp_ctx                  ctx;
	struct v4l2_async_notifier	subdev_notifier;
	struct v4l2_async_subdev	*async_subdevs[ISP_NUM_INPUTS];
};

struct bm_dwa_vdev {
	struct video_device             vdev;
	struct v4l2_m2m_dev             *m2m_dev;
	atomic_t                        opened;
	u32                             vid_caps;
	struct mutex                    mutex;
	const struct bm_vip_fmt         *fmt;
	u32                             colorspace;
	struct v4l2_fh                  fh;

	// private data
	struct bm_dwa_data {
	u32                         bytesperline[VIP_MAX_PLANES];
	u32                         sizeimage[VIP_MAX_PLANES];
	u16                         w;
	u16                         h;
	} cap_data, out_data;
};

/**
 * @dev_idx:  index of the device
 * @img_type: the type of this img_vdev
 * @sc_bounding: which sc instances are bounding with this img_vdev
 * @input_type: input type(isp, dwa, mem,...)
 * @job_flags: job status
 * @job_lock: lock of job
 * @src_size: img's input size
 * @crop_rect: img's output size, only work if input_type is mem
 */
struct bm_img_vdev {
	DEFINE_BASE_VDEV_PARAMS;

	// private data
	u8                              dev_idx;
	enum sclr_img_in                img_type;
	enum bm_img_2_sc_bounding       sc_bounding;
	enum bm_input_type              input_type;
	unsigned long                   job_flags;
	spinlock_t                      job_lock;

	struct v4l2_frmsize_discrete    src_size;
	struct v4l2_rect                crop_rect;
};

/**
 * @dev_idx:  index of the device
 * @img_src:  bounding source of this sc_vdev
 * @sink_rect: sc's output rectangle, only if out-2-mem
 * @compose_out: sc's output size,
 * @crop_rect: sc's crop rectangle
 */
struct bm_sc_vdev {
	DEFINE_BASE_VDEV_PARAMS;

	// private data
	u8                              dev_idx;
	enum bm_img_type                img_src;

	struct v4l2_rect                sink_rect;
	struct v4l2_rect                compose_out;
	struct v4l2_rect                crop_rect;
};

/**
 * @frame_number:  the number of vsync
 * @dv_timings: current v4l2 dv timing
 * @sink_rect: panel's resolution
 * @compose_out: output rectangle on frame
 * @online: if online mode from sc_d
 * @disp_interface: display interface
 */
struct bm_disp_vdev {
	DEFINE_BASE_VDEV_PARAMS;

	// private data
	u32                             frame_number;

	struct v4l2_dv_timings          dv_timings;
	struct v4l2_rect                sink_rect;
	struct v4l2_rect                compose_out;
	bool                            online;
	enum bm_disp_intf               disp_interface;
};

struct bm_vip_dev {
	struct v4l2_device              v4l2_dev;
	spinlock_t               lock;
	struct mutex                    mutex;

	unsigned int                    irq_num_scl;
	unsigned int                    irq_num_dwa;
	unsigned int                    irq_num_isp;

	struct bm_dwa_vdev              dwa_vdev;
	struct bm_img_vdev              img_vdev[BM_VIP_IMG_MAX];
	struct bm_sc_vdev               sc_vdev[BM_VIP_SC_MAX];
	struct bm_disp_vdev             disp_vdev;
	struct bm_isp_vdev              isp_vdev;
};

const struct bm_vip_fmt *bm_vip_get_format(u32 pixelformat);
int bm_vip_enum_fmt_vid(struct file *file, void *priv, struct v4l2_fmtdesc *f);
int bm_vip_try_fmt_vid_mplane(struct v4l2_format *f);
void bm_vip_buf_queue(struct bm_base_vdev *vdev, struct bm_vip_buffer *b);
struct bm_vip_buffer *bm_vip_next_buf(struct bm_base_vdev *vdev);
struct bm_vip_buffer *bm_vip_buf_remove(struct bm_base_vdev *vdev);
void bm_vip_try_schedule(struct bm_img_vdev *idev);
void bm_vip_job_finish(struct bm_img_vdev *idev);
bool bm_vip_job_is_queued(struct bm_img_vdev *idev);

#endif
