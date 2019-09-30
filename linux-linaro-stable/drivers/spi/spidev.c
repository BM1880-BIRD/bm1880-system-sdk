/*
 * Simple synchronous userspace interface to SPI devices
 *
 * Copyright (C) 2006 SWAPP
 *	Andrea Paterniani <a.paterniani@swapp-eng.it>
 * Copyright (C) 2007 David Brownell (simplification, cleanup)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/compat.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/acpi.h>
#include <linux/fb.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <linux/of_reserved_mem.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/delay.h>


/*
 * This supports access to SPI devices using normal userspace I/O calls.
 * Note that while traditional UNIX/POSIX I/O semantics are half duplex,
 * and often mask message boundaries, full SPI support requires full duplex
 * transfers.  There are several kinds of internal message boundaries to
 * handle chipselect management and other protocol options.
 *
 * SPI has a character major number assigned.  We allocate minor numbers
 * dynamically using a bitmask.  You must use hotplug tools, such as udev
 * (or mdev with busybox) to create and destroy the /dev/spidevB.C device
 * nodes, since there is no fixed association of minor numbers with any
 * particular SPI bus or device.
 */
#define SPIDEV_MAJOR			153	/* assigned */
#define N_SPI_MINORS			32	/* ... up to 256 */

static DECLARE_BITMAP(minors, N_SPI_MINORS);


/* Bit masks for spi_device.mode management.  Note that incorrect
 * settings for some settings can cause *lots* of trouble for other
 * devices on a shared bus:
 *
 *  - CS_HIGH ... this device will be active when it shouldn't be
 *  - 3WIRE ... when active, it won't behave as it should
 *  - NO_CS ... there will be no explicit message boundaries; this
 *	is completely incompatible with the shared bus model
 *  - READY ... transfers may proceed when they shouldn't.
 *
 * REVISIT should changing those flags be privileged?
 */
#define SPI_MODE_MASK		(SPI_CPHA | SPI_CPOL | SPI_CS_HIGH \
				| SPI_LSB_FIRST | SPI_3WIRE | SPI_LOOP \
				| SPI_NO_CS | SPI_READY | SPI_TX_DUAL \
				| SPI_TX_QUAD | SPI_RX_DUAL | SPI_RX_QUAD)

struct spidev_data {
	dev_t			devt;
	spinlock_t		spi_lock;
	struct spi_device	*spi;
	struct list_head	device_entry;

	/* TX/RX buffers are NULL unless this device is open (users > 0) */
	struct mutex		buf_lock;
	unsigned int		users;
	u8			*tx_buffer;
	u8			*rx_buffer;
	u32			speed_hz;
};

static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);

#ifdef CONFIG_FB
static struct spidev_data	*spidev;
#endif

static unsigned int bufsiz = 4096;
module_param(bufsiz, uint, 0444);
MODULE_PARM_DESC(bufsiz, "data bytes in biggest supported SPI message");

/*-------------------------------------------------------------------------*/

static ssize_t
spidev_sync(struct spidev_data *spidev, struct spi_message *message)
{
	DECLARE_COMPLETION_ONSTACK(done);
	int status;
	struct spi_device *spi;

	spin_lock_irq(&spidev->spi_lock);
	spi = spidev->spi;
	spin_unlock_irq(&spidev->spi_lock);

	if (spi == NULL)
		status = -ESHUTDOWN;
	else
		status = spi_sync(spi, message);

	if (status == 0)
		status = message->actual_length;

	return status;
}

static inline ssize_t
spidev_sync_write(struct spidev_data *spidev, size_t len)
{
	struct spi_transfer	t = {
			.tx_buf		= spidev->tx_buffer,
			.len		= len,
			.speed_hz	= spidev->speed_hz,
		};
	struct spi_message	m;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	return spidev_sync(spidev, &m);
}

static inline ssize_t
spidev_sync_read(struct spidev_data *spidev, size_t len)
{
	struct spi_transfer	t = {
			.rx_buf		= spidev->rx_buffer,
			.len		= len,
			.speed_hz	= spidev->speed_hz,
		};
	struct spi_message	m;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	return spidev_sync(spidev, &m);
}

/*-------------------------------------------------------------------------*/

/* Read-only message with current device setup */
static ssize_t
spidev_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	struct spidev_data	*spidev;
	ssize_t			status = 0;

	/* chipselect only toggles at start or end of operation */
	if (count > bufsiz)
		return -EMSGSIZE;

	spidev = filp->private_data;

	mutex_lock(&spidev->buf_lock);
	status = spidev_sync_read(spidev, count);
	if (status > 0) {
		unsigned long	missing;

		missing = copy_to_user(buf, spidev->rx_buffer, status);
		if (missing == status)
			status = -EFAULT;
		else
			status = status - missing;
	}
	mutex_unlock(&spidev->buf_lock);

	return status;
}

/* Write-only message with current device setup */
static ssize_t
spidev_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *f_pos)
{
	struct spidev_data	*spidev;
	ssize_t			status = 0;
	unsigned long		missing;

	/* chipselect only toggles at start or end of operation */
	if (count > bufsiz)
		return -EMSGSIZE;

	spidev = filp->private_data;

	mutex_lock(&spidev->buf_lock);
	missing = copy_from_user(spidev->tx_buffer, buf, count);
	if (missing == 0)
		status = spidev_sync_write(spidev, count);
	else
		status = -EFAULT;
	mutex_unlock(&spidev->buf_lock);

	return status;
}

static int spidev_message(struct spidev_data *spidev,
		struct spi_ioc_transfer *u_xfers, unsigned int n_xfers)
{
	struct spi_message	msg;
	struct spi_transfer	*k_xfers;
	struct spi_transfer	*k_tmp;
	struct spi_ioc_transfer *u_tmp;
	unsigned int		n, total, tx_total, rx_total;
	u8			*tx_buf, *rx_buf;
	int			status = -EFAULT;

	spi_message_init(&msg);
	k_xfers = kcalloc(n_xfers, sizeof(*k_tmp), GFP_KERNEL);
	if (k_xfers == NULL)
		return -ENOMEM;

	/* Construct spi_message, copying any tx data to bounce buffer.
	 * We walk the array of user-provided transfers, using each one
	 * to initialize a kernel version of the same transfer.
	 */
	tx_buf = spidev->tx_buffer;
	rx_buf = spidev->rx_buffer;
	total = 0;
	tx_total = 0;
	rx_total = 0;
	for (n = n_xfers, k_tmp = k_xfers, u_tmp = u_xfers;
			n;
			n--, k_tmp++, u_tmp++) {
		k_tmp->len = u_tmp->len;

		total += k_tmp->len;
		/* Since the function returns the total length of transfers
		 * on success, restrict the total to positive int values to
		 * avoid the return value looking like an error.  Also check
		 * each transfer length to avoid arithmetic overflow.
		 */
		if (total > INT_MAX || k_tmp->len > INT_MAX) {
			status = -EMSGSIZE;
			goto done;
		}

		if (u_tmp->rx_buf) {
			/* this transfer needs space in RX bounce buffer */
			rx_total += k_tmp->len;
			if (rx_total > bufsiz) {
				status = -EMSGSIZE;
				goto done;
			}
			k_tmp->rx_buf = rx_buf;
			if (!access_ok(VERIFY_WRITE, (u8 __user *)
						(uintptr_t) u_tmp->rx_buf,
						u_tmp->len))
				goto done;
			rx_buf += k_tmp->len;
		}
		if (u_tmp->tx_buf) {
			/* this transfer needs space in TX bounce buffer */
			tx_total += k_tmp->len;
			if (tx_total > bufsiz) {
				status = -EMSGSIZE;
				goto done;
			}
			k_tmp->tx_buf = tx_buf;
			if (copy_from_user(tx_buf, (const u8 __user *)
						(uintptr_t) u_tmp->tx_buf,
					u_tmp->len))
				goto done;
			tx_buf += k_tmp->len;
		}

		k_tmp->cs_change = !!u_tmp->cs_change;
		k_tmp->tx_nbits = u_tmp->tx_nbits;
		k_tmp->rx_nbits = u_tmp->rx_nbits;
		k_tmp->bits_per_word = u_tmp->bits_per_word;
		k_tmp->delay_usecs = u_tmp->delay_usecs;
		k_tmp->speed_hz = u_tmp->speed_hz;
		if (!k_tmp->speed_hz)
			k_tmp->speed_hz = spidev->speed_hz;
#ifdef VERBOSE
		dev_dbg(&spidev->spi->dev,
			"  xfer len %u %s%s%s%dbits %u usec %uHz\n",
			u_tmp->len,
			u_tmp->rx_buf ? "rx " : "",
			u_tmp->tx_buf ? "tx " : "",
			u_tmp->cs_change ? "cs " : "",
			u_tmp->bits_per_word ? : spidev->spi->bits_per_word,
			u_tmp->delay_usecs,
			u_tmp->speed_hz ? : spidev->spi->max_speed_hz);
#endif
		spi_message_add_tail(k_tmp, &msg);
	}

	status = spidev_sync(spidev, &msg);
	if (status < 0)
		goto done;

	/* copy any rx data out of bounce buffer */
	rx_buf = spidev->rx_buffer;
	for (n = n_xfers, u_tmp = u_xfers; n; n--, u_tmp++) {
		if (u_tmp->rx_buf) {
			if (__copy_to_user((u8 __user *)
					(uintptr_t) u_tmp->rx_buf, rx_buf,
					u_tmp->len)) {
				status = -EFAULT;
				goto done;
			}
			rx_buf += u_tmp->len;
		}
	}
	status = total;

done:
	kfree(k_xfers);
	return status;
}

static struct spi_ioc_transfer *
spidev_get_ioc_message(unsigned int cmd, struct spi_ioc_transfer __user *u_ioc,
		unsigned int *n_ioc)
{
	struct spi_ioc_transfer	*ioc;
	u32	tmp;

	/* Check type, command number and direction */
	if (_IOC_TYPE(cmd) != SPI_IOC_MAGIC
			|| _IOC_NR(cmd) != _IOC_NR(SPI_IOC_MESSAGE(0))
			|| _IOC_DIR(cmd) != _IOC_WRITE)
		return ERR_PTR(-ENOTTY);

	tmp = _IOC_SIZE(cmd);
	if ((tmp % sizeof(struct spi_ioc_transfer)) != 0)
		return ERR_PTR(-EINVAL);
	*n_ioc = tmp / sizeof(struct spi_ioc_transfer);
	if (*n_ioc == 0)
		return NULL;

	/* copy into scratch area */
	ioc = kmalloc(tmp, GFP_KERNEL);
	if (!ioc)
		return ERR_PTR(-ENOMEM);
	if (__copy_from_user(ioc, u_ioc, tmp)) {
		kfree(ioc);
		return ERR_PTR(-EFAULT);
	}
	return ioc;
}

static long
spidev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int			err = 0;
	int			retval = 0;
	struct spidev_data	*spidev;
	struct spi_device	*spi;
	u32			tmp;
	unsigned int		n_ioc;
	struct spi_ioc_transfer	*ioc;

	/* Check type and command number */
	if (_IOC_TYPE(cmd) != SPI_IOC_MAGIC)
		return -ENOTTY;

	/* Check access direction once here; don't repeat below.
	 * IOC_DIR is from the user perspective, while access_ok is
	 * from the kernel perspective; so they look reversed.
	 */
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE,
				(void __user *)arg, _IOC_SIZE(cmd));
	if (err == 0 && _IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ,
				(void __user *)arg, _IOC_SIZE(cmd));
	if (err)
		return -EFAULT;

	/* guard against device removal before, or while,
	 * we issue this ioctl.
	 */
	spidev = filp->private_data;
	spin_lock_irq(&spidev->spi_lock);
	spi = spi_dev_get(spidev->spi);
	spin_unlock_irq(&spidev->spi_lock);

	if (spi == NULL)
		return -ESHUTDOWN;

	/* use the buffer lock here for triple duty:
	 *  - prevent I/O (from us) so calling spi_setup() is safe;
	 *  - prevent concurrent SPI_IOC_WR_* from morphing
	 *    data fields while SPI_IOC_RD_* reads them;
	 *  - SPI_IOC_MESSAGE needs the buffer locked "normally".
	 */
	mutex_lock(&spidev->buf_lock);

	switch (cmd) {
	/* read requests */
	case SPI_IOC_RD_MODE:
		retval = __put_user(spi->mode & SPI_MODE_MASK,
					(__u8 __user *)arg);
		break;
	case SPI_IOC_RD_MODE32:
		retval = __put_user(spi->mode & SPI_MODE_MASK,
					(__u32 __user *)arg);
		break;
	case SPI_IOC_RD_LSB_FIRST:
		retval = __put_user((spi->mode & SPI_LSB_FIRST) ?  1 : 0,
					(__u8 __user *)arg);
		break;
	case SPI_IOC_RD_BITS_PER_WORD:
		retval = __put_user(spi->bits_per_word, (__u8 __user *)arg);
		break;
	case SPI_IOC_RD_MAX_SPEED_HZ:
		retval = __put_user(spidev->speed_hz, (__u32 __user *)arg);
		break;

	/* write requests */
	case SPI_IOC_WR_MODE:
	case SPI_IOC_WR_MODE32:
		if (cmd == SPI_IOC_WR_MODE)
			retval = __get_user(tmp, (u8 __user *)arg);
		else
			retval = __get_user(tmp, (u32 __user *)arg);
		if (retval == 0) {
			u32	save = spi->mode;

			if (tmp & ~SPI_MODE_MASK) {
				retval = -EINVAL;
				break;
			}

			tmp |= spi->mode & ~SPI_MODE_MASK;
			spi->mode = (u16)tmp;
			retval = spi_setup(spi);
			if (retval < 0)
				spi->mode = save;
			else
				dev_dbg(&spi->dev, "spi mode %x\n", tmp);
		}
		break;
	case SPI_IOC_WR_LSB_FIRST:
		retval = __get_user(tmp, (__u8 __user *)arg);
		if (retval == 0) {
			u32	save = spi->mode;

			if (tmp)
				spi->mode |= SPI_LSB_FIRST;
			else
				spi->mode &= ~SPI_LSB_FIRST;
			retval = spi_setup(spi);
			if (retval < 0)
				spi->mode = save;
			else
				dev_dbg(&spi->dev, "%csb first\n",
						tmp ? 'l' : 'm');
		}
		break;
	case SPI_IOC_WR_BITS_PER_WORD:
		retval = __get_user(tmp, (__u8 __user *)arg);
		if (retval == 0) {
			u8	save = spi->bits_per_word;

			spi->bits_per_word = tmp;
			retval = spi_setup(spi);
			if (retval < 0)
				spi->bits_per_word = save;
			else
				dev_dbg(&spi->dev, "%d bits per word\n", tmp);
		}
		break;
	case SPI_IOC_WR_MAX_SPEED_HZ:
		retval = __get_user(tmp, (__u32 __user *)arg);
		if (retval == 0) {
			u32	save = spi->max_speed_hz;

			spi->max_speed_hz = tmp;
			retval = spi_setup(spi);
			if (retval >= 0)
				spidev->speed_hz = tmp;
			else
				dev_dbg(&spi->dev, "%d Hz (max)\n", tmp);
			spi->max_speed_hz = save;
		}
		break;

	default:
		/* segmented and/or full-duplex I/O request */
		/* Check message and copy into scratch area */
		ioc = spidev_get_ioc_message(cmd,
				(struct spi_ioc_transfer __user *)arg, &n_ioc);
		if (IS_ERR(ioc)) {
			retval = PTR_ERR(ioc);
			break;
		}
		if (!ioc)
			break;	/* n_ioc is also 0 */

		/* translate to spi_message, execute */
		retval = spidev_message(spidev, ioc, n_ioc);
		kfree(ioc);
		break;
	}

	mutex_unlock(&spidev->buf_lock);
	spi_dev_put(spi);
	return retval;
}

#ifdef CONFIG_COMPAT
static long
spidev_compat_ioc_message(struct file *filp, unsigned int cmd,
		unsigned long arg)
{
	struct spi_ioc_transfer __user	*u_ioc;
	int				retval = 0;
	struct spidev_data		*spidev;
	struct spi_device		*spi;
	unsigned int			n_ioc, n;
	struct spi_ioc_transfer		*ioc;

	u_ioc = (struct spi_ioc_transfer __user *) compat_ptr(arg);
	if (!access_ok(VERIFY_READ, u_ioc, _IOC_SIZE(cmd)))
		return -EFAULT;

	/* guard against device removal before, or while,
	 * we issue this ioctl.
	 */
	spidev = filp->private_data;
	spin_lock_irq(&spidev->spi_lock);
	spi = spi_dev_get(spidev->spi);
	spin_unlock_irq(&spidev->spi_lock);

	if (spi == NULL)
		return -ESHUTDOWN;

	/* SPI_IOC_MESSAGE needs the buffer locked "normally" */
	mutex_lock(&spidev->buf_lock);

	/* Check message and copy into scratch area */
	ioc = spidev_get_ioc_message(cmd, u_ioc, &n_ioc);
	if (IS_ERR(ioc)) {
		retval = PTR_ERR(ioc);
		goto done;
	}
	if (!ioc)
		goto done;	/* n_ioc is also 0 */

	/* Convert buffer pointers */
	for (n = 0; n < n_ioc; n++) {
		ioc[n].rx_buf = (uintptr_t) compat_ptr(ioc[n].rx_buf);
		ioc[n].tx_buf = (uintptr_t) compat_ptr(ioc[n].tx_buf);
	}

	/* translate to spi_message, execute */
	retval = spidev_message(spidev, ioc, n_ioc);
	kfree(ioc);

done:
	mutex_unlock(&spidev->buf_lock);
	spi_dev_put(spi);
	return retval;
}

static long
spidev_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	if (_IOC_TYPE(cmd) == SPI_IOC_MAGIC
			&& _IOC_NR(cmd) == _IOC_NR(SPI_IOC_MESSAGE(0))
			&& _IOC_DIR(cmd) == _IOC_WRITE)
		return spidev_compat_ioc_message(filp, cmd, arg);

	return spidev_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#else
#define spidev_compat_ioctl NULL
#endif /* CONFIG_COMPAT */

static int spidev_open(struct inode *inode, struct file *filp)
{
	struct spidev_data	*spidev;
	int			status = -ENXIO;

	mutex_lock(&device_list_lock);

	list_for_each_entry(spidev, &device_list, device_entry) {
		if (spidev->devt == inode->i_rdev) {
			status = 0;
			break;
		}
	}

	if (status) {
		pr_debug("spidev: nothing for minor %d\n", iminor(inode));
		goto err_find_dev;
	}

	if (!spidev->tx_buffer) {
		spidev->tx_buffer = kmalloc(bufsiz, GFP_KERNEL);
		if (!spidev->tx_buffer) {
			dev_dbg(&spidev->spi->dev, "open/ENOMEM\n");
			status = -ENOMEM;
			goto err_find_dev;
		}
	}

	if (!spidev->rx_buffer) {
		spidev->rx_buffer = kmalloc(bufsiz, GFP_KERNEL);
		if (!spidev->rx_buffer) {
			dev_dbg(&spidev->spi->dev, "open/ENOMEM\n");
			status = -ENOMEM;
			goto err_alloc_rx_buf;
		}
	}

	spidev->users++;
	filp->private_data = spidev;
	nonseekable_open(inode, filp);

	mutex_unlock(&device_list_lock);
	return 0;

err_alloc_rx_buf:
	kfree(spidev->tx_buffer);
	spidev->tx_buffer = NULL;
err_find_dev:
	mutex_unlock(&device_list_lock);
	return status;
}

static int spidev_release(struct inode *inode, struct file *filp)
{
	struct spidev_data	*spidev;

	mutex_lock(&device_list_lock);
	spidev = filp->private_data;
	filp->private_data = NULL;

	/* last close? */
	spidev->users--;
	if (!spidev->users) {
		int		dofree;

		kfree(spidev->tx_buffer);
		spidev->tx_buffer = NULL;

		kfree(spidev->rx_buffer);
		spidev->rx_buffer = NULL;

		spin_lock_irq(&spidev->spi_lock);
		if (spidev->spi)
			spidev->speed_hz = spidev->spi->max_speed_hz;

		/* ... after we unbound from the underlying device? */
		dofree = (spidev->spi == NULL);
		spin_unlock_irq(&spidev->spi_lock);

		if (dofree)
			kfree(spidev);
	}
	mutex_unlock(&device_list_lock);

	return 0;
}

static const struct file_operations spidev_fops = {
	.owner =	THIS_MODULE,
	/* REVISIT switch to aio primitives, so that userspace
	 * gets more complete API coverage.  It'll simplify things
	 * too, except for the locking.
	 */
	.write =	spidev_write,
	.read =		spidev_read,
	.unlocked_ioctl = spidev_ioctl,
	.compat_ioctl = spidev_compat_ioctl,
	.open =		spidev_open,
	.release =	spidev_release,
	.llseek =	no_llseek,
};

/*-------------------------------------------------------------------------*/

/* The main reason to have this class is to make mdev/udev create the
 * /dev/spidevB.C character device nodes exposing our userspace API.
 * It also simplifies memory management.
 */

static struct class *spidev_class;

#ifdef CONFIG_OF
static const struct of_device_id spidev_dt_ids[] = {
	{ .compatible = "rohm,dh2228fv" },
	{ .compatible = "lineartechnology,ltc2488" },
	{},
};
MODULE_DEVICE_TABLE(of, spidev_dt_ids);
#endif

#ifdef CONFIG_ACPI

/* Dummy SPI devices not to be used in production systems */
#define SPIDEV_ACPI_DUMMY	1

static const struct acpi_device_id spidev_acpi_ids[] = {
	/*
	 * The ACPI SPT000* devices are only meant for development and
	 * testing. Systems used in production should have a proper ACPI
	 * description of the connected peripheral and they should also use
	 * a proper driver instead of poking directly to the SPI bus.
	 */
	{ "SPT0001", SPIDEV_ACPI_DUMMY },
	{ "SPT0002", SPIDEV_ACPI_DUMMY },
	{ "SPT0003", SPIDEV_ACPI_DUMMY },
	{},
};
MODULE_DEVICE_TABLE(acpi, spidev_acpi_ids);

static void spidev_probe_acpi(struct spi_device *spi)
{
	const struct acpi_device_id *id;

	if (!has_acpi_companion(&spi->dev))
		return;

	id = acpi_match_device(spidev_acpi_ids, &spi->dev);
	if (WARN_ON(!id))
		return;

	if (id->driver_data == SPIDEV_ACPI_DUMMY)
		dev_warn(&spi->dev, "do not use this driver in production systems!\n");
}
#else
static inline void spidev_probe_acpi(struct spi_device *spi) {}
#endif

/*-------------------------------------------------------------------------*/

#ifdef CONFIG_FB
#define EDB_GPIO(x) ((x < 32)?(480 + x):((x < 64)?(448 + x - 32):((x <= 67)?(444 + x - 64):(-1))))
//GPIO7
#define BM1880_LCD_RD_GPIO EDB_GPIO(7)
//GPIO51
#define BM1880_LCD_RESET_GPIO EDB_GPIO(51)
#define TFT_COL  320
#define TFT_ROW  240

enum __lcd_display {
	LCD_DRIVE_ST7789V = 0,
	LCD_DRIVE_ILI9341,
	FRAME_BUFFER_ENABLE
};

unsigned int lcd_type = LCD_DRIVE_ILI9341;

static int spi_fb_write_1byte(unsigned char data)
{
	struct spi_transfer	t = {
			.tx_buf		= &data,
			.len		= 1,
			.speed_hz	= 25000000,
			.bits_per_word = 8,
	};
	struct spi_message	m;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	return spidev_sync(spidev, &m);
}

static void LcdWritecomm(unsigned char u1Cmd)
{
	gpio_direction_output(BM1880_LCD_RD_GPIO, 0); //RD=0
	spi_fb_write_1byte(u1Cmd);
}
static void LcdWritedata(unsigned char u1Data)
{
	gpio_direction_output(BM1880_LCD_RD_GPIO, 1); //RD=1
	spi_fb_write_1byte(u1Data);
}

static void LcdIli9341Init(void)
{
	//printk("mark: %s, %d .\n", __FUNCTION__, __LINE__);
	gpio_direction_output(BM1880_LCD_RESET_GPIO, 1); //RESET=1
	mdelay(200);
	gpio_direction_output(BM1880_LCD_RESET_GPIO, 0); //RESET=0
	mdelay(800);
	gpio_direction_output(BM1880_LCD_RESET_GPIO, 1); //RESET=1
	mdelay(800);

	LcdWritecomm(0xCF);
	LcdWritedata(0x00);
	LcdWritedata(0xC1);
	LcdWritedata(0x30);

	LcdWritecomm(0xED);
	LcdWritedata(0x64);
	LcdWritedata(0x03);
	LcdWritedata(0x12);
	LcdWritedata(0x81);

	LcdWritecomm(0xE8);
	LcdWritedata(0x85);
	LcdWritedata(0x00);
	LcdWritedata(0x7A);

	LcdWritecomm(0xCB);
	LcdWritedata(0x39);
	LcdWritedata(0x2C);
	LcdWritedata(0x00);
	LcdWritedata(0x34);
	LcdWritedata(0x02);

	LcdWritecomm(0xF7);
	LcdWritedata(0x20);

	LcdWritecomm(0xEA);
	LcdWritedata(0x00);
	LcdWritedata(0x00);

	LcdWritecomm(0xc0);
	LcdWritedata(0x21);

	LcdWritecomm(0xc1);
	LcdWritedata(0x11);

	LcdWritecomm(0xc5);
	LcdWritedata(0x25);
	LcdWritedata(0x32);

	LcdWritecomm(0xc7);
	LcdWritedata(0xaa);

	LcdWritecomm(0x36);
	//LcdWritedata(0x08);
	LcdWritedata((1<<5)|(0<<6)|(1<<7)|(1<<3));

	LcdWritecomm(0xb6);
	LcdWritedata(0x0a);
	LcdWritedata(0xA2);

	LcdWritecomm(0xb1);
	LcdWritedata(0x00);
	LcdWritedata(0x1B);

	LcdWritecomm(0xf2);
	LcdWritedata(0x00);

	LcdWritecomm(0x26);
	LcdWritedata(0x01);

	LcdWritecomm(0x3a);
	LcdWritedata(0x55);

	LcdWritecomm(0xE0);
	LcdWritedata(0x0f);
	LcdWritedata(0x2D);
	LcdWritedata(0x0e);
	LcdWritedata(0x08);
	LcdWritedata(0x12);
	LcdWritedata(0x0a);
	LcdWritedata(0x3d);
	LcdWritedata(0x95);
	LcdWritedata(0x31);
	LcdWritedata(0x04);
	LcdWritedata(0x10);
	LcdWritedata(0x09);
	LcdWritedata(0x09);
	LcdWritedata(0x0d);
	LcdWritedata(0x00);

	LcdWritecomm(0xE1);
	LcdWritedata(0x00);
	LcdWritedata(0x12);
	LcdWritedata(0x17);
	LcdWritedata(0x03);
	LcdWritedata(0x0d);
	LcdWritedata(0x05);
	LcdWritedata(0x2c);
	LcdWritedata(0x44);
	LcdWritedata(0x41);
	LcdWritedata(0x05);
	LcdWritedata(0x0f);
	LcdWritedata(0x0a);
	LcdWritedata(0x30);
	LcdWritedata(0x32);
	LcdWritedata(0x0F);

	LcdWritecomm(0x11);
	mdelay(120);

	LcdWritecomm(0x29);
}

static void LcdSt7789vInit(void)
{
	gpio_direction_output(BM1880_LCD_RESET_GPIO, 1); //RESET=1
	mdelay(1);  //delay 1ms
	gpio_direction_output(BM1880_LCD_RESET_GPIO, 0); //RESET=0
	mdelay(10);  //delay 10ms
	gpio_direction_output(BM1880_LCD_RESET_GPIO, 1); //RESET=1
	mdelay(120);  //delay 120ms

	//Display Setting
	LcdWritecomm(0x36);
	LcdWritedata(0xE0);

	LcdWritecomm(0x3a);
	LcdWritedata(0x55);
	//ST7789V Frame rate setting
	LcdWritecomm(0xb2);
	LcdWritedata(0x0c);
	LcdWritedata(0x0c);
	LcdWritedata(0x00);
	LcdWritedata(0x33);
	LcdWritedata(0x33);
	LcdWritecomm(0xb7);
	LcdWritedata(0x35); //VGH=13V, VGL=-10.4V
	//--------------------------
	LcdWritecomm(0xbb);
	LcdWritedata(0x19);
	LcdWritecomm(0xc0);
	LcdWritedata(0x2c);
	LcdWritecomm(0xc2);
	LcdWritedata(0x01);
	LcdWritecomm(0xc3);
	LcdWritedata(0x12);
	LcdWritecomm(0xc4);
	LcdWritedata(0x20);
	LcdWritecomm(0xc6);
	LcdWritedata(0x0f);
	LcdWritecomm(0xd0);
	LcdWritedata(0xa4);
	LcdWritedata(0xa1);
	//--------------------------
	LcdWritecomm(0xe0); //gamma setting
	LcdWritedata(0xd0);
	LcdWritedata(0x04);
	LcdWritedata(0x0d);
	LcdWritedata(0x11);
	LcdWritedata(0x13);
	LcdWritedata(0x2b);
	LcdWritedata(0x3f);
	LcdWritedata(0x54);
	LcdWritedata(0x4c);
	LcdWritedata(0x18);
	LcdWritedata(0x0d);
	LcdWritedata(0x0b);
	LcdWritedata(0x1f);
	LcdWritedata(0x23);
	LcdWritecomm(0xe1);
	LcdWritedata(0xd0);
	LcdWritedata(0x04);
	LcdWritedata(0x0c);
	LcdWritedata(0x11);
	LcdWritedata(0x13);
	LcdWritedata(0x2c);
	LcdWritedata(0x3f);
	LcdWritedata(0x44);
	LcdWritedata(0x51);
	LcdWritedata(0x2f);
	LcdWritedata(0x1f);
	LcdWritedata(0x1f);
	LcdWritedata(0x20);
	LcdWritedata(0x23);
	LcdWritecomm(0x11);
	mdelay(120);
	LcdWritecomm(0x29); //display on

}

static void LcdInit(void)
{
	static bool first_init = true;
	int err;

	if (likely(first_init == false))
		return;

	//printk(KERN_INFO, "%s : ", __FUNCTION__);
	spidev->spi->mode = SPI_MODE_0;
	spidev->spi->max_speed_hz = 25000000;
	spidev->spi->bits_per_word = 8;
	err = spi_setup(spidev->spi);
	if (err < 0) {
		//printk("error");
		return;
	}

	err = gpio_request(BM1880_LCD_RD_GPIO, "lcd_chip_rd");
	if (err < 0) {
		//printk(KERN_WARNING, "request gpio for lcd failed!\n");
		return;
	}
	err = gpio_request(BM1880_LCD_RESET_GPIO, "lcd_chip_reset");
	if (err < 0) {
		//printk(KERN_WARNING, "request gpio for lcd failed!\n");
		return;
	}
	switch (lcd_type) {
	case LCD_DRIVE_ST7789V:
		LcdSt7789vInit();
		break;
	case LCD_DRIVE_ILI9341:
		LcdIli9341Init();
		break;
	default:
		break;
	}
	first_init = false;
}

static void LcdBlockWrite(unsigned int Xstart, unsigned int Xend, unsigned int Ystart, unsigned int Yend)
{
	LcdWritecomm(0x2A);
	LcdWritedata(Xstart>>8);
	LcdWritedata(Xstart);
	LcdWritedata(Xend>>8);
	LcdWritedata(Xend);

	LcdWritecomm(0x2B);
	LcdWritedata(Ystart>>8);
	LcdWritedata(Ystart);
	LcdWritedata(Yend>>8);
	LcdWritedata(Yend);

	LcdWritecomm(0x2C);
}

static int spi_fb_pan_display(struct fb_var_screeninfo *var, struct fb_info *info)
{
	struct spi_transfer	t = {
			.tx_buf		= info->screen_base,
			.len		= TFT_COL*TFT_ROW*2,
			.speed_hz	= 25000000,
			//.speed_hz	= spidev->speed_hz,
			.bits_per_word = 16,
	};
	struct spi_message m;

	LcdInit();
	LcdBlockWrite(0, TFT_COL-1, 0, TFT_ROW-1);
	gpio_direction_output(BM1880_LCD_RD_GPIO, 1); //RD=1

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	return spidev_sync(spidev, &m);
}

static struct fb_ops spi_fbops = {
	.owner		= THIS_MODULE,
	.fb_check_var	= NULL,
	.fb_set_par	= NULL,
	.fb_setcolreg	= NULL,
	.fb_blank	= NULL,
	.fb_pan_display	= spi_fb_pan_display,
	.fb_fillrect	= NULL,
	.fb_copyarea	= NULL,
	.fb_imageblit	= NULL,
};
#endif

static int spidev_probe(struct spi_device *spi)
{
	#ifndef CONFIG_FB
	struct spidev_data	*spidev;
	#endif
	int			status;
	unsigned long		minor;
#ifdef CONFIG_FB
	u8 *fb_mem = NULL;
	int fb_memlen = TFT_COL*TFT_ROW*2*2;
	struct fb_info *fb_info;
	int err;
#endif

	/*
	 * spidev should never be referenced in DT without a specific
	 * compatible string, it is a Linux implementation thing
	 * rather than a description of the hardware.
	 */
	if (spi->dev.of_node && !of_match_device(spidev_dt_ids, &spi->dev)) {
		dev_err(&spi->dev, "buggy DT: spidev listed directly in DT\n");
		WARN_ON(spi->dev.of_node &&
			!of_match_device(spidev_dt_ids, &spi->dev));
	}

#ifdef CONFIG_FB
	//fb_mem = devm_kzalloc(&(spi->dev), fb_memlen, GFP_KERNEL);
	fb_mem = kzalloc(fb_memlen, GFP_KERNEL);
	if (!fb_mem)
		return -1;
	//printk("memlen = %d, fbmem = 0x%llX , pa = 0x%llX .\n", fb_memlen, fb_mem, __pa(fb_mem));
	fb_info = framebuffer_alloc(0, NULL);
	if (!fb_info)
		return -1;
	fb_info->var.width = TFT_COL;
	fb_info->var.height = TFT_ROW;
	fb_info->var.xres = TFT_COL;
	fb_info->var.yres = TFT_ROW;
	fb_info->fbops = &spi_fbops;
	fb_info->fix.smem_start = __pa(fb_mem);
	fb_info->fix.smem_len = fb_memlen;
	fb_info->screen_base = fb_mem;
	fb_info->fix.type = FB_TYPE_PACKED_PIXELS;
	fb_info->fix.visual = FB_VISUAL_TRUECOLOR;
	fb_info->fix.line_length = TFT_COL*2;
	fb_info->var.bits_per_pixel = 16;

	fb_info->var.red.offset = 11;
	fb_info->var.red.length = 5;
	fb_info->var.green.offset = 5;
	fb_info->var.green.length = 6;
	fb_info->var.blue.offset = 0;
	fb_info->var.blue.length = 5;
	fb_info->var.transp.offset = 0;
	fb_info->var.transp.length = 0;

	err = register_framebuffer(fb_info);
	if (err != 0)
		return -1;
#endif

	spidev_probe_acpi(spi);

	/* Allocate driver data */
	spidev = kzalloc(sizeof(*spidev), GFP_KERNEL);
	if (!spidev)
		return -ENOMEM;

	/* Initialize the driver data */
	spidev->spi = spi;
	spin_lock_init(&spidev->spi_lock);
	mutex_init(&spidev->buf_lock);

	INIT_LIST_HEAD(&spidev->device_entry);

	/* If we can allocate a minor number, hook up this device.
	 * Reusing minors is fine so long as udev or mdev is working.
	 */
	mutex_lock(&device_list_lock);
	minor = find_first_zero_bit(minors, N_SPI_MINORS);
	if (minor < N_SPI_MINORS) {
		struct device *dev;

		spidev->devt = MKDEV(SPIDEV_MAJOR, minor);
		dev = device_create(spidev_class, &spi->dev, spidev->devt,
				    spidev, "spidev%d.%d",
				    spi->master->bus_num, spi->chip_select);
		status = PTR_ERR_OR_ZERO(dev);
	} else {
		dev_dbg(&spi->dev, "no minor number available!\n");
		status = -ENODEV;
	}
	if (status == 0) {
		set_bit(minor, minors);
		list_add(&spidev->device_entry, &device_list);
	}
	mutex_unlock(&device_list_lock);

	spidev->speed_hz = spi->max_speed_hz;

	if (status == 0)
		spi_set_drvdata(spi, spidev);
	else
		kfree(spidev);

	return status;
}

static int spidev_remove(struct spi_device *spi)
{
	struct spidev_data	*spidev = spi_get_drvdata(spi);

	/* make sure ops on existing fds can abort cleanly */
	spin_lock_irq(&spidev->spi_lock);
	spidev->spi = NULL;
	spin_unlock_irq(&spidev->spi_lock);

	/* prevent new opens */
	mutex_lock(&device_list_lock);
	list_del(&spidev->device_entry);
	device_destroy(spidev_class, spidev->devt);
	clear_bit(MINOR(spidev->devt), minors);
	if (spidev->users == 0)
		kfree(spidev);
	mutex_unlock(&device_list_lock);

	return 0;
}

static struct spi_driver spidev_spi_driver = {
	.driver = {
		.name =		"spidev",
		.of_match_table = of_match_ptr(spidev_dt_ids),
		.acpi_match_table = ACPI_PTR(spidev_acpi_ids),
	},
	.probe =	spidev_probe,
	.remove =	spidev_remove,

	/* NOTE:  suspend/resume methods are not necessary here.
	 * We don't do anything except pass the requests to/from
	 * the underlying controller.  The refrigerator handles
	 * most issues; the controller driver handles the rest.
	 */
};

static int __init spidev_init(void)
{
	int status;

	/* Claim our 256 reserved device numbers.  Then register a class
	 * that will key udev/mdev to add/remove /dev nodes.  Last, register
	 * the driver which manages those device numbers.
	 */
	BUILD_BUG_ON(N_SPI_MINORS > 256);
	status = register_chrdev(SPIDEV_MAJOR, "spi", &spidev_fops);
	if (status < 0)
		return status;

	spidev_class = class_create(THIS_MODULE, "spidev");
	if (IS_ERR(spidev_class)) {
		unregister_chrdev(SPIDEV_MAJOR, spidev_spi_driver.driver.name);
		return PTR_ERR(spidev_class);
	}

	status = spi_register_driver(&spidev_spi_driver);
	if (status < 0) {
		class_destroy(spidev_class);
		unregister_chrdev(SPIDEV_MAJOR, spidev_spi_driver.driver.name);
	}

	return status;
}

#ifdef CONFIG_FB
late_initcall(spidev_init);
#else
module_init(spidev_init);
#endif

static void __exit spidev_exit(void)
{
	spi_unregister_driver(&spidev_spi_driver);
	class_destroy(spidev_class);
	unregister_chrdev(SPIDEV_MAJOR, spidev_spi_driver.driver.name);
}
module_exit(spidev_exit);

MODULE_AUTHOR("Andrea Paterniani, <a.paterniani@swapp-eng.it>");
MODULE_DESCRIPTION("User mode SPI device interface");
MODULE_LICENSE("GPL");
MODULE_ALIAS("spi:spidev");
