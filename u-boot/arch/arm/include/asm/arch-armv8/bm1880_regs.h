#ifndef __BM_REGS_H
#define __BM_REGS_H

#include <asm/arch/mmio.h>

#define TOP_BASE      0x50010000
#define PINMUX_BASE     (TOP_BASE + 0x400)

#define PINMUX_UART0    0x0
#define PINMUX_I2C0     0x1
#define PINMUX_SPIF     0x2
#define PINMUX_EMMC     0x3
#define PINMUX_NAND     0x4
#define PINMUX_SDIO     0x5
#define PINMUX_UART1    0x6

/* rst */
#define REG_TOP_SOFT_RST        0xc00
#define BIT_TOP_SOFT_RST_USB    (1 << 9)
#define BIT_TOP_SOFT_RST_SDIO   (1 << 14)
#define BIT_TOP_SOFT_RST_NAND   (1 << 12)

/* irq */
#define IRQ_LEVEL   0
#define IRQ_EDGE    3

#endif
