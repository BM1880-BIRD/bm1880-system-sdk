#include <ce_inst.h>
#include <ce.h>
#include <ce_hash.h>
#include <ce_cipher.h>

#define CE_REQUEST_QUEUE_SIZE	(50)

static void qtask(struct work_struct *work)
{
	struct ce_inst *this = container_of(work, struct ce_inst, qwork);
	struct crypto_async_request *req, *backlog;
	unsigned long flag;

	spin_lock_irqsave(&this->lock, flag);
	backlog = crypto_get_backlog(&this->queue);
	req = crypto_dequeue_request(&this->queue);
	if (req == NULL) {
		this->busy = false; /* all requests have been done */
		spin_unlock_irqrestore(&this->lock, flag);
		return;
	}
	spin_unlock_irqrestore(&this->lock, flag);
	if (backlog)
		backlog->complete(backlog, -EINPROGRESS);

	/* get cra_type */
	switch (crypto_async_request_type(req)) {
	case CRYPTO_ALG_TYPE_ABLKCIPHER:
		ce_cipher_handle_request(this, req);
		break;
	case CRYPTO_ALG_TYPE_AHASH:
		ce_hash_handle_request(this, req);
		break;
	default:
		/* TODO: handle base64 -- compression */
		ce_err("unsupport crypto graphic algorithm\n");
		ce_assert(0);
	}
	schedule_work(&this->qwork);
}

static irqreturn_t __maybe_unused ce_irq(int irq, void *dev_id)
{
	struct ce_inst *this = dev_id;

	ce_int_clear(this->reg);
	complete(&this->comp);
	return IRQ_HANDLED;
}

void ce_inst_wait(struct ce_inst *this)
{
#ifndef CONFIG_CRYPTO_DEV_BCE_DUMMY
	wait_for_completion(&this->comp);
	reinit_completion(&this->comp);
#endif
}

/* allocate and init a instance */
int ce_inst_new(struct ce_inst **inst, struct platform_device *dev)
{
	struct ce_inst *this;
	struct resource *res;

	this = devm_kzalloc(&dev->dev, sizeof(*this), GFP_KERNEL);
	if (this == NULL)
		return -ENOMEM;

	/* get resources delcared in DT */
	res = platform_get_resource(dev, IORESOURCE_MEM, 0);
	this->reg = devm_ioremap_resource(&dev->dev, res);
	if (IS_ERR(this->reg)) {
		ce_err("map register failed\n");
		return -ENOMEM;
	}

#ifndef CONFIG_CRYPTO_DEV_BCE_DUMMY
	this->irq = platform_get_irq(dev, 0);
	if (this->irq < 0) {
		ce_err("no interrupt resource\n");
		return -EINVAL;
	}
	if (devm_request_irq(&dev->dev, this->irq, ce_irq, 0,
			     "bitmain crypto engine", this)) {
		ce_err("request irq failed");
		return -EBUSY;
	}
	/* disable system irq on init
	 * enable it before using
	 */
	disable_irq(this->irq);
#endif

	/* hardware init */
	ce_init(this->reg);

	this->dev = dev;

	INIT_WORK(&this->qwork, qtask);
	crypto_init_queue(&this->queue, CE_REQUEST_QUEUE_SIZE);
	spin_lock_init(&this->lock);
	init_completion(&this->comp);
	this->busy = false;
	platform_set_drvdata(dev, this);
	*inst = this;
	return 0;
}

void ce_inst_free(struct ce_inst *this)
{
	ce_destroy(this->reg);
}

int ce_inst_enqueue_request(struct ce_inst *this,
			 struct crypto_async_request *req)
{
	unsigned long flag;
	int err;

	spin_lock_irqsave(&this->lock, flag);
	err = crypto_enqueue_request(&this->queue, req);
	if (this->busy) {
		spin_unlock_irqrestore(&this->lock, flag);
		return err;
	}
	this->busy = true;
	spin_unlock_irqrestore(&this->lock, flag);
	schedule_work(&this->qwork);

	return err;
}

