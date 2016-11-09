/*
 * File name: tdmb_debug.h
 *
 * Description: TDMB debug macro header file.
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
 */

#ifndef __TDMB_DEBUG_H__
#define __TDMB_DEBUG_H__

#include "tdmb.h"

#ifdef DEBUG_TSP_BUF
static INLINE void reset_debug_tspb_stat(void)
{
	tdmb_cb_ptr->max_alloc_tspb_cnt = 0;
	tdmb_cb_ptr->max_enqueued_tspb_cnt = 0;
	tdmb_cb_ptr->alloc_tspb_err_cnt = 0;	
}
#define RESET_DEBUG_TSPB_STAT	reset_debug_tspb_stat()

#else
#define RESET_DEBUG_TSPB_STAT	do {} while (0)
#endif /* #ifdef DEBUG_TSP_BUF*/


#ifdef DEBUG_INTERRUPT
static inline void reset_debug_interrupt_stat(void)
{
	tdmb_cb_ptr->invalid_intr_cnt = 0;
	tdmb_cb_ptr->level_intr_cnt = 0;
	tdmb_cb_ptr->ovf_intr_cnt = 0;
	tdmb_cb_ptr->udf_intr_cnt = 0;	
}

#define RESET_DEBUG_INTR_STAT	reset_debug_interrupt_stat()
#define DMB_LEVEL_INTR_INC	tdmb_cb_ptr->level_intr_cnt++;
#define DMB_INV_INTR_INC	tdmb_cb_ptr->invalid_intr_cnt++;
#define DMB_OVF_INTR_INC	tdmb_cb_ptr->ovf_intr_cnt++;
#define DMB_UDF_INTR_INC	tdmb_cb_ptr->udf_intr_cnt++;

#else
#define RESET_DEBUG_INTR_STAT	do {} while (0)
#define DMB_LEVEL_INTR_INC	do {} while (0)
#define DMB_INV_INTR_INC	do {} while (0)
#define DMB_OVF_INTR_INC	do {} while (0)
#define DMB_UDF_INTR_INC	do {} while (0)
#endif /* #ifdef DEBUG_INTERRUPT*/


#if defined(DEBUG_TSP_BUF) && defined(DEBUG_INTERRUPT)
	#define SHOW_TDMB_DEBUG_STAT	\
	do {	\
		DMBMSG("ovf(%ld), udf(%ld), inv(%ld), level(%ld),\n\
			\t max_alloc(%u), max_enqueued(%u), alloc_err(%ld)\n",\
		tdmb_cb_ptr->ovf_intr_cnt, tdmb_cb_ptr->udf_intr_cnt,\
		tdmb_cb_ptr->invalid_intr_cnt, tdmb_cb_ptr->level_intr_cnt,\
		tdmb_cb_ptr->max_alloc_tspb_cnt,\
		tdmb_cb_ptr->max_enqueued_tspb_cnt,\
		tdmb_cb_ptr->alloc_tspb_err_cnt);\
	} while (0)
#elif !defined(DEBUG_TSP_BUF) && defined(DEBUG_INTERRUPT)
	#define SHOW_TDMB_DEBUG_STAT	\
	do {	\
		DMBMSG("ovf(%ld), udf(%ld), inv(%ld), level(%ld)\n",\
			tdmb_cb_ptr->ovf_intr_cnt, tdmb_cb_ptr->udf_intr_cnt,\
			tdmb_cb_ptr->invalid_intr_cnt, tdmb_cb_ptr->level_intr_cnt);\
	} while (0)

#elif defined(DEBUG_TSP_BUF) && !defined(DEBUG_INTERRUPT)
	#define SHOW_TDMB_DEBUG_STAT	\
	do {	\
		DMBMSG("max_alloc(%u), max_enqueued(%u), alloc_err(%ld)\n",\
			tdmb_cb_ptr->max_alloc_tspb_cnt,\
			tdmb_cb_ptr->max_enqueued_tspb_cnt,\
			tdmb_cb_ptr->alloc_tspb_err_cnt);\
	} while (0)

#elif !defined(DEBUG_TSP_BUF) && !defined(DEBUG_INTERRUPT)
	#define SHOW_TDMB_DEBUG_STAT		do {} while (0)
#endif

static inline int tdmb_msc_dump_kfile_write(char *buf, size_t len)
{
#ifdef _TDMB_KERNEL_FILE_DUMP_ENABLE
	mm_segment_t oldfs;
	struct file *filp;
	int ret = 0;

	if (tdmb_msc_dump_filp != NULL) {
		filp = tdmb_msc_dump_filp;
		oldfs = get_fs();
		set_fs(KERNEL_DS);
		ret = filp->f_op->write(filp, buf, len, &filp->f_pos);
		set_fs(oldfs);
		if (!ret)
			DMBERR("File write error (%d)\n", ret);
	} else
		DMBERR("tdmb_msc_dump_filp is NULL\n");

	return ret;
#else
	return 0;
#endif
}

static inline void tdmb_msc_dump_kfile_close(void)
{
#ifdef _TDMB_KERNEL_FILE_DUMP_ENABLE
	if (tdmb_msc_dump_filp != NULL) {
		filp_close(tdmb_msc_dump_filp, NULL);
		tdmb_msc_dump_filp = NULL;

		DMBMSG("Kernel dump file closed...\n");
	}
#endif
}

static inline int tdmb_msc_dump_kfile_open(unsigned int channel)
{
#ifdef _TDMB_KERNEL_FILE_DUMP_ENABLE
	char fname[32];
	struct file *filp = NULL;

	if (tdmb_msc_dump_filp == NULL) {
		sprintf(fname, "/data/local/tdmb_msc_%u.ts", channel);
		filp = filp_open(fname, O_RDWR|O_CREAT|O_TRUNC, S_IRUSR);

		if (IS_ERR(filp)) {
			filp = NULL;
			DMBERR("File open error: %s!\n", fname);
			return PTR_ERR(filp);
		}

		tdmb_msc_dump_filp = filp;

		DMBMSG("Kernel dump file opened(%s)\n", fname);
	} else {
		DMBERR("Already TS file opened! Should closed!\n");
		return -1;
	}
#endif

	return 0;
}

static inline int tdmb_fic_dump_kfile_write(char *buf, size_t len)
{
#ifdef _TDMB_KERNEL_FILE_DUMP_ENABLE
	mm_segment_t oldfs;
	struct file *filp;
	int ret = 0;

	if (tdmb_fic_dump_filp != NULL) {
		filp = tdmb_fic_dump_filp;
		oldfs = get_fs();
		set_fs(KERNEL_DS);
		ret = filp->f_op->write(filp, buf, len, &filp->f_pos);
		set_fs(oldfs);
		if (!ret)
			DMBERR("File write error (%d)\n", ret);
	} else
		DMBERR("tdmb_fic_dump_filp is NULL\n");

	return ret;
#else
	return 0;
#endif
}

static inline void tdmb_fic_dump_kfile_close(void)
{
#ifdef _TDMB_KERNEL_FILE_DUMP_ENABLE
	if (tdmb_fic_dump_filp != NULL) {
		filp_close(tdmb_fic_dump_filp, NULL);
		tdmb_fic_dump_filp = NULL;

		DMBMSG("Kernel dump file closed...\n");
	}
#endif
}

static inline int tdmb_fic_dump_kfile_open(unsigned int channel)
{
#ifdef _TDMB_KERNEL_FILE_DUMP_ENABLE
	char fname[32];
	struct file *filp = NULL;

	if (tdmb_fic_dump_filp == NULL) {
		sprintf(fname, "/data/local/tdmb_fic_%u.ts", channel);
		filp = filp_open(fname, O_RDWR|O_CREAT|O_TRUNC, S_IRUSR);

		if (IS_ERR(filp)) {
			filp = NULL;
			DMBERR("File open error: %s!\n", fname);
			return PTR_ERR(filp);
		}

		tdmb_fic_dump_filp = filp;

		DMBMSG("FIC file opened(%s). filp(0x%p)\n", fname, filp);
	} else {
		DMBERR("Already TS file opened! Should closed!\n");
		return -1;
	}
#endif

	return 0;
}

#endif /* __TDMB_DEBUG_H__*/


