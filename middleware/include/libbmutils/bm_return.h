/*
 * Copyright Bitmain Technologies Inc.
 * Written by:
 *   Pengchao Hu <pengchao.hu@bitmain.com>
 * Created Time: 2018-08-08 15:34
 */

#ifndef LIBBMRUNTIME_BM_RETURN_HPP_
#define LIBBMRUNTIME_BM_RETURN_HPP_

#define RET_IF(_cond, fmt, ...) \
  do { \
    if ((_cond)) { \
      printf("ERROR [%s:%d]:" fmt "\n", \
              __func__, __LINE__, ##__VA_ARGS__); \
      return; \
    } \
  }while(0)

#define RET_ERR(fmt, ...) \
  do { \
      printf("ERROR [%s:%d]:" fmt "\n", \
              __func__, __LINE__, ##__VA_ARGS__); \
      return BM_ERR_FAILURE; \
  }while(0)

#define RET_ERR_IF(_cond, fmt, ...) \
  do { \
    if ((_cond)) { \
      printf("ERROR [%s:%d]:" fmt "\n", \
              __func__, __LINE__, ##__VA_ARGS__); \
      return BM_ERR_FAILURE; \
    } \
  }while(0)

#endif
