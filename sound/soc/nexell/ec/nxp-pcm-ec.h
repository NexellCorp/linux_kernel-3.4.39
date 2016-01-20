/*
 *
 */

#ifndef __NXP_PCM_H__
#define __NXP_PCM_H__

#include <mach/devices.h>
#include <linux/amba/pl08x.h>
#include "../nxp-i2s.h"
#include "resample.h"

#define SND_SOC_PCM_FORMATS	SNDRV_PCM_FMTBIT_U8 | SNDRV_PCM_FMTBIT_S8	|	\
							SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_U16_LE |	\
			    			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_U24_LE

struct nxp_pcm_dma_param {
	bool		 active;
	bool  	  (*dma_filter)(struct dma_chan *chan, void *filter_param);
	char 	   *dma_ch_name;
	dma_addr_t	peri_addr;
	int	 		bus_width_byte;
	int	 		max_burst_byte;
	unsigned int real_clock;
	long long start_time_ns;
	bool is_run;
};

struct nxp_pcm_runtime_data {
	/* hw params */
	int channels;
	int period_size;
	int period_bytes;
	int periods;
	int buffer_bytes;
	unsigned int sample_offset;	/* alsa buffer offset */
	unsigned int dma_offset;	/* hw dma offset */
	/* DMA param */
	struct dma_chan  *dma_chan;
	struct nxp_pcm_dma_param *dma_param;

	/* capture hw dma for resampler */
	struct snd_dma_buffer dma_buffer;
	int trans_period;
	u64 total_counts;
	u64 total_times;
	struct timespec *ts;	/* periods timespec arrays */
	void *private_data;
	/* for resampler */
	int devno;
	struct task_struct *task;
	bool task_running;
	ReSampleContext *resampler;

	int   input_rate;
	int   output_rate;
	bool  run_resampler;
	bool  rate_changed;
	int   rate_detect_cnt;
	void *rs_buffer;	/* for resampler */
	int dma_avail_size;
	/* even work */
	int hw_channel_no;
	struct work_struct work;
	struct hrtimer rate_timer;
	long rate_duration_us;
	bool sample_exist;
	bool is_run_resample;
	bool resample_closed;
	spinlock_t 	lock;
	struct mutex mutex;
	struct snd_pcm_substream *substream;
	char message[256];
};

#define	STREAM_STR(dir)	(SNDRV_PCM_STREAM_PLAYBACK == dir ? "playback" : "capture ")

#define	SNDDEV_STATUS_CLEAR		(0)
#define	SNDDEV_STATUS_SETUP		(1<<0)
#define	SNDDEV_STATUS_POWER		(1<<1)
#define	SNDDEV_STATUS_PLAY		(1<<2)
#define	SNDDEV_STATUS_CAPT		(1<<3)
#define	SNDDEV_STATUS_RUNNING	(SNDDEV_STATUS_PLAY | SNDDEV_STATUS_CAPT)

#endif
