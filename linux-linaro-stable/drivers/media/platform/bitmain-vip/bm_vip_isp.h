#ifndef _BM_VIP_ISP_H_
#define _BM_VIP_ISP_H_

int isp_create_instance(struct platform_device *pdev);
void isp_irq_handler(struct bm_vip_dev *bdev);

#endif
