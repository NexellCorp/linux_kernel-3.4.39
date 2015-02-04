/*
 * rt5623.c  --  RT5623 ALSA SoC audio codec driver
 *
 * Copyright 2011 Realtek Semiconductor Corp.
 * Author: Johnny Hsu <johnnyhsu@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include "rt5623_fdone.h"

/*
#define VMID_ADD_WIDGET
*/

#define RT5623_DEMO 1 /* only for demo; please remove it */

struct rt5623_priv {
	unsigned int sysclk;
};

static const u16 rt5623_reg[RT5623_VENDOR_ID2 + 1] = {
	[RT5623_RESET] = 0x59b4,
	[RT5623_SPK_OUT_VOL] = 0x8080,
	[RT5623_HP_OUT_VOL] = 0x8080,
	[RT5623_MONO_AUX_OUT_VOL] = 0x8080,
	[RT5623_AUXIN_VOL] = 0xe808,
	[RT5623_LINE_IN_VOL] = 0xe808,
	[RT5623_STEREO_DAC_VOL] = 0xe808,
	[RT5623_MIC_VOL] = 0x0808,
	[RT5623_MIC_ROUTING_CTRL] = 0xe0e0,
	[RT5623_ADC_REC_GAIN] = 0xf58b,
	[RT5623_ADC_REC_MIXER] = 0x7f7f,
	[RT5623_SOFT_VOL_CTRL_TIME] = 0x000a,
	[RT5623_OUTPUT_MIXER_CTRL] = 0xd000,
	[RT5623_AUDIO_INTERFACE] = 0x8000,
	[RT5623_STEREO_AD_DA_CLK_CTRL] = 0x166d,
	[RT5623_ADD_CTRL_REG] = 0x5300,
	[RT5623_GPIO_PIN_CONFIG] = 0x1c0e,
	[RT5623_GPIO_PIN_POLARITY] = 0x1c0e,
	[RT5623_GPIO_PIN_STATUS] = 0x0002,
 	[RT5623_OVER_TEMP_CURR_STATUS] = 0x003c,
 	[RT5623_PSEDUEO_SPATIAL_CTRL] = 0x0497,
 	[RT5623_AVC_CTRL] = 0x000b,
 	[RT5623_VENDOR_ID1] = 0x10ec,
	[RT5623_VENDOR_ID2] = 0x2003,
};

#ifdef RT5623_DEMO
struct rt5623_reg {
	u8 reg_index;
	u16 reg_value;
};

static struct rt5623_reg init_data[] = {
	{RT5623_SPK_OUT_VOL			, 0x8080},    //default speaker volume
	{RT5623_HP_OUT_VOL			, 0xc0c0},    //default HP volume	
	{RT5623_MONO_AUX_OUT_VOL	, 0x4444},    //default Mono Aux out volume
	{RT5623_AUXIN_VOL			, 0xe808},    //default Aux in volume
	{RT5623_LINE_IN_VOL			, 0x0505},    //default line in volume
	{RT5623_STEREO_DAC_VOL		, 0x6000},    //default stereo DAC volume
	{RT5623_MIC_VOL				, 0xe808},    //default MIC volume
	{RT5623_MIC_ROUTING_CTRL	, 0xe0e0},    //default MIC routing control
	{RT5623_ADC_REC_GAIN		, 0xf58b},    //default ADC Record
	{RT5623_ADC_REC_MIXER		, 0x7f7f},    //default ADC Record Mixer control
	{RT5623_SOFT_VOL_CTRL_TIME	, 0x0000},    //default Avol soft voulume time
	{RT5623_OUTPUT_MIXER_CTRL	, 0x0740},    //default Output Mixer Control
	{RT5623_MIC_CTRL			, 0x0000},    //default Microphone
	{RT5623_AUDIO_INTERFACE		, 0x8000},    //Digital Audio Interface Control
	{RT5623_STEREO_AD_DA_CLK_CTRL	, 0x066d},    //set Dac filter to 256fs
	{RT5623_COMPANDING_CTRL		, 0x0000},    //Reg-38h: Companding control
	{RT5623_PWR_MANAG_ADD1		, 0xc144},    //Reg-3ah: Power Management Addition 1
	{RT5623_PWR_MANAG_ADD2		, 0xb7ff},    //Reg-3ch: Power Management Addition 2 
	{RT5623_PWR_MANAG_ADD3		, 0xf6ff},    //Reg-3eh: Power Management Addition 3
	{RT5623_ADD_CTRL_REG		, 0x5300},    //Class AB/D speaker ratio is 1VDD
	{RT5623_GLOBAL_CLK_CTRL_REG	, 0x0000},    //Reg-42h: Global Clock Control Register
	{RT5623_PLL_CTRL			, 0x0000},    //Reg-44h: PLL Control Register
	{RT5623_EQ_CTRL				, 0x0000},    //Reg-62h: EQ Control
	{RT5623_EQ_MODE_ENABLE		, 0x0000},    //Reg-66h: EQ Mode Change Enable Control
	{RT5623_AVC_CTRL			, 0x0000},    //Reg-68h: AVC Control
//	{RT5623_HID_CTRL_INDEX		, 0x0011}, {RT5623_HID_CTRL_DATA		, 0x0000},    //EQ Input Volume
//	{RT5623_HID_CTRL_INDEX		, 0x0012}, {RT5623_HID_CTRL_DATA		, 0x0002},    //EQ Output Volume
	{RT5623_EQ_CTRL				, 0x8000},    //Reg-62h: EQ Control
	{RT5623_EQ_MODE_ENABLE		, 0x001f},    //Reg-66h: EQ Mode Change Enable Control
	{RT5623_EQ_MODE_ENABLE		, 0x0000},    //Reg-66h: EQ Mode Change Enable Control

};
#define RT5623_INIT_REG_NUM ARRAY_SIZE(init_data)

static int rt5623_reg_init(struct snd_soc_codec *codec)
{
	int i;

	for (i = 0; i < RT5623_INIT_REG_NUM; i++)
		snd_soc_write(codec, init_data[i].reg_index,
				init_data[i].reg_value);

	return 0;
}
#endif

static int rt5623_reset(struct snd_soc_codec *codec)
{
	return snd_soc_write(codec, RT5623_RESET, 0);
}

static int rt5623_volatile_register(
	struct snd_soc_codec *codec, unsigned int reg)
{
	switch (reg) {
	case RT5623_RESET:
	case RT5623_HID_CTRL_DATA:
	case RT5623_GPIO_PIN_STATUS:
	case RT5623_OVER_TEMP_CURR_STATUS:
		return 1;
	default:
		return 0;
	}
}

static int rt5623_readable_register(
	struct snd_soc_codec *codec, unsigned int reg)
{
	switch (reg) {
	case RT5623_RESET:
	case RT5623_SPK_OUT_VOL:
	case RT5623_HP_OUT_VOL:
	case RT5623_MONO_AUX_OUT_VOL:
	case RT5623_AUXIN_VOL:
	case RT5623_LINE_IN_VOL:
	case RT5623_STEREO_DAC_VOL:
	case RT5623_MIC_VOL:
	case RT5623_MIC_ROUTING_CTRL:
	case RT5623_ADC_REC_GAIN:
	case RT5623_ADC_REC_MIXER:
	case RT5623_SOFT_VOL_CTRL_TIME:
	case RT5623_OUTPUT_MIXER_CTRL:
	case RT5623_MIC_CTRL:
	case RT5623_AUDIO_INTERFACE:
	case RT5623_STEREO_AD_DA_CLK_CTRL:
	case RT5623_COMPANDING_CTRL:
	case RT5623_PWR_MANAG_ADD1:
	case RT5623_PWR_MANAG_ADD2:
	case RT5623_PWR_MANAG_ADD3:
	case RT5623_ADD_CTRL_REG:
	case RT5623_GLOBAL_CLK_CTRL_REG:
	case RT5623_PLL_CTRL:
	case RT5623_GPIO_OUTPUT_PIN_CTRL:
	case RT5623_GPIO_PIN_CONFIG:
	case RT5623_GPIO_PIN_POLARITY:
	case RT5623_GPIO_PIN_STICKY:
	case RT5623_GPIO_PIN_WAKEUP:
	case RT5623_GPIO_PIN_STATUS:
	case RT5623_GPIO_PIN_SHARING:
	case RT5623_OVER_TEMP_CURR_STATUS:
	case RT5623_JACK_DET_CTRL:
	case RT5623_MISC_CTRL:
	case RT5623_PSEDUEO_SPATIAL_CTRL:
	case RT5623_EQ_CTRL:
	case RT5623_EQ_MODE_ENABLE:
	case RT5623_AVC_CTRL:
	case RT5623_HID_CTRL_INDEX:
	case RT5623_HID_CTRL_DATA:
	case RT5623_VENDOR_ID1:
	case RT5623_VENDOR_ID2:
		return 1;
	default:
		return 0;
	}
}


static const char *rt5623_spkn_src[] = {"RN", "RP", "LN"};

static const SOC_ENUM_SINGLE_DECL(rt5625_spkn_src_enum,
	RT5623_OUTPUT_MIXER_CTRL, RT5623_SPKOUT_N_SOUR_SFT, rt5623_spkn_src);

static const char *rt5623_amp_type_sel[] = {"Class AB","Class D"};

static const SOC_ENUM_SINGLE_DECL(
	rt5623_amp_type_enum, RT5623_OUTPUT_MIXER_CTRL,
	RT5623_SPK_OUTPUT_CLASS_SFT, rt5623_amp_type_sel);

//static const char *rt5623_mic_boost_sel[] = {"Bypass","20db","30db","40db"};

//static const struct soc_enum rt5623_enum[] = {
//SOC_ENUM_SINGLE(RT5623_OUTPUT_MIXER_CTRL, 14, 3, rt5623_spkn_source_sel), /* spkn source from hp mixer */	
//SOC_ENUM_SINGLE(RT5623_OUTPUT_MIXER_CTRL, 10, 4, rt5623_spk_pga), /* spk input sel 1 */	
//SOC_ENUM_SINGLE(RT5623_OUTPUT_MIXER_CTRL, 9, 2, rt5623_hpl_pga), /* hp left input sel 2 */	
//SOC_ENUM_SINGLE(RT5623_OUTPUT_MIXER_CTRL, 8, 2, rt5623_hpr_pga), /* hp right input sel 3 */	
//SOC_ENUM_SINGLE(RT5623_OUTPUT_MIXER_CTRL, 6, 4, rt5623_mono_pga), /* mono input sel 4 */
//SOC_ENUM_SINGLE(RT5623_MIC_CTRL, 10,4, rt5623_mic_boost_sel), /*Mic1 boost sel 5 */
//SOC_ENUM_SINGLE(RT5623_MIC_CTRL, 8,4,rt5623_mic_boost_sel), /*Mic2 boost sel 6 */
//SOC_ENUM_SINGLE(RT5623_OUTPUT_MIXER_CTRL,13,2,rt5623_amp_type_sel), /*Speaker AMP sel 7 */
//};

static int rt5623_amp_sel_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned short val;
	unsigned short mask, bitmask;

	for (bitmask = 1; bitmask < e->max; bitmask <<= 1)
		;
	if (ucontrol->value.enumerated.item[0] > e->max - 1)
		return -EINVAL;
	val = ucontrol->value.enumerated.item[0] << e->shift_l;
	mask = (bitmask - 1) << e->shift_l;
	if (e->shift_l != e->shift_r) {
		if (ucontrol->value.enumerated.item[1] > e->max - 1)
			return -EINVAL;
		val |= ucontrol->value.enumerated.item[1] << e->shift_r;
		mask |= (bitmask - 1) << e->shift_r;
	}

	snd_soc_update_bits(codec, e->reg, mask, val);
	val &= (0x1 << 13);
	if (val == 0) {
		 snd_soc_update_bits(codec, 0x3c, 0x0000, 0x4000);       /*power off classd*/
		 snd_soc_update_bits(codec, 0x3c, 0x8000, 0x8000);       /*power on classab*/
	} else {
	 	 snd_soc_update_bits(codec, 0x3c, 0x0000, 0x8000);       /*power off classab*/
		 snd_soc_update_bits(codec, 0x3c, 0x4000, 0x4000);       /*power on classd*/
	}

	return 0;
}


static const DECLARE_TLV_DB_SCALE(out_vol_tlv, -4650, 150, 0);
static const DECLARE_TLV_DB_SCALE(dac_vol_tlv, -3450, 150, 0);
static const DECLARE_TLV_DB_SCALE(in_vol_tlv, -3450, 150, 0);
static const DECLARE_TLV_DB_SCALE(adc_vol_tlv, -1650, 150, 0);
/* {0, +20, +30, +40} dB */
static unsigned int mic_bst_tlv[] = {
	TLV_DB_RANGE_HEAD(2),
	0, 0, TLV_DB_SCALE_ITEM(0, 0, 0),
	1, 3, TLV_DB_SCALE_ITEM(2000, 1000, 0),
};


static const struct snd_kcontrol_new rt5623_snd_controls[] = {
	SOC_DOUBLE_TLV("Speaker Playback Volume", RT5623_SPK_OUT_VOL,
		RT5623_L_VOL_SFT, RT5623_R_VOL_SFT, 31, 1, out_vol_tlv),
	SOC_DOUBLE("Speaker Playback Switch", RT5623_SPK_OUT_VOL,
		RT5623_L_MUTE_SFT, RT5623_R_MUTE_SFT, 1, 1),
	SOC_ENUM_EXT("Speaker Amp Type", rt5623_amp_type_enum,
		snd_soc_get_enum_double, rt5623_amp_sel_put),
	SOC_DOUBLE_TLV("Headphone Playback Volume", RT5623_HP_OUT_VOL,
		RT5623_L_VOL_SFT, RT5623_R_VOL_SFT, 31, 1, out_vol_tlv),
	SOC_DOUBLE("Headphone Playback Switch", RT5623_HP_OUT_VOL,
		RT5623_L_MUTE_SFT, RT5623_R_MUTE_SFT, 1, 1),
	SOC_DOUBLE_TLV("AXO Playback Volume", RT5623_MONO_AUX_OUT_VOL,
		RT5623_L_VOL_SFT, RT5623_R_VOL_SFT, 31, 1, out_vol_tlv),
	SOC_DOUBLE("AXO Playback Switch", RT5623_MONO_AUX_OUT_VOL,
		RT5623_L_MUTE_SFT, RT5623_R_MUTE_SFT, 1, 1),
	SOC_DOUBLE_TLV("PCM Playback Volume", RT5623_STEREO_DAC_VOL,
		RT5623_L_VOL_SFT, RT5623_R_VOL_SFT, 31, 1, dac_vol_tlv),
	SOC_DOUBLE_TLV("Line In Volume", RT5623_LINE_IN_VOL,
		RT5623_L_VOL_SFT, RT5623_R_VOL_SFT, 31, 1, in_vol_tlv),
	SOC_SINGLE_TLV("Mic 1 Volume", RT5623_MIC_VOL,
		RT5623_L_VOL_SFT, 31, 1, in_vol_tlv),
	SOC_SINGLE_TLV("Mic 1 Boost", RT5623_MIC_CTRL,
		RT5623_MIC1_BOOST_CTRL_SFT, 3, 0, mic_bst_tlv),
	SOC_SINGLE_TLV("Mic 2 Volume", RT5623_MIC_VOL,
		RT5623_R_VOL_SFT, 31, 1, in_vol_tlv),
	SOC_SINGLE_TLV("MIC2 Boost", RT5623_MIC_CTRL,
		RT5623_MIC2_BOOST_CTRL_SFT, 3, 0, mic_bst_tlv),
	SOC_DOUBLE_TLV("AUX In Volume", RT5623_AUXIN_VOL,
		RT5623_L_VOL_SFT, RT5623_R_VOL_SFT, 31, 1, in_vol_tlv),
	SOC_DOUBLE_TLV("Capture Volume", RT5623_ADC_REC_GAIN,
		RT5623_ADCL_VOL_SFT, RT5623_R_VOL_SFT, 31, 0, adc_vol_tlv),
};

#if 1
static const struct snd_kcontrol_new rt5623_adcl_rec_mixer[] = {
	SOC_DAPM_SINGLE("Mic1 Capture Switch", RT5623_ADC_REC_MIXER,
		RT5623_M_MIC1_TO_ADC_L_MIXER_SFT, 1, 1),
	SOC_DAPM_SINGLE("Mic2 Capture Switch", RT5623_ADC_REC_MIXER,
		RT5623_M_MIC2_TO_ADC_L_MIXER_SFT, 1, 1),
	SOC_DAPM_SINGLE("LineInL Capture Switch", RT5623_ADC_REC_MIXER,
		RT5623_M_LINEIN_L_TO_ADC_L_MIXER_SFT, 1, 1),
	SOC_DAPM_SINGLE("AUXIN Capture Switch", RT5623_ADC_REC_MIXER,
		RT5623_M_AUXIN_L_TO_ADC_L_MIXER_SFT, 1, 1),
	SOC_DAPM_SINGLE("HPMixerL Capture Switch", RT5623_ADC_REC_MIXER,
		RT5623_M_HPMIXER_L_TO_ADC_L_MIXER_SFT, 1, 1),
	SOC_DAPM_SINGLE("SPKMixer Capture Switch", RT5623_ADC_REC_MIXER,
		RT5623_M_SPKMIXER_L_TO_ADC_L_MIXER_SFT, 1, 1),
	SOC_DAPM_SINGLE("MonoMixer Capture Switch", RT5623_ADC_REC_MIXER,
		RT5623_M_MONOMIXER_L_TO_ADC_L_MIXER_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5623_adcr_rec_mixer[] = {
	SOC_DAPM_SINGLE("Mic1 Capture Switch", RT5623_ADC_REC_MIXER,
		RT5623_M_MIC1_TO_ADC_R_MIXER_SFT, 1, 1),
	SOC_DAPM_SINGLE("Mic2 Capture Switch", RT5623_ADC_REC_MIXER,
		RT5623_M_MIC2_TO_ADC_R_MIXER_SFT, 1, 1),
	SOC_DAPM_SINGLE("LineInR Capture Switch", RT5623_ADC_REC_MIXER,
		RT5623_M_LINEIN_R_TO_ADC_R_MIXER_SFT, 1, 1),
	SOC_DAPM_SINGLE("AUXIN Capture Switch", RT5623_ADC_REC_MIXER,
		RT5623_M_AUXIN_R_TO_ADC_R_MIXER_SFT, 1, 1),
	SOC_DAPM_SINGLE("HPMixerR Capture Switch", RT5623_ADC_REC_MIXER,
		RT5623_M_HPMIXER_R_TO_ADC_R_MIXER_SFT, 1, 1),
	SOC_DAPM_SINGLE("SPKMixer Capture Switch", RT5623_ADC_REC_MIXER,
		RT5623_M_SPKMIXER_R_TO_ADC_R_MIXER_SFT, 1, 1),
	SOC_DAPM_SINGLE("MonoMixer Capture Switch", RT5623_ADC_REC_MIXER,
		RT5623_M_MONOMIXER_R_TO_ADC_R_MIXER_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5623_hp_mixer[] = {
	SOC_DAPM_SINGLE("LineIn Playback Switch",
		RT5623_LINE_IN_VOL, RT5623_L_MUTE_SFT, 1, 1),
	SOC_DAPM_SINGLE("AUXIN Playback Switch",
		RT5623_AUXIN_VOL, RT5623_L_MUTE_SFT, 1, 1),
	SOC_DAPM_SINGLE("Mic1 Playback Switch",
		RT5623_MIC_ROUTING_CTRL, RT5623_L_MUTE_SFT, 1, 1),
	SOC_DAPM_SINGLE("Mic2 Playback Switch",
		RT5623_MIC_ROUTING_CTRL, RT5623_R_MUTE_SFT, 1, 1),
	SOC_DAPM_SINGLE("PCM Playback Switch",
		RT5623_STEREO_DAC_VOL, RT5623_L_MUTE_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5623_hpl_mixer[] = {
	SOC_DAPM_SINGLE("RecordL Playback Switch",
		RT5623_ADC_REC_GAIN, RT5623_L_MUTE_SFT, 1,1),
};

static const struct snd_kcontrol_new rt5623_hpr_mixer[] = {
	SOC_DAPM_SINGLE("RecordR Playback Switch", RT5623_ADC_REC_GAIN,
		RT5623_M_ADC_R_TO_HP_MIXER_SFT, 1,1),
};

static const struct snd_kcontrol_new rt5623_speaker_mixer_controls[] = {
	SOC_DAPM_SINGLE("LineIn Playback Switch", RT5623_LINE_IN_VOL,
			RT5623_M_LINEIN_TO_SPKM_SFT, 1, 1),
	SOC_DAPM_SINGLE("AUXIN Playback Switch", RT5623_AUXIN_VOL,
			RT5623_M_AXI_TO_SPKM_SFT, 1, 1),
	SOC_DAPM_SINGLE("Mic1 Playback Switch", RT5623_MIC_ROUTING_CTRL,
			RT5623_M_MIC1_TO_SPK_MIXER_SFT, 1, 1),
	SOC_DAPM_SINGLE("Mic2 Playback Switch", RT5623_MIC_ROUTING_CTRL,
			RT5623_M_MIC2_TO_SPK_MIXER_SFT, 1, 1),
	SOC_DAPM_SINGLE("PCM Playback Switch", RT5623_STEREO_DAC_VOL,
			RT5623_M_DAC_TO_SPKM_SFT, 1, 1),
};

static const struct snd_kcontrol_new rt5623_mono_mixer_controls[] = {
	SOC_DAPM_SINGLE("LineIn Playback Switch", RT5623_LINE_IN_VOL,
			RT5623_M_LINEIN_TO_MOM_SFT, 1, 1),
	SOC_DAPM_SINGLE("Mic1 Playback Switch", RT5623_MIC_ROUTING_CTRL,
			RT5623_M_MIC1_TO_MONO_MIXER_SFT, 1, 1),
	SOC_DAPM_SINGLE("Mic2 Playback Switch", RT5623_MIC_ROUTING_CTRL,
			RT5623_M_MIC2_TO_MONO_MIXER_SFT, 1, 1),
	SOC_DAPM_SINGLE("AUXIN Playback Switch", RT5623_AUXIN_VOL,
			RT5623_M_AXI_TO_SPKM_SFT, 1, 1),
	SOC_DAPM_SINGLE("PCM Playback Switch", RT5623_STEREO_DAC_VOL,
			RT5623_M_DAC_TO_MOM_SFT, 1, 1),
	SOC_DAPM_SINGLE("RecordL Playback Switch", RT5623_ADC_REC_GAIN,
			RT5623_M_ADC_L_TO_MONO_MIXER_SFT, 1,1),
	SOC_DAPM_SINGLE("RecordR Playback Switch", RT5623_ADC_REC_GAIN,
			RT5623_M_ADC_R_TO_MONO_MIXER_SFT, 1,1),
};

static const char *rt5623_spk_vol_src[] = {
	"Vmid", "HP Mixer", "SPK Mixer", "Mono Mixer"};

static const SOC_ENUM_SINGLE_DECL(rt5623_spk_vol_src_enum,
	RT5623_OUTPUT_MIXER_CTRL, RT5623_SPKOUT_INPUT_SEL_SFT, rt5623_spk_vol_src);

static const struct snd_kcontrol_new rt5623_spk_vol_src_mux =
	SOC_DAPM_ENUM("SPK Vol Src Mux", rt5623_spk_vol_src_enum);

static const char *rt5623_hpl_vol_src[]  = {"Vmid", "HPL Mixer"};

static const SOC_ENUM_SINGLE_DECL(
	rt5623_hpl_vol_src_enum, RT5623_OUTPUT_MIXER_CTRL,
	RT5623_HPL_INPUT_SEL_HPLMIXER_SFT, rt5623_hpl_vol_src);

static const struct snd_kcontrol_new rt5623_hpl_vol_src_mux =
	SOC_DAPM_ENUM("HPL Vol Src Mux", rt5623_hpl_vol_src_enum);

static const char *rt5623_hpr_vol_src[]  = {"Vmid", "HPR Mixer"};

static const SOC_ENUM_SINGLE_DECL(
	rt5623_hpr_vol_src_enum, RT5623_OUTPUT_MIXER_CTRL,
	RT5623_HPR_INPUT_SEL_HPRMIXER_SFT, rt5623_hpr_vol_src);

static const struct snd_kcontrol_new rt5623_hpr_vol_src_mux =
	SOC_DAPM_ENUM("HPR Vol Src Mux", rt5623_hpr_vol_src_enum);

static const char *rt5623_mono_vol_src[] = {
	"Vmid", "HP Mixer", "SPK Mixer", "Mono Mixer"};

static const SOC_ENUM_SINGLE_DECL(
	rt5623_mono_vol_src_enum, RT5623_OUTPUT_MIXER_CTRL,
	RT5623_MONO_AUX_INPUT_SEL_SFT, rt5623_mono_vol_src);

static const struct snd_kcontrol_new rt5623_mono_vol_src_mux =
	SOC_DAPM_ENUM("Mono Vol Src Mux", rt5623_mono_vol_src_enum);

static int check_sclk_source(struct snd_soc_dapm_widget *source,
			 struct snd_soc_dapm_widget *sink)
{
	unsigned int val;

	val = snd_soc_read(source->codec, RT5623_GLOBAL_CLK_CTRL_REG);
	val &= RT5623_SYSCLK_SOUR_SEL_MASK;
	return (val == RT5623_SYSCLK_SOUR_SEL_PLL);
}

static int check_class_ab(struct snd_soc_dapm_widget *source,
			 struct snd_soc_dapm_widget *sink)
{
	unsigned int val;

	val = snd_soc_read(source->codec, RT5623_OUTPUT_MIXER_CTRL);
	val &= RT5623_SPK_OUTPUT_CLASS_MASK;
	return (val == RT5623_SPK_OUTPUT_CLASS_AB);
}

static int check_class_d(struct snd_soc_dapm_widget *source,
			 struct snd_soc_dapm_widget *sink)
{
	unsigned int val;

	val = snd_soc_read(source->codec, RT5623_OUTPUT_MIXER_CTRL);
	val &= RT5623_SPK_OUTPUT_CLASS_MASK;
	return (val == RT5623_SPK_OUTPUT_CLASS_D);
}

static int rt5623_mono_hpmix_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	unsigned int val;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMD:
		val = snd_soc_read(codec, RT5623_ADC_REC_GAIN);
		if (val & RT5623_M_ADC_L_TO_HP_MIXER)
			snd_soc_update_bits(codec, RT5623_PWR_MANAG_ADD2,
					RT5623_PWR_L_HP_MIXER, 0);
		if (val & RT5623_M_ADC_R_TO_HP_MIXER)
			snd_soc_update_bits(codec, RT5623_PWR_MANAG_ADD2,
					RT5623_PWR_R_HP_MIXER, 0);
		break;

	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5623_PWR_MANAG_ADD2,
			RT5623_PWR_L_HP_MIXER | RT5623_PWR_R_HP_MIXER,
			RT5623_PWR_L_HP_MIXER | RT5623_PWR_R_HP_MIXER);
		break;

	default:
		return 0;
	}

	return 0;
}

static int rt5623_hpmix_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	unsigned int val, val1, val2, val3, bmap;

	if (!strcmp(w->name, "Left HP Mixer"))
		bmap = RT5623_PWR_L_HP_MIXER;
	else
		bmap = RT5623_PWR_R_HP_MIXER;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMD:
		val = snd_soc_read(codec, RT5623_LINE_IN_VOL);
		val1 = snd_soc_read(codec, RT5623_AUXIN_VOL);
		val2 = snd_soc_read(codec, RT5623_MIC_ROUTING_CTRL);
		val3 = snd_soc_read(codec, RT5623_STEREO_DAC_VOL);
		if ((val & RT5623_L_MUTE) && (val1 & RT5623_L_MUTE) &&
			(val2 & RT5623_L_MUTE) && (val2 & RT5623_R_MUTE) &&
			(val3 & RT5623_L_MUTE))
			snd_soc_update_bits(codec,
				RT5623_PWR_MANAG_ADD2, bmap, 0);
		break;

	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5623_PWR_MANAG_ADD2, bmap, bmap);
		break;

	default:
		return 0;
	}

	return 0;
}

/*static int rt5623_hpmix_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, RT5623_PWR_MANAG_ADD2,
			PWR_L_HP_MIXER | PWR_R_HP_MIXER, 0);
		break;

	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, RT5623_PWR_MANAG_ADD2,
			PWR_L_HP_MIXER | PWR_R_HP_MIXER,
			PWR_L_HP_MIXER | PWR_R_HP_MIXER);
		break;

	default:
		return 0;
	}

	return 0;
}*/

static void hp_depop_mode2(struct snd_soc_codec *codec)
{
	snd_soc_update_bits(codec, RT5623_PWR_MANAG_ADD3,
		RT5623_PWR_MAIN_BIAS, RT5623_PWR_MAIN_BIAS);
	snd_soc_update_bits(codec, RT5623_HP_OUT_VOL,
		RT5623_L_MUTE | RT5623_R_MUTE,
		RT5623_L_MUTE | RT5623_R_MUTE);
	snd_soc_update_bits(codec, RT5623_PWR_MANAG_ADD1,
		RT5623_PWR_SOFTGEN_EN, RT5623_PWR_SOFTGEN_EN);
	snd_soc_update_bits(codec, RT5623_PWR_MANAG_ADD2,
		RT5623_PWR_VREF, RT5623_PWR_VREF);
	snd_soc_update_bits(codec, RT5623_PWR_MANAG_ADD3,
		RT5623_PWR_HP_L_OUT_VOL | RT5623_PWR_HP_R_OUT_VOL,
		RT5623_PWR_HP_L_OUT_VOL | RT5623_PWR_HP_R_OUT_VOL);
	snd_soc_update_bits(codec, RT5623_MISC_CTRL,
		RT5623_HP_DEPOP_MODE2_EN,
		RT5623_HP_DEPOP_MODE2_EN);
	msleep(300);
}

static int rt5623_hp_event(struct snd_soc_dapm_widget *w, 
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		hp_depop_mode2(codec);
		snd_soc_update_bits(codec, RT5623_HP_OUT_VOL,
			RT5623_L_MUTE | RT5623_R_MUTE, 0);
		snd_soc_update_bits(codec, RT5623_PWR_MANAG_ADD1,
			RT5623_PWR_HP_OUT_AMP, RT5623_PWR_HP_OUT_AMP);
		break;

	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, RT5623_HP_OUT_VOL,
			RT5623_L_MUTE | RT5623_R_MUTE,
			RT5623_L_MUTE | RT5623_R_MUTE);
		//snd_soc_update_bits(codec, RT5623_PWR_MANAG_ADD1,
		//	RT5623_PWR_HP_OUT_ENH_AMP, 0);
		snd_soc_update_bits(codec, RT5623_PWR_MANAG_ADD1,
			RT5623_PWR_HP_OUT_AMP, 0);
		snd_soc_update_bits(codec, RT5623_PWR_MANAG_ADD1,
			RT5623_PWR_SOFTGEN_EN, 0);
		snd_soc_update_bits(codec, RT5623_PWR_MANAG_ADD3,
			RT5623_PWR_HP_L_OUT_VOL |
			RT5623_PWR_HP_R_OUT_VOL, 0);
		break;	
 
	default:
		break;
	}

	return 0;
}

void aux_depop_mode2(struct snd_soc_codec *codec)
{
	snd_soc_update_bits(codec, RT5623_PWR_MANAG_ADD3,
		RT5623_PWR_MAIN_BIAS, RT5623_PWR_MAIN_BIAS);
	snd_soc_update_bits(codec, RT5623_MONO_AUX_OUT_VOL,
		RT5623_L_MUTE | RT5623_R_MUTE,
		RT5623_L_MUTE | RT5623_R_MUTE);
	snd_soc_update_bits(codec, RT5623_PWR_MANAG_ADD1,
		RT5623_PWR_SOFTGEN_EN, RT5623_PWR_SOFTGEN_EN);
	snd_soc_update_bits(codec, RT5623_PWR_MANAG_ADD2,
		RT5623_PWR_VREF, RT5623_PWR_VREF);
	snd_soc_update_bits(codec, RT5623_PWR_MANAG_ADD3,
		RT5623_PWR_AUXOUT_L_VOL_AMP | RT5623_PWR_AUXOUT_R_VOL_AMP,
		RT5623_PWR_AUXOUT_L_VOL_AMP | RT5623_PWR_AUXOUT_R_VOL_AMP);
	snd_soc_update_bits(codec, RT5623_MISC_CTRL,
		RT5623_AUXOUT_DEPOP_MODE2_EN, RT5623_AUXOUT_DEPOP_MODE2_EN);
	msleep(300);
	snd_soc_update_bits(codec, RT5623_PWR_MANAG_ADD1,
		RT5623_PWR_AUX_OUT_AMP, RT5623_PWR_AUX_OUT_AMP);
	snd_soc_update_bits(codec, RT5623_PWR_MANAG_ADD1,
		RT5623_PWR_AUX_OUT_ENH_AMP, RT5623_PWR_AUX_OUT_ENH_AMP);
}

static int rt5623_aux_event(struct snd_soc_dapm_widget *w, 
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		aux_depop_mode2(codec);
		snd_soc_update_bits(codec, RT5623_MONO_AUX_OUT_VOL,
				RT5623_L_MUTE | RT5623_R_MUTE, 0);
		break;

	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, RT5623_MONO_AUX_OUT_VOL,
			RT5623_L_MUTE | RT5623_R_MUTE,
			RT5623_L_MUTE | RT5623_R_MUTE);
		snd_soc_update_bits(codec, RT5623_PWR_MANAG_ADD1,
			RT5623_PWR_AUX_OUT_ENH_AMP, 0);
		snd_soc_update_bits(codec, RT5623_PWR_MANAG_ADD1,
			RT5623_PWR_AUX_OUT_AMP, 0);
		snd_soc_update_bits(codec, RT5623_PWR_MANAG_ADD1,
			RT5623_PWR_SOFTGEN_EN, 0);
		snd_soc_update_bits(codec, RT5623_PWR_MANAG_ADD3,
			RT5623_PWR_AUXOUT_L_VOL_AMP |
			RT5623_PWR_AUXOUT_R_VOL_AMP, 0);
		break;

	default:
		break;
	}

	return 0;
}

static const struct snd_soc_dapm_widget rt5623_dapm_widgets[] = {
	/* supply */
	SND_SOC_DAPM_SUPPLY("IIS Interface", RT5623_PWR_MANAG_ADD1,
			RT5623_PWR_MAIN_I2S_EN_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("PLL", RT5623_PWR_MANAG_ADD2,
			RT5623_PWR_PLL_BIT, 0, NULL, 0),
#ifdef VMID_ADD_WIDGET	
	SND_SOC_DAPM_VMID("Vmid"),
#endif
	SND_SOC_DAPM_SUPPLY("DAC Ref", RT5623_PWR_MANAG_ADD2,
			RT5623_PWR_DAC_REF_CIR_BIT, 0, NULL, 0),

	SND_SOC_DAPM_MICBIAS("Mic Bias1", RT5623_PWR_MANAG_ADD1,
			RT5623_PWR_MIC1_BIAS_EN_BIT, 0),

	SND_SOC_DAPM_INPUT("LINEL"),
	SND_SOC_DAPM_INPUT("LINER"),
	SND_SOC_DAPM_INPUT("AUXINL"),
	SND_SOC_DAPM_INPUT("AUXINR"),
	SND_SOC_DAPM_INPUT("Mic1"),
	SND_SOC_DAPM_INPUT("Mic2"),

	SND_SOC_DAPM_PGA("Mic1 Boost", RT5623_PWR_MANAG_ADD3,
		RT5623_PWR_MIC1_BOOST_MIXER_BIT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Mic2 Boost", RT5623_PWR_MANAG_ADD3,
		RT5623_PWR_MIC2_BOOST_MIXER_BIT, 0, NULL, 0),

	SND_SOC_DAPM_MIXER("Left Record Mixer", RT5623_PWR_MANAG_ADD2,
		RT5623_PWR_L_ADC_REC_MIXER_BIT, 0, rt5623_adcl_rec_mixer,
		ARRAY_SIZE(rt5623_adcl_rec_mixer)),
	SND_SOC_DAPM_MIXER("Right Record Mixer", RT5623_PWR_MANAG_ADD2,
		RT5623_PWR_R_ADC_REC_MIXER_BIT, 0, rt5623_adcr_rec_mixer,
		ARRAY_SIZE(rt5623_adcr_rec_mixer)),	

	SND_SOC_DAPM_ADC("Left ADC", "HiFi Capture", RT5623_PWR_MANAG_ADD2,
				RT5623_PWR_L_ADC_CLK_GAIN_BIT, 0),
	SND_SOC_DAPM_ADC("Right ADC", "HiFi Capture", RT5623_PWR_MANAG_ADD2,
				RT5623_PWR_R_ADC_CLK_GAIN_BIT, 0),

	SND_SOC_DAPM_DAC("Left DAC", "HiFi Playback", RT5623_PWR_MANAG_ADD2,
					RT5623_PWR_L_DAC_CLK_BIT, 0),
	SND_SOC_DAPM_DAC("Right DAC", "HiFi Playback", RT5623_PWR_MANAG_ADD2,
					RT5623_PWR_R_DAC_CLK_BIT, 0),

	SND_SOC_DAPM_PGA("Mic1 Volume", RT5623_PWR_MANAG_ADD3,
		RT5623_PWR_MIC1_FUN_CTRL_BIT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Mic2 Volume", RT5623_PWR_MANAG_ADD3,
		RT5623_PWR_MIC2_FUN_CTRL_BIT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Left LineIn Volume", RT5623_PWR_MANAG_ADD3,
		RT5623_PWR_LINEIN_L_VOL_BIT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Right LineIn Volume", RT5623_PWR_MANAG_ADD3,
		RT5623_PWR_LINEIN_R_VOL_BIT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Left AXI Volume", RT5623_PWR_MANAG_ADD3,
		RT5623_PWR_AUXIN_L_VOL_BIT, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Right AXI Volume", RT5623_PWR_MANAG_ADD3,
		RT5623_PWR_AUXIN_R_VOL_BIT, 0, NULL, 0),

	SND_SOC_DAPM_MIXER("Mono Mixer", RT5623_PWR_MANAG_ADD2,
		RT5623_PWR_MONO_MIXER_BIT, 0, rt5623_mono_mixer_controls,
		ARRAY_SIZE(rt5623_mono_mixer_controls)),
#if 1
	SND_SOC_DAPM_MIXER_E("Mono HP Mixer", SND_SOC_NOPM, 0, 0,
		rt5623_hp_mixer, ARRAY_SIZE(rt5623_hp_mixer),
		rt5623_mono_hpmix_event, SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MIXER_E("Left STO HP Mixer", SND_SOC_NOPM, 0, 0,
		rt5623_hpl_mixer, ARRAY_SIZE(rt5623_hpl_mixer),
		rt5623_hpmix_event, SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MIXER_E("Right STO HP Mixer", SND_SOC_NOPM, 0, 0,
		rt5623_hpr_mixer, ARRAY_SIZE(rt5623_hpr_mixer),
		rt5623_hpmix_event, SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMU),
#else
	SND_SOC_DAPM_MIXER("Mono HP Mixer", SND_SOC_NOPM, 0, 0,
		rt5623_hp_mixer, ARRAY_SIZE(rt5623_hp_mixer)),
	SND_SOC_DAPM_MIXER("Left HP Mixer", SND_SOC_NOPM, 0, 0,
		rt5623_hpl_mixer, ARRAY_SIZE(rt5623_hpl_mixer)),
	SND_SOC_DAPM_MIXER("Right HP Mixer", SND_SOC_NOPM, 0, 0,
		rt5623_hpr_mixer, ARRAY_SIZE(rt5623_hpr_mixer)),
#endif
	SND_SOC_DAPM_MIXER("SPK Mixer", RT5623_PWR_MANAG_ADD2,
		RT5623_PWR_SPK_MIXER_BIT, 0, rt5623_speaker_mixer_controls,
		ARRAY_SIZE(rt5623_speaker_mixer_controls)),	

#if 1
	SND_SOC_DAPM_MIXER("Left HP Mixer", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("Right HP Mixer", SND_SOC_NOPM, 0, 0, NULL, 0),
#else
	SND_SOC_DAPM_MIXER_E("HP Mixer", SND_SOC_NOPM, 0, 0, NULL, 0,
		rt5623_hpmix_event, SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMU),
#endif

	SND_SOC_DAPM_MIXER("DAC Mixer", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("Line Mixer", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("AXI Mixer", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MUX("SPK Vol Input Mux", RT5623_PWR_MANAG_ADD3,
		RT5623_PWR_SPK_OUT_BIT, 0, &rt5623_spk_vol_src_mux),
	SND_SOC_DAPM_MUX("HPL Vol Input Mux", SND_SOC_NOPM,
			0, 0, &rt5623_hpl_vol_src_mux),
	SND_SOC_DAPM_MUX("HPR Vol Input Mux", SND_SOC_NOPM,
			0, 0, &rt5623_hpr_vol_src_mux),
	SND_SOC_DAPM_MUX("AXO Vol Input Mux", SND_SOC_NOPM,
			0, 0, &rt5623_mono_vol_src_mux),

	SND_SOC_DAPM_PGA_E("HP Amp", SND_SOC_NOPM, 0, 0, NULL, 0,
		rt5623_hp_event, SND_SOC_DAPM_POST_PMD |
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA_E("AXO", SND_SOC_NOPM, 0, 0, NULL, 0,
		rt5623_aux_event, SND_SOC_DAPM_POST_PMD |
		SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_SUPPLY("ClassAB Amp", RT5623_PWR_MANAG_ADD2,
			RT5623_PWR_CLASS_AB_BIT, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("ClassD Amp", RT5623_PWR_MANAG_ADD2,
			RT5623_PWR_CLASS_D_BIT, 0, NULL, 0),

	SND_SOC_DAPM_OUTPUT("AUXL"),
	SND_SOC_DAPM_OUTPUT("AUXR"),
	SND_SOC_DAPM_OUTPUT("HPL"),
	SND_SOC_DAPM_OUTPUT("HPR"),
	SND_SOC_DAPM_OUTPUT("SPK"),
};

static const struct snd_soc_dapm_route rt5623_dapm_routes[] = {
	{"DAC Ref", NULL, "IIS Interface"},
	{"DAC Ref", NULL, "PLL", check_sclk_source},

	/* Input */
	{"Mic1 Boost", NULL, "Mic1"},
	{"Mic2 Boost", NULL, "Mic2"},

	{"Left Record Mixer", "Mic1 Capture Switch", "Mic1 Boost"},
	{"Left Record Mixer", "Mic2 Capture Switch", "Mic2 Boost"},
	{"Left Record Mixer", "LineInL Capture Switch", "LINEL"},
	{"Left Record Mixer", "AUXIN Capture Switch", "AUXINL"},
	{"Left Record Mixer", "HPMixerL Capture Switch", "Left HP Mixer"},
	{"Left Record Mixer", "SPKMixer Capture Switch", "SPK Mixer"},
	{"Left Record Mixer", "MonoMixer Capture Switch", "Mono Mixer"},
	
	{"Right Record Mixer", "Mic1 Capture Switch", "Mic1 Boost"},
	{"Right Record Mixer", "Mic2 Capture Switch", "Mic2 Boost"},
	{"Right Record Mixer", "LineInR Capture Switch", "LINER"},
	{"Right Record Mixer", "AUXIN Capture Switch", "AUXINR"},
	{"Right Record Mixer", "HPMixerR Capture Switch", "Right HP Mixer"},
	{"Right Record Mixer", "SPKMixer Capture Switch", "SPK Mixer"},
	{"Right Record Mixer", "MonoMixer Capture Switch", "Mono Mixer"},	

	{"Left ADC", NULL, "DAC Ref"},
	{"Left ADC", NULL, "Left Record Mixer"},
	{"Right ADC", NULL, "DAC Ref"},
	{"Right ADC", NULL, "Right Record Mixer"},

	/* Output */
	{"Left DAC", NULL, "DAC Ref"},
	{"Right DAC", NULL, "DAC Ref"},

	{"Left LineIn Volume", NULL, "LINEL"},
	{"Right LineIn Volume", NULL, "LINER"},
	{"Left AXI Volume", NULL, "AUXINL"},
	{"Right AXI Volume", NULL, "AUXINR"},
	{"Mic1 Volume", NULL, "Mic1 Boost"},
	{"Mic2 Volume", NULL, "Mic2 Boost"},

	{"DAC Mixer", NULL, "Left DAC"},
	{"DAC Mixer", NULL, "Right DAC"},
	{"Line Mixer", NULL, "Left LineIn Volume"},
	{"Line Mixer", NULL, "Right LineIn Volume"},
	{"AXI Mixer", NULL, "Left AXI Volume"},
	{"AXI Mixer", NULL, "Right AXI Volume"},

	{"SPK Mixer", "LineIn Playback Switch", "Line Mixer"},
	{"SPK Mixer", "AUXIN Playback Switch", "AXI Mixer"},
	{"SPK Mixer", "Mic1 Playback Switch", "Mic1 Volume"},
	{"SPK Mixer", "Mic2 Playback Switch", "Mic2 Volume"},
	{"SPK Mixer", "PCM Playback Switch", "DAC Mixer"},

	{"Mono Mixer", "LineIn Playback Switch", "Line Mixer"},
	{"Mono Mixer", "Mic1 Playback Switch"	, "Mic1 Volume"},
	{"Mono Mixer", "Mic2 Playback Switch"	, "Mic2 Volume"},
	{"Mono Mixer", "PCM Playback Switch", "DAC Mixer"},
	{"Mono Mixer", "AUXIN Playback Switch", "AXI Mixer"},
	{"Mono Mixer", "RecordL Playback Switch", "Left Record Mixer"},
	{"Mono Mixer", "RecordR Playback Switch", "Right Record Mixer"},

	{"Mono HP Mixer", "LineIn Playback Switch", "Line Mixer"},
	{"Mono HP Mixer", "AUXIN Playback Switch", "AXI Mixer"},
	{"Mono HP Mixer", "Mic1 Playback Switch", "Mic1 Volume"},
	{"Mono HP Mixer", "Mic2 Playback Switch", "Mic2 Volume"},
	{"Mono HP Mixer", "PCM Playback Switch", "DAC Mixer"},
	{"Left STO HP Mixer", "RecordL Playback Switch", "Left Record Mixer"},
	{"Right STO HP Mixer", "RecordR Playback Switch", "Right Record Mixer"},

#if 1
#ifdef VMID_ADD_WIDGET
	{"Left HP Mixer", NULL, "Mono HP Mixer"},
	{"Left HP Mixer", NULL, "Left STO HP Mixer"},
	{"Right HP Mixer", NULL, "Mono HP Mixer"},
	{"Right HP Mixer", NULL, "Right STO HP Mixer"},

	{"SPK Vol Input Mux", "Vmid", "Vmid"},
	{"SPK Vol Input Mux", "HP Mixer", "Left HP Mixer"},
	{"SPK Vol Input Mux", "HP Mixer", "Right HP Mixer"},
	{"SPK Vol Input Mux", "SPK Mixer", "SPK Mixer"},
	{"SPK Vol Input Mux", "Mono Mixer", "Mono Mixer"},

	{"HPL Vol Input Mux", "HPL Mixer", "Left HP Mixer"},
	{"HPL Vol Input Mux", "Vmid", "Vmid"},
	{"HP Amp", NULL, "HPL Vol Input Mux"},
	{"HPR Vol Input Mux", "HPR Mixer", "Right HP Mixer"},
	{"HPR Vol Input Mux", "Vmid", "Vmid"},
	{"HP Amp", NULL, "HPR Vol Input Mux"},

	{"AXO Vol Input Mux", "Vmid", "Vmid"},
	{"AXO Vol Input Mux", "HP Mixer", "Left HP Mixer"},
	{"AXO Vol Input Mux", "HP Mixer", "Right HP Mixer"},
	{"AXO Vol Input Mux", "SPK Mixer", "SPK Mixer"},
	{"AXO Vol Input Mux", "Mono Mixer", "Mono Mixer"},
	{"AXO", NULL, "AXO Vol Input Mux"},
#else
	{"Left HP Mixer", NULL, "Mono HP Mixer"},
	{"Left HP Mixer", NULL, "Left STO HP Mixer"},
	{"Right HP Mixer", NULL, "Mono HP Mixer"},
	{"Right HP Mixer", NULL, "Right STO HP Mixer"},

	{"SPK Vol Input Mux", "HP Mixer", "Left HP Mixer"},
	{"SPK Vol Input Mux", "HP Mixer", "Right HP Mixer"},
	{"SPK Vol Input Mux", "SPK Mixer", "SPK Mixer"},
	{"SPK Vol Input Mux", "Mono Mixer", "Mono Mixer"},

	{"HPL Vol Input Mux", "HPL Mixer", "Left HP Mixer"},
	{"HP Amp", NULL, "HPL Vol Input Mux"},
	{"HPR Vol Input Mux", "HPR Mixer", "Right HP Mixer"},
	{"HP Amp", NULL, "HPR Vol Input Mux"},

	{"AXO Vol Input Mux", "HP Mixer", "Left HP Mixer"},
	{"AXO Vol Input Mux", "HP Mixer", "Right HP Mixer"},
	{"AXO Vol Input Mux", "SPK Mixer", "SPK Mixer"},
	{"AXO Vol Input Mux", "Mono Mixer", "Mono Mixer"},
	{"AXO", NULL, "AXO Vol Input Mux"},
#endif
#else
	{"HP Mixer", NULL, "Mono HP Mixer"},
	{"HP Mixer", NULL, "Left STO HP Mixer"},
	{"HP Mixer", NULL, "Right STO HP Mixer"},

	{"SPK Vol Input Mux", "Vmid", "Vmid"},
	{"SPK Vol Input Mux", "HP Mixer", "HP Mixer"},
	{"SPK Vol Input Mux", "SPK Mixer", "SPK Mixer"},
	{"SPK Vol Input Mux", "Mono Mixer", "Mono Mixer"},

	{"HPL Vol Input Mux", "HPL mixer", "HP Mixer"},
	{"HPL Vol Input Mux", "Vmid", "Vmid"},
	{"HP Amp", NULL, "HPL Vol Input Mux"},
	{"HPR Vol Input Mux", "HPR mixer", "HP Mixer"},
	{"HPR Vol Input Mux", "Vmid", "Vmid"},
	{"HP Amp", NULL, "HPR Vol Input Mux"},

	{"AXO Vol Input Mux", "Vmid", "Vmid"},
	{"AXO Vol Input Mux", "HP Mixer", "HP Mixer"},
	{"AXO Vol Input Mux", "SPK Mixer", "SPK Mixer"},
	{"AXO Vol Input Mux", "Mono Mixer", "Mono Mixer"},
	{"AXO", NULL, "AXO Vol Input Mux"},
#endif

	{"SPK", NULL, "ClassAB Amp", check_class_ab},
	{"SPK", NULL, "ClassD Amp", check_class_d},
	{"SPK", NULL, "SPK Vol Input Mux"},
	{"HPL", NULL, "HP Amp"},
	{"HPR", NULL, "HP Amp"},
	{"AUXL", NULL, "AXO"},
	{"AUXR", NULL, "AXO"},
};
#endif

/* PLL divisors */
struct _pll_div {
	u32 pll_in;
	u32 pll_out;
	u16 regvalue;
};

static const struct _pll_div codec_pll_div[] = {
	{  2048000,  8192000,  0x0ea0},
	{  3686400,  8192000,  0x4e27},
	{ 12000000,  8192000,  0x456b},   
	{ 13000000,  8192000,  0x495f},
	{ 13100000,  8192000,  0x0320},
	{  2048000,  11289600,  0xf637},
	{  3686400,  11289600,  0x2f22},
	{ 12000000,  11289600,  0x3e2f},   
	{ 13000000,  11289600,  0x4d5b},
	{ 13100000,  11289600,  0x363b},
	{  2048000,  16384000,  0x1ea0},
	{  3686400,  16384000,  0x9e27},
	{ 12000000,  16384000,  0x452b},   
	{ 13000000,  16384000,  0x542f},
	{ 13100000,  16384000,  0x03a0},
	{  2048000,  16934400,  0xe625},
	{  3686400,  16934400,  0x9126},
	{ 12000000,  16934400,  0x4d2c},
	{ 13000000,  16934400,  0x742f},
	{ 13100000,  16934400,  0x3c27},
	{  2048000,  22579200,  0x2aa0},
	{  3686400,  22579200,  0x2f20},
	{ 12000000,  22579200,  0x7e2f},   
	{ 13000000,  22579200,  0x742f},
	{ 13100000,  22579200,  0x3c27},
	{  2048000,  24576000,  0x2ea0},
	{  3686400,  24576000,  0xee27},
	{ 12000000,  24576000,  0x2915},   
	{ 13000000,  24576000,  0x772e},
	{ 13100000,  24576000,  0x0d20},
	{ 26000000,  24576000,  0x2027},
	{ 26000000,  22579200,  0x392f},
	{ 24576000,  22579200,  0x0921},
	{ 24576000,  24576000,  0x02a0},
};

static const struct _pll_div codec_bclk_pll_div[] = {
	{ 256000,  2048000,  0x46f0},
	{ 256000,  4096000,  0x3ea0},
	{ 352800,  5644800,  0x3ea0},
	{ 512000,  8192000,  0x3ea0},
	{ 1024000,  8192000,  0x46f0},
	{ 705600,  11289600,  0x3ea0},
	{ 1024000,  16384000,  0x3ea0},
	{ 1411200,  22579200,  0x3ea0},
	{ 1536000,  24576000,  0x3ea0},
	{ 2048000,  16384000,  0x1ea0},
	{ 2822400,  22579200,  0x1ea0},
	{ 2822400,  45158400,  0x5ec0},
	{ 5644800,  45158400,  0x46f0},
	{ 3072000,  24576000,  0x1ea0},
	{ 3072000,  49152000,  0x5ec0},
	{ 6144000,  49152000,  0x46f0},
	{ 705600,  11289600,  0x3ea0},
	{ 705600,  8467200,  0x3ab0},
	{ 24576000,  24576000,  0x02a0},
	{ 1411200,  11289600,  0x1690},
	{ 2822400,  11289600,  0x0a90},
	{ 1536000,  12288000,  0x1690},
	{ 3072000,  12288000,  0x0a90},
	{  512000,  24576000,  0x8e90},
	{  256000,  24576000,  0xbe80},
};


static int rt5623_set_dai_pll(struct snd_soc_dai *dai,
		int pll_id,int source, unsigned int freq_in, unsigned int freq_out)
{
	int i;
	int ret = -EINVAL;
	struct snd_soc_codec *codec = dai->codec;

	if (pll_id < RT5623_PLL_FR_MCLK || pll_id > RT5623_PLL_FR_BCLK)
		return ret;

	if (!freq_in || !freq_out) {
		dev_dbg(codec->dev, "PLL disabled\n");
		snd_soc_update_bits(codec, RT5623_GLOBAL_CLK_CTRL_REG,
				RT5623_SYSCLK_SOUR_SEL_MASK,
				RT5623_SYSCLK_SOUR_SEL_MCLK);
		return 0;
	}

	if (RT5623_PLL_FR_MCLK == pll_id) {
		for (i = 0; i < ARRAY_SIZE(codec_pll_div); i++)
			if (codec_pll_div[i].pll_in == freq_in &&
				codec_pll_div[i].pll_out == freq_out) {
				snd_soc_update_bits(codec,
					RT5623_GLOBAL_CLK_CTRL_REG,
					0x0000, 0x4000);
			 	snd_soc_write(codec, RT5623_PLL_CTRL,
					codec_pll_div[i].regvalue);
				ret = 0;
				break;
			}
	} else {
		for (i = 0; i < ARRAY_SIZE(codec_bclk_pll_div); i++)
			if ((freq_in == codec_bclk_pll_div[i].pll_in) &&
				(freq_out == codec_bclk_pll_div[i].pll_out)) {
				snd_soc_update_bits(codec,
					RT5623_GLOBAL_CLK_CTRL_REG,
					0x4000, 0x4000);
				snd_soc_write(codec, RT5623_PLL_CTRL,
					codec_bclk_pll_div[i].regvalue);
				ret = 0;
				break;
			}
	}
	snd_soc_update_bits(codec, RT5623_GLOBAL_CLK_CTRL_REG,
		RT5623_SYSCLK_SOUR_SEL_MASK, RT5623_SYSCLK_SOUR_SEL_PLL);

	return ret;
}


struct _coeff_div {
	u32 mclk;
	u32 rate;
	u16 fs;
	u16 regvalue;
};

/* codec hifi mclk (after PLL) clock divider coefficients */
static const struct _coeff_div coeff_div[] = {
	/* 8k */
	{ 2048000,  8000, 256*1, 0x0a2d},
	{ 8192000,  8000, 256*4, 0x2a2d},
	{12288000,  8000, 384*4, 0x2c2f},
	/* 11.025k */
	{11289600, 11025, 256*4, 0x2a2d},
	{16934400, 11025, 384*4, 0x2c2f},
	/* 16k */
	{12288000, 16000, 384*2, 0x1c2f},
	{16384000, 16000, 256*4, 0x2a2d},
	{24576000, 16000, 384*4, 0x2c2f},
	/* 22.05k */	
	{11289600, 22050, 256*2, 0x1a2d},
	{16934400, 22050, 384*2, 0x1c2f},
	/* 32k */
	{12288000, 32000, 384  , 0x0c2f},
	{16384000, 32000, 256*2, 0x1a2d},
	{24576000, 32000, 384*2, 0x1c2f},
	/* 44.1k */
	{11289600, 44100, 256*1, 0x0a2d},
	{22579200, 44100, 256*2, 0x1a2d},
	{45158400, 44100, 256*4, 0x2a2d},	
	/* 48k */
	{12288000, 48000, 256*1, 0x0a2d},
	{24576000, 48000, 256*2, 0x1a2d},
	{49152000, 48000, 256*4, 0x2a2d},
};

static int get_coeff(int mclk, int rate)
{
	int i;
	
	pr_debug("get_coeff mclk=%d,rate=%d\n",mclk,rate);
	for (i = 0; i < ARRAY_SIZE(coeff_div); i++)
		if (coeff_div[i].rate == rate && coeff_div[i].mclk == mclk)
			return i;

	return -EINVAL;
}


/*
 * Clock after PLL and dividers
 */
 /*in this driver, you have to set sysclk to be 24576000,
 * but you don't need to give a clk to be 24576000, our 
 * internal pll will generate this clock! so it won't make
 * you any difficult.
 */
static int rt5623_set_dai_sysclk(struct snd_soc_dai *dai,
		int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = dai->codec;
	struct rt5623_priv *rt5623 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "enter %s\n", __func__);	
	if ((freq >= (256 * 8000)) && (freq <= (512 * 48000))) {
		rt5623->sysclk = freq;
		return 0;	
	}
	
	dev_err(codec->dev,  "unsupported sysclk freq %u for audio i2s\n", freq);
	return -EINVAL;
}


static int rt5623_set_dai_fmt(struct snd_soc_dai *dai,
		unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;
	unsigned int iface = 0;

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		iface = RT5623_SDP_SLAVE_MODE;
		break;
	default:
		return -EINVAL;
	}

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		iface |= RT5623_I2S_DF_RIGHT;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		iface |= RT5623_I2S_DF_LEFT;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		iface |= RT5623_I2S_DF_PCM;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		iface |= (RT5623_I2S_PCM_MODE | RT5623_I2S_DF_PCM);
		break;
	default:
		return -EINVAL;
	}

	/* clock inversion */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		iface |= 0x0000;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		iface |= 0x0100;
		break;
	default:
		return -EINVAL;
	}

	snd_soc_write(codec, RT5623_AUDIO_INTERFACE, iface);

	return 0;
}


static int rt5623_pcm_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct rt5623_priv *rt5623 = snd_soc_codec_get_drvdata(codec);
	int coeff = get_coeff(rt5623->sysclk, params_rate(params));
	unsigned int iface = 0;

	dev_dbg(codec->dev, "enter %s\n", __func__);	
	if (coeff < 0) {
		dev_err(codec->dev, "get_coeff err!\n");
		return -EINVAL;
	}

	/* bit size */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		iface |= RT5623_I2S_DL_20;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		iface |= RT5623_I2S_DL_24;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		iface |= RT5623_I2S_DL_32;
		break;
	}

	/* set iface & srate */
	snd_soc_update_bits(codec, RT5623_AUDIO_INTERFACE,
			RT5623_I2S_DL_MASK, iface);
	snd_soc_write(codec, RT5623_STEREO_AD_DA_CLK_CTRL,
			coeff_div[coeff].regvalue);

	return 0;
}

static int rt5623_set_bias_level(struct snd_soc_codec *codec,
			enum snd_soc_bias_level level)
{
	switch (level) {
	case SND_SOC_BIAS_ON:
#ifdef RT5623_DEMO
		snd_soc_update_bits(codec, RT5623_SPK_OUT_VOL,
			RT5623_L_MUTE | RT5623_R_MUTE, 0);
		snd_soc_update_bits(codec, RT5623_HP_OUT_VOL,
			RT5623_L_MUTE | RT5623_R_MUTE, 0);
#endif
		break;

	case SND_SOC_BIAS_PREPARE:
#ifdef RT5623_DEMO
		snd_soc_update_bits(codec, RT5623_PWR_MANAG_ADD1,
			RT5623_PWR_MIC1_BIAS_EN, RT5623_PWR_MIC1_BIAS_EN);
#endif
		break;

	case SND_SOC_BIAS_STANDBY:
#ifdef RT5623_DEMO
		snd_soc_update_bits(codec, RT5623_PWR_MANAG_ADD1,
			RT5623_PWR_MIC1_BIAS_EN, 0);
		snd_soc_update_bits(codec, RT5623_SPK_OUT_VOL,
			RT5623_L_MUTE | RT5623_R_MUTE,
			RT5623_L_MUTE | RT5623_R_MUTE);
		snd_soc_update_bits(codec, RT5623_HP_OUT_VOL,
			RT5623_L_MUTE | RT5623_R_MUTE,
			RT5623_L_MUTE | RT5623_R_MUTE);
#endif
		if (SND_SOC_BIAS_OFF == codec->dapm.bias_level) {
			snd_soc_write(codec, RT5623_PWR_MANAG_ADD3,
					RT5623_PWR_MAIN_BIAS);
			snd_soc_write(codec, RT5623_PWR_MANAG_ADD2,
					RT5623_PWR_VREF);
			codec->cache_only = false;
			snd_soc_cache_sync(codec);
		}
		break;

	case SND_SOC_BIAS_OFF:
#ifdef RT5623_DEMO
		snd_soc_update_bits(codec, RT5623_SPK_OUT_VOL,
			RT5623_L_MUTE | RT5623_R_MUTE,
			RT5623_L_MUTE | RT5623_R_MUTE);
		snd_soc_update_bits(codec, RT5623_HP_OUT_VOL,
			RT5623_L_MUTE | RT5623_R_MUTE,
			RT5623_L_MUTE | RT5623_R_MUTE);
#endif
		snd_soc_write(codec, RT5623_PWR_MANAG_ADD3, 0x0000);
		snd_soc_write(codec, RT5623_PWR_MANAG_ADD2, 0x0000);
		snd_soc_write(codec, RT5623_PWR_MANAG_ADD1, 0x0000);
		break;

	default:
		break;
	}
	codec->dapm.bias_level = level;

	return 0;
}


static int rt5623_probe(struct snd_soc_codec *codec)
{
	int ret;

	ret = snd_soc_codec_set_cache_io(codec, 8, 16, SND_SOC_I2C);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}

	rt5623_reset(codec);
	snd_soc_write(codec, RT5623_PWR_MANAG_ADD3, RT5623_PWR_MAIN_BIAS);
	snd_soc_write(codec, RT5623_PWR_MANAG_ADD2, RT5623_PWR_VREF);
	hp_depop_mode2(codec);

#ifdef RT5623_DEMO
	rt5623_reg_init(codec);
#endif

	snd_soc_update_bits(codec, RT5623_PWR_MANAG_ADD1,
		RT5623_PWR_SOFTGEN_EN, 0);
	snd_soc_update_bits(codec, RT5623_PWR_MANAG_ADD3,
		RT5623_PWR_HP_L_OUT_VOL | RT5623_PWR_HP_R_OUT_VOL, 0);

	codec->dapm.bias_level = SND_SOC_BIAS_STANDBY;

	return 0;
}

static int rt5623_remove(struct snd_soc_codec *codec)
{
	rt5623_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

#ifdef CONFIG_PM
static int rt5623_suspend(struct snd_soc_codec *codec, pm_message_t state)
{
	rt5623_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static int rt5623_resume(struct snd_soc_codec *codec)
{
	rt5623_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	return 0;
}
#else
#define rt5623_suspend NULL
#define rt5623_resume NULL
#endif


#define RT5623_HIFI_RATES SNDRV_PCM_RATE_8000_48000
#define RT5623_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |\
					SNDRV_PCM_FMTBIT_S24_LE)

struct snd_soc_dai_ops rt5623_hifi_ops = {
	.hw_params = rt5623_pcm_hw_params,	
	.set_fmt = rt5623_set_dai_fmt,
	.set_sysclk = rt5623_set_dai_sysclk,
	.set_pll = rt5623_set_dai_pll,
};

struct snd_soc_dai_driver rt5623_dai = { 
	.name = "rt5623-hifi",
	.playback = {
		.stream_name = "HiFi Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = RT5623_HIFI_RATES,
		.formats = RT5623_FORMATS,
	},
	.capture = {
		.stream_name = "HiFi Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = RT5623_HIFI_RATES,
		.formats = RT5623_FORMATS,
	},
	.ops = &rt5623_hifi_ops,
};

static struct snd_soc_codec_driver soc_codec_dev_rt5623 = {
	.probe = 	rt5623_probe,
	.remove = rt5623_remove,
	.suspend = rt5623_suspend,
	.resume = rt5623_resume,
	.set_bias_level = rt5623_set_bias_level,
	.reg_cache_size = RT5623_VENDOR_ID2 + 1,
	.reg_word_size = sizeof(u16),
	.reg_cache_default = rt5623_reg,
	.volatile_register = rt5623_volatile_register,
	.readable_register = rt5623_readable_register,
	.reg_cache_step = 1,
	.controls = rt5623_snd_controls,
	.num_controls = ARRAY_SIZE(rt5623_snd_controls),
	.dapm_widgets = rt5623_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(rt5623_dapm_widgets),
	.dapm_routes = rt5623_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(rt5623_dapm_routes),
};

static const struct i2c_device_id rt5623_i2c_id[] = {
	{ "rt5623", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, rt5623_i2c_id);

static int __devinit rt5623_i2c_probe(struct i2c_client *i2c,
		    const struct i2c_device_id *id)
{
	struct rt5623_priv *rt5623;
	int ret;
pr_debug("%s(%d)\n", __func__, __LINE__);
	rt5623 = kzalloc(sizeof(struct rt5623_priv), GFP_KERNEL);
	if (NULL == rt5623)
		return -ENOMEM;

	i2c_set_clientdata(i2c, rt5623);

	ret = snd_soc_register_codec(&i2c->dev,
		&soc_codec_dev_rt5623, &rt5623_dai, 1);
	if (ret < 0)
		kfree(rt5623);
pr_debug("%s(%d)\n", __func__, __LINE__);

	return ret;
}

static int __devexit rt5623_i2c_remove(struct i2c_client *i2c)
{
	snd_soc_unregister_codec(&i2c->dev);
	kfree(i2c_get_clientdata(i2c));
	return 0;
}

struct i2c_driver rt5623_i2c_driver = {
	.driver = {
		.name = "rt5623",
		.owner = THIS_MODULE,
	},
	.probe = rt5623_i2c_probe,
	.remove   = __devexit_p(rt5623_i2c_remove),
	.id_table = rt5623_i2c_id,
};

static int __init rt5623_modinit(void)
{
	return i2c_add_driver(&rt5623_i2c_driver);
}
module_init(rt5623_modinit);

static void __exit rt5623_modexit(void)
{
	i2c_del_driver(&rt5623_i2c_driver);
}
module_exit(rt5623_modexit);

MODULE_DESCRIPTION("ASoC RT5623 driver");
MODULE_AUTHOR("Johnny Hsu <johnnyhsu@realtek.com>");
MODULE_LICENSE("GPL");
