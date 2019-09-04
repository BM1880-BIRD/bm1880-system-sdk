#ifndef _SYSTEM_COMMON_H_
#define _SYSTEM_COMMON_H_

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

static inline u32 float_to_u32(float x)
{
	union {
		int ival;
		float fval;
	} v = { .fval = x };
	return v.ival;
}

static inline float u32_to_float(u32 x)
{
	union {
		int ival;
		float fval;
	} v = { .ival = x };
	return v.fval;
}

#define array_len(a)             ARRAY_SIZE(a)

#define ALIGNMENT(x, a)			  __ALIGNMENT_MASK((x), (typeof(x))(a) - 1)
#define __ALIGNMENT_MASK(x, mask)	(((x) + (mask)) & ~(mask))
#define PTR_ALIGNMENT(p, a)		 ((typeof(p))ALIGNMENT((unsigned long)(p), (a)))
#define IS_ALIGNMENT(x, a)		(((x) & ((typeof(x))(a) - 1)) == 0)

#define __raw_readb(a)		  (*(/*volatile */unsigned char *)(a))
#define __raw_readw(a)		  (*(/*volatile */unsigned short *)(a))
#define __raw_readl(a)		  (*(/*volatile */unsigned int *)(a))
#define __raw_readq(a)		  (*(/*volatile */unsigned long long *)(a))

#define __raw_writeb(a, v)	   (*(/*volatile */unsigned char *)(a) = (v))
#define __raw_writew(a, v)	   (*(/*volatile */unsigned short *)(a) = (v))
#define __raw_writel(a, v)	   (*(/*volatile */unsigned int *)(a) = (v))
#define __raw_writeq(a, v)	   (*(/*volatile */unsigned long long *)(a) = (v))

#define readb(a)		__raw_readb(a)
#define readw(a)		__raw_readw(a)
#define readl(a)		__raw_readl(a)
#define readq(a)		__raw_readq(a)

#define writeb(a, v)		__raw_writeb(a, v)
#define writew(a, v)		__raw_writew(a, v)
#define writel(a, v)		__raw_writel(a, v)
#define writeq(a, v)		__raw_writeq(a, v)

#define cpu_write8(a, v)	writeb(a, v)
#define cpu_write16(a, v)	writew(a, v)
#define cpu_write32(a, v)	writel(a, v)

#define cpu_read8(a)		readb(a)
#define cpu_read16(a)		readw(a)
#define cpu_read32(a)		readl(a)

#ifndef __weak
#define __weak __attribute__((weak))
#endif

#define ENABLE_PRINT

#ifdef ENABLE_PRINT

#define uartlog(fmt, args...)	tf_printf(fmt, ##args)
#else
#define uartlog(...)
#endif

/**/
/*#if LOG_LEVEL >= LOG_LEVEL_NOTICE*/
/*# define NOTICE(...)    printf("NOTICE:  " __VA_ARGS__)*/
/*#else*/
/*# define NOTICE(...)*/
/*#endif*/
/**/
/*#if LOG_LEVEL >= LOG_LEVEL_ERROR*/
/*# define ERROR(...)    printf("ERROR:   " __VA_ARGS__)*/
/*#else*/
/*# define ERROR(...)*/
/*#endif*/
/**/
/*#if LOG_LEVEL >= LOG_LEVEL_WARNING*/
/*# define WARN(...)    printf("WARNING: " __VA_ARGS__)*/
/*#else*/
/*# define WARN(...)*/
/*#endif*/
/**/
/*#if LOG_LEVEL >= LOG_LEVEL_INFO*/
/*# define INFO(...)    printf("INFO: " __VA_ARGS__)*/
/*#else*/
/*# define INFO(...)*/
/*#endif*/
/**/
/*#if LOG_LEVEL >= LOG_LEVEL_VERBOSE*/
/*# define VERBOSE(...)    printf("VERBOSE: " __VA_ARGS__)*/
/*#else*/
/*# define VERBOSE(...)*/
/*#endif*/

#if 0
extern u32 debug_level;
#define debug_out(flag, fmt, args...)           \
	do {                                          \
		if ((flag) <= debug_level)                    \
			printf(fmt, ##args);                      \
	} while (0)
#endif

#ifdef USE_BMTAP
#define call_atomic(nodechip_idx, atomic_func, p_command, eng_id)       \
	emit_task_descriptor(p_command, eng_id)
#endif

#define PERI_UART3_INTR     50
#define PERI_UART2_INTR     47
#define PERI_UART1_INTR     44
#define PERI_UART0_INTR     41
#define USB_OTG_INTR        (114 - INTR_SPI_BASE)
#define USB_DEV_INTR1       (116 - INTR_SPI_BASE)

#endif
