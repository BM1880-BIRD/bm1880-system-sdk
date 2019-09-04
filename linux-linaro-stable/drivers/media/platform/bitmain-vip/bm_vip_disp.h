#ifndef _BM_VIP_DISP_H_
#define _BM_VIP_DISP_H_

#define BM_DISP_NEVENTS   5

int disp_create_instance(struct platform_device *pdev);
void disp_irq_handler(union sclr_intr intr_status, struct bm_vip_dev *bdev);

#endif
