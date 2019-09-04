#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/videodev2.h>
#include <linux/platform_device.h>
#include <media/videobuf2-vmalloc.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-event.h>

#include "vip/vip_common.h"
#include "vip/scaler.h"

#include "bm_debug.h"
#include "bm_vip_core.h"
#include "bm_vip_img.h"
#include "bm_vip_sc.h"

static bool service_isr = true;

/*************************************************************************
 *	VB2_OPS definition
 *************************************************************************/
/**
 * call before VIDIOC_REQBUFS to setup buf-queue.
 * nbuffers: number of buffer requested
 * nplanes:  number of plane each buffer
 * sizes:    size of each plane(bytes)
 */
static int bm_img_queue_setup(struct vb2_queue *vq,
	       unsigned int *nbuffers, unsigned int *nplanes,
	       unsigned int sizes[], struct device *alloc_devs[])
{
	struct bm_img_vdev *idev = vb2_get_drv_priv(vq);
	unsigned int planes = idev->fmt->buffers;
	unsigned int h = idev->src_size.height;
	unsigned int p;

	dprintk(VIP_VB2, "+\n");

	for (p = 0; p < planes; ++p)
		sizes[p] = idev->bytesperline[p] * h;

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
static void bm_img_buf_queue(struct vb2_buffer *vb)
{
	struct bm_img_vdev *idev = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct bm_vip_buffer *bm_vb2 =
		container_of(vbuf, struct bm_vip_buffer, vb);

	dprintk(VIP_VB2, "+\n");

	bm_vip_buf_queue((struct bm_base_vdev *)idev, bm_vb2);

	bm_vip_try_schedule(idev);
}

static int bm_img_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct bm_img_vdev *idev = vb2_get_drv_priv(vq);
	int rc = 0;

	dprintk(VIP_VB2, "+\n");

	idev->seq_count = 0;

	return rc;
}

/* abort streaming and wait for last buffer */
static void bm_img_stop_streaming(struct vb2_queue *vq)
{
	struct bm_img_vdev *idev = vb2_get_drv_priv(vq);
	struct bm_vip_buffer *bm_vb2, *tmp;
	unsigned long flags;
	struct vb2_buffer *vb2_buf;

	dprintk(VIP_VB2, "+\n");

	/*
	 * Release all the buffers enqueued to driver
	 * when streamoff is issued
	 */
	spin_lock_irqsave(&idev->rdy_lock, flags);
	list_for_each_entry_safe(bm_vb2, tmp, &(idev->rdy_queue), list) {
		vb2_buf = &(bm_vb2->vb.vb2_buf);
		if (vb2_buf->state == VB2_BUF_STATE_DONE)
			continue;
		vb2_buffer_done(vb2_buf, VB2_BUF_STATE_DONE);
	}
	idev->num_rdy = 0;
	spin_unlock_irqrestore(&idev->rdy_lock, flags);
}

const struct vb2_ops bm_img_qops = {
//    .buf_init           =
	.queue_setup        = bm_img_queue_setup,
//    .buf_finish         = bm_img_buf_finish,
	.buf_queue          = bm_img_buf_queue,
	.start_streaming    = bm_img_start_streaming,
	.stop_streaming     = bm_img_stop_streaming,
//    .wait_prepare       = vb2_ops_wait_prepare,
//    .wait_finish        = vb2_ops_wait_finish,
};

/*************************************************************************
 *	VB2-MEM-OPS definition
 *************************************************************************/
static void *img_get_userptr(struct device *dev, unsigned long vaddr,
	unsigned long size, enum dma_data_direction dma_dir)
{
	return (void *)0xdeadbeef;
}

static void img_put_userptr(void *buf_priv)
{
}

static const struct vb2_mem_ops bm_img_vb2_mem_ops = {
	.get_userptr = img_get_userptr,
	.put_userptr = img_put_userptr,
};

/*************************************************************************
 *	FOPS definition
 *************************************************************************/
static int bm_img_open(struct file *file)
{
	int rc = 0;
	struct bm_img_vdev *idev = video_drvdata(file);
	struct bm_vip_dev *bdev = NULL;
	struct vb2_queue *q;

	WARN_ON(!idev);

	/* !!! only ONE open is allowed !!! */
	if (atomic_cmpxchg(&idev->opened, 0, 1)) {
		dprintk(VIP_ERR, " device has been occupied. Reject %s\n",
				current->comm);
		return -EBUSY;
	}

	bdev = container_of(idev, struct bm_vip_dev, img_vdev[idev->dev_idx]);
	spin_lock_init(&idev->rdy_lock);

	// vb2_queue init
	q = &idev->vb_q;
	q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	q->io_modes = VB2_USERPTR;
	q->buf_struct_size = sizeof(struct bm_vip_buffer);
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	q->min_buffers_needed = 0;

	q->drv_priv = idev;
	q->dev = bdev->v4l2_dev.dev;
	q->ops = &bm_img_qops;
	q->mem_ops = &bm_img_vb2_mem_ops;
	//q->lock = &idev->lock;

	rc = vb2_queue_init(q);
	if (rc) {
		dprintk(VIP_ERR, "errcode(%d)\n", rc);
	} else {
		INIT_LIST_HEAD(&idev->rdy_queue);
		idev->num_rdy = 0;
		dprintk(VIP_INFO, "by %s\n", current->comm);
	}

	return rc;
}

static int bm_img_release(struct file *file)
{
	struct bm_img_vdev *idev = video_drvdata(file);

	WARN_ON(!idev);

	atomic_set(&idev->opened, 0);

	dprintk(VIP_INFO, "-\n");
	return 0;
}

#if 0
static unsigned int bm_img_poll(struct file *file,
	struct poll_table_struct *wait)
{
	struct bm_img_vdev *idev = video_drvdata(file);
	struct vb2_buffer *cap_vb = NULL;
	unsigned long flags;
	int rc = 0;

	WARN_ON(!idev);

	poll_wait(file, &idev->vb_q.done_wq, wait);
	rc = vb2_fop_poll(file, wait);
	spin_lock_irqsave(&idev->rdy_lock, flags);
	if (!list_empty(&idev->vb_q.done_list))
	cap_vb = list_first_entry(&idev->vb_q.done_list, struct vb2_buffer,
				  done_entry);
	if (cap_vb && (cap_vb->state == VB2_BUF_STATE_DONE
		|| cap_vb->state == VB2_BUF_STATE_ERROR))
	rc |= POLLIN | POLLRDNORM;
	spin_unlock_irqrestore(&idev->rdy_lock, flags);
	return rc;
}
#endif

static struct v4l2_file_operations bm_img_fops = {
	.owner = THIS_MODULE,
	.open = bm_img_open,
	.release = bm_img_release,
	.poll = vb2_fop_poll, //    .poll = bm_img_poll,
	.unlocked_ioctl = video_ioctl2,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = video_ioctl2,
#endif
};

/*************************************************************************
 *	IOCTL definition
 *************************************************************************/
static int bm_img_querycap(struct file *file, void *priv,
		    struct v4l2_capability *cap)
{
	struct bm_img_vdev *idev = video_drvdata(file);
	struct bm_vip_dev *bdev =
		container_of(idev, struct bm_vip_dev, img_vdev[idev->dev_idx]);

	strlcpy(cap->driver, BM_VIP_DRV_NAME, sizeof(cap->driver));
	strlcpy(cap->card, BM_VIP_DVC_NAME, sizeof(cap->card));
	snprintf(cap->bus_info, sizeof(cap->bus_info),
	    "platform:%s", bdev->v4l2_dev.name);

	cap->capabilities = idev->vid_caps | V4L2_CAP_DEVICE_CAPS;
	return 0;
}

static int bm_img_g_ctrl(struct file *file, void *priv, struct v4l2_control *vc)
{
	int rc = -EINVAL;
	return rc;
}

static int bm_img_s_ctrl(struct file *file, void *priv, struct v4l2_control *vc)
{
	int rc = -EINVAL;
	return rc;
}

static int bm_img_s_ext_ctrls(struct file *file, void *priv,
	struct v4l2_ext_controls *vc)
{
	struct v4l2_ext_control *ext_ctrls;
	int rc = -EINVAL, i = 0;
	struct bm_img_vdev *idev = video_drvdata(file);
	union sclr_intr intr_mask;

	ext_ctrls = vc->controls;
	for (i = 0; i < vc->count; ++i) {
		switch (ext_ctrls[i].id) {
		case V4L2_CID_DV_VIP_IMG_INTR:
			service_isr = !service_isr;
			dprintk(VIP_DBG, "service_irs(%d)\n", service_isr);
			intr_mask = sclr_get_intr_mask();
			if (idev->img_type == SCL_IMG_D) {
				intr_mask.b.img_in_d_frame_end =
					(service_isr) ? 1 : 0;
			} else {
				intr_mask.b.img_in_v_frame_end =
					(service_isr) ? 1 : 0;
			}
			sclr_set_intr_mask(intr_mask);
			rc = 0;
		break;

		default:
		break;
		}
	}
	return rc;
}

int bm_img_g_selection(struct file *file, void *priv,
		struct v4l2_selection *sel)
{
	struct bm_img_vdev *idev = video_drvdata(file);

	dprintk(VIP_DBG, "+\n");
	if (sel->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
	return -EINVAL;

	sel->r.left = sel->r.top = 0;
	switch (sel->target) {
	case V4L2_SEL_TGT_CROP_BOUNDS:
	case V4L2_SEL_TGT_CROP_DEFAULT:
		sel->r.top = sel->r.left = 0;
		sel->r.width = idev->src_size.width;
		sel->r.height = idev->src_size.height;
	break;

	case V4L2_SEL_TGT_CROP:
		sel->r = idev->crop_rect;
	break;

	default:
	return -EINVAL;
	}

	dprintk(VIP_INFO, "target(%d) rect(%d %d %d %d)\n", sel->target,
			sel->r.left, sel->r.top, sel->r.width, sel->r.height);
	return 0;
}

int bm_img_s_selection(struct file *file, void *fh, struct v4l2_selection *sel)
{
	struct bm_img_vdev *idev = video_drvdata(file);
	//struct bm_vip_dev *bdev =
	//container_of(idev, struct bm_vip_dev, img_vdev[idev->dev_idx]);

	dprintk(VIP_DBG, "+\n");
	if (sel->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		return -EINVAL;

	switch (sel->target) {
	case V4L2_SEL_TGT_CROP:
		if (memcmp(&idev->crop_rect, &sel->r, sizeof(sel->r))) {
			struct sclr_img_cfg *cfg;

			if (bm_vip_job_is_queued(idev)) {
				dprintk(VIP_ERR, "job in queue\n");
				break;
			}

			cfg = sclr_img_get_cfg(idev->img_type);
			cfg->mem.start_x = sel->r.left;
			cfg->mem.start_y = sel->r.top;
			cfg->mem.width   = sel->r.width;
			cfg->mem.height  = sel->r.height;
			sclr_img_set_mem(idev->img_type, &cfg->mem);

			idev->crop_rect = sel->r;
		}
	break;

	default:
	return -EINVAL;
	}

	dprintk(VIP_INFO, "target(%d) rect(%d %d %d %d)\n", sel->target,
			sel->r.left, sel->r.top, sel->r.width, sel->r.height);
	return 0;
}

int bm_img_enum_fmt_vid_mplane(struct file *file, void  *priv,
		    struct v4l2_fmtdesc *f)
{
	dprintk(VIP_DBG, "+\n");
	return bm_vip_enum_fmt_vid(file, priv, f);
}

int bm_img_g_fmt_vid_cap_mplane(struct file *file, void *priv,
		    struct v4l2_format *f)
{
	struct bm_img_vdev *idev = video_drvdata(file);
	struct v4l2_pix_format_mplane *mp = &f->fmt.pix_mp;
	unsigned int p;

	dprintk(VIP_DBG, "+\n");
	WARN_ON(!idev);

	mp->width        = idev->crop_rect.width;
	mp->height       = idev->crop_rect.height;
	mp->field        = V4L2_FIELD_NONE;
	mp->pixelformat  = idev->fmt->fourcc;
	mp->colorspace   = idev->colorspace;
	mp->xfer_func    = V4L2_XFER_FUNC_DEFAULT;
	mp->ycbcr_enc    = V4L2_YCBCR_ENC_DEFAULT;
	mp->quantization = V4L2_QUANTIZATION_DEFAULT;
	mp->num_planes   = idev->fmt->buffers;
	for (p = 0; p < mp->num_planes; p++) {
		mp->plane_fmt[p].bytesperline = idev->bytesperline[p];
		mp->plane_fmt[p].sizeimage = idev->bytesperline[p] * mp->height;
	}

	return 0;
}

int bm_img_try_fmt_vid_cap_mplane(struct file *file, void *priv,
	    struct v4l2_format *f)
{
	return bm_vip_try_fmt_vid_mplane(f);
}

static void _fill_img_cfg(struct sclr_img_cfg *cfg,
		struct v4l2_pix_format_mplane *mp)
{
	const struct bm_vip_fmt *fmt;
	struct v4l2_plane_pix_format *pfmt = mp->plane_fmt;

	fmt = bm_vip_get_format(mp->pixelformat);

	cfg->fmt = fmt->fmt;
	if (mp->colorspace == V4L2_COLORSPACE_SRGB)
		cfg->csc = SCL_CSC_NONE;
	else if (mp->colorspace == V4L2_COLORSPACE_SMPTE170M)
		cfg->csc = SCL_CSC_601_LIMIT_YUV2RGB;
	else
		cfg->csc = SCL_CSC_709_LIMIT_YUV2RGB;

	cfg->mem.pitch_y = pfmt[0].bytesperline;
	cfg->mem.pitch_c = pfmt[1].bytesperline;
	cfg->mem.start_x = cfg->mem.start_y = 0;
	cfg->mem.width = mp->width;
	cfg->mem.height = mp->height;
}

int bm_img_s_fmt_vid_cap_mplane(struct file *file, void *priv,
	    struct v4l2_format *f)
{
	struct bm_img_vdev *idev = video_drvdata(file);
	struct v4l2_pix_format_mplane *mp = &f->fmt.pix_mp;
	struct v4l2_plane_pix_format *pfmt = mp->plane_fmt;
	const struct bm_vip_fmt *fmt;
	unsigned int p;
	struct sclr_img_cfg *cfg;
	int rc = bm_img_try_fmt_vid_cap_mplane(file, priv, f);

	dprintk(VIP_DBG, "+\n");
	if (rc < 0)
		return rc;
	if (bm_vip_job_is_queued(idev)) {
		dprintk(VIP_ERR, "job in queue\n");
		return -EINVAL;
	}

	fmt = bm_vip_get_format(mp->pixelformat);
	idev->fmt = fmt;
	idev->colorspace = mp->colorspace;
	for (p = 0; p < mp->num_planes; p++)
		idev->bytesperline[p] = pfmt[p].bytesperline;
	idev->src_size.width = mp->width;
	idev->src_size.height = mp->height;
	idev->crop_rect.left = idev->crop_rect.top = 0;
	idev->crop_rect.width = mp->width;
	idev->crop_rect.height = mp->height;
	dprintk(VIP_INFO, "src size(%d-%d) crop size(%d-%d)\n",
			idev->src_size.width, idev->src_size.height,
			idev->crop_rect.width, idev->crop_rect.height);

	cfg = sclr_img_get_cfg(idev->img_type);
	_fill_img_cfg(cfg, mp);
	sclr_img_set_cfg(idev->img_type, cfg);

	return rc;
}

int bm_img_streamon(struct file *file, void *fh, enum v4l2_buf_type i)
{
	struct bm_img_vdev *idev = video_drvdata(file);
	int rc = 0;

	rc = vb2_streamon(&idev->vb_q, i);
	if (!rc)
		bm_vip_try_schedule(idev);

	return rc;
}

int bm_img_streamoff(struct file *file, void *fh, enum v4l2_buf_type i)
{
	struct bm_img_vdev *idev = video_drvdata(file);
	int rc = 0;

	rc = vb2_streamoff(&idev->vb_q, i);
	if (rc)
		return rc;

	idev->job_flags = 0;
	return rc;
}

int bm_img_enum_input(struct file *file, void *priv, struct v4l2_input *inp)
{
	int rc = 0;
	return rc;
}

int bm_img_g_input(struct file *file, void *priv, unsigned int *i)
{
	struct bm_img_vdev *idev = video_drvdata(file);
	int rc = 0;
	*i = idev->input_type;
	return rc;
}

int bm_img_s_input(struct file *file, void *priv, unsigned int i)
{
	struct bm_img_vdev *idev = video_drvdata(file);
	int rc = 0;
	union sclr_input input;
	struct sclr_img_cfg *cfg;

	dprintk(VIP_DBG, "+\n");
	if (i >= BM_VIP_INPUT_MAX)
		return -EINVAL;

	idev->input_type = i;

	// update hw
	cfg = sclr_img_get_cfg(idev->img_type);
	if (idev->img_type == SCL_IMG_D) {
		if (i == BM_VIP_INPUT_DWA) {
			dprintk(VIP_ERR, "img_d doesn't have dwa input.\n");
			return -EINVAL;
		}

		if (i == BM_VIP_INPUT_ISP)
			input.d = SCL_INPUT_D_ISP;
		else
			input.d = SCL_INPUT_D_MEM;
	} else {
		if (i == BM_VIP_INPUT_ISP)
			input.v = SCL_INPUT_V_ISP;
		else if (i == BM_VIP_INPUT_DWA)
			input.v = SCL_INPUT_V_DWA;
		else
			input.v = SCL_INPUT_V_MEM;
	}
	sclr_ctrl_set_input(idev->img_type, input, cfg->fmt, cfg->csc);
	return rc;
}

static const struct v4l2_ioctl_ops bm_img_ioctl_ops = {
	.vidioc_querycap = bm_img_querycap,
	.vidioc_g_ctrl = bm_img_g_ctrl,
	.vidioc_s_ctrl = bm_img_s_ctrl,
	.vidioc_s_ext_ctrls = bm_img_s_ext_ctrls,

	.vidioc_g_selection     = bm_img_g_selection,
	.vidioc_s_selection     = bm_img_s_selection,
	.vidioc_enum_fmt_vid_cap_mplane = bm_img_enum_fmt_vid_mplane,
	.vidioc_g_fmt_vid_cap_mplane    = bm_img_g_fmt_vid_cap_mplane,
	.vidioc_try_fmt_vid_cap_mplane  = bm_img_try_fmt_vid_cap_mplane,
	.vidioc_s_fmt_vid_cap_mplane    = bm_img_s_fmt_vid_cap_mplane,

	.vidioc_reqbufs         = vb2_ioctl_reqbufs,
	//.vidioc_create_bufs     = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf     = vb2_ioctl_prepare_buf,
	.vidioc_querybuf        = vb2_ioctl_querybuf,
	.vidioc_qbuf            = vb2_ioctl_qbuf,
	.vidioc_dqbuf           = vb2_ioctl_dqbuf,
	//.vidioc_expbuf          = vb2_ioctl_expbuf,
	.vidioc_streamon        = bm_img_streamon,
	.vidioc_streamoff       = bm_img_streamoff,

	.vidioc_enum_input      = bm_img_enum_input,
	.vidioc_g_input         = bm_img_g_input,
	.vidioc_s_input         = bm_img_s_input,
	//.vidioc_subscribe_event     = img_subscribe_event,
	//.vidioc_unsubscribe_event   = v4l2_event_unsubscribe,
};

/*************************************************************************
 *	General functions
 *************************************************************************/
int img_create_instance(struct platform_device *pdev)
{
	int rc = 0;
	struct bm_vip_dev *bdev;
	struct video_device *vfd;
	struct bm_img_vdev *idev;
	u8 i = 0;

	bdev = dev_get_drvdata(&pdev->dev);
	if (!bdev) {
		dprintk(VIP_ERR, "invalid data\n");
		return -EINVAL;
	}

	for (i = 0; i < BM_VIP_IMG_MAX; ++i) {
		idev = &bdev->img_vdev[i];
		idev->dev_idx = i;
		idev->img_type = (i == 0) ? SCL_IMG_D : SCL_IMG_V;
		idev->sc_bounding =
			(i == 0) ? BM_VIP_IMG_2_SC_D : BM_VIP_IMG_2_SC_V;
		idev->fmt = bm_vip_get_format(V4L2_PIX_FMT_RGBM);
		idev->vid_caps = V4L2_CAP_VIDEO_CAPTURE_MPLANE |
			V4L2_CAP_STREAMING;
		spin_lock_init(&idev->rdy_lock);
		memset(&idev->src_size, 0, sizeof(idev->src_size));
		memset(&idev->crop_rect, 0, sizeof(idev->crop_rect));

		vfd = &(idev->vdev);
		snprintf(vfd->name, sizeof(vfd->name), "vip-img%d", i);
		vfd->fops = &bm_img_fops;
		vfd->ioctl_ops = &bm_img_ioctl_ops;
		vfd->vfl_dir = VFL_DIR_RX;
		vfd->vfl_type = VFL_TYPE_GRABBER;
		vfd->minor = -1;
		vfd->device_caps = idev->vid_caps;
		vfd->release = video_device_release_empty;
		vfd->v4l2_dev = &bdev->v4l2_dev;
		vfd->queue = &idev->vb_q;

		rc = video_register_device(vfd, VFL_TYPE_GRABBER,
				IMG_DEVICE_IDX + i);
		if (rc) {
			dprintk(VIP_ERR, "Failed to register img-dev%d\n", i);
			continue;
		}

		vfd->lock = &bdev->mutex;
		video_set_drvdata(vfd, idev);
		atomic_set(&idev->opened, 0);
		idev->job_flags = 0;

		dprintk(VIP_INFO, "img registered as %s\n",
				video_device_node_name(vfd));
	}

	return rc;
}

void img_irq_handler(union sclr_intr intr_status, struct bm_vip_dev *bdev)
{
	u8 img_idx = 0, sc_idx = 0;
	struct bm_img_vdev *idev = NULL;

	for (img_idx = 0; img_idx < BM_VIP_IMG_MAX; ++img_idx) {
		struct bm_vip_buffer *img_b = NULL;
		struct bm_vip_buffer *sc_b[BM_VIP_SC_MAX] = {
						NULL, NULL, NULL, NULL};
		struct bm_base_vdev *base = NULL;

		if (((img_idx == BM_VIP_IMG_D) &&
				(intr_status.b.img_in_d_frame_end == 0)) ||
		    ((img_idx == BM_VIP_IMG_V) &&
				(intr_status.b.img_in_v_frame_end == 0)))
			continue;

		idev = &bdev->img_vdev[img_idx];
		img_b = bm_vip_buf_remove((struct bm_base_vdev *)idev);
		if (!img_b) {
			dprintk(VIP_ERR, "no img%d buf, intr-status(%#x)\n",
					img_idx, intr_status.raw);
			continue;
		}
		if (idev->sc_bounding & BM_VIP_IMG_2_SC_D) {
			base = (struct bm_base_vdev *)
				&bdev->sc_vdev[BM_VIP_SC_D];
			sc_b[BM_VIP_SC_D] = bm_vip_buf_remove(base);
		}
		if (idev->sc_bounding & BM_VIP_IMG_2_SC_V) {
			for (sc_idx = BM_VIP_SC_V0; sc_idx <= BM_VIP_SC_V2;
					++sc_idx) {
				base = (struct bm_base_vdev *)
					&bdev->sc_vdev[sc_idx];
				sc_b[sc_idx] = bm_vip_buf_remove(base);
			}
		}

		// TODO: timestamp should use isp's
		// update vb2's info
		img_b->vb.vb2_buf.timestamp = ktime_get_ns();
		img_b->vb.sequence = ++idev->seq_count;
		for (sc_idx = BM_VIP_SC_D; sc_idx < BM_VIP_SC_MAX; ++sc_idx) {
			if (!sc_b[sc_idx])
				continue;
			sc_b[sc_idx]->vb.vb2_buf.timestamp =
				img_b->vb.vb2_buf.timestamp;
			sc_b[sc_idx]->vb.sequence = img_b->vb.sequence;

			vb2_buffer_done(&sc_b[sc_idx]->vb.vb2_buf,
					VB2_BUF_STATE_DONE);
		}

		vb2_buffer_done(&img_b->vb.vb2_buf, VB2_BUF_STATE_DONE);

		// update job-flag and see if there are other jobs
		bm_vip_job_finish(idev);
	}
}
