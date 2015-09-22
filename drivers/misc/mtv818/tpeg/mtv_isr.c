/*
 * File name: mtv_isr.c
 *
 * Description: MTV ISR driver.
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
 */

#include "./src/raontv_internal.h"
#include "mtv.h"

 
/* RTV_REG_GET(reg) */
#define NUM_ISTATUS_BUF				1
#define MSC_FIC_INTR_BUF_IDX		0	

#ifdef RTV_FIC_I2C_INTR_ENABLED
static INLINE void proc_i2c_fic(U8 istatus)
{
	if( istatus & FIC_E_INT )
	{
//		DMBMSG("FIC_E_INT occured!\n");

		kill_fasync(&mtv_cb_ptr->fasync, SIGIO, POLL_IN);

		RTV_REG_MAP_SEL(DD_PAGE);
		RTV_REG_SET(INT_E_UCLRL, 0x01);
	}
}
#endif /* #ifdef RTV_FIC_I2C_INTR_ENABLED */

#ifdef RTV_FIC_SPI_INTR_ENABLED
static INLINE void read_fic(struct mtv_cb * mtv_cb_ptr, MTV_TS_PKT_INFO *tsp)
{
#if defined(RTV_TDMB_ENABLE)
	unsigned int size = 384;
#elif defined(RTV_DAB_ENABLE)
	unsigned int size = mtv_cb_ptr->fic_size;

	if(size == 0)
	{	/* This case can be occured at weak signal area. */
		if ((size = rtv_GetFicSize(mtv_cb_ptr->demod_no)) == 0)
		{
			DMBERR("Weak singal\n");
			tsp->fic_size = 0;
			return;
		}
		mtv_cb_ptr->fic_size = size; /* Update FIC size. */
	}
#endif

	RTV_REG_MAP_SEL(mtv_cb_ptr->demod_no, FIC_PAGE);
	RTV_REG_BURST_GET(mtv_cb_ptr->demod_no, 0x10, tsp->fic_buf, size+1);

	tsp->fic_size = size;
}

static INLINE MTV_TS_PKT_INFO *proc_spi_fic(struct mtv_cb * mtv_cb_ptr, 
									MTV_TS_PKT_INFO *tsp, U8 istatus)
{
	if( istatus & FIC_E_INT )
	{
		/*DMBMSG("FIC_E_INT occured!\n");*/

		/* Allocate a TS packet from TSP pool 
		if MSC1 and MSC0 interrupts are not occured. */
		if(tsp == NULL)
			tsp = mtv_alloc_tsp_tpeg(mtv_cb_ptr);
		
		if(tsp != NULL)
			read_fic(mtv_cb_ptr, tsp);
		else
			DMBERR("FIC: No more TSP buffer\n");

		RTV_REG_MAP_SEL(mtv_cb_ptr->demod_no, DD_PAGE);
		RTV_REG_SET(mtv_cb_ptr->demod_no, INT_E_UCLRL, 0x01);
	}

	return tsp;
}
#endif /* #ifdef RTV_FIC_SPI_INTR_ENABLED */


#ifdef RTV_SPI_MSC0_ENABLED
#ifdef RTV_CIF_MODE_ENABLED
/* Only for DAB multiple DATA sub channels. */
static INLINE void read_cif_msc0(struct mtv_cb * mtv_cb_ptr, MTV_TS_PKT_INFO *tsp)
{
	unsigned int size;
	U8 msc_tsize[2+1];

	RTV_REG_MAP_SEL(mtv_cb_ptr->demod_no, DD_PAGE);
	RTV_REG_BURST_GET(mtv_cb_ptr->demod_no, MSC0_E_TSIZE_L, msc_tsize, 2+1);
	size = (msc_tsize[1] << 8) | msc_tsize[2];
	if(size <= (3*1024-1))
	{
		RTV_REG_MAP_SEL(mtv_cb_ptr->demod_no, MSC0_PAGE);
		RTV_REG_BURST_GET(mtv_cb_ptr->demod_no, 0x10, tsp->msc0_buf, size+1);
		tsp->msc0_size = rtv_VerifySpiCif_MSC0(mtv_cb_ptr->demod_no, &tsp->msc0_buf[1], size);
		if(tsp->msc0_size == size)
			return; /* OK */
	}

	RTV_REG_MAP_SEL(mtv_cb_ptr->demod_no, DD_PAGE);
	rtv_ClearAndSetupMemory_MSC0(mtv_cb_ptr->demod_no);	
	DMBERR("read: %u, valid: %d\n", size, tsp->msc0_size);
#ifdef DEBUG_MTV_IF_MEMORY
	mtv_cb_ptr->msc0_cife_cnt++;
#endif
}
#endif /* #ifdef RTV_CIF_MODE_ENABLED */

/* TDMB/DAB threshold or FM RDS multi service. */
static INLINE void read_threshold_msc0(MTV_TS_PKT_INFO *tsp)
{
	unsigned int size = mtv_cb_ptr->msc0_thres_size;

	RTV_REG_MAP_SEL(MSC0_PAGE);
	RTV_REG_BURST_GET(0x10, tsp->msc0_buf, size+1); 
	tsp->msc0_size = size;
}

static INLINE void read_msc0(struct mtv_cb * mtv_cb_ptr, MTV_TS_PKT_INFO *tsp)
{
#if defined(RTV_TDMBorDAB_ONLY_ENABLED)
	#ifdef RTV_CIF_MODE_ENABLED
	read_cif_msc0(mtv_cb_ptr, tsp);
	#else
	read_threshold_msc0(mtv_cb_ptr, tsp);
	#endif
	
#else
	/* TDMB/DAB/FM. */
	switch (mtv_cb_ptr->tv_mode)
	{
	#if defined(RTV_TDMB_ENABLE) || defined(RTV_DAB_ENABLE)
	case DMB_TV_MODE_TDMB:
	case DMB_TV_MODE_DAB:
		#ifdef RTV_CIF_MODE_ENABLED
		read_cif_msc0(mtv_cb_ptr, tsp);
		#else
		read_threshold_msc0(mtv_cb_ptr, tsp);
		#endif
		break;
	#endif

	#ifdef RTV_FM_ENABLE
	case DMB_TV_MODE_FM:
		read_threshold_msc0(mtv_cb_ptr, tsp);
		break;	
	#endif

	default: /* Do nothing */
		break;
	}
#endif /* #ifdef RTV_TDMBorDAB_ONLY_ENABLED */


#ifdef DEBUG_MTV_IF_MEMORY
	mtv_cb_ptr->msc0_ts_intr_cnt++;
#endif

#if 0
	DMBMSG("[MSC0: %u] [0x%02X], [0x%02X], [0x%02X], [0x%02X]\n",
		tsp->msc0_size,
		tsp->msc0_buf[1], tsp->msc0_buf[2],
		tsp->msc0_buf[3],tsp->msc0_buf[4]);	
#endif
}

/*===================================================================
* Processing MSC0 interrupt.
* TDMB/DAB: Max 4 DATA data
* FM: 1 RDS data. defined(RTV_FM_RDS_ENABLE). NOT implemeted.
*==================================================================*/
static INLINE MTV_TS_PKT_INFO *proc_msc0(struct mtv_cb * mtv_cb_ptr,
									MTV_TS_PKT_INFO *tsp, U8 istatus)
{
	if (istatus & MSC0_INTR_BITS) {
		if (istatus & (MSC0_E_UNDER_FLOW|MSC0_E_OVER_FLOW)) {
			RTV_REG_MAP_SEL(DD_PAGE);						
			rtv_ClearAndSetupMemory_MSC0(mtv_cb_ptr->demod_no);
			RTV_REG_SET(mtv_cb_ptr->demod_no, INT_E_UCLRL, 0x02);

			DMBMSG("MSC0 OF/UF: 0x%02X\n", istatus);

		#ifdef DEBUG_MTV_IF_MEMORY
			mtv_cb_ptr->msc0_ovf_intr_cnt++;
		#endif	
		}
		else {
			/* Allocate a TS packet from TSP pool if MSC1 not occured interrupt. */
			if (tsp == NULL)
				tsp = mtv_alloc_tsp_tpeg(mtv_cb_ptr);
			
			if (tsp != NULL)
				read_msc0(mtv_cb_ptr, tsp);
			else
				DMBERR("MSC0: No more TSP buffer.\n");
	
			RTV_REG_MAP_SEL(mtv_cb_ptr->demod_no, DD_PAGE);
			RTV_REG_SET(mtv_cb_ptr->demod_no, INT_E_UCLRL, 0x02); /* MSC0 Interrupt clear. */
		}
	}

	return tsp;
}
#endif /* #ifdef RTV_SPI_MSC0_ENABLED */  


#ifdef RTV_SPI_MSC1_ENABLED
static INLINE void read_msc1(struct mtv_cb * mtv_cb_ptr, MTV_TS_PKT_INFO *tsp)
{
	unsigned int size = mtv_cb_ptr->msc1_thres_size;

//printk("isr: mtv_cb_ptr->msc1_thres_size(%u)\n", mtv_cb_ptr->msc1_thres_size);
	
	RTV_REG_MAP_SEL(mtv_cb_ptr->demod_no, MSC1_PAGE);					
	RTV_REG_BURST_GET(mtv_cb_ptr->demod_no, 0x10, tsp->msc1_buf, size+1/*0xFF*/); 
	tsp->msc1_size = size; 

#ifdef DEBUG_MTV_IF_MEMORY
	mtv_cb_ptr->msc1_ts_intr_cnt++;
#endif

#if 0
	DMBMSG("[MSC1: %u] [0x%02X], [0x%02X], [0x%02X], [0x%02X]\n",
		tsp->msc1_size, 
		tsp->msc1_buf[1], tsp->msc1_buf[2],
		tsp->msc1_buf[3],tsp->msc1_buf[4]);
#endif	
}

/*===================================================================
* Processing MSC1 interrupt. The first processing in ISR.
* TDMB/DAB: 1 VIDEO or 1 AUDIO or 1 DATA.
* 1 seg: 1 VIDEO data
* FM: 1 PCM data
*==================================================================*/
static INLINE MTV_TS_PKT_INFO *proc_msc1(struct mtv_cb * mtv_cb_ptr, MTV_TS_PKT_INFO *tsp, U8 istatus)
{
	int demod_no = mtv_cb_ptr->demod_no;
	
	if (istatus & MSC1_INTR_BITS) {
		if (istatus & (MSC1_E_UNDER_FLOW|MSC1_E_OVER_FLOW)) {
			rtv_ClearAndSetupMemory_MSC1(demod_no, mtv_cb_ptr->tv_mode);
			/* Clear MSC1 Interrupt. */
			RTV_REG_SET(demod_no, INT_E_UCLRL, 0x04);
		
			DMBMSG("MSC1 OF/UF: 0x%02X\n", istatus);
		
#ifdef DEBUG_MTV_IF_MEMORY
			mtv_cb_ptr->msc1_ovf_intr_cnt++;
#endif
		}
		else {
			/* Allocate a TS packet from TSP pool. */
			tsp = mtv_alloc_tsp_tpeg(mtv_cb_ptr);
			if(tsp != NULL)
				read_msc1(mtv_cb_ptr, tsp);
			else
				DMBERR("MSC1: No more TSP buffer.\n");

			RTV_REG_MAP_SEL(demod_no, DD_PAGE);
			RTV_REG_SET(demod_no, INT_E_UCLRL, 0x04);
		}
	}

	return tsp;
}
#endif /* #ifdef RTV_SPI_MSC1_ENABLED */

static UINT __isr_cnt;
static UINT __isr_thread_cnt;



#if defined(RTV_IF_SPI) || defined(RTV_FIC_I2C_INTR_ENABLED)
static void mtv_isr_handler(struct mtv_cb * mtv_cb_ptr)
{
#if defined(RTV_IF_SPI)
	MTV_TS_PKT_INFO *tsp = NULL; /* reset */
#endif
	U8 intr_status[NUM_ISTATUS_BUF];
	//U8 itt_intr_status;
	int demod_no = mtv_cb_ptr->demod_no;

__isr_thread_cnt++;

	RTV_GUARD_LOCK(demod_no);

	/* Read the register of interrupt status. */
	RTV_REG_MAP_SEL(demod_no, DD_PAGE);
	intr_status[0] = RTV_REG_GET(demod_no, INT_E_STATL);
//	DMBMSG("[isr: %u] 0x%02X\n", __isr_cnt, intr_status[0]);

#if 0
	itt_intr_status = RTV_REG_GET(0x34);
	if (itt_intr_status & 0x40) {
		RTV_TDMB_TII_INFO tii;

		rtv_GetTii(demod_no, &tii);

		DMBMSG("tii_combo(0x%02X), tii_pattern(0x%02X)\n",
			tii.tii_combo, tii.tii_pattern);
	}
#endif

#ifdef RTV_SPI_MSC1_ENABLED
	/* TDMB/DAB: 1 VIDEO or 1 AUDIO or 1 DATA. 1 seg: 1 VIDEO. FM: PCM. */
	tsp = proc_msc1(mtv_cb_ptr, tsp, intr_status[MSC_FIC_INTR_BUF_IDX]);
#endif

#ifdef RTV_SPI_MSC0_ENABLED
	/* TDMB/DAB: Max 4 DATA data. FM: 1 RDS data(NOT implemeted). */
	tsp = proc_msc0(mtv_cb_ptr, tsp, intr_status[MSC_FIC_INTR_BUF_IDX]); 
#endif

	/* Processing FIC interrupt. */
#if defined(RTV_FIC_SPI_INTR_ENABLED)
	tsp = proc_spi_fic(mtv_cb_ptr, tsp, intr_status[MSC_FIC_INTR_BUF_IDX]);
#elif defined(RTV_FIC_I2C_INTR_ENABLED)
	proc_i2c_fic(mtv_cb_ptr, intr_status[MSC_FIC_INTR_BUF_IDX]);
#endif

	RTV_GUARD_FREE(demod_no);

#if defined(RTV_IF_SPI)
	/* Enqueue a ts packet into ts data queue if a packet was exist. */
	if (tsp != NULL) {
		tpeg_mtv_put_tsp(demod_no, tsp);

		/* Wake up threads blocked on read() function
			if the file is open_flag in blocking mode. */
		if ((mtv_cb_ptr->f_flags & O_NONBLOCK) == 0) {
			if (!mtv_cb_ptr->first_interrupt) {
				if ((tpeg_get_tsp_queue_count(demod_no) >= mtv_cb_ptr->wakeup_tspq_thres_cnt))
					wake_up_interruptible(&mtv_cb_ptr->read_wq);
			} else {
				mtv_cb_ptr->first_interrupt = false;
				wake_up_interruptible(&mtv_cb_ptr->read_wq);
			}
		}			
	}
#endif /* #if defined(RTV_IF_SPI) */

#if 0
	if ((__isr_thread_cnt % 30) == 0) {
		printk("Delayed... tsp_q_cnt(%u)\n", tpeg_get_tsp_queue_count());
		RTV_DELAY_MS(100);
	}
#endif
}


int mtv_isr_thread_tpeg(void *data)
{
	struct mtv_cb * mtv_cb_ptr = mtv_cb_ptrs_tpeg;

	DMBMSG("Start 0 ...\n");

	set_user_nice(current, -10);
	
	while (!kthread_should_stop()) {
		wait_event_interruptible(mtv_cb_ptr->isr_wq,
				kthread_should_stop() || mtv_cb_ptr->isr_cnt);

		if (kthread_should_stop())
			break;

		if (mtv_cb_ptr->is_power_on == TRUE) {
			mtv_isr_handler(mtv_cb_ptr);
			mtv_cb_ptr->isr_cnt--;
		}
	}

	DMBMSG("Exit.\n");

	return 0;
}

irqreturn_t mtv_isr_tpeg(int irq, void *param)
{
	struct mtv_cb * mtv_cb_ptr = mtv_cb_ptrs_tpeg;
	
	if(mtv_cb_ptr->is_power_on == TRUE) {
	__isr_cnt++;
		mtv_cb_ptr->isr_cnt++;
		wake_up_interruptible(&mtv_cb_ptr->isr_wq);
	}

	return IRQ_HANDLED;
}
#endif /* #if defined(RTV_IF_SPI) ||defined(RTV_FIC_I2C_INTR_ENABLED) */


