#ifndef _BM_DEBUG_H_
#define _BM_DEBUG_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#if defined(__cplusplus) && defined(__linux__)
#define USE_GLOG
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define BM_ASSERT(_cond)                                \
  do {                                                  \
    if (!(_cond)) {                                     \
      printf("ASSERT %s: %d: %s: %s\n",                 \
             __FILE__, __LINE__, __func__, #_cond);     \
      fflush(stdout);                                   \
      while(1) ;                                        \
    }                                                   \
} while(0)

void dump_hex(const char * const desc, void *addr, int len);
void dump_data_float_abs(const char * const desc, void *addr, int n, int c, int h, int w);
void dump_data_float(const char * const desc, void *addr, int n, int c, int h, int w);
void dump_data_float_to_file(const char* desc, uint8_t *addr, int size);
void dump_data_int(const char * const desc, void *addr, int n, int c, int h, int w);
void dump_data_int16(const char * const desc, void *addr, int n, int c, int h, int w);
void dump_data_int8(const char * const desc, void *addr, int n, int c, int h, int w);
void dump_matrix_float(const char * const desc, void *addr, int row, int col);
int write_binary_file(uint8_t *buf, size_t len, char *filename);
int write_string_file(const char * const str, char *filename);

#define FW_ERR(fmt, ...)           printf("FW_ERR: " fmt, ##__VA_ARGS__)
#define FW_INFO(fmt, ...)          printf("FW: " fmt, ##__VA_ARGS__)
#ifdef TRACE_FIRMWARE
#define FW_TRACE(fmt, ...)         printf("FW_TRACE: " fmt, ##__VA_ARGS__)
#else
#define FW_TRACE(fmt, ...)
#endif
#ifdef DEBUG_FIRMWARE
#define FW_DBG(fmt, ...)           printf("FW_DBG: " fmt, ##__VA_ARGS__)
#else
#define FW_DBG(fmt, ...)
#endif

#define CMODEL_ERR(fmt, ...)       printf("CM_ERR: " fmt, ##__VA_ARGS__)
#define CMODEL_INFO(fmt, ...)      printf("CM: " fmt, ##__VA_ARGS__)
//#ifdef TRACE_CMODEL
//#define CMODEL_TRACE(fmt, ...)     printf("CM_TRACE: " fmt, ##__VA_ARGS__)
//#else
//#define CMODEL_TRACE(fmt, ...)
//#endif
#ifdef DEBUG_CMODEL
#define CMODEL_DBG(fmt, ...)       printf("CM_DBG: " fmt, ##__VA_ARGS__)
#else
#define CMODEL_DBG(fmt, ...)
#endif

#define DRV_ERR(fmt, ...)          printf("DRV_ERR: " fmt, ##__VA_ARGS__)
#define DRV_INFO(fmt, ...)         printf("DRV: " fmt, ##__VA_ARGS__)
#ifdef TRACE_DRIVER
#define DRV_TRACE(fmt, ...)        printf("DRV_TRACE: " fmt, ##__VA_ARGS__)
#else
#define DRV_TRACE(fmt, ...)
#endif
#ifdef DEBUG_DRIVER
#define DRV_DBG(fmt, ...)          printf("DRV_DBG: " fmt, ##__VA_ARGS__)
#else
#define DRV_DBG(fmt, ...)
#endif

#define BMRT_ERR(fmt, ...)         printf("RT_ERR: " fmt, ##__VA_ARGS__)
#define BMRT_INFO(fmt, ...)        printf("RT: " fmt, ##__VA_ARGS__)
#ifdef TRACE_BMRT
#define BMRT_TRACE(fmt, ...)       printf("RT_TRACE: " fmt, ##__VA_ARGS__)
#else
#define BMRT_TRACE(fmt, ...)
#endif
#ifdef DEBUG_BMRT
#define BMRT_DBG(fmt, ...)         printf("RT_DBG: " fmt, ##__VA_ARGS__)
#else
#define BMRT_DBG(fmt, ...)
#endif

// add for debug bmkernel
#ifdef DEBUG_BMBK
#define BMBK_DBG(fmt, ...)         printf("BK_DBG: " fmt, ##__VA_ARGS__)
#else
#define BMBK_DBG(fmt, ...)
#endif

#define LINE_DBG()                 printf("LINE_DBG: %s %d\n", __func__, __LINE__);

#ifdef __cplusplus
}
#endif

#ifdef USE_GLOG
#include <glog/logging.h>

#define FATAL          LOG(FATAL)
#define LOGE           LOG(ERROR)
#define LOGW           LOG(WARNING)
#define LOGI           LOG(INFO)

#define LOGD           DLOG(INFO)
#define LOGV           VLOG(1)
#define LOGVV          VLOG(2)
#define LOGVVV         VLOG(3)

#define LOGV_IS_ON()   (VLOG_IS_ON(1))
#define LOGVV_IS_ON()  (VLOG_IS_ON(2))
#define LOGVVV_IS_ON() (VLOG_IS_ON(3))

#else
#include <stdio.h>
//#define DEBUG
//#define DEBUG_V1
//#define DEBUG_V2
//#define DEBUG_V3

#define CHECK(_cond)           BM_ASSERT((_cond))
#define CHECK_NOTNULL(ptr)     BM_ASSERT((ptr) != NULL)
#define CHECK_EQ(a, b)         CHECK((a) == (b))
#define CHECK_NE(a, b)         CHECK((a) != (b))
#define CHECK_GT(a, b)         CHECK((a) > (b))
#define CHECK_GE(a, b)         CHECK((a) >= (b))
#define CHECK_LT(a, b)         CHECK((a) < (b))
#define CHECK_LE(a, b)         CHECK((a) <= (b))
#define CHECK_NE(a, b)         CHECK((a) != (b))

#define FATAL(fmt, ...)        do {printf("FATAL: " fmt, ##__VA_ARGS__); BM_ASSERT(0);} while(0)
#define LOGE(fmt, ...)         printf("ERROR: " fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...)         printf("WARNING: " fmt, ##__VA_ARGS__)
#define LOGI(fmt, ...)         printf(fmt, ##__VA_ARGS__)

#ifdef DEBUG
#define LOGD(fmt, ...)         printf(fmt, ##__VA_ARGS__)
#else
#define LOGD(fmt, ...)
#endif

#if defined(DEBUG) && (defined(DEBUG_V1) || defined(DEBUG_V2) || defined(DEBUG_V3))
#define LOGV(fmt, ...)         printf(fmt, ##__VA_ARGS__)
#define LOGV_IS_ON()           (1)
#else
#define LOGV(fmt, ...)
#define LOGV_IS_ON()           (0)
#endif

#if defined(DEBUG) && (defined(DEBUG_V2) || defined(DEBUG_V3))
#define LOGVV(fmt, ...)        printf(fmt, ##__VA_ARGS__)
#define LOGVV_IS_ON()          (1)
#else
#define LOGVV(fmt, ...)
#define LOGVV_IS_ON()          (0)
#endif

#if defined(DEBUG) && defined(DEBUG_V3)
#define LOGVVV(fmt, ...)       printf(fmt, ##__VA_ARGS__)
#define LOGVVV_IS_ON()         (1)
#else
#define LOGVVV(fmt, ...)
#define LOGVVV_IS_ON()         (0)
#endif

#endif

#endif /* _BM_DEBUG_H_ */
