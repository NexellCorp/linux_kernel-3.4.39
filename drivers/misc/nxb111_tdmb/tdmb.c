/*
 * tdmb.c
 *
 * TDMB main driver.
 *
 * Copyright (C) (2011, RAONTECH)
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

#include "mtv319.h" /* Place before tdmb_ioctl.h ! */
#include "mtv319_internal.h" // for reg read/write ?

#include "tdmb.h"
#include "tdmb_debug.h"
#include "tdmb_gpio.h"
#include "tdmb_ioctl.h"
#include "tdmb_ioctl_func.h"

#ifdef RTV_MULTIPLE_CHANNEL_MODE
	#ifndef RTV_MCHDEC_IN_DRIVER
		#error "Must define RTV_MCHDEC_IN_DRIVER"
	#endif
	#include "mtv319_cifdec.h"
#endif

#ifdef _TDMB_KERNEL_FILE_DUMP_ENABLE
	struct file *tdmb_msc_dump_filp = NULL;
	struct file *tdmb_fic_dump_filp = NULL;
#endif


struct tdmb_cb *tdmb_cb_ptr;

#if defined(CONFIG_TDMB_SPI) || defined(CONFIG_TDMB_EBI2) || defined(RTV_FIC_I2C_INTR_ENABLED)
extern irqreturn_t tdmb_irq_handler(int irq, void *param);
//extern void tdmb_isr_handler(struct work_struct *work);
#endif

#if defined(CONFIG_TDMB_SPI) || defined(CONFIG_TDMB_EBI2)

#ifdef RTV_MULTIPLE_CHANNEL_MODE
#ifndef RTV_MCHDEC_DIRECT_COPY_USER_SPACE
/* Free the MSC decoding-buffer if the multi service was enabled. */
static void delete_msc_decoding_buffer(void)
{
	int i;

	for (i = 0; i < RTV_MAX_NUM_USE_SUBCHANNEL; i++) {
		if (tdmb_cb_ptr->dec_msc_kbuf_ptr[i]) {
			kfree(tdmb_cb_ptr->dec_msc_kbuf_ptr[i]);
			tdmb_cb_ptr->dec_msc_kbuf_ptr[i] = NULL;
		}
	}
}

/* Allocate the MSC decoding-buffer if the multi service was enabled. */
static int create_msc_decoding_buffer(void)
{
	int i;

	for (i = 0; i < RTV_MAX_NUM_USE_SUBCHANNEL; i++) {
		tdmb_cb_ptr->dec_msc_kbuf_ptr[i]
			= kmalloc(MAX_MULTI_MSC_BUF_SIZE, GFP_KERNEL);
		if (tdmb_cb_ptr->dec_msc_kbuf_ptr[i] == NULL) {
			DMBERR("CIF buffer(%d) alloc error!\n", i);
			return -ENOMEM;
		}
	}

	return 0;
}
#endif /* #ifndef RTV_MCHDEC_DIRECT_COPY_USER_SPACE */

/* for (1 AVD service and FIC) or (2 more AVD services ) */
static inline ssize_t read_multiple_service(char *buf)
{		
	ssize_t ret = 0;
	struct RTV_CIF_DEC_INFO cifdec;
	TDMB_TSPB_INFO *tspb = NULL;
	UINT i, dec_idx, dec_msc_buf_size, max_dec_size = 0, total_msc_size = 0;
	IOCTL_TDMB_MULTI_SVC_BUF __user *m = (IOCTL_TDMB_MULTI_SVC_BUF __user *)buf;
	UINT subch_size_tbl[RTV_MAX_NUM_USE_SUBCHANNEL];
	UINT subch_id_tbl[RTV_MAX_NUM_USE_SUBCHANNEL];
	BOOL dec_data_copied = FALSE;
	UINT loop = 0; /* Debug */

#ifdef RTV_SPI_FIC_DECODE_IN_PLAY
	UINT fic_size = 0;
#endif

#ifndef RTV_MCHDEC_DIRECT_COPY_USER_SPACE
	/* decoded data => kernel buffer => user buffer */
	U8 *ubuf_ptr; /* Buffer pointer of the user space. */

	#ifdef RTV_SPI_FIC_DECODE_IN_PLAY
	U8 dec_fic_kbuf[384];
	cifdec.fic_buf_ptr = dec_fic_kbuf; /* kernel buffer */
	#else
	cifdec.fic_buf_ptr = NULL;
	#endif
#else
	/* decoded data => user buffer */
	cifdec.fic_buf_ptr = (U8 __user *)m->fic_buf; /* user buffer */
#endif

	if (tdmb_cb_ptr->f_flags & O_NONBLOCK) { /* Non-blocking mode. */
		if (tdmb_tspb_queue_count() == 0)
			return -EAGAIN;
	}
	else { /* Blocking mode. */
		if (wait_event_interruptible(tdmb_cb_ptr->read_wq,
			tdmb_tspb_queue_count()	|| (tdmb_cb_ptr->read_stop == TRUE))) {
			DMBMSG("Woken up by signal.\n");
			return -ERESTARTSYS;
		}
	}

	/* Reset the return inforamtion for each sub channels. */
	dec_msc_buf_size = MAX_MULTI_MSC_BUF_SIZE;
	for (i = 0; i < RTV_MAX_NUM_USE_SUBCHANNEL; i++) {
		subch_id_tbl[i] = 0xFF; /* Default Invalid ID. */
		subch_size_tbl[i] = 0;
		if (put_user(subch_size_tbl[i], &m->msc_size[i])) {
			ret = -EFAULT;
			goto exit_read;
		}

		cifdec.subch_buf_size[i] = MAX_MULTI_MSC_BUF_SIZE;

		/* Set the address of MSC buffer to be decoded. */
	#ifndef RTV_MCHDEC_DIRECT_COPY_USER_SPACE
		cifdec.subch_buf_ptr[i] = tdmb_cb_ptr->dec_msc_kbuf_ptr[i]; /* kernel buffer */
	#else
		cifdec.subch_buf_ptr[i] = (U8 __user *)m->msc_buf[i]; /* user buffer */
	#endif
	}

#ifdef RTV_SPI_FIC_DECODE_IN_PLAY
	/* Copy the init value into user buffer */
	if (put_user(fic_size, &m->fic_size)) {
		ret = -EFAULT;
		goto exit_read;
	}
#endif

	/* Acquire the read-mutex. */
	mutex_lock(&tdmb_cb_ptr->read_lock);

	while (1) {
		if (tdmb_cb_ptr->read_stop == TRUE) {
			ret = -EINTR; /* Force stopped. */
			goto unlock_read;
		}

		/* Check if the size of MSC decoding-buffer is ok ? */
		if (atomic_read(&tdmb_cb_ptr->num_opened_subch)) { /* subch opened */
			if ((tspb = tdmb_tspb_peek()) != NULL) {

				//DMBMSG("tspb->size(%u), dec_msc_buf_size(%u)\n", tspb->size, dec_msc_buf_size);

				if (tspb->size > dec_msc_buf_size) {
					//DMBERR("(%u) Exit for the small decoding buffer size\n", loop);
					goto unlock_read;
				}
			}
			else
				goto unlock_read;
		}

		/* Dequeue a tspb from tsp_queue. */
		if ((tspb = tdmb_tspb_dequeue()) == NULL)
			goto unlock_read; /* Stop. */

		/* Can be cleared by tdmb_tspb_queue_clear_contents()*/
		if (tspb->size == 0) {
			DMBMSG("Zero tsp size! tspb(0x%p)\n", tspb);
			goto skip_tsp;
		}

		rtvCIFDEC_Decode(&cifdec, tspb->buf, tspb->size);

		/* MSC */
		if (atomic_read(&tdmb_cb_ptr->num_opened_subch) == 0)
			goto skip_msc_copy; /* Case for full-scan. */
		
		for (i = 0; i < RTV_MAX_NUM_USE_SUBCHANNEL; i++) {
			if (cifdec.subch_size[i]) {
				/* Get the index of decoded output buffer. */
				dec_idx = rtvCIFDEC_GetDecBufIndex(cifdec.subch_id[i]);

				/* Check if subch ID was closed in the run-time ? */
				if (dec_idx == RTV_CIFDEC_INVALID_BUF_IDX) {
					DMBMSG("Closed ID(%u)\n", cifdec.subch_id[i]);
					continue;
				}

				if (subch_id_tbl[dec_idx] == 0xFF) /* Default Invalid. */
					subch_id_tbl[dec_idx] = cifdec.subch_id[i];

				/* Check if subch ID was changed in the same buf index ? */
				if (subch_id_tbl[dec_idx] != cifdec.subch_id[i]) {
					DMBMSG("Changed ID: from old(%u) to new(%u)\n",
						subch_size_tbl[dec_idx], cifdec.subch_id[i]);
					continue;
				}

		#ifndef RTV_MCHDEC_DIRECT_COPY_USER_SPACE
				if (tdmb_cb_ptr->read_stop == TRUE) {
					ret = -EINTR; /* Force stopped. */
					goto free_out_tspb;
				}

				/* Copy the decoded data into user-space. */
				ubuf_ptr = m->msc_buf[dec_idx] + subch_size_tbl[dec_idx];
				if (copy_to_user(ubuf_ptr, cifdec.subch_buf_ptr[i],
						cifdec.subch_size[i])) {
					ret = -EFAULT;
					goto free_out_tspb;
				}
		#endif
				subch_size_tbl[dec_idx] += cifdec.subch_size[i];
				cifdec.subch_buf_ptr[i] += cifdec.subch_size[i]; /* Advance */

				max_dec_size = MAX(max_dec_size, cifdec.subch_size[i]);
				total_msc_size += cifdec.subch_size[i];
				dec_data_copied = TRUE;
			}
		}

		if (dec_data_copied == TRUE) { /* Decoding data copied ? */
			dec_data_copied = FALSE; /* Reset */
			dec_msc_buf_size -= max_dec_size;
			max_dec_size = 0; /* Reset */
		}

skip_msc_copy:
		/* FIC */
#ifdef RTV_SPI_FIC_DECODE_IN_PLAY
		if (cifdec.fic_size) {
			fic_size = cifdec.fic_size;
		#ifndef RTV_MCHDEC_DIRECT_COPY_USER_SPACE
			if (copy_to_user(m->fic_buf, cifdec.fic_buf_ptr, fic_size)) {
				ret = -EFAULT;
				goto free_out_tspb;
			}
		#endif
			goto free_out_tspb; /* Force to stop for FIC parsing in time. */
		}
#endif

skip_tsp:
		tdmb_tspb_free_buffer(tspb);
		tspb = NULL;
		loop++;
	}

free_out_tspb:
	tdmb_tspb_free_buffer(tspb); /* for "goto unlock_read;" statement. */

unlock_read:
	/* Release the read-mutex. */
	mutex_unlock(&tdmb_cb_ptr->read_lock);

	if (ret == 0) {
		if (total_msc_size) {
			for (i = 0; i < RTV_MAX_NUM_USE_SUBCHANNEL; i++) {
				if (subch_size_tbl[i]) {
					if (put_user(subch_size_tbl[i], &m->msc_size[i])) {
						ret = -EFAULT;
						goto exit_read;
					}
					
					if (put_user(subch_id_tbl[i], &m->msc_subch_id[i])) {
						ret = -EFAULT;
						goto exit_read;
					}
					ret += subch_size_tbl[i];
				}
			}
		}

#ifdef RTV_SPI_FIC_DECODE_IN_PLAY
		if (fic_size) {
			if (put_user(fic_size, &m->fic_size)) {
				ret = -EFAULT;
				goto exit_read;
			}
			ret += fic_size;
		}
#endif		
	}

exit_read:
	if ((tdmb_cb_ptr->f_flags & O_NONBLOCK) == 0) {
		if (tdmb_tspb_queue_count()) /* for next time */
			wake_up_interruptible(&tdmb_cb_ptr->read_wq);
	}

	return ret;
}
#else
/* In case of 1 av or 1 data. */
static inline ssize_t read_single_service(char *buf, size_t count)
{
	ssize_t ret = 0;
	int copy_bytes; /* # of bytes to be copied. */
	TDMB_TSPB_INFO *tspb = NULL;
	unsigned int req_read_size = MIN(count, 32 * 188);
	static U8 *msc_buf_ptr; /* Be used in the next read time. */

	if (req_read_size == 0)
		return 0;

	if (tdmb_cb_ptr->f_flags & O_NONBLOCK) { /* non-blocking mode. */
		unsigned int have_tsp_bytes
			= tdmb_tspb_queue_count() * tdmb_cb_ptr->intr_ts_size;

		if (have_tsp_bytes < req_read_size)
			return -EAGAIN;
	}
	else { /* Blocking mode. */
		if (wait_event_interruptible(tdmb_cb_ptr->read_wq,
			tdmb_tspb_queue_count() || (tdmb_cb_ptr->read_stop == TRUE))) {
			DMBMSG("Woken up by signal.\n");
			return -ERESTARTSYS;
		}
	}

	/* Acquire the read-mutex. */
	mutex_lock(&tdmb_cb_ptr->read_lock);

	do {
		if (tdmb_cb_ptr->read_stop == TRUE) {
			ret = -EINTR; /* no data was transferred */
			goto free_prev_tsp;
		}

		if (tdmb_cb_ptr->prev_tspb == NULL) {
			/* Dequeue a tspb from tsp_queue. */
			if ((tspb = tdmb_tspb_dequeue()) == NULL)
				goto unlock_read; /* Stop. */

			tdmb_cb_ptr->prev_tspb = tspb;
			msc_buf_ptr = tspb->buf;
		}
		else
			tspb = tdmb_cb_ptr->prev_tspb;

		copy_bytes = MIN(tspb->size, req_read_size);
		if (copy_to_user(buf, msc_buf_ptr, copy_bytes)) {
			ret = -EFAULT;
			goto free_prev_tsp;
		}

		msc_buf_ptr += copy_bytes;
		buf += copy_bytes;
		ret += copy_bytes; /* Total return size */
		req_read_size -= copy_bytes;
		tspb->size -= copy_bytes;

		if (tspb->size == 0) {
			tdmb_tspb_free_buffer(tspb);
			tdmb_cb_ptr->prev_tspb = NULL; /* All used. */
		}
	} while (req_read_size != 0);

free_prev_tsp:
	if (tdmb_cb_ptr->prev_tspb) {
		tdmb_tspb_free_buffer(tdmb_cb_ptr->prev_tspb);
		tdmb_cb_ptr->prev_tspb = NULL; 
	}

unlock_read:
	mutex_unlock(&tdmb_cb_ptr->read_lock);

	if ((tdmb_cb_ptr->f_flags & O_NONBLOCK) == 0) {
		if (tdmb_tspb_queue_count()) /* for next time */
			wake_up_interruptible(&tdmb_cb_ptr->read_wq);
	}

	return ret;
}
#endif /* #ifdef RTV_MULTIPLE_CHANNEL_MODE */

static int tdmb_control_irq(bool set)
{
	int ret = 0;

#if defined(CONFIG_TDMB_SPI) || defined(CONFIG_TDMB_EBI2)\
	|| defined(RTV_FIC_I2C_INTR_ENABLED)
	if (set) {
		tdmb_cb_ptr->irq = gpio_to_irq(TDMB_IRQ_INT);

		ret = request_threaded_irq(tdmb_cb_ptr->irq, NULL, tdmb_irq_handler,
	#if defined(RTV_INTR_POLARITY_LOW_ACTIVE)
							IRQ_TYPE_EDGE_FALLING,
	#elif defined(RTV_INTR_POLARITY_HIGH_ACTIVE)
							IRQ_TYPE_EDGE_RISING,
	#endif
							TDMB_DEV_NAME, NULL);
		if (ret != 0) {
			DMBERR("Failed to install irq (%d)\n", ret);
			return ret;
		}
		//disable_irq(tdmb_cb_ptr->irq); /* Must disabled */
	} else {
			free_irq(tdmb_cb_ptr->irq, NULL);
	}
#endif

	return ret;
}

//// temp
extern unsigned int total_int_cnt;

static ssize_t tdmb_read(struct file *filp, char *buf, size_t count, loff_t *pos)
{
	int ret;

	/* Check if the device is already readed ? */
	if (atomic_cmpxchg(&tdmb_cb_ptr->read_flag, 0, 1)) {
		DMBERR("%s is already readed.\n", TDMB_DEV_NAME);
		return -EBUSY;
	}

#ifdef RTV_MULTIPLE_CHANNEL_MODE
	ret = read_multiple_service(buf);
#else
	ret = read_single_service(buf, count);
#endif

	atomic_set(&tdmb_cb_ptr->read_flag, 0);

	return ret;
}
#endif /* #if defined(CONFIG_TDMB_SPI) || defined(CONFIG_TDMB_EBI2) */

static void tdmb_power_off(void)
{
	if (tdmb_cb_ptr->is_power_on == FALSE)
		return;

	tdmb_cb_ptr->is_power_on = FALSE;

	DMBMSG("START\n");

	tdmb_deinit();

#if defined(CONFIG_TDMB_SPI) || defined(CONFIG_TDMB_EBI2) || defined(RTV_FIC_I2C_INTR_ENABLED)
	tdmb_control_irq(false);

	tdmb_cb_ptr->irq_thread_sched_changed = false;

	#if 0
	disable_irq(tdmb_cb_ptr->irq);

	/* Delete the ISR workqueue. */
	if (tdmb_cb_ptr->isr_workqueue) {
		cancel_work_sync(&tdmb_cb_ptr->tdmb_isr_work);
		flush_workqueue(tdmb_cb_ptr->isr_workqueue);	
		destroy_workqueue(tdmb_cb_ptr->isr_workqueue);
		tdmb_cb_ptr->isr_workqueue = NULL;
	}
	#endif
#endif

	rtvOEM_PowerOn(0);

#if defined(CONFIG_TDMB_SPI) || defined(CONFIG_TDMB_EBI2)
	tdmb_tspb_queue_reset();

	/* Wake up threads blocked on read() to finish read operation. */
	if ((tdmb_cb_ptr->f_flags & O_NONBLOCK) == 0)
		wake_up_interruptible(&tdmb_cb_ptr->read_wq);
#endif

	DMBMSG("END\n");
}

static int tdmb_power_on(unsigned long arg)
{
	int ret = 0;

	if (tdmb_cb_ptr->is_power_on == TRUE)	
		return 0;

	DMBMSG("Start\n");

	rtvOEM_PowerOn(1);
	tdmb_cb_ptr->is_power_on = TRUE;

	ret = tdmb_init(arg);
	if (ret != 0) {
		return ret;
	}

#if defined(CONFIG_TDMB_SPI) || defined(CONFIG_TDMB_EBI2) || defined(CONFIG_TDMB_EBI2)
	/* Allow thread to block on read() if the blocking mode. */
	tdmb_cb_ptr->read_stop = FALSE;
	tdmb_cb_ptr->prev_tspb = NULL;
#endif

#if defined(CONFIG_TDMB_SPI) || defined(CONFIG_TDMB_EBI2) || defined(RTV_FIC_I2C_INTR_ENABLED)

	#if 0
	INIT_WORK(&tdmb_cb_ptr->tdmb_isr_work, tdmb_isr_handler);
	tdmb_cb_ptr->isr_workqueue = create_singlethread_workqueue(TDMB_DEV_NAME);
	if (tdmb_cb_ptr->isr_workqueue == NULL) {
		DMBERR("Couldn't create workqueue\n");
		return -ENOMEM;
	}
	#endif

	tdmb_cb_ptr->irq_thread_sched_changed = false;
	tdmb_control_irq(true);
//	enable_irq(tdmb_cb_ptr->irq); /* After DMB init */
#endif

	DMBMSG("End\n");

	return ret;
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
static long tdmb_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
#else
static int tdmb_ioctl(struct inode *inode, struct file *filp,
			unsigned int cmd, unsigned long arg)
#endif
{
	int ret = 0;

	mutex_lock(&tdmb_cb_ptr->ioctl_lock);
	
	switch (cmd) {
	case IOCTL_TDMB_POWER_ON:
		ret = tdmb_power_on(arg);
		if (ret)
			tdmb_power_off();
		break;

	case IOCTL_TDMB_POWER_OFF:
		tdmb_power_off();
		break;

	case IOCTL_TDMB_SCAN_FREQ:
		ret = tdmb_scan_freq(arg);
		break;

	case IOCTL_TDMB_OPEN_FIC:
		ret = tdmb_open_fic(arg);
		break;
            
	case IOCTL_TDMB_CLOSE_FIC:
		tdmb_close_fic();
		break;

	case IOCTL_TDMB_READ_FIC:
		ret = tdmb_read_fic(arg);
		break;
            
	case IOCTL_TDMB_OPEN_SUBCHANNEL:
		ret = tdmb_open_subchannel(arg);		
		break;

	case IOCTL_TDMB_CLOSE_SUBCHANNEL: {
		IOCTL_CLOSE_SUBCHANNEL_INFO __user *argp = (IOCTL_CLOSE_SUBCHANNEL_INFO __user *)arg;
		unsigned int subch_id;

		get_user(subch_id, &argp->subch_id);
		ret = tdmb_close_subchannel(subch_id);
	}
		break;

	case IOCTL_TDMB_CLOSE_ALL_SUBCHANNELS:
		tdmb_close_all_subchannels();
		break;

	case IOCTL_TDMB_GET_LOCK_STATUS:
		ret = tdmb_get_lock_status(arg);
		break;

	case IOCTL_TDMB_GET_SIGNAL_INFO:
		ret = tdmb_get_signal_info(arg);
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
		DMBERR("Invalid ioctl command: %d\n", cmd);
		ret = -ENOIOCTLCMD;
		break;
	}

	mutex_unlock(&tdmb_cb_ptr->ioctl_lock);

	return ret;
}


#if defined(RTV_FIC_I2C_INTR_ENABLED)
static int tdmb_sigio_fasync(int fd, struct file *filp, int mode)
{
	return fasync_helper(fd, filp, mode, &tdmb_cb_ptr->fasync);
}
#endif

static int tdmb_close(struct inode *inode, struct file *filp)
{
	DMBMSG("START\n");

	mutex_lock(&tdmb_cb_ptr->ioctl_lock);
	tdmb_power_off();
	mutex_unlock(&tdmb_cb_ptr->ioctl_lock);
	mutex_destroy(&tdmb_cb_ptr->ioctl_lock);

#if defined(CONFIG_TDMB_SPI) || defined(CONFIG_TDMB_EBI2)
	tdmb_tspb_delete_pool();

	/* Free the MSC decoding-buffer if the multi service was enabled. */
	#if defined(RTV_MULTIPLE_CHANNEL_MODE) && !defined(RTV_MCHDEC_DIRECT_COPY_USER_SPACE)
	delete_msc_decoding_buffer();
	#endif

	mutex_destroy(&tdmb_cb_ptr->read_lock);
#endif

#if defined(RTV_FIC_I2C_INTR_ENABLED)
	fasync_helper(-1, filp, 0, &tdmb_cb_ptr->fasync);
#endif

	/* Allow the DMB applicaton to entering suspend. */
	wake_unlock(&tdmb_cb_ptr->wake_lock);

	atomic_set(&tdmb_cb_ptr->open_flag, 0);

	DMBMSG("END\n");

	return 0;
}

static int tdmb_open(struct inode *inode, struct file *filp)
{
	int ret = 0;

	/* Check if the device is already opened ? */
	if (atomic_cmpxchg(&tdmb_cb_ptr->open_flag, 0, 1)) {
		DMBERR("%s is already opened\n", TDMB_DEV_NAME);
		return -EBUSY;
	}

	tdmb_cb_ptr->is_power_on = FALSE;

#if defined(CONFIG_TDMB_SPI) || defined(CONFIG_TDMB_EBI2)
	ret = tdmb_tspb_create_pool();
	if (ret)
		return ret;

	/* Allocate the MSC decoding-buffer if the multi service was enabled. */
	#if defined(RTV_MULTIPLE_CHANNEL_MODE) && !defined(RTV_MCHDEC_DIRECT_COPY_USER_SPACE)
	if ((ret = create_msc_decoding_buffer()))
		return ret;
	#endif

	if ((filp->f_flags & O_NONBLOCK) == 0) { /* Blocking mode */
		init_waitqueue_head(&tdmb_cb_ptr->read_wq);
		DMBMSG("Opened as the Blocking mode\n");
	}
	else
		DMBMSG("Opened as the Non-blocking mode\n");

	/* Save the flag of file. */
	tdmb_cb_ptr->f_flags = filp->f_flags;

	/* Allow thread to block on read() if the blocking mode. */
	tdmb_cb_ptr->read_stop = FALSE;
	mutex_init(&tdmb_cb_ptr->read_lock);
#endif

	mutex_init(&tdmb_cb_ptr->ioctl_lock);

	/* Prevents the DMB applicaton from entering suspend. */
	wake_lock(&tdmb_cb_ptr->wake_lock);

	return ret;
}


static struct file_operations mtv_fops =
{
    .owner = THIS_MODULE,

#if defined(CONFIG_TDMB_SPI) || defined(CONFIG_TDMB_EBI2)
    .read = tdmb_read,
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
	.unlocked_ioctl = tdmb_ioctl,
#else
	.ioctl = tdmb_ioctl,    
#endif

#if defined(RTV_FIC_I2C_INTR_ENABLED)
	.fasync = tdmb_sigio_fasync,
#endif
	.release = tdmb_close,
	.open = tdmb_open
};

static struct miscdevice tdmb_misc_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = TDMB_DEV_NAME,
    .fops = &mtv_fops,
};

static void tdmb_deinit_device(void)
{
	wake_lock_destroy(&tdmb_cb_ptr->wake_lock);

#if defined(CONFIG_TDMB_TSIF)
	tdmb_deinit_i2c_bus();
#elif defined(CONFIG_TDMB_SPI)
	tdmb_deinit_spi_bus();
#elif defined(CONFIG_TDMB_EBI2)
	tdmb_deinit_ebi_bus();
#endif

	if (tdmb_cb_ptr) {
		kfree(tdmb_cb_ptr);
		tdmb_cb_ptr = NULL;
	}

	DMBMSG("END\n");
}

static int tdmb_init_device(void)
{
	int ret = 0;

	tdmb_cb_ptr = kzalloc(sizeof(struct tdmb_cb), GFP_KERNEL);
	if (!tdmb_cb_ptr)
		return -ENOMEM;

	mtv_configure_gpio();

	/* Initialize interface BUS */
#if defined(CONFIG_TDMB_TSIF)
	ret = tdmb_init_i2c_bus();
#elif defined(CONFIG_TDMB_SPI)
	ret = tdmb_init_spi_bus();
#elif defined(CONFIG_TDMB_EBI2)
	ret = tdmb_init_ebi_bus();
#endif
	if (ret < 0)
		goto init_err;

#if defined(CONFIG_TDMB_SPI) || defined(CONFIG_TDMB_EBI2)
	atomic_set(&tdmb_cb_ptr->read_flag, 0);
#endif

	atomic_set(&tdmb_cb_ptr->open_flag, 0);
	wake_lock_init(&tdmb_cb_ptr->wake_lock, WAKE_LOCK_SUSPEND, TDMB_DEV_NAME);

	return 0;

init_err:
	tdmb_deinit_device();

	return ret;
}

static const char *build_date = __DATE__;
static const char *build_time = __TIME__;

static int __init tdmb_module_init(void)
{
   	int ret;

	printk(KERN_INFO "\t==================================================\n");
	printk(KERN_INFO "\tBuild with Linux Kernel(%d.%d.%d)\n",
			(LINUX_VERSION_CODE>>16)&0xFF,
			(LINUX_VERSION_CODE>>8)&0xFF,
			LINUX_VERSION_CODE&0xFF);
	printk(KERN_INFO "\t %s Module Build Date/Time: %s, %s\n",
				tdmb_misc_device.name, build_date, build_time);
	printk(KERN_INFO "\t %s: %s\n",
				__func__, __FILE__);
	printk(KERN_INFO "\t==================================================\n\n");

	ret = tdmb_init_device();
	if (ret < 0)
		return ret;

	/* misc device registration */
	ret = misc_register(&tdmb_misc_device);
	if (ret) {
		DMBERR("misc_register() failed! : %d", ret);
		return ret;
	}

	return 0;
}


static void __exit tdmb_module_exit(void)
{
	tdmb_deinit_device();
	
	misc_deregister(&tdmb_misc_device);
}

module_init(tdmb_module_init);
module_exit(tdmb_module_exit);
MODULE_DESCRIPTION("TDMB driver");
MODULE_LICENSE("GPL");

