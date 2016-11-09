/*
 * tdmb_ioctl.h
 *
 * TDMB IO control header file.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef __TDMB_IOCTL_H__
#define __TDMB_IOCTL_H__


#ifdef __cplusplus 
extern "C"{ 
#endif  

#ifndef __KERNEL__
	#include "mtv319.h"
#endif

#define TDMB_DEV_NAME	"nxb110"
//#define TDMB_DEV_NAME	"nxb111_tdmb"
#define TDMB_IOC_MAGIC	'R'

#if defined(RTV_MULTIPLE_CHANNEL_MODE)
#define MAX_MULTI_MSC_BUF_SIZE	(2 * 4096)
typedef struct {
#ifdef RTV_SPI_FIC_DECODE_IN_PLAY
	unsigned int fic_size;
	unsigned char fic_buf[384];
#endif

	unsigned int msc_size[RTV_MAX_NUM_USE_SUBCHANNEL];  
	unsigned int msc_subch_id[RTV_MAX_NUM_USE_SUBCHANNEL]; 
	unsigned char msc_buf[RTV_MAX_NUM_USE_SUBCHANNEL][MAX_MULTI_MSC_BUF_SIZE];
} IOCTL_TDMB_MULTI_SVC_BUF;
#endif

/*============================================================================
 * Test IO control commands(0~10)
 *==========================================================================*/
#define IOCTL_TEST_MTV_POWER_ON		_IO(TDMB_IOC_MAGIC, 0)
#define IOCTL_TEST_MTV_POWER_OFF	_IO(TDMB_IOC_MAGIC, 1)

#define MAX_NUM_MTV_REG_READ_BUF	(16 * 188)
typedef struct {
	int	demod_no;
	unsigned int page; /* page value */
	unsigned int addr; /* input */

	unsigned int write_data;

	unsigned long param1;
	
	unsigned int read_cnt;
	unsigned char read_data[MAX_NUM_MTV_REG_READ_BUF]; /* output */
} IOCTL_REG_ACCESS_INFO;


#define IOCTL_TEST_REG_SINGLE_READ	_IOWR(TDMB_IOC_MAGIC, 3, IOCTL_REG_ACCESS_INFO)
#define IOCTL_TEST_REG_BURST_READ	_IOWR(TDMB_IOC_MAGIC, 4, IOCTL_REG_ACCESS_INFO)
#define IOCTL_TEST_REG_WRITE		_IOW(TDMB_IOC_MAGIC, 5, IOCTL_REG_ACCESS_INFO)
#define IOCTL_TEST_REG_SPI_MEM_READ	_IOWR(TDMB_IOC_MAGIC, 6, IOCTL_REG_ACCESS_INFO)
#define IOCTL_TEST_REG_ONLY_SPI_MEM_READ _IOWR(TDMB_IOC_MAGIC, 7, IOCTL_REG_ACCESS_INFO)

typedef struct {
	int	demod_no;
	unsigned int pin; /* input */
	unsigned int value; /* input for write. output for read.  */
} IOCTL_GPIO_ACCESS_INFO;
#define IOCTL_TEST_GPIO_SET	_IOW(TDMB_IOC_MAGIC, 6, IOCTL_GPIO_ACCESS_INFO)
#define IOCTL_TEST_GPIO_GET	_IOWR(TDMB_IOC_MAGIC, 7, IOCTL_GPIO_ACCESS_INFO)


/*============================================================================
* TDMB IO control commands(10 ~ 29)
*===========================================================================*/
typedef struct
{
	int	demod_no; /* Must frist! */
	int tuner_err_code; // ouput
} IOCTL_POWER_ON_INFO;

typedef struct
{
	int	demod_no; /* Must frist! */
} IOCTL_POWER_OFF_INFO;

typedef struct {
	int	demod_no;
	unsigned int ch_freq_khz; // input
	int tuner_err_code;  // ouput
} IOCTL_TDMB_SCAN_INFO;

typedef struct
{
	int	demod_no; /* Must frist! */
	int tuner_err_code;  // ouput
} IOCTL_OPEN_FIC_INFO;

typedef struct
{
	int	demod_no; /* Must frist! */
} IOCTL_CLOSE_FIC_INFO;

typedef struct { /* FIC polling mode Only */
	int	demod_no;
	unsigned char buf[384];  // ouput
	int tuner_err_code;  // ouput
} IOCTL_TDMB_READ_FIC_INFO;

typedef struct {
	int	demod_no;
	unsigned int ch_freq_khz; // input
	unsigned int subch_id;  // input
	enum E_RTV_SERVICE_TYPE svc_type;   // input
	int tuner_err_code; // ouput
} IOCTL_TDMB_SUB_CH_INFO;

typedef struct
{
	int	demod_no; /* Must frist! */
	unsigned int subch_id;
} IOCTL_CLOSE_SUBCHANNEL_INFO;

typedef struct
{
	int	demod_no; /* Must frist! */
} IOCTL_CLOSE_ALL_SUBCHANNELS_INFO;

typedef struct
{
	int	demod_no; /* Must frist! */
	unsigned int lock_mask;
} IOCTL_TDMB_GET_LOCK_STATUS_INFO;

typedef struct {
	int	demod_no;
	unsigned int 	lock_mask;
	unsigned int	ant_level;
	unsigned int 	ber; // output
	unsigned int 	cer; // output
	unsigned int 	cnr; // output
	unsigned int 	per; // output
	int 		rssi; // output
} IOCTL_TDMB_SIGNAL_INFO;

#define IOCTL_TDMB_POWER_ON		_IOR(TDMB_IOC_MAGIC, 30, IOCTL_POWER_ON_INFO)
#define IOCTL_TDMB_POWER_OFF		_IOR(TDMB_IOC_MAGIC, 31, IOCTL_POWER_OFF_INFO)
#define IOCTL_TDMB_SCAN_FREQ		_IOWR(TDMB_IOC_MAGIC,32, IOCTL_TDMB_SCAN_INFO)
#define IOCTL_TDMB_OPEN_FIC		_IOR(TDMB_IOC_MAGIC, 33, IOCTL_OPEN_FIC_INFO)
#define IOCTL_TDMB_CLOSE_FIC		_IOR(TDMB_IOC_MAGIC, 34, IOCTL_CLOSE_FIC_INFO)
#define IOCTL_TDMB_READ_FIC		_IOR(TDMB_IOC_MAGIC,35, IOCTL_TDMB_READ_FIC_INFO)
#define IOCTL_TDMB_OPEN_SUBCHANNEL	_IOWR(TDMB_IOC_MAGIC,36, IOCTL_TDMB_SUB_CH_INFO)
#define IOCTL_TDMB_CLOSE_SUBCHANNEL	_IOW(TDMB_IOC_MAGIC,37, IOCTL_CLOSE_SUBCHANNEL_INFO)
#define IOCTL_TDMB_CLOSE_ALL_SUBCHANNELS _IOR(TDMB_IOC_MAGIC,38, IOCTL_CLOSE_ALL_SUBCHANNELS_INFO)
#define IOCTL_TDMB_GET_LOCK_STATUS	_IOR(TDMB_IOC_MAGIC,39, IOCTL_TDMB_GET_LOCK_STATUS_INFO)
#define IOCTL_TDMB_GET_SIGNAL_INFO	_IOR(TDMB_IOC_MAGIC,40, IOCTL_TDMB_SIGNAL_INFO)

#ifdef __cplusplus 
} 
#endif 

#endif /* __TDMB_IOCTL_H__*/




