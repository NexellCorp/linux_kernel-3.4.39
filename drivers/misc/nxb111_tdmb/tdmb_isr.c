/*
 * File name: tdmb_isr.c
 *
 * Description: TDMB ISR driver.
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

#include "mtv319.h"
#include "mtv319_internal.h"
#include "tdmb.h"
#include "tdmb_debug.h"

#include "tdmb_gpio.h"

unsigned int total_int_cnt = 0;


#ifdef RTV_FIC_I2C_INTR_ENABLED
static void tdmb_isr_handler(void)
{
	U8 istatus;
	U8 buf[MTV319_FIC_BUF_SIZE];
	UINT timeout_cnt = 100;

	RTV_GUARD_LOCK;

DMBMSG("ENTER...\n");
	RTV_REG_MAP_SEL(FEC_PAGE);
//	istatus = RTV_REG_GET(0x13);
//DMBMSG("istatus(0x%02X)\n", istatus);

#if 0
RTV_REG_MAP_SEL(HOST_PAGE);
printk("[ISR] HOST 0x1A(0x%02X) 0x08(0x%02X)\n",
	RTV_REG_GET(0x1A), RTV_REG_GET(0x08));

	RTV_REG_MAP_SEL(FEC_PAGE);
	printk("[ISR] FEC 0x11(0x%02X) 0x26(0x%02X) 0x17(0x%02X) 0xB2(0x%02X)\n",
		RTV_REG_GET(0x11), RTV_REG_GET(0x26),
		RTV_REG_GET(0x17), RTV_REG_GET(0xB2));
#endif

#if 1
	while (1) {
		istatus = RTV_REG_GET(0x13);
		printk("[ISR] istatus(0x%02X)\n", istatus);
		//if (istatus & 0x10) {
		if (istatus == 0x10) {
			//DMBMSG("I2C FIC interrupt occured!\n");

			kill_fasync(&tdmb_cb_ptr->fasync, SIGIO, POLL_IN);

		#if 0
			RTV_REG_MASK_SET(0x26, 0x10, 0x10);
			RTV_REG_BURST_GET(0x29, buf, 400);
			RTV_REG_SET(0x26, 0x01);

			/* FIC I2C memory clear. */
			RTV_REG_MASK_SET(0x26, 0x04, 0x04);
			RTV_REG_MASK_SET(0x26, 0x04, 0x00);

			/* FIC I2C interrupt status clear. */
			RTV_REG_SET(0x11, I2C_INTR_POL_ACTIVE|0x04);
			RTV_REG_SET(0x11, I2C_INTR_POL_ACTIVE);

			//RTV_REG_MASK_SET(0x11, 0x04, 0x04);
			//RTV_REG_MASK_SET(0x11, 0x04, 0x00);


			//memmove(pbBuf+215, buf+231, 169);

			printk("[ISR] 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X [0x%02X 0x%02X] | 0x%02X 0x%02X\n",
			buf[0], buf[1], buf[2], buf[3], buf[4], buf[5],
			buf[382], buf[383], buf[398], buf[399]);
		#endif

			break;
		}
		else {
			/* FIC I2C interrupt status clear. */
			RTV_REG_SET(0x11, I2C_INTR_POL_ACTIVE|0x04);
			RTV_REG_SET(0x11, I2C_INTR_POL_ACTIVE);
			break;
		}

		if (timeout_cnt--)
			RTV_DELAY_MS(1);
		else {
			break;
		}
	}

#else

	if (istatus & 0x10) {
		DMBMSG("I2C FIC interrupt occured!\n");

		///////
		RTV_REG_MASK_SET(0x26, 0x10, 0x10);
		RTV_REG_BURST_GET(0x29, buf, 400);
		RTV_REG_SET(0x26, 0x01);
		//////

		/* FIC I2C interrupt status clear. */
		RTV_REG_SET(0x11, I2C_INTR_POL_ACTIVE|0x04);
		RTV_REG_SET(0x11, I2C_INTR_POL_ACTIVE);
		
		/* FIC I2C memory clear. */
		RTV_REG_MASK_SET(0x26, 0x04, 0x04);
		RTV_REG_MASK_SET(0x26, 0x04, 0x00);


	printk("[ISR] 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X [0x%02X 0x%02X] | 0x%02X 0x%02X\n",
	buf[0], buf[1], buf[2], buf[3], buf[4], buf[5],
	buf[382], buf[383], buf[398], buf[399]);

		rtvTDMB_PutI2cIntrFic(buf);

		kill_fasync(&tdmb_cb_ptr->fasync, SIGIO, POLL_IN);
	}
	else {
	#if 1
		//RTV_REG_MAP_SEL(HOST_PAGE);
		//RTV_REG_SET(0x05, 0x00);

		//RTV_REG_SET(0x26, 0x01);

		RTV_REG_MAP_SEL(FEC_PAGE);
		RTV_REG_SET(0x11, I2C_INTR_POL_ACTIVE|0x01);
		RTV_REG_SET(0x11, I2C_INTR_POL_ACTIVE);

		/* FIC I2C memory clear. */
		RTV_REG_MASK_SET(0x26, 0x04, 0x04);
		RTV_REG_MASK_SET(0x26, 0x04, 0x00);
		
		//RTV_REG_MASK_SET(0x11, 0x04, 0x04);
		//RTV_REG_MASK_SET(0x11, 0x04, 0x00);
		//RTV_REG_MASK_SET(0x26, 0x04, 0x04); /* FIC mem clr */
		//RTV_REG_MASK_SET(0x26, 0x04, 0x00);
	#endif
		DMBMSG("Invalid FIC interrupt\n");
	}
#endif
	RTV_GUARD_FREE;
}

#elif defined(CONFIG_TDMB_SPI) || defined(CONFIG_TDMB_EBI2)
static void isr_print_tsp(U8 *tspb, UINT size)
{
#if 1
	unsigned int i, cnt = 4;
	const U8 *tsp_buf_ptr = (const U8 *)tspb;

	for (i = 0; i < cnt; i++, tsp_buf_ptr += 188)
	{
		DMBMSG("[%d] 0x%02X 0x%02X 0x%02X 0x%02X | 0x%02X\n",
			i, tsp_buf_ptr[0], tsp_buf_ptr[1],
			tsp_buf_ptr[2], tsp_buf_ptr[3],
			tsp_buf_ptr[187]);
	}
#endif
}

static inline void enqueue_wakeup(TDMB_TSPB_INFO *tspb)
{
	/* Enqueue a TSP buffer into ts queue. */
	tdmb_tspb_enqueue(tspb);

	//printk("[enqueue_wakeup_0] tdmb_tspb_queue_count(%u)\n", tdmb_tspb_queue_count());
	
	/* Wake up threads blocked on read() function
	if the file was opened as the blocking mode. */
	if ((tdmb_cb_ptr->f_flags & O_NONBLOCK) == 0) {
		if (!tdmb_cb_ptr->first_interrupt) {
			if ((tdmb_tspb_queue_count() >= tdmb_cb_ptr->wakeup_tspq_thres_cnt))
				wake_up_interruptible(&tdmb_cb_ptr->read_wq);
		} else {
			tdmb_cb_ptr->first_interrupt = false;
			wake_up_interruptible(&tdmb_cb_ptr->read_wq);
		}
	}

	//printk("[enqueue_wakeup_1] tdmb_tspb_queue_count(%u)\n", tdmb_tspb_queue_count());
}

extern void tdmb_spi_recover(unsigned char *buf, unsigned int size);

static void tdmb_isr_handler(void)
{
	TDMB_TSPB_INFO *tspb = NULL; /* reset */
	U8 *tspb_ptr = NULL;
	U8 ifreg, istatus;
	U8 ts_intr_cnt = 0; /* for debug */
	UINT intr_size;
	static U8 recover_buf[MTV319_SPI_CMD_SIZE + 32*188];

	//printk("\n");

	RTV_GUARD_LOCK;

	intr_size = rtvTDMB_GetInterruptLevelSize();

	RTV_REG_MAP_SEL(SPI_CTRL_PAGE);

	ifreg = RTV_REG_GET(0x55);
	if (ifreg != 0xAA)
	{
		tdmb_spi_recover(recover_buf, MTV319_SPI_CMD_SIZE + intr_size);
		DMBMSG("Interface error 1\n");
		DMB_INV_INTR_INC;
	}

	istatus = RTV_REG_GET(0x10);
	if (istatus & (U8)(~SPI_INTR_BITS)) {
		tdmb_spi_recover(recover_buf, MTV319_SPI_CMD_SIZE + intr_size);
		RTV_REG_SET(0x2A, 1);
		RTV_REG_SET(0x2A, 0);
		DMBMSG("Interface error 2 (0x%02X)\n", istatus);
		goto exit_read_mem;
	}	

	//DMBMSG("$$$$$$$$ ts_intr_cnt(%d), istatus(0x%02X)\n", ts_intr_cnt, istatus);

	if (istatus & SPI_UNDERFLOW_INTR)
	{
		RTV_REG_SET(0x2A, 1);
		RTV_REG_SET(0x2A, 0);
		DMBMSG("UDF: 0x%02X\n", istatus);
		DMB_UDF_INTR_INC;
		goto exit_read_mem;
	}

	if (istatus & SPI_THRESHOLD_INTR) {
		/* Allocate a TS packet from TSP pool. */
		tspb = tdmb_tspb_alloc_buffer();
		if (tspb) {
			tspb_ptr = tspb->buf;

		#if 0
			RTV_REG_MAP_SEL(SPI_CTRL_PAGE);
			ts_intr_cnt = RTV_REG_GET(0x12); // to be deleted
			if (ts_intr_cnt > 1)
				DMBMSG("1 more threshold (%d)\n", ts_intr_cnt);
		#endif

			RTV_REG_MAP_SEL(SPI_MEM_PAGE);
			RTV_REG_BURST_GET(0x10, tspb_ptr, intr_size);

#ifdef _TDMB_KERNEL_FILE_DUMP_ENABLE
			tdmb_msc_dump_kfile_write(tspb_ptr, intr_size);
			//tdmb_fic_dump_kfile_write(tspb_ptr, intr_size);
#endif

			tspb->size = intr_size;
			//isr_print_tsp(tspb_ptr, intr_size); /* To debug */

			//DMBMSG("Read TS data\n");
			//RTV_REG_BURST_GET(0x10, tspb_ptr, 1);

			/* To debug */
			if (istatus & SPI_OVERFLOW_INTR) {
				DMB_OVF_INTR_INC;
				DMBMSG("OVF: 0x%02X\n", istatus);
			}

			DMB_LEVEL_INTR_INC;
	
	#if 0 ////#### delay
			if ((total_int_cnt%20) == 0) {
				RTV_DELAY_MS(220);
				printk("[tdmb_isr_handler] Delayed!\n");
			}
	#endif			
		} else {
			RTV_REG_MAP_SEL(SPI_CTRL_PAGE);
			RTV_REG_SET(0x2A, 1); /* SRAM init */
			RTV_REG_SET(0x2A, 0);
			tdmb_cb_ptr->alloc_tspb_err_cnt++;
			DMBERR("No more TSP buffer from pool.\n");
		}
	}
	else {
		RTV_REG_MAP_SEL(SPI_CTRL_PAGE);
		tdmb_spi_recover(recover_buf, MTV319_SPI_CMD_SIZE + intr_size);
		RTV_REG_SET(0x2A, 1);
		RTV_REG_SET(0x2A, 0);
		DMBMSG("No data interrupt (0x%02X)\n", istatus);
	}

exit_read_mem:
	RTV_GUARD_FREE;

	if (tspb) /* Check if a tspb was exist to be enqueud? */
		enqueue_wakeup(tspb);
}
#endif /* #if defined(CONFIG_TDMB_SPI) || defined(CONFIG_TDMB_EBI2) */

#if defined(CONFIG_TDMB_SPI) || defined(CONFIG_TDMB_EBI2) || defined(RTV_FIC_I2C_INTR_ENABLED)

#define SCHED_FIFO_USE
irqreturn_t tdmb_irq_handler(int irq, void *param)
{
#ifdef SCHED_FIFO_USE
	struct sched_param sch_param = { .sched_priority = MAX_RT_PRIO - 1 };
#endif

	if (!tdmb_cb_ptr->irq_thread_sched_changed) {
#ifdef SCHED_FIFO_USE
		sched_setscheduler(current, SCHED_FIFO, &sch_param);
#else
		set_user_nice(current, -20);
#endif
		set_user_nice(current, -20);

		tdmb_cb_ptr->irq_thread_sched_changed = true;

		DMBMSG("Sched changed!\n");
	}

	tdmb_isr_handler();

	return IRQ_HANDLED;
}
#endif


