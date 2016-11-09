#ifndef __MTV_H__
#define __MTV_H__

#ifdef __cplusplus 
extern "C"{
#endif

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/wait.h>
#include <linux/stat.h>
#include <linux/ioctl.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/atomic.h>
//#include <asm/atomic.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/poll.h>  
#include <linux/list.h> 
#include <linux/freezer.h>
#include <linux/completion.h>
//#include <linux/smp_lock.h>
#include <linux/jiffies.h>
#include <linux/wakelock.h>

#include "./src/nxb110tv.h" /* Place before mtv_ioctl.h ! */
#include "mtv_ioctl.h"


#define DMB_DEBUG


#if defined(RTV_IF_SPI)
	#define DEBUG_MTV_IF_MEMORY
#endif

#define DMBERR(fmt, args...) \
	do { printk(KERN_ERR "MTV: %s(): " fmt, __func__, ## args); } while (0)

#ifdef DMB_DEBUG
	#define DMBMSG(fmt, args...) \
		printk(KERN_INFO "MTV: %s(): " fmt, __func__, ## args)
#else 
	#define DMBMSG(x...)  ((void)0) /* null */
#endif 


#define MAX_NUM_TS_PKT_BUF 	40

#if defined(RTV_IF_SPI)
	#define MTV_TS_THRESHOLD_SIZE			(10 * 188) /* Video */
	#define MTV_TS_AUDIO_THRESHOLD_SIZE  	(4 * 384) /* Audio */
	#define MTV_TS_DATA_THRESHOLD_SIZE  	(4 * 96) /* Data */

#else /* TSIF */
	#define MTV_TSIF_TSP_NUM	1//16 /* (8 x 188) or (16 x 188) */
	
	#define MTV_TS_THRESHOLD_SIZE			(MTV_TSIF_TSP_NUM * 188) /* Video */
	#define MTV_TS_AUDIO_THRESHOLD_SIZE 	(MTV_TSIF_TSP_NUM * 188) /* Audio */
	#define MTV_TS_DATA_THRESHOLD_SIZE		(MTV_TSIF_TSP_NUM * 188) /* Data */
#endif


/* Wakeup MSC TSP count in blocking read mode for SPI interface */
#define MTV_SPI_WAKEUP_TSP_Q_CNT	2

typedef enum
{	
	DMB_TV_MODE_TDMB   = 0,
	DMB_TV_MODE_DAB     = 1,
	DMB_TV_MODE_1SEG   = 3,
	DMB_TV_MODE_FM       = 4
} E_DMB_TV_MODE_TYPE;

typedef struct
{
	struct list_head link; /* to queuing */

#ifdef RTV_SPI_MSC1_ENABLED
	UINT msc1_size;
	U8   msc1_buf[MTV_TS_THRESHOLD_SIZE+1]; /* Max Video buffering size. */
#endif

#ifdef RTV_SPI_MSC0_ENABLED
	/* Used in the case of TDMB/DAB(Multi subch) and FM(RDS) */
	UINT msc0_size; /* msc0 ts size. */
	U8   msc0_buf[3*1024];
#endif

#ifdef RTV_FIC_SPI_INTR_ENABLED
	/* FIC interrupt Mode */
	UINT fic_size;
	#if defined(RTV_IF_SPI)
	U8 fic_buf[384 + 1];
	#else
	U8 fic_buf[384];
	#endif
#endif
} MTV_TS_PKT_INFO; 


/* Control Block */
struct mtv_cb
{
	int demod_no;

	E_DMB_TV_MODE_TYPE tv_mode;
	BOOL is_power_on;
	//atomic_t open_flag; /* to open only once */

	/* to prevent reading from the multiple applicatoin threads. */
	atomic_t read_flag;

	/* Prevent concurrent accessing of prev_tsp on read:read_single_service(), 
	and mtv_free_tsp(mtv_cb_ptr->prev_tsp) on ioctl:mtv_reset_tsp_queue(). */
	struct mutex read_lock;

	struct mutex ioctl_lock;

	struct wake_lock wake_lock;
	struct device *dev;
	unsigned int f_flags;
	unsigned int freq_khz;

#if defined(RTV_IF_SPI) || defined(RTV_FIC_I2C_INTR_ENABLED)
	struct task_struct *isr_thread_cb;
	wait_queue_head_t isr_wq;

	#if defined(RTV_IF_SPI)
	/* Belows variables used for the blocking mode read */
	wait_queue_head_t read_wq;
	unsigned int wakeup_tspq_thres_cnt; /* # of threshold q count to wake. */
	volatile BOOL read_stop;

	/* Saved previous tsp to use in next time if not finishded. */
	/* previous tsp. shared by ioctl() and read() in reset_tsp()  */
	MTV_TS_PKT_INFO *prev_tsp;
	#endif /* #if defined(RTV_IF_SPI) */

	int irq; /* IRQ number */
	unsigned long isr_cnt;

	bool first_interrupt;
#endif

#ifdef DEBUG_MTV_IF_MEMORY
	#ifdef RTV_SPI_MSC0_ENABLED
	unsigned long msc0_ts_intr_cnt; /* up to DM get */
	unsigned long msc0_ovf_intr_cnt; /* up to DM get */

	#ifdef RTV_CIF_MODE_ENABLED
	unsigned long msc0_cife_cnt; /* up to DM get */
	#endif
	#endif /* RTV_SPI_MSC0_ENABLED */

	#ifdef RTV_SPI_MSC1_ENABLED
	unsigned long msc1_ts_intr_cnt; /* up to DM get */
	unsigned long msc1_ovf_intr_cnt; /* up to DM get */
	#endif
	
	unsigned int max_alloc_tsp_cnt;
	unsigned int max_remaining_tsp_cnt;
#endif

#if defined(RTV_DAB_ENABLE) || defined(RTV_TDMB_ENABLE)
	volatile UINT av_subch_id; // MSC1

	#ifndef RTV_CIF_MODE_ENABLED
	volatile UINT data_subch_id; // MSC0. Use for threshold mode.
	#endif

	/* Number of opened sub chaneel. To user reset tsp pool. */
	atomic_t num_opened_subch;

	BOOL fic_opened;

	#ifdef RTV_DAB_ENABLE
	UINT fic_size;
	#endif

	#if defined(RTV_IF_SPI) && defined(RTV_CIF_MODE_ENABLED)\
	&& defined(RTV_BUILD_CIFDEC_WITH_DRIVER)
	/* Pointer to DATA MSC buffer to be decoded. */
	unsigned char *dec_msc_kbuf_ptr[RTV_MAX_NUM_DAB_DATA_SVC];
	#endif
#endif
	/* Threshold size of MSC1 memory to raise the interrupt. */
	unsigned int msc1_thres_size;

	/* Threshold size of MSC0 memory to raise the interrupt. */
	unsigned int msc0_thres_size;

#if defined(RTV_FIC_I2C_INTR_ENABLED)
	struct fasync_struct *fasync;
#endif


#if defined(RTV_IF_TSIF) || defined(RTV_IF_SPI_SLAVE)
	struct i2c_client *i2c_client_ptr;
	struct i2c_adapter *i2c_adapter_ptr;
#elif defined(RTV_IF_SPI)
	struct spi_device *spi_ptr;	
#endif
};


typedef struct
{
	struct list_head head;
	unsigned int cnt; /* queue count */
	unsigned int total_bytes;
	spinlock_t lock;
} MTV_TSP_QUEUE_INFO;


extern struct mtv_cb *mtv_cb_ptr;
#if defined(RTV_IF_TSIF) || defined(RTV_IF_SPI_SLAVE)
	extern struct i2c_driver mtv_i2c_driver;
#elif defined(RTV_IF_SPI)
	extern struct spi_driver mtv_spi_driver_0;
#endif


unsigned int get_tsp_queue_count(int demod_no);
MTV_TS_PKT_INFO *mtv_peek_tsp(int demod_no);
MTV_TS_PKT_INFO *mtv_get_tsp(int demod_no);
void mtv_put_tsp(int demod_no, MTV_TS_PKT_INFO *pkt);
void mtv_free_tsp(int demod_no, MTV_TS_PKT_INFO *pkt);
MTV_TS_PKT_INFO *mtv_alloc_tsp(struct mtv_cb *mtv_cb_ptr);

#ifdef __cplusplus 
} 
#endif 

#endif /* __MTV_H__*/

