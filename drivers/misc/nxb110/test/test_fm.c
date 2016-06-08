#include "test.h"

#ifdef RTV_FM_ENABLE

/*============================================================================
 * Configuration for File dump
 *===========================================================================*/
//#define _FM_FILE_DUMP_ENABLE /* for PCM data*/

//#define _FM_RDS_FILE_DUMP_ENABLE /* for RDS data*/

/* PCM filename: /data/nexell/fm_FREQ.pcm or fm_FREQ.rds */
#define FM_DUMP_FILENAME_PREFIX	"fm"


#ifdef RTV_FM_RDS_ENABLED
	#define MAX_NUM_FM_FILE		2 /* index 0: PCM, index 1: RDS */
#else
	#define MAX_NUM_FM_FILE		1
#endif

#if defined(_FM_FILE_DUMP_ENABLE) || defined(_FM_RDS_FILE_DUMP_ENABLE)
	static FILE *fd_dump[MAX_NUM_FM_FILE];
	static char fm_fname[MAX_NUM_FM_FILE][64];
#endif


static int fd_fm_dmb_dev; /* MTV device file descriptor. */
#if defined(RTV_IF_TSIF) || defined(RTV_IF_SPI_SLAVE)
static int fd_fm_tsif_dev;
#endif


static BOOL fm_is_power_on;
static BOOL fm_is_ebable_ts;

#ifndef RTV_FM_RDS_ENABLED
static unsigned char fm_single_svc_buf[MAX_READ_TSP_SIZE];
#else
static IOCTL_MULTI_SERVICE_BUF fm_multi_svc_buf;
#endif


//============ Start of file dump =============================
static int fm_open_msc_dump_file(int fidx, unsigned int ch_freq_khz)
{
#ifdef _FM_FILE_DUMP_ENABLE
	if(fd_dump[fidx] != NULL)
	{
		EMSG0("[MTV] Must close dump file before open the new file\n");
		return -1;
	}

	if(fidx == 0)
		sprintf(fm_fname[fidx], "%s/%s_%u.pcm",
			TS_DUMP_DIR, FM_DUMP_FILENAME_PREFIX, ch_freq_khz);
	else
		sprintf(fm_fname[fidx], "%s/%s_%u.rds",
			TS_DUMP_DIR, FM_DUMP_FILENAME_PREFIX, ch_freq_khz);
	
	if((fd_dump[fidx]=fopen(fm_fname[fidx], "wb")) == NULL)
	{
		EMSG1("[MTV] Fail to open error: %s\n", fm_fname[fidx]);
		return -2;
	}

	DMSG1("[MTV] Opend dump file: %s\n", fm_fname[fidx]);
#endif

	return 0;
}

static int fm_close_msc_dump_file(void)
{
#ifdef _FM_FILE_DUMP_ENABLE
	int i;

	for(i=0; i<MAX_NUM_FM_FILE; i++)
	{
		if(fd_dump[i] != NULL)
		{
			fclose(fd_dump[i]);
			fd_dump[i] = NULL;

			DMSG1("[MTV] Closed dump file: %s\n", fm_fname[i]);
		}
	}
#endif

	return 0;
}
//============ END of file dump =============================


static void fm_processing_pcm_ts(unsigned char *buf, unsigned int size)
{
//	DMSG5("\t PCm Size: %d [0x%02X], [0x%02X], [0x%02X], [0x%02X]\n", size, pcm[0], pcm[1], pcm[2], pcm[3]);

#ifdef _FM_FILE_DUMP_ENABLE
	if(fd_dump[0])
		fwrite(buf, sizeof(char), size, fd_dump[0]);
#endif
}

#ifdef RTV_FM_RDS_ENABLED
static void fm_processing_rds_ts(unsigned char *buf, unsigned int size)
{
//	DMSG5("\t RDS Size: %d [0x%02X], [0x%02X], [0x%02X], [0x%02X]\n", size, pcm[0], pcm[1], pcm[2], pcm[3]);

#if defined(_FM_FILE_DUMP_ENABLE) && defined(_FM_RDS_FILE_DUMP_ENABLE)
	if(fd_dump[1])
		fwrite(buf, sizeof(char), size, fd_dump[1]);
#endif
}
#endif

static void fm_read(int dev)
{
	int len;
#ifndef RTV_FM_RDS_ENABLED /* Pcm only */
	len = read(dev, fm_single_svc_buf, MAX_READ_TSP_SIZE);		
	if(len > 0)	
	{
		fm_processing_pcm_ts(fm_single_svc_buf, len);
	}
	else
	{
		usleep(40 * 1000);
	}
	
#else /* PCM + RDS data */
	IOCTL_MULTI_SERVICE_BUF *multi_svc_buf = &fm_multi_svc_buf;
	int i;

	/* Initialisze the size of buffers before call read(). */
	for(i=0; i<MAX_NUM_MTV_MULTI_SVC_BUF; i++)
	{
		multi_svc_buf->av_size[i] = 0;
		multi_svc_buf->data_size[i] = 0; /* Init return size.*/
	}
	
	len = read(dev, multi_svc_buf, MAX_READ_TSP_SIZE);		
	if(len > 0)			
	{	
		for(i=0; i<multi_svc_buf->max_num_item; i++)
		{
			//DMSG3("[%d]: av_size: %u, data_size: %u\n", i, multi_svc_buf->av_size[i], multi_svc_buf->data_size[i]);
				
			if(multi_svc_buf->av_size[i] != 0)
				fm_processing_pcm_ts(multi_svc_buf->av_ts[i], multi_svc_buf->av_size[i]);

			if(multi_svc_buf->data_size[i] != 0)
				fm_processing_rds_ts(multi_svc_buf->data_ts[i], multi_svc_buf->data_size[i]);
		}
	}		
	else
	{
		usleep(24 * 1000);
	}
#endif		
}


void *fm_read_thread(void *arg)
{
#if defined(RTV_IF_SPI) 	
	int dev = fd_fm_dmb_dev;
#elif defined(RTV_IF_TSIF) || defined(RTV_IF_SPI_SLAVE)
	int dev = fd_fm_tsif_dev; 
#endif	  

	DMSG0("[fm_read_thread] Entered\n");

	for(;;)
	{
		if(mtv_read_thread_should_stop())
			break;

		fm_read(dev);
	}

	DMSG0("[fm_read_thread] Exit...\n");

	return NULL;
}


static int fm_check_rssi(void)
{
	int rssi;
	int ret;
	
	if((ret=ioctl(fd_fm_dmb_dev, IOCTL_FM_GET_RSSI, &rssi)) < 0)
	{
		EMSG1("IOCTL_ISDBT_GET_TMCC failed: %d\n", ret);
	}

	DMSG1("rssi = %d\n", rssi);

	return 0;
}


static int fm_check_lock_status(void)
{
	IOCTL_FM_LOCK_STATUS_INFO lock_status;
	
	if(ioctl(fd_fm_dmb_dev, IOCTL_FM_GET_LOCK_STATUS, &lock_status) == 0)
	{
		DMSG1("lock val = %u\n", lock_status.val);
		DMSG1("lock cnt = %u\n", lock_status.cnt);
	}
	else
		EMSG0("[FM] IOCTL_FM_GET_LOCK_STATUS failed\n");
	
	return 0;
}

static BOOL is_valid_fm_freq(unsigned int ch_freq_khz)
{	
	if((ch_freq_khz<RTV_FM_CH_MIN_FREQ_KHz) || (ch_freq_khz>RTV_FM_CH_MAX_FREQ_KHz))
	{
		EMSG2("[FM] freq must be in the (%dKHz ~ %dKHz)", RTV_FM_CH_MIN_FREQ_KHz, RTV_FM_CH_MAX_FREQ_KHz);
		return FALSE;
	}

	if((ch_freq_khz % RTV_FM_CH_STEP_FREQ_KHz) != 0)
	{
		EMSG1("[FM] The step of freq must be %dKHz", RTV_FM_CH_STEP_FREQ_KHz);
		return FALSE;
	}	

	return TRUE;
}


static int fm_disable_ts(void)
{
	int ret;

	if(fm_is_ebable_ts == FALSE)
		return 0;
	
	if((ret=ioctl(fd_fm_dmb_dev, IOCTL_FM_STOP_TS)) < 0)
	{
		EMSG1("[FM] IOCTL_FM_STOP_TS failed: %d\n", ret);
		return ret;
	}

	fm_close_msc_dump_file();

	fm_is_ebable_ts = FALSE;

	return 0;
}


static int fm_enable_ts(void)
{
	int ret;

	if(fm_is_ebable_ts == TRUE)
		return 0;

	fm_open_msc_dump_file(0, mtv_prev_channel);
#if defined(RTV_FM_RDS_ENABLED) && defined(_FM_RDS_FILE_DUMP_ENABLE)
	fm_open_msc_dump_file(1, mtv_prev_channel);
#endif

	if((ret=ioctl(fd_fm_dmb_dev, IOCTL_FM_START_TS)) < 0)
	{
		EMSG1("[FM] IOCTL_FM_START_TS failed: %d\n", ret);
		return ret;
	}

	fm_is_ebable_ts = TRUE;
	
	return 0;
}
	

static int fm_set_frequency(void)
{
	unsigned int ch_freq_khz;
	IOCTL_FM_SET_FREQ_INFO set_freq_info;

	while(1)
	{
		DMSG0("Input Channel freq(ex. 107700):");
		scanf("%u", &ch_freq_khz);
		CLEAR_STDIN;

		if(is_valid_fm_freq(ch_freq_khz) == TRUE)
			break;

		EMSG1("[FM Set Freq] (ch: %u)\n", ch_freq_khz);
		break;
	}

	fm_disable_ts();

	set_freq_info.ch_freq_khz = ch_freq_khz;
	if(ioctl(fd_fm_dmb_dev, IOCTL_FM_SET_FREQ, &set_freq_info) < 0)
	{
		EMSG1("[FM] IOCTL_FM_SET_FREQ: tuner(%d)\n", set_freq_info.tuner_err_code);
		return -2;
	}								

	mtv_prev_channel = ch_freq_khz;
	
	return 0;
}


static void get_search_freq_from_user(IOCTL_FM_SRCH_INFO *srch_info_ptr)
{
	unsigned int start_freq, end_freq;
	
	while(1)
	{
		DMSG3("Input the starting freq (range: %u ~ %u, step: %u):",
			RTV_FM_CH_MIN_FREQ_KHz, RTV_FM_CH_MAX_FREQ_KHz,
			RTV_FM_CH_STEP_FREQ_KHz);

		scanf("%u", &start_freq);
		CLEAR_STDIN;

		if(is_valid_fm_freq(start_freq) == TRUE)
			break;

		EMSG0("[FM] Invalid frequency\n");
	}

	if(start_freq == RTV_FM_CH_MIN_FREQ_KHz)
		end_freq = RTV_FM_CH_MAX_FREQ_KHz;
	else
		end_freq = start_freq - RTV_FM_CH_STEP_FREQ_KHz;

	srch_info_ptr->start_freq = start_freq;
	srch_info_ptr->end_freq = end_freq;

	DMSG3("[FM Search] start_freq(%u) ~ end_freq(%u), step: %u \n",
			start_freq, end_freq, RTV_FM_CH_STEP_FREQ_KHz);
}

static int fm_srch_freq(void)
{
	int ret;
	IOCTL_FM_SRCH_INFO srch_info;

	get_search_freq_from_user(&srch_info);

	if((ret=ioctl(fd_fm_dmb_dev, IOCTL_FM_SRCH_FREQ, &srch_info)) < 0)
	{
		EMSG1("[FM] IOCTL_FM_SRCH_FREQ failed: %d\n", srch_info.tuner_err_code);
		return ret;
	}

	DMSG1("[FM Search] Detected freq (%u)\n", srch_info.detected_freq);
	
	return 0;
}


static void get_full_scan_freq_from_user(IOCTL_FM_SCAN_INFO *scan_info_ptr)
{
	unsigned int start_freq, end_freq;

	while(1)
	{
		DMSG3("Input the starting freq (range: %u ~ %u, step: %u):",
			RTV_FM_CH_MIN_FREQ_KHz, RTV_FM_CH_MAX_FREQ_KHz,
			RTV_FM_CH_STEP_FREQ_KHz);

		scanf("%u", &start_freq);
		CLEAR_STDIN;

		if(is_valid_fm_freq(start_freq) == TRUE)
			break;

		EMSG0("[FM] Invalid frequency\n");
	}

	if(start_freq == RTV_FM_CH_MIN_FREQ_KHz)
		end_freq = RTV_FM_CH_MAX_FREQ_KHz;
	else
		end_freq = start_freq - RTV_FM_CH_STEP_FREQ_KHz;

	scan_info_ptr->start_freq = start_freq;
	scan_info_ptr->end_freq = end_freq;
	scan_info_ptr->num_ch_buf = MAX_NUM_FM_EXIST_CHANNEL;

	DMSG3("[FM Scan] start_freq(%u) ~ end_freq(%u), step: %u \n",
			start_freq, end_freq, RTV_FM_CH_STEP_FREQ_KHz);
}

static int fm_full_scan_freq(void)
{
	int i, ret;
	IOCTL_FM_SCAN_INFO full_scan_info;
	double scan_end_time;

	get_full_scan_freq_from_user(&full_scan_info);
	
	time_elapse();

	if((ret=ioctl(fd_fm_dmb_dev, IOCTL_FM_SCAN_FREQ, &full_scan_info)) < 0)
	{
		EMSG1("[FM] scan failed: %d\n", full_scan_info.tuner_err_code);
		return -1;
	}		
		
	for(i=0; i<full_scan_info.num_detected_ch; i++)
	{
		DMSG1("[FM] Detected freq (%u)\n", full_scan_info.ch_buf[i]);
	}				

	scan_end_time = time_elapse();
	DMSG1("Total scan time: %f\n\n", scan_end_time);

	return 0;
}

static int fm_power_up(void)
{
	int ret;
	int adc_clk_type;
	IOCTL_FM_POWER_ON_INFO pwr_on_info;	

	if(fm_is_power_on == TRUE)
		return 0;
	
	while(1)
	{
		DMSG0("Input ADC clock type (0[8 MHz] or 1[8.192 MHz]):" );
		scanf("%d", &adc_clk_type);
		CLEAR_STDIN;	

		if((adc_clk_type == RTV_ADC_CLK_FREQ_8_MHz)
		|| (adc_clk_type == RTV_ADC_CLK_FREQ_8_192_MHz))
			break;
	}
	
	DMSG1("[FM ADC clock] : %s\n",
		adc_clk_type==RTV_ADC_CLK_FREQ_8_MHz ? "8 MHz" : "8.192 MHz");

	pwr_on_info.adc_clk_type = adc_clk_type;

	if((ret = ioctl(fd_fm_dmb_dev, IOCTL_FM_POWER_ON, &pwr_on_info)) < 0)
	{
		EMSG2("[FM] Power Up failed: ret(%d), tuner(%d)", ret, pwr_on_info.tuner_err_code);
		return ret;		
	}

	/* Create the read thread. */
	if((ret = create_mtv_read_thread(fm_read_thread)) != 0)
	{
		EMSG1("create_mtv_read_thread() failed: %d\n", ret);
		return ret;
	}	

	fm_is_power_on = TRUE;
	fm_is_ebable_ts = FALSE;

#ifdef _FM_FILE_DUMP_ENABLE
{
	int i;

	for(i=0; i<MAX_NUM_FM_FILE; i++)
		fd_dump[i] = NULL;
}
#endif


/* Test */	
#if 0
{
	unsigned int ch_freq_khz;
		
	ch_freq_khz = 107700;
	fm_set_frequency(ch_freq_khz);
}
#endif
	
	return 0;
}

static int fm_power_down(void)
{	
	int ret;

	if(fm_is_power_on == FALSE)
		return 0;
	
	if((ret=ioctl(fd_fm_dmb_dev, IOCTL_FM_POWER_OFF)) < 0)
	{
		EMSG0("[FM] IOCTL_FM_POWER_OFF failed\n");
		return ret;
	}

	fm_is_power_on = FALSE;
	fm_is_ebable_ts = FALSE;

	delete_mtv_read_thread();

	return 0;
}


void test_FM()
{
	int key, ret;

	fm_is_power_on = FALSE;

	if((fd_fm_dmb_dev=open_mtv_device()) < 0)
		return;
		
#if defined(RTV_IF_TSIF) || defined(RTV_IF_SPI_SLAVE)
	if((fd_fm_tsif_dev=open_tsif_device()) < 0)
		return;

#if defined(TSIF_SAMSUNG_AP)
	tsif_run(fd_fm_tsif_dev, 1);
#endif
#endif

	while(1)
	{
		DMSG0("===============================================\n");
		DMSG0("\t0: FM Power ON\n");
		DMSG0("\t1: FM Power OFF\n");
		DMSG0("\t2: FM Scan freq\n");
		DMSG0("\t3: FM Search freq\n");
		DMSG0("\t4: FM Set Channel\n");
		DMSG0("\t5: ISDBT Start TS\n");
		DMSG0("\t6: ISDBT Stop TS\n");
		DMSG0("\t7: FM Get Lockstatus\n");
		DMSG0("\t8: FM Get RSSI\n");		
		DMSG0("\t9: [TEST] Register IO Test\n");
		
		DMSG0("\tq or Q: Quit\n");
		DMSG0("===============================================\n");
   		
		key = getc(stdin);				
		CLEAR_STDIN;

		if((key >= '2') && (key <= '9'))
		{
			if(fm_is_power_on == FALSE)
			{
				EMSG0("Power Down state!Must Power ON\n");
				continue;
			}
		}
		
		switch( key )
		{
			case '0':
				DMSG0("[FM Power ON]\n");
				fm_power_up();				
				break;
				
			case '1':	
				DMSG0("[FM Power OFF]\n");
				fm_power_down();
				break;

			case '2':
				DMSG0("[FM full scan freq]\n");
				fm_full_scan_freq();				
				break;

			case '3':
				DMSG0("[FM search freq]\n");
				fm_srch_freq();

			case '4':
				DMSG0("[FM Set freq]\n");
				fm_set_frequency();
				break;

			case '5':
				fm_enable_ts();
				break;

			case '6':
				fm_disable_ts();
				break;

			case '7':
				DMSG0("[FM Get Lockstatus]\n");
				fm_check_lock_status();
				break;

			case '8':
				DMSG0("[FM Get RSSI]\n");
				fm_check_rssi();
				break;

			case '9':
				test_RegisterIO(fd_fm_dmb_dev);
				break;

			case 'q':
			case 'Q':
				goto FM_EXIT;

			default:
				DMSG1("[%c]\n", key);
		}

		DMSG0("\n");
	} 

FM_EXIT:

	fm_power_down();

	ret = close(fd_fm_dmb_dev);
	DMSG2("[FM] %s close() result : %d\n", NXB110TV_DEV_NAME, ret);
	
#if defined(RTV_IF_TSIF) || defined(RTV_IF_SPI_SLAVE)
	close(fd_dab_tsif_dev);
#endif

	DMSG0("FM EXIT\n");
	
	return;
}

#endif /* #ifdef RTV_FM_ENABLE */

