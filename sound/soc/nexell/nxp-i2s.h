/*
 */
#ifndef __NXP_I2S_H__
#define __NXP_I2S_H__

#include "nxp-pcm.h"

#define SND_SOC_I2S_FORMATS (SNDRV_PCM_FMTBIT_U8 | SNDRV_PCM_FMTBIT_S8 | \
		SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_U16_LE | \
		SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_U24_LE)

#define SND_SOC_I2S_RATES	SNDRV_PCM_RATE_8000_192000

extern int nxp_i2s_trigger(struct snd_pcm_substream *substream,
				int cmd, struct snd_soc_dai *dai);
#endif /* __NXP_I2S_H__ */

