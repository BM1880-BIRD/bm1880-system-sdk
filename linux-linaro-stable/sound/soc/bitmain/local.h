/*
 * I2S driver on bitmain BM1880
 *
 * Copyright 2018 Bitmain
 *
 * Author: EthanChen
 *
 * Licensed under the GPL-2 or later.
 *
 */

#ifndef __BITMAIM_LOCAL_H
#define __BITMAIN_LOCAL_H

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/types.h>
#include <sound/dmaengine_pcm.h>
#include <sound/pcm.h>
#include <sound/designware_i2s.h>

/* common register for all channel */
#define IER		0x000
#define IRER		0x004
#define ITER		0x008
#define CER		0x00C
#define CCR		0x010
#define RXFFR		0x014
#define TXFFR		0x018

/* Interrupt status register fields */
#define ISR_TXFO	BIT(5)
#define ISR_TXFE	BIT(4)
#define ISR_RXFO	BIT(1)
#define ISR_RXDA	BIT(0)

/* I2STxRxRegisters for all channels */
#define LRBR_LTHR(x)	(0x40 * x + 0x020)
#define RRBR_RTHR(x)	(0x40 * x + 0x024)
#define RER(x)		(0x40 * x + 0x028)
#define TER(x)		(0x40 * x + 0x02C)
#define RCR(x)		(0x40 * x + 0x030)
#define TCR(x)		(0x40 * x + 0x034)
#define ISR(x)		(0x40 * x + 0x038)
#define IMR(x)		(0x40 * x + 0x03C)
#define ROR(x)		(0x40 * x + 0x040)
#define TOR(x)		(0x40 * x + 0x044)
#define RFCR(x)		(0x40 * x + 0x048)
#define TFCR(x)		(0x40 * x + 0x04C)
#define RFF(x)		(0x40 * x + 0x050)
#define TFF(x)		(0x40 * x + 0x054)

/* I2SCOMPRegisters */
#define I2S_COMP_PARAM_2	0x01F0
#define I2S_COMP_PARAM_1	0x01F4
#define I2S_COMP_VERSION	0x01F8
#define I2S_COMP_TYPE		0x01FC

#define I2S_USER_REG_CTL	0x1000
#define I2S_USER_REG_CLK	0x1004
#define I2S_USER_REG_FRM	0x1008

#define I2S_USER_MCLK_OUT_ENABLE	(1 << 17)
#define I2S_USER_MCLK_OUT_DISABLE	(0 << 17)
#define I2S_USER_MCLK_IN			(1 << 16)
#define I2S_USER_AUD_IN				(0 << 16)
#define I2S_USER_AU_EN		0x0001

#define I2S_BCLK_MASK	0x00FF

#define WSS_16_CLKCYCLE   0x0020
#define WSS_24_CLKCYCLE   0x0030
#define WSS_32_CLKCYCLE   0x0040

#define WLEN_12_BIT			0x1
#define WLEN_16_BIT			0x2
#define WLEN_20_BIT			0x3
#define WLEN_24_BIT			0x4
#define WLEN_32_BIT			0x5

#define SAMPLEBIT_MASK		0x0700
#define SAMPLEBIT_16_CLKCYCLE   (3 << 8)
#define SAMPLEBIT_24_CLKCYCLE   (1 << 8)
#define SAMPLEBIT_32_CLKCYCLE   (0 << 8)

#define DW_USER_I2S_MASTER_MASK 0x0002
#define DW_USER_I2S_MASTER (0 << 1)
#define DW_USER_I2S_SLAVE  (1 << 1)

/* I2S format:
 * NB: Normal BCLK, data is available at rising edge of BCLK
 * IB: Invert BCLK, data is abailable at falling edge of BCLK
 * NF: Normal FSYNC (WS clock), left channel (ch0) is in low level
 * IF: Invert FSYNC (WS clock), left channle (ch0) is in high level
*/
#define DW_I2S_FMT_INV_MASK 0x00C0
#define DW_I2S_FMT_IB_NF (0 << 6)
#define DW_I2S_FMT_IB_IF (1 << 6)
#define DW_I2S_FMT_NB_NF (2 << 6)
#define DW_I2S_FMT_NB_IF (3 << 6)

#define DW_I2S_FMT_MASK 0x000C
#define DW_I2S_FMT_I2S_MODE (0 << 2)
#define DW_I2S_FMT_LJ_MODE (1 << 2)
#define DW_I2S_FMT_RJ_MODE (2 << 2)

/*
 * Component parameter register fields - define the I2S block's
 * configuration.
 */
#define	COMP1_TX_WORDSIZE_3(r)	(((r) & GENMASK(27, 25)) >> 25)
#define	COMP1_TX_WORDSIZE_2(r)	(((r) & GENMASK(24, 22)) >> 22)
#define	COMP1_TX_WORDSIZE_1(r)	(((r) & GENMASK(21, 19)) >> 19)
#define	COMP1_TX_WORDSIZE_0(r)	(((r) & GENMASK(18, 16)) >> 16)
#define	COMP1_TX_CHANNELS(r)	(((r) & GENMASK(10, 9)) >> 9)
#define	COMP1_RX_CHANNELS(r)	(((r) & GENMASK(8, 7)) >> 7)
#define	COMP1_RX_ENABLED(r)	(((r) & BIT(6)) >> 6)
#define	COMP1_TX_ENABLED(r)	(((r) & BIT(5)) >> 5)
#define	COMP1_MODE_EN(r)	(((r) & BIT(4)) >> 4)
#define	COMP1_FIFO_DEPTH_GLOBAL(r)	(((r) & GENMASK(3, 2)) >> 2)
#define	COMP1_APB_DATA_WIDTH(r)	(((r) & GENMASK(1, 0)) >> 0)

#define	COMP2_RX_WORDSIZE_3(r)	(((r) & GENMASK(12, 10)) >> 10)
#define	COMP2_RX_WORDSIZE_2(r)	(((r) & GENMASK(9, 7)) >> 7)
#define	COMP2_RX_WORDSIZE_1(r)	(((r) & GENMASK(5, 3)) >> 3)
#define	COMP2_RX_WORDSIZE_0(r)	(((r) & GENMASK(2, 0)) >> 0)

/* Number of entries in WORDSIZE and DATA_WIDTH parameter registers */
#define	COMP_MAX_WORDSIZE	(1 << 3)
#define	COMP_MAX_DATA_WIDTH	(1 << 2)

#define MAX_CHANNEL_NUM		8
#define MIN_CHANNEL_NUM		2

union dw_i2s_snd_dma_data {
	struct i2s_dma_data pd;
	struct snd_dmaengine_dai_dma_data dt;
};

struct dw_i2s_dev {
	void __iomem *i2s_base;
	struct clk *clk;
	int active;
	unsigned int capability;
	unsigned int quirks;
	unsigned int i2s_reg_comp1;
	unsigned int i2s_reg_comp2;
	struct device *dev;
	u32 ccr;
	u32 xfer_resolution;
	u32 fifo_th;

	/* data related to DMA transfers b/w i2s and DMAC */
	union dw_i2s_snd_dma_data play_dma_data;
	union dw_i2s_snd_dma_data capture_dma_data;
	struct i2s_clk_config_data config;
	int (*i2s_clk_cfg)(struct i2s_clk_config_data *config);

	/* data related to PIO transfers (TX) */
	bool use_pio;
	struct snd_pcm_substream __rcu *tx_substream;
	unsigned int (*tx_fn)(struct dw_i2s_dev *dev,
			struct snd_pcm_runtime *runtime, unsigned int tx_ptr,
			bool *period_elapsed);
	unsigned int tx_ptr;
};

#if IS_ENABLED(CONFIG_SND_DESIGNWARE_PCM)
void dw_pcm_push_tx(struct dw_i2s_dev *dev);
int dw_pcm_register(struct platform_device *pdev);
#else
void dw_pcm_push_tx(struct dw_i2s_dev *dev) { }
int dw_pcm_register(struct platform_device *pdev)
{
	return -EINVAL;
}
#endif

#endif
