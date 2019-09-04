#ifndef __CE_PORT_H__
#define __CE_PORT_H__


#define pr_fmt(fmt)	"ce:"fmt

#include <linux/types.h>
#include <linux/ctype.h>
#include <linux/crypto.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <crypto/algapi.h>
#include <crypto/hash.h>
#include <crypto/sha.h>
#include <crypto/internal/hash.h>
#include <linux/semaphore.h>
#include <linux/compiler.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/compiler.h>
#include <asm-generic/bug.h>

#define ce_err	pr_err
#define ce_warn	pr_warn

/* #define CE_DBG */

#ifdef CE_DBG
#define ce_cont	pr_cont
#define ce_info pr_info
#define ce_dbg	pr_info
#define enter()	ce_info("enter %s\n", __func__)
#define line()	ce_info("%s %d\n", __FILE__, __LINE__)
#else
#define ce_cont(...)	do {} while (0)
#define ce_info(...)	do {} while (0)
#define ce_dbg(...)	do {} while (0)
#define enter(...)	do {} while (0)
#define line(...)	do {} while (0)
#endif

#define ce_assert(cond)	WARN_ON(!(cond))

typedef u32	ce_u32_t;
typedef u64	ce_u64_t;
typedef u8	ce_u8_t;

#define ce_readl(reg)		readl(&(reg))
#define ce_writel(reg, value)	writel(value, &(reg))
#define ce_setbit(reg, bit)	ce_writel(reg, ce_readl(reg) | (1 << bit))
#define ce_clrbit(reg, bit)	ce_writel(reg, ce_readl(reg) & ~(1 << bit))
#define ce_getbit(reg, bit)	((ce_readl(reg) >> bit) & 1)

static inline void dumpp(const void *p, const unsigned long l)
{
	unsigned long i;
	char buf[128];
	unsigned int count;

	if (p == NULL) {
		ce_info("null\n");
		return;
	}
	count = 0;
	for (i = 0; i < l; ++i) {
		count += snprintf(buf + count, sizeof(buf) - count,
				   "%02x ", ((unsigned char *)p)[i]);
		if (count % 64 == 0) {
			ce_info("%s\n", buf);
			count = 0;
		}
	}
	if (count)
		ce_info("%s\n", buf);
}

static inline void dump(const void *p, const unsigned long l)
{
	unsigned long i;
	char buf[128];
	unsigned int count;

	if (p == NULL) {
		ce_info("null\n");
		return;
	}
	count = 0;
	for (i = 0; i < l; ++i) {
		if (i % 16 == 0)
			count += snprintf(buf + count,
					   sizeof(buf) - count, "%04lx: ", i);

		count += snprintf(buf + count, sizeof(buf) - count,
				   "%02x ", ((unsigned char *)p)[i]);

		if (i % 16 == 15) {
			ce_info("%s\n", buf);
			count = 0;
		}
	}
	if (i % 16 != 15)
		ce_info("%s\n", buf);
}

static inline void *strnlower(char *dst, const char *src, unsigned int n)
{
	int i;

	for (i = 0; i < n && src[i]; ++i)
		dst[i] = tolower(src[i]);
	return dst;
}

static inline int crypto_async_request_type(struct crypto_async_request *req)
{
	return crypto_tfm_alg_type(req->tfm);
}

#endif
