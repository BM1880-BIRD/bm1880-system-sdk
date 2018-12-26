#ifndef _BM_RUNTIME_H_
#define _BM_RUNTIME_H_

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include <bmkernel/bm_kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bm_context;
typedef struct bm_context *bmctx_t;
struct bm_device;
typedef struct bm_device *bmdev_t;

#define BM_DEVICE_MAX_NUM      (8)
void bm_enum_devices(int *count, bmk_chip_info_t devinfo[]);
bmerr_t bm_device_open(int index, bmdev_t *dev);
void bm_device_close(bmdev_t dev);
bmerr_t bm_device_query(bmdev_t dev, int id, void *buf);
bmerr_t bm_device_config(bmdev_t dev, int id, void *buf);
bmk_chip_info_t bm_device_get_info(bmdev_t dev);
u64 bm_device_get_gmem_size(bmdev_t dev);
bmerr_t bm_context_create(bmctx_t *ctx);
void bm_context_destroy(bmctx_t ctx);
bmerr_t bm_bind_device(bmctx_t ctx, bmdev_t dev);
void bm_unbind_device(bmctx_t ctx);
bmdev_t bm_get_device(bmctx_t ctx);

void bm_set_cmdbuf(bmctx_t ctx, u8 *cmdbuf);
void bm_free_cmdbuf(bmctx_t ctx);

bmerr_t bm_init(int index, bmctx_t *ctx);
void bm_exit(bmctx_t ctx);

struct bm_memory;
typedef struct bm_memory *bmmem_t;
typedef bmmem_t bmmem_device_t;
typedef bmmem_t bmmem_host_t;
typedef bmmem_t bmmem_system_t;

typedef enum bmfmt_e {
  BM_FMT_FP32 = 0,
  BM_FMT_FP16 = 1,
  BM_FMT_INT16 = 2,
  BM_FMT_INT8 = 3,
  BM_FMT_MAX = 4
} bmfmt_t;

static const int bmfmt_bpp[BM_FMT_MAX] = {32, 16, 16, 8};
#define BM_FMT_BPP(_fmt_)      (bmfmt_bpp[(_fmt_)])

#define BM_SHAPE_MAX_DIM       (4)
typedef struct bmshape_s {
  bmfmt_t fmt;
  int dim_size;
  int dim[BM_SHAPE_MAX_DIM];
} bmshape_t;
#define BM_TENSOR_FP32(n, c, h, w) \
    {.fmt = BM_FMT_FP32, \
     .dim_size = 4, \
     .dim = {n, c, h, w} \
    }
#define BM_TENSOR_INT16(n, c, h, w) \
    {.fmt = BM_FMT_INT16, \
     .dim_size = 4, \
     .dim = {n, c, h, w} \
    }
#define BM_TENSOR_INT8(n, c, h, w) \
    {.fmt = BM_FMT_INT8, \
     .dim_size = 4, \
     .dim = {n, c, h, w} \
    }
#define BM_TENSOR_WITH_FMT(n, c, h, w, data_fmt) \
    {.fmt = data_fmt, \
     .dim_size = 4, \
     .dim = {n, c, h, w} \
    }
#define BM_MATRIX_INT16(l, r) \
    {.fmt = BM_FMT_INT16, \
     .dim_size = 2, \
     .dim = {l, r} \
    }
#define BM_MATRIX_INT8(l, r) \
    {.fmt = BM_FMT_INT8, \
     .dim_size = 2, \
     .dim = {l, r} \
    }
#define BM_MATRIX_FP32(l, r) \
    {.fmt = BM_FMT_FP32, \
     .dim_size = 2, \
     .dim = {l, r} \
    }

bmmem_device_t bmmem_device_alloc_raw(bmctx_t ctx, size_t size);
bmmem_device_t bmmem_device_prealloc_raw(bmctx_t ctx, bmmem_device_t mem, uint64_t offset, size_t size);
bmmem_device_t bmmem_device_alloc(bmctx_t ctx, bmshape_t *shape);
bmmem_device_t bmmem_device_prealloc(bmctx_t ctx, bmmem_device_t mem, uint64_t offset, bmshape_t *shape);
void bmmem_device_free(bmctx_t ctx, bmmem_device_t mem);
bmmem_host_t bmmem_host_alloc(bmctx_t ctx, bmshape_t *shape);
void bmmem_host_free(bmctx_t ctx, bmmem_host_t mem);

size_t bmmem_device_size(bmctx_t ctx, bmmem_device_t mem);
uint64_t bmmem_device_addr(bmctx_t ctx, bmmem_device_t mem);
uint8_t* bmmem_device_v_addr(bmctx_t ctx, bmmem_device_t mem);
void* bmmem_host_v_addr(bmctx_t ctx, bmmem_host_t mem);
uint64_t bmmem_host_p_addr(bmctx_t ctx, bmmem_host_t mem);

bmerr_t bm_memcpy_h2d(bmctx_t ctx, bmmem_device_t dst, bmmem_host_t src,
    uint32_t src_host_offset);
bmerr_t bm_memcpy_d2h(bmctx_t ctx, bmmem_host_t dst, bmmem_device_t src,
    uint32_t dst_host_offset);

bmerr_t bm_memcpy_s2d(bmctx_t ctx, bmmem_device_t dst, uint8_t* src);
bmerr_t bm_memcpy_d2s(bmctx_t ctx, uint8_t* dst, bmmem_device_t src);

bmerr_t bm_load_cmdbuf(bmctx_t ctx, uint8_t *cmdbuf, size_t sz,
                       bmmem_device_t *cmdbuf_mem);
bmerr_t bm_run_cmdbuf(bmctx_t ctx, bmmem_device_t cmdbuf_mem,
                      uint16_t *seq_no);
bmerr_t bm_send_cmdbuf(bmctx_t ctx, uint8_t *cmdbuf, size_t sz,
                       uint16_t *seq_no);
bmerr_t bm_wait_cmdbuf_done(bmctx_t ctx, uint16_t seq_no);

bmerr_t bm_run_cmdbuf_pio(bmctx_t ctx, uint8_t *cmdbuf, size_t sz);

const char * bm_library_type();

#ifdef __cplusplus
}
#endif

#endif /* _BM_RUNTIME_H_ */
