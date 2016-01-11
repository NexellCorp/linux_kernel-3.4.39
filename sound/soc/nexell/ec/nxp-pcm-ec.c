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
#include "nxp-pcm-ec.h"
#include "nxp-pcm-timer.h"

/*
#define pr_debug		printk
*/

#define	CFG_SND_PCM_CAPTURE_INPUT_RATE				48000	// 48000
#define	CFG_SND_PCM_CAPTURE_RESAMPLE_HZ				16000	// 16000
#define	CFG_SND_PCM_CAPTURE_RESAMPLE_COPY			0
#define	CFG_SND_PCM_CAPTURE_RESAMPLEER_ON			true
#define	CFG_SND_PCM_SKIP_1ST_FRAME					false

//#define	CFG_SND_PCM_CAPTURE_SAMPLE_DETECT

/* Sample define */
#define	SAMPLE_DETECT_COUNT							4	// 3
#define	SAMPLE_DETECT_DELTA							1000
#define	F_DIV	1000	// 1000 E: 100000

/*
 * PCM INFO
 */
#define	PERIOD_BYTES_MAX		8192

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
	.buffer_bytes_max	= 64 * 1024 * 2,
	.period_bytes_min	= 32,
	.period_bytes_max	= PERIOD_BYTES_MAX,
	.periods_min		= 2,
	.periods_max		= 64,
	.fifo_size			= 32,
};
#define	substream_to_prtd(s)	(substream->runtime->private_data)

#define	CHK_PERIOD_COUNTS			10
#define FILTER_DEPTH				CHK_PERIOD_COUNTS

#define	SAMPLE_RATE_HZ(frames, time)	(div_u64((1000000000*(u64)frames), time))
#define	SAMPLE_PERIOD_NS(ns)			(1000000000/ns)

#define	SAMPLE_FRAME_NS_16000		SAMPLE_PERIOD_NS(16000)
#define	SAMPLE_FRAME_NS_44100		SAMPLE_PERIOD_NS(44100)
#define	SAMPLE_FRAME_NS_48000		SAMPLE_PERIOD_NS(48000)	// 48000
#define	SAMPLE_FRAME_NS_96000		SAMPLE_PERIOD_NS(96000)

static int sample_rate_table[] = { 44100, 48000, 96000 } ;
static int pcm_rate_chaned_event_up = false;
static int pcm_sample_rate_hz = CFG_SND_PCM_CAPTURE_INPUT_RATE;

static int find_sample_rate(int *table, int table_size, int rate)
{
	int i = 0, min = 0, new = 0;
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

/*
 * DMA resample buffer
 */
static int nxp_pcm_dma_mem_allocate(struct snd_pcm *pcm,
					struct snd_pcm_substream *substream)
{
	struct nxp_pcm_runtime_data *prtd = substream_to_prtd(substream);
	struct snd_dma_buffer *buf = &prtd->dma_buffer;
	size_t size = nxp_pcm_hardware.buffer_bytes_max;

	if (false == prtd->run_resampler ||
		substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		return 0;

	pr_debug("%s: %s, request dma %d byte\n",
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

	prtd->rs_buffer = kmalloc(PERIOD_BYTES_MAX, GFP_KERNEL);
	if (!prtd->rs_buffer) {
		printk(KERN_ERR "Fail, %s for resampler buffer (%d)\n",
			STREAM_STR(substream->stream), PERIOD_BYTES_MAX);
		return -ENOMEM;
	}

	pr_debug("%s: %s, dma_alloc_writecombine %d byte, v=0x%x, p=0x%x\n",
		__func__, STREAM_STR(substream->stream), size,
		(unsigned int)buf->area, buf->addr);

	return 0;
}

static void nxp_pcm_dma_mem_free(struct snd_pcm *pcm,
					struct snd_pcm_substream *substream)
{
	struct nxp_pcm_runtime_data *prtd;
	struct snd_dma_buffer *buf;

	if (!substream ||
		 substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		return;

	prtd = substream_to_prtd(substream);
	if (!prtd || false == prtd->run_resampler)
		return;

	buf = &prtd->dma_buffer;
	if (!buf->area)
		return;

	pr_debug("%s: %s, release dma %d byte, v=0x%x, p=0x%x\n",
		__func__, STREAM_STR(substream->stream),
		buf->bytes, (unsigned int)buf->area, buf->addr);

	dma_free_writecombine(pcm->card->dev, buf->bytes, buf->area, buf->addr);
	kfree(prtd->rs_buffer);
}

#define	RESAMPLE_INC(s, f, l)	{ s += f; if (s > l) s = f; }
#define	RESAMPLE_DEC(s, f, l)	{ s -= f; if (0 > s) s = l-f; }

static inline void get_time_stamp(struct pcm_timer_data *tm,
						struct timespec *ts)
{
	u_long tcnt = __raw_readl(TIMER_CNTO(tm->channel));
	tcnt = (tcnt == 0 ? tm->tcount : tcnt);
	ts->tv_sec = tm->ts.tv_sec;
	ts->tv_nsec = (tm->tcount - tcnt) * tm->nsec;
}

static int nxp_pcm_dma_capture_resample(void *data)
{
	struct snd_pcm_substream *substream = data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm *pcm = rtd->pcm;
	struct nxp_pcm_runtime_data *prtd = substream_to_prtd(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct kobject *kobj = &pcm->card->dev->kobj;
	int out_bytes = snd_pcm_lib_period_bytes(substream);
	int buffer_bytes = snd_pcm_lib_buffer_bytes(substream);
#if !(CFG_SND_PCM_CAPTURE_RESAMPLE_COPY)
	int frame_bytes = frames_to_bytes(runtime, 1);
	int frame_size = out_bytes/frame_bytes;
	int out_frames;
#endif
	void *src, *dst;

	if (false == prtd->run_resampler ||
		substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		return 0;

	prtd->task_running = true;

	while (!kthread_should_stop()) {

		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
		 __set_current_state(TASK_RUNNING);

		if (prtd->rate_changed &&
			(false == pcm_rate_chaned_event_up)) {
			char  message[64];
			char *envp[] = { message, NULL };
			struct pcm_timer_data *tm = prtd->private_data;
			struct timespec ts;

			get_time_stamp(tm, &ts);

			printk("RATE CHANE EVENT [%s][%6d.%9ld] [%d hz -> %d hz]\n",
				prtd->dma_param->dma_ch_name,
				(int)ts.tv_sec, (long)ts.tv_nsec, pcm_sample_rate_hz,(int)prtd->input_rate);

			sprintf(message, "SAMPLERATE_CHANGED=%d", (int)prtd->input_rate);
			kobject_uevent_env(kobj, KOBJ_CHANGE, envp);
			prtd->rate_changed = false;
			prtd->rate_detect_cnt = 0;
			pcm_rate_chaned_event_up = true;
			pcm_sample_rate_hz = prtd->input_rate;
		}

	#if (CFG_SND_PCM_CAPTURE_RESAMPLE_COPY)
		src = (void*)(prtd->dma_buffer.area + prtd->dma_offset);	/* hw */
		dst = (void*)(runtime->dma_area + prtd->sample_offset);
		memcpy(dst, src, snd_pcm_lib_period_bytes(substream));
	#else

		src = (void*)(prtd->dma_buffer.area + prtd->dma_offset);
		dst = prtd->rs_buffer;

		out_frames = audio_resample(prtd->resampler, (short*)dst, (short*)src, frame_size);
		out_bytes = out_frames * frame_bytes;

		src = prtd->rs_buffer;
		dst = (void*)(runtime->dma_area + prtd->sample_offset);

		if (out_bytes > prtd->dma_avail_size) {
			/* copy */
			memcpy(dst, src, prtd->dma_avail_size);

			/* remain samples */
			src = prtd->rs_buffer + prtd->dma_avail_size;
			dst = (void*)(runtime->dma_area);
			out_bytes -= prtd->dma_avail_size;
			prtd->dma_avail_size = buffer_bytes;

			/* notify */
			prtd->sample_offset = 0;
			snd_pcm_period_elapsed(substream);
		}

		memcpy(dst, src, out_bytes);
		prtd->dma_avail_size -= out_bytes;
	#endif

		/* next sample offset */
		prtd->sample_offset += out_bytes;
		if (prtd->sample_offset >= buffer_bytes)
			prtd->sample_offset = 0;

		/* next dma offset */
		prtd->dma_offset += snd_pcm_lib_period_bytes(substream);
		if (prtd->dma_offset >= buffer_bytes)
			prtd->dma_offset = 0;

		snd_pcm_period_elapsed(substream);
	}

	prtd->task_running = false;
	pr_debug("Exit %s resampler ....\n", STREAM_STR(substream->stream));
	return 0;
}

/*
 * PCM INTERFACE
 */
static void nxp_pcm_dma_complete(void *arg)
{
	struct snd_pcm_substream *substream = arg;
	struct nxp_pcm_runtime_data *prtd = substream_to_prtd(substream);
	struct pcm_timer_data *tm = prtd->private_data;
	int trans_period = prtd->trans_period;

	prtd->trans_period++;
	prtd->total_counts++;

	if (prtd->trans_period >= prtd->periods)
		prtd->trans_period = 0;

#if 1
	if (prtd->run_resampler &&
		substream->stream == SNDRV_PCM_STREAM_CAPTURE)
	{
		struct timespec *ts = &prtd->ts[prtd->trans_period];
		u64 t1, t2, jt;
		u64 totals = prtd->total_counts;
		int rate_hz;

		get_time_stamp(tm, ts);

		t1 = ((u64)prtd->ts[trans_period].tv_sec*1000000000) + (prtd->ts[trans_period].tv_nsec);
		t2 = ((u64)ts->tv_sec*1000000000) + (ts->tv_nsec);
		jt = (t2-t1);

		prtd->total_times += jt;
		rate_hz	= (int)SAMPLE_RATE_HZ(prtd->period_size, jt);

		#if 0
		printk("(%6llu)[%09llu:%6d(%6d)][%6d](%s)\n",
			totals, jt, prtd->period_bytes, prtd->period_size, (int)rate_hz, prtd->dma_param->dma_ch_name);
		#endif

		/*
		 * detect samplerate change
		 */
		if (abs(prtd->input_rate - rate_hz) > SAMPLE_DETECT_DELTA) {
			printk("RATE %d (%6llu:%012llu)(%6llu:%4d)[%6d]->[%6d] (%s:%d)\n",
				prtd->rate_detect_cnt, totals, prtd->total_times, jt, prtd->period_size,
				(int)prtd->input_rate, (int)rate_hz,
				prtd->dma_param->dma_ch_name, prtd->dma_chan->chan_id);

			prtd->rate_detect_cnt++;
#ifdef CFG_SND_PCM_CAPTURE_SAMPLE_DETECT
			if (SAMPLE_DETECT_COUNT > prtd->rate_detect_cnt)
				goto done_complete;

			if ((sample_rate_table[0] - SAMPLE_DETECT_DELTA) > rate_hz ||
				rate_hz > (sample_rate_table[ARRAY_SIZE(sample_rate_table)-1] +
				SAMPLE_DETECT_DELTA)) {
					printk("W: invalid sample %d hz, retry ...\n", (int)rate_hz);
					prtd->rate_detect_cnt = 0;
					goto done_complete;
			}

			prtd->input_rate =
					find_sample_rate(sample_rate_table, ARRAY_SIZE(sample_rate_table), rate_hz);;
			prtd->rate_changed = true;
#endif
		}
	}
#endif

done_complete:
	if (prtd->task)
		wake_up_process(prtd->task);

	if (prtd->run_resampler)
		return;

	prtd->dma_offset += snd_pcm_lib_period_bytes(substream);
	if (prtd->dma_offset >= snd_pcm_lib_buffer_bytes(substream))
		prtd->dma_offset = 0;

	snd_pcm_period_elapsed(substream);
}

static int nxp_pcm_dma_request_channel(void *runtime_data, int stream)
{
	struct nxp_pcm_runtime_data *prtd = runtime_data;
	dma_filter_fn filter_fn;
	void *filter_data;
	dma_cap_mask_t mask;

	if (NULL == prtd || NULL == prtd->dma_param)
		return -ENXIO;

	filter_fn   = prtd->dma_param->dma_filter;
	filter_data = prtd->dma_param->dma_ch_name;

	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE , mask);
	dma_cap_set(DMA_CYCLIC, mask);
	pr_debug("request %s dma '%s'\n", STREAM_STR(stream), (char*)filter_data);

	prtd->dma_chan = dma_request_channel(mask, filter_fn, filter_data);
	if (!prtd->dma_chan) {
		printk(KERN_ERR "Error: %s dma '%s'\n", STREAM_STR(stream), (char*)filter_data);
		return -ENXIO;
	}

	return 0;
}

static void nxp_pcm_dma_release_channel(void *runtime_data)
{
	struct nxp_pcm_runtime_data *prtd = runtime_data;
	if (prtd && prtd->dma_chan)
		dma_release_channel(prtd->dma_chan);
	pr_debug("release dma '%s'\n", (char*)prtd->dma_param->dma_ch_name);
}

static int nxp_pcm_dma_slave_config(void *runtime_data, int stream)
{
	struct nxp_pcm_runtime_data *prtd = runtime_data;
	struct nxp_pcm_dma_param *dma_param = prtd->dma_param;
	struct dma_slave_config slave_config = { 0, };
	dma_addr_t	peri_addr = dma_param->peri_addr;
	int	bus_width = dma_param->bus_width_byte;
	int	max_burst = dma_param->max_burst_byte/bus_width;
	int ret;

	if (SNDRV_PCM_STREAM_PLAYBACK == stream) {
		slave_config.direction 		= DMA_MEM_TO_DEV;
		slave_config.dst_addr 		= peri_addr;
		slave_config.dst_addr_width = bus_width;
		slave_config.dst_maxburst 	= max_burst;
		slave_config.src_addr_width = bus_width;
		slave_config.src_maxburst 	= max_burst;
		slave_config.device_fc 		= false;
	} else {
		slave_config.direction 		= DMA_DEV_TO_MEM;
		slave_config.src_addr 		= peri_addr;
		slave_config.src_addr_width = bus_width;
		slave_config.src_maxburst 	= max_burst;
		slave_config.dst_addr_width = bus_width;
		slave_config.dst_maxburst 	= max_burst;
		slave_config.device_fc 		= false;
	}

	ret = dmaengine_slave_config(prtd->dma_chan, &slave_config);

	pr_debug("%s: %s %s, %s, addr=0x%x, bus=%d byte, burst=%d (%d)\n",
		__func__, ret?"FAIL":"DONE", STREAM_STR(stream),
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
	dma_addr_t dma_addr = runtime->dma_addr;

    if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
        direction = DMA_MEM_TO_DEV;
    else
        direction = DMA_DEV_TO_MEM;

	/* dma offset */
	prtd->sample_offset = 0;
	prtd->dma_offset = 0;
	prtd->trans_period = 0;
	prtd->total_counts = 0;
	prtd->total_times = 0;
	prtd->dma_avail_size = snd_pcm_lib_buffer_bytes(substream);

  	if (prtd->run_resampler && substream->stream == SNDRV_PCM_STREAM_CAPTURE)
 		dma_addr = prtd->dma_buffer.addr;

	desc = dmaengine_prep_dma_cyclic(chan,
				dma_addr,
				snd_pcm_lib_buffer_bytes(substream),
				snd_pcm_lib_period_bytes(substream), direction);

	if (!desc) {
		printk(KERN_ERR "%s: cannot prepare slave %s dma (0x%lx)\n",
			__func__, prtd->dma_param->dma_ch_name, (ulong)dma_addr);
		return -EINVAL;
	}

	desc->callback = nxp_pcm_dma_complete;
	desc->callback_param = substream;
	dmaengine_submit(desc);

	pr_debug("%s: %s\n", __func__, STREAM_STR(substream->stream));
	pr_debug("buffer_bytes=%6d, period_bytes=%6d, periods=%2d, rate=%6d, dma (0x%lx)\n",
		snd_pcm_lib_buffer_bytes(substream), snd_pcm_lib_period_bytes(substream),
		runtime->periods, runtime->rate, (ulong)dma_addr);

	return 0;
}

static int nxp_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct nxp_pcm_runtime_data *prtd = substream_to_prtd(substream);
	struct nxp_pcm_dma_param *dma_param = prtd->dma_param;
	struct timespec *ts = &prtd->ts[0];

	struct pcm_timer_data *tm = prtd->private_data;
	int ch = tm->channel;
	u_long tcount = tm->tcount;
	u_long tcnt, flags;
	int ret = 0;

	pr_debug("%s: %s cmd=%d [%d]\n", __func__, STREAM_STR(substream->stream), cmd, dma_param->is_run);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		ret = nxp_pcm_dma_prepare_and_submit(substream);
		if (ret)
			return ret;
		dma_async_issue_pending(prtd->dma_chan);

		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			spin_lock_irqsave(&tm->lock, flags);
			tcnt = __raw_readl(TIMER_CNTO(ch));
			ts->tv_sec = tm->ts.tv_sec;
			spin_unlock_irqrestore(&tm->lock, flags);
			ts->tv_nsec = (tcount - tcnt) * tm->nsec;
		}
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
	unsigned int offset = prtd->run_resampler ? prtd->sample_offset : prtd->dma_offset;

	return bytes_to_frames(runtime, offset);
}

static int nxp_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm *pcm = rtd->pcm;
	struct snd_pcm_hardware *hw = &nxp_pcm_hardware;
	struct nxp_pcm_runtime_data *prtd;
	struct pcm_timer_data *tm = &pcm_timer;
	char *dma_name;
	int ret = 0;

	pr_debug("%s %s\n", __func__, STREAM_STR(substream->stream));
	prtd = kzalloc(sizeof(struct nxp_pcm_runtime_data), GFP_KERNEL);
	if (prtd == NULL) {
		printk(KERN_ERR "Error: %s %s dma runtime allocate %d\n",
			__func__, STREAM_STR(substream->stream),
			sizeof(struct nxp_pcm_runtime_data));
		return -ENOMEM;
	}

	runtime->private_data = prtd;

	prtd->dma_param = snd_soc_dai_get_dma_data(rtd->cpu_dai, substream);
	ret = nxp_pcm_dma_request_channel(prtd, substream->stream);
	if (0 > ret)
		return ret;

	ret = snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);
	if (0 > ret) {
		nxp_pcm_dma_release_channel(prtd);
		return ret;
	}

	prtd->private_data = &pcm_timer;
	tm->data = prtd;

	/* resampler  */
	if (prtd->dma_chan->chan_id == DMA_PERIPHERAL_ID_I2S0_RX ||
		prtd->dma_chan->chan_id == DMA_PERIPHERAL_ID_I2S1_RX ||
		prtd->dma_chan->chan_id == DMA_PERIPHERAL_ID_I2S2_RX) {
		prtd->run_resampler = CFG_SND_PCM_CAPTURE_RESAMPLEER_ON;
		prtd->devno = prtd->dma_chan->chan_id == DMA_PERIPHERAL_ID_I2S0_RX ? 0 :
				prtd->dma_chan->chan_id == DMA_PERIPHERAL_ID_I2S1_RX ? 1 : 2;
		pr_debug("***[%d:%s resampler ON]***\n",
			prtd->dma_chan->chan_id, prtd->dma_param->dma_ch_name);
	}

	ret = nxp_pcm_dma_mem_allocate(pcm, substream);
	if (0 > ret)
		return ret;

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
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm *pcm = rtd->pcm;
	struct nxp_pcm_runtime_data *prtd = runtime->private_data;

	pr_debug("%s %s\n", __func__, STREAM_STR(substream->stream));
	nxp_pcm_dma_mem_free(pcm, substream);
	nxp_pcm_dma_release_channel(prtd);
	kfree(prtd);
	return 0;
}

static int nxp_pcm_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct nxp_pcm_runtime_data *prtd = substream_to_prtd(substream);
	struct timespec *ts = NULL;
	int periods = params_periods(params);
	int ret;

	ts = kzalloc((periods * sizeof(*ts)), GFP_KERNEL);
	if (!ts)
		return -ENOMEM;

	ret = nxp_pcm_dma_slave_config(prtd, substream->stream);
	if (0 > ret)
		return ret;

	prtd->channels = params_channels(params);
	prtd->periods = periods;
	prtd->period_size = params_period_size(params);
	prtd->period_bytes = params_period_bytes(params);
	prtd->buffer_bytes = params_buffer_bytes(params);
	prtd->rate_changed = false;
	prtd->rate_detect_cnt = 0;
	prtd->ts = ts;
	/* resample rate */
	prtd->input_rate = pcm_sample_rate_hz;
	prtd->output_rate = CFG_SND_PCM_CAPTURE_RESAMPLE_HZ;

	pcm_rate_chaned_event_up = false;

	if (prtd->run_resampler) {
		struct task_struct *p;
		int (*func)(void*) = nxp_pcm_dma_capture_resample;
		prtd->resampler = audio_resample_init(prtd->channels,
							prtd->channels, (float)prtd->output_rate,
							(float)prtd->input_rate);

		p = kthread_create(func, substream, "snd-capture-resampler");
		if (IS_ERR(p)) {
			pr_err("Error: %s thread for capture resampler\n", __func__);
			return PTR_ERR(p);
		}
		prtd->task = p;
		pr_debug("%s %s create resampler task (ch:%d %d->%d) (0x%p)...\n",
			__func__, STREAM_STR(substream->stream),
			prtd->channels, (int)prtd->input_rate, (int)prtd->output_rate,
			prtd->resampler);
	}
	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);

	pr_debug("%s: %s\n", __func__, STREAM_STR(substream->stream));
	pr_debug("ch=%d, buffer_size=%6d, period_size=%6d, periods=%2d, rate=%6d, resample (%s)\n\n",
		prtd->channels, params_buffer_size(params), params_period_size(params),
		params_periods(params), params_rate(params), prtd->run_resampler?"O":"X");

	return 0;
}

static int nxp_pcm_hw_free(struct snd_pcm_substream *substream)
{
	struct nxp_pcm_runtime_data *prtd = substream_to_prtd(substream);

	pr_debug("%s: %s\n", __func__, STREAM_STR(substream->stream));

	if (prtd->task)
		kthread_stop(prtd->task);

	if (prtd->resampler) {
		while(prtd->task_running) { };
		audio_resample_close(prtd->resampler);
		prtd->resampler = NULL;
	}

	prtd->task = NULL;
	pr_debug("%s: %d\n", __func__, STREAM_STR(substream->stream), prtd->task_running);

	snd_pcm_set_runtime_buffer(substream, NULL);

	return 0;
}

static int nxp_pcm_mmap(struct snd_pcm_substream *substream,
		struct vm_area_struct *vma)
{
	dma_addr_t dma_addr = substream->runtime->dma_addr;
	unsigned char *dma_area = substream->runtime->dma_area;
	size_t dma_bytes = substream->runtime->dma_bytes;

	pr_debug("%s: %s, dma_mmap_writecombine %d byte, vir = 0x%p, phy = 0x%lx\n",
		__func__, STREAM_STR(substream->stream), dma_bytes, dma_area, (ulong)dma_addr);

	return dma_mmap_writecombine(substream->pcm->card->dev, vma,
					dma_area, dma_addr, dma_bytes);
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

static int nxp_pcm_preallocate_sample_buffer(struct snd_pcm *pcm, int stream)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	size_t size = nxp_pcm_hardware.buffer_bytes_max;

	pr_debug("%s: %s, request dma %d byte\n",
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
		__func__, STREAM_STR(substream->stream), size, (unsigned int)buf->area, buf->addr);
	return 0;
}

static void nxp_pcm_release_sample_buffer(struct snd_pcm *pcm, int stream)
{
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;

	substream = pcm->streams[stream].substream;
	if (!substream)
		return;

	buf = &substream->dma_buffer;
	if (!buf->area)
		return;

	dma_free_writecombine(pcm->card->dev, buf->bytes,
				buf->area, buf->addr);
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
		ret = nxp_pcm_preallocate_sample_buffer(pcm, SNDRV_PCM_STREAM_PLAYBACK);
		if (ret)
			goto err;
	}

	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream) {
		ret = nxp_pcm_preallocate_sample_buffer(pcm, SNDRV_PCM_STREAM_CAPTURE);
		if (ret)
			goto err_free;
	}
	return 0;

err_free:
	nxp_pcm_release_sample_buffer(pcm, SNDRV_PCM_STREAM_PLAYBACK);
err:
	return ret;
}

static void nxp_pcm_free(struct snd_pcm *pcm)
{
	nxp_pcm_release_sample_buffer(pcm, SNDRV_PCM_STREAM_CAPTURE);
	nxp_pcm_release_sample_buffer(pcm, SNDRV_PCM_STREAM_PLAYBACK);
}

static struct snd_soc_platform_driver pcm_platform = {
	.ops		= &nxp_pcm_ops,
	.pcm_new	= nxp_pcm_new,
	.pcm_free	= nxp_pcm_free,
};

static irqreturn_t nxp_pcm_timer_handler(int irq, void *desc)
{
	struct pcm_timer_data *tm = desc;
	int ch = tm->channel;
	struct timespec *ts = &tm->ts;

	spin_lock(&tm->lock);
	ts->tv_sec++;
	spin_unlock(&tm->lock);
	timer_clear(ch);

	return IRQ_HANDLED;
}

static int nxp_pcm_timer_setup(struct pcm_timer_data *pcm_timer)
{
	int ch = pcm_timer->channel;
	int mux = pcm_timer->mux;
	int prescale = pcm_timer->prescale;
	unsigned long tcount = pcm_timer->tcount;
	int irq = pcm_timer->irq;
	struct timespec *ts = &pcm_timer->ts;
	int ret = 0;

	ret = request_irq(irq, &nxp_pcm_timer_handler,
				IRQF_DISABLED, "snd-pcm-lctimer", pcm_timer);
	if (ret) {
		printk("Error: pcm local timer.%d request irq %d \n", ch, irq);
		return ret;
	}

	pcm_timer->nsec = 1000000000/tcount;
	ts->tv_sec = 0, ts->tv_nsec = 0;
	spin_lock_init(&pcm_timer->lock);

	timer_stop (ch, 1);
	timer_clock(ch, mux, prescale);
	timer_count(ch, tcount);
	timer_start(ch, T_IRQ_ON);

	return 0;
}

static void nxp_pcm_timer_free(struct pcm_timer_data *pcm_timer)
{
	int ch = pcm_timer->channel;
	int irq = IRQ_PHY_TIMER_INT0 + ch;

	timer_stop(ch, 1);
	free_irq(irq, pcm_timer);
}

static int __devinit nxp_pcm_probe(struct platform_device *pdev)
{
	struct pcm_timer_data *tm = &pcm_timer;
	int ret;

	tm->channel = LC_TIMER_CHANNEL;
	tm->irq = IRQ_PHY_TIMER_INT0 + LC_TIMER_CHANNEL;
	tm->mux = T_MUX_1_2;
	tm->prescale = 1;
	tm->tcount = LC_TIMER_TCOUNT;

	ret = snd_soc_register_platform(&pdev->dev, &pcm_platform);
	if (!ret)
		ret = nxp_pcm_timer_setup(tm);

	printk("SND PCM: %s sound platform '%s'\n", ret?"fail":"register", pdev->name);
	return ret;
}

static int __devexit nxp_pcm_remove(struct platform_device *pdev)
{
	nxp_pcm_timer_free(&pcm_timer);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static struct platform_driver pcm_driver = {
	.driver = {
		.name  = "nxp-pcm-ec",
		.owner = THIS_MODULE,
	},
	.probe = nxp_pcm_probe,
	.remove = __devexit_p(nxp_pcm_remove),
};

static struct platform_device pcm_device = {
	.name	= "nxp-pcm-ec",
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

