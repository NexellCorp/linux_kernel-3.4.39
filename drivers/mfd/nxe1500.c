/*
 * driver/mfd/nxe1500.c
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
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/mfd/core.h>
#include <linux/mfd/nxe1500.h>
#include <mach/platform.h>
#include <mach/pm.h>


static struct i2c_client *nxe1500_i2c_client;

static inline int __nxe1500_read(struct i2c_client *client,
				  u8 reg, uint8_t *val)
{
	int ret;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0) {
		dev_err(&client->dev, "failed reading at 0x%02x\n", reg);
		return ret;
	}

	*val = (uint8_t)ret;
	dev_dbg(&client->dev, "nxe1500: reg read  reg=%x, val=%x\n",
				reg, *val);
	return 0;
}

static inline int __nxe1500_bulk_reads(struct i2c_client *client, u8 reg,
				int len, uint8_t *val)
{
	int ret;
	int i;

	ret = i2c_smbus_read_i2c_block_data(client, reg, len, val);
	if (ret < 0) {
		dev_err(&client->dev, "failed reading from 0x%02x\n", reg);
		return ret;
	}
	for (i = 0; i < len; ++i) {
		dev_dbg(&client->dev, "nxe1500: reg read  reg=%x, val=%x\n",
				reg + i, *(val + i));
	}
	return 0;
}

static inline int __nxe1500_write(struct i2c_client *client,
				 u8 reg, uint8_t val)
{
	int ret;

	dev_dbg(&client->dev, "nxe1500: reg write  reg=%x, val=%x\n",
				reg, val);
	ret = i2c_smbus_write_byte_data(client, reg, val);
	if (ret < 0) {
		dev_err(&client->dev, "failed writing 0x%02x to 0x%02x\n",
				val, reg);
		return ret;
	}

	return 0;
}

static inline int __nxe1500_bulk_writes(struct i2c_client *client, u8 reg,
				  int len, uint8_t *val)
{
	int ret;
	int i;

	for (i = 0; i < len; ++i) {
		dev_dbg(&client->dev, "nxe1500: reg write  reg=%x, val=%x\n",
				reg + i, *(val + i));
	}

	ret = i2c_smbus_write_i2c_block_data(client, reg, len, val);
	if (ret < 0) {
		dev_err(&client->dev, "failed writings to 0x%02x\n", reg);
		return ret;
	}

	return 0;
}

int nxe1500_pm_write(u8 reg, uint8_t val)
{
	int ret = 0;

	ret = nxe1500_write(&nxe1500_i2c_client->dev, reg, val);

	return ret;
}
EXPORT_SYMBOL_GPL(nxe1500_pm_write);

int nxe1500_write(struct device *dev, u8 reg, uint8_t val)
{
	struct nxe1500 *nxe1500 = dev_get_drvdata(dev);
	int ret = 0;

	mutex_lock(&nxe1500->io_lock);
	if (!ret)
		ret = __nxe1500_write(to_i2c_client(dev), reg, val);
	mutex_unlock(&nxe1500->io_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(nxe1500_write);

int nxe1500_write_bank1(struct device *dev, u8 reg, uint8_t val)
{
	struct nxe1500 *nxe1500 = dev_get_drvdata(dev);
	int ret = 0;

	mutex_lock(&nxe1500->io_lock);
	if (!ret)
		ret = __nxe1500_write(to_i2c_client(dev), reg, val);
	mutex_unlock(&nxe1500->io_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(nxe1500_write_bank1);

int nxe1500_bulk_writes(struct device *dev, u8 reg, u8 len, uint8_t *val)
{
	struct nxe1500 *nxe1500 = dev_get_drvdata(dev);
	int ret = 0;

	mutex_lock(&nxe1500->io_lock);
	if (!ret)
		ret = __nxe1500_bulk_writes(to_i2c_client(dev), reg, len, val);
	mutex_unlock(&nxe1500->io_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(nxe1500_bulk_writes);

int nxe1500_bulk_writes_bank1(struct device *dev, u8 reg, u8 len, uint8_t *val)
{
	struct nxe1500 *nxe1500 = dev_get_drvdata(dev);
	int ret = 0;

	mutex_lock(&nxe1500->io_lock);
	if (!ret)
		ret = __nxe1500_bulk_writes(to_i2c_client(dev), reg, len, val);
	mutex_unlock(&nxe1500->io_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(nxe1500_bulk_writes_bank1);

int nxe1500_pm_read(u8 reg, uint8_t *val)
{
	int ret = 0;

	ret = nxe1500_read(&nxe1500_i2c_client->dev, reg, val);

	return ret;
}
EXPORT_SYMBOL_GPL(nxe1500_pm_read);

int nxe1500_read(struct device *dev, u8 reg, uint8_t *val)
{
	struct nxe1500 *nxe1500 = dev_get_drvdata(dev);
	int ret = 0;

	mutex_lock(&nxe1500->io_lock);
	if (!ret)
		ret = __nxe1500_read(to_i2c_client(dev), reg, val);
	mutex_unlock(&nxe1500->io_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(nxe1500_read);

int nxe1500_read_bank1(struct device *dev, u8 reg, uint8_t *val)
{
	struct nxe1500 *nxe1500 = dev_get_drvdata(dev);
	int ret = 0;

	mutex_lock(&nxe1500->io_lock);
	if (!ret)
		ret =  __nxe1500_read(to_i2c_client(dev), reg, val);
	mutex_unlock(&nxe1500->io_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(nxe1500_read_bank1);

int nxe1500_bulk_reads(struct device *dev, u8 reg, u8 len, uint8_t *val)
{
	struct nxe1500 *nxe1500 = dev_get_drvdata(dev);
	int ret = 0;

	mutex_lock(&nxe1500->io_lock);
	if (!ret)
		ret = __nxe1500_bulk_reads(to_i2c_client(dev), reg, len, val);
	mutex_unlock(&nxe1500->io_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(nxe1500_bulk_reads);

int nxe1500_bulk_reads_bank1(struct device *dev, u8 reg, u8 len, uint8_t *val)
{
	struct nxe1500 *nxe1500 = dev_get_drvdata(dev);
	int ret = 0;

	mutex_lock(&nxe1500->io_lock);
	if (!ret)
		ret = __nxe1500_bulk_reads(to_i2c_client(dev), reg, len, val);
	mutex_unlock(&nxe1500->io_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(nxe1500_bulk_reads_bank1);

int nxe1500_set_bits(struct device *dev, u8 reg, uint8_t bit_mask)
{
	struct nxe1500 *nxe1500 = dev_get_drvdata(dev);
	uint8_t reg_val;
	int ret = 0;

	mutex_lock(&nxe1500->io_lock);
	if (!ret) {
		ret = __nxe1500_read(to_i2c_client(dev), reg, &reg_val);
		if (ret)
			goto out;

		if ((reg_val & bit_mask) != bit_mask) {
			reg_val |= bit_mask;
			ret = __nxe1500_write(to_i2c_client(dev), reg,
								 reg_val);
		}
	}
out:
	mutex_unlock(&nxe1500->io_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(nxe1500_set_bits);

int nxe1500_clr_bits(struct device *dev, u8 reg, uint8_t bit_mask)
{
	struct nxe1500 *nxe1500 = dev_get_drvdata(dev);
	uint8_t reg_val;
	int ret = 0;

	mutex_lock(&nxe1500->io_lock);
	if (!ret) {
		ret = __nxe1500_read(to_i2c_client(dev), reg, &reg_val);
		if (ret)
			goto out;

		if (reg_val & bit_mask) {
			reg_val &= ~bit_mask;
			ret = __nxe1500_write(to_i2c_client(dev), reg,
								 reg_val);
		}
	}
out:
	mutex_unlock(&nxe1500->io_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(nxe1500_clr_bits);

int nxe1500_update(struct device *dev, u8 reg, uint8_t val, uint8_t mask)
{
	struct nxe1500 *nxe1500 = dev_get_drvdata(dev);
	uint8_t reg_val;
	int ret = 0;

	mutex_lock(&nxe1500->io_lock);
	if (!ret) {
		ret = __nxe1500_read(nxe1500->client, reg, &reg_val);
		if (ret)
			goto out;

		if ((reg_val & mask) != val) {
			reg_val = (reg_val & ~mask) | (val & mask);
			ret = __nxe1500_write(nxe1500->client, reg, reg_val);
		}
	}
out:
	mutex_unlock(&nxe1500->io_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(nxe1500_update);

int nxe1500_update_bank1(struct device *dev, u8 reg, uint8_t val, uint8_t mask)
{
	struct nxe1500 *nxe1500 = dev_get_drvdata(dev);
	uint8_t reg_val;
	int ret = 0;

	mutex_lock(&nxe1500->io_lock);
	if (!ret) {
		ret = __nxe1500_read(nxe1500->client, reg, &reg_val);
		if (ret)
			goto out;

		if ((reg_val & mask) != val) {
			reg_val = (reg_val & ~mask) | (val & mask);
			ret = __nxe1500_write(nxe1500->client, reg, reg_val);
		}
	}
out:
	mutex_unlock(&nxe1500->io_lock);
	return ret;
}

#ifdef CONFIG_NXE1500_WDG_TEST
static irqreturn_t nxe1500_watchdog_isr(int irq, void *data)
{
	struct nxe1500 *info = data;
	printk(KERN_ERR "## \e[31m%s\e[0m() \n", __func__);

	nxe1500_clr_bits(info->dev, NXE1500_INT_IR_SYS, 0x40);

	return IRQ_HANDLED;
}

static void nxe1500_watchdog_init(struct nxe1500 *nxe1500)
{
	int ret;

	printk(KERN_ERR "## \e[31m%s\e[0m() \n", __func__);
	
	ret = request_threaded_irq((IRQ_SYSTEM_END + NXE1500_IRQ_WD),
					NULL, nxe1500_watchdog_isr, IRQF_ONESHOT, "nxe1500_watchdog_isr", nxe1500);

	nxe1500_set_bits(nxe1500->dev, NXE1500_PWR_REP_CNT, 0x01);
	nxe1500_write(nxe1500->dev, NXE1500_PWR_WD, 0x05);
	return;
}
#endif

#if 0
static int nxe1500_gpio_get(struct gpio_chip *gc, unsigned offset)
{
	struct nxe1500 *nxe1500 = container_of(gc, struct nxe1500,
								 gpio_chip);
	uint8_t val;
	int ret;

	ret = nxe1500_read(nxe1500->dev, NXE1500_GPIO_MON_IOIN, &val);
	if (ret < 0)
		return ret;

	return ((val & (0x1 << offset)) != 0);
}

static void nxe1500_gpio_set(struct gpio_chip *gc, unsigned offset,
			int value)
{
	struct nxe1500 *nxe1500 = container_of(gc, struct nxe1500,
								 gpio_chip);
	if (value)
		nxe1500_set_bits(nxe1500->dev, NXE1500_GPIO_IOOUT,
						1 << offset);
	else
		nxe1500_clr_bits(nxe1500->dev, NXE1500_GPIO_IOOUT,
						1 << offset);
}

static int nxe1500_gpio_input(struct gpio_chip *gc, unsigned offset)
{
	struct nxe1500 *nxe1500 = container_of(gc, struct nxe1500,
								 gpio_chip);

	return nxe1500_clr_bits(nxe1500->dev, NXE1500_GPIO_IOSEL,
						1 << offset);
}

static int nxe1500_gpio_output(struct gpio_chip *gc, unsigned offset,
				int value)
{
	struct nxe1500 *nxe1500 = container_of(gc, struct nxe1500,
								 gpio_chip);

	nxe1500_gpio_set(gc, offset, value);
	return nxe1500_set_bits(nxe1500->dev, NXE1500_GPIO_IOSEL,
						1 << offset);
}

static int nxe1500_gpio_to_irq(struct gpio_chip *gc, unsigned off)
{
	struct nxe1500 *nxe1500 = container_of(gc, struct nxe1500,
								 gpio_chip);

	if ((off >= 0) && (off < 8))
		return nxe1500->irq_base + NXE1500_IRQ_GPIO0 + off;

	return -EIO;
}


static void nxe1500_gpio_init(struct nxe1500 *nxe1500,
	struct nxe1500_platform_data *pdata)
{
	int ret;
	int i;
	struct nxe1500_gpio_init_data *ginit;

	if (pdata->gpio_base  <= 0)
		return;

	for (i = 0; i < pdata->num_gpioinit_data; ++i) {
		ginit = &pdata->gpio_init_data[i];

		if (!ginit->init_apply)
			continue;

		if (ginit->output_mode_en) {
			/* GPIO output mode */
			if (ginit->output_val)
				/* output H */
				ret = nxe1500_set_bits(nxe1500->dev,
					NXE1500_GPIO_IOOUT, 1 << i);
			else
				/* output L */
				ret = nxe1500_clr_bits(nxe1500->dev,
					NXE1500_GPIO_IOOUT, 1 << i);
			if (!ret)
				ret = nxe1500_set_bits(nxe1500->dev,
					NXE1500_GPIO_IOSEL, 1 << i);
		} else
			/* GPIO input mode */
			ret = nxe1500_clr_bits(nxe1500->dev,
					NXE1500_GPIO_IOSEL, 1 << i);

		/* if LED function enabled in OTP */
		if (ginit->led_mode) {
			/* LED Mode 1 */
			if (i == 0)	/* GP0 */
				ret = nxe1500_set_bits(nxe1500->dev,
					 NXE1500_GPIO_LED_FUNC,
					 0x04 | (ginit->led_func & 0x03));
			if (i == 1)	/* GP1 */
				ret = nxe1500_set_bits(nxe1500->dev,
					 NXE1500_GPIO_LED_FUNC,
					 0x40 | (ginit->led_func & 0x03) << 4);

		}

		if (ret < 0)
			dev_err(nxe1500->dev, "Gpio %d init "
				"dir configuration failed: %d\n", i, ret);
	}

	nxe1500->gpio_chip.owner		= THIS_MODULE;
	nxe1500->gpio_chip.label		= nxe1500->client->name;
	nxe1500->gpio_chip.dev			= nxe1500->dev;
	nxe1500->gpio_chip.base			= pdata->gpio_base;
	nxe1500->gpio_chip.ngpio		= pdata->num_gpioinit_data;	//NXE1500_NR_GPIO;
	nxe1500->gpio_chip.can_sleep	= 1;

	nxe1500->gpio_chip.direction_input	= nxe1500_gpio_input;
	nxe1500->gpio_chip.direction_output	= nxe1500_gpio_output;
	nxe1500->gpio_chip.set			= nxe1500_gpio_set;
	nxe1500->gpio_chip.get			= nxe1500_gpio_get;
	nxe1500->gpio_chip.to_irq		= nxe1500_gpio_to_irq;

	ret = gpiochip_add(&nxe1500->gpio_chip);
	if (ret)
		dev_warn(nxe1500->dev, "GPIO registration failed: %d\n", ret);
}
#endif

static int nxe1500_remove_subdev(struct device *dev, void *unused)
{
	platform_device_unregister(to_platform_device(dev));
	return 0;
}

static int nxe1500_remove_subdevs(struct nxe1500 *nxe1500)
{
	return device_for_each_child(nxe1500->dev, NULL,
				     nxe1500_remove_subdev);
}

static int nxe1500_add_subdevs(struct nxe1500 *nxe1500,
				struct nxe1500_platform_data *pdata)
{
	struct nxe1500_subdev_info *subdev;
	struct platform_device *pdev;
	int i, ret = 0;

	for (i = 0; i < pdata->num_subdevs; i++) {
		subdev = &pdata->subdevs[i];

		pdev = platform_device_alloc(subdev->name, subdev->id);

		pdev->dev.parent = nxe1500->dev;
		pdev->dev.platform_data = subdev->platform_data;

		ret = platform_device_add(pdev);
		if (ret)
			goto failed;
	}
	return 0;

failed:
	nxe1500_remove_subdevs(nxe1500);
	return ret;
}

#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#include <linux/seq_file.h>
static void print_regs(const char *header, struct seq_file *s,
		struct i2c_client *client, int start_offset,
		int end_offset)
{
	uint8_t reg_val;
	int i;
	int ret;

	seq_printf(s, "%s\n", header);
	for (i = start_offset; i <= end_offset; ++i) {
		ret = __nxe1500_read(client, i, &reg_val);
		if (ret >= 0)
			seq_printf(s, "Reg 0x%02x Value 0x%02x\n", i, reg_val);
	}
	seq_printf(s, "------------------\n");
}

static int dbg_nxe1500_show(struct seq_file *s, void *unused)
{
	struct nxe1500 *nxe1500 = s->private;
	struct i2c_client *client = nxe1500->client;

	seq_printf(s, "NXE1500 Registers\n");
	seq_printf(s, "------------------\n");

	print_regs("System Regs",		s, client, 0x0, 0x05);
	print_regs("Power Control Regs",	s, client, 0x07, 0x2B);
	print_regs("DCDC  Regs",		s, client, 0x2C, 0x43);
	print_regs("LDO   Regs",		s, client, 0x44, 0x61);
	print_regs("ADC   Regs",		s, client, 0x64, 0x8F);
	print_regs("GPIO  Regs",		s, client, 0x90, 0x98);
	print_regs("INTC  Regs",		s, client, 0x9C, 0x9E);
	print_regs("RTC   Regs",		s, client, 0xA0, 0xAF);
	print_regs("OPT   Regs",		s, client, 0xB0, 0xB1);
	print_regs("CHG   Regs",		s, client, 0xB2, 0xDF);
	print_regs("FUEL  Regs",		s, client, 0xE0, 0xFC);
	return 0;
}

static int dbg_nxe1500_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_nxe1500_show, inode->i_private);
}

static const struct file_operations debug_fops = {
	.open		= dbg_nxe1500_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};
static void nxe1500_debuginit(struct nxe1500 *nxe1500)
{
	(void)debugfs_create_file("nxe1500", S_IRUGO, NULL,
			nxe1500, &debug_fops);
}
#endif

static void nxe1500_noe_init(struct nxe1500 *nxe1500)
{
#if 0
	struct i2c_client *client = nxe1500->client;

	/* N_OE timer setting to 128mS */
	__nxe1500_write(client, NXE1500_PWR_NOE_TIMSET, 0x0);
	/* Repeat power ON after reset (Power Off/N_OE) */
	__nxe1500_write(client, NXE1500_PWR_REP_CNT, 0x1);
#endif
}

#if 0
extern void (*nxp_board_reset)(char str, const char *cmd);
static void (*backup_nxp_board_reset)(char str, const char *cmd);
extern void nxe1500_set_default_vol(int id);

void nxe1500_restart(char str, const char *cmd)
{
	nxe1500_set_default_vol(0);

	if (backup_nxp_board_reset)
		backup_nxp_board_reset(str, cmd);
}
#endif

void nxe1500_power_off(void)
{
	int ret;

	if (!nxe1500_i2c_client)
		return;

	/* Disable all Interrupt */
	ret = nxe1500_write(&nxe1500_i2c_client->dev, NXE1500_INTC_INTEN, 0);
	if (ret < 0)
		ret = nxe1500_write(&nxe1500_i2c_client->dev, NXE1500_INTC_INTEN, 0);

	/* Not repeat power ON after power off(Power Off/N_OE) */
	ret = nxe1500_write(&nxe1500_i2c_client->dev, NXE1500_PWR_REP_CNT, 0x0);
	if (ret < 0)
		ret = nxe1500_write(&nxe1500_i2c_client->dev, NXE1500_PWR_REP_CNT, 0x0);

	/* Power OFF */
	ret = nxe1500_write(&nxe1500_i2c_client->dev, NXE1500_PWR_SLP_CNT, 0x1);
	if (ret < 0)
		ret = nxe1500_write(&nxe1500_i2c_client->dev, NXE1500_PWR_SLP_CNT, 0x1);

	//if (backup_pm_power_off)
	//	backup_pm_power_off();
	//halt();
}

static int nxe1500_i2c_probe(struct i2c_client *client,
			      const struct i2c_device_id *id)
{
	struct nxe1500 *nxe1500;
	struct nxe1500_platform_data *pdata = client->dev.platform_data;
	int ret;

	nxe1500 = kzalloc(sizeof(struct nxe1500), GFP_KERNEL);
	if (nxe1500 == NULL)
		return -ENOMEM;

	nxe1500->client = client;
	nxe1500->dev = &client->dev;
	i2c_set_clientdata(client, nxe1500);

	mutex_init(&nxe1500->io_lock);

	nxe1500->bank_num = 0;

	if (client->irq) {
		nxe1500->irq_base       = pdata->irq_base;
		nxe1500->chip_irq       = gpio_to_irq(client->irq);
		nxe1500->chip_irq_type  = pdata->irq_type;

		ret = nxe1500_irq_init(nxe1500);
		if (ret) {
			dev_err(&client->dev, "IRQ init failed: %d\n", ret);
			goto err_irq_init;
		}
	}

	ret = nxe1500_add_subdevs(nxe1500, pdata);
	if (ret) {
		dev_err(&client->dev, "add devices failed: %d\n", ret);
		goto err_add_devs;
	}

	nxe1500_noe_init(nxe1500);

#if 0
	nxe1500_gpio_init(nxe1500, pdata);
#endif

#ifdef CONFIG_DEBUG_FS
	nxe1500_debuginit(nxe1500);
#endif

#ifdef CONFIG_NXE1500_WDG_TEST
	nxe1500_watchdog_init(nxe1500);
#endif

	nxe1500_i2c_client = client;

	nxp_board_shutdown = nxe1500_power_off;

#if 0
	if(nxp_board_reset)
		backup_nxp_board_reset = nxp_board_reset;
	else
		backup_nxp_board_reset = NULL;
	nxp_board_reset = nxe1500_restart;
#endif

	return 0;

err_add_devs:
	if (client->irq)
		nxe1500_irq_exit(nxe1500);
err_irq_init:
	kfree(nxe1500);
	return ret;
}

static int  __devexit nxe1500_i2c_remove(struct i2c_client *client)
{
	struct nxe1500 *nxe1500 = i2c_get_clientdata(client);

#ifdef CONFIG_NXE1500_WDG_TEST
	free_irq((IRQ_SYSTEM_END + NXE1500_IRQ_WD), nxe1500);
#endif

	if (client->irq)
		nxe1500_irq_exit(nxe1500);

	nxe1500_remove_subdevs(nxe1500);

	cancel_delayed_work(&nxe1500->dcdc_int_work);
	flush_workqueue(nxe1500->workqueue);
	destroy_workqueue(nxe1500->workqueue);

	kfree(nxe1500);
	return 0;
}

#ifdef CONFIG_PM
static int nxe1500_i2c_suspend(struct i2c_client *client, pm_message_t state)
{
	if (client->irq)
		disable_irq(client->irq);

	return 0;
}

int pwrkey_wakeup;
static int nxe1500_i2c_resume(struct i2c_client *client)
{
	uint8_t reg_val = 0;
	int ret;

	/* Disable all Interrupt */
	__nxe1500_write(client, NXE1500_INTC_INTEN, 0x0);

	ret = __nxe1500_read(client, NXE1500_INT_IR_SYS, &reg_val);
	if (reg_val & 0x01) { /* If PWR_KEY wakeup */
		pwrkey_wakeup = 1;
		/* Clear PWR_KEY IRQ */
		__nxe1500_write(client, NXE1500_INT_IR_SYS, 0x0);
	}
	enable_irq(client->irq);
	
	/* Enable all Interrupt */
	__nxe1500_write(client, NXE1500_INTC_INTEN, 0xff);

	return 0;
}

#endif

static const struct i2c_device_id nxe1500_i2c_id[] = {
	{"nxe1500", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, nxe1500_i2c_id);

static struct i2c_driver nxe1500_i2c_driver = {
	.driver = {
		   .name = "nxe1500",
		   .owner = THIS_MODULE,
		   },
	.probe = nxe1500_i2c_probe,
	.remove = __devexit_p(nxe1500_i2c_remove),
#ifdef CONFIG_PM
	.suspend = nxe1500_i2c_suspend,
	.resume = nxe1500_i2c_resume,
#endif
	.id_table = nxe1500_i2c_id,
};


static int __init nxe1500_i2c_init(void)
{
	int ret = -ENODEV;

	ret = i2c_add_driver(&nxe1500_i2c_driver);
	if (ret != 0)
		pr_err("Failed to register I2C driver: %d\n", ret);

	return ret;
}

subsys_initcall_sync(nxe1500_i2c_init);

static void __exit nxe1500_i2c_exit(void)
{
	i2c_del_driver(&nxe1500_i2c_driver);
}

module_exit(nxe1500_i2c_exit);

MODULE_DESCRIPTION("NEXELL NXE1500 PMU multi-function core driver");
MODULE_LICENSE("GPL");
