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

static struct i2c_client *i2c;

struct tas880021_priv {
	enum snd_soc_control_type control_type;
	u8 id;
	unsigned int sysclk;
	unsigned int add_ctrl;
	unsigned int jack_det_ctrl;
};

#if 0// PJSIN 20170103 add-- [ 1
static int init_regs[][2]=
{
    {0x02,0x0000},
    {0x04,0x0808},
    {0x14,0x3f3f},
    {0x0c,0x0808},
    {0x22,0x0101},
    {0x0a,0x0202},
    {0x62,0x880e},
    {0x12,0xff9f},
    {0x34,0x8000},
    {0x36,0x066f},
    {0x40,0x3410},
    #if 1
    {0x1c,0x8740},
    #else
    {0x1c,0xA740},
    {0x6a,0x0046},
    {0x6c,0xFFFF},
    #endif
    {0x5a,0x0000},
    //{0x3A,0x1d60},
    //{0x3C,0xf7ff},
    //{0x3E,0xf6ff},
    {0x02,0x0000},
    {0x04,0x0000},
    {0x06,0x0000},
    {0x0c,0x0000},
};

#define REG_INIT_NUM (sizeof(init_regs)/sizeof(init_regs[0]))

static int set_init_regs(struct snd_soc_codec *codec)
{
    int i=0,reg,ret;
    for (i=0;i<REG_INIT_NUM;i++) {
        //snd_soc_write(codec, init_regs[i][0], init_regs[i][1]);
        ret=i2c_smbus_write_word_data(i2c,(u8)init_regs[i][0], (u16)init_regs[i][1]);
        if (ret<0) {
            printk("i2c_smbus_write_word_data error %d\n",ret);
        }
        msleep(10);
        reg= i2c_smbus_read_word_data(i2c,(u8)init_regs[i][0]);
        pr_debug("Read reg[0x%02x] = 0x%04x \n",init_regs[i][0],reg);


    }
    pr_debug("%s over %d regs \n",__func__,REG_INIT_NUM);
    return 0;
}
#endif// ]-- end

unsigned char STA339_Coeff_Tbl1[15] = {
	//0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00
};
unsigned char STA339_Coeff_Tbl2[15] = {
	//0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25
	0xd8, 0x35, 0x19, 0x4b, 0x70, 0x58, 0x27, 0xca, 0xe7, 0xb0, 0x08, 0xae, 0x42, 0x43, 0x7c
};
unsigned char STA339_Coeff_Tbl3[15] = {
	//0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25
	0x81, 0xDF, 0x90, 0x7C, 0x58, 0xFF, 0x7E, 0x20, 0x70, 0x83, 0x71, 0x3E, 0x40, 0x1A, 0xE1
};
unsigned char STA339_Coeff_Tbl4[15] = {
	//0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25
	0x83, 0xB2, 0x48, 0x79, 0x59, 0x30, 0x7C, 0x4D, 0xB8, 0x86, 0x58, 0x73, 0x40, 0x27, 0x2E
};
unsigned char STA339_Coeff_Tbl5[15] = {
	//0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00
};
unsigned char STA339_Coeff_Tbl6[15] = {
	//0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00
};
unsigned char STA339_Coeff_Tbl7[15] = {
	//0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00
};
unsigned char STA339_Coeff_Tbl8[15] = {
	//0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00
};
unsigned char STA339_Coeff_Tbl9[15] = {
	//0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25
	0x5a, 0x9d, 0xf7, 0x40, 0x00, 0x00, 0x40, 0x00, 0x00, 0x40, 0x00, 0x00, 0x40, 0x00, 0x00
};
unsigned char STA339_Voice_Coeff_Tbl9[15] = {
	//0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25
	0x5a, 0x9d, 0xf7, 0x7f, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xff, 0xff
};


/*
        x0  x1  x2  x3  x4  x5  x6  x7  x8  x9  xA  xB  xC  xD  xE  xF
0x     63 80 97 59 C2 DE 14 00 2A FF 20 80 F0 -- 00 80
1x     80 AA 0A 69 0A D9 00 00 00 00 00 00 00 00 00 00
2x     00 00 00 00 00 00 00 1A C0 F3 33 00 0C 7F -- --
3x     -- 00 9F 9F 9E 9E 84 00 00 01 EE FF 7E C0 26 00
*/

unsigned char STA339_Init_Code_Tbl[64] = {
	0x63, 0x80, 0x97, 0x59, 0xC2, 0xDE, 0x14, 0x00, 0x2A, 0xFF, 0x20, 0x80, 0xF0, 0x0, 0x00, 0x80,
	0x80, 0xAA, 0x0A, 0x69, 0x0A, 0xD9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1A, 0xC0, 0xF3, 0x33, 0x00, 0x0C, 0x7F, 0x0, 0x0,
	0x0, 0x00, 0x9F, 0x9F, 0x9E, 0x9E, 0x84, 0x00, 0x00, 0x01, 0xEE, 0xFF, 0x7E, 0xC0, 0x26, 0x00
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

    ret = init_codec(i2c);

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
    i2c=client;

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
