/*
 * isdbt.c
 *
 * Driver for ISDB-T.
 *
 * Copyright (C) (2013, NEXELL)
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/version.h>

#include "nxb220.h" /* Place before isdbt_ioctl.h ! */
#include "nxb220_internal.h"

#include "isdbt.h"
#include "isdbt_debug.h"
#include "isdbt_gpio.h"
#include "isdbt_ioctl.h"
#include "isdbt_ioctl_func.h"


#if defined(NXTV_IF_SPI)
	// Select time of ksmem allocation.
	#define _MAKE_TSB_ALLOC_FROM_BOOT

	#ifdef _MAKE_TSB_ALLOC_FROM_BOOT
		#define MAX_TSB_DESC_SIZE 	PAGE_ALIGN(sizeof(struct ISDBT_TSB_DESC_INFO))
		#define MAX_TSB_SEG_SIZE	PAGE_ALIGN(188 * 212)

		#define TOTAL_TSB_MAPPING_SIZE\
			(MAX_TSB_DESC_SIZE\
				+ ISDBT_MAX_NUM_TSB_SEG*MAX_TSB_SEG_SIZE)
	#endif /* _MAKE_TSB_ALLOC_FROM_BOOT */	
#endif


struct ISDBT_CB *isdbt_cb_ptr = NULL;

#ifdef CONFIG_ISDBT_CAMIF
extern int camif_isdbt_stop(void);
extern int camif_isdbt_start(unsigned int num_tsp);
extern int camif_isdbt_close(void);
extern int camif_isdbt_open(void);
#endif

#if defined(NXTV_IF_SPI)
	extern irqreturn_t isdbt_irq_handler(int irq, void *param);

	#ifdef ISDBT_SPI_ISR_HANDLER_IS_KTHREAD
		extern int isdbt_isr_thread(void *data);
	#else
		extern void isdbt_isr_handler(struct work_struct *work);
	#endif

static int isdbt_create_isr(void)
{
	int ret = 0;

#ifdef ISDBT_SPI_ISR_HANDLER_IS_KTHREAD
	if (isdbt_cb_ptr->isr_thread_cb == NULL) {
		atomic_set(&isdbt_cb_ptr->isr_cnt, 0); /* Reset */
		init_waitqueue_head(&isdbt_cb_ptr->isr_wq);
 
		isdbt_cb_ptr->isr_thread_cb
			= kthread_run(isdbt_isr_thread, NULL, ISDBT_DEV_NAME);
		if (IS_ERR(isdbt_cb_ptr->isr_thread_cb)) {
			WARN(1, KERN_ERR "Create ISR thread error\n");
			ret = PTR_ERR(isdbt_cb_ptr->isr_thread_cb);
			isdbt_cb_ptr->isr_thread_cb = NULL;
		}
	}
#else
	INIT_WORK(&isdbt_cb_ptr->isr_work, isdbt_isr_handler);
	isdbt_cb_ptr->isr_workqueue
			= create_singlethread_workqueue(ISDBT_DEV_NAME);
	if (isdbt_cb_ptr->isr_workqueue == NULL) {
		DMBERR("Couldn't create workqueue\n");
		ret = -ENOMEM;
	}
#endif
	return ret;
}

static void isdbt_destory_isr(void)
{
#ifdef ISDBT_SPI_ISR_HANDLER_IS_KTHREAD
	if (isdbt_cb_ptr->isr_thread_cb) {
		kthread_stop(isdbt_cb_ptr->isr_thread_cb);
		isdbt_cb_ptr->isr_thread_cb = NULL;
	}
#else
	if (isdbt_cb_ptr->isr_workqueue) {
		cancel_work_sync(&isdbt_cb_ptr->isr_work);
		//flush_workqueue(isdbt_cb_ptr->isr_workqueue); 
		destroy_workqueue(isdbt_cb_ptr->isr_workqueue);
		isdbt_cb_ptr->isr_workqueue = NULL;
	}
#endif
}

unsigned int isdbt_get_enqueued_tsp_count(void)
{
	int readi, writei;
	unsigned int num_tsp = 0;
	struct ISDBT_TSB_CB_INFO *tsb_cb = &isdbt_cb_ptr->tsb_cb;

	if (!tsb_cb->tsbd) {
		DMBERR("Not memory mapped\n");
		return 0;
	}

	if (tsb_cb->tsbd->op_enabled) {
		readi = tsb_cb->tsbd->read_idx;
		writei = tsb_cb->tsbd->write_idx;

		if (writei > readi)
			num_tsp = writei - readi;
		else if (writei < readi)
			num_tsp = tsb_cb->num_total_tsp - (readi - writei);
		else
			num_tsp = 0; /* Empty */
	}

	return num_tsp;
}

/* Get a TS buffer */
U8 *isdbt_tsb_get(void)
{
	int readi;
	int nwi; /* Next index of tsp buffer to be write. */
	struct ISDBT_TSB_CB_INFO *tsb_cb = &isdbt_cb_ptr->tsb_cb;
	unsigned char *tspb = NULL;
	int num_tsp_per_seg = tsb_cb->num_tsp_per_seg;
#ifdef DEBUG_TSP_BUF
	int num_used_segment; /* Should NOT zero. */
	int write_seg_idx, read_seg_idx;
#endif

	if (!tsb_cb->tsbd) {
		DMBERR("Not memory mapped\n");
		return NULL;
	}

	if (tsb_cb->tsbd->op_enabled) {
		readi = tsb_cb->tsbd->read_idx;

		/* Get the next avaliable index of segment to be write in the next time. */
		nwi = tsb_cb->avail_write_tspb_idx + num_tsp_per_seg;
		if (nwi >= tsb_cb->num_total_tsp)
			nwi = 0;

		if ((readi < nwi) || (readi >= (nwi + num_tsp_per_seg))) {
			tspb = tsb_cb->seg_buf[tsb_cb->avail_seg_idx];

			/* Update the writting index of tsp buffer. */
			tsb_cb->avail_write_tspb_idx = nwi;

			/* Update the avaliable index of segment to be write in the next time. */
			if (++tsb_cb->avail_seg_idx >= tsb_cb->num_total_seg)
				tsb_cb->avail_seg_idx = 0;

#ifdef DEBUG_TSP_BUF
			write_seg_idx = tsb_cb->avail_seg_idx;
			read_seg_idx = readi / num_tsp_per_seg;

			if (write_seg_idx > read_seg_idx)
				num_used_segment = write_seg_idx - read_seg_idx;
			else
				num_used_segment
					= tsb_cb->num_total_seg - (read_seg_idx - write_seg_idx);

			DMBMSG("wseg_idx(%d), rseg_idx(%d), num_used_segment(%d)\n",
						write_seg_idx, read_seg_idx, num_used_segment);

			isdbt_cb_ptr->max_alloc_seg_cnt
				= MAX(isdbt_cb_ptr->max_alloc_seg_cnt, num_used_segment);
#endif

			//DMBMSG("@@ readi(%d), next_writei(%d), avail_seg_idx(%d), tspb(0x%08lX)\n",
			//		readi, nwi, tsb_cb->avail_seg_idx, (unsigned long)tspb);
		} else
			DMBERR("Full tsp buffer.\n");
	}

	return tspb;
}

void isdbt_tsb_enqueue(unsigned char *ts_chunk)
{
#ifdef DEBUG_TSP_BUF
	int readi, writei, num_euqueued_tsp, num_euqueued_seg;
#endif
	struct ISDBT_TSB_CB_INFO *tsb_cb = &isdbt_cb_ptr->tsb_cb;

	if (!tsb_cb->tsbd) {
		DMBERR("Not memory mapped\n");
		return;
	}

	if (tsb_cb->tsbd->op_enabled) {
		/* Check if the specified tspb is the allocated tspb? */
		if (ts_chunk == tsb_cb->seg_buf[tsb_cb->enqueue_seg_idx]) {
			/* Update the next index of write-tsp. */
			tsb_cb->tsbd->write_idx = tsb_cb->avail_write_tspb_idx;

			/* Update the next index of segment. */
			tsb_cb->enqueue_seg_idx = tsb_cb->avail_seg_idx;

#ifdef DEBUG_TSP_BUF
			readi = tsb_cb->tsbd->read_idx;
			writei = tsb_cb->tsbd->write_idx;

			if (writei > readi)
				num_euqueued_tsp = writei - readi;
			else if (writei < readi)
				num_euqueued_tsp = tsb_cb->num_total_tsp - (readi - writei);
			else
				num_euqueued_tsp = 0;

			isdbt_cb_ptr->max_enqueued_tsp_cnt
				= MAX(isdbt_cb_ptr->max_enqueued_tsp_cnt, num_euqueued_tsp);

			num_euqueued_seg = num_euqueued_tsp / tsb_cb->num_tsp_per_seg;
			isdbt_cb_ptr->max_enqueued_seg_cnt
				= MAX(isdbt_cb_ptr->max_enqueued_seg_cnt, num_euqueued_seg);
#endif
		} else
			DMBERR("Invalid the enqueuing chunk address!\n");
	}
}

static inline void tsb_free_mapping_area(void)
{
	int i;
	unsigned int order;
	struct ISDBT_TSB_CB_INFO *tsb_cb = &isdbt_cb_ptr->tsb_cb;

	order = get_order(tsb_cb->seg_size);
	for (i = 0; i < tsb_cb->num_total_seg; i++) {
		if (tsb_cb->seg_buf[i]) {
			//DMBMSG("SEG[%d]: seg_buf(0x%lX)\n", i, (unsigned long)tsb_cb->seg_buf[i]);
			free_pages((unsigned long)tsb_cb->seg_buf[i], order);
			tsb_cb->seg_buf[i] = NULL;
		}
	}

	tsb_cb->seg_bufs_allocated = false;
	tsb_cb->seg_size = 0;
	tsb_cb->num_total_seg = 0;

	if (tsb_cb->tsbd) {
		order = get_order(tsb_cb->desc_size);
		free_pages((unsigned long)tsb_cb->tsbd, order);

		tsb_cb->tsbd = NULL;
		tsb_cb->desc_size = 0;
	}
}

#ifdef _MAKE_TSB_ALLOC_FROM_BOOT
static inline int tsb_alloc_mapping_area(unsigned int desc_size,
										unsigned int seg_size, int num_seg)
{
	int i, ret;
	unsigned int order;
	struct ISDBT_TSB_CB_INFO *tsb_cb = &isdbt_cb_ptr->tsb_cb;

	/* Allocate the TSB descriptor. */
	order = get_order(desc_size);
	tsb_cb->tsbd
		= (struct ISDBT_TSB_DESC_INFO *)__get_dma_pages(GFP_KERNEL, order);
	if (!tsb_cb->tsbd) {
		DMBERR("DESC allocation error\n");
		return -ENOMEM;
	}

	/* Allocate the TSB segments. */
	order = get_order(seg_size);
	DMBMSG("SEG order(%u)\n", order);

	if (order > MAX_ORDER) {
		DMBERR("Invalid page order value of segment (%u)\n", order);
		ret = -ENOMEM;
		goto free_tsb;
	}

	for (i = 0; i < num_seg; i++) {
		tsb_cb->seg_buf[i] = (U8 *)__get_dma_pages(GFP_KERNEL, order);
		if (!tsb_cb->seg_buf[i]) {
			DMBERR("SEG[%u] allocation error\n", i);
			ret = -ENOMEM;
			goto free_tsb;
		}
	}

	tsb_cb->seg_bufs_allocated = true;

	DMBMSG("Success\n");

	return 0;

free_tsb:
	tsb_free_mapping_area();

	return ret;
}
#endif /* #ifndef _MAKE_TSB_ALLOC_FROM_BOOT */

static void isdbt_mmap_close(struct vm_area_struct *vma)
{
	DMBMSG("Entered. mmap_completed(%d)\n", isdbt_cb_ptr->tsb_cb.mmap_completed);

#ifndef _MAKE_TSB_ALLOC_FROM_BOOT
	if (isdbt_cb_ptr->tsb_cb.mmap_completed == true)
		tsb_free_mapping_area();
#endif

	isdbt_cb_ptr->tsb_cb.mmap_completed = false;

	DMBMSG("Leaved...\n");
}

static const struct vm_operations_struct isdbt_mmap_ops = {
	.close = isdbt_mmap_close,
};

static int isdbt_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int ret, num_total_seg;
	unsigned int i, mmap_size, desc_size, seg_size;
	unsigned long pfn;
	unsigned long start = vma->vm_start;
	struct ISDBT_TSB_CB_INFO *tsb_cb = &isdbt_cb_ptr->tsb_cb;
#ifndef _MAKE_TSB_ALLOC_FROM_BOOT
	unsigned int desc_order, seg_order;
#endif

	vma->vm_flags |= VM_RESERVED;
	vma->vm_ops = &isdbt_mmap_ops;

	mmap_size = vma->vm_end - vma->vm_start;

#if 0
	DMBMSG("mmap_size(0x%X), vm_start(0x%lX), vm_page_prot(0x%lX)\n",
				mmap_size, vma->vm_start, vma->vm_page_prot);
#endif

	if (mmap_size & (~PAGE_MASK)) {
		DMBERR("Must align with PAGE size\n");
		return -EINVAL;
	}

	if (tsb_cb->mmap_completed == true) {
		DMBERR("Already mapped!\n");
		return 0;
	}

	seg_size = vma->vm_pgoff << PAGE_SHIFT;
	num_total_seg = (mmap_size - PAGE_SIZE) / seg_size;

	desc_size = mmap_size - (num_total_seg * seg_size);

	/* Save */
	tsb_cb->desc_size = desc_size;
	tsb_cb->seg_size = seg_size;
	tsb_cb->num_total_seg = num_total_seg;

#if 1
	DMBMSG("mmap_size(%u), seg_size(%u) #seg(%d), desc_size(%u)\n",
				mmap_size, seg_size, num_total_seg, desc_size);
#endif

	if (num_total_seg > ISDBT_MAX_NUM_TSB_SEG) {
		DMBERR("Too large request #seg! kernel(%u), req(%d)\n",
			ISDBT_MAX_NUM_TSB_SEG, num_total_seg);
		return -ENOMEM;
	}

#ifdef _MAKE_TSB_ALLOC_FROM_BOOT
	if (desc_size > MAX_TSB_DESC_SIZE) {
		DMBERR("Too large request desc size! kernel(%u), req(%u)\n",
			MAX_TSB_DESC_SIZE, desc_size);
		return -ENOMEM;
	}

	if (seg_size > MAX_TSB_SEG_SIZE) {
		DMBERR("Too large request seg size! kernel(%u), req(%u)\n",
			MAX_TSB_SEG_SIZE, seg_size);
		return -ENOMEM;
	}

	if (!tsb_cb->tsbd) {
		DMBERR("TSB DESC was NOT allocated!\n");
		return -ENOMEM;
	}

	if (tsb_cb->seg_bufs_allocated == false) {
		DMBERR("TSB SEG are NOT allocated!\n");
		return -ENOMEM;
	}
#else
	seg_order = get_order(seg_size);
	DMBMSG("SEG order(%u)\n", seg_order);
	
	if (seg_order > MAX_ORDER) {
		DMBERR("Invalid page order value of segment (%u)\n", seg_order);
		return -ENOMEM;
	}

	/* Allocate the TSB descriptor. */
	desc_order = get_order(desc_size);
	tsb_cb->tsbd
		= (struct ISDBT_TSB_DESC_INFO *)__get_dma_pages(GFP_KERNEL, desc_order);
	if (!tsb_cb->tsbd) {		DMBERR("DESC allocation error\n");
		return -ENOMEM;
	}
#endif

	/* Map the shared informations. */
	pfn = virt_to_phys(tsb_cb->tsbd) >> PAGE_SHIFT;
	if (remap_pfn_range(vma, vma->vm_start, pfn, desc_size, vma->vm_page_prot)) {
		DMBERR("HDR remap_pfn_range() error!\n");
		ret = -EAGAIN;
		goto out;
	}

	/* Init descriptor except the addres of segments */
	tsb_cb->tsbd->op_enabled = 0;
	tsb_cb->tsbd->read_idx = 0;
	tsb_cb->tsbd->write_idx = 0;

#if 0
	DMBMSG("tsbd(0x%lX), pfn(0x%lX), start(0x%lX)\n",
		(unsigned long)tsb_cb->tsbd, pfn, start);
#endif

	start += desc_size; /* Avdance VMA. */

	/* Allocate and map the TSP buffer segments. */
	for (i = 0; i < num_total_seg; i++) {
#ifndef _MAKE_TSB_ALLOC_FROM_BOOT
		tsb_cb->seg_buf[i] = (U8 *)__get_dma_pages(GFP_KERNEL, seg_order);
		if (!tsb_cb->seg_buf[i]) {
			DMBERR("SEG[%u] allocation error\n", i);
			ret = -ENOMEM;
			goto out;
		}
#endif

		pfn = virt_to_phys(tsb_cb->seg_buf[i]) >> PAGE_SHIFT;

#if 0
		DMBMSG("SEG[%d]: seg_buf(0x%lX) pfn(0x%lX) start(0x%lX)\n",
				i, (unsigned long)tsb_cb->seg_buf[i], pfn, start);
#endif

		if (remap_pfn_range(vma, start, pfn, seg_size, vma->vm_page_prot)) {
			DMBERR("SEG[%u] remap_pfn_range() error!\n", i);
			ret = -EAGAIN;
			goto out;
		}

		tsb_cb->tsbd->seg_base[i] = start;
		start += seg_size;
	}

#ifndef _MAKE_TSB_ALLOC_FROM_BOOT
	tsb_cb->seg_bufs_allocated = true;
#endif
	tsb_cb->mmap_completed = true;

	return 0;

out:
#ifndef _MAKE_TSB_ALLOC_FROM_BOOT
	/* Free kernel mapped memory */
	tsb_free_mapping_area();
#endif

	return ret;
}
#endif /* #if defined(NXTV_IF_SPI) */

static int isdbt_power_off(void)
{
	int ret = 0;

	if (isdbt_cb_ptr->is_power_on == FALSE)
		return 0;

	isdbt_cb_ptr->is_power_on = FALSE;
	isdbt_cb_ptr->tsout_enabled = false;

	DMBMSG("START\n");

	isdbt_disable_ts_out();

#if defined(NXTV_IF_SPI)
	disable_irq(isdbt_cb_ptr->irq);

	isdbt_destory_isr();
#endif

	ret = __isdbt_deinit();

	nxtvOEM_PowerOn_FULLSEG(0);

	DMBMSG("END\n");

	return ret;
}

static int isdbt_power_on(unsigned long arg)
{
	int ret = 0;

	if (isdbt_cb_ptr->is_power_on == TRUE)	
		return 0;

	DMBMSG("Start\n");

	nxtvOEM_PowerOn_FULLSEG(1);
	isdbt_cb_ptr->is_power_on = TRUE;

	ret = __isdbt_power_on(arg);
	if (ret != 0) {
		return ret;
	}

#if defined(NXTV_IF_SPI)
	ret = isdbt_create_isr();
	if (ret != 0)
		return ret;

	enable_irq(isdbt_cb_ptr->irq); /* After DMB init */
#endif

	DMBMSG("End\n");

	return ret;
}

static ssize_t isdbt_read(struct file *filp, char *buf,
				size_t count, loff_t *pos)
{
	DMBMSG("Empty function\n");
	return 0;
}

static long isdbt_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	mutex_lock(&isdbt_cb_ptr->ioctl_lock);
	
	switch (cmd) {
	case IOCTL_ISDBT_POWER_ON:
		ret = isdbt_power_on(arg);
		if (ret)
			ret = isdbt_power_off();
		break;

	case IOCTL_ISDBT_POWER_OFF:
		isdbt_power_off();
		break;

	case IOCTL_ISDBT_SCAN_CHANNEL:
		ret = isdbt_scan_channel(arg);
		break;
            
	case IOCTL_ISDBT_SET_CHANNEL:
		ret = isdbt_set_channel(arg);		
		break;

	case IOCTL_ISDBT_START_TS:
		ret = isdbt_enable_ts_out();
		break;

	case IOCTL_ISDBT_STOP_TS:
		isdbt_disable_ts_out();
		break;			

	case IOCTL_ISDBT_GET_LOCK_STATUS:
		ret = isdbt_get_lock_status(arg);
		break;

	case IOCTL_ISDBT_GET_SIGNAL_INFO:
		ret = isdbt_get_signal_info(arg);
		break;

	case IOCTL_ISDBT_SUSPEND:
		isdbt_enable_standby_mode(arg);
		break;

	case IOCTL_ISDBT_RESUME:
		isdbt_disable_standby_mode(arg);
		break;

	case IOCTL_ISDBT_GET_BER_PER_INFO:
		ret = isdbt_get_ber_per_info(arg);
		break;

	case IOCTL_ISDBT_GET_RSSI:
		ret = isdbt_get_rssi(arg);
		break;

	case IOCTL_ISDBT_GET_CNR:
		ret = isdbt_get_cnr(arg);
		break;

	case IOCTL_ISDBT_GET_SIGNAL_QUAL_INFO:
		ret = isdbt_get_signal_qual_info(arg);
		break;

	/* Test IO command */
	case IOCTL_TEST_GPIO_SET:
	case IOCTL_TEST_GPIO_GET:
		ret = test_gpio(arg, cmd);
		break;

	case IOCTL_TEST_MTV_POWER_ON:	
	case IOCTL_TEST_MTV_POWER_OFF:
		test_power_on_off(cmd);
		break;		

	case IOCTL_TEST_REG_SINGLE_READ:
	case IOCTL_TEST_REG_BURST_READ:
	case IOCTL_TEST_REG_WRITE:
	case IOCTL_TEST_REG_SPI_MEM_READ:
	case IOCTL_TEST_REG_ONLY_SPI_MEM_READ:
		ret = test_register_io(arg, cmd);
		break;
	
	default:
		DMBERR("Invalid ioctl command: 0x%X\n", cmd);
		ret = -ENOIOCTLCMD;
		break;
	}

	mutex_unlock(&isdbt_cb_ptr->ioctl_lock);

	return ret;
}

static int isdbt_close(struct inode *inode, struct file *filp)
{
	DMBMSG("START\n");

	mutex_lock(&isdbt_cb_ptr->ioctl_lock);
	isdbt_power_off();
	mutex_unlock(&isdbt_cb_ptr->ioctl_lock);
	mutex_destroy(&isdbt_cb_ptr->ioctl_lock);

	/* Allow the DMB applicaton to entering suspend. */
	wake_unlock(&isdbt_cb_ptr->wake_lock);

	atomic_set(&isdbt_cb_ptr->open_flag, 0);

	DMBMSG("END\n");

	return 0;
}

static int isdbt_open(struct inode *inode, struct file *filp)
{
	/* Check if the device is already opened ? */
	if (atomic_cmpxchg(&isdbt_cb_ptr->open_flag, 0, 1)) {
		DMBERR("%s driver was already opened!\n", ISDBT_DEV_NAME);
		return -EBUSY;
	}

	isdbt_cb_ptr->is_power_on = FALSE;

	mutex_init(&isdbt_cb_ptr->ioctl_lock);

	/* Prevents the DMB applicaton from entering suspend. */
	wake_lock(&isdbt_cb_ptr->wake_lock);

	DMBMSG("Open OK\n");

	return 0;
}

static struct file_operations isdbt_fops =
{
	.owner = THIS_MODULE,
	.read = isdbt_read,
	.unlocked_ioctl = isdbt_ioctl,
	.release = isdbt_close,
	.open = isdbt_open,
#if defined(NXTV_IF_SPI)
	.mmap = isdbt_mmap
#endif
};

static struct miscdevice isdbt_misc_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = ISDBT_DEV_NAME,
    .fops = &isdbt_fops,
};

static void isdbt_deinit_device(void)
{
	wake_lock_destroy(&isdbt_cb_ptr->wake_lock);

#if defined(NXTV_IF_SPI) // || defined(NXTV_IF_EBI)
	free_irq(isdbt_cb_ptr->irq, NULL);
#endif

#if defined(NXTV_IF_TSIF_0) || defined(NXTV_IF_TSIF_1) || defined(NXTV_IF_SPI_SLAVE)
	#ifndef NXTV_IF_SPI_TSIFx
	i2c_del_driver(&isdbt_i2c_driver);
	#endif
#endif

#if defined(NXTV_IF_SPI) || defined(NXTV_IF_SPI_TSIFx)
	spi_unregister_driver(&isdbt_spi_driver);
#endif

#ifdef _MAKE_TSB_ALLOC_FROM_BOOT
	tsb_free_mapping_area();
#endif

	if (isdbt_cb_ptr) {
		kfree(isdbt_cb_ptr);
		isdbt_cb_ptr = NULL;
	}
}

static int isdbt_init_device(void)
{
	int irq_pin_no;
	int ret = 0;

	isdbt_cb_ptr = kzalloc(sizeof(struct ISDBT_CB), GFP_KERNEL);
	if (!isdbt_cb_ptr)
		return -ENOMEM;

	isdbt_configure_gpio();

#if defined(NXTV_IF_TSIF_0) || defined(NXTV_IF_TSIF_1) || defined(NXTV_IF_SPI_SLAVE)
	#ifndef NXTV_IF_SPI_TSIFx
	if ((ret=i2c_add_driver(&isdbt_i2c_driver)) < 0) {
		DMBERR("NXTV I2C driver register failed\n");
		goto exit_init;
	}
	#endif
#endif

#if defined(NXTV_IF_SPI) || defined(NXTV_IF_SPI_TSIFx)
	if ((ret=spi_register_driver(&isdbt_spi_driver)) < 0) {
		DMBERR("NXTV SPI driver register failed\n");
		goto exit_init;
	}
#endif

#if defined(NXTV_IF_SPI) // || defined(NXTV_IF_EBI)
	#ifndef _ISDBT_DEVICE_TREE_UNUSED
	irq_pin_no = dt_pdata->isdbt_irq;
	#else
	irq_pin_no = ISDBT_IRQ_INT;
	#endif

	isdbt_cb_ptr->irq = gpio_to_irq(irq_pin_no);

	ret = request_irq(isdbt_cb_ptr->irq, isdbt_irq_handler,
	#if defined(NXTV_INTR_POLARITY_LOW_ACTIVE)
					IRQ_TYPE_EDGE_FALLING,
	#elif defined(NXTV_INTR_POLARITY_HIGH_ACTIVE)
					IRQ_TYPE_EDGE_RISING,
	#endif
					ISDBT_DEV_NAME, NULL);
	if (ret != 0) {
		DMBERR("Failed to install irq (%d)\n", ret);
		goto exit_init;
	}
	disable_irq(isdbt_cb_ptr->irq); /* Must disabled */
#endif

	atomic_set(&isdbt_cb_ptr->open_flag, 0);

	wake_lock_init(&isdbt_cb_ptr->wake_lock,
			WAKE_LOCK_SUSPEND, ISDBT_DEV_NAME);

#ifdef _MAKE_TSB_ALLOC_FROM_BOOT
	ret = tsb_alloc_mapping_area(MAX_TSB_DESC_SIZE, MAX_TSB_SEG_SIZE,
									ISDBT_MAX_NUM_TSB_SEG);
	if (ret)
		goto exit_init;
#endif

	return 0;

exit_init:
	isdbt_deinit_device();

	return ret;
}

static const char *build_date = __DATE__;
static const char *build_time = __TIME__;

static int __init isdbt_module_init(void)
{
   	int ret;

	printk(KERN_INFO "\t===============================================\n");
	printk(KERN_INFO "\tBuild with Linux Kernel(%d.%d.%d)\n",
			(LINUX_VERSION_CODE>>16)&0xFF,
			(LINUX_VERSION_CODE>>8)&0xFF,
			LINUX_VERSION_CODE&0xFF);
	printk(KERN_INFO "\t[%s] Module Build Date/Time: %s, %s\n",
				isdbt_misc_device.name, build_date, build_time);
	printk(KERN_INFO "\t===============================================\n");

	ret = isdbt_init_device();
	if (ret < 0)
		return ret;

	/* misc device registration */
	ret = misc_register(&isdbt_misc_device);
	if (ret) {
		DMBERR("misc_register() failed! : %d", ret);
		return ret;
	}

	return 0;
}


static void __exit isdbt_module_exit(void)
{
	isdbt_deinit_device();
	
	misc_deregister(&isdbt_misc_device);
}

module_init(isdbt_module_init);
module_exit(isdbt_module_exit);
MODULE_DESCRIPTION("ISDBT driver");
MODULE_LICENSE("GPL");

