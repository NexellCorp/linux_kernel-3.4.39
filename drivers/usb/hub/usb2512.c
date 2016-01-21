/*
 * drivers/usb/hub/usb2512.c
 *
 * Copyright (c) 2015 Usolex Co., Ltd.
 *	Kwangwoo Lee <ischoi@usolex.com>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/delay.h>

#include <mach/platform.h>
#include <mach/devices.h>
#include <mach/soc.h>

#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <linux/file.h>
#include <asm/uaccess.h>

/*
#define	USB_HUB_RESET_PIN			(PAD_GPIO_E+7)
#define	USB_HUB_PORT1_PWR_EN		(PAD_GPIO_B+14)
#define	USB_HUB_PORT2_PWR_EN		(PAD_GPIO_E+15)
*/

struct usb2512 {
	struct i2c_client	*client;
	u16			model;
};

static struct i2c_client	*this_client;

typedef struct 
{
	u8 reg;
	u8 value;
} i2cparam;

i2cparam usb2512_default_setting[] =
{
	{0x00, 0x24},	{0x01, 0x04},	{0x02, 0x12},	{0x03, 0x25},	{0x04, 0xB3},	{0x05, 0x0B},	{0x06, 0x9B},	{0x07, 0x20},
	{0x08, 0x02},	{0x09, 0x00},	{0x0A, 0x00},	{0x0B, 0x00},	{0x0C, 0x01},	{0x0D, 0x32},	{0x0E, 0x01},	{0x0F, 0x32},
	{0x10, 0x32},	{0xF6, 0x00},	{0xF8, 0x00},	{0xFB, 0x21},	{0xFF, 0x01},
};


struct usb2512 *ts;

static int __devinit usb2512_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	int err = 0;
	int i, j, ret;
	u16 value;;
   u8 block_buffer[I2C_SMBUS_BLOCK_MAX + 1];
   s32 nread;

	printk("%s: Enter\n",__FUNCTION__);

   if (!i2c_check_functionality(client->adapter,
					I2C_FUNC_SMBUS_READ_WORD_DATA))
	   return -EIO;
/*
   // Enable Power of Hub Port
   nxp_soc_gpio_set_out_value(USB_HUB_PORT1_PWR_EN, 1);
   nxp_soc_gpio_set_out_value(USB_HUB_PORT2_PWR_EN, 1);

   // reset the hub chip
   nxp_soc_gpio_set_io_dir(USB_HUB_RESET_PIN, 1);
   nxp_soc_gpio_set_out_value(USB_HUB_RESET_PIN, 1);
   mdelay(10);
   nxp_soc_gpio_set_out_value(USB_HUB_RESET_PIN, 0);
   mdelay(10);
   nxp_soc_gpio_set_out_value(USB_HUB_RESET_PIN, 1);
   mdelay(10);
*/

   for (i = 0; i < (sizeof(usb2512_default_setting)/sizeof(i2cparam)); i++)
   {
	   i2c_smbus_write_block_data(client, usb2512_default_setting[i].reg, 1, &(usb2512_default_setting[i].value));
   }
   ts = kzalloc(sizeof(struct usb2512), GFP_KERNEL);

   i2c_set_clientdata(client, ts);
   
   this_client = client;
   
   kfree(ts);
   return 0;

err_free_mem:
   kfree(ts);
   return err;
	
}

static int __devexit usb2512_remove(struct i2c_client *client)
{
	kfree(ts);

	return 0;
}

static const struct i2c_device_id usb2512_idtable[] = {
	{ "usb2512", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, usb2512_idtable);

static struct i2c_driver usb2512_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "usb2512"
	},
	.id_table	= usb2512_idtable,
	.probe		= usb2512_probe,
	.remove		= __devexit_p(usb2512_remove),
};

module_i2c_driver(usb2512_driver);

MODULE_AUTHOR("Joseph Choi <ischoi@usolex.com>");
MODULE_DESCRIPTION("USB Hub Driver");
MODULE_LICENSE("GPL");
