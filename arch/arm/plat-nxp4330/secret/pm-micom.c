/*!
 * @date     2014/03/15
 * @id       " "
 * @version  v1.0.0
 * @brief    
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/pm.h>
#include <mach/platform.h>
#include <mach/soc.h>

#define DEVICE_NAME	"pm-micom"
#define DEVICE_ADDR	0x30
#define DEVICE_BUS 	0

#define MAX_RETRY_I2C_XFER 				(100)

#define MICOM_REG_PWR_STATE				0x00
#define MICOM_REG_BOOT_SOURCE			0x01
#define MICOM_REG_INT_SOURCE			0x02
#define MICOM_REG_IR_VALUE				0x03
#define MICOM_REG_PWR_MANAGER			0x04

#define MICOM_CMD_PWR_OFF				0x00
#define MICOM_CMD_PWR_SUSPEND_FLASH		0x10
#define MICOM_CMD_PWR_SUSPEND_RAM		0x11
#define MICOM_CMD_PWR_RUN				0x0F

#define MICOM_PWRSTATE_OFF				0x00
#define MICOM_PWRSTATE_SUSPEND_FLASH	0x10
#define MICOM_PWRSTATE_SUSPEND_RAM		0x11
#define MICOM_PWRSTATE_RUN				0x0F

struct micom_data {
	struct i2c_client *client;
};

static struct drvPXMICOM {
	struct class* class;
	struct device* device;
	dev_t version;
	struct i2c_client* client;
	struct semaphore sem;
	struct cdev cdev;
} *drvPXMICOM;

static struct i2c_client *mi_client = NULL;

static int micom_smbus_read_byte(struct i2c_client *client, unsigned char reg_addr, unsigned char *data)
{
	s32 dummy;
	dummy = i2c_smbus_read_byte_data(client, reg_addr);
	if (dummy < 0)
	{
		dev_err(&client->dev, "%s: fail!!! \n", __func__);
	    PM_DBGOUT("%s: fail!!! \n", __func__);
		return -1;
	}
	*data = dummy & 0x000000ff;

	return 0;
}

static int micom_smbus_write_byte(struct i2c_client *client,
		unsigned char reg_addr, unsigned char data)
{
	s32 dummy;

	dummy = i2c_smbus_write_byte_data(client, reg_addr, data);
	if (dummy < 0)
	{
		dev_err(&client->dev, "%s: fail!!! \n", __func__);
	    PM_DBGOUT("%s: fail!!! \n", __func__);
		return -1;
	}
	return 0;
}

static int micom_smbus_read_byte_block(struct i2c_client *client,
		unsigned char reg_addr, unsigned char *data, unsigned char len)
{
	s32 dummy;
	dummy = i2c_smbus_read_i2c_block_data(client, reg_addr, len, data);
	if (dummy < 0)
	{
		dev_err(&client->dev, "%s: fail!!! \n", __func__);
	    PM_DBGOUT("%s: fail!!! \n", __func__);
		return -1;
	}
	return 0;
}

static int bma_i2c_burst_read(struct i2c_client *client, u8 reg_addr,
		u8 *data, u16 len)
{
	int retry;

	struct i2c_msg msg[] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .len = 1,
		 .buf = &reg_addr,
		},

		{
		 .addr = client->addr,
		 .flags = I2C_M_RD,
		 .len = len,
		 .buf = data,
		 },
	};

	for (retry = 0; retry < MAX_RETRY_I2C_XFER ; retry++) {
		if (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg)) > 0)
			break;
		else
			mdelay(1);
	}

	if (MAX_RETRY_I2C_XFER  <= retry) {
		dev_err(&client->dev, "%s: fail!!! \n", __func__);
	    PM_DBGOUT("%s: fail!!! \n", __func__);
		return -EIO;
	}

	return 0;
}

static ssize_t micom_suspend_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data=0;
	//struct i2c_client *client = to_i2c_client(dev);

	return sprintf(buf, "%d\n", data);
}

static ssize_t micom_suspend_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	//struct i2c_client *client = to_i2c_client(dev);

	error = strict_strtoul(buf, 10, &data);
	if (error)
		return error;

	return count;
}
static DEVICE_ATTR(suspend, S_IRUGO|S_IWUSR|S_IWGRP|S_IWOTH,
		micom_suspend_show, micom_suspend_store);

static struct attribute *micom_attributes[] = {
	&dev_attr_suspend.attr,
	NULL
};

static struct attribute_group micom_attribute_group = {
	.attrs = micom_attributes
};


static int micom_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int err = 0;
//	int value_gpio = 0;
//	unsigned char value=0;
	struct micom_data *data;

	data = kzalloc(sizeof(struct micom_data), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto exit;
	}
	i2c_set_clientdata(client, data);
	data->client = client;
	mi_client = client;
/*
//	value_gpio = gpio_get_value(CFG_MSP_READ);
//	printk(KERN_ERR "CFG_MSP_READ=[%d]\n", value_gpio);

//	err=micom_smbus_read_byte(client, MICOM_REG_PWR_STATE, &value);
	switch(value)
	{
		case MICOM_PWRSTATE_OFF:
			printk(KERN_INFO "%s() : Micom Power : OFF \n", __func__);
			break;
		case MICOM_PWRSTATE_SUSPEND_FLASH:
			printk(KERN_INFO "%s() : Micom Power : SUSPEND_FLASH \n", __func__);
			break;
		case MICOM_PWRSTATE_SUSPEND_RAM:
			printk(KERN_INFO "%s() : Micom Power : SUSPEND_RAM \n", __func__);
			break;
		case MICOM_PWRSTATE_RUN:
			printk(KERN_INFO "%s() : Micom Power : RUN \n", __func__);
			break;
		default:
			printk(KERN_INFO "%s() : Micom Power : unkown!! \n", __func__);
			break;
	}
*/
	err = sysfs_create_group(&client->dev.kobj, &micom_attribute_group);
	if (err < 0)
		goto error_sysfs;

	return 0;

error_sysfs:
	kfree(data);
exit:
	return err;
}

int _pm_check_wakeup_dev(char *dev, int io) 
{
	printk("chekc wakeup dev:%s io[%d]\n", dev, io);
	return 1;
}

static int __devexit micom_remove(struct i2c_client *client)
{
	struct micom_data *data = i2c_get_clientdata(client);

    PM_DBGOUT("+%s\n", __func__);

	sysfs_remove_group(&client->dev.kobj, &micom_attribute_group);
	kfree(data);

    PM_DBGOUT("-%s\n", __func__);
	return 0;
}

static void micom_shutdown(void)
{
	struct micom_data *data = i2c_get_clientdata(mi_client);

    PM_DBGOUT("+%s\n", __func__);

	micom_smbus_write_byte(data->client, MICOM_REG_PWR_STATE, MICOM_CMD_PWR_OFF);

    PM_DBGOUT("-%s\n", __func__);
	return;
}

#ifdef CONFIG_PM

static int micom_suspend(struct i2c_client *client, pm_message_t mesg)
{
	
	struct micom_data *data = i2c_get_clientdata(client);
    PM_DBGOUT("+%s\n", __func__);
	
	micom_smbus_write_byte(data->client, MICOM_REG_PWR_STATE, MICOM_CMD_PWR_SUSPEND_RAM);
	
    PM_DBGOUT("-%s\n", __func__);
	return 0;
}

static int micom_resume(struct i2c_client *client)
{
//	int err = 0;
//	unsigned char value=0;

//	struct micom_data *data = i2c_get_clientdata(client);
    PM_DBGOUT("+%s\n", __func__);

	//micom_smbus_write_byte(data->client, MICOM_REG_PWR_STATE, MICOM_CMD_PWR_RUN);

    PM_DBGOUT("-%s\n", __func__);
	return 0;
}

#else

#define micom_suspend		NULL
#define micom_resume		NULL

#endif /* CONFIG_PM */

static const struct i2c_device_id micom_id[] = {
	{ DEVICE_NAME, 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, micom_id);

static struct i2c_driver micom_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= DEVICE_NAME,
	},
	.suspend	= micom_suspend,
	.resume		= micom_resume,
	.id_table	= micom_id,
	.probe		= micom_probe,
	.remove		= __devexit_p(micom_remove)
};	

static struct i2c_board_info info =
{
    I2C_BOARD_INFO(DEVICE_NAME, DEVICE_ADDR),
};

static ssize_t drvPXMICOM_read(struct file* filp, char* buff, size_t length, loff_t* offset)
{
    buff[0] = 1;
    return 1;
}

static ssize_t drvPXMICOM_write(struct file* filp, const char* buff, size_t len, loff_t* off)
{
	unsigned int i=0;

	micom_smbus_write_byte(mi_client, buff[0], buff[1]);

	return 1;
}

static struct file_operations fops =
{
    .read = drvPXMICOM_read,
    .write = drvPXMICOM_write
};


static int __init micom_init(void)
{
	int reval = -ENOMEM;
	int result;
	struct i2c_adapter* adapter;
	struct i2c_client* client;

	printk(KERN_ALERT"+%s\n", __func__);
	pm_power_off_prepare = micom_shutdown;
	nxp_check_pm_wakeup_dev = _pm_check_wakeup_dev;
	
    drvPXMICOM = kmalloc(sizeof *drvPXMICOM, GFP_KERNEL);
	if(!drvPXMICOM)
	{
		printk(KERN_ALERT"pm-micom.c: cannot allocate memory for drvPXMICOM driver\n");
	}
    adapter = i2c_get_adapter(DEVICE_BUS);
	if(!adapter)
	{
		printk(KERN_ALERT"pm-micom.c: cannot get adapter\n");
	}
    client = i2c_new_device(adapter, &info);
	if(!client)
	{
		printk(KERN_ALERT"(%s-%s():%4d): Cannot create new device \n", __FILE__, __func__, __LINE__);
	}else{
		printk(KERN_ALERT"(%s-%s():%4d): Create new device \n", __FILE__, __func__, __LINE__);
	}

    drvPXMICOM->version = MKDEV(0,0);
    reval = alloc_chrdev_region(&drvPXMICOM->version, 0, 1, DEVICE_NAME);
	if(reval < 0)
	{
		printk(KERN_ALERT"pm-micom.c: error getting major number %d\r\n", reval);
	}

    drvPXMICOM->class = class_create(THIS_MODULE, DEVICE_NAME);
	if(!drvPXMICOM->class)
	{
		printk(KERN_ALERT"pm-micom.c: error creating class\n");
	}

    drvPXMICOM->device = device_create(drvPXMICOM->class, NULL, drvPXMICOM->version, NULL, DEVICE_NAME);
	if(!drvPXMICOM->device)
	{
		printk(KERN_ALERT"pm-micom.c: error creating device\n");
	}

    cdev_init(&drvPXMICOM->cdev, &fops);
    drvPXMICOM->cdev.owner = THIS_MODULE;
    drvPXMICOM->cdev.ops = &fops;
    reval = cdev_add(&drvPXMICOM->cdev, drvPXMICOM->version, 1);

	if(reval)
	{
		printk(KERN_ALERT"pm-micom.c: fail to add cdev\n");
	}
//	return i2c_add_driver(&micom_driver);

	result = i2c_add_driver(&micom_driver);
	printk(KERN_ALERT"pm-micom.c: i2c_add_driver return : %d\r\n", result);	
	return result;
}

static void __exit micom_exit(void)
{
    device_destroy(drvPXMICOM->class, drvPXMICOM->version);
    class_destroy(drvPXMICOM->class);
    unregister_chrdev_region(drvPXMICOM->version, 1);
    i2c_unregister_device(drvPXMICOM->client);
    kfree(drvPXMICOM);
	i2c_del_driver(&micom_driver);
    PM_DBGOUT("-%s\n", __func__);
}

module_init(micom_init);
module_exit(micom_exit);

MODULE_AUTHOR(" < @nexell.co.kr>");
MODULE_DESCRIPTION("micom driver");
MODULE_LICENSE("GPL");
