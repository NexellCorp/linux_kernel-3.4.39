/*
 * alcdummy.c  --  ALCDUMMY ALSA Soc Audio driver
 *
 * Copyright 2011 Realtek Microelectronics
 *
 * Author: flove <flove@realtek.com>
 *
 * Based on WM8753.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <mach/platform.h>
#include <mach/s5p4418.h>
#include <mach/soc.h>

#include "alcdummy.h"

/*
#define	VMID_ADD_WIDGET
*/

#define VERSION "0.1.0 alsa 1.0.25"

struct alcdummy_init_reg {
	u8 reg;
	u16 val;
};

struct alcdummy_priv {
        struct snd_soc_codec *codec;
        int codec_version;
        int master;
        int sysclk;
        int rate;
        int rx_rate;
        int bclk_rate;
        int dmic_used_flag;
        struct i2c_client *client;
};

static const u16 alcdummy_reg[ALCDUMMY_VENDOR_ID1 + 1] = {
#if 0
        [ALCDUMMY_SPK_OUT_VOL] = 0x8888,
        [ALCDUMMY_HP_OUT_VOL] = 0x8080,
        [ALCDUMMY_MONO_AXO_1_2_VOL] = 0xa080,
        [ALCDUMMY_AUX_IN_VOL] = 0x0808,
        [ALCDUMMY_ADC_REC_MIXER] = 0xf0f0,
        //[ALCDUMMY_VDAC_DIG_VOL] = 0x0010,  //for ALC5631V
        [ALCDUMMY_OUTMIXER_L_CTRL] = 0xffc0,
        [ALCDUMMY_OUTMIXER_R_CTRL] = 0xffc0,
        [ALCDUMMY_AXO1MIXER_CTRL] = 0x88c0,
        [ALCDUMMY_AXO2MIXER_CTRL] = 0x88c0,
        [ALCDUMMY_DIG_MIC_CTRL] = 0x3000,
        [ALCDUMMY_MONO_INPUT_VOL] = 0x8808,
        [ALCDUMMY_SPK_MIXER_CTRL] = 0xf8f8,
        [ALCDUMMY_SPK_MONO_OUT_CTRL] = 0xfc00,
        [ALCDUMMY_SPK_MONO_HP_OUT_CTRL] = 0x4440,
        [ALCDUMMY_SDP_CTRL] = 0x8000,
        //[ALCDUMMY_MONO_SDP_CTRL] = 0x8000,     //for ALC5631V
        [ALCDUMMY_STEREO_AD_DA_CLK_CTRL] = 0x2010,
        [ALCDUMMY_GEN_PUR_CTRL_REG] = 0x0e00,
        [ALCDUMMY_INT_ST_IRQ_CTRL_2] = 0x0710,
        [ALCDUMMY_MISC_CTRL] = 0x2040,
        [ALCDUMMY_DEPOP_FUN_CTRL_2] = 0x8000,
        [ALCDUMMY_SOFT_VOL_CTRL] = 0x07e0,
        [ALCDUMMY_ALC_CTRL_1] = 0x0206,
        [ALCDUMMY_ALC_CTRL_3] = 0x2000,
        [ALCDUMMY_PSEUDO_SPATL_CTRL] = 0x0553,
#endif
};


static struct alcdummy_init_reg init_list[] = {
        {ALCDUMMY_SPK_OUT_VOL            , 0xe0a5},//speaker output volume is 0db by default,
        {ALCDUMMY_SPK_HP_MIXER_CTRL      , 0x0020},//HP from HP_VOL
        {ALCDUMMY_HP_OUT_VOL             , 0xc0c0},//HP output volume is 0 db by default
        {ALCDUMMY_AUXOUT_VOL             , 0x0010},//Auxout volume is 0db by default
        {ALCDUMMY_REC_MIXER_CTRL         , 0x7d7d},//ADC Record Mixer Control
        {ALCDUMMY_ADC_CTRL               , 0x000a},
        {ALCDUMMY_MIC_CTRL_2             , 0x7700},//boost 40db
        {ALCDUMMY_HPMIXER_CTRL           , 0x3e3e},//"HP Mixer Control"
        {ALCDUMMY_AUXMIXER_CTRL          , 0x3e3e},//"AUX Mixer Control" // tnn
        {ALCDUMMY_SPKMIXER_CTRL          , 0x08fc},//"SPK Mixer Control"
        {ALCDUMMY_SPK_AMP_CTRL           , 0x0000},
//      {ALCDUMMY_GEN_PUR_CTRL_1         , 0x8C00}, //set spkratio to auto
        {ALCDUMMY_ZC_SM_CTRL_1           , 0x0001},      //Disable Zero Cross
        {ALCDUMMY_ZC_SM_CTRL_2           , 0x3000},      //Disable Zero cross
        {ALCDUMMY_MIC_CTRL_1             , 0x8808}, //set mic1 to differnetial mode
        {ALCDUMMY_DEPOP_CTRL_2           , 0xB000},
        {ALCDUMMY_PRI_REG_ADD                , 0x0056},
        {ALCDUMMY_PRI_REG_DATA           , 0x303f},
        {ALCDUMMY_DIG_BEEP_IRQ_CTRL      , 0x01E0},
        {ALCDUMMY_ALC_CTRL_1                 , 0x0808},
        {ALCDUMMY_ALC_CTRL_2                 , 0x0003},
        {ALCDUMMY_ALC_CTRL_3                 , 0xe081},
};

#define ALCDUMMY_INIT_REG_LEN ARRAY_SIZE(init_list)

int audio_dummy_path = 7;

int alcdummy_write_mask(struct snd_soc_codec *codec, unsigned short reg,
                         unsigned int value, unsigned int mask)
{
      //printk("%s reg=0x%x, mask=0x%x, val=0x%x\n", __func__, reg, mask, value);
    return 0;//snd_soc_update_bits(codec, reg, mask, value);
}

//static struct snd_soc_device *alcdummy_socdev;
//static struct snd_soc_codec *alcdummy_codec;
/*
 * read alcdummy register cache
 */
static inline unsigned int alcdummy_read_reg_cache(struct snd_soc_codec *codec,
        unsigned int reg)
{
        u16 *cache = codec->reg_cache;
        if (reg < 1 || reg > (ARRAY_SIZE(alcdummy_reg) + 1))
                return -1;
        return cache[reg];
}


/*
 * write alcdummy register cache
 */

static inline void alcdummy_write_reg_cache(struct snd_soc_codec *codec,
        unsigned int reg, unsigned int value)
{
        u16 *cache = codec->reg_cache;
        if (reg < 0 || reg > 0x7e)
                return;
        cache[reg] = value;
}


static int alcdummy_write(struct snd_soc_codec *codec, unsigned int reg, unsigned int val)
{
      //printk("%s reg=0x%x, val=0x%x\n", __func__, reg, val);
        //snd_soc_write(codec, reg, val);
        //alcdummy_write_reg_cache(codec, reg, val);

	return 0;
}

static unsigned int alcdummy_read(struct snd_soc_codec *codec, unsigned int reg)
{
        return 0;//(snd_soc_read(codec, reg));
}

static int alcdummy_reg_init(struct snd_soc_codec *codec)
{
	int i;

	for (i = 0; i < ALCDUMMY_INIT_REG_LEN; i++)
		alcdummy_write(codec, init_list[i].reg, init_list[i].val);

	return 0;
}

static int alcdummy_write_index(struct snd_soc_codec *codec, unsigned int index,unsigned int value)
{
    unsigned char RetVal = 0;

    RetVal = alcdummy_write(codec,ALCDUMMY_PRI_REG_ADD,index);

    if(RetVal != 0)
      return RetVal;

    RetVal = alcdummy_write(codec,ALCDUMMY_PRI_REG_DATA,value);
    return RetVal;
}

unsigned int alcdummy_read_index(struct snd_soc_codec *codec, unsigned int reg)
{
        unsigned int value = 0x0;
        alcdummy_write(codec,ALCDUMMY_PRI_REG_ADD,reg);
        value=alcdummy_read(codec,ALCDUMMY_PRI_REG_DATA);

        return value;
}

void alcdummy_write_index_mask(struct snd_soc_codec *codec, unsigned int reg,unsigned int value,unsigned int mask)
{
        unsigned  int CodecData;

        if(!mask)
                return;

        if(mask!=0xffff)
         {
                CodecData=alcdummy_read_index(codec,reg);
                CodecData&=~mask;
                CodecData|=(value&mask);
                alcdummy_write_index(codec,reg,CodecData);
         }
        else
        {
                alcdummy_write_index(codec,reg,value);
        }
}

static int alcdummy_reset(struct snd_soc_codec *codec)
{
	return alcdummy_write(codec, ALCDUMMY_RESET, 0);
}


// tnn
#define CFG_GPIO_AMP_EN					(PAD_GPIO_B + 26)
//#define CFG_GPIO_AMP_EN					(PAD_GPIO_B + 24)

void alcdummy_amp_en()
{
    unsigned int temp_gpio;
#if 0
	temp_gpio = gpio_get_value(CFG_GPIO_AMP_EN);

	if(temp_gpio == 0) {
		gpio_set_value(CFG_GPIO_AMP_EN, 1);
	}
#endif
}

static int spk_event(struct snd_soc_dapm_widget *w,
        struct snd_kcontrol *kcontrol, int event)
{
        struct snd_soc_codec *codec = w->codec;
        unsigned int l, r;
		
#if 0
		unsigned int temp_gpio;// tnn

		temp_gpio = gpio_get_value(CFG_GPIO_AMP_EN);
	
		if(temp_gpio == 0) {
			gpio_set_value(CFG_GPIO_AMP_EN, 1);
		}
#endif
	if(!(audio_dummy_path & 0x01))
		return 0;

        l = (alcdummy_read(codec, ALCDUMMY_PWR_MANAG_ADD4) & (0x01 << 15)) >> 15;
        r = (alcdummy_read(codec, ALCDUMMY_PWR_MANAG_ADD4) & (0x01 << 14)) >> 14;

        switch (event) {
        case SND_SOC_DAPM_PRE_PMD:
                if (l && r)
                {
                        alcdummy_write_mask(codec, ALCDUMMY_SPK_OUT_VOL, 0x8000, 0x8000);

                        alcdummy_write_mask(codec, ALCDUMMY_PWR_MANAG_ADD1, 0x0000, 0x2020);
			//printk("spk_event 0\n");
			//gpio_set_value(CFG_GPIO_AMP_EN, 0);
                }

                break;
        case SND_SOC_DAPM_POST_PMU:
                if (l && r)
                {
                        alcdummy_write_mask(codec, ALCDUMMY_PWR_MANAG_ADD1, 0x2020, 0x2020);
                        alcdummy_write_mask(codec, ALCDUMMY_SPK_OUT_VOL, 0x0000, 0x8000);
                        alcdummy_write(codec, ALCDUMMY_DAC_DIG_VOL, 0x1010);
                        alcdummy_write_index(codec, 0X45, 0X4100);
			//printk("spk_event 1\n");
			//gpio_set_value(CFG_GPIO_AMP_EN, 1);
                }
                break;
        default:
                return -EINVAL;
        }

	return 0;
}

static int auxout_event(struct snd_soc_dapm_widget *w,
        struct snd_kcontrol *kcontrol, int event)
{
        struct snd_soc_codec *codec = w->codec;
        unsigned int l, r;
	static unsigned int aux_out_enable=0;

	if(!(audio_dummy_path & 0x02))
		return 0;

        l = (alcdummy_read(codec, ALCDUMMY_PWR_MANAG_ADD4) & (0x01 << 9)) >> 9;
        r = (alcdummy_read(codec, ALCDUMMY_PWR_MANAG_ADD4) & (0x01 << 8)) >> 8;

        switch (event) {
        case SND_SOC_DAPM_PRE_PMD:
                if ((l && r)&&(aux_out_enable))
                {
                        alcdummy_write_mask(codec, ALCDUMMY_AUXOUT_VOL, 0x8080, 0x8080);
			aux_out_enable=0;
			//printk("auxout_event 0\n");
                }

                break;
        case SND_SOC_DAPM_POST_PMU:
                if ((l && r)&&(!aux_out_enable))
                {
                        alcdummy_write_mask(codec, ALCDUMMY_AUXOUT_VOL, 0x0000, 0x8080);
			aux_out_enable=1;
			//printk("auxout_event 1\n");
                }
                break;
        default:
                return -EINVAL;
        }

        return 0;
}

//HP mute/unmute depop

static void hp_mute_unmute_depop(struct snd_soc_codec *codec,unsigned int Mute)
{
        if(Mute)
        {
                alcdummy_write_mask(codec,ALCDUMMY_DEPOP_CTRL_1,PW_SOFT_GEN|EN_SOFT_FOR_S_M_DEPOP|EN_HP_R_M_UM_DEPOP|EN_HP_L_M_UM_DEPOP
                                  ,PW_SOFT_GEN|EN_SOFT_FOR_S_M_DEPOP|EN_HP_R_M_UM_DEPOP|EN_HP_L_M_UM_DEPOP);
                alcdummy_write_mask(codec, ALCDUMMY_HP_OUT_VOL, 0x8080, 0x8080);
                msleep(80);
                alcdummy_write_mask(codec,ALCDUMMY_DEPOP_CTRL_1,0,PW_SOFT_GEN|EN_SOFT_FOR_S_M_DEPOP|EN_HP_R_M_UM_DEPOP|EN_HP_L_M_UM_DEPOP);
        }
        else
        {
                alcdummy_write_mask(codec,ALCDUMMY_DEPOP_CTRL_1,PW_SOFT_GEN|EN_SOFT_FOR_S_M_DEPOP|EN_HP_R_M_UM_DEPOP|EN_HP_L_M_UM_DEPOP
                                  ,PW_SOFT_GEN|EN_SOFT_FOR_S_M_DEPOP|EN_HP_R_M_UM_DEPOP|EN_HP_L_M_UM_DEPOP);
                alcdummy_write_mask(codec, ALCDUMMY_HP_OUT_VOL, 0x0000, 0x8080);
                msleep(80);
                alcdummy_write_mask(codec,ALCDUMMY_DEPOP_CTRL_1,0,PW_SOFT_GEN|EN_SOFT_FOR_S_M_DEPOP|EN_HP_R_M_UM_DEPOP|EN_HP_L_M_UM_DEPOP);
        }
}

//HP power on depop

static void hp_depop_mode2(struct snd_soc_codec *codec)
{
        alcdummy_write_mask(codec,ALCDUMMY_PWR_MANAG_ADD3,PWR_MAIN_BIAS|PWR_VREF,PWR_VREF|PWR_MAIN_BIAS);
        alcdummy_write_mask(codec,ALCDUMMY_DEPOP_CTRL_1,PW_SOFT_GEN,PW_SOFT_GEN);
        alcdummy_write_mask(codec,ALCDUMMY_PWR_MANAG_ADD3,PWR_HP_AMP,PWR_HP_AMP);
        alcdummy_write_mask(codec,ALCDUMMY_DEPOP_CTRL_1,EN_DEPOP_2,EN_DEPOP_2);
       // schedule_timeout_uninterruptible(msecs_to_jiffies(300));
        alcdummy_write_mask(codec,ALCDUMMY_PWR_MANAG_ADD3,PWR_HP_DIS_DEPOP|PWR_HP_AMP_DRI,PWR_HP_DIS_DEPOP|PWR_HP_AMP_DRI);
        alcdummy_write_mask(codec,ALCDUMMY_PWR_MANAG_ADD4,PWR_HP_L_VOL|PWR_HP_R_VOL,PWR_HP_L_VOL|PWR_HP_R_VOL);
        alcdummy_write_mask(codec,ALCDUMMY_DEPOP_CTRL_1,0,EN_DEPOP_2);
}

static int open_hp_end_widgets(struct snd_soc_codec *codec)
{
	hp_mute_unmute_depop(codec, 0);

	return 0;
}

static int close_hp_end_widgets(struct snd_soc_codec *codec)
{
	hp_mute_unmute_depop(codec, 1);
	
	alcdummy_write_mask(codec, ALCDUMMY_PWR_MANAG_ADD3, 0x0000, 0x000F);
	return 0;
}

static int hp_event(struct snd_soc_dapm_widget *w,
        struct snd_kcontrol *kcontrol, int event)
{
        struct snd_soc_codec *codec = w->codec;
        unsigned int l, r;
	static unsigned int hp_out_enable=0;

	if(!(audio_dummy_path & 0x04))
		return 0;

        l = (alcdummy_read(codec, ALCDUMMY_PWR_MANAG_ADD4) & (0x01 << 11)) >> 11;
        r = (alcdummy_read(codec, ALCDUMMY_PWR_MANAG_ADD4) & (0x01 << 10)) >> 10;

        switch (event) {
        case SND_SOC_DAPM_PRE_PMD:
                if ((l && r)&&(hp_out_enable))
                {
			close_hp_end_widgets(codec);
			hp_out_enable = 0;
			//printk("hp_event 0\n");
                }

                break;
        case SND_SOC_DAPM_POST_PMU:
                if ((l && r)&&(!hp_out_enable))
                {
			hp_depop_mode2(codec);
			open_hp_end_widgets(codec);
			hp_out_enable = 1;
                        alcdummy_write(codec, ALCDUMMY_DAC_DIG_VOL, 0x0000);
			//printk("hp_event 1\n");
                }
                break;
        default:
                return -EINVAL;
        }

        return 0;
}

static int dac_event(struct snd_soc_dapm_widget *w,
        struct snd_kcontrol *kcontrol, int event)
{
	//printk("dac_event\n");
        struct snd_soc_codec *codec = w->codec;
        static unsigned int dac_enable=0;

        switch (event) {

        case SND_SOC_DAPM_PRE_PMD:

                //printk("dac_event --SND_SOC_DAPM_PRE_PMD\n");
                if (dac_enable)
                {

#ifdef ALCDUMMY_EQ_FUNC_ENA

        #ifdef (ALCDUMMY_EQ_FUNC_SEL==ALCDUMMY_EQ_FOR_DAC)

                alcdummy_update_eqmode(codec,NORMAL);    //disable EQ

        #endif

#endif

#ifdef ALCDUMMY_ALC_FUNC_ENA

        #ifdef (ALCDUMMY_ALC_FUNC_SEL==ALCDUMMY_ALC_FOR_DAC)

                alcdummy_alc_enable(codec,0);            //disable ALC

        #endif

#endif

                        dac_enable=0;
                }
                break;

        case SND_SOC_DAPM_POST_PMU:

                //printk("dac_event --SND_SOC_DAPM_POST_PMU\n");
                if(!dac_enable)
                {

#ifdef ALCDUMMY_EQ_FUNC_ENA

        #if (ALCDUMMY_EQ_FUNC_SEL==ALCDUMMY_EQ_FOR_DAC)

                alcdummy_update_eqmode(codec,EQ_DEFAULT_PRESET); //enable EQ preset

        #endif

#endif

#if ALCDUMMY_ALC_FUNC_ENA

        #if (ALCDUMMY_ALC_FUNC_SEL==ALCDUMMY_ALC_FOR_DAC)

                alcdummy_alc_enable(codec,1);            //enable ALC

        #endif

#endif
                        dac_enable=1;
                }
                break;
        default:
                return 0;
        }

	return 0;
}

static int adc_event(struct snd_soc_dapm_widget *w,
        struct snd_kcontrol *kcontrol, int event)
{
	printk("adc_event- not yet\n");
	return 0;
}


static const char *alcdummy_spo_source_sel[] = {"VMID", "HPMIX", "SPKMIX", "AUXMIX"};
static const char *alcdummy_input_mode_source_sel[] = {"Single-end", "Differential"};
static const char *alcdummy_auxout_mode_source_sel[] = {"Differential", "Stereo"};
static const char *alcdummy_mic_boost[] = {"Bypass", "+20db", "+24db", "+30db",
                        "+35db", "+40db", "+44db", "+50db", "+52db"};
static const char *alcdummy_spor_source_sel[] = {"RN", "RP", "LN", "VMID"};

static const struct soc_enum alcdummy_enum[] = {
SOC_ENUM_SINGLE(ALCDUMMY_SPKMIXER_CTRL, 10, 4, alcdummy_spo_source_sel),   /*0*/
SOC_ENUM_SINGLE(ALCDUMMY_MIC_CTRL_1, 15, 2,  alcdummy_input_mode_source_sel),     /*1*/
SOC_ENUM_SINGLE(ALCDUMMY_MIC_CTRL_1, 7, 2,  alcdummy_input_mode_source_sel),      /*2*/
SOC_ENUM_SINGLE(ALCDUMMY_SPK_OUT_VOL, 12, 2, alcdummy_input_mode_source_sel),     /*3*/
SOC_ENUM_SINGLE(ALCDUMMY_MIC_CTRL_2, 12, 8, alcdummy_mic_boost),                  /*4*/
SOC_ENUM_SINGLE(ALCDUMMY_MIC_CTRL_2, 8, 8, alcdummy_mic_boost),                   /*5*/
SOC_ENUM_SINGLE(ALCDUMMY_SPK_OUT_VOL, 13, 4, alcdummy_spor_source_sel), /*6*/
SOC_ENUM_SINGLE(ALCDUMMY_AUXOUT_VOL, 14, 2, alcdummy_auxout_mode_source_sel), /*7*/
};


static const struct snd_kcontrol_new alcdummy_snd_controls[] = {
#if 0
SOC_ENUM("MIC1 Mode Control",  alcdummy_enum[1]),
SOC_ENUM("MIC1 Boost", alcdummy_enum[4]),

SOC_ENUM("MIC2 Mode Control", alcdummy_enum[2]),
SOC_ENUM("MIC2 Boost", alcdummy_enum[5]),
SOC_ENUM("Classab Mode Control", alcdummy_enum[3]),
SOC_ENUM("SPKR Out Control", alcdummy_enum[6]),
SOC_ENUM("AUXOUT Control", alcdummy_enum[7]),
SOC_DOUBLE("Line1 Capture Volume", ALCDUMMY_LINE_IN_1_VOL, 8, 0, 31, 1),
SOC_DOUBLE("Line2 Capture Volume", ALCDUMMY_LINE_IN_2_VOL, 8, 0, 31, 1),

SOC_SINGLE("MIC1 Playback Volume", ALCDUMMY_MIC_CTRL_1, 8, 31, 1),
SOC_SINGLE("MIC2 Playback Volume", ALCDUMMY_MIC_CTRL_1, 0, 31, 1),

SOC_SINGLE("AXOL Playback Switch", ALCDUMMY_AUXOUT_VOL, 15, 1, 1),
SOC_SINGLE("AXOR Playback Switch", ALCDUMMY_AUXOUT_VOL, 7, 1, 1),
SOC_DOUBLE("AUX Playback Volume", ALCDUMMY_AUXOUT_VOL, 8, 0, 31, 1),
SOC_SINGLE("SPK Playback Switch", ALCDUMMY_SPK_OUT_VOL, 15, 1, 1),
SOC_DOUBLE("SPK Playback Volume", ALCDUMMY_SPK_OUT_VOL, 5, 0, 31, 1),
SOC_SINGLE("HPL Playback Switch", ALCDUMMY_HP_OUT_VOL, 15, 1, 1),
SOC_SINGLE("HPR Playback Switch", ALCDUMMY_HP_OUT_VOL, 7, 1, 1),
SOC_DOUBLE("HP Playback Volume", ALCDUMMY_HP_OUT_VOL, 8, 0, 31, 1),
#endif
};



static const struct snd_kcontrol_new alcdummy_recmixl_mixer_controls[] = {
SOC_DAPM_SINGLE("HPMIXL Capture Switch", ALCDUMMY_REC_MIXER_CTRL, 14, 1, 1),
SOC_DAPM_SINGLE("AUXMIXL Capture Switch", ALCDUMMY_REC_MIXER_CTRL, 13, 1, 1),
SOC_DAPM_SINGLE("SPKMIX Capture Switch", ALCDUMMY_REC_MIXER_CTRL, 12, 1, 1),
SOC_DAPM_SINGLE("LINE1L Capture Switch", ALCDUMMY_REC_MIXER_CTRL, 11, 1, 1),
SOC_DAPM_SINGLE("LINE2L Capture Switch", ALCDUMMY_REC_MIXER_CTRL, 10, 1, 1),
SOC_DAPM_SINGLE("MIC1 Capture Switch", ALCDUMMY_REC_MIXER_CTRL, 9, 1, 1),
SOC_DAPM_SINGLE("MIC2 Capture Switch", ALCDUMMY_REC_MIXER_CTRL, 8, 1, 1),
};

static const struct snd_kcontrol_new alcdummy_recmixr_mixer_controls[] = {
SOC_DAPM_SINGLE("HPMIXR Capture Switch", ALCDUMMY_REC_MIXER_CTRL, 6, 1, 1),
SOC_DAPM_SINGLE("AUXMIXR Capture Switch", ALCDUMMY_REC_MIXER_CTRL, 5, 1, 1),
SOC_DAPM_SINGLE("SPKMIX Capture Switch", ALCDUMMY_REC_MIXER_CTRL, 4, 1, 1),
SOC_DAPM_SINGLE("LINE1R Capture Switch", ALCDUMMY_REC_MIXER_CTRL, 3, 1, 1),
SOC_DAPM_SINGLE("LINE2R Capture Switch", ALCDUMMY_REC_MIXER_CTRL, 2, 1, 1),
SOC_DAPM_SINGLE("MIC1 Capture Switch", ALCDUMMY_REC_MIXER_CTRL, 1, 1, 1),
SOC_DAPM_SINGLE("MIC2 Capture Switch", ALCDUMMY_REC_MIXER_CTRL, 0, 1, 1),
};


static const struct snd_kcontrol_new alcdummy_hp_mixl_mixer_controls[] = {
SOC_DAPM_SINGLE("RECMIXL Playback Switch", ALCDUMMY_HPMIXER_CTRL, 13, 1, 1),
SOC_DAPM_SINGLE("MIC1 Playback Switch", ALCDUMMY_HPMIXER_CTRL, 12, 1, 1),
SOC_DAPM_SINGLE("MIC2 Playback Switch", ALCDUMMY_HPMIXER_CTRL, 11, 1, 1),
SOC_DAPM_SINGLE("LINE1 Playback Switch", ALCDUMMY_HPMIXER_CTRL, 10, 1, 1),
SOC_DAPM_SINGLE("LINE2 Playback Switch", ALCDUMMY_HPMIXER_CTRL, 9, 1, 1),
SOC_DAPM_SINGLE("DAC Playback Switch", ALCDUMMY_HPMIXER_CTRL, 8, 1, 1),


};

static const struct snd_kcontrol_new alcdummy_hp_mixr_mixer_controls[] = {
SOC_DAPM_SINGLE("RECMIXR Playback Switch", ALCDUMMY_HPMIXER_CTRL, 5, 1, 1),
SOC_DAPM_SINGLE("MIC1 Playback Switch", ALCDUMMY_HPMIXER_CTRL, 4, 1, 1),
SOC_DAPM_SINGLE("MIC2 Playback Switch", ALCDUMMY_HPMIXER_CTRL, 3, 1, 1),
SOC_DAPM_SINGLE("LINE1 Playback Switch", ALCDUMMY_HPMIXER_CTRL, 2, 1, 1),
SOC_DAPM_SINGLE("LINE2 Playback Switch", ALCDUMMY_HPMIXER_CTRL, 1, 1, 1),
SOC_DAPM_SINGLE("DAC Playback Switch", ALCDUMMY_HPMIXER_CTRL, 0, 1, 1),
};

static const struct snd_kcontrol_new alcdummy_auxmixl_mixer_controls[] = {
SOC_DAPM_SINGLE("RECMIXL Playback Switch", ALCDUMMY_AUXMIXER_CTRL, 13, 1, 1),
SOC_DAPM_SINGLE("MIC1 Playback Switch", ALCDUMMY_AUXMIXER_CTRL, 12, 1, 1),
SOC_DAPM_SINGLE("MIC2 Playback Switch", ALCDUMMY_AUXMIXER_CTRL, 11, 1, 1),
SOC_DAPM_SINGLE("LINE1 Playback Switch", ALCDUMMY_AUXMIXER_CTRL, 10, 1, 1),
SOC_DAPM_SINGLE("LINE2 Playback Switch", ALCDUMMY_AUXMIXER_CTRL, 9, 1, 1),
SOC_DAPM_SINGLE("DAC Playback Switch", ALCDUMMY_AUXMIXER_CTRL, 8, 1, 1),

};

static const struct snd_kcontrol_new alcdummy_auxmixr_mixer_controls[] = {
SOC_DAPM_SINGLE("RECMIXR Playback Switch", ALCDUMMY_AUXMIXER_CTRL, 5, 1, 1),
SOC_DAPM_SINGLE("MIC1 Playback Switch", ALCDUMMY_AUXMIXER_CTRL, 4, 1, 1),
SOC_DAPM_SINGLE("MIC2 Playback Switch", ALCDUMMY_AUXMIXER_CTRL, 3, 1, 1),
SOC_DAPM_SINGLE("LINE1 Playback Switch", ALCDUMMY_AUXMIXER_CTRL, 2, 1, 1),
SOC_DAPM_SINGLE("LINE2 Playback Switch", ALCDUMMY_AUXMIXER_CTRL, 1, 1, 1),
SOC_DAPM_SINGLE("DAC Playback Switch", ALCDUMMY_AUXMIXER_CTRL, 0, 1, 1),
};

static const struct snd_kcontrol_new alcdummy_spkmixr_mixer_controls[]  = {
SOC_DAPM_SINGLE("MIC1 Playback Switch", ALCDUMMY_SPKMIXER_CTRL, 7, 1, 1),
SOC_DAPM_SINGLE("MIC2 Playback Switch", ALCDUMMY_SPKMIXER_CTRL, 6, 1, 1),
SOC_DAPM_SINGLE("LINE1L Playback Switch", ALCDUMMY_SPKMIXER_CTRL, 5, 1, 1),
SOC_DAPM_SINGLE("LINE1R Playback Switch", ALCDUMMY_SPKMIXER_CTRL, 4, 1, 1),
SOC_DAPM_SINGLE("LINE2L Playback Switch", ALCDUMMY_SPKMIXER_CTRL, 3, 1, 1),
SOC_DAPM_SINGLE("LINE2R Playback Switch", ALCDUMMY_SPKMIXER_CTRL, 2, 1, 1),
SOC_DAPM_SINGLE("DACL Playback Switch", ALCDUMMY_SPKMIXER_CTRL, 1, 1, 1),
SOC_DAPM_SINGLE("DACR Playback Switch", ALCDUMMY_SPKMIXER_CTRL, 0, 1, 1),
};

static const struct snd_kcontrol_new alcdummy_spo_mux_control =
SOC_DAPM_ENUM("Route", alcdummy_enum[0]);

static const struct snd_soc_dapm_widget alcdummy_dapm_widgets[] = {
#if 0
SND_SOC_DAPM_INPUT("MIC1"),
SND_SOC_DAPM_INPUT("MIC2"),
SND_SOC_DAPM_INPUT("LINE1L"),
SND_SOC_DAPM_INPUT("LINE2L"),
SND_SOC_DAPM_INPUT("LINE1R"),
SND_SOC_DAPM_INPUT("LINE2R"),


SND_SOC_DAPM_PGA("Mic1 Boost", ALCDUMMY_PWR_MANAG_ADD2, 5, 0, NULL, 0),
SND_SOC_DAPM_PGA("Mic2 Boost", ALCDUMMY_PWR_MANAG_ADD2, 4, 0, NULL, 0),

SND_SOC_DAPM_PGA("LINE1L Inp Vol", ALCDUMMY_PWR_MANAG_ADD2, 9, 0, NULL, 0),
SND_SOC_DAPM_PGA("LINE1R Inp Vol", ALCDUMMY_PWR_MANAG_ADD2, 8, 0, NULL, 0),
SND_SOC_DAPM_PGA("LINE2L Inp Vol", ALCDUMMY_PWR_MANAG_ADD2, 7, 0, NULL, 0),
SND_SOC_DAPM_PGA("LINE2R Inp Vol", ALCDUMMY_PWR_MANAG_ADD2, 6, 0, NULL, 0),



SND_SOC_DAPM_MIXER("RECMIXL Mixer", ALCDUMMY_PWR_MANAG_ADD2, 11, 0,
        &alcdummy_recmixl_mixer_controls[0], ARRAY_SIZE(alcdummy_recmixl_mixer_controls)),
SND_SOC_DAPM_MIXER("RECMIXR Mixer", ALCDUMMY_PWR_MANAG_ADD2, 10, 0,
        &alcdummy_recmixr_mixer_controls[0], ARRAY_SIZE(alcdummy_recmixr_mixer_controls)),


SND_SOC_DAPM_ADC_E("Left ADC","Left ADC HIFI Capture", ALCDUMMY_PWR_MANAG_ADD1,12, 0,
                adc_event, SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
SND_SOC_DAPM_ADC_E("Right ADC","Right ADC HIFI Capture", ALCDUMMY_PWR_MANAG_ADD1,11, 0,
                adc_event, SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),

SND_SOC_DAPM_DAC_E("Left DAC", "Left DAC HIFI Playback", ALCDUMMY_PWR_MANAG_ADD1, 10, 0,
                dac_event, SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
SND_SOC_DAPM_DAC_E("Right DAC", "Right DAC HIFI Playback", ALCDUMMY_PWR_MANAG_ADD1, 9, 0,
                dac_event, SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),

SND_SOC_DAPM_MIXER("HPMIXL Mixer", ALCDUMMY_PWR_MANAG_ADD2, 15, 0,
        &alcdummy_hp_mixl_mixer_controls[0], ARRAY_SIZE(alcdummy_hp_mixl_mixer_controls)),
SND_SOC_DAPM_MIXER("HPMIXR Mixer", ALCDUMMY_PWR_MANAG_ADD2, 14, 0,
        &alcdummy_hp_mixr_mixer_controls[0], ARRAY_SIZE(alcdummy_hp_mixr_mixer_controls)),
SND_SOC_DAPM_MIXER("AUXMIXL Mixer", ALCDUMMY_PWR_MANAG_ADD2, 13, 0,
        &alcdummy_auxmixl_mixer_controls[0], ARRAY_SIZE(alcdummy_auxmixl_mixer_controls)),
SND_SOC_DAPM_MIXER("AUXMIXR Mixer", ALCDUMMY_PWR_MANAG_ADD2, 12, 0,
        &alcdummy_auxmixr_mixer_controls[0], ARRAY_SIZE(alcdummy_auxmixr_mixer_controls)),
SND_SOC_DAPM_MIXER("SPXMIX Mixer", ALCDUMMY_PWR_MANAG_ADD2, 0, 0,
        &alcdummy_spkmixr_mixer_controls[0], ARRAY_SIZE(alcdummy_spkmixr_mixer_controls)),

SND_SOC_DAPM_PGA_E("Left SPK Vol", ALCDUMMY_PWR_MANAG_ADD4, 15, 0, NULL, 0,
                spk_event, SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("Right SPK Vol", ALCDUMMY_PWR_MANAG_ADD4, 14, 0, NULL, 0,
                spk_event, SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),

SND_SOC_DAPM_PGA_E("Left HP Vol", ALCDUMMY_PWR_MANAG_ADD4, 11, 0, NULL, 0,
                hp_event, SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("Right HP Vol", ALCDUMMY_PWR_MANAG_ADD4, 10, 0, NULL, 0,
                hp_event, SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("Left AUX Out Vol", ALCDUMMY_PWR_MANAG_ADD4, 9, 0, NULL, 0,
                auxout_event, SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA_E("Right AUX Out Vol", ALCDUMMY_PWR_MANAG_ADD4, 8, 0, NULL, 0,
                auxout_event, SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),

SND_SOC_DAPM_MUX("SPKO Mux", SND_SOC_NOPM, 0, 0, &alcdummy_spo_mux_control),

SND_SOC_DAPM_MICBIAS("Mic Bias1", ALCDUMMY_PWR_MANAG_ADD2, 3, 0),
SND_SOC_DAPM_MICBIAS("Mic Bias2", ALCDUMMY_PWR_MANAG_ADD2, 2, 0),

SND_SOC_DAPM_OUTPUT("AUXOUTL"),
SND_SOC_DAPM_OUTPUT("AUXOUTR"),
SND_SOC_DAPM_OUTPUT("SPOL"),
SND_SOC_DAPM_OUTPUT("SPOR"),
SND_SOC_DAPM_OUTPUT("HPOL"),
SND_SOC_DAPM_OUTPUT("HPOR"),
#endif
};

static const struct snd_soc_dapm_route alcdummy_dapm_routes[] = {
#if 0
        {"Mic1 Boost", NULL, "MIC1"},
        {"Mic2 Boost", NULL, "MIC2"},


        {"LINE1L Inp Vol", NULL, "LINE1L"},
        {"LINE1R Inp Vol", NULL, "LINE1R"},

        {"LINE2L Inp Vol", NULL, "LINE2L"},
        {"LINE2R Inp Vol", NULL, "LINE2R"},


        {"RECMIXL Mixer", "HPMIXL Capture Switch", "HPMIXL Mixer"},
        {"RECMIXL Mixer", "AUXMIXL Capture Switch", "AUXMIXL Mixer"},
        {"RECMIXL Mixer", "SPKMIX Capture Switch", "SPXMIX Mixer"},
        {"RECMIXL Mixer", "LINE1L Capture Switch", "LINE1L Inp Vol"},
        {"RECMIXL Mixer", "LINE2L Capture Switch", "LINE2L Inp Vol"},
        {"RECMIXL Mixer", "MIC1 Capture Switch", "Mic1 Boost"},
        {"RECMIXL Mixer", "MIC2 Capture Switch", "Mic2 Boost"},

        {"RECMIXR Mixer", "HPMIXR Capture Switch", "HPMIXR Mixer"},
        {"RECMIXR Mixer", "AUXMIXR Capture Switch", "AUXMIXR Mixer"},
        {"RECMIXR Mixer", "SPKMIX Capture Switch", "SPXMIX Mixer"},
        {"RECMIXR Mixer", "LINE1R Capture Switch", "LINE1R Inp Vol"},
        {"RECMIXR Mixer", "LINE2R Capture Switch", "LINE2R Inp Vol"},
        {"RECMIXR Mixer", "MIC1 Capture Switch", "Mic1 Boost"},
        {"RECMIXR Mixer", "MIC2 Capture Switch", "Mic2 Boost"},

        {"Left ADC", NULL, "RECMIXL Mixer"},
        {"Right ADC", NULL, "RECMIXR Mixer"},

        {"HPMIXL Mixer", "RECMIXL Playback Switch", "RECMIXL Mixer"},
        {"HPMIXL Mixer", "MIC1 Playback Switch", "Mic1 Boost"},
        {"HPMIXL Mixer", "MIC2 Playback Switch", "Mic2 Boost"},
        {"HPMIXL Mixer", "LINE1 Playback Switch", "LINE1L Inp Vol"},
        {"HPMIXL Mixer", "LINE2 Playback Switch", "LINE2L Inp Vol"},
        {"HPMIXL Mixer", "DAC Playback Switch", "Left DAC"},

        {"HPMIXR Mixer", "RECMIXR Playback Switch", "RECMIXR Mixer"},
        {"HPMIXR Mixer", "MIC1 Playback Switch", "Mic1 Boost"},
        {"HPMIXR Mixer", "MIC2 Playback Switch", "Mic2 Boost"},
        {"HPMIXR Mixer", "LINE1 Playback Switch", "LINE1R Inp Vol"},
        {"HPMIXR Mixer", "LINE2 Playback Switch", "LINE2R Inp Vol"},
        {"HPMIXR Mixer", "DAC Playback Switch", "Right DAC"},

        {"AUXMIXL Mixer", "RECMIXL Playback Switch", "RECMIXL Mixer"},
        {"AUXMIXL Mixer", "MIC1 Playback Switch", "Mic1 Boost"},
        {"AUXMIXL Mixer", "MIC2 Playback Switch", "Mic2 Boost"},
        {"AUXMIXL Mixer", "LINE1 Playback Switch", "LINE1L Inp Vol"},
        {"AUXMIXL Mixer", "LINE2 Playback Switch", "LINE2L Inp Vol"},
        {"AUXMIXL Mixer", "DAC Playback Switch", "Left DAC"},

        {"AUXMIXR Mixer", "RECMIXR Playback Switch", "RECMIXR Mixer"},
        {"AUXMIXR Mixer", "MIC1 Playback Switch", "Mic1 Boost"},
        {"AUXMIXR Mixer", "MIC2 Playback Switch", "Mic2 Boost"},
        {"AUXMIXR Mixer", "LINE1 Playback Switch", "LINE1R Inp Vol"},
        {"AUXMIXR Mixer", "LINE2 Playback Switch", "LINE2R Inp Vol"},
        {"AUXMIXR Mixer", "DAC Playback Switch", "Right DAC"},

        {"SPXMIX Mixer", "MIC1 Playback Switch", "Mic1 Boost"},
        {"SPXMIX Mixer", "MIC2 Playback Switch", "Mic2 Boost"},
        {"SPXMIX Mixer", "DACL Playback Switch", "Left DAC"},
        {"SPXMIX Mixer", "DACR Playback Switch", "Right DAC"},
        {"SPXMIX Mixer", "LINE1L Playback Switch", "LINE1L Inp Vol"},
        {"SPXMIX Mixer", "LINE1R Playback Switch", "LINE1R Inp Vol"},
        {"SPXMIX Mixer", "LINE2L Playback Switch", "LINE2L Inp Vol"},
        {"SPXMIX Mixer", "LINE2R Playback Switch", "LINE2R Inp Vol"},

        {"SPKO Mux", "HPMIX", "HPMIXL Mixer"},
        {"SPKO Mux", "SPKMIX", "SPXMIX Mixer"},
        {"SPKO Mux", "AUXMIX", "AUXMIXL Mixer"},

        {"Left SPK Vol",  NULL, "SPKO Mux"},
        {"Right SPK Vol",  NULL, "SPKO Mux"},

        {"Right HP Vol",  NULL, "HPMIXR Mixer"},
        {"Left HP Vol",  NULL, "HPMIXL Mixer"},

        {"Left AUX Out Vol",  NULL, "AUXMIXL Mixer"},
        {"Right AUX Out Vol",  NULL, "AUXMIXR Mixer"},

        {"AUXOUTL", NULL, "Left AUX Out Vol"},
        {"AUXOUTR", NULL, "Right AUX Out Vol"},
        {"SPOL", NULL, "Left SPK Vol"},
        {"SPOR", NULL, "Right SPK Vol"},
        {"HPOL", NULL, "Left HP Vol"},
        {"HPOR", NULL, "Right HP Vol"},
#endif
};

struct _coeff_dummy_div{
        unsigned int mclk;       //pllout or MCLK
        unsigned int bclk;       //master mode
        unsigned int rate;
        unsigned int reg_val;
};

/* PLL divisors */
struct _pll_div {
	u32 pll_in;
	u32 pll_out;
	u16 regvalue;
};

static const struct _pll_div codec_master_pll_div[] = {

        {  2048000,  8192000,   0x0ea0},
        {  3686400,  8192000,   0x4e27},
        { 12000000,  8192000,   0x456b},
        { 13000000,  8192000,   0x495f},
        { 13100000,      8192000,       0x0320},
        {  2048000,  11289600,  0xf637},
        {  3686400,  11289600,  0x2f22},
        { 12000000,  11289600,  0x3e2f},
        { 13000000,  11289600,  0x4d5b},
        { 13100000,      11289600,      0x363b},
        {  2048000,  16384000,  0x1ea0},
        {  3686400,  16384000,  0x9e27},
        { 12000000,  16384000,  0x452b},
        { 13000000,  16384000,  0x542f},
        { 13100000,      16384000,      0x03a0},
        {  2048000,  16934400,  0xe625},
        {  3686400,  16934400,  0x9126},
        { 12000000,  16934400,  0x4d2c},
        { 13000000,  16934400,  0x742f},
        { 13100000,      16934400,      0x3c27},
        {  2048000,  22579200,  0x2aa0},
        {  3686400,  22579200,  0x2f20},
        { 12000000,  22579200,  0x7e2f},
        { 13000000,  22579200,  0x742f},
        { 13100000,      22579200,      0x3c27},
        {  2048000,  24576000,  0x2ea0},
        {  3686400,  24576000,  0xee27},
        { 12000000,  24576000,  0x2915},
        { 13000000,  24576000,  0x772e},
        { 13100000,      24576000,      0x0d20},
        { 26000000,  24576000,  0x2027},
        { 26000000,  22579200,  0x392f},
        { 24576000,  22579200,  0x0921},
        { 24576000,  24576000,  0x02a0},
};

static const struct _pll_div codec_slave_pll_div[] = {

        {  1024000,  16384000,  0x3ea0},
        {  1411200,  22579200,  0x3ea0},
        {  1536000,      24576000,      0x3ea0},
        {  2048000,  16384000,  0x1ea0},
        {  2822400,  22579200,  0x1ea0},
        {  3072000,      24576000,      0x1ea0},
        {       705600,  11289600,      0x3ea0},
        {       705600,   8467200,      0x3ab0},

};

struct _coeff_dummy_div coeff_dummy_div[] = {

        //sysclk is 256fs
        { 2048000,  8000 * 32,  8000, 0x1000},
        { 2048000,  8000 * 64,  8000, 0x0000},
        { 2822400, 11025 * 32, 11025, 0x1000},
        { 2822400, 11025 * 64, 11025, 0x0000},
        { 4096000, 16000 * 32, 16000, 0x1000},
        { 4096000, 16000 * 64, 16000, 0x0000},
        { 5644800, 22050 * 32, 22050, 0x1000},
        { 5644800, 22050 * 64, 22050, 0x0000},
        { 8192000, 32000 * 32, 32000, 0x1000},
        { 8192000, 32000 * 64, 32000, 0x0000},
        {11289600, 44100 * 32, 44100, 0x1000},
        {11289600, 44100 * 64, 44100, 0x0000},
        {12288000, 48000 * 32, 48000, 0x1000},
        {12288000, 48000 * 64, 48000, 0x0000},
        //sysclk is 512fs
        { 4096000,  8000 * 32,  8000, 0x3000},
        { 4096000,  8000 * 64,  8000, 0x2000},
        { 5644800, 11025 * 32, 11025, 0x3000},
        { 5644800, 11025 * 64, 11025, 0x2000},
        { 8192000, 16000 * 32, 16000, 0x3000},
        { 8192000, 16000 * 64, 16000, 0x2000},
        {11289600, 22050 * 32, 22050, 0x3000},
        {11289600, 22050 * 64, 22050, 0x2000},
        {16384000, 32000 * 32, 32000, 0x3000},
        {16384000, 32000 * 64, 32000, 0x2000},
        {22579200, 44100 * 32, 44100, 0x3000},
        {22579200, 44100 * 64, 44100, 0x2000},
        {24576000, 48000 * 32, 48000, 0x3000},
        {24576000, 48000 * 64, 48000, 0x2000},
        //SYSCLK is 22.5792Mhz or 24.576Mhz(8k to 48k)
        {24576000, 48000 * 32, 48000, 0x3000},
        {24576000, 48000 * 64, 48000, 0x2000},
        {22579200, 44100 * 32, 44100, 0x3000},
        {22579200, 44100 * 64, 44100, 0x2000},
        {24576000, 32000 * 32, 32000, 0x1080},
        {24576000, 32000 * 64, 32000, 0x0080},
        {22579200, 22050 * 32, 22050, 0x5000},
        {22579200, 22050 * 64, 22050, 0x4000},
        {24576000, 16000 * 32, 16000, 0x3080},
        {24576000, 16000 * 64, 16000, 0x2080},
        {22579200, 11025 * 32, 11025, 0x7000},
        {22579200, 11025 * 64, 11025, 0x6000},
        {24576000,      8000 * 32,      8000, 0x7080},
        {24576000,      8000 * 64,      8000, 0x6080},

};

static int get_coeff(int mclk, int rate, int timesofbclk)
{
        int i;

        for (i = 0; i < ARRAY_SIZE(coeff_dummy_div); i++) {
                if ((coeff_dummy_div[i].mclk == mclk)
                                && (coeff_dummy_div[i].rate == rate)
                                && ((coeff_dummy_div[i].bclk / coeff_dummy_div[i].rate) == timesofbclk))
                                return i;
        }

                return -1;
}

static unsigned int BLCK_FREQ=32; 
static int get_coeff_in_slave_mode(int mclk, int rate)
{
        return get_coeff(mclk, rate, BLCK_FREQ);
}

static int get_coeff_in_master_mode(int mclk, int rate)
{
        return get_coeff(mclk, rate ,BLCK_FREQ);
}

static int alcdummy_hifi_pcm_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
        struct snd_soc_pcm_runtime *rtd = substream->private_data;
        struct snd_soc_codec *codec = rtd->codec;
        struct alcdummy_priv *alcdummy = snd_soc_codec_get_drvdata(codec);
        unsigned int iface = 0;
        int rate = params_rate(params);
        int coeff = 0;

        //printk(KERN_DEBUG "enter %s\n", __func__);
        if (!alcdummy->master)
                coeff = get_coeff_in_slave_mode(alcdummy->sysclk, rate);
        else
                coeff = get_coeff_in_master_mode(alcdummy->sysclk, rate);
        if (coeff < 0) {
                printk(KERN_ERR "%s get_coeff err!\n", __func__);
        //      return -EINVAL;
        }
        switch (params_format(params))
        {
                case SNDRV_PCM_FORMAT_S16_LE:
                        break;
                case SNDRV_PCM_FORMAT_S20_3LE:
                        iface |= 0x0004;
                        break;
                case SNDRV_PCM_FORMAT_S24_LE:
                        iface |= 0x0008;
                        break;
                case SNDRV_PCM_FORMAT_S8:
                        iface |= 0x000c;
                        break;
                default:
                        return -EINVAL;
        }

        alcdummy_write_mask(codec, ALCDUMMY_SDP_CTRL, iface, SDP_I2S_DL_MASK);
        alcdummy_write(codec, ALCDUMMY_STEREO_AD_DA_CLK_CTRL, coeff_dummy_div[coeff].reg_val);
        alcdummy_write_mask(codec, ALCDUMMY_PWR_MANAG_ADD1, 0x81C0, 0x81C0);


//    alcdummy_reg_dump(codec);

        return 0;
}

static int alcdummy_hifi_codec_set_dai_fmt(
	struct snd_soc_dai *codec_dai, unsigned int fmt)
{
        struct snd_soc_codec *codec = codec_dai->codec;
        struct alcdummy_priv *alcdummy =  snd_soc_codec_get_drvdata(codec);
        u16 iface = 0;

        //printk(KERN_DEBUG "enter %s\n", __func__);
        switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
        case SND_SOC_DAIFMT_CBM_CFM:
                alcdummy->master = 1;
                break;
        case SND_SOC_DAIFMT_CBS_CFS:
                iface |= (0x0001 << 15);
                alcdummy->master = 0;
                break;
        default:
                return -EINVAL;
        }

        switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
        case SND_SOC_DAIFMT_I2S:
                break;
        case SND_SOC_DAIFMT_LEFT_J:
                iface |= (0x0001);
                break;
        case SND_SOC_DAIFMT_DSP_A:
                iface |= (0x0002);
                break;
        case SND_SOC_DAIFMT_DSP_B:
                iface  |= (0x0003);
                break;
        default:
                return -EINVAL;
        }

        switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
        case SND_SOC_DAIFMT_NB_NF:
                break;
        case SND_SOC_DAIFMT_IB_NF:
                iface |= (0x0001 << 7);
                break;
        default:
                return -EINVAL;
        }

        alcdummy_write(codec, ALCDUMMY_SDP_CTRL, iface);
        return 0;

}

static int alcdummy_hifi_codec_set_dai_sysclk(struct snd_soc_dai *codec_dai,
				int clk_id, unsigned int freq, int dir)
{
        struct snd_soc_codec *codec = codec_dai->codec;
        struct alcdummy_priv *alcdummy = snd_soc_codec_get_drvdata(codec);

        //printk(KERN_DEBUG "enter %s\n", __func__);
        if ((freq >= (256 * 8000)) && (freq <= (512 * 96000))) {
                alcdummy->sysclk = freq;
                return 0;
        }

        printk(KERN_ERR "unsupported sysclk freq %u for audio i2s\n", freq);

	return -EINVAL;
}

static int alcdummy_codec_set_dai_pll(struct snd_soc_dai *codec_dai, int pll_id,
		int source, unsigned int freq_in, unsigned int freq_out)
{
        struct snd_soc_codec *codec = codec_dai->codec;
        struct alcdummy_priv *alcdummy = snd_soc_codec_get_drvdata(codec);
        int i;
        int ret = -EINVAL;

        alcdummy_write_mask(codec, ALCDUMMY_PWR_MANAG_ADD2, 0, PWR_PLL);

        if (!freq_in || !freq_out)
                return 0;

        if (alcdummy->master) {
                for (i = 0; i < ARRAY_SIZE(codec_master_pll_div); i ++) {
                        if ((freq_in == codec_master_pll_div[i].pll_in) && (freq_out == codec_master_pll_div[i].pll_out)) {
                                alcdummy_write(codec, ALCDUMMY_PLL_CTRL, codec_master_pll_div[i].regvalue);
                                alcdummy_write_mask(codec, ALCDUMMY_PWR_MANAG_ADD2, PWR_PLL, PWR_PLL);
                                msleep(20);
                                alcdummy_write(codec, ALCDUMMY_GBL_CLK_CTRL, 0x0000);
                                ret = 0;
                        }
                }
        } else {
                for (i = 0; i < ARRAY_SIZE(codec_slave_pll_div); i ++) {
                        if ((freq_in == codec_slave_pll_div[i].pll_in) && (freq_out == codec_slave_pll_div[i].pll_out))  {
                                alcdummy_write(codec, ALCDUMMY_PLL_CTRL, codec_slave_pll_div[i].regvalue);
                                alcdummy_write_mask(codec, ALCDUMMY_PWR_MANAG_ADD2, PWR_PLL, PWR_PLL);
                                msleep(20);
                                alcdummy_write(codec, ALCDUMMY_GBL_CLK_CTRL, 0x6000);
                                ret = 0;
                        }
                }
        }

        return 0;
}

#if 0
#define ALCDUMMY_SPK_VOL_MASK		0x3f
#define ALCDUMMY_HP_VOL_MASK		ALCDUMMY_VOL_MASK

/*
 * Speaker Volume control
 * 	amixer -c 0 cget numid=16
 *
 * Headphone Volume control
 * 	amixer -c 0 cget numid=20
 *
 * Mic boost control (MIC1=2, MIC2=4)
 * 	amixer -c 0 cget numid=2
 * 	amixer -c 0 cget numid=4
 */
static void alcdummy_setup(struct snd_soc_codec *codec)
{
	int spk_vol_up = 4; 		// 0 ~ 8 : 0 ~ 12db
	int hp_vol_up  = 0x10; 		// 0 ~ 1f : -46.5 ~ 0 db (step 1.5)
	int mic_boost  = 1; 		// 0=bypass, 1=20, 2=24, 3=30, 4=35, 5=40, 6=44, 7=50, 8=+52 db

	snd_soc_write(codec, ALCDUMMY_SPK_OUT_VOL, 0xc8c8);			// 02h: 0xc8c8
	snd_soc_write(codec, ALCDUMMY_HP_OUT_VOL, 0xc0c0);			// 04h: 0x4848
	snd_soc_write(codec, ALCDUMMY_MONO_AXO_1_2_VOL, 0xa080);
	snd_soc_write(codec, ALCDUMMY_ADC_REC_MIXER, 0xb0b0);
	snd_soc_write(codec, ALCDUMMY_MIC_CTRL_2, 0x1100);			// 22h: 5500, no boost
	snd_soc_write(codec, ALCDUMMY_OUTMIXER_L_CTRL, 0xdfC0);
	snd_soc_write(codec, ALCDUMMY_OUTMIXER_R_CTRL, 0xdfC0);
	snd_soc_write(codec, ALCDUMMY_SPK_MIXER_CTRL, 0xe8e8);		// 28h: 0xd8d8
	snd_soc_write(codec, ALCDUMMY_SPK_MONO_OUT_CTRL, 0x6c00);
	snd_soc_write(codec, ALCDUMMY_GEN_PUR_CTRL_REG, 0x4e00);		// 40h: HP volume
	snd_soc_write(codec, ALCDUMMY_SPK_MONO_HP_OUT_CTRL, 0x0000);

	/* 02h : speaker volume */
	snd_soc_update_bits(codec, ALCDUMMY_SPK_OUT_VOL,
		ALCDUMMY_SPK_VOL_MASK << ALCDUMMY_L_VOL_SHIFT | ALCDUMMY_SPK_VOL_MASK,
		abs(spk_vol_up - 0x8) << ALCDUMMY_L_VOL_SHIFT | abs(spk_vol_up - 0x8));

	/* 04h : headphone volume */
	snd_soc_update_bits(codec, ALCDUMMY_HP_OUT_VOL,
		ALCDUMMY_VOL_MASK << ALCDUMMY_L_VOL_SHIFT | ALCDUMMY_VOL_MASK,
		abs(hp_vol_up - 0x1f) << ALCDUMMY_L_VOL_SHIFT | abs(hp_vol_up - 0x1f));

	/* 22h : mic boost */
	snd_soc_update_bits(codec, ALCDUMMY_MIC_CTRL_2,
		ALCDUMMY_MIC1_BOOST_CTRL_MASK | ALCDUMMY_MIC2_BOOST_CTRL_MASK,
		mic_boost << ALCDUMMY_MIC1_BOOST_SHIFT |
		mic_boost << ALCDUMMY_MIC2_BOOST_SHIFT);
}
#endif

static int alcdummy_set_bias_level(struct snd_soc_codec *codec,
			enum snd_soc_bias_level level)
{
        printk(KERN_DEBUG "enter %s\n", __func__);

        switch (level) {
        case SND_SOC_BIAS_ON:
        case SND_SOC_BIAS_PREPARE:
                alcdummy_write_mask(codec, ALCDUMMY_PWR_MANAG_ADD3,PWR_VREF|PWR_MAIN_BIAS, PWR_VREF|PWR_MAIN_BIAS);
                alcdummy_write_mask(codec, ALCDUMMY_PWR_MANAG_ADD2,0x0008, 0x0008);
                break;
        case SND_SOC_BIAS_STANDBY:
                if (codec->dapm.bias_level == SND_SOC_BIAS_OFF) {
                        alcdummy_write_mask(codec, ALCDUMMY_PWR_MANAG_ADD3,PWR_VREF|PWR_MAIN_BIAS, PWR_VREF|PWR_MAIN_BIAS);
                        msleep(80);
                        alcdummy_write_mask(codec, ALCDUMMY_PWR_MANAG_ADD3,PWR_DIS_FAST_VREF,PWR_DIS_FAST_VREF);

                        codec->cache_only = false;
                        snd_soc_cache_sync(codec);
                }
                break;
        case SND_SOC_BIAS_OFF:
                alcdummy_write_mask(codec, ALCDUMMY_SPK_OUT_VOL, 0x8000, 0x8000); //mute speaker volume
                alcdummy_write_mask(codec, ALCDUMMY_HP_OUT_VOL, 0x8080, 0x8080);  //mute hp volume
                alcdummy_write(codec, ALCDUMMY_PWR_MANAG_ADD1, 0x0000);
                alcdummy_write(codec, ALCDUMMY_PWR_MANAG_ADD2, 0x0000);
                alcdummy_write(codec, ALCDUMMY_PWR_MANAG_ADD3, 0x0000);
                alcdummy_write(codec, ALCDUMMY_PWR_MANAG_ADD4, 0x0000);
                break;
        }

	codec->dapm.bias_level = level;

	return 0;
}

/**
 * alcdummy_index_show - Dump private registers.
 * @dev: codec device.
 * @attr: device attribute.
 * @buf: buffer for display.
 *
 * To show non-zero values of all private registers.
 *
 * Returns buffer length.
 */
static ssize_t alcdummy_index_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct alcdummy_priv *alcdummy = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = alcdummy->codec;
	unsigned int val;
	int cnt = 0, i;

	cnt += sprintf(buf, "ALCDUMMY index register\n");
#if 0
	for (i = 0; i <= 0x23; i++) {
		if (cnt + 9 >= PAGE_SIZE - 1)
			break;
		val = alcdummy_index_read(codec, i);
		if (!val)
			continue;
		cnt += snprintf(buf + cnt, 10, "%02x: %04x\n", i, val);
	}

	if (cnt >= PAGE_SIZE)
		cnt = PAGE_SIZE - 1;

#endif
	return cnt;
}
static DEVICE_ATTR(index_reg, 0444, alcdummy_index_show, NULL);

static ssize_t show_audiopath (struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t set_audiopath (struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static DEVICE_ATTR(audiopath, S_IRWXUGO, show_audiopath, set_audiopath);

static ssize_t show_audiopath(struct device *dev, struct device_attribute *attr, char *buf)
{
	//read register
	buf[0] = audio_dummy_path + '0';
	buf[1] = NULL;

	return 1;
}

struct snd_soc_codec *attr_dummy_codec;

static ssize_t set_audiopath (struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int temp;

	temp = buf[0] - '0';
	temp ^= audio_dummy_path;

	audio_dummy_path = buf[0] - '0';
	if(temp & 0x01)
	{
		if(audio_dummy_path & 0x01)
                	alcdummy_write_mask(attr_dummy_codec, ALCDUMMY_SPK_OUT_VOL, 0x0000, 0x8000);
		else
                        alcdummy_write_mask(attr_dummy_codec, ALCDUMMY_SPK_OUT_VOL, 0x8000, 0x8000);
	}		

        if(temp & 0x02) 
        {       
                if(audio_dummy_path & 0x02)
                        alcdummy_write_mask(attr_dummy_codec, ALCDUMMY_AUXOUT_VOL, 0x0000, 0x8080);
                else
                        alcdummy_write_mask(attr_dummy_codec, ALCDUMMY_AUXOUT_VOL, 0x8080, 0x8080);

        }

        if(temp & 0x04)
        {
                if(audio_dummy_path & 0x04)
                	alcdummy_write_mask(attr_dummy_codec, ALCDUMMY_HP_OUT_VOL, 0x0000, 0x8080);
                else
                	alcdummy_write_mask(attr_dummy_codec, ALCDUMMY_HP_OUT_VOL, 0x8080, 0x8080);
        }

	return count;
}

static struct attribute *tnn_audio_sysfs_entries[] = {
	&dev_attr_audiopath,
	NULL
};

static struct attribute_group tnn_audio_attr_group = {
	.name = NULL,
	.attrs = tnn_audio_sysfs_entries,
};

//int tnn_audio_sysfs_create(struct platform_device *pdev)
int tnn_dummy_sysfs_create(struct device *pdev)
{
	//return sysfs_create_group(&pdev->dev.kobj, &tnn_audio_attr_group);
	return sysfs_create_group(&pdev->kobj, &tnn_audio_attr_group);
}

//void tnn_audio_sysfs_remove(struct platform_device *pdev)
void tnn_dummy_sysfs_remove(struct device *pdev)
{
        //sysfs_remove_group(&pdev->dev.kobj, &tnn_audio_attr_group);
        sysfs_remove_group(&pdev->kobj, &tnn_audio_attr_group);
}

static int alcdummy_probe(struct snd_soc_codec *codec)
{
	struct alcdummy_priv *alcdummy = snd_soc_codec_get_drvdata(codec);
	//unsigned int val;
	int ret;

	pr_info("Codec driver version %s\n", VERSION);
	//printk("%s\n", __func__);

	ret = snd_soc_codec_set_cache_io(codec, 8, 16, SND_SOC_I2C);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}

	alcdummy_reset(codec);
	alcdummy_write_mask(codec, ALCDUMMY_PWR_MANAG_ADD3,
		PWR_VREF | PWR_MAIN_BIAS,
		PWR_VREF | PWR_MAIN_BIAS);
	msleep(110);
	alcdummy_write_mask(codec, ALCDUMMY_PWR_MANAG_ADD3,
		PWR_DIS_FAST_VREF,PWR_DIS_FAST_VREF);	

      //printk("call alcdummy_reg_init\n");
	alcdummy_reg_init(codec);

#if 0
#ifndef	VMID_ADD_WIDGET
	alcdummy_setup(codec);
#endif
#endif

	codec->dapm.bias_level = SND_SOC_BIAS_STANDBY;
	alcdummy->codec = codec;

	attr_dummy_codec = codec;

#if 0
	ret = device_create_file(codec->dev, &dev_attr_index_reg);
 	if (ret < 0) {
 		dev_err(codec->dev,
			"Failed to create index_reg sysfs files: %d\n", ret);
		return ret;
	}
#endif
//	alcdummy_amp_en();

	if(tnn_dummy_sysfs_create(codec->dev)) {
 		dev_err(codec->dev,
			"Failed to create tnn_audio sysfs files.\n");
	}

	return 0;
}

static int alcdummy_remove(struct snd_soc_codec *codec)
{
	tnn_dummy_sysfs_remove(codec->dev);
	alcdummy_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

#ifdef CONFIG_PM
static int alcdummy_suspend(struct snd_soc_codec *codec)
{
	alcdummy_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static int alcdummy_resume(struct snd_soc_codec *codec)
{
	alcdummy_write_mask(codec, ALCDUMMY_PWR_MANAG_ADD3,
		PWR_VREF | PWR_MAIN_BIAS,
		PWR_VREF | PWR_MAIN_BIAS);
	msleep(110);
	alcdummy_reg_init(codec);
	alcdummy_write_mask(codec, ALCDUMMY_PWR_MANAG_ADD1, 0x81C0, 0x81C0);

	alcdummy_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	return 0;
}
#else
#define alcdummy_suspend NULL
#define alcdummy_resume NULL
#endif

#define ALCDUMMY_STEREO_RATES SNDRV_PCM_RATE_8000_96000
#define ALCDUMMY_VOICE_RATES (SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_8000)
#define ALCDUMMY_FORMAT	(SNDRV_PCM_FMTBIT_S16_LE | \
			SNDRV_PCM_FMTBIT_S20_3LE | \
			SNDRV_PCM_FMTBIT_S24_LE | \
			SNDRV_PCM_FMTBIT_S8)

struct snd_soc_dai_ops alcdummy_ops = {
	.hw_params = alcdummy_hifi_pcm_params,
	.set_fmt = alcdummy_hifi_codec_set_dai_fmt,
	.set_sysclk = alcdummy_hifi_codec_set_dai_sysclk,
	.set_pll = alcdummy_codec_set_dai_pll,
};

struct snd_soc_dai_driver alcdummy_dai[] = {
	{
		.name = "alcdummy-hifi",
		.id = ALCDUMMY_AIF1,
		.playback = {
			.stream_name = "HIFI Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ALCDUMMY_STEREO_RATES,
			.formats = ALCDUMMY_FORMAT,
		},
		.capture = {
			.stream_name = "HIFI Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ALCDUMMY_STEREO_RATES,
			.formats = ALCDUMMY_FORMAT,
		},
		.ops = &alcdummy_ops,
	},
#if 0
	{
		.name = "alcdummy-reserve",
		.id = ALCDUMMY_AIF2,
	}
#endif
#if 1
	{
		.name = "alcdummy-voice",
		.id = ALCDUMMY_AIF2,
		.playback = {
			.stream_name = "Mono Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ALCDUMMY_VOICE_RATES,
			.formats = ALCDUMMY_FORMAT,
		},
		.capture = {
			.stream_name = "Voice HIFI Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = ALCDUMMY_VOICE_RATES,
			.formats = ALCDUMMY_FORMAT,
		},
		.ops = &alcdummy_ops,
	},
#endif
};

static struct snd_soc_codec_driver soc_codec_dev_alcdummy = {
	.probe = alcdummy_probe,
	.remove = alcdummy_remove,
	.suspend = alcdummy_suspend,
	.resume = alcdummy_resume,

	.set_bias_level = alcdummy_set_bias_level,
	.reg_cache_size = ARRAY_SIZE(alcdummy_reg),
	.reg_word_size = sizeof(u16),
	.reg_cache_default = alcdummy_reg,

	//.volatile_register = alcdummy_volatile_register,
	//.readable_register = alcdummy_readable_register,
	//.reg_cache_step = 1,

	.controls = alcdummy_snd_controls,
	.num_controls = ARRAY_SIZE(alcdummy_snd_controls),
	.dapm_widgets = alcdummy_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(alcdummy_dapm_widgets),
	.dapm_routes = alcdummy_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(alcdummy_dapm_routes),
};

static const struct i2c_device_id alcdummy_i2c_id[] = {
	{ "alcdummy", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, alcdummy_i2c_id);

static int alcdummy_i2c_probe(struct i2c_client *i2c,
		    const struct i2c_device_id *id)
{
	struct alcdummy_priv *alcdummy;
	int ret;

	alcdummy = kzalloc(sizeof(struct alcdummy_priv), GFP_KERNEL);
	if (NULL == alcdummy)
		return -ENOMEM;

	alcdummy->client = i2c;
	i2c_set_clientdata(i2c, alcdummy);

	ret = snd_soc_register_codec(&i2c->dev, &soc_codec_dev_alcdummy,
			alcdummy_dai, ARRAY_SIZE(alcdummy_dai));

        if (ret < 0) {
                kfree(alcdummy);
                printk("failed to initialise alcdummy!\n");
        }

	return ret;
}

static __devexit int alcdummy_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);
	kfree(i2c_get_clientdata(client));
	return 0;
}

static void alcdummy_i2c_shutdown(struct i2c_client *client)
{
	struct alcdummy_priv *alcdummy = i2c_get_clientdata(client);
	struct snd_soc_codec *codec = alcdummy->codec;

	if (codec != NULL)
		alcdummy_set_bias_level(codec, SND_SOC_BIAS_OFF);
}

struct i2c_driver alcdummy_i2c_driver = {
	.driver = {
		.name = "alcdummy",
		.owner = THIS_MODULE,
	},
	.probe = alcdummy_i2c_probe,
	.remove = __devexit_p(alcdummy_i2c_remove),
	.shutdown = alcdummy_i2c_shutdown,
	.id_table = alcdummy_i2c_id,
};

static int __init alcdummy_modinit(void)
{
        int ret;

        //printk(KERN_DEBUG "enter %s\n", __func__);
        //printk("enter %s\n", __func__);
        ret = i2c_add_driver(&alcdummy_i2c_driver);
        if (ret != 0) {
                printk(KERN_ERR "Failed to register ALCDUMMY I2C driver: %d\n", ret);
        }

        return ret;

}
module_init(alcdummy_modinit);

static void __exit alcdummy_modexit(void)
{
	i2c_del_driver(&alcdummy_i2c_driver);
}
module_exit(alcdummy_modexit);

MODULE_DESCRIPTION("ASoC ALCDUMMY driver");
MODULE_AUTHOR("flove <flove@realtek.com>");
MODULE_LICENSE("GPL");
