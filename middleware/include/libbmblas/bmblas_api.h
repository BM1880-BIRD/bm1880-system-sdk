/*****************************************************************************
*
*    Copyright (c) 2016-2022 by Bitmain Technologies Inc. All rights reserved.
*
*    The material in this file is confidential and contains trade secrets
*    of Bitmain Technologies Inc. This is proprietary information owned by
*    Bitmain Technologies Inc. No part of this work may be disclosed,
*    reproduced, copied, transmitted, or used in any way for any purpose,
*    without the express written permission of Bitmain Technologies Inc.
*
*****************************************************************************/

#ifndef _BMDNN_API_H_
#define _BMDNN_API_H_
#include <libbmutils/bm_debug.h>
#include <bmkernel/bm168x/bmkernel_1682.h>
#include <libbmruntime/bmruntime_bmkernel.h>
#include <libbmruntime/bmruntime_bmnet.h>
#include <libbmruntime/bmruntime.h>
#include <bmkernel/bm1880/bmkernel_1880.h>
#include <bmkernel/bm1882/bmkernel_1882.h>
#include <libbmutils/bm_native_ref.h>

typedef enum {
  FORMAT_U8,
  FORMAT_FP32,
} TYPE_SUPPORTED;

void bmblas_gemm (
    bool is_A_trans,
    bool is_B_trans,
    size_t M, size_t N, size_t K,
    u64 gaddr_a,
    u64 gaddr_b,
    u64 gaddr_c,
    TYPE_SUPPORTED type);

void bmblas_gemm (
    bmk1880_context_t* ctx,
    size_t M, size_t N, size_t K,
    u64 gaddr_a,
    u64 gaddr_b,
    u64 gaddr_c);

#endif /* _BMDNN_API_H_ */
