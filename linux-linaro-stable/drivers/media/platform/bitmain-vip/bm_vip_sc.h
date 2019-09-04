#ifndef _BM_VIP_SC_H_
#define _BM_VIP_SC_H_

int sc_create_instance(struct platform_device *pdev);
void sc_irq_handler(union sclr_intr intr_status, struct bm_vip_dev *bdev);
void bm_sc_device_run(void *priv);

#endif
