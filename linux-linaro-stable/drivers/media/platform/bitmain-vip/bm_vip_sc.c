#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/videodev2.h>
#include <linux/platform_device.h>
#include <media/videobuf2-vmalloc.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-event.h>
#include <media/v4l2-rect.h>

#include "vip/vip_common.h"
#include "vip/scaler.h"

#include "bm_debug.h"
#include "bm_vip_core.h"
#include "bm_vip_sc.h"

void bm_sc_device_run(void *priv)
{
	struct bm_sc_vdev *sdev = priv;
	struct vb2_buffer *vb2_buf;
	struct bm_vip_buffer *b = NULL;

	b = bm_vip_next_buf((struct bm_base_vdev *)sdev);
	if (!b)
		return;
	vb2_buf = &b->vb.vb2_buf;

	dprintk(VIP_DBG, "update sc-buf: 0x%lx-0x%lx-0x%lx\n",
	    vb2_buf->planes[0].m.userptr, vb2_buf->planes[1].m.userptr,
	    vb2_buf->planes[2].m.userptr);
	sclr_odma_set_addr(sdev->dev_idx, vb2_buf->planes[0].m.userptr,
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
static int bm_sc_queue_setup(struct vb2_queue *vq,
	       unsigned int *nbuffers, unsigned int *nplanes,
	       unsigned int sizes[], struct device *alloc_devs[])
{
	struct bm_sc_vdev *sdev = vb2_get_drv_priv(vq);
	unsigned int planes = sdev->fmt->buffers;
	unsigned int h = sdev->compose_out.height;
	unsigned int p;

	dprintk(VIP_VB2, "+\n");

	for (p = 0; p < planes; ++p)
		sizes[p] = sdev->bytesperline[p] * h;

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
static void bm_sc_buf_queue(struct vb2_buffer *vb)
{
	struct bm_sc_vdev *sdev = vb2_get_drv_priv(vb->vb2_queue);
	struct bm_vip_dev *bdev =
		container_of(sdev, struct bm_vip_dev, sc_vdev[sdev->dev_idx]);
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct bm_vip_buffer *bm_vb2 =
		container_of(vbuf, struct bm_vip_buffer, vb);

	dprintk(VIP_VB2, "+\n");

	bm_vip_buf_queue((struct bm_base_vdev *)sdev, bm_vb2);

	bm_vip_try_schedule(&bdev->img_vdev[sdev->img_src]);
}

static int bm_sc_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct bm_sc_vdev *sdev = vb2_get_drv_priv(vq);
	int rc = 0;

	dprintk(VIP_VB2, "+\n");

	sdev->seq_count = 0;
	return rc;
}

/* abort streaming and wait for last buffer */
static void bm_sc_stop_streaming(struct vb2_queue *vq)
{
	struct bm_sc_vdev *sdev = vb2_get_drv_priv(vq);
	struct bm_vip_buffer *bm_vb2, *tmp;
	unsigned long flags;
	struct vb2_buffer *vb2_buf;

	dprintk(VIP_VB2, "+\n");

	/*
	 * Release all the buffers enqueued to driver
	 * when streamoff is issued
	 */
	spin_lock_irqsave(&sdev->rdy_lock, flags);
	list_for_each_entry_safe(bm_vb2, tmp, &(sdev->rdy_queue), list) {
		vb2_buf = &(bm_vb2->vb.vb2_buf);
		if (vb2_buf->state == VB2_BUF_STATE_DONE)
			continue;
		vb2_buffer_done(vb2_buf, VB2_BUF_STATE_DONE);
	}
	sdev->num_rdy = 0;
	spin_unlock_irqrestore(&sdev->rdy_lock, flags);
}

const struct vb2_ops bm_sc_qops = {
//    .buf_init           =
	.queue_setup        = bm_sc_queue_setup,
//    .buf_finish         = bm_sc_buf_finish,
	.buf_queue          = bm_sc_buf_queue,
	.start_streaming    = bm_sc_start_streaming,
	.stop_streaming     = bm_sc_stop_streaming,
//    .wait_prepare       = vb2_ops_wait_prepare,
//    .wait_finish        = vb2_ops_wait_finish,
};

/*************************************************************************
 *	VB2-MEM-OPS definition
 *************************************************************************/
static void *sc_get_userptr(struct device *dev, unsigned long vaddr,
	unsigned long size, enum dma_data_direction dma_dir)
{
	return (void *)0xdeadbeef;
}

static void sc_put_userptr(void *buf_priv)
{
}

static const struct vb2_mem_ops bm_sc_vb2_mem_ops = {
	.get_userptr = sc_get_userptr,
	.put_userptr = sc_put_userptr,
};

/*************************************************************************
 *	FOPS definition
 *************************************************************************/
static int bm_sc_open(struct file *file)
{
	int rc = 0;
	struct bm_sc_vdev *sdev = video_drvdata(file);
	struct bm_vip_dev *bdev = NULL;
	struct vb2_queue *q;

	WARN_ON(!sdev);

	/* !!! only ONE open is allowed !!! */
	if (atomic_cmpxchg(&sdev->opened, 0, 1)) {
		dprintk(VIP_ERR, " device has been occupied. Reject %s\n",
				current->comm);
		return -EBUSY;
	}

	bdev = container_of(sdev, struct bm_vip_dev, sc_vdev[sdev->dev_idx]);
	spin_lock_init(&sdev->rdy_lock);

	// vb2_queue init
	q = &sdev->vb_q;
	q->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	q->io_modes = VB2_USERPTR;
	q->buf_struct_size = sizeof(struct bm_vip_buffer);
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	q->min_buffers_needed = 0;

	q->drv_priv = sdev;
	q->dev = bdev->v4l2_dev.dev;
	q->ops = &bm_sc_qops;
	q->mem_ops = &bm_sc_vb2_mem_ops;
	//q->lock = &sdev->lock;

	rc = vb2_queue_init(q);
	if (rc) {
		dprintk(VIP_ERR, "errcode(%d)\n", rc);
	} else {
		INIT_LIST_HEAD(&sdev->rdy_queue);
		sdev->num_rdy = 0;
		dprintk(VIP_INFO, "by %s\n", current->comm);
	}

	return rc;
}

static int bm_sc_release(struct file *file)
{
	struct bm_sc_vdev *sdev = video_drvdata(file);

	WARN_ON(!sdev);

	atomic_set(&sdev->opened, 0);

	dprintk(VIP_INFO, "-\n");
	return 0;
}

#if 0
static unsigned int bm_sc_poll(struct file *file,
	struct poll_table_struct *wait)
{
	struct bm_sc_vdev *sdev = video_drvdata(file);
	struct vb2_buffer *out_vb = NULL;
	unsigned long flags;
	int rc = 0;

	WARN_ON(!sdev);

	poll_wait(file, &sdev->vb_q.done_wq, wait);
	rc = vb2_fop_poll(file, wait);
	spin_lock_irqsave(&sdev->rdy_lock, flags);
	if (!list_empty(&sdev->vb_q.done_list))
	out_vb = list_first_entry(&sdev->vb_q.done_list, struct vb2_buffer,
				  done_entry);
	if (out_vb && (out_vb->state == VB2_BUF_STATE_DONE
		|| out_vb->state == VB2_BUF_STATE_ERROR))
	rc |= POLLOUT | POLLWRNORM;
	spin_unlock_irqrestore(&sdev->rdy_lock, flags);
	return rc;
}
#endif

static struct v4l2_file_operations bm_sc_fops = {
	.owner = THIS_MODULE,
	.open = bm_sc_open,
	.release = bm_sc_release,
	.poll = vb2_fop_poll, // .poll = bm_sc_poll,
	.unlocked_ioctl = video_ioctl2,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = video_ioctl2,
#endif
};

/*************************************************************************
 *	IOCTL definition
 *************************************************************************/
static int bm_sc_querycap(struct file *file, void *priv,
		    struct v4l2_capability *cap)
{
	struct bm_sc_vdev *sdev = video_drvdata(file);
	struct bm_vip_dev *bdev =
		container_of(sdev, struct bm_vip_dev, sc_vdev[sdev->dev_idx]);

	strlcpy(cap->driver, BM_VIP_DRV_NAME, sizeof(cap->driver));
	strlcpy(cap->card, BM_VIP_DVC_NAME, sizeof(cap->card));
	snprintf(cap->bus_info, sizeof(cap->bus_info),
	    "platform:%s", bdev->v4l2_dev.name);

	cap->capabilities = sdev->vid_caps | V4L2_CAP_DEVICE_CAPS;
	return 0;
}

static int bm_sc_g_ctrl(struct file *file, void *priv, struct v4l2_control *vc)
{
	int rc = -EINVAL;
	return rc;
}

static int bm_sc_s_ctrl(struct file *file, void *priv, struct v4l2_control *vc)
{
	int rc = -EINVAL;
	return rc;
}

static int _sc_ext_set_sc_d_to_img_v(struct bm_sc_vdev *sdev, u8 enable)
{
	struct sclr_top_cfg *cfg = NULL;
	struct bm_vip_dev *bdev =
		container_of(sdev, struct bm_vip_dev, sc_vdev[sdev->dev_idx]);

	if (sdev->dev_idx != 0) {
		dprintk(VIP_ERR,
			"sc-(%d) invalid for SC-D set img_src.\n",
			sdev->dev_idx);
		return -EINVAL;
	}

	if (enable >= 2) {
		dprintk(VIP_ERR, "invalid parameter(%d).\n", enable);
		return -EINVAL;
	}

	cfg = sclr_top_get_cfg();
	cfg->sclr_d_src = enable;
	sclr_top_set_cfg(cfg);

	sdev->img_src = enable;
	if (enable == 0) {
		bdev->img_vdev[BM_VIP_IMG_D].sc_bounding = BM_VIP_IMG_2_SC_D;
		bdev->img_vdev[BM_VIP_IMG_V].sc_bounding = BM_VIP_IMG_2_SC_V;
	} else {
		bdev->img_vdev[BM_VIP_IMG_D].sc_bounding = BM_VIP_IMG_2_SC_NONE;
		bdev->img_vdev[BM_VIP_IMG_V].sc_bounding = BM_VIP_IMG_2_SC_ALL;
	}

	return 0;
}

static int _sc_ext_set_flip(struct bm_sc_vdev *sdev, enum sclr_flip_mode flip)
{
	struct sclr_odma_cfg *cfg =  NULL;

	if (flip >= SCL_FLIP_MAX) {
		dprintk(VIP_ERR, "invalid parameter(%d) for flip.\n", flip);
		return -EINVAL;
	}
	cfg = sclr_odma_get_cfg(sdev->dev_idx);
	cfg->flip = flip;
	sclr_odma_set_cfg(sdev->dev_idx, cfg);

	return 0;
}

static int bm_sc_s_ext_ctrls(struct file *file, void *priv,
	struct v4l2_ext_controls *vc)
{
	struct v4l2_ext_control *ext_ctrls;
	int rc = -EINVAL, i = 0;
	struct bm_sc_vdev *sdev = video_drvdata(file);

	ext_ctrls = vc->controls;
	for (i = 0; i < vc->count; ++i) {
		switch (ext_ctrls[i].id) {
		case V4L2_CID_DV_VIP_SC_SET_SC_D_SRC_TO_V:
			rc = _sc_ext_set_sc_d_to_img_v(sdev,
					ext_ctrls[i].value);
		break;

		case V4L2_CID_DV_VIP_SC_SET_FLIP:
			rc = _sc_ext_set_flip(sdev, ext_ctrls[i].value);
		break;

		default:
		break;
		}
	}
	return rc;
}

int bm_sc_g_selection(struct file *file, void *priv, struct v4l2_selection *sel)
{
	struct bm_sc_vdev *sdev = video_drvdata(file);
	struct bm_vip_dev *bdev =
		container_of(sdev, struct bm_vip_dev, sc_vdev[sdev->dev_idx]);

	dprintk(VIP_DBG, "+\n");
	if (sel->type != V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
	return -EINVAL;

	sel->r.left = sel->r.top = 0;
	switch (sel->target) {
	case V4L2_SEL_TGT_CROP_BOUNDS:
	case V4L2_SEL_TGT_CROP_DEFAULT:
		sel->r.top = sel->r.left = 0;
		sel->r.width = bdev->img_vdev[sdev->img_src].crop_rect.width;
		sel->r.height = bdev->img_vdev[sdev->img_src].crop_rect.height;
	break;

	case V4L2_SEL_TGT_CROP:
		sel->r = sdev->crop_rect;
	break;

	case V4L2_SEL_TGT_COMPOSE:
		sel->r = sdev->compose_out;
	break;

	case V4L2_SEL_TGT_COMPOSE_DEFAULT:
	case V4L2_SEL_TGT_COMPOSE_BOUNDS:
		sel->r = sdev->sink_rect;
	break;

	default:
	return -EINVAL;
	}

	dprintk(VIP_INFO, "target(%d) rect(%d %d %d %d)\n", sel->target,
			sel->r.left, sel->r.top, sel->r.width, sel->r.height);
	return 0;
}

int bm_sc_s_selection(struct file *file, void *fh, struct v4l2_selection *sel)
{
	struct bm_sc_vdev *sdev = video_drvdata(file);

	dprintk(VIP_DBG, "+\n");
	if (sel->type != V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		return -EINVAL;

	switch (sel->target) {
	case V4L2_SEL_TGT_CROP_BOUNDS:
	{
		struct sclr_size size;

		size.w = sel->r.width;
		size.h = sel->r.height;
		sclr_set_input_size(sdev->dev_idx, size);
		sclr_set_scale(sdev->dev_idx);
	}
	break;
	case V4L2_SEL_TGT_CROP:
	if (memcmp(&sdev->crop_rect, &sel->r, sizeof(sel->r))) {
		struct sclr_rect rect;

		// TODO: check if crop-size over??
		vip_fill_rect_from_v4l2(rect, sel->r);
		sclr_set_crop(sdev->dev_idx, rect);
		sclr_set_scale(sdev->dev_idx);

		sdev->crop_rect = sel->r;
	}
	break;

	case V4L2_SEL_TGT_COMPOSE:
	if (memcmp(&sdev->compose_out, &sel->r, sizeof(sel->r))) {
		struct sclr_size size;
		struct sclr_odma_cfg *cfg;

		v4l2_rect_map_inside(&sel->r, &sdev->sink_rect);

		// update sc's output
		size.w = sel->r.width;
		size.h = sel->r.height;
		sclr_set_output_size(sdev->dev_idx, size);
		sclr_set_scale(sdev->dev_idx);

		// update out-2-mem's pos & size
		cfg = sclr_odma_get_cfg(sdev->dev_idx);
		cfg->mem.start_x = sel->r.left;
		cfg->mem.start_y = sel->r.top;
		cfg->mem.width   = sel->r.width;
		cfg->mem.height  = sel->r.height;
		sclr_odma_set_mem(sdev->dev_idx, &cfg->mem);

		sdev->compose_out = sel->r;
	}
	break;
	default:
	return -EINVAL;
	}

	dprintk(VIP_INFO, "target(%d) rect(%d %d %d %d)\n", sel->target,
			sel->r.left, sel->r.top, sel->r.width, sel->r.height);
	return 0;
}

int bm_sc_enum_fmt_vid_mplane(struct file *file, void  *priv,
		    struct v4l2_fmtdesc *f)
{
	dprintk(VIP_DBG, "+\n");
	return bm_vip_enum_fmt_vid(file, priv, f);
}

int bm_sc_g_fmt_vid_out_mplane(struct file *file, void *priv,
		    struct v4l2_format *f)
{
	struct bm_sc_vdev *sdev = video_drvdata(file);
	struct v4l2_pix_format_mplane *mp = &f->fmt.pix_mp;
	unsigned int p;

	dprintk(VIP_DBG, "+\n");
	WARN_ON(!sdev);

	mp->width        = sdev->compose_out.width;
	mp->height       = sdev->compose_out.height;
	mp->field        = V4L2_FIELD_NONE;
	mp->pixelformat  = sdev->fmt->fourcc;
	mp->colorspace   = sdev->colorspace;
	mp->xfer_func    = V4L2_XFER_FUNC_DEFAULT;
	mp->ycbcr_enc    = V4L2_YCBCR_ENC_DEFAULT;
	mp->quantization = V4L2_QUANTIZATION_DEFAULT;
	mp->num_planes   = sdev->fmt->buffers;
	for (p = 0; p < mp->num_planes; p++) {
	mp->plane_fmt[p].bytesperline = sdev->bytesperline[p];
	mp->plane_fmt[p].sizeimage =
	    sdev->bytesperline[p] * mp->height;
	}

	return 0;
}

int bm_sc_try_fmt_vid_out_mplane(struct file *file, void *priv,
	    struct v4l2_format *f)
{
	return bm_vip_try_fmt_vid_mplane(f);
}

static void _fill_odma_cfg(struct sclr_odma_cfg *cfg,
		struct v4l2_pix_format_mplane *mp)
{
	const struct bm_vip_fmt *fmt;
	struct v4l2_plane_pix_format *pfmt = mp->plane_fmt;

	fmt = bm_vip_get_format(mp->pixelformat);

	cfg->fmt = fmt->fmt;
	if (mp->colorspace == V4L2_COLORSPACE_SRGB)
		cfg->csc = SCL_CSC_NONE;
	else if (mp->colorspace == V4L2_COLORSPACE_SMPTE170M)
		cfg->csc = SCL_CSC_601_LIMIT_RGB2YUV;
	else
		cfg->csc = SCL_CSC_709_LIMIT_RGB2YUV;

	cfg->mem.pitch_y = pfmt[0].bytesperline;
	cfg->mem.pitch_c = pfmt[1].bytesperline;
	cfg->mem.width = mp->width;
	cfg->mem.height = mp->height;
}

int bm_sc_s_fmt_vid_out_mplane(struct file *file, void *priv,
	    struct v4l2_format *f)
{
	struct bm_sc_vdev *sdev = video_drvdata(file);
	struct v4l2_pix_format_mplane *mp = &f->fmt.pix_mp;
	struct v4l2_plane_pix_format *pfmt = mp->plane_fmt;
	const struct bm_vip_fmt *fmt;
	unsigned int p;
	struct sclr_odma_cfg *cfg;
	int rc = bm_sc_try_fmt_vid_out_mplane(file, priv, f);

	dprintk(VIP_DBG, "+\n");
	if (rc < 0)
		return rc;
	//TODO: check if job queue

	fmt = bm_vip_get_format(mp->pixelformat);
	sdev->fmt = fmt;
	sdev->colorspace = mp->colorspace;
	for (p = 0; p < mp->num_planes; p++)
		sdev->bytesperline[p] = pfmt[p].bytesperline;
	sdev->sink_rect.width = mp->width;
	sdev->sink_rect.height = mp->height;
	dprintk(VIP_INFO, "sink size(%d-%d)\n", sdev->sink_rect.width,
			sdev->sink_rect.height);

	cfg = sclr_odma_get_cfg(sdev->dev_idx);
	_fill_odma_cfg(cfg, mp);
	sclr_odma_set_cfg(sdev->dev_idx, cfg);

	if (mp->pixelformat == V4L2_PIX_FMT_HSV24) {
		enum sclr_out_mode mode = SCL_OUT_HSV;

		sclr_ctrl_set_output(sdev->dev_idx, mode, fmt->fmt, NULL);
	} else {
		enum sclr_out_mode mode = SCL_OUT_CSC;

		sclr_ctrl_set_output(sdev->dev_idx, mode, fmt->fmt, &cfg->csc);
	}

	return rc;
}

int bm_sc_streamon(struct file *file, void *fh, enum v4l2_buf_type i)
{
	struct bm_sc_vdev *sdev = video_drvdata(file);
	struct bm_vip_dev *bdev =
		container_of(sdev, struct bm_vip_dev, sc_vdev[sdev->dev_idx]);
	int rc = 0;

	rc = vb2_streamon(&sdev->vb_q, i);
	if (!rc) {
		struct sclr_top_cfg *cfg = sclr_top_get_cfg();

		cfg->sclr_enable[sdev->dev_idx] = true;
		sclr_top_set_cfg(cfg);

		bm_vip_try_schedule(&bdev->img_vdev[sdev->img_src]);
	}
	return rc;
}

int bm_sc_streamoff(struct file *file, void *fh, enum v4l2_buf_type i)
{
	struct bm_sc_vdev *sdev = video_drvdata(file);
	struct sclr_top_cfg *cfg = sclr_top_get_cfg();
	int rc = 0;

	rc = vb2_streamoff(&sdev->vb_q, i);
	cfg->sclr_enable[sdev->dev_idx] = false;
	sclr_top_set_cfg(cfg);
	return rc;
}

static const struct v4l2_ioctl_ops bm_sc_ioctl_ops = {
	.vidioc_querycap = bm_sc_querycap,
	.vidioc_g_ctrl = bm_sc_g_ctrl,
	.vidioc_s_ctrl = bm_sc_s_ctrl,
	.vidioc_s_ext_ctrls = bm_sc_s_ext_ctrls,

	.vidioc_g_selection     = bm_sc_g_selection,
	.vidioc_s_selection     = bm_sc_s_selection,
	.vidioc_enum_fmt_vid_out_mplane = bm_sc_enum_fmt_vid_mplane,
	.vidioc_g_fmt_vid_out_mplane    = bm_sc_g_fmt_vid_out_mplane,
	.vidioc_try_fmt_vid_out_mplane  = bm_sc_try_fmt_vid_out_mplane,
	.vidioc_s_fmt_vid_out_mplane    = bm_sc_s_fmt_vid_out_mplane,

	.vidioc_reqbufs         = vb2_ioctl_reqbufs,
	//.vidioc_create_bufs     = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf     = vb2_ioctl_prepare_buf,
	.vidioc_querybuf        = vb2_ioctl_querybuf,
	.vidioc_qbuf            = vb2_ioctl_qbuf,
	.vidioc_dqbuf           = vb2_ioctl_dqbuf,
	//.vidioc_expbuf          = vb2_ioctl_expbuf,
	.vidioc_streamon        = bm_sc_streamon,
	.vidioc_streamoff       = bm_sc_streamoff,

	//.vidioc_subscribe_event     = sc_subscribe_event,
	//.vidioc_unsubscribe_event   = v4l2_event_unsubscribe,
};

/*************************************************************************
 *	General functions
 *************************************************************************/
int sc_create_instance(struct platform_device *pdev)
{
	int rc = 0;
	struct bm_vip_dev *bdev;
	struct video_device *vfd;
	struct bm_sc_vdev *sdev;
	u8 i = 0;

	bdev = dev_get_drvdata(&pdev->dev);
	if (!bdev) {
		dprintk(VIP_ERR, "invalid data\n");
		return -EINVAL;
	}

	for (i = 0; i < BM_VIP_SC_MAX; ++i) {
		sdev = &bdev->sc_vdev[i];
		sdev->dev_idx = i;
		sdev->fmt = bm_vip_get_format(V4L2_PIX_FMT_RGBM);
		sdev->vid_caps = V4L2_CAP_VIDEO_OUTPUT_MPLANE |
			V4L2_CAP_STREAMING;
		sdev->img_src = (i == 0) ? BM_VIP_IMG_D : BM_VIP_IMG_V;
		spin_lock_init(&sdev->rdy_lock);
		memset(&sdev->crop_rect, 0, sizeof(sdev->crop_rect));
		memset(&sdev->compose_out, 0, sizeof(sdev->compose_out));
		memset(&sdev->sink_rect, 0, sizeof(sdev->sink_rect));

		vfd = &(sdev->vdev);
		snprintf(vfd->name, sizeof(vfd->name), "vip-sc%d", i);
		vfd->fops = &bm_sc_fops;
		vfd->ioctl_ops = &bm_sc_ioctl_ops;
		vfd->vfl_dir = VFL_DIR_TX;
		vfd->vfl_type = VFL_TYPE_GRABBER;
		vfd->minor = -1;
		vfd->device_caps = sdev->vid_caps;
		vfd->release = video_device_release_empty;
		vfd->v4l2_dev = &bdev->v4l2_dev;
		vfd->queue = &sdev->vb_q;

		rc = video_register_device(vfd, VFL_TYPE_GRABBER,
				SC_DEVICE_IDX + i);
		if (rc) {
			dprintk(VIP_ERR, "Failed to register sc-device%d\n", i);
			continue;
		}

		vfd->lock = &bdev->mutex;
		video_set_drvdata(vfd, sdev);
		atomic_set(&sdev->opened, 0);

		dprintk(VIP_INFO, "sc registered as %s\n",
				video_device_node_name(vfd));
	}

	return rc;
}

void sc_irq_handler(union sclr_intr intr_status, struct bm_vip_dev *bdev)
{
	u8 i = 0;
	struct bm_sc_vdev *sdev = NULL;

	for (i = 0; i < BM_VIP_SC_MAX; ++i) {

		if ((i == 0 && intr_status.b.scl0_frame_end == 0) ||
		    (i == 1 && intr_status.b.scl1_frame_end == 0) ||
		    (i == 2 && intr_status.b.scl2_frame_end == 0) ||
		    (i == 3 && intr_status.b.scl3_frame_end == 0))
		continue;

		sdev = &bdev->sc_vdev[i];
	}
}
