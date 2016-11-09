#include "test.h"
#include "mtv319_ficdec.h"


/*============================================================================
 * Configuration for File dump
 *===========================================================================*/
//#define _TDMB_MSC_FILE_DUMP_ENABLE /* for MSC data*/
//#define _TDMB_FIC_FILE_DUMP_ENABLE /* for FIC data */

/* MSC filename: /data/raontech/tdmb_msc_FREQ_SUBCHID.ts */
#define TDMB_DUMP_MSC_FILENAME_PREFIX		"tdmb_msc"
#define TDMB_DUMP_FIC_FILENAME_PREFIX		"tdmb_fic"


#define INVALIDE_SUBCH_ID	0xFFFF

#define MAX_NUM_SUB_CHANNEL		64
typedef struct {
	BOOL opened;
	enum E_RTV_SERVICE_TYPE svc_type;
	
	unsigned int msc_buf_index;

#ifdef _TDMB_MSC_FILE_DUMP_ENABLE
	FILE *fd_msc;
	char fname[64];
#endif
} SUB_CH_INFO;

#ifdef _TDMB_FIC_FILE_DUMP_ENABLE
	static FILE *fd_fic;
	static char fic_fname[64];
#endif

#ifdef RTV_DAB_ENABLE
static volatile BOOL full_scan_state;
#endif

static volatile BOOL is_open_fic;
static unsigned int tdmb_fic_size;

static BOOL is_power_on;

static int fd_tdmb_dev; /* MTV device file descriptor. */

#if defined(RTV_IF_TSIF)
static int fd_tdmb_tsif_dev;
#endif

/* Use the mutex for lock the add/delete/set_reconfiguration subch 
at read, fic, main threads when DAB reconfiguration occured. */
static pthread_mutex_t tdmb_mutex;

#define TDMB_LOCK_INIT	pthread_mutex_init(&tdmb_mutex, NULL)
#define TDMB_LOCK		pthread_mutex_lock(&tdmb_mutex)
#define TDMB_FREE		pthread_mutex_unlock(&tdmb_mutex)
#define TDMB_LOCK_DEINIT	((void)0)

#ifdef RTV_MULTIPLE_CHANNEL_MODE
static IOCTL_TDMB_MULTI_SVC_BUF multi_svc_buf;
#else
static unsigned char single_svc_buf[MAX_READ_TSP_SIZE];
#endif

#if (RTV_MAX_NUM_USE_SUBCHANNEL == 1)
static unsigned int prev_opened_subch_id; /* Previous sub channel ID. used for 1 service */
#endif

static unsigned int av_subch_id;
static unsigned int num_opend_subch;

SUB_CH_INFO subch_info[MAX_NUM_SUB_CHANNEL];

static struct ensemble_info_type ensemble_info;

static struct mrevent fic_parsing_event;

static unsigned int tdmb_num_fic_parsing_done_freq;

#define TEST_MODE_STR	"TDMB"


/*============================================================================
 * Forward local functions.
 *===========================================================================*/
static int check_signal_info(void);
static int close_fic(void);
static int open_fic(void);
static void processing_fic(unsigned char *buf, unsigned int size, unsigned int freq_khz);

static void tdmb_dm_timer_handler(void)
{
	unsigned int periodic_debug_info_mask = get_periodic_debug_info_mask();

	if (periodic_debug_info_mask & SIG_INFO_MASK)
		check_signal_info();

	if (periodic_debug_info_mask & TSP_STAT_INFO_MASK)
	{
		if(av_subch_id != INVALIDE_SUBCH_ID)
		{
			if(subch_info[av_subch_id].svc_type == RTV_SERVICE_DMB) {
				show_video_tsp_statistics();
			}
		}
	}

	if(periodic_debug_info_mask & (SIG_INFO_MASK|TSP_STAT_INFO_MASK))
		DMSG0("\n");
}


//============ Start of file dump =============================
static int open_msc_dump_file(unsigned int ch_freq_khz, unsigned int subch_id)
{
#ifdef _TDMB_MSC_FILE_DUMP_ENABLE
	SUB_CH_INFO *subch_ptr;
	
	if(subch_info[subch_id].fd_msc != NULL)
	{
		EMSG2("[DMB] Fail to open dump file. freq: %u, subch_id: %u\n",
			ch_freq_khz, subch_id);
		return -1;
	}

	subch_ptr = &subch_info[subch_id];

	sprintf(subch_ptr->fname, "%s/%s_%u_%u.ts",
		TS_DUMP_DIR, TDMB_DUMP_MSC_FILENAME_PREFIX, 
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

static int close_msc_dump_file(unsigned int subch_id)
{
#ifdef _TDMB_MSC_FILE_DUMP_ENABLE
	if(subch_info[subch_id].fd_msc != NULL)
	{
		fclose(subch_info[subch_id].fd_msc);
		subch_info[subch_id].fd_msc = NULL;

		DMSG1("[DMB] Closed dump file: %s\n", subch_info[subch_id].fname);
	}
#endif

	return 0;
}

static inline void write_msc_dump_file(const void *buf, unsigned int size,
					unsigned int subch_id)
{
#ifdef _TDMB_MSC_FILE_DUMP_ENABLE
	if(subch_id < MAX_NUM_SUB_CHANNEL)
	{
		if(subch_info[subch_id].fd_msc != NULL)
			fwrite(buf, sizeof(char), size, subch_info[subch_id].fd_msc);
		else
			EMSG0("[write_msc_dump_file] Invalid sub ch ID\n");
	}
#endif
}

static int open_fic_dump_file(void)
{
#ifdef _TDMB_FIC_FILE_DUMP_ENABLE
	if(fd_fic != NULL)
	{
		EMSG0("[DMB] Must close dump file before open the new file\n");
		return -1;
	}
		
	sprintf(fic_fname, "%s/%s.ts",
		TS_DUMP_DIR, TDMB_DUMP_FIC_FILENAME_PREFIX);
	
	if((fd_fic=fopen(fic_fname, "wb")) == NULL)
	{
		EMSG1("[DMB] Fail to open error: %s\n", fic_fname);
		return -2;
	}

	DMSG1("[DMB] Opend FIC dump file: %s\n", fic_fname);
#endif

	return 0;
}

static int close_fic_dump_file(void)
{
#ifdef _TDMB_FIC_FILE_DUMP_ENABLE
	if(fd_fic != NULL)
	{
		fclose(fd_fic);
		fd_fic = NULL;

		DMSG1("[MTV] Closed FIC dump file: %s\n", fic_fname);
	}
#endif

	return 0;
}
//============ END of file dump =============================


static inline int get_fic_size(void)
{
	unsigned int fic_size = 0;

	fic_size = 384;

	DMSG1("[get_fic_size] fic_size: %d\n", fic_size);

	return (int)fic_size;
}

#ifdef RTV_FIC_I2C_INTR_ENABLED
static void tsif_fic_sig_handler(int signo)
{
	int ret;
	IOCTL_TDMB_READ_FIC_INFO read_info;
	int fic_size = 384;//get_fic_size();

	//EMSG0("[tsif_fic_sig_handler] Enter.\n");

	ret = ioctl(fd_tdmb_dev, IOCTL_TDMB_READ_FIC, &read_info);
	if (ret == 0) {
		if(fic_size != 0)
			processing_fic(read_info.buf, fic_size, mtv_prev_channel);
		else {
			EMSG0("[tsif_fic_sig_handler] fic_size error.\n");
		#ifdef RTV_DAB_ENABLE
			fic_size = get_fic_size(); /* Retry for DAB */
			read_info.size = fic_size;
		#endif
		}
	}
	else {
		//EMSG1("[tsif_fic_sig_handler] READ_FIC ioctl error. tuner_err_code(%d)\n",
		//	read_info.tuner_err_code);
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
	if (sigaction(SIGIO, &sigact, NULL) < 0) {
		EMSG1("[%s] sigaction() error\n", TEST_MODE_STR);
		return -1;
	}

	fcntl(fd_tdmb_dev, F_SETOWN, getpid());
	oflag = fcntl(fd_tdmb_dev, F_GETFL);
	fcntl(fd_tdmb_dev, F_SETFL, oflag | FASYNC);

	return 0;
}
#endif /* #ifdef RTV_FIC_I2C_INTR_ENABLED */


static void reset_subch_info(void)
{
	unsigned int i;

	for(i=0; i<MAX_NUM_SUB_CHANNEL; i++)
	{
		subch_info[i].opened = FALSE;
#ifdef _TDMB_MSC_FILE_DUMP_ENABLE
		subch_info[i].fd_msc = NULL;
#endif
	}

#ifdef _TDMB_FIC_FILE_DUMP_ENABLE
	fd_fic = NULL;
#endif

	num_opend_subch = 0;
	av_subch_id = INVALIDE_SUBCH_ID;	

#if (RTV_MAX_NUM_USE_SUBCHANNEL == 1)  /* Single Sub Channel Mode */
	prev_opened_subch_id = INVALIDE_SUBCH_ID;
#endif
}

static int close_fic(void)
{
	IOCTL_CLOSE_FIC_INFO param;
	
	TDMB_LOCK;

	DMSG0("[close_fic] Enter\n");
	
	if (is_open_fic) {
		if (ioctl(fd_tdmb_dev, IOCTL_TDMB_CLOSE_FIC, &param) < 0)
			EMSG0("[DMB] CLOSE_FIC failed.\n");
	
		is_open_fic = FALSE;
	}

	tdmb_fic_size = 0;

	TDMB_FREE;
	
	return 0;
}

static int open_fic(void)
{
	int ret;
	IOCTL_OPEN_FIC_INFO param;

	DMSG0("[open_fic] Enter\n");

	TDMB_LOCK;

	if (!is_open_fic) {
		rtvFICDEC_Init(); /* FIC parser Init */

		ret = ioctl(fd_tdmb_dev, IOCTL_TDMB_OPEN_FIC, &param);
		if (ret) {
			EMSG1("[open_fic] OPEN_FIC failed: %d\n", param.tuner_err_code);
		}

		is_open_fic = TRUE;
	}

	TDMB_FREE;

	DMSG0("[open_fic] Leave\n");

	return 0;
}

static enum E_RTV_FIC_DEC_RET_TYPE proc_fic_parsing(unsigned char *buf, 
			unsigned int size, unsigned int freq_khz)
{
	enum E_RTV_FIC_DEC_RET_TYPE ficdec_ret;
	
//	DMSG6("\t[FIC(%u)] size(%u) 0x%02X 0x%02X 0x%02X 0x%02X\n",
//		freq_khz, size, buf[0], buf[1], buf[2], buf[3]);

#ifdef _TDMB_FIC_FILE_DUMP_ENABLE
	if(fd_fic != NULL)
		fwrite(buf, sizeof(char), size, fd_fic);
#endif

	ficdec_ret = rtvFICDEC_Decode(buf, size);
	if (ficdec_ret != RTV_FIC_RET_GOING) {
		DMSG1("[DMB] FIC parsing result: %s\n",
			(ficdec_ret==RTV_FIC_RET_DONE ? "RTV_FIC_RET_DONE" : "RTV_FIC_RET_CRC_ERR"));

		if (ficdec_ret == RTV_FIC_RET_DONE) {
			rtvFICDEC_GetEnsembleInfo(&ensemble_info, (unsigned long)freq_khz);

			// Show a decoded table.
			show_fic_information(&ensemble_info, freq_khz);

			tdmb_num_fic_parsing_done_freq++;
		}
		else if (ficdec_ret == RTV_FIC_RET_CRC_ERR)
			DMSG1("[DMB] FIC CRC error (%u)\n", freq_khz);
	}

	return ficdec_ret;
}


#ifndef RTV_FIC_POLLING_MODE /* FIC interrupt mode */
static void processing_fic(unsigned char *buf, unsigned int size, unsigned int freq_khz)
{
	enum E_RTV_FIC_DEC_RET_TYPE ficdec_ret;

	DMSG6("\t[processing_fic(%u)] size(%u) 0x%02X 0x%02X 0x%02X 0x%02X\n",
		freq_khz, size, buf[0], buf[1], buf[2], buf[3]);

	ficdec_ret = proc_fic_parsing(buf, size, freq_khz);

	DMSG1("[processing_fic] ficdec_ret(%d)\n", ficdec_ret);

	if (ficdec_ret == RTV_FIC_RET_GOING)
		return;

	//DMSG1("[processing_fic] ficdec_ret(%d)\n", ficdec_ret);

	/* Set the event to allow scanning of the next freq. */
	mrevent_trigger(&fic_parsing_event);
}
#endif /* #ifndef RTV_FIC_POLLING_MODE */

static void processing_msc(U8 *buf, UINT size, UINT subch_id)
{
{
	int i;

//	printf("\n[processing_msc] size(%u) subch_id(%u)\n");

#if 0
	for (i = 0; i < (MAX_READ_TSP_SIZE/188)/4; i++) {
	//for (i = 0; i < (MAX_READ_TSP_SIZE/188); i++) {
		printf("[processing_msc: %d] 0x%02X 0x%02X 0x%02X 0x%02X, 0x%02X 0x%02X 0x%02X 0x%02X | 0x%02X 0x%02X 0x%02X\n",
			i, buf[i*188+0], buf[i*188+1], buf[i*188+2], buf[i*188+3],
			buf[i*188+4], buf[i*188+5], buf[i*188+6], buf[i*188+7],
			buf[i*188+185], buf[i*188+186], buf[i*188+187]);
	}
#endif
}

	if (subch_info[subch_id].opened == TRUE) {
		switch (subch_info[subch_id].svc_type)
		{
		case RTV_SERVICE_DMB:
			verify_video_tsp(buf, size);
			write_msc_dump_file(buf, size, subch_id);
			break;

		case RTV_SERVICE_DAB:
			DMSG6("\t DAB Subch ID: %d: Subch Size: %d [0x%02X], [0x%02X], [0x%02X], [0x%02X]\n", 
				subch_id, size, buf[0], buf[1], buf[2], buf[3]);
			write_msc_dump_file(buf, size, subch_id);
			break;

		case RTV_SERVICE_DABPLUS:
			DMSG6("\t DAB+ Subch ID: %d: Subch Size: %d [0x%02X], [0x%02X], [0x%02X], [0x%02X]\n", 
				subch_id, size, buf[0], buf[1], buf[2], buf[3]);
			write_msc_dump_file(buf, size, subch_id);
			break;

		default:
			EMSG1("[processing_msc] Invalid subch ID: %u\n", subch_id);
			break;
		}
	}
	else
		EMSG1("[processing_msc] Not opened subch ID: %u\n", subch_id);
}


static void tdmb_read(int dev)
{
	int len;
#ifdef RTV_MULTIPLE_CHANNEL_MODE /* Multi service mode */
	IOCTL_TDMB_MULTI_SVC_BUF *svc = &multi_svc_buf;
	int i;

	len = read(dev, svc, MAX_READ_TSP_SIZE);
//printf("\t[read] len: %d\n", len);

	if (len > 0) {
		for (i = 0; i < RTV_MAX_NUM_USE_SUBCHANNEL; i++) {
			if(svc->msc_size[i] != 0) {
				//DMSG3("[%d]: msc_size: %u, msc_subch_id: %u\n", i, svc->msc_size[i], svc->msc_subch_id[i]);

				processing_msc(svc->msc_buf[i], 
							svc->msc_size[i],
							svc->msc_subch_id[i]);
			}
		}

	#ifndef RTV_FIC_POLLING_MODE
		if(svc->fic_size != 0)
			processing_fic(svc->fic_buf, svc->fic_size, mtv_prev_channel);
	#endif

	}		
	else {
	//#ifndef MTV_BLOCKING_READ_MODE
		usleep(24 * 1000); // 1 CIF period.
	//#endif
	}
	
#else
	len = read(dev, single_svc_buf, MAX_READ_TSP_SIZE);
//printf("\t[read] len: %d\n", len);

	if(len > 0)	{
		processing_msc(single_svc_buf, len, prev_opened_subch_id);
	}
	else {
	#ifndef MTV_BLOCKING_READ_MODE
		usleep(48 * 1000); /* (frame duration / 2) */
	#endif
	}
#endif
}

static void *tdmb_read_thread(void *arg)
{
#if defined(RTV_IF_SPI) || defined(RTV_IF_EBI2)
	int dev = fd_tdmb_dev;
#elif defined(RTV_IF_TSIF)
	int dev = fd_tdmb_tsif_dev; 
#endif	  

	DMSG0("[tdmb_read_thread] Entered\n");

	for(;;) {
		if(mtv_read_thread_should_stop())
			break;

/////////////////////////////////////////
		tdmb_read(dev);
//		sleep(1);
	}

	DMSG0("[tdmb_read_thread] Exit...\n");

	return NULL;
}
	
static int check_signal_info(void)
{
	int ret;
	unsigned int lock;
	IOCTL_TDMB_SIGNAL_INFO sig_info;

	ret = ioctl(fd_tdmb_dev, IOCTL_TDMB_GET_SIGNAL_INFO, &sig_info);
	if(ret < 0)
	{
		EMSG0("[DMB] GET_SIGNAL_INFO failed\n");
		return ret;
	}

	lock = (sig_info.lock_mask == RTV_TDMB_CHANNEL_LOCK_OK) ? 1 : 0;
	DMSG0("\t########## [TDMB Signal Inforamtions] ##############\n");
	DMSG2("\t# LOCK: %u (1:LOCK[0x%02X], 0: UNLOCK)\n", lock, sig_info.lock_mask);
	DMSG1("\t# Antenna Level: %u\n", sig_info.ant_level);
	DMSG1("\t# ber: %f\n", (float)sig_info.ber/RTV_TDMB_BER_DIVIDER);
	DMSG1("\t# cer: %u\n", sig_info.cer);
	DMSG1("\t# cnr: %f\n", (float)sig_info.cnr/RTV_TDMB_CNR_DIVIDER);
	DMSG1("\t# rssi: %f\n", (float)sig_info.rssi/RTV_TDMB_RSSI_DIVIDER);
	DMSG1("\t# per: %u\n", sig_info.per);
	DMSG0("\t###################################################\n");

	return 0;
}

static int check_lock_status(void)
{
	int ret;
	unsigned int lock_mask;
	unsigned int lock;
	IOCTL_TDMB_GET_LOCK_STATUS_INFO param;

	ret = ioctl(fd_tdmb_dev, IOCTL_TDMB_GET_LOCK_STATUS, &param);
	lock = (param.lock_mask == RTV_TDMB_CHANNEL_LOCK_OK) ? 1 : 0;
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


static inline void delete_sub_channel(unsigned int subch_id)
{
	enum E_RTV_SERVICE_TYPE svc_type;
		
	/* Lockup mutex */
	TDMB_LOCK;

	subch_info[subch_id].opened = FALSE;

	/* Get the service type */
	svc_type = subch_info[subch_id].svc_type;
	
	if((svc_type==RTV_SERVICE_DMB) || (svc_type == RTV_SERVICE_DAB))
		av_subch_id = INVALIDE_SUBCH_ID;

#if (RTV_MAX_NUM_USE_SUBCHANNEL == 1)
	prev_opened_subch_id = INVALIDE_SUBCH_ID;
#endif

	if(--num_opend_subch == 0)
	{
		mtv_prev_channel = 0; /* Reset to use open_sub_channel() */
		suspend_dm_timer();
	}

	close_msc_dump_file(subch_id);

	/* Unlock mutex */
	TDMB_FREE;
}

static int close_all_sub_channels(void)
{
	unsigned int subch_id;
	IOCTL_CLOSE_ALL_SUBCHANNELS_INFO param;

	DMSG0("[close_all_sub_channels] Enter\n");

	if(num_opend_subch == 0)
		return 0;

	if (ioctl(fd_tdmb_dev, IOCTL_TDMB_CLOSE_ALL_SUBCHANNELS, &param) < 0) {
		EMSG0("[DMB] CLOSE_ALL_SUBCHANNELS failed\n");
		return -1;
	}
		
	for (subch_id=0; subch_id<MAX_NUM_SUB_CHANNEL; subch_id++) {
		if(num_opend_subch == 0)
			return 0;

		if(subch_info[subch_id].opened == TRUE)
			delete_sub_channel(subch_id);
	}

	DMSG0("[close_all_sub_channels] End\n");

	return 0;
}

static int close_sub_channel(unsigned int subch_id)
{
	int ret;
	IOCTL_CLOSE_SUBCHANNEL_INFO param;

	DMSG1("[close_sub_channel] ID: %u\n", subch_id);

#if (RTV_MAX_NUM_USE_SUBCHANNEL == 1)
	if (prev_opened_subch_id != INVALIDE_SUBCH_ID)
		subch_id = prev_opened_subch_id;
	else {
		DMSG0("[close_sub_channel] Invalid subchannel ID\n");
		return -1;
	}
#else
	if (subch_id > (MAX_NUM_SUB_CHANNEL-1))
		return -1;
#endif

	if (subch_info[subch_id].opened == FALSE) {
		DMSG1("[DMB] Not opened sub channed ID(%d)\n", subch_id);
		return 0;
	}

	param.subch_id = subch_id;

	if ((ret=ioctl(fd_tdmb_dev, IOCTL_TDMB_CLOSE_SUBCHANNEL, &param)) < 0) {
		EMSG1("[DMB] IOCTL_DAB_CLOSE_SUBCHANNEL failed: %d\n", ret);
		return -2;
	}

	delete_sub_channel(subch_id);

	return 0;
}

static inline int add_sub_channel(unsigned int ch_freq_khz,
				unsigned int subch_id,
				enum E_RTV_SERVICE_TYPE svc_type)
{
	int ret = 0;
	
	/* Lockup mutex */
	TDMB_LOCK;

	subch_info[subch_id].opened = TRUE;
	subch_info[subch_id].svc_type = svc_type;

#if (RTV_MAX_NUM_USE_SUBCHANNEL == 1)
	prev_opened_subch_id = subch_id;
#endif

	if ((svc_type==RTV_SERVICE_DMB) || (svc_type == RTV_SERVICE_DAB))
		av_subch_id = subch_id;

	if (num_opend_subch == 0) { /* Debug */
		init_periodic_debug_info();
		
		init_tsp_statistics();
		resume_dm_timer();
	}
	
	open_msc_dump_file(ch_freq_khz, subch_id);

	num_opend_subch++;
	
	/* Unlock mutex */
	TDMB_FREE;

	return ret;
}

static int open_sub_channel(IOCTL_TDMB_SUB_CH_INFO *sub_ch_ptr)
{
	int ret;
	unsigned int subch_id = sub_ch_ptr->subch_id;

	DMSG3("[open_sub_channel] FREQ(%u), SUBCH_ID(%u), SVC_TYPE(%d)\r\n",
			sub_ch_ptr->ch_freq_khz, subch_id, sub_ch_ptr->svc_type);

	if (sub_ch_ptr->ch_freq_khz == mtv_prev_channel) {
		if (subch_info[subch_id].opened) {
			DMSG1("[DMB] Already opened sub channed ID(%d)\n", subch_id);
			return 0;
		}

#if (RTV_MAX_NUM_USE_SUBCHANNEL == 1) /* Single Sub Channel Mode */
		if (prev_opened_subch_id != INVALIDE_SUBCH_ID)
			close_sub_channel(prev_opened_subch_id);
#endif
	}
	else {
#if (RTV_MAX_NUM_USE_SUBCHANNEL == 1) /* Single Sub Channel Mode */
		if (prev_opened_subch_id != INVALIDE_SUBCH_ID)
			close_sub_channel(prev_opened_subch_id);
#else
		close_all_sub_channels();
#endif

#if defined(RTV_MULTIPLE_CHANNEL_MODE) && !defined(RTV_MCHDEC_IN_DRIVER)
		////////temp rtvCIFDEC_Init();
#endif

		/* Update the prev freq. Must after close sub channel. */
		mtv_prev_channel = sub_ch_ptr->ch_freq_khz;
	}

	if((ret=add_sub_channel(sub_ch_ptr->ch_freq_khz, subch_id, sub_ch_ptr->svc_type) < 0))
	{
		EMSG1("[DMB] add sub channel failed: %d\n", sub_ch_ptr->tuner_err_code);
		return -1;
	}

	if((ret=ioctl(fd_tdmb_dev, IOCTL_TDMB_OPEN_SUBCHANNEL, sub_ch_ptr)) < 0)
	{
		delete_sub_channel(subch_id);	

		EMSG1("[DMB] open sub channel failed: %d\n", ret);
		return -2;
	}

	tdmb_fic_size = 384;

	return 0;
}


static int do_ansemble_acquisition(U32 ch_freq_khz)
{
	int ret;
#ifdef RTV_FIC_POLLING_MODE /* FIC polling mode */
	int k;
	enum E_RTV_FIC_DEC_RET_TYPE dec_ret;
	IOCTL_TDMB_READ_FIC_INFO read_info;
	int fic_size = 384;

	for (k = 0; k < 30; k++) { // TEMP
		ret = ioctl(fd_tdmb_dev, IOCTL_TDMB_READ_FIC, &read_info);
		if (ret == 0) {
			dec_ret = proc_fic_parsing(read_info.buf, fic_size, ch_freq_khz);
			if (dec_ret != RTV_FIC_RET_GOING)
				break; /* escape for() loop. */
		} 
		else {
			DMSG1("[do_ansemble_acquisition] IOCTL_TDMB_READ_FIC error: %d\n", read_info.tuner_err_code);
			//break; // delete???
		}
	}

	if (k == 30)
		DMSG0("[do_ansemble_acquisition] Parsing timeout\n");

#else /* FIC interrupt mode */

	EMSG0("[do_ansemble_acquisition] mrevent_wait...\n");
	time_elapse();
//while(1);
	/* Wait for FIC pasring was completed in the specified time. */
	ret = mrevent_wait(&fic_parsing_event, 3000);

	EMSG1("[do_ansemble_acquisition] ret: %d\n", ret);

	if (ret != 0) {
		if (ret == ETIMEDOUT)
			EMSG0("[do_ansemble_acquisition] FIC parsing Timeout!\n");
		else
			EMSG1("[do_ansemble_acquisition] wait error: %d\n", ret);

		ret = -100;
	}

	mrevent_reset(&fic_parsing_event);
	
	printf("End FIC parsing wait %f ms\n", time_elapse());
#endif

	return ret;
}


static int full_scan_freq(void)
{
	int ret;
	unsigned int num_freq;
	const DAB_FREQ_TBL_INFO *freq_tbl_ptr;
	IOCTL_TDMB_SCAN_INFO scan_info;
	double scan_end_time;

	freq_tbl_ptr = get_dab_freq_table_from_user(&num_freq);
	if(freq_tbl_ptr == NULL)
		return -1;

	/* The first, we close all sub channels.*/
	close_all_sub_channels();

	/* The 2nd, we close FIC to stop the receving of FIC data 
	and initialize the FIC decoder.*/
	close_fic();
	
	tdmb_fic_size = 0;

	open_fic_dump_file();

	time_elapse();

	tdmb_num_fic_parsing_done_freq = 0;

	do {
		DMSG1("[full_scan_freq] Scan start: %u\n", freq_tbl_ptr->freq);
		
		/* Update the prev freq */
		mtv_prev_channel = freq_tbl_ptr->freq;		
		scan_info.ch_freq_khz = freq_tbl_ptr->freq;

		ret = ioctl(fd_tdmb_dev, IOCTL_TDMB_SCAN_FREQ, &scan_info);
		if (ret == 0) {
			DMSG2("[full_scan_freq] Channel Detected %s(%u)\n", freq_tbl_ptr->str, freq_tbl_ptr->freq);
			open_fic();

			do_ansemble_acquisition(freq_tbl_ptr->freq);

			/* Close FIC to stop the receving of FIC data at read() */
			close_fic();
		}
		else {
			tdmb_fic_size = 0; /////

			if(scan_info.tuner_err_code == RTV_CHANNEL_NOT_DETECTED)
				DMSG3("[full_scan_freq] Channel NOT dectected %s(%u): %d\n",
					freq_tbl_ptr->str, freq_tbl_ptr->freq, scan_info.tuner_err_code);
			else
				EMSG3("[full_scan_freq] Scan Devcie Error %s(%u): %d\n",
					freq_tbl_ptr->str, freq_tbl_ptr->freq, scan_info.tuner_err_code);
		}	

		DMSG0("\n");

		freq_tbl_ptr++;
	} while(--num_freq );

	close_fic();

	scan_end_time = time_elapse();
	DMSG2("[full_scan_freq] Total scan time: %f, #decoded(%u)\n\n",
		scan_end_time, tdmb_num_fic_parsing_done_freq);

	tdmb_fic_size = 0;

#ifdef RTV_DAB_ENABLE
	full_scan_state = FALSE;
#endif

	close_fic_dump_file();

	return 0;
}

static int power_up(void)
{
	int ret;
	IOCTL_POWER_ON_INFO param;

	if(is_power_on == TRUE)
		return 0;

	reset_subch_info();

	ret = ioctl(fd_tdmb_dev, IOCTL_TDMB_POWER_ON, &param);
	if(ret < 0)
	{
		EMSG1("[DMB] POWER_ON failed: %d\n", param.tuner_err_code);
		return ret;		
	}

	mtv_prev_channel = 0;

#if defined(TSIF_SAMSUNG_AP)
	tsif_run(fd_tdmb_tsif_dev, 1);
#endif

	is_open_fic = FALSE;
	tdmb_fic_size = 0;
	
	rtvFICDEC_Init();

	TDMB_LOCK_INIT;

	/* Create the read thread. */
	if ((ret = create_mtv_read_thread(tdmb_read_thread)) != 0) {
		EMSG1("[DMB] create_mtv_read_thread() failed: %d\n", ret);
		return ret;
	}

	init_periodic_debug_info();
	def_dm_timer(tdmb_dm_timer_handler);

	is_power_on = TRUE;

	return 0;
}

static int power_down(void)
{
	int ret;
	IOCTL_POWER_OFF_INFO param;
	
	if(is_power_on == FALSE)
		return 0;

	suspend_dm_timer();

	close_fic();
	close_all_sub_channels();

	close_fic_dump_file();

	TDMB_FREE;
	TDMB_LOCK_DEINIT;

printf("[TDMB power_down] IOCTL_TDMB_POWER_OFF\n");
	ret = ioctl(fd_tdmb_dev, IOCTL_TDMB_POWER_OFF, &param);
	if(ret < 0)
	{
		EMSG1("[DMB] POWER_OFF failed: %d\n", ret);
		return -1;
	}

printf("[TDMB power_down] IOCTL_TDMB_POWER_OFF Done...\n");

#if defined(TSIF_SAMSUNG_AP)
	tsif_run(fd_tdmb_tsif_dev, 0);
#endif

	is_power_on = FALSE;

	delete_mtv_read_thread();

printf("[TDMB power_down] delete_mtv_read_thread() Done...\n");

	return 0;
}

static void AGING_TEST_sub_channel_change(void)
{
	IOCTL_TDMB_SUB_CH_INFO sub_ch_info_1st, sub_ch_info_2nd;
	unsigned int ch_freq_khz, subch_id_1st, subch_id_2nd;
	enum E_RTV_SERVICE_TYPE svc_type_1st, svc_type_2nd;
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

static void AGING_TEST_power_on_off(void)
{
}

static void AGING_TEST_tdmb(void)
{
	int key, ret;

	while (1) {
		DMSG1("============== [%s] AGING TEST =====================\n", TEST_MODE_STR);
		DMSG1("\t0: %s Power On/Off\n", TEST_MODE_STR);
		DMSG1("\t1: %s Scan\n", TEST_MODE_STR);
		DMSG1("\t2: %s Freq Change\n", TEST_MODE_STR);
		DMSG1("\t3: %s Sub channel Change\n", TEST_MODE_STR);

		key = getc(stdin);				
		CLEAR_STDIN;

		switch (key) {
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
				AGING_TEST_sub_channel_change();
				break;
			case 'q':
			case 'Q':
				goto AGING_TEST_tdmb_exit;
		
			default:
				DMSG1("[%c]\n", key);
		}
		DMSG0("\n");
	}

AGING_TEST_tdmb_exit:
	power_down();
	DMSG0("AGING_TEST_tdmb EXIT\n");
}

void test_TDMB(void)
{
	int key, ret;
	IOCTL_TDMB_SUB_CH_INFO sub_ch_info;
	unsigned int ch_freq_khz, subch_id;
	enum E_RTV_SERVICE_TYPE svc_type;

	is_power_on = FALSE;

	if((fd_tdmb_dev=open_tdmb_device()) < 0)
		return;
		
#if defined(RTV_IF_TSIF)
	if((fd_tdmb_tsif_dev=open_tsif_device()) < 0)
		return;
#endif

#ifdef RTV_FIC_I2C_INTR_ENABLED
	setup_tsif_fic_sig_handler();
#endif

	mrevent_init(&fic_parsing_event);
	
	while (1) {
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

		DMSG0("\tf: Open FIC\n");
		DMSG0("\ti: Close FIC\n");

		DMSG0("\ts: [DEBUG] Show the periodic SIGNAL information\n");
		DMSG0("\th: [DEBUG] Hide the periodic SIGNAL information\n");
		
		DMSG0("\tp: [DEBUG] Show the periodic TSP statistics\n");
		DMSG0("\tc: [DEBUG] Hide the periodic TSP statistics\n");

		DMSG0("\ta: [TEST] Aging Test\n");

		DMSG0("\tq or Q: Quit\n");
		DMSG0("===============================================\n");
   		
		key = getc(stdin);				
		CLEAR_STDIN;

		if ((key >= '2') && (key <= '9')) {
			if (is_power_on == FALSE) {
				DMSG0("Power Down state! Must Power ON!\n");
				continue;
			}
		}
		
		switch (key) {
			case '0':
				DMSG0("[DAB Power ON]\n");
				power_up();				
				break;
				
			case '1':	
				DMSG0("[DAB Power OFF]\n");
				power_down();
				break;

			case '2':
				DMSG0("[DAB Scan freq]\n");
				full_scan_freq();				
				break;

			case '3':
				DMSG0("[DAB Open Sub Channel]\n");
				ch_freq_khz = get_dab_freq_from_user();
				subch_id = get_subch_id_from_user();
				svc_type = get_dab_service_type_from_user();

				sub_ch_info.ch_freq_khz = ch_freq_khz;
				sub_ch_info.subch_id = subch_id;
				sub_ch_info.svc_type = svc_type;
				open_sub_channel(&sub_ch_info);
				break;

			case '4':
				DMSG0("[DAB Close Sub Channel]\n");
				subch_id = get_subch_id_from_user();
				close_sub_channel(subch_id);
				break;

			case '5':
				DMSG0("[DAB Close ALL Sub Channels]\n");
				close_all_sub_channels();
				break;

			case '6':
				DMSG0("[DAB Get Lockstatus]\n");
				check_lock_status();
				break;

			case '7':
				DMSG0("[DAB Get Singal Info]\n");
				check_signal_info();
				break;

			case '8':
				DMSG0("[TEST] Register IO Test\n");
				test_RegisterIO(fd_tdmb_dev);
				break;

			case '9':
				DMSG0("[TEST] DAB Auto Open Subchannel Test\n");

				sub_ch_info.ch_freq_khz = 208736;
				sub_ch_info.subch_id = 1;
				sub_ch_info.svc_type = RTV_SERVICE_DMB;
				open_sub_channel(&sub_ch_info);
				open_fic();

		#if 0
				sub_ch_info.ch_freq_khz = 183008;
				sub_ch_info.subch_id = 1;
				sub_ch_info.svc_type = RTV_SERVICE_DMB;
				open_sub_channel(&sub_ch_info);

				sub_ch_info.subch_id = 3;
				sub_ch_info.svc_type = RTV_SERVICE_DAB;
				open_sub_channel(&sub_ch_info);
		#endif


/*
				sub_ch_info.ch_freq_khz = 208736;
				sub_ch_info.subch_id = 0;
				sub_ch_info.svc_type = RTV_SERVICE_DMB;
				open_sub_channel(&sub_ch_info);
				
				sub_ch_info.ch_freq_khz = 208736;
				sub_ch_info.subch_id = 3;
				sub_ch_info.svc_type = RTV_SERVICE_DAB;
				open_sub_channel(&sub_ch_info);
		#if 1
				sub_ch_info.ch_freq_khz = 208736;
				sub_ch_info.subch_id = 8;
				sub_ch_info.svc_type = RTV_SERVICE_DAB;
				open_sub_channel(&sub_ch_info);
		#endif
*/
				break;

			case 'f':	
				DMSG0("[Open FIC]\n");
				open_fic();
				break;


			case 'i':	
				DMSG0("[Close FIC]\n");
				close_fic();
				break;

			case 's':
				DMSG0("[DEBUG] Show the periodic SIGNAL information\n");
				show_periodic_sig_info();
				break;

			case 'h':
				DMSG0("[DEBUG] Hide the periodic SIGNAL information\n");
				hide_periodic_sig_info();
				break;

			case 'p':
				DMSG0("[DEBUG] Show the periodic TSP statistics\n");
				show_periodic_tsp_statistics();
				break;

			case 'c':
				DMSG0("[DEBUG] Hide the periodic TSP statistics\n");
				hide_periodic_tsp_statistics();
				break;

			case 'a':
				DMSG0("[TEST] Aging Test\n");
				AGING_TEST_tdmb();
				break;

			case 'q':
			case 'Q':
				goto TDMB_EXIT;

			default:
				DMSG1("[%c]\n", key);
		}
		
		DMSG0("\n");
	} 

TDMB_EXIT:

	power_down();

	ret = close(fd_tdmb_dev);
	DMSG2("[DMB] %s close() result : %d\n", TDMB_DEV_NAME, ret);

#if defined(RTV_IF_TSIF)
	#if defined(TSIF_SAMSUNG_AP)
	tsif_run(fd_tdmb_tsif_dev, 0);
	#endif


	close(fd_tdmb_tsif_dev);
#endif

	DMSG0("TDMB_EXIT\n");
	
	return;
}



