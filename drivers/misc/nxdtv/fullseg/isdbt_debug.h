/*
 * File name: isdbt_debug.h
 *
 * Description: ISDBT debug macro header file.
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
 */

#ifndef __ISDBT_DEBUG_H__
#define __ISDBT_DEBUG_H__

#include "isdbt.h"

#ifdef DEBUG_TSP_BUF
static INLINE void reset_debug_tspb_stat(void)
{
	isdbt_cb_ptr->max_alloc_seg_cnt = 0;
	isdbt_cb_ptr->max_enqueued_seg_cnt = 0;
	isdbt_cb_ptr->max_enqueued_tsp_cnt = 0;
	isdbt_cb_ptr->alloc_tspb_err_cnt = 0;	
}
#define RESET_DEBUG_TSPB_STAT	reset_debug_tspb_stat()

#else
#define RESET_DEBUG_TSPB_STAT	do {} while (0)
#endif /* #ifdef DEBUG_TSP_BUF*/


#ifdef DEBUG_INTERRUPT
static inline void reset_debug_interrupt_stat(void)
{
	isdbt_cb_ptr->invalid_intr_cnt = 0;
	isdbt_cb_ptr->level_intr_cnt = 0;
	isdbt_cb_ptr->ovf_intr_cnt = 0;
	isdbt_cb_ptr->udf_intr_cnt = 0;	
}

#define RESET_DEBUG_INTR_STAT	reset_debug_interrupt_stat()
#define DMB_LEVEL_INTR_INC	isdbt_cb_ptr->level_intr_cnt++;
#define DMB_INV_INTR_INC	isdbt_cb_ptr->invalid_intr_cnt++;
#define DMB_OVF_INTR_INC	isdbt_cb_ptr->ovf_intr_cnt++;
#define DMB_UNF_INTR_INC	isdbt_cb_ptr->udf_intr_cnt++;

#else
#define RESET_DEBUG_INTR_STAT	do {} while (0)
#define DMB_LEVEL_INTR_INC	do {} while (0)
#define DMB_INV_INTR_INC	do {} while (0)
#define DMB_OVF_INTR_INC	do {} while (0)
#define DMB_UNF_INTR_INC	do {} while (0)
#endif /* #ifdef DEBUG_INTERRUPT*/


#if defined(DEBUG_TSP_BUF) && defined(DEBUG_INTERRUPT)
	#define SHOW_ISDBT_DEBUG_STAT	\
	do {	\
		DMBMSG("ovf(%ld), unf(%ld), inv(%ld), level(%ld),\n\
			\t max_alloc_seg(%u), max_enqueued_seg(%u),\n\
			\t max_enqueued_tsp(%u), alloc_err(%ld)\n",\
		isdbt_cb_ptr->ovf_intr_cnt, isdbt_cb_ptr->udf_intr_cnt,\
		isdbt_cb_ptr->invalid_intr_cnt, isdbt_cb_ptr->level_intr_cnt,\
		isdbt_cb_ptr->max_alloc_seg_cnt,\
		isdbt_cb_ptr->max_enqueued_seg_cnt,\
		isdbt_cb_ptr->max_enqueued_tsp_cnt,\
		isdbt_cb_ptr->alloc_tspb_err_cnt);\
	} while (0)
#elif !defined(DEBUG_TSP_BUF) && defined(DEBUG_INTERRUPT)
	#define SHOW_ISDBT_DEBUG_STAT	\
	do {	\
		DMBMSG("ovf(%ld), unf(%ld), inv(%ld), level(%ld)\n",\
			isdbt_cb_ptr->ovf_intr_cnt, isdbt_cb_ptr->udf_intr_cnt,\
			isdbt_cb_ptr->invalid_intr_cnt, isdbt_cb_ptr->level_intr_cnt);\
	} while (0)

#elif defined(DEBUG_TSP_BUF) && !defined(DEBUG_INTERRUPT)
	#define SHOW_ISDBT_DEBUG_STAT	\
	do {	\
		DMBMSG("max_alloc_seg(%u), max_enqueued_seg(%u)	max_enqueued_tsp(%u), alloc_err(%ld)\n",\
			isdbt_cb_ptr->max_alloc_seg_cnt,\
			isdbt_cb_ptr->max_enqueued_seg_cnt,\
			isdbt_cb_ptr->max_enqueued_tsp_cnt,\
			isdbt_cb_ptr->alloc_tspb_err_cnt);\
	} while (0)

#elif !defined(DEBUG_TSP_BUF) && !defined(DEBUG_INTERRUPT)
	#define SHOW_ISDBT_DEBUG_STAT		do {} while (0)
#endif

#endif /* __ISDBT_DEBUG_H__*/


