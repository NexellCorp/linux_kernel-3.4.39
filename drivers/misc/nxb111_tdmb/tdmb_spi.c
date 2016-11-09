/*
 * tdmb_spi.c
 *
 * RAONTECH MTV spi driver.
 *
 * Copyright (C) (2011, RAONTECH)
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

#include "mtv319.h"
#include "mtv319_internal.h"

#include "tdmb.h"


#ifdef RTV_IF_SPI

#define SPI_DEV_NAME	"mtvspi"

unsigned char tdmb_spi_read(unsigned char page, unsigned char reg)
{
	int ret;
	u8 out_buf[4], in_buf[4];
	struct spi_message msg;
	struct spi_transfer msg_xfer = {
		.tx_buf = out_buf,
		.rx_buf = in_buf,
		.len = 4,
		.cs_change = 0,
		.delay_usecs = 0
	};

	out_buf[0] = 0x90 | page;
	out_buf[1] = reg;
	out_buf[2] = 1; /* Read size */

	spi_message_init(&msg);
	spi_message_add_tail(&msg_xfer, &msg);

	ret = spi_sync(tdmb_cb_ptr->spi_ptr, &msg);
	if (ret) {
		DMBERR("error: %d\n", ret);
		return 0xFF;
	}

#if 0
	DMBMSG("0x%02X 0x%02X 0x%02X 0x%02X\n",
			in_buf[0], in_buf[1], in_buf[2], in_buf[3]);
#endif

	return in_buf[MTV319_SPI_CMD_SIZE];
}

#if 1
void tdmb_spi_read_burst(unsigned char page, unsigned char reg,
			unsigned char *buf, int size)
{
	int ret;
	u8 out_buf[MTV319_SPI_CMD_SIZE];
	struct spi_message msg;
	struct spi_transfer xfer0 = {
		.tx_buf = out_buf,
		.rx_buf = buf,
		.len = MTV319_SPI_CMD_SIZE,
		.cs_change = 0,
		.delay_usecs = 0,
	};

	struct spi_transfer xfer1 = {
		.tx_buf = buf,
		.rx_buf = buf,
		.len = size,
		.cs_change = 0,
		.delay_usecs = 0,
	};

	out_buf[0] = 0xA0; /* Memory read */
	out_buf[1] = 0x00;
	out_buf[2] = 188; /* Fix */

	spi_message_init(&msg);
	spi_message_add_tail(&xfer0, &msg);
	spi_message_add_tail(&xfer1, &msg);

	ret = spi_sync(tdmb_cb_ptr->spi_ptr, &msg);
	if (ret)
		DMBERR("error: %d\n", ret);	
}

#else
void tdmb_spi_read_burst(unsigned char page, unsigned char reg,
			unsigned char *buf, int size)
{
	int ret;
	u8 out_buf[MTV319_SPI_CMD_SIZE];
	struct spi_message msg;
	struct spi_transfer msg_xfer = {
		.tx_buf = out_buf,
		.rx_buf = buf,
		.len = MTV319_SPI_CMD_SIZE,
		.cs_change = 0,
		.delay_usecs = 0,
	};

	struct spi_transfer msg_xfer1 = {
		.tx_buf = buf,
		.rx_buf = buf,
		.len = size,
		.cs_change = 0,
		.delay_usecs = 0,
	};

	spi_message_init(&msg);
	out_buf[0] = 0xA0; /* Memory read */
	out_buf[1] = 0x00;
	out_buf[2] = 188; /* Fix */

	spi_message_add_tail(&msg_xfer, &msg);
	ret = spi_sync(tdmb_cb_ptr->spi_ptr, &msg);
	if (ret) {
		DMBERR("0 error: %d\n", ret);
		return;
	}

	spi_message_init(&msg);
	spi_message_add_tail(&msg_xfer1, &msg);
	ret = spi_sync(tdmb_cb_ptr->spi_ptr, &msg);
	if (ret)
		DMBERR("1 error: %d\n", ret);	
}
#endif

void tdmb_spi_write(unsigned char page, unsigned char reg, unsigned char val)
{
	u8 out_buf[4];
	u8 in_buf[4];
	struct spi_message msg;
	struct spi_transfer msg_xfer = {
		.tx_buf = out_buf,
		.rx_buf = in_buf,
		.len = 4,
		.cs_change = 0,
		.delay_usecs = 0
	};
	int ret;

	out_buf[0] = 0x80 | page;
	out_buf[1] = reg;
	out_buf[2] = 1; /* size */
	out_buf[3] = val;

	spi_message_init(&msg);
	spi_message_add_tail(&msg_xfer, &msg);

	ret = spi_sync(tdmb_cb_ptr->spi_ptr, &msg);
	if (ret)
		DMBERR("error: %d\n", ret);
}

void tdmb_spi_recover(unsigned char *buf, unsigned int size)
{
	int ret;
	struct spi_message msg;
	struct spi_transfer msg_xfer = {
		.tx_buf = buf,
		.rx_buf = buf,
		.len = size,
		.cs_change = 0,
		.delay_usecs = 0,
	};

	memset(buf, 0xFF, size);

	spi_message_init(&msg);
	spi_message_add_tail(&msg_xfer, &msg);

	ret = spi_sync(tdmb_cb_ptr->spi_ptr, &msg);
	if (ret)
		DMBERR("error: %d\n", ret);
}

static int mtv_spi_probe(struct spi_device *spi)
{
	int ret;

	printk("\n---------------------------------- \n");
	printk("nxb111 spi probe ENTERED!!!!!!!!!!!!!!\n");
	printk("---------------------------------- \n\n");

	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_0;

	ret = spi_setup(spi);
	if (ret)
		return ret;

	tdmb_cb_ptr->spi_ptr = spi;

	return 0;
}


static int tdmb_spi_remove(struct spi_device *spi)
{
	return 0;
}

static struct spi_driver tdmb_spi_driver = {
	.driver = {
		.name = SPI_DEV_NAME,
		.owner = THIS_MODULE,
	},

	.probe = mtv_spi_probe,
	.suspend = NULL,
	.resume	= NULL,
	.remove	= __devexit_p(tdmb_spi_remove),
};

int tdmb_deinit_spi_bus(void)
{
	spi_unregister_driver(&tdmb_spi_driver);

	return 0;
}

int tdmb_init_spi_bus(void)
{
	int ret;

	ret = spi_register_driver(&tdmb_spi_driver);
	if (ret < 0)
		DMBERR("%s SPI driver register failed\n", SPI_DEV_NAME);

	return ret;
}
#endif /* #ifdef RTV_IF_SPI */

