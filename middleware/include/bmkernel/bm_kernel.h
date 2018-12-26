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

typedef u32 laddr_t;
typedef u64 gaddr_t;

#ifdef __cplusplus
extern "C" {
#endif

#define LADDR_INVALID          (0xFFFFFFFF)
#define GADDR_INVALID          (0x000000FFFFFFFFFFULL)

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

#define ENGINE_BD    0     // Broadcast Engine
#define ENGINE_CPU   1     // CPU, Reserved
#define ENGINE_GDMA  2     // GDMA Engine
#define ENGINE_CDMA  3     // CDMA Engine
#define ENGINE_END   4     // Invalid

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
  u32 magic;
  u32 dmabuf_size;
  u32 cpu_desc_count;
  u32 bd_desc_count;
  u32 gdma_desc_count;
  u32 reserve[11]; // for 64Byte align
} __attribute__((packed)) dma_hdr_t;

typedef struct {
  u32 version;
  u32 npu_num;
  u32 eu_num;
  u32 lmem_size;
  u32 lmem_banks;
  u32 lmem_bank_size;
} bmk_chip_info_t;

typedef struct {
  u32 chip_version;
  u32 cmdbuf_size;
  u8 *cmdbuf;
} bmk_info_t;

#define FLOAT_SIZE 4
#define INT8_SIZE 1

#define UNUSED(x)               (void)(x)

#define __ALIGN_MASK(x,mask)    (((x)+(mask))&~(mask))
#define ALIGN(x,a)              __ALIGN_MASK(x,(__typeof__(x))(a)-1)
#define ALIGN_DOWN(x, a)        ((x) / (a) * (a))

#define math_min(x, y)          ((x) < (y) ? (x) : (y))
#define math_max(x, y)          ((x) > (y) ? (x) : (y))

static inline int get_num_shift(uint64_t num)
{
  int n = 0;
  while (!(num & 1)) {
    n++;
    num >>= 1;
  }
  return n;
}

static inline int ceiling_func(int numerator, int denominator)
{
  return (numerator + denominator - 1) / denominator;
}

static inline int ceiling_func_shift(int numerator, int shift)
{
  return (numerator + (1 << shift) - 1) >> shift;
}

typedef struct {
  u32 dim;
  u32 n;
  u32 c;
  union {
    u32 h;
    u32 row;
  };
  union {
    u32 w;
    u32 col;
  };
} shape_t;

shape_t shape_t4(int n, int c, int h, int w);
shape_t shape_t3(int d3, int d2, int d1);
shape_t shape_t2(int row, int col);
shape_t shape_t1(int len);

bool shape_equal(shape_t s1, shape_t s2);

typedef struct {
  u32 n;
  u32 c;
  union {
    u32 h;
    u32 row;
  };
  union {
    u32 w;
    u32 col;
  };
} stride_t;

static inline stride_t stride_st4(int n, int c, int h, int w)
{
  stride_t st;
  st.n = n;
  st.c = c;
  st.h = h;
  st.w = w;
  return st;
}

typedef u32 ctrl_t;
#define CTRL_NULL                   0
#define CTRL_AL                     (1 << 0) // alloc aligned with EU_NUM
#define CTRL_RA                     (1 << 2)  // result add
#define CTRL_BN                     (1 << 3)  // B_N_is_1 broadcast to A
#define CTRL_TP                     (1 << 5)  // transpose
#define CTRL_ADDR_ALIGN             (1 << 7)
#define CTRL_RELU                   (1 << 8)
#define CTRL_KFLIP                  (1 << 9)  // kernel flip
#define CTRL_WEIGHT                 (1 << 10) // mark weight address in GDMA
#define CTRL_NEURON                 (1 << 11) // mark neuron address in GDMA
#define CTRL_WINOGRAD               (1 << 12) // GDMA reshap winograd kernel
#define CTRL_WINOGRAD_SCALE_FACTOR  (1 << 28) // GDMA reshap winograd kernel

typedef u32 tl_type;
#define TL_TYPE_TENSOR 0
#define TL_TYPE_TENSOR_PREALLOC 1
#define TL_TYPE_CONSTANT 2
#define TL_TYPE_SLICE 3
#define TL_TYPE_CLONE 4

typedef union {
  u32 reg_val;
  float fp32_val;
} const_fp32_t;

typedef union {
  laddr_t laddr;
  const_fp32_t const_fp32;
} opd_t;

typedef struct {
  tl_type type;
  opd_t operand;
  shape_t shape;
  stride_t *stride;
  bool aligned;
  fmt_t fmt;
  u32 bank_id;
  int reserved_size;
} tensor_lmem;

static inline laddr_t tl_address(tensor_lmem *tlp)
{
  if (tlp->type == TL_TYPE_CONSTANT) {
    return LADDR_INVALID;
  }
  return tlp->operand.laddr;
}

typedef struct {
  u64 addr;
  shape_t shape;
  stride_t stride;
} tensor_gmem;

static inline int tg_is_matrix(tensor_gmem *t)
{
  return t->shape.dim == 2;
}

static inline int tg_matrix_row(tensor_gmem *t)
{
  return t->shape.row;
}

static inline int tg_matrix_col(tensor_gmem *t)
{
  return t->shape.col;
}

#ifdef __cplusplus
}
#endif

#endif /* __BM_KERNEL_H__ */
