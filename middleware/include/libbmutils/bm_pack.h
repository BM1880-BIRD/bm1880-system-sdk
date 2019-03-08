#ifndef __BM_PACK_H__
#define __BM_PACK_H__

#include <bmkernel/bm_kernel.h>

#define BM_WEIGHT_ENC_SIGNED (1 << 0) /** the input data is signed value */
#define BM_WEIGHT_ENC_DRYRUN (1 << 1) /** just return size of output buffer don't allocate*/

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @name    bm_weight_enc_int8
 * @brief   encode the 8bit weight value
 * @ingroup bmkernekl
 *
 * @param [in]  ibuf   pointer to input buffer
 * @param [in]  isz    size in byte
 * @param [out]  obuf  assigned a new allocate buffer
 * @param [out]  osz   assigned the size of output buf in byte
 * @param [in]  flag  addition options
 *
 * @retval BM_SUCCESS                 success
 * @retval BM_ERR_DATA                > 0xFFFFFF non-zero value
 * @retval BM_ERR_INVALID_ARGUMENT    intput argument is null pointer
 */
bmerr_t bm_weight_enc_int8(const uint8_t *ibuf, size_t isz, uint8_t **obuf, size_t *osz,
                           unsigned flag);

/**
 * @name    bm_weight_dec_int8
 * @brief   decode the 8bit packed weight value
 * @ingroup cmodel
 *
 * @param [in]  ibuf   pointer to input buffer
 * @param [in]  icnt   # of weight element in original weight data
 * @param [out]  obuf  assigned a new allocate buffer
 * @param [out]  osz   assigned the size of output buf in byte
 *
 * @retval BM_SUCCESS                 success
 * @retval BM_ERR_DATA                it is not compressed weight data or wrong
 *                                    bit depth
 * @retval BM_ERR_INVALID_ARGUMENT    intput argument is null pointer
 */
bmerr_t bm_weight_dec_int8(const uint8_t *ibuf, size_t icnt, uint8_t **obuf, size_t *osz);

#ifdef __cplusplus
}
#endif

#endif /* __BM_PACK_H__ */
