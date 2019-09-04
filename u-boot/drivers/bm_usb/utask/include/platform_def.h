/*
 * Copyright (c) 2015-2016, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __PLATFORM_DEF_H__
#define __PLATFORM_DEF_H__

#include <common.h>

#if defined(CONFIG_TARGET_BITMAIN_BM1880)
#include <asm/arch/bm1880_regs.h>
#elif defined(CONFIG_TARGET_BITMAIN_BM1882)
#include <../../../board/bitmain/bm1882/bm1882_reg.h>
#else
#error "use bm_utask at wrong platform"
#endif

#define DISABLE_DCACHE

#endif /* __PLATFORM_DEF_H__ */
