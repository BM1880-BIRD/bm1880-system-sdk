#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/videodev2.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <media/videobuf2-vmalloc.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-event.h>

#include "vip/vip_common.h"
#include "vip/inc/isp_reg.h"
#include "vip/isp_drv.h"
#include "vip/reg.h"

#include "bm_debug.h"
#include "bm_vip_core.h"
#include "bm_vip_isp.h"

struct isp_buffer {
	uint64_t          addr;
	struct list_head  list;
};

struct isp_queue {
	struct list_head rdy_queue;
	uint8_t num_rdy;
} pre_queue, post_queue;

uint64_t mem_base = 0x130000000;
uint64_t mem_offset;

void isp_buf_queue(struct isp_queue *q, struct isp_buffer *b)
{
	list_add_tail(&b->list, &q->rdy_queue);
	++q->num_rdy;
}

struct isp_buffer *isp_next_buf(struct isp_queue *q)
{
	struct isp_buffer *b = NULL;

	if (!list_empty(&q->rdy_queue))
		b = list_first_entry(&q->rdy_queue, struct isp_buffer, list);

	return b;
}

struct isp_buffer *isp_buf_remove(struct isp_queue *q)
{
	struct isp_buffer *b = NULL;

	if (!list_empty(&q->rdy_queue)) {
		b = list_first_entry(&q->rdy_queue, struct isp_buffer, list);
		list_del_init(&b->list);
		--q->num_rdy;
	}

	return b;
}

void isp_init_param(struct isp_ctx *ctx)
{
	memset(ctx, 0, sizeof(*ctx));

	ctx->phys_regs          = isp_get_phys_reg_bases();
	ctx->rgb_color_mode	= ISP_BAYER_TYPE_GB;
	ctx->csibdg_patgen_en   = false;
	ctx->sensor_bitdepth    = 12;

	ctx->cam_id		= 0;
	ctx->is_yuv_sensor      = false;
	ctx->is_hdr_on          = false;
	ctx->is_offline_preraw  = false;
	ctx->is_offline_postraw = true;
	ctx->is_offline_scaler  = true;
	ctx->is_preraw0_done    = true;
	ctx->is_preraw1_done    = true;
	ctx->is_postraw_done    = true;

	INIT_LIST_HEAD(&pre_queue.rdy_queue);
	INIT_LIST_HEAD(&post_queue.rdy_queue);
	pre_queue.num_rdy       = 0;
	post_queue.num_rdy      = 0;
}

void isp_dma_setup(struct isp_ctx *ictx)
{
	uint64_t bufaddr = 0;

	mem_offset = 0;

	// rgbmap
	bufaddr = mem_base + mem_offset;
	mem_offset += ispblk_dma_config(ictx, ISP_BLK_ID_DMA15, bufaddr);

	// lmap
	bufaddr = mem_base + mem_offset;
	mem_offset += ispblk_dma_config(ictx, ISP_BLK_ID_DMA9, bufaddr);
	ispblk_dma_config(ictx, ISP_BLK_ID_DMA12, bufaddr);

	// for preraw read
#if 0
	if (ictx->is_offline_preraw) {
		bufaddr = membase + mem_offset;
		mem_offset += ispblk_dma_config(ictx, ISP_BLK_ID_DMA8, bufaddr);

		ispblk_dma_enable(ictx, ISP_BLK_ID_DMA8, 1);
	} else {
		ispblk_dma_enable(ictx, ISP_BLK_ID_DMA8, 0);
	}
#endif

	if (ictx->is_offline_postraw) {
		struct isp_buffer *b;
		uint32_t bufsize = 0;

		bufaddr = mem_base + mem_offset;
		bufsize = ispblk_dma_config(ictx, ISP_BLK_ID_DMA0, bufaddr);
		ispblk_dma_config(ictx, ISP_BLK_ID_DMA6, bufaddr);
		mem_offset += bufsize;

		// TODO:  temporary triple buf
		// 1st buf
		b = kmalloc(sizeof(*b), GFP_KERNEL);
		b->addr = bufaddr;
		isp_buf_queue(&pre_queue, b);
		// 2nd buf
		bufaddr = mem_base + mem_offset;
		b = kmalloc(sizeof(*b), GFP_KERNEL);
		b->addr = bufaddr;
		isp_buf_queue(&pre_queue, b);
		mem_offset += bufsize;
		// 3rd buf
		bufaddr = mem_base + mem_offset;
		b = kmalloc(sizeof(*b), GFP_KERNEL);
		b->addr = bufaddr;
		isp_buf_queue(&pre_queue, b);
		mem_offset += bufsize;

		ispblk_dma_enable(ictx, ISP_BLK_ID_DMA0, 1);
		ispblk_dma_enable(ictx, ISP_BLK_ID_DMA6, 1);
	} else {
		ispblk_dma_enable(ictx, ISP_BLK_ID_DMA0, 0);
		ispblk_dma_enable(ictx, ISP_BLK_ID_DMA6, 0);
	}

	if (ictx->is_hdr_on) {
		bufaddr = mem_base + mem_offset;
		mem_offset += ispblk_dma_config(ictx, ISP_BLK_ID_DMA1, bufaddr);
		ispblk_dma_config(ictx, ISP_BLK_ID_DMA7, bufaddr);

		ispblk_dma_enable(ictx, ISP_BLK_ID_DMA1, 1);
		ispblk_dma_enable(ictx, ISP_BLK_ID_DMA7, 1);
	} else {
		ispblk_dma_enable(ictx, ISP_BLK_ID_DMA1, 0);
		ispblk_dma_enable(ictx, ISP_BLK_ID_DMA7, 0);
	}

	if (ictx->is_offline_scaler) {
		ispblk_dma_config(ictx, ISP_BLK_ID_DMA3, 0);
		ispblk_dma_config(ictx, ISP_BLK_ID_DMA4, 0);
		ispblk_dma_config(ictx, ISP_BLK_ID_DMA5, 0);
		ispblk_dma_enable(ictx, ISP_BLK_ID_DMA3, 1);
		ispblk_dma_enable(ictx, ISP_BLK_ID_DMA4, 1);
		ispblk_dma_enable(ictx, ISP_BLK_ID_DMA5, 1);
	} else {
		ispblk_dma_enable(ictx, ISP_BLK_ID_DMA3, 0);
		ispblk_dma_enable(ictx, ISP_BLK_ID_DMA4, 0);
		ispblk_dma_enable(ictx, ISP_BLK_ID_DMA5, 0);
	}
}

void isp_ctrl_setup(struct isp_ctx *ictx)
{
	union REG_ISP_DPC_2 dpc_reg2;

	isp_init(ictx);

	// preraw
	ispblk_preraw_config(ictx);
	ispblk_csibdg_config(ictx);

	ispblk_rgbmap_config(ictx, ISP_BLK_ID_RGBMAP0);

	ispblk_lmap_config(ictx, ISP_BLK_ID_LMP0);

	// raw_top
	dpc_reg2.bits.DPC_ENABLE = 1;
	dpc_reg2.bits.GE_ENABLE = 1;
	dpc_reg2.bits.DPC_DYNAMICBPC_ENABLE = 1;
	dpc_reg2.bits.DPC_STATICBPC_ENABLE = 1;
	dpc_reg2.bits.DPC_CLUSTER_SIZE = 2;
	ispblk_dpc_config(ictx, dpc_reg2);

	ispblk_rawtop_config(ictx);
	ispblk_blc_enable(ictx, ISP_BLK_ID_BLC0, true, true);
	ispblk_blc_enable(ictx, ISP_BLK_ID_BLC2, true, true);
	if (ictx->is_hdr_on)
		ispblk_fusion_config(ictx, true, false, true, ISP_FS_OUT_FS);
	else
		ispblk_fusion_config(ictx, false, true, false, ISP_FS_OUT_FS);

//    ispblk_ltm_b_lut(ictx, 0, ltm_b_lut);
//    ispblk_ltm_d_lut(ictx, 0, ltm_d_lut);
//    ispblk_ltm_g_lut(ictx, 0, ltm_g_lut);
	ispblk_ltm_config(ictx, false, true, true, false, false, 0, 0);
	ispblk_wbg_config(ictx, ISP_BLK_ID_WBG0, 0x677, 0x400, 0x4B0);
	ispblk_wbg_enable(ictx, ISP_BLK_ID_WBG0, true, false);
	ispblk_wbg_config(ictx, ISP_BLK_ID_WBG2, 0x677, 0x400, 0x4B0);
	ispblk_wbg_enable(ictx, ISP_BLK_ID_WBG2, true, false);
	ispblk_bnr_config(ictx, ISP_BNR_OUT_B_OUT, false, 0, 0);

	// rgb_top
	if (!ictx->is_yuv_sensor) {
		ispblk_rgb_config(ictx);
		ispblk_cfa_config(ictx);
		ispblk_rgbee_config(ictx, true, 358, 310);
//	ispblk_gamma_config(ictx, 0, gamma_data);
		ispblk_gamma_enable(ictx, false, 0);
		ispblk_dhz_config(ictx, true, 30, 200);
//	ispblk_hsv_config(ictx, 0, 0, hsv_sgain_data);
//	ispblk_hsv_config(ictx, 0, 1, hsv_vgain_data);
//	ispblk_hsv_config(ictx, 0, 2, hsv_htune_data);
//	ispblk_hsv_config(ictx, 0, 3, hsv_stune_data);
		ispblk_hsv_enable(ictx, false, 0, true, true, true, true);
		ispblk_rgbdither_config(ictx, true, true, true, true);
		ispblk_csc_config(ictx);
	}

	// yuv_top
	ispblk_yuvtop_config(ictx);
	ispblk_yuvdither_config(ictx, 0, true, true, true, true);
	ispblk_yuvdither_config(ictx, 1, true, true, true, true);
	ispblk_444_422_config(ictx);
	ispblk_ynr_config(ictx, ISP_YNR_OUT_Y_OUT, 16, 0);
	ispblk_cnr_config(ictx, true, true, 32);
	ispblk_ee_config(ictx, false, false);
//    ispblk_ycur_config(ictx, 0, ycur_data);
	ispblk_ycur_enable(ictx, false, 0);

	ispblk_isptop_config(ictx);

}

static void _pre_hw_enque(struct bm_isp_vdev *vdev)
{
	struct isp_ctx *ctx = &vdev->ctx;
	uint64_t preraw0 = ctx->phys_regs[ISP_BLK_ID_PRERAW0];
	struct isp_buffer *b = NULL;

	if (!vdev || !ctx->is_offline_postraw || !ctx->is_preraw0_done) {
		dprintk(VIP_DBG, "postraw_offline(%d) pre_done(%d)\n",
			ctx->is_offline_postraw, ctx->is_preraw0_done);
		return;
	}
	// TODO:
	// Did it after frame_done if...
	// [offline]
	// 1. check buf available
	// [online]
	// 1. check later module buf available
	b = isp_next_buf(&pre_queue);
	if (!b) {
		dprintk(VIP_DBG, "preraw no buf\n");
		return;
	}
	ispblk_dma_setaddr(ctx, ISP_BLK_ID_DMA0, b->addr);
	dprintk(VIP_DBG, "update pre-w-buf: 0x%llx\n", b->addr);

	ctx->is_preraw0_done = false;
	// open frame vld
	ISP_WO_BITS(preraw0, REG_PRE_RAW_T, FRAME_VLD, PRE_RAW_FRAME_VLD, 1);
}

static void _post_hw_enque(struct bm_isp_vdev *vdev)
{
	struct vb2_buffer *vb2_buf;
	struct bm_vip_buffer *b = NULL;
	struct isp_ctx *ctx = &vdev->ctx;
	struct isp_buffer *ispb = NULL;

	if (!vdev || !ctx->is_postraw_done) {
		dprintk(VIP_DBG, " post_done(%d)\n", ctx->is_postraw_done);
		return;
	}

	// check if portraw's
	b = bm_vip_next_buf((struct bm_base_vdev *)vdev);
	if (!b) {
		dprintk(VIP_DBG, "postraw no output buf\n");
		return;
	}
	vb2_buf = &b->vb.vb2_buf;

	if (ctx->is_offline_postraw) {
		ispb = isp_next_buf(&post_queue);
		if (!ispb) {
			dprintk(VIP_DBG, "postraw no input buf\n");
			return;
		}

		dprintk(VIP_DBG, "update post-r-buf: 0x%llx\n", ispb->addr);
		ispblk_dma_setaddr(ctx, ISP_BLK_ID_DMA6, ispb->addr);
	}

	dprintk(VIP_DBG, "update isp-buf: 0x%lx-0x%lx-0x%lx\n",
		vb2_buf->planes[0].m.userptr, vb2_buf->planes[1].m.userptr,
		vb2_buf->planes[2].m.userptr);

	ispblk_dma_setaddr(ctx, ISP_BLK_ID_DMA3, vb2_buf->planes[0].m.userptr);
	ispblk_dma_setaddr(ctx, ISP_BLK_ID_DMA4, vb2_buf->planes[1].m.userptr);
	ispblk_dma_setaddr(ctx, ISP_BLK_ID_DMA5, vb2_buf->planes[2].m.userptr);

	ctx->is_postraw_done = 0;
	if (ctx->is_offline_postraw) {
		uint64_t isptopb = ctx->phys_regs[ISP_BLK_ID_ISPTOP];

		// kick postraw
		ISP_WO_BITS(isptopb, REG_ISP_TOP_T, REG_2, SHAW_UP_POST, 1);
		ISP_WO_BITS(isptopb, REG_ISP_TOP_T, REG_2, TRIG_STR_POST, 1);
	} else {
		uint64_t preraw0 = ctx->phys_regs[ISP_BLK_ID_PRERAW0];

		ISP_WO_BITS(preraw0, REG_PRE_RAW_T, FRAME_VLD,
			    PRE_RAW_FRAME_VLD, 1);
	}
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
static int bm_isp_queue_setup(struct vb2_queue *vq,
			      unsigned int *nbuffers, unsigned int *nplanes,
			      unsigned int sizes[], struct device *alloc_devs[])
{
	struct bm_isp_vdev *vdev = vb2_get_drv_priv(vq);
	unsigned int planes = vdev->fmt->buffers;
	unsigned int h = vdev->src_size.height;
	unsigned int p;

	dprintk(VIP_VB2, "+\n");

	for (p = 0; p < planes; ++p)
		sizes[p] = vdev->bytesperline[p] * h;

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
static void bm_isp_buf_queue(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct bm_isp_vdev *vdev = vb2_get_drv_priv(vb->vb2_queue);
	struct bm_vip_buffer *bm_vb2 =
		container_of(vbuf, struct bm_vip_buffer, vb);

	dprintk(VIP_VB2, "+\n");

	bm_vip_buf_queue((struct bm_base_vdev *)vdev, bm_vb2);

	if (vdev->num_rdy == 1)
		_post_hw_enque(vdev);
}

static int bm_isp_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct bm_isp_vdev *vdev = vb2_get_drv_priv(vq);
	int rc = 0;
	uint64_t preraw0 = vdev->ctx.phys_regs[ISP_BLK_ID_PRERAW0];

	dprintk(VIP_VB2, "+\n");

	vdev->seq_count = 0;
	vdev->preraw_frame_number[0] = 0;
	vdev->preraw_frame_number[1] = 0;
	vdev->postraw_frame_number = 0;
	vdev->preraw_sof_count[0] = 0;
	vdev->preraw_sof_count[1] = 0;

	isp_ctrl_setup(&vdev->ctx);
	isp_dma_setup(&vdev->ctx);

	ISP_WO_BITS(preraw0, REG_PRE_RAW_T, FRAME_VLD, PRE_RAW_FRAME_VLD, 1);
	return rc;
}

/* abort streaming and wait for last buffer */
static void bm_isp_stop_streaming(struct vb2_queue *vq)
{
	struct bm_isp_vdev *vdev = vb2_get_drv_priv(vq);
	struct bm_vip_buffer *bm_vb2, *tmp;
	unsigned long flags;
	struct vb2_buffer *vb2_buf;
	struct isp_buffer *isp_b;

	dprintk(VIP_VB2, "+\n");

	/*
	 * Release all the buffers enqueued to driver
	 * when streamoff is issued
	 */
	spin_lock_irqsave(&vdev->rdy_lock, flags);
	list_for_each_entry_safe(bm_vb2, tmp, &(vdev->rdy_queue), list) {
		vb2_buf = &(bm_vb2->vb.vb2_buf);
		if (vb2_buf->state == VB2_BUF_STATE_DONE)
			continue;
		vb2_buffer_done(vb2_buf, VB2_BUF_STATE_DONE);
	}
	vdev->num_rdy = 0;
	spin_unlock_irqrestore(&vdev->rdy_lock, flags);

	while ((isp_b = isp_buf_remove(&pre_queue)) != NULL)
		kfree(isp_b);
	while ((isp_b = isp_buf_remove(&post_queue)) != NULL)
		kfree(isp_b);
}

const struct vb2_ops bm_isp_qops = {
//    .buf_init           =
	.queue_setup        = bm_isp_queue_setup,
//    .buf_finish         = bm_isp_buf_finish,
	.buf_queue          = bm_isp_buf_queue,
	.start_streaming    = bm_isp_start_streaming,
	.stop_streaming     = bm_isp_stop_streaming,
//    .wait_prepare       = vb2_ops_wait_prepare,
//    .wait_finish        = vb2_ops_wait_finish,
};

/*************************************************************************
 *	VB2-MEM-OPS definition
 *************************************************************************/
static void *isp_get_userptr(struct device *dev, unsigned long vaddr,
			     unsigned long size,
			     enum dma_data_direction dma_dir)
{
	return (void *)0xdeadbeef;
}

static void isp_put_userptr(void *buf_priv)
{
}

static const struct vb2_mem_ops bm_isp_vb2_mem_ops = {
	.get_userptr = isp_get_userptr,
	.put_userptr = isp_put_userptr,
};

/*************************************************************************
 *	FOPS definition
 *************************************************************************/
static int bm_isp_open(struct file *file)
{
	int rc = 0;
	struct bm_isp_vdev *vdev = video_drvdata(file);
	struct bm_vip_dev *bdev = NULL;
	struct vb2_queue *q;

	WARN_ON(!vdev);

	/* !!! only ONE open is allowed !!! */
	if (atomic_cmpxchg(&vdev->opened, 0, 1)) {
		dprintk(VIP_ERR, " device has been occupied. Reject %s\n",
			current->comm);
		return -EBUSY;
	}

	bdev = container_of(vdev, struct bm_vip_dev, isp_vdev);
	spin_lock_init(&vdev->rdy_lock);

	// vb2_queue init
	q = &vdev->vb_q;
	q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	q->io_modes = VB2_USERPTR;
	q->buf_struct_size = sizeof(struct bm_vip_buffer);
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	q->min_buffers_needed = 0;

	q->drv_priv = vdev;
	q->dev = bdev->v4l2_dev.dev;
	q->ops = &bm_isp_qops;
	q->mem_ops = &bm_isp_vb2_mem_ops;
	//q->lock = &vdev->mutex;

	rc = vb2_queue_init(q);
	if (rc) {
		dprintk(VIP_ERR, "errcode(%d)\n", rc);
	} else {
		INIT_LIST_HEAD(&vdev->rdy_queue);
		vdev->num_rdy = 0;
		dprintk(VIP_INFO, "by %s\n", current->comm);
	}

	return rc;
}

static int bm_isp_release(struct file *file)
{
	struct bm_isp_vdev *vdev = video_drvdata(file);

	WARN_ON(!vdev);

	atomic_set(&vdev->opened, 0);

	dprintk(VIP_INFO, "-\n");
	return 0;
}

static struct v4l2_file_operations bm_isp_fops = {
	.owner = THIS_MODULE,
	.open = bm_isp_open,
	.release = bm_isp_release,
	.poll = vb2_fop_poll, //.poll = bm_isp_poll,
	.unlocked_ioctl = video_ioctl2,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = video_ioctl2,
#endif
};

/*************************************************************************
 *	IOCTL definition
 *************************************************************************/
static int bm_isp_querycap(struct file *file, void *priv,
			   struct v4l2_capability *cap)
{
	struct bm_isp_vdev *vdev = video_drvdata(file);
	struct bm_vip_dev *bdev =
		container_of(vdev, struct bm_vip_dev, isp_vdev);

	strlcpy(cap->driver, BM_VIP_DRV_NAME, sizeof(cap->driver));
	strlcpy(cap->card, BM_VIP_DVC_NAME, sizeof(cap->card));
	snprintf(cap->bus_info, sizeof(cap->bus_info),
		 "platform:%s", bdev->v4l2_dev.name);

	cap->capabilities = vdev->vid_caps | V4L2_CAP_DEVICE_CAPS;
	return 0;
}

static int bm_isp_g_ctrl(struct file *file, void *priv,
			 struct v4l2_control *vc)
{
	int rc = -EINVAL;
	return rc;
}

static int bm_isp_s_ctrl(struct file *file, void *priv,
			 struct v4l2_control *vc)
{
	int rc = -EINVAL;
	return rc;
}

static int bm_isp_s_ext_ctrls(struct file *file, void *priv,
			      struct v4l2_ext_controls *vc)
{
	struct bm_isp_vdev *vdev = video_drvdata(file);
	struct v4l2_ext_control *ext_ctrls;
	int rc = -EINVAL, i = 0;

	ext_ctrls = vc->controls;
	for (i = 0; i < vc->count; ++i) {
		switch (ext_ctrls[i].id) {

		case V4L2_CID_DV_VIP_ISP_PATTERN:
			if (ext_ctrls[i].value >= BM_VIP_PAT_MAX) {
				dprintk(VIP_ERR, "invalid isp-pattern(%d)\n",
					ext_ctrls[i].value);
				break;
			}
			// TODO: for isp patgen ctrl
			break;

		case V4L2_CID_DV_VIP_ISP_ONLINE:
			vdev->online = ext_ctrls[i].value;
			vdev->ctx.is_offline_postraw = !ext_ctrls[i].value;
			sclr_ctrl_set_disp_src(vdev->online);
			break;

		default:
			break;
		}
	}
	return rc;
}

int bm_isp_enum_fmt_vid_mplane(struct file *file, void  *priv,
			       struct v4l2_fmtdesc *f)
{
	dprintk(VIP_DBG, "+\n");
	return bm_vip_enum_fmt_vid(file, priv, f);
}

int bm_isp_g_fmt_vid_cap_mplane(struct file *file, void *priv,
				struct v4l2_format *f)
{
	struct bm_isp_vdev *vdev = video_drvdata(file);
	struct v4l2_pix_format_mplane *mp = &f->fmt.pix_mp;
	unsigned int p;

	dprintk(VIP_DBG, "+\n");
	WARN_ON(!vdev);

	mp->width        = vdev->src_size.width;
	mp->height       = vdev->src_size.height;
	mp->field        = V4L2_FIELD_NONE;
	mp->pixelformat  = vdev->fmt->fourcc;
	mp->colorspace   = vdev->colorspace;
	mp->xfer_func    = V4L2_XFER_FUNC_DEFAULT;
	mp->ycbcr_enc    = V4L2_YCBCR_ENC_DEFAULT;
	mp->quantization = V4L2_QUANTIZATION_DEFAULT;
	mp->num_planes   = vdev->fmt->buffers;
	for (p = 0; p < mp->num_planes; p++) {
		mp->plane_fmt[p].bytesperline = vdev->bytesperline[p];
		mp->plane_fmt[p].sizeimage = vdev->bytesperline[p] * mp->height;
	}

	return 0;
}

int bm_isp_try_fmt_vid_cap_mplane(struct file *file, void *priv,
				  struct v4l2_format *f)
{
	struct bm_isp_vdev *vdev = video_drvdata(file);
	struct v4l2_pix_format_mplane *mp = &f->fmt.pix_mp;
	int rc = bm_vip_try_fmt_vid_mplane(f);

	if (rc < 0)
		return rc;

	if ((mp->width < vdev->src_size.width) ||
	    (mp->height < vdev->src_size.height))
		return -EINVAL;
	if (mp->pixelformat != V4L2_PIX_FMT_YUV420M) {
		dprintk(VIP_ERR, "fourcc(%x) isnot yuv420.\n", mp->pixelformat);
		return -EINVAL;
	}

	return 0;
}

int bm_isp_s_fmt_vid_cap_mplane(struct file *file, void *priv,
				struct v4l2_format *f)
{
	struct bm_isp_vdev *vdev = video_drvdata(file);
	struct v4l2_pix_format_mplane *mp = &f->fmt.pix_mp;
	struct v4l2_plane_pix_format *pfmt = mp->plane_fmt;
	const struct bm_vip_fmt *fmt;
	unsigned int p;
	int rc = bm_isp_try_fmt_vid_cap_mplane(file, priv, f);

	dprintk(VIP_DBG, "+\n");
	if (rc < 0)
		return rc;

	fmt = bm_vip_get_format(mp->pixelformat);
	vdev->fmt = fmt;
	vdev->colorspace = mp->colorspace;
	for (p = 0; p < mp->num_planes; p++)
		vdev->bytesperline[p] = pfmt[p].bytesperline;

	vdev->ctx.img_width = vdev->src_size.width;
	vdev->ctx.img_height = vdev->src_size.height;
	ispblk_dma_config(&vdev->ctx, ISP_BLK_ID_DMA3, 0);
	ispblk_dma_config(&vdev->ctx, ISP_BLK_ID_DMA4, 0);
	ispblk_dma_config(&vdev->ctx, ISP_BLK_ID_DMA5, 0);

	return rc;
}

int bm_isp_streamon(struct file *file, void *fh, enum v4l2_buf_type i)
{
	struct bm_isp_vdev *vdev = video_drvdata(file);
	int rc = 0;

	rc = vb2_streamon(&vdev->vb_q, i);
	if (!rc)
		isp_streaming(&vdev->ctx, true);

	return rc;
}

int bm_isp_streamoff(struct file *file, void *fh, enum v4l2_buf_type i)
{
	struct bm_isp_vdev *vdev = video_drvdata(file);
	int rc = 0;

	rc = vb2_streamoff(&vdev->vb_q, i);
	if (!rc) {
		uint8_t count = 5;

		// wait to make sure hw stopped.
		isp_streaming(&vdev->ctx, false);
		while (--count > 0) {
			if (vdev->ctx.is_preraw0_done &&
			    vdev->ctx.is_postraw_done)
				break;
			dprintk(VIP_DBG, "wait count(%d)\n", count);
			mdelay(10);
		}

		// reset at stop for next run.
		isp_reset(&vdev->ctx);
	}

	return rc;
}

static const struct v4l2_ioctl_ops bm_isp_ioctl_ops = {
	.vidioc_querycap = bm_isp_querycap,
	.vidioc_g_ctrl = bm_isp_g_ctrl,
	.vidioc_s_ctrl = bm_isp_s_ctrl,
	.vidioc_s_ext_ctrls = bm_isp_s_ext_ctrls,

	.vidioc_enum_fmt_vid_cap_mplane = bm_isp_enum_fmt_vid_mplane,
	.vidioc_g_fmt_vid_cap_mplane    = bm_isp_g_fmt_vid_cap_mplane,
	.vidioc_try_fmt_vid_cap_mplane  = bm_isp_try_fmt_vid_cap_mplane,
	.vidioc_s_fmt_vid_cap_mplane    = bm_isp_s_fmt_vid_cap_mplane,

//	.vidioc_s_dv_timings        = bm_isp_s_dv_timings,
//	.vidioc_g_dv_timings        = bm_isp_g_dv_timings,
//	.vidioc_query_dv_timings    = bm_isp_query_dv_timings,
//	.vidioc_enum_dv_timings     = bm_isp_enum_dv_timings,
//	.vidioc_dv_timings_cap      = bm_isp_dv_timings_cap,

	.vidioc_reqbufs         = vb2_ioctl_reqbufs,
	//.vidioc_create_bufs     = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf     = vb2_ioctl_prepare_buf,
	.vidioc_querybuf        = vb2_ioctl_querybuf,
	.vidioc_qbuf            = vb2_ioctl_qbuf,
	.vidioc_dqbuf           = vb2_ioctl_dqbuf,
	//.vidioc_expbuf          = vb2_ioctl_expbuf,
	.vidioc_streamon        = bm_isp_streamon,
	.vidioc_streamoff       = bm_isp_streamoff,

	//.vidioc_subscribe_event     = disp_subscribe_event,
	//.vidioc_unsubscribe_event   = v4l2_event_unsubscribe,
};

static inline struct bm_isp_vdev *notifier_to_bmisp(
		struct v4l2_async_notifier *n)
{
	return container_of(n, struct bm_isp_vdev, subdev_notifier);
}

static int subdev_notifier_bound(struct v4l2_async_notifier *notifier,
				 struct v4l2_subdev *subdev,
				 struct v4l2_async_subdev *asd)
{
	struct bm_isp_vdev *vdev = notifier_to_bmisp(notifier);
	struct bm_sensor_info *si = NULL;
	int i;

	/* Find platform data for this sensor subdev */
	for (i = 0; i < ARRAY_SIZE(vdev->sensor); i++)
		if (vdev->sensor[i].asd.match.of.node == subdev->dev->of_node)
			si = &vdev->sensor[i];

	if (si == NULL)
		return -EINVAL;

	/* v4l2_set_subdev_hostdata(subdev, &si->pdata); */

	si->subdev = subdev;

	dprintk(VIP_INFO, "%s: subdev, %s.\n", __func__, subdev->name);
	vdev->num_sd++;

	return 0;
}

static int subdev_notifier_complete(struct v4l2_async_notifier *notifier)
{
	struct bm_isp_vdev *vdev = notifier_to_bmisp(notifier);
	struct bm_vip_dev *bdev  = container_of(vdev,
			struct bm_vip_dev, isp_vdev);

	dprintk(VIP_INFO, "%s\n", __func__);
	return v4l2_device_register_subdev_nodes(&bdev->v4l2_dev);
}

static int _get_sensor_device(
	struct device *dev,
	struct bm_isp_vdev *vdev,
	unsigned int array_len)
{
	struct device_node *node = NULL;
	struct device_node *camera_list_node = NULL;
	int index, size = 0;
	const __be32 *phandle;

	node = of_node_get(dev->of_node);
	if (IS_ERR_OR_NULL(node)) {
		dev_err(dev, "Unable to obtain bitmain-vip device node\n");
		return -EEXIST;
	}

	phandle = of_get_property(node, "bitmain,camera-modules", &size);
	if (IS_ERR_OR_NULL(phandle)) {
		dprintk(VIP_ERR, "no camera-modules-attached'\n");
		of_node_put(node);
		return -EINVAL;
	}

	of_node_put(node);
	for (index = 0; index < size / sizeof(*phandle); index++) {
		struct bm_sensor_info *sensor = &vdev->sensor[index];

		camera_list_node = of_parse_phandle(node,
			"bitmain,camera-modules", index);
		of_node_put(node);
		if (IS_ERR_OR_NULL(camera_list_node)) {
			dprintk(VIP_ERR, "invalid index %d for ", index);
			dprintk(VIP_ERR, "property 'bitmain,camera-modules'\n");
			return -EINVAL;
		}

		sensor->asd.match_type = V4L2_ASYNC_MATCH_OF;
		sensor->asd.match.of.node = camera_list_node;
		vdev->async_subdevs[index] = &sensor->asd;

		vdev->num_sd++;
		dprintk(VIP_INFO, "find async camera, %s.\n",
			camera_list_node->name);
	}
	return 0;
}

/*************************************************************************
 *	General functions
 *************************************************************************/
int isp_create_instance(struct platform_device *pdev)
{
	int rc = 0;
	struct bm_vip_dev *bdev;
	struct video_device *vfd;
	struct bm_isp_vdev *vdev;

	bdev = dev_get_drvdata(&pdev->dev);
	if (!bdev) {
		dprintk(VIP_ERR, "invalid data\n");
		return -EINVAL;
	}
	vdev = &bdev->isp_vdev;
	vdev->fmt = bm_vip_get_format(V4L2_PIX_FMT_YUV420M);
	vdev->vid_caps = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING;
	//vdev->dv_timings = def_dv_timings[0];
	spin_lock_init(&vdev->rdy_lock);

	vfd = &(vdev->vdev);
	snprintf(vfd->name, sizeof(vfd->name), "vip-isp");
	vfd->fops = &bm_isp_fops;
	vfd->ioctl_ops = &bm_isp_ioctl_ops;
	vfd->vfl_dir = VFL_DIR_RX;
	vfd->vfl_type = VFL_TYPE_GRABBER;
	vfd->minor = -1;
	vfd->device_caps = vdev->vid_caps;
	vfd->release = video_device_release_empty;
	vfd->v4l2_dev = &bdev->v4l2_dev;
	vfd->queue = &vdev->vb_q;

	rc = video_register_device(vfd, VFL_TYPE_GRABBER, ISP_DEVICE_IDX);
	if (rc) {
		dprintk(VIP_ERR, "Failed to register isp-device\n");
		goto err_register;
	}

	vfd->lock = &bdev->mutex;
	video_set_drvdata(vfd, vdev);
	atomic_set(&vdev->opened, 0);
	vdev->online = false;

	// TODO: get sensor's fmt and update
	/* Sensor subdev */
	_get_sensor_device(&pdev->dev, &bdev->isp_vdev, ISP_NUM_INPUTS);

	if (vdev->num_sd) {
		vdev->subdev_notifier.subdevs = vdev->async_subdevs;
		vdev->subdev_notifier.num_subdevs = vdev->num_sd;
		vdev->subdev_notifier.bound = subdev_notifier_bound;
		vdev->subdev_notifier.complete = subdev_notifier_complete;
		vdev->num_sd = 0;

		rc = v4l2_async_notifier_register(&bdev->v4l2_dev,
						&vdev->subdev_notifier);
		if (rc)
			goto err_register;

	}

	vdev->src_size.width = 1308;
	vdev->src_size.height = 736;
	isp_init_param(&vdev->ctx);

	dprintk(VIP_INFO, "isp registered as %s\n",
		video_device_node_name(vfd));
	return rc;

err_register:
	return rc;
}

void isp_irq_handler(struct bm_vip_dev *bdev)
{
	struct bm_isp_vdev *vdev;
	uint32_t isr_status, cbdg_st;
	uint64_t isp_top, csi_bdg0;
	struct isp_ctx *ctx = NULL;

	if (!bdev) {
		pr_err("%s dev is null\n", __func__);
		return;
	}

	vdev = &bdev->isp_vdev;
	ctx = &vdev->ctx;
	isp_top = ctx->phys_regs[ISP_BLK_ID_ISPTOP];
	csi_bdg0 = ctx->phys_regs[ISP_BLK_ID_CSIBDG0];

	isr_status = ISP_RD_REG(isp_top, REG_ISP_TOP_T, REG_0);
	cbdg_st = ISP_RD_REG(csi_bdg0, REG_ISP_CSI_BDG_T, CSI_INTERRUPT_STATUS);

	ISP_WR_REG(isp_top, REG_ISP_TOP_T, REG_0, isr_status);
	ISP_WR_REG(csi_bdg0, REG_ISP_CSI_BDG_T, CSI_INTERRUPT_STATUS, cbdg_st);

	dprintk(VIP_DBG, "top_sts(%#x), csi_sts(%#x)\n", isr_status, cbdg_st);
	if (cbdg_st & 0x02) {
		dprintk(VIP_ERR, "csi_bdg rt_pxl_cnt(%d), rt_line_cnt(%d)\n",
			ISP_RD_REG(csi_bdg0, REG_ISP_CSI_BDG_T,
				   CSI_RT_INFO_PXL_CNT),
			ISP_RD_REG(csi_bdg0, REG_ISP_CSI_BDG_T,
				   CSI_RT_INFO_LINE_CNT));
		dprintk(VIP_ERR, "csi_bdg ch0 no rspd cyc(%d)\n",
			ISP_RD_REG(csi_bdg0, REG_ISP_CSI_BDG_T,
				   CH0_NO_RSPD_CYC_CNT));
	}

	/* prerarw frm_done */
	if (isr_status & 0x01) {
		++vdev->preraw_frame_number[0];
		ctx->is_preraw0_done = true;

		// TODO: policy review
		if (ctx->is_offline_postraw && !ctx->is_offline_preraw) {
			struct isp_buffer *b;

			b = isp_buf_remove(&pre_queue);
			if (vb2_is_streaming(&vdev->vb_q)) {
				_pre_hw_enque(vdev);
				if (b)
					isp_buf_queue(&post_queue, b);
				_post_hw_enque(vdev);
			}
		}
	}

	/* preraw1 frm_done */
	if (isr_status & 0x02)
		++vdev->preraw_frame_number[1];

	/* prerarw0 sof */
	if (isr_status & 0x1000)
		++vdev->preraw_sof_count[0];

	/* prerarw1 sof */
	if (isr_status & 0x2000)
		++vdev->preraw_sof_count[1];

	/* postraw frm_done */
	if (isr_status & 0x04) {
		struct bm_vip_buffer *b = NULL;

		ctx->is_postraw_done = 1;
		if (vb2_is_streaming(&vdev->vb_q)) {
			b = bm_vip_buf_remove((struct bm_base_vdev *)vdev);
			b->vb.vb2_buf.timestamp = ktime_get_ns();
			b->vb.sequence = ++vdev->seq_count;
			vb2_buffer_done(&b->vb.vb2_buf, VB2_BUF_STATE_DONE);

			if (ctx->is_offline_postraw) {
				struct isp_buffer *ispb;

				ispb = isp_buf_remove(&post_queue);
				isp_buf_queue(&pre_queue, ispb);
				_pre_hw_enque(vdev);
			}

			_post_hw_enque(vdev);
		}
	}
}
