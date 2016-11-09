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

#include "mtv319.h"
#include "mtv319_internal.h"

#include "tdmb.h"


#if defined(CONFIG_TDMB_TSIF)

#define I2C_DEV_NAME	"tdmbi2c"

static const struct i2c_device_id mtv_i2c_device_id[] = {
	{I2C_DEV_NAME, 0},
	{},
};


MODULE_DEVICE_TABLE(i2c, mtv_i2c_device_id);

void tdmb_i2c_read_burst(unsigned char reg, unsigned char *buf, int size)
{
	int ret;
	u8 wbuf[1] = {reg};

	struct i2c_msg msg[2] = {
		 {
		 	.addr = RTV_CHIP_ADDR>>1,
		 	.flags = 0,
		 	.buf = wbuf,
		 	.len = 1
		 },
		 {
		 	.addr = RTV_CHIP_ADDR>>1,
		 	.flags = I2C_M_RD,
		 	.buf = buf,
		 	.len = size
		 }
	};

	ret = i2c_transfer(tdmb_cb_ptr->i2c_adapter_ptr, msg, 2);
	if (ret != 2) {
		 DMBMSG("error: %d\n", ret);
	}
}

unsigned char tdmb_i2c_read(unsigned char chipid, unsigned char reg)
{
	int ret;
	u8 wbuf[1] = {reg};
	u8 rbuf[1];
	
	struct i2c_msg msg[2] = {
	     {.addr = chipid>>1, .flags = 0, .buf = wbuf, .len = 1},
	     {.addr = chipid>>1, .flags = I2C_M_RD, .buf = rbuf, .len = 1}
	};

	ret = i2c_transfer(tdmb_cb_ptr->i2c_adapter_ptr, msg, 2);
	if (ret != 2) {
	     DMBMSG("error: %d\n", ret);
	     return 0x00;
	}
	
	return rbuf[0];
}



void tdmb_i2c_write(unsigned char chipid, unsigned char reg, unsigned char val)
{
	int ret;
	u8 wbuf[2] = {reg, val};
	
	struct i2c_msg msg =
		{.addr = chipid>>1, .flags = 0, .buf = wbuf, .len = 2};

	ret = i2c_transfer(tdmb_cb_ptr->i2c_adapter_ptr, &msg, 1);
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


static int mtv_i2c_remove(struct i2c_client *client)
{
    int ret = 0;

    return ret;
}

static int mtv_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{	
	DMBMSG("[mtv_i2c_probe] %s probed! addr(0x%02X)\n",
		client->name, client->addr<<1);

	tdmb_cb_ptr->i2c_client_ptr = client;
	tdmb_cb_ptr->i2c_adapter_ptr = to_i2c_adapter(client->dev.parent); 

	return 0;
}

static struct i2c_driver tdmb_i2c_driver = {
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

int tdmb_deinit_i2c_bus(void)
{
	i2c_del_driver(&tdmb_i2c_driver);

	return 0;
}

int tdmb_init_i2c_bus(void)
{
	int ret;

	ret = i2c_add_driver(&tdmb_i2c_driver);
	if (ret < 0)
		DMBERR("%s I2C driver register failed\n", I2C_DEV_NAME);

	return ret;
}
#endif /* #if defined(CONFIG_TDMB_TSIF) */

