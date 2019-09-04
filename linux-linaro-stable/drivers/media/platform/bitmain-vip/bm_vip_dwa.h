#ifndef _BM_VIP_DWA_H_
#define _BM_VIP_DWA_H_

int dwa_create_instance(struct platform_device *pdev);
void dwa_irq_handler(u8 intr_status, struct bm_vip_dev *bdev);

#endif
