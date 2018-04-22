/*
 * (C) Copyright 2018
 * Author: junghyun, kim <jhkim@nexell.co.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/pwm.h>
#include <linux/delay.h>
#include <sound/soc.h>

#include <mach/platform.h>
#include <mach/devices.h>
#include <mach/soc.h>

#include "nxp-pcm.h"
#include "nxp-spi-pl022.h"

//#define dev_dbg		dev_info

#define	SVI_IOBASE_MAX		6
#define	SVI_CLKGENBASE_MAX	3
#define	SVI_PDM_PLL		8000000	/* 8Mhz */
#define	DETECT_LRCK_WAIT 2000

#define IO_GROUP(io)	((io >> 0x5) & 0x07)
#define IO_OFFSET(io)	((io & 0x1F) >> 0x0)
#define us_to_ktime(u)  ns_to_ktime((u64)u * 1000)

struct io_base {
	unsigned int p;
	void __iomem *v;
};

struct gpio_data {
	int io, f_alt, f_io;
};

struct clk_gen {
	int s[2], d[2];
};

#define	CLKGEN_REG_ENB	0x00
#define	CLKGEN_REG_CON0	0x04
#define	CLKGEN_REG_CON1	0x0C

static const dma_addr_t gpio_base[] = {
	PHY_BASEADDR_GPIOA, PHY_BASEADDR_GPIOB, PHY_BASEADDR_GPIOC,
	PHY_BASEADDR_GPIOD, PHY_BASEADDR_GPIOE, PHY_BASEADDR_ALIVE,
};

static const dma_addr_t spi_base[] = {
	PHY_BASEADDR_SSP0, PHY_BASEADDR_SSP1, PHY_BASEADDR_SSP2,
};

static const dma_addr_t i2s_base[] = {
	PHY_BASEADDR_I2S0, PHY_BASEADDR_I2S1, PHY_BASEADDR_I2S2,
};

static struct gpio_i2s {
	struct gpio_data mclk, bclk, lr, out, in;
} gpio_i2s[] = {
	[0] = {
		.mclk = {((3 * 32) + 13), 1, 0},
		.bclk = {((3 * 32) + 10), 1, 0},
		.lr   = {((3 * 32) + 12), 1, 0},
		.out  = {((3 * 32) +  9), 1, 0},
		.in   = {((3 * 32) + 11), 1, 0},
	},
	[1] = {
		.mclk = {((0 * 32) + 28), 3, 0},
		.bclk = {((0 * 32) + 30), 3, 0},
		.lr   = {((1 * 32) +  0), 3, 0},
		.out  = {((1 * 32) +  6), 3, 0},
		.in   = {((1 * 32) +  9), 3, 0},
	},
	[2] = {
		.mclk = {((0 * 32) + 28), 2, 0},
		.bclk = {((1 * 32) +  2), 3, 0},
		.lr   = {((1 * 32) +  4), 3, 0},
		.out  = {((1 * 32) +  8), 3, 0},
		.in   = {((1 * 32) + 10), 3, 0},
	},
};

static const dma_addr_t clkgen_i2s_base[] = {
	PHY_BASEADDR_CLKGEN15, PHY_BASEADDR_CLKGEN16, PHY_BASEADDR_CLKGEN17,
};

enum svoice_pin_type {
	SVI_PIN_SPI_CS,
	SVI_PIN_PDM_LCRCK,
	SVI_PIN_PDM_ISRUN,
	SVI_PIN_PDM_NRST,
	SVI_PIN_I2S_LRCK,
};

struct svoice_pin {
	const char *property;
	int output;
	int nr;
	void __iomem *base;
	int group;
	int offset;
};

static struct svoice_pin svoice_pins[] = {
	[SVI_PIN_SPI_CS] = { "spi-cs-gpio", 1, -1, },		/* output */
	[SVI_PIN_PDM_LCRCK] = { "pdm-lrck-gpio", 1, -1, },	/* output */
	[SVI_PIN_PDM_ISRUN] = { "pdm-isrun-gpio", 1, -1, },	/* output */
	[SVI_PIN_PDM_NRST] = { "pdm-nrst-gpio", 1, -1, },	/* output */
	[SVI_PIN_I2S_LRCK] = { "i2s-lrck-gpio", 0, -1, },	/* status */
};
#define	SVI_PIN_NUM	ARRAY_SIZE(svoice_pins)

struct svoice_dev;
struct svoice_snd;

struct svoice_dev_ops {
	void (*prepare)(struct svoice_dev *, bool);
	void (*unprepare)(struct svoice_dev *);
	void (*start)(struct svoice_dev *);
	void (*stop)(struct svoice_dev *);
	void (*clear)(struct svoice_dev *);
};

struct svoice_dev {
	struct snd_soc_dai *cpu_dai;
	struct list_head list;	/* next */
	struct svoice_snd *snd;
	int channel;
	struct io_base base;
	struct svoice_dev_ops *ops;
	enum svoice_dev_type type;
	bool start;
	bool is_ref;
	bool mclk_ext;
	struct clk_gen clk;

	/*
	 *  snd_soc_dai_ops member is linked with const type,
	 *  replace with all ops struct
	 */
	const struct snd_soc_dai_ops *save_ops;
	struct snd_soc_dai_ops hook_ops;
};

struct svoice_snd {
	struct device *dev;
	struct list_head list;
	int num_lists;
	struct svoice_dev *ref;
	int ref_mode; /* 0 = I2S, 1 = Left (MSB), 2 = Right (LSB)  */
	struct gpio_data ref_gpio;
#ifdef CONFIG_HAVE_PWM
	struct pwm_device *pwm;
#endif
	spinlock_t lock;
	enum svoice_dev_type mic_type;
	bool exist_play;

	bool detect;
	struct nxp_pcm_rate_detector *rate_detector;
	struct io_base io_bases[SVI_IOBASE_MAX];
	struct io_base clkgen_bases[SVI_CLKGENBASE_MAX];
};

#define	find_list_dev(d, s, t)	{ \
	list_for_each_entry(d, &s->list, list)	\
		if (d->cpu_dai == t)	\
			break;	\
	}

/* SPI CS: L ON */
static inline void __spi_cs_set(void)
{
	struct svoice_pin *pin = &svoice_pins[SVI_PIN_SPI_CS];

	pr_debug("%s [gpio_%c.%02d] %s\n", __func__,
		('A' + pin->group), pin->offset, pin->property);

	gpio_set_value(pin->nr, 0);
}

/* SPI CS: H OFF */
static inline void __spi_cs_free(void)
{
	struct svoice_pin *pin = &svoice_pins[SVI_PIN_SPI_CS];

	pr_debug("%s [gpio_%c.%02d] %s\n", __func__,
		('A' + pin->group), pin->offset, pin->property);

	gpio_set_value(pin->nr, 1);
}

/* SSPCR1 = 0x04 */
static inline void __spi_start(struct svoice_dev *sv)
{
	void __iomem *base = sv->base.v;

	pl022_enable(base, 1);
}

/* SSPSR */
static inline void __spi_clear(struct svoice_dev *sv)
{
	void __iomem *base = sv->base.v;
	u32 val;

	/* wait for fifo flush */
	val = __raw_readl(SSP_SR(base));
	if (val & (1 << 2)) {
		pr_info("SPI: spi.%p rx not empty (0x%08x)!!!\n", base, val);
		while (__raw_readl(SSP_SR(base)) & (1 << 2))
			val = __raw_readl(SSP_DR(base));
	}
}

static struct svoice_dev_ops spi_ops = {
	.start = __spi_start,
	.clear = __spi_clear,
};

static void __i2s_prepare(struct svoice_dev *sv, bool refin)
{
	void __iomem *base = sv->snd->clkgen_bases[sv->channel].v;
	int s[2] = { sv->clk.s[0], sv->clk.s[1] };
	int d[2] = { sv->clk.d[0], sv->clk.d[1] };
	u32 con0, con1;

	if (sv->snd->exist_play && (sv->channel == 0))
		return;

	/*
	 * I2S mclk change:
	 * if exist reference signal, set external mclk from reference signal.
	 * if no exist reference signal, use inn
	 */
	con0 = readl(base + CLKGEN_REG_CON0);
	con1 = readl(base + CLKGEN_REG_CON1);

	con0 &= ~((0x7 << 2) | (0xff << 5));
	con1 &= ~((0x7 << 2) | (0xff << 5));
	con0 |= ((s[0] & 0x7) << 2) | ((d[0] & 0xff) << 5);
	con1 |= ((s[1] & 0x7) << 2) | ((d[1] & 0xff) << 5);

	dev_dbg(sv->snd->dev,
		"clk [0] s:%d, d:%d [1] s:%d, d:%d\n",
		(con0 & (0x7 << 2)) >> 2, (con0 & (0xff << 5)) >> 5,
		(con1 & (0x7 << 2)) >> 2, (con1 & (0xff << 5)) >> 5);

	writel(0<<2, base + CLKGEN_REG_ENB);
	writel(con0, base + CLKGEN_REG_CON0);
	writel(con1, base + CLKGEN_REG_CON1);
	writel(1<<2, base + CLKGEN_REG_ENB);

	dev_dbg(sv->snd->dev,
		"I2S.%d clk[0x%08x] con0:0x%08x, con1:0x%08x\n",
		sv->channel, sv->snd->clkgen_bases[sv->channel].p,
		con0, con1);
}

static void __i2s_unprepare(struct svoice_dev *sv)
{
	void __iomem *base = sv->base.v;

	if (sv->snd->exist_play && (sv->channel == 0))
		return;

	/* disable clkgen */
	base = sv->snd->clkgen_bases[sv->channel].v;
	writel(0<<2, base + CLKGEN_REG_ENB);
}

static void __i2s_start(struct svoice_dev *sv)
{
	void __iomem *base = sv->base.v;
	u32 con, csr;

	if (sv->snd->exist_play && (sv->channel == 0))
		return;

	nxp_soc_peri_reset_set(RESET_ID_I2S0 + sv->channel);

	con = (readl(base + 0x00) | (1 << 1) | (1 << 0));
	csr = (readl(base + 0x04) & ~(3 << 8)) | (1 << 8) | 0;
	if (!sv->mclk_ext)
		csr |= (1 << 10);
	else
		csr |= (3 << 10) | (1 << 12);
	if (!sv->is_ref)
		csr |= (1 << 5);

	writel(csr, (base + 0x04));
	writel(con, (base + 0x00));

	dev_dbg(sv->snd->dev,
		"I2S.%d 0x%08x, %p, con:0x%08x, csr:0x%08x\n",
		sv->channel, sv->base.p, sv->base.v, con, csr);
}

static inline void __i2s_stop(struct svoice_dev *sv)
{
}

static void __i2s_clear(struct svoice_dev *sv)
{
	void __iomem *base = sv->base.v;
	u32 val;

	if (sv->snd->exist_play && (sv->channel == 0))
		return;

	writel(1<<7, (base + 0x08));	/* Clear the Rx Flush bit */
	writel(0, (base + 0x08));	/* Clear the Flush bit */

	val = readl(base + 0x08);
	if (val & (0x1f))
		dev_info(sv->snd->dev,
			"I2S: i2s.%p rx not empty (0x%08x)\n",
			base, val);
}

static struct svoice_dev_ops i2s_ops = {
	.prepare = __i2s_prepare,
	.unprepare = __i2s_unprepare,
	.start = __i2s_start,
	.clear = __i2s_clear,
};

static inline bool __i2s_frame_wait(int wait)
{
	struct svoice_pin *pin = &svoice_pins[SVI_PIN_I2S_LRCK];
	int count, val;

	if (!gpio_is_valid(pin->nr))
		return false;

	/* wait Low */
	count = wait;
	do {
		val = readl(pin->base + 0x18) & (1 << pin->offset);
	} while (val && --count > 0);

	/* no Low -> no lrck */
	if (count == 0)
		return false;

	/* wait High */
	count = wait;
	do {
		val = readl(pin->base + 0x18) & (1 << pin->offset);
	} while (!val && --count > 0);

	/* no High : no lrck */
	if (count == 0)
		return false;

	/* wait Low */
	count = wait;
	do {
		val = readl(pin->base + 0x18) & (1 << pin->offset);
	} while (val && --count > 0);

	pr_debug("%s [gpio_%c.%02d] %s\n", __func__,
		('A' + pin->group), pin->offset, pin->property);

	return true;
}

static inline bool __pin_start(struct svoice_snd *snd)
{
	struct svoice_pin *pin;
	bool lrck;
	u32 val;

	__spi_cs_set();

	lrck = __i2s_frame_wait(DETECT_LRCK_WAIT);

	/* pin: ISRUN */
	pin = &svoice_pins[SVI_PIN_PDM_ISRUN];
	val = readl(pin->base) | (1 << pin->offset);
	writel(val, pin->base);

	pr_debug("%s [gpio_%c.%02d] %s\n", __func__,
		('A' + pin->group), pin->offset, pin->property);

	/* pin: LRCK */
	pin = &svoice_pins[SVI_PIN_PDM_LCRCK];
	val = readl(pin->base);
	if (lrck)
		val &= ~(1 << pin->offset);
	else
		val |= (1 << pin->offset);	/* no lrck */

	writel(val, pin->base);

	pr_debug("%s [gpio_%c.%02d] %s\n", __func__,
		('A' + pin->group), pin->offset, pin->property);

	return lrck;
}

static inline void __pin_stop(void)
{
	struct svoice_pin *pin;
	u32 val;

	__spi_cs_free();

	/* pin: LRCK */
	pin = &svoice_pins[SVI_PIN_PDM_LCRCK];
	val = readl(pin->base) | (1 << pin->offset); /* no lrck */
	writel(val, pin->base);

	/* pin: ISRUN */
	pin = &svoice_pins[SVI_PIN_PDM_ISRUN];
	val = readl(pin->base) & ~(1 << pin->offset);
	writel(val, pin->base);

	pr_debug("%s [gpio_%c.%02d] %s\n", __func__,
		('A' + pin->group), pin->offset, pin->property);
}

static void __pin_nolrck(void *data)
{
	struct svoice_pin *pin;
	u32 val;

	/* pin: LRCK */
	pin = &svoice_pins[SVI_PIN_PDM_LCRCK];
	val = readl(pin->base) | (1 << pin->offset); /* no lrck */
	writel(val, pin->base);

	pr_debug("%s [gpio_%c.%02d] %s\n", __func__,
		('A' + pin->group), pin->offset, pin->property);
}

static void __pin_prepare(void)
{
	struct svoice_pin *pin = &svoice_pins[SVI_PIN_PDM_NRST];
	u32 val;

	/*
	 * set nreset after pll insert
	 * to run with external pll
	 */
	if (gpio_is_valid(pin->nr)) {
		pr_debug("%s [gpio_%c.%02d] %s\n", __func__,
			('A' + pin->group), pin->offset, pin->property);

		val = readl(pin->base) & ~(1 << pin->offset);
		writel(val, pin->base);

		msleep(100);

		val = readl(pin->base) | (1 << pin->offset);
		writel(val, pin->base);
	}

	__pin_stop();
}


static inline bool __ref_valid(struct svoice_snd *snd, int wait)
{
	int gp = IO_GROUP(snd->ref_gpio.io);
	int bit = IO_OFFSET(snd->ref_gpio.io);
	void __iomem *base = snd->io_bases[gp].v;
	int val;

	val = readl(base + 0x18) & (1 << bit);

	do {
		if (val != (readl(base + 0x18) & (1 << bit)))
			break;
	} while (--wait > 0);

	if (wait == 0)
		return false;

	return true;
}

/*
 * I2S mode      : L - H
 * Left jusified : H - L  (MSB)
 * Right jusified: H - L  (LSB)
 */
static inline bool __ref_sync(struct svoice_snd *snd, int wait)
{
	int gp = IO_GROUP(snd->ref_gpio.io);
	int bit = IO_OFFSET(snd->ref_gpio.io);
	void __iomem *base = snd->io_bases[gp].v;
	int count, val;
	int mode = snd->ref_mode;

	/* jusified */
	if (mode != 0) {
		/* wait High */
		count = wait;
		do {
			val = readl(base + 0x18) & (1 << bit);
		} while (!val && --count > 0);

		/* wait Low */
		count = wait;
		do {
			val = readl(base + 0x18) & (1 << bit);
		} while (val && --count > 0);
	/* I2S  */
	} else {
		/* wait Low */
		count = wait;
		do {
			val = readl(base + 0x18) & (1 << bit);
		} while (val && --count > 0);

		/* wait High */
		count = wait;
		do {
			val = readl(base + 0x18) & (1 << bit);
		} while (!val && --count > 0);
	}

	return true;
}

static void __ref_cb(void *data)
{
	struct snd_soc_pcm_runtime *rtd = (struct snd_soc_pcm_runtime *)data;
	struct svoice_snd *snd =
		snd_soc_codec_get_drvdata(rtd->codec_dai->codec);
	struct svoice_dev *sv;
	unsigned long flags;

	spin_lock_irqsave(&snd->lock, flags);
	list_for_each_entry(sv, &snd->list, list) {
		if (sv->ops->prepare)
			sv->ops->prepare(sv, false);
	}
	spin_unlock_irqrestore(&snd->lock, flags);
}

static int svoice_start(struct svoice_snd *snd)
{
	struct svoice_dev *sv;
	struct svoice_pin *pin = &svoice_pins[SVI_PIN_PDM_LCRCK];
	unsigned long flags;
	bool refin = false;
	bool lrck;
	u32 val;

	dev_dbg(snd->dev, "run smart-voice ...\n");

	spin_lock_irqsave(&snd->lock, flags);

	if (!snd->exist_play)
		refin = __ref_valid(snd, 2000);

	spin_unlock_irqrestore(&snd->lock, flags);

	dev_dbg(snd->dev, "ref (%s)\n", refin ? "O" : "X");

	spin_lock_irqsave(&snd->lock, flags);

	/* prepare */
	list_for_each_entry(sv, &snd->list, list) {
		if (sv->ops->prepare)
			sv->ops->prepare(sv, refin);
	}

	/* clear mask */
	list_for_each_entry(sv, &snd->list, list) {
		if (sv->ops->clear)
			sv->ops->clear(sv);
	}

	if (refin)
		__ref_sync(snd, 2000);

	/* start */
	list_for_each_entry(sv, &snd->list, list) {
		if (sv->ops->start)
			sv->ops->start(sv);
	}

	spin_unlock_irqrestore(&snd->lock, flags);

	if (snd->mic_type == SVI_DEV_SPI) {
		/* last set 'is run' gpio */
		lrck = __pin_start(snd);

		/* read LRCK status */
		val = readl(pin->base + 0x18) & (1 << pin->offset);

		pr_info("Smart Voice LRCK: %s (%s)\n",
			lrck ? "exist" : "no exist", val ? "H" : "L");
	}

	if (snd->rate_detector) {
		/* wait multiple time */
		long duration = snd->rate_detector->duration_us * 2;

		hrtimer_start(&snd->rate_detector->timer,
			us_to_ktime(duration), HRTIMER_MODE_REL_PINNED);
	}

	return 0;
}

static void svoice_stop(struct svoice_dev *sv)
{
	if (sv->ops->unprepare)
		sv->ops->unprepare(sv);

	if (sv->ops->stop)
		sv->ops->stop(sv);

	sv->start = false;

	if (sv->type != SVI_DEV_SPI)
		return;

	__pin_stop();
}

/* cpu dai trigger */
static int svoice_trigger_hook(struct snd_pcm_substream *substream,
				int cmd, struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct svoice_snd *snd = snd_soc_codec_get_drvdata(codec_dai->codec);
	struct svoice_dev *sv;
	int nr = 0, ret;

	dev_dbg(rtd->dev, "%s <-> %s trigger >> %d\n", dai->name, codec_dai->name, cmd);

	if (!strcmp("nxp-i2s.0", dai->name)) {
		nxp_i2s_trigger(substream, cmd, dai);
		return 0;
	}

	if (cmd == SNDRV_PCM_TRIGGER_STOP)
		return 0;

	spin_lock(&snd->lock);
	find_list_dev(sv, snd, rtd->cpu_dai);
	if (!sv) {
		spin_unlock(&snd->lock);
		return -1;
	}

	sv->start = true;

	dev_dbg(rtd->dev, "smart voice hook trigger\n");
	dev_dbg(rtd->dev, "%s <-> %s trigger\n", dai->name, codec_dai->name);

	list_for_each_entry(sv, &snd->list, list) {
		dev_dbg(rtd->dev, "%s.%p, %s %d - %dEA\n",
			sv->type == SVI_DEV_I2S ? "i2s" : "spi",
			sv->base.v, sv->start ? "run":"stopped",
			nr + 1, snd->num_lists);
		if (!sv->start)
			break;
		nr++;
	}
	spin_unlock(&snd->lock);

	if (snd->exist_play)
		nr++;
	if (nr >= snd->num_lists) {
		ret = svoice_start(snd);
	}
	return ret;
}

/*
 * start : codec trigger -> cpu trigger
 * start : codec trigger -> cpu trigger
 */
static void hook_cpu_ops(struct snd_soc_pcm_runtime *runtime,
			struct snd_soc_dai *dai, bool hooking)
{
	struct snd_soc_dai *cpu_dai = runtime->cpu_dai;
	struct svoice_snd *snd = snd_soc_codec_get_drvdata(dai->codec);
	struct svoice_dev *sv;

	spin_lock(&snd->lock);
	find_list_dev(sv, snd, cpu_dai);
	if (!sv) {
		spin_unlock(&snd->lock);
		return;
	}

	spin_unlock(&snd->lock);

	dev_dbg(dai->dev, "%s %s %p, type:%s\n",
		cpu_dai->name, hooking ? "hook" : "unhook",
		sv->base.v, sv->type == SVI_DEV_I2S ? "i2s" : "spi");

	spin_lock(&snd->lock);

	/* hooking to start synchronization */
	if (hooking) {
		sv->save_ops = cpu_dai->driver->ops;
		memcpy(&sv->hook_ops,
			cpu_dai->driver->ops,
			sizeof(struct snd_soc_dai_ops));
		sv->hook_ops.trigger = svoice_trigger_hook;
		cpu_dai->driver->ops = &sv->hook_ops;
	} else {
		cpu_dai->driver->ops = sv->save_ops;
		svoice_stop(sv);
	}

	spin_unlock(&snd->lock);
}

static int svoice_trigger(struct snd_pcm_substream *substream, int cmd,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *runtime = substream->private_data;

	dev_dbg(dai->dev, "cmd=%d\n", cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	case SNDRV_PCM_TRIGGER_START:
		hook_cpu_ops(runtime, dai, true);
		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_STOP:
		hook_cpu_ops(runtime, dai, false);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int svoice_startup(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct nxp_pcm_runtime_data *prtd = substream->runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct svoice_snd *snd = snd_soc_codec_get_drvdata(dai->codec);
	struct svoice_dev *sv = NULL;

	dev_dbg(dai->dev, "startup for %s\n", rtd->cpu_dai->name);

	list_for_each_entry(sv, &snd->list, list) {
		char *name;
		name = kzalloc(strlen("nxp-pdm-i2s") + 4, GFP_KERNEL);
		sprintf(name, "%s.%d", "nxp-i2s", sv->channel);

		if (!strcmp(name, rtd->cpu_dai->name))
			break;
	}

	if (WARN_ON(!sv))
		return -ENODEV;

	if (!snd->detect && sv->is_ref) {
		snd->detect = true;
		prtd->run_detector = true;
	}

	sv->cpu_dai = rtd->cpu_dai;

	dev_dbg(dai->dev, "%s: 0x%x -> %p, type:%s\n",
		rtd->cpu_dai->name, sv->base.p, sv->base.v,
		sv->type == SVI_DEV_I2S ? "i2s" : "spi");

	return 0;
}

static void svoice_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct svoice_snd *snd = snd_soc_codec_get_drvdata(dai->codec);
	struct svoice_dev *sv = NULL;

	spin_lock(&snd->lock);

	find_list_dev(sv, snd, rtd->cpu_dai);
	if (!sv) {
		spin_unlock(&snd->lock);
		return;
	}

	spin_unlock(&snd->lock);

	if (snd->detect) {
		snd->detect = false;
		snd->rate_detector = NULL;
	}

	dev_dbg(dai->dev, "%s: %p type:%s\n",
		rtd->cpu_dai->name, sv->base.v,
		sv->type == SVI_DEV_I2S ? "i2s" : "spi");

}

static int svoice_prepare(struct snd_pcm_substream *substream,
			struct snd_soc_dai *dai)
{
	struct nxp_pcm_runtime_data *prtd = substream->runtime->private_data;
	struct svoice_snd *snd = snd_soc_codec_get_drvdata(dai->codec);

	if (prtd->rate_detector) {
		if (snd->mic_type == SVI_DEV_SPI)
			prtd->rate_detector->cb = __pin_nolrck;
		else
			prtd->rate_detector->cb = __ref_cb;
		snd->rate_detector = prtd->rate_detector;
	}

	return 0;
}

/* startup -> prepare -> trigger */
static const struct snd_soc_dai_ops snd_svoice_ops = {
	.startup = svoice_startup,
	.prepare = svoice_prepare,
	.trigger = svoice_trigger,
	.shutdown = svoice_shutdown,
};

/* not support play */
static struct snd_soc_dai_driver snd_svoice_dai = {
	.name = "nxp-smartvoice",
	.capture = {
		.stream_name = "Smart Voice Capture",
		.channels_min = 1,
		.channels_max = 4,
		.rates = SNDRV_PCM_RATE_8000_192000,
		.formats = SNDRV_PCM_FMTBIT_U8 | SNDRV_PCM_FMTBIT_S8 |
			SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_U16_LE |
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_U24_LE,
	},
	.ops = &snd_svoice_ops,
};

static int svoice_parse_dev(struct platform_device *pdev,
			    struct svoice_snd *snd,
			    struct svoice_dev *sv)
{
	struct device *dev = snd->dev;
	enum svoice_dev_type type = SVI_DEV_NONE;
	struct svoice_dev_ops *ops = NULL;
	int ch = 0, ret;

	if (sv->type == SVI_DEV_SPI) {
		ops = &spi_ops;
		sv->base.p = spi_base[ch];
	} else {
		ops = &i2s_ops;
		sv->base.p = i2s_base[ch];
	}

	sv->base.v = ioremap(sv->base.p, PAGE_SIZE);
	if (!sv->base.v) {
		dev_err(dev, "failed to ioremap 0x%x(%lx)\n",
			sv->base.p, PAGE_SIZE);
		ret = -ENOMEM;
		return -EINVAL;
	}

	sv->ops = ops;
	sv->snd = snd;

	dev_dbg(dev, "\t i2s.%d: 0x%x -> %p, type:%s\n",
		sv->channel, sv->base.p, sv->base.v,
		sv->type == SVI_DEV_I2S ? "i2s" : "spi");

	return 0;
}

static int svoice_parse_ref(struct platform_device *pdev,
			    struct svoice_snd *snd)
{
	struct nxp_svoice_plat_data *plat = pdev->dev.platform_data;
	struct device *dev = snd->dev;
	struct svoice_dev *sv;
	int ret;

	if (!plat->ref_info.type)
		return -EINVAL;

	sv = kzalloc(sizeof(*sv), GFP_KERNEL);
	if (!sv)
		return -ENOMEM;

	dev_dbg(dev, "REF: \n");

	sv->type = plat->ref_info.type;
	sv->channel = plat->ref_info.ch;

	ret = svoice_parse_dev(pdev, snd, sv);
	if (ret)
		return ret;

	sv->is_ref = true;

	sv->mclk_ext = plat->ref_mclk_ext;
	sv->clk.s[0] = 4;
	sv->clk.s[1] = 7;
	sv->clk.d[0] = 0;
	sv->clk.d[1] = 0;

	if (plat->ref_switch_clkgen[0] >= 0) {
		/* Divider value = CLKDIV0 + 1 */
		sv->clk.s[0] = plat->ref_switch_clkgen[0];
		sv->clk.d[0] = plat->ref_switch_clkgen[1] - 1;
		sv->clk.s[1] = plat->ref_switch_clkgen[2];
		sv->clk.d[1] = plat->ref_switch_clkgen[3] - 1;

		if (WARN_ON(sv->clk.d[0] < 0))
			sv->clk.d[0] = 0;

		if (WARN_ON(sv->clk.d[1] < 0))
			sv->clk.d[1] = 0;

		dev_dbg(dev, "\t clock s0:%d, d0:%d, s1:%d, d1:%d\n",
			sv->clk.s[0], sv->clk.d[0],
			sv->clk.s[1], sv->clk.d[1]);
	}

	list_add(&sv->list, &snd->list);

	snd->num_lists++;
	snd->ref = sv;
	snd->ref_mode = (plat->ref_mode >= 0)? plat->ref_mode : 0;
	snd->ref_gpio.io =
		plat->i2s_lrck_gpio? plat->i2s_lrck_gpio : gpio_i2s[plat->ref_info.ch].lr.io;

	return 0;
}

static int svoice_parse_mic(struct platform_device *pdev,
			    struct svoice_snd *snd)
{
	struct nxp_svoice_plat_data *plat = pdev->dev.platform_data;
	struct device *dev = snd->dev;
	struct svoice_pin *pin = &svoice_pins[0];
	struct svoice_dev *sv;
	int num = 0, ret;
	int val, i;

	for (num = 0; num < 2; num++) {

		if (!plat->mic_info[num].type)
			continue;

		sv = kzalloc(sizeof(*sv), GFP_KERNEL);
		if (!sv)
			return -ENOMEM;

		dev_dbg(dev, "MIC: %d\n", num);

		sv->type = plat->mic_info[num].type;
		sv->channel = plat->mic_info[num].ch;

		ret = svoice_parse_dev(pdev, snd, sv);
		if (ret)
			break;

		if (sv->type == SVI_DEV_SPI) {
			for (i = 0; i < SVI_PIN_NUM; i++, pin++) {

				if (!strcmp(pin->property, "spi-cs-gpio"))
					val = plat->spi_cs_gpio;
				else if (!strcmp(pin->property, "pdm-isrun-gpio"))
					val = plat->pdm_isrun_gpio;
				else if (!strcmp(pin->property, "pdm-lrck-gpio"))
					val = plat->pdm_lrck_gpio;
				else if (!strcmp(pin->property, "pdm-nrst-gpio"))
					val = plat->pdm_nrst_gpio;
				else if (!strcmp(pin->property, "i2s-lrck-gpio"))
					val = plat->i2s_lrck_gpio;
				else
					continue;

				pin->nr = val;
				pin->group = IO_GROUP(pin->nr);
				pin->offset = IO_OFFSET(pin->nr);
				pin->base = snd->io_bases[pin->group].v;

				if (gpio_request(val, pin->property)) {
					dev_err(dev, "can't request gpio_%c.%02d\n",
						('A' + pin->group), pin->offset);
					return -EINVAL;
				}

				if (pin->output && gpio_direction_output(val, 1)) {
					dev_err(dev, "can't set gpio_%c.%02d output\n",
						('A' + pin->group), pin->offset);
					return -EINVAL;
				}

				dev_info(dev, "[gpio_%c.%02d] %s, %s\n",
					('A' + pin->group), pin->offset, pin->property,
					pin->output ? "out" : "status");
			}

			/* pdm prepare */
			__pin_prepare();

		} else {
			sv->mclk_ext = plat->mic_mclk_ext;

			sv->clk.s[0] = 4;
			sv->clk.s[1] = 7;
			sv->clk.d[0] = 0;
			sv->clk.d[1] = 0;

			if (plat->mic_switch_clkgen[0] >= 0) {
				/* Divider value = CLKDIV0 + 1 */
				sv->clk.s[0] = plat->mic_switch_clkgen[0];
				sv->clk.d[0] = plat->mic_switch_clkgen[1] - 1;
				sv->clk.s[1] = plat->mic_switch_clkgen[2];
				sv->clk.d[1] = plat->mic_switch_clkgen[3] - 1;

				if (WARN_ON(sv->clk.d[0] < 0))
					sv->clk.d[0] = 0;

				if (WARN_ON(sv->clk.d[1] < 0))
					sv->clk.d[1] = 0;

				dev_dbg(dev, "\t clock s0:%d, d0:%d, s1:%d, d1:%d\n",
					sv->clk.s[0], sv->clk.d[0],
					sv->clk.s[1], sv->clk.d[1]);
			}
		}

		snd->mic_type = sv->type;
		list_add(&sv->list, &snd->list);
	}


	snd->num_lists += num;
	dev_dbg(dev, "'mic' channels: %d\n", num);

	return 0;
}

static int svoice_parse_setup(struct platform_device *pdev,
			    struct svoice_snd *snd)
{
	struct nxp_svoice_plat_data *plat = pdev->dev.platform_data;
	struct device *dev = snd->dev;
	int i;

	for (i = 0; i < ARRAY_SIZE(gpio_base); i++) {
		struct io_base *b = &snd->io_bases[i];
		b->p = gpio_base[i];

		b->v = ioremap(b->p, PAGE_SIZE);
		if (!b->v) {
			dev_err(dev, "can't map gpio_%c (0x%x,0x%lx)\n",
				('A' + i), b->p, PAGE_SIZE);
			return -EINVAL;
		}
	}

	for (i = 0; i < ARRAY_SIZE(clkgen_i2s_base); i++) {
		struct io_base *b = &snd->clkgen_bases[i];
		b->p = clkgen_i2s_base[i];;

		b->v = ioremap(b->p, PAGE_SIZE);
		if (!b->v) {
			dev_err(dev,
				"can't map i2s.%d clock gen (0x%x,0x%lx)\n",
				i, b->p, PAGE_SIZE);
			return -EINVAL;
		}
	}

	/* for PDM PLL */
#ifdef CONFIG_HAVE_PWM
	if (plat->pwm_id >= 0) {
		snd->pwm = pwm_request(plat->pwm_id, plat->pwm_label);
		if (snd->pwm) {
			unsigned int period_ns = 1000000000 / SVI_PDM_PLL;
			unsigned int duty_ns = period_ns / 2;
			int ret;

			ret = pwm_config(snd->pwm, duty_ns, period_ns);
			if (!ret) {
				ret = pwm_enable(snd->pwm);
				if (ret) {
					dev_err(dev,
						"can't enable PWM.%d\n",
						plat->pwm_id);
					return ret;
				}
				dev_info(dev,
					"Smart Voice - pwm.%d %d (%d)\n",
					plat->pwm_id, period_ns, duty_ns);
			} else {
				pwm_free(snd->pwm);
				snd->pwm = NULL;
			}
		}
	}
#endif
	return 0;
}

static int svoice_setup(struct platform_device *pdev)
{
	struct svoice_snd *snd;
	struct svoice_dev *pos, *next;
	struct file *filp;

	snd = kzalloc(sizeof(*snd), GFP_KERNEL);
	if (!snd)
		return -ENOMEM;

	snd->dev = &pdev->dev;

	INIT_LIST_HEAD(&snd->list);
	spin_lock_init(&snd->lock);

	if (svoice_parse_setup(pdev, snd))
		goto err_setup;

	if (svoice_parse_ref(pdev, snd))
		goto err_setup;

	if (svoice_parse_mic(pdev, snd))
		goto err_setup;

	dev_set_drvdata(&pdev->dev, snd);

	filp = filp_open("/dev/snd/pcmC0D0p", O_RDONLY, 0664);
	if (filp) {
		snd->exist_play = true;
		dev_info(&pdev->dev, "exist playback device\n");
	}

	return 0;

err_setup:
	list_for_each_entry_safe(pos, next, &snd->list, list) {
		list_del(&pos->list);
		kfree(pos);
	}

	kfree(snd);

	return -EINVAL;
}

static struct snd_soc_codec_driver snd_svoice_codec;

static int svoice_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret;

	ret = svoice_setup(pdev);
	if (ret < 0)
		return ret;

	ret = snd_soc_register_codec(&pdev->dev,
			&snd_svoice_codec, &snd_svoice_dai, 1);

	dev_info(dev, "%s register\n", ret ? "Failed" : "Success");

	return ret;
}

static int svoice_remove(struct platform_device *pdev)
{
	struct svoice_snd *snd = dev_get_drvdata(&pdev->dev);
	struct svoice_dev *pos, *next;
	int i;

	if (!snd)
		return 0;

	snd_soc_unregister_codec(&pdev->dev);

	list_for_each_entry_safe(pos, next, &snd->list, list) {
		list_del(&pos->list);
		kfree(pos);
	}

	for (i = 0; i < ARRAY_SIZE(snd->io_bases); i++) {
		if (snd->io_bases[i].v) {
			iounmap(snd->io_bases[i].v);
			snd->io_bases[i].v = NULL;
		}
	}

	for (i = 0; i < ARRAY_SIZE(snd->clkgen_bases); i++) {
		if (snd->clkgen_bases[i].v) {
			iounmap(snd->clkgen_bases[i].v);
			snd->clkgen_bases[i].v = NULL;
		}
	}

	kfree(snd);

	return 0;
}

static struct platform_driver svoice_drv_codec = {
	.probe		= svoice_probe,
	.remove		= svoice_remove,
	.driver		= {
		.name	= "nxp-smartvoice-codec",
		.owner	= THIS_MODULE,
	},
};
module_platform_driver(svoice_drv_codec);

MODULE_AUTHOR("jhkim <jhkim@nexell.co.kr>");
MODULE_DESCRIPTION("Sound smartvoice PDM/I2S codec driver");
MODULE_LICENSE("GPL");

