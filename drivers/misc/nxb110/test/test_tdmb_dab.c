#include "test.h"


#if defined(RTV_DAB_ENABLE) || defined(RTV_TDMB_ENABLE)

/*============================================================================
 * Configuration for File dump
 *===========================================================================*/
//#define _DAB_MSC_FILE_DUMP_ENABLE /* for MSC data*/
//#define _DAB_FIC_FILE_DUMP_ENABLE /* for FIC data */

/* MSC filename: /data/nexell/dab_msc_FREQ_SUBCHID.ts */
#define DAB_DUMP_MSC_FILENAME_PREFIX		"dab_msc"
#define DAB_DUMP_FIC_FILENAME_PREFIX		"dab_fic"


#define INVALIDE_SUBCH_ID	0xFFFF

#define MAX_NUM_SUB_CHANNEL		64
typedef struct
{
	BOOL opened;
	E_RTV_SERVICE_TYPE svc_type;
	
	unsigned int msc_buf_index;

#ifdef _DAB_MSC_FILE_DUMP_ENABLE
	FILE *fd_msc;
	char fname[64];
#endif
} SUB_CH_INFO;

#ifdef _DAB_FIC_FILE_DUMP_ENABLE
	static FILE *fd_fic[MAX_NUM_BB_DEMOD];
	static char fic_fname[MAX_NUM_BB_DEMOD][64];
#endif

#ifdef RTV_DAB_ENABLE
static volatile BOOL full_scan_state[MAX_NUM_BB_DEMOD];
#endif

static volatile BOOL is_open_fic[MAX_NUM_BB_DEMOD];
static unsigned int dab_tdmb_fic_size[MAX_NUM_BB_DEMOD];

static BOOL is_power_on[MAX_NUM_BB_DEMOD];

static int fd_dab_dmb_dev[MAX_NUM_BB_DEMOD]; /* MTV device file descriptor. */
#if defined(RTV_IF_TSIF) || defined(RTV_IF_SPI_SLAVE)
static int fd_dab_tsif_dev;
#endif

/* Use the mutex for lock the add/delete/set_reconfiguration subch 
at read, fic, main threads when DAB reconfiguration occured. */
static pthread_mutex_t dab_mutex[MAX_NUM_BB_DEMOD];

#define DAB_LOCK_INIT(demod_no)		pthread_mutex_init(&dab_mutex[demod_no], NULL)
#define DAB_LOCK(demod_no)			pthread_mutex_lock(&dab_mutex[demod_no])
#define DAB_FREE(demod_no)			pthread_mutex_unlock(&dab_mutex[demod_no])
#define DAB_LOCK_DEINIT(demod_no)	((void)0)

#ifdef RTV_MULTI_SERVICE_MODE
static IOCTL_MULTI_SERVICE_BUF multi_svc_buf[MAX_NUM_BB_DEMOD];
#else
static unsigned char single_svc_buf[MAX_NUM_BB_DEMOD][MAX_READ_TSP_SIZE];
#endif

#if (RTV_NUM_DAB_AVD_SERVICE == 1)
static unsigned int prev_opened_subch_id[MAX_NUM_BB_DEMOD]; /* Previous sub channel ID. used for 1 service */
#endif

static unsigned int av_subch_id[MAX_NUM_BB_DEMOD];
static unsigned int num_opend_subch[MAX_NUM_BB_DEMOD];

SUB_CH_INFO subch_info[MAX_NUM_BB_DEMOD][MAX_NUM_SUB_CHANNEL];

static RTV_FIC_ENSEMBLE_INFO ensemble_info;

static struct mrevent fic_parsing_event;

#ifdef RTV_TDMB_ENABLE
	#define TEST_MODE_STR	"TDMB"
#else
	#define TEST_MODE_STR	"DAB/DAB+"
#endif

/*============================================================================
 * Forward local functions.
 *===========================================================================*/
static int check_signal_info(int demod_no);
static int close_fic(int demod_no);
static int open_fic(int demod_no);
static void processing_fic(int demod_no, unsigned char *buf, unsigned int size, unsigned int freq_khz);

static void tdmb_data_proc_dm_timer(int demod_no)
{
	SUB_CH_INFO *subch_info_ptr;
	unsigned int av_subch = av_subch_id[demod_no];
	unsigned int periodic_debug_info_mask = get_periodic_debug_info_mask(demod_no);

	subch_info_ptr = &subch_info[demod_no][av_subch];

	if(periodic_debug_info_mask & SIG_INFO_MASK)
		check_signal_info(demod_no);

	if(periodic_debug_info_mask & TSP_STAT_INFO_MASK)
	{
		if(av_subch != INVALIDE_SUBCH_ID)
		{
			if(subch_info_ptr->svc_type == RTV_SERVICE_VIDEO)
				show_video_tsp_statistics(demod_no);
		}
	}

	if(periodic_debug_info_mask & (SIG_INFO_MASK|TSP_STAT_INFO_MASK))
		DMSG0("\n");
}

static void tdmb_dab_dm_timer_handler(void)
{
	tdmb_data_proc_dm_timer(0);

#if (MAX_NUM_BB_DEMOD == 2)
	tdmb_data_proc_dm_timer(1);
#endif
}


//============ Start of file dump =============================
static int open_msc_dump_file(int demod_no, unsigned int ch_freq_khz, unsigned int subch_id)
{
#ifdef _DAB_MSC_FILE_DUMP_ENABLE
	SUB_CH_INFO *subch_ptr;
	
	if(subch_info[demod_no][subch_id].fd_msc != NULL)
	{
		EMSG2("[DMB] Fail to open dump file. freq: %u, subch_id: %u\n",
			ch_freq_khz, subch_id);
		return -1;
	}

	subch_ptr = &subch_info[demod_no][subch_id];

	sprintf(subch_ptr->fname, "%s/%d_%s_%u_%u.ts",
		TS_DUMP_DIR, demod_no, DAB_DUMP_MSC_FILENAME_PREFIX, 
		ch_freq_khz, subch_id);
	
	if((subch_ptr->fd_msc=fopen(subch_ptr->fname, "wb")) == NULL)
	{
		EMSG1("[DMB] Fail to open error: %s\n", subch_ptr->fname);
		return -2;
	}

	DMSG1("[DMB] Opend dump file: %s\n", subch_ptr->fname);
#endif

	return 0;
}

static int close_msc_dump_file(int demod_no, unsigned int subch_id)
{
#ifdef _DAB_MSC_FILE_DUMP_ENABLE
	if(subch_info[demod_no][subch_id].fd_msc != NULL)
	{
		fclose(subch_info[demod_no][subch_id].fd_msc);
		subch_info[demod_no][subch_id].fd_msc = NULL;

		DMSG1("[DMB] Closed dump file: %s\n", subch_info[demod_no][subch_id].fname);
	}
#endif

	return 0;
}

static inline void write_msc_dump_file(int demod_no, const void *buf,
						unsigned int size, unsigned int subch_id)
{
#ifdef _DAB_MSC_FILE_DUMP_ENABLE
	if(subch_id < MAX_NUM_SUB_CHANNEL)
	{
		if(subch_info[demod_no][subch_id].fd_msc != NULL)
			fwrite(buf, sizeof(char), size, subch_info[demod_no][subch_id].fd_msc);
		else
			EMSG0("[write_msc_dump_file] Invalid sub ch ID\n");
	}
#endif
}

static int open_fic_dump_file(int demod_no)
{
#ifdef _DAB_FIC_FILE_DUMP_ENABLE
	if(fd_fic[demod_no] != NULL)
	{
		EMSG0("[DMB] Must close dump file before open the new file\n");
		return -1;
	}
		
	sprintf(fic_fname[demod_no], "%s/%d_%s.ts",
		TS_DUMP_DIR, demod_no, DAB_DUMP_FIC_FILENAME_PREFIX);
	
	if((fd_fic[demod_no]=fopen(fic_fname[demod_no], "wb")) == NULL)
	{
		EMSG1("[DMB] Fail to open error: %s\n", fic_fname[demod_no]);
		return -2;
	}

	DMSG1("[DMB] Opend FIC dump file: %s\n", fic_fname[demod_no]);
#endif

	return 0;
}

static int close_fic_dump_file(int demod_no)
{
#ifdef _DAB_FIC_FILE_DUMP_ENABLE
	if(fd_fic[demod_no] != NULL)
	{
		fclose(fd_fic[demod_no]);
		fd_fic[demod_no] = NULL;

		DMSG1("[MTV] Closed FIC dump file: %s\n", fic_fname[demod_no]);
	}
#endif

	return 0;
}
//============ END of file dump =============================
static inline int get_fic_size(void)
{
	unsigned int fic_size = 0;

//	if(dab_tdmb_fic_size != 0)
//		return dab_tdmb_fic_size;
	
#ifdef RTV_TDMB_ENABLE
	fic_size = 384;
#else
	if (ioctl(fd_dab_dmb_dev[demod_no], IOCTL_DAB_GET_FIC_SIZE, &fic_size))
		DMSG0("[do_ansemble_acquisition] IOCTL_DAB_GET_FIC_SIZE error\n");
#endif

	DMSG1("[get_fic_size] fic_size: %d\n", fic_size);

	return (int)fic_size;
}

#ifdef RTV_FIC_I2C_INTR_ENABLED
static void tsif_fic_sig_handler(int signo)
{
	int ret;
#ifdef RTV_TDMB_ENABLE
	IOCTL_TDMB_READ_FIC_INFO read_info;
	int fic_size = 384;//get_fic_size();
#else
	IOCTL_DAB_READ_FIC_INFO read_info;
	int fic_size = get_fic_size();;
#endif

#ifdef RTV_TDMB_ENABLE
	ret = ioctl(fd_dab_dmb_dev[demod_no], IOCTL_TDMB_READ_FIC, &read_info);
#else
	read_info.size = fic_size;
	ret = ioctl(fd_dab_dmb_dev[demod_no], IOCTL_DAB_READ_FIC, &read_info);
#endif
	if (ret == 0)
	{
		if(fic_size != 0)
			processing_fic(read_info.buf, fic_size, mtv_prev_channel);
		else
		{
			EMSG0("[tsif_fic_sig_handler] fic_size error.\n");
		#ifdef RTV_DAB_ENABLE
			fic_size = get_fic_size(); /* Retry for DAB */
			read_info.size = fic_size;
		#endif
		}
	}
	else
	{
		EMSG0("[tsif_fic_sig_handler] READ_FIC ioctl error\n");
	}

}
static int setup_tsif_fic_sig_handler(void)
{
	struct sigaction sigact;
	int oflag;

	/* Setup the FIC SIGIO for TSIF. */
	sigact.sa_handler = tsif_fic_sig_handler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
#ifdef SA_RESTART
	sigact.sa_flags |= SA_RESTART;
#endif
	if(sigaction(SIGIO, &sigact, NULL) < 0)
	{
		EMSG1("[%s] sigaction() error\n", TEST_MODE_STR);
		return -1;
	}

	fcntl(fd_dab_dmb_dev[demod_no], F_SETOWN, getpid());
	oflag = fcntl(fd_dab_dmb_dev[demod_no], F_GETFL);
	fcntl(fd_dab_dmb_dev[demod_no], F_SETFL, oflag | FASYNC);

	return 0;
}
#endif /* #ifdef RTV_FIC_I2C_INTR_ENABLED */



static void reset_subch_info(int demod_no)
{
	unsigned int i;

	for(i=0; i<MAX_NUM_SUB_CHANNEL; i++)
	{
		subch_info[demod_no][i].opened = FALSE;
#ifdef _DAB_MSC_FILE_DUMP_ENABLE
		subch_info[demod_no][i].fd_msc = NULL;
#endif
	}

#ifdef _DAB_FIC_FILE_DUMP_ENABLE
	fd_fic[demod_no] = NULL;
#endif

	num_opend_subch[demod_no] = 0;
	av_subch_id[demod_no] = INVALIDE_SUBCH_ID;	

#if (RTV_NUM_DAB_AVD_SERVICE == 1)  /* Single Sub Channel Mode */
	prev_opened_subch_id[demod_no] = INVALIDE_SUBCH_ID;
#endif
}

static int close_fic(int demod_no)
{
	IOCTL_CLOSE_FIC_INFO param;

	DAB_LOCK(demod_no);

	DMSG0("[close_fic] Enter\n");
	
	if(is_open_fic[demod_no] == TRUE)
	{
		param.demod_no = demod_no;
#ifdef RTV_TDMB_ENABLE
		if(ioctl(fd_dab_dmb_dev[demod_no], IOCTL_TDMB_CLOSE_FIC, &param) < 0)
#else
		if(ioctl(fd_dab_dmb_dev[demod_no], IOCTL_DAB_CLOSE_FIC, &param) < 0)
#endif
			EMSG0("[DMB] CLOSE_FIC failed.\n");
	
		is_open_fic[demod_no] = FALSE;
	}

	dab_tdmb_fic_size[demod_no] = 0;

	DAB_FREE(demod_no);
	
	return 0;
}

static int open_fic(int demod_no)
{
	int ret;
	IOCTL_OPEN_FIC_INFO param;

	DMSG0("[open_fic] Enter\n");

	DAB_LOCK(demod_no);

	if(is_open_fic[demod_no] == FALSE)
	{
		param.demod_no = demod_no;
#ifdef RTV_TDMB_ENABLE
		ret = ioctl(fd_dab_dmb_dev[demod_no], IOCTL_TDMB_OPEN_FIC, &param);
#else
		ret = ioctl(fd_dab_dmb_dev[demod_no], IOCTL_DAB_OPEN_FIC, &param);
#endif
		if(ret)
		{
			EMSG1("[open_fic] OPEN_FIC failed: %d\n", param.tuner_err_code);
			return -1;
		}

		rtvFICDEC_Init(demod_no); /* FIC parser Init */
		is_open_fic[demod_no] = TRUE;
	}

	DAB_FREE(demod_no);

	return 0;
}

static E_RTV_FIC_DEC_RET_TYPE proc_fic_parsing(int demod_no, unsigned char *buf, 
			unsigned int size, unsigned int freq_khz)
{
	E_RTV_FIC_DEC_RET_TYPE ficdec_ret;
	
//	DMSG6("\t[FIC(%u)] size(%u) 0x%02X 0x%02X 0x%02X 0x%02X\n",
//		freq_khz, size, buf[0], buf[1], buf[2], buf[3]);

#ifdef _DAB_FIC_FILE_DUMP_ENABLE
	if(fd_fic[demod_no] != NULL)
		fwrite(buf, sizeof(char), size, fd_fic[demod_no]);
#endif

	ficdec_ret = rtvFICDEC_Decode(demod_no, buf, size);
	if(ficdec_ret != RTV_FIC_RET_GOING)
	{
		DMSG1("[DMB] FIC parsing result: %s\n",
			(ficdec_ret==RTV_FIC_RET_DONE ? "RTV_FIC_RET_DONE" : "RTV_FIC_RET_CRC_ERR"));

		if(ficdec_ret == RTV_FIC_RET_DONE)
		{
			rtvFICDEC_GetEnsembleInfo(demod_no, &ensemble_info);

			// Show a decoded table.
			show_fic_information(&ensemble_info, freq_khz);
		}
		else if(ficdec_ret == RTV_FIC_RET_CRC_ERR)
			DMSG1("[DMB] FIC CRC error (%u)\n", freq_khz);
	}
	
	return ficdec_ret;
}


#ifndef RTV_FIC_POLLING_MODE /* FIC interrupt mode */
static void processing_fic(int demod_no, unsigned char *buf,
						unsigned int size, unsigned int freq_khz)
{
//	DMSG6("\t[processing_fic(%u)] size(%u) 0x%02X 0x%02X 0x%02X 0x%02X\n",
//		freq_khz, size, buf[0], buf[1], buf[2], buf[3]);

	if(proc_fic_parsing(demod_no, buf, size, freq_khz) == RTV_FIC_RET_GOING)
	{
#ifdef RTV_DAB_ENABLE
		if(full_scan_state[demod_no] == FALSE) /* Reconfig FIC parsing mode */
		{
			/*
			if( timeout counter is reached? )
				close_fic();
			*/
		}
#endif
		return;
	}

#if defined(RTV_TDMB_ENABLE)
	/* Set the event to allow scanning of the next freq. */
	mrevent_trigger(&fic_parsing_event);
	
#elif defined(RTV_DAB_ENABLE)
	if(full_scan_state == TRUE) /* If the full scan state. */
		mrevent_trigger(&fic_parsing_event);
	else 
	{
	//????????? Delete?????????
		close_fic(demod_no); /* Reconfig FIC parsing mode. */
	}
#endif		
}
#endif /* #ifndef RTV_FIC_POLLING_MODE */


static void processing_msc(int demod_no, U8 *buf, UINT size, UINT subch_id)
{
	int i;

	if (subch_info[demod_no][subch_id].opened == TRUE) {
		switch (subch_info[demod_no][subch_id].svc_type)
		{
		case RTV_SERVICE_VIDEO:
		#if 0
			for (i = 0; i < 1/*(MAX_READ_TSP_SIZE/188)*/; i++) {
				printf("[(%d): processing_msc: %d] 0x%02X 0x%02X 0x%02X 0x%02X, 0x%02X 0x%02X 0x%02X 0x%02X | 0x%02X 0x%02X 0x%02X\n",
					demod_no, i, buf[i*188+0], buf[i*188+1], buf[i*188+2], buf[i*188+3],
					buf[i*188+4], buf[i*188+5], buf[i*188+6], buf[i*188+7],
					buf[i*188+185], buf[i*188+186], buf[i*188+187]);
			}
		#endif
			verify_video_tsp(demod_no, buf, size);
			write_msc_dump_file(demod_no, buf, size, subch_id);
			break;

		case RTV_SERVICE_AUDIO:
		#if 0
			DMSG7("\t Chip(%d) AUDIO Subch ID: %d: Subch Size: %d [0x%02X], [0x%02X], [0x%02X], [0x%02X]\n", 
				demod_no, subch_id, size, buf[0], buf[1], buf[2], buf[3]);
		#endif
			write_msc_dump_file(demod_no, buf, size, subch_id);
			break;

		case RTV_SERVICE_DATA:
		#if 0
			DMSG7("\t Chip(%d) DATA Subch ID: %d: Subch Size: %d [0x%02X], [0x%02X], [0x%02X], [0x%02X]\n", 
				demod_no, subch_id, size, buf[0], buf[1], buf[2], buf[3]);
		#endif
			write_msc_dump_file(demod_no, buf, size, subch_id);
			break;

		default:
			EMSG1("[processing_msc] Invalid subch ID: %u\n", subch_id);
			break;
		}
	}
	else
		EMSG1("[processing_msc] Not opened subch ID: %u\n", subch_id);
}


static void tdmb_dab_read(int demod_no, int dev)
{
	int len;
#ifdef RTV_MULTI_SERVICE_MODE /* Multi service mode */
	IOCTL_MULTI_SERVICE_BUF *svc = &multi_svc_buf[demod_no];
	int i;

	len = read(dev, svc, MAX_READ_TSP_SIZE);
//printf("\t[read] len: %d\n", len);

	if (len > 0) {
		for (i = 0; i < RTV_NUM_DAB_AVD_SERVICE; i++) {
			if(svc->msc_size[i] != 0) {
				//DMSG3("[%d]: msc_size: %u, msc_subch_id: %u\n", i, svc->msc_size[i], svc->msc_subch_id[i]);

				/*
		#ifndef RTV_BUILD_CIFDEC_WITH_DRIVER
				rtvCIFDEC_Decode();
		#endif
				*/

				processing_msc(demod_no, svc->msc_buf[i], 
							svc->msc_size[i],
							svc->msc_subch_id[i]);
			}
		}

	#ifndef RTV_FIC_POLLING_MODE
		if(svc->fic_size != 0)
			processing_fic(demod_no, svc->fic_buf, svc->fic_size, mtv_prev_channel[demod_no]);
	#endif

	}		
	else {
	//#ifndef MTV_BLOCKING_READ_MODE
		usleep(24 * 1000); // 1 CIF period.
	//#endif
	}
	
#else
	unsigned char *buf_ptr = single_svc_buf[demod_no];

	/* Must place the demod index at the frist array element! */
	buf_ptr[0] = demod_no;
	//single_svc_buf[demod_no][0] = demod_no;

	len = read(dev, buf_ptr, MAX_READ_TSP_SIZE);
//printf("\t[read] len: %d\n", len);

	if(len > 0)	{
		processing_msc(demod_no, buf_ptr,
						len, prev_opened_subch_id[demod_no]);
	}
	else {
	#ifndef MTV_BLOCKING_READ_MODE
		usleep(48 * 1000); /* (frame duration / 2) */
	#endif
	}
#endif
}

#if (MAX_NUM_BB_DEMOD == 2)
static void *tdmb_dab_read_thread_secondary(void *arg)
{
#if defined(RTV_IF_SPI) 	
	int dev = fd_dab_dmb_dev[BB_TPEG_DEMOD_IDX];
#elif defined(RTV_IF_TSIF) || defined(RTV_IF_SPI_SLAVE)
	int dev = fd_dab_tsif_dev; 
#endif	  

	DMSG0("[tdmb_dab_read_thread_secondary] Entered\n");

	for(;;)
	{
		if(mtv_read_thread_should_stop(BB_TPEG_DEMOD_IDX))
			break;

		tdmb_dab_read(BB_TPEG_DEMOD_IDX, dev);
	}

	DMSG0("[tdmb_dab_read_thread_secondary] Exit...\n");

	return NULL;

}
#endif

/* Primary read thread for 2 chips.
   or 1 chip.
*/
static void *tdmb_dab_read_thread_primary(void *arg)
{
#if defined(RTV_IF_SPI) 	
	int dev = fd_dab_dmb_dev[BB_AV_DEMOD_IDX];
#elif defined(RTV_IF_TSIF) || defined(RTV_IF_SPI_SLAVE)
	int dev = fd_dab_tsif_dev; 
#endif	  

	DMSG0("[tdmb_dab_read_thread_primary] Entered\n");

	for(;;)
	{
		if(mtv_read_thread_should_stop(BB_AV_DEMOD_IDX))
			break;

		tdmb_dab_read(BB_AV_DEMOD_IDX, dev);
	}

	DMSG0("[tdmb_dab_read_thread_primary] Exit...\n");

	return NULL;
}


static int check_signal_info(int demod_no)
{
	int ret;
	unsigned int lock;

#ifdef RTV_TDMB_ENABLE
	IOCTL_TDMB_SIGNAL_INFO sig_info;
	sig_info.demod_no = demod_no;
	ret = ioctl(fd_dab_dmb_dev[demod_no], IOCTL_TDMB_GET_SIGNAL_INFO, &sig_info);
#else
	IOCTL_DAB_SIGNAL_INFO sig_info;
	ret = ioctl(fd_dab_dmb_dev[demod_no], IOCTL_DAB_GET_SIGNAL_INFO, &sig_info);
#endif
	if(ret < 0)
	{
		EMSG0("[DMB] GET_SIGNAL_INFO failed\n");
		return ret;
	}

#ifdef RTV_TDMB_ENABLE
	lock = (sig_info.lock_mask == RTV_TDMB_CHANNEL_LOCK_OK) ? 1 : 0;
	DMSG0("\t########## [TDMB Signal Inforamtions] ##############\n");
	DMSG1("\t# LOCK: %u (1:LOCK, 0: UNLOCK)\n", lock);
	DMSG1("\t# Antenna Level: %u\n", sig_info.ant_level);
	DMSG1("\t# ber: %f\n", (float)sig_info.ber/RTV_TDMB_BER_DIVIDER);
	DMSG1("\t# cer: %u\n", sig_info.cer);
	DMSG1("\t# cnr: %f\n", (float)sig_info.cnr/RTV_TDMB_CNR_DIVIDER);
	DMSG1("\t# rssi: %f\n", (float)sig_info.rssi/RTV_TDMB_RSSI_DIVIDER);
	DMSG1("\t# per: %u\n", sig_info.per);
	DMSG0("\t###################################################\n");
#else
	lock = (sig_info.lock_mask == RTV_DAB_CHANNEL_LOCK_OK) ? 1 : 0;	
	DMSG0("\t########## [DAB Signal Inforamtions] ##############\n");
	DMSG1("\t# LOCK: %u (1:LOCK, 0: UNLOCK)\n", lock);
	DMSG1("\t# Antenna Level: %u\n", sig_info.ant_level);
	DMSG1("\t# ber: %f\n", (float)sig_info.ber/RTV_DAB_BER_DIVIDER);
	DMSG1("\t# cer: %u\n", sig_info.cer);
	DMSG1("\t# cnr: %f\n", (float)sig_info.cnr/RTV_DAB_CNR_DIVIDER);
	DMSG1("\t# rssi: %f\n", (float)sig_info.rssi/RTV_DAB_RSSI_DIVIDER);
	DMSG1("\t# per: %u\n", sig_info.per);
	DMSG0("\t###################################################\n");
#endif

	return 0;
}

static int check_lock_status(int demod_no)
{
	int ret;
	unsigned int lock_mask;
	unsigned int lock;
	IOCTL_TDMB_GET_LOCK_STATUS_INFO param;

	param.demod_no = demod_no;
#ifdef RTV_TDMB_ENABLE
	ret = ioctl(fd_dab_dmb_dev[demod_no], IOCTL_TDMB_GET_LOCK_STATUS, &param);
	lock = (param.lock_mask == RTV_TDMB_CHANNEL_LOCK_OK) ? 1 : 0;
#else
	ret = ioctl(fd_dab_dmb_dev[demod_no], IOCTL_DAB_GET_LOCK_STATUS, &param);
	lock = (param.lock_mask == RTV_DAB_CHANNEL_LOCK_OK) ? 1 : 0;
#endif

	if(ret == 0)
	{
		DMSG1("[DMB] LOCK status: %d\n", lock);
		return 0;
	}
	else
	{
		EMSG0("[DMB] GET_LOCK_STATUS failed\n");
		return -1;
	}
}


static inline void delete_sub_channel(int demod_no, unsigned int subch_id)
{
	E_RTV_SERVICE_TYPE svc_type;
		
	/* Lockup mutex */
	DAB_LOCK(demod_no);

	subch_info[demod_no][subch_id].opened = FALSE;

	/* Get the service type */
	svc_type = subch_info[demod_no][subch_id].svc_type;
	
	if( svc_type & (RTV_SERVICE_VIDEO|RTV_SERVICE_AUDIO) )
		av_subch_id[demod_no] = INVALIDE_SUBCH_ID;

#if (RTV_NUM_DAB_AVD_SERVICE == 1)
	prev_opened_subch_id[demod_no] = INVALIDE_SUBCH_ID;
#endif

	if(--num_opend_subch[demod_no] == 0)
	{
		mtv_prev_channel[demod_no] = 0; /* Reset to use open_sub_channel() */
		suspend_dm_timer();
	}

	close_msc_dump_file(demod_no, subch_id);

	/* Unlock mutex */
	DAB_FREE(demod_no);
}

static int close_all_sub_channels(int demod_no)
{
	unsigned int subch_id;
	IOCTL_CLOSE_ALL_SUBCHANNELS_INFO param;

	DMSG1("[close_all_sub_channels: %d] Enter\n", demod_no);

	if(num_opend_subch[demod_no] == 0)
		return 0;

	param.demod_no = demod_no;
#ifdef RTV_TDMB_ENABLE
	if(ioctl(fd_dab_dmb_dev[demod_no], IOCTL_TDMB_CLOSE_ALL_SUBCHANNELS, &param) < 0)
#else
	if(ioctl(fd_dab_dmb_dev[demod_no], IOCTL_DAB_CLOSE_ALL_SUBCHANNELS, &param) < 0)
#endif
	{
		EMSG0("[DMB] CLOSE_ALL_SUBCHANNELS failed\n");
		return -1;
	}
		
	for(subch_id=0; subch_id<MAX_NUM_SUB_CHANNEL; subch_id++)
	{
		if(num_opend_subch[demod_no] == 0)
			return 0;

		if(subch_info[demod_no][subch_id].opened == TRUE)
			delete_sub_channel(demod_no, subch_id);
	}

	DMSG0("[close_all_sub_channels] End\n");

	return 0;
}

static int close_sub_channel(int demod_no, unsigned int subch_id)
{
	int ret;
	IOCTL_CLOSE_SUBCHANNEL_INFO param;

	DMSG1("[close_sub_channel] ID: %u\n", subch_id);

#if (RTV_NUM_DAB_AVD_SERVICE == 1)
	if(prev_opened_subch_id[demod_no] != INVALIDE_SUBCH_ID)
		subch_id = prev_opened_subch_id[demod_no];
	else
		return -1;
#else
	if(subch_id > (MAX_NUM_SUB_CHANNEL-1))
		return -1;
#endif

	if(subch_info[demod_no][subch_id].opened == FALSE)
	{
		DMSG1("[DMB] Not opened sub channed ID(%d)\n", subch_id);
		return 0;
	}

	param.demod_no = demod_no;
	param.subch_id = subch_id;
#ifdef RTV_TDMB_ENABLE
	if((ret=ioctl(fd_dab_dmb_dev[demod_no], IOCTL_TDMB_CLOSE_SUBCHANNEL, &param)) < 0)
#else
	if((ret=ioctl(fd_dab_dmb_dev[demod_no], IOCTL_DAB_CLOSE_SUBCHANNEL, &param)) < 0)
#endif
	{
		EMSG1("[DMB] IOCTL_DAB_CLOSE_SUBCHANNEL failed: %d\n", ret);
		return -2;
	}

	delete_sub_channel(demod_no, subch_id);

	return 0;
}


static inline int add_sub_channel(int demod_no, unsigned int ch_freq_khz,
				unsigned int subch_id,
				E_RTV_SERVICE_TYPE svc_type)
{
	int ret = 0;
	
	/* Lockup mutex */
	DAB_LOCK(demod_no);

	subch_info[demod_no][subch_id].opened = TRUE;
	subch_info[demod_no][subch_id].svc_type = svc_type;

#if (RTV_NUM_DAB_AVD_SERVICE == 1)
	prev_opened_subch_id[demod_no] = subch_id;
#endif

	if( svc_type & (RTV_SERVICE_VIDEO|RTV_SERVICE_AUDIO) )
		av_subch_id[demod_no] = subch_id;

	if(num_opend_subch[demod_no] == 0) /* Debug */
	{
		init_periodic_debug_info(demod_no);
		
		init_tsp_statistics(demod_no);
		resume_dm_timer();
	}
	
	open_msc_dump_file(demod_no, ch_freq_khz, subch_id);

	num_opend_subch[demod_no]++;
	
	/* Unlock mutex */
	DAB_FREE(demod_no);

	return ret;
}

static int open_sub_channel(int demod_no, IOCTL_TDMB_SUB_CH_INFO *sub_ch_ptr)
{
	int ret;
	unsigned int subch_id = sub_ch_ptr->subch_id;

	DMSG3("[open_sub_channel] FREQ(%u), SUBCH_ID(%u), SVC_TYPE(%d)\r\n",
			sub_ch_ptr->ch_freq_khz, subch_id, sub_ch_ptr->svc_type);

	if(sub_ch_ptr->ch_freq_khz == mtv_prev_channel[demod_no])
	{
		if(subch_info[demod_no][subch_id].opened == TRUE)
		{
			DMSG1("[DMB] Already opened sub channed ID(%d)\n", subch_id);
			return 0;
		}

#if (RTV_NUM_DAB_AVD_SERVICE == 1) /* Single Sub Channel Mode */
		close_sub_channel(demod_no, prev_opened_subch_id[demod_no]);
#endif
	}
	else
	{
#if (RTV_NUM_DAB_AVD_SERVICE == 1) /* Single Sub Channel Mode */
		close_sub_channel(demod_no, prev_opened_subch_id[demod_no]);
#else
		close_all_sub_channels(demod_no);
#endif

#if defined(RTV_CIF_MODE_ENABLED) && !defined(RTV_BUILD_CIFDEC_WITH_DRIVER)
		////////temp rtvCIFDEC_Init();
#endif

		/* Update the prev freq. Must after close sub channel. */
		mtv_prev_channel[demod_no] = sub_ch_ptr->ch_freq_khz;
	}

	if((ret=add_sub_channel(demod_no, sub_ch_ptr->ch_freq_khz, subch_id, sub_ch_ptr->svc_type) < 0))
	{
		EMSG1("[DMB] add sub channel failed: %d\n", sub_ch_ptr->tuner_err_code);
		return -1;
	}

	sub_ch_ptr->demod_no = demod_no;
#ifdef RTV_TDMB_ENABLE
	if((ret=ioctl(fd_dab_dmb_dev[demod_no], IOCTL_TDMB_OPEN_SUBCHANNEL, sub_ch_ptr)) < 0)
#else
	if((ret=ioctl(fd_dab_dmb_dev[demod_no], IOCTL_DAB_OPEN_SUBCHANNEL, sub_ch_ptr)) < 0)
#endif
	{
		delete_sub_channel(demod_no, subch_id);	

		EMSG1("[DMB] open sub channel failed: %d\n", ret);
		return -2;
	}

#ifdef RTV_TDMB_ENABLE
	dab_tdmb_fic_size [demod_no]= 384;
#else
	if (ioctl(fd_dab_dmb_dev[demod_no], IOCTL_DAB_GET_FIC_SIZE, &dab_tdmb_fic_size))
		DMSG0("[do_ansemble_acquisition] IOCTL_DAB_GET_FIC_SIZE error\n");
	else
		DMSG1("[open_sub_channel] FIC size: %u\n", dab_tdmb_fic_size);
#endif

	return 0;
}


static int do_ansemble_acquisition(int demod_no, U32 ch_freq_khz)
{
	int ret;
#ifdef RTV_FIC_POLLING_MODE /* FIC polling mode */
	int k;
	E_RTV_FIC_DEC_RET_TYPE dec_ret;
#ifdef RTV_TDMB_ENABLE
	IOCTL_TDMB_READ_FIC_INFO read_info;
	int fic_size = get_fic_size();
#else
	IOCTL_DAB_READ_FIC_INFO read_info;
	int fic_size = get_fic_size();

	read_info.size = fic_size;
#endif

	for(k=0; k<50; k++) // TEMP
	{
		if(fic_size != 0)
		{
			read_info.demod_no = demod_no;
		#ifdef RTV_TDMB_ENABLE
			ret = ioctl(fd_dab_dmb_dev[demod_no], IOCTL_TDMB_READ_FIC, &read_info);
		#else
			ret = ioctl(fd_dab_dmb_dev[demod_no], IOCTL_DAB_READ_FIC, &read_info);
		#endif
			if(ret == 0)
			{
				dec_ret = proc_fic_parsing(demod_no, read_info.buf, fic_size, ch_freq_khz);
				if(dec_ret != RTV_FIC_RET_GOING)
					break; /* escape for() loop. */
			}
			else
			{
				DMSG1("[do_ansemble_acquisition] IOCTL_DAB_READ_FIC error: %d\n", read_info.tuner_err_code);
				break;
			}
		}
		else
		{	
			EMSG1("[do_ansemble_acquisition] fic_size error. Anyway scan!: %d\n", k);
	#ifdef RTV_DAB_ENABLE
			fic_size = get_fic_size(); /* Retry for DAB */
			read_info.size = fic_size;
	#endif
		}
	}

#else /* FIC interrupt mode */
	/* Wait for FIC pasring was completed in the specified time. */
	ret = mrevent_wait(&fic_parsing_event, 3000);
	if(ret != 0)
	{
		if(ret == ETIMEDOUT)
			EMSG0("[do_ansemble_acquisition] FIC parsing Timeout!\n");
		else
			EMSG1("[do_ansemble_acquisition] mrevent_wait error: %d\n", ret);

		ret = -100;
	}
#endif

	return ret;
}


static int full_scan_freq(int demod_no)
{
	int ret;
	unsigned int num_freq;
	const DAB_FREQ_TBL_INFO *freq_tbl_ptr;
#ifdef RTV_TDMB_ENABLE
	IOCTL_TDMB_SCAN_INFO scan_info;
#else
	IOCTL_DAB_SCAN_INFO scan_info;
#endif
	double scan_end_time;

	freq_tbl_ptr = get_dab_freq_table_from_user(&num_freq);
	if(freq_tbl_ptr == NULL)
		return -1;

#ifdef RTV_DAB_ENABLE
	full_scan_state = TRUE;
#endif	

	/* The first, we close all sub channels.*/
	close_all_sub_channels(demod_no);

	/* The 2nd, we close FIC to stop the receving of FIC data 
	and initialize the FIC decoder.*/
	close_fic(demod_no);
	
	dab_tdmb_fic_size[demod_no] = 0;

	open_fic_dump_file(demod_no);

	time_elapse();

	do {
		DMSG1("[full_scan_freq] Scan start: %u\n", freq_tbl_ptr->freq);
		
		/* Update the prev freq */
		mtv_prev_channel[demod_no] = freq_tbl_ptr->freq;		
		scan_info.ch_freq_khz = freq_tbl_ptr->freq;

		/* Open FIC anyway becase of scanning at weak singla area. */
		open_fic(demod_no);

		scan_info.demod_no = demod_no;

	#ifdef RTV_TDMB_ENABLE
		ret = ioctl(fd_dab_dmb_dev[demod_no], IOCTL_TDMB_SCAN_FREQ, &scan_info);
	#else
		ret = ioctl(fd_dab_dmb_dev[demod_no], IOCTL_DAB_SCAN_FREQ, &scan_info);
	#endif
		if(ret == 0)
		{
			DMSG2("[full_scan_freq] Channel Detected %s(%u)\n",	freq_tbl_ptr->str, freq_tbl_ptr->freq);

			do_ansemble_acquisition(demod_no, freq_tbl_ptr->freq);
		}
		else
		{
			dab_tdmb_fic_size[demod_no] = 0; /////

			if(scan_info.tuner_err_code == RTV_CHANNEL_NOT_DETECTED)
				DMSG3("[full_scan_freq] Channel NOT dectected %s(%u): %d\n",
					freq_tbl_ptr->str, freq_tbl_ptr->freq, scan_info.tuner_err_code);
			else
				EMSG3("[full_scan_freq] Scan Devcie Error %s(%u): %d\n",
					freq_tbl_ptr->str, freq_tbl_ptr->freq, scan_info.tuner_err_code);
		}	

		/* Close FIC to stop the receving of FIC data at read() */
		close_fic(demod_no);

		DMSG0("\n");

		freq_tbl_ptr++;
	} while(--num_freq );

	scan_end_time = time_elapse();
	DMSG1("[full_scan_freq] Total scan time: %f\n\n", scan_end_time);

	dab_tdmb_fic_size[demod_no] = 0;

#ifdef RTV_DAB_ENABLE
	full_scan_state[demod_no] = FALSE;
#endif

	close_fic_dump_file(demod_no);

	return 0;
}

static int power_up(int demod_no)
{
	int ret;
	int tuner_err_code;
	IOCTL_POWER_ON_INFO param;

	if(is_power_on[demod_no] == TRUE)
		return 0;

	reset_subch_info(demod_no);

	param.demod_no = demod_no;
#ifdef RTV_TDMB_ENABLE
	ret = ioctl(fd_dab_dmb_dev[demod_no], IOCTL_TDMB_POWER_ON, &param);
#else
	ret = ioctl(fd_dab_dmb_dev[demod_no], IOCTL_DAB_POWER_ON, &param);
#endif
	if(ret < 0)
	{
		EMSG1("[DMB] POWER_ON failed: %d\n", param.tuner_err_code);
		return ret;		
	}

	mtv_prev_channel[demod_no] = 0;

#ifdef RTV_DAB_ENABLE
	full_scan_state[demod_no] = FALSE; /* User can be start with the previous opened channel. */
#endif

	is_open_fic[demod_no] = FALSE;
	dab_tdmb_fic_size[demod_no] = 0;
	
	rtvFICDEC_Init(demod_no);

	DAB_LOCK_INIT(demod_no);

	/* Create the read thread for AV demod. */
	if (demod_no == 0) {
		if((ret = create_mtv_read_thread(0, tdmb_dab_read_thread_primary)) != 0)
		{
			EMSG1("[DMB] create_mtv_read_thread() failed: %d\n", ret);
			return ret;
		}
	}

#if (MAX_NUM_BB_DEMOD == 2)
	if (demod_no == 1) {
		if((ret = create_mtv_read_thread(1, tdmb_dab_read_thread_secondary)) != 0)
		{
			EMSG1("[DMB] create_mtv_read_thread() failed: %d\n", ret);
			return ret;
		}
	}
#endif

	init_periodic_debug_info(demod_no);
	def_dm_timer(tdmb_dab_dm_timer_handler);

	is_power_on[demod_no] = TRUE;

	return 0;
}

static int power_down(int demod_no)
{
	int ret;
	IOCTL_POWER_OFF_INFO param;
	
	if(is_power_on[demod_no] == FALSE)
		return 0;

	suspend_dm_timer();
	
	close_all_sub_channels(demod_no);

	close_fic_dump_file(demod_no);

	DAB_FREE(demod_no);
	DAB_LOCK_DEINIT(demod_no);

printf("[TDMB power_down] IOCTL_TDMB_POWER_OFF\n");

	param.demod_no = demod_no;
#ifdef RTV_TDMB_ENABLE
	ret = ioctl(fd_dab_dmb_dev[demod_no], IOCTL_TDMB_POWER_OFF, &param);
#else
	ret = ioctl(fd_dab_dmb_dev[demod_no], IOCTL_DAB_POWER_OFF);
#endif
	if(ret < 0)
	{
		EMSG1("[DMB] POWER_OFF failed: %d\n", ret);
		return -1;
	}

printf("[TDMB power_down] IOCTL_TDMB_POWER_OFF Done...\n");

	is_power_on[demod_no] = FALSE;

	delete_mtv_read_thread(demod_no);

printf("[TDMB power_down] delete_mtv_read_thread() Done...\n");

	return 0;
}

#if 0
static void AGING_TEST_sub_channel_change(void)
{
	IOCTL_TDMB_SUB_CH_INFO sub_ch_info_1st, sub_ch_info_2nd;
	unsigned int ch_freq_khz, subch_id_1st, subch_id_2nd;
	E_RTV_SERVICE_TYPE svc_type_1st, svc_type_2nd;
	unsigned int test_cnt;

	ch_freq_khz = get_dab_freq_from_user();

	DMSG0("\t#### Input 1st sub channel ID and service type ####\n");
	subch_id_1st = get_subch_id_from_user();
	svc_type_1st = get_dab_service_type_from_user();

	sub_ch_info_1st.ch_freq_khz = ch_freq_khz;
	sub_ch_info_1st.subch_id = subch_id_1st;
	sub_ch_info_1st.svc_type = svc_type_1st;

	DMSG0("\t#### Input 2nd sub channel ID and service type ####\n");
	subch_id_2nd = get_subch_id_from_user();
	svc_type_2nd = get_dab_service_type_from_user();

	sub_ch_info_2nd.ch_freq_khz = ch_freq_khz;
	sub_ch_info_2nd.subch_id = subch_id_2nd;
	sub_ch_info_2nd.svc_type = svc_type_2nd;

	while (1) {
		DMSG0("Input test mode(0: Forever, 1: Limit):");
		scanf("%u", &test_cnt);
		CLEAR_STDIN;

		if((test_cnt == 0) || (test_cnt == 1))
			break;
	}

	if (test_cnt == 0) /* forever */
		test_cnt = 0xFFFFFFFF;
	else {
		DMSG0("Input Test count:");
		scanf("%u", &test_cnt);
		CLEAR_STDIN;
	}

	power_up();

	while (test_cnt) {
		open_sub_channel(&sub_ch_info_1st);
		sleep(5); /* to play */
		close_sub_channel(subch_id_1st);

		open_sub_channel(&sub_ch_info_2nd);
		sleep(5);
		close_sub_channel(subch_id_2nd);

		if (test_cnt == 0xFFFFFFFF) /* forever */
			continue;
		else
			test_cnt--;
	}

	power_down();
}
#endif

static void AGING_TEST_power_on_off(void)
{
}

#if 0
static void AGING_TEST_tdmb_dab(int demod_no)
{
	int key, ret;

	while(1)
	{
		DMSG1("============== [%s] AGING TEST =====================\n", TEST_MODE_STR);
		DMSG1("\t0: %s Power On/Off\n", TEST_MODE_STR);
		DMSG1("\t1: %s Scan\n", TEST_MODE_STR);
		DMSG1("\t2: %s Freq Change\n", TEST_MODE_STR);
		DMSG1("\t3: %s Sub channel Change\n", TEST_MODE_STR);

		key = getc(stdin);				
		CLEAR_STDIN;

		switch(key)	{
			case '0':
				DMSG1("[%s Power On/Off]\n", TEST_MODE_STR);
				AGING_TEST_power_on_off(); 			
				break;
				
			case '1':	
				DMSG1("[%s Scan]\n", TEST_MODE_STR);
				//power_down();
				break;
		
			case '2':
				DMSG1("[%s Freq Change]\n", TEST_MODE_STR);
				//full_scan_freq();				
				break;

			case '3':
				DMSG1("[%s Sub channel Change]\n", TEST_MODE_STR);
				//AGING_TEST_sub_channel_change();
				break;
			case 'q':
			case 'Q':
				goto AGING_TEST_tdmb_dab_exit;
		
			default:
				DMSG1("[%c]\n", key);
		}
		DMSG0("\n");
	}

AGING_TEST_tdmb_dab_exit:
	power_down();
	DMSG0("AGING_TEST_tdmb_dab EXIT\n");
}
#endif

///
#if (MAX_NUM_BB_DEMOD == 2)
void test_TDMB_DAB_proc(int demod_no)
{
	int key, ret;
	IOCTL_TDMB_SUB_CH_INFO sub_ch_info;
	unsigned int ch_freq_khz, subch_id;
	E_RTV_SERVICE_TYPE svc_type;

	if((fd_dab_dmb_dev[demod_no]=open_mtv_device(demod_no)) < 0)
		return;

	while(1)
	{
		DMSG1("==============[%s] =========================\n", TEST_MODE_STR);
		DMSG1("\t0: %s Power ON\n", TEST_MODE_STR);
		DMSG1("\t1: %s Power OFF\n", TEST_MODE_STR);
		
		DMSG1("\t2: %s Scan freq\n", TEST_MODE_STR);
		DMSG1("\t3: %s Open Sub Channel\n", TEST_MODE_STR);
		DMSG1("\t4: %s Close 1 Sub Channel\n", TEST_MODE_STR);
		DMSG1("\t5: %s Close All Sub Channels\n", TEST_MODE_STR);
		DMSG1("\t6: %s Get Lockstatus\n", TEST_MODE_STR);
		DMSG1("\t7: %s Get Signal Info\n", TEST_MODE_STR);
		
		DMSG0("\t8: [TEST] Register IO Test\n");
		DMSG0("\t9: [TEST] Auto Open Subchannel Test\n");

		DMSG0("\ts: [DEBUG] Show the periodic SIGNAL information\n");
		DMSG0("\th: [DEBUG] Hide the periodic SIGNAL information\n");
		
		DMSG0("\tp: [DEBUG] Show the periodic TSP statistics\n");
		DMSG0("\tc: [DEBUG] Hide the periodic TSP statistics\n");

		DMSG0("\ta: [TEST] Aging Test\n");

		DMSG0("\tg: [TEST] GPIO Test\n");
		DMSG0("\tr: Register IO Test\n");

		DMSG0("\tq or Q: Quit\n");
		DMSG0("===============================================\n");
   		
		key = getc(stdin);				
		CLEAR_STDIN;

		if((key >= '2') && (key <= '9'))
		{
			if(is_power_on[demod_no] == FALSE)
			{
				DMSG0("Power Down state! Must Power ON!\n");
				continue;
			}
		}
		
		switch( key )
		{
			case '0':
				DMSG0("[DAB Power ON]\n");
				power_up(demod_no);				
				break;
				
			case '1':	
				DMSG0("[DAB Power OFF]\n");
				power_down(demod_no);
				break;

			case '2':
				DMSG0("[DAB Scan freq]\n");
				full_scan_freq(demod_no);				
				break;

			case '3':
				DMSG0("[DAB Open Sub Channel]\n");
				ch_freq_khz = get_dab_freq_from_user();
				subch_id = get_subch_id_from_user();
				svc_type = get_dab_service_type_from_user();

				sub_ch_info.ch_freq_khz = ch_freq_khz;
				sub_ch_info.subch_id = subch_id;
				sub_ch_info.svc_type = svc_type;
				open_sub_channel(demod_no, &sub_ch_info);
				break;

			case '4':
				DMSG0("[DAB Close Sub Channel]\n");
				subch_id = get_subch_id_from_user();
				close_sub_channel(demod_no, subch_id);
				break;

			case '5':
				DMSG0("[DAB Close ALL Sub Channels]\n");
				close_all_sub_channels(demod_no);
				break;

			case '6':
				DMSG0("[DAB Get Lockstatus]\n");
				check_lock_status(demod_no);
				break;

			case '7':
				DMSG0("[DAB Get Singal Info]\n");
				check_signal_info(demod_no);
				break;

			case '8':
				DMSG0("[TEST] Register IO Test\n");
				test_RegisterIO(demod_no, fd_dab_dmb_dev[demod_no]);
				break;

			case '9':
				DMSG0("[TEST] DAB Auto Open Subchannel Test\n");

				sub_ch_info.ch_freq_khz = 183008;
				sub_ch_info.subch_id = 1;
				sub_ch_info.svc_type = RTV_SERVICE_VIDEO;
				open_sub_channel(demod_no, &sub_ch_info);

				sub_ch_info.subch_id = 3;
				sub_ch_info.svc_type = RTV_SERVICE_DATA;
				open_sub_channel(demod_no, &sub_ch_info);

/*
				sub_ch_info.ch_freq_khz = 208736;
				sub_ch_info.subch_id = 0;
				sub_ch_info.svc_type = RTV_SERVICE_VIDEO;
				open_sub_channel(&sub_ch_info);
				
				sub_ch_info.ch_freq_khz = 208736;
				sub_ch_info.subch_id = 3;
				sub_ch_info.svc_type = RTV_SERVICE_DATA;
				open_sub_channel(&sub_ch_info);
		#if 1
				sub_ch_info.ch_freq_khz = 208736;
				sub_ch_info.subch_id = 8;
				sub_ch_info.svc_type = RTV_SERVICE_DATA;
				open_sub_channel(&sub_ch_info);
		#endif
*/
				break;

			case 's':
				DMSG0("[DEBUG] Show the periodic SIGNAL information\n");
				show_periodic_sig_info(demod_no);
				break;

			case 'h':
				DMSG0("[DEBUG] Hide the periodic SIGNAL information\n");
				hide_periodic_sig_info(demod_no);
				break;

			case 'p':
				DMSG0("[DEBUG] Show the periodic TSP statistics\n");
				show_periodic_tsp_statistics(demod_no);
				break;

			case 'c':
				DMSG0("[DEBUG] Hide the periodic TSP statistics\n");
				hide_periodic_tsp_statistics(demod_no);
				break;

			case 'a':
				DMSG0("[TEST] Aging Test\n");
				//AGING_TEST_tdmb_dab();
				break;

			case 'g':
				test_GPIO(demod_no, fd_dab_dmb_dev[demod_no]);
				break;

			case 'r':
				test_RegisterIO(demod_no, fd_dab_dmb_dev[demod_no]);
				break;

			case 'q':
			case 'Q':
				goto TDMB_DAB_EXIT;

			default:
				DMSG1("[%c]\n", key);
		}
		
		DMSG0("\n");
	} 

TDMB_DAB_EXIT:
	power_down(demod_no);

	ret = close_mtv_device(fd_dab_dmb_dev[demod_no]);
	DMSG2("[DMB] %d close() result : %d\n", demod_no, ret);

	DMSG0("TDMB_DAB_EXIT\n");
	
	return;
}

void test_TDMB_DAB(void)
{
	int key, ret;
	IOCTL_TDMB_SUB_CH_INFO sub_ch_info;
	unsigned int ch_freq_khz, subch_id;
	E_RTV_SERVICE_TYPE svc_type;
	int demod_no;

	is_power_on[0] = FALSE;
	is_power_on[1] = FALSE;

#if defined(RTV_IF_TSIF) || defined(RTV_IF_SPI_SLAVE)
	if((fd_dab_tsif_dev=open_tsif_device()) < 0)
		return;

	#if defined(TSIF_SAMSUNG_AP)
	tsif_run(fd_dab_tsif_dev, 1);
	#endif
#endif

#ifdef RTV_FIC_I2C_INTR_ENABLED
	setup_tsif_fic_sig_handler();
#endif

	mrevent_init(&fic_parsing_event);

	while (1) {
		DMSG1("==============[%s] =========================\n", TEST_MODE_STR);
#if (MAX_NUM_BB_DEMOD == 2)		
		DMSG0("============== [Select Demod Chip] ==================\n");
		DMSG0("\t0: Primaray chip\n");
		DMSG0("\t1: Secondary Chip\n");
#endif
		DMSG0("\tq or Q: Quit\n");
		DMSG0("===============================================\n");

#if (MAX_NUM_BB_DEMOD == 2)
		demod_no = getc(stdin);				
		CLEAR_STDIN;
#else
		demod_no = 0; /* Primaray */
		test_TDMB_DAB_proc(0);
#endif

		switch (demod_no) {
		case '0':
			DMSG1("[%s] Primaray chip Selected.\n", TEST_MODE_STR);
			test_TDMB_DAB_proc(BB_AV_DEMOD_IDX);
			return;
			
		case '1':	
			DMSG1("[%s] Secondary chip Selected.\n", TEST_MODE_STR);
			test_TDMB_DAB_proc(BB_TPEG_DEMOD_IDX);
			return;

		case 'q':
		case 'Q':
			goto TDMB_DAB_EXIT;

		default:
			DMSG2("[%s] Invalid input number (%d).\n", TEST_MODE_STR, demod_no);
			break;
		}

		DMSG0("\n");
	} 

TDMB_DAB_EXIT:
	
#if defined(RTV_IF_TSIF) || defined(RTV_IF_SPI_SLAVE)
	close(fd_dab_tsif_dev);
#endif

	DMSG0("TDMB_DAB_EXIT\n");
	
	return;
}

#else
void test_TDMB_DAB(void)
{
	int key, ret;
	IOCTL_TDMB_SUB_CH_INFO sub_ch_info;
	unsigned int ch_freq_khz, subch_id;
	E_RTV_SERVICE_TYPE svc_type;

	is_power_on[0] = FALSE;

	if((fd_dab_dmb_dev[BB_AV_DEMOD_IDX] = open_mtv_device(BB_AV_DEMOD_IDX)) < 0)
		return;
		
#if defined(RTV_IF_TSIF) || defined(RTV_IF_SPI_SLAVE)
	if((fd_dab_tsif_dev=open_tsif_device()) < 0)
		return;

	#if defined(TSIF_SAMSUNG_AP)
	tsif_run(fd_dab_tsif_dev, 1);
	#endif
#endif

#ifdef RTV_FIC_I2C_INTR_ENABLED
	setup_tsif_fic_sig_handler();
#endif

	mrevent_init(&fic_parsing_event);
	
	while(1)
	{
		DMSG1("==============[%s] =========================\n", TEST_MODE_STR);
		DMSG1("\t0: %s Power ON\n", TEST_MODE_STR);
		DMSG1("\t1: %s Power OFF\n", TEST_MODE_STR);
		
		DMSG1("\t2: %s Scan freq\n", TEST_MODE_STR);
		DMSG1("\t3: %s Open Sub Channel\n", TEST_MODE_STR);
		DMSG1("\t4: %s Close 1 Sub Channel\n", TEST_MODE_STR);
		DMSG1("\t5: %s Close All Sub Channels\n", TEST_MODE_STR);
		DMSG1("\t6: %s Get Lockstatus\n", TEST_MODE_STR);
		DMSG1("\t7: %s Get Signal Info\n", TEST_MODE_STR);
		
		DMSG0("\t8: [TEST] Register IO Test\n");
		DMSG0("\t9: [TEST] Auto Open Subchannel Test\n");

		DMSG0("\ts: [DEBUG] Show the periodic SIGNAL information\n");
		DMSG0("\th: [DEBUG] Hide the periodic SIGNAL information\n");
		
		DMSG0("\tp: [DEBUG] Show the periodic TSP statistics\n");
		DMSG0("\tc: [DEBUG] Hide the periodic TSP statistics\n");

		DMSG0("\ta: [TEST] Aging Test\n");

		DMSG0("\tg: [TEST] GPIO Test\n");
		DMSG0("\tr: Register IO Test\n");

		DMSG0("\tq or Q: Quit\n");
		DMSG0("===============================================\n");
   		
		key = getc(stdin);				
		CLEAR_STDIN;

		if((key >= '2') && (key <= '9'))
		{
			if(is_power_on[0] == FALSE)
			{
				DMSG0("Power Down state! Must Power ON!\n");
				continue;
			}
		}
		
		switch( key )
		{
			case '0':
				DMSG0("[DAB Power ON]\n");
				power_up(BB_AV_DEMOD_IDX);				
				break;
				
			case '1':	
				DMSG0("[DAB Power OFF]\n");
				power_down(BB_AV_DEMOD_IDX);
				break;

			case '2':
				DMSG0("[DAB Scan freq]\n");
				full_scan_freq(BB_AV_DEMOD_IDX);				
				break;

			case '3':
				DMSG0("[DAB Open Sub Channel]\n");
				ch_freq_khz = get_dab_freq_from_user();
				subch_id = get_subch_id_from_user();
				svc_type = get_dab_service_type_from_user();

				sub_ch_info.ch_freq_khz = ch_freq_khz;
				sub_ch_info.subch_id = subch_id;
				sub_ch_info.svc_type = svc_type;
				open_sub_channel(BB_AV_DEMOD_IDX, &sub_ch_info);
				break;

			case '4':
				DMSG0("[DAB Close Sub Channel]\n");
				subch_id = get_subch_id_from_user();
				close_sub_channel(BB_AV_DEMOD_IDX, subch_id);
				break;

			case '5':
				DMSG0("[DAB Close ALL Sub Channels]\n");
				close_all_sub_channels(BB_AV_DEMOD_IDX);
				break;

			case '6':
				DMSG0("[DAB Get Lockstatus]\n");
				check_lock_status(BB_AV_DEMOD_IDX);
				break;

			case '7':
				DMSG0("[DAB Get Singal Info]\n");
				check_signal_info(BB_AV_DEMOD_IDX);
				break;

			case '8':
				DMSG0("[TEST] Register IO Test\n");
				test_RegisterIO(BB_AV_DEMOD_IDX, fd_dab_dmb_dev[BB_AV_DEMOD_IDX]);
				break;

			case '9':
				DMSG0("[TEST] DAB Auto Open Subchannel Test\n");

				sub_ch_info.ch_freq_khz = 183008;
				sub_ch_info.subch_id = 1;
				sub_ch_info.svc_type = RTV_SERVICE_VIDEO;
				open_sub_channel(BB_AV_DEMOD_IDX, &sub_ch_info);

				sub_ch_info.subch_id = 3;
				sub_ch_info.svc_type = RTV_SERVICE_DATA;
				open_sub_channel(BB_AV_DEMOD_IDX, &sub_ch_info);

/*
				sub_ch_info.ch_freq_khz = 208736;
				sub_ch_info.subch_id = 0;
				sub_ch_info.svc_type = RTV_SERVICE_VIDEO;
				open_sub_channel(&sub_ch_info);
				
				sub_ch_info.ch_freq_khz = 208736;
				sub_ch_info.subch_id = 3;
				sub_ch_info.svc_type = RTV_SERVICE_DATA;
				open_sub_channel(&sub_ch_info);
		#if 1
				sub_ch_info.ch_freq_khz = 208736;
				sub_ch_info.subch_id = 8;
				sub_ch_info.svc_type = RTV_SERVICE_DATA;
				open_sub_channel(&sub_ch_info);
		#endif
*/
				break;

			case 's':
				DMSG0("[DEBUG] Show the periodic SIGNAL information\n");
				show_periodic_sig_info(BB_AV_DEMOD_IDX);
				break;

			case 'h':
				DMSG0("[DEBUG] Hide the periodic SIGNAL information\n");
				hide_periodic_sig_info(BB_AV_DEMOD_IDX);
				break;

			case 'p':
				DMSG0("[DEBUG] Show the periodic TSP statistics\n");
				show_periodic_tsp_statistics(BB_AV_DEMOD_IDX);
				break;

			case 'c':
				DMSG0("[DEBUG] Hide the periodic TSP statistics\n");
				hide_periodic_tsp_statistics(BB_AV_DEMOD_IDX);
				break;

			case 'a':
				DMSG0("[TEST] Aging Test\n");
				//AGING_TEST_tdmb_dab(0);
				break;

			case 'g':
				test_GPIO(BB_AV_DEMOD_IDX, fd_dab_dmb_dev[BB_AV_DEMOD_IDX]);
				break;

			case 'r':
				test_RegisterIO(BB_AV_DEMOD_IDX, fd_dab_dmb_dev[BB_AV_DEMOD_IDX]);
				break;

			case 'q':
			case 'Q':
				goto TDMB_DAB_EXIT;

			default:
				DMSG1("[%c]\n", key);
		}
		
		DMSG0("\n");
	} 

TDMB_DAB_EXIT:

	power_down(BB_AV_DEMOD_IDX);

	ret = close_mtv_device(fd_dab_dmb_dev[BB_AV_DEMOD_IDX]);
	DMSG2("[DMB] %s close() result : %d\n", NXB110TV_DEV_NAME, ret);
	
#if defined(RTV_IF_TSIF) || defined(RTV_IF_SPI_SLAVE)
	close(fd_dab_tsif_dev);
#endif

	DMSG0("TDMB_DAB_EXIT\n");
	
	return;
}
#endif /* #if (MAX_NUM_BB_DEMOD == 2) */

#endif /* #if defined(RTV_DAB_ENABLE) || defined(RTV_TDMB_ENABLE) */

