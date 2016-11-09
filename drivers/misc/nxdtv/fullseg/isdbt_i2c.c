/*
 * mtv_i2c.c
 *
 * NEXELL MTV I2C driver.
 *
 * Copyright (C) (2013, NEXELL)
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

#include "nxb220.h"
#include "nxb220_internal.h"

#include "isdbt.h"


#if !defined(NXTV_IF_SPI_TSIFx)\
&& (defined(NXTV_IF_TSIF_0) || defined(NXTV_IF_TSIF_1) || defined(NXTV_IF_SPI_SLAVE))

#define I2C_DEV_NAME	"nxb220_i2c"

static const struct i2c_device_id mtv_i2c_device_id[] = {
	{I2C_DEV_NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, mtv_i2c_device_id);

void isdbt_i2c_read_burst(unsigned char i2c_chip_addr, unsigned char reg,
						unsigned char *buf, int size)
{
	int ret;
	u8 wbuf[1] = {reg};

	struct i2c_msg msg[] = {
		 {
		 	.addr = i2c_chip_addr,
		 	.flags = 0,
		 	.buf = wbuf,
		 	.len = 1
		 },
		 {
		 	.addr = i2c_chip_addr,
		 	.flags = I2C_M_RD,
		 	.buf = buf,
		 	.len = size
		 }
	};

	ret = i2c_transfer(isdbt_cb_ptr->i2c_adapter_ptr, msg, 2);
	if (ret != 2) {
		 DMBERR("error: %d\n", ret);
	}
}

unsigned char isdbt_i2c_read(unsigned char i2c_chip_addr, unsigned char reg)
{
	int ret;
	u8 wbuf[1] = {reg};
	u8 rbuf[1];
	
	struct i2c_msg msg[] = {
	     {.addr = i2c_chip_addr, .flags = 0, .buf = wbuf, .len = 1},
	     {.addr = i2c_chip_addr, .flags = I2C_M_RD, .buf = rbuf, .len = 1}
	};

	ret = i2c_transfer(isdbt_cb_ptr->i2c_adapter_ptr, msg, 2);
	if (ret != 2) {
	     DMBERR("error: %d\n", ret);
	     return 0x00;
	}
	
	return rbuf[0];
}



void isdbt_i2c_write(unsigned char i2c_chip_addr, unsigned char reg,
				unsigned char val)
{
	int ret;
	u8 wbuf[2] = {reg, val};
	
	struct i2c_msg msg =
		{.addr = i2c_chip_addr, .flags = 0, .buf = wbuf, .len = 2};

	ret = i2c_transfer(isdbt_cb_ptr->i2c_adapter_ptr, &msg, 1);
	if (ret != 1) {
	     DMBMSG("error: %d\n", ret);
	}
}


static int isdbt_i2c_resume(struct i2c_client *client)
{	
	return 0;
}


static int isdbt_i2c_suspend(struct i2c_client *client, pm_message_t mesg)
{
	return 0;
}


static int isdbt_i2c_remove(struct i2c_client *client)
{
    int ret = 0;

    return ret;
}

static int isdbt_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{	
	DMBMSG("%s probed! addr(0x%02X)\n",
		client->name, client->addr<<1);

	isdbt_cb_ptr->i2c_client_ptr = client;
	isdbt_cb_ptr->i2c_adapter_ptr = to_i2c_adapter(client->dev.parent); 

	return 0;
}

struct i2c_driver isdbt_i2c_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name	= I2C_DEV_NAME,
	},
	.probe = isdbt_i2c_probe,
	.remove = __devexit_p(isdbt_i2c_remove),
	.suspend = isdbt_i2c_suspend,
	.resume  = isdbt_i2c_resume,
	.id_table  = mtv_i2c_device_id,
};
#endif /* #if defined(NXTV_IF_TSIF_0) || defined(NXTV_IF_TSIF_1) || defined(NXTV_IF_SPI_SLAVE) */

