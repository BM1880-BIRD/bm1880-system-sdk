#ifndef _SUBSYS_INT_H_
#define _SUBSYS_INT_H_

void bm_enable_i2s_int(int port, bool en);
void bm_enable_uart_dma_int(int port, bool en);
void bm_enable_uart_int(int port, bool en);
void bm_enable_sysdma_int(bool en);

#endif
