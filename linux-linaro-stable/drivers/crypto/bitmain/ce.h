#ifndef __CE_H__
#define __CE_H__

enum {
	CE_EINVAL = 100,	/* invalid argument */
	CE_EALG,		/* invalid algrithm */
	CE_EBLKLEN,		/* not aligned to block length */
	CE_EKEYLEN,		/* key length not valid */
	CE_ESKEY,		/* secure key not valid, but requested */
	CE_ENULL,		/* null pointer */
};

//symmetric
enum {
	CE_DES_ECB = 0,
	CE_DES_CBC,
	CE_DES_CTR,
	CE_DES_EDE3_ECB,
	CE_DES_EDE3_CBC,
	CE_DES_EDE3_CTR,
	CE_AES_128_ECB,
	CE_AES_128_CBC,
	CE_AES_128_CTR,
	CE_AES_192_ECB,
	CE_AES_192_CBC,
	CE_AES_192_CTR,
	CE_AES_256_ECB,
	CE_AES_256_CBC,
	CE_AES_256_CTR,
	CE_SM4_ECB,
	CE_SM4_CBC,
	CE_SM4_CTR,
	CE_CIPHER_MAX,
};

//hash
enum {
	CE_SHA1,
	CE_SHA256,
	CE_HASH_MAX,
};

//base N
enum {
	CE_BASE64,
	CE_BASE_MAX,
};

struct ce_cipher {
	int             alg;
	//set 0 to decryption, otherwise encryption
	int		enc;
	void            *iv;
	void            *key;
	void            *src;
	void            *dst;
	unsigned long   len;
	void		*iv_out;
};

struct ce_hash {
	int			alg;
	void			*init;
	void			*src;
	unsigned long		len;
	void			*hash;
};

//Base N -- Base16 Base32 Base64
//Now only stdardard base64 is supported which defined in RFC4648 section 4
struct ce_base {
	int             alg;
	//set 0 to decryption, otherwise encryption
	int		enc;
	void            *src;
	void            *dst;
	unsigned long   len;
};

//ce is short for CryptoEngine
int ce_init(void *dev);
int ce_destroy(void *dev);
int ce_cipher_do(void *dev, struct ce_cipher *cipher);
int ce_save_iv(void *dev, struct ce_cipher *cipher);
int ce_save_hash(void *dev, struct ce_hash *hash);
int ce_hash_do(void *dev, struct ce_hash *hash);
int ce_base_do(void *dev, struct ce_base *base);
unsigned int ce_int_status(void *dev);
void ce_int_clear(void *dev);
int ce_status(void *dev);
int ce_is_secure_key_valid(void *dev);
int ce_cipher_block_len(unsigned int alg);
int ce_hash_hash_len(unsigned int alg);
int ce_hash_block_len(unsigned int alg);
unsigned long ce_base_len(struct ce_base *base);

#endif
