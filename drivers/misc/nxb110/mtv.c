/*
 * mtv.c
 *
 * NXB110 MTV main driver.
 *
 * Copyright (C) (2015, NEXELL)
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

#include "mtv.h"
#include "mtv_gpio.h"
  
#include "./src/nxb110tv_internal.h"
#include "mtv_ioctl_func.h"

#ifdef RTV_CIF_MODE_ENABLED
	#ifdef RTV_BUILD_CIFDEC_WITH_DRIVER
		#include "./src/nxb110tv_cif_dec.h"
	#endif
#endif


struct mtv_cb *mtv_cb_ptr;

#if defined(RTV_IF_SPI) || defined(RTV_FIC_I2C_INTR_ENABLED)
extern irqreturn_t mtv_isr_av(int irq, void *param);

#if 1
extern int mtv_isr_thread_av(void *data);

#else
extern void mtv_isr_handler(struct work_struct *work);////////////
#endif
#endif

#if defined(RTV_IF_SPI) /* Use memory pool to store DMB data. */
static MTV_TSP_QUEUE_INFO mtv_tsp_pool;
static MTV_TSP_QUEUE_INFO mtv_tsp_queue;

unsigned int get_tsp_queue_count(int demod_no)
{
	return mtv_tsp_queue.cnt;
}

/* This function should called after the stream stopped. */
static void mtv_reset_tsp_queue(struct mtv_cb *mtv_cb_ptr)
{
	MTV_TS_PKT_INFO *tsp;

	/* Set the flag to stop the operation of reading. */
	mtv_cb_ptr->read_stop = TRUE;

	/* Acquire the mutex. */
	mutex_lock(&mtv_cb_ptr->read_lock);

	if (mtv_tsp_pool.cnt != MAX_NUM_TS_PKT_BUF) {
		/* Free a previous TSP use in read(). */
		if (mtv_cb_ptr->prev_tsp) {
			mtv_free_tsp(mtv_cb_ptr->demod_no, mtv_cb_ptr->prev_tsp);
			mtv_cb_ptr->prev_tsp = NULL; 
		}

		while ((tsp=mtv_get_tsp(mtv_cb_ptr->demod_no)) != NULL) 
			mtv_free_tsp(mtv_cb_ptr->demod_no, tsp);

		if (mtv_tsp_pool.cnt != MAX_NUM_TS_PKT_BUF)
			WARN(1, KERN_ERR "[mtv_reset_tsp_queue] Abnormal cnt! (%u/%u)\n",
						mtv_tsp_pool.cnt, MAX_NUM_TS_PKT_BUF);
	}

	mutex_unlock(&mtv_cb_ptr->read_lock);

#ifdef DEBUG_MTV_IF_MEMORY
	DMBMSG("Max alloc #tsp: %u\n", mtv_cb_ptr->max_alloc_tsp_cnt);
#endif
}

/* Peek a ts packet from ts data queue. */
MTV_TS_PKT_INFO * mtv_peek_tsp(int demod_no)
{
	MTV_TS_PKT_INFO *tsp = NULL;
	struct list_head *head_ptr = &mtv_tsp_queue.head;

	spin_lock(&mtv_tsp_queue.lock);
	
	if (!list_empty(head_ptr))
		tsp = list_first_entry(head_ptr, MTV_TS_PKT_INFO, link);

	spin_unlock(&mtv_tsp_queue.lock);

	return tsp;
}

/* Dequeue a ts packet from ts data queue. */
MTV_TS_PKT_INFO * mtv_get_tsp(int demod_no)
{
	MTV_TS_PKT_INFO *tsp = NULL;
	struct list_head *head_ptr = &mtv_tsp_queue.head;

	spin_lock(&mtv_tsp_queue.lock);
	
	if (!list_empty(head_ptr)) {
		tsp = list_first_entry(head_ptr, MTV_TS_PKT_INFO, link);
		list_del(&tsp->link);
		mtv_tsp_queue.cnt--;
	}

	spin_unlock(&mtv_tsp_queue.lock);

	return tsp;
}

/* Enqueue a ts packet into ts data queue. */
void mtv_put_tsp(int demod_no, MTV_TS_PKT_INFO *tsp)
{
	spin_lock(&mtv_tsp_queue.lock);

	list_add_tail(&tsp->link, &mtv_tsp_queue.head);
	mtv_tsp_queue.cnt++;

	spin_unlock(&mtv_tsp_queue.lock);
}

void mtv_free_tsp(int demod_no, MTV_TS_PKT_INFO *tsp)
{	
	if (tsp == NULL)
		return;

	spin_lock(&mtv_tsp_pool.lock);

#ifdef RTV_SPI_MSC1_ENABLED
	tsp->msc1_size = 0;
#endif

#ifdef RTV_SPI_MSC0_ENABLED
	tsp->msc0_size = 0;
#endif

#ifdef RTV_FIC_SPI_INTR_ENABLED
	tsp->fic_size = 0;
#endif

	list_add_tail(&tsp->link, &mtv_tsp_pool.head);
	mtv_tsp_pool.cnt++;

	spin_unlock(&mtv_tsp_pool.lock);
}


MTV_TS_PKT_INFO *mtv_alloc_tsp(struct mtv_cb *mtv_cb_ptr)
{	
	MTV_TS_PKT_INFO *tsp = NULL;
	struct list_head *head_ptr = &mtv_tsp_pool.head;

	spin_lock(&mtv_tsp_pool.lock);

	if (!list_empty(head_ptr)) {
		tsp = list_first_entry(head_ptr, MTV_TS_PKT_INFO, link);
		list_del(&tsp->link);
		mtv_tsp_pool.cnt--;

#ifdef DEBUG_MTV_IF_MEMORY
		mtv_cb_ptr->max_alloc_tsp_cnt 
			= MAX(mtv_cb_ptr->max_alloc_tsp_cnt,
				MAX_NUM_TS_PKT_BUF - mtv_tsp_pool.cnt);
#endif	
	}

	spin_unlock(&mtv_tsp_pool.lock);

	return tsp;
}

static int mtv_delete_tsp_pool(struct mtv_cb *mtv_cb_ptr)
{
	struct list_head *head_ptr = &mtv_tsp_pool.head;
	MTV_TS_PKT_INFO *tsp;

	while ((tsp = mtv_get_tsp(mtv_cb_ptr->demod_no)) != NULL)
		kfree(tsp);

	while (!list_empty(head_ptr)) {
		tsp = list_entry(head_ptr->next, MTV_TS_PKT_INFO, link);
		list_del(&tsp->link);
		kfree(tsp);
	}

	return 0;
}

static int mtv_create_tsp_pool(struct mtv_cb *mtv_cb_ptr)
{
	unsigned int i;
	MTV_TS_PKT_INFO *tsp;

	/* TSP queue */
	mtv_cb_ptr->prev_tsp = NULL;
	spin_lock_init(&mtv_tsp_queue.lock);
	INIT_LIST_HEAD(&mtv_tsp_queue.head);
	mtv_tsp_queue.cnt = 0;
	
	spin_lock_init(&mtv_tsp_pool.lock);
	INIT_LIST_HEAD(&mtv_tsp_pool.head);

	mtv_tsp_pool.cnt = 0;
#ifdef DEBUG_MTV_IF_MEMORY
	mtv_cb_ptr->max_alloc_tsp_cnt = 0;
#endif

	for (i = 0; i < MAX_NUM_TS_PKT_BUF; i++) {
		tsp = (MTV_TS_PKT_INFO *)kmalloc(sizeof(MTV_TS_PKT_INFO), GFP_DMA);
		if (tsp == NULL) {
			WARN(1, KERN_ERR "[mtv_create_tsp_pool] %d TSP failed!\n", i);
			mtv_delete_tsp_pool(mtv_cb_ptr);
			return -ENOMEM;
		}

#ifdef RTV_SPI_MSC1_ENABLED
		tsp->msc1_size = 0;
#endif

#ifdef RTV_SPI_MSC0_ENABLED
		tsp->msc0_size = 0; 
#endif

#ifdef RTV_FIC_SPI_INTR_ENABLED
		tsp->fic_size = 0;
#endif

		list_add_tail(&tsp->link, &mtv_tsp_pool.head);
		mtv_tsp_pool.cnt++;
	}

 	return 0;
}

#ifdef RTV_MULTI_SERVICE_MODE
	#ifdef RTV_SPI_MSC1_ENABLED
		#define MSC0_BASE_IDX	1 /* MSC1(Video/Audio) is 0 */
	#else
		#define MSC0_BASE_IDX	0
	#endif

#if defined(RTV_DAB_ENABLE) || defined(RTV_TDMB_ENABLE)
/* fic_msc_type: 0(FIC), 1(MSC): Only for multiple-service mode. */
void mtv_clear_contents_only_in_tsp_queue(struct mtv_cb *mtv_cb_ptr, int fic_msc_type)
{
	MTV_TS_PKT_INFO *tsp = NULL;
	struct list_head *pos;

	/* Acquire the read-mutex. */
	mutex_lock(&mtv_cb_ptr->read_lock);

	spin_lock(&mtv_tsp_queue.lock); /* for IRQ */

	list_for_each (pos, &mtv_tsp_queue.head) { /* Lookup tsq Q */
		tsp = list_entry(pos, MTV_TS_PKT_INFO, link);
		if(fic_msc_type == 1) {
#ifdef RTV_SPI_MSC1_ENABLED
			tsp->msc1_size = 0; /* Clear */
#endif

#ifdef RTV_SPI_MSC0_ENABLED
			tsp->msc0_size = 0; /* Clear */
#endif
		}
		else {
#ifdef RTV_FIC_SPI_INTR_ENABLED
			tsp->fic_size = 0; /* Clear */
#endif
		}
	}

	spin_unlock(&mtv_tsp_queue.lock);

	mutex_unlock(&mtv_cb_ptr->read_lock);
}

#if defined(RTV_CIF_MODE_ENABLED) && defined(RTV_BUILD_CIFDEC_WITH_DRIVER)
/* Free the MSC decoding-buffer if the multi service was enabled. */
static void delete_msc_decoding_buffer(struct mtv_cb * mtv_cb_ptr)
{
	int i;

	for (i = 0; i < RTV_MAX_NUM_DAB_DATA_SVC; i++) {
		if (mtv_cb_ptr->dec_msc_kbuf_ptr[i]) {
			vfree(mtv_cb_ptr->dec_msc_kbuf_ptr[i]);
			mtv_cb_ptr->dec_msc_kbuf_ptr[i] = NULL;
		}
	}
}

/* Allocate the MSC decoding-buffer if the multi service was enabled. */
static int create_msc_decoding_buffer(struct mtv_cb * mtv_cb_ptr)
{
	int i;

	for (i = 0; i < RTV_MAX_NUM_DAB_DATA_SVC; i++) {
		mtv_cb_ptr->dec_msc_kbuf_ptr[i] = vmalloc(MAX_MULTI_MSC_BUF_SIZE);
		if (mtv_cb_ptr->dec_msc_kbuf_ptr[i] == NULL) {
			DMBERR("CIF buffer(%d) alloc error!\n", i);
			return -ENOMEM;
		}
	}

	return 0;
}
#endif /* #if defined(RTV_CIF_MODE_ENABLED) && defined(RTV_BUILD_CIFDEC_WITH_DRIVER) */

static inline ssize_t read_dab_multiple_service(struct mtv_cb * mtv_cb_ptr, char *buf)
{		
	ssize_t ret = 0;
	MTV_TS_PKT_INFO *tsp = NULL;
	UINT i;
	IOCTL_MULTI_SERVICE_BUF __user *m = (IOCTL_MULTI_SERVICE_BUF __user *)buf;
	UINT subch_size_tbl[RTV_NUM_DAB_AVD_SERVICE];
	UINT subch_id_tbl[RTV_NUM_DAB_AVD_SERVICE];
	U8 *ubuf_ptr; /* Buffer pointer of the user space. */
	UINT user_buf_size_tbl[RTV_NUM_DAB_AVD_SERVICE];
	UINT loop = 0; // debug

#ifdef RTV_CIF_MODE_ENABLED /* More 2 DATA services */
	#ifdef RTV_BUILD_CIFDEC_WITH_DRIVER
	RTV_CIF_DEC_INFO cifdec;
	UINT dec_idx;
	#endif
	BOOL dec_data_copied = FALSE;
	UINT max_dec_size = 0, dec_msc_buf_size;
#endif

#ifndef RTV_FIC_POLLING_MODE /* FIC interrupt mode */
	unsigned int fic_size = 0;
#endif

	if (mtv_cb_ptr->f_flags & O_NONBLOCK) { /* Non-blocking mode. */
		if (mtv_tsp_queue.cnt == 0)
			return -EAGAIN;
	}
	else { /* The file is opened in blocking mode. */
		if (wait_event_interruptible(mtv_cb_ptr->read_wq,
			mtv_tsp_queue.cnt || (mtv_cb_ptr->read_stop == TRUE))) {
			DMBMSG("Woken up by signal.\n");
			return -ERESTARTSYS;
		}
	}

	/* Reset the return inforamtion for each sub channels. */
	for (i = 0; i < RTV_NUM_DAB_AVD_SERVICE; i++) {
		subch_size_tbl[i] = 0; 
		subch_id_tbl[i] = 0xFF; /* Default Invalid */
		user_buf_size_tbl[i] = MAX_MULTI_MSC_BUF_SIZE;
	}

#ifdef RTV_CIF_MODE_ENABLED
	/* Set the address of decoded MSC0 buffer to be received. */
	dec_msc_buf_size = MAX_MULTI_MSC_BUF_SIZE;
	#ifdef RTV_BUILD_CIFDEC_WITH_DRIVER
	for (i = 0; i < RTV_MAX_NUM_DAB_DATA_SVC; i++)
		#ifdef RTV_CIF_LINUX_USER_SPACE_COPY_USED
		cifdec.msc_buf_ptr[i] = (U8 __user *)m->msc_buf[i+MSC0_BASE_IDX]; /* user buffer */
		#else
		cifdec.msc_buf_ptr[i] = mtv_cb_ptr->dec_msc_kbuf_ptr[i]; /* kernel buffer */
		#endif
	#endif
#endif

	/* Acquire the read-mutex. */
	mutex_lock(&mtv_cb_ptr->read_lock);

	while (1) {
		if (mtv_cb_ptr->read_stop == TRUE) {
			ret = -EINTR; /* Force stopped. */
			goto unlock_read;
		}

		/* Check if the size of MSC input buffer to be copied. */
		if (atomic_read(&mtv_cb_ptr->num_opened_subch)) { /* subch opened */
			if ((tsp = mtv_peek_tsp(mtv_cb_ptr->demod_no)) != NULL) {
		#ifdef RTV_SPI_MSC1_ENABLED
				if (tsp->msc1_size > user_buf_size_tbl[0]) {
						DMBMSG("(%u) Exit for the small msc1 buffer size\n", loop);
						tsp = NULL;
						goto unlock_read;
				}
		#endif

		#ifdef RTV_SPI_MSC0_ENABLED
			#ifndef RTV_CIF_MODE_ENABLED /* Threshold mode */
				if (tsp->msc0_size > user_buf_size_tbl[MSC0_BASE_IDX]) {
						DMBMSG("(%u) Exit for the small msc0 buffer size\n", loop);
						tsp = NULL;
						goto unlock_read;
				}
			#else
				if (tsp->msc0_size > dec_msc_buf_size) {
						DMBMSG("(%u) Exit for the small decoding buffer size\n", loop);
						tsp = NULL;
						goto unlock_read;
				}
			#endif
		#endif
			}
			else
				goto unlock_read;
		}

		/* Dequeue a tsp from tsp_queue. */
		if ((tsp = mtv_get_tsp(mtv_cb_ptr->demod_no)) == NULL)
			goto unlock_read; /* Stop. */

		/* MSC */
		if (atomic_read(&mtv_cb_ptr->num_opened_subch) == 0)
			goto skip_msc_copy; /* Case for full-scan. */

#ifdef RTV_SPI_MSC1_ENABLED
		if (tsp->msc1_size != 0) { /* Force 0 index to Video/Audio.*/
			if (subch_id_tbl[0] == 0xFF) /* Default Invalid. */
				subch_id_tbl[0] = mtv_cb_ptr->av_subch_id;

			/* Check if subch ID was changed in the same out buf index ? */
			if (subch_id_tbl[0] == mtv_cb_ptr->av_subch_id) {
				ubuf_ptr = m->msc_buf[0] + subch_size_tbl[0];
				if (copy_to_user(ubuf_ptr,
								&tsp->msc1_buf[1], tsp->msc1_size)) {
					ret = -EFAULT;
					goto unlock_read;
				}

				subch_size_tbl[0] += tsp->msc1_size;
				user_buf_size_tbl[0] -= tsp->msc1_size;
			}
			else
				DMBMSG("Changed ID: from old(%u) to new(%u)\n",
						subch_size_tbl[0], mtv_cb_ptr->av_subch_id);
		}
#endif

#ifdef RTV_SPI_MSC0_ENABLED
		if (tsp->msc0_size != 0) {
	#ifndef RTV_CIF_MODE_ENABLED /* Threshold mode */
			if (subch_id_tbl[MSC0_BASE_IDX] == 0xFF) /* Default Invalid. */
				subch_id_tbl[MSC0_BASE_IDX] = mtv_cb_ptr->data_subch_id;
			
			/* Check if subch ID was changed in the same out buf index ? */
			if (subch_id_tbl[MSC0_BASE_IDX] == mtv_cb_ptr->data_subch_id) {
				ubuf_ptr = m->msc_buf[MSC0_BASE_IDX] + subch_size_tbl[MSC0_BASE_IDX];
				if (copy_to_user(ubuf_ptr,
								&tsp->msc0_buf[1], tsp->msc0_size)) {
					ret = -EFAULT;
					goto unlock_read;
				}

				subch_size_tbl[MSC0_BASE_IDX] += tsp->msc0_size;
				user_buf_size_tbl[MSC0_BASE_IDX] -= tsp->msc1_size;
			}
			else
				DMBMSG("Changed ID: from old(%u) to new(%u)\n",
					subch_size_tbl[MSC0_BASE_IDX], mtv_cb_ptr->data_subch_id);
	#else
		#ifdef RTV_BUILD_CIFDEC_WITH_DRIVER
			rtvCIFDEC_Decode(&cifdec, dec_msc_buf_size,
							&tsp->msc0_buf[1], tsp->msc0_size);

			for (i = 0; i < RTV_MAX_NUM_DAB_DATA_SVC; i++) {
				if (cifdec.msc_size[i]) {
					dec_idx = rtvCIFDEC_GetDecBufIndex(cifdec.msc_subch_id[i]);

					/* Check if subch ID was closed in the run-time ? */
					if (dec_idx == RTV_CIFDEC_INVALID_BUF_IDX) {
						DMBMSG("Closed ID(%u)\n", cifdec.msc_subch_id[i]);
						continue;
					}

					if (subch_id_tbl[dec_idx] == 0xFF) /* Default Invalid. */
						subch_id_tbl[dec_idx] = cifdec.msc_subch_id[i];

					/* Check if subch ID was changed in the same buf index ? */
					if (subch_id_tbl[dec_idx] != cifdec.msc_subch_id[i]) {
						DMBMSG("Changed ID: from old(%u) to new(%u)\n",
							subch_size_tbl[dec_idx], cifdec.msc_subch_id[i]);
						continue;
					}
				
					if (mtv_cb_ptr->read_stop == TRUE) {
						ret = -EINTR; /* Force stopped. */
						goto unlock_read;
					}

					/* Copy the decoded data into user-space. */
					ubuf_ptr = m->msc_buf[dec_idx] + subch_size_tbl[dec_idx];
					if (copy_to_user(ubuf_ptr, 
							cifdec.msc_buf_ptr[i], cifdec.msc_size[i])) {
						ret = -EFAULT;
						goto unlock_read;
					}

					subch_size_tbl[dec_idx] += cifdec.msc_size[i];
					cifdec.msc_buf_ptr[i] += cifdec.msc_size[i]; /* Advance */

					max_dec_size = MAX(max_dec_size, cifdec.msc_size[i]);
					dec_data_copied = TRUE;
				}
			} /* end of for() */

			if (dec_data_copied == TRUE) { /* Decoding data copied ? */
				dec_data_copied = FALSE; /* Reset */
				dec_msc_buf_size -= max_dec_size;
				max_dec_size = 0; /* Reset */
			}
		#else /* RTV_BUILD_CIFDEC_WITH_DRIVER */
			subch_id_tbl[MSC0_BASE_IDX] = 0xFF; /* NA */
			
			ubuf_ptr = m->msc_buf[MSC0_BASE_IDX] + subch_size_tbl[MSC0_BASE_IDX];
			if (copy_to_user(ubuf_ptr, &tsp->msc0_buf[1], tsp->msc0_size)) {
				ret = -EFAULT;
				goto unlock_read;
			}
		
			subch_size_tbl[MSC0_BASE_IDX] += tsp->msc0_size;
			user_buf_size_tbl[MSC0_BASE_IDX] -= tsp->msc1_size;
		#endif /* !RTV_BUILD_CIFDEC_WITH_DRIVER */
	#endif /* RTV_CIF_MODE_ENABLED */
		}
#endif

skip_msc_copy:
		/* FIC */
#ifndef RTV_FIC_POLLING_MODE /* FIC interrupt mode */
		if (tsp->fic_size) {
			fic_size = tsp->fic_size;
			if (copy_to_user(m->fic_buf, &tsp->fic_buf[1], tsp->fic_size)) {
				ret = -EFAULT;
				goto unlock_read;
			}

			goto unlock_read; /* Force to stop for FIC parsing in time. */
		}
#endif

		mtv_free_tsp(mtv_cb_ptr->demod_no, tsp);
		tsp = NULL;

		loop++;
	}

unlock_read:
	mtv_free_tsp(mtv_cb_ptr->demod_no, tsp); /* for "goto unlock_read;" statement. */

	if ((mtv_cb_ptr->f_flags & O_NONBLOCK) == 0) {
		if (mtv_tsp_queue.cnt) /* for next time */
			wake_up_interruptible(&mtv_cb_ptr->read_wq);
	}

	mutex_unlock(&mtv_cb_ptr->read_lock);

	if (ret == 0) {
		/* Copy the size of MSC anyway to notify the result size to user. */
		for (i = 0; i < RTV_NUM_DAB_AVD_SERVICE; i++) {
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

#ifndef RTV_FIC_POLLING_MODE /* FIC interrupt mode */
		if (put_user(fic_size, &m->fic_size)) {
			ret = -EFAULT;
			goto exit_read;
		}
		ret += fic_size;
#endif
	}


exit_read:
#ifdef DEBUG_MTV_IF_MEMORY
	mtv_cb_ptr->max_remaining_tsp_cnt
		= MAX(mtv_cb_ptr->max_remaining_tsp_cnt, mtv_tsp_queue.cnt);
#endif

	//printk("loop: %u\n", loop);
	
	return ret;
}
#endif /* #if defined(RTV_DAB_ENABLE) || defined(RTV_TDMB_ENABLE) */

#ifdef RTV_FM_ENABLE
static inline ssize_t read_fm_multiple_service(char *buf)
{
	return 0;
}
#endif /* #ifdef RTV_FM_ENABLE */
#endif /* #ifdef RTV_MULTI_SERVICE_MODE */

#ifdef RTV_SPI_MSC1_ENABLED
/* In case of 1 av(TDMB/DAB), 1 data(TDMB/DAB), 
1 video(ISDBT) or 1 pcm(FM) using MSC1 memory. */
static inline ssize_t read_single_service(struct mtv_cb * mtv_cb_ptr, char *buf, size_t cnt)
{
	ssize_t ret = 0;
	unsigned int copy_bytes;/* # of bytes to be copied. */
	MTV_TS_PKT_INFO *tsp = NULL;
	unsigned int req_read_size = MIN(cnt, 32 * 188);
	static U8 *msc_buf_ptr; /* Be used in the next read time. */

	if (!cnt)
		return 0;

	if (mtv_cb_ptr->f_flags & O_NONBLOCK) { /* non-blocking mode. */
		unsigned int have_tsp_bytes
			= mtv_tsp_queue.cnt * mtv_cb_ptr->msc1_thres_size;

		if (have_tsp_bytes < req_read_size)
			return -EAGAIN;
	}
	else { /* Blocking mode. */
		if (wait_event_interruptible(mtv_cb_ptr->read_wq,
			mtv_tsp_queue.cnt || (mtv_cb_ptr->read_stop == TRUE))) {
			DMBMSG("Woken up by signal.\n");
			return -ERESTARTSYS;
		}
	}

	/* Acquire the mutex. */
	mutex_lock(&mtv_cb_ptr->read_lock);

	do {
		if (mtv_cb_ptr->read_stop == TRUE) {
			ret = -EINTR; /* no data was transferred */
			goto free_prev_tsp;
		}

		if (mtv_cb_ptr->prev_tsp == NULL) {
			/* Dequeue a tsp from tsp_queue. */
			if ((tsp = mtv_get_tsp(mtv_cb_ptr->demod_no)) == NULL)
				goto unlock_read; /* Stop. */

			mtv_cb_ptr->prev_tsp = tsp;
			msc_buf_ptr = &tsp->msc1_buf[1];
		}
		else
			tsp = mtv_cb_ptr->prev_tsp;

		copy_bytes = MIN(tsp->msc1_size, req_read_size);

		if (copy_to_user(buf, msc_buf_ptr, copy_bytes)) {
			ret = -EFAULT;
			goto free_prev_tsp;
		}
		else {
			msc_buf_ptr += copy_bytes;
			buf += copy_bytes;
			ret += copy_bytes; /* Total return size */
			req_read_size -= copy_bytes;

			tsp->msc1_size -= copy_bytes;
			if (tsp->msc1_size == 0) {
				mtv_free_tsp(mtv_cb_ptr->demod_no, tsp);
				mtv_cb_ptr->prev_tsp = NULL; /* All used. */
			}
		}
	} while (req_read_size != 0);
	goto unlock_read; /* Don't free a previous tsp */

free_prev_tsp:
	if (mtv_cb_ptr->prev_tsp) {
		mtv_free_tsp(mtv_cb_ptr->demod_no, mtv_cb_ptr->prev_tsp);
		mtv_cb_ptr->prev_tsp = NULL; 
	}

unlock_read:
	if ((mtv_cb_ptr->f_flags & O_NONBLOCK) == 0) {
		if (mtv_tsp_queue.cnt) /* for next time */
			wake_up_interruptible(&mtv_cb_ptr->read_wq);
	}

#ifdef DEBUG_MTV_IF_MEMORY
	mtv_cb_ptr->max_remaining_tsp_cnt 
		= MAX(mtv_cb_ptr->max_remaining_tsp_cnt, mtv_tsp_queue.cnt);
#endif

	mutex_unlock(&mtv_cb_ptr->read_lock);

	return ret;
}
#endif /* #ifdef RTV_SPI_MSC1_ENABLED */

static ssize_t mtv_read(struct file *filp, char *buf, size_t count, loff_t *pos)
{
	int ret;

	/* Must place the demod index at the frist array element! */
	//if (get_user(demod_no, (char *)buf))
	//	return -EFAULT;

	//DMBMSG("%d demod seleted\n", demod_no);

	/* Check if the device is already readed ? */
	if (atomic_cmpxchg(&mtv_cb_ptr->read_flag, 0, 1)) {
		DMBERR("%s is already readed.\n", NXB110TV_DEV_NAME);
		return -EBUSY;
	}

#if defined(RTV_ISDBT_ONLY_ENABLED)
	ret = read_single_service(mtv_cb_ptr, buf, count); /* ISDBT only */

#elif defined(RTV_TDMBorDAB_ONLY_ENABLED)
	#if defined(RTV_MULTI_SERVICE_MODE)
	ret = read_dab_multiple_service(mtv_cb_ptr, buf);
	#else
	ret = read_single_service(mtv_cb_ptr, buf, count);
	#endif
#else
	/* Assume that FM only project was not exist. */
	switch (mtv_cb_ptr->tv_mode) {
	#if defined(RTV_TDMB_ENABLE) || defined(RTV_DAB_ENABLE)
	case DMB_TV_MODE_TDMB:
	case DMB_TV_MODE_DAB:
		#ifdef RTV_MULTI_SERVICE_MODE
		ret = read_dab_multiple_service(mtv_cb_ptr, buf);
		#else
		ret = read_single_service(mtv_cb_ptr, buf, count);
		#endif
		break;
	#endif

	#if defined(RTV_ISDBT_ENABLE)
	case DMB_TV_MODE_1SEG:
		ret = read_single_service(mtv_cb_ptr, buf, count);
		break;
	#endif

	#ifdef RTV_FM_ENABLE
	case DMB_TV_MODE_FM:
		#ifdef RTV_FM_RDS_ENABLED
		ret = read_fm_multiple_service(mtv_cb_ptr, buf);
		#else
		ret = read_single_service(mtv_cb_ptr, buf, count);
		#endif
		break;
	#endif /* #ifdef RTV_FM_ENABLE */

	default:
		ret = -ENODEV;
		break;
	}
#endif

	atomic_set(&mtv_cb_ptr->read_flag, 0);

	return ret;
}
#endif /* #if defined(RTV_IF_SPI) */

#if defined(RTV_IF_SPI) || defined(RTV_FIC_I2C_INTR_ENABLED)
static int mtv_create_thread(struct mtv_cb * mtv_cb_ptr)
{
#if 1
	int ret = 0;

	if (mtv_cb_ptr->isr_thread_cb == NULL) {
		init_waitqueue_head(&mtv_cb_ptr->isr_wq);

		mtv_cb_ptr->isr_thread_cb
			= kthread_run(mtv_isr_thread_av, NULL, "mtv_isr_thread_av");

		if (IS_ERR(mtv_cb_ptr->isr_thread_cb)) {
			WARN(1, KERN_ERR "[mtv_create_thread] Thread create error\n");
			ret = PTR_ERR(mtv_cb_ptr->isr_thread_cb);
			mtv_cb_ptr->isr_thread_cb = NULL;
		}
	}
	
	return ret;
#else
	return 0;
#endif
}

static void mtv_delete_thread(struct mtv_cb * mtv_cb_ptr)
{
#if 1
	if (mtv_cb_ptr->isr_thread_cb != NULL) {
		kthread_stop(mtv_cb_ptr->isr_thread_cb);
		mtv_cb_ptr->isr_thread_cb = NULL;
	}
#else
#endif
}
#endif /* #if defined(RTV_IF_SPI) || defined(RTV_FIC_I2C_INTR_ENABLED) */


static void mtv_power_off(struct mtv_cb * mtv_cb_ptr)
{
	if (mtv_cb_ptr->is_power_on == FALSE)
		return;

	mtv_cb_ptr->is_power_on = FALSE;

	DMBMSG("START\n");

#if defined(RTV_IF_SPI) || defined(RTV_FIC_I2C_INTR_ENABLED)
	disable_irq(mtv_cb_ptr->irq);

	#if 1
	mtv_delete_thread(mtv_cb_ptr);
	#else
	cancel_work_sync(&mtv_cb_ptr->tdmb_isr_work);
	flush_workqueue(mtv_cb_ptr->isr_workqueue);
	destroy_workqueue(mtv_cb_ptr->isr_workqueue);
	mtv_cb_ptr->isr_workqueue = NULL;
	#endif
#endif

	switch(mtv_cb_ptr->tv_mode) {
#ifdef RTV_TDMB_ENABLE
	case DMB_TV_MODE_TDMB : tdmb_deinit(mtv_cb_ptr); break;
#endif
#ifdef RTV_DAB_ENABLE
	case DMB_TV_MODE_DAB : dab_deinit(); break;
#endif
#ifdef RTV_ISDBT_ENABLE
	case DMB_TV_MODE_1SEG : isdbt_deinit(); break;
#endif
#ifdef RTV_FM_ENABLE
	case DMB_TV_MODE_FM : fm_deinit(); break;
#endif
	default: break;
	}

	rtvOEM_PowerOn(mtv_cb_ptr->demod_no, 0);

#if defined(RTV_IF_SPI)
	mtv_reset_tsp_queue(mtv_cb_ptr);

	#if defined(RTV_DAB_ENABLE) || defined(RTV_TDMB_ENABLE)
	#if defined(RTV_CIF_MODE_ENABLED) && defined(RTV_BUILD_CIFDEC_WITH_DRIVER)
	/* Free the MSC decoding-buffer if the multi service was enabled. */
	delete_msc_decoding_buffer(mtv_cb_ptr);
	#endif
	#endif /* #if defined(RTV_DAB_ENABLE) || defined(RTV_TDMB_ENABLE) */

	/* Wake up threads blocked on read() to finish read operation. */
	if ((mtv_cb_ptr->f_flags & O_NONBLOCK) == 0)
		wake_up_interruptible(&mtv_cb_ptr->read_wq);
#endif

	DMBMSG("END\n");
}

static int mtv_power_on(struct mtv_cb * mtv_cb_ptr, E_DMB_TV_MODE_TYPE tv_mode, unsigned long arg)
{
	int ret = 0;

	if (mtv_cb_ptr->is_power_on == TRUE)	
		return 0;

	DMBMSG("Start\n");

	rtvOEM_PowerOn(mtv_cb_ptr->demod_no, 1);
	
	switch (tv_mode) {
#ifdef RTV_TDMB_ENABLE	
	case DMB_TV_MODE_TDMB :
		ret = tdmb_init(mtv_cb_ptr, arg);
		break;
#endif
#ifdef RTV_DAB_ENABLE
	case DMB_TV_MODE_DAB :
		ret = dab_init(arg);
		break;
#endif
#ifdef RTV_ISDBT_ENABLE
	case DMB_TV_MODE_1SEG :
		ret = isdbt_init(arg);
		break;
#endif
#ifdef RTV_FM_ENABLE
	case DMB_TV_MODE_FM :
		ret = fm_init(arg);
		break;
#endif
	default: break;
		ret = -EINVAL;
	}

	if (ret != 0)
		return ret;

#if defined(RTV_IF_SPI)
	/* Allow thread to block on read() if the blocking mode. */
	mtv_cb_ptr->read_stop = FALSE;
	mtv_cb_ptr->prev_tsp = NULL;

	mtv_cb_ptr->first_interrupt = TRUE;

	#ifdef DEBUG_MTV_IF_MEMORY
	mtv_cb_ptr->max_alloc_tsp_cnt = 0;
	#endif

	#if defined(RTV_DAB_ENABLE) || defined(RTV_TDMB_ENABLE)
	#if defined(RTV_CIF_MODE_ENABLED) && defined(RTV_BUILD_CIFDEC_WITH_DRIVER)
	/* Free the MSC decoding-buffer if the multi service was enabled. */
	ret = create_msc_decoding_buffer(mtv_cb_ptr);
	if (ret)
		return ret;
	#endif
	#endif /* #if defined(RTV_DAB_ENABLE) || defined(RTV_TDMB_ENABLE) */	
#endif

#if defined(RTV_IF_SPI) || defined(RTV_FIC_I2C_INTR_ENABLED)

	#if 0
	INIT_WORK(&mtv_cb_ptr->tdmb_isr_work, mtv_isr_handler);
	mtv_cb_ptr->isr_workqueue = create_singlethread_workqueue(NXB110TV_DEV_NAME);
	if (mtv_cb_ptr->isr_workqueue == NULL) {
		DMBERR("couldn't create workqueue\n");
		return -ENOMEM;
	}
	#else
	mtv_cb_ptr->isr_cnt = 0;
	if ((ret=mtv_create_thread(mtv_cb_ptr)) != 0)
		return ret;
	#endif

	enable_irq(mtv_cb_ptr->irq); /* After DMB init */
#endif

	mtv_cb_ptr->tv_mode = tv_mode;
	mtv_cb_ptr->is_power_on = TRUE;

	DMBMSG("End\n");

	return 0;
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
static long mtv_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
#else
static int mtv_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
#endif
{
	int ret = 0;
//	IOCTL_POWER_ON_INFO __user *argp = (IOCTL_POWER_ON_INFO __user *)arg;
	//if (get_user(demod_no, &argp->demod_no))
	//	return -EFAULT;

	/* Copy the first element to save demod number from all IOCTL structures */
//	if (get_user(demod_no, (int *)arg))
//		return -EFAULT;

	//DMBMSG("%d demod seleted\n", demod_no);

	mutex_lock(&mtv_cb_ptr->ioctl_lock);
	
	switch (cmd) {
#ifdef RTV_TDMB_ENABLE
	case IOCTL_TDMB_POWER_ON:
		ret = mtv_power_on(mtv_cb_ptr, DMB_TV_MODE_TDMB, arg);
		break;

	case IOCTL_TDMB_POWER_OFF:
		mtv_power_off(mtv_cb_ptr);
		break;

	case IOCTL_TDMB_SCAN_FREQ:
		ret = tdmb_scan_freq(mtv_cb_ptr, arg);
		break;

	case IOCTL_TDMB_OPEN_FIC:
		ret = tdmb_open_fic(mtv_cb_ptr, arg);
		break;
            
	case IOCTL_TDMB_CLOSE_FIC:
		tdmb_close_fic(mtv_cb_ptr);
		break;

	case IOCTL_TDMB_READ_FIC:
      	ret = tdmb_read_fic(mtv_cb_ptr, arg);
		break;
            
	case IOCTL_TDMB_OPEN_SUBCHANNEL:
		ret = tdmb_open_subchannel(mtv_cb_ptr, arg);		
		break;

	case IOCTL_TDMB_CLOSE_SUBCHANNEL: {
		IOCTL_CLOSE_SUBCHANNEL_INFO __user *argp = (IOCTL_CLOSE_SUBCHANNEL_INFO __user *)arg;
		ret = tdmb_close_subchannel(mtv_cb_ptr, argp->subch_id);
	}
		break;

	case IOCTL_TDMB_CLOSE_ALL_SUBCHANNELS:
		tdmb_close_all_subchannels(mtv_cb_ptr);
		break;

	case IOCTL_TDMB_GET_LOCK_STATUS:
		ret = tdmb_get_lock_status(mtv_cb_ptr, arg);
		break;

	case IOCTL_TDMB_GET_SIGNAL_INFO:
		ret = tdmb_get_signal_info(mtv_cb_ptr, arg);
		break;
#endif /* #ifdef RTV_TDMB_ENABLE */

#ifdef RTV_DAB_ENABLE
	case IOCTL_DAB_POWER_ON:
		ret = mtv_power_on(DMB_TV_MODE_DAB, arg);
		break;

	case IOCTL_DAB_POWER_OFF:
		mtv_power_off();
		break;

	case IOCTL_DAB_SCAN_FREQ: /* Full scan by user. */
		ret = dab_scan_freq(arg);
		break;

	case IOCTL_DAB_GET_FIC_SIZE:
		ret = dab_get_fic_size(arg);
		break;

	case IOCTL_DAB_OPEN_FIC:
		ret = dab_open_fic(arg);
		break;
            
	case IOCTL_DAB_CLOSE_FIC:
		dab_close_fic();
		break;

	case IOCTL_DAB_READ_FIC: /* FIC polling Mode. */
		ret = dab_read_fic(arg);
		break;

	case IOCTL_DAB_OPEN_SUBCHANNEL:
		ret = dab_open_subchannel(arg);
		break;		

	case IOCTL_DAB_CLOSE_SUBCHANNEL:
		ret = dab_close_subchannel(arg);
		break;		

	case IOCTL_DAB_CLOSE_ALL_SUBCHANNELS:
		dab_close_all_subchannels();
		break;

	case IOCTL_DAB_GET_LOCK_STATUS:
		ret = dab_get_lock_status(arg);
		break;

	case IOCTL_DAB_GET_SIGNAL_INFO:
		ret = dab_get_signal_info(arg);
		break;
#endif /* #ifdef RTV_DAB_ENABLE */	

#ifdef RTV_ISDBT_ENABLE
	case IOCTL_ISDBT_POWER_ON:
		ret = mtv_power_on(DMB_TV_MODE_1SEG, arg);
		break;

	case IOCTL_ISDBT_POWER_OFF:
		mtv_power_off();
		break;

	case IOCTL_ISDBT_SCAN_FREQ:
		ret = isdbt_scan_freq(arg);
		break;
		
	case IOCTL_ISDBT_SET_FREQ:
		ret = isdbt_set_freq(arg);
		break;		

	case IOCTL_ISDBT_START_TS:
		isdbt_start_ts();
		break;

	case IOCTL_ISDBT_STOP_TS:
		isdbt_stop_ts();
		break;			

	case IOCTL_ISDBT_GET_LOCK_STATUS:
		ret = isdbt_get_lock_status(arg);
		break;

	case IOCTL_ISDBT_GET_TMCC:
		ret = isdbt_get_tmcc(arg);
		break;

	case IOCTL_ISDBT_GET_SIGNAL_INFO:
		ret = isdbt_get_signal_info(arg);
		break;		
#endif /* #ifdef RTV_ISDBT_ENABLE */

#ifdef RTV_FM_ENABLE
	case IOCTL_FM_POWER_ON: /* with adc clk type */
		ret = mtv_power_on(DMB_TV_MODE_FM, arg);
		break;

	case IOCTL_FM_POWER_OFF:
		mtv_power_off();
		break;

	case IOCTL_FM_SCAN_FREQ:
		ret = fm_scan_freq(arg);
		break;

	case IOCTL_FM_SRCH_FREQ:
		ret = fm_search_freq(arg);
		break;
		
	case IOCTL_FM_SET_FREQ:
		ret = fm_set_freq(arg);
		break;			
		
	case IOCTL_FM_START_TS:
		fm_start_ts();
		break;

	case IOCTL_FM_STOP_TS:
		fm_stop_ts();
		break;			

	case IOCTL_FM_GET_LOCK_STATUS :
		ret = fm_get_lock_status(arg);
		break;

	case IOCTL_FM_GET_RSSI:
		ret = fm_get_rssi(arg);
		break;
#endif /* #ifdef RTV_FM_ENABLE */

	case IOCTL_GET_DMOED_CNT:
	{
		int demod_cnt = NXB110TV_MAX_NUM_DEMOD_CHIP;
		
		if (put_user(demod_cnt, (int *)arg))
			ret = -EFAULT;
	}
		break;

	case IOCTL_TEST_GPIO_SET:
	case IOCTL_TEST_GPIO_GET:
		ret = test_gpio(arg, cmd);
		break;

	case IOCTL_TEST_MTV_POWER_ON:	
	case IOCTL_TEST_MTV_POWER_OFF:
		test_power_on_off(mtv_cb_ptr, cmd);
		break;		

	case IOCTL_TEST_REG_SINGLE_READ:
	case IOCTL_TEST_REG_BURST_READ:
	case IOCTL_TEST_REG_WRITE:
		ret = test_register_io(mtv_cb_ptr, arg, cmd);
		break;
	
	default:
		DMBERR("Invalid ioctl command: %d\n", cmd);
		ret = -ENOIOCTLCMD;
		break;
	}

	mutex_unlock(&mtv_cb_ptr->ioctl_lock);

	return ret;
}


#if defined(RTV_FIC_I2C_INTR_ENABLED)
static int mtv_sigio_fasync(int fd, struct file *filp, int mode)
{
	return fasync_helper(fd, filp, mode, &mtv_cb_ptr->fasync);
}
#endif

atomic_t 	tdmb_open_flag; /* to open only once */

static int mtv_close(struct inode *inode, struct file *filp)
{
	DMBMSG("START\n");

	mutex_lock(&mtv_cb_ptr->ioctl_lock);
	mtv_power_off(mtv_cb_ptr);
	mutex_unlock(&mtv_cb_ptr->ioctl_lock);
	mutex_destroy(&mtv_cb_ptr->ioctl_lock);

#if defined(RTV_IF_SPI)
	mtv_delete_tsp_pool(mtv_cb_ptr);
	mutex_destroy(&mtv_cb_ptr->read_lock);
#endif

#if defined(RTV_FIC_I2C_INTR_ENABLED)
	fasync_helper(-1, filp, 0, &mtv_cb_ptr->fasync);
#endif

	wake_unlock(&mtv_cb_ptr->wake_lock);

	atomic_set(&tdmb_open_flag, 0);

	DMBMSG("END\n");

	return 0;
}

static int mtv_open(struct inode *inode, struct file *filp)
{
	int ret = 0;

	/* Check if the device is already opened ? */
	if (atomic_cmpxchg(&tdmb_open_flag, 0, 1)) {
		DMBERR("%s is already opened\n", NXB110TV_DEV_NAME);
		//return -EBUSY;
		return 0;
	}

#if defined(RTV_IF_SPI)
	ret = mtv_create_tsp_pool(mtv_cb_ptr);
	if (ret)
		return ret;

	/* Allow thread to block on read() if the blocking mode. */
	mtv_cb_ptr->read_stop = FALSE;
	mutex_init(&mtv_cb_ptr->read_lock);

	if ((filp->f_flags & O_NONBLOCK) == 0) { /* Blocking mode */
		init_waitqueue_head(&mtv_cb_ptr->read_wq);
		DMBMSG("Opened as the Blocking mode\n");
	}
	else
		DMBMSG("Opened as the Non-blocking mode\n");
#endif

	/* Save the flag of file. */
	mtv_cb_ptr->f_flags = filp->f_flags;
	mtv_cb_ptr->is_power_on = FALSE;
	mutex_init(&mtv_cb_ptr->ioctl_lock);

	wake_lock(&mtv_cb_ptr->wake_lock);
	
	return ret;
}


static struct file_operations mtv_fops =
{
    .owner = THIS_MODULE,

#if defined(RTV_IF_SPI)
    .read = mtv_read,
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
	.unlocked_ioctl = mtv_ioctl,
#else
	.ioctl = mtv_ioctl,    
#endif

#if defined(RTV_FIC_I2C_INTR_ENABLED)
	.fasync = mtv_sigio_fasync,
#endif
	.release = mtv_close,
	.open = mtv_open
};

static struct miscdevice mtv_misc_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = NXB110TV_DEV_NAME,
    .fops = &mtv_fops,
};

static void mtv_deinit_device(void)
{
	wake_lock_destroy(&mtv_cb_ptr->wake_lock);

#if defined(RTV_IF_SPI) ||defined(RTV_FIC_I2C_INTR_ENABLED)
	free_irq(gpio_to_irq(NXB110TV_IRQ_INT), NULL);
#endif

#if defined(RTV_IF_TSIF) || defined(RTV_IF_SPI_SLAVE)
	i2c_del_driver(&mtv_i2c_driver);

#elif defined(RTV_IF_SPI)
	spi_unregister_driver(&mtv_spi_driver_0);
#endif

	if (mtv_cb_ptr) {
		kfree(mtv_cb_ptr);
		mtv_cb_ptr = NULL;
	}

	DMBMSG("END\n");
}

static int mtv_init_device(void)
{
	int ret = 0;

	mtv_configure_gpio();

	mtv_cb_ptr = kzalloc(sizeof(struct mtv_cb), GFP_KERNEL);
	if (!mtv_cb_ptr) {
		DMBERR("mtv_cb_ptr allocate error!\n");
		return -ENOMEM;
	}

	mtv_cb_ptr->demod_no = 0;

	//DMBMSG("mtv_cb_ptr(0x%p), mtv_cb_ptr[%d](0x%p)\n", mtv_cb_ptr, i, mtv_cb_ptr[i]);

#if defined(RTV_IF_TSIF) || defined(RTV_IF_SPI_SLAVE)
	if ((ret=i2c_add_driver(&mtv_i2c_driver)) < 0) {
		DMBERR("NXB110TV I2C driver register failed\n");
		goto err;
	}
	
#elif defined(RTV_IF_SPI)
	if ((ret=spi_register_driver(&mtv_spi_driver_0)) < 0) {
		DMBERR("NXB110TV SPI0 driver register failed\n");
		goto err;
	}
#endif

#if defined(RTV_IF_SPI) || defined(RTV_FIC_I2C_INTR_ENABLED)
	mtv_cb_ptr->irq = gpio_to_irq(NXB110TV_IRQ_INT); ///!!!! 
	ret = request_irq(mtv_cb_ptr->irq, mtv_isr_av,
					IRQ_TYPE_EDGE_FALLING, "mtv_isr_av", NULL);
	if (ret != 0) {
		DMBERR("Failed to install irq (%d)\n", ret);
		goto err;
	}

	disable_irq(mtv_cb_ptr->irq); /* Must disabled */
#endif

	atomic_set(&mtv_cb_ptr->read_flag, 0);

	wake_lock_init(&mtv_cb_ptr->wake_lock, WAKE_LOCK_SUSPEND, NXB110TV_DEV_NAME);

	atomic_set(&tdmb_open_flag, 0);

	return 0;

err:
	mtv_deinit_device();

	return ret;	
}

static const char *build_date = __DATE__;
static const char *build_time = __TIME__;

static int __init mtv_av_module_init(void)
{
   	int ret;

	DMBMSG("\t==================================================\n");
	DMBMSG("\tBuild with Linux Kernel(%d.%d.%d)\n",
			(LINUX_VERSION_CODE>>16)&0xFF,
			(LINUX_VERSION_CODE>>8)&0xFF,
			LINUX_VERSION_CODE&0xFF);
	DMBMSG("\t %s Module Build Date/Time: %s, %s\n",
				mtv_misc_device.name, build_date, build_time);
	DMBMSG("\t==================================================\n\n");

	ret = mtv_init_device();
	if (ret < 0)
		return ret;

	/* misc device registration */
	ret = misc_register(&mtv_misc_device);
	if (ret) {
		DMBERR("misc_register() failed! : %d", ret);
		return ret;
	}

	return 0;
}


static void __exit mtv_av_module_exit(void)
{
	mtv_deinit_device();
	
	misc_deregister(&mtv_misc_device);
}

module_init(mtv_av_module_init);
module_exit(mtv_av_module_exit);
MODULE_DESCRIPTION("MTV AV driver");
MODULE_LICENSE("GPL");

