#ifndef __BM_HARDWARE_H
#define __BM_HARDWARE_H

#include <asm/arch/mmio.h>

#ifdef CONFIG_TARGET_BITMAIN_BM1682
//#include <asm/arch/bm1682_regs.h>
#endif

#ifdef CONFIG_TARGET_BITMAIN_BM1684
#include <asm/arch/bm1684_regs.h>
#endif

#ifdef CONFIG_TARGET_BITMAIN_BM1880
#include <asm/arch/bm1880_regs.h>
#endif

#endif
