/*
 * (C) Copyright 2009
 * hyun seok jung, Nexell Co, <hsjung@nexell.co.kr>
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
#include <mach/devices.h>
#include <mach/soc.h>
#include <mach/pdm.h>

#include "../nxp-pdm.h"

/*
#define pr_debug(msg...)		printk(KERN_INFO msg)
*/

#define	DEF_SAMPLE_RATE			48000
#define	DEF_SAMPLE_BIT			16	// 16 (PCM)

#define	PDM_BASEADDR			PHY_BASEADDR_PDM
#define	PDM_BUS_WIDTH			4	// Byte
#define	PDM_MAX_BURST			32	// Byte

#define PDM_IRQ_COUNT			8

/*
 * parameters
 */
struct nxp_pdm_snd_param {
    int sample_rate;
	int	status;
	spinlock_t	lock;
	/* DMA channel */
	struct nxp_pcm_dma_param dma;
	/* Register */
	unsigned int base_addr;
};

static int nxp_pdm_check_param(struct nxp_pdm_snd_param *par)
{
    struct nxp_pcm_dma_param *dmap = &par->dma;
	dmap->real_clock = par->sample_rate;
	par->status |= SNDDEV_STATUS_POWER;
	return 0;
}

static int nxp_pdm_set_plat_param(struct nxp_pdm_snd_param *par, void *data)
{
	struct platform_device *pdev = data;
	struct nxp_pdm_plat_data *plat = pdev->dev.platform_data;
	struct nxp_pcm_dma_param *dma = &par->dma;

    par->sample_rate = plat->sample_rate ? plat->sample_rate : DEF_SAMPLE_RATE;
	par->base_addr = IO_ADDRESS(PHY_BASEADDR_INTC1);
	spin_lock_init(&par->lock);
#if 1

	//dma->active = true;
	//dma->dma_filter = plat->dma_filter;
	dma->dma_ch_name = (char*)(plat->dma_ch);
	//dma->peri_addr = phy_base + PDM_DATA_OFFSET;	/* PDM DAT */
	dma->bus_width_byte = PDM_BUS_WIDTH;
	dma->max_burst_byte = PDM_MAX_BURST;
	pr_debug("pdm-rec: %s, %s dma, addr 0x%x, bus %dbyte, burst %dbyte\n",
		STREAM_STR(1), dma->dma_ch_name, dma->peri_addr,
		dma->bus_width_byte, dma->max_burst_byte);
#endif
	return nxp_pdm_check_param(par);
}

/*
 * snd_soc_dai_ops
 */
static int  nxp_pdm_startup(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct nxp_pdm_snd_param *par = snd_soc_dai_get_drvdata(dai);
	struct nxp_pcm_dma_param *dmap = &par->dma;
	snd_soc_dai_set_dma_data(dai, substream, dmap);
	return 0;
}

static void nxp_pdm_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	/* TODO */
}

static int nxp_pdm_trigger(struct snd_pcm_substream *substream,
				int cmd, struct snd_soc_dai *dai)
{
	pr_debug("%s: %s cmd=%d\n", __func__, STREAM_STR(substream->stream), cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_START:
		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_STOP:
		break;

	default:
		return -EINVAL;
	}
	return 0;
}

static int nxp_pdm_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	pr_debug("%s : change format:0x%x, channels:%d \n",
		__func__, params_format(params), params_channels(params));
	return 0;
}

static struct snd_soc_dai_ops nxp_pdm_ops = {
	.startup	= nxp_pdm_startup,
	.shutdown	= nxp_pdm_shutdown,
	.trigger	= nxp_pdm_trigger,
	.hw_params	= nxp_pdm_hw_params,
};

/*
 * snd_soc_dai_driver
 */
static int nxp_pdm_dai_suspend(struct snd_soc_dai *dai)
{
	return 0;
}

static int nxp_pdm_dai_resume(struct snd_soc_dai *dai)
{
	return 0;
}

static int nxp_pdm_dai_probe(struct snd_soc_dai *dai)
{
	return 0;
}

static int nxp_pdm_dai_remove(struct snd_soc_dai *dai)
{
	return 0;
}

static struct snd_soc_dai_driver pdm_dai_driver = {
	.capture	= {
		.channels_min 	= 2,
		.channels_max 	= 4,
		.formats		= SND_SOC_PDM_FORMATS,
		.rates			= SND_SOC_PDM_RATES,
		.rate_min 		= 16000,
		.rate_max 		= 48000,
		},
	.probe 		= nxp_pdm_dai_probe,
	.remove		= nxp_pdm_dai_remove,
	.suspend	= nxp_pdm_dai_suspend,
	.resume 	= nxp_pdm_dai_resume,
	.ops 		= &nxp_pdm_ops,
};

static __devinit int nxp_pdm_probe(struct platform_device *pdev)
{
	struct nxp_pdm_snd_param *par;
	int ret = 0;

    /*  allocate driver data */
    par = kzalloc(sizeof(struct nxp_pdm_snd_param), GFP_KERNEL);
    if (! par) {
        printk(KERN_ERR "fail, %s allocate driver info ...\n", pdev->name);
        return -ENOMEM;
    }

	ret = nxp_pdm_set_plat_param(par, pdev);
	if (ret)
		goto err_out;

	ret = snd_soc_register_dai(&pdev->dev, &pdm_dai_driver);
	if (ret) {
        printk(KERN_ERR "fail, %s snd_soc_register_dai ...\n", pdev->name);
		goto err_out;
	}

	dev_set_drvdata(&pdev->dev, par);
	return ret;

err_out:
	if (par)
		kfree(par);
	return ret;
}

static __devexit int nxp_pdm_remove(struct platform_device *pdev)
{
	struct nxp_pdm_snd_param *par = dev_get_drvdata(&pdev->dev);
	snd_soc_unregister_dai(&pdev->dev);
	if (par)
		kfree(par);
	return 0;
}

static struct platform_driver pdm_driver = {
	.probe  = nxp_pdm_probe,
	.remove = nxp_pdm_remove,
	.driver = {
	.name 	= "nxp-pdm-virt",
	.owner 	= THIS_MODULE,
	},
};

static int __init nxp_pdm_init(void)
{
	return platform_driver_register(&pdm_driver);
}

static void __exit nxp_pdm_exit(void)
{
	platform_driver_unregister(&pdm_driver);
}

module_init(nxp_pdm_init);
module_exit(nxp_pdm_exit);

MODULE_AUTHOR("hsjung <hsjung@nexell.co.kr>");
MODULE_DESCRIPTION("Sound PDM virtual recorder driver for the SLSI");
MODULE_LICENSE("GPL");

