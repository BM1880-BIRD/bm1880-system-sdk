#ifndef __CE_INST_H__
#define __CE_INST_H__

#include "ce_port.h"
#include "ce_types.h"

struct ce_inst {
	/* get from probe */
	struct platform_device *dev;
	void *reg;
	int irq;

	/* kernel resources */
	struct work_struct qwork;
	struct crypto_queue queue; /* request queue */
	spinlock_t lock;
	struct completion comp;

	/* status */
	int busy;
};

/* allocate and init an instance */
int ce_inst_new(struct ce_inst **inst, struct platform_device *dev);
void ce_inst_free(struct ce_inst *this);
int ce_inst_enqueue_request(struct ce_inst *this,
			    struct crypto_async_request *req);
void ce_inst_wait(struct ce_inst *this);

#endif
