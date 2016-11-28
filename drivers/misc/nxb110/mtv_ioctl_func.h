/*
 * File name: mtv_ioctl_func.h
 *
 * Description: MTV IO control functions header file.
 *                   Only used in the mtv.c
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
 */

#include "mtv.h"

/* Forward functions. */
#if defined(RTV_IF_SPI)
static void mtv_reset_tsp_queue(struct mtv_cb *mtv_cb_ptr);
#if defined(RTV_DAB_ENABLE) || defined(RTV_TDMB_ENABLE)
void mtv_clear_contents_only_in_tsp_queue(struct mtv_cb *mtv_cb_ptr, int fic_msc_type);
#endif
#endif


#ifdef DEBUG_MTV_IF_MEMORY
static INLINE void reset_msc_if_debug(int demod_no)
{
#ifdef RTV_SPI_MSC0_ENABLED
	mtv_cb_ptr->msc0_ts_intr_cnt = 0;
	mtv_cb_ptr->msc0_ovf_intr_cnt = 0;

	#ifdef RTV_CIF_MODE_ENABLED
	mtv_cb_ptr->msc0_cife_cnt = 0;
	#endif
#endif	

#ifdef RTV_SPI_MSC1_ENABLED
	mtv_cb_ptr->msc1_ts_intr_cnt = 0;
	mtv_cb_ptr->msc1_ovf_intr_cnt = 0;
#endif

	mtv_cb_ptr->max_remaining_tsp_cnt = 0;
}

static INLINE void show_msc_if_statistics(int demod_no)
{
#if defined(RTV_SPI_MSC1_ENABLED) && defined(RTV_SPI_MSC0_ENABLED)
	#ifndef RTV_CIF_MODE_ENABLED
	DMBMSG("MSC1[ovf: %ld/%ld], MSC0[ovf: %ld/%ld], # remaining tsp: %u\n",
		mtv_cb_ptr->msc1_ovf_intr_cnt, mtv_cb_ptr->msc1_ts_intr_cnt,
		mtv_cb_ptr->msc0_ovf_intr_cnt, mtv_cb_ptr->msc0_ts_intr_cnt,
		mtv_cb_ptr->max_remaining_tsp_cnt);
	#else
	DMBMSG("MSC1[ovf: %ld/%ld], MSC0[ovf: %ld/%ld, cife: %ld], # remaining tsp: %u\n",
		mtv_cb_ptr->msc1_ovf_intr_cnt, mtv_cb_ptr->msc1_ts_intr_cnt,
		mtv_cb_ptr->msc0_ovf_intr_cnt, mtv_cb_ptr->msc0_ts_intr_cnt,
		mtv_cb_ptr->msc0_cife_cnt, mtv_cb_ptr->max_remaining_tsp_cnt);
	#endif
	
#elif defined(RTV_SPI_MSC1_ENABLED) && !defined(RTV_SPI_MSC0_ENABLED)
	DMBMSG("MSC1[ovf: %ld/%ld], # remaining tsp: %u\n",
		mtv_cb_ptr->msc1_ovf_intr_cnt, mtv_cb_ptr->msc1_ts_intr_cnt,
		mtv_cb_ptr->max_remaining_tsp_cnt);

#elif !defined(RTV_SPI_MSC1_ENABLED) && defined(RTV_SPI_MSC0_ENABLED)
	#ifndef RTV_CIF_MODE_ENABLED
	DMBMSG("MSC0[ovf: %ld/%ld], # remaining tsp: %u\n",
		mtv_cb_ptr->msc0_ovf_intr_cnt, mtv_cb_ptr->msc0_ts_intr_cnt,
		mtv_cb_ptr->max_remaining_tsp_cnt);
	#else
	DMBMSG("MSC0[ovf: %ld/%ld, cife: %ld], # remaining tsp: %u\n",
		mtv_cb_ptr->msc0_ovf_intr_cnt, mtv_cb_ptr->msc0_ts_intr_cnt,
		mtv_cb_ptr->msc0_cife_cnt, mtv_cb_ptr->max_remaining_tsp_cnt);
	#endif
#endif
}

#define RESET_MSC_IF_DEBUG(demod_no)		reset_msc_if_debug(demod_no)
#define SHOW_MSC_IF_STATISTICS(demod_no)	show_msc_if_statistics(demod_no)

#else
#define RESET_MSC_IF_DEBUG		((void)0)
#define SHOW_MSC_IF_STATISTICS	((void)0)
#endif /* #ifdef DEBUG_MTV_IF_MEMORY*/

/*============================================================================
 * Test IO control commands(0~10)
 *==========================================================================*/
static unsigned char get_reg_page_value(struct mtv_cb *mtv_cb_ptr, unsigned int page_idx)
{
	unsigned char page_val = 0x0;
	static const unsigned char mtv_reg_page_addr[] = {
		0x07/*HOST*/, 0x0F/*RF*/, 0x04/*COMM*/, 0x09/*DD*/,
		0x0B/*MSC0*/, 0x0C/*MSC1*/
	};
	
	switch ( mtv_cb_ptr->tv_mode ) {
	case DMB_TV_MODE_TDMB:
	case DMB_TV_MODE_DAB:
	case DMB_TV_MODE_FM:
		switch (page_idx) {
			case 6: page_val = 0x06; break; /* OFDM */
			case 7: page_val = 0x09; break; /* FEC */
			case 8: page_val = 0x0A; break; /* FEC */
			default: page_val = mtv_reg_page_addr[page_idx];					
		}
		break;

	case DMB_TV_MODE_1SEG:
		switch(page_idx) {
			case 6: page_val = 0x02; break; /* OFDM */
			case 7: page_val = 0x03; break; /* FEC */
			default: page_val = mtv_reg_page_addr[page_idx];					
		}
		break;
	default:
		break;
	}

	return page_val;
}

static int test_register_io(struct mtv_cb *mtv_cb_ptr, unsigned long arg, unsigned int cmd)
{
	unsigned int page, addr, write_data, read_cnt, i;
	unsigned char value;
	U8 reg_page_val = 0;
#if defined(RTV_IF_SPI)	
	unsigned char reg_read_buf[MAX_NUM_MTV_REG_READ_BUF+1];
#else
	unsigned char reg_read_buf[MAX_NUM_MTV_REG_READ_BUF];
#endif
	U8 *src_ptr, *dst_ptr;
	IOCTL_REG_ACCESS_INFO __user *argp = (IOCTL_REG_ACCESS_INFO __user *)arg;

	if (mtv_cb_ptr->is_power_on == FALSE) {			
		DMBMSG("Power Down state!Must Power ON\n");
		return -EFAULT;
	}

	if (get_user(page, &argp->page))
		return -EFAULT;

	if (get_user(addr, &argp->addr))
		return -EFAULT;

	reg_page_val = get_reg_page_value(mtv_cb_ptr, page);
	RTV_REG_MAP_SEL(mtv_cb_ptr->demod_no, reg_page_val);

	switch (cmd) {
	case IOCTL_TEST_REG_SINGLE_READ:
		value = RTV_REG_GET(mtv_cb_ptr->demod_no, addr);
		if (put_user(value, &argp->read_data[0]))
			return -EFAULT;
		break;

	case IOCTL_TEST_REG_BURST_READ:
		if (get_user(read_cnt, &argp->read_cnt))
			return -EFAULT;

	#if defined(RTV_IF_SPI)
		RTV_REG_BURST_GET(mtv_cb_ptr->demod_no, addr, reg_read_buf, read_cnt+1);
		src_ptr = &reg_read_buf[1];
	#else
		RTV_REG_BURST_GET(mtv_cb_ptr->demod_no, addr, reg_read_buf, read_cnt);
		src_ptr = &reg_read_buf[0];
	#endif
		dst_ptr = argp->read_data;

		for (i = 0; i< read_cnt; i++, src_ptr++, dst_ptr++) {
			if(put_user(*src_ptr, dst_ptr))
				return -EFAULT;
		}
		break;

	case IOCTL_TEST_REG_WRITE:
		if (get_user(write_data, &argp->write_data))
			return -EFAULT;
		
		RTV_REG_SET(mtv_cb_ptr->demod_no, addr, write_data);
		break;
	}

	return 0;
}

static int test_gpio(unsigned long arg, unsigned int cmd)
{
	unsigned int pin, value;
	IOCTL_GPIO_ACCESS_INFO __user *argp	= (IOCTL_GPIO_ACCESS_INFO __user *)arg;

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

static void test_power_on_off(struct mtv_cb *mtv_cb_ptr, unsigned int cmd)
{
	switch (cmd) {
	case IOCTL_TEST_MTV_POWER_ON:
		DMBMSG("IOCTL_TEST_MTV_POWER_ON\n");

		if (mtv_cb_ptr->is_power_on == FALSE) {
			rtvOEM_PowerOn(mtv_cb_ptr->demod_no, 1);

			/* Only for test command to confirm. */
			RTV_DELAY_MS(100);

			/* To read the page of RF. */
			RTV_REG_MAP_SEL(mtv_cb_ptr->demod_no, HOST_PAGE);
			RTV_REG_SET(mtv_cb_ptr->demod_no, 0x7D, 0x06);

			mtv_cb_ptr->is_power_on = TRUE;
		}
		break;

	case IOCTL_TEST_MTV_POWER_OFF:	
		if(mtv_cb_ptr->is_power_on == TRUE) {
			rtvOEM_PowerOn(mtv_cb_ptr->demod_no, 0);
			mtv_cb_ptr->is_power_on = FALSE;
		}
		break;	
	}
}


/*============================================================================
 * TDMB/DAB untiltiy functions.
 *==========================================================================*/
#if (defined(RTV_DAB_ENABLE)||defined(RTV_TDMB_ENABLE)) && defined(RTV_IF_SPI)
/* This function must used in 
dab_close_subchannel()/tdmb_close_subchannel(),
dab_close_fic()/tdmb_close_fic(), 
dab_close_all_subchannels()/tdmb_close_all_subchannels() */
static INLINE void tdmb_dab_reset_tsp_queue(struct mtv_cb *mtv_cb_ptr)
{
	if (get_tsp_queue_count(mtv_cb_ptr->demod_no) == 0)
		return; /* For fast scanning in user manual scan-time */

#ifdef RTV_FIC_POLLING_MODE /* FIC polling mode */
	if (atomic_read(&mtv_cb_ptr->num_opened_subch) == 0)
		mtv_reset_tsp_queue(mtv_cb_ptr); /* All reset. */

#else /* FIC interrupt mode */
	if (atomic_read(&mtv_cb_ptr->num_opened_subch) == 0) {
		if(mtv_cb_ptr->fic_opened == FALSE) /* FIC was closed. */
			mtv_reset_tsp_queue(mtv_cb_ptr); /* All reset. */
		else /* Clear MSC contents only */
			mtv_clear_contents_only_in_tsp_queue(mtv_cb_ptr, 1);
	}
	else { /* MSC was opened yet */
		if (mtv_cb_ptr->fic_opened == FALSE) /* Clear FIC contents only */
			mtv_clear_contents_only_in_tsp_queue(mtv_cb_ptr, 0);
	}
#endif
}
/* #if (defined(RTV_DAB_ENABLE)||defined(RTV_TDMB_ENABLE)) && defined(RTV_IF_SPI) */
#endif

#ifdef RTV_DAB_ENABLE
/*============================================================================
 * DAB IO control commands(70 ~ 89)
 *==========================================================================*/
static INLINE int dab_get_lock_status(unsigned long arg)
{
	unsigned int lock_mask;

	lock_mask = rtvDAB_GetLockStatus();

	if (put_user(lock_mask, (unsigned int *)arg))
		return -EFAULT;

	return 0;
}

static INLINE int dab_get_signal_info(unsigned long arg)
{
	IOCTL_DAB_SIGNAL_INFO sig;
	void __user *argp = (void __user *)arg;

	sig.lock_mask = rtvDAB_GetLockStatus();
	sig.ber = rtvDAB_GetBER();	
	sig.cnr = rtvDAB_GetCNR(); 
	sig.per = rtvDAB_GetPER(); 
	sig.rssi = rtvDAB_GetRSSI();
	sig.cer = rtvDAB_GetCER(); 
	sig.ant_level = rtvDAB_GetAntennaLevel(sig.cer);

	if (copy_to_user(argp, &sig, sizeof(IOCTL_DAB_SIGNAL_INFO)))
		return -EFAULT;

	SHOW_MSC_IF_STATISTICS;

	return 0;
}

static INLINE void dab_close_all_subchannels(void)
{
	rtvDAB_CloseAllSubChannels();

	atomic_set(&mtv_cb_ptr->num_opened_subch, 0);
#if defined(RTV_IF_SPI)
	tdmb_dab_reset_tsp_queue();
#endif
}

static INLINE int dab_close_subchannel(unsigned long arg)
{
	int ret;
	unsigned int subch_id = 0;

#if (RTV_NUM_DAB_AVD_SERVICE >= 2)
	if (get_user(subch_id, (unsigned int *)arg))
		return -EFAULT;

	DMBMSG("subch_id(%d)\n", subch_id); 
#endif

	ret = rtvDAB_CloseSubChannel(subch_id);
	if (ret == RTV_SUCCESS) {
		if (atomic_read(&mtv_cb_ptr->num_opened_subch) != 0)
			atomic_dec(&mtv_cb_ptr->num_opened_subch);
	}
	else
		DMBERR("failed: %d\n", ret);

#if defined(RTV_IF_SPI)
	if (atomic_read(&mtv_cb_ptr->num_opened_subch) == 0)
		tdmb_dab_reset_tsp_queue();
#endif

	return 0;
}

static INLINE int dab_open_subchannel(unsigned long arg)
{
	int ret = 0;
	unsigned int ch_freq_khz;
	unsigned int subch_id;
	E_RTV_SERVICE_TYPE svc_type;
	unsigned int thres_size;
	IOCTL_DAB_SUB_CH_INFO __user *argp = (IOCTL_DAB_SUB_CH_INFO __user *)arg;

	DMBMSG("[dab_open_subchannel] Start\n");

	/* Most of TDMB application use set channel instead of open/close method. 
	So, we reset buffer before open sub channel. */
#if (RTV_NUM_DAB_AVD_SERVICE == 1)
	dab_close_subchannel(0/* don't care for 1 AV subch application */);
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
	case RTV_SERVICE_VIDEO:
		mtv_cb_ptr->av_subch_id = subch_id;
		thres_size = MTV_TS_THRESHOLD_SIZE;
		break;
	case RTV_SERVICE_AUDIO:
		mtv_cb_ptr->av_subch_id = subch_id;
		thres_size = MTV_TS_AUDIO_THRESHOLD_SIZE;
		break;
	case RTV_SERVICE_DATA:
	#ifndef RTV_CIF_MODE_ENABLED
		mtv_cb_ptr->data_subch_id = subch_id;
	#endif	
		thres_size = MTV_TS_DATA_THRESHOLD_SIZE;
		break;
	default:
		DMBERR("Invaild service type: %d\n", svc_type);
		return -EINVAL;
	}

#if (RTV_NUM_DAB_AVD_SERVICE == 1)
	/* If 1 DATA service only, the interrupt occured at MSC1. 
	This is not required in CIF mode. */
	mtv_cb_ptr->msc1_thres_size = thres_size;
#else
	if (svc_type & (RTV_SERVICE_VIDEO | RTV_SERVICE_AUDIO))
		mtv_cb_ptr->msc1_thres_size = thres_size;
	else
		mtv_cb_ptr->msc0_thres_size = thres_size;
#endif

	ret = rtvDAB_OpenSubChannel(ch_freq_khz, subch_id, svc_type, thres_size);
	if (ret == RTV_SUCCESS) {
		if (atomic_read(&mtv_cb_ptr->num_opened_subch) == 0) { /* The first open */
		#if defined(RTV_IF_SPI)
			/* Allow thread to read TSP data on read() */
			mtv_cb_ptr->read_stop = FALSE;
			mtv_cb_ptr->first_interrupt = TRUE;
		#endif

			RESET_MSC_IF_DEBUG;
		}

		/* Increment the number of opened sub channel. */
		atomic_inc(&mtv_cb_ptr->num_opened_subch);
	}
	else {
		if (ret != RTV_ALREADY_OPENED_SUB_CHANNEL_ID) {
			DMBERR("failed: %d\n", ret);
			if (put_user(ret, &argp->tuner_err_code))
				return -EFAULT;
			else
				return -EIO;
		}
	}

	return 0;
}

static INLINE int dab_scan_freq(unsigned long arg)
{
	int ret;
	unsigned int ch_freq_khz;
	IOCTL_DAB_SCAN_INFO __user *argp = (IOCTL_DAB_SCAN_INFO __user *)arg;
	
	if (get_user(ch_freq_khz, &argp->ch_freq_khz))
		return -EFAULT;

	ret = rtvDAB_ScanFrequency(ch_freq_khz);
	if (ret == RTV_SUCCESS)
		return 0;
	else {
		if(ret == RTV_CHANNEL_NOT_DETECTED)
			DMBMSG("Channel not detected: %d\n", ret);
		else
			DMBERR("Device error: %d\n", ret);
		/* Copy the tuner error-code to application */
		if(put_user(ret, &argp->tuner_err_code))
			return -EFAULT;
		else
			return -EINVAL;
	}
}

/* Can be calling anywhre */
static INLINE int dab_get_fic_size(unsigned long arg)
{
	unsigned int fic_size = rtvDAB_GetFicSize();

	if (put_user(fic_size, (unsigned int *)arg))
		return -EFAULT;

	return 0;
}

static INLINE int dab_read_fic(unsigned long arg)
{
	int ret;
	unsigned int fic_size;
#if defined(RTV_IF_SPI)
	U8 fic_buf[384+1];
#else
	U8 fic_buf[384];
#endif
	IOCTL_DAB_READ_FIC_INFO __user *argp
					= (IOCTL_DAB_READ_FIC_INFO __user *)arg;

	if (get_user(fic_size, &argp->size))
		return -EFAULT;

	ret = rtvDAB_ReadFIC(fic_buf, fic_size);
	if (ret > 0)	{
#if defined(RTV_IF_SPI)		
		if (copy_to_user((void __user*)argp->buf, &fic_buf[1], ret))
#else
		if (copy_to_user((void __user*)argp->buf, fic_buf, ret))
#endif
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

static INLINE void dab_close_fic(void)
{
	rtvDAB_CloseFIC();

	mtv_cb_ptr->fic_opened = FALSE;
	mtv_cb_ptr->fic_size = 0; /* Reset */

#ifdef RTV_FIC_SPI_INTR_ENABLED
	mtv_cb_ptr->wakeup_tspq_thres_cnt = MTV_SPI_WAKEUP_TSP_Q_CNT;
	tdmb_dab_reset_tsp_queue();
#endif
}

/* Can be calling anywhere  */
static INLINE int dab_open_fic(unsigned long arg)
{
	int ret;

	ret = rtvDAB_OpenFIC();
	if (ret == RTV_SUCCESS) {
		mtv_cb_ptr->fic_opened = TRUE;

		/* NOTE:
		Don't get and save the size of FIC.
		This function can be called before set freq, 
		so the size of FIC would be abnormal.
		ex) rtvDAB_ScanFrequency() or rtvDAB_OpenSubChannel()
		*/
#ifdef RTV_FIC_SPI_INTR_ENABLED /* FIC interrupt mode */
		/* Allow thread to read TSP data on read() */
		mtv_cb_ptr->read_stop = FALSE;
		mtv_cb_ptr->wakeup_tspq_thres_cnt = 1; /* For FIC parsing in time. */	
#endif

		return 0;
	}
	else {
		if (put_user(ret, (int *)arg))
			return -EFAULT;
		else
			return -EINVAL;
	}
}

static INLINE void dab_deinit(void)
{
	dab_close_all_subchannels();
	dab_close_fic();			
}

static INLINE int dab_init(unsigned long arg)
{
	int ret;

	mtv_cb_ptr->tv_mode = DMB_TV_MODE_DAB;
	atomic_set(&mtv_cb_ptr->num_opened_subch, 0); /* NOT opened state */

	mtv_cb_ptr->fic_opened = FALSE;
	mtv_cb_ptr->fic_size = 0;

#if defined(RTV_IF_SPI) /* Blocking or non-blocking both. */
	mtv_cb_ptr->wakeup_tspq_thres_cnt = MTV_SPI_WAKEUP_TSP_Q_CNT;
#endif

	ret = rtvDAB_Initialize();
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
#endif /* #ifdef RTV_DAB_ENABLE */


#ifdef RTV_TDMB_ENABLE
/*==============================================================================
 * TDMB IO control commands(30 ~ 49)
 *============================================================================*/ 
static INLINE int tdmb_get_lock_status(struct mtv_cb *mtv_cb_ptr, unsigned long arg)
{
	unsigned int lock_mask;
	IOCTL_TDMB_GET_LOCK_STATUS_INFO __user *argp = (IOCTL_TDMB_GET_LOCK_STATUS_INFO __user *)arg;

	lock_mask = rtvTDMB_GetLockStatus(mtv_cb_ptr->demod_no);

	if(put_user(lock_mask, &argp->lock_mask))
		return -EFAULT;

	return 0;
}

static INLINE int tdmb_get_signal_info(struct mtv_cb *mtv_cb_ptr, unsigned long arg)
{
	IOCTL_TDMB_SIGNAL_INFO sig;
	void __user *argp = (void __user *)arg;

	sig.lock_mask = rtvTDMB_GetLockStatus(mtv_cb_ptr->demod_no);	
	sig.ber = rtvTDMB_GetBER(mtv_cb_ptr->demod_no); 	 
	sig.cnr = rtvTDMB_GetCNR(mtv_cb_ptr->demod_no); 
	sig.per = rtvTDMB_GetPER(mtv_cb_ptr->demod_no); 
	sig.rssi = rtvTDMB_GetRSSI(mtv_cb_ptr->demod_no);
	sig.cer = rtvTDMB_GetCER(mtv_cb_ptr->demod_no);
	sig.ant_level = rtvTDMB_GetAntennaLevel(mtv_cb_ptr->demod_no, sig.cer);

	if (copy_to_user(argp, &sig, sizeof(IOCTL_TDMB_SIGNAL_INFO)))
		return -EFAULT;

printk("[TDMB] LOCK(%d) BER(%d) CER(%d) CNR(%d) RSSI(%d) PER(%d) \n", sig.lock_mask, sig.ber, sig.cer, sig.cnr, sig.rssi, sig.per);

	//SHOW_MSC_IF_STATISTICS(mtv_cb_ptr->demod_no);

	return 0;
}

static INLINE void tdmb_close_all_subchannels(struct mtv_cb *mtv_cb_ptr)
{
	rtvTDMB_CloseAllSubChannels(mtv_cb_ptr->demod_no);

	atomic_set(&mtv_cb_ptr->num_opened_subch, 0);
#if defined(RTV_IF_SPI)
	tdmb_dab_reset_tsp_queue(mtv_cb_ptr);
#endif
}

static INLINE int tdmb_close_subchannel(struct mtv_cb * mtv_cb_ptr, unsigned int subch_id)
{
	int ret;

#if (RTV_NUM_DAB_AVD_SERVICE >= 2)
	DMBMSG("subch_id(%d)\n", subch_id); 
#endif

	ret = rtvTDMB_CloseSubChannel(mtv_cb_ptr->demod_no, subch_id);
	if (ret == RTV_SUCCESS) {
		if (atomic_read(&mtv_cb_ptr->num_opened_subch) != 0)
			atomic_dec(&mtv_cb_ptr->num_opened_subch);
	}
	else {
		if (ret != RTV_NOT_OPENED_SUB_CHANNEL_ID)
			DMBERR("Failed: %d\n", ret);
	}

#if defined(RTV_IF_SPI)
	if (atomic_read(&mtv_cb_ptr->num_opened_subch) == 0)
		tdmb_dab_reset_tsp_queue(mtv_cb_ptr);
#endif

	return 0;
}

static INLINE int tdmb_open_subchannel(struct mtv_cb * mtv_cb_ptr, unsigned long arg)
{
	int ret = 0;
	unsigned int ch_freq_khz;
	unsigned int subch_id;
	E_RTV_SERVICE_TYPE svc_type;
	unsigned int thres_size;
	IOCTL_TDMB_SUB_CH_INFO __user *argp = (IOCTL_TDMB_SUB_CH_INFO __user *)arg;

	DMBMSG("Start\n");

	/* Most of TDMB application use set channel instead of open/close method. 
	So, we reset buffer before open sub channel. */
#if (RTV_NUM_DAB_AVD_SERVICE == 1)
	tdmb_close_subchannel(mtv_cb_ptr, 0/* don't care for 1 AV subch application */);
#endif

	if (get_user(ch_freq_khz, &argp->ch_freq_khz))
		return -EFAULT;

	if (get_user(subch_id, &argp->subch_id))
		return -EFAULT;

	if (get_user(svc_type, &argp->svc_type))
		return -EFAULT;

   	printk("svc_type(%d)\n", svc_type);

#if 0
	DMBMSG("freq(%d), subch_id(%u), svc_type(%d)\n", 
		ch_freq_khz, subch_id, svc_type);
#endif

	switch (svc_type) {
	case RTV_SERVICE_VIDEO:
		mtv_cb_ptr->av_subch_id = subch_id;
		thres_size = MTV_TS_THRESHOLD_SIZE;
		break;
	case RTV_SERVICE_AUDIO:
		mtv_cb_ptr->av_subch_id = subch_id;
		thres_size = MTV_TS_AUDIO_THRESHOLD_SIZE;
		break;
	case RTV_SERVICE_DATA:
	#ifndef RTV_CIF_MODE_ENABLED
		mtv_cb_ptr->data_subch_id = subch_id;
	#endif
		thres_size = MTV_TS_DATA_THRESHOLD_SIZE;
		break;
	default:
		DMBERR("Invaild service type: %d\n", svc_type);
		return -EINVAL;
	}

#if (RTV_NUM_DAB_AVD_SERVICE == 1)
	/* If 1 DATA service only, the interrupt occured at MSC1. 
	This is not required in CIF mode. */
	mtv_cb_ptr->msc1_thres_size = thres_size;
#else
	if ((svc_type == RTV_SERVICE_VIDEO)
	|| (svc_type == RTV_SERVICE_AUDIO))
		mtv_cb_ptr->msc1_thres_size = thres_size;
	else
		mtv_cb_ptr->msc0_thres_size = thres_size;
#endif

	ret = rtvTDMB_OpenSubChannel(mtv_cb_ptr->demod_no, ch_freq_khz, subch_id, svc_type, thres_size);
	if (ret == RTV_SUCCESS) {
		if (atomic_read(&mtv_cb_ptr->num_opened_subch) == 0) {/* The first open */
	#if defined(RTV_IF_SPI)
			/* Allow thread to read TSP data on read() */
			mtv_cb_ptr->read_stop = FALSE;
			mtv_cb_ptr->first_interrupt = TRUE;
	#endif

			RESET_MSC_IF_DEBUG(mtv_cb_ptr->demod_no);
		}

		/* Increment the number of opened sub channel. */
		atomic_inc(&mtv_cb_ptr->num_opened_subch);
	}
	else {
		if (ret != RTV_ALREADY_OPENED_SUB_CHANNEL_ID) {
			DMBERR("failed: %d\n", ret);
			if (put_user(ret, &argp->tuner_err_code))
				return -EFAULT;
			else
				return -EIO;
		}
	}

	DMBMSG("End\n");

	return 0;
}

static INLINE int tdmb_scan_freq(struct mtv_cb * mtv_cb_ptr, unsigned long arg)
{
	int ret;
	unsigned int ch_freq_khz;
	IOCTL_TDMB_SCAN_INFO __user *argp = (IOCTL_TDMB_SCAN_INFO __user *)arg;
	
	if (get_user(ch_freq_khz, &argp->ch_freq_khz))
		return -EFAULT;

	ret = rtvTDMB_ScanFrequency(mtv_cb_ptr->demod_no, ch_freq_khz);
	if (ret == RTV_SUCCESS)
		return 0;
	else {
		if(ret == RTV_CHANNEL_NOT_DETECTED)
			DMBMSG("Channel not detected: %d\n", ret);
		else
			DMBERR("Device error: %d\n", ret);
		/* Copy the tuner error-code to application */
		if(put_user(ret, &argp->tuner_err_code))
			return -EFAULT;
		else
			return -EINVAL;
	}
}

static INLINE int tdmb_read_fic(struct mtv_cb * mtv_cb_ptr, unsigned long arg)
{
	int ret;
#if defined(RTV_IF_SPI)
	U8 fic_buf[384+1];
#else
	U8 fic_buf[384];
#endif
	IOCTL_TDMB_READ_FIC_INFO __user *argp
					= (IOCTL_TDMB_READ_FIC_INFO __user *)arg;

	ret = rtvTDMB_ReadFIC(mtv_cb_ptr->demod_no, fic_buf);
	if (ret > 0)	{
#if defined(RTV_IF_SPI)
		if (copy_to_user((void __user*)argp->buf, &fic_buf[1], ret))
#else
		if (copy_to_user((void __user*)argp->buf, fic_buf, ret))
#endif
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

static INLINE void tdmb_close_fic(struct mtv_cb *mtv_cb_ptr)
{
	rtvTDMB_CloseFIC(mtv_cb_ptr->demod_no);

	mtv_cb_ptr->fic_opened = FALSE;

#ifdef RTV_FIC_SPI_INTR_ENABLED
	mtv_cb_ptr->wakeup_tspq_thres_cnt = MTV_SPI_WAKEUP_TSP_Q_CNT;
	tdmb_dab_reset_tsp_queue(mtv_cb_ptr);
#endif
}

/* Can be calling anyway  */
static INLINE int tdmb_open_fic(struct mtv_cb *mtv_cb_ptr, unsigned long arg)
{
	int ret;

	ret = rtvTDMB_OpenFIC(mtv_cb_ptr->demod_no);
	if (ret == RTV_SUCCESS) {
		mtv_cb_ptr->fic_opened = TRUE;

#ifdef RTV_FIC_SPI_INTR_ENABLED /* FIC interrupt mode */
		/* Allow thread to read TSP data on read() */
		mtv_cb_ptr->read_stop = FALSE;
		mtv_cb_ptr->wakeup_tspq_thres_cnt = 1; /* For FIC parsing in time. */	
#endif

		return 0;
	}
	else {
		if (put_user(ret, (int *)arg))
			return -EFAULT;
		else
			return -EINVAL;
	}
}

static INLINE void tdmb_deinit(struct mtv_cb *mtv_cb_ptr)
{
	tdmb_close_all_subchannels(mtv_cb_ptr);
	tdmb_close_fic(mtv_cb_ptr);
}

static INLINE int tdmb_init(struct mtv_cb * mtv_cb_ptr, unsigned long arg)
{
	int ret;

	mtv_cb_ptr->tv_mode = DMB_TV_MODE_TDMB;
	atomic_set(&mtv_cb_ptr->num_opened_subch, 0); /* NOT opened state */
	mtv_cb_ptr->fic_opened = FALSE;

#if defined(RTV_IF_SPI) /* Blocking or non-blocking both. */
	mtv_cb_ptr->wakeup_tspq_thres_cnt = MTV_SPI_WAKEUP_TSP_Q_CNT;
#endif

	ret = rtvTDMB_Initialize(mtv_cb_ptr->demod_no, RTV_COUNTRY_BAND_KOREA);
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
#endif /* #ifdef RTV_TDMB_ENABLE */


#ifdef RTV_ISDBT_ENABLE
/*============================================================================
* ISDB-T IO control commands(10~29)
*===========================================================================*/
static INLINE int isdbt_get_tmcc(unsigned long arg)
{
	RTV_ISDBT_TMCC_INFO tmcc_info;
	void __user *argp = (void __user *)arg;

	rtvISDBT_GetTMCC(&tmcc_info);

	if (copy_to_user(argp, &tmcc_info, sizeof(RTV_ISDBT_TMCC_INFO)))
		return -EFAULT;

	return 0;
}

static INLINE int isdbt_get_lock_status(unsigned long arg)
{
	unsigned int lock_mask;
	
	lock_mask = rtvISDBT_GetLockStatus();

	if (put_user(lock_mask, (unsigned int *)arg))
		return -EFAULT;

	return 0;
}

static INLINE int isdbt_get_signal_info(unsigned long arg)
{
	int ret;	
	IOCTL_ISDBT_SIGNAL_INFO sig;
	void __user *argp = (void __user *)arg;

	sig.lock_mask = rtvISDBT_GetLockStatus();
	sig.ber = rtvISDBT_GetBER(); 
	sig.cnr = rtvISDBT_GetCNR();
	sig.ant_level = rtvISDBT_GetAntennaLevel(sig.cnr);
	sig.per = rtvISDBT_GetPER(); 
	sig.rssi = rtvISDBT_GetRSSI(); 

	if (copy_to_user(argp, &sig, sizeof(IOCTL_ISDBT_SIGNAL_INFO)))
		return -EFAULT;

	SHOW_MSC_IF_STATISTICS;

	return ret;
}

static INLINE void isdbt_stop_ts(void)
{
	rtvISDBT_DisableStreamOut();

#if defined(RTV_IF_SPI)
	mtv_reset_tsp_queue();
#endif
}

static INLINE void isdbt_start_ts(void)
{
#if defined(RTV_IF_SPI)
	/* Allow thread to read TSP data on read() */
	mtv_cb_ptr->read_stop = FALSE;
	mtv_cb_ptr->first_interrupt = TRUE;
#endif

	rtvISDBT_EnableStreamOut();

	RESET_MSC_IF_DEBUG;
}

static INLINE int isdbt_set_freq(unsigned long arg)
{
	int ret;
	unsigned int ch_num;
	IOCTL_ISDBT_SET_FREQ_INFO __user *argp
			= (IOCTL_ISDBT_SET_FREQ_INFO __user *)arg;

	if (get_user(ch_num, &argp->ch_num))
		return -EFAULT;

	ret = rtvISDBT_SetFrequency(ch_num);
	if (ret == RTV_SUCCESS)
		return 0;
	else {
		DMBERR("failed: %d\n", ret);

		if (put_user(ret, &argp->tuner_err_code))
			return -EFAULT;
		else
			return -EIO;
	}
}

static INLINE int isdbt_scan_freq(unsigned long arg)
{
	int ret;
	unsigned int ch_num;
	IOCTL_ISDBT_SCAN_INFO __user *argp = (IOCTL_ISDBT_SCAN_INFO __user *)arg;

	if (get_user(ch_num, &argp->ch_num))
		return -EFAULT;

	ret = rtvISDBT_ScanFrequency(ch_num);
	if (ret == RTV_SUCCESS)
		return 0;
	else {
		if (ret == RTV_CHANNEL_NOT_DETECTED)
			DMBMSG("Channel not detected: %d\n", ret);
		else
			DMBERR("Device error: %d\n", ret);

		/* Copy the error-code to application */
		if (put_user(ret, &argp->tuner_err_code))
			return -EFAULT;
		else
			return -EIO;
	}
}

static INLINE void isdbt_deinit(void)
{
	isdbt_stop_ts();	
}

static INLINE int isdbt_init(unsigned long arg)
{
	int ret;
	E_RTV_COUNTRY_BAND_TYPE band_type;
	IOCTL_ISDBT_POWER_ON_INFO __user *argp
			= (IOCTL_ISDBT_POWER_ON_INFO __user *)arg;

	mtv_cb_ptr->tv_mode = DMB_TV_MODE_1SEG;
	mtv_cb_ptr->msc1_thres_size = MTV_TS_THRESHOLD_SIZE;
#if defined(RTV_IF_SPI)
	mtv_cb_ptr->wakeup_tspq_thres_cnt = MTV_SPI_WAKEUP_TSP_Q_CNT;
#endif

	if (get_user(band_type, &argp->country_band_type))
		return -EFAULT;

	ret = rtvISDBT_Initialize(band_type, MTV_TS_THRESHOLD_SIZE);
	if (ret == RTV_SUCCESS)
		return 0;
	else {
		DMBERR("Tuner initialization failed: %d\n", ret);
		if (put_user(ret, &argp->tuner_err_code))
			return -EFAULT;
		else
			return -EIO; /* error occurred during the open() */
	}		
}
#endif /* #ifdef RTV_ISDBT_ENABLE */


#ifdef RTV_FM_ENABLE
/*============================================================================
* FM IO control commands(50 ~ 69)
*===========================================================================*/
static INLINE int fm_get_lock_status(unsigned long arg)
{
	IOCTL_FM_LOCK_STATUS_INFO lock_status;
	void __user *argp = (void __user *)arg;

	rtvFM_GetLockStatus(&lock_status.val, &lock_status.cnt);

	SHOW_MSC_IF_STATISTICS;

	if (copy_to_user(argp, &lock_status, sizeof(IOCTL_FM_LOCK_STATUS_INFO)))
		return -EFAULT;

	return 0;
}

static INLINE int fm_get_rssi(unsigned long arg)
{
	int rssi;

	rssi = rtvFM_GetRSSI();

	if (put_user(rssi, (int *)arg))
		return -EFAULT;

	return 0;
}

static INLINE void fm_stop_ts(void)
{
	rtvFM_DisableStreamOut();

#if defined(RTV_IF_SPI)	
	mtv_reset_tsp_queue();
#endif
}

static INLINE void fm_start_ts(void)
{
#if defined(RTV_IF_SPI)
	/* Allow thread to read TSP data on read() */
	mtv_cb_ptr->read_stop = FALSE;
	mtv_cb_ptr->first_interrupt = TRUE;
#endif

	rtvFM_EnableStreamOut();

	RESET_MSC_IF_DEBUG;
}

static INLINE int fm_set_freq(unsigned long arg)
{
	int ret;
	unsigned int ch_freq_khz;
	IOCTL_FM_SET_FREQ_INFO __user *argp	= (IOCTL_FM_SET_FREQ_INFO __user *)arg;

	if (get_user(ch_freq_khz, &argp->ch_freq_khz))
		return -EFAULT;

	ret = rtvFM_SetFrequency(ch_freq_khz);
	if (ret == RTV_SUCCESS)
		return 0;
	else {
		DMBERR("failed: %d\n", ret);

		if (put_user(ret, &argp->tuner_err_code))
			return -EFAULT;
		else
			return -EIO;
	}
}

static INLINE int fm_search_freq(unsigned long arg)
{
	int ret;
	unsigned int start_freq;
	unsigned int end_freq;
	int srch_freq;
	IOCTL_FM_SRCH_INFO __user *argp = (IOCTL_FM_SRCH_INFO __user *)arg;

	if (get_user(start_freq, &argp->start_freq))
		return -EFAULT;
	
	if (get_user(end_freq, &argp->end_freq))
		return -EFAULT;

	ret = rtvFM_SearchFrequency(&srch_freq, start_freq, end_freq);
	if (ret == RTV_SUCCESS) {
		if(put_user(srch_freq, &argp->detected_freq))
			return -EFAULT;

		return 0;
	}
	else {
		DMBERR("failed: %d\n", ret);

		if (put_user(ret, &argp->tuner_err_code))
			return -EFAULT;
		else
			return -EINVAL;
	}
}

static INLINE int fm_scan_freq(unsigned long arg)
{
	int ret;
	unsigned int start_freq, end_freq, num_ch_buf;
	IOCTL_FM_SCAN_INFO __user *argp = (IOCTL_FM_SCAN_INFO __user *)arg;
	
	if(get_user(start_freq, &argp->start_freq))
		return -EFAULT;
	
	if(get_user(end_freq, &argp->end_freq))
		return -EFAULT;
	
	if(get_user(num_ch_buf, &argp->num_ch_buf))
		return -EFAULT;
	
	//DMBMSG("[fm_scan_freq] start(%d) ~ end(%d)\n", start_freq, end_freq);	
	
	ret = rtvFM_ScanFrequency(argp->ch_buf, num_ch_buf, start_freq, end_freq);
	if (ret >= 0 /* include zero */) {
		if (put_user(ret, &argp->num_detected_ch))
			return -EFAULT;

		return 0;
	}
	else {
		DMBERR("failed: %d\n", ret);

		if (put_user(ret, &argp->tuner_err_code))
			return -EFAULT;
		else
			return -EINVAL;
	}
}

static INLINE void fm_deinit(void)
{
	fm_stop_ts();	
}

static INLINE int fm_init(unsigned long arg)
{
	int ret;
	E_RTV_ADC_CLK_FREQ_TYPE adc_clk_type;
	IOCTL_FM_POWER_ON_INFO __user *argp	= (IOCTL_FM_POWER_ON_INFO __user *)arg;

	mtv_cb_ptr->tv_mode = DMB_TV_MODE_FM;
	mtv_cb_ptr->msc1_thres_size = MTV_TS_THRESHOLD_SIZE;
#if defined(RTV_IF_SPI)
	mtv_cb_ptr->wakeup_tspq_thres_cnt = MTV_SPI_WAKEUP_TSP_Q_CNT;
#endif

	if (get_user(adc_clk_type, &argp->adc_clk_type))
		return -EFAULT;

	ret = rtvFM_Initialize(adc_clk_type, MTV_TS_THRESHOLD_SIZE);
	if (ret == RTV_SUCCESS)
		return 0;
	else {
		DMBERR("Tuner initialization failed: %d\n", ret);
		if (put_user(ret, &argp->tuner_err_code))
			return -EFAULT;
		else
			return -EIO; /* error occurred during the open() */
	}
}
#endif /* #ifdef RTV_FM_ENABLE */

