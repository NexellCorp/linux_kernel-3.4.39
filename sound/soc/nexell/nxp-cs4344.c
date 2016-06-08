/*
 * (C) Copyright 2016
 * Chris Lee, Nexell Co, <kjlee@nexell.co.kr>
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

#include "nxp-i2s.h"

static char str_dai_name[16] = DEV_NAME_I2S;

static struct snd_soc_dai_link cs4344_dai_link = {
    .name           = "I2S-CS4344",
    .stream_name    = "CS4344 PCM Playback",
    .cpu_dai_name   = str_dai_name,         /* nxp_snd_i2s_driver name */
    .platform_name  = DEV_NAME_PCM,         /* nxp_snd_pcm_driver name */
    .codec_dai_name = "cs4344-hifi",        /* cs4344_dai's name */
    .codec_name     = "cs4344-dac",      	/* cs4344 driver name */
    .symmetric_rates = 1,
};

static struct snd_soc_card cs4344_card = {
    .name           = "I2S-CS4344",     	/* proc/asound/cards */
    .dai_link       = &cs4344_dai_link,
    .num_links      = 1,
};

static int cs4344_probe(struct platform_device *pdev)
{
	struct nxp_snd_dai_plat_data *plat = pdev->dev.platform_data;
	struct snd_soc_card *card = &cs4344_card;
	struct snd_soc_dai *codec_dai = NULL;
	struct snd_soc_dai_driver *driver = NULL;
	unsigned int sample_rate = 0;
	int ret;

	/* Set I2S name */
	if (plat)
		sprintf(str_dai_name, "%s.%d", DEV_NAME_I2S, plat->i2s_ch);

	/* Register card */
	card->dev = &pdev->dev;
	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register card : %d\n", ret);
		return ret;
	}

	if (card->rtd) {
		codec_dai = card->rtd->codec_dai;
		if (codec_dai)
			driver = codec_dai->driver;
	}

    /* Reset sample rates and format */
    if (plat && driver) {
        if (plat->sample_rate) {
            sample_rate = snd_pcm_rate_to_rate_bit(plat->sample_rate);
            if (SNDRV_PCM_RATE_KNOT != sample_rate)
                driver->playback.rates = sample_rate;
            else
                printk("%s : Invalid sample rates = %d\n", __func__, plat->sample_rate);
        }

        if(plat->pcm_format)
            driver->playback.formats = plat->pcm_format;
	}

    printk("cs4344-audio: Register card %s -> %s\n", card->dai_link->codec_dai_name, card->dai_link->cpu_dai_name);
	return 0;
}

static int cs4344_remove(struct platform_device *pdev)
{
    struct snd_soc_card *card = platform_get_drvdata(pdev);
    snd_soc_unregister_card(card);
    return 0;
}

static struct platform_driver cs4344_audio_driver = {
    .driver     = {
        .name   = "cs4344-audio",
        .owner  = THIS_MODULE,
    },
    .probe      = cs4344_probe,
    .remove     = __devexit_p(cs4344_remove),
};
module_platform_driver(cs4344_audio_driver);

MODULE_AUTHOR("Chris Lee <kjlee@nexell.co.kr>");
MODULE_DESCRIPTION("Sound CS4344 DAC Audio driver for the SLSI");
MODULE_LICENSE("GPL");
