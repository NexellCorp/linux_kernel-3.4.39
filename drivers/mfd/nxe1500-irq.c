/*
 * driver/mfd/nxe1500-irq.c
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
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/mfd/nxe1500.h>
#include <nxe1500-private.h>


static int gpedge_add[] = {
	NXE1500_GPIO_GPEDGE1,
	NXE1500_GPIO_GPEDGE2
};

static int irq_en_add[] = {
	NXE1500_INT_EN_SYS,
	NXE1500_INT_EN_DCDC,
	NXE1500_INT_EN_GPIO,
	NXE1500_INT_EN_GPIO2,
};

static int irq_mon_add[] = {
	NXE1500_INT_IR_SYS,		/* NXE1500_INT_MON_SYS, */
	NXE1500_INT_IR_DCDC,		/* NXE1500_INT_MON_DCDC, */
	NXE1500_INT_IR_GPIOR,
	NXE1500_INT_IR_GPIOF,
};

static int irq_clr_add[] = {
	NXE1500_INT_IR_SYS,
	NXE1500_INT_IR_DCDC,
	NXE1500_INT_IR_GPIOR,
	NXE1500_INT_IR_GPIOF,
};

static int main_int_type[] = {
	SYS_INT,
	DCDC_INT,
	GPIO_INT,
	GPIO_INT,
};

struct nxe1500_irq_data {
	u8	int_type;
	u8	master_bit;
	u8	int_en_bit;
	u8	mask_reg_index;
	int	grp_index;
};

#define NXE1500_IRQ(_int_type, _master_bit, _grp_index, _int_bit, _mask_ind) \
	{						\
		.int_type	= _int_type,		\
		.master_bit	= _master_bit,		\
		.grp_index	= _grp_index,		\
		.int_en_bit	= _int_bit,		\
		.mask_reg_index	= _mask_ind,		\
	}

static const struct nxe1500_irq_data nxe1500_irqs[NXE1500_NR_IRQS] = {
	[NXE1500_IRQ_POWER_ON]		= NXE1500_IRQ(SYS_INT,  0, 0, 0, 0),
	[NXE1500_IRQ_EXTIN]			= NXE1500_IRQ(SYS_INT,  0, 1, 1, 0),
	[NXE1500_IRQ_PRE_VINDT]		= NXE1500_IRQ(SYS_INT,  0, 2, 2, 0),
	[NXE1500_IRQ_PREOT]			= NXE1500_IRQ(SYS_INT,  0, 3, 3, 0),
	[NXE1500_IRQ_POWER_OFF]		= NXE1500_IRQ(SYS_INT,  0, 4, 4, 0),
	[NXE1500_IRQ_NOE_OFF]		= NXE1500_IRQ(SYS_INT,  0, 5, 5, 0),
	[NXE1500_IRQ_WD]			= NXE1500_IRQ(SYS_INT,  0, 6, 6, 0),

	[NXE1500_IRQ_DC1LIM]		= NXE1500_IRQ(DCDC_INT, 1, 0, 0, 1),
	[NXE1500_IRQ_DC2LIM]		= NXE1500_IRQ(DCDC_INT, 1, 1, 1, 1),
	[NXE1500_IRQ_DC3LIM]		= NXE1500_IRQ(DCDC_INT, 1, 2, 2, 1),
	[NXE1500_IRQ_DC4LIM]		= NXE1500_IRQ(DCDC_INT, 1, 3, 3, 1),

	[NXE1500_IRQ_GPIO0]			= NXE1500_IRQ(GPIO_INT, 4, 0, 0, 6),
	[NXE1500_IRQ_GPIO1]			= NXE1500_IRQ(GPIO_INT, 4, 1, 1, 6),
	[NXE1500_IRQ_GPIO2]			= NXE1500_IRQ(GPIO_INT, 4, 2, 2, 6),
	[NXE1500_IRQ_GPIO3]			= NXE1500_IRQ(GPIO_INT, 4, 3, 3, 6),
	[NXE1500_IRQ_GPIO4]			= NXE1500_IRQ(GPIO_INT, 4, 4, 4, 6),
};

static void nxe1500_irq_lock(struct irq_data *irq_data)
{
	struct nxe1500 *nxe1500 = irq_data_get_irq_chip_data(irq_data);

	mutex_lock(&nxe1500->irq_lock);
}

static void nxe1500_irq_unmask(struct irq_data *irq_data)
{
	struct nxe1500 *nxe1500 = irq_data_get_irq_chip_data(irq_data);
	unsigned int __irq = irq_data->irq - nxe1500->irq_base;
	const struct nxe1500_irq_data *data = &nxe1500_irqs[__irq];

	nxe1500->group_irq_en[data->master_bit] |= (1 << data->grp_index);
	if (nxe1500->group_irq_en[data->master_bit])
		nxe1500->intc_inten_reg |= 1 << data->master_bit;

	if (data->master_bit == 6)	/* if Charger */
		nxe1500->irq_en_reg[data->mask_reg_index]
						&= ~(1 << data->int_en_bit);
	else
		nxe1500->irq_en_reg[data->mask_reg_index]
						|= 1 << data->int_en_bit;
}

static void nxe1500_irq_mask(struct irq_data *irq_data)
{
	struct nxe1500 *nxe1500 = irq_data_get_irq_chip_data(irq_data);
	unsigned int __irq = irq_data->irq - nxe1500->irq_base;
	const struct nxe1500_irq_data *data = &nxe1500_irqs[__irq];

	nxe1500->group_irq_en[data->master_bit] &= ~(1 << data->grp_index);
	if (!nxe1500->group_irq_en[data->master_bit])
		nxe1500->intc_inten_reg &= ~(1 << data->master_bit);

	if (data->master_bit == 6)	/* if Charger */
		nxe1500->irq_en_reg[data->mask_reg_index]
						|= 1 << data->int_en_bit;
	else
		nxe1500->irq_en_reg[data->mask_reg_index]
						&= ~(1 << data->int_en_bit);
}

static void nxe1500_irq_sync_unlock(struct irq_data *irq_data)
{
	struct nxe1500 *nxe1500 = irq_data_get_irq_chip_data(irq_data);
	int i;

	for (i = 0; i < ARRAY_SIZE(nxe1500->gpedge_reg); i++) {
		if (nxe1500->gpedge_reg[i] != nxe1500->gpedge_cache[i]) {
			if (!WARN_ON(nxe1500_write(nxe1500->dev,
						    gpedge_add[i],
						    nxe1500->gpedge_reg[i])))
				nxe1500->gpedge_cache[i] =
						nxe1500->gpedge_reg[i];
		}
	}

	for (i = 0; i < ARRAY_SIZE(nxe1500->irq_en_reg); i++) {
		if (nxe1500->irq_en_reg[i] != nxe1500->irq_en_cache[i]) {
			if (!WARN_ON(nxe1500_write(nxe1500->dev,
						irq_en_add[i],
						nxe1500->irq_en_reg[i])))
				nxe1500->irq_en_cache[i] =
						nxe1500->irq_en_reg[i];
		}
	}

	if (nxe1500->intc_inten_reg != nxe1500->intc_inten_cache) {
		if (!WARN_ON(nxe1500_write(nxe1500->dev,
				NXE1500_INTC_INTEN, nxe1500->intc_inten_reg)))
			nxe1500->intc_inten_cache = nxe1500->intc_inten_reg;
	}

	mutex_unlock(&nxe1500->irq_lock);
}

static int nxe1500_irq_set_type(struct irq_data *irq_data, unsigned int type)
{
	struct nxe1500 *nxe1500 = irq_data_get_irq_chip_data(irq_data);
	unsigned int __irq = irq_data->irq - nxe1500->irq_base;
	const struct nxe1500_irq_data *data = &nxe1500_irqs[__irq];
	int val = 0;
	int gpedge_index;
	int gpedge_bit_pos;

	if (data->int_type & GPIO_INT) {
		gpedge_index = data->int_en_bit / 4;
		gpedge_bit_pos = data->int_en_bit % 4;

		if (type & IRQ_TYPE_EDGE_FALLING)
			val |= 0x2;

		if (type & IRQ_TYPE_EDGE_RISING)
			val |= 0x1;

		nxe1500->gpedge_reg[gpedge_index] &= ~(3 << gpedge_bit_pos);
		nxe1500->gpedge_reg[gpedge_index] |= (val << gpedge_bit_pos);
		nxe1500_irq_unmask(irq_data);
	}
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int nxe1500_irq_set_wake(struct irq_data *irq_data, unsigned int on)
{
	struct nxe1500 *nxe1500 = irq_data_get_irq_chip_data(irq_data);
	return irq_set_irq_wake(nxe1500->chip_irq, on);	/* i2c->irq */
}
#else
#define nxe1500_irq_set_wake NULL
#endif

static void nxe1500_dcdc_int_work(struct work_struct *work)
{
	struct nxe1500 *nxe1500 = container_of(work, struct nxe1500, dcdc_int_work.work);
	int ret = 0;

	printk(KERN_ERR "## DCDC Interrupt Flag Register(0x%x) clear. \n", NXE1500_INT_IR_DCDC);
	ret = nxe1500_write(nxe1500->dev, NXE1500_INT_IR_DCDC, 0x00);
	if (ret < 0) {
		dev_err(nxe1500->dev, "Error in write reg 0x%02x error: %d\n", NXE1500_INT_IR_DCDC, ret);
	}
	ret = nxe1500_write(nxe1500->dev,NXE1500_INT_EN_DCDC, 0x03);

	return;
}

static irqreturn_t nxe1500_dcdc2_lim_isr(int irq, void *data)
{
	struct nxe1500 *nxe1500 = data;
	u8 value;
	int ret;

	ret = nxe1500_read(nxe1500->dev, NXE1500_REG_DC2CTL2, &value);
	value =	0x3 & (value >> NXE1500_POS_DCxCTL2_DCxLIM);

	switch(value)
	{
		case 0: 
			printk(KERN_ERR "##\e[31m The current limit detection of DCDC2(No Limit). \e[0m \n");
			break;
		
		case 1: 
			printk(KERN_ERR "##\e[31m The current limit detection of DCDC2(3.2A). \e[0m \n");
			break;

		case 2: 
			printk(KERN_ERR "##\e[31m The current limit detection of DCDC2(3.7A). \e[0m \n");
			break;

		case 3: 
			printk(KERN_ERR "##\e[31m The current limit detection of DCDC2(4A). \e[0m \n");
			break;
	}
	return IRQ_HANDLED;
}
static irqreturn_t nxe1500_dcdc1_lim_isr(int irq, void *data)
{
	struct nxe1500 *nxe1500 = data;
	u8 value;
	int ret;

	ret = nxe1500_read(nxe1500->dev, NXE1500_REG_DC1CTL2, &value);
	value =	0x3 & (value >> NXE1500_POS_DCxCTL2_DCxLIM);

	switch(value)
	{
		case 0: 
			printk(KERN_ERR "##\e[31m The current limit detection of DCDC1(No Limit). \e[0m \n");
			break;
		
		case 1: 
			printk(KERN_ERR "##\e[31m The current limit detection of DCDC1(3.2A). \e[0m \n");
			break;

		case 2: 
			printk(KERN_ERR "##\e[31m The current limit detection of DCDC1(3.7A). \e[0m \n");
			break;

		case 3: 
			printk(KERN_ERR "##\e[31m The current limit detection of DCDC1(4A). \e[0m \n");
			break;
	}

	return IRQ_HANDLED;
}

static irqreturn_t nxe1500_irq_isr(int irq, void *data)
{
	struct nxe1500 *nxe1500 = data;
	u8 int_sts[MAX_INTERRUPT_MASKS];
	u8 master_int;
	int i;
	int ret;
	unsigned int rtc_int_sts = 0;

	/* disable_irq_nosync(irq); */
	/* Clear the status */
	for (i = 0; i < MAX_INTERRUPT_MASKS; i++)
		int_sts[i] = 0;

	ret = nxe1500_read(nxe1500->dev, NXE1500_INTC_INTMON,
						&master_int);
	if (ret < 0) {
		dev_err(nxe1500->dev, "Error in reading reg 0x%02x "
			"error: %d\n", NXE1500_INTC_INTMON, ret);
		return IRQ_HANDLED;
	}

	for (i = 0; i < MAX_INTERRUPT_MASKS; ++i) {
		/* Even if INTC_INTMON register = 1, INT signal might not
		 * output because INTC_INTMON register indicates only interrupt
		 * facter level.
		 * So remove the following procedure
		 */
		if (!(master_int & main_int_type[i])) 
			continue;

		ret = nxe1500_read(nxe1500->dev,
				irq_mon_add[i], &int_sts[i]);
		if (ret < 0) {
			dev_err(nxe1500->dev, "Error in reading reg 0x%02x "
				"error: %d\n", irq_mon_add[i], ret);
			int_sts[i] = 0;
			continue;
		}
		if (!int_sts[i])
			continue;

		if (main_int_type[i] & RTC_INT) {
			/* Changes status bit position
				 from RTCCNT2 to RTCCNT1 */
			rtc_int_sts = 0;
			if (int_sts[i] & 0x1)
				rtc_int_sts |= BIT(6);
			if (int_sts[i] & 0x4)
				rtc_int_sts |= BIT(0);
		}

		if(irq_clr_add[i] == NXE1500_INT_IR_RTC)
		{
			int_sts[i] &= ~0x85;
			ret = nxe1500_write(nxe1500->dev,
				irq_clr_add[i], int_sts[i]);
			if (ret < 0) {
				dev_err(nxe1500->dev, "Error in writing reg 0x%02x "
				"error: %d\n", irq_clr_add[i], ret);
			}
		}
#ifdef CONFIG_NXE1500_WDG_TEST
		else if (main_int_type[i] & WDG_INT) /* Mask Watchdog Interrupt */
		{
			printk(KERN_ERR "## \e[31m%s\e[0m() WDG_INT \n", __func__);
		}
#endif
		else if (main_int_type[i] & DCDC_INT) { /* Mask DCDC Interrupt */
			// printk(KERN_ERR "## \e[31m %s\e[0m() DCDC_INT \n", __func__);
			ret = nxe1500_write(nxe1500->dev, NXE1500_INT_EN_DCDC, 0x0);
			if (ret < 0) {
				dev_err(nxe1500->dev, "Error in writing reg 0x%02x "
									"error: %d\n", NXE1500_INT_EN_DCDC, ret);
			}
			queue_delayed_work(nxe1500->workqueue, &nxe1500->dcdc_int_work, 
							msecs_to_jiffies(1000));
		}
		else 
		{
			ret = nxe1500_write(nxe1500->dev, irq_clr_add[i], ~int_sts[i]);
			if (ret < 0) {
				dev_err(nxe1500->dev, "Error in writing reg 0x%02x "
				"error: %d\n", irq_clr_add[i], ret);
			}
		}
		
		/* Mask Charger Interrupt */
		if (main_int_type[i] & CHG_INT) {
			if (int_sts[i])
				ret = nxe1500_write(nxe1500->dev,
							irq_en_add[i], 0xff);
				if (ret < 0) {
					dev_err(nxe1500->dev,
						"Error in write reg 0x%02x error: %d\n",
							irq_en_add[i], ret);
				}
		}
		/* Mask ADC Interrupt */
		if (main_int_type[i] & ADC_INT) {
			if (int_sts[i])
				ret = nxe1500_write(nxe1500->dev,
							irq_en_add[i], 0);
				if (ret < 0) {
					dev_err(nxe1500->dev,
						"Error in write reg 0x%02x error: %d\n",
							irq_en_add[i], ret);
				}
		}

		if (main_int_type[i] & RTC_INT)
			int_sts[i] = rtc_int_sts;

	}

	/* Call interrupt handler if enabled */
	for (i = 0; i < NXE1500_NR_IRQS; ++i) {
		const struct nxe1500_irq_data *data = &nxe1500_irqs[i];
		if ((int_sts[data->mask_reg_index] & (1 << data->int_en_bit)) &&
			(nxe1500->group_irq_en[data->master_bit] &
					(1 << data->grp_index)))
			handle_nested_irq(nxe1500->irq_base + i);
	}

	return IRQ_HANDLED;
}

static struct irq_chip nxe1500_irq_chip = {
	.name = "nxe1500",
	.irq_mask = nxe1500_irq_mask,
	.irq_unmask = nxe1500_irq_unmask,
	.irq_bus_lock = nxe1500_irq_lock,
	.irq_bus_sync_unlock = nxe1500_irq_sync_unlock,
	.irq_set_type = nxe1500_irq_set_type,
	.irq_set_wake = nxe1500_irq_set_wake,
};

int nxe1500_irq_init(struct nxe1500 *nxe1500)
{
	int i, ret;
	u8 reg_data = 0;

	if (!nxe1500->irq_base) {
		dev_warn(nxe1500->dev, "No interrupt support on IRQ base\n");
		return -EINVAL;
	}

	mutex_init(&nxe1500->irq_lock);

	/* NXE1500_INT_EN_SYS */
	nxe1500->irq_en_cache[0] = 0;
	nxe1500->irq_en_reg[0] = 0;

	/* NXE1500_INT_EN_DCDC */
	nxe1500->irq_en_cache[1] = 0x03;
	nxe1500->irq_en_reg[1] = 0x03;

	/* NXE1500_INT_EN_GPIO */
	nxe1500->irq_en_cache[2] = 0x0;
	nxe1500->irq_en_reg[2] = 0x0;

	nxe1500->intc_inten_cache = 0;
	nxe1500->intc_inten_reg = 0;

	for (i = 0; i < MAX_GPEDGE_REG; i++) {
		nxe1500->gpedge_cache[i] = 0;
		nxe1500->gpedge_reg[i] = 0;
	}

	/* Initailize all int register to 0 */
	for (i = 0; i < MAX_INTERRUPT_MASKS; i++)  {
		ret = nxe1500_write(nxe1500->dev,
				irq_en_add[i],
				nxe1500->irq_en_reg[i]);
		if (ret < 0)
			dev_err(nxe1500->dev, "Error in writing reg 0x%02x "
				"error: %d\n", irq_en_add[i], ret);
	}

	for (i = 0; i < MAX_GPEDGE_REG; i++)  {
		ret = nxe1500_write(nxe1500->dev,
				gpedge_add[i],
				nxe1500->gpedge_reg[i]);
		if (ret < 0)
			dev_err(nxe1500->dev, "Error in writing reg 0x%02x "
				"error: %d\n", gpedge_add[i], ret);
	}

	ret = nxe1500_write(nxe1500->dev, NXE1500_INTC_INTEN, 0x0);
	if (ret < 0)
		dev_err(nxe1500->dev, "Error in writing reg 0x%02x "
				"error: %d\n", NXE1500_INTC_INTEN, ret);

	/* Clear all interrupts in case they woke up active. */
	for (i = 0; i < MAX_INTERRUPT_MASKS; i++)  {
		if (irq_clr_add[i] != NXE1500_INT_IR_RTC) {
			ret = nxe1500_write(nxe1500->dev,
						irq_clr_add[i], 0);
			if (ret < 0)
				dev_err(nxe1500->dev, "Error in writing reg 0x%02x "
					"error: %d\n", irq_clr_add[i], ret);
		} else {
			ret = nxe1500_read(nxe1500->dev,
					NXE1500_INT_IR_RTC, &reg_data);
			if (ret < 0)
				dev_err(nxe1500->dev, "Error in reading reg 0x%02x "
					"error: %d\n", NXE1500_INT_IR_RTC, ret);
			reg_data &= 0xf0;
			ret = nxe1500_write(nxe1500->dev,
					NXE1500_INT_IR_RTC, reg_data);
			if (ret < 0)
				dev_err(nxe1500->dev, "Error in writing reg 0x%02x "
					"error: %d\n", NXE1500_INT_IR_RTC, ret);
		}
	}


	for (i = 0; i < NXE1500_NR_IRQS; i++) {
		int __irq = i + nxe1500->irq_base;
		irq_set_chip_data(__irq, nxe1500);
		irq_set_chip_and_handler(__irq, &nxe1500_irq_chip,
					 handle_simple_irq);
		irq_set_nested_thread(__irq, 1);
#ifdef CONFIG_ARM
		set_irq_flags(__irq, IRQF_VALID);
#endif
	}

	ret = request_threaded_irq(nxe1500->chip_irq, NULL, nxe1500_irq_isr,
			nxe1500->chip_irq_type|IRQF_DISABLED|IRQF_ONESHOT,
						"nxe1500", nxe1500);

	nxe1500->workqueue = create_singlethread_workqueue("nxe1500_dcdc_irq");
	INIT_DELAYED_WORK_DEFERRABLE(&nxe1500->dcdc_int_work, nxe1500_dcdc_int_work);

	ret = nxe1500_update(nxe1500->dev, NXE1500_REG_DC1CTL2, 0x06, 0x07);
	ret = request_threaded_irq(nxe1500->irq_base + NXE1500_IRQ_DC1LIM,
					NULL, nxe1500_dcdc1_lim_isr, IRQF_ONESHOT, "nxe1500_dc1lim", nxe1500);

	ret = nxe1500_update(nxe1500->dev, NXE1500_REG_DC2CTL2, 0x06, 0x07);
	ret = request_threaded_irq(nxe1500->irq_base + NXE1500_IRQ_DC2LIM,
					NULL, nxe1500_dcdc2_lim_isr, IRQF_ONESHOT, "nxe1500_dc2lim", nxe1500);

	if (ret < 0)
		dev_err(nxe1500->dev, "Error in registering interrupt "
				"error: %d\n", ret);
	if (!ret) {
		device_init_wakeup(nxe1500->dev, 1);
		enable_irq_wake(nxe1500->chip_irq);
	}

	return ret;
}

int nxe1500_irq_exit(struct nxe1500 *nxe1500)
{
	if (nxe1500->chip_irq)
		free_irq(nxe1500->chip_irq, nxe1500);
	return 0;
}

