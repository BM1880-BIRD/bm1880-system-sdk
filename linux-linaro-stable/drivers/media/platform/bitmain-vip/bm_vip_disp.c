#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/videodev2.h>
#include <linux/v4l2-dv-timings.h>
#include <linux/platform_device.h>
#include <media/videobuf2-vmalloc.h>
#include <media/v4l2-common.h>
#include <media/v4l2-dv-timings.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-event.h>
#include <media/v4l2-rect.h>

#include "vip/vip_common.h"
#include "vip/scaler.h"

#include "bm_debug.h"
#include "bm_vip_core.h"
#include "bm_vip_disp.h"

#define MAX_DISP_WIDTH  1920
#define MAX_DISP_HEIGHT 1080

const struct v4l2_dv_timings_cap bm_disp_dv_timings_caps = {
	.type = V4L2_DV_BT_656_1120,
	/* keep this initialization for compatibility with GCC < 4.4.6 */
	.reserved = { 0 },
	V4L2_INIT_BT_TIMINGS(0, MAX_DISP_WIDTH, 0, MAX_DISP_HEIGHT,
		14000000, 148500000,
		V4L2_DV_BT_STD_CEA861 | V4L2_DV_BT_STD_DMT |
		V4L2_DV_BT_STD_CVT | V4L2_DV_BT_STD_GTF,
		V4L2_DV_BT_CAP_PROGRESSIVE)
};

const struct v4l2_dv_timings def_dv_timings[] = {
	{   // TTL for FPGA
		.type = V4L2_DV_BT_656_1120,
		V4L2_INIT_BT_TIMINGS(800, 600, 0, V4L2_DV_HSYNC_POS_POL,
			  27000000, 16, 9, 5, 1, 3, 24, 0, 0, 0,
			  V4L2_DV_BT_STD_CEA861)
	},
	V4L2_DV_BT_CEA_1920X1080P25,
	V4L2_DV_BT_CEA_1920X1080P30,
	V4L2_DV_BT_CEA_1920X1080I50,
	V4L2_DV_BT_CEA_1920X1080P50,
	V4L2_DV_BT_CEA_1920X1080I60,
	V4L2_DV_BT_CEA_1920X1080P60,
};

struct bm_vip_disp_pattern {
	enum sclr_disp_pat_type type;
	enum sclr_disp_pat_color color;
};

const struct bm_vip_disp_pattern patterns[BM_VIP_PAT_MAX] = {
	{.type = SCL_PAT_TYPE_OFF,	.color = SCL_PAT_COLOR_MAX},
	{.type = SCL_PAT_TYPE_SNOW,	.color = SCL_PAT_COLOR_MAX},
	{.type = SCL_PAT_TYPE_AUTO,	.color = SCL_PAT_COLOR_MAX},
	{.type = SCL_PAT_TYPE_FULL,	.color = SCL_PAT_COLOR_RED},
	{.type = SCL_PAT_TYPE_FULL,	.color = SCL_PAT_COLOR_GREEN},
	{.type = SCL_PAT_TYPE_FULL,	.color = SCL_PAT_COLOR_BLUE},
	{.type = SCL_PAT_TYPE_FULL,	.color = SCL_PAT_COLOR_BAR},
	{.type = SCL_PAT_TYPE_H_GRAD,   .color = SCL_PAT_COLOR_WHITE},
	{.type = SCL_PAT_TYPE_V_GRAD,   .color = SCL_PAT_COLOR_WHITE},
};

static bool service_isr = true;


static void _hw_enque(struct bm_disp_vdev *ddev)
{
	struct vb2_buffer *vb2_buf;
	struct bm_vip_buffer *b = NULL;

	if (!ddev || ddev->online)
		return;

	b = bm_vip_next_buf((struct bm_base_vdev *)ddev);
	if (!b)
		return;
	vb2_buf = &b->vb.vb2_buf;

	dprintk(VIP_DBG, "update disp-buf: 0x%lx-0x%lx-0x%lx\n",
	vb2_buf->planes[0].m.userptr, vb2_buf->planes[1].m.userptr,
	vb2_buf->planes[2].m.userptr);
	sclr_disp_set_addr(vb2_buf->planes[0].m.userptr,
		vb2_buf->planes[1].m.userptr, vb2_buf->planes[2].m.userptr);
}

/*************************************************************************
 *	VB2_OPS definition
 *************************************************************************/
/**
 * call before VIDIOC_REQBUFS to setup buf-queue.
 * nbuffers: number of buffer requested
 * nplanes:  number of plane each buffer
 * sizes:    size of each plane(bytes)
 */
static int bm_disp_queue_setup(struct vb2_queue *vq,
		unsigned int *nbuffers, unsigned int *nplanes,
		unsigned int sizes[], struct device *alloc_devs[])
{
	struct bm_disp_vdev *ddev = vb2_get_drv_priv(vq);
	unsigned int planes = ddev->fmt->buffers;
	unsigned int h = ddev->compose_out.height;
	unsigned int p;

	dprintk(VIP_VB2, "+\n");

	for (p = 0; p < planes; ++p)
		sizes[p] = ddev->bytesperline[p] * h;

	if (vq->num_buffers + *nbuffers < 2)
		*nbuffers = 2 - vq->num_buffers;

	*nplanes = planes;

	dprintk(VIP_INFO, "num_buffer=%d, num_plane=%d\n", *nbuffers, *nplanes);
	for (p = 0; p < *nplanes; p++)
		dprintk(VIP_INFO, "size[%u]=%u\n", p, sizes[p]);

	return 0;
}

/**
 * for VIDIOC_STREAMON, start fill data.
 */
static void bm_disp_buf_queue(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct bm_disp_vdev *ddev = vb2_get_drv_priv(vb->vb2_queue);
	struct bm_vip_buffer *bm_vb2 =
		container_of(vbuf, struct bm_vip_buffer, vb);

	dprintk(VIP_VB2, "+\n");

	bm_vip_buf_queue((struct bm_base_vdev *)ddev, bm_vb2);

	if (ddev->num_rdy == 1)
		_hw_enque(ddev);
}

static int bm_disp_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct bm_disp_vdev *ddev = vb2_get_drv_priv(vq);
	int rc = 0;

	dprintk(VIP_VB2, "+\n");

	ddev->seq_count = 0;
	ddev->frame_number = 0;

	sclr_disp_tgen_enable(true);
	return rc;
}

/* abort streaming and wait for last buffer */
static void bm_disp_stop_streaming(struct vb2_queue *vq)
{
	struct bm_disp_vdev *ddev = vb2_get_drv_priv(vq);
	struct bm_vip_buffer *bm_vb2, *tmp;
	unsigned long flags;
	struct vb2_buffer *vb2_buf;

	dprintk(VIP_VB2, "+\n");
	sclr_disp_tgen_enable(false);

	/*
	 * Release all the buffers enqueued to driver
	 * when streamoff is issued
	 */
	spin_lock_irqsave(&ddev->rdy_lock, flags);
	list_for_each_entry_safe(bm_vb2, tmp, &(ddev->rdy_queue), list) {
		vb2_buf = &(bm_vb2->vb.vb2_buf);
		if (vb2_buf->state == VB2_BUF_STATE_DONE)
			continue;
		vb2_buffer_done(vb2_buf, VB2_BUF_STATE_DONE);
	}
	ddev->num_rdy = 0;
	spin_unlock_irqrestore(&ddev->rdy_lock, flags);
}

const struct vb2_ops bm_disp_qops = {
//    .buf_init           =
	.queue_setup        = bm_disp_queue_setup,
//    .buf_finish         = bm_disp_buf_finish,
	.buf_queue          = bm_disp_buf_queue,
	.start_streaming    = bm_disp_start_streaming,
	.stop_streaming     = bm_disp_stop_streaming,
//    .wait_prepare       = vb2_ops_wait_prepare,
//    .wait_finish        = vb2_ops_wait_finish,
};

/*************************************************************************
 *	VB2-MEM-OPS definition
 *************************************************************************/
static void *disp_get_userptr(struct device *dev, unsigned long vaddr,
	unsigned long size, enum dma_data_direction dma_dir)
{
	return (void *)0xdeadbeef;
}

static void disp_put_userptr(void *buf_priv)
{
}

static const struct vb2_mem_ops bm_disp_vb2_mem_ops = {
	.get_userptr = disp_get_userptr,
	.put_userptr = disp_put_userptr,
};

/*************************************************************************
 *	FOPS definition
 *************************************************************************/
static int bm_disp_open(struct file *file)
{
	int rc = 0;
	struct bm_disp_vdev *ddev = video_drvdata(file);
	struct bm_vip_dev *bdev = NULL;
	struct vb2_queue *q;

	WARN_ON(!ddev);

	/* !!! only ONE open is allowed !!! */
	if (atomic_cmpxchg(&ddev->opened, 0, 1)) {
		dprintk(VIP_ERR, " device has been occupied. Reject %s\n",
				current->comm);
		return -EBUSY;
	}

	bdev = container_of(ddev, struct bm_vip_dev, disp_vdev);
	spin_lock_init(&ddev->rdy_lock);

	// vb2_queue init
	q = &ddev->vb_q;
	q->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	q->io_modes = VB2_USERPTR;
	q->buf_struct_size = sizeof(struct bm_vip_buffer);
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	q->min_buffers_needed = 0;

	q->drv_priv = ddev;
	q->dev = bdev->v4l2_dev.dev;
	q->ops = &bm_disp_qops;
	q->mem_ops = &bm_disp_vb2_mem_ops;
	//q->lock = &ddev->mutex;

	rc = vb2_queue_init(q);
	if (rc) {
		dprintk(VIP_ERR, "errcode(%d)\n", rc);
	} else {
		INIT_LIST_HEAD(&ddev->rdy_queue);
		ddev->num_rdy = 0;
		dprintk(VIP_INFO, "by %s\n", current->comm);
	}

	return rc;
}

static int bm_disp_release(struct file *file)
{
	struct bm_disp_vdev *ddev = video_drvdata(file);

	WARN_ON(!ddev);

	atomic_set(&ddev->opened, 0);

	dprintk(VIP_INFO, "-\n");
	return 0;
}

#if 0
static unsigned int bm_disp_poll(struct file *file,
	struct poll_table_struct *wait)
{
	struct bm_disp_vdev *ddev = video_drvdata(file);
	struct vb2_buffer *out_vb = NULL;
	unsigned long flags;
	int rc = 0;

	WARN_ON(!ddev);

	poll_wait(file, &ddev->vb_q.done_wq, wait);
	rc = vb2_fop_poll(file, wait);
	spin_lock_irqsave(&ddev->rdy_lock, flags);
	if (!list_empty(&ddev->vb_q.done_list))
	out_vb = list_first_entry(&ddev->vb_q.done_list, struct vb2_buffer,
				  done_entry);
	if (out_vb && (out_vb->state == VB2_BUF_STATE_DONE
		|| out_vb->state == VB2_BUF_STATE_ERROR))
	rc |= POLLOUT | POLLWRNORM;
	spin_unlock_irqrestore(&ddev->rdy_lock, flags);
	return rc;
}
#endif

static int disp_subscribe_event(struct v4l2_fh *fh,
	const struct v4l2_event_subscription *sub)
{
	if (sub->type != V4L2_EVENT_FRAME_SYNC)
	return -EINVAL;

	return v4l2_event_subscribe(fh, sub, BM_DISP_NEVENTS, NULL);
}

static struct v4l2_file_operations bm_disp_fops = {
	.owner = THIS_MODULE,
	.open = bm_disp_open,
	.release = bm_disp_release,
	.poll = vb2_fop_poll, //.poll = bm_disp_poll,
	.unlocked_ioctl = video_ioctl2,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = video_ioctl2,
#endif
};

/*************************************************************************
 *	IOCTL definition
 *************************************************************************/
static int bm_disp_querycap(struct file *file, void *priv,
			struct v4l2_capability *cap)
{
	struct bm_disp_vdev *ddev = video_drvdata(file);
	struct bm_vip_dev *bdev =
		container_of(ddev, struct bm_vip_dev, disp_vdev);

	strlcpy(cap->driver, BM_VIP_DRV_NAME, sizeof(cap->driver));
	strlcpy(cap->card, BM_VIP_DVC_NAME, sizeof(cap->card));
	snprintf(cap->bus_info, sizeof(cap->bus_info),
		"platform:%s", bdev->v4l2_dev.name);

	cap->capabilities = ddev->vid_caps | V4L2_CAP_DEVICE_CAPS;
	return 0;
}

static int bm_disp_g_ctrl(struct file *file, void *priv,
		struct v4l2_control *vc)
{
	int rc = -EINVAL;
	return rc;
}

static int bm_disp_s_ctrl(struct file *file, void *priv,
		struct v4l2_control *vc)
{
	int rc = -EINVAL;
	return rc;
}

static int bm_disp_s_ext_ctrls(struct file *file, void *priv,
	struct v4l2_ext_controls *vc)
{
	struct bm_disp_vdev *ddev = video_drvdata(file);
	struct v4l2_ext_control *ext_ctrls;
	union sclr_intr intr_mask;
	int rc = -EINVAL, i = 0;

	ext_ctrls = vc->controls;
	for (i = 0; i < vc->count; ++i) {
		switch (ext_ctrls[i].id) {
		case V4L2_CID_DV_VIP_DISP_INTR:
			service_isr = !service_isr;
			dprintk(VIP_DBG, "service_irs(%d)\n", service_isr);
			intr_mask = sclr_get_intr_mask();
			intr_mask.b.disp_frame_end = (service_isr) ? 1 : 0;
			sclr_set_intr_mask(intr_mask);
			rc = 0;
		break;

		case V4L2_CID_DV_VIP_DISP_OUT_CSC:
			if (ext_ctrls[i].value >= SCL_CSC_601_LIMIT_YUV2RGB &&
			    ext_ctrls[i].value <= SCL_CSC_709_FULL_YUV2RGB) {
				dprintk(VIP_ERR, "invalid disp-out-csc(%d)\n",
						ext_ctrls[i].value);
			} else {
				sclr_disp_set_out_csc(ext_ctrls[i].value);
			}
		break;

		case V4L2_CID_DV_VIP_DISP_PATTERN:
			if (ext_ctrls[i].value >= BM_VIP_PAT_MAX) {
				dprintk(VIP_ERR, "invalid disp-pattern(%d)\n",
						ext_ctrls[i].value);
				break;
			}
			sclr_disp_set_pattern(patterns[ext_ctrls[i].value].type,
					patterns[ext_ctrls[i].value].color);
		break;

		case V4L2_CID_DV_VIP_DISP_FRAME_BGCOLOR:
		{
			u16 r, g, b;

			r = *ext_ctrls[i].p_u16;
			g = *(ext_ctrls[i].p_u16 + 1);
			b = *(ext_ctrls[i].p_u16 + 2);
			sclr_disp_set_frame_bgcolor(r, g, b);
		}
		break;

		case V4L2_CID_DV_VIP_DISP_WINDOW_BGCOLOR:
		{
			u16 r, g, b;

			r = *ext_ctrls[i].p_u16;
			g = *(ext_ctrls[i].p_u16 + 1);
			b = *(ext_ctrls[i].p_u16 + 2);
			sclr_disp_set_window_bgcolor(r, g, b);
		}
		break;

		case V4L2_CID_DV_VIP_DISP_ONLINE:
			ddev->online = ext_ctrls[i].value;
			sclr_ctrl_set_disp_src(ddev->online);
		break;

		default:
		break;
		}
	}
	return rc;
}

int bm_disp_g_selection(struct file *file, void *priv,
		struct v4l2_selection *sel)
{
	struct bm_disp_vdev *ddev = video_drvdata(file);

	dprintk(VIP_DBG, "+\n");
	if (sel->type != V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		return -EINVAL;

	sel->r.left = sel->r.top = 0;
	switch (sel->target) {
	case V4L2_SEL_TGT_COMPOSE:
		sel->r = ddev->compose_out;
	break;

	case V4L2_SEL_TGT_COMPOSE_DEFAULT:
	case V4L2_SEL_TGT_COMPOSE_BOUNDS:
		sel->r = ddev->sink_rect;
	break;

	default:
	return -EINVAL;
	}

	dprintk(VIP_INFO, "target(%d) rect(%d %d %d %d)\n", sel->target,
			sel->r.left, sel->r.top, sel->r.width, sel->r.height);
	return 0;
}

int bm_disp_s_selection(struct file *file, void *fh, struct v4l2_selection *sel)
{
	struct bm_disp_vdev *ddev = video_drvdata(file);

	dprintk(VIP_DBG, "+\n");
	if (sel->type != V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		return -EINVAL;

	switch (sel->target) {
	case V4L2_SEL_TGT_COMPOSE:
		if (memcmp(&ddev->compose_out, &sel->r, sizeof(sel->r))) {
			struct sclr_rect rect;

			v4l2_rect_set_max_size(&sel->r, &ddev->sink_rect);
			vip_fill_rect_from_v4l2(rect, sel->r);
			if (sclr_disp_set_rect(rect) == 0)
				ddev->compose_out = sel->r;
		}
	break;

	default:
	return -EINVAL;
	}

	dprintk(VIP_INFO, "target(%d) rect(%d %d %d %d)\n", sel->target,
			sel->r.left, sel->r.top, sel->r.width, sel->r.height);
	return 0;
}

int bm_disp_enum_fmt_vid_mplane(struct file *file, void  *priv,
		struct v4l2_fmtdesc *f)
{
	dprintk(VIP_DBG, "+\n");
	return bm_vip_enum_fmt_vid(file, priv, f);
}

int bm_disp_g_fmt_vid_out_mplane(struct file *file, void *priv,
		struct v4l2_format *f)
{
	struct bm_disp_vdev *ddev = video_drvdata(file);
	struct v4l2_pix_format_mplane *mp = &f->fmt.pix_mp;
	unsigned int p;

	dprintk(VIP_DBG, "+\n");
	WARN_ON(!ddev);

	mp->width        = ddev->compose_out.width;
	mp->height       = ddev->compose_out.height;
	mp->field        = V4L2_FIELD_NONE;
	mp->pixelformat  = ddev->fmt->fourcc;
	mp->colorspace   = ddev->colorspace;
	mp->xfer_func    = V4L2_XFER_FUNC_DEFAULT;
	mp->ycbcr_enc    = V4L2_YCBCR_ENC_DEFAULT;
	mp->quantization = V4L2_QUANTIZATION_DEFAULT;
	mp->num_planes   = ddev->fmt->buffers;
	for (p = 0; p < mp->num_planes; p++) {
		mp->plane_fmt[p].bytesperline = ddev->bytesperline[p];
		mp->plane_fmt[p].sizeimage = ddev->bytesperline[p] * mp->height;
	}

	return 0;
}

int bm_disp_try_fmt_vid_out_mplane(struct file *file, void *priv,
		struct v4l2_format *f)
{
	return bm_vip_try_fmt_vid_mplane(f);
}

static void _fill_disp_cfg(struct sclr_disp_cfg *cfg,
		struct v4l2_pix_format_mplane *mp)
{
	const struct bm_vip_fmt *fmt;
	struct v4l2_plane_pix_format *pfmt = mp->plane_fmt;

	fmt = bm_vip_get_format(mp->pixelformat);

	cfg->fmt = fmt->fmt;
	if (mp->colorspace == V4L2_COLORSPACE_SRGB)
		cfg->in_csc = SCL_CSC_NONE;
	else if (mp->colorspace == V4L2_COLORSPACE_SMPTE170M)
		cfg->in_csc = SCL_CSC_601_LIMIT_RGB2YUV;
	else
		cfg->in_csc = SCL_CSC_709_LIMIT_RGB2YUV;

	cfg->mem.pitch_y = pfmt[0].bytesperline;
	cfg->mem.pitch_c = pfmt[1].bytesperline;
	cfg->mem.width = mp->width;
	cfg->mem.height = mp->height;
}

int bm_disp_s_fmt_vid_out_mplane(struct file *file, void *priv,
	struct v4l2_format *f)
{
	struct bm_disp_vdev *ddev = video_drvdata(file);
	struct v4l2_pix_format_mplane *mp = &f->fmt.pix_mp;
	struct v4l2_plane_pix_format *pfmt = mp->plane_fmt;
	const struct bm_vip_fmt *fmt;
	unsigned int p;
	struct sclr_disp_cfg *cfg;
	int rc = bm_disp_try_fmt_vid_out_mplane(file, priv, f);

	dprintk(VIP_DBG, "+\n");
	if (rc < 0)
		return rc;

	fmt = bm_vip_get_format(mp->pixelformat);
	ddev->fmt = fmt;
	ddev->colorspace = mp->colorspace;
	for (p = 0; p < mp->num_planes; p++)
		ddev->bytesperline[p] = pfmt[p].bytesperline;

	cfg = sclr_disp_get_cfg();
	_fill_disp_cfg(cfg, mp);
	sclr_disp_set_cfg(cfg);
	return rc;
}

static void _fill_disp_timing(struct sclr_disp_timing *timing,
		struct v4l2_bt_timings *bt_timing)
{
	// TODO: check calculation
	timing->vtotal = V4L2_DV_BT_FRAME_HEIGHT(bt_timing);
	timing->htotal = V4L2_DV_BT_FRAME_WIDTH(bt_timing);
	timing->vsync_start = 1;
	timing->vsync_end = timing->vsync_start + bt_timing->vsync;
	timing->vfde_start = timing->vmde_start =
		timing->vsync_end + bt_timing->vbackporch;
	timing->vfde_end = timing->vmde_end =
		timing->vfde_start + bt_timing->height - 1;
	timing->hsync_start = 1;
	timing->hsync_end = timing->hsync_start + bt_timing->hsync;
	timing->hfde_start = timing->hmde_start =
		timing->hsync_end + bt_timing->hbackporch;
	timing->hfde_end = timing->hmde_end =
		timing->hfde_start + bt_timing->width - 1;
	timing->vsync_pol = bt_timing->polarities & V4L2_DV_VSYNC_POS_POL;
	timing->hsync_pol = bt_timing->polarities & V4L2_DV_HSYNC_POS_POL;
}

int bm_disp_s_dv_timings(struct file *file, void *_fh,
		struct v4l2_dv_timings *timings)
{
	struct bm_disp_vdev *ddev = video_drvdata(file);
	struct sclr_disp_timing timing;

	dprintk(VIP_DBG, "+\n");
//	if (!v4l2_find_dv_timings_cap(timings, &bm_disp_dv_timings_caps, 0,
//		NULL, NULL))
//		return -EINVAL;

//	if (v4l2_match_dv_timings(timings, &ddev->dv_timings, 0, false))
//		return 0;
	if (vb2_is_busy(&ddev->vb_q))
		return -EBUSY;

	ddev->dv_timings = *timings;
	ddev->sink_rect.width = timings->bt.width;
	ddev->sink_rect.height = timings->bt.height;
	dprintk(VIP_INFO, "timing %d-%d\n", timings->bt.width,
			timings->bt.height);

	_fill_disp_timing(&timing, &timings->bt);
	sclr_disp_set_timing(&timing);
	return 0;
}

int bm_disp_g_dv_timings(struct file *file, void *_fh,
			struct v4l2_dv_timings *timings)
{
	struct bm_disp_vdev *ddev = video_drvdata(file);

	dprintk(VIP_DBG, "+\n");
	*timings = ddev->dv_timings;
	return 0;
}

int bm_disp_enum_dv_timings(struct file *file, void *_fh,
			struct v4l2_enum_dv_timings *timings)
{
	return v4l2_enum_dv_timings_cap(timings, &bm_disp_dv_timings_caps,
			NULL, NULL);
}

int bm_disp_dv_timings_cap(struct file *file, void *_fh,
			struct v4l2_dv_timings_cap *cap)
{
	*cap = bm_disp_dv_timings_caps;
	return 0;
}

int bm_disp_streamon(struct file *file, void *fh, enum v4l2_buf_type i)
{
	struct bm_disp_vdev *ddev = video_drvdata(file);
	int rc = 0;

	rc = vb2_streamon(&ddev->vb_q, i);
	if (!rc) {
		struct sclr_top_cfg *cfg = sclr_top_get_cfg();

		cfg->disp_enable = true;
		sclr_top_set_cfg(cfg);
	}
	return rc;
}

int bm_disp_streamoff(struct file *file, void *fh, enum v4l2_buf_type i)
{
	struct bm_disp_vdev *ddev = video_drvdata(file);
	struct sclr_top_cfg *cfg = sclr_top_get_cfg();
	int rc = 0;

	rc = vb2_streamoff(&ddev->vb_q, i);
	cfg->disp_enable = false;
	sclr_top_set_cfg(cfg);
	return rc;
}

int bm_disp_enum_output(struct file *file, void *priv,
			struct v4l2_output *out)
{
	int rc = 0;
	return rc;
}

int bm_disp_g_output(struct file *file, void *priv, unsigned int *o)
{
	struct bm_disp_vdev *ddev = video_drvdata(file);
	int rc = 0;
	*o = ddev->disp_interface;
	return rc;
}

int bm_disp_s_output(struct file *file, void *priv, unsigned int o)
{
	struct bm_disp_vdev *ddev = video_drvdata(file);
	int rc = 0;

	dprintk(VIP_DBG, "+\n");
	if (o >= BM_VIP_DISP_INTF_MAX)
		return -EINVAL;

	ddev->disp_interface = o;

	// update hw

	return rc;
}

static const struct v4l2_ioctl_ops bm_disp_ioctl_ops = {
	.vidioc_querycap = bm_disp_querycap,
	.vidioc_g_ctrl = bm_disp_g_ctrl,
	.vidioc_s_ctrl = bm_disp_s_ctrl,
	.vidioc_s_ext_ctrls = bm_disp_s_ext_ctrls,

	.vidioc_g_selection     = bm_disp_g_selection,
	.vidioc_s_selection     = bm_disp_s_selection,
	.vidioc_enum_fmt_vid_out_mplane = bm_disp_enum_fmt_vid_mplane,
	.vidioc_g_fmt_vid_out_mplane    = bm_disp_g_fmt_vid_out_mplane,
	.vidioc_try_fmt_vid_out_mplane  = bm_disp_try_fmt_vid_out_mplane,
	.vidioc_s_fmt_vid_out_mplane    = bm_disp_s_fmt_vid_out_mplane,

	.vidioc_s_dv_timings        = bm_disp_s_dv_timings,
	.vidioc_g_dv_timings        = bm_disp_g_dv_timings,
//	.vidioc_query_dv_timings    = bm_disp_query_dv_timings,
	.vidioc_enum_dv_timings     = bm_disp_enum_dv_timings,
	.vidioc_dv_timings_cap      = bm_disp_dv_timings_cap,

	.vidioc_reqbufs         = vb2_ioctl_reqbufs,
	//.vidioc_create_bufs     = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf     = vb2_ioctl_prepare_buf,
	.vidioc_querybuf        = vb2_ioctl_querybuf,
	.vidioc_qbuf            = vb2_ioctl_qbuf,
	.vidioc_dqbuf           = vb2_ioctl_dqbuf,
	//.vidioc_expbuf          = vb2_ioctl_expbuf,
	.vidioc_streamon        = bm_disp_streamon,
	.vidioc_streamoff       = bm_disp_streamoff,

	.vidioc_enum_output     = bm_disp_enum_output,
	.vidioc_g_output        = bm_disp_g_output,
	.vidioc_s_output        = bm_disp_s_output,

	.vidioc_subscribe_event     = disp_subscribe_event,
	.vidioc_unsubscribe_event   = v4l2_event_unsubscribe,
};

/*************************************************************************
 *	General functions
 *************************************************************************/
int disp_create_instance(struct platform_device *pdev)
{
	int rc = 0;
	struct bm_vip_dev *bdev;
	struct video_device *vfd;
	struct bm_disp_vdev *ddev;

	bdev = dev_get_drvdata(&pdev->dev);
	if (!bdev) {
		dprintk(VIP_ERR, "invalid data\n");
		return -EINVAL;
	}
	ddev = &bdev->disp_vdev;
	ddev->fmt = bm_vip_get_format(V4L2_PIX_FMT_RGBM);
	ddev->vid_caps = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING;
	ddev->dv_timings = def_dv_timings[0];
	spin_lock_init(&ddev->rdy_lock);
	memset(&ddev->sink_rect, 0, sizeof(ddev->sink_rect));
	ddev->sink_rect.width = ddev->dv_timings.bt.width;
	ddev->sink_rect.height = ddev->dv_timings.bt.height;

	vfd = &(ddev->vdev);
	snprintf(vfd->name, sizeof(vfd->name), "vip-disp");
	vfd->fops = &bm_disp_fops;
	vfd->ioctl_ops = &bm_disp_ioctl_ops;
	vfd->vfl_dir = VFL_DIR_TX;
	vfd->vfl_type = VFL_TYPE_GRABBER;
	vfd->minor = -1;
	vfd->device_caps = ddev->vid_caps;
	vfd->release = video_device_release_empty;
	vfd->v4l2_dev = &bdev->v4l2_dev;
	vfd->queue = &ddev->vb_q;

	rc = video_register_device(vfd, VFL_TYPE_GRABBER, DISP_DEVICE_IDX);
	if (rc) {
		dprintk(VIP_ERR, "Failed to register disp-device\n");
		goto err_register;
	}

	vfd->lock = &bdev->mutex;
	video_set_drvdata(vfd, ddev);
	atomic_set(&ddev->opened, 0);
	ddev->online = false;

	dprintk(VIP_INFO, "disp registered as %s\n",
			video_device_node_name(vfd));
	return rc;

err_register:
	return rc;
}

void disp_irq_handler(union sclr_intr intr_status, struct bm_vip_dev *bdev)
{
	if (intr_status.b.disp_frame_end) {
		struct bm_disp_vdev *ddev = &bdev->disp_vdev;
		struct bm_vip_buffer *b = NULL;
		++bdev->disp_vdev.frame_number;

		if (ddev->num_rdy > 1) {
			b = bm_vip_buf_remove((struct bm_base_vdev *)ddev);
			b->vb.vb2_buf.timestamp = ktime_get_ns();
			b->vb.sequence = ++ddev->seq_count;
			vb2_buffer_done(&b->vb.vb2_buf, VB2_BUF_STATE_DONE);

			_hw_enque(ddev);
		}

	}
}
