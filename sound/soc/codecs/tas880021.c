/*
 * tas880021.c  --  ALSA Soc Audio driver
 *
 * Copyright 2016 Nexell
 * JongshinPark <pjsin865@nexell.co.kr>
 *
 *
 * Based on alc5632.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tas880021.h>

#include "tas880021.h"

/*
#define pr_debug	printk
*/


struct tas880021_priv {
	struct i2c_client *i2c;
	enum snd_soc_control_type control_type;
	u8 id;
	unsigned int sysclk;
	unsigned int add_ctrl;
	unsigned int jack_det_ctrl;
};

static char tas880021_i2c_read(struct i2c_client *client, char addr)
{
	char data[2] = {0};
	data[0] = addr;
	if(i2c_master_send(client, data, 1) == 1) {
		i2c_master_recv(client, data, 1);
		return data[0];
	} else {
		printk(KERN_ERR "## [%s():%s:%d\t] error(0x%02x) \n", __FUNCTION__, strrchr(__FILE__, '/')+1, __LINE__, addr);
	}
	return 0;
}

static void tas880021_i2c_write_burst(struct i2c_client *client, char *value, u8 size)
{
	if(i2c_master_send(client, value, size) != size) {
		printk(KERN_ERR "## [%s():%s:%d\t] error \n", __FUNCTION__, strrchr(__FILE__, '/')+1, __LINE__);
	}
}

static void tas880021_i2c_write(struct i2c_client *client, char addr, u8 value)
{
	char data[3];
	data[0] = addr;
	data[1] = value;
	if(i2c_master_send(client, data, 2) != 2) {
		printk(KERN_ERR "## [%s():%s:%d\t] error(0x%02x, 0x%02x) \n", __FUNCTION__, strrchr(__FILE__, '/')+1, __LINE__, addr, value);
	}
}

static int init_codec(struct i2c_client *client)
{
	int i = 0;
	char value = 0;
	char data[32] = {0, };

#if 0
	tas880021_i2c_write(client, 0x00, 0x00);
	tas880021_i2c_write(client, 0x7F, 0x00);
	value = tas880021_i2c_read(client, 0x58);
	printk(KERN_ERR "## [%s():%s:%d\t] Device ID : 0x%02x \n", __FUNCTION__, strrchr(__FILE__, '/')+1, value);
#endif

#if 1
	// Soft Reset
	tas880021_i2c_write(client, 0x00, 0x00);
	tas880021_i2c_write(client, 0x7F, 0x00);
	tas880021_i2c_write(client, 0x02, 0x10);
	tas880021_i2c_write(client, 0x01, 0x11);
	mdelay(5);

	// Amp software wetting
	tas880021_i2c_write(client, 0x00, 0x00);
	tas880021_i2c_write(client, 0x7F, 0x00);
	tas880021_i2c_write(client, 0x03, 0x00); // DSP unmute
	tas880021_i2c_write(client, 0x2a, 0x11); // AMP unmute

	tas880021_i2c_write(client, 0x25, 0x18);
	tas880021_i2c_write(client, 0x0d, 0x10);
	tas880021_i2c_write(client, 0x02, 0x00);

	data[0] = 0x07;	data[1] = 0x00;	data[2] = 0x20;
	tas880021_i2c_write_burst(client, data, 3);
	mdelay(5);

	tas880021_i2c_write(client, 0x00, 0x00);
	tas880021_i2c_write(client, 0x7F, 0x00);
	data[0] = 0x3d;	data[1] = 0x5f;	data[2] = 0x5f;
	tas880021_i2c_write_burst(client, data, 3);

#endif


#if 0

	tas880021_i2c_write(client, 0x02, 0x90);
	mdelay(1);

	tas880021_i2c_write(client, 0x32, 0x00);

	// gain setting
	tas880021_i2c_write(client, 0x00, 0x0b);

	data[0] = 0x64;	data[1] = 0x00;	data[2] = 0x40; data[3] = 0x00; data[4] = 0x00;
	tas880021_i2c_write_burst(client, data, 5);

	data[0] = 0x68;	data[1] = 0x00;	data[2] = 0x00; data[3] = 0x00; data[4] = 0x00;
	tas880021_i2c_write_burst(client, data, 5);

	data[0] = 0x6c;	data[1] = 0x00;	data[2] = 0x00; data[3] = 0x00; data[4] = 0x00;
	tas880021_i2c_write_burst(client, data, 5);

	data[0] = 0x70;	data[1] = 0x00;	data[2] = 0x40; data[3] = 0x00; data[4] = 0x00;
	tas880021_i2c_write_burst(client, data, 5);

	data[0] = 0x74;	data[1] = 0x04;	data[2] = 0x40; data[3] = 0x00; data[4] = 0x00;
	tas880021_i2c_write_burst(client, data, 5);

	data[0] = 0x78;	data[1] = 0x04;	data[2] = 0x40; data[3] = 0x00; data[4] = 0x00;
	tas880021_i2c_write_burst(client, data, 5);
#endif

	return 0;
}

/*
 * Clock after PLL and dividers
 */
static int tas880021_set_dai_sysclk(struct snd_soc_dai *codec_dai,
		int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct tas880021_priv *tas880021 = snd_soc_codec_get_drvdata(codec);
    pr_debug("%s  freq is %d \n",__func__,freq);

	switch (freq) {
	case  8192000:
	case 11289600:
	case 12288000:
	case 16384000:
	case 16934400:
	case 18432000:
	case 22579200:
	case 24576000:
		tas880021->sysclk = freq;
		return 0;
	}
	return -EINVAL;
}

static int tas880021_set_dai_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{
	return 0;
}

static int tas880021_pcm_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	return 0;
}

static int tas880021_mute(struct snd_soc_dai *dai, int mute)
{
    return 0;
}

static int tas880021_set_dai_pll(struct snd_soc_dai *codec_dai, int pll_id,
		int source, unsigned int freq_in, unsigned int freq_out)
{
	return 0;
}

#define TAS880021_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE \
			| SNDRV_PCM_FMTBIT_S24_LE \
			| SNDRV_PCM_FMTBIT_S32_LE)

static const struct snd_soc_dai_ops tas880021_dai_ops = {
		.hw_params = tas880021_pcm_hw_params,
		.digital_mute = tas880021_mute,
		.set_fmt = tas880021_set_dai_fmt,
		.set_sysclk = tas880021_set_dai_sysclk,
		.set_pll = tas880021_set_dai_pll,
};

static struct snd_soc_dai_driver tas880021_dai = {
	.name = "tas880021-hifi",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rate_min =	8000,
		.rate_max =	48000,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.formats = TAS880021_FORMATS,},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rate_min =	8000,
		.rate_max =	48000,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.formats = TAS880021_FORMATS,},

	//.ops = &tas880021_dai_ops,
};

static int tas880021_suspend(struct snd_soc_codec *codec)
{
	return 0;
}

static int tas880021_resume(struct snd_soc_codec *codec)
{
	struct tas880021_priv *tas880021 = snd_soc_codec_get_drvdata(codec);
	struct i2c_client *client = tas880021->i2c;

	int i = 0;
	char value = 0;
	char data[32] = {0, };

	// Soft Reset
	tas880021_i2c_write(client, 0x00, 0x00);
	tas880021_i2c_write(client, 0x7F, 0x00);
	tas880021_i2c_write(client, 0x02, 0x10);
	tas880021_i2c_write(client, 0x01, 0x11);
	mdelay(5);

	// Amp software wetting
	tas880021_i2c_write(client, 0x00, 0x00);
	tas880021_i2c_write(client, 0x7F, 0x00);
	tas880021_i2c_write(client, 0x03, 0x00); // DSP unmute
	tas880021_i2c_write(client, 0x2a, 0x11); // AMP unmute

	tas880021_i2c_write(client, 0x25, 0x18);
	tas880021_i2c_write(client, 0x0d, 0x10);
	tas880021_i2c_write(client, 0x02, 0x00);

	data[0] = 0x07;	data[1] = 0x00;	data[2] = 0x20;
	tas880021_i2c_write_burst(client, data, 3);
	mdelay(5);

	tas880021_i2c_write(client, 0x00, 0x00);
	tas880021_i2c_write(client, 0x7F, 0x00);
	data[0] = 0x3d;	data[1] = 0x5f;	data[2] = 0x5f;
	tas880021_i2c_write_burst(client, data, 3);

	return 0;
}

static int tas880021_probe(struct snd_soc_codec *codec)
{
	struct tas880021_priv *tas880021 = snd_soc_codec_get_drvdata(codec);
	//struct snd_soc_dapm_context *dapm = &codec->dapm;
	int ret;
    pr_debug("%s ............\n",__func__);

	ret = snd_soc_codec_set_cache_io(codec, 8, 16, tas880021->control_type);

	if (ret < 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}

    ret = init_codec(tas880021->i2c);

	if (ret < 0)
		printk(KERN_ERR "## [%s():%s:%d\t] init_codec fail(ret:%d).\n", __FUNCTION__, strrchr(__FILE__, '/')+1, __LINE__, ret);

	return ret;
}

/* power down chip */
static int tas880021_remove(struct snd_soc_codec *codec)
{
	return 0;
}

static struct snd_soc_codec_driver soc_codec_device_tas880021 = {
	.probe = tas880021_probe,
	.remove = tas880021_remove,
	.suspend = tas880021_suspend,
	.resume = tas880021_resume,
	//.set_bias_level = tas880021_set_bias_level,
	//.reg_cache_size = 0x05,
	//.reg_word_size = sizeof(u8),
	//.reg_cache_step = 2,
};

/*
 * ALC5623 2 wire address is determined by A1 pin
 * state during powerup.
 *    low  = 0x1a
 *    high = 0x1b
 */
static __devinit int tas880021_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct tas880021_platform_data *pdata;
	struct tas880021_priv *tas880021;
	int ret;

    pr_debug("%s ............\n",__func__);

	tas880021 = devm_kzalloc(&client->dev, sizeof(struct tas880021_priv),
			       GFP_KERNEL);
	if (tas880021 == NULL)
		return -ENOMEM;

	pdata = client->dev.platform_data;
	if (pdata) {
		tas880021->add_ctrl = pdata->add_ctrl;
		tas880021->jack_det_ctrl = pdata->jack_det_ctrl;
	}

	tas880021->id = 0x21;
	tas880021->control_type = SND_SOC_I2C;

	i2c_set_clientdata(client, tas880021);
    tas880021->i2c=client;

	ret = snd_soc_register_codec(&client->dev,
					&soc_codec_device_tas880021, &tas880021_dai, 1);
	if (ret != 0)
		dev_err(&client->dev, "Failed to register codec: %d\n", ret);

	return ret;
}

static __devexit int tas880021_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);
	return 0;
}

static const struct i2c_device_id tas880021_i2c_table[] = {
	{"tas880021", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, tas880021_i2c_table);

/*  i2c codec control layer */
static struct i2c_driver tas880021_i2c_driver = {
	.driver = {
		.name = "tas880021",
		.owner = THIS_MODULE,
	},
	.probe = tas880021_i2c_probe,
	.remove =  __devexit_p(tas880021_i2c_remove),
	.id_table = tas880021_i2c_table,
};

static int __init tas880021_modinit(void)
{
	int ret;

	ret = i2c_add_driver(&tas880021_i2c_driver);
	if (ret != 0) {
		printk(KERN_ERR "%s: can't add i2c driver", __func__);
		return ret;
	}

	return ret;
}
module_init(tas880021_modinit);

static void __exit tas880021_modexit(void)
{
	i2c_del_driver(&tas880021_i2c_driver);
}
module_exit(tas880021_modexit);

MODULE_DESCRIPTION("ASoC tas880021 driver");
MODULE_AUTHOR("  ");
MODULE_LICENSE("GPL");
