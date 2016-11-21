/*
 * Copyright (C) 2016  Nexell Co., Ltd.
 * Author: Bon-gyu, KOO <freestyle@nexell.co.kr>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/suspend.h>
#include <linux/notifier.h>

#include <mach/platform.h>
#include <mach/devices.h>
#include <mach/soc.h>

#include "../iio.h"
#include "../sysfs.h"
#include "../machine.h"

#include "nxp_adc.h"

#ifdef CONFIG_ARCH_S5P4418
#define ADC_LOCK_INIT(LOCK)		spin_lock_init(LOCK)
#define ADC_LOCK(LOCK, FLAG)		spin_lock_irqsave(LOCK, FLAG)
#define ADC_UNLOCK(LOCK, FLAG)		spin_unlock_irqrestore(LOCK, FLAG)
#else	/* CONFIG_ARCH_S5P6818 */
#define ADC_LOCK_INIT(LOCK)		do { } while (0)
#define ADC_LOCK(LOCK, FLAG)		do { } while (0)
#define ADC_UNLOCK(LOCK, FLAG)		do { } while (0)
#endif

#ifdef CONFIG_ARM_NXP_CPUFREQ
#else
#endif

#define ADC_ACCESS_DELAY		80 // ns

#define	ADC_HW_RESET()	do { nxp_soc_peri_reset_set(RESET_ID_ADC); } while (0)


/*
 * ADC data
 */
struct nxp_adc_info {
	struct nxp_adc_data *data;
	void __iomem *adc_base;
	ulong clk_rate;
	ulong sample_rate;
	ulong max_sample_rate;
	ulong min_sample_rate;
	ulong polling_wait;
	int		value;
	int		prescale;
	struct completion completion;
	int irq;
	struct iio_map *map;
	spinlock_t	lock;
};

struct nxp_adc_data {
	int version;

	int (*adc_con)(struct nxp_adc_info *adc);
	int (*read_polling)(struct nxp_adc_info *adc, int ch);
	int (*read_val)(struct iio_dev *indio_dev,
			struct iio_chan_spec const *chan,
			int *val,
			int *val2,
			long mask);
};

static int after_powerup;

static const char *str_adc_ch[] = {
	"adc.0", "adc.1", "adc.2", "adc.3",
	"adc.4", "adc.5", "adc.6", "adc.7",
};

static const char *str_adc_label[] = {
	"ADC0", "ADC1", "ADC2", "ADC3",
	"ADC4", "ADC5", "ADC6", "ADC7",
};

#define ADC_CHANNEL_SPEC(_id) {		\
	.type = IIO_VOLTAGE,		\
	.indexed = 1,			\
	.channel = _id,			\
	.scan_index = _id,		\
}

static struct iio_chan_spec nxp_adc_iio_channels [] = {
	ADC_CHANNEL_SPEC(0),
	ADC_CHANNEL_SPEC(1),
	ADC_CHANNEL_SPEC(2),
	ADC_CHANNEL_SPEC(3),
	ADC_CHANNEL_SPEC(4),
	ADC_CHANNEL_SPEC(5),
	ADC_CHANNEL_SPEC(6),
	ADC_CHANNEL_SPEC(7),
};

/*
 * for get channel
 * 		consumer_dev_name 	= iio_st_channel_get_all 	: name = nxp-adc
 * 		consumer_channel 	= iio_st_channel_get 		: channel_name = adc.0, 1, ...
 *
 * for find channel spec (iio_chan_spec)
 * 		adc_channel_label	= must be equal to iio_chan_spec->datasheet_name = ADC0, 1, ...
 */
#define ADC_CHANNEL_MAP(_id) {	\
    .consumer_dev_name =  DEV_NAME_ADC,	\
}

static struct iio_map nxp_adc_iio_maps [] = {
	ADC_CHANNEL_MAP(0),
	ADC_CHANNEL_MAP(1),
	ADC_CHANNEL_MAP(2),
	ADC_CHANNEL_MAP(3),
	ADC_CHANNEL_MAP(4),
	ADC_CHANNEL_MAP(5),
	ADC_CHANNEL_MAP(6),
	ADC_CHANNEL_MAP(7),
	{ },
};

extern int iio_map_array_register(struct iio_dev *indio_dev, struct iio_map *maps);
extern int iio_map_array_unregister(struct iio_dev *indio_dev, struct iio_map *maps);

static int setup_adc_con(struct nxp_adc_info *adc)
{
	if (adc->data->adc_con)
		adc->data->adc_con(adc);

	return 0;
}

/*
 * ADC functions
 */
static irqreturn_t nxp_adc_v2_isr(int irq, void *dev_id)
{
	struct nxp_adc_info *adc = (struct nxp_adc_info *)dev_id;
	void __iomem *reg = adc->adc_base;

	writel(ADC_V2_INTCLR_CLR, ADC_V2_INTCLR(reg));	/* pending clear */
	adc->value = readl(ADC_V2_DAT(reg));	/* get value */

	complete(&adc->completion);

	return IRQ_HANDLED;
}

static void nxp_adc_v1_ch_start(void __iomem *reg, int ch)
{
	unsigned int adcon = 0;

	adcon = readl(ADC_V1_CON(reg)) & ~ADC_V1_CON_ASEL(7);
	adcon &= ~ADC_V1_CON_ADEN;
	adcon |= ADC_V1_CON_ASEL(ch);	/* channel */
	writel(adcon, ADC_V1_CON(reg));
	adcon = readl(ADC_V1_CON(reg));

	adcon |= ADC_V1_CON_ADEN;	/* start */
	writel(adcon, ADC_V1_CON(reg));
}

static int nxp_adc_v1_read_polling(struct nxp_adc_info *adc, int ch)
{
	void __iomem *reg = adc->adc_base;
	unsigned long wait = adc->polling_wait;
	unsigned long flags = 0;

	unsigned long need_delay;

	ADC_LOCK(&adc->lock, flags);

	nxp_adc_v1_ch_start(reg, ch);

	dsb();
	while (wait > 0) {
		if (!(readl(ADC_V1_CON(reg)) & ADC_V1_CON_ADEN)) {
			/* get value */
			adc->value = readl(ADC_V1_DAT(reg));
			/* pending clear */
			writel(ADC_V1_INTCLR_CLR, ADC_V1_INTCLR(reg));
			break;
		}
		wait--;

		dsb();
	}

	need_delay = 1000000000 / (adc->clk_rate) * (adc->prescale) * 5;
	need_delay = DIV_ROUND_UP(need_delay, 1000);

	ADC_UNLOCK(&adc->lock, flags);

	if (wait == 0)
		return -ETIMEDOUT;

	usleep_range(need_delay, need_delay);

	return 0;
}

static int nxp_adc_v1_adc_con(struct nxp_adc_info *adc)
{
	unsigned int adcon = 0;
	void __iomem *reg = adc->adc_base;

	adcon = ADC_V1_CON_APSV(adc->prescale);
	adcon &= ~ADC_V1_CON_STBY;
	writel(adcon, ADC_V1_CON(reg));
	adcon |= ADC_V1_CON_APEN;	/* after APSV setting */
	writel(adcon, ADC_V1_CON(reg));

	/* ********************************************************
	 * Turn-around invalid value after Power On
	 * ********************************************************/
	if (after_powerup) {
		nxp_adc_v1_read_polling(adc, 0);
		adc->value = 0;

		writel(ADC_V1_INTCLR_CLR, ADC_V1_INTCLR(reg));
		writel(ADC_V1_INTENB_ENB, ADC_V1_INTENB(reg));
	}

	return 0;
}

static int nxp_adc_v1_read_val(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan,
		int *val,
		int *val2,
		long mask)
{
	struct nxp_adc_info *adc = iio_priv(indio_dev);
	int ch = chan->channel;
	int ret = 0;

	INIT_COMPLETION(adc->completion);

	if (adc->data->read_polling)
		ret = adc->data->read_polling(adc, ch);
	if (ret < 0) {
		dev_warn(&indio_dev->dev,
				"Conversion timed out! resetting....\n");
		ADC_HW_RESET();
		setup_adc_con(adc);
		ret = -ETIMEDOUT;
	}

	return ret;
}

static const struct nxp_adc_data nxp_adc_s5p4418_data = {
	.version	= 1,
	.adc_con	= nxp_adc_v1_adc_con,
	.read_polling	= nxp_adc_v1_read_polling,
	.read_val	= nxp_adc_v1_read_val,
};

static void nxp_adc_v2_ch_start(void __iomem *reg, int ch)
{
	unsigned int adcon = 0;

	adcon = readl(ADC_V2_CON(reg)) & ~ADC_V2_CON_ASEL(7);
	adcon &= ~ADC_V2_CON_ADEN;
	adcon |= ADC_V2_CON_ASEL(ch);	/* channel */
	writel(adcon, ADC_V2_CON(reg));
	adcon = readl(ADC_V2_CON(reg));

	adcon |= ADC_V2_CON_ADEN;	/* start */
	writel(adcon, ADC_V2_CON(reg));
}

static int nxp_adc_v2_read_polling(struct nxp_adc_info *adc, int ch)
{
	void __iomem *reg = adc->adc_base;
	unsigned long wait = adc->polling_wait;
	unsigned long flags = 0;

	ADC_LOCK(&adc->lock, flags);

	nxp_adc_v2_ch_start(reg, ch);

	while (wait > 0) {
		if (readl(ADC_V2_INTCLR(reg)) & ADC_V2_INTCLR_CLR) {
			/* pending clear */
			writel(ADC_V2_INTCLR_CLR, ADC_V2_INTCLR(reg));
			/* get value */
			adc->value = readl(ADC_V2_DAT(reg));
			break;
		}
		wait--;
	}

	ADC_UNLOCK(&adc->lock, flags);

	if (wait == 0)
		return -ETIMEDOUT;

	return 0;
}

static int nxp_adc_v2_adc_con(struct nxp_adc_info *adc)
{
	unsigned int adcon = 0;
	unsigned int pres = 0;
	void __iomem *reg = adc->adc_base;

	adcon = ADC_V2_CON_DATA_SEL(ADC_V2_DATA_SEL_VAL) |
		ADC_V2_CON_CLK_CNT(ADC_V2_CLK_CNT_VAL);
	adcon &= ~ADC_V2_CON_STBY;
	writel(adcon, ADC_V2_CON(reg));

	pres = ADC_V2_PRESCON_PRES(adc->prescale);
	writel(pres, ADC_V2_PRESCON(reg));
	pres |= ADC_V2_PRESCON_APEN;
	writel(pres, ADC_V2_PRESCON(reg));

	/* ********************************************************
	 * Turn-around invalid value after Power On
	 * ********************************************************/
	if (after_powerup) {
		nxp_adc_v2_read_polling(adc, 0);
		adc->value = 0;

		writel(ADC_V2_INTCLR_CLR, ADC_V2_INTCLR(reg));
		writel(ADC_V2_INTENB_ENB, ADC_V2_INTENB(reg));
	}

	return 0;
}

static int nxp_adc_v2_read_val(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan,
		int *val,
		int *val2,
		long mask)
{
	struct nxp_adc_info *adc = iio_priv(indio_dev);
	void __iomem *reg = adc->adc_base;
	int ch = chan->channel;
	unsigned long timeout;
	int ret = 0;

	INIT_COMPLETION(adc->completion);

	nxp_adc_v2_ch_start(reg, ch);

	timeout = wait_for_completion_timeout(&adc->completion, ADC_TIMEOUT);
	if (timeout == 0) {
		dev_warn(&indio_dev->dev,
				"Conversion timed out! resetting....\n");
		ADC_HW_RESET();
		setup_adc_con(adc);
		ret = -ETIMEDOUT;
	}

	return ret;
}

static const struct nxp_adc_data nxp_adc_s5p6818_data = {
	.version	= 2,
	.adc_con	= nxp_adc_v2_adc_con,
	.read_polling	= nxp_adc_v2_read_polling,
	.read_val	= nxp_adc_v2_read_val,
};



static int nxp_adc_setup(struct nxp_adc_info *adc, struct platform_device *pdev)
{
	ulong sample_rate, min_rate;
	int prescale = 0;
	int ret = 0;

	sample_rate = *(ulong *)(pdev->dev.platform_data);

	prescale = (adc->clk_rate) / (sample_rate * ADC_MAX_SAMPLE_BITS);
	min_rate = (adc->clk_rate) / (ADC_MAX_PRESCALE * ADC_MAX_SAMPLE_BITS);

	if (sample_rate > ADC_MAX_SAMPLE_RATE ||
		min_rate > sample_rate) {
		pr_err("ADC: not suport %lu(%d ~ %lu) sample rate\n",
			sample_rate, ADC_MAX_SAMPLE_RATE, min_rate);
		return -EINVAL;
	}

	adc->sample_rate = sample_rate;
	adc->max_sample_rate = ADC_MAX_SAMPLE_RATE;
	adc->min_sample_rate = min_rate;
	adc->prescale = prescale-1;
	adc->polling_wait = (ADC_MAX_SAMPLE_BITS + 1) * \
		(1000000000 / (adc->sample_rate * ADC_ACCESS_DELAY));

	ADC_LOCK_INIT(&adc->lock);

	setup_adc_con(adc);
	init_completion(&adc->completion);

	pr_info("ADC: CHs %d, %ld(%ld ~ %ld) sample rate, scale=%d(bit %d)\n",
		ARRAY_SIZE(nxp_adc_iio_channels), adc->sample_rate,
		adc->max_sample_rate, adc->min_sample_rate,
		adc->prescale, ADC_MAX_SAMPLE_BITS);

	return ret;
}

static void nxp_adc_release(struct nxp_adc_info *adc)
{
}

static int nxp_read_raw(struct iio_dev *indio_dev,
				struct iio_chan_spec const *chan,
				int *val,
				int *val2,
				long mask)
{
	struct nxp_adc_info *adc = iio_priv(indio_dev);
	int ret;

	mutex_lock(&indio_dev->mlock);

	nx_dvfs_target_lock();

	if (adc->data->read_val) {
		ret = adc->data->read_val(indio_dev, chan, val, val2, mask);
		if (ret < 0)
			goto out;
	}

	*val = adc->value;
	*val2 = 0;
	ret = IIO_VAL_INT;

	pr_debug("%s, ch=%d, val=0x%x\n", __func__, chan->channel, *val);

out:
	nx_dvfs_target_unlock();
	mutex_unlock(&indio_dev->mlock);

	return ret;
}

static const struct iio_info nxp_adc_iio_info = {
	.read_raw = &nxp_read_raw,
	.driver_module = THIS_MODULE,
};

static int nxp_adc_suspend(struct platform_device *pdev, pm_message_t state)
{
	after_powerup = 0;
	return 0;
}

static int nxp_adc_resume(struct platform_device *pdev)
{
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);
	struct nxp_adc_info *adc = iio_priv(indio_dev);

	ADC_HW_RESET();

	after_powerup = 1;
	setup_adc_con(adc);

	return 0;
}

static int __devinit nxp_adc_probe(struct platform_device *pdev)
{
	struct clk *clk = NULL;
	struct iio_dev *iio = NULL;
	struct nxp_adc_info *adc = NULL;
	struct iio_chan_spec *spec;
	struct resource	*mem;
	struct iio_map *map;
	int i = 0;
	int ret = -ENODEV;

	iio = iio_allocate_device(sizeof(struct nxp_adc_info));
	if (!iio) {
		pr_err("Fail: allocating iio ADC device\n");
		return -ENOMEM;
	}

	adc = iio_priv(iio);

#ifdef CONFIG_ARCH_S5P4418
	adc->data = (struct nxp_adc_data *)&nxp_adc_s5p4418_data;
#else
	adc->data = (struct nxp_adc_data *)&nxp_adc_s5p6818_data;
#endif

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	adc->adc_base = devm_request_and_ioremap(&pdev->dev, mem);
	if (!adc->adc_base) {
		ret = -ENOMEM;
		goto err_iio_free;
	}

	after_powerup = 1;

	/* setup: clock */
	clk	= clk_get(&pdev->dev, NULL);
	if (IS_ERR(clk)) {
		pr_err("Fail: getting clock ADC !!!\n");
		return -EINVAL;
	}
	adc->clk_rate = clk_get_rate(clk);
	clk_put(clk);

	/* setup: reset */
	ADC_HW_RESET();

	/* setup: irq */
	if (adc->data->version == 2) {
		int irq;

		irq = platform_get_irq(pdev, 0);
		if (irq < 0) {
			pr_err("failed get irq resource\n");
			goto err_iio_release;
		}
		if ((irq >= 0) || (NR_IRQS > irq)) {
			ret = devm_request_irq(&pdev->dev, irq, nxp_adc_v2_isr,
					0, DEV_NAME_ADC, adc);
			if (ret < 0) {
				pr_err("failed get irq (%d)\n", irq);
				goto err_iio_release;
			}
		}

		adc->irq = irq;
	}

	/* setup: adc */
	ret = nxp_adc_setup(adc, pdev);
	if (0 > ret) {
		pr_err("Fail: setup iio ADC device\n");
		goto err_iio_free;
	}

	platform_set_drvdata(pdev, iio);

	iio->name = DEV_NAME_ADC;
	iio->dev.parent = &pdev->dev;
	iio->info = &nxp_adc_iio_info;
	iio->modes = INDIO_DIRECT_MODE;
	iio->channels = nxp_adc_iio_channels;
	iio->num_channels = ARRAY_SIZE(nxp_adc_iio_channels);

	/*
	 * sys interface : user interface
	 */
	spec = nxp_adc_iio_channels;
	for (i = 0; iio->num_channels > i; i++)
		spec[i].datasheet_name = str_adc_label[i];

	ret = iio_device_register(iio);
	if (ret)
		goto err_iio_release;

	/*
	 * inkern interface : kernel interface
	 */
	map = nxp_adc_iio_maps;
	for (i = 0; ARRAY_SIZE(nxp_adc_iio_maps) - 1 > i; i++) {
		map[i].consumer_channel = str_adc_ch[i];
		map[i].adc_channel_label = str_adc_label[i];
	}

	ret = iio_map_array_register(iio, map);
	if (ret)
		goto err_iio_register;

	adc->map = nxp_adc_iio_maps;


	pr_debug("ADC init success\n");

	return 0;

err_iio_register:
	iio_device_unregister(iio);
err_iio_release:
	nxp_adc_release(adc);
err_iio_free:
	iio_free_device(iio);
	pr_err("Fail: load ADC driver ...\n");
	return ret;
}

static int __devexit nxp_adc_remove(struct platform_device *pdev)
{
	struct iio_dev *iio = platform_get_drvdata(pdev);
	struct nxp_adc_info *adc = iio_priv(iio);

	iio_device_unregister(iio);
	if (adc->map)
		iio_map_array_unregister(iio, adc->map);

	nxp_adc_release(adc);
	platform_set_drvdata(pdev, NULL);

	iio_free_device(iio);
	return 0;
}

static struct platform_driver nxp_adc_driver = {
	.probe		= nxp_adc_probe,
	.remove		= __devexit_p(nxp_adc_remove),
	.suspend	= nxp_adc_suspend,
	.resume		= nxp_adc_resume,
	.driver		= {
		.name	= DEV_NAME_ADC,
		.owner	= THIS_MODULE,
	},
};

module_platform_driver(nxp_adc_driver);

MODULE_AUTHOR("Bon-gyu, KOO <freestyle@nexell.co.kr>");
MODULE_DESCRIPTION("ADC driver for the Nexell");
MODULE_LICENSE("GPL");
