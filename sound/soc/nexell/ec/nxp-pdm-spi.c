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
#include <linux/gpio.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include <mach/platform.h>
#include <mach/devices.h>
#include <mach/soc.h>


#include "nxp-pdm-spi.h"
#include "nxp-pcm-sync.h"

/*
#define pr_debug(msg...)		printk(KERN_INFO msg)
*/

/********************************************************************************/
#define	DEF_SAMPLE_RATE			16000
#define	DEF_SAMPLE_BIT			16	// 16 (PCM)

#define	SPI_BASEADDR			PHY_BASEADDR_SSP0
#define	SPI_BUS_WIDTH			1	// Byte
#define	SPI_MAX_BURST			4	// Byte

#define SPI_DATA_OFFSET			8

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
	struct clk *clk;
	int ch;
	unsigned int base_addr;
};

#define SPI_CLK 100 * 1000 *1000

static  const int reset[3][2] = {
    {RESET_ID_SSP0_P,RESET_ID_SSP0} ,
    {RESET_ID_SSP1_P,RESET_ID_SSP1} ,
    {RESET_ID_SSP2_P,RESET_ID_SSP2} ,
};

static int pdm_spi_init(struct nxp_pdm_snd_param *par)
{
	char name[10] = {0};
	int ch = par->ch;

	NX_SSP_Initialize();
	NX_SSP_SetBaseAddress(ch, (void*)IO_ADDRESS(NX_SSP_GetPhysicalAddress(ch)));
	pr_debug("%s: ch.%d, base=0x%x\n", __func__, ch, par->base_addr);

	sprintf(name, "nxp-spi.%d",(unsigned char)ch);

	par->clk = clk_get(NULL, name);
	clk_set_rate(par->clk, SPI_CLK);
	clk_enable(par->clk);

	NX_RSTCON_SetnRST(NX_SSP_GetResetNumber(ch, NX_SSP_PRESETn), RSTCON_nDISABLE);
	NX_RSTCON_SetnRST(NX_SSP_GetResetNumber(ch, NX_SSP_nSSPRST), RSTCON_nDISABLE);
	NX_RSTCON_SetnRST(NX_SSP_GetResetNumber(ch, NX_SSP_PRESETn), RSTCON_nENABLE);
	NX_RSTCON_SetnRST(NX_SSP_GetResetNumber(ch, NX_SSP_nSSPRST), RSTCON_nENABLE);

	NX_SSP_SetEnable(ch, CFALSE); 			// SSP operation disable
	NX_SSP_SetProtocol(ch, 0); 				// Protocol : Motorola SPI


	// 0, 0	-> No receive 	-> No receive
	// 0, 1 -> corrupt		-> 완전 깨져 (4ch 다 신호)
	// 1, 0 -> No receive	-> No receive
	// 1, 1 -> corrupt		-> corrupt 이지마 원하는 1채널만

	NX_SSP_SetClockPolarityInvert(ch, 1);
	NX_SSP_SetClockPhase(ch, 1);

	NX_SSP_SetBitWidth(ch, 8); 				// 8 bit
	NX_SSP_SetSlaveMode(ch, CTRUE); 		// slave mode

	NX_SSP_SetInterruptEnable(ch, 0, CFALSE);
	NX_SSP_SetInterruptEnable(ch, 1, CFALSE);
	NX_SSP_SetInterruptEnable(ch, 2, CFALSE);
	NX_SSP_SetInterruptEnable(ch, 3, CFALSE);
//	NX_SSP_SetClockPrescaler(ch, 0x02,0x02);

	NX_SSP_SetDMATransferMode(ch, CTRUE);   //DMA_USE

	gpio_request(PDM_IO_CSSEL, NULL);
	gpio_request(PDM_IO_LRCLK, NULL);
	gpio_request(PDM_IO_ISRUN, NULL);

	gpio_direction_output(PDM_IO_CSSEL, 1);		// OFF
	gpio_direction_output(PDM_IO_ISRUN, 0);		// OFF
	gpio_direction_output(PDM_IO_LRCLK, 1);		// No Exist

	return 0;
}

static int pdm_spi_start(struct nxp_pdm_snd_param *par, int stream)
{
		int ch = par->ch;
	pr_debug("[%s: ch.%d]\n", __func__, par->ch);

#if 1
	NX_SSP_SetProtocol(ch, 0); 				// Protocol : Motorola SPI

	// 0, 0	-> No receive 	-> No receive
	// 0, 1 -> corrupt		-> 완전 깨져 (4ch 다 신호)
	// 1, 0 -> No receive	-> No receive
	// 1, 1 -> corrupt		-> corrupt 이지마 원하는 1채널만
	NX_SSP_SetClockPolarityInvert(ch, 1);
	NX_SSP_SetClockPhase(ch, 1);

	NX_SSP_SetBitWidth(ch, 8); 				// 8 bit
	NX_SSP_SetSlaveMode(ch, CTRUE); 		// slave mode
	NX_SSP_SetDMATransferMode(ch, CTRUE);   //DMA_USE
#endif
	NX_SSP_SetSlaveOutputEnable(par->ch, CTRUE);
	NX_SSP_SetEnable(par->ch, CTRUE);

	gpio_set_value(PDM_IO_LRCLK, 0);		// Exist
	gpio_set_value(PDM_IO_CSSEL, 0);		// ON
	gpio_set_value(PDM_IO_ISRUN, 1);		// RUN

	return 0;
}

static void pdm_spi_stop(struct nxp_pdm_snd_param *par, int stream)
{
	pr_debug("[%s: ch.%d]\n", __func__, par->ch);

	gpio_set_value(PDM_IO_CSSEL, 1);		// OFF
	gpio_set_value(PDM_IO_ISRUN, 0);		// OFF
	gpio_set_value(PDM_IO_LRCLK, 1);		// No Exist

	NX_SSP_SetEnable(par->ch, CFALSE);
}

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

	/* TODO */
	par->ch = plat->channel;

	if (par->ch != 2)
		par->base_addr = (SPI_BASEADDR + par->ch*0x1000);
	else
		par->base_addr = (SPI_BASEADDR + 0x4000);

	pr_debug("PDM: spi base_addr : %x, spi ch : %d\n", par->base_addr, par->ch);

	pdm_spi_init(par);
	spin_lock_init(&par->lock);

	if (!plat->dma_ch)
		return -EINVAL;

	dma->active = true;
	dma->dma_filter = plat->dma_filter;
	dma->dma_ch_name = (char*)(plat->dma_ch);
	/* TODO */
	dma->peri_addr = par->base_addr + SPI_DATA_OFFSET;	/* SPI DAT */
	dma->bus_width_byte = SPI_BUS_WIDTH;
	dma->max_burst_byte = SPI_MAX_BURST;

	pr_debug("pdm-spi: %s, %s dma, addr 0x%x, bus %dbyte, burst %dbyte\n",
		STREAM_STR(1), dma->dma_ch_name, dma->peri_addr,
		dma->bus_width_byte, dma->max_burst_byte);

	return nxp_pdm_check_param(par);
}

static int nxp_pdm_setup(struct snd_soc_dai *dai)
{
	struct nxp_pdm_snd_param *par = snd_soc_dai_get_drvdata(dai);

	if (SNDDEV_STATUS_SETUP & par->status)
		return 0;

	par->status |= SNDDEV_STATUS_SETUP;

	return 0;
}

static void nxp_pdm_release(struct snd_soc_dai *dai)
{
	struct nxp_pdm_snd_param *par = snd_soc_dai_get_drvdata(dai);
	par->status = SNDDEV_STATUS_CLEAR;
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
}

static int nxp_pdm_trigger(struct snd_pcm_substream *substream,
				int cmd, struct snd_soc_dai *dai)
{
	struct nxp_pdm_snd_param *par = snd_soc_dai_get_drvdata(dai);
	int stream = substream->stream;
	enum snd_pcm_dev_type type = SND_DEVICE_PDM;
	pr_debug("%s: %s cmd=%d, ch=%d\n", __func__, STREAM_STR(stream), cmd, par->ch);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_START:
		#ifdef SND_DEV_SYNC_I2S_PDM
		if (SNDRV_PCM_STREAM_CAPTURE == stream) {
			nxp_snd_sync_trigger(substream, cmd, type,
				(void*)IO_ADDRESS(par->base_addr), par->ch);
		} else
		#endif
			pdm_spi_start(par, stream);
		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_STOP:
		pdm_spi_stop(par, stream);
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
	/* TODO */
	return 0;
}

static int nxp_pdm_dai_resume(struct snd_soc_dai *dai)
{
	return 0;
}

static int nxp_pdm_dai_probe(struct snd_soc_dai *dai)
{
	return nxp_pdm_setup(dai);
}

static int nxp_pdm_dai_remove(struct snd_soc_dai *dai)
{
	nxp_pdm_release(dai);
	return 0;
}

static struct snd_soc_dai_driver pdm_dai_driver = {
	.capture	= {
		.channels_min 	= 2,
		.channels_max 	= 4,
		.formats		= SND_SOC_SPI_FORMATS,
		.rates			= SND_SOC_SPI_RATES,
		.rate_min 		= 16000,
		.rate_max 		= 16000,
		},
	.playback	= {
		.channels_min 	= 2,
		.channels_max 	= 4,
		.formats		= SND_SOC_SPI_FORMATS,
		.rates			= SND_SOC_SPI_RATES,
		.rate_min 		= 16000,
		.rate_max 		= 16000,
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
	.name 	= "nxp-pdm-spi",
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
MODULE_DESCRIPTION("Sound SPI recorder driver for the SLSI");
MODULE_LICENSE("GPL");

