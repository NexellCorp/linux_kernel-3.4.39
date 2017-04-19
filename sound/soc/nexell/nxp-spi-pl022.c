/*
 * Copyright (C) 2016  Nexell Co., Ltd.
 * Author: Hyunseok, Jung <hsjung@nexell.co.kr>
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <sound/soc.h>

#include <mach/platform.h>
#include <mach/devices.h>
#include <mach/soc.h>

#include "nxp-spi-pl022.h"
#include "nxp-pcm.h"

#define SPI_DMA_RX_OFFS		(0x8)
#define	SPI_SAMPLE_RATE		16000
#define	SPI_DATA_BIT		8
#define	SPI_RESET_MAX		2

/* smart voice spi clock */
#define SPI_CLOCK_HZ		(100 * 1000 * 1000)
#define SPI_CLOCK_PHASE		1
#define SPI_CLOCK_INVERT	1

static const dma_addr_t spi_base_address[3] = {
	PHY_BASEADDR_SSP0, PHY_BASEADDR_SSP1, PHY_BASEADDR_SSP2,
};

static  const int spi_reset_ids[3][2] = {
	{ RESET_ID_SSP0_P, RESET_ID_SSP0 },
	{ RESET_ID_SSP1_P, RESET_ID_SSP1 },
	{ RESET_ID_SSP2_P, RESET_ID_SSP2 },
};

struct pl022_snd {
	struct device *dev;
	int sample_rate;
	int status;
	spinlock_t lock;
	/* DT */
	int ch;
	int master_mode;	/* 1 = master_mode, 0 = slave */
	int clk_freq;
	int clk_invert;
	int clk_phase;
	int data_bit;
	enum ssp_rx_level_trig rx_lev_trig;
	enum ssp_protocol protocol;
	int tx_out;
	/* DMA channel */
	struct nxp_pcm_dma_param dma;
	/* clock and reset */
	struct clk *clk;
	/* register base */
	resource_size_t phybase;
	void __iomem *virtbase;
};

static int pl022_start(struct pl022_snd *pl022, int stream)
{
	void __iomem *base = pl022->virtbase;

	pl022_enable(base, 1);
	return 0;
}

static void pl022_stop(struct pl022_snd *pl022, int stream)
{
	void __iomem *base = pl022->virtbase;
	u32 val;

	pl022_enable(base, 0);

	/* wait for fifo flush */
	val = __raw_readl(SSP_SR(base));
	if (val & (1 << 2)) {
		while (__raw_readl(SSP_SR(base)) & (1 << 2))
			val = __raw_readl(SSP_DR(base));
	}
}

static int  pl022_ops_startup(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct pl022_snd *pl022 = snd_soc_dai_get_drvdata(dai);
	struct nxp_pcm_dma_param *dmap = &pl022->dma;

	snd_soc_dai_set_dma_data(dai, substream, dmap);
	return 0;
}

static void pl022_ops_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
}

static int pl022_ops_trigger(struct snd_pcm_substream *substream,
				int cmd, struct snd_soc_dai *dai)
{
	struct pl022_snd *pl022 = snd_soc_dai_get_drvdata(dai);
	int stream = substream->stream;

	dev_dbg(pl022->dev, "%s cmd=%d, spi:%p\n",
		STREAM_STR(stream), cmd, pl022->virtbase);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_START:
		pl022_start(pl022, stream);
		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_STOP:
		pl022_stop(pl022, stream);
		break;

	default:
		return -EINVAL;
	}
	return 0;
}

static struct snd_soc_dai_ops pl022_dai_ops = {
	.startup	= pl022_ops_startup,
	.shutdown	= pl022_ops_shutdown,
	.trigger	= pl022_ops_trigger,
};

static int pl022_dai_suspend(struct snd_soc_dai *dai)
{
	return 0;
}

static int pl022_dai_resume(struct snd_soc_dai *dai)
{
	return 0;
}

static struct snd_soc_dai_driver pl022_dai_driver = {
	.suspend = pl022_dai_suspend,
	.resume = pl022_dai_resume,
	.ops = &pl022_dai_ops,
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 4,
		.formats = SNDRV_PCM_FMTBIT_U8 | SNDRV_PCM_FMTBIT_S8 |
				SNDRV_PCM_FMTBIT_S16_LE,
		.rates	= SNDRV_PCM_RATE_8000_192000,
		.rate_min = 16000,
		.rate_max = 16000 * 4,	/* 4channel */
	},
};

static int pl022_dma_config(struct platform_device *pdev,
			struct pl022_snd *pl022)
{
	struct nxp_pcm_dma_param *dma = &pl022->dma;
	struct nxp_pdm_spi_plat_data *plat = pdev->dev.platform_data;
	int bus_width, max_burst;

	switch (pl022->data_bit) {
	case 0:
		/* Use the same as for writing */
		bus_width = DMA_SLAVE_BUSWIDTH_UNDEFINED;
		break;
	case 8:
		bus_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
		break;
	case 16:
		bus_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
		break;
	case 32:
		bus_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		break;
	default:
		dev_err(pl022->dev, "not support pcm read %dbits\n",
			pl022->data_bit);
		return -EINVAL;
	}

	switch (pl022->rx_lev_trig) {
	case SSP_RX_1_OR_MORE_ELEM:
		max_burst = 1;
		break;
	case SSP_RX_4_OR_MORE_ELEM:
		max_burst = 4;
		break;
	case SSP_RX_8_OR_MORE_ELEM:
		max_burst = 8;
		break;
	case SSP_RX_16_OR_MORE_ELEM:
		max_burst = 16;
		break;
	case SSP_RX_32_OR_MORE_ELEM:
		max_burst = 32;
		break;
	default:
		max_burst = 4;
		break;
	}

	dma->dev = pl022->dev;
	dma->active = true;
	dma->dma_ch_name = (char *)(plat->dma_ch);
	dma->dma_filter = plat->dma_filter;
	dma->peri_addr = pl022->phybase + SPI_DMA_RX_OFFS;
	dma->bus_width_byte = bus_width;
	dma->max_burst_byte = max_burst;
	dma->real_clock = (16000 * 4);
	dma->dfs = 0;

	dev_dbg(pl022->dev, "spi-rx: %s dma (%s), 0x%p, bus %dbyte, burst %d\n",
		 STREAM_STR(1), dma->dma_ch_name,
		 (void *)dma->peri_addr, dma->bus_width_byte,
		 dma->max_burst_byte);

	return 0;
}

static int pl022_setup(struct platform_device *pdev, struct pl022_snd *pl022)
{
	void __iomem *base = pl022->virtbase;
	int i, ret;

	ret = pl022_dma_config(pdev, pl022);
	if (ret)
		return ret;

	/* must be 12 x input clock */
	clk_set_rate(pl022->clk, pl022->clk_freq);
	clk_enable(pl022->clk);

	/* reset */
	for (i = 0; i < SPI_RESET_MAX; i++) {
		NX_RSTCON_SetnRST(spi_reset_ids[pl022->ch][i], RSTCON_nDISABLE);
		NX_RSTCON_SetnRST(spi_reset_ids[pl022->ch][i], RSTCON_nENABLE);
	}

	if (pl022->protocol != SSP_PROTOCOL_SPI &&
	pl022->protocol != SSP_PROTOCOL_SSP &&
	pl022->protocol != SSP_PROTOCOL_NM) {
		dev_err(pl022->dev,
			"could not support spi protocol %d\n",
			pl022->protocol);
		return -EINVAL;
	}

	pl022_enable(base, 0);
	pl022_protocol(base, pl022->protocol); /* Motorola SPI */

	pl022_clock_polarity(base, pl022->clk_invert);
	pl022_clock_phase(base, pl022->clk_phase);

	pl022_bit_width(base, pl022->data_bit);  /* 8 bit */
	pl022_mode(base, pl022->master_mode);	/* slave mode */
	pl022_irq_enable(base, 0xf, 0);
	pl022_slave_output(base, pl022->tx_out); /* NO TX */

	pl022_dma_mode(base, 1, 1);

	return 0;
}

static int pl022_data_parse(struct platform_device *pdev,
			struct pl022_snd *pl022,
			struct snd_soc_dai_driver *dai)
{
	struct device *dev = &pdev->dev;
	struct nxp_pdm_spi_plat_data *plat = pdev->dev.platform_data;
	char name[10] = { 0, };

	pl022->dev = &pdev->dev;
	pl022->ch =  pdev->id;

	sprintf(name, "nxp-spi.%d", (unsigned char)pl022->ch);

	/* spi clock */
	pl022->clk = clk_get(NULL, name);
	if (IS_ERR(pl022->clk)) {
		dev_err(dev, "failed to get clock\n");
		return PTR_ERR(pl022->clk);
	}

	pl022->phybase = spi_base_address[pl022->ch];
	pl022->virtbase = ioremap(pl022->phybase, PAGE_SIZE);
	if (!pl022->virtbase) {
		dev_err(dev, "failed to ioremap for 0x%x ch.%d\n",
			pl022->phybase, pl022->ch);
		return -ENOMEM;
	}

	/* parse device tree source */
	pl022->clk_freq = SPI_CLOCK_HZ;
	if (plat->clk_freq > 0)
		pl022->clk_freq = plat->clk_freq;

	pl022->clk_invert = plat->clk_invert;
	pl022->clk_phase = plat->clk_phase;

	pl022->data_bit = SPI_DATA_BIT;
	if (plat->data_bits > 0)
		pl022->data_bit = plat->data_bits;

	pl022->sample_rate = SPI_SAMPLE_RATE;
	pl022->rx_lev_trig = SSP_RX_4_OR_MORE_ELEM;
	pl022->protocol = SSP_PROTOCOL_SPI;

	/* set snd_soc_dai_driver */
	dai->playback.rates = snd_pcm_rate_to_rate_bit(pl022->sample_rate);

	if (pl022->master_mode) {
		dev_err(dev, "not support master mode\n");
		return -EINVAL;
	}

	dev_info(dev, "spi.%d 0x%x %d bit (%dlv) %s %d clk %s %dkhz (%d/%d)\n",
		pl022->ch, pl022->phybase,
		pl022->data_bit, pl022->rx_lev_trig,
		pl022->protocol == SSP_PROTOCOL_SPI ? "Motorola" :
		pl022->protocol == SSP_PROTOCOL_SSP ? "TI" : "Microwire",
		pl022->sample_rate, name, pl022->clk_freq/1000,
		pl022->clk_invert, pl022->clk_phase);

	return 0;
}

static int pl022_probe(struct platform_device *pdev)
{
	struct pl022_snd *pl022;
	static struct snd_soc_dai_driver *dai = &pl022_dai_driver;
	int ret = 0;

	/*  allocate driver data */
	pl022 = kzalloc(sizeof(struct pl022_snd), GFP_KERNEL);
	if (!pl022)
		return -ENOMEM;

	ret = pl022_data_parse(pdev, pl022, dai);
	if (ret)
		goto err_out;

	ret = snd_soc_register_dai(&pdev->dev, dai);
	if (ret) {
		dev_err(&pdev->dev,
			"devm_snd_soc_register_component: %s\n",
			pdev->name);
		goto err_out;
	}

	ret = pl022_setup(pdev, pl022);
	if (ret)
		goto err_out;

	dev_set_drvdata(&pdev->dev, pl022);

	return ret;

err_out:
	kfree(pl022);
	return ret;
}

static int pl022_remove(struct platform_device *pdev)
{
	struct pl022_snd *pl022 = dev_get_drvdata(&pdev->dev);

	snd_soc_unregister_dai(&pdev->dev);

	if (pl022->virtbase)
		iounmap(pl022->virtbase);

	kfree(pl022);

	return 0;
}

static struct platform_driver spi_driver = {
	.probe  = pl022_probe,
	.remove = pl022_remove,
	.driver = {
		.name	= "nxp-pdm-spi",
		.owner	= THIS_MODULE,
	},
};
module_platform_driver(spi_driver);

MODULE_AUTHOR("jhkim <jhkim@nexell.co.kr>");
MODULE_DESCRIPTION("Sound SPI driver for Nexell sound");
MODULE_LICENSE("GPL");

