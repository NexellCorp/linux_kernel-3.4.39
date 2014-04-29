/*
 * (C) Copyright 2009
 * jung hyun kim, Nexell Co, <jhkim@nexell.co.kr>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/rfkill.h>

#include <mach/platform.h>

/*
#define	pr_debug	pr_info
*/

#define	GPIO_BT_HOST_WAKE 	(PAD_GPIO_D + 28)
#define	GPIO_BT_DEV_WAKE 	(PAD_GPIO_E + 3)
#define	GPIO_BT_REG_ON		(PAD_GPIO_E + 2)

static const char *strio_name[] = { "GPIOA", "GPIOB", "GPIOC", "GPIOD", "GPIOE", "ALIVE" };
#define	GPIO_GROUP(n)	(strio_name[n/32])
#define	GPIO_BITNR(n)	(n&0x1F)

static struct rfkill *bt_rfkill;

struct bt_bcm_gpio {
	char *name;
	int  gpio;
	int  direction;	/* 0: input, 1: output */
	int  init_val;
	bool power;
};

struct plat_bt_bcm_data {
	struct bt_bcm_gpio *bt_gpios;
	int bt_gpio_num;
};

struct bt_bcm_data {
	struct bt_bcm_gpio *bt_gpios;
	int bt_gpio_num;
	struct rfkill *rfkill;
};

static struct bt_bcm_gpio bt_gpios[] = {
	{
		.name 		= "bt power on/off",
		.gpio 		= GPIO_BT_REG_ON,
		.direction	= 1,
		.init_val	= 0,
		.power		= true,
	},
#if (0)
	{
		.name 		= "bt device wake",
		.gpio 		= GPIO_BT_DEV_WAKE,
		.direction	= 1,
		.init_val	= 1,
	},
	{
		.name 		= "bt host wakeup",
		.gpio 		= GPIO_BT_HOST_WAKE,
		.direction	= 0,
	},
#else
	{
		.name 		= "bt device wake",
		.gpio 		= GPIO_BT_HOST_WAKE,
		.direction	= 1,
		.init_val	= 1,
	},
	{
		.name 		= "bt host wakeup",
		.gpio 		= GPIO_BT_DEV_WAKE,
		.direction	= 0,
	},
#endif
};

static struct plat_bt_bcm_data bt_plat_data = {
	.bt_gpios 	 = bt_gpios,
	.bt_gpio_num = ARRAY_SIZE(bt_gpios),
};

/*
 * Bluetooth BCM Rfkill
 */
static int bt_bcm_rfkill_set_block(void *data, bool blocked)
{
	struct bt_bcm_gpio *btio = data;

	if (NULL == btio) {
		pr_err("bt_bcm: Failed %s, no power data...\n", __func__);
		return -EINVAL;
	}

	pr_info("rfkill: bt bcm set_block %s %s.%d\n",
		 blocked?"Off":"On ", GPIO_GROUP(btio->gpio), GPIO_BITNR(btio->gpio));

	if (!blocked)
		gpio_set_value(btio->gpio, 1);
	else
		gpio_set_value(btio->gpio, 0);

	return 0;
}

static const struct rfkill_ops bt_bcm_rfkill_ops = {
	.set_block = bt_bcm_rfkill_set_block,
};

static int bt_bcm_rfkill_probe(struct platform_device *pdev)
{
	struct plat_bt_bcm_data *bt_data = pdev->dev.platform_data;
	struct bt_bcm_data *bt_bcm = NULL;
	struct bt_bcm_gpio *bt_pwr = NULL;
	struct rfkill *rfkill = NULL;
	int i = 0, ret = 0;

	if (!bt_data || 0 == bt_data->bt_gpio_num)
		return -EINVAL;

	bt_bcm = kzalloc(sizeof(*bt_bcm), GFP_KERNEL);
	if (!bt_bcm)
		return -ENOMEM;

	bt_bcm->bt_gpios = bt_data->bt_gpios;
	bt_bcm->bt_gpio_num = bt_data->bt_gpio_num;

	for (i = 0; bt_bcm->bt_gpio_num > i; i++) {
		struct bt_bcm_gpio *btio = &bt_bcm->bt_gpios[i];
		ret = gpio_request(btio->gpio, btio->name);
		if (unlikely(ret)) {
			pr_err("bt_bcm: Cannot %s get %s.%d ...\n",
				btio->name, GPIO_GROUP(btio->gpio), GPIO_BITNR(btio->gpio));
			goto err_gpio;
		}

		if (btio->direction)
			gpio_direction_output(btio->gpio, btio->init_val);
		else
			gpio_direction_input(btio->gpio);

		if (btio->power)
			bt_pwr = btio;

		pr_debug("bt_bcm: %s %s.%d %s ...\n", btio->name,
			GPIO_GROUP(btio->gpio), GPIO_BITNR(btio->gpio), btio->direction?"output":"input");
	}

	rfkill = rfkill_alloc("BCM Bluetooth", &pdev->dev,
				RFKILL_TYPE_BLUETOOTH, &bt_bcm_rfkill_ops, bt_pwr);
	if (unlikely(!rfkill)) {
		pr_err("bt_bcm: rfkill alloc failed.\n");
		ret =  -ENOMEM;
		goto err_gpio;
	}

	rfkill_init_sw_state(rfkill, false);
	ret = rfkill_register(rfkill);
	if (unlikely(ret)) {
		pr_err("bt_bcm: rfkill register failed.\n");
		ret = -1;
		goto err_rfkill;
	}
	bt_bcm->rfkill = rfkill;

	rfkill_set_sw_state(rfkill, true);
	platform_set_drvdata(pdev, bt_bcm);
	pr_info("bt_bcm: bluetooth rfkill register ....\n");

	return ret;

err_rfkill:
	rfkill_destroy(bt_rfkill);
err_gpio:
	for (i = 0; bt_bcm->bt_gpio_num > i; i++) {
		struct bt_bcm_gpio *btio = &bt_bcm->bt_gpios[i];
		gpio_free(btio->gpio);
	}
	return ret;
}

static int bt_bcm_rfkill_remove(struct platform_device *pdev)
{
	struct bt_bcm_data *bt_bcm = platform_get_drvdata(pdev);
	struct rfkill *rfkill = bt_bcm->rfkill;
	int i;

	rfkill_unregister(rfkill);
	rfkill_destroy(rfkill);

	for (i = 0; bt_bcm->bt_gpio_num > i; i++) {
		struct bt_bcm_gpio *btio = &bt_bcm->bt_gpios[i];
		gpio_free(btio->gpio);
	}
	return 0;
}

static struct platform_device  bt_bcm_device = {
	.name			= "bt_bcm_rfkill",
	.id				= 0,
	.dev			= {
		.platform_data	= &bt_plat_data,
	}
};

static struct platform_driver  bt_bcm_driver = {
	.probe 	= bt_bcm_rfkill_probe,
	.remove = bt_bcm_rfkill_remove,
	.driver = {
		   .name = "bt_bcm_rfkill",
		   .owner = THIS_MODULE,
	},
};

static int __init bt_bcm_rfkill_init(void)
{
	int ret;

	platform_device_register(&bt_bcm_device);
    ret = platform_driver_register(&bt_bcm_driver);
	return ret;
}
static void __exit bt_bcm_rfkill_exit(void)
{
	platform_driver_unregister(&bt_bcm_driver);
	platform_device_unregister(&bt_bcm_device);
}

module_init(bt_bcm_rfkill_init);
module_exit(bt_bcm_rfkill_exit);

MODULE_ALIAS("platform:bcm43241");
MODULE_DESCRIPTION("bt_bcm");
MODULE_LICENSE("GPL");
