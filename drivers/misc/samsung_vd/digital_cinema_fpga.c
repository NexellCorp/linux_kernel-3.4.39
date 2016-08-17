#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>

#define I2C_DEV_NAME	"digital_cinema_fpga"

static int dc_fpga_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = client->adapter;
	return 0;
}

static int dc_fpga_remove(struct i2c_client *client)
{
    kfree(i2c_get_clientdata(client));
    return 0;
}

static const struct i2c_device_id dc_fpga_id[] = {
    {I2C_DEV_NAME, 0 },
    { }
};

static struct i2c_driver dc_fpga_driver = {
    .driver = {
        .name   = I2C_DEV_NAME,
    },
    .probe      = dc_fpga_probe,
    .remove     = dc_fpga_remove,
    .id_table   = dc_fpga_id,
    .class      = I2C_CLASS_DDC | I2C_CLASS_SPD,
};

module_i2c_driver(dc_fpga_driver);

MODULE_AUTHOR("Nexell Co., Ltd");
MODULE_DESCRIPTION("I2C Samsung Digital Cinema Driver");
MODULE_LICENSE("GPL");

