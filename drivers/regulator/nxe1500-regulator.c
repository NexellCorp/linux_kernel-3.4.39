/*
 * drivers/regulator/nxe1500-regulator.c
 *
 *  Copyright (C) 2016 Nexell
 *  Jongsin Park <pjsin865@nexell.co.kr>
 *
 * Based on code
 *	Copyright (C) 2011 NVIDIA Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/*#define DEBUG			1*/
/*#define VERBOSE_DEBUG		1*/
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/mfd/nxe1500.h>
#include <linux/regulator/nxe1500-regulator.h>

#include <mach/tags.h>

#define	VOLTAGE_STEP_UV				(12500)	/* 12.5 mV */

static struct tag_asv_margin tag_margin = { 0, };

static int __init parse_tag_core_margin(const struct tag *tag)
{
	struct tag_asv_margin *t = (struct tag_asv_margin *)&tag->u;
	struct tag_asv_margin *p = &tag_margin;

	p->value = t->value;
	p->minus = t->minus;
	p->percent = t->percent;
	printk("ASV: Core margin:%s%d%s\n",
		p->minus?"-":"+", p->value, p->percent?"%":"mV");
	return 0;
}
__tagtable(ATAG_CORE_MARGIN, parse_tag_core_margin);

static long nxe1500_calculate_margin(long init_uv)
{
	struct tag_asv_margin *p = &tag_margin;
	long step_vol = VOLTAGE_STEP_UV;
	long uV = init_uv;
	long dv, new, al = 0;

	int  value = p->value;
	bool minus = p->minus;
	bool percent = p->percent;

	if (0 == p->value)
		return init_uv;

	dv  = percent ? ((uV/100) * value) : (value * 1000);;
	new = minus ? uV - dv : uV + dv;

	if ((new % step_vol)) {
		new = (new / step_vol) * step_vol;
		al = 1;
		if (minus) new += step_vol;	/* Upper */
	}
	printk("Core %7ld (%s%ld) align %ld (%s) -> %7ld\n",
		uV, minus?"-":"+", dv, step_vol, al?"X":"O", new);

	return new;
}

struct nxe1500_regulator {
	int		id;
	int		sleep_id;

	/* Regulator register address.*/
	u8		reg_en_reg;
	u8		en_bit;
	u8		reg_disc_reg;
	u8		disc_bit;
	u8		vout_reg;
	u8		vout_mask;
	u8		vout_reg_cache;
	u8		sleep_slot_reg;
	u8		sleep_volt_reg;
	u8		eco_reg;
	u8		eco_bit;
	u8		eco_slp_reg;
	u8		eco_slp_bit;

	/* chip constraints on regulator behavior */
	int		min_uV;
	int		max_uV;
	int		step_uV;
	int		nsteps;

	/* regulator specific turn-on delay */
	u16		delay;
	u16		en_delay;
	u16		cmd_delay;

	/* used by regulator core */
	struct regulator_desc	desc;

	/* Device */
	struct device		*dev;
};

static unsigned int nxe1500_suspend_status;

static inline struct device *to_nxe1500_dev(struct regulator_dev *rdev)
{
	return rdev_get_dev(rdev)->parent->parent;
}

static int nxe1500_regulator_enable_time(struct regulator_dev *rdev)
{
	struct nxe1500_regulator *ri = rdev_get_drvdata(rdev);

	return ri->en_delay;
}

static int nxe1500_reg_is_enabled(struct regulator_dev *rdev)
{
	struct nxe1500_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_nxe1500_dev(rdev);
	uint8_t control;
	int ret;

	ret = nxe1500_read(parent, ri->reg_en_reg, &control);
	if (ret < 0) {
		dev_err(&rdev->dev, "Error in reading the control register\n");
		return ret;
	}
	return (((control >> ri->en_bit) & 1) == 1);
}

static int nxe1500_reg_enable(struct regulator_dev *rdev)
{
	struct nxe1500_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_nxe1500_dev(rdev);
	int ret;

	ret = nxe1500_set_bits(parent, ri->reg_en_reg, (1 << ri->en_bit));
	if (ret < 0) {
		dev_err(&rdev->dev, "Error in updating the STATE register\n");
		return ret;
	}
	udelay(ri->delay);
	return ret;
}

static int nxe1500_reg_disable(struct regulator_dev *rdev)
{
	struct nxe1500_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_nxe1500_dev(rdev);
	int ret;

	ret = nxe1500_clr_bits(parent, ri->reg_en_reg, (1 << ri->en_bit));
	if (ret < 0)
		dev_err(&rdev->dev, "Error in updating the STATE register\n");

	return ret;
}

static int nxe1500_list_voltage(struct regulator_dev *rdev, unsigned index)
{
	struct nxe1500_regulator *ri = rdev_get_drvdata(rdev);

	return ri->min_uV + (ri->step_uV * index);
}

static int __nxe1500_set_voltage(struct device *parent,
		struct nxe1500_regulator *ri, int min_uV, int max_uV,
		unsigned *selector, int sleep_mode)
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

	vout_val = (ri->vout_reg_cache & ~ri->vout_mask) |
				(vsel & ri->vout_mask);
	if (sleep_mode)
		ret = nxe1500_write(parent, ri->sleep_volt_reg, vout_val);
	else
		ret = nxe1500_write(parent, ri->vout_reg, vout_val);
	if (ret < 0)
		dev_err(ri->dev, "Error in writing the Voltage register\n");
	else
		ri->vout_reg_cache = vout_val;

#ifdef CONFIG_PM_DBGOUT
	vout_val = 0x00;
	nxe1500_read(parent, ri->vout_reg, &vout_val);
	if(ri->vout_reg_cache != vout_val)
		printk(KERN_ERR "## \e[31m%s()\e[0m Data is different! set:0x%02x, read:0x%02x  \n", __func__, ri->vout_reg_cache, vout_val);
#endif

	return ret;
}

static int nxe1500_set_voltage_time_sel(struct regulator_dev *rdev,
					     unsigned int old_sel,
					     unsigned int new_sel)
{
	struct nxe1500_regulator *ri = rdev_get_drvdata(rdev);

	if (old_sel < new_sel)
		return ((new_sel - old_sel) * ri->delay) + ri->cmd_delay;

	return 0;
}

static int nxe1500_set_voltage_sel(struct regulator_dev *rdev,
				       unsigned selector)
{
	struct nxe1500_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_nxe1500_dev(rdev);

	int uV;

	uV = ri->min_uV + (ri->step_uV * selector);

	if (nxe1500_suspend_status)
		return -EBUSY;

	return __nxe1500_set_voltage(parent, ri, uV, uV, NULL, 0);
}

static int nxe1500_set_voltage(struct regulator_dev *rdev,
		int min_uV, int max_uV, unsigned *selector)
{
	struct nxe1500_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_nxe1500_dev(rdev);

	if (nxe1500_suspend_status)
		return -EBUSY;

	return __nxe1500_set_voltage(parent, ri, min_uV, max_uV, selector, 0);
}

static int nxe1500_get_voltage_sel(struct regulator_dev *rdev)
{
	struct nxe1500_regulator *ri = rdev_get_drvdata(rdev);
	uint8_t vsel;

	vsel = ri->vout_reg_cache & ri->vout_mask;
	return vsel ;
}

static int nxe1500_get_voltage(struct regulator_dev *rdev)
{
	struct nxe1500_regulator *ri = rdev_get_drvdata(rdev);
	uint8_t vsel;

	vsel = ri->vout_reg_cache & ri->vout_mask;
	return ri->min_uV + vsel * ri->step_uV;
}

int nxe1500_regulator_enable_eco_mode(struct regulator_dev *rdev)
{
	struct nxe1500_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_nxe1500_dev(rdev);
	int ret;

	ret = nxe1500_set_bits(parent, ri->eco_reg, (1 << ri->eco_bit));
	if (ret < 0)
		dev_err(&rdev->dev, "Error Enable LDO eco mode\n");

	return ret;
}
EXPORT_SYMBOL_GPL(nxe1500_regulator_enable_eco_mode);

int nxe1500_regulator_disable_eco_mode(struct regulator_dev *rdev)
{
	struct nxe1500_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_nxe1500_dev(rdev);
	int ret;

	ret = nxe1500_clr_bits(parent, ri->eco_reg, (1 << ri->eco_bit));
	if (ret < 0)
		dev_err(&rdev->dev, "Error Disable LDO eco mode\n");

	return ret;
}
EXPORT_SYMBOL_GPL(nxe1500_regulator_disable_eco_mode);

int nxe1500_regulator_enable_eco_slp_mode(struct regulator_dev *rdev)
{
	struct nxe1500_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_nxe1500_dev(rdev);
	int ret;

	ret = nxe1500_set_bits(parent, ri->eco_slp_reg,
						(1 << ri->eco_slp_bit));
	if (ret < 0)
		dev_err(&rdev->dev, "Error Enable LDO eco mode in d during sleep\n");

	return ret;
}
EXPORT_SYMBOL_GPL(nxe1500_regulator_enable_eco_slp_mode);

int nxe1500_regulator_disable_eco_slp_mode(struct regulator_dev *rdev)
{
	struct nxe1500_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_nxe1500_dev(rdev);
	int ret;

	ret = nxe1500_clr_bits(parent, ri->eco_slp_reg,
						(1 << ri->eco_slp_bit));
	if (ret < 0)
		dev_err(&rdev->dev, "Error Enable LDO eco mode in d during sleep\n");

	return ret;
}
EXPORT_SYMBOL_GPL(nxe1500_regulator_disable_eco_slp_mode);

static struct regulator_ops nxe1500_dcdc1_ops = {
	.list_voltage		= nxe1500_list_voltage,
	.set_voltage_sel	= nxe1500_set_voltage_sel,
	.set_voltage_time_sel = nxe1500_set_voltage_time_sel,
	.get_voltage_sel	= nxe1500_get_voltage_sel,
	.enable				= nxe1500_reg_enable,
	.disable			= nxe1500_reg_disable,
	.is_enabled			= nxe1500_reg_is_enabled,
	.enable_time		= nxe1500_regulator_enable_time,
};

static struct regulator_ops nxe1500_ops = {
	.list_voltage	= nxe1500_list_voltage,
	.set_voltage	= nxe1500_set_voltage,
	.get_voltage	= nxe1500_get_voltage,
	.enable			= nxe1500_reg_enable,
	.disable		= nxe1500_reg_disable,
	.is_enabled		= nxe1500_reg_is_enabled,
	.enable_time	= nxe1500_regulator_enable_time,
};

#define NXE1500_REG(_id, _en_reg, _en_bit, _disc_reg, _disc_bit, _vout_reg, \
		_vout_mask, _slp_volt_reg, _slp_slot_reg, _min_mv, _max_mv, _step_uV, _nsteps,    \
		_ops, _delay, _en_delay, _eco_reg, _eco_bit, _eco_slp_reg, _eco_slp_bit) \
{											\
	.reg_en_reg		= _en_reg,				\
	.en_bit			= _en_bit,				\
	.reg_disc_reg	= _disc_reg,			\
	.disc_bit		= _disc_bit,			\
	.vout_reg		= _vout_reg,			\
	.vout_mask		= _vout_mask,			\
	.sleep_volt_reg	= _slp_volt_reg,		\
	.sleep_slot_reg	= _slp_slot_reg,		\
	.min_uV			= _min_mv * 1000,		\
	.max_uV			= _max_mv * 1000,		\
	.step_uV		= _step_uV,				\
	.nsteps			= _nsteps,				\
	.delay			= _delay,				\
	.en_delay		= _en_delay,			\
	.cmd_delay		= 50,					\
	.id				= NXE1500_ID_##_id,		\
	.sleep_id		= NXE1500_DS_##_id,		\
	.eco_reg		= _eco_reg,				\
	.eco_bit		= _eco_bit,				\
	.eco_slp_reg	= _eco_slp_reg,			\
	.eco_slp_bit	= _eco_slp_bit,			\
	.desc = {								\
		.name		= nxe1500_rails(_id),	\
		.id			= NXE1500_ID_##_id,		\
		.n_voltages = _nsteps,				\
		.ops		= &_ops,				\
		.type		= REGULATOR_VOLTAGE,	\
		.owner		= THIS_MODULE,			\
	},										\
}

/* DCDC Enable Rising Time
*  DCDC1 = 300us/1.3V
*  DCDC2 = 300us/1.2V
*  DCDC3 = 700us/3.3V
*  DCDC4 = 400us/1.5V
*  DCDC5 = 400us/1.5V
*/
static struct nxe1500_regulator nxe1500_regulator[] = {
#ifndef CONFIG_REGULATOR_MP8845C
	NXE1500_REG(DC1, 0x2C, 0, 0x2C, 1, 0x36, 0xFF, 0x3B, 0x16,
			600, 3500, 12500, 0xE8, nxe1500_dcdc1_ops, 2, 300,
			0x00, 0, 0x00, 0),

	NXE1500_REG(DC2, 0x2E, 0, 0x2E, 1, 0x37, 0xFF, 0x3C, 0x17,
			600, 3500, 12500, 0xE8, nxe1500_ops, 2, 300,
			0x00, 0, 0x00, 0),
#endif
	NXE1500_REG(DC3, 0x30, 0, 0x30, 1, 0x38, 0xFF, 0x3D, 0x18,
			600, 3500, 12500, 0xE8, nxe1500_ops, 2, 700,
			0x00, 0, 0x00, 0),

	NXE1500_REG(DC4, 0x32, 0, 0x32, 1, 0x39, 0xFF, 0x3E, 0x19,
			600, 3500, 12500, 0xE8, nxe1500_ops, 2, 400,
			0x00, 0, 0x00, 0),

	NXE1500_REG(LDO1, 0x44, 0, 0x46, 0, 0x4C, 0x7F, 0x58, 0x1B,
			900, 3500, 25000, 0x68, nxe1500_ops, 0, 500,
			0x48, 0, 0x4A, 0),

	NXE1500_REG(LDO2, 0x44, 1, 0x46, 1, 0x4D, 0x7F, 0x59, 0x1C,
			900, 3500, 25000, 0x68, nxe1500_ops, 0, 500,
			0x48, 1, 0x4A, 1),

	NXE1500_REG(LDO3, 0x44, 2, 0x46, 2, 0x4E, 0x7F, 0x5A, 0x1D,
			600, 3500, 25000, 0x74, nxe1500_ops, 0, 500,
			0x48, 2, 0x4A, 2),

	NXE1500_REG(LDO4, 0x44, 3, 0x46, 3, 0x4F, 0x7F, 0x5B, 0x1E,
			900, 3500, 25000, 0x68, nxe1500_ops, 0, 500,
			0x48, 3, 0x4A, 3),

	NXE1500_REG(LDO5, 0x44, 4, 0x46, 4, 0x50, 0x7F, 0x5C, 0x1F,
			900, 3500, 25000, 0x68, nxe1500_ops, 0, 500,
			0x48, 4, 0x4A, 4),

	NXE1500_REG(LDORTC1, 0x45, 4, 0x00, 0, 0x56, 0x7F, 0x00, 0x00,
			1200, 3500, 25000, 0x58, nxe1500_ops, 0, 500,
			0x00, 0, 0x00, 0),

	NXE1500_REG(LDORTC2, 0x45, 5, 0x00, 0, 0x57, 0x7F, 0x00, 0x00,
			900, 3500, 25000, 0x68, nxe1500_ops, 0, 500,
			0x00, 0, 0x00, 0),
};
static inline struct nxe1500_regulator *find_regulator_info(int id)
{
	struct nxe1500_regulator *ri;
	int i;

	for (i = 0; i < ARRAY_SIZE(nxe1500_regulator); i++) {
		ri = &nxe1500_regulator[i];
		if (ri->desc.id == id)
			return ri;
	}
	return NULL;
}

#if 0
struct nxe1500_regulator_platform_data *nxe1500_regulator_pdata = NULL;
void nxe1500_set_default_vol(int id)
{
	struct nxe1500_regulator_platform_data *nxe1500_pdata = NULL;
	struct nxe1500_regulator *ri = NULL;

	int ret = 0;
	int cur_vol = 0;
	uint8_t vsel;

	if (!nxe1500_regulator_pdata)
		return;

	nxe1500_pdata = nxe1500_regulator_pdata;


	ri = find_regulator_info(id);

	vsel = ri->vout_reg_cache & ri->vout_mask;
	cur_vol = ri->min_uV + vsel * ri->step_uV;

	if(cur_vol < nxe1500_pdata->init_uV) {
		ret = __nxe1500_set_voltage_nonlock(ri->dev, ri, nxe1500_pdata->init_uV, nxe1500_pdata->init_uV, 0, 0);
		if (ret < 0)
			dev_err(ri->dev, "Not able to initialize voltage %d for rail %d err %d\n", nxe1500_pdata->init_uV, ri->desc.id, ret);
	}

	return;
}
EXPORT_SYMBOL_GPL(nxe1500_set_default_vol);
#endif

static int nxe1500_regulator_preinit(struct device *parent,
		struct nxe1500_regulator *ri,
		struct nxe1500_regulator_platform_data *nxe1500_pdata)
{
	struct regulator_consumer_supply *consumer_supplies =
		nxe1500_pdata->regulator.consumer_supplies;
	int ret = 0;

	if (ret < 0)
		dev_err(ri->dev, "Not able to %s rail %d err %d\n",
			(nxe1500_pdata->init_enable) ? "enable" : "disable",
			ri->desc.id, ret);

	if (nxe1500_pdata->sleep_slots > -1)
		ret = nxe1500_update(parent, ri->sleep_slot_reg, nxe1500_pdata->sleep_slots, 0xF);
	else
		ret = nxe1500_update(parent, ri->sleep_slot_reg, 0xF, 0xF);

	if (ret < 0)
		dev_err(ri->dev, "Not able to 0x%02X rail %d err %d\n",
			nxe1500_pdata->sleep_slots, ri->desc.id, ret);

#ifndef CONFIG_ENABLE_INIT_VOLTAGE
	if(ri->id == NXE1500_ID_DC1 
		|| ri->id == NXE1500_ID_DC2
		|| ri->id == NXE1500_ID_DC3
		|| ri->id == NXE1500_ID_DC4)
	{
		return ret;
	}
#endif

	if (0 == strcmp(consumer_supplies->supply, "vdd_core_1.2V")) {
		if (nxe1500_pdata->init_uV > -1)
			nxe1500_pdata->init_uV = nxe1500_calculate_margin(nxe1500_pdata->init_uV);
	}

	if (nxe1500_pdata->init_uV > -1) {
		ret = __nxe1500_set_voltage(parent, ri,	nxe1500_pdata->init_uV, nxe1500_pdata->init_uV, 0, 0);
		if (ret < 0) {
			dev_err(ri->dev, "Not able to initialize voltage %d for rail %d err %d\n", nxe1500_pdata->init_uV, ri->desc.id, ret);
			return ret;
		}
	}

	if (nxe1500_pdata->sleep_slots == -1) {
		ret = __nxe1500_set_voltage(parent, ri,	nxe1500_pdata->init_uV, nxe1500_pdata->init_uV, 0, 1);
		if (ret < 0) {
			dev_err(ri->dev, "Not able to sleep voltage %d for rail %d err %d\n", nxe1500_pdata->init_uV, ri->desc.id, ret);
			return ret;
		}
	}

	if (nxe1500_pdata->init_enable)
		ret = nxe1500_set_bits(parent, ri->reg_en_reg, (1 << ri->en_bit));
	else
		ret = nxe1500_clr_bits(parent, ri->reg_en_reg, (1 << ri->en_bit));

	return ret;
}

static inline int nxe1500_cache_regulator_register(struct device *parent,
	struct nxe1500_regulator *ri)
{
	ri->vout_reg_cache = 0;
	return nxe1500_read(parent, ri->vout_reg, &ri->vout_reg_cache);
}

static int __devinit nxe1500_regulator_probe(struct platform_device *pdev)
{
	struct nxe1500_regulator *ri = NULL;
	struct regulator_dev *rdev;
	struct nxe1500_regulator_platform_data *tps_pdata;
	int id = pdev->id;
	int err;

	ri = find_regulator_info(id);
	if (ri == NULL) {
		dev_err(&pdev->dev, "invalid regulator ID specified\n");
		return -EINVAL;
	}
	tps_pdata = pdev->dev.platform_data;
	ri->dev = &pdev->dev;

#if 0
	if(id == NXE1500_ID_DC1)
		nxe1500_regulator_pdata = tps_pdata;
#endif

	nxe1500_suspend_status = 0;

	err = nxe1500_cache_regulator_register(pdev->dev.parent, ri);
	if (err) {
		dev_err(&pdev->dev, "Fail in caching register\n");
		return err;
	}

	err = nxe1500_regulator_preinit(pdev->dev.parent, ri, tps_pdata);
	if (err) {
		dev_err(&pdev->dev, "Fail in pre-initialisation\n");
		return err;
	}

	rdev = regulator_register(&ri->desc, &pdev->dev,
				&tps_pdata->regulator, ri, NULL);
	if (IS_ERR_OR_NULL(rdev)) {
		dev_err(&pdev->dev, "failed to register regulator %s\n",
				ri->desc.name);
		return PTR_ERR(rdev);
	}

	platform_set_drvdata(pdev, rdev);

	return 0;
}

static int __devexit nxe1500_regulator_remove(struct platform_device *pdev)
{
	struct regulator_dev *rdev = platform_get_drvdata(pdev);

	regulator_unregister(rdev);
	return 0;
}

#ifdef CONFIG_PM
static int nxe1500_regulator_suspend(struct device *dev)
{
//	struct nxe1500_regulator *info = dev_get_drvdata(dev);

	nxe1500_suspend_status = 1;

	return 0;
}

static int nxe1500_regulator_resume(struct device *dev)
{
//	struct nxe1500_regulator *info = dev_get_drvdata(dev);

	nxe1500_suspend_status = 0;

	return 0;
}

static const struct dev_pm_ops nxe1500_regulator_pm_ops = {
	.suspend	= nxe1500_regulator_suspend,
	.resume		= nxe1500_regulator_resume,
};
#endif

static struct platform_driver nxe1500_regulator_driver = {
	.driver	= {
		.name	= "nxe1500-regulator",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm	= &nxe1500_regulator_pm_ops,
#endif
	},
	.probe		= nxe1500_regulator_probe,
	.remove		= __devexit_p(nxe1500_regulator_remove),
};

static int __init nxe1500_regulator_init(void)
{
	return platform_driver_register(&nxe1500_regulator_driver);
}
subsys_initcall(nxe1500_regulator_init);

static void __exit nxe1500_regulator_exit(void)
{
	platform_driver_unregister(&nxe1500_regulator_driver);
}
module_exit(nxe1500_regulator_exit);

MODULE_DESCRIPTION("NEXELL NXE1500 regulator driver");
MODULE_ALIAS("platform:nxe1500-regulator");
MODULE_LICENSE("GPL");

