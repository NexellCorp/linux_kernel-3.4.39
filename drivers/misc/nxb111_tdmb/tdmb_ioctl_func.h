/*
 * File name: tdmb_ioctl_func.h
 *
 * Description: MTV IO control functions header file.
 *                   Only used in the mtv.c
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

#include "tdmb.h"

#if defined(RTV_IF_SPI) || defined(RTV_IF_EBI2)
static unsigned long diff_jiffies_1st, diff_jiffies0, hours_cnt;
#endif

/*============================================================================
 * Test IO control commands(0 ~ 10)
 *==========================================================================*/
static int test_register_io(unsigned long arg, unsigned int cmd)
{
	int ret = 0;
	unsigned int page, addr, write_data, read_cnt, i;
	U8 value;
#if defined(RTV_IF_SPI) || defined(RTV_IF_EBI2)
	unsigned long diff_jiffies1;
	unsigned int elapsed_ms;
	unsigned long param1;
#endif
	static U8 reg_read_buf[MAX_NUM_MTV_REG_READ_BUF];
	U8 *src_ptr, *dst_ptr;
	IOCTL_REG_ACCESS_INFO __user *argp
				= (IOCTL_REG_ACCESS_INFO __user *)arg;

	if (tdmb_cb_ptr->is_power_on == FALSE) {			
		DMBMSG("[mtv] Power Down state!Must Power ON\n");
		return -EFAULT;
	}

	if (get_user(page, &argp->page))
		return -EFAULT;

	if (get_user(addr, &argp->addr))
		return -EFAULT;

	RTV_GUARD_LOCK;

	switch (cmd) {
	case IOCTL_TEST_REG_SINGLE_READ:
		RTV_REG_MAP_SEL(page);
		value = RTV_REG_GET(addr);
		if (put_user(value, &argp->read_data[0])) {
			ret = -EFAULT;
			goto regio_exit;
		}
		break;

	case IOCTL_TEST_REG_BURST_READ:
		if (get_user(read_cnt, &argp->read_cnt)) {
			ret = -EFAULT;
			goto regio_exit;
		}

		RTV_REG_MAP_SEL(page);
		RTV_REG_BURST_GET(addr, reg_read_buf, read_cnt);
		src_ptr = &reg_read_buf[0];
		dst_ptr = argp->read_data;

		for (i = 0; i< read_cnt; i++, src_ptr++, dst_ptr++) {
			if(put_user(*src_ptr, dst_ptr)) {
				ret = -EFAULT;
				goto regio_exit;
			}
		}
		break;

	case IOCTL_TEST_REG_WRITE:
		if (get_user(write_data, &argp->write_data)) {
			ret = -EFAULT;
			goto regio_exit;
		}

		RTV_REG_MAP_SEL(page);
		RTV_REG_SET(addr, write_data);
		break;

	case IOCTL_TEST_REG_SPI_MEM_READ:
	#if defined(RTV_IF_SPI) || defined(RTV_IF_EBI2)
		if (get_user(write_data, &argp->write_data)) {
			ret = -EFAULT;
			goto regio_exit;
		}

		if (get_user(read_cnt, &argp->read_cnt)) {
			ret = -EFAULT;
			goto regio_exit;
		}

		if (get_user(param1, &argp->param1)) {
			ret = -EFAULT;
			goto regio_exit;
		}

		if (param1 == 0) {
			diff_jiffies_1st = diff_jiffies0 = get_jiffies_64();
			hours_cnt = 0;
			DMBMSG("START [AGING SPI Memory Test with Single IO]\n");
		}

		RTV_REG_MAP_SEL(page);
		RTV_REG_SET(addr, write_data);

		RTV_REG_MAP_SEL(SPI_MEM_PAGE);
		RTV_REG_BURST_GET(0x10, reg_read_buf, read_cnt);

		RTV_REG_MAP_SEL(page);
		value = RTV_REG_GET(addr);

		diff_jiffies1 = get_jiffies_64();
		elapsed_ms = jiffies_to_msecs(diff_jiffies1-diff_jiffies0);
		if (elapsed_ms >= (1000 * 60 * 60)) {
			diff_jiffies0 = get_jiffies_64(); /* Re-start */
			hours_cnt++;
			DMBMSG("\t %lu hours elaspesed...\n", hours_cnt);
		}

		if (write_data != value) {
			unsigned int min, sec;
			elapsed_ms = jiffies_to_msecs(diff_jiffies1-diff_jiffies_1st);
			sec = elapsed_ms / 1000;
			min = sec / 60;			
			DMBMSG("END [AGING SPI Memory Test with Single IO]\n");
			DMBMSG("Total minutes: %u\n", min);
		}

		if (put_user(value, &argp->read_data[0])) {
			ret = -EFAULT;
			goto regio_exit;
		}
	#else
		DMBERR("Not SPI interface\n");
	#endif
		break;

	case IOCTL_TEST_REG_ONLY_SPI_MEM_READ:
	#if defined(RTV_IF_SPI) || defined(RTV_IF_EBI2)
		if (get_user(read_cnt, &argp->read_cnt)) {
			ret = -EFAULT;
			goto regio_exit;
		}

		if (get_user(write_data, &argp->write_data)) {
			ret = -EFAULT;
			goto regio_exit;
		}

		if (write_data == 0) /* only one-time page selection */
			RTV_REG_MAP_SEL(page);

		RTV_REG_BURST_GET(addr, reg_read_buf, read_cnt);
	
		src_ptr = reg_read_buf;
		dst_ptr = argp->read_data;

		for (i = 0; i< read_cnt; i++, src_ptr++, dst_ptr++) {
			if(put_user(*src_ptr, dst_ptr)) {
				ret = -EFAULT;
				goto regio_exit;
			}
		}
	#else
		DMBERR("Not SPI interface\n");
	#endif
		break;

	default:
		break;
	}

regio_exit:
	RTV_GUARD_FREE;

	return 0;
}

static int test_gpio(unsigned long arg, unsigned int cmd)
{
	unsigned int pin, value;
	IOCTL_GPIO_ACCESS_INFO __user *argp = (IOCTL_GPIO_ACCESS_INFO __user *)arg;

	if (get_user(pin, &argp->pin))
		return -EFAULT;

	switch (cmd) {
	case IOCTL_TEST_GPIO_SET:
		if(get_user(value, &argp->value))
			return -EFAULT;

		gpio_set_value(pin, value);
		break;

	case IOCTL_TEST_GPIO_GET:
		value = gpio_get_value(pin);
		if(put_user(value, &argp->value))
			return -EFAULT;
	}

	return 0;
}

static void test_power_on_off(unsigned int cmd)
{
#if defined(RTV_IF_SPI)
	#define _WR27_VAL	0x10
#elif defined(RTV_IF_EBI2)
	#define _WR27_VAL	0x12
#endif
	#define _WR29_VAL	0x81


	int k;
	unsigned char WR27, WR29;

	switch (cmd) {
	case IOCTL_TEST_MTV_POWER_ON:
		DMBMSG("IOCTL_TEST_MTV_POWER_ON\n");

		if (tdmb_cb_ptr->is_power_on == FALSE) {
			rtvOEM_PowerOn(1);

			/* Only for test command to confirm. */
			RTV_DELAY_MS(100);

		#if defined(RTV_IF_SPI) || defined(RTV_IF_EBI2)
			for (k = 0; k < 100; k++) {
				RTV_REG_MAP_SEL(SPI_CTRL_PAGE);
				RTV_REG_SET(0x29, _WR29_VAL);
				RTV_REG_SET(0x27, _WR27_VAL);
				WR27 = RTV_REG_GET(0x27);
				WR29 = RTV_REG_GET(0x29);
				if((WR27 ==_WR27_VAL) && (WR29 == _WR29_VAL))
					break;
		
				RTV_DELAY_MS(5);
			}
		
			RTV_REG_MAP_SEL(HOST_PAGE);
			RTV_REG_SET(0x05, 0x80|0x3F); //80 = SPI	
		#else
			RTV_REG_MAP_SEL(HOST_PAGE);
			RTV_REG_SET(0x05, 0x00|0x3F);
		#endif
			tdmb_cb_ptr->is_power_on = TRUE;
		}
		break;

	case IOCTL_TEST_MTV_POWER_OFF:	
		if(tdmb_cb_ptr->is_power_on == TRUE) {
			rtvOEM_PowerOn(0);
			tdmb_cb_ptr->is_power_on = FALSE;
		}
		break;	
	}
}


/*============================================================================
 * TDMB untiltiy functions.
 *==========================================================================*/
/* This function must used in 
tdmb_close_subchannel(), tdmb_close_fic(), tdmb_close_all_subchannels() */
static INLINE void __reset_tspb_queue(void)
{
	if (tdmb_tspb_queue_count() == 0)
		return; /* For fast scanning in user manual scan-time */

#ifndef RTV_MULTIPLE_CHANNEL_MODE
	if (atomic_read(&tdmb_cb_ptr->num_opened_subch) == 0)
		tdmb_tspb_queue_reset(); /* All reset. */

#else /* CIF mode */
	if (atomic_read(&tdmb_cb_ptr->num_opened_subch) == 0) {
		if(tdmb_cb_ptr->fic_opened == FALSE) /* FIC was closed. */
			tdmb_tspb_queue_reset(); /* All reset. */
		else /* Clear MSC contents only */
			tdmb_tspb_queue_clear_contents(1);
	}
	else { /* MSC was opened yet */
		if (tdmb_cb_ptr->fic_opened == FALSE) /* Clear FIC contents only */
			tdmb_tspb_queue_clear_contents(0);
	}
#endif
}


/*==============================================================================
 * TDMB IO control commands(30 ~ 49)
 *============================================================================*/ 
static INLINE int tdmb_get_lock_status(unsigned long arg)
{
	IOCTL_TDMB_GET_LOCK_STATUS_INFO __user *argp = (IOCTL_TDMB_GET_LOCK_STATUS_INFO __user *)arg;
	unsigned int lock_mask = rtvTDMB_GetLockStatus();

	if(put_user(lock_mask, &argp->lock_mask))
		return -EFAULT;

	return 0;
}

static INLINE int tdmb_get_signal_info(unsigned long arg)
{
	IOCTL_TDMB_SIGNAL_INFO sig;
	void __user *argp = (void __user *)arg;

	sig.lock_mask = rtvTDMB_GetLockStatus();	
	sig.ber = rtvTDMB_GetBER();	 
	sig.cnr = rtvTDMB_GetCNR(); 
	sig.per = rtvTDMB_GetPER(); 
	sig.rssi = rtvTDMB_GetRSSI();
	sig.cer = rtvTDMB_GetCER();	
	sig.ant_level = rtvTDMB_GetAntennaLevel(sig.cer);

	if (copy_to_user(argp, &sig, sizeof(IOCTL_TDMB_SIGNAL_INFO)))
		return -EFAULT;

	SHOW_TDMB_DEBUG_STAT;

	return 0;
}

static INLINE void tdmb_close_fic(void)
{
	DMBMSG("Enter...\n");

	rtvTDMB_CloseFIC();

	tdmb_cb_ptr->fic_opened = FALSE;

#ifdef RTV_FIC_SPI_INTR_ENABLED
	tdmb_cb_ptr->wakeup_tspq_thres_cnt = TDMB_SPI_WAKEUP_TSP_Q_CNT;
	__reset_tspb_queue();
#endif

#ifdef _TDMB_KERNEL_FILE_DUMP_ENABLE
	tdmb_fic_dump_kfile_close();
#endif
}

static INLINE int tdmb_open_fic(unsigned long arg)
{
	int ret;
	IOCTL_OPEN_FIC_INFO __user *argp = (IOCTL_OPEN_FIC_INFO __user *)arg;

	DMBMSG("Enter...\n");

	ret = rtvTDMB_OpenFIC();
	if (ret == RTV_SUCCESS) {
		RTV_GUARD_LOCK;

		tdmb_cb_ptr->fic_opened = TRUE;

#ifdef RTV_FIC_SPI_INTR_ENABLED /* FIC interrupt mode */
		/* Allow thread to read TSP data on read() */
		tdmb_cb_ptr->read_stop = FALSE;
		tdmb_cb_ptr->wakeup_tspq_thres_cnt = 1; /* For FIC parsing in time. */
		tdmb_cb_ptr->first_interrupt = TRUE;
	#ifndef RTV_MULTIPLE_CHANNEL_MODE
		tdmb_cb_ptr->intr_ts_size = 384;
	#else
		tdmb_cb_ptr->intr_ts_size = TDMB_MULTI_SERVICE_THRESHOLD_SIZE;
	#endif
#endif

		RTV_GUARD_FREE;

		return 0;
	}
	else {
		if (put_user(ret, &argp->tuner_err_code))
			return -EFAULT;
		else
			return -EINVAL;
	}
}

static INLINE void tdmb_close_all_subchannels(void)
{
	//DMBMSG("Enter...\n");

	if (atomic_read(&tdmb_cb_ptr->num_opened_subch)) {
		rtvTDMB_CloseAllSubChannels();
		atomic_set(&tdmb_cb_ptr->num_opened_subch, 0);

#if defined(RTV_IF_SPI) || defined(RTV_IF_EBI2)
		__reset_tspb_queue();
#endif

#ifdef _TDMB_KERNEL_FILE_DUMP_ENABLE
		tdmb_msc_dump_kfile_close();
#endif
	}
}

static INLINE int tdmb_close_subchannel(unsigned int subch_id)
{
	int ret;

	DMBMSG("Enter...\n");

	ret = rtvTDMB_CloseSubChannel(subch_id);
	if (ret == RTV_SUCCESS) {
		if (atomic_read(&tdmb_cb_ptr->num_opened_subch) != 0)
			atomic_dec(&tdmb_cb_ptr->num_opened_subch);
		else {
	#if defined(RTV_IF_SPI) || defined(RTV_IF_EBI2)
			__reset_tspb_queue();
	#endif

	#ifdef _TDMB_KERNEL_FILE_DUMP_ENABLE
			tdmb_msc_dump_kfile_close();
	#endif
		}
	}
	else
		DMBERR("Failed: %d\n", ret);

	return 0;
}

static INLINE int tdmb_open_subchannel(unsigned long arg)
{
	int ret = 0;
	unsigned int ch_freq_khz, subch_id, intr_ts_size;
	enum E_RTV_SERVICE_TYPE svc_type;
	IOCTL_TDMB_SUB_CH_INFO __user *argp
				= (IOCTL_TDMB_SUB_CH_INFO __user *)arg;

	DMBMSG("Start\n");

	/* Most of TDMB application use set channel instead of open/close method. 
	So, we reset buffer before open sub channel. */
#if (RTV_MAX_NUM_USE_SUBCHANNEL == 1)
	tdmb_close_all_subchannels();
#endif

	if (get_user(ch_freq_khz, &argp->ch_freq_khz))
		return -EFAULT;

	if (get_user(subch_id, &argp->subch_id))
		return -EFAULT;

	if (get_user(svc_type, &argp->svc_type))
		return -EFAULT;

#if 0
	DMBMSG("freq(%d), subch_id(%u), svc_type(%d)\n", 
		ch_freq_khz, subch_id, svc_type);
#endif

	switch (svc_type) {
	case RTV_SERVICE_DMB:
		intr_ts_size = TDMB_DMB_THRESHOLD_SIZE;
		break;

	case RTV_SERVICE_DAB:
		intr_ts_size = TDMB_DAB_THRESHOLD_SIZE;
		break;

	case RTV_SERVICE_DABPLUS:
		intr_ts_size = TDMB_DABPLUS_THRESHOLD_SIZE;
		break;

	default:
		DMBERR("Invaild service type: %d\n", svc_type);
		return -EINVAL;
	}

#if defined(CONFIG_TDMB_SPI) || defined(CONFIG_TDMB_EBI2)
	#ifndef RTV_MULTIPLE_CHANNEL_MODE
	tdmb_cb_ptr->intr_ts_size = intr_ts_size;
	#else
	tdmb_cb_ptr->intr_ts_size = TDMB_MULTI_SERVICE_THRESHOLD_SIZE;
	#endif
#else
	intr_ts_size = 188;
#endif

	ret = rtvTDMB_OpenSubChannel(ch_freq_khz, subch_id, svc_type, intr_ts_size);
	if (ret == RTV_SUCCESS) {
		if (atomic_read(&tdmb_cb_ptr->num_opened_subch) == 0) {/* The first open */
	#if defined(RTV_IF_SPI) || defined(RTV_IF_EBI2)
			/* Allow thread to read TSP data on read() */
			tdmb_cb_ptr->read_stop = FALSE;
			tdmb_cb_ptr->first_interrupt = TRUE;
			RESET_DEBUG_INTR_STAT;
			RESET_DEBUG_TSPB_STAT;
	#endif

	#ifdef _TDMB_KERNEL_FILE_DUMP_ENABLE
			if (tdmb_msc_dump_kfile_open(ch_freq_khz) != 0)
				return -EFAULT;	
	#endif	
		}

		/* Increment the number of opened sub channel. */
		atomic_inc(&tdmb_cb_ptr->num_opened_subch);
	}
	else {
		if (ret != RTV_ALREADY_OPENED_SUBCHANNEL_ID) {
			DMBERR("failed: %d\n", ret);
			if (put_user(ret, &argp->tuner_err_code))
				return -EFAULT;
			else
				return -EIO;
		}
	}

	return 0;
}

static INLINE int tdmb_scan_freq(unsigned long arg)
{
	int ret;
	unsigned int ch_freq_khz;
	IOCTL_TDMB_SCAN_INFO __user *argp = (IOCTL_TDMB_SCAN_INFO __user *)arg;
	
	if (get_user(ch_freq_khz, &argp->ch_freq_khz))
		return -EFAULT;

	tdmb_close_all_subchannels();

	ret = rtvTDMB_ScanFrequency(ch_freq_khz);
	if (ret == RTV_SUCCESS) {
#ifdef _TDMB_KERNEL_FILE_DUMP_ENABLE
		if (tdmb_fic_dump_kfile_open(ch_freq_khz) != 0)
			return -EFAULT; 
#endif

		return 0;
	}
	else {
		if(ret != RTV_CHANNEL_NOT_DETECTED)
			DMBERR("Device error: %d\n", ret);
		/* Copy the tuner error-code to application */
		if(put_user(ret, &argp->tuner_err_code))
			return -EFAULT;
		else
			return -EINVAL;
	}
}

static INLINE int tdmb_read_fic(unsigned long arg)
{
	int ret;
	U8 fic_buf[MTV319_FIC_BUF_SIZE];
#if !defined(RTV_MULTIPLE_CHANNEL_MODE) && defined(RTV_SCAN_FIC_HDR_ENABLED)	
	U8 assemble_buf[384];
#endif

	IOCTL_TDMB_READ_FIC_INFO __user *argp
			= (IOCTL_TDMB_READ_FIC_INFO __user *)arg;

	//DMBMSG("Enter...\n");

	ret = rtvTDMB_ReadFIC(fic_buf);
	if (ret > 0)	{
#ifdef _TDMB_KERNEL_FILE_DUMP_ENABLE
		tdmb_fic_dump_kfile_write(fic_buf, ret);
		//tdmb_fic_dump_kfile_write((void __user*)argp->buf, ret);
#endif

#if !defined(RTV_MULTIPLE_CHANNEL_MODE) && defined(RTV_SCAN_FIC_HDR_ENABLED)	
		ret = mtv319_assemble_fic(assemble_buf, fic_buf, ret);
		memcpy(fic_buf, assemble_buf, ret);
#endif

		if (copy_to_user((void __user*)argp->buf, fic_buf, ret))
			return -EFAULT;
		else
			return 0;
	}
	else {
		if (put_user(ret, &argp->tuner_err_code))
			return -EFAULT;
		else
			return -EINVAL;
	}
}

static INLINE void tdmb_deinit(void)
{
	tdmb_close_all_subchannels();
	tdmb_close_fic();
}

static INLINE int tdmb_init(unsigned long arg)
{
	int ret;

	atomic_set(&tdmb_cb_ptr->num_opened_subch, 0); /* NOT opened state */
	tdmb_cb_ptr->fic_opened = FALSE;

#if defined(RTV_IF_SPI) || defined(RTV_IF_EBI2)
	/* Blocking or non-blocking both. */
	tdmb_cb_ptr->wakeup_tspq_thres_cnt = TDMB_SPI_WAKEUP_TSP_Q_CNT;
#endif

	ret = rtvTDMB_Initialize();
	if (ret == RTV_SUCCESS)
		return 0;
	else {
		DMBERR("Tuner initialization failed: %d\n", ret);
		if (put_user(ret, (int *)arg))
			return -EFAULT;
		else
			return -EIO; /* error occurred during the open() */
	}
}


