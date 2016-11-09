/*
 * tdmb_tsp_buf.c
 *
 * TDMB TSP buffer management driver for SPI interface.
 *
 * Copyright (C) (2012, RAONTECH)
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

#include "mtv319.h" // must before tdmb.h
#include "tdmb.h"
#include "tdmb_debug.h"

#if defined(CONFIG_TDMB_SPI) || defined(CONFIG_TDMB_EBI2)

/* Memory pool and queue to store DMB data. */
static TDMB_TSPB_HEAD_INFO tspb_pool; /* Head of TSP buffer pool */
static TDMB_TSPB_HEAD_INFO tspb_queue; /* Head of TSP buffer queue */

#ifdef RTV_MULTIPLE_CHANNEL_MODE
#include "mtv319_cifdec.h"

/* fic_msc_type: 0(FIC), 1(MSC): Only for multiple-service mode. */
void tdmb_tspb_queue_clear_contents(int fic_msc_type)
{
	TDMB_TSPB_INFO *tspb = NULL;
	struct list_head *pos;
	unsigned int discard_size;

	/* Acquire the read-mutex. */
	mutex_lock(&tdmb_cb_ptr->read_lock);

	spin_lock(&tspb_queue.lock); /* for IRQ */

	list_for_each (pos, &tspb_queue.head) { /* Lookup tsq Q */
		tspb = list_entry(pos, TDMB_TSPB_INFO, link);

		discard_size = rtvCIFDEC_SetDiscardTS(fic_msc_type,
						&tspb->buf[TDMB_SPI_CMD_SIZE],
						tspb->size);
		tspb->size -= discard_size;
	}

	spin_unlock(&tspb_queue.lock);

	mutex_unlock(&tdmb_cb_ptr->read_lock);
}
#endif /* #ifdef RTV_MULTIPLE_CHANNEL_MODE */

unsigned int tdmb_tspb_queue_count(void)
{
	return tspb_queue.cnt;
}

/* This function should called after the stream stopped. */
void tdmb_tspb_queue_reset(void)
{
	TDMB_TSPB_INFO *tspb;

	/* Set the flag to stop the operation of reading. */
	tdmb_cb_ptr->read_stop = true;

	/* Acquire the mutex. */
	mutex_lock(&tdmb_cb_ptr->read_lock);

	if (tspb_pool.cnt != MAX_NUM_TS_PKT_BUF) {
		/* Free a previous TSP use in read(). */
		if (tdmb_cb_ptr->prev_tspb) {
			tdmb_tspb_free_buffer(tdmb_cb_ptr->prev_tspb);
			tdmb_cb_ptr->prev_tspb = NULL; 
		}

		while ((tspb=tdmb_tspb_dequeue()) != NULL) 
			tdmb_tspb_free_buffer(tspb);

		if (tspb_pool.cnt != MAX_NUM_TS_PKT_BUF)
			WARN(1, KERN_ERR "[tdmb_tspb_queue_reset] Abnormal cnt! (%u/%u)\n",
						tspb_pool.cnt, MAX_NUM_TS_PKT_BUF);
	}

	mutex_unlock(&tdmb_cb_ptr->read_lock);

#ifdef DEBUG_TSP_BUF
	DMBMSG("Max alloc #tspb: %u\n", tdmb_cb_ptr->max_alloc_tspb_cnt);
#endif
}

/* Peek a TSP at the head of list. */
TDMB_TSPB_INFO *tdmb_tspb_peek(void)
{
	TDMB_TSPB_INFO *tspb = NULL;
	struct list_head *head_ptr = &tspb_queue.head;

	spin_lock(&tspb_queue.lock);
	
	if (!list_empty(head_ptr))
		tspb = list_first_entry(head_ptr, TDMB_TSPB_INFO, link);

	spin_unlock(&tspb_queue.lock);

	return tspb;
}

/* Dequeue a TSP from ts data queue. */
TDMB_TSPB_INFO *tdmb_tspb_dequeue(void)
{
	TDMB_TSPB_INFO *tspb = NULL;
	struct list_head *head_ptr = &tspb_queue.head;

	spin_lock(&tspb_queue.lock);
	
	if (!list_empty(head_ptr)) {
		tspb = list_first_entry(head_ptr, TDMB_TSPB_INFO, link);
		list_del(&tspb->link);
		tspb_queue.cnt--;
	}

	spin_unlock(&tspb_queue.lock);

	return tspb;
}

/* Enqueue a ts packet into ts data queue. */
void tdmb_tspb_enqueue(TDMB_TSPB_INFO *tspb)
{
	spin_lock(&tspb_queue.lock);

	list_add_tail(&tspb->link, &tspb_queue.head);
	tspb_queue.cnt++;

#ifdef DEBUG_TSP_BUF
	tdmb_cb_ptr->max_enqueued_tspb_cnt 
		= MAX(tdmb_cb_ptr->max_enqueued_tspb_cnt, tspb_queue.cnt);
#endif

	spin_unlock(&tspb_queue.lock);
}

unsigned int tdmb_tspb_freepool_count(void)
{
	return tspb_pool.cnt;
}

void tdmb_tspb_free_buffer(TDMB_TSPB_INFO *tspb)
{	
	if (tspb == NULL)
		return;

	spin_lock(&tspb_pool.lock);

	tspb->size = 0;

	list_add_tail(&tspb->link, &tspb_pool.head);
	tspb_pool.cnt++;

	spin_unlock(&tspb_pool.lock);
}


TDMB_TSPB_INFO *tdmb_tspb_alloc_buffer(void)
{	
	TDMB_TSPB_INFO *tspb = NULL;
	struct list_head *head_ptr = &tspb_pool.head;

	spin_lock(&tspb_pool.lock);

	if (!list_empty(head_ptr)) {
		tspb = list_first_entry(head_ptr, TDMB_TSPB_INFO, link);
		list_del(&tspb->link);
		tspb_pool.cnt--;

#ifdef DEBUG_TSP_BUF
		tdmb_cb_ptr->max_alloc_tspb_cnt 
			= max(tdmb_cb_ptr->max_alloc_tspb_cnt,
				MAX_NUM_TS_PKT_BUF - tspb_pool.cnt);
#endif	
	}

	spin_unlock(&tspb_pool.lock);

	return tspb;
}

int tdmb_tspb_delete_pool(void)
{
	struct list_head *head_ptr = &tspb_pool.head;
	TDMB_TSPB_INFO *tspb;

	while ((tspb = tdmb_tspb_dequeue()) != NULL)
		kfree(tspb);

	while (!list_empty(head_ptr)) {
		tspb = list_entry(head_ptr->next, TDMB_TSPB_INFO, link);
		list_del(&tspb->link);
		kfree(tspb);
	}

	return 0;
}

int tdmb_tspb_create_pool(void)
{
	unsigned int i;
	TDMB_TSPB_INFO *tspb;

	tdmb_cb_ptr->prev_tspb = NULL;

	spin_lock_init(&tspb_queue.lock);
	INIT_LIST_HEAD(&tspb_queue.head);
	tspb_queue.cnt = 0;
	
	spin_lock_init(&tspb_pool.lock);
	INIT_LIST_HEAD(&tspb_pool.head);
	tspb_pool.cnt = 0;

	RESET_DEBUG_TSPB_STAT;

	for (i = 0; i < MAX_NUM_TS_PKT_BUF; i++) {
		tspb = (TDMB_TSPB_INFO *)kmalloc(sizeof(TDMB_TSPB_INFO), GFP_DMA);
		if (tspb == NULL) {
			WARN(1, KERN_ERR "[tdmb_tspb_create_pool] %d TSP failed!\n", i);
			tdmb_tspb_delete_pool();
			return -ENOMEM;
		}

		tspb->size = 0;

		list_add_tail(&tspb->link, &tspb_pool.head);
		tspb_pool.cnt++;
	}

 	return 0;
}
#endif /* #if defined(CONFIG_TDMB_SPI) || defined(CONFIG_TDMB_EBI2) */

