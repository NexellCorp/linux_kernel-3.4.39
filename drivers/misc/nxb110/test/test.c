#include "test.h"

static IOCTL_REG_ACCESS_INFO ioctl_reg_access_info;
static IOCTL_GPIO_ACCESS_INFO gpio_info;

unsigned int mtv_curr_channel[MAX_NUM_BB_DEMOD]; /* TDMB/DAB/FM: KHz, ISDBT: Channel number. */
unsigned int mtv_prev_channel[MAX_NUM_BB_DEMOD]; /* TDMB/DAB/FM: KHz, ISDBT: Channel number. */


static int dmb_read_thread_should_stop[MAX_NUM_BB_DEMOD];
static pthread_t tsp_read_thread_cb[MAX_NUM_BB_DEMOD];
static pthread_t *tsp_read_thread_cb_ptr[MAX_NUM_BB_DEMOD];


/* To get signal inforamtion.*/
static volatile int dm_timer_thread_run;
static pthread_t dm_timer_thread_cb;
static pthread_t *dm_timer_thread_cb_ptr;

static volatile unsigned int periodic_debug_info_mask[MAX_NUM_BB_DEMOD];


typedef struct
{
	BOOL	is_resumed;
	unsigned int sleep_msec;
	void (*callback)(void);
	struct mrevent event;
} MTV_DM_TIMER_INFO;
static MTV_DM_TIMER_INFO dm_timer;


struct timeval st, sp;                                                                                
double time_elapse(void)
{
                                                                                
        struct timeval tt;
        gettimeofday( &sp, NULL );
                                                                                
        tt.tv_sec = sp.tv_sec - st.tv_sec;
        tt.tv_usec = sp.tv_usec - st.tv_usec;
                                                                                
        gettimeofday(&st, NULL );
                                                                                
        if( tt.tv_usec < 0 ){
                                                                                
                tt.tv_usec += 1000000;
                tt.tv_sec--;
                                                                                
        }
	
        return (double)tt.tv_usec/1000000 + (double)tt.tv_sec;                                                                                
}


#define NUM_PID_GROUP	(8192/32/*bits*/)

typedef struct
{
	unsigned long	pid_cnt;	
	unsigned int	continuity_counter;
	unsigned long	discontinuity_cnt;
} PID_INFO;

typedef struct
{
	unsigned long	tsp_cnt;
		
	unsigned long	sync_byte_err_cnt;
	unsigned long	tei_cnt;
	unsigned long	null_cnt;
} TSP_INFO;

static PID_INFO pid_info[MAX_NUM_BB_DEMOD][8192];
static TSP_INFO tsp_info[MAX_NUM_BB_DEMOD];
static __u32 pid_grp_bits[MAX_NUM_BB_DEMOD][NUM_PID_GROUP]; /* 32 Bits for PID pre group*/

void show_video_tsp_statistics(int demod_no)
{
	__u32 pid_bits;
	unsigned int  grp_idx, pid;
	float sync_err_rate = 0.0, total_err_rate = 0.0;
	unsigned long num_err_pkt;

	DMSG1("\t################# [Chip(%d) Video TSP Statistics] ###############\n", demod_no);
	for(grp_idx=0; grp_idx<NUM_PID_GROUP; grp_idx++)
	{
		pid = grp_idx * 32;
		pid_bits = pid_grp_bits[demod_no][grp_idx];
		while( pid_bits )
		{
			/* Check if the pid was countered. */
			if( pid_bits & 0x1 )
			{
				DMSG3("\t# PID: 0x%04X, Pkts: %ld, Discontinuity: %ld\n",
					pid, pid_info[demod_no][pid].pid_cnt,
					pid_info[demod_no][pid].discontinuity_cnt);
			
			}
			pid_bits >>= 1;
			pid++;
		}
	}

	if(tsp_info[demod_no].sync_byte_err_cnt > tsp_info[demod_no].tei_cnt)
		num_err_pkt = tsp_info[demod_no].sync_byte_err_cnt;
	else
		num_err_pkt = tsp_info[demod_no].tei_cnt;

	if(tsp_info[demod_no].tsp_cnt != 0)
	{
		sync_err_rate = ((float)tsp_info[demod_no].sync_byte_err_cnt / (float)tsp_info[demod_no].tsp_cnt) * 100;
		total_err_rate = ((float)num_err_pkt / (float)tsp_info[demod_no].tsp_cnt) * 100;
	}
	
	DMSG1("\t#\t\t Total TSP: %ld\n", tsp_info[demod_no].tsp_cnt);
	DMSG3("\t# [Error Kind] Sync Byte: %ld, TEI: %ld, Null: %ld \n",
		tsp_info[demod_no].sync_byte_err_cnt,
		tsp_info[demod_no].tei_cnt, tsp_info[demod_no].null_cnt);	
	DMSG2("\t# [Error Rate] Total: %.3f %%, Sync: %.3f %%\n",
		total_err_rate, sync_err_rate);	
	DMSG0("\t######################################################\n");
}


static inline void verify_video_pid(int demod_no, unsigned char *buf, unsigned int pid)
{
	unsigned int prev_conti;	
	unsigned int cur_conti = buf[3] & 0x0F;
	unsigned int grp_idx = pid >> 5;
	unsigned int pid_idx = pid & 31;
		
	if((pid_grp_bits[demod_no][grp_idx] & (1<<pid_idx)) != 0x0)
	{
		prev_conti = pid_info[demod_no][pid].continuity_counter;
		
		if(((prev_conti + 1) & 0x0F) != cur_conti)
			pid_info[demod_no][pid].discontinuity_cnt++;
	}
	else
	{
		pid_grp_bits[demod_no][grp_idx] |= (1<<pid_idx);		
		pid_info[demod_no][pid].pid_cnt = 0;
	}

	pid_info[demod_no][pid].continuity_counter = cur_conti;
	pid_info[demod_no][pid].pid_cnt++;
}

void verify_video_tsp(int demod_no, unsigned char *buf, unsigned int size) 
{
	unsigned int pid;
	TSP_INFO *tsp_info_ptr = &tsp_info[demod_no];
	do
	{
		tsp_info_ptr->tsp_cnt++;

		pid = ((buf[1] & 0x1F) << 8) | buf[2];

		if(buf[0] == 0x47)
		{
			if((buf[1] & 0x80) != 0x80)
			{				
				if(pid != 0x1FFF)
					verify_video_pid(demod_no, buf, pid);

				else
					tsp_info_ptr->null_cnt++;
			}
			else
				tsp_info_ptr->tei_cnt++;
		}
		else
			tsp_info_ptr->sync_byte_err_cnt++;
	
		buf += 188;
		size -= 188;
	}while(size != 0);
}

void init_tsp_statistics(int demod_no) 
{
	memset(&tsp_info[demod_no], 0, sizeof(TSP_INFO));
	memset(pid_grp_bits[demod_no], 0, sizeof(__u32) * NUM_PID_GROUP);
}

#if defined(RTV_TDMB_ENABLE) || defined(RTV_DAB_ENABLE)
void show_fic_information(RTV_FIC_ENSEMBLE_INFO *ensble, U32 dwFreqKHz)
{
	unsigned int i;

	DMSG0("\t########################################################\n");
	DMSG1("\t#\t\tFreq: %u Khz\n", dwFreqKHz);
	
	for(i=0; i<ensble->bTotalSubCh; i++)
	{
		DMSG1("\t# [%-16s] ", ensble->tSubChInfo[i].szSvcLabel);
		DMSG1("SubCh ID: %-2d, ", ensble->tSubChInfo[i].bSubChID);
		DMSG1("Bit Rate: %3d, ", ensble->tSubChInfo[i].wBitRate);
		DMSG1("TMId: %d, ", ensble->tSubChInfo[i].bTMId);
		DMSG1("Svc Type: 0x%02X\n", ensble->tSubChInfo[i].bSvcType);
	}

	DMSG1("\t#\tTotal: %d\n", ensble->bTotalSubCh);
	DMSG0("\t########################################################\n");
}


/* TDMB/DAB both used */
E_RTV_SERVICE_TYPE get_dab_service_type_from_user(void)
{
	E_RTV_SERVICE_TYPE svc_type;
	
	while(1)
	{
		DMSG0("Input service type(1: Video, 2:Audio, 4: Data):");
		scanf("%u", &svc_type); 		
		CLEAR_STDIN;

		if((svc_type ==RTV_SERVICE_VIDEO) 
		|| (svc_type ==RTV_SERVICE_AUDIO) 
		|| (svc_type ==RTV_SERVICE_DATA))
			break;
	}

	return svc_type;
}


/* TDMB/DAB both used */
unsigned int get_subch_id_from_user(void)
{
	unsigned int subch_id;
	
	while(1)
	{
		DMSG0("Input sub channel ID(ex. 0):");
		scanf("%u", &subch_id);			
		CLEAR_STDIN;
		if(subch_id < 64)
			break;
	}

	return subch_id;
}
#endif

#if defined(TSIF_SAMSUNG_AP)
/* TSIF command should be changed depends on TSIF driver. */
void tsif_run(int fd_tsif_dev, int run)
{
	if(run == 1)
	{
		/* Start TSI */
		if(ioctl(fd_tsif_dev, 0xAABB) != 0)
		{
			EMSG0("TSI start error");					
		}	
	}
	else
	{	
		/* Stop TSI */
		if(ioctl(fd_tsif_dev, 0xAACC) != 0) 
		{
			EMSG0("TSI stop error");
		}
	}
}
#endif

#if defined(TSIF_QUALCOMM_AP)
int tsif_get_msm_hts_pkt(unsigned char *pkt_buf, const unsigned char *ts_buf,
							int ts_size)
{
	unsigned int i, num_pkts = ts_size / TSIF_TSP_SIZE;
	
	for(i = 0; i < num_pkts; i++)
	{
		memcpy(pkt_buf, ts_buf, MSM_TSIF_HTS_PKT_SIZE);
		pkt_buf += MSM_TSIF_HTS_PKT_SIZE;
		ts_buf += TSIF_TSP_SIZE;
	}

	return num_pkts * MSM_TSIF_HTS_PKT_SIZE;
}
#endif

void resume_dm_timer(void)
{
	if(dm_timer.is_resumed == TRUE)
		return;

	dm_timer.is_resumed = TRUE;
	
	dm_timer.sleep_msec = MTV_DM_TIMER_EXPIRE_MS;

	/* Send the event to timer thread. */
	mrevent_trigger(&dm_timer.event);
}

void suspend_dm_timer(void)
{
	dm_timer.sleep_msec = 0;
	dm_timer.is_resumed = FALSE;
}

/* Define DM timer. */
int def_dm_timer(void (*callback)(void))
{
	if(callback == NULL)
	{
		EMSG0("[MTV] Invalid exipre handler\n");
		return -2;
	}
	
	dm_timer.callback = callback;

	return 0;
}

static void init_dm_timer(void)
{
	mrevent_init(&dm_timer.event);

	dm_timer.is_resumed = FALSE;
	dm_timer.sleep_msec = 0;
	dm_timer.callback = NULL;
}

void *test_dm_timer_thread(void *arg)
{
	unsigned int sleep_msec;

	DMSG0("[test_dm_timer_thread] Entered\n");

	init_dm_timer();

	for(;;)
	{
		/* Wait for the event. */
		mrevent_wait(&dm_timer.event, WAIT_INFINITE);

		/* Clear the event. */
		mrevent_reset(&dm_timer.event);
		
		if(dm_timer_thread_run == 0)
			break;

		/* To prevent crash , we use the copied address. */
		sleep_msec = dm_timer.sleep_msec;
		if(sleep_msec != 0)
		{
			if(dm_timer.callback != NULL)
				(*(dm_timer.callback))();
			
			usleep(sleep_msec*1000);
			//sleep(1);
			
			mrevent_trigger(&dm_timer.event);
		}
	}
	
	DMSG0("[test_dm_timer_thread] Exit...\n");			
	
	pthread_exit((void *)NULL);
}


static int test_create_timer_thread(void)
{
	int ret = 0;

	if(dm_timer_thread_cb_ptr == NULL)
	{
		dm_timer_thread_run = 1;

		ret = pthread_create(&dm_timer_thread_cb, NULL, test_dm_timer_thread, NULL);
		if(ret < 0)
		{
			EMSG1("DM Timer thread create error: %d\n", ret);
			return ret;
		}

		dm_timer_thread_cb_ptr = &dm_timer_thread_cb;
	}

	return ret;
}

static void test_delete_timer_thread(void)
{
	if(dm_timer_thread_cb_ptr != NULL)
	{
		dm_timer_thread_run = 0;
		mrevent_trigger(&dm_timer.event);

		pthread_join(dm_timer_thread_cb, NULL);
		dm_timer_thread_cb_ptr = NULL;
	}
}

int mtv_read_thread_should_stop(int demod_no)
{
	return dmb_read_thread_should_stop[demod_no];
}

int create_mtv_read_thread(int demod_no, void *(*start_routine)(void *))
{
	int ret = 0;

	if(tsp_read_thread_cb_ptr[demod_no] == NULL)
	{
		dmb_read_thread_should_stop[demod_no] = 0;

		ret = pthread_create(&tsp_read_thread_cb[demod_no], NULL, start_routine, NULL);
		if(ret < 0)
		{
			EMSG1("read thread create error: %d\n", ret);
			return ret;
		}

		tsp_read_thread_cb_ptr[demod_no] = &tsp_read_thread_cb[demod_no];
	}

	return ret;
}

void delete_mtv_read_thread(int demod_no)
{
	if(tsp_read_thread_cb_ptr[demod_no] != NULL)
	{
		dmb_read_thread_should_stop[demod_no] = 1;

		pthread_join(tsp_read_thread_cb[demod_no], NULL);
		tsp_read_thread_cb_ptr[demod_no] = NULL;
	}
}


unsigned int get_periodic_debug_info_mask(int demod_no)
{
	return periodic_debug_info_mask[demod_no];
}

void hide_periodic_tsp_statistics(int demod_no)
{
	periodic_debug_info_mask[demod_no] &= ~TSP_STAT_INFO_MASK;
}

void show_periodic_tsp_statistics(int demod_no)
{
	periodic_debug_info_mask[demod_no] |= TSP_STAT_INFO_MASK;
}

void hide_periodic_sig_info(int demod_no)
{
	periodic_debug_info_mask[demod_no] &= ~SIG_INFO_MASK;
}

void show_periodic_sig_info(int demod_no)
{
	periodic_debug_info_mask[demod_no] |= SIG_INFO_MASK;
}

void init_periodic_debug_info(int demod_no)
{
	periodic_debug_info_mask[demod_no] = 0x0;	
}



void test_GPIO(int demod_no, int fd_dmb_dev)
{
	int key;

	while(1)
	{
		DMSG0("================ GPIO Test ===============\n");
		DMSG0("\t1: GPIO Write(Set) Test\n");
		DMSG0("\t2: GPIO Read(Get) Test\n");
		DMSG0("\tq or Q: Quit\n");
		DMSG0("========================================\n");
		
		key = getc(stdin);				
		CLEAR_STDIN;
		
		switch( key )
		{
		case '1':
			DMSG0("[GPIO Write(Set) Test]\n");	

			DMSG0("Select Pin Number:");
			scanf("%u" , &gpio_info.pin);	
			CLEAR_STDIN;

			while(1)
			{
				DMSG0("Input Pin Level(0 or 1):");
				scanf("%u" , &gpio_info.value); 
				CLEAR_STDIN;
				if((gpio_info.value==0) || (gpio_info.value==1))
					break;				
			}

			gpio_info.demod_no = demod_no;
			if(ioctl(fd_dmb_dev, IOCTL_TEST_GPIO_SET, &gpio_info) < 0)
			{						
				EMSG0("IOCTL_TEST_GPIO_SET failed\n");
			}						
			break;

		case '2':
			DMSG0("[GPIO Read(Set) Test]\n"); 

			DMSG0("Select Pin Number:");
			scanf("%u" , &gpio_info.pin);	
			CLEAR_STDIN;

			gpio_info.demod_no = demod_no;
			if(ioctl(fd_dmb_dev, IOCTL_TEST_GPIO_GET, &gpio_info) < 0)
			{						
				EMSG0("IOCTL_TEST_GPIO_GET failed\n");
			}		

			DMSG2("Pin(%u): %u\n", gpio_info.pin, gpio_info.value);
			DMSG0("\n");		
			break;
		
		case 'q':
		case 'Q':
			goto GPIO_TEST_EXIT;
		
		default:
			DMSG1("[%c]\n", key);
		}
	}

GPIO_TEST_EXIT:
	return;
}

static void show_register_page(void)
{
	DMSG0("===============================================\n");
	DMSG0("\t0: HOST_PAGE\n");
	DMSG0("\t1: RF_PAGE\n");				
	DMSG0("\t2: COMM_PAGE\n");
	DMSG0("\t3: DD_PAGE\n");
	DMSG0("\t4: MSC0_PAGE\n");
	DMSG0("\t5: MSC1_PAGE\n");				
	DMSG0("\t6: OFDM PAGE\n");
	DMSG0("\t7: FEC_PAGE\n");
	DMSG0("\t8: FIC_PAGE\n");
	DMSG0("===============================================\n");
}

void test_RegisterIO(int demod_no, int fd_dmb_dev)
{
	IOCTL_TEST_MTV_POWER_ON_INFO param;
	int key;
	unsigned int i, page, addr, read_cnt, write_data;
	const char *page_str[] = 
		{"HOST", "RF", "COMM", "DD", "MSC0", "MSC1", "OFDM", "FEC", "FIC"};

	/* Power-up chip*/
	param.demod_no = 0;
	if(ioctl(fd_dmb_dev, IOCTL_TEST_MTV_POWER_ON, &param) < 0) {	
		EMSG0("IOCTL_TEST_MTV_POWER_ON failed\n");
		return;
	}

	while(1)
	{
		DMSG0("================ Register IO Test ===============\n");
		DMSG0("\t1: Single Read\n");
		DMSG0("\t2: Burst Read\n");
		DMSG0("\t3: Write\n");		
		DMSG0("\tq or Q: Quit\n");
		DMSG0("================================================\n");
		
		//fflush(stdin);
		key = getc(stdin);				
		CLEAR_STDIN;
		
		switch( key )
		{
		case '1':
			DMSG0("[Reg IO] Single Read\n");
			show_register_page();			
			while(1)
			{
				DMSG0("Select Page:");
				scanf("%x" , &page);
				CLEAR_STDIN;
				if(page <= 8)
					break;
			}

			DMSG0("Input Address(hex) : ");
			scanf("%x", &addr);
			CLEAR_STDIN;

			ioctl_reg_access_info.demod_no = demod_no;
			ioctl_reg_access_info.page = page;
			ioctl_reg_access_info.addr = addr;

			DMSG2("\t[INPUT] %s[0x%02X]\n", page_str[page], addr);

			if(ioctl(fd_dmb_dev, IOCTL_TEST_REG_SINGLE_READ, &ioctl_reg_access_info) == 0)
				DMSG3("%s[0x%02X]: 0x%02X\n", 
					page_str[page], 
					addr, ioctl_reg_access_info.read_data[0]);
			else
				EMSG0("IOCTL_TEST_REG_SINGLE_READ failed\n");

			break;	

		case '2':
			DMSG0("[Reg IO] Burst Read\n");
			show_register_page();
			while(1)
			{
				DMSG0("Select Page:");
				scanf("%x" , &page);
				CLEAR_STDIN;
				if(page <= 8)
					break;
			}
			
			DMSG0("Input Address(hex): ");
			scanf("%x", &addr);
			CLEAR_STDIN;			
			
			while(1)
			{
				DMSG1("Input the number of reading (up to %d:",
						MAX_NUM_MTV_REG_READ_BUF);
				scanf("%d" , &read_cnt);	
				CLEAR_STDIN;
				if(read_cnt <= MAX_NUM_MTV_REG_READ_BUF)
					break;
			}

			ioctl_reg_access_info.demod_no = demod_no;
			ioctl_reg_access_info.page = page;
			ioctl_reg_access_info.addr = addr;
			ioctl_reg_access_info.read_cnt = read_cnt;

			DMSG3("\t[INPUT] %s[0x%02X]: %u\n", page_str[page], addr, read_cnt);
			if(ioctl(fd_dmb_dev, IOCTL_TEST_REG_BURST_READ, &ioctl_reg_access_info) == 0)
			{
				for(i = 0; i < read_cnt; i++, addr++)
					DMSG3("%s[0x%02X]: 0x%02X\n", 
						page_str[page], addr,
						ioctl_reg_access_info.read_data[i]);
			}
			else
				EMSG0("IOCTL_TEST_REG_BURST_READ failed\n");

			break;	
				
		case '3':
			DMSG0("[Reg IO] Write\n");
			show_register_page();
			while(1)
			{
				DMSG0("Select Page:");
				scanf("%x" , &page);
				CLEAR_STDIN;
				if(page <= 8)
					break;
			}

			DMSG0("Input Address(hex) :,  data(hex) : ");
			scanf("%x" , &addr);	
			CLEAR_STDIN;
			
			scanf("%x" , &write_data);
			CLEAR_STDIN;
			
			ioctl_reg_access_info.demod_no = demod_no;
			ioctl_reg_access_info.page = page;
			ioctl_reg_access_info.addr = addr;
			ioctl_reg_access_info.write_data = write_data;

			DMSG3("%s[0x%02X]: 0x%02X\n", page_str[page], addr, write_data);
			if(ioctl(fd_dmb_dev, IOCTL_TEST_REG_WRITE, &ioctl_reg_access_info) < 0)
				EMSG0("IOCTL_TEST_REG_WRITE failed\n");

			break;

		case 'q':
		case 'Q':
			goto REG_IO_TEST_EXIT;
		
		default:
			DMSG1("[%c]\n", key);
		}
		DMSG0("\n");
	}

REG_IO_TEST_EXIT:
	param.demod_no = 0;
	if(ioctl(fd_dmb_dev, IOCTL_TEST_MTV_POWER_OFF,	&param) < 0)
		EMSG0("IOCTL_TEST_MTV_POWER_OFF failed\n");

	return;
}

#if defined(RTV_IF_TSIF) || defined(RTV_IF_SPI_SLAVE)
int open_tsif_device(void)
{
	int fd;
	char name[32];
	
	sprintf(name,"/dev/%s", TSIF_DEV_NAME);
	fd = open(name, O_RDWR);
	if(fd < 0)
		EMSG1("Can't not open %s\n", name);

	return fd;
}
#endif /* #if defined(RTV_IF_TSIF) || defined(RTV_IF_SPI_SLAVE) */

int tdmb_drv_open_cnt;

int  close_mtv_device(int fd)
{
	tdmb_drv_open_cnt--;

	DMSG0("[close_mtv_device] Closed......\n");

	return close(fd);
}

int open_mtv_device(int demod_no)
{
	int fd;
	char name[32];
	const char *dev_name[] = {"nxb110", "nxb110_tpeg"};

	DMSG1("[open_mtv_device] Entered...... (%d)\n", ++tdmb_drv_open_cnt);
	
	sprintf(name,"/dev/%s", dev_name[demod_no]);

#ifdef MTV_BLOCKING_READ_MODE
	fd = open(name, O_RDWR); /* Blocking mode */
#else
	fd = open(name, O_RDWR|O_NONBLOCK); /* Non-blocking mode */
#endif	
	if(fd < 0)
		EMSG1("Can't not open %s\n", name);

	DMSG0("[open_mtv_device] Exit......\n");

	return fd;
}

int main(void)
{
	int key, ret;
	int fd_dmb_dev;

	tdmb_drv_open_cnt = 0;

	dmb_read_thread_should_stop[0] = 1;
	tsp_read_thread_cb_ptr[0] = NULL;

#if (MAX_NUM_BB_DEMOD == 2)
	dmb_read_thread_should_stop[1] = 1;
	tsp_read_thread_cb_ptr[1] = NULL;
#endif

	dm_timer_thread_run = 0;
	dm_timer_thread_cb_ptr = NULL;

	mkdir(TS_DUMP_DIR, 0777);

	/* Create the read thread. */
	if((ret = test_create_timer_thread()) != 0)
	{
		EMSG1("test_create_timer_thread() failed: %d\n", ret);
		goto APP_MAIN_EXIT;
	}

  	while(1)
	{
		DMSG0("===============================================\n");
	#if defined(RTV_TDMB_ENABLE)
		DMSG0("\t1: TDMB Test\n");
	#elif defined(RTV_DAB_ENABLE)
		DMSG0("\t1: DAB/DAB+ Test\n");
	#endif

	#ifdef RTV_ISDBT_ENABLE
		DMSG0("\t2: ISDBT Test\n");
	#endif
		
	#ifdef RTV_FM_ENABLE
		DMSG0("\t3: FM Test\n");
	#endif
//		DMSG0("\t4: GPIO Test\n");
//		DMSG0("\t5: Register IO Test\n");
		DMSG0("\tq or Q: Quit\n");
		DMSG0("===============================================\n");
   		
		fflush(stdin);
		key = getc(stdin);				
		CLEAR_STDIN;
		
		switch( key )
		{
	#if defined(RTV_TDMB_ENABLE) || defined(RTV_DAB_ENABLE)
			case '1': test_TDMB_DAB(); break;
	#endif

	#ifdef RTV_ISDBT_ENABLE
			case '2': test_ISDBT(); break;
	#endif

	#ifdef RTV_FM_ENABLE
			case '3': test_FM(); break;
	#endif
			//case '4': test_GPIO(); break;
				
			case 'q':
			case 'Q': goto APP_MAIN_EXIT;
			default: DMSG1("[%c]\n", key);
		}
	}

APP_MAIN_EXIT:

	test_delete_timer_thread();

	return 0;
}
