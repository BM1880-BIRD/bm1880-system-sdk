#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/videodev2.h>
#include <linux/platform_device.h>
#include <media/videobuf2-vmalloc.h>
#include <media/v4l2-common.h>

#include "vip/vip_common.h"
#include "vip/scaler.h"
#include "vip/dwa.h"
#include "vip/inc/isp_reg.h"
#include "vip/isp_drv.h"

#include "bm_debug.h"
#include "bm_vip_core.h"
#include "bm_vip_disp.h"
#include "bm_vip_sc.h"
#include "bm_vip_img.h"
#include "bm_vip_dwa.h"
#include "bm_vip_isp.h"

/* Instance is already queued on the job_queue */
#define TRANS_QUEUED        (1 << 0)
/* Instance is currently running in hardware */
#define TRANS_RUNNING       (1 << 1)
/* Instance is currently aborting */
#define TRANS_ABORT         (1 << 2)


int bm_vip_debug = VIP_ERR | VIP_WARN | VIP_INFO | VIP_DBG | VIP_VB2;
const struct bm_vip_fmt bm_vip_formats[] = {
	{
	.fourcc      = V4L2_PIX_FMT_YUV420M,
	.fmt         = SCL_FMT_YUV420,
	.bit_depth   = { 8, 2, 2 },
	.buffers     = 3,
	.plane_sub_h = 2,
	.plane_sub_v = 2,
	},
	{
	.fourcc      = V4L2_PIX_FMT_YUV422M,
	.fmt         = SCL_FMT_YUV422,
	.bit_depth   = { 8, 4, 4 },
	.buffers     = 3,
	.plane_sub_h = 2,
	.plane_sub_v = 1,
	},
	{
	.fourcc      = V4L2_PIX_FMT_RGBM, /* rgb */
	.fmt         = SCL_FMT_RGB_PLANAR,
	.bit_depth   = { 8, 8, 8 },
	.buffers     = 3,
	.plane_sub_h = 1,
	.plane_sub_v = 1,
	},
	{
	.fourcc      = V4L2_PIX_FMT_RGB24, /* rgb */
	.fmt         = SCL_FMT_RGB_PACKED,
	.bit_depth   = { 24 },
	.buffers     = 1,
	.plane_sub_h = 1,
	.plane_sub_v = 1,
	},
	{
	.fourcc      = V4L2_PIX_FMT_BGR24, /* bgr */
	.fmt         = SCL_FMT_BGR_PACKED,
	.bit_depth   = { 24 },
	.buffers     = 1,
	.plane_sub_h = 1,
	.plane_sub_v = 1,
	},
	{
	.fourcc      = V4L2_PIX_FMT_GREY, /* Y-Only */
	.fmt         = SCL_FMT_Y_ONLY,
	.bit_depth   = { 8 },
	.buffers     = 1,
	.plane_sub_h = 1,
	.plane_sub_v = 1,
	},
	{
	.fourcc      = V4L2_PIX_FMT_HSV24, /* hsv */
	.fmt         = SCL_FMT_BGR_PACKED,
	.bit_depth   = { 24 },
	.buffers     = 1,
	.plane_sub_h = 1,
	.plane_sub_v = 1,
	},
};


const struct bm_vip_fmt *bm_vip_get_format(u32 pixelformat)
{
	const struct bm_vip_fmt *fmt;
	unsigned int k;

	for (k = 0; k < ARRAY_SIZE(bm_vip_formats); k++) {
		fmt = &bm_vip_formats[k];
		if (fmt->fourcc == pixelformat)
			return fmt;
	}

	return NULL;
}

int bm_vip_enum_fmt_vid(struct file *file, void *priv, struct v4l2_fmtdesc *f)
{
	const struct bm_vip_fmt *fmt;

	if (f->index >= ARRAY_SIZE(bm_vip_formats))
		return -EINVAL;

	fmt = &bm_vip_formats[f->index];
	f->pixelformat = fmt->fourcc;

	return 0;
}

int bm_vip_try_fmt_vid_mplane(struct v4l2_format *f)
{
	struct v4l2_pix_format_mplane *mp = &f->fmt.pix_mp;
	struct v4l2_plane_pix_format *pfmt = mp->plane_fmt;
	const struct bm_vip_fmt *fmt;
	unsigned int bytesperline;
	u8 p;

	dprintk(VIP_DBG, "+\n");
	fmt = bm_vip_get_format(mp->pixelformat);
	dprintk(VIP_INFO, "size(%d-%d) fourcc(%x)\n", mp->width, mp->height,
			mp->pixelformat);
	if (!fmt) {
		dprintk(VIP_ERR, "Fourcc format(0x%08x) unknown.\n",
				mp->pixelformat);
		return -EINVAL;
	}

	mp->field = V4L2_FIELD_NONE;    // progressive only
	mp->width = clamp_val(mp->width, SCL_MIN_WIDTH, SCL_MAX_WIDTH);
	mp->height = clamp_val(mp->height, SCL_MIN_HEIGHT, SCL_MAX_HEIGHT);
	if (IS_YUV_FMT(fmt->fmt)) {
		// YUV422/420
		mp->width &= ~(fmt->plane_sub_h - 1);
		mp->height &= ~(fmt->plane_sub_v - 1);
	}

	mp->num_planes = fmt->buffers;
	for (p = 0; p < mp->num_planes; p++) {
		u8 plane_sub_h = (p == 0) ? 1 : fmt->plane_sub_h;
		u8 plane_sub_v = (p == 0) ? 1 : fmt->plane_sub_v;
		/* Calculate the minimum supported bytesperline value */
		bytesperline = VIP_ALIGN((mp->width * fmt->bit_depth[p]
					  * plane_sub_h) >> 3);

		if (pfmt[p].bytesperline < bytesperline)
			pfmt[p].bytesperline = bytesperline;

		pfmt[p].sizeimage = pfmt[p].bytesperline * mp->height
				    / plane_sub_v;

		memset(pfmt[p].reserved, 0, sizeof(pfmt[p].reserved));
	}

	mp->xfer_func = V4L2_XFER_FUNC_DEFAULT;
	mp->ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
	mp->quantization = V4L2_QUANTIZATION_DEFAULT;
	if (mp->pixelformat == V4L2_PIX_FMT_RGBM ||
	    mp->pixelformat == V4L2_PIX_FMT_RGB24 ||
	    mp->pixelformat == V4L2_PIX_FMT_BGR24) {
		mp->colorspace = V4L2_COLORSPACE_SRGB;
	} else if (mp->pixelformat == V4L2_PIX_FMT_HSV24) {
		mp->colorspace = V4L2_COLORSPACE_SRGB;
	} else if (mp->width <= 720) {
		mp->colorspace = V4L2_COLORSPACE_SMPTE170M;
	} else {
		mp->colorspace = V4L2_COLORSPACE_REC709;
	}
	memset(mp->reserved, 0, sizeof(mp->reserved));

	return 0;
}

void bm_vip_buf_queue(struct bm_base_vdev *vdev, struct bm_vip_buffer *b)
{
	unsigned long flags;

	spin_lock_irqsave(&vdev->rdy_lock, flags);
	list_add_tail(&b->list, &vdev->rdy_queue);
	++vdev->num_rdy;
	spin_unlock_irqrestore(&vdev->rdy_lock, flags);
}

struct bm_vip_buffer *bm_vip_next_buf(struct bm_base_vdev *vdev)
{
	unsigned long flags;
	struct bm_vip_buffer *b = NULL;

	spin_lock_irqsave(&vdev->rdy_lock, flags);
	if (!list_empty(&vdev->rdy_queue))
		b = list_first_entry(&vdev->rdy_queue,
			struct bm_vip_buffer, list);
	spin_unlock_irqrestore(&vdev->rdy_lock, flags);

	return b;
}

struct bm_vip_buffer *bm_vip_buf_remove(struct bm_base_vdev *vdev)
{
	unsigned long flags;
	struct bm_vip_buffer *b = NULL;

	spin_lock_irqsave(&vdev->rdy_lock, flags);
	if (!list_empty(&vdev->rdy_queue)) {
		b = list_first_entry(&vdev->rdy_queue,
			struct bm_vip_buffer, list);
		list_del_init(&b->list);
		--vdev->num_rdy;
	}
	spin_unlock_irqrestore(&vdev->rdy_lock, flags);

	return b;
}

void bm_img_device_run(void *priv)
{
	struct bm_img_vdev *idev = priv;
	struct vb2_buffer *vb2_buf;
	struct bm_vip_buffer *b = NULL;
	//unsigned long flags;

	b = bm_vip_next_buf((struct bm_base_vdev *)idev);
	if (!b)
		return;
	vb2_buf = &b->vb.vb2_buf;

	dprintk(VIP_DBG, "update img-buf: 0x%lx-0x%lx-0x%lx\n",
	    vb2_buf->planes[0].m.userptr, vb2_buf->planes[1].m.userptr,
	    vb2_buf->planes[2].m.userptr);
	sclr_img_set_addr(idev->img_type, vb2_buf->planes[0].m.userptr,
	    vb2_buf->planes[1].m.userptr, vb2_buf->planes[2].m.userptr);

	//spin_lock_irqsave(&dev->job_lock, flags);
	idev->job_flags |= TRANS_RUNNING;
	//spin_unlock_irqrestore(&dev->job_lock, flags);

	// TODO: need to check all possible instances at once,
	// create a work to do?
	// img-start if related sc's queue isn't empty
	sclr_img_start(idev->img_type);
}

void bm_vip_try_schedule(struct bm_img_vdev *idev)
{
	struct bm_vip_dev *bdev = NULL;
	unsigned long flags_out[4], flags_img, flags_job;
	u8 i = 0;
	bool check_img_buffer = (idev->input_type == BM_VIP_INPUT_MEM);
	bool sc_need_check[4] = {false, false, false, false};
	bool sc_locked[4] = {false, false, false, false};

	bdev = container_of(idev, struct bm_vip_dev, img_vdev[idev->dev_idx]);

	// check if instances is on
	if (!idev->vb_q.streaming) {
		dprintk(VIP_WARN, "img needs to be on.\n");
		return;
	}

	// check sc_d's streaming if bounding
	if (idev->sc_bounding & BM_VIP_IMG_2_SC_D) {
		if (!bdev->sc_vdev[BM_VIP_SC_D].vb_q.streaming) {
			dprintk(VIP_WARN, "sc-d needs to be on.\n");
			return;
		}
		sc_need_check[BM_VIP_SC_D] = true;
	}

	// check sc_v's streaming if bounding
	if (idev->sc_bounding & BM_VIP_IMG_2_SC_V) {
		if (!bdev->sc_vdev[BM_VIP_SC_V0].vb_q.streaming &&
		    !bdev->sc_vdev[BM_VIP_SC_V1].vb_q.streaming &&
		    !bdev->sc_vdev[BM_VIP_SC_V2].vb_q.streaming) {
			dprintk(VIP_WARN, "sc-v needs to be on.\n");
			return;
		}
		sc_need_check[BM_VIP_SC_V0] =
			bdev->sc_vdev[BM_VIP_SC_V0].vb_q.streaming;
		sc_need_check[BM_VIP_SC_V1] =
			bdev->sc_vdev[BM_VIP_SC_V1].vb_q.streaming;
		sc_need_check[BM_VIP_SC_V2] =
			bdev->sc_vdev[BM_VIP_SC_V2].vb_q.streaming;
	}

	spin_lock_irqsave(&idev->job_lock, flags_job);
	if (idev->job_flags & TRANS_QUEUED) {
		dprintk(VIP_WARN, "On job queue already\n");
		goto job_unlock;
	}

	spin_lock_irqsave(&idev->rdy_lock, flags_img);
	// if img_in online, then buffer no needed
	if (check_img_buffer) {
		if (list_empty(&idev->rdy_queue)) {
			dprintk(VIP_WARN, "No input buffers available.\n");
			goto img_unlock;
		}
	}

	// check sc's queue if bounding
	for (i = BM_VIP_SC_D; i <= BM_VIP_SC_V2; ++i) {
		if (!sc_need_check[i])
			continue;

		spin_lock_irqsave(&bdev->sc_vdev[i].rdy_lock, flags_out[i]);
		sc_locked[i] = true;
		if (list_empty(&bdev->sc_vdev[i].rdy_queue)) {
			dprintk(VIP_WARN, "No sc-%d buffer available.\n", i);
			goto sc_unlock;
		}
	}

	// hw operations
	idev->job_flags |= TRANS_QUEUED;
	for (i = BM_VIP_SC_D; i <= BM_VIP_SC_V2; ++i) {
		if (!sc_locked[i])
			continue;

		spin_unlock_irqrestore(&bdev->sc_vdev[i].rdy_lock,
				       flags_out[i]);
		bm_sc_device_run(&bdev->sc_vdev[i]);
	}

	spin_unlock_irqrestore(&idev->rdy_lock, flags_img);
	bm_img_device_run(idev);

	spin_unlock_irqrestore(&idev->job_lock, flags_job);
	return;

sc_unlock:
	for (i = BM_VIP_SC_D; i <= BM_VIP_SC_V2; ++i) {
		if (!sc_locked[i])
			continue;
		spin_unlock_irqrestore(&bdev->sc_vdev[i].rdy_lock,
				       flags_out[i]);
	}
img_unlock:
	spin_unlock_irqrestore(&idev->rdy_lock, flags_img);
job_unlock:
	spin_unlock_irqrestore(&idev->job_lock, flags_job);
}

void bm_vip_job_finish(struct bm_img_vdev *idev)
{
	idev->job_flags &= ~(TRANS_QUEUED | TRANS_RUNNING);
	bm_vip_try_schedule(idev);
}

bool bm_vip_job_is_queued(struct bm_img_vdev *idev)
{
	return (idev->job_flags & TRANS_QUEUED);
}

static irqreturn_t scl_isr(int irq, void *_dev)
{
	union sclr_intr intr_status = sclr_intr_status();

	sclr_intr_clr(intr_status);

	//dprintk(VIP_DBG, "status(0x%x)\n", intr_status.all);
	disp_irq_handler(intr_status, _dev);
	img_irq_handler(intr_status, _dev);
	//sc_irq_handler(intr_status, _dev);

	return IRQ_HANDLED;
}

static irqreturn_t dwa_isr(int irq, void *_dev)
{
	u8 intr_status = dwa_intr_status();

	dwa_intr_clr(intr_status);

	dwa_irq_handler(intr_status, _dev);

	return IRQ_HANDLED;
}

static irqreturn_t isp_isr(int irq, void *_dev)
{
	//dprintk(VIP_DBG, "status(0x%x)\n", intr_status.all);
	isp_irq_handler(_dev);

	return IRQ_HANDLED;
}

static int _init_resources(struct platform_device *pdev)
{
	int rc = 0;
#if (DEVICE_FROM_DTS)
	int irq_num[3];
	static const char * const irq_name[] = {"sc", "dwa", "isp"};
	struct resource *res = NULL;
	void *reg_base[4];
	struct bm_vip_dev *dev;
	int i;

	dev = dev_get_drvdata(&pdev->dev);
	if (!dev) {
		dev_err(&pdev->dev, "Can not get bm_vip drvdata\n");
		return -EINVAL;
	}

	for (i = 0; i < 4; ++i) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		reg_base[i] = devm_ioremap_nocache(&pdev->dev, res->start,
						    res->end - res->start);
		dprintk(VIP_INFO, "(%d) res-reg: start: 0x%llx, end: 0x%llx.",
			i, res->start, res->end);
		dprintk(VIP_INFO, " virt-addr(%p)\n", reg_base[i]);
	}
	sclr_set_base_addr(reg_base[0]);
	dwa_set_base_addr(reg_base[1]);
	vip_set_base_addr(reg_base[2]);
	isp_set_base_addr(reg_base[3]);

	/* Interrupt */
	for (i = 0; i < ARRAY_SIZE(irq_name); ++i) {
		irq_num[i] = platform_get_irq_byname(pdev, irq_name[i]);
		if (irq_num[i] < 0) {
			dev_err(&pdev->dev, "No IRQ resource for %s\n",
				irq_name[i]);
			return -ENODEV;
		}
		dprintk(VIP_INFO, "irq(%d) for %s get from platform driver.\n",
			irq_num[i], irq_name[i]);
	}
	dev->irq_num_scl = irq_num[0];
	dev->irq_num_dwa = irq_num[1];
	dev->irq_num_isp = irq_num[2];
#endif

	return rc;
}

static int bm_vip_probe(struct platform_device *pdev)
{
	int rc = 0;
	u8 i = 0;
	struct bm_vip_dev *dev;

	/* allocate main vivid state structure */
	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		dprintk(VIP_ERR, "Failed to allocate memory for dev-core\n");
		return -ENOMEM;
	}

	/* initialize locks */
	spin_lock_init(&dev->lock);
	mutex_init(&dev->mutex);

	dev_set_drvdata(&pdev->dev, dev);
	platform_set_drvdata(pdev, dev);

	/* register v4l2_device */
	snprintf(dev->v4l2_dev.name, sizeof(dev->v4l2_dev.name), "bm_vip");
	rc = v4l2_device_register(&pdev->dev, &dev->v4l2_dev);
	if (rc) {
		dprintk(VIP_ERR, "Failed to register v4l2 device\n");
		goto err_v4l2_reg;
	}

	// for dwa - m2m
	rc = dwa_create_instance(pdev);
	if (rc) {
		dprintk(VIP_ERR, "Failed to create dwa instance\n");
		goto err_dwa_reg;
	}

	// for img(2) - cap
	rc = img_create_instance(pdev);
	if (rc) {
		dprintk(VIP_ERR, "Failed to create img instance\n");
		goto err_img_reg;
	}

	// for sc(4) - out
	rc = sc_create_instance(pdev);
	if (rc) {
		dprintk(VIP_ERR, "Failed to create sc instance\n");
		goto err_sc_reg;
	}

	// for disp - out
	rc = disp_create_instance(pdev);
	if (rc) {
		dprintk(VIP_ERR, "Failed to create disp instance\n");
		goto err_disp_reg;
	}

	// for isp - cap
	rc = isp_create_instance(pdev);
	if (rc) {
		dprintk(VIP_ERR, "Failed to create isp instance\n");
		goto err_isp_reg;
	}

	// get hw-resources
	rc = _init_resources(pdev);
	if (rc)
		goto err_irq;

	// hw init
	sclr_ctrl_init();
	//dwa_init();
	//dwa_intr_ctrl(0x01);

	sclr_ctrl_set_disp_src(false);

	// TODO: move to dts
	{
		union sclr_lvdstx cfg;

		sclr_lvdstx_get(&cfg);
		cfg.b.out_bit = 0;
		cfg.b.vesa_mode = 1;
		cfg.b.dual_ch = 0;
		cfg.b.vs_out_en = 1;
		cfg.b.hs_out_en = 1;
		cfg.b.hs_blk_en = 1;
		cfg.b.ml_swap = 1;
		cfg.b.ctrl_rev = 0;
		cfg.b.oe_swap = 0;
		cfg.b.en = 1;
		sclr_lvdstx_set(cfg);
	}
	dprintk(VIP_DBG, "hw init done\n");

	if (devm_request_irq(&pdev->dev, dev->irq_num_scl, scl_isr, IRQF_SHARED,
		 "BM_VIP_SCL", dev)) {
		dev_err(&pdev->dev, "Unable to request scl IRQ(%d)\n",
				dev->irq_num_scl);
		return -EINVAL;
	}

	if (devm_request_irq(&pdev->dev, dev->irq_num_dwa, dwa_isr, IRQF_SHARED,
		 "BM_VIP_DWA", dev)) {
		dev_err(&pdev->dev, "Unable to request dwa IRQ(%d)\n",
				dev->irq_num_dwa);
		return -EINVAL;
	}

	if (devm_request_irq(&pdev->dev, dev->irq_num_isp, isp_isr, IRQF_SHARED,
		 "BM_VIP_ISP", dev)) {
		dev_err(&pdev->dev, "Unable to request isp IRQ(%d)\n",
				dev->irq_num_isp);
		return -EINVAL;
	}

	dprintk(VIP_DBG, "done with rc(%d).\n", rc);
	return rc;

err_irq:
	video_unregister_device(&dev->isp_vdev.vdev);
err_isp_reg:
	video_unregister_device(&dev->disp_vdev.vdev);
err_disp_reg:
	for (i = 0; i < BM_VIP_SC_MAX; ++i)
		video_unregister_device(&dev->sc_vdev[i].vdev);
err_sc_reg:
	for (i = 0; i < BM_VIP_IMG_MAX; ++i)
		video_unregister_device(&dev->img_vdev[i].vdev);
err_img_reg:
	video_unregister_device(&dev->dwa_vdev.vdev);
err_dwa_reg:
	v4l2_device_unregister(&dev->v4l2_dev);
err_v4l2_reg:
	dev_set_drvdata(&pdev->dev, NULL);

	dprintk(VIP_DBG, "failed with rc(%d).\n", rc);
	return rc;
}

/*
 * bmd_remove - device remove method.
 * @pdev: Pointer of platform device.
 */
static int bm_vip_remove(struct platform_device *pdev)
{
	u8 i = 0;
	struct bm_vip_dev *dev;

	if (!pdev) {
		dev_err(&pdev->dev, "invalid param");
		return -EINVAL;
	}

	dev = dev_get_drvdata(&pdev->dev);
	if (!dev) {
		dev_err(&pdev->dev, "Can not get bm_vip drvdata");
		return 0;
	}

	for (i = 0; i < ISP_NUM_INPUTS; ++i) {
		struct bm_sensor_info *sensor = &dev->isp_vdev.sensor[i];

		if (sensor->subdev)
			v4l2_device_unregister_subdev(sensor->subdev);
	}
	video_unregister_device(&dev->disp_vdev.vdev);
	for (i = 0; i < BM_VIP_SC_MAX; ++i)
		video_unregister_device(&dev->sc_vdev[i].vdev);
	for (i = 0; i < BM_VIP_IMG_MAX; ++i)
		video_unregister_device(&dev->img_vdev[i].vdev);
	video_unregister_device(&dev->dwa_vdev.vdev);
	v4l2_device_unregister(&dev->v4l2_dev);
	dev_set_drvdata(&pdev->dev, NULL);

	return 0;
}

static const struct of_device_id bm_vip_dt_match[] = {
	{.compatible = "bitmain,vip"},
	{}
};

#if (!DEVICE_FROM_DTS)
static void bm_vip_pdev_release(struct device *dev)
{
}

static struct platform_device bm_vip_pdev = {
	.name		= "vip",
	.dev.release	= bm_vip_pdev_release,
};
#endif

static struct platform_driver bm_vip_pdrv = {
	.probe      = bm_vip_probe,
	.remove     = bm_vip_remove,
	.driver     = {
		.name		= "vip",
		.owner		= THIS_MODULE,
#if (DEVICE_FROM_DTS)
		.of_match_table	= bm_vip_dt_match,
#endif
	},
};

static int __init bm_vip_init(void)
{
	int rc;

	dprintk(VIP_INFO, " +\n");
	#if (DEVICE_FROM_DTS)
	rc = platform_driver_register(&bm_vip_pdrv);
	#else
	rc = platform_device_register(&bm_vip_pdev);
	if (rc)
		return rc;

	rc = platform_driver_register(&bm_vip_pdrv);
	if (rc)
		platform_device_unregister(&bm_vip_pdev);
	#endif

	return rc;
}

static void __exit bm_vip_exit(void)
{
	dprintk(VIP_INFO, " +\n");
	platform_driver_unregister(&bm_vip_pdrv);
	#if (!DEVICE_FROM_DTS)
	platform_device_unregister(&bm_vip_pdev);
	#endif
}

MODULE_DESCRIPTION("Bitmain Video Driver");
MODULE_AUTHOR("Jammy Huang");
MODULE_LICENSE("GPL");
module_init(bm_vip_init);
module_exit(bm_vip_exit);
