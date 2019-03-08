/**********************************************************************
 * Copyright (C) 2014-2015 Cadence Design Systems, Inc.
 * All rights reserved worldwide.
 ***********************************************************************
 * big_endian.h
 * File contains generic macros for Big Endian coding
 ***********************************************************************/

#ifndef BYTEORDER_BENDIAN_H
#define BYTEORDER_BENDIAN_H

#include "swap.h"

#define cpuToLe32(x) ((uint32_t)swap32(x))
#define le32ToCpu(x) ((uint32_t)swap32(x))
#define cpuToLe16(x) ((uint16_t)swap16(x))
#define le16ToCpu(x) ((uint16_t)swap16(x))

#define cpuToBe32(x) (x)
#define be32ToCpu(x) (x)
#define cpuToBe16(x) (x)
#define be16ToCpu(x) (x)

#endif /* BYTEORDER_BENDIAN_H */

