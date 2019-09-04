#include <ce.h>
#include <ce_port.h>
#include <linux/delay.h>
#include <crypto/aes.h>
#include <ce_types.h>
#include <ce_soft_hash.h>

struct ce_desc {
	ce_u32_t     ctrl;
	ce_u32_t     alg;
	ce_u64_t     next;
	ce_u64_t     src;
	ce_u64_t     dst;
	ce_u64_t     len;
	union {
		ce_u8_t      key[32];
		ce_u64_t     dstlen; //destination amount, only used in base64
	};
	ce_u8_t      iv[16];
};

struct ce_reg {
	ce_u32_t ctrl; //control
	ce_u32_t intr_enable; //interrupt mask
	ce_u32_t desc_low; //descriptor base low 32bits
	ce_u32_t desc_high; //descriptor base high 32bits
	ce_u32_t intr_raw; //interrupt raw, write 1 to clear
	ce_u32_t se_key_valid; //secure key valid, read only
	ce_u32_t cur_desc_low; //current reading descriptor
	ce_u32_t cur_desc_high; //current reading descriptor
	ce_u32_t r1[24]; //reserved
	ce_u32_t desc[22]; //PIO mode descriptor register
	ce_u32_t r2[10]; //reserved
	ce_u32_t key[24];		//keys
	ce_u32_t r3[8]; //reserved
	ce_u32_t iv[12]; //iv
	ce_u32_t r4[4]; //reserved
	ce_u32_t sha_param[8]; //sha parameter
};

struct cipher_info {
	ce_u32_t	ctrl;
	ce_u32_t	alg;
	unsigned int	block_len;
	unsigned int	key_len;
};

struct cipher_info cipher_info[] = {
	[CE_DES_ECB] = {
		.ctrl = 0x01880407,
		.alg = 0x00000000,
		.block_len = 8,
		.key_len = 8,
	},
	[CE_DES_CBC] = {
		.ctrl = 0x01880407,
		.alg = 0x00000002,
		.block_len = 8,
		.key_len = 8,
	},
	[CE_DES_CTR] = {
		.ctrl = 0x01880407,
		.alg = 0x00000004,
		.block_len = 8,
		.key_len = 8,
	},
	[CE_DES_EDE3_ECB] = {
		.ctrl = 0x01880407,
		.alg = 0x00000008,
		.block_len = 8,
		.key_len = 24,
	},
	[CE_DES_EDE3_CBC] = {
		.ctrl = 0x01880407,
		.alg = 0x0000000a,
		.block_len = 8,
		.key_len = 24,
	},
	[CE_DES_EDE3_CTR] = {
		.ctrl = 0x01880407,
		.alg = 0x0000000c,
		.block_len = 8,
		.key_len = 24,
	},
	[CE_AES_128_ECB] = {
		.ctrl = 0x01880207,
		.alg = 0x00000020,
		.block_len = 16,
		.key_len = 16,
	},
	[CE_AES_128_CBC] = {
		.ctrl = 0x01880207,
		.alg = 0x00000022,
		.block_len = 16,
		.key_len = 16,
	},
	[CE_AES_128_CTR] = {
		.ctrl = 0x01880207,
		.alg = 0x00000024,
		.block_len = 16,
		.key_len = 16,
	},
	[CE_AES_192_ECB] = {
		.ctrl = 0x01880207,
		.alg = 0x00000010,
		.block_len = 16,
		.key_len = 24,
	},
	[CE_AES_192_CBC] = {
		.ctrl = 0x01880207,
		.alg = 0x00000012,
		.block_len = 16,
		.key_len = 24,
	},
	[CE_AES_192_CTR] = {
		.ctrl = 0x01880207,
		.alg = 0x00000014,
		.block_len = 16,
		.key_len = 24,
	},
	[CE_AES_256_ECB] = {
		.ctrl = 0x01880207,
		.alg = 0x00000008,
		.block_len = 16,
		.key_len = 32,
	},
	[CE_AES_256_CBC] = {
		.ctrl = 0x01880207,
		.alg = 0x0000000a,
		.block_len = 16,
		.key_len = 32,
	},
	[CE_AES_256_CTR] = {
		.ctrl = 0x01880207,
		.alg = 0x0000000c,
		.block_len = 16,
		.key_len = 32,
	},
	[CE_SM4_ECB] = {
		.ctrl = 0x01880807,
		.alg = 0x00000000,
		.block_len = 16,
		.key_len = 16,
	},
	[CE_SM4_CBC] = {
		.ctrl = 0x01880807,
		.alg = 0x00000002,
		.block_len = 16,
		.key_len = 16,
	},
	[CE_SM4_CTR] = {
		.ctrl = 0x01880807,
		.alg = 0x00000004,
		.block_len = 16,
		.key_len = 16,
	},
};

struct hash_info {
	ce_u32_t	ctrl;
	ce_u32_t	alg;
	unsigned int	block_len;
	unsigned int	hash_len;
};

struct hash_info hash_info[] = {
	[CE_SHA1] = {
		.ctrl = 0x01881007,
		.alg = 0x00000001,
		.block_len = 64,
		.hash_len = 20,
	},
	[CE_SHA256] = {
		.ctrl = 0x01881007,
		.alg = 0x00000003,
		.block_len = 64,
		.hash_len = 32,
	},
};

struct base_info {
	ce_u32_t	ctrl;
	unsigned int	data_align;
	unsigned int	encoded_align;
};

struct base_info base_info[] = {
	[CE_BASE64] = {
		.ctrl		= 0x01882007,
		.data_align	= 3,
		.encoded_align	= 4,
	},
};

int ce_init(void *dev)
{
	enter();
	return 0;
}

int ce_destroy(void *dev)
{
	enter();
	return 0;
}

int ce_cipher_block_len(unsigned int alg)
{
	if (alg >= CE_CIPHER_MAX) {
		ce_err("unknown cipher algrithm [%d]\n", alg);
		return -1;
	}
	return cipher_info[alg].block_len;
}

int ce_cipher_intr(void *dev)
{
	enter();
	return 1;
}

static char *cipher_name[] = {
	"des-ecb",
	"des-cbc",
	"des-ctr",
	"des-ede3-ecb",
	"des-ede3-cbc",
	"des-ede3-ctr",
	"aes-128-ecb",
	"aes-128-cbc",
	"aes-128-ctr",
	"aes-192-ecb",
	"aes-192-cbc",
	"aes-192-ctr",
	"aes-256-ecb",
	"aes-256-cbc",
	"aes-256-ctr",
	"sm4-ecb",
	"sm4-cbc",
	"sm4-ctr",
	"invalid-cipher",
};

static char *ce_cipher_name(unsigned int alg)
{
	unsigned int index;

	index = alg > CE_CIPHER_MAX ? CE_CIPHER_MAX : alg;
	return cipher_name[index];
}

static char *hash_name[] = {
	"sha1",
	"sha256",
	"invalid-hash",
};

static char *ce_hash_name(unsigned int alg)
{
	unsigned int index;

	index = alg > CE_HASH_MAX ? CE_HASH_MAX : alg;
	return hash_name[index];
}

static char *base_name[] = {
	"base64",
	"invalid-base",
};

static char *ce_base_name(unsigned int alg)
{
	unsigned int index;

	index = alg > CE_BASE_MAX ? CE_BASE_MAX : alg;
	return base_name[index];
}


/****************************aes calculation**********************************/
static inline u8 byte(const u32 x, const unsigned int n)
{
	return x >> (n << 3);
}

static const u32 rco_tab[10] = { 1, 2, 4, 8, 16, 32, 64, 128, 27, 54 };

/* initialise the key schedule from the user supplied key */

#define star_x(x) ((((x) & 0x7f7f7f7f) << 1) ^ \
		   ((((x) & 0x80808080) >> 7) * 0x1b))

#define imix_col(y, x)	do {		\
	u	= star_x(x);		\
	v	= star_x(u);		\
	w	= star_x(v);		\
	t	= w ^ (x);		\
	(y)	= u ^ v ^ w;		\
	(y)	^= ror32(u ^ t, 8) ^	\
		ror32(v ^ t, 16) ^	\
		ror32(t, 24);		\
} while (0)

#define ls_box(x)		\
	(crypto_fl_tab[0][byte(x, 0)] ^	\
	crypto_fl_tab[1][byte(x, 1)] ^	\
	crypto_fl_tab[2][byte(x, 2)] ^	\
	crypto_fl_tab[3][byte(x, 3)])

#define loop4(i)	do {		\
	t = ror32(t, 8);		\
	t = ls_box(t) ^ rco_tab[i];	\
	t ^= ctx->key_enc[4 * i];		\
	ctx->key_enc[4 * i + 4] = t;		\
	t ^= ctx->key_enc[4 * i + 1];		\
	ctx->key_enc[4 * i + 5] = t;		\
	t ^= ctx->key_enc[4 * i + 2];		\
	ctx->key_enc[4 * i + 6] = t;		\
	t ^= ctx->key_enc[4 * i + 3];		\
	ctx->key_enc[4 * i + 7] = t;		\
} while (0)

#define loop6(i)	do {		\
	t = ror32(t, 8);		\
	t = ls_box(t) ^ rco_tab[i];	\
	t ^= ctx->key_enc[6 * i];		\
	ctx->key_enc[6 * i + 6] = t;		\
	t ^= ctx->key_enc[6 * i + 1];		\
	ctx->key_enc[6 * i + 7] = t;		\
	t ^= ctx->key_enc[6 * i + 2];		\
	ctx->key_enc[6 * i + 8] = t;		\
	t ^= ctx->key_enc[6 * i + 3];		\
	ctx->key_enc[6 * i + 9] = t;		\
	t ^= ctx->key_enc[6 * i + 4];		\
	ctx->key_enc[6 * i + 10] = t;		\
	t ^= ctx->key_enc[6 * i + 5];		\
	ctx->key_enc[6 * i + 11] = t;		\
} while (0)

#define loop8tophalf(i)	do {			\
	t = ror32(t, 8);			\
	t = ls_box(t) ^ rco_tab[i];		\
	t ^= ctx->key_enc[8 * i];			\
	ctx->key_enc[8 * i + 8] = t;			\
	t ^= ctx->key_enc[8 * i + 1];			\
	ctx->key_enc[8 * i + 9] = t;			\
	t ^= ctx->key_enc[8 * i + 2];			\
	ctx->key_enc[8 * i + 10] = t;			\
	t ^= ctx->key_enc[8 * i + 3];			\
	ctx->key_enc[8 * i + 11] = t;			\
} while (0)

#define loop8(i)	do {				\
	loop8tophalf(i);				\
	t  = ctx->key_enc[8 * i + 4] ^ ls_box(t);	\
	ctx->key_enc[8 * i + 12] = t;			\
	t ^= ctx->key_enc[8 * i + 5];			\
	ctx->key_enc[8 * i + 13] = t;			\
	t ^= ctx->key_enc[8 * i + 6];			\
	ctx->key_enc[8 * i + 14] = t;			\
	t ^= ctx->key_enc[8 * i + 7];			\
	ctx->key_enc[8 * i + 15] = t;			\
} while (0)

/* encrypt a block of text */

#define f_rn(bo, bi, n, k) { \
	bo[n] = crypto_ft_tab[0][byte(bi[n], 0)] ^			\
		crypto_ft_tab[1][byte(bi[(n + 1) & 3], 1)] ^		\
		crypto_ft_tab[2][byte(bi[(n + 2) & 3], 2)] ^		\
		crypto_ft_tab[3][byte(bi[(n + 3) & 3], 3)] ^ *(k + n);	\
	}

#define f_nround(bo, bi, k)	do {\
	f_rn(bo, bi, 0, k);	\
	f_rn(bo, bi, 1, k);	\
	f_rn(bo, bi, 2, k);	\
	f_rn(bo, bi, 3, k);	\
	k += 4;			\
} while (0)

#define f_rl(bo, bi, n, k)	{				\
	bo[n] = crypto_fl_tab[0][byte(bi[n], 0)] ^			\
		crypto_fl_tab[1][byte(bi[(n + 1) & 3], 1)] ^		\
		crypto_fl_tab[2][byte(bi[(n + 2) & 3], 2)] ^		\
		crypto_fl_tab[3][byte(bi[(n + 3) & 3], 3)] ^ *(k + n);	\
	}

#define f_lround(bo, bi, k)	do {\
	f_rl(bo, bi, 0, k);	\
	f_rl(bo, bi, 1, k);	\
	f_rl(bo, bi, 2, k);	\
	f_rl(bo, bi, 3, k);	\
} while (0)

static void aes_encrypt(struct crypto_aes_ctx *ctx, u8 *out, const u8 *in)
{
	const __le32 *src = (const __le32 *)in;
	__le32 *dst = (__le32 *)out;
	u32 b0[4], b1[4];
	const u32 *kp = ctx->key_enc + 4;
	const int key_len = ctx->key_length;

	b0[0] = le32_to_cpu(src[0]) ^ ctx->key_enc[0];
	b0[1] = le32_to_cpu(src[1]) ^ ctx->key_enc[1];
	b0[2] = le32_to_cpu(src[2]) ^ ctx->key_enc[2];
	b0[3] = le32_to_cpu(src[3]) ^ ctx->key_enc[3];

	if (key_len > 24) {
		f_nround(b1, b0, kp);
		f_nround(b0, b1, kp);
	}

	if (key_len > 16) {
		f_nround(b1, b0, kp);
		f_nround(b0, b1, kp);
	}

	f_nround(b1, b0, kp);
	f_nround(b0, b1, kp);
	f_nround(b1, b0, kp);
	f_nround(b0, b1, kp);
	f_nround(b1, b0, kp);
	f_nround(b0, b1, kp);
	f_nround(b1, b0, kp);
	f_nround(b0, b1, kp);
	f_nround(b1, b0, kp);
	f_lround(b0, b1, kp);

	dst[0] = cpu_to_le32(b0[0]);
	dst[1] = cpu_to_le32(b0[1]);
	dst[2] = cpu_to_le32(b0[2]);
	dst[3] = cpu_to_le32(b0[3]);
}

/* decrypt a block of text */

#define i_rn(bo, bi, n, k)	{				\
	bo[n] = crypto_it_tab[0][byte(bi[n], 0)] ^			\
		crypto_it_tab[1][byte(bi[(n + 3) & 3], 1)] ^		\
		crypto_it_tab[2][byte(bi[(n + 2) & 3], 2)] ^		\
		crypto_it_tab[3][byte(bi[(n + 1) & 3], 3)] ^ *(k + n);	\
	}

#define i_nround(bo, bi, k)	do {\
	i_rn(bo, bi, 0, k);	\
	i_rn(bo, bi, 1, k);	\
	i_rn(bo, bi, 2, k);	\
	i_rn(bo, bi, 3, k);	\
	k += 4;			\
} while (0)

#define i_rl(bo, bi, n, k)	{			\
	bo[n] = crypto_il_tab[0][byte(bi[n], 0)] ^			\
		crypto_il_tab[1][byte(bi[(n + 3) & 3], 1)] ^		\
		crypto_il_tab[2][byte(bi[(n + 2) & 3], 2)] ^		\
		crypto_il_tab[3][byte(bi[(n + 1) & 3], 3)] ^ *(k + n);	\
	}

#define i_lround(bo, bi, k)	do {\
	i_rl(bo, bi, 0, k);	\
	i_rl(bo, bi, 1, k);	\
	i_rl(bo, bi, 2, k);	\
	i_rl(bo, bi, 3, k);	\
} while (0)

static void aes_decrypt(struct crypto_aes_ctx *ctx, u8 *out, const u8 *in)
{
	const __le32 *src = (const __le32 *)in;
	__le32 *dst = (__le32 *)out;
	u32 b0[4], b1[4];
	const int key_len = ctx->key_length;
	const u32 *kp = ctx->key_dec + 4;

	b0[0] = le32_to_cpu(src[0]) ^  ctx->key_dec[0];
	b0[1] = le32_to_cpu(src[1]) ^  ctx->key_dec[1];
	b0[2] = le32_to_cpu(src[2]) ^  ctx->key_dec[2];
	b0[3] = le32_to_cpu(src[3]) ^  ctx->key_dec[3];

	if (key_len > 24) {
		i_nround(b1, b0, kp);
		i_nround(b0, b1, kp);
	}

	if (key_len > 16) {
		i_nround(b1, b0, kp);
		i_nround(b0, b1, kp);
	}

	i_nround(b1, b0, kp);
	i_nround(b0, b1, kp);
	i_nround(b1, b0, kp);
	i_nround(b0, b1, kp);
	i_nround(b1, b0, kp);
	i_nround(b0, b1, kp);
	i_nround(b1, b0, kp);
	i_nround(b0, b1, kp);
	i_nround(b1, b0, kp);
	i_lround(b0, b1, kp);

	dst[0] = cpu_to_le32(b0[0]);
	dst[1] = cpu_to_le32(b0[1]);
	dst[2] = cpu_to_le32(b0[2]);
	dst[3] = cpu_to_le32(b0[3]);
}
int ce_cipher_do(void *dev, struct ce_cipher *cipher)
{
	struct cipher_info *info;
	unsigned int keylen, blocklen;
	struct crypto_aes_ctx ctx;
	unsigned long i;
	unsigned char *pt, *ct;
	unsigned char iv[AES_BLOCK_SIZE];

	enter();
	if (cipher->alg >= CE_CIPHER_MAX) {
		ce_err("unknown cipher algrithm [%d]\n", cipher->alg);
		return -1;
	}

	info = cipher_info + cipher->alg;
	keylen = info->key_len;
	blocklen = info->block_len;

	ce_info("%s %s key length %d block length %d\n",
		ce_cipher_name(cipher->alg), cipher->enc ? "enc" : "dec",
		keylen, blocklen);
	ce_info("key\n");
	dumpp(cipher->key, keylen);
	ce_info("iv\n");
	dumpp(cipher->iv, blocklen);
	ce_info("src %p, dst %p, len %lu\n", cipher->src, cipher->dst,
		cipher->len);

	if (cipher->iv)
		memcpy(iv, cipher->iv, blocklen);
	if (cipher->enc) {
		pt = cipher->src;
		ct = cipher->dst;
	} else {
		pt = cipher->dst;
		ct = cipher->src;
	}

	ce_info("pt %p ct %p\n", pt, ct);

	switch (cipher->alg) {
	case CE_AES_128_ECB:
	case CE_AES_192_ECB:
	case CE_AES_256_ECB:
	case CE_AES_128_CBC:
	case CE_AES_192_CBC:
	case CE_AES_256_CBC:
	case CE_AES_128_CTR:
	case CE_AES_192_CTR:
	case CE_AES_256_CTR:
		crypto_aes_expand_key(&ctx, cipher->key, keylen);
		break;
	default:
		ce_info("software function not support, cipher algorthm\n");
	}

	switch (cipher->alg) {
	case CE_AES_128_ECB:
	case CE_AES_192_ECB:
	case CE_AES_256_ECB:
		for (i = 0; i < cipher->len;
		     i += blocklen, pt += blocklen, ct += blocklen) {
			if (cipher->enc)
				aes_encrypt(&ctx, ct, pt);
			else
				aes_decrypt(&ctx, pt, ct);
		}
		break;
	case CE_AES_128_CBC:
	case CE_AES_192_CBC:
	case CE_AES_256_CBC:
		for (i = 0; i < cipher->len;
		     i += blocklen, pt += blocklen, ct += blocklen) {
			if (cipher->enc) {
				crypto_xor(iv, pt, blocklen);
				aes_encrypt(&ctx, ct, iv);
				memcpy(iv, ct, blocklen);
			} else {
				aes_decrypt(&ctx, pt, ct);
				crypto_xor(pt, iv, blocklen);
				memcpy(iv, ct, blocklen);
			}
		}
		break;
	case CE_AES_128_CTR:
	case CE_AES_192_CTR:
	case CE_AES_256_CTR:
		ct = cipher->dst;
		pt = cipher->src;
		for (i = 0; i < cipher->len;
		     i += blocklen, pt += blocklen, ct += blocklen) {
			aes_encrypt(&ctx, ct, iv);
			crypto_xor(ct, pt, blocklen);
			crypto_inc(iv, blocklen);
		}
		break;
	default:
		ce_info("software function not support, chain mode\n");
	}

	return 0;
}

int ce_save_iv(void *dev, struct ce_cipher *cipher)
{
	struct cipher_info *info;
	unsigned int keylen, blocklen;
	unsigned long *tmp;

	if (cipher->alg >= CE_CIPHER_MAX) {
		ce_err("unknown cipher algrithm [%d]\n", cipher->alg);
		return -1;
	}

	info = cipher_info + cipher->alg;
	keylen = info->key_len;
	blocklen = info->block_len;

	tmp = (unsigned long *)cipher->iv_out;
	memset(cipher->iv_out, 0x00, blocklen);
	++(*tmp);
	return 0;
}

int ce_hash_do(void *dev, struct ce_hash *hash)
{
	struct hash_info const *info;
	u32 tmp_hash[MAX_HASHLEN / 4];
	unsigned long hashlen;
	int i, j, blocknum;
	u8 *src;

	if (hash->alg >= CE_HASH_MAX) {
		ce_err("unknown hash [%d]\n", hash->alg);
		return -CE_EALG;
	}

	info = hash_info + hash->alg;

	if (hash->len % info->block_len) {
		ce_err("only support aligned length and no padding\n");
		return -CE_EBLKLEN;
	}
	if (hash->init == NULL) {
		ce_err("no standard initialize parameter support\n");
		ce_err("init yourself\n");
		return -CE_EINVAL;
	}
	if (hash->hash == NULL) {
		ce_err("no output buffer\n");
		return -CE_ENULL;
	}

	ce_info("hash %s data length %lu\n", ce_hash_name(hash->alg),
		hash->len);
	ce_info("init parameter\n");
	dumpp(hash->init, info->hash_len);
	ce_info("physicall address %p\n", hash->src);

	hashlen = info->hash_len;
	ce_assert(hashlen % 4 == 0);
	blocknum = hash->len / info->block_len;

	for (i = 0; i < hashlen / 4; ++i)
		tmp_hash[i] = be32_to_cpu(((u32 *)hash->init)[i]);

	for (j = 0, src = hash->src;
	     j < blocknum; ++j, src += info->block_len) {
		if (hash->alg == CE_SHA1)
			ce_sha1_transform(tmp_hash, src);
		else
			ce_sha256_transform(tmp_hash, src);
	}

	for (i = 0; i < hashlen / 4; ++i)
		((u32 *)hash->hash)[i] = cpu_to_be32(tmp_hash[i]);

	return 0;
}

int ce_hash_hash_len(unsigned int alg)
{
	if (alg >= CE_HASH_MAX) {
		ce_err("unknown hash algorithm\n");
		return -1;
	}
	return hash_info[alg].hash_len;
}

int ce_hash_block_len(unsigned int alg)
{
	if (alg >= CE_HASH_MAX) {
		ce_err("unknown hash algorithm\n");
		return -1;
	}
	return hash_info[alg].block_len;
}

int ce_base_do(void *dev, struct ce_base *base)
{
	return 0;
}

void ce_int_clear(void *dev)
{
}

int ce_save_hash(void *dev, struct ce_hash *hash)
{
	return 0;
}


