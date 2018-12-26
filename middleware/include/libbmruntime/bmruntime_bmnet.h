/**
 * @mainpage BMNET Runtime Interface: bmruntime_bmnet.h
 * @file   bmruntime_bmnet.h
 * @Author Wang Lei (lei.wang@bitmain.com)
 * @brief  BMNET runtime APIs.
 *
 * This file describes BMNET runtime interface APIs.
 */
#ifndef _BMRUNTIME_BMNET_H_
#define _BMRUNTIME_BMNET_H_

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include <libbmruntime/bmruntime.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @name    bmnet_info_t
 * @brief   Structure to describe information of a BMNET.
 * @ingroup bmruntime
 */
typedef struct bmnet_info_s {
    uint32_t          batch_size;      //!< batch size
    uint8_t          *weight;          //!< pointer to weight buffer
    size_t            weight_size;     //!< size of weight in bytes
    uint8_t          *cmdbuf;          //!< pointer to cmdbuf buffer
    size_t            cmdbuf_size;     //!< size of cmdbuf in bytes
    size_t            neuron_size;     //!< total neuron size in bytes
    size_t            input_size;      //!< input neuron size in bytes
    uint64_t          output_offset;   //!< output neuron offset to neuron base
    size_t            output_size;     //!< output neuron size in bytes
} bmnet_info_t;

/**
 * @name    bmnet_output_info_t
 * @brief   Structure to describe output info of a BMNET.
 * @ingroup bmruntime
 */
typedef struct bmnet_output_info_s {
    size_t            output_size;     // !< output size in bytes
    size_t            output_num;      // !< output number
    char            **name_array;      // !< array of pointers to the output names
    shape_t          *shape_array;     // !< array of pointers to the output shapes
    float            *threshold_array; // !< array of pointers to the output threshold
} bmnet_output_info_t;

struct bmnet_context;
/**
 * @name    bmnet_t
 * @brief   BMNET runtime context handler.
 * @ingroup bmruntime
 */
typedef struct bmnet_context *bmnet_t;

/**
 * @name    bmnet_register
 * @brief   To register a compiled neuron network (bmnet) to BM runtime.
 * @ingroup bmruntime
 *
 * This API registers a compiled neuron network (bmnet) to BM runtime. It
 * first allocates sufficient device memory for neuron, weight, and cmdbuf,
 * does gmem address relocation to the with allocated addresses, then preloads
 * weight and cmdbuf to device memory. By doing weight and cmdbuf preload
 * during register, we don't need to load them again every time inference runs.
 * @todo: to preload to local memory directly if certain conditions are met.
 *
 * @param [in]  ctx    BM runtime context.
 * @param [in]  info   BMNET info, in bmnet_info_t.
 * @param [out] net    BMNET context handler.
 *
 * @retval BM_SUCCESS  Successfully.
 * @retval others      Error code.
 */
bmerr_t bmnet_register(bmctx_t ctx, bmnet_info_t *info, bmnet_t *net);

/**
 * @name    bmnet_register_noalloc
 * @brief   To register a compiled neuron network (bmnet) to BM runtime,
 *          without allocating weight and neuron devmem.
 * @ingroup bmruntime
 *
 * This API registers a compiled neuron network (bmnet) to BM runtime, without
 * allocating weight and neuron devmem. Compared to bmnet_register, this API
 * doesn't allocate weight and neuron devmem, and doesn't do gmem address
 * relocation. We need to import weight and neuron devmem, and load cmdbuf
 * to do gmem allocate later. By doing this, application can implement device
 * memory management itself or share device memory between bmnet context.
 *
 * @param [in]  ctx    BM runtime context.
 * @param [in]  info   BMNET info, in bmnet_info_t.
 * @param [out] net    BMNET context handler.
 *
 * @retval BM_SUCCESS  Successfully.
 * @retval others      Error code.
 */
bmerr_t bmnet_register_noalloc(bmctx_t ctx, bmnet_info_t *info, bmnet_t *net);

/**
 * @name    bmnet_register_bmodel
 * @brief   To register a neuron network (bmnet) to BM runtime,
 *          with bmodel file.
 * @ingroup bmruntime
 *
 * This API registers a neuron network (bmnet) to BM runtime, with bmodel
 * file. Compared to bmnet_register, this API use bmodel file to register
 * neuron network directly, instead of generating weight.bin and cmdbuf.bin
 * firstly. Besides, this API can support different input shape, so the user
 * must set input shape later.
 *
 * @param [in]  ctx        BM runtime context.
 * @param [in]  bmodel     bmodel filename.
 * @param [out] net        BMNET context handler.
 *
 * @retval BM_SUCCESS  Successfully.
 * @retval others      Error code.
 */
bmerr_t bmnet_register_bmodel(bmctx_t ctx, const char *bmodel, bmnet_t *net);

/**
 * @name    bmnet_register_bmodel_data
 * @brief   To register a neuron network (bmnet) to BM runtime,
 *          with bmodel data in memory.
 * @ingroup bmruntime
 *
 * This API registers a neuron network (bmnet) to BM runtime, with bmodel
 * data. Compared to bmnet_register, this API use bmodel data to register
 * neuron network directly, instead of generating weight.bin and cmdbuf.bin
 * firstly. Besides, this API can support different input shape, so the user
 * must set input shape later.
 *
 * @param [in]  ctx           BM runtime context.
 * @param [in]  bmodel_data   bmodel data in memory.
 * @param [in]  size          bmodel data size in byte.
 * @param [out] net           BMNET context handler.
 *
 * @retval BM_SUCCESS  Successfully.
 * @retval others      Error code.
 */
bmerr_t bmnet_register_bmodel_data(bmctx_t ctx, uint8_t *bmodel_data,
                                   size_t size, bmnet_t *net);

/**
 * @name    bmnet_set_input_shape
 * @brief   To set ipnut shape for a registered bmnet.
 * @ingroup bmruntime
 *
 * This API set input shape for a registered bmnet. The bmodel or caffemodel
 * net may suport different input shape, this API can set the input shape in
 * these shapes. This API only match one input shape.
 *
 * @param [in]  ctx    BM runtime context.
 * @param [in]  shape  input shape.
 *
 * @retval BM_SUCCESS  Successfully.
 * @retval others      Error code.
 */
bmerr_t bmnet_set_input_shape(bmnet_t net, shape_t shape);

/**
 * @name    bmnet_set_input_shape2
 * @brief   To set multiple ipnut shapes for a registered bmnet.
 * @ingroup bmruntime
 *
 * This API set multiple input shapes for a registered bmnet. The bmodel or
 * caffemodel net may suport different input shape, this API can set the input 
 * shapes in these shapes.
 *
 * @param [in]  ctx    BM runtime context.
 * @param [in]  shape  input shape array.
 * @param [in]  num    number of input shapes.
 *
 * @retval BM_SUCCESS  Successfully.
 * @retval others      Error code.
 */
bmerr_t bmnet_set_input_shape2(bmnet_t net, shape_t *shape, int num);

/**
 * @name    bmnet_get_output info
 * @brief   To get output info of current ipnut shape.
 * @ingroup bmruntime
 *
 * This API get output info of current input shape. The bmodel or caffemodel
 * net may suport different input shape, this API can get output info of the
 * current input shape.
 *
 * @param [in]  ctx          BM runtime context.
 * @param [out] output_info  output information.
 *
 * @retval BM_SUCCESS  Successfully.
 * @retval others      Error code.
 */
bmerr_t bmnet_get_output_info(bmnet_t net, bmnet_output_info_t *output_info);

/**
 * @name    bmnet_cleanup
 * @brief   To cleanup a registered bmnet.
 * @ingroup bmruntime
 *
 * This API cleanups a registered bmnet. It frees all resources allocated
 * during register and runtime.
 *
 * @param [in]  net    BMNET context handler.
 *
 * @retval
 */
void bmnet_cleanup(bmnet_t net);

/**
 * @name    bmnet_run
 * @brief   To run a registered bmnet.
 * @ingroup bmruntime
 *
 * This API runs a registered bmnet, by sending the cmdbuf to hardware, and
 * wait for it to finish. It assumes input and weight are already loaded into
 * device memory. If cmdbuf_preload is set, it also assumes cmdbuf is already
 * loaded into device memory, otherwise, cmdbuf will be sent to device memory
 * within this function.
 *
 * @param [in]  ctx    BM runtime context.
 *
 * @retval BM_SUCCESS  Successfully.
 * @retval others      Error code.
 */
bmerr_t bmnet_run(bmnet_t net);

/**
 * @name    bmnet_weight_devmem
 * @brief   To retrieve weight devmem handler from a registered bmnet.
 * @ingroup bmruntime
 *
 * This API retrieve weight devmem handler from a registered bmnet. By doing
 * this, the caller can put weight data directly into the devmem , therefore
 * eliminates a memory copy to save bandwidth. Besides, the caller can share
 * weight devmem to other related bmnet context, this saves memory.
 *
 * @param [in]  ctx    BM runtime context.
 *
 * @retval  bmmem_device_t   Handler to device memory
 */
bmmem_device_t bmnet_weight_devmem(bmnet_t net);

/**
 * @name    bmnet_neuron_devmem
 * @brief   To retrieve neuron devmem handler from a registered bmnet.
 * @ingroup bmruntime
 *
 * This API retrieve neuron devmem handler from a registered bmnet. By doing
 * this, the caller can put neuron data directly into the devmem , therefore
 * eliminates a memory copy to save bandwidth. Besides, the caller can share
 * neuron devmem to other related bmnet context, this saves memory.
 *
 * @param [in]  ctx    BM runtime context.
 *
 * @retval  bmmem_device_t   Handler to device memory
 */
bmmem_device_t bmnet_neuron_devmem(bmnet_t net);

/**
 * @name    bmnet_input_devmem
 * @brief   To retrieve input devmem handler from a registered bmnet.
 * @ingroup bmruntime
 *
 * This API retrieve input devmem handler from a registered bmnet. By doing
 * this, the caller can put input data directly into the devmem (from VPP
 * driver for example), therefore eliminates a memory copy to save bandwidth.
 *
 * @param [in]  ctx    BM runtime context.
 *
 * @retval  bmmem_device_t   Handler to device memory
 */
bmmem_device_t bmnet_input_devmem(bmnet_t net);

/**
 * @name    bmnet_output_devmem
 * @brief   To retrieve output devmem handler from a registered bmnet.
 * @ingroup bmruntime
 *
 * This API retrieve output devmem handler from a registered bmnet. By doing
 * this, the caller can fetch output data directly from the devmem, therefore
 * eliminates a memory copy to save bandwidth.
 *
 * @param [in]  ctx    BM runtime context.
 *
 * @retval  bmmem_device_t   Handler to device memory
 */
bmmem_device_t bmnet_output_devmem(bmnet_t net);

/**
 * @name    bmnet_load_input
 * @brief   To load input data for a registered bmnet.
 * @ingroup bmruntime
 *
 * This API loads input data for a registered bmnet, from system memory. A
 * memory copy from system memory to device memory happens during loading.
 *
 * @param [in]  ctx    BM runtime context.
 * @param [in]  input  Pointer to the input buffer.
 *
 * @retval BM_SUCCESS  Successfully.
 * @retval others      Error code.
 */
bmerr_t bmnet_load_input(bmnet_t net, uint8_t *input);

/**
 * @name    bmnet_load_cmdbuf
 * @brief   To load cmdbuf for a registered bmnet.
 * @ingroup bmruntime
 *
 * This API loads cmdbuf for a registered bmnet.
 *
 * @param [in]  ctx          BM runtime context.
 * @param [in]  cmdbuf       Pointer to the cmdbuf buffer.
 * @param [in]  cmdbuf_size  Size of the cmdbuf buffer.
 *
 * @retval BM_SUCCESS  Successfully.
 * @retval others      Error code.
 */
bmerr_t bmnet_load_cmdbuf(bmnet_t net, const uint8_t *cmdbuf, int cmdbuf_size);

/**
 * @name    bmnet_import_weight_mem
 * @brief   To import weight devmem for a registered bmnet.
 * @ingroup bmruntime
 *
 * This API imports weight devmem for a registered bmnet. The preload weight
 * devmem, if exist, will be freed, the new weight devmem will be referred. We
 * need to call bmnet_load_cmdbuf to relocate cmdbuf after this.
 *
 * @param [in]  ctx         BM runtime context.
 * @param [in]  weight_mem  Pointer to the weight devmem.
 *
 * @retval BM_SUCCESS  Successfully.
 * @retval others      Error code.
 */
bmerr_t bmnet_import_weight_devmem(bmnet_t net, bmmem_device_t weight_mem);

/**
 * @name    bmnet_import_neuron_mem
 * @brief   To import neuron devmem for a registered bmnet.
 * @ingroup bmruntime
 *
 * This API imports neuron devmem for a registered bmnet. The preload neuron
 * devmem, if exist, will be freed, the new neuron devmem will be referred. We
 * need to call bmnet_load_cmdbuf to relocate cmdbuf after this.
 *
 * @param [in]  ctx         BM runtime context.
 * @param [in]  neuron_mem  Pointer to the neuron devmem.
 *
 * @retval BM_SUCCESS  Successfully.
 * @retval others      Error code.
 */
bmerr_t bmnet_import_neuron_devmem(bmnet_t net, bmmem_device_t neuron_mem);

/**
 * @name    bmnet_store_output
 * @brief   To store output data for a registered bmnet.
 * @ingroup bmruntime
 *
 * This API stores output data for a registered bmnet, to system memory. A
 * memory copy from device memory to system memory happens during storing.
 *
 * @param [in]  ctx    BM runtime context.
 * @param [in]  output Pointer to the output buffer.
 *
 * @retval BM_SUCCESS  Successfully.
 * @retval others      Error code.
 */
bmerr_t bmnet_store_output(bmnet_t net, uint8_t *output);

/**
 * @name    bmnet_store_neuron
 * @brief   To store neuron data for a registered bmnet.
 * @ingroup bmruntime
 *
 * This API stores neuron data for a registered bmnet, to system memory. A
 * memory copy from device memory to system memory happens during storing.
 *
 * @param [in]  ctx           BM runtime context.
 * @param [in]  neuron_offset Offset of the neuron buffer.
 * @param [in]  neuron_size   Size of the neuron buffer.
 * @param [in]  neuron        Pointer to the neuron buffer.
 *
 * @retval BM_SUCCESS  Successfully.
 * @retval others      Error code.
 */
bmerr_t bmnet_store_neuron(bmnet_t net, uint64_t neuron_offset, int neuron_size,
                           uint8_t *neuron);

/**
 * @name    bmnet_load_neuron
 * @brief   To load neuron data for a registered bmnet.
 * @ingroup bmruntime
 *
 * This API loads neuron data for a registered bmnet, to device memory. A
 * memory copy from system memory to device memory happens during loading.
 *
 * @param [in]  ctx           BM runtime context.
 * @param [in]  neuron_offset Offset of the neuron buffer.
 * @param [in]  neuron_size   Size of the neuron buffer.
 * @param [in]  neuron        Pointer to the neuron buffer.
 *
 * @retval BM_SUCCESS  Successfully.
 * @retval others      Error code.
 */
bmerr_t bmnet_load_neuron(bmnet_t net, uint64_t neuron_offset, int neuron_size,
                           uint8_t *neuron);
/**
 * @name    bmnet_inference
 * @brief   To run inference with a registered bmnet
 * @ingroup bmruntime
 *
 * This API runs inference with a registered bmnet, with both input and output
 * data in system memory. Memory copy between device memory and system memory
 * happens during the call.
 *
 * @param [in]  ctx    BM runtime context.
 * @param [in]  input  Pointer to the input buffer.
 * @param [in]  output Pointer to the output buffer.
 *
 * @retval BM_SUCCESS  Successfully.
 * @retval others      Error code.
 */
bmerr_t bmnet_inference(bmnet_t net, uint8_t *input, uint8_t *output);

/**
 * @name    bmnet_inference_once
 * @brief   To run inference all in one function (for testing only)
 * @ingroup bmruntime
 *
 * This API runs inference all in one function. It registers a bmnet first,
 * runs inference, and then cleanup the bmnet. This function is provided as a
 * wrapper for fast testing, and should NOT been used during production
 * environment.
 *
 * @param [in]  ctx    BM runtime context.
 * @param [in]  input  Pointer to the input buffer.
 * @param [in]  output Pointer to the output buffer.
 *
 * @retval BM_SUCCESS  Successfully.
 * @retval others      Error code.
 */
bmerr_t bmnet_inference_once(
    bmctx_t             ctx,
    uint8_t            *input,
    uint8_t            *output,
    uint32_t            batch_size,
    uint8_t            *weight,
    size_t              weight_size,
    uint8_t            *cmdbuf,
    size_t              cmdbuf_size,
    size_t              neuron_size,
    size_t              input_size,
    uint64_t            output_offset,
    size_t              output_size);

int bmnet_data_transition(
    bmctx_t ctx,
    u64   gaddr_a,
    int   input_n,
    int   input_c,
    int   input_h,
    int   input_w,
    int   stride_n,
    int   stride_c,
    int   stride_h,
    float S,
    float B,
    u64   gaddr_r);

int bmnet_data_copy_u8(
    bmctx_t ctx,
    u64   gaddr_s,
    int   input_n,
    int   input_c,
    int   input_h,
    int   input_w,
    int   stride_n,
    int   stride_c,
    int   stride_h,
    u64   gaddr_d);

float bmmet_get_input_threshold(bmnet_t net);

#ifdef __cplusplus
}
#endif

#endif /* _BMRUNTIME_BMNET_H_ */
