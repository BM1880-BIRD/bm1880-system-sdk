#ifndef __BM_KERNEL_H__
#define __BM_KERNEL_H__

#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

#ifdef __cplusplus
extern "C" {
#endif

typedef u32 bmerr_t;
#define BM_SUCCESS 0               // The operation was successful
#define BM_ERR_AGAIN 1             // Not ready yet
#define BM_ERR_FAILURE 2           // General failure
#define BM_ERR_TIMEOUT 3           // Timeout
#define BM_ERR_UNINITIALIZED 4     // Uninitialzed
#define BM_ERR_INVALID_ARGUMENT 5  // Arguments invalid
#define BM_ERR_NOMEM 6             // Not enough memory
#define BM_ERR_DATA 7              // Data error
#define BM_ERR_BUSY 8              // Busy
#define BM_ERR_NOT_SUPPORTED 9     // Not supported yet

typedef u32 fmt_t;
#define FMT_F32 0
#define FMT_F16 1
#define FMT_I32 2
#define FMT_I16 3
#define FMT_I8  4
#define FMT_I4  5
#define FMT_I2  6
#define FMT_I1  7
#define FMT_U16 8
#define FMT_U8  9
#define FMT_INVALID 10

#define BM_CMB_HDR_MAGIC       (0xA5)
#define BM_CMB_HDR_FLAG_NEURON     (0x1)
#define BM_CMB_HDR_FLAG_WEIGHT     (0x2)

typedef struct __cmd_hdr_s {
  u8 magic;              // 0xA5
  u8 len;                // lens in bytes
  u8 engine_id: 4;       // TPU, GDMA, CDMA
  u8 __deprecated: 4;
  u8 flags;              // CMD_ID, sync flags, etc. TBD
  u32 mask;              // bit mask for which register need to write
  u8 cmd[0];
} __attribute__((packed)) cmd_hdr_t;

typedef struct {
  u32 chip_version;
  u32 cmdbuf_size;
  u8 *cmdbuf;
} bmk_info_t;

static inline int ceiling_func(int numerator, int denominator)
{
  return (numerator + denominator - 1) / denominator;
}

static inline int ceiling_func_shift(int numerator, int shift)
{
  return (numerator + (1 << shift) - 1) >> shift;
}

static inline u64 align_up(u64 x, u64 n)
{
  return (x + n - 1) / n * n;
}

// len max number is 255, sometimes cmd larger than 255
static inline u32 cmd_hdr_len(cmd_hdr_t * hdr) {
  if (hdr->len == 0) {
    return hdr->mask;
  }
  return hdr->len;
}

#ifdef __cplusplus
}
#endif

#endif /* __BM_KERNEL_H__ */
