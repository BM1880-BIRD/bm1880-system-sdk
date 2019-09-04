#ifndef __BMKERNEL_1880_H__
#define __BMKERNEL_1880_H__

#include <bmkernel/bm_kernel_legacy.h>

#ifdef __cplusplus
extern "C" {
#endif

bmerr_t bmk1880_dmabuf_size(uint8_t *cmdbuf, size_t sz, size_t *psize);
bmerr_t bmk1880_dmabuf_relocate(uint8_t *dmabuf, u64 dmabuf_devaddr);
bmerr_t bmk1880_dmabuf_convert(uint8_t *cmdbuf, size_t sz, uint8_t *dmabuf);
void bmk1880_dmabuf_dump(uint8_t *dmabuf);

typedef struct bmk1880_context_t_ bmk1880_context_t;
typedef struct ec_desc bmk1880_op_t;

typedef bmk_chip_info_t bmk1880_chip_info_t;
typedef bmk_info_t bmk1880_info_t;

bmk1880_chip_info_t bmk1880_chip_info();

bmk1880_context_t * bmk1880_register(bmk1880_info_t *info);
void bmk1880_cleanup(bmk1880_context_t *ctx);
void bmk1880_reset(bmk1880_context_t *ctx);
u8 *bmk1880_acquire_cmdbuf(bmk1880_context_t *ctx, u32 *size);

void bmk1880_parallel_enable(bmk1880_context_t *ctx);
void bmk1880_parallel_disable(bmk1880_context_t *ctx);

void bmk1880_create_streams(bmk1880_context_t *ctx, int nr_streams);
void bmk1880_destroy_streams(bmk1880_context_t *ctx);
void bmk1880_set_stream(bmk1880_context_t *ctx, int i);

void bmk1880_add_dependency(
    bmk1880_context_t *ctx,
    bmk1880_op_t *before,
    bmk1880_op_t *after);

void bmk1880_tl_shape_to_nchw(
    const bmk1880_context_t *ctx, shape_t s, int *n, int *c, int *h, int *w);
int bmk1880_tl_shape_to_size(
    const bmk1880_context_t *ctx, shape_t s, bool aligned, fmt_t fmt);
int bmk1880_tl_size(
    const bmk1880_context_t *ctx, tensor_lmem *tlp);

size_t bmk1880_tg_shape_to_size(shape_t s, fmt_t fmt);

tensor_lmem * bmk1880_tl_alloc(
    bmk1880_context_t *ctx, shape_t s, fmt_t fmt, u32 ctrls);
tensor_lmem * bmk1880_tl_alloc_bank(
    bmk1880_context_t *ctx, u32 bank_id, shape_t s, fmt_t fmt, u32 ctrls);
tensor_lmem * bmk1880_tl_prealloc(
    bmk1880_context_t *ctx, laddr_t la, shape_t s, fmt_t fmt);
tensor_lmem * bmk1880_tl_prealloc_align(
    bmk1880_context_t *ctx, laddr_t la, shape_t s, fmt_t fmt);

void bmk1880_tl_free(
    bmk1880_context_t *ctx, tensor_lmem *tlp);

void bmk1880_cpu_op(
    bmk1880_context_t *ctx,
    const char* op_name, char *params, int size);

bmk1880_op_t * bmk1880_gdma_copy_gmem(
    bmk1880_context_t *ctx,
    tensor_gmem *dst,
    tensor_gmem *src,
    ctrl_t ctrls);

bmk1880_op_t * bmk1880_gdma_load(
    bmk1880_context_t *ctx,
    tensor_lmem *t,
    u64 gaddr,
    ctrl_t ctrls);

bmk1880_op_t * bmk1880_gdma_store(
    bmk1880_context_t *ctx,
    tensor_lmem *t,
    u64 gaddr,
    ctrl_t ctrls);

bmk1880_op_t * bmk1880_gdma_load_stride(
    bmk1880_context_t *ctx,
    tensor_lmem *t,
    u64 gaddr,
    stride_t stride,
    ctrl_t ctrls);

bmk1880_op_t * bmk1880_gdma_store_stride(
    bmk1880_context_t *ctx,
    tensor_lmem *t,
    u64 gaddr,
    stride_t stride,
    ctrl_t ctrls);

bmk1880_op_t * bmk1880_gdma_copy_lmem(
    bmk1880_context_t *ctx,
    tensor_lmem *dst,
    tensor_lmem *src);

bmk1880_op_t * bmk1880_gdma_lrn_shift(
    bmk1880_context_t *ctx,
    tensor_lmem *dst,
    tensor_lmem *src,
    bool right_shift,
    int lrn_step);

/*
 * General rules for tensor arithmetic APIs:
 *
 * 1, All tensors can be either signed or unsigned, if not mentioned otherwise.
 * 2, A tensor @x with both @x_high and @x_low as parameters can optionally be
 *    8-bit (when @x_high is NULL) or 16-bit (otherwise).
 */

typedef struct {
  tensor_lmem *res_high;
  tensor_lmem *res_low;
  tensor_lmem *a;
  tensor_lmem *b;
  int rshift_width;
} bmk1880_mul_param_t;

bmk1880_op_t * bmk1880_tpu_mul(
    bmk1880_context_t *ctx,
    const bmk1880_mul_param_t *p);

typedef struct {
  tensor_lmem *res_high;
  tensor_lmem *res_low;
  tensor_lmem *a;
  s8 b;
  bool b_is_signed;
  int rshift_width;
} bmk1880_mul_const_param_t;

bmk1880_op_t * bmk1880_tpu_mul_const(
    bmk1880_context_t *ctx,
    const bmk1880_mul_const_param_t *p);

/*
 * @res = @a * @b + @res
 *
 * 1, @res_high must not be NULL since input @res must be 16-bit.
 * 2, If output @res is 8-bit (@res_is_int8 == 1), only @res_low is used
 *    as output tensor.
 */
typedef struct {
  tensor_lmem *res_high;
  tensor_lmem *res_low;
  bool res_is_int8;
  tensor_lmem *a;
  tensor_lmem *b;
  int lshift_width;
  int rshift_width;
} bmk1880_mac_param_t;

bmk1880_op_t * bmk1880_tpu_mac(
    bmk1880_context_t *ctx,
    const bmk1880_mac_param_t *p);

/*
 * @res = @a * @b + @res
 *
 * 1, @res_high must not be NULL since input @res must be 16-bit.
 * 2, If output @res is 8-bit (@res_is_int8 == 1), only @res_low is used
 *    as output tensor.
 */
typedef struct {
  tensor_lmem *res_high;
  tensor_lmem *res_low;
  bool res_is_int8;
  tensor_lmem *a;
  s8 b;
  bool b_is_signed;
  int lshift_width;
  int rshift_width;
} bmk1880_mac_const_param_t;

bmk1880_op_t * bmk1880_tpu_mac_const(
    bmk1880_context_t *ctx,
    const bmk1880_mac_const_param_t *p);

/*
 * @a and @b must all be 16-bit.
 */
typedef struct {
  tensor_lmem *res_high;
  tensor_lmem *res_low;
  tensor_lmem *a_high;
  tensor_lmem *a_low;
  tensor_lmem *b_high;
  tensor_lmem *b_low;
} bmk1880_add_param_t;

bmk1880_op_t * bmk1880_tpu_add(
    bmk1880_context_t *ctx,
    const bmk1880_add_param_t *p);

/*
 * @a must be 16-bit.
 */
typedef struct {
  tensor_lmem *res_high;
  tensor_lmem *res_low;
  tensor_lmem *a_high;
  tensor_lmem *a_low;
  s16 b;
  bool b_is_signed;
} bmk1880_add_const_param_t;

bmk1880_op_t * bmk1880_tpu_add_const(
    bmk1880_context_t *ctx,
    const bmk1880_add_const_param_t *p);

/*
 * 1, @a and @b must all be 16-bit.
 * 2, @res must be signed.
 */
typedef struct {
  tensor_lmem *res_high;
  tensor_lmem *res_low;
  tensor_lmem *a_high;
  tensor_lmem *a_low;
  tensor_lmem *b_high;
  tensor_lmem *b_low;
} bmk1880_sub_param_t;

bmk1880_op_t * bmk1880_tpu_sub(
    bmk1880_context_t *ctx,
    const bmk1880_sub_param_t *p);

/*
 * @a and @b must both be signed or unsigned.
 */
typedef struct {
  tensor_lmem *max;
  tensor_lmem *a;
  tensor_lmem *b;
} bmk1880_max_param_t;

bmk1880_op_t * bmk1880_tpu_max(
    bmk1880_context_t *ctx,
    const bmk1880_max_param_t *p);

typedef struct {
  tensor_lmem *max;
  tensor_lmem *a;
  s8 b;
  bool b_is_signed;
} bmk1880_max_const_param_t;

bmk1880_op_t * bmk1880_tpu_max_const(
    bmk1880_context_t *ctx,
    const bmk1880_max_const_param_t *p);

/*
 * @a and @b must both be signed or unsigned.
 */
typedef struct {
  tensor_lmem *min;
  tensor_lmem *a;
  tensor_lmem *b;
} bmk1880_min_param_t;

bmk1880_op_t * bmk1880_tpu_min(
    bmk1880_context_t *ctx,
    const bmk1880_min_param_t *p);

typedef struct {
  tensor_lmem *min;
  tensor_lmem *a;
  s8 b;
  bool b_is_signed;
} bmk1880_min_const_param_t;

bmk1880_op_t * bmk1880_tpu_min_const(
    bmk1880_context_t *ctx,
    const bmk1880_min_const_param_t *p);

/*
 * 1, @a must be 16-bit and signed.
 * 2, @res must be 16-bit.
 * 3, @bits must be signed and ranges in [-16, 16].
 */
typedef struct {
  tensor_lmem *res_high;
  tensor_lmem *res_low;
  tensor_lmem *a_high;
  tensor_lmem *a_low;
  tensor_lmem *bits;
} bmk1880_arith_shift_param_t;

bmk1880_op_t * bmk1880_tpu_arith_shift(
    bmk1880_context_t *ctx,
    const bmk1880_arith_shift_param_t *p);

/*
 * TODO: bmk1880_tl_logic_shift()
 */

typedef struct {
  tensor_lmem *res;
  tensor_lmem *a;
  tensor_lmem *b;
} bmk1880_and_int8_param_t;

bmk1880_op_t * bmk1880_tpu_and_int8(
    bmk1880_context_t *ctx,
    const bmk1880_and_int8_param_t *p);

/*
 * All parameters must be 16-bit.
 */
typedef struct {
  tensor_lmem *res_high;
  tensor_lmem *res_low;
  tensor_lmem *a_high;
  tensor_lmem *a_low;
  tensor_lmem *b_high;
  tensor_lmem *b_low;
} bmk1880_and_int16_param_t;

bmk1880_op_t * bmk1880_tpu_and_int16(
    bmk1880_context_t *ctx,
    const bmk1880_and_int16_param_t *p);

typedef struct {
  tensor_lmem *res;
  tensor_lmem *a;
  tensor_lmem *b;
} bmk1880_or_int8_param_t;

bmk1880_op_t * bmk1880_tpu_or_int8(
    bmk1880_context_t *ctx,
    const bmk1880_or_int8_param_t *p);

/*
 * All parameters must be 16-bit.
 */

typedef struct {
  tensor_lmem *res_high;
  tensor_lmem *res_low;
  tensor_lmem *a_high;
  tensor_lmem *a_low;
  tensor_lmem *b_high;
  tensor_lmem *b_low;
} bmk1880_or_int16_param_t;

bmk1880_op_t * bmk1880_tpu_or_int16(
    bmk1880_context_t *ctx,
    const bmk1880_or_int16_param_t *p);

typedef struct {
  tensor_lmem *res;
  tensor_lmem *a;
  tensor_lmem *b;
} bmk1880_xor_int8_param_t;

bmk1880_op_t * bmk1880_tpu_xor_int8(
    bmk1880_context_t *ctx,
    const bmk1880_xor_int8_param_t *p);

/*
 * All parameters must be 16-bit.
 */
typedef struct {
  tensor_lmem *res_high;
  tensor_lmem *res_low;
  tensor_lmem *a_high;
  tensor_lmem *a_low;
  tensor_lmem *b_high;
  tensor_lmem *b_low;
} bmk1880_xor_int16_param_t;

bmk1880_op_t * bmk1880_tpu_xor_int16(
    bmk1880_context_t *ctx,
    const bmk1880_xor_int16_param_t *p);

typedef struct {
  tensor_lmem *dst;
  tensor_lmem *src;
} bmk1880_copy_param_t;

bmk1880_op_t * bmk1880_tpu_copy(
    bmk1880_context_t *ctx,
    const bmk1880_copy_param_t *p);

typedef struct {
  tensor_lmem *dst;
  stride_t dst_stride;
  tensor_lmem *src;
  stride_t src_stride;
} bmk1880_copy_with_stride_param_t;

bmk1880_op_t * bmk1880_tpu_copy_with_stride(
    bmk1880_context_t *ctx,
    const bmk1880_copy_with_stride_param_t *p);

/*
 * @res and @a must be both signed or unsigned.
 */
typedef struct {
  tensor_lmem *res_high;
  tensor_lmem *res_low;
  tensor_lmem *a;
} bmk1880_mdsum_param_t;

bmk1880_op_t * bmk1880_tpu_mdsum(
    bmk1880_context_t *ctx,
    const bmk1880_mdsum_param_t *p);

/*
 * NOTE:
 *   @table is treated logically as a linear list of length @table_n, where
 *   @table_n is a multiple of 16 and is smaller than or equal to 256.
 *   When stored in local memory, @table is a tensor of shape (1, npu_num,
 *   1, @table_n), that is, the data of the linear list should be copied across
 *   each NPU's local memory by user. The behavior when these copies differ is
 *   not defined.
 */

typedef struct {
  tensor_lmem *ofmap;
  tensor_lmem *ifmap;
  tensor_lmem *table;
} bmk1880_lut_param_t;

bmk1880_op_t * bmk1880_tpu_lut(
    bmk1880_context_t *ctx,
    const bmk1880_lut_param_t *p);

typedef struct {
  tensor_lmem *ofmap;
  tensor_lmem *ifmap;
} bmk1880_relu_param_t;

bmk1880_op_t * bmk1880_tpu_relu(
    bmk1880_context_t *ctx,
    const bmk1880_relu_param_t *p);

typedef struct {
  tensor_lmem *ofmap;
  tensor_lmem *ifmap;
  tensor_lmem *weight;
  tensor_lmem *bias;
  u8 ins_h, ins_last_h;
  u8 ins_w, ins_last_w;
  u8 pad_top, pad_bottom;
  u8 pad_left, pad_right;
  u8 stride_h, stride_w;
  u8 dilation_h, dilation_w;
  bool relu_enable;
  int rshift_width;
} bmk1880_conv_param_t;

bmk1880_op_t * bmk1880_tpu_conv(
    bmk1880_context_t *ctx,
    const bmk1880_conv_param_t *p);

typedef struct {
  tensor_lmem *ofmap;
  tensor_lmem *ifmap;
  tensor_lmem *weight;
  tensor_lmem *bias;
  u8 pad_top, pad_bottom;
  u8 pad_left, pad_right;
  bool relu_enable;
  int rshift_width;
} bmk1880_winograd_param_t;

bmk1880_op_t * bmk1880_tpu_winograd(
    bmk1880_context_t *ctx,
    const bmk1880_winograd_param_t *p);

typedef struct {
  tensor_lmem *ofmap;
  tensor_lmem *ifmap;
  u8 kh, kw;
  u8 pad_top, pad_bottom;
  u8 pad_left, pad_right;
  u8 stride_h, stride_w;
} bmk1880_max_pooling_param_t;

bmk1880_op_t * bmk1880_tpu_max_pooling(
    bmk1880_context_t *ctx,
    const bmk1880_max_pooling_param_t *p);

typedef struct {
  tensor_lmem *ofmap;
  tensor_lmem *ifmap;
  u8 kh, kw;
  u8 ins_h, ins_last_h;
  u8 ins_w, ins_last_w;
  u8 pad_top, pad_bottom;
  u8 pad_left, pad_right;
  u8 stride_h, stride_w;
  u8 avg_pooling_const;
  int rshift_width;
} bmk1880_avg_pooling_param_t;

bmk1880_op_t * bmk1880_tpu_avg_pooling(
    bmk1880_context_t *ctx,
    const bmk1880_avg_pooling_param_t *p);

typedef struct {
  tensor_lmem *ofmap;
  tensor_lmem *ifmap;
  tensor_lmem *weight;
  tensor_lmem *bias;
  u8 ins_h, ins_last_h;
  u8 ins_w, ins_last_w;
  u8 pad_top, pad_bottom;
  u8 pad_left, pad_right;
  u8 stride_h, stride_w;
  int rshift_width;
} bmk1880_depthwise_param_t;

bmk1880_op_t * bmk1880_tpu_depthwise(
    bmk1880_context_t *ctx,
    const bmk1880_depthwise_param_t *p);

typedef struct {
  tensor_lmem *res;
  tensor_lmem *left;
  tensor_lmem *right;
  tensor_lmem *bias;
  int lshift_width;
  int rshift_width;
  bool res_is_int8;
  ctrl_t ctrls;
} bmk1880_matrix_mac_param_t;

bmk1880_op_t * bmk1880_tpu_matrix_mac(
    bmk1880_context_t *ctx,
    const bmk1880_matrix_mac_param_t *p);

typedef struct {
  tensor_lmem *res;
  tensor_lmem *left;
  tensor_lmem *right;
} bmk1880_matrix_mac_2_param_t;

bmk1880_op_t * bmk1880_tpu_matrix_mac_2(
    bmk1880_context_t *ctx,
    const bmk1880_matrix_mac_2_param_t *p);

#ifdef __cplusplus
}
#endif

#endif /* __BMKERNEL_1880_H__ */
