/**********************************************************************
 * Copyright (C) 2014-2015 Cadence Design Systems, Inc.
 * All rights reserved worldwide.
 ***********************************************************************
 * little_endian.h
 * File contains generic macros for Big Endian coding
 ***********************************************************************/
#ifndef BYTEORDER_LENDIAN_H
#define BYTEORDER_LENDIAN_H

#include "swap.h"

#define cpuToLe32(x) (x)
#define le32ToCpu(x) (x)
#define cpuToLe16(x) ((uint16_t)(x))
#define le16ToCpu(x) ((uint16_t)(x))

#define cpuToBe32(x) ((uint32_t)swap32(x))
#define be32ToCpu(x) ((uint32_t)swap32(x))
#define cpuToBe16(x) ((uint16_t)swap16(x))
#define be16ToCpu(x) ((uint16_t)swap16(x))

/**
 * Macros used for reading 16-bits and 32-bits data from memory which
 * starting address could be unaligned.
 */

#endif /* BYTEORDER_LENDIAN_H */

