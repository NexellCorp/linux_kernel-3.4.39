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

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/dmaengine.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>
#include <linux/kthread.h>
#include <asm/uaccess.h>

#include <sound/core.h>
#include <sound/initval.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <mach/platform.h>
#include <mach/devices.h>

#include "../nxp-pcm.h"
#include "../nxp-pdm.h"

/*
#define pr_debug		printk
*/

#define	PDM_VIRTUAL_MEM_MAP_BASE		0x7FF00000
#define	PDM_VIRTUAL_MEM_MAP_SIZE		0x00100000
#define	PDM_VIRTUAL_MEM_PERIOD_BYTES	(4*160*8)	// -> 5120 : 640 * 8(FRAME: 4CH*2) PDM WRITE SIZE / PER 10ms
#define	PDM_VIRTUAL_MEM_PERIOD_SIZE		2

/*
 * PCM INFO
 */
#define	PERIOD_BYTES_MAX		32768

static struct snd_pcm_hardware nxp_pcm_hardware = {
	.info				= 	SNDRV_PCM_INFO_MMAP |
				    		SNDRV_PCM_INFO_MMAP_VALID |
				    		SNDRV_PCM_INFO_INTERLEAVED	|
				    		SNDRV_PCM_INFO_PAUSE |
				    		SNDRV_PCM_INFO_RESUME,	//  | SNDRV_PCM_INFO_BLOCK_TRANSFER
	.formats			= SND_SOC_PCM_FORMATS,
#if defined(CONFIG_SND_NXP_DFS)
   	.rates        = SNDRV_PCM_RATE_8000_192000,
#endif
	.rate_min			= 8000,
	.rate_max			= 192000,
	.channels_min		= 1,
	.channels_max		= 2,
	.buffer_bytes_max	= 128 * 1024 * 2,
	.period_bytes_min	= 32,
	.period_bytes_max	= PERIOD_BYTES_MAX,
	.periods_min		= 2,
	.periods_max		= 64,
	.fifo_size			= 32,
};
#define	substream_to_prtd(s)	(substream->runtime->private_data)

/*
 * PCM INTERFACE
 */
struct nxp_cpu_pdm_reg {
     u32 is_running;
     u32 sample_rate;
     u32 data_ptr;
};

#define	PDM_REG_START(r)	__raw_writel(1, (void __iomem*)&r->is_running)
#define	PDM_REG_STOP(r)		__raw_writel(0, (void __iomem*)&r->is_running)
#define	PDM_REG_RATE(r, s)	__raw_writel(s, (void __iomem*)&r->sample_rate)
#define	PDM_REG_DPTR(r)		__raw_readl((void __iomem*)&r->data_ptr)

struct nxp_pcm_virtual_data {
	struct snd_dma_buffer buffer;
	unsigned int period_size;
	unsigned int period_bytes;
	unsigned int buffer_bytes;
	unsigned int offset;
	void *devptr;
	int irq;
};

#define PCM_VIRTULA_IRQ_CLEAR(a)	__raw_writel(0xffffffff, a)

static irqreturn_t nxp_pcm_dma_complete(int irq, void *arg)
{
	struct snd_pcm_substream *substream = arg;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct nxp_pcm_runtime_data *prtd = substream_to_prtd(substream);
	struct nxp_pcm_virtual_data *pvd = prtd->private_data;
	unsigned int dma_offset = prtd->offset;
	struct nxp_cpu_pdm_reg *reg = (struct nxp_cpu_pdm_reg *)pvd->devptr;
	void *src, *dst;

	unsigned int intc = IO_ADDRESS(PHY_BASEADDR_INTC1);
	unsigned int offs = 0x1C;

	PCM_VIRTULA_IRQ_CLEAR(intc+offs);

	if (SNDRV_PCM_STATE_RUNNING != runtime->status->state)
		return IRQ_HANDLED;

	src = (void*)(pvd->buffer.area + (PDM_REG_DPTR(reg) - pvd->buffer.addr));
	dst = (void*)(runtime->dma_area + dma_offset);

#if (0)
    {
		static long ts = 0;
		long new = ktime_to_ms(ktime_get());
		long df = new - ts;
		ts = new;
		printk("[CPY 0x%p (0x%08x)-> %p (0x%08x)] (%d:%d:%d) (%3ld)\n",
			src, PDM_REG_DPTR(reg), dst, dma_offset, pvd->period_bytes, prtd->period_bytes, frames_to_bytes(runtime, 1), df);

    }
#endif
	memcpy(dst, src, pvd->period_bytes);

	prtd->offset += pvd->period_bytes;
	if (prtd->offset >= snd_pcm_lib_buffer_bytes(substream))
		prtd->offset = 0;

	snd_pcm_period_elapsed(substream);

	return IRQ_HANDLED;
}

static int nxp_pcm_dma_prepare_and_submit(struct snd_pcm_substream *substream)
{
	struct nxp_pcm_runtime_data *prtd = substream_to_prtd(substream);

	/* dma offset */
	prtd->offset = 0;

	pr_debug("%s: %s\n", __func__, STREAM_STR(substream->stream));
	pr_debug("buffer_bytes=%6d, period_bytes=%6d, periods=%2d, rate=%6d\n",
		snd_pcm_lib_buffer_bytes(substream), snd_pcm_lib_period_bytes(substream),
		substream->runtime->periods, substream->runtime->rate);

	return 0;
}

static int nxp_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct nxp_pcm_runtime_data *prtd = substream_to_prtd(substream);
	struct nxp_pcm_virtual_data *pvd = prtd->private_data;
	struct nxp_cpu_pdm_reg *reg = (struct nxp_cpu_pdm_reg *)pvd->devptr;

	int ret = 0;
	pr_debug("%s: %s cmd=%d\n", __func__, STREAM_STR(substream->stream), cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		ret = nxp_pcm_dma_prepare_and_submit(substream);
		if (ret)
			return ret;

		PDM_REG_RATE (reg, 0);	// free running -> OK
		PDM_REG_RATE (reg, 48000);

		PDM_REG_START(reg);
		break;

	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		break;

	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		break;

	case SNDRV_PCM_TRIGGER_STOP:
		PDM_REG_STOP(reg);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static snd_pcm_uframes_t nxp_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct nxp_pcm_runtime_data *prtd = substream_to_prtd(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	return bytes_to_frames(runtime, prtd->offset);
}

static int nxp_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	static struct snd_pcm_hardware *hw = &nxp_pcm_hardware;
	struct nxp_pcm_runtime_data *prtd;
	struct nxp_pcm_virtual_data *pvd;
	unsigned long addr = PDM_VIRTUAL_MEM_MAP_BASE;
	unsigned int  size = PDM_VIRTUAL_MEM_MAP_SIZE;
	struct nxp_cpu_pdm_reg *reg = NULL;
	int ret = 0;

	pr_debug("%s %s\n", __func__, STREAM_STR(substream->stream));
	prtd = kzalloc(sizeof(*prtd) + sizeof(*pvd), GFP_KERNEL);
	if (prtd == NULL)
		return -ENOMEM;

	runtime->private_data = prtd;
	pvd = runtime->private_data + sizeof(*prtd);

	prtd->dma_param = snd_soc_dai_get_dma_data(rtd->cpu_dai, substream);
	prtd->private_data = pvd;

	pvd->irq = IRQ_PHY_PDM;
	pvd->period_size = PDM_VIRTUAL_MEM_PERIOD_SIZE;
	pvd->period_bytes = PDM_VIRTUAL_MEM_PERIOD_BYTES;
	pvd->buffer_bytes = pvd->period_bytes * pvd->period_size;
	pvd->offset = 0;
	pvd->buffer.addr = addr;
	pvd->buffer.bytes = size;

	pvd->buffer.area = ioremap_nocache(pvd->buffer.addr, pvd->buffer.bytes);
	if (!prtd->private_data) {
		printk("ERR: %s pdm virtual device memory map 0x%x (%d) !!!\n",
			__func__, pvd->buffer.addr, pvd->buffer.bytes);
		return -ENOMEM;
	}
	pvd->devptr = (void*)(pvd->buffer.area + 0x80000);
	reg = (struct nxp_cpu_pdm_reg *)pvd->devptr;

	PDM_REG_STOP(reg);
	msleep(15);

	ret = request_irq(pvd->irq, nxp_pcm_dma_complete,
						0, "nxp-pdm-virt-irq", substream);
	if (0 > ret) {
		printk("ERR: %s pdm virtual irq.%d !!!\n", __func__, pvd->irq);
		return ret;
	}
	pr_debug("%s %s pdm:0x%p (0x%x), irq:%d\n",
		__func__, STREAM_STR(substream->stream),
		pvd->buffer.area, (unsigned int)pvd->buffer.addr, pvd->irq);

	ret = snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);
	if (0 > ret)
		return ret;

	return snd_soc_set_runtime_hwparams(substream, hw);
}

static int nxp_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct nxp_pcm_runtime_data *prtd = runtime->private_data;
	struct nxp_pcm_virtual_data *pvd = prtd->private_data;

	pr_debug("%s %s\n", __func__, STREAM_STR(substream->stream));
	free_irq(pvd->irq, substream);
	iounmap(pvd->buffer.area);
	kfree(prtd);

	return 0;
}

static int nxp_pcm_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct nxp_pcm_runtime_data *prtd = substream_to_prtd(substream);
	struct nxp_pcm_virtual_data *pvd = prtd->private_data;

	/* debug info */
	prtd->periods = params_periods(params);
	prtd->period_bytes = params_period_bytes(params);
	prtd->buffer_bytes = params_buffer_bytes(params);

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);

	if (pvd->period_bytes != prtd->period_bytes) {
		printk("E: period bytes %d is invalid (%d bytes %d size)\n",
			prtd->period_bytes, pvd->period_bytes,
			frames_to_bytes(substream->runtime, pvd->period_bytes));
		return -EINVAL;
	}
	/*
	 * debug msg
	 */
	pr_debug("%s: %s\n", __func__, STREAM_STR(substream->stream));
	pr_debug("buffer_size =%6d, period_size =%6d, periods=%2d, rate=%6d\n\n",
		params_buffer_size(params),	params_period_size(params),
		params_periods(params), params_rate(params));
	pr_debug("buffer_bytes=%6d, period_bytes=%6d, periods=%2d\n",
		prtd->buffer_bytes, prtd->period_bytes, prtd->periods);
	return 0;
}

static int nxp_pcm_hw_free(struct snd_pcm_substream *substream)
{
	snd_pcm_set_runtime_buffer(substream, NULL);
	return 0;
}

static int nxp_pcm_mmap(struct snd_pcm_substream *substream,
		struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	return dma_mmap_writecombine(substream->pcm->card->dev, vma,
					runtime->dma_area,
					runtime->dma_addr,
					runtime->dma_bytes);
}

static struct snd_pcm_ops nxp_pcm_ops = {
	.open		= nxp_pcm_open,
	.close		= nxp_pcm_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= nxp_pcm_hw_params,
	.hw_free	= nxp_pcm_hw_free,
	.trigger	= nxp_pcm_trigger,
	.pointer	= nxp_pcm_pointer,
	.mmap		= nxp_pcm_mmap,
};

static int nxp_pcm_preallocate_dma_buffer(struct snd_pcm *pcm, int stream)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	size_t size = nxp_pcm_hardware.buffer_bytes_max;

	pr_debug("%s: %s, dma_alloc_writecombine %d byte\n",
		__func__, STREAM_STR(substream->stream), size);

	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
	buf->private_data = NULL;
	buf->bytes = size;
	buf->area = dma_alloc_writecombine(buf->dev.dev, size, &buf->addr, GFP_KERNEL);
	if (!buf->area) {
		printk(KERN_ERR "Fail, %s dma buffer allocate (%d)\n",
			STREAM_STR(substream->stream), size);
		return -ENOMEM;
	}

	pr_debug("%s: %s, dma_alloc_writecombine %d byte, vir = 0x%x, phy = 0x%x\n",
		__func__, STREAM_STR(substream->stream), size,
		(unsigned int)buf->area, buf->addr);
	return 0;
}

static void nxp_pcm_release_dma_buffer(struct snd_pcm *pcm, int stream)
{
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;

	substream = pcm->streams[stream].substream;
	if (!substream)
		return;

	buf = &substream->dma_buffer;
	if (!buf->area)
		return;

	dma_free_writecombine(pcm->card->dev, buf->bytes, buf->area, buf->addr);
	buf->area = NULL;
}

static u64 nxp_pcm_dmamask = DMA_BIT_MASK(32);

static int nxp_pcm_new(struct snd_soc_pcm_runtime *runtime)
{
	struct snd_card *card = runtime->card->snd_card;
	struct snd_pcm *pcm = runtime->pcm;
	int ret = -EINVAL;

	pr_debug("%s\n", __func__);

	/* dma mask */
	if (!card->dev->dma_mask)
		card->dev->dma_mask = &nxp_pcm_dmamask;
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(32);

	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream)
		ret = nxp_pcm_preallocate_dma_buffer(pcm, SNDRV_PCM_STREAM_CAPTURE);

	return ret;
}

static void nxp_pcm_free(struct snd_pcm *pcm)
{
	nxp_pcm_release_dma_buffer(pcm, SNDRV_PCM_STREAM_CAPTURE);
}

static struct snd_soc_platform_driver pcm_platform = {
	.ops		= &nxp_pcm_ops,
	.pcm_new	= nxp_pcm_new,
	.pcm_free	= nxp_pcm_free,
};

static int __devinit nxp_pcm_probe(struct platform_device *pdev)
{
	int ret = snd_soc_register_platform(&pdev->dev, &pcm_platform);
	printk("SND PCM: %s sound platform '%s'\n", ret?"fail":"register", pdev->name);
	return ret;
}

static int __devexit nxp_pcm_remove(struct platform_device *pdev)
{
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static struct platform_driver pcm_driver = {
	.driver = {
		.name  = "nxp-pcm-virt",
		.owner = THIS_MODULE,
	},
	.probe = nxp_pcm_probe,
	.remove = __devexit_p(nxp_pcm_remove),
};

static struct platform_device pcm_device = {
	.name	= "nxp-pcm-virt",
	.id		= -1,
};

static int __init nxp_pcm_init(void)
{
	platform_device_register(&pcm_device);
	return platform_driver_register(&pcm_driver);
}

static void __exit nxp_pcm_exit(void)
{
	platform_driver_unregister(&pcm_driver);
	platform_device_unregister(&pcm_device);
}

module_init(nxp_pcm_init);
module_exit(nxp_pcm_exit);

MODULE_AUTHOR("jhkim <jhkim@nexell.co.kr>");
MODULE_DESCRIPTION("Sound PCM driver for the SLSI");
MODULE_LICENSE("GPL");

