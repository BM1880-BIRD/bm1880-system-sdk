#include <ce_port.h>
#include <ce_types.h>
#include <ce.h>
#include <ce_dev.h>
#include <ce_inst.h>
#include <ce_soft_hash.h>

struct ce_ctx {
	int alg; /* hash algorithm */
	u8 hash[MAX_HASHLEN];
	/* count in bytes of data processed*/
	unsigned int count;
	/* data wait to process */
	u8 buf[MAX_HASH_BLOCKLEN];
	int final;
	int err;
};

static void __maybe_unused ctx_info(struct ce_ctx *this)
{
	ce_info("type %s\n", "hash");
	ce_info("err %d\n", this->err);
	ce_info("alg %d\n", this->alg);
	ce_info("hash");
	dumpp(this->hash, MAX_HASHLEN);
	ce_info("count (%u bytes)", this->count);
	dumpp(this->buf, this->count);
}

static inline void *ahash_request_tfmctx(struct ahash_request *req)
{
	return crypto_ahash_ctx(crypto_ahash_reqtfm(req));
}

static const u8 sha1_init_state[] = {
	0x67, 0x45, 0x23, 0x01,
	0xef, 0xcd, 0xab, 0x89,
	0x98, 0xba, 0xdc, 0xfe,
	0x10, 0x32, 0x54, 0x76,
	0xc3, 0xd2, 0xe1, 0xf0,
};

static const u8 sha256_init_state[] = {
	0x6a, 0x09, 0xe6, 0x67,
	0xbb, 0x67, 0xae, 0x85,
	0x3c, 0x6e, 0xf3, 0x72,
	0xa5, 0x4f, 0xf5, 0x3a,
	0x51, 0x0e, 0x52, 0x7f,
	0x9b, 0x05, 0x68, 0x8c,
	0x1f, 0x83, 0xd9, 0xab,
	0x5b, 0xe0, 0xcd, 0x19,
};

static const u8 *hash_init_state_table[] = {
	[CE_SHA1] = sha1_init_state,
	[CE_SHA256] = sha256_init_state,
};

static void ctx_transform(struct ce_ctx *ctx)
{
	u32 hash[MAX_HASHLEN / 4];
	unsigned long hashlen = ce_hash_hash_len(ctx->alg);
	int i;

	ce_assert(hashlen % 4 == 0);

	for (i = 0; i < hashlen / 4; ++i)
		hash[i] = be32_to_cpu(((u32 *)ctx->hash)[i]);

	if (ctx->alg == CE_SHA1)
		ce_sha1_transform(hash, ctx->buf);
	else
		ce_sha256_transform(hash, ctx->buf);

	for (i = 0; i < hashlen / 4; ++i)
		((u32 *)ctx->hash)[i] = cpu_to_be32(hash[i]);
}

static int ce_hash_process_single_sg(struct ce_inst *inst, struct ce_ctx *ctx,
				     struct ce_hash *cfg,
				     struct scatterlist *src)
{
	u8 *vaddr = sg_virt(src);
#ifndef CONFIG_CRYPTO_DEV_BCE_DUMMY
	unsigned long len = sg_dma_len(src);
	dma_addr_t paddr = sg_dma_address(src);
#else
	unsigned long len = src->length;
	u8 *paddr = vaddr;
#endif
	unsigned long blocklen = ce_hash_block_len(ctx->alg);
	unsigned int partial = ctx->count % blocklen;

	ctx->count += len;

	if (unlikely((partial + len) >= blocklen)) {
		int blocks;

		if (partial) {
			int p = blocklen - partial;

			memcpy(ctx->buf + partial, vaddr, p);
			vaddr += p;
			paddr += p;
			len -= p;

			ctx_transform(ctx);
		}

		blocks = len / blocklen;
		len %= blocklen;

		if (blocks) {
			cfg->src = (void *)paddr;
			cfg->len = blocks * blocklen;
			ctx->err = ce_hash_do(inst->reg, cfg);
			if (ctx->err) {
				ctx->err = -EINVAL;
				return -EINVAL;
			}
			ce_inst_wait(inst);
			vaddr += blocks * blocklen;
			paddr += blocks * blocklen;
		}
		partial = 0;
	}
	if (len)
		memcpy(ctx->buf + partial, vaddr, len);

	return 0;
}

void ce_hash_handle_request(struct ce_inst *inst,
			    struct crypto_async_request *req)
{
	struct ahash_request *hash_req;
	struct scatterlist *src;
	struct ce_hash cfg;
	struct ce_ctx *ctx;
	struct device *dev;

	hash_req = ahash_request_cast(req);
	ctx = ahash_request_tfmctx(hash_req);
	src = hash_req->src;
	dev = &inst->dev->dev;

	ce_assert(ctx->err == 0);

	if (hash_req->nbytes == 0) {
		ctx->err = 0;
		goto err_nop;
	}

#ifndef CONFIG_CRYPTO_DEV_BCE_DUMMY
	if (dma_map_sg(dev, hash_req->src, sg_nents(hash_req->src),
		       DMA_TO_DEVICE) == 0) {
		ce_err("map source scatter list failed\n");
		ctx->err = -ENOMEM;
		goto err_nop;
	}
#endif

	cfg.alg = ctx->alg;
	cfg.init = ctx->hash;
	cfg.hash = ctx->hash;

	ce_info("src %p\n", src);
	for (; src; src = sg_next(src)) {
		ce_info("src %p\n", src);
		if (ce_hash_process_single_sg(inst, ctx, &cfg, src))
			goto err_unmap;
	}

err_unmap:
#ifndef CONFIG_CRYPTO_DEV_BCE_DUMMY
	dma_unmap_sg(dev, hash_req->src, sg_nents(hash_req->src),
		     DMA_TO_DEVICE);
#endif
err_nop:
	req->complete(req, ctx->err);
}

/*****************************************************************************/

#undef HASH
#define HASH(hash)	CRA_##hash,
enum {
	#include <alg.def>
	CRA_HASH_MAX,
};
#undef HASH

struct ahash_alg_container {
	int registered;
	struct ahash_alg alg;
};

static const char * const hash_name_table[] = {
	[CE_SHA1] = "sha1",
	[CE_SHA256] = "sha256",
};

static int get_type_by_name(const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(hash_name_table); ++i)
		if (strcmp(name, hash_name_table[i]) == 0)
			break;
	ce_assert(i < CE_HASH_MAX);
	return i;
}

static int ce_ahash_init(struct ahash_request *req)
{
	struct ce_ctx *ctx = ahash_request_tfmctx(req);
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	const char *cra_name = crypto_ahash_alg_name(tfm);

	ctx->alg = get_type_by_name(cra_name);
	memcpy(ctx->hash, hash_init_state_table[ctx->alg],
	       ce_hash_hash_len(ctx->alg));
	ctx->count = 0;
	ctx->err = 0;
	return ctx->err;
}
static int ce_ahash_final(struct ahash_request *req)
{
	struct ce_ctx *ctx = ahash_request_tfmctx(req);
	unsigned int blocklen = ce_hash_block_len(ctx->alg);
	unsigned int hashlen = ce_hash_hash_len(ctx->alg);
	const int bit_offset = blocklen - sizeof(__be64);
	__be64 *bits = (__be64 *)(ctx->buf + bit_offset);
	unsigned int partial = ctx->count % blocklen;

	ce_assert(ctx->err == 0);

	ctx->buf[partial++] = 0x80;
	if (partial > bit_offset) {
		memset(ctx->buf + partial, 0x0, blocklen - partial);
		partial = 0;
		ctx_transform(ctx);
	}

	memset(ctx->buf + partial, 0x0, bit_offset - partial);
	*bits = cpu_to_be64(ctx->count << 3);
	ctx_transform(ctx);
	ce_assert(req->result);
	memcpy(req->result, ctx->hash, hashlen);
	return ctx->err;
}

static int init(struct ahash_request *req)
{
	return ce_ahash_init(req);
}

static int update(struct ahash_request *req)
{
	return ce_enqueue_request(&req->base);
}

static int final(struct ahash_request *req)
{
	return ce_ahash_final(req);
}

static int finup(struct ahash_request *req)
{
	struct ce_ctx *ctx = ahash_request_tfmctx(req);

	ctx->final = true;
	return ce_enqueue_request(&req->base);
}

static int digest(struct ahash_request *req)
{
	ce_ahash_init(req);
	return finup(req);
}

static int export(struct ahash_request *req, void *out)
{
	struct ce_ctx *ctx = ahash_request_tfmctx(req);
	unsigned int blocklen = ce_hash_block_len(ctx->alg);

	if (ctx->alg == CE_SHA1) {
		struct sha1_state *state = out;

		memcpy(state->state, ctx->hash, sizeof(state->state));
		state->count = ctx->count;
		memcpy(state->buffer, ctx->buf, state->count % blocklen);
	} else {
		struct sha256_state *state = out;

		memcpy(state->state, ctx->hash, sizeof(state->state));
		state->count = ctx->count;
		memcpy(state->buf, ctx->buf, state->count % blocklen);
	}
	return 0;
}

static int import(struct ahash_request *req, const void *in)
{
	struct ce_ctx *ctx = ahash_request_tfmctx(req);
	unsigned int blocklen = ce_hash_block_len(ctx->alg);

	if (ctx->alg == CE_SHA1) {
		const struct sha1_state *state = in;

		memcpy(ctx->hash, state->state, sizeof(state->state));
		ctx->count = state->count;
		memcpy(ctx->buf, state->buffer, state->count % blocklen);
	} else {
		const struct sha256_state *state = in;

		memcpy(ctx->hash, state->state, sizeof(state->state));
		ctx->count = state->count;
		memcpy(ctx->buf, state->buf, state->count % blocklen);
	}
	return 0;
}

#undef HASH
#define HASH(hash)							\
[CRA_##hash] = {							\
	.alg = {							\
		.init = init,						\
		.update = update,					\
		.final = final,						\
		.finup = finup,						\
		.digest = digest,					\
		.export = export,					\
		.import = import,					\
		.setkey = NULL,						\
		.halg = {						\
			.digestsize = hash##_DIGEST_SIZE,		\
			.statesize = sizeof(struct sha256_state),	\
			.base = {					\
				.cra_name = #hash,			\
				.cra_driver_name = #hash"-bitmain-ce",	\
				.cra_priority = CRA_PRIORITY,		\
				.cra_flags = CRYPTO_ALG_ASYNC |		\
					CRYPTO_ALG_TYPE_AHASH,		\
				.cra_blocksize = hash##_BLOCK_SIZE,	\
				.cra_ctxsize = sizeof(struct ce_ctx),	\
				.cra_module = THIS_MODULE,		\
				.cra_init = NULL,			\
			}						\
		}							\
	},								\
},

static struct ahash_alg_container algs[CRA_HASH_MAX] = {
	#include <alg.def>
};

#undef HASH

int unregister_all_hash(void)
{
	int i;
	struct ahash_alg_container *ctn;
	struct ahash_alg *alg;

	for (i = 0; i < ARRAY_SIZE(algs); ++i) {
		ctn = algs + i;
		alg = &ctn->alg;

		if (ctn->registered) {
			ce_info("unregistering algrithm %s : %s",
				alg->halg.base.cra_name,
				alg->halg.base.cra_driver_name);
			ce_assert(crypto_unregister_ahash(alg) == 0);
			ctn->registered = 0;
		}
	}
	return 0;
}

static void alg_2nd_init(struct crypto_alg *alg)
{
	strnlower(alg->cra_name, alg->cra_name, sizeof(alg->cra_name));
	strnlower(alg->cra_driver_name, alg->cra_driver_name,
		  sizeof(alg->cra_driver_name));
}

int register_all_hash(void)
{
	int i;
	struct ahash_alg_container *ctn;
	struct ahash_alg *alg;

	for (i = 0; i < ARRAY_SIZE(algs); ++i) {
		ctn = algs + i;
		alg = &ctn->alg;

		if (!ctn->registered) {
			alg_2nd_init(&(alg->halg.base));
			ce_info("registering algrithm %s : %s",
				alg->halg.base.cra_name,
				alg->halg.base.cra_driver_name);
			ce_assert(crypto_register_ahash(alg) == 0);
		}
	}
	return 0;
}

