/*
 * (C) Copyright 2009
 * jung hyun kim, Nexell Co, <jhkim@nexell.co.kr>
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
#include <sound/jack.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <mach/platform.h>

#include "../codecs/tas880021.h"
#include "nxp-i2s.h"


/*
#define	pr_debug	printk
*/
#if defined (CFG_IO_AUDIO_STA3339BWS_AMP_POWER )
#define	AUDIO_AMP_POWER		CFG_IO_AUDIO_STA3339BWS_AMP_POWER
#endif
#if defined (CFG_IO_AUDIO_STA3339BWS_AMP_EN )
#define	AUDIO_AMP_EN		CFG_IO_AUDIO_STA3339BWS_AMP_EN
#endif

//static struct snd_soc_jack_gpio jack_gpio;
static struct snd_soc_codec *tas880021 = NULL;
static int codec_bias_level = 0;
static int (*cpu_resume_fn)(struct snd_soc_dai *dai) = NULL;

#if defined(CONFIG_PLAT_S5P4418_NBOX)
/***************************************/
// jimmy@zhongwei, 20140609 Testing
/***************************************/
/* sysfs name HeadsetObserver.java looks for to track headset state
 */
#include <linux/switch.h>
struct switch_dev switch_nxl_jack_detection = {
	.name = "h2w",
};
static int jack_report_enable = 0;
static int NXL_JackInOut = 0;
//struct wake_lock detect_jack_wake_lock;

#endif

static int tas880021_jack_status_check(void);
static struct snd_soc_jack_gpio jack_gpio = {
	//.invert		= true,			// High detect : invert = false
	.name		= "hp-gpio",
	.report		= SND_JACK_HEADPHONE | SND_JACK_HEADSET | SND_JACK_MECHANICAL | SND_JACK_AVOUT,
	.debounce_time = 200,
	.jack_status_check = tas880021_jack_status_check,
};

static struct snd_soc_jack hp_jack;


static int tas880021_jack_status_check(void)
{
	struct snd_soc_codec *codec = tas880021;
	int jack = jack_gpio.gpio;
	int invert = jack_gpio.invert;
	int level = gpio_get_value_cansleep(jack);

	if (!codec)
		return -1;

	if (NULL != jack_gpio.name) {
		if(invert)
			level = !level;

		printk("%s: hp jack %s\n", __func__, level?"IN":"OUT");

		if (!level) {
#if defined(CONFIG_PLAT_S5P4418_NBOX)
			/***************************************/
			// jimmy@zhongwei, 20140609 Testing
			/***************************************/
			NXL_JackInOut = 0x00; // 1: jack In
#endif
		    snd_soc_update_bits(codec, 0x04, 0x8080, 0x8080);
#if defined (AUDIO_AMP_POWER)
			gpio_direction_output(AUDIO_AMP_POWER, 1);
#endif
#if defined (AUDIO_AMP_EN)
			gpio_direction_output(AUDIO_AMP_EN, 1);
#endif
		} else {
#if defined(CONFIG_PLAT_S5P4418_NBOX)
			/***************************************/
			// jimmy@zhongwei, 20140609 Testing
			/***************************************/
			NXL_JackInOut = 0x02; // 1: jack In
#endif
		    snd_soc_update_bits(codec, 0x04, 0x8080, 0);
#if defined (AUDIO_AMP_EN)
			gpio_direction_output(AUDIO_AMP_EN, 0);
#endif
#if defined (AUDIO_AMP_POWER)
			gpio_direction_output(AUDIO_AMP_POWER, 0);
#endif
		}

#if defined(CONFIG_PLAT_S5P4418_NBOX)
		/***************************************/
		// jimmy@zhongwei, 20140609 Testing
		/***************************************/
		//if(jack_report_enable)
		{
			printk(" Jack Report (%d)\n", NXL_JackInOut);
			//wake_lock_timeout(&detect_jack_wake_lock, WAKE_LOCK_TIME);
			switch_set_state(&switch_nxl_jack_detection, NXL_JackInOut); //  2->Jack In
		}
#endif
	}

	return !level;
}

static int tas880021_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{

	return 0;
}

static int tas880021_startup(struct snd_pcm_substream *substream)
{
	int jack = jack_gpio.gpio;
	int invert = jack_gpio.invert;
	int level = gpio_get_value_cansleep(jack);

	pr_debug("%s\n", __func__);

	if (NULL != jack_gpio.name) {
		if(invert)
			level = !level;

		if (!level) {
			pr_debug("AMP ON\n");

#if defined(CONFIG_PLAT_S5P4418_NBOX)
			/***************************************/
			// jimmy@zhongwei, 20140609 Testing
			/***************************************/
			if(level == 0)
			switch_set_state(&switch_nxl_jack_detection, 0); //
			else
			switch_set_state(&switch_nxl_jack_detection, 0x2); //
#endif
#if defined (AUDIO_AMP_POWER)
			gpio_direction_output(AUDIO_AMP_POWER, 1);
#endif
#if defined (AUDIO_AMP_EN)
			gpio_direction_output(AUDIO_AMP_EN, 1);
#endif
		}
		//jack_report_enable=1;
	} else {
			pr_debug("AMP ON\n");
#if defined (AUDIO_AMP_POWER)
			gpio_direction_output(AUDIO_AMP_POWER, 1);
#endif
#if defined (AUDIO_AMP_EN)
			gpio_direction_output(AUDIO_AMP_EN, 1);
#endif
	}
	return 0;
}

static void tas880021_shutdown(struct snd_pcm_substream *substream)
{
	int stream = substream->stream;

    if (stream == SNDRV_PCM_STREAM_PLAYBACK)
	{
		pr_debug("AMP OFF\n");
#if 0
		/***************************************/
		// jimmy@zhongwei, 20140609 Testing
		/***************************************/
		switch_set_state(&switch_nxl_jack_detection, 0); //  1->Jack In
#endif
#if defined (AUDIO_AMP_EN)
		gpio_direction_output(AUDIO_AMP_EN, 0);
#endif
#if defined (AUDIO_AMP_POWER)
		gpio_direction_output(AUDIO_AMP_POWER, 0);
#endif
	}
}

static int tas880021_suspend_pre(struct snd_soc_card *card)
{

	return 0;
}

static int tas880021_resume_pre(struct snd_soc_card *card)
{
    struct snd_soc_dai *cpu_dai = card->rtd->cpu_dai;
	struct snd_soc_codec *codec = tas880021;

	int ret = 0;

	if (cpu_dai->driver->resume && ! cpu_resume_fn) {
		cpu_resume_fn = cpu_dai->driver->resume;
		cpu_dai->driver->resume = NULL;
	}

	if (cpu_resume_fn)
		ret = cpu_resume_fn(cpu_dai);

	PM_DBGOUT("%s BAIAS=%d\n", __func__, codec->dapm.bias_level);
	codec_bias_level = codec->dapm.bias_level;
	return ret;
}

static int tas880021_resume_post(struct snd_soc_card *card)
{
	struct snd_soc_codec *codec = tas880021;
	PM_DBGOUT("%s BAIAS=%d\n", __func__, codec->dapm.bias_level);

	if (SND_SOC_BIAS_OFF != codec_bias_level)
		codec->driver->resume(codec);

	tas880021_jack_status_check();
	return 0;
}



static int tas880021_dai_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_jack_gpio *jack = &jack_gpio;
	int ret;

	pr_debug("%s: %s\n", __func__, jack->name);

	tas880021 = codec;

	if (NULL == jack->name)
		return 0;

#if defined(CONFIG_PLAT_S5P4418_NBOX)
	/***************************************/
	// jimmy@zhongwei, 20140609 Testing
	/***************************************/
	ret = switch_dev_register(&switch_nxl_jack_detection);
	if (ret < 0) {
		printk("%s : Failed to register switch device\n", __func__);
	}
	//wake_lock_init(&detect_jack_wake_lock, WAKE_LOCK_SUSPEND, "nxl_jack_detect");
#endif

	ret = snd_soc_jack_new(codec, "Headphone Jack", SND_JACK_HEADPHONE, &hp_jack);
	if (ret)
		return ret;

	ret = snd_soc_jack_add_gpios(&hp_jack, 1, jack);
	printk("%s: %s register audio jack detect, %d\n", ret?"Fail":"Done", __func__, jack->gpio);

	return 0;
}

static struct snd_soc_ops tas880021_ops = {
	.hw_params 	= tas880021_hw_params,
	.startup	= tas880021_startup,
	.shutdown	= tas880021_shutdown,
};

static char str_dai_name[16] = DEV_NAME_I2S;

static struct snd_soc_dai_link tas880021_dai_link = {
	.name 			= "ASOC-tas880021",
	.stream_name 	= "tas880021 HiFi",
	.cpu_dai_name 	= str_dai_name,			/* nxp_snd_i2s_driver name */
	.platform_name  = DEV_NAME_PCM,			/* nxp_snd_pcm_driver name */
	.codec_dai_name = "tas880021-hifi",		/* tas880021_dai's name */
	.codec_name 	= "tas880021.0-0048",		/* tas880021_i2c_driver name + '.' + bus + '-' + address(7bit) */
	.ops 			= &tas880021_ops,
	.symmetric_rates = 1,
	.init			= tas880021_dai_init,
	.ops 			= &tas880021_ops,
};

static struct snd_soc_card tas880021_card = {
	.name 			= "I2S-tas880021",		/* proc/asound/cards */
	.owner 			= THIS_MODULE,
	.dai_link 		= &tas880021_dai_link,
	.num_links 		= 1,
	.suspend_pre	= &tas880021_suspend_pre,
	.resume_pre		= &tas880021_resume_pre,
	.resume_post	= &tas880021_resume_post,
};


/*
 * codec driver
 */
static int tas880021_probe(struct platform_device *pdev)
{
	struct nxp_snd_dai_plat_data *plat = pdev->dev.platform_data;
	struct snd_soc_card *card = &tas880021_card;
	struct snd_soc_jack_gpio *jack = &jack_gpio;
	struct nxp_snd_jack_pin *hpin = NULL;
	unsigned int rates = 0, format = 0;
	int ret;

	if (plat) {
		rates = plat->sample_rate;
		format = plat->pcm_format;
		hpin = &plat->hp_jack;
		if (hpin->support) {
			jack->gpio = hpin->detect_io;
			jack->invert = hpin->detect_level ?  false : true;
			jack->debounce_time = hpin->debounce_time ? : 200;
		} else {
			jack->name = NULL;
		}
		sprintf(str_dai_name, "%s.%d", DEV_NAME_I2S, plat->i2s_ch);	// set I2S name
	}
#if defined (AUDIO_AMP_POWER)
    gpio_request(AUDIO_AMP_POWER, "tas880021_amp_power");
#endif
#if defined (AUDIO_AMP_EN)
    gpio_request(AUDIO_AMP_EN, "tas880021_amp_en");
#endif

	/*
	 * register card
	 */
	card->dev = &pdev->dev;
	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card() failed: %d\n", ret);
		return ret;
	}

	if (card->rtd->cpu_dai) {
		struct snd_soc_dai *cpu_dai = card->rtd->cpu_dai;
		struct snd_soc_dai_driver *i2s = cpu_dai->driver;
		if (rates) {
			rates = snd_pcm_rate_to_rate_bit(rates);
			if (SNDRV_PCM_RATE_KNOT == rates)
				printk("%s, invalid sample rates=%d\n", __func__, plat->sample_rate);
			else {
				i2s->playback.rates = rates;
				i2s->capture.rates = rates;
			}
		}
		if (format) {
			i2s->playback.formats = format;
			i2s->capture.formats = format;
		}
	}

	printk("tas880021-dai: register card %s -> %s\n",
		card->dai_link->codec_dai_name, card->dai_link->cpu_dai_name);
	return 0;
}

static int tas880021_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	pr_debug("%s\n", __func__);
#if defined(CONFIG_PLAT_S5P4418_NBOX)
	/***************************************/
	// jimmy@zhongwei, 20140609 Testing
	/***************************************/
	//wake_lock_destroy(&detect_jack_wake_lock);
	switch_dev_unregister(&switch_nxl_jack_detection);
#endif
	snd_soc_unregister_card(card);
#if defined (AUDIO_AMP_POWER)
	gpio_free(AUDIO_AMP_POWER);
#endif
#if defined (AUDIO_AMP_EN)
	gpio_free(AUDIO_AMP_EN);
#endif
	return 0;
}

static struct platform_driver tas880021_driver = {
	.driver		= {
		.name	= "tas880021-audio",
		.owner	= THIS_MODULE,
		.pm 	= &snd_soc_pm_ops,	/* for suspend */
	},
	.probe		= tas880021_probe,
	.remove		= __devexit_p(tas880021_remove),
};
module_platform_driver(tas880021_driver);

MODULE_AUTHOR("jhkim <jhkim@nexell.co.kr>");
MODULE_DESCRIPTION("Sound codec-tas880021 driver for the SLSI");
MODULE_LICENSE("GPL");

