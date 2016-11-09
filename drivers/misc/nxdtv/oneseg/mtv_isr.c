/*
 * File name: mtv_isr.c
 *
 * Description: MTV ISR driver.
 *
 * Copyright (C) (2011, NEXELL)
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

#include "./src/nxtv_internal.h"
#include "mtv.h"

 
/* NXTV_REG_GET(reg) */
#define NUM_ISTATUS_BUF				1
#define MSC_FIC_INTR_BUF_IDX		0	

#ifdef NXTV_FIC_I2C_INTR_ENABLED
static INLINE void proc_i2c_fic(U8 istatus)
{
	if( istatus & FIC_E_INT )
	{
//		DMBMSG("FIC_E_INT occured!\n");

		kill_fasync(&mtv_1seg_cb_ptr->fasync, SIGIO, POLL_IN);

		NXTV_REG_MAP_SEL(DD_PAGE);
		NXTV_REG_SET(INT_E_UCLRL, 0x01);
	}
}
#endif /* #ifdef NXTV_FIC_I2C_INTR_ENABLED */

#ifdef NXTV_FIC_SPI_INTR_ENABLED
static INLINE void read_fic(MTV_TS_PKT_INFO *tsp)
{
#if defined(NXTV_TDMB_ENABLE)
	unsigned int size = 384;
#elif defined(NXTV_DAB_ENABLE)
	unsigned int size = mtv_1seg_cb_ptr->fic_size;

	if(size == 0)
	{	/* This case can be occured at weak signal area. */
		if ((size = nxtv_GetFicSize()) == 0)
		{
			DMBERR("Weak singal\n");
			tsp->fic_size = 0;
			return;
		}
		mtv_1seg_cb_ptr->fic_size = size; /* Update FIC size. */
	}
#endif

	NXTV_REG_MAP_SEL(FIC_PAGE);
	NXTV_REG_BURST_GET(0x10, tsp->fic_buf, size+1);

	tsp->fic_size = size;
}

static INLINE MTV_TS_PKT_INFO *proc_spi_fic(MTV_TS_PKT_INFO *tsp, U8 istatus)
{
	if( istatus & FIC_E_INT )
	{
		/*DMBMSG("FIC_E_INT occured!\n");*/

		/* Allocate a TS packet from TSP pool 
		if MSC1 and MSC0 interrupts are not occured. */
		if(tsp == NULL)
			tsp = mtv_alloc_tsp_1seg();
		
		if(tsp != NULL)
			read_fic(tsp);
		else
			DMBERR("FIC: No more TSP buffer\n");

		NXTV_REG_MAP_SEL(DD_PAGE);
		NXTV_REG_SET(INT_E_UCLRL, 0x01);
	}

	return tsp;
}
#endif /* #ifdef NXTV_FIC_SPI_INTR_ENABLED */


#ifdef NXTV_SPI_MSC0_ENABLED
#ifdef NXTV_CIF_MODE_ENABLED
/* Only for DAB multiple DATA sub channels. */
static INLINE void read_cif_msc0(MTV_TS_PKT_INFO *tsp)
{
	unsigned int size;
	U8 msc_tsize[2+1];

	NXTV_REG_MAP_SEL(DD_PAGE);
	NXTV_REG_BURST_GET(MSC0_E_TSIZE_L, msc_tsize, 2+1);
	size = (msc_tsize[1] << 8) | msc_tsize[2];
	if(size <= (3*1024-1))
	{
		NXTV_REG_MAP_SEL(MSC0_PAGE);
		NXTV_REG_BURST_GET(0x10, tsp->msc0_buf, size+1);
		tsp->msc0_size = nxtv_VerifySpiCif_MSC0(&tsp->msc0_buf[1], size);
		if(tsp->msc0_size == size)
			return; /* OK */
	}

	NXTV_REG_MAP_SEL(DD_PAGE);
	nxtv_ClearAndSetupMemory_MSC0();	
	DMBERR("read: %u, valid: %d\n", size, tsp->msc0_size);
#ifdef DEBUG_MTV_IF_MEMORY
	mtv_1seg_cb_ptr->msc0_cife_cnt++;
#endif
}
#endif /* #ifdef NXTV_CIF_MODE_ENABLED */

/* TDMB/DAB threshold or FM RDS multi service. */
static INLINE void read_threshold_msc0(MTV_TS_PKT_INFO *tsp)
{
	unsigned int size = mtv_1seg_cb_ptr->msc0_thres_size;

	NXTV_REG_MAP_SEL(MSC0_PAGE);
	NXTV_REG_BURST_GET(0x10, tsp->msc0_buf, size+1); 
	tsp->msc0_size = size;
}

static INLINE void read_msc0(MTV_TS_PKT_INFO *tsp)
{
#if defined(NXTV_TDMBorDAB_ONLY_ENABLED)
	#ifdef NXTV_CIF_MODE_ENABLED
	read_cif_msc0(tsp);
	#else
	read_threshold_msc0(tsp);
	#endif
	
#else
	/* TDMB/DAB/FM. */
	switch (mtv_1seg_cb_ptr->tv_mode)
	{
	#if defined(NXTV_TDMB_ENABLE) || defined(NXTV_DAB_ENABLE)
	case DMB_TV_MODE_TDMB:
	case DMB_TV_MODE_DAB:
		#ifdef NXTV_CIF_MODE_ENABLED
		read_cif_msc0(tsp);
		#else
		read_threshold_msc0(tsp);
		#endif
		break;
	#endif

	#ifdef NXTV_FM_ENABLE
	case DMB_TV_MODE_FM:
		read_threshold_msc0(tsp);
		break;	
	#endif

	default: /* Do nothing */
		break;
	}
#endif /* #ifdef NXTV_TDMBorDAB_ONLY_ENABLED */


#ifdef DEBUG_MTV_IF_MEMORY
	mtv_1seg_cb_ptr->msc0_ts_intr_cnt++;
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
* FM: 1 RDS data. defined(NXTV_FM_RDS_ENABLE). NOT implemeted.
*==================================================================*/
static INLINE MTV_TS_PKT_INFO *proc_msc0(MTV_TS_PKT_INFO *tsp, U8 istatus)
{
	if (istatus & MSC0_INTR_BITS) {
		if (istatus & (MSC0_E_UNDER_FLOW|MSC0_E_OVER_FLOW)) {
			NXTV_REG_MAP_SEL(DD_PAGE);						
			nxtv_ClearAndSetupMemory_MSC0();
			NXTV_REG_SET(INT_E_UCLRL, 0x02);

			DMBMSG("MSC0 OF/UF: 0x%02X\n", istatus);

		#ifdef DEBUG_MTV_IF_MEMORY
			mtv_1seg_cb_ptr->msc0_ovf_intr_cnt++;
		#endif	
		}
		else {
			/* Allocate a TS packet from TSP pool if MSC1 not occured interrupt. */
			if (tsp == NULL)
				tsp = mtv_alloc_tsp_1seg();
			
			if (tsp != NULL)
				read_msc0(tsp);
			else
				DMBERR("MSC0: No more TSP buffer.\n");
	
			NXTV_REG_MAP_SEL(DD_PAGE);
			NXTV_REG_SET(INT_E_UCLRL, 0x02); /* MSC0 Interrupt clear. */
		}
	}

	return tsp;
}
#endif /* #ifdef NXTV_SPI_MSC0_ENABLED */  


#ifdef NXTV_SPI_MSC1_ENABLED
static INLINE void read_msc1(MTV_TS_PKT_INFO *tsp)
{
	unsigned int size = mtv_1seg_cb_ptr->msc1_thres_size;

//printk("isr: mtv_1seg_cb_ptr->msc1_thres_size(%u)\n", mtv_1seg_cb_ptr->msc1_thres_size);
	
	NXTV_REG_MAP_SEL(MSC1_PAGE);					
	NXTV_REG_BURST_GET(0x10, tsp->msc1_buf, size+1/*0xFF*/); 
	tsp->msc1_size = size; 

#ifdef DEBUG_MTV_IF_MEMORY
	mtv_1seg_cb_ptr->msc1_ts_intr_cnt++;
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
static INLINE MTV_TS_PKT_INFO *proc_msc1(MTV_TS_PKT_INFO *tsp, U8 istatus)
{
	if (istatus & MSC1_INTR_BITS) {
		if (istatus & (MSC1_E_UNDER_FLOW|MSC1_E_OVER_FLOW)) {
			nxtv_ClearAndSetupMemory_MSC1(mtv_1seg_cb_ptr->tv_mode);
			/* Clear MSC1 Interrupt. */
			NXTV_REG_SET(INT_E_UCLRL, 0x04);
		
			DMBMSG("MSC1 OF/UF: 0x%02X\n", istatus);
		
#ifdef DEBUG_MTV_IF_MEMORY
			mtv_1seg_cb_ptr->msc1_ovf_intr_cnt++;
#endif
		}
		else {
			/* Allocate a TS packet from TSP pool. */
			tsp = mtv_alloc_tsp_1seg();
			if(tsp != NULL)
				read_msc1(tsp);
			else
				DMBERR("MSC1: No more TSP buffer.\n");

			NXTV_REG_MAP_SEL(DD_PAGE);
			NXTV_REG_SET(INT_E_UCLRL, 0x04);
		}
	}

	return tsp;
}
#endif /* #ifdef NXTV_SPI_MSC1_ENABLED */

static UINT __isr_cnt;
static UINT __isr_thread_cnt;



#if defined(NXTV_IF_SPI) || defined(NXTV_FIC_I2C_INTR_ENABLED)
static void mtv_isr_handler(void)
{
#if defined(NXTV_IF_SPI)
	MTV_TS_PKT_INFO *tsp = NULL; /* reset */
#endif
	U8 intr_status[NUM_ISTATUS_BUF];
	//U8 itt_intr_status;

__isr_thread_cnt++;

	NXTV_GUARD_LOCK;

	/* Read the register of interrupt status. */
	NXTV_REG_MAP_SEL(DD_PAGE);
	intr_status[0] = NXTV_REG_GET(INT_E_STATL);
	//DMBMSG("[isr: %u] 0x%02X\n", __isr_cnt, intr_status[0]);

#if 0
	itt_intr_status = NXTV_REG_GET(0x34);
	if (itt_intr_status & 0x40) {
		NXTV_TDMB_TII_INFO tii;

		nxtv_GetTii(&tii);

		DMBMSG("tii_combo(0x%02X), tii_pattern(0x%02X)\n",
			tii.tii_combo, tii.tii_pattern);
	}
#endif

#ifdef NXTV_SPI_MSC1_ENABLED
	/* TDMB/DAB: 1 VIDEO or 1 AUDIO or 1 DATA. 1 seg: 1 VIDEO. FM: PCM. */
	tsp = proc_msc1(tsp, intr_status[MSC_FIC_INTR_BUF_IDX]);
#endif

#ifdef NXTV_SPI_MSC0_ENABLED
	/* TDMB/DAB: Max 4 DATA data. FM: 1 RDS data(NOT implemeted). */
	tsp = proc_msc0(tsp, intr_status[MSC_FIC_INTR_BUF_IDX]); 
#endif

	/* Processing FIC interrupt. */
#if defined(NXTV_FIC_SPI_INTR_ENABLED)
	tsp = proc_spi_fic(tsp, intr_status[MSC_FIC_INTR_BUF_IDX]);
#elif defined(NXTV_FIC_I2C_INTR_ENABLED)
	proc_i2c_fic(intr_status[MSC_FIC_INTR_BUF_IDX]);
#endif

	NXTV_GUARD_FREE;

#if defined(NXTV_IF_SPI)
	/* Enqueue a ts packet into ts data queue if a packet was exist. */
	if (tsp != NULL) {
		mtv_put_tsp_1seg(tsp);

		/* Wake up threads blocked on read() function
			if the file is open_flag in blocking mode. */
		if ((mtv_1seg_cb_ptr->f_flags & O_NONBLOCK) == 0) {
			if (!mtv_1seg_cb_ptr->first_interrupt) {
				if ((get_tsp_queue_count_1seg() >= mtv_1seg_cb_ptr->wakeup_tspq_thres_cnt))
					wake_up_interruptible(&mtv_1seg_cb_ptr->read_wq);
			} else {
				mtv_1seg_cb_ptr->first_interrupt = false;
				wake_up_interruptible(&mtv_1seg_cb_ptr->read_wq);
			}
		}			
	}
#endif /* #if defined(NXTV_IF_SPI) */

#if 0
	if ((__isr_thread_cnt % 30) == 0) {
		printk("Delayed... tsp_q_cnt(%u)\n", get_tsp_queue_count_1seg());
		NXTV_DELAY_MS(100);
	}
#endif
}


int mtv_isr_thread_1seg(void *data)
{
	DMBMSG("Start 0 ...\n");

	set_user_nice(current, -10);
	
	while (!kthread_should_stop()) {
		wait_event_interruptible(mtv_1seg_cb_ptr->isr_wq,
				kthread_should_stop() || mtv_1seg_cb_ptr->isr_cnt);

		if (kthread_should_stop())
			break;

		if (mtv_1seg_cb_ptr->is_power_on == TRUE) {
			mtv_isr_handler();
			mtv_1seg_cb_ptr->isr_cnt--;
		}
	}

	DMBMSG("Exit.\n");

	return 0;
}

irqreturn_t mtv_isr_1seg(int irq, void *param)
{
	if(mtv_1seg_cb_ptr->is_power_on == TRUE) {
	__isr_cnt++;
		mtv_1seg_cb_ptr->isr_cnt++;
		wake_up_interruptible(&mtv_1seg_cb_ptr->isr_wq);
	}

	return IRQ_HANDLED;
}

#endif /* #if defined(NXTV_IF_SPI) ||defined(NXTV_FIC_I2C_INTR_ENABLED) */


