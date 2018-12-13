#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>

#include <mach/platform.h>
#include <mach/devices.h>
#include <mach/soc.h>

struct delayed_work gl852g_late_work;

static void gl852g_reset(void)
{
	nxp_soc_gpio_set_out_value(CFG_IO_HUB_PWREN, 1);
	mdelay(10);
	nxp_soc_gpio_set_out_value(CFG_IO_HUB_NRST, 1);
}

static void gl852g_late_reset_work(struct work_struct *work)
{
	gl852g_reset();
}

static int gl852g_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE))
		return -ENODEV;

	INIT_DELAYED_WORK(&gl852g_late_work, gl852g_late_reset_work);
	schedule_delayed_work(&gl852g_late_work,
                msecs_to_jiffies(1000));
	nxp_soc_gpio_set_out_value(CFG_IO_HUB_PWREN, 0);
	nxp_soc_gpio_set_out_value(CFG_IO_HUB_NRST, 0);

	return 0;
}

static int gl852g_remove(struct i2c_client *client)
{
	kfree(i2c_get_clientdata(client));

	return 0;
}

static int gl852g_suspend(struct i2c_client *client, pm_message_t mesg)
{
	nxp_soc_gpio_set_out_value(CFG_IO_HUB_NRST, 0);

	return 0;
}

static int gl852g_resume(struct i2c_client *client)
{
	// Reset
	nxp_soc_gpio_set_out_value(CFG_IO_HUB_NRST, 1);

	return 0;
}

static const struct i2c_device_id gl852g_id[] = {
	{"gl852g", 0},
	{ }
};

MODULE_DEVICE_TABLE(i2c, gl852g_id);

static struct i2c_driver gl852g_driver = {
	.driver	= {
		.owner	= THIS_MODULE,
		.name	= "gl852g"
	},
	.id_table	= gl852g_id,
	.probe		= gl852g_probe,
	.remove		= gl852g_remove,
	.suspend	= gl852g_suspend,
	.resume		= gl852g_resume,
};

module_i2c_driver(gl852g_driver);

MODULE_AUTHOR("Nexell Co., Ltd");
MODULE_DESCRIPTION("Genesys Logic USB HUB Driver");
MODULE_LICENSE("GPL");

