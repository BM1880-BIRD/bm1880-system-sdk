#ifndef _BMD_REG_H_
#define _BMD_REG_H_

#ifdef ENV_BMTEST
#define _reg_read(addr) readl((uintptr_t)addr)
#define _reg_write(addr, data) writel((uintptr_t)addr, data)

void _reg_write_mask(uintptr_t addr, u32 mask, u32 data);

#elif defined(ENV_EMU)
u32 _reg_read(uintptr_t addr);
void _reg_write(uintptr_t addr, u32 data);
void _reg_write_mask(uintptr_t addr, u32 mask, u32 data);

#else
#include <linux/io.h>

#define _reg_read(addr) readl((void __iomem *)addr)
#define _reg_write(addr, data) writel(data, (void __iomem *)addr)
void _reg_write_mask(uintptr_t addr, u32 mask, u32 data);
#endif

#endif //_BMD_REG_H_
