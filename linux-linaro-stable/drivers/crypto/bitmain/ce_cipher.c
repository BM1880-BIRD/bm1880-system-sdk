/*
 * Testcase for crypto engine
 */
#include <ce.h>
#include <ce_port.h>
#include <ce_dev.h>
#include <ce_types.h>
#include <ce_inst.h>

struct ce_ctx {
	int alg, mode, keylen, enc;
	u8 iv[MAX_BLOCKLEN];
	u8 key[MAX_KEYLEN];
	int err;
};

static void __maybe_unused ctx_info(struct ce_ctx *this)
{
	ce_info("type %s\n", "cipher");
	ce_info("err %d\n", this->err);
	ce_info("alg %d\n", this->alg);
	ce_info("mode %d\n", this->mode);
	ce_info("keylen %d\n", this->keylen);
	ce_info("enc %d\n", this->enc);
	ce_info("iv\n");
	dumpp(this->iv, MAX_BLOCKLEN);
	ce_info("key\n");
	dumpp(this->key, MAX_KEYLEN);
}

static void ctx_set_key(struct ce_ctx *this, const void *key, int keylen)
{
	this->keylen = keylen;
	memcpy(this->key, key, keylen);
}

static void ctx_set_iv(struct ce_ctx *this, void *iv)
{
	int ivlen;

	ce_assert(this->alg >= 0 && this->alg < CE_ALG_MAX);

	if (this->mode == CE_ECB)
		return;

	switch (this->alg) {
	case CE_DES:
	case CE_DES_EDE3:
		ivlen = DES_BLOCKLEN;
		break;
	case CE_AES:
		ivlen = AES_BLOCKLEN;
		break;
	case CE_SM4:
		ivlen = SM4_BLOCKLEN;
		break;
	default:
		ce_err("unknown cipher %d\n", this->alg);
		ivlen = 0;
		break;
	}
	memcpy(this->iv, iv, ivlen);
}

enum {
	CE_AES128,
	CE_AES192,
	CE_AES256,
	CE_AESKEYLEN_MAX,
};

static const int cipher_index_des[CE_MODE_MAX] = {
	CE_DES_ECB, CE_DES_CBC, CE_DES_CTR,
};

static const int cipher_index_des_ede3[CE_MODE_MAX] = {
	CE_DES_EDE3_ECB, CE_DES_EDE3_CBC, CE_DES_EDE3_CTR,
};

static const int cipher_index_sm4[CE_MODE_MAX] = {
	CE_SM4_ECB, CE_SM4_CBC, CE_SM4_CTR,
};

static const int cipher_index_aes[CE_AESKEYLEN_MAX][CE_MODE_MAX] = {
	{ CE_AES_128_ECB, CE_AES_128_CBC, CE_AES_128_CTR, },
	{ CE_AES_192_ECB, CE_AES_192_CBC, CE_AES_192_CTR, },
	{ CE_AES_256_ECB, CE_AES_256_CBC, CE_AES_256_CTR, },
};

static int get_cipher_index(unsigned int cipher, unsigned int mode,
			    unsigned int keylen)
{
	switch (cipher) {
	case CE_DES:
		return cipher_index_des[mode];
	case CE_DES_EDE3:
		return cipher_index_des_ede3[mode];
	case CE_SM4:
		return cipher_index_sm4[mode];
	case CE_AES:
		switch (keylen) {
		case 128 / 8:
			keylen = CE_AES128;
			break;
		case 192 / 8:
			keylen = CE_AES192;
			break;
		case 256 / 8:
			keylen = CE_AES256;
			break;
		default:
			return -1;
		}
		return cipher_index_aes[keylen][mode];
	default:
		return -1;
	}
}

static void cipher_unmap_sg(struct device *dev,
			    struct scatterlist *src, struct scatterlist *dst)
{
#ifndef CONFIG_CRYPTO_DEV_BCE_DUMMY
	if (src == dst) {
		dma_unmap_sg(dev, src, sg_nents(src),
			     DMA_BIDIRECTIONAL);
	} else {
		dma_unmap_sg(dev, src, sg_nents(src), DMA_TO_DEVICE);
		dma_unmap_sg(dev, dst, sg_nents(dst), DMA_FROM_DEVICE);
	}
#endif
}

static int cipher_map_sg(struct device *dev,
			 struct scatterlist *src, struct scatterlist *dst)
{
#ifndef CONFIG_CRYPTO_DEV_BCE_DUMMY
	ce_assert(sg_nents(src) == sg_nents(dst));

	if (src == dst) {
		if (dma_map_sg(dev, src, sg_nents(src),
			       DMA_BIDIRECTIONAL) == 0) {
			ce_err("map scatter list(in place) failed\n");
			goto err_nop;
		}
	} else {
		if (dma_map_sg(dev, src, sg_nents(src),
			       DMA_TO_DEVICE) == 0) {
			ce_err("map source scatter list failed\n");
			goto err_nop;
		}
		if (dma_map_sg(dev, dst, sg_nents(dst),
			       DMA_FROM_DEVICE) == 0) {
			ce_err("map destination scatter list failed\n");
			goto err_unmap_src;
		}
	}
	return 0;
err_unmap_src:
	dma_unmap_sg(dev, src, sg_nents(src), DMA_TO_DEVICE);
err_nop:
	return -ENOMEM;
#else
	return 0;
#endif
}

void ce_cipher_handle_request(struct ce_inst *inst,
			      struct crypto_async_request *req)
{
	struct ablkcipher_request *cipher_req;
	struct ablkcipher_walk walk;
	struct ce_cipher cfg;
	struct ce_ctx *ctx;
	unsigned long nbytes;
	unsigned long src, dst, len;
	int blocklen;

	cipher_req = ablkcipher_request_cast(req);
	ctx = crypto_ablkcipher_ctx(crypto_ablkcipher_reqtfm(cipher_req));
	ctx->err = cipher_map_sg(&inst->dev->dev,
				 cipher_req->src, cipher_req->dst);
	if (ctx->err)
		goto err;

	cfg.alg = get_cipher_index(ctx->alg, ctx->mode,
				    ctx->keylen);
	blocklen = ce_cipher_block_len(cfg.alg);
	ablkcipher_walk_init(&walk, cipher_req->dst, cipher_req->src,
			     cipher_req->nbytes);
	ctx->err = ablkcipher_walk_phys(cipher_req, &walk);
	if (ctx->err) {
		ce_err("ablkcipher_walk_phys failed with %d\n", ctx->err);
		goto err_unmap;
	}

	cfg.enc = ctx->enc;
	cfg.iv = ctx->iv;
	cfg.key = ctx->key;
	cfg.iv_out = cfg.iv; //save iv in place

	while ((nbytes = walk.nbytes) > 0) {
		/* TODO: physical address to device address(dma address) */
#ifdef CONFIG_CRYPTO_DEV_BCE_DUMMY
		src = ((unsigned long)page_to_virt(walk.src.page) +
		       walk.src.offset);
		dst = ((unsigned long)page_to_virt(walk.dst.page) +
		       walk.dst.offset);
#else
		src = (page_to_phys(walk.src.page) + walk.src.offset);
		dst = (page_to_phys(walk.dst.page) + walk.dst.offset);
#endif
		len = nbytes - (nbytes % blocklen);
		cfg.src = (void *)src;
		cfg.dst = (void *)dst;
		cfg.len = len;
		ctx->err = ce_cipher_do(inst->reg, &cfg);
		if (ctx->err) {
			ce_err("hardware configuration failed\n");
			ctx->err = -EINVAL;
			goto err_unmap;
		}
		ce_inst_wait(inst);
		if (ctx->mode != CE_ECB)
			ce_save_iv(inst->reg, &cfg);
		nbytes -= len;
		ctx->err = ablkcipher_walk_done(cipher_req, &walk, nbytes);
		if (ctx->err)
			goto err_unmap;
	}

	ctx->err = 0;
err_unmap:
	cipher_unmap_sg(&inst->dev->dev, cipher_req->src, cipher_req->dst);
err:
	req->complete(req, ctx->err);
}

/*****************************************************************************/

#undef ALG
#define ALG(mode, cipher)	CRA_##mode##_##cipher,
enum {
	#include <alg.def>
	CRA_ALG_MAX,
};
#undef ALG

struct crypto_alg_container {
	int type, cipher, mode, registered;
	struct crypto_alg alg;
};

static int cipher_op(struct ablkcipher_request *req, int enc)
{
	struct crypto_ablkcipher *tfm = crypto_ablkcipher_reqtfm(req);
	struct ce_ctx *ctx = crypto_ablkcipher_ctx(tfm);

	ctx->enc = enc;
	ctx_set_iv(ctx, req->info);
	return ce_enqueue_request(&req->base);
}

static int encrypt(struct ablkcipher_request *req)
{
	return cipher_op(req, true);
}

static int decrypt(struct ablkcipher_request *req)
{
	return cipher_op(req, false);
}

static int setkey(struct crypto_ablkcipher *tfm,
		  const u8 *key, unsigned int keylen)
{
	struct ce_ctx *ctx = crypto_ablkcipher_ctx(tfm);

	ctx_set_key(ctx, key, keylen);
	return 0;
}

static int init(struct crypto_tfm *tfm)
{
	struct ce_ctx *ctx = crypto_tfm_ctx(tfm);
	struct crypto_alg *alg = tfm->__crt_alg;
	struct crypto_alg_container *ctn = container_of(alg,
					struct crypto_alg_container, alg);

	ctx->alg = ctn->cipher;
	ctx->mode = ctn->mode;
	return 0;
}

#undef ALG
#define ALG(xmode, xcipher)						\
[CRA_##xmode##_##xcipher] = {						\
	.cipher = CE_##xcipher,						\
	.mode = CE_##xmode,						\
	.registered = 0,						\
	.alg = {							\
		.cra_name = #xmode"("#xcipher")",			\
		.cra_driver_name = #xmode"("#xcipher")-bitmain-ce",	\
		.cra_priority = CRA_PRIORITY,				\
		.cra_flags = CRYPTO_ALG_TYPE_ABLKCIPHER,		\
		.cra_blocksize = xcipher##_BLOCKLEN,			\
		.cra_ctxsize = sizeof(struct ce_ctx),			\
		.cra_type = &crypto_ablkcipher_type,			\
		.cra_init = init,					\
		.cra_module = THIS_MODULE,				\
		.cra_u = {						\
			.ablkcipher = {					\
				.min_keysize =				\
					xcipher##_MIN_KEYLEN,		\
				.max_keysize =				\
					xcipher##_MAX_KEYLEN,		\
				.setkey = setkey,			\
				.encrypt = encrypt,			\
				.decrypt = decrypt,			\
			},						\
		},							\
	},								\
},

static struct crypto_alg_container algs[CRA_ALG_MAX] = {
	#include <alg.def>
};
#undef ALG

int unregister_all_alg(void)
{
	int i;
	struct crypto_alg_container *ctn;
	struct crypto_alg *alg;

	for (i = 0; i < ARRAY_SIZE(algs); ++i) {
		ctn = algs + i;
		alg = &ctn->alg;

		if (ctn->registered) {
			ce_info("unregistering algrithm %s : %s",
				alg->cra_name, alg->cra_driver_name);
			ce_assert(crypto_unregister_alg(alg) == 0);
			ctn->registered = 0;
		}
	}
	return 0;
}

static void alg_2nd_init(struct crypto_alg *alg)
{
	struct crypto_alg_container *ctn = container_of(alg,
					struct crypto_alg_container,
					alg);

	strnlower(alg->cra_name, alg->cra_name, sizeof(alg->cra_name));
	strnlower(alg->cra_driver_name, alg->cra_driver_name,
		  sizeof(alg->cra_driver_name));
	if (ctn->mode != CE_ECB)
		alg->cra_u.ablkcipher.ivsize = alg->cra_blocksize;
}

int register_all_alg(void)
{
	int i;
	struct crypto_alg_container *ctn;
	struct crypto_alg *alg;

	for (i = 0; i < ARRAY_SIZE(algs); ++i) {
		ctn = algs + i;
		alg = &ctn->alg;

		if (!ctn->registered) {
			alg_2nd_init(alg);
			ce_info("registering algrithm %s : %s",
				alg->cra_name, alg->cra_driver_name);
			ce_assert(crypto_register_alg(alg) == 0);
			ctn->registered = 1;
		}
	}
	return 0;
}
