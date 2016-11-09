/*
 * mtv_ioctl.h
 *
 * NEXELL MTV IO control header file.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef __MTV_IOCTL_H__
#define __MTV_IOCTL_H__


#ifdef __cplusplus 
extern "C"{ 
#endif  

#if !defined(__KERNEL__)
#include "nxtv.h"
#endif

#define NXTV_DEV_NAME		"nxb110_1seg"
#define NXTV_IOC_MAGIC	'R'


#if defined(NXTV_MULTI_SERVICE_MODE)
#define MAX_MULTI_MSC_BUF_SIZE	(2 * 4096)
typedef struct
{
#if (NXTV_NUM_FIC_SVC == 1)
	unsigned int fic_size;
	unsigned char fic_buf[384];
#endif

	unsigned int msc_size[NXTV_NUM_DAB_AVD_SERVICE];  
	unsigned int msc_subch_id[NXTV_NUM_DAB_AVD_SERVICE]; 
	unsigned char msc_buf[NXTV_NUM_DAB_AVD_SERVICE][MAX_MULTI_MSC_BUF_SIZE];
} IOCTL_MULTI_SERVICE_BUF;
#endif

/*============================================================================
 * Test IO control commands(0~10)
 *==========================================================================*/
#define IOCTL_TEST_MTV_POWER_ON	_IO(NXTV_IOC_MAGIC, 0)
#define IOCTL_TEST_MTV_POWER_OFF	_IO(NXTV_IOC_MAGIC, 1)

#define MAX_NUM_MTV_REG_READ_BUF	40
typedef struct
{
	unsigned int page; /* index */
	unsigned int addr; /* input */

	unsigned int write_data;
	
	unsigned int read_cnt;
	unsigned char read_data[MAX_NUM_MTV_REG_READ_BUF]; /* output */
} IOCTL_REG_ACCESS_INFO;
#define IOCTL_TEST_REG_SINGLE_READ	_IOWR(NXTV_IOC_MAGIC, 3, IOCTL_REG_ACCESS_INFO)
#define IOCTL_TEST_REG_BURST_READ	_IOWR(NXTV_IOC_MAGIC, 4, IOCTL_REG_ACCESS_INFO)
#define IOCTL_TEST_REG_WRITE		_IOW(NXTV_IOC_MAGIC, 5, IOCTL_REG_ACCESS_INFO)

typedef struct
{
	unsigned int pin; /* input */
	unsigned int value; /* input for write. output for read.  */
} IOCTL_GPIO_ACCESS_INFO;
#define IOCTL_TEST_GPIO_SET	_IOW(NXTV_IOC_MAGIC, 6, IOCTL_GPIO_ACCESS_INFO)
#define IOCTL_TEST_GPIO_GET	_IOWR(NXTV_IOC_MAGIC, 7, IOCTL_GPIO_ACCESS_INFO)


/*==============================================================================
 * ISDB-T IO control commands(10~29)
 *============================================================================*/
typedef struct
{
	E_NXTV_COUNTRY_BAND_TYPE country_band_type; // input
	int tuner_err_code;  // ouput
} IOCTL_ISDBT_POWER_ON_INFO;

typedef struct
{
	unsigned int ch_num; // input
	int tuner_err_code;  // ouput
} IOCTL_ISDBT_SCAN_INFO;

typedef struct
{
	unsigned int ch_num; // input
	int tuner_err_code;  // ouput
} IOCTL_ISDBT_SET_FREQ_INFO;

typedef struct
{
	unsigned int 	lock_mask;
	unsigned int	ant_level;
	unsigned int	ber; // output
	unsigned int	cnr;  // output
	unsigned int	per;  // output
	int 		rssi;  // output
} IOCTL_ISDBT_SIGNAL_INFO;

#define IOCTL_ISDBT_POWER_ON		_IOWR(NXTV_IOC_MAGIC,10, IOCTL_ISDBT_POWER_ON_INFO)
#define IOCTL_ISDBT_POWER_OFF		_IO(NXTV_IOC_MAGIC, 11)
#define IOCTL_ISDBT_SCAN_FREQ		_IOWR(NXTV_IOC_MAGIC,12, IOCTL_ISDBT_SCAN_INFO)
#define IOCTL_ISDBT_SET_FREQ		_IOWR(NXTV_IOC_MAGIC,13, IOCTL_ISDBT_SET_FREQ_INFO)
#define IOCTL_ISDBT_GET_LOCK_STATUS _IOR(NXTV_IOC_MAGIC,14, unsigned int)
#define IOCTL_ISDBT_GET_TMCC		_IOR(NXTV_IOC_MAGIC,15, NXTV_ISDBT_TMCC_INFO)
#define IOCTL_ISDBT_GET_SIGNAL_INFO	_IOR(NXTV_IOC_MAGIC,16, IOCTL_ISDBT_SIGNAL_INFO)
#define IOCTL_ISDBT_START_TS		_IO(NXTV_IOC_MAGIC, 17)
#define IOCTL_ISDBT_STOP_TS			_IOR(NXTV_IOC_MAGIC, 18, int)

/*============================================================================
* TDMB IO control commands(30 ~ 49)
*===========================================================================*/
#if defined(NXTV_TDMB_ENABLE) || defined(NXTV_DAB_ENABLE)
typedef struct
{
	unsigned int ch_freq_khz; // input
	int tuner_err_code;  // ouput
} IOCTL_TDMB_SCAN_INFO;

typedef struct /* FIC polling mode Only */
{
	unsigned char buf[384];  // ouput
	int tuner_err_code;  // ouput
} IOCTL_TDMB_READ_FIC_INFO;

typedef struct
{
	unsigned int ch_freq_khz; // input
	unsigned int subch_id;  // input
	E_NXTV_SERVICE_TYPE svc_type;   // input
	int tuner_err_code; // ouput
} IOCTL_TDMB_SUB_CH_INFO;

typedef struct
{
	unsigned int 	lock_mask;
	unsigned int	ant_level;
	unsigned int 	ber; // output
	unsigned int 	cer; // output
	unsigned int 	cnr;  // output
	unsigned int 	per;  // output
	int 		rssi;  // output
} IOCTL_TDMB_SIGNAL_INFO;

#define IOCTL_TDMB_POWER_ON			_IOR(NXTV_IOC_MAGIC, 30, int)
#define IOCTL_TDMB_POWER_OFF		_IO(NXTV_IOC_MAGIC, 31)
#define IOCTL_TDMB_SCAN_FREQ		_IOWR(NXTV_IOC_MAGIC,32, IOCTL_TDMB_SCAN_INFO)
#define IOCTL_TDMB_OPEN_FIC			_IOR(NXTV_IOC_MAGIC, 33, int)
#define IOCTL_TDMB_CLOSE_FIC		_IO(NXTV_IOC_MAGIC, 34)
#define IOCTL_TDMB_READ_FIC			_IOR(NXTV_IOC_MAGIC,35, IOCTL_TDMB_READ_FIC_INFO)
#define IOCTL_TDMB_OPEN_SUBCHANNEL		_IOWR(NXTV_IOC_MAGIC,36, IOCTL_TDMB_SUB_CH_INFO)
#define IOCTL_TDMB_CLOSE_SUBCHANNEL		_IOW(NXTV_IOC_MAGIC,37, unsigned int)
#define IOCTL_TDMB_CLOSE_ALL_SUBCHANNELS	_IO(NXTV_IOC_MAGIC,38)
#define IOCTL_TDMB_GET_LOCK_STATUS		_IOR(NXTV_IOC_MAGIC,39, unsigned int)
#define IOCTL_TDMB_GET_SIGNAL_INFO		_IOR(NXTV_IOC_MAGIC,40, IOCTL_TDMB_SIGNAL_INFO)
#endif

/*==============================================================================
 * FM IO control commands(50 ~ 69)
 *============================================================================*/
#define MAX_NUM_FM_EXIST_CHANNEL 256

typedef struct
{
	E_NXTV_ADC_CLK_FREQ_TYPE adc_clk_type; // input
	int tuner_err_code;  // ouput
} IOCTL_FM_POWER_ON_INFO;

typedef struct
{
	unsigned int start_freq; // input
	unsigned int end_freq;   // input
	unsigned int num_ch_buf;  // input
	unsigned int ch_buf[MAX_NUM_FM_EXIST_CHANNEL]; // output
	int num_detected_ch; // output
	int tuner_err_code;  // ouput
} IOCTL_FM_SCAN_INFO;

typedef struct
{
	unsigned int start_freq; // input
	unsigned int end_freq;   // input
	unsigned int detected_freq; /* output */
	int tuner_err_code;  // ouput
} IOCTL_FM_SRCH_INFO;

typedef struct
{
	unsigned int ch_freq_khz; // input
	int tuner_err_code;  // ouput
} IOCTL_FM_SET_FREQ_INFO;

typedef struct
{
	unsigned int val; // output
	unsigned int cnt;   // output
} IOCTL_FM_LOCK_STATUS_INFO;


#define IOCTL_FM_POWER_ON		_IOWR(NXTV_IOC_MAGIC,50, IOCTL_FM_POWER_ON_INFO)
#define IOCTL_FM_POWER_OFF		_IO(NXTV_IOC_MAGIC, 51)
#define IOCTL_FM_SET_FREQ		_IOWR(NXTV_IOC_MAGIC,52, IOCTL_FM_SET_FREQ_INFO)
#define IOCTL_FM_SCAN_FREQ		_IOWR(NXTV_IOC_MAGIC,53, IOCTL_FM_SCAN_INFO)
#define IOCTL_FM_SRCH_FREQ		_IOWR(NXTV_IOC_MAGIC,54, IOCTL_FM_SRCH_INFO)
#define IOCTL_FM_START_TS		_IO(NXTV_IOC_MAGIC, 55)
#define IOCTL_FM_STOP_TS		_IO(NXTV_IOC_MAGIC, 56)
#define IOCTL_FM_GET_LOCK_STATUS	_IOR(NXTV_IOC_MAGIC,57, IOCTL_FM_LOCK_STATUS_INFO)
#define IOCTL_FM_GET_RSSI		_IOR(NXTV_IOC_MAGIC,58, int)


/*==============================================================================
 * DAB IO control commands(70 ~ 89)
 *============================================================================*/
typedef struct
{
	unsigned int ch_freq_khz; // input
	int tuner_err_code;  // ouput
} IOCTL_DAB_SCAN_INFO;

typedef struct /* FIC polling mode Only */
{
	unsigned int size; // input
	unsigned char buf[384];  // ouput
	int tuner_err_code;  // ouput
} IOCTL_DAB_READ_FIC_INFO;

typedef struct
{
	unsigned int ch_freq_khz; // input
	unsigned int subch_id;  // input
	E_NXTV_SERVICE_TYPE svc_type;   // input
	int tuner_err_code;  // ouput
} IOCTL_DAB_SUB_CH_INFO;

typedef struct
{
	unsigned int 	lock_mask;
	unsigned int	ant_level;
	unsigned int 	ber; // output
	unsigned int 	cer; // output
	unsigned int 	cnr;  // output
	unsigned int 	per;  // output
	int 		rssi;  // output
} IOCTL_DAB_SIGNAL_INFO;

#define IOCTL_DAB_POWER_ON			_IOR(NXTV_IOC_MAGIC, 70, int)
#define IOCTL_DAB_POWER_OFF			_IO(NXTV_IOC_MAGIC, 71)
#define IOCTL_DAB_SCAN_FREQ			_IOWR(NXTV_IOC_MAGIC,72, IOCTL_DAB_SCAN_INFO)
#define IOCTL_DAB_GET_FIC_SIZE		_IOR(NXTV_IOC_MAGIC, 73, unsigned int)
#define IOCTL_DAB_OPEN_FIC			_IOR(NXTV_IOC_MAGIC, 74, int)
#define IOCTL_DAB_CLOSE_FIC			_IO(NXTV_IOC_MAGIC, 75)
#define IOCTL_DAB_READ_FIC			_IOWR(NXTV_IOC_MAGIC,76, IOCTL_DAB_READ_FIC_INFO)
#define IOCTL_DAB_OPEN_SUBCHANNEL		_IOWR(NXTV_IOC_MAGIC,77, IOCTL_DAB_SUB_CH_INFO)
#define IOCTL_DAB_CLOSE_SUBCHANNEL		_IOW(NXTV_IOC_MAGIC,78, unsigned int)
#define IOCTL_DAB_CLOSE_ALL_SUBCHANNELS	_IO(NXTV_IOC_MAGIC,79)
#define IOCTL_DAB_GET_LOCK_STATUS		_IOR(NXTV_IOC_MAGIC,80, unsigned int)
#define IOCTL_DAB_GET_SIGNAL_INFO		_IOR(NXTV_IOC_MAGIC,81, IOCTL_DAB_SIGNAL_INFO)

#ifdef __cplusplus 
} 
#endif 

#endif /* __MTV_IOCTL_H__*/




