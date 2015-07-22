/*
 * mtv_i2c.c
 *
 * RAONTECH MTV I2C driver.
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
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include "src/raontv.h"
#include "src/raontv_internal.h"

#include "mtv.h"


#if defined(RTV_IF_TSIF) || defined(RTV_IF_SPI_SLAVE)

#define I2C_DEV_NAME	"mtvi2c"

static const struct i2c_device_id mtv_i2c_device_id[] = {
	{I2C_DEV_NAME, 0},
	{},
};


MODULE_DEVICE_TABLE(i2c, mtv_i2c_device_id);


static int mtv_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{	
	DMBMSG("ENTERED!!!!!!!!!!!!!!\n");

	mtv_cb_ptr->i2c_client_ptr = client;
	mtv_cb_ptr->i2c_adapter_ptr = to_i2c_adapter(client->dev.parent); 

	return 0;
}


static int mtv_i2c_remove(struct i2c_client *client)
{
    int ret = 0;

    return ret;
}


void mtv_i2c_read_burst(unsigned char reg, unsigned char *buf, int size)
{
	int ret;
	u8 wbuf[1] = {reg};

	struct i2c_msg msg[] = {
		 {.addr = RAONTV_CHIP_ADDR>>1, .flags = 0, .buf = wbuf, .len = 1},
		 {.addr = RAONTV_CHIP_ADDR>>1, .flags = I2C_M_RD, .buf = buf, .len = size}
	};

	ret = i2c_transfer(mtv_cb_ptr->i2c_adapter_ptr, msg, 2);
	if (ret != 2) {
		 DMBMSG("error: %d\n", ret);
	}
}

unsigned char mtv_i2c_read(unsigned char reg)
{
	int ret;
	u8 wbuf[1] = {reg};
	u8 rbuf[1];
	
	struct i2c_msg msg[] = {
	     {.addr = RAONTV_CHIP_ADDR>>1, .flags = 0, .buf = wbuf, .len = 1},
	     {.addr = RAONTV_CHIP_ADDR>>1, .flags = I2C_M_RD, .buf = rbuf, .len = 1}
	};

	ret = i2c_transfer(mtv_cb_ptr->i2c_adapter_ptr, msg, 2);
	if (ret != 2) {
	     DMBMSG("error: %d\n", ret);
	     return 0x00;
	}
	
	return rbuf[0];
}



void mtv_i2c_write(unsigned char reg, unsigned char val)
{
	int ret;
	u8 wbuf[2] = {reg, val};
	
	struct i2c_msg msg =
		{.addr = RAONTV_CHIP_ADDR>>1, .flags = 0, .buf = wbuf, .len = 2};

	ret = i2c_transfer(mtv_cb_ptr->i2c_adapter_ptr, &msg, 1);
	if (ret != 1) {
	     DMBMSG("error: %d\n", ret);
	}
}


static int mtv_i2c_resume(struct i2c_client *client)
{	
	return 0;
}


static int mtv_i2c_suspend(struct i2c_client *client, pm_message_t mesg)
{
	return 0;
}


struct i2c_driver mtv_i2c_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name	= I2C_DEV_NAME,
	},
	.probe = mtv_i2c_probe,
	.remove = __devexit_p(mtv_i2c_remove),
	.suspend = mtv_i2c_suspend,
	.resume  = mtv_i2c_resume,
	.id_table  = mtv_i2c_device_id,
};
#endif /* #if defined(RTV_IF_TSIF) || defined(RTV_IF_SPI_SLAVE) */

