#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/videodev2.h>
#include <linux/platform_device.h>
#include <media/videobuf2-vmalloc.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-event.h>
#include <media/v4l2-mem2mem.h>

#include "vip/vip_common.h"
#include "vip/dwa.h"

#include "bm_debug.h"
#include "bm_vip_core.h"

static struct dwa_cfg cfg;
//TODO: mmap for user to generate mesh, get from dts or ion??
static u64 mesh_id_addr = 0x12F000000;

static struct bm_dwa_data *_dwa_get_data(struct bm_dwa_vdev *wdev,
		enum v4l2_buf_type type)
{
	if (V4L2_TYPE_IS_OUTPUT(type))
		return &wdev->out_data;
	return &wdev->cap_data;
}

static void bm_dwa_device_run(void *priv)
{
	struct bm_dwa_vdev *wdev = priv;
	struct vb2_v4l2_buffer *src_buf, *dst_buf;
	struct bm_dwa_data *src_fmt, *dst_fmt;
	u8 i = 0;

	src_buf = v4l2_m2m_next_src_buf(wdev->fh.m2m_ctx);
	dst_buf = v4l2_m2m_next_dst_buf(wdev->fh.m2m_ctx);

	src_fmt = _dwa_get_data(wdev, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	dst_fmt = _dwa_get_data(wdev, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);

	switch (wdev->fmt->fmt) {
	case SCL_FMT_YUV420:
		cfg.pix_fmt = YUV420p;
	break;
	case SCL_FMT_Y_ONLY:
		cfg.pix_fmt = YUV400;
	break;
	case SCL_FMT_RGB_PLANAR:
	default:
		cfg.pix_fmt = RGB888p;
	break;
	};

	cfg.src_width  = src_fmt->w;
	cfg.src_height = src_fmt->h;
	cfg.dst_width  = dst_fmt->w;
	cfg.dst_height = dst_fmt->h;

	for (i = 0; i < 3; ++i) {
		u64 addr = src_buf->vb2_buf.planes[i].m.userptr;

		cfg.src_buf[i].addrl    = addr;
		cfg.src_buf[i].addrh    = addr >> 32;
		cfg.src_buf[i].pitch    = src_fmt->bytesperline[i];
		cfg.src_buf[i].offset_x = cfg.src_buf[i].offset_y = 0;

		addr = dst_buf->vb2_buf.planes[i].m.userptr;
		cfg.dst_buf[i].addrl    = addr;
		cfg.dst_buf[i].addrh    = addr >> 32;
		cfg.dst_buf[i].pitch    = dst_fmt->bytesperline[i];
		cfg.dst_buf[i].offset_x = cfg.src_buf[i].offset_y = 0;
	}

	dwa_engine(&cfg);
}

static int bm_dwa_job_ready(void *priv)
{
	// TODO: needed??
	return dwa_is_busy() ? 0 : 1;
}

static void bm_dwa_job_abort(void *priv)
{
	// TODO: needed? m2m required
}

static const struct v4l2_m2m_ops bm_dwa_m2m_ops = {
	.device_run = bm_dwa_device_run,
	.job_ready  = bm_dwa_job_ready,
	.job_abort  = bm_dwa_job_abort,
};

/*************************************************************************
 *	VB2_OPS definition
 *************************************************************************/
/**
 * call before VIDIOC_REQBUFS to setup buf-queue.
 * nbuffers: number of buffer requested
 * nplanes:  number of plane each buffer
 * sizes:    size of each plane(bytes)
 */
static int bm_dwa_queue_setup(struct vb2_queue *vq,
	       unsigned int *nbuffers, unsigned int *nplanes,
	       unsigned int sizes[], struct device *alloc_devs[])
{
	struct bm_dwa_vdev *wdev = vb2_get_drv_priv(vq);
	struct bm_dwa_data *data = NULL;
	unsigned int planes, p;

	dprintk(VIP_VB2, "+\n");

	data = _dwa_get_data(wdev, vq->type);
	if (!data)
		return -EINVAL;

	planes = wdev->fmt->buffers;

	for (p = 0; p < planes; ++p)
		sizes[p] = data->sizeimage[p];

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
static void bm_dwa_buf_queue(struct vb2_buffer *vb)
{
	struct bm_dwa_vdev *wdev = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);

	dprintk(VIP_VB2, "+\n");

	v4l2_m2m_buf_queue(wdev->fh.m2m_ctx, vbuf);
}

static struct vb2_v4l2_buffer *_dwa_buf_remove(struct bm_dwa_vdev *ctx,
					       enum v4l2_buf_type type)
{
	if (V4L2_TYPE_IS_OUTPUT(type))
		return v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx);
	else
		return v4l2_m2m_dst_buf_remove(ctx->fh.m2m_ctx);
}

static int bm_dwa_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct bm_dwa_vdev *wdev = vb2_get_drv_priv(vq);
	int rc = 0;
	struct vb2_v4l2_buffer *vb;

	dprintk(VIP_VB2, "+\n");

	while ((vb = _dwa_buf_remove(wdev, vq->type)))
		v4l2_m2m_buf_done(vb, VB2_BUF_STATE_ERROR);

	return rc;
}

/* abort streaming and wait for last buffer */
static void bm_dwa_stop_streaming(struct vb2_queue *vq)
{
	struct bm_dwa_vdev *wdev = vb2_get_drv_priv(vq);
	struct vb2_v4l2_buffer *vb;

	dprintk(VIP_VB2, "+\n");

	while ((vb = _dwa_buf_remove(wdev, vq->type)))
	v4l2_m2m_buf_done(vb, VB2_BUF_STATE_ERROR);
}

const struct vb2_ops bm_dwa_qops = {
	.queue_setup        = bm_dwa_queue_setup,
	.buf_queue          = bm_dwa_buf_queue,
	.start_streaming    = bm_dwa_start_streaming,
	.stop_streaming     = bm_dwa_stop_streaming,
	.wait_prepare       = vb2_ops_wait_prepare,
	.wait_finish        = vb2_ops_wait_finish,
};

/*************************************************************************
 *	VB2-MEM-OPS definition
 *************************************************************************/
static void *_get_userptr(struct device *dev, unsigned long vaddr,
	unsigned long size, enum dma_data_direction dma_dir)
{
	return (void *)0xdeadbeef;
}

static void _put_userptr(void *buf_priv)
{
}

static const struct vb2_mem_ops bm_dwa_vb2_mem_ops = {
	.get_userptr = _get_userptr,
	.put_userptr = _put_userptr,
};

/*************************************************************************
 *	FOPS definition
 *************************************************************************/
static int bm_dwa_queue_init(void *priv, struct vb2_queue *src_vq,
			       struct vb2_queue *dst_vq)
{
	struct bm_dwa_vdev *wdev = priv;
	struct bm_vip_dev *bdev = NULL;
	int rc = 0;

	bdev = container_of(wdev, struct bm_vip_dev, dwa_vdev);

	src_vq->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	src_vq->io_modes = VB2_USERPTR;
	src_vq->buf_struct_size = sizeof(struct bm_vip_buffer);
	src_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	src_vq->min_buffers_needed = 0;

	src_vq->drv_priv = priv;
	src_vq->dev = bdev->v4l2_dev.dev;
	src_vq->ops = &bm_dwa_qops;
	src_vq->mem_ops = &bm_dwa_vb2_mem_ops;
	//q->lock = &wdev->lock;

	rc = vb2_queue_init(src_vq);
	if (rc) {
		dprintk(VIP_ERR, "src_vq errcode(%d)\n", rc);
		return rc;
	}

	dst_vq->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	dst_vq->io_modes = VB2_USERPTR;
	dst_vq->buf_struct_size = sizeof(struct bm_vip_buffer);
	dst_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	dst_vq->min_buffers_needed = 0;

	dst_vq->drv_priv = priv;
	dst_vq->dev = bdev->v4l2_dev.dev;
	dst_vq->ops = &bm_dwa_qops;
	dst_vq->mem_ops = &bm_dwa_vb2_mem_ops;
	//q->lock = &wdev->lock;

	rc = vb2_queue_init(dst_vq);
	if (rc)
		dprintk(VIP_ERR, "dst_vq errcode(%d)\n", rc);

	return rc;
}

static int bm_dwa_open(struct file *file)
{
	int rc = 0;
	struct bm_dwa_vdev *wdev = video_drvdata(file);
	struct bm_vip_dev *bdev = NULL;

	WARN_ON(!wdev);

	/* !!! only ONE open is allowed !!! */
	if (atomic_cmpxchg(&wdev->opened, 0, 1)) {
		dprintk(VIP_ERR, " device has been occupied. Reject %s\n",
				current->comm);
		return -EBUSY;
	}

	bdev = container_of(wdev, struct bm_vip_dev, dwa_vdev);

	v4l2_fh_init(&wdev->fh, &wdev->vdev);
	file->private_data = &wdev->fh;
	v4l2_fh_add(&wdev->fh);

	// vb2_queue init
	wdev->fh.m2m_ctx = v4l2_m2m_ctx_init(wdev->m2m_dev, wdev,
			bm_dwa_queue_init);

	return rc;
}

static int bm_dwa_release(struct file *file)
{
	struct bm_dwa_vdev *wdev = video_drvdata(file);

	WARN_ON(!wdev);

	atomic_set(&wdev->opened, 0);

	dprintk(VIP_INFO, "-\n");
	return 0;
}

static int bm_dwa_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long len;

	len = vma->vm_end - vma->vm_start;
	if (remap_pfn_range(vma, vma->vm_start, mesh_id_addr >> PAGE_SHIFT,
			len, vma->vm_page_prot))
		return -EAGAIN;

	return 0;
}

static struct v4l2_file_operations bm_dwa_fops = {
	.owner          = THIS_MODULE,
	.open           = bm_dwa_open,
	.release        = bm_dwa_release,
	.poll           = v4l2_m2m_fop_poll,
	.unlocked_ioctl = video_ioctl2,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = video_ioctl2,
#endif
	.mmap           = bm_dwa_mmap,
};
/*************************************************************************
 *	IOCTL definition
 *************************************************************************/
static int bm_dwa_querycap(struct file *file, void *priv,
		    struct v4l2_capability *cap)
{
	struct bm_dwa_vdev *wdev = video_drvdata(file);
	struct bm_vip_dev *bdev =
		container_of(wdev, struct bm_vip_dev, dwa_vdev);

	strlcpy(cap->driver, BM_VIP_DRV_NAME, sizeof(cap->driver));
	strlcpy(cap->card, BM_VIP_DVC_NAME, sizeof(cap->card));
	snprintf(cap->bus_info, sizeof(cap->bus_info),
	    "platform:%s", bdev->v4l2_dev.name);

	cap->capabilities = wdev->vid_caps | V4L2_CAP_DEVICE_CAPS;
	return 0;
}

static int bm_dwa_g_ctrl(struct file *file, void *priv, struct v4l2_control *vc)
{
	int rc = -EINVAL;
	return rc;
}

static int bm_dwa_s_ctrl(struct file *file, void *priv, struct v4l2_control *vc)
{
	int rc = -EINVAL;
	return rc;
}

static int bm_dwa_s_ext_ctrls(struct file *file, void *priv,
	struct v4l2_ext_controls *vc)
{
	struct v4l2_ext_control *ext_ctrls;
	int rc = -EINVAL, i = 0;
	//struct bm_dwa_vdev *wdev = video_drvdata(file);

	ext_ctrls = vc->controls;
	for (i = 0; i < vc->count; ++i) {
		switch (ext_ctrls[i].id) {
		default:
		break;
		}
	}
	return rc;
}

int bm_dwa_enum_fmt_vid_mplane(struct file *file, void  *priv,
		    struct v4l2_fmtdesc *f)
{
	dprintk(VIP_DBG, "+\n");
	return bm_vip_enum_fmt_vid(file, priv, f);
}

int bm_dwa_g_fmt_vid_mplane(struct file *file, void *priv,
		    struct v4l2_format *f)
{
	struct bm_dwa_vdev *wdev = video_drvdata(file);
	struct v4l2_pix_format_mplane *mp = &f->fmt.pix_mp;
	struct bm_dwa_data *data = NULL;
	unsigned int p;

	dprintk(VIP_DBG, "+\n");
	WARN_ON(!wdev);

	data = _dwa_get_data(wdev, f->type);
	if (!data)
		return -EINVAL;

	mp->width        = data->w;
	mp->height       = data->h;
	mp->field        = V4L2_FIELD_NONE;
	mp->pixelformat  = wdev->fmt->fourcc;
	mp->colorspace   = wdev->colorspace;
	mp->xfer_func    = V4L2_XFER_FUNC_DEFAULT;
	mp->ycbcr_enc    = V4L2_YCBCR_ENC_DEFAULT;
	mp->quantization = V4L2_QUANTIZATION_DEFAULT;
	mp->num_planes   = wdev->fmt->buffers;
	for (p = 0; p < mp->num_planes; p++) {
		mp->plane_fmt[p].bytesperline = data->bytesperline[p];
		mp->plane_fmt[p].sizeimage = data->sizeimage[p];
	}

	return 0;
}

int bm_dwa_try_fmt_vid_mplane(struct file *file, void *priv,
	    struct v4l2_format *f)
{
	return bm_vip_try_fmt_vid_mplane(f);
}

int bm_dwa_s_fmt_vid_mplane(struct file *file, void *priv,
	    struct v4l2_format *f)
{
	struct bm_dwa_vdev *wdev = video_drvdata(file);
	struct v4l2_pix_format_mplane *mp = &f->fmt.pix_mp;
	struct v4l2_plane_pix_format *pfmt = mp->plane_fmt;
	const struct bm_vip_fmt *fmt;
	unsigned int p;
	int rc = bm_dwa_try_fmt_vid_mplane(file, priv, f);
	struct bm_dwa_data *data = NULL;

	dprintk(VIP_DBG, "+\n");
	if (rc < 0)
		return rc;

	data = _dwa_get_data(wdev, f->type);
	if (!data)
		return -EINVAL;

	fmt = bm_vip_get_format(mp->pixelformat);
	wdev->fmt = fmt;
	wdev->colorspace = mp->colorspace;
	for (p = 0; p < mp->num_planes; p++) {
		data->bytesperline[p] = pfmt[p].bytesperline;
		data->sizeimage[p] = pfmt[p].sizeimage;
	}

	return rc;
}

static const struct v4l2_ioctl_ops bm_dwa_ioctl_ops = {
	.vidioc_querycap = bm_dwa_querycap,
	.vidioc_g_ctrl = bm_dwa_g_ctrl,
	.vidioc_s_ctrl = bm_dwa_s_ctrl,
	.vidioc_s_ext_ctrls = bm_dwa_s_ext_ctrls,

	.vidioc_enum_fmt_vid_cap_mplane = bm_dwa_enum_fmt_vid_mplane,
	.vidioc_enum_fmt_vid_out_mplane = bm_dwa_enum_fmt_vid_mplane,
	.vidioc_g_fmt_vid_cap_mplane    = bm_dwa_g_fmt_vid_mplane,
	.vidioc_g_fmt_vid_out_mplane    = bm_dwa_g_fmt_vid_mplane,
	.vidioc_try_fmt_vid_cap_mplane  = bm_dwa_try_fmt_vid_mplane,
	.vidioc_try_fmt_vid_out_mplane  = bm_dwa_try_fmt_vid_mplane,
	.vidioc_s_fmt_vid_cap_mplane    = bm_dwa_s_fmt_vid_mplane,
	.vidioc_s_fmt_vid_out_mplane    = bm_dwa_s_fmt_vid_mplane,

	.vidioc_reqbufs         = v4l2_m2m_ioctl_reqbufs,
	.vidioc_create_bufs     = v4l2_m2m_ioctl_create_bufs,
	.vidioc_prepare_buf     = v4l2_m2m_ioctl_prepare_buf,
	.vidioc_querybuf        = v4l2_m2m_ioctl_querybuf,
	.vidioc_qbuf            = v4l2_m2m_ioctl_qbuf,
	.vidioc_dqbuf           = v4l2_m2m_ioctl_dqbuf,
	.vidioc_expbuf          = v4l2_m2m_ioctl_expbuf,
	.vidioc_streamon        = v4l2_m2m_ioctl_streamon,
	.vidioc_streamoff       = v4l2_m2m_ioctl_streamoff,

};

/*************************************************************************
 *	General functions
 *************************************************************************/
int dwa_create_instance(struct platform_device *pdev)
{
	int rc = 0;
	struct bm_vip_dev *bdev;
	struct video_device *vfd;
	struct bm_dwa_vdev *wdev;
	u8 i = 0;

	bdev = dev_get_drvdata(&pdev->dev);
	if (!bdev) {
		dprintk(VIP_ERR, "invalid data\n");
		return -EINVAL;
	}

	wdev = &bdev->dwa_vdev;

	wdev->m2m_dev = v4l2_m2m_init(&bm_dwa_m2m_ops);
	if (IS_ERR(wdev->m2m_dev)) {
		dprintk(VIP_ERR, "Failed to init mem2mem device\n");
		rc = PTR_ERR(wdev->m2m_dev);
		goto err_m2m_init;
	}

	wdev->fmt = bm_vip_get_format(V4L2_PIX_FMT_YUV420M);
	wdev->vid_caps = V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING;

	vfd = &(wdev->vdev);
	snprintf(vfd->name, sizeof(vfd->name), "vip-dwa");
	vfd->fops = &bm_dwa_fops;
	vfd->ioctl_ops = &bm_dwa_ioctl_ops;
	vfd->vfl_dir = VFL_DIR_M2M;
	vfd->vfl_type = VFL_TYPE_GRABBER;
	vfd->minor = -1;
	vfd->device_caps = wdev->vid_caps;
	vfd->release = video_device_release_empty;
	vfd->v4l2_dev = &bdev->v4l2_dev;
	vfd->lock = &wdev->mutex;

	rc = video_register_device(vfd, VFL_TYPE_GRABBER, DWA_DEVICE_IDX);
	if (rc) {
		dprintk(VIP_ERR, "Failed to register img-device%d\n", i);
		goto err_dec_vdev_register;
	}

	video_set_drvdata(vfd, wdev);
	atomic_set(&wdev->opened, 0);

	dprintk(VIP_INFO, "img registered as %s\n",
			video_device_node_name(vfd));

	return 0;

err_dec_vdev_register:
	v4l2_m2m_release(wdev->m2m_dev);
err_m2m_init:
	return rc;
}

void dwa_irq_handler(u8 intr_status, struct bm_vip_dev *bdev)
{
	struct bm_dwa_vdev *wdev = NULL;
	struct vb2_v4l2_buffer *src_buf, *dst_buf;

	wdev = &bdev->dwa_vdev;

	src_buf = v4l2_m2m_src_buf_remove(wdev->fh.m2m_ctx);
	dst_buf = v4l2_m2m_src_buf_remove(wdev->fh.m2m_ctx);

	v4l2_m2m_buf_done(src_buf, VB2_BUF_STATE_DONE);
	v4l2_m2m_buf_done(dst_buf, VB2_BUF_STATE_DONE);
	v4l2_m2m_job_finish(wdev->m2m_dev, wdev->fh.m2m_ctx);
}
