/*
 * (C) Copyright 2015
 * hyun seok jung, Nexell Co, <hsjung@nexell.co.kr>
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

#include <linux/module.h>
#include <linux/platform_device.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>
#include <mach/platform.h>
#include "nxp-pdm-spi.h"
#include "../nxp-pcm.h"


#define pr_debug(msg...)		printk(KERN_INFO msg)

static char str_dai_name[] = "nxp-pdm-spi";
/*
 * SPI Record
 */
#define STUB_RATES		SND_SOC_SPI_RATES
#define STUB_FORMATS	SND_SOC_SPI_FORMATS

static struct snd_soc_codec_driver soc_codec_spi_rec;

static struct snd_soc_dai_driver pdm_spi = {
	.name		= "dit-mulch-recorder",
	.capture 	= {
		.stream_name	= "Capture",
		.channels_min	= 1,
		.channels_max	= 4,
		.rates			= STUB_RATES,
		.formats		= STUB_FORMATS,
	},
};

static int pdm_spi_probe(struct platform_device *pdev)
{
	printk("[%s:%d]\n", __func__, __LINE__);
	return snd_soc_register_codec(&pdev->dev, &soc_codec_spi_rec, &pdm_spi, 1);
}

static int pdm_spi_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

/*
 * SPI Record
 */
static struct platform_driver spi_rec = {
	.probe		= pdm_spi_probe,
	.remove		= pdm_spi_remove,
	.driver		= {
		.name	= "pdm-spi-dit-recorder",
		.owner	= THIS_MODULE,
	},
};

module_platform_driver(spi_rec);

MODULE_AUTHOR("hsjung <hsjung@nexell.co.kr>");
MODULE_DESCRIPTION("SPI recorder driver");
MODULE_LICENSE("GPL");

/*
 * SPI CARD DAI
 */
static struct snd_soc_dai_link pdm_spi_dai_link = {
	.name 			= "PDM Rec",
	.stream_name 	= "PDM PCM Capture",
	.cpu_dai_name 	= str_dai_name,			/* pdm_driver name */
	.platform_name  = DEV_NAME_PCM,				/* nxp_snd_pcm_driver name */
	.codec_dai_name = "dit-mulch-recorder",
	.codec_name 	= "pdm-spi-dit-recorder",
	.symmetric_rates = 1,
};

static struct snd_soc_card pdm_spi_card_card[] = {
	{
	.name 			= "PDM-SPI-Recorder.0",		/* proc/asound/cards */
	.dai_link 		= &pdm_spi_dai_link,
	.num_links 		= 1,
	},
	{
	.name 			= "PDM-SPI-Recorder.1",		/* proc/asound/cards */
	.dai_link 		= &pdm_spi_dai_link,
	.num_links 		= 1,
	},
	{
	.name 			= "PDM-SPI-Recorder.2",		/* proc/asound/cards */
	.dai_link 		= &pdm_spi_dai_link,
	.num_links 		= 1,
	},
};

static int pdm_spi_card_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &pdm_spi_card_card[0];
	struct snd_soc_dai *codec_dai = NULL;
	struct snd_soc_dai_driver *driver = NULL;
	int ret;

	if(pdev->id != -1)  {
		sprintf(str_dai_name, "%s.%d", "nxp-pdm-spi", pdev->id);
		card = &pdm_spi_card_card[pdev->id];
	}

	/* register card */
	card->dev = &pdev->dev;
	ret = snd_soc_register_card(card);

	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card() failed: %d\n", ret);
		return ret;
	}

	if (card->rtd) {
		codec_dai = card->rtd->codec_dai;
		if (codec_dai)
			driver = codec_dai->driver;
	}

	pr_debug("pdm-rec-dai: register card %s -> %s\n",
		card->dai_link->codec_dai_name, card->dai_link->cpu_dai_name);

	return ret;
}

static int pdm_spi_card_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	snd_soc_unregister_card(card);
	return 0;
}

static struct platform_driver pdm_spi_card_driver = {
	.driver		= {
		.name	= "pdm-spi-recorder",
		.owner	= THIS_MODULE,
		.pm 	= &snd_soc_pm_ops,	/* for suspend */
	},
	.probe		= pdm_spi_card_probe,
	.remove		= pdm_spi_card_remove,
};
module_platform_driver(pdm_spi_card_driver);

MODULE_AUTHOR("hsjung <hsjung@nexell.co.kr>");
MODULE_DESCRIPTION("Sound SPI recorder driver for the SLSI");
MODULE_LICENSE("GPL");

