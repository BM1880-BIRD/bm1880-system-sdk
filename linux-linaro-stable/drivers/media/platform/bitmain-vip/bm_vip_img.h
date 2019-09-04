#ifndef _BM_VIP_IMG_H_
#define _BM_VIP_IMG_H_

int img_create_instance(struct platform_device *pdev);
void img_irq_handler(union sclr_intr intr_status, struct bm_vip_dev *bdev);

#endif
