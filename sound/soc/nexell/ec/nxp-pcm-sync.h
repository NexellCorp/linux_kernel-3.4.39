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

#include <linux/init.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <mach/platform.h>

#include "nxp-pcm-ec.h"

#ifndef __NXP_DEV_SYNC_H__
#define __NXP_DEV_SYNC_H__

enum snd_pcm_dev_type {
	SND_DEVICE_I2S0,	/* 1st */
	SND_DEVICE_I2S1,
	SND_DEVICE_I2S2,
	SND_DEVICE_PDM,		/* last */
};

#define SND_SYNC_DEV_MASK	(	\
		(1<<SND_DEVICE_I2S0) |		\
		(1<<SND_DEVICE_I2S1) |		\
		(1<<SND_DEVICE_PDM)			\
		)

#define	SND_SYNC_DEV_NUM	(4)


extern int nxp_snd_sync_trigger(struct snd_pcm_substream *substream,
					int cmd, enum snd_pcm_dev_type type, void *base, int status);

/********************************************************************************/
#define	SND_DEV_SYNC_I2S_PDM

/********************************************************************************/
#define	PDM_I2S_LRCLK		(PAD_GPIO_D + 12)	/* H -> L */

#define	PDM_IO_CSSEL		(PAD_GPIO_C + 22)	/* H -> L */
#define	PDM_IO_ISRUN		(PAD_GPIO_D + 19)	/* B 27, D19, L -> H : H : RUN, 0: STOP */
#define	PDM_IO_LRCLK		(PAD_GPIO_B + 26)	/* H -> L : H : No LR, 0: LR */

#define	IO_BASE(io)	(IO_ADDRESS(PHY_BASEADDR_GPIOA) + (0x1000*PAD_GET_GROUP(io)))
/********************************************************************************/

#endif