/*
 * Copyright (C) 2016  Nexell Co., Ltd.
 * Author: Hyunseok, Jung <hsjung@nexell.co.kr>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#if 0
#define DEBUG	/* set loglevel=8 for bootup debug message */
#endif

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
#include <linux/uaccess.h>

#include <sound/core.h>
#include <sound/initval.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "nxp-pcm.h"

/*
 * PCM INFO
 */
#define	PERIOD_BYTES_MAX		8192

static struct snd_pcm_hardware nxp_pcm_hardware = {
	/*  | SNDRV_PCM_INFO_BLOCK_TRANSFER */
	.info			= SNDRV_PCM_INFO_MMAP |
					SNDRV_PCM_INFO_MMAP_VALID |
					SNDRV_PCM_INFO_INTERLEAVED |
					SNDRV_PCM_INFO_PAUSE |
					SNDRV_PCM_INFO_RESUME,
	.formats		= SND_SOC_PCM_FORMATS,
	.rate_min		= 8000,
	.rate_max		= 192000,
	.channels_min		= 1,
	.channels_max		= 2,
	.buffer_bytes_max	= 64 * 1024 * 2,
	.period_bytes_min	= 32,
	.period_bytes_max	= PERIOD_BYTES_MAX,
	.periods_min		= 2,
	.periods_max		= 64,
	.fifo_size		= 32,
};
#define	substream_to_prtd(s)	(substream->runtime->private_data)

/*
 * Sample rate dector
 */
#define	SAMPLE_RATE_HZ(frames, time)	(div_u64((1000000*(u64)frames), time))
#define	SAMPLE_DETECT_DELTA		1000
#define SAMPLE_DETECT_TABLE		{ 44100, 48000, 88200, 96000 }
#define	SAMPLE_DETECT_COUNT		2
#define us_to_ktime(u)  ns_to_ktime((u64)u * 1000)

static void nxp_pcm_rate_detect_work(struct work_struct *work)
{
	struct nxp_pcm_rate_detector *detect =
		container_of(work, struct nxp_pcm_rate_detector, work);
	struct snd_pcm_substream *substream = detect->substream;
	struct nxp_pcm_runtime_data *prtd = substream_to_prtd(substream);
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm *pcm = rtd->pcm;
	struct kobject *kobj = &pcm->card->dev->kobj;
	unsigned long flags;
	char *envp[2] = { detect->message, NULL };

	dev_dbg(prtd->dev, "%s: rate changed msg %d -> [%s]\n",
		__func__, detect->i_rate, detect->message);

	kobject_uevent_env(kobj, KOBJ_CHANGE, envp);

	spin_lock_irqsave(&detect->lock, flags);
	detect->i_rate = detect->changed_rate;
	spin_unlock_irqrestore(&detect->lock, flags);
}

static enum hrtimer_restart nxp_pcm_rate_detect_timer(struct hrtimer *hrtimer)
{
	struct nxp_pcm_rate_detector *detect =
		container_of(hrtimer, struct nxp_pcm_rate_detector, timer);
	struct snd_pcm_substream *substream = detect->substream;
	struct nxp_pcm_runtime_data *prtd = substream_to_prtd(substream);
	unsigned long flags;

	if (detect->exist) {
		/* check next sample */
		hrtimer_start(&detect->timer,
			us_to_ktime(detect->duration_us * 2),
			HRTIMER_MODE_REL_PINNED);
	} else {
		int period_bytes = snd_pcm_lib_buffer_bytes(substream);

		sprintf(detect->message, "SAMPLE_NO_DATA=YES");

		spin_lock_irqsave(&detect->lock, flags);
		detect->changed_rate = 0;
		spin_unlock_irqrestore(&detect->lock, flags);

		/* Notify no LRCK */
		detect->cb(detect);

		schedule_work(&detect->work);

		/* Note>
		 * Exit from capture wait status when I2S no signal
		 * call snd_pcm_period_elapsed
		 */
		prtd->offset += period_bytes;

		if (prtd->offset >= period_bytes)
			prtd->offset = 0;

		snd_pcm_period_elapsed(substream);

		detect->d_count = 0;
	}

	spin_lock_irqsave(&detect->lock, flags);
	detect->exist = false;
	spin_unlock_irqrestore(&detect->lock, flags);

	return HRTIMER_NORESTART;
}

static int nxp_pcm_rate_search(int *table, int table_size, int rate)
{
	int i, min, new;
	int find = 0;

	min = abs(table[0] - rate);
	for (i = 0; table_size > i; i++) {
		new = abs(table[i] - rate);
		if (new > min)
			continue;
		find = i, min = new;
	}

	return table[find];
}

static void nxp_pcm_rate_detect_rate(
			struct snd_pcm_substream *substream, s64 us)
{
	struct nxp_pcm_runtime_data *prtd = substream_to_prtd(substream);
	struct nxp_pcm_rate_detector *detect = prtd->rate_detector;
	struct snd_pcm_runtime *runtime = substream->runtime;
	int period_size, count, rate;
	s64 ts;

	count = detect->t_count;
	period_size = runtime->period_size;
	detect->t_count += 1;

	if (detect->t_count >= prtd->periods)
		detect->t_count = 0;

	detect->us[detect->t_count] = us;
	detect->exist = true;

	ts = (detect->us[detect->t_count] - detect->us[count]);
	rate = (int)SAMPLE_RATE_HZ(period_size, ts);

	/*
	 * detect samplerate change
	 */
	if (abs(detect->i_rate - rate) < SAMPLE_DETECT_DELTA)
		return;

	dev_dbg(prtd->dev, "%s: R %6llu:%4d [%6d]->[%6d]\n",
		__func__, ts, period_size, (int)detect->i_rate, (int)rate);
	detect->d_count++;

	/* recheck */
	if (detect->d_count < SAMPLE_DETECT_COUNT)
		return;

	/* invalid sample rate, recheck */
	if ((rate < (detect->table[0] - SAMPLE_DETECT_DELTA)) ||
		(rate > (detect->table[detect->table_size - 1] +
		SAMPLE_DETECT_DELTA))) {
		dev_warn(prtd->dev,
			"Warn: sample %d hz, retry %d...\n",
			(int)rate, detect->d_count);
		detect->d_count = 0;
		return;
	}

	rate = nxp_pcm_rate_search(detect->table,
				ARRAY_SIZE(detect->table), rate);
	if (detect->i_rate == rate) {
		detect->d_count = 0;
		return;
	}
	detect->changed_rate = rate;

	sprintf(detect->message, "SAMPLERATE_CHANGED=%d",
		(int)detect->changed_rate);
	detect->d_count = 0;

	/* sample changed event */
	schedule_work(&detect->work);
}

static void nxp_pcm_rate_detect_complete(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct nxp_pcm_runtime_data *prtd = substream_to_prtd(substream);
	struct nxp_pcm_rate_detector *detect = prtd->rate_detector;
	bool dynamic_rate = true;	/* prtd->dma_param->dfs */

	if (!prtd->run_detector || !detect)
		return;

	if (dynamic_rate) {
		nxp_pcm_rate_detect_rate(
				substream, ktime_to_us(ktime_get()));
		return;
	}

	detect->exist = true;

	if (!detect->i_rate) {
		detect->i_rate = runtime->rate;
		detect->changed_rate = detect->i_rate;
		sprintf(detect->message, "SAMPLERATE_CHANGED=%d",
			(int)detect->changed_rate);
		schedule_work(&detect->work);
	}
}

static int nxp_pcm_rate_detect_submit(struct snd_pcm_substream *substream)
{
	struct nxp_pcm_runtime_data *prtd = substream_to_prtd(substream);
	struct nxp_pcm_rate_detector *detect = prtd->rate_detector;
	long duration;

	if (!prtd->run_detector)
		return 0;

	duration = detect->duration_us * 2; /* wait multiple time */

	hrtimer_start(&detect->timer,
		us_to_ktime(duration), HRTIMER_MODE_REL_PINNED);

	return 0;
}

static int nxp_pcm_rate_detect_params(struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params)
{
	struct nxp_pcm_runtime_data *prtd = substream_to_prtd(substream);
	struct nxp_pcm_rate_detector *detect;
	int table[] = SAMPLE_DETECT_TABLE;

	if (!prtd->run_detector)
		return 0;

	detect = kzalloc(sizeof(*detect), GFP_KERNEL);
	if (!detect)
		return -ENOMEM;

	detect->us = kcalloc(params_periods(params), sizeof(s64), GFP_KERNEL);
	if (!detect->us) {
		kfree(detect);
		return -ENOMEM;
	}

	detect->t_count = 0;
	detect->exist = false;
	detect->i_rate = params_rate(params);
	detect->changed_rate = 0;
	detect->us[0] = ktime_to_us(ktime_get());
	detect->table_size = ARRAY_SIZE(table);
	detect->substream = substream;
	detect->duration_us = (1000000/params_rate(params)) *
			params_period_size(params) + 10000; /* add 10ms */

	memcpy(detect->table, table, sizeof(table));

	spin_lock_init(&detect->lock);
	INIT_WORK(&detect->work, nxp_pcm_rate_detect_work);
	hrtimer_init(&detect->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	detect->timer.function = nxp_pcm_rate_detect_timer;
	prtd->rate_detector = detect;

	return 0;
}

static void nxp_pcm_rate_detect_free(struct snd_pcm_substream *substream)
{
	struct nxp_pcm_runtime_data *prtd = substream_to_prtd(substream);

	if (prtd->run_detector && prtd->rate_detector) {
		struct nxp_pcm_rate_detector *detect = prtd->rate_detector;

		hrtimer_cancel(&detect->timer);
		cancel_work_sync(&detect->work);
		prtd->run_detector = false;

		kfree(detect->us);
		kfree(detect);
	}
}

/*
 * PCM INTERFACE
 */
static void nxp_pcm_dma_clear(struct snd_pcm_substream *substream)
{
	struct nxp_pcm_runtime_data *prtd = substream_to_prtd(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	unsigned offset = prtd->offset;
	int length = snd_pcm_lib_period_bytes(substream);
	void *src_addr = NULL;

	if (offset == 0)
		offset = snd_pcm_lib_buffer_bytes(substream);

	offset -= length;
	src_addr = (void *)(runtime->dma_area + offset);

	if (strstr(prtd->dma_param->dma_ch_name, "i2s")) {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			memset(src_addr, 0, length);
	}
}

static void nxp_pcm_dma_complete(void *arg)
{
	struct snd_pcm_substream *substream = arg;
	struct nxp_pcm_runtime_data *prtd = substream_to_prtd(substream);

	prtd->offset += snd_pcm_lib_period_bytes(substream);

	if (prtd->offset >= snd_pcm_lib_buffer_bytes(substream))
		prtd->offset = 0;

	nxp_pcm_rate_detect_complete(substream);
	nxp_pcm_dma_clear(substream);
	snd_pcm_period_elapsed(substream);
}

static int nxp_pcm_dma_request_channel(void *runtime_data, int stream)
{
	struct nxp_pcm_runtime_data *prtd = runtime_data;
	dma_cap_mask_t mask;

	if (!prtd || !prtd->dma_param)
		return -ENXIO;

	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);
	dma_cap_set(DMA_CYCLIC, mask);

	dev_dbg(prtd->dev, "%s: request %s dma '%s'\n",
		__func__, STREAM_STR(stream),
		(char *)prtd->dma_param->dma_ch_name);

	prtd->dma_chan = dma_request_channel(mask,
				prtd->dma_param->dma_filter,
				prtd->dma_param->dma_ch_name);
	if (!prtd->dma_chan) {
		dev_err(prtd->dev, "Error, %s: %s dma '%s'\n",
			__func__, STREAM_STR(stream),
			(char *)prtd->dma_param->dma_ch_name);
		return -ENXIO;
	}
	return 0;
}

static void nxp_pcm_dma_release_channel(void *runtime_data, int stream)
{
	struct nxp_pcm_runtime_data *prtd = runtime_data;

	if (!prtd || !prtd->dma_chan)
		return;

	dma_release_channel(prtd->dma_chan);

	dev_dbg(prtd->dev, "%s: release dma '%s'\n",
		__func__, (char *)prtd->dma_param->dma_ch_name);
}

static int nxp_pcm_dma_slave_config(void *runtime_data, int stream)
{
	struct nxp_pcm_runtime_data *prtd = runtime_data;
	struct nxp_pcm_dma_param *dma_param = prtd->dma_param;
	struct dma_slave_config slave_config = { 0, };
	dma_addr_t peri_addr = dma_param->peri_addr;
	int bus_width = dma_param->bus_width_byte;
	int max_burst = dma_param->max_burst_byte/bus_width;
	int ret;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		slave_config.direction = DMA_MEM_TO_DEV;
		slave_config.dst_addr = peri_addr;
		slave_config.dst_addr_width = bus_width;
		slave_config.dst_maxburst = max_burst;
		slave_config.src_addr_width = bus_width;
		slave_config.src_maxburst = max_burst;
		slave_config.device_fc = false;
	} else {
		slave_config.direction = DMA_DEV_TO_MEM;
		slave_config.src_addr = peri_addr;
		slave_config.src_addr_width = bus_width;
		slave_config.src_maxburst = max_burst;
		slave_config.dst_addr_width = bus_width;
		slave_config.dst_maxburst = max_burst;
		slave_config.device_fc = false;
	}

	ret = dmaengine_slave_config(prtd->dma_chan, &slave_config);

	dev_dbg(prtd->dev, "%s: %s %s, %s, addr=0x%x, bus=%d byte, burst=%d (%d)\n",
		__func__, ret ? "FAIL" : "DONE", STREAM_STR(stream),
		dma_param->dma_ch_name,	peri_addr, bus_width,
		dma_param->max_burst_byte, max_burst);

	return ret;
}

static int nxp_pcm_dma_prepare_and_submit(struct snd_pcm_substream *substream)
{
	struct nxp_pcm_runtime_data *prtd = substream_to_prtd(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct dma_chan *chan = prtd->dma_chan;
	struct dma_async_tx_descriptor *desc;
	enum dma_transfer_direction direction;

	nxp_pcm_rate_detect_submit(substream);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		direction = DMA_MEM_TO_DEV;
	else
		direction = DMA_DEV_TO_MEM;

	/* dma offset */
	prtd->offset = 0;

	desc = dmaengine_prep_dma_cyclic(chan,
				runtime->dma_addr,
				snd_pcm_lib_buffer_bytes(substream),
				snd_pcm_lib_period_bytes(substream),
				direction);

	if (!desc) {
		dev_err(prtd->dev,
			"Fail, %s: cannot prepare slave %s dma\n",
			__func__, prtd->dma_param->dma_ch_name);
		return -EINVAL;
	}

	desc->callback = nxp_pcm_dma_complete;
	desc->callback_param = substream;
	dmaengine_submit(desc);

	dev_dbg(prtd->dev, "%s:%s\n", __func__, STREAM_STR(substream->stream));
	dev_dbg(prtd->dev,
		"bytes: buffer=%6d, period=%6d, periods=%2d, rate=%6d\n",
		snd_pcm_lib_buffer_bytes(substream),
		snd_pcm_lib_period_bytes(substream),
		runtime->periods, runtime->rate);

	return 0;
}

static int nxp_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct nxp_pcm_runtime_data *prtd = substream_to_prtd(substream);
	int ret = 0;

	dev_dbg(prtd->dev, "%s: %s (%d)\n",
		__func__, STREAM_STR(substream->stream), cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		ret = nxp_pcm_dma_prepare_and_submit(substream);
		if (ret)
			return ret;
		dma_async_issue_pending(prtd->dma_chan);
		break;

	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		dmaengine_resume(prtd->dma_chan);
		break;

	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		dmaengine_pause(prtd->dma_chan);
		break;

	case SNDRV_PCM_TRIGGER_STOP:
		dmaengine_terminate_all(prtd->dma_chan);
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
	char *dma_name;
	int ret = 0;

	dev_dbg(substream->pcm->card->dev,
		"%s: %s\n", __func__, STREAM_STR(substream->stream));

	prtd = kzalloc(sizeof(struct nxp_pcm_runtime_data), GFP_KERNEL);
	if (prtd == NULL)
		return -ENOMEM;

	runtime->private_data = prtd;
	prtd->dev = substream->pcm->card->dev;

	prtd->dma_param = snd_soc_dai_get_dma_data(rtd->cpu_dai, substream);
	ret = nxp_pcm_dma_request_channel(prtd, substream->stream);
	if (ret < 0)
		return ret;

	ret = snd_pcm_hw_constraint_integer(runtime,
					SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0) {
		nxp_pcm_dma_release_channel(prtd, substream->stream);
		return ret;
	}

	/*
	 * change period_bytes_max value for SPDIFTX
	 * SDPIF min bus width is 2 byte for 16bit pcm
	 */
	dma_name = prtd->dma_param->dma_ch_name;
	if (!strcmp(dma_name, DMA_PERIPHERAL_NAME_SPDIFTX))
		hw->period_bytes_max = 4096;
	else
		hw->period_bytes_max = PERIOD_BYTES_MAX;

	return snd_soc_set_runtime_hwparams(substream, &nxp_pcm_hardware);
}

static int nxp_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct nxp_pcm_runtime_data *prtd = runtime->private_data;

	dev_dbg(substream->pcm->card->dev,
		"%s: %s\n", __func__, STREAM_STR(substream->stream));

	nxp_pcm_dma_release_channel(prtd, substream->stream);
	kfree(prtd);

	return 0;
}

static int nxp_pcm_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct nxp_pcm_runtime_data *prtd = substream_to_prtd(substream);
	int ret;

	ret = nxp_pcm_rate_detect_params(substream, params);
	if (ret < 0)
		return ret;

	ret = nxp_pcm_dma_slave_config(prtd, substream->stream);
	if (ret < 0)
		return ret;

	/* debug info */
	prtd->periods = params_periods(params);
	prtd->period_bytes = params_period_bytes(params);
	prtd->buffer_bytes = params_buffer_bytes(params);

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);

	/*
	 * debug msg
	 */
	dev_dbg(prtd->dev, "%s: %s\n", __func__, STREAM_STR(substream->stream));
	dev_dbg(prtd->dev,
		"size: buffer=%6d, period=%6d, periods=%2d, rate=%6d (%6d)\n",
		params_buffer_size(params), params_period_size(params),
		params_periods(params), params_rate(params),
		prtd->dma_param->real_clock);
	dev_dbg(prtd->dev,
		"bytes: buffer=%6d, period=%6d, periods=%2d\n",
		prtd->buffer_bytes, prtd->period_bytes, prtd->periods);

	return 0;
}

static int nxp_pcm_hw_free(struct snd_pcm_substream *substream)
{
	nxp_pcm_rate_detect_free(substream);
	snd_pcm_set_runtime_buffer(substream, NULL);
	return 0;
}

static int nxp_pcm_mmap(struct snd_pcm_substream *substream,
		struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	return dma_mmap_coherent(substream->pcm->card->dev, vma,
					runtime->dma_area,
					runtime->dma_addr,
					runtime->dma_bytes);
}

static struct snd_pcm_ops nxp_pcm_ops = {
	.open = nxp_pcm_open,
	.close = nxp_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = nxp_pcm_hw_params,
	.hw_free = nxp_pcm_hw_free,
	.trigger = nxp_pcm_trigger,
	.pointer = nxp_pcm_pointer,
	.mmap = nxp_pcm_mmap,
};

static int nxp_pcm_preallocate_dma_buffer(struct snd_pcm *pcm, int stream)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	size_t size = nxp_pcm_hardware.buffer_bytes_max;

	dev_dbg(pcm->card->dev,
		"%s: %s, dma_alloc_writecombine %d byte\n",
		__func__, STREAM_STR(substream->stream), size);

	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
	buf->private_data = NULL;
	buf->bytes = size;
	buf->area = dma_alloc_writecombine(buf->dev.dev,
					size, &buf->addr, GFP_KERNEL);
	if (!buf->area) {
		dev_err(pcm->card->dev, "Fail, %s: %s dma allocate(%d)\n",
			__func__, STREAM_STR(substream->stream), size);
		return -ENOMEM;
	}

	dev_dbg(pcm->card->dev,
		"%s: %s, dma alloc %d byte, vir=0x%x, phy=0x%x",
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

	dma_free_writecombine(pcm->card->dev,
			buf->bytes, buf->area, buf->addr);
	buf->area = NULL;
}

static u64 nxp_pcm_dmamask = DMA_BIT_MASK(32);

static int nxp_pcm_new(struct snd_soc_pcm_runtime *runtime)
{
	struct snd_card *card = runtime->card->snd_card;
	struct snd_pcm *pcm = runtime->pcm;
	int ret = 0;

	/* dma mask */
	if (!card->dev->dma_mask)
		card->dev->dma_mask = &nxp_pcm_dmamask;
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(32);

	if (pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream) {
		ret = nxp_pcm_preallocate_dma_buffer(
				pcm, SNDRV_PCM_STREAM_PLAYBACK);
		if (ret)
			goto err;
	}

	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream) {
		ret = nxp_pcm_preallocate_dma_buffer(
				pcm, SNDRV_PCM_STREAM_CAPTURE);
		if (ret)
			goto err_free;
	}
	return 0;

err_free:
	nxp_pcm_release_dma_buffer(pcm, SNDRV_PCM_STREAM_PLAYBACK);
err:
	return ret;
}

static void nxp_pcm_free(struct snd_pcm *pcm)
{
	nxp_pcm_release_dma_buffer(pcm, SNDRV_PCM_STREAM_CAPTURE);
	nxp_pcm_release_dma_buffer(pcm, SNDRV_PCM_STREAM_PLAYBACK);
}

static struct snd_soc_platform_driver nxp_pcm_rate_detector = {
	.ops = &nxp_pcm_ops,
	.pcm_new = nxp_pcm_new,
	.pcm_free = nxp_pcm_free,
};

static int nxp_pcm_probe(struct platform_device *pdev)
{
	int ret = snd_soc_register_platform(
				&pdev->dev, &nxp_pcm_rate_detector);

	dev_info(&pdev->dev, "snd pcm: %s sound platform '%s'\n",
	       ret ? "fail" : "register", pdev->name);
	return ret;
}

static int nxp_pcm_remove(struct platform_device *pdev)
{
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static struct platform_driver pcm_driver = {
	.driver = {
		.name  = "nxp-pcm-rate-detector",
		.owner = THIS_MODULE,
	},
	.probe = nxp_pcm_probe,
	.remove = nxp_pcm_remove,
};

static struct platform_device pcm_device = {
	.name	= "nxp-pcm-rate-detector",
	.id	= -1,
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

MODULE_AUTHOR("hsjung <hsjung@nexell.co.kr>");
MODULE_DESCRIPTION("Sound PCM Rate Detector for Nexell sound");
MODULE_LICENSE("GPL");
