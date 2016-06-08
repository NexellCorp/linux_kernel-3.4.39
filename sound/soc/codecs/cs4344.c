/*
 * sound/soc/codecs/cs4344.c
 * 
 * CS4344  --  SoC Audio DAC driver
 * 
 * Author:     Chris Lee, <kjlee@nexell.co.kr>
 * Created:    June 7th, 2016
 * Copyright:  (C) Copyright 2016 Nexell Ltd.
 *
 * 2016-06-07  ported from Nexell FAE Platform Code (cs4344.c)
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <sound/soc.h>
#include <sound/initval.h>

#define CODEC_NAME "cs4344-dac"

#define CS4344_PLAYBACK_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |\
        SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 | SNDRV_PCM_RATE_44100 | \
        SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 | SNDRV_PCM_RATE_96000)

#define CS4344_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |\
        SNDRV_PCM_FMTBIT_S24_LE)

static struct snd_soc_codec_driver soc_codec_dev_cs4344;

struct snd_soc_dai_driver cs4344_dai[] = {
	{
		.name = "cs4344-hifi",
		.playback = {
			.stream_name = "Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = CS4344_PLAYBACK_RATES,
			.formats = CS4344_FORMATS,
		},
	},
};

static int cs4344_probe(struct platform_device *pdev)
{
	int ret;

	ret = snd_soc_register_codec(&pdev->dev, &soc_codec_dev_cs4344, cs4344_dai, ARRAY_SIZE(cs4344_dai));

	if (ret != 0)
		dev_err(&pdev->dev, "Failed to register codec: %d\n", ret);
	return ret;
}

static int cs4344_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

static struct platform_driver cs4344_device = {
	.probe 	= cs4344_probe,
	.remove = cs4344_remove, 
	.driver = {
		.name  = CODEC_NAME,
		.owner = THIS_MODULE,
	}
};

static int __init cs4344_mod_init(void)
{
	return platform_driver_register(&cs4344_device);
}
module_init(cs4344_mod_init);

static void __exit cs4344_exit(void)
{
	platform_driver_unregister(&cs4344_device);
}
module_exit(cs4344_exit);

MODULE_AUTHOR("Chris Lee <kjlee@nexell.co.kr>");
MODULE_DESCRIPTION("ASoC CS4344 Audio Codec driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" CODEC_NAME);
