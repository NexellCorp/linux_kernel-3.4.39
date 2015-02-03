/*
 * mp8845c-regulator.c  --  Regulator driver for the mp8845c
 *
 * Copyright(C) 2009. Nexell Co., <pjsin865@nexell.co.kr>
 *
 * See file CREDITS for list of people who contributed to this project.
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
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/mp8845c-regulator.h>

static int mp8845c_read(struct i2c_client *client, u8 reg, uint8_t *val)
{
	int ret = 0;

	ret = i2c_smbus_read_byte_data(client, reg);

	if (ret < 0) {
		dev_err(&client->dev, "failed reading at 0x%02x\n", reg);
		return ret;
	}

	*val = (uint8_t)ret;

	dev_dbg(&client->dev, "read reg=%x, val=%x\n", reg, *val);

	return 0;
}

static int mp8845c_write(struct i2c_client *client, u8 reg, uint8_t val)
{
	int ret = 0;

	ret = i2c_smbus_write_byte_data(client, reg, val);

	if (ret < 0) {
		dev_err(&client->dev, "failed writing 0x%02x to 0x%02x\n", val, reg);
		return ret;
	}

	dev_dbg(&client->dev, "write reg=%x, val=%x\n", reg, val);

	return 0;
}

int mp8845c_set_bits(struct i2c_client *client, u8 reg, uint8_t bit_mask)
{
	uint8_t reg_val;
	int ret = 0;

	ret = mp8845c_read(client, reg, &reg_val);

	if (ret < 0) 
		return ret;

	if ((reg_val & bit_mask) != bit_mask) {
		reg_val |= bit_mask;
		ret = mp8845c_write(client, reg, reg_val);
	}
	return ret;
}

int mp8845c_clr_bits(struct i2c_client *client, u8 reg, uint8_t bit_mask)
{
	uint8_t reg_val;
	int ret = 0;

	ret = mp8845c_read(client, reg, &reg_val);

	if (ret < 0) 
		return ret;

	if (reg_val & bit_mask) {
		reg_val &= ~bit_mask;
		ret = mp8845c_write(client, reg, reg_val);
	}
	return ret;
}

int mp8845c_update(struct i2c_client *client, u8 reg, uint8_t val, uint8_t mask)
{
	uint8_t reg_val;
	int ret = 0;

	ret = mp8845c_read(client, reg, &reg_val);

	if (ret < 0) 
		return ret;

	if ((reg_val & mask) != val) {
		reg_val = (reg_val & ~mask) | (val & mask);
		ret = mp8845c_write(client, reg, reg_val);
	}
	return ret;
}

static int mp8845c_regulator_enable_time(struct regulator_dev *rdev)
{
	struct mp8845c_regulator *ri = rdev_get_drvdata(rdev);

	return ri->en_delay;
}

static int mp8845c_reg_is_enabled(struct regulator_dev *rdev)
{
	struct mp8845c_regulator *ri = rdev_get_drvdata(rdev);
	uint8_t control;
	int ret;

	ret = mp8845c_read(ri->client, ri->reg_en_reg, &control);
	if (ret < 0) {
		dev_err(&rdev->dev, "Error in %s()!\n", __func__);
		return ret;
	}
	return (((control >> ri->en_bit) & 1) == 1);
}

static int mp8845c_reg_enable(struct regulator_dev *rdev)
{
	struct mp8845c_regulator *ri = rdev_get_drvdata(rdev);
	int ret;

	ret = mp8845c_set_bits(ri->client, ri->reg_en_reg, (1 << ri->en_bit));
	if (ret < 0)
		dev_err(&rdev->dev, "Error in %s()!\n", __func__);
	else
		ri->vout_en = 1;

	return ret;
}

static int mp8845c_reg_disable(struct regulator_dev *rdev)
{
	struct mp8845c_regulator *ri = rdev_get_drvdata(rdev);
	int ret;

	ret = mp8845c_clr_bits(ri->client, ri->reg_en_reg, (1 << ri->en_bit));
	if (ret < 0)
		dev_err(&rdev->dev, "Error in %s()!\n", __func__);
	else
		ri->vout_en = 0;

	return ret;
}

static int mp8845c_list_voltage(struct regulator_dev *rdev, unsigned index)
{
	struct mp8845c_regulator *ri = rdev_get_drvdata(rdev);

	return ri->min_uV + (ri->step_uV * index);
}

static int __mp8845c_set_voltage(struct mp8845c_regulator *ri, int min_uV, int max_uV, unsigned *selector)
{
	int vsel;
	int ret;
	uint8_t vout_val;

	if ((min_uV < ri->min_uV) || (max_uV > ri->max_uV))
		return -EDOM;

	vsel = (min_uV - ri->min_uV + ri->step_uV - 1)/ri->step_uV;
	if (vsel > ri->nsteps)
		return -EDOM;

	if (selector)
		*selector = vsel;

	vout_val = (ri->vout_en << ri->en_bit)|(ri->vout_reg_cache & ~ri->vout_mask)|(vsel & ri->vout_mask);

	ret = mp8845c_write(ri->client, ri->vout_reg, vout_val);

	if (ret < 0)
		dev_err(ri->dev, "Error in writing the Voltage register\n");
	else
		ri->vout_reg_cache = vout_val;

	return ret;
}

static int mp8845c_set_voltage_time_sel(struct regulator_dev *rdev, unsigned int old_sel, unsigned int new_sel)
{
	struct mp8845c_regulator *ri = rdev_get_drvdata(rdev);

	if (old_sel < new_sel)
		return ((new_sel - old_sel) * ri->delay) + ri->cmd_delay;

	return 0;
}

#if 1
static int mp8845c_set_voltage_sel(struct regulator_dev *rdev, unsigned selector)
{
	struct mp8845c_regulator *ri = rdev_get_drvdata(rdev);
	int uV;

	uV = ri->min_uV + (ri->step_uV * selector);

	return __mp8845c_set_voltage(ri, uV, uV, NULL);
}

static int mp8845c_get_voltage_sel(struct regulator_dev *rdev)
{
	struct mp8845c_regulator *ri = rdev_get_drvdata(rdev);
	uint8_t vsel;

	vsel = ri->vout_reg_cache & ri->vout_mask;
	return vsel;
}
#else
static int mp8845c_set_voltage(struct regulator_dev *rdev, int min_uV, int max_uV, unsigned *selector)
{
	struct mp8845c_regulator *ri = rdev_get_drvdata(rdev);

	return __mp8845c_set_voltage(ri, min_uV, max_uV, selector);
}

static int mp8845c_get_voltage(struct regulator_dev *rdev)
{
	struct mp8845c_regulator *ri = rdev_get_drvdata(rdev);
	uint8_t vsel;

	vsel = ri->vout_reg_cache & ri->vout_mask;
	return ri->min_uV + vsel * ri->step_uV;
}
#endif

static struct regulator_ops mp8845c_vout_ops = {
	.list_voltage		= mp8845c_list_voltage,
#if 1
	.set_voltage_sel	= mp8845c_set_voltage_sel,
	.set_voltage_time_sel = mp8845c_set_voltage_time_sel,
	.get_voltage_sel	= mp8845c_get_voltage_sel,
#else
	.set_voltage		= mp8845c_set_voltage,
	.get_voltage		= mp8845c_get_voltage,
#endif
	.enable				= mp8845c_reg_enable,
	.disable			= mp8845c_reg_disable,
	.is_enabled			= mp8845c_reg_is_enabled,
	.enable_time		= mp8845c_regulator_enable_time,
};

#define MP8845C_REG(_id, _name_id, _en_reg, _en_bit, _vout_reg, _vout_mask,	\
				_min_mv, _max_mv, _step_uV, _nsteps, _delay, _cmd_delay, _ops)	\
{											\
	.id				= MP8845C_##_id##_VOUT,	\
	.reg_en_reg		= _en_reg,				\
	.en_bit			= _en_bit,				\
	.vout_reg		= _vout_reg,			\
	.vout_mask		= _vout_mask,			\
	.min_uV			= _min_mv * 1000,		\
	.max_uV			= _max_mv * 1000,		\
	.step_uV		= _step_uV,				\
	.nsteps			= _nsteps, 				\
	.delay			= _delay,				\
	.cmd_delay		= _cmd_delay,			\
	.desc = {								\
		.name		= mp8845c_rails(_name_id),	\
		.id			= MP8845C_##_id##_VOUT,	\
		.n_voltages = _nsteps,				\
		.ops		= _ops,					\
		.type		= REGULATOR_VOLTAGE,	\
		.owner		= THIS_MODULE,			\
	},										\
}

static struct mp8845c_regulator mp8845c_regulator_data[] = 
{
	MP8845C_REG(0, 1,
				MP8845C_REG_VSEL, 
				MP8845C_POS_EN, 
				MP8845C_REG_VSEL, 
				MP8845C_POS_OUT_VOL_MASK, 
				600, 1450, 6700, 0x80, 1, 0,
				&mp8845c_vout_ops),

	MP8845C_REG(1, 2,
				MP8845C_REG_VSEL, 
				MP8845C_POS_EN, 
				MP8845C_REG_VSEL, 
				MP8845C_POS_OUT_VOL_MASK, 
				600, 1450, 6700, 0x80, 1, 0,
				&mp8845c_vout_ops),
};

static inline struct mp8845c_regulator *find_regulator_info(int id)
{
	struct mp8845c_regulator *ri;
	int i;

	for (i = 0; i < ARRAY_SIZE(mp8845c_regulator_data); i++) {
		ri = &mp8845c_regulator_data[i];
		if (ri->desc.id == id)
			return ri;
	}
	return NULL;
}

static int mp8845c_regulator_preinit(struct mp8845c_regulator *ri, struct mp8845c_regulator_platform_data *mp8845c_pdata)
{
	int ret = 0;

	if (mp8845c_pdata->init_uV > -1) {
		ret = __mp8845c_set_voltage(ri, mp8845c_pdata->init_uV, mp8845c_pdata->init_uV, NULL);
		if (ret < 0) {
			dev_err(ri->dev, "Not able to initialize voltage %d for rail %d err %d\n", 
								mp8845c_pdata->init_uV, ri->desc.id, ret);
			return ret;
		}
	}

	if (mp8845c_pdata->init_enable)
	{
		ret = mp8845c_set_bits(ri->client, ri->reg_en_reg, (1 << ri->en_bit));

		if (ret < 0)
			dev_err(ri->dev, "Not able to enable rail %d err %d\n", ri->desc.id, ret);
		else
			ri->vout_en = 1;

	}
	else
	{
		ret = mp8845c_clr_bits(ri->client, ri->reg_en_reg, (1 << ri->en_bit));
		if (ret < 0)
			dev_err(ri->dev, "Not able to disable rail %d err %d\n", ri->desc.id, ret);
		else
			ri->vout_en = 0;
	}

	return ret;
}

static int __devinit mp8845c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct mp8845c_platform_data *init_data = client->dev.platform_data;
	struct mp8845c_regulator_platform_data *reg_plat_data = init_data->platform_data;
	struct mp8845c_regulator *ri;
	int id_info = init_data->id;
	int ret = 0;

	ri = find_regulator_info(id_info);
	if (ri == NULL) {
		dev_err(&client->dev, "invalid regulator ID specified\n");
		return -EINVAL;
	}

	ri->client = client;
	ri->dev = &client->dev;

	ret = mp8845c_regulator_preinit(ri, reg_plat_data);
	if (ret) {
		dev_err(&client->dev, "Fail in pre-initialisation\n");
		return ret;
	}

	ri->rdev = regulator_register(&ri->desc, &client->dev, &reg_plat_data->regulator, ri, NULL);

	if (IS_ERR(ri->rdev)) {
		ret = PTR_ERR(ri->rdev);
		dev_err(&client->dev, "failed to register %s\n", id->name);
		return ret;
	}

	i2c_set_clientdata(client, ri);
	dev_dbg(&client->dev, "%s regulator driver is registered.\n", id->name);

	return 0;

}

static int __devexit mp8845c_remove(struct i2c_client *client)
{
	struct mp8845c_regulator *ri = i2c_get_clientdata(client);

	regulator_unregister(ri->rdev);

	return 0;
}

#ifdef CONFIG_PM
static int mp8845c_suspend(struct i2c_client *client, pm_message_t state)
{
	int ret = 0;

	return ret;
}

static int mp8845c_resume(struct i2c_client *client)
{
	int ret = 0;

	return ret;
}

#endif

static const struct i2c_device_id mp8845c_id[] = {
	{ "mp8845c", 0},
	{ }
};
MODULE_DEVICE_TABLE(i2c, mp8845c_id);

static struct i2c_driver mp8845c_driver = {
	.probe = mp8845c_probe,
	.remove = __devexit_p(mp8845c_remove),
#ifdef CONFIG_PM
	.suspend = mp8845c_suspend,
	.resume = mp8845c_resume,
#endif
	.driver		= {
		.name	= "mp8845c",
		.owner = THIS_MODULE,
	},
	.id_table	= mp8845c_id,
};

static int __init mp8845c_init(void)
{
	int ret = -ENODEV;

	ret = i2c_add_driver(&mp8845c_driver);

	if (ret != 0)
		pr_err("%s(), Failed to register I2C driver: %d\n", __func__, ret);

	return ret;
}
subsys_initcall(mp8845c_init);

static void __exit mp8845c_exit(void)
{
	i2c_del_driver(&mp8845c_driver);
}
module_exit(mp8845c_exit);

MODULE_DESCRIPTION("MP8845C current regulator driver");
MODULE_AUTHOR("pjsin865@nexell.co.kr");
MODULE_LICENSE("GPL");
MODULE_ALIAS("i2c:mp8845c-regulator");
