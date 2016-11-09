#include "test.h"

static IOCTL_REG_ACCESS_INFO ioctl_reg_access_info;
static IOCTL_GPIO_ACCESS_INFO gpio_info;

unsigned int mtv_curr_channel; /* TDMB/DAB/FM: KHz, ISDBT: Channel number. */
unsigned int mtv_prev_channel; /* TDMB/DAB/FM: KHz, ISDBT: Channel number. */


static volatile int dmb_read_thread_should_stop;
static pthread_t tsp_read_thread_cb;
static pthread_t *tsp_read_thread_cb_ptr;


/* To get signal inforamtion.*/
static volatile int dm_timer_thread_run;
static pthread_t dm_timer_thread_cb;
static pthread_t *dm_timer_thread_cb_ptr;

static volatile unsigned int periodic_debug_info_mask;


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

static PID_INFO pid_info[8192];
static TSP_INFO tsp_info;
static __u32 pid_grp_bits[NUM_PID_GROUP]; /* 32 Bits for PID pre group*/

void show_video_tsp_statistics(void)
{
	__u32 pid_bits;
	unsigned int  grp_idx, pid;
	float sync_err_rate = 0.0, total_err_rate = 0.0;
	unsigned long num_err_pkt;

	DMSG0("\t################# [Video TSP Statistics] ###############\n");
	for(grp_idx=0; grp_idx<NUM_PID_GROUP; grp_idx++)
	{
		pid = grp_idx * 32;
		pid_bits = pid_grp_bits[grp_idx];
		while( pid_bits )
		{
			/* Check if the pid was countered. */
			if( pid_bits & 0x1 )
			{
				DMSG3("\t# PID: 0x%04X, Pkts: %ld, Discontinuity: %ld\n",
					pid, pid_info[pid].pid_cnt,
					pid_info[pid].discontinuity_cnt);
			
			}
			pid_bits >>= 1;
			pid++;
		}
	}

	if(tsp_info.sync_byte_err_cnt > tsp_info.tei_cnt)
		num_err_pkt = tsp_info.sync_byte_err_cnt;
	else
		num_err_pkt = tsp_info.tei_cnt;

	if(tsp_info.tsp_cnt != 0)
	{
		sync_err_rate = ((float)tsp_info.sync_byte_err_cnt / (float)tsp_info.tsp_cnt) * 100;
		total_err_rate = ((float)num_err_pkt / (float)tsp_info.tsp_cnt) * 100;
	}
	
	DMSG1("\t#\t\t Total TSP: %ld\n", tsp_info.tsp_cnt);
	DMSG3("\t# [Error Kind] Sync Byte: %ld, TEI: %ld, Null: %ld \n",
		tsp_info.sync_byte_err_cnt,
		tsp_info.tei_cnt, tsp_info.null_cnt);	
	DMSG2("\t# [Error Rate] Total: %.3f %%, Sync: %.3f %%\n",
		total_err_rate, sync_err_rate);	
	DMSG0("\t######################################################\n");
}


static inline void verify_video_pid(unsigned char *buf, unsigned int pid)
{
	unsigned int prev_conti;	
	unsigned int cur_conti = buf[3] & 0x0F;
	unsigned int grp_idx = pid >> 5;
	unsigned int pid_idx = pid & 31;
		
	if((pid_grp_bits[grp_idx] & (1<<pid_idx)) != 0x0)
	{
		prev_conti = pid_info[pid].continuity_counter;
		
		if(((prev_conti + 1) & 0x0F) != cur_conti)
			pid_info[pid].discontinuity_cnt++;
	}
	else
	{
		pid_grp_bits[grp_idx] |= (1<<pid_idx);		
		pid_info[pid].pid_cnt = 0;
	}

	pid_info[pid].continuity_counter = cur_conti;
	pid_info[pid].pid_cnt++;
}

void verify_video_tsp(unsigned char *buf, unsigned int size) 
{
	unsigned int pid;

	do
	{
		tsp_info.tsp_cnt++;

		pid = ((buf[1] & 0x1F) << 8) | buf[2];

		if(buf[0] == 0x47)
		{
			if((buf[1] & 0x80) != 0x80)
			{				
				if(pid != 0x1FFF)
					verify_video_pid(buf, pid);

				else
					tsp_info.null_cnt++;
			}
			else
				tsp_info.tei_cnt++;
		}
		else
			tsp_info.sync_byte_err_cnt++;
	
		buf += 188;
		size -= 188;
	}while(size != 0);
}

void init_tsp_statistics(void) 
{
	memset(&tsp_info, 0, sizeof(TSP_INFO));
	memset(pid_grp_bits, 0, sizeof(__u32) * NUM_PID_GROUP);
}

void show_fic_information(struct ensemble_info_type *ensble, U32 dwFreqKHz)
{
	unsigned int i;

	DMSG0("########################################################\n");
	DMSG1("#\t\tFreq: %u Khz\n", dwFreqKHz);
	
	for (i=0; i<ensble->tot_sub_ch; i++) {
		DMSG1("# [%-16s] ", ensble->sub_ch[i].svc_label);
		DMSG1("SubCh ID: %-2d, ", ensble->sub_ch[i].sub_ch_id);
		DMSG1("Bit Rate: %3d, ", ensble->sub_ch[i].bit_rate);
		DMSG1("TMId: %d, ", ensble->sub_ch[i].tmid);
		DMSG1("Svc Type: 0x%02X, ", ensble->sub_ch[i].svc_type);

	#if 1 // SAMSUNG
		DMSG1("ECC: 0x%02X, ", ensble->sub_ch[i].ecc);
		DMSG1("SCIDS: 0x%02X\n", ensble->sub_ch[i].scids);
	#endif
	}

	DMSG1("#\tTotal: %d\n", ensble->tot_sub_ch);
	DMSG0("########################################################\n");
}


/* TDMB/DAB both used */
enum E_RTV_SERVICE_TYPE get_dab_service_type_from_user(void)
{
	enum E_RTV_SERVICE_TYPE svc_type;
	
	while (1) {
		DMSG0("Input service type(0: DMB, 1:DAB, 2: DAB+ ):");
		scanf("%u", &svc_type); 		
		CLEAR_STDIN;

		if((svc_type == RTV_SERVICE_DMB) 
		|| (svc_type == RTV_SERVICE_DAB)
		|| (svc_type == RTV_SERVICE_DABPLUS))
			break;
	}

	return svc_type;
}


/* TDMB/DAB both used */
unsigned int get_subch_id_from_user(void)
{
	unsigned int subch_id;
	
	while (1) {
		DMSG0("Input sub channel ID(ex. 0):");
		scanf("%u", &subch_id);			
		CLEAR_STDIN;
		if(subch_id < 64)
			break;
	}

	return subch_id;
}


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

	if (dm_timer_thread_cb_ptr == NULL)
	{
		dm_timer_thread_run = 1;

		ret = pthread_create(&dm_timer_thread_cb, NULL, test_dm_timer_thread, NULL);
		if (ret < 0)
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
	if (dm_timer_thread_cb_ptr != NULL)
	{
		dm_timer_thread_run = 0;
		mrevent_trigger(&dm_timer.event);

		pthread_join(dm_timer_thread_cb, NULL);
		dm_timer_thread_cb_ptr = NULL;
	}
}

int mtv_read_thread_should_stop(void)
{
	return dmb_read_thread_should_stop;
}

int create_mtv_read_thread(void *(*start_routine)(void *))
{
	int ret = 0;

	if(tsp_read_thread_cb_ptr == NULL)
	{
		dmb_read_thread_should_stop = 0;

		ret = pthread_create(&tsp_read_thread_cb, NULL, start_routine, NULL);
		if(ret < 0)
		{
			EMSG1("read thread create error: %d\n", ret);
			return ret;
		}

		tsp_read_thread_cb_ptr = &tsp_read_thread_cb;
	}

	return ret;
}

void delete_mtv_read_thread(void)
{
	if(tsp_read_thread_cb_ptr != NULL)
	{
		dmb_read_thread_should_stop = 1;

		pthread_join(tsp_read_thread_cb, NULL);
		tsp_read_thread_cb_ptr = NULL;
	}
}


unsigned int get_periodic_debug_info_mask(void)
{
	return periodic_debug_info_mask;
}

void hide_periodic_tsp_statistics(void)
{
	periodic_debug_info_mask &= ~TSP_STAT_INFO_MASK;
}

void show_periodic_tsp_statistics(void)
{
	periodic_debug_info_mask |= TSP_STAT_INFO_MASK;
}

void hide_periodic_sig_info(void)
{
	periodic_debug_info_mask &= ~SIG_INFO_MASK;
}

void show_periodic_sig_info(void)
{
	periodic_debug_info_mask |= SIG_INFO_MASK;
}

void init_periodic_debug_info(void)
{
	periodic_debug_info_mask = 0x0;	
}



void test_GPIO(void)
{
	int key;
	int fd_dmb_dev;

	if((fd_dmb_dev=open_tdmb_device()) < 0)
		return;
		
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

	close(fd_dmb_dev);
}

static void show_register_page(void)
{
	DMSG0("===============================================\n");
	DMSG0("\t0: TOP_PAGE(HOST_PAGE)\n");				
	DMSG0("\t1: OFDM_PAGE\n");
	DMSG0("\t2: FEC_PAGE\n");
	DMSG0("\t3: SPI_CTRL_PAGE\n");
	DMSG0("\t4: RF_PAGE\n");				
	DMSG0("\t5: SPI_MEM PAGE\n");
	DMSG0("===============================================\n");
}


void test_RegisterIO(int fd_dmb_dev)
{
	int key;
	unsigned int i, page_idx, addr, read_cnt, write_data, err_cnt;
	const char *page_str[] = 
		{"TOP(HOST)", "OFDM", "FEC", "SPI_CTRL", "RF", "SPI_MEM"};
	unsigned int page_value[] = {0x00, 0x08, 0x09, 0x0E, 0x0F, 0xFF};

	while(1)
	{
		DMSG0("================ Register IO Test ===============\n");
		DMSG0("\t0: Single Read\n");
		DMSG0("\t1: Burst Read\n");
		DMSG0("\t2: Write\n");
		DMSG0("\t3: AGING Single Read/Write\n");
		DMSG0("\t4: Multiple Single Read\n");
		DMSG0("\t5: AGING Single/Burst toggle Read/Write\n");
		DMSG0("\t6: AGING SPI Memory Test with Single IO\n");
		DMSG0("\t7: AGING SPI Memory Test Only\n");
		DMSG0("\tq or Q: Quit\n");
		DMSG0("================================================\n");
		
		//fflush(stdin);
		key = getc(stdin);				
		CLEAR_STDIN;
		
		switch( key )
		{
		case '0':
			DMSG0("[Reg IO] Single Read\n");
			show_register_page();			
			while(1)
			{
				DMSG0("Select Page:");
				scanf("%x" , &page_idx);
				CLEAR_STDIN;
				if(page_idx <= 5)
					break;
			}

			DMSG0("Input Address(hex) : ");
			scanf("%x", &addr);
			CLEAR_STDIN;

			ioctl_reg_access_info.page = page_value[page_idx];
			ioctl_reg_access_info.addr = addr;

			DMSG2("\t[INPUT] %s[0x%02X]\n", page_str[page_idx], addr);

			if(ioctl(fd_dmb_dev, IOCTL_TEST_REG_SINGLE_READ, &ioctl_reg_access_info) == 0)
				DMSG3("%s[0x%02X]: 0x%02X\n", 
					page_str[page_idx], 
					addr, ioctl_reg_access_info.read_data[0]);
			else
				EMSG0("IOCTL_TEST_REG_SINGLE_READ failed\n");

			break;	

		case '1':
			DMSG0("[Reg IO] Burst Read\n");
			show_register_page();
			while(1)
			{
				DMSG0("Select Page:");
				scanf("%x" , &page_idx);
				CLEAR_STDIN;
				if(page_idx <= 5)
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

			ioctl_reg_access_info.page = page_value[page_idx];
			ioctl_reg_access_info.addr = addr;
			ioctl_reg_access_info.read_cnt = read_cnt;

			DMSG3("\t[INPUT] %s[0x%02X]: %u\n", page_str[page_idx], addr, read_cnt);
			if(ioctl(fd_dmb_dev, IOCTL_TEST_REG_BURST_READ, &ioctl_reg_access_info) == 0)
			{
				for(i = 0; i < read_cnt; i++, addr++)
					DMSG3("%s[0x%02X]: 0x%02X\n", 
						page_str[page_idx], addr,
						ioctl_reg_access_info.read_data[i]);
			}
			else
				EMSG0("IOCTL_TEST_REG_BURST_READ failed\n");

			break;	
				
		case '2':
			DMSG0("[Reg IO] Write\n");
			show_register_page();
			while(1)
			{
				DMSG0("Select Page:");
				scanf("%x" , &page_idx);
				CLEAR_STDIN;
				if(page_idx <= 5)
					break;
			}

			DMSG0("Input Address(hex) :,  data(hex) : ");
			scanf("%x" , &addr);	
			CLEAR_STDIN;
			
			scanf("%x" , &write_data);
			CLEAR_STDIN;
			
		
			ioctl_reg_access_info.page = page_value[page_idx];
			ioctl_reg_access_info.addr = addr;
			ioctl_reg_access_info.write_data = write_data;

			DMSG3("%s[0x%02X]: 0x%02X\n", page_str[page_idx], addr, write_data);
			if(ioctl(fd_dmb_dev, IOCTL_TEST_REG_WRITE, &ioctl_reg_access_info) < 0)
				EMSG0("IOCTL_TEST_REG_WRITE failed\n");

			break;

		case '3':
			DMSG0("[Reg IO] AGING Single Read/Write\n");
			show_register_page();
			while(1)
			{
				DMSG0("Select Page:");
				scanf("%x" , &page_idx);
				CLEAR_STDIN;
				if(page_idx <= 5)
					break;
			}

			DMSG0("WRITE. Input Address(hex) :,  data(hex) : ");
			scanf("%x" , &addr);	
			CLEAR_STDIN;
			
			scanf("%x" , &write_data);
			CLEAR_STDIN;

	#if 1
			//read_cnt = 1;
	#else
			while(1)
			{
				DMSG1("Input the number of aging test (up to %d:",
						MAX_NUM_MTV_REG_READ_BUF);
				scanf("%d" , &read_cnt);	
				CLEAR_STDIN;
				if(read_cnt <= MAX_NUM_MTV_REG_READ_BUF)
					break;
			}
	#endif

			DMSG3("%s[0x%02X]: 0x%02X\n", page_str[page_idx], addr, write_data);

			err_cnt = 0;
			//read_cnt = 1000000;
			//read_cnt = 100000;
			//read_cnt = 100;
			//read_cnt = 100000000;
			//for(i = 0; i < read_cnt; i++) {
			for(i = 0; ; i++) {
				ioctl_reg_access_info.page = page_value[page_idx];
				ioctl_reg_access_info.addr = addr;
				if((i%2) == 0)
					write_data = 0x55;
				else
					write_data = 0xAA;
				ioctl_reg_access_info.write_data = write_data;
				
				if(ioctl(fd_dmb_dev, IOCTL_TEST_REG_WRITE, &ioctl_reg_access_info) < 0)
					EMSG0("IOCTL_TEST_REG_WRITE failed\n");

				ioctl_reg_access_info.page = page_value[page_idx];
				ioctl_reg_access_info.addr = addr;
					if(ioctl(fd_dmb_dev, IOCTL_TEST_REG_SINGLE_READ, &ioctl_reg_access_info) == 0)
					DMSG3("%s[0x%02X]: 0x%02X\n", 
						page_str[page_idx], 
						addr, ioctl_reg_access_info.read_data[0]);
				else
					EMSG0("IOCTL_TEST_REG_SINGLE_READ failed\n");

				if (write_data != ioctl_reg_access_info.read_data[0]) {
					err_cnt++;
					printf("###########ERROR: W(0x%02X) != R(0x%02X): (%u / %u)\n",
						write_data, ioctl_reg_access_info.read_data[0],
						err_cnt, i+1);
					//
					///break;
				}
				else {
					printf(" %%% AGING result (%u / %u)\n", err_cnt, i+1);
				}
			}
			break;			

		case '4':
			DMSG0("[Reg IO] Multiple Single Read\n");
			show_register_page();
			while(1)
			{
				DMSG0("Select Page:");
				scanf("%x" , &page_idx);
				CLEAR_STDIN;
				if(page_idx <= 5)
					break;
			}
			
			DMSG0("Input Address(hex): ");
			scanf("%x", &addr);
			CLEAR_STDIN;			

		#if 0
			while(1)
			{
				DMSG1("Input the number of reading (up to %d:",
						MAX_NUM_MTV_REG_READ_BUF);
				scanf("%d" , &read_cnt);	
				CLEAR_STDIN;
				if(read_cnt <= MAX_NUM_MTV_REG_READ_BUF)
					break;
			}
		#else
		DMSG0("Input the number of reading :");
		scanf("%d" , &read_cnt);	
		CLEAR_STDIN;
		#endif

			ioctl_reg_access_info.page = page_value[page_idx];
			ioctl_reg_access_info.addr = addr;
			ioctl_reg_access_info.read_cnt = read_cnt;

			DMSG3("\t[INPUT] %s[0x%02X]: %u\n", page_str[page_idx], addr, read_cnt);

			for(i = 0; i < read_cnt; i++) {
				if(ioctl(fd_dmb_dev, IOCTL_TEST_REG_SINGLE_READ, &ioctl_reg_access_info) == 0)
					DMSG3("%s[0x%02X]: 0x%02X\n", 
						page_str[page_idx], ioctl_reg_access_info.addr,
						ioctl_reg_access_info.read_data[0]);
				else				
					EMSG0("IOCTL_TEST_REG_SINGLE_READ failed\n");

				//ioctl_reg_access_info.addr++;
			}
			break;	

		case '5':
			DMSG0("[Reg IO] AGING Single/Burst toggle Read/Write\n");
			show_register_page();
			while(1)
			{
				DMSG0("Select Page:");
				scanf("%x" , &page_idx);
				CLEAR_STDIN;
				if(page_idx <= 5)
					break;
			}
	
			DMSG0("WRITE. Input Address(hex) :,  data(hex) : ");
			scanf("%x" , &addr);	
			CLEAR_STDIN;
			
			scanf("%x" , &write_data);
			CLEAR_STDIN;
	
			//read_cnt = 1;
	
			DMSG3("%s[0x%02X]: 0x%02X\n", page_str[page_idx], addr, write_data);
	
			err_cnt = 0;
			//read_cnt = 1000000;
			//read_cnt = 100000;
			read_cnt = 100;
			//read_cnt = 100000000;
			//for(i = 0; i < read_cnt; i++) {
			for(i = 0; ; i++) {
				ioctl_reg_access_info.page = page_value[page_idx];
				ioctl_reg_access_info.addr = addr;
				if((i%2) == 0)
					write_data = 0x55;
				else
					write_data = 0xAA;
				ioctl_reg_access_info.write_data = write_data;
				
				if(ioctl(fd_dmb_dev, IOCTL_TEST_REG_WRITE, &ioctl_reg_access_info) < 0)
					EMSG0("IOCTL_TEST_REG_WRITE failed\n");


	
				ioctl_reg_access_info.page = page_value[5]; /* SPI_MEM */
				ioctl_reg_access_info.addr = 0x10;
				ioctl_reg_access_info.read_cnt = 16 * 188;
				if(ioctl(fd_dmb_dev, IOCTL_TEST_REG_BURST_READ, &ioctl_reg_access_info) == 0)
				{
					DMSG7("%s[0x%02X (%u)bytes]: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n", 
						page_str[page_idx],
						ioctl_reg_access_info.addr,
						ioctl_reg_access_info.read_cnt,
						ioctl_reg_access_info.read_data[0],
						ioctl_reg_access_info.read_data[1],
						ioctl_reg_access_info.read_data[2],
						ioctl_reg_access_info.read_data[3]);
				}
				else
					EMSG0("IOCTL_TEST_REG_BURST_READ failed\n");


				ioctl_reg_access_info.page = page_value[page_idx];
				ioctl_reg_access_info.addr = addr;
					if(ioctl(fd_dmb_dev, IOCTL_TEST_REG_SINGLE_READ, &ioctl_reg_access_info) == 0)
					DMSG3("%s[0x%02X]: 0x%02X\n", 
						page_str[page_idx], 
						addr, ioctl_reg_access_info.read_data[0]);
				else
					EMSG0("IOCTL_TEST_REG_SINGLE_READ failed\n");
	
				if (write_data != ioctl_reg_access_info.read_data[0]) {
					err_cnt++;
					printf("###########ERROR: W(0x%02X) != R(0x%02X): (%u / %u)\n",
						write_data, ioctl_reg_access_info.read_data[0],
						err_cnt, i+1);
					//
					break;
				}
				else {
					printf(" %%% AGING result (%u / %u)\n", err_cnt, i+1);
				}
			}
			break;			

		case '6':
			DMSG0("[Reg IO] AGING SPI Memory Test with Single IO\n");
			show_register_page();
			while(1)
			{
				DMSG0("Select Page:");
				scanf("%x" , &page_idx);
				CLEAR_STDIN;
				if(page_idx <= 5)
					break;
			}
	
			DMSG0("WRITE. Input Address(hex) :,  data(hex) : ");
			scanf("%x" , &addr);	
			CLEAR_STDIN;
			
			scanf("%x" , &write_data);
			CLEAR_STDIN;
	
			//read_cnt = 1;
	
			DMSG3("%s[0x%02X]: 0x%02X\n", page_str[page_idx], addr, write_data);
	
			err_cnt = 0;
			//read_cnt = 1000000;
			//read_cnt = 100000;
			read_cnt = 100;
			//read_cnt = 100000000;
			//for(i = 0; i < read_cnt; i++) {
			for(i = 0; ; i++) {
				if (i == 0)
					ioctl_reg_access_info.param1 = 0;
				else
					ioctl_reg_access_info.param1 = 1;

				ioctl_reg_access_info.page = page_value[page_idx];
				ioctl_reg_access_info.addr = addr;
				if((i%2) == 0)
					write_data = 0x55;
				else
					write_data = 0xAA;
				ioctl_reg_access_info.write_data = write_data;
				ioctl_reg_access_info.read_cnt = 16 * 188;
				if(ioctl(fd_dmb_dev, IOCTL_TEST_REG_SPI_MEM_READ, &ioctl_reg_access_info) == 0)
				{
					DMSG4("%s[0x%02X (%u)bytes]: 0x%02X\n", 
						page_str[page_idx],
						ioctl_reg_access_info.addr,
						ioctl_reg_access_info.read_cnt,
						ioctl_reg_access_info.read_data[0]);
				}
				else
					EMSG0("IOCTL_TEST_REG_SPI_MEM_READ failed\n");

				if (write_data != ioctl_reg_access_info.read_data[0]) {
					err_cnt++;
					printf("###########ERROR: W(0x%02X) != R(0x%02X): (%u / %u)\n",
						write_data, ioctl_reg_access_info.read_data[0],
						err_cnt, i+1);
					//
					break;
				}
				else {
					printf(" %%% AGING result (%u / %u)\n", err_cnt, i+1);
				}
			}
			break;		

		case '7':
			DMSG0("[Reg IO] AGING SPI Memory Test Only\n");
			
			err_cnt = 0;
			//read_cnt = 1000000;
			//read_cnt = 100000;
			read_cnt = 100;
			//read_cnt = 100000000;


			page_idx = 5;
			addr = 0x10;
			//for(i = 0; i < read_cnt; i++) {
			for(i = 0; ; i++) {
				int k, ff_cnt;

				/* for one-time page selection */
				ioctl_reg_access_info.write_data = i;

				ioctl_reg_access_info.page = page_value[page_idx];
				ioctl_reg_access_info.addr = 0x10;
				ioctl_reg_access_info.read_cnt = 16 * 188;
				if(ioctl(fd_dmb_dev, IOCTL_TEST_REG_ONLY_SPI_MEM_READ, &ioctl_reg_access_info) == 0)
				{
					DMSG7("%s[0x%02X (%u)bytes]: 0x%02X 0x%02X 0x%02X 0x%02X\n", 
						page_str[page_idx],
						ioctl_reg_access_info.addr,
						ioctl_reg_access_info.read_cnt,
						ioctl_reg_access_info.read_data[0],
						ioctl_reg_access_info.read_data[1],
						ioctl_reg_access_info.read_data[2],
						ioctl_reg_access_info.read_data[3]);
				}
				else
					EMSG0("IOCTL_TEST_REG_ONLY_SPI_MEM_READ failed\n");

				ff_cnt = 0;
				for (k = 0; k < ioctl_reg_access_info.read_cnt; k++) {
					if (ioctl_reg_access_info.read_data[k] == 0xFF)
						ff_cnt++;
				}

				if (ff_cnt == ioctl_reg_access_info.read_cnt) {
					err_cnt++;
					printf("###########ERROR: All 0xFF: (%u / %u)\n",
						err_cnt, i+1);
				}				
				else {
					printf(" %%% AGING result (%u / %u)\n", err_cnt, i+1);
				}
			}
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

int open_tdmb_device(void)
{
	int fd;
	char name[32];
	
	sprintf(name,"/dev/%s", TDMB_DEV_NAME);

#ifdef MTV_BLOCKING_READ_MODE
	fd = open(name, O_RDWR); /* Blocking mode */
#else
	fd = open(name, O_RDWR|O_NONBLOCK); /* Non-blocking mode */
#endif	
	if(fd < 0)
		EMSG1("Can't not open %s\n", name);

	return fd;
}

int main(void)
{
	int key, ret;
	int fd_dmb_dev;

	dmb_read_thread_should_stop = 1;
	tsp_read_thread_cb_ptr = NULL;

	dm_timer_thread_run = 0;
	dm_timer_thread_cb_ptr = NULL;

	mkdir(TS_DUMP_DIR, 0777);

	/* Create the read thread. */
	if ((ret = test_create_timer_thread()) != 0) {
		EMSG1("test_create_timer_thread() failed: %d\n", ret);
		goto APP_MAIN_EXIT;
	}

  	while (1) {
		DMSG0("===============================================\n");
		DMSG0("\t1: TDMB Test\n");
		DMSG0("\t4: GPIO Test\n");
		DMSG0("\t5: Register IO Test\n");
		DMSG0("\tq or Q: Quit\n");
		DMSG0("===============================================\n");
   		
		fflush(stdin);
		key = getc(stdin);				
		CLEAR_STDIN;
		
		switch( key )
		{
		case '1':
			test_TDMB();
			break;

		case '4':
			test_GPIO();
			break;

		case '5':
			if((fd_dmb_dev=open_tdmb_device()) < 0)
				goto APP_MAIN_EXIT;				
			
			/* Power-up chip*/
			if(ioctl(fd_dmb_dev, IOCTL_TEST_MTV_POWER_ON) < 0)
				EMSG0("IOCTL_TEST_MTV_POWER_ON failed\n");
			else				
				test_RegisterIO(fd_dmb_dev);
			
			if(ioctl(fd_dmb_dev, IOCTL_TEST_MTV_POWER_OFF) < 0)
				EMSG0("IOCTL_TEST_MTV_POWER_OFF failed\n");

			close(fd_dmb_dev);
			break;
			
		case 'q':
		case 'Q': goto APP_MAIN_EXIT;
		default: DMSG1("[%c]\n", key);
		}
	}

APP_MAIN_EXIT:

	test_delete_timer_thread();

	return 0;
}
