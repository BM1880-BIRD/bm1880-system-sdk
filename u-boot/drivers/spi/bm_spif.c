/*
 * Xilinx SPI driver
 *
 * Supports 8 bit SPI transfers only, with or w/o FIFO
 *
 * Based on bfin_spi.c, by way of altera_spi.c
 * Copyright (c) 2015 Jagan Teki <jteki@openedev.com>
 * Copyright (c) 2012 Stephan Linz <linz@li-pro.net>
 * Copyright (c) 2010 Graeme Smecher <graeme.smecher@mail.mcgill.ca>
 * Copyright (c) 2010 Thomas Chou <thomas@wytron.com.tw>
 * Copyright (c) 2005-2008 Analog Devices Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

// #define DEBUG

#include <config.h>
#include <common.h>
#include <dm.h>
#include <errno.h>
#include <malloc.h>
#include <spi.h>
#include <asm/io.h>

#define SPI_CMD_WREN            0x06
#define SPI_CMD_WRDI            0x04
#define SPI_CMD_RDID            0x9F
#define SPI_CMD_RDSR            0x05
#define SPI_CMD_WRSR            0x01
#define SPI_CMD_READ            0x03
#define SPI_CMD_FAST_READ       0x0B
#define SPI_CMD_PP              0x02
#define SPI_CMD_SE              0xD8
#define SPI_CMD_BE              0xC7

#define SPI_STATUS_WIP          (0x01 << 0)
#define SPI_STATUS_WEL          (0x01 << 1)
#define SPI_STATUS_BP0          (0x01 << 2)
#define SPI_STATUS_BP1          (0x01 << 3)
#define SPI_STATUS_BP2          (0x01 << 4)
#define SPI_STATUS_SRWD         (0x01 << 7)

#define SPI_FLASH_BLOCK_SIZE             256
#define SPI_TRAN_CSR_ADDR_BYTES_SHIFT    8
#define SPI_MAX_FIFO_DEPTH               8

/* register definitions */
#define REG_SPI_CTRL                     0x000
#define REG_SPI_CE_CTRL                  0x004
#define REG_SPI_DLY_CTRL                 0x008
#define REG_SPI_DMMR                     0x00C
#define REG_SPI_TRAN_CSR                 0x010
#define REG_SPI_TRAN_NUM                 0x014
#define REG_SPI_FIFO_PORT                0x018
#define REG_SPI_FIFO_PT                  0x020
#define REG_SPI_INT_STS                  0x028
#define REG_SPI_INT_EN                   0x02C

/* bit definition */
#define BIT_SPI_CTRL_CPHA                    (0x01 << 12)
#define BIT_SPI_CTRL_CPOL                    (0x01 << 13)
#define BIT_SPI_CTRL_HOLD_OL                 (0x01 << 14)
#define BIT_SPI_CTRL_WP_OL                   (0x01 << 15)
#define BIT_SPI_CTRL_LSBF                    (0x01 << 20)
#define BIT_SPI_CTRL_SRST                    (0x01 << 21)
#define BIT_SPI_CTRL_SCK_DIV_SHIFT           0
#define BIT_SPI_CTRL_FRAME_LEN_SHIFT         16

#define BIT_SPI_CE_CTRL_CEMANUAL             (0x01 << 0)
#define BIT_SPI_CE_CTRL_CEMANUAL_EN          (0x01 << 1)

#define BIT_SPI_CTRL_FM_INTVL_SHIFT          0
#define BIT_SPI_CTRL_CET_SHIFT               8

#define BIT_SPI_DMMR_EN                      (0x01 << 0)

#define BIT_SPI_TRAN_CSR_TRAN_MODE_RX        (0x01 << 0)
#define BIT_SPI_TRAN_CSR_TRAN_MODE_TX        (0x01 << 1)
#define BIT_SPI_TRAN_CSR_CNTNS_READ          (0x01 << 2)
#define BIT_SPI_TRAN_CSR_FAST_MODE           (0x01 << 3)
#define BIT_SPI_TRAN_CSR_BUS_WIDTH_1_BIT     (0x0 << 4)
#define BIT_SPI_TRAN_CSR_BUS_WIDTH_2_BIT     (0x01 << 4)
#define BIT_SPI_TRAN_CSR_BUS_WIDTH_4_BIT     (0x02 << 4)
#define BIT_SPI_TRAN_CSR_DMA_EN              (0x01 << 6)
#define BIT_SPI_TRAN_CSR_MISO_LEVEL          (0x01 << 7)
#define BIT_SPI_TRAN_CSR_ADDR_BYTES_NO_ADDR  (0x0 << 8)
#define BIT_SPI_TRAN_CSR_WITH_CMD            (0x01 << 11)
#define BIT_SPI_TRAN_CSR_FIFO_TRG_LVL_1_BYTE (0x0 << 12)
#define BIT_SPI_TRAN_CSR_FIFO_TRG_LVL_2_BYTE (0x01 << 12)
#define BIT_SPI_TRAN_CSR_FIFO_TRG_LVL_4_BYTE (0x02 << 12)
#define BIT_SPI_TRAN_CSR_FIFO_TRG_LVL_8_BYTE (0x03 << 12)
#define BIT_SPI_TRAN_CSR_GO_BUSY             (0x01 << 15)

#define BIT_SPI_TRAN_CSR_TRAN_MODE_MASK      0x0003
#define BIT_SPI_TRAN_CSR_ADDR_BYTES_MASK     0x0700
#define BIT_SPI_TRAN_CSR_FIFO_TRG_LVL_MASK   0x3000

#define BIT_SPI_INT_TRAN_DONE                (0x01 << 0)
#define BIT_SPI_INT_RD_FIFO                  (0x01 << 2)
#define BIT_SPI_INT_WR_FIFO                  (0x01 << 3)
#define BIT_SPI_INT_RX_FRAME                 (0x01 << 4)
#define BIT_SPI_INT_TX_FRAME                 (0x01 << 5)

#define BIT_SPI_INT_TRAN_DONE_EN             (0x01 << 0)
#define BIT_SPI_INT_RD_FIFO_EN               (0x01 << 2)
#define BIT_SPI_INT_WR_FIFO_EN               (0x01 << 3)
#define BIT_SPI_INT_RX_FRAME_EN              (0x01 << 4)
#define BIT_SPI_INT_TX_FRAME_EN              (0x01 << 5)

#define mmio_write_8(a, v) writeb(v, a)
#define mmio_read_8(a) readb(a)

#define mmio_write_16(a, v) writew(v, a)
#define mmio_read_16(a) readw(a)

#define mmio_write_32(a, v) writel(v, a)
#define mmio_read_32(a) readl(a)

/* Bitmain spi priv */
struct bitmain_spi_priv {
	unsigned long ctrl_base;
	unsigned int freq;
	unsigned int mode;
};

static int bitmain_spi_probe(struct udevice *bus)
{
	struct bitmain_spi_priv *priv = dev_get_priv(bus);

	debug("%s\n", __func__);

	priv->ctrl_base = BM_SPIF_BASE;

	// disable DMMR (direct memory mapping read)
	mmio_write_8(priv->ctrl_base + REG_SPI_DMMR, 0);
	// soft reset
	mmio_write_32(priv->ctrl_base + REG_SPI_CTRL,
			mmio_read_32(priv->ctrl_base + REG_SPI_CTRL) | BIT_SPI_CTRL_SRST | 0x3);
	// manual CE control, output high to deactivate flash
	mmio_write_8(priv->ctrl_base + REG_SPI_CE_CTRL, 0x3);

	return 0;
}

static int bitmain_spi_claim_bus(struct udevice *dev)
{
	debug("bitmain_spi_claim_bus\n");
	return 0;
}

static int bitmain_spi_release_bus(struct udevice *dev)
{
	debug("bitmain_spi_release_bus\n");
	return 0;
}

static uint8_t bitmain_spi_data_out_tran(unsigned long spi_base, const uint8_t *src_buf, uint32_t data_bytes)
{
	uint32_t tran_csr = 0;
	uint32_t xfer_size, off;
	uint32_t wait = 0;
	int i;

	if (data_bytes > 65535) {
		printf("data out overflow, should be less than 65535 bytes(%d)\n", data_bytes);
		return -1;
	}

	/* init tran_csr */
	tran_csr = mmio_read_16(spi_base + REG_SPI_TRAN_CSR);
	tran_csr &= ~(BIT_SPI_TRAN_CSR_TRAN_MODE_MASK
	          | BIT_SPI_TRAN_CSR_ADDR_BYTES_MASK
	          | BIT_SPI_TRAN_CSR_FIFO_TRG_LVL_MASK
	          | BIT_SPI_TRAN_CSR_WITH_CMD);
	tran_csr |= BIT_SPI_TRAN_CSR_FIFO_TRG_LVL_8_BYTE;
	tran_csr |= BIT_SPI_TRAN_CSR_TRAN_MODE_TX;

	mmio_write_32(spi_base + REG_SPI_FIFO_PT, 0);

	/* issue tran */
	mmio_write_16(spi_base + REG_SPI_TRAN_NUM, data_bytes);
	tran_csr |= BIT_SPI_TRAN_CSR_GO_BUSY;
	mmio_write_16(spi_base + REG_SPI_TRAN_CSR, tran_csr);

	while((mmio_read_8(spi_base + REG_SPI_INT_STS) & BIT_SPI_INT_WR_FIFO) == 0)
		;

	/* fill data */
	off = 0;
	while (off < data_bytes) {
		if (data_bytes - off >= SPI_MAX_FIFO_DEPTH)
			xfer_size = SPI_MAX_FIFO_DEPTH;
		else
			xfer_size = data_bytes - off;

		wait = 0;
		while ((mmio_read_8(spi_base + REG_SPI_FIFO_PT) & 0xF) != 0) {
			wait++;
			udelay(10);
			if (wait > 30000) { // 300ms
				printf("wait to write FIFO timeout\n");
				return -1;
			}
		}

		/*
		 * odd thing, if we use mmio_write_8, the BIT_SPI_INT_WR_FIFO bit can't
		 * be cleared after transfer done. and BIT_SPI_INT_RD_FIFO bit will not
		 * be set even when REG_SPI_FIFO_PT shows non-zero value.
		 */
		for (i = 0; i < xfer_size; i++)
			mmio_write_8(spi_base + REG_SPI_FIFO_PORT, *(src_buf + off + i));

		off += xfer_size;
	}

	/* wait tran done */
	while((mmio_read_8(spi_base + REG_SPI_INT_STS) & BIT_SPI_INT_TRAN_DONE) == 0)
		;
	mmio_write_32(spi_base + REG_SPI_FIFO_PT, 0);

	/* clear interrupts */
	mmio_write_8(spi_base + REG_SPI_INT_STS, mmio_read_8(spi_base + REG_SPI_INT_STS) & ~BIT_SPI_INT_TRAN_DONE);
	mmio_write_8(spi_base + REG_SPI_INT_STS, mmio_read_8(spi_base + REG_SPI_INT_STS) & ~BIT_SPI_INT_WR_FIFO);
	return 0;
}

static int bitmain_spi_data_in_tran(unsigned long spi_base, uint8_t *dst_buf, int data_bytes)
{
	uint32_t tran_csr = 0;
	int i, xfer_size, off;

	if (data_bytes > 65535) {
		printf("SPI data in overflow, should be less than 65535 bytes(%d)\n", data_bytes);
		return -1;
	}

	/* init tran_csr */
	tran_csr = mmio_read_16(spi_base + REG_SPI_TRAN_CSR);
	tran_csr &= ~(BIT_SPI_TRAN_CSR_TRAN_MODE_MASK
			| BIT_SPI_TRAN_CSR_ADDR_BYTES_MASK
			| BIT_SPI_TRAN_CSR_FIFO_TRG_LVL_MASK
			| BIT_SPI_TRAN_CSR_WITH_CMD);
	tran_csr |= BIT_SPI_TRAN_CSR_FIFO_TRG_LVL_8_BYTE;
	tran_csr |= BIT_SPI_TRAN_CSR_TRAN_MODE_RX;

	mmio_write_32(spi_base + REG_SPI_FIFO_PT, 0);

	/* issue tran */
	mmio_write_16(spi_base + REG_SPI_TRAN_NUM, data_bytes);
	tran_csr |= BIT_SPI_TRAN_CSR_GO_BUSY;
	mmio_write_16(spi_base + REG_SPI_TRAN_CSR, tran_csr);

	while((mmio_read_8(spi_base + REG_SPI_INT_STS) & BIT_SPI_INT_RD_FIFO) == 0 &&
		(mmio_read_8(spi_base + REG_SPI_INT_STS) & BIT_SPI_INT_TRAN_DONE) == 0)
		;

	/* get data */
	off = 0;
	while (off < data_bytes) {
		if (data_bytes - off >= SPI_MAX_FIFO_DEPTH)
			xfer_size = SPI_MAX_FIFO_DEPTH;
		else
			xfer_size = data_bytes - off;

		/*
		 * sometimes we get more than we want, why...
		 */
		while ((mmio_read_8(spi_base + REG_SPI_FIFO_PT) & 0xF) < xfer_size)
			;
		for (i = 0; i < xfer_size; i++)
			*(dst_buf + off + i) = mmio_read_8(spi_base + REG_SPI_FIFO_PORT);

		off += xfer_size;
	}

	/* wait tran done */
	while((mmio_read_8(spi_base + REG_SPI_INT_STS) & BIT_SPI_INT_TRAN_DONE) == 0)
		;
	mmio_write_8(spi_base + REG_SPI_FIFO_PT, 0); // flush unwanted data

	/* clear interrupts */
	mmio_write_8(spi_base + REG_SPI_INT_STS, mmio_read_8(spi_base + REG_SPI_INT_STS) & ~BIT_SPI_INT_TRAN_DONE);
	mmio_write_8(spi_base + REG_SPI_INT_STS, mmio_read_8(spi_base + REG_SPI_INT_STS) & ~BIT_SPI_INT_RD_FIFO);
	return 0;
}

void dump_mem(const void *start_addr, int len)
{
	unsigned int *curr_p = (unsigned int *)start_addr;
	unsigned char *curr_ch_p;
	int _16_fix_num = len/16;
	int tail_num = len%16;
	char buf[16];
	int i, j;

	if (curr_p == NULL || len == 0) {
		printf("nothing to dump!\n");
		return;
	}

	printf("Base: %p\n", start_addr);
	// Fix section
	for (i = 0; i < _16_fix_num; i++) {
		printf("%03X: %08X %08X %08X %08X\n",
				i*16, *curr_p, *(curr_p+1), *(curr_p+2), *(curr_p+3));
		curr_p += 4;
	}

	// Tail section
	if (tail_num > 0) {
		curr_ch_p = (unsigned char *)curr_p;
		for (j = 0; j < tail_num; j++) {
			buf[j] = *curr_ch_p;
			curr_ch_p++;
		}
		for (; j < 16; j++)
			buf[j] = 0;
		curr_p = (unsigned int *)buf;
		printf("%03X: %08X %08X %08X %08X\n",
				i*16, *curr_p, *(curr_p+1), *(curr_p+2), *(curr_p+3));
	}
}

static int bitmain_spi_xfer(struct udevice *dev, unsigned int bitlen,
			    const void *dout, void *din, unsigned long flags)
{
	struct udevice *bus = dev_get_parent(dev);
	struct bitmain_spi_priv *priv = dev_get_priv(bus);
	struct dm_spi_slave_platdata *slave_plat = dev_get_parent_platdata(dev);
	/* assume spi core configured to do 8 bit transfers */
	unsigned int bytes = bitlen / 8;

	debug("spi_xfer: base:0x%lx bus:%i cs:%i bitlen:%i bytes:%i flags:%lx freq:%i dout:0x%p din:0x%p\n",
	      priv->ctrl_base, bus->seq, slave_plat->cs, bitlen, bytes, flags, priv->freq, dout, din);

	if (bitlen == 0)
		goto done;

	if (bitlen % 8) {
		printf("xfer bit length not a multiple of 8 bits\n");
		flags |= SPI_XFER_END;
		goto done;
	}

	if (flags & SPI_XFER_BEGIN)
		mmio_write_8(priv->ctrl_base + REG_SPI_CE_CTRL, 0x2);

	if (dout) {
#ifdef DEBUG
		dump_mem(dout, bytes);
#endif
		bitmain_spi_data_out_tran(priv->ctrl_base, dout, bytes);
	}
	if (din) {
		bitmain_spi_data_in_tran(priv->ctrl_base, din, bytes);
#ifdef DEBUG
		dump_mem(din, bytes);
#endif
	}

done:
	if (flags & SPI_XFER_END)
		mmio_write_8(priv->ctrl_base + REG_SPI_CE_CTRL, 0x3);
	return 0;
}

static int bitmain_spi_set_speed(struct udevice *bus, uint speed)
{
	struct bitmain_spi_priv *priv = dev_get_priv(bus);

	priv->freq = speed;

	debug("bitmain_spi_set_speed: speed=%d\n", priv->freq);

	return 0;
}

static int bitmain_spi_set_mode(struct udevice *bus, uint mode)
{
	struct bitmain_spi_priv *priv = dev_get_priv(bus);

	priv->mode = mode;

	debug("bitmain_spi_set_mode: mode=%d\n", priv->mode);

	return 0;
}

static const struct dm_spi_ops bitmain_spi_ops = {
	.claim_bus	= bitmain_spi_claim_bus,
	.release_bus	= bitmain_spi_release_bus,
	.xfer		= bitmain_spi_xfer,
	.set_speed	= bitmain_spi_set_speed,
	.set_mode	= bitmain_spi_set_mode,
};

static const struct udevice_id bitmain_spi_ids[] = {
	{ .compatible = "bitmain,spif" },
	{ }
};

U_BOOT_DRIVER(bitmain_spi) = {
	.name	= "bitmain_spi",
	.id	= UCLASS_SPI,
	.of_match = bitmain_spi_ids,
	.ops	= &bitmain_spi_ops,
	.priv_auto_alloc_size = sizeof(struct bitmain_spi_priv),
	.probe	= bitmain_spi_probe,
};
