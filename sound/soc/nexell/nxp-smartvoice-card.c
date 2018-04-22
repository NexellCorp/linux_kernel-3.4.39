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
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>
#include <mach/platform.h>

#include "nxp-pcm.h"

static struct snd_soc_dai_link spi_svoice_dai_link = {
	.name = "SPI SmartVoice",
	.stream_name = "SPI SmartVoice Capture",
	.platform_name = DEV_NAME_PCM,
	.codec_dai_name = "nxp-smartvoice",
	.codec_name = "nxp-smartvoice-codec",
	/*
	 * .symmetric_rates = 1,
	 */
};

static struct snd_soc_dai_link i2s_svoice_dai_link = {
	.name = "I2S SmartVoice",
	.stream_name = "I2S SmartVoice Capture",
#ifndef CONFIG_SND_NXP_PCM_RATE_DETECTOR
	.platform_name = DEV_NAME_PCM,
#else
	.platform_name = "nxp-pcm-rate-detector",
#endif
	.codec_dai_name = "nxp-smartvoice",
	.codec_name = "nxp-smartvoice-codec",
	/*
	 * .symmetric_rates = 1,
	 */
};

static struct snd_soc_card svoice_card_card[] = {
	{
	.name = "SmartVoice.0",
	.num_links = 1,
	},
	{
	.name = "SmartVoice.1",
	.num_links = 1,
	},
	{
	.name = "SmartVoice.2",
	.num_links = 1,
	},
	{
	.name = "SmartVoice.3",
	.num_links = 1,
	},
};

static int svoice_card_probe(struct platform_device *pdev)
{
	struct nxp_snd_svoice_dai_plat_data *plat = pdev->dev.platform_data;
	struct snd_soc_card *card = &svoice_card_card[0];
	char *buf;
	int ret;

	if (pdev->id != -1) {
		if (pdev->id >= ARRAY_SIZE(svoice_card_card)) {
			dev_err(&pdev->dev,
				"Error, not support card id %d\n",
				pdev->id);
			return -EINVAL;
		}
		card = &svoice_card_card[pdev->id];
	}

	if (plat->cpu_dai == SVI_DEV_I2S) {
		buf = kzalloc(strlen("nxp-pdm-i2s") + 4, GFP_KERNEL);
		if (!buf)
			return -ENOMEM;

		sprintf(buf, "%s.%d", "nxp-i2s", plat->ch);

		card->dai_link = &i2s_svoice_dai_link;
		card->dai_link->cpu_dai_name = buf;
	} else {
		buf = kzalloc(strlen("nxp-pdm-spi") + 4, GFP_KERNEL);
		if (!buf)
			return -ENOMEM;

		sprintf(buf, "%s.%d", "nxp-pdm-spi", plat->ch);

		card->dai_link = &spi_svoice_dai_link;
		card->dai_link->cpu_dai_name = buf;
	}

	/* register card */
	card->dev = &pdev->dev;
	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev,
			"snd_soc_register_card() failed: %d\n", ret);
		return ret;
	}

	dev_dbg(card->dev, "%s smartvoice: card %s -> %s\n",
		plat->cpu_dai == SVI_DEV_I2S ? "i2s" : "spi",
		card->dai_link->codec_dai_name, card->dai_link->cpu_dai_name);

	return ret;
}

static int svoice_card_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	const char *buf = card->dai_link->cpu_dai_name;

	snd_soc_unregister_card(card);
	kfree(buf);

	return 0;
}

static struct platform_driver svoice_card_driver = {
	.driver = {
		.name = "nxp-smartvoice-card",
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
	},
	.probe = svoice_card_probe,
	.remove = svoice_card_remove,
};
module_platform_driver(svoice_card_driver);

MODULE_AUTHOR("jhkim <jhkim@nexell.co.kr>");
MODULE_DESCRIPTION("Sound PDM/I2S or SPI/I2S Smart Voice Card driver for the SLSI");
MODULE_LICENSE("GPL");

