
#ifndef __TDMB_H__
#define __TDMB_H__

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
#include <linux/ioctl.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/atomic.h>
//#include <asm/atomic.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/wait.h>
#include <linux/list.h> 
#include <linux/jiffies.h>
#include <linux/wakelock.h>
#include <linux/kthread.h>

#define DMB_DEBUG_MSG_ENABLE
#define CONFIG_TDMB_SPI 

#if defined(CONFIG_TDMB_SPI) || defined(CONFIG_TDMB_EBI2)
	/* Select debug options */
	#define DEBUG_INTERRUPT
	#define DEBUG_TSP_BUF
#endif

/*############################################################################
# File dump Configuration
	* TS dump filename: /data/local/isdbt_ts_FREQ.ts
############################################################################*/
//#define _TDMB_KERNEL_FILE_DUMP_ENABLE

#ifdef _TDMB_KERNEL_FILE_DUMP_ENABLE
	extern struct file *tdmb_msc_dump_filp;
	extern struct file *tdmb_fic_dump_filp;
#endif


#define DMBERR(fmt, args...) \
	printk(KERN_ERR "TDMB: %s(): " fmt, __func__, ## args)

#ifdef DMB_DEBUG_MSG_ENABLE
	#define DMBMSG(fmt, args...) \
		printk(KERN_INFO "TDMB: %s(): " fmt, __func__, ## args)
#else 
	#define DMBMSG(x...)  do {} while (0)
#endif 


#define MAX_NUM_TS_PKT_BUF 	40

#if defined(CONFIG_TDMB_SPI) || defined(CONFIG_TDMB_EBI2)
	#define TDMB_DMB_THRESHOLD_SIZE		(16 * 188) /* Video */
	#define TDMB_DAB_THRESHOLD_SIZE  	(4 * 384) /* DAB */
	#define TDMB_DABPLUS_THRESHOLD_SIZE  	(4 * 384) /* DAB+ */

	/* 2 more subch or fic decode in play. */
	#define TDMB_MULTI_SERVICE_THRESHOLD_SIZE (16 * 188)

#else /* TSIF */
	#define TDMB_DMB_THRESHOLD_SIZE	(188) /* Video */
	#define TDMB_DAB_THRESHOLD_SIZE	(188) /* Data */
	#define TDMB_DABPLUS_THRESHOLD_SIZE  	(188) /* DAB+ Data */
#endif


/* Wakeup MSC TSP count in blocking read mode for SPI interface */
#define TDMB_SPI_WAKEUP_TSP_Q_CNT	4

//#if defined(CONFIG_TDMB_MTV319)
	#define TDMB_SPI_CMD_SIZE	3
//#else
//	#error "Code not present"
//#endif

/* TSP Buffer */
typedef struct {
	struct list_head link; /* to queuing */
	unsigned int size;
	unsigned char buf[TDMB_DMB_THRESHOLD_SIZE];
} TDMB_TSPB_INFO;

/* TSPB queue shared by ISR and read() */
typedef struct {
	struct list_head head;
	unsigned int cnt; /* queue count */
	unsigned int total_bytes;
	spinlock_t lock;
} TDMB_TSPB_HEAD_INFO;

/* Control Block */
struct tdmb_cb {
#if defined(CONFIG_TDMB_TSIF)
	struct i2c_client *i2c_client_ptr;
	struct i2c_adapter *i2c_adapter_ptr;
#elif defined(CONFIG_TDMB_SPI)
	struct spi_device *spi_ptr; 
#elif defined(CONFIG_TDMB_EBI2)
	void __iomem *ioaddr;
	struct resource *io_mem;
#endif

	bool is_power_on;
	atomic_t num_opened_subch; /* Number of opened sub channel. */
	atomic_t open_flag; /* to open only once */
	struct mutex ioctl_lock;
	struct wake_lock wake_lock;
	bool fic_opened;

	bool irq_thread_sched_changed;

	int irq; /* IRQ number */
	struct workqueue_struct *isr_workqueue;
	struct work_struct tdmb_isr_work;

#if defined(CONFIG_TDMB_TSIF) // && defined(RTV_FIC_I2C_INTR_ENABLED)
	/* Used when TSIF and FIC interrupt mode. */
	struct fasync_struct *fasync;

#elif defined(CONFIG_TDMB_SPI) || defined(CONFIG_TDMB_EBI2)
	/* Belows variables used for the reading of blocking mode. */
	wait_queue_head_t read_wq;
	unsigned int wakeup_tspq_thres_cnt; /* # of threshold q count to wake. */

	volatile bool read_stop;

	/* to prevent reading from the multiple applicatoin threads. */
	atomic_t read_flag;

	/* Prevent concurrent accessing of prev_tspb on read:read_single_service(), 
	and tdmb_tspb_free_buffer(tdmb_cb_ptr->prev_tspb) on ioctl:mtv_reset_tsp_queue(). */
	struct mutex read_lock;

	/* Saved previous tspb to use in next time if not finishded. */
	TDMB_TSPB_INFO *prev_tspb;

	#ifndef RTV_MCHDEC_DIRECT_COPY_USER_SPACE
	/* Pointer to MSC buffer to be decoded. */
	unsigned char *dec_msc_kbuf_ptr[RTV_MAX_NUM_USE_SUBCHANNEL];
	#endif

	unsigned int intr_ts_size; /* real size of data to be read */
	bool first_interrupt;

	struct device *dev;
	unsigned int f_flags;
	unsigned int freq_khz;
#endif

#ifdef DEBUG_INTERRUPT
	unsigned long invalid_intr_cnt;
	unsigned long level_intr_cnt;
	unsigned long ovf_intr_cnt;
	unsigned long udf_intr_cnt;
#endif

#ifdef DEBUG_TSP_BUF
	unsigned int max_alloc_tspb_cnt;
	unsigned int max_enqueued_tspb_cnt;
	unsigned long alloc_tspb_err_cnt;
#endif
};


extern struct tdmb_cb *tdmb_cb_ptr;
#if defined(CONFIG_TDMB_TSIF)
	int tdmb_deinit_i2c_bus(void);
	int tdmb_init_i2c_bus(void);

#elif defined(CONFIG_TDMB_SPI)
	int tdmb_deinit_spi_bus(void);
	int tdmb_init_spi_bus(void);

#elif defined(CONFIG_TDMB_EBI2)
	int tdmb_deinit_ebi_bus(void);
	int tdmb_init_ebi_bus(void);
#endif


#ifdef RTV_MULTIPLE_CHANNEL_MODE
void tdmb_tspb_queue_clear_contents(int fic_msc_type);
#endif

unsigned int tdmb_tspb_queue_count(void);
void tdmb_tspb_queue_reset(void);
TDMB_TSPB_INFO *tdmb_tspb_peek(void);
TDMB_TSPB_INFO *tdmb_tspb_dequeue(void);
void tdmb_tspb_enqueue(TDMB_TSPB_INFO *pkt);

unsigned int tdmb_tspb_freepool_count(void);

void tdmb_tspb_free_buffer(TDMB_TSPB_INFO *pkt);
TDMB_TSPB_INFO *tdmb_tspb_alloc_buffer(void);
int tdmb_tspb_delete_pool(void);
int tdmb_tspb_create_pool(void);

#endif /* __TDMB_H__*/

