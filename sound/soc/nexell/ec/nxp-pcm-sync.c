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
#include <linux/gpio.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include <mach/platform.h>
#include <mach/pdm.h>

#include "nxp-pcm-sync.h"

/*
#define pr_debug		printk
*/

struct snd_sync_dev {
	bool is_start;
	void *base;
	unsigned int status;
};

static struct snd_sync_dev sync_dev[SND_SYNC_DEV_NUM] = {
	[0] = { false, NULL, 0 },
	[1] = { false, NULL, 0 },
	[2] = { false, NULL, 0 },
	[3] = { false, NULL, 0 },
};

static DEFINE_SPINLOCK(pcm_sync_lock);

#define	DETECT_COUNT 2000

#define	WAIT_LRCLK_H(c, w)	do {	\
		int l = 0;	\
		c = w;	do { l = __raw_readl(IO_BASE(PDM_I2S_LRCLK)+0x18) & 1<<PAD_GET_BITNO(PDM_I2S_LRCLK); } while (!l && --c > 0);	\
	} while (0)

#define	WAIT_LRCLK_L(c, w)	do {	\
		int l = 0;	\
		c = w;  do { l = __raw_readl(IO_BASE(PDM_I2S_LRCLK)+0x18) & 1<<PAD_GET_BITNO(PDM_I2S_LRCLK); } while (l && --c > 0);	\
	} while (0)

static inline void __sync_capture_start(void)
{
	unsigned int CON;	///< 0x00 :
	unsigned int CSR;	///< 0x04 :
	void *base = NULL;

	int  spie_bit = PAD_GET_BITNO(PDM_IO_CSSEL);
	int  lclk_bit  = PAD_GET_BITNO(PDM_IO_LRCLK);
	int  run_bit = PAD_GET_BITNO(PDM_IO_ISRUN);
	int  count[3] = { 0, };
	bool bLRCK = false;
	unsigned int io_lrclk = 0, io_run = 0;

	io_lrclk = __raw_readl(IO_BASE(PDM_IO_LRCLK));
	io_run	 = __raw_readl(IO_BASE(PDM_IO_ISRUN));

	/* bLRCK */
	WAIT_LRCLK_H(count[0], DETECT_COUNT);
	if (count[0] > 0) {
		WAIT_LRCLK_L(count[1], DETECT_COUNT);
		if (count[1] > 0) {
			WAIT_LRCLK_H(count[2], DETECT_COUNT);
			bLRCK = true;
		}
	}

	if (bLRCK)
		io_lrclk &= ~(1<<lclk_bit);
	else
		io_lrclk |=  (1<<lclk_bit);

	/* SPI CSSEL : L ON */
	__raw_writel(__raw_readl(IO_BASE(PDM_IO_CSSEL)) & ~(1<<spie_bit), IO_BASE(PDM_IO_CSSEL));

	/* PDM LRCLK : L EXIST, H No Exist */
	__raw_writel(io_lrclk, IO_BASE(PDM_IO_LRCLK));

	 /* ISRUN : H RUN */
	__raw_writel(io_run | (1<<run_bit), IO_BASE(PDM_IO_ISRUN));

	/* I2S 0 */
	base = (void*)0xf0055000;
	CON  =  __raw_readl(base + 0x00) | ((1 << 1)  | (1 << 0));
	CSR  = (__raw_readl(base + 0x04) & ~(3 << 8)) | (1 << 8) | 0;

	__raw_writel(0x0, (base+0x08));	/* Clear the Flush bit */
	__raw_writel(CSR, (base+0x04));
	__raw_writel(CON, (base+0x00));

	/* I2S 1 */
	base = (void*)0xf0056000;
	__raw_writel(0x0, (base+0x08));	/* Clear the Flush bit */
	__raw_writel(CSR, (base+0x04));
	__raw_writel(CON, (base+0x00));

	/* PDM */
	base = (void*)0xf005f000;
	__raw_writel(__raw_readl(base + 0x04) | 0x02, (base + 0x04));	// SSPCR1 = 0x04
	pr_debug("***** LRCK %s [%d:%d:%d]*****\n",
		bLRCK ? "Exist" : "No Exist", count[0], count[1], count[2]);
}

int nxp_snd_sync_trigger(struct snd_pcm_substream *substream,
					int cmd, enum snd_pcm_dev_type type, void *base, int status)
{
	struct snd_sync_dev *pdev = sync_dev;
	unsigned long mask = 0, flags;
	int i = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		return -EINVAL;

	pr_debug("[type:%d]\n", type);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	case SNDRV_PCM_TRIGGER_START:

		spin_lock_irqsave(&pcm_sync_lock, flags);

		pdev[type].is_start = true;
		pdev[type].base = base;
		pdev[type].status = status;

		for (i = 0; SND_SYNC_DEV_NUM > i; i++)
			mask |= sync_dev[i].is_start ? (1<<i): (0<<i);

		if (SND_SYNC_DEV_MASK != mask) {
			spin_unlock_irqrestore(&pcm_sync_lock, flags);
			break;
		}

		pr_debug("***** RUN PCM CAPTURE SYNC (I2S/PDM) *****\n");
		for (i = 0; SND_SYNC_DEV_NUM > i; i++)
			pdev[i].is_start = false;

		__sync_capture_start();

		spin_unlock_irqrestore(&pcm_sync_lock, flags);
		break;

	default:
		return -EINVAL;
	}

	return 0;
}
