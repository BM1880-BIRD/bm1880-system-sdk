#include <linux/types.h>

#include "vip_common.h"
#include "vip_sys_reg.h"
#include "reg.h"


static uintptr_t reg_base;

void vip_set_base_addr(void *base)
{
	reg_base = (uintptr_t)base;
}

union vip_sys_clk vip_get_clk(void)
{
	union vip_sys_clk clk;

	clk.raw = _reg_read(reg_base + REG_VIP_CLK_LP);
	return clk;
}

void vip_set_clk(union vip_sys_clk clk)
{
	_reg_write(reg_base + REG_VIP_CLK_LP, clk.raw);
}

union vip_sys_reset vip_get_reset(void)
{
	union vip_sys_reset reset;

	reset.raw = _reg_read(reg_base + REG_VIP_RESETS);
	return reset;
}

void vip_set_reset(union vip_sys_reset reset)
{
	_reg_write(reg_base + REG_VIP_RESETS, reset.raw);
}

union vip_sys_intr vip_get_intr_status(void)
{
	union vip_sys_intr intr;

	intr.raw = _reg_read(reg_base + REG_VIP_INT_STS);
	return intr;
}

union vip_sys_intr vip_get_intr_mask(void)
{
	union vip_sys_intr intr;

	intr.raw = _reg_read(reg_base + REG_VIP_INT_EN);
	return intr;
}

void vip_set_intr_mask(union vip_sys_intr intr)
{
	_reg_write(reg_base + REG_VIP_INT_EN, intr.raw);
}
