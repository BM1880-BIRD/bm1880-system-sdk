#ifndef BM_CRC32_H_
#define BM_CRC32_H_

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t bm_crc32(const void *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif
