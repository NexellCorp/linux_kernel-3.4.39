/*
 * isdbt_ioctl.h
 *
 * ISDBT IO control header file.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef __ISDBT_IOCTL_H__
#define __ISDBT_IOCTL_H__

#ifdef __cplusplus 
extern "C"{ 
#endif  

#if defined(__KERNEL__) /* Linux kernel */
	#include <linux/ioctl.h>
#elif !defined(__KERNEL__) && defined(__linux__) /* Linux application */
	#include <sys/ioctl.h>
#elif defined(WINCE) || defined(WINDOWS) || defined(WIN32)
	#error "Code not present" // TODO
#else
	#error "Code not present"
#endif

#define ISDBT_DEV_NAME	"nxb220"
#define ISDBT_IOC_MAGIC	'R'

/*============================================================================
 * Test IO control commands(0~10)
 *==========================================================================*/
#define IOCTL_TEST_MTV_POWER_ON		_IO(ISDBT_IOC_MAGIC, 0)
#define IOCTL_TEST_MTV_POWER_OFF	_IO(ISDBT_IOC_MAGIC, 1)

#define MAX_NUM_MTV_REG_READ_BUF	(16 * 188)
typedef struct {
	unsigned int page; /* page value */
	unsigned int addr; /* input */

	unsigned int write_data;

	unsigned long param1;
	
	unsigned int read_cnt;
	unsigned char read_data[MAX_NUM_MTV_REG_READ_BUF]; /* output */
} IOCTL_REG_ACCESS_INFO;


#define IOCTL_TEST_REG_SINGLE_READ	_IOWR(ISDBT_IOC_MAGIC, 3, IOCTL_REG_ACCESS_INFO)
#define IOCTL_TEST_REG_BURST_READ	_IOWR(ISDBT_IOC_MAGIC, 4, IOCTL_REG_ACCESS_INFO)
#define IOCTL_TEST_REG_WRITE		_IOW(ISDBT_IOC_MAGIC, 5, IOCTL_REG_ACCESS_INFO)
#define IOCTL_TEST_REG_SPI_MEM_READ	_IOWR(ISDBT_IOC_MAGIC, 6, IOCTL_REG_ACCESS_INFO)
#define IOCTL_TEST_REG_ONLY_SPI_MEM_READ _IOWR(ISDBT_IOC_MAGIC, 7, IOCTL_REG_ACCESS_INFO)

typedef struct {
	unsigned int pin; /* input */
	unsigned int value; /* input for write. output for read.  */
} IOCTL_GPIO_ACCESS_INFO;
#define IOCTL_TEST_GPIO_SET	_IOW(ISDBT_IOC_MAGIC, 6, IOCTL_GPIO_ACCESS_INFO)
#define IOCTL_TEST_GPIO_GET	_IOWR(ISDBT_IOC_MAGIC, 7, IOCTL_GPIO_ACCESS_INFO)


/*============================================================================
* TDMB IO control commands(10 ~ 29)
*===========================================================================*/
typedef struct {
	int bandwidth; // enum E_NXTV_BANDWIDTH_TYPE
	unsigned int spi_intr_size[7]; // input
	int tuner_err_code;  // ouput
} IOCTL_ISDBT_POWER_ON_INFO;

typedef struct {
	unsigned int freq_khz; // input
	unsigned int subch_id;  // input
	int svc_type;   // input: enum E_NXTV_SERVICE_TYPE
	int bandwidth;   // input: enum E_NXTV_BANDWIDTH_TYPE
	int tuner_err_code;  // ouput
} IOCTL_ISDBT_SCAN_INFO;

typedef struct {
	unsigned int freq_khz; // input
	unsigned int subch_id;  // input
	int svc_type; // input: enum E_NXTV_SERVICE_TYPE
	int bandwidth; // input: enum E_NXTV_BANDWIDTH_TYPE
	int tuner_err_code; // ouput
} IOCTL_ISDBT_SET_CH_INFO;

typedef struct {
	unsigned int 	lock_mask;
	unsigned int	ant_level;
	unsigned int 	ber; // output
	unsigned int 	cnr; // output
	unsigned int 	per; // output
	int 		rssi; // output
} IOCTL_ISDBT_SIGNAL_INFO;

typedef struct {
	unsigned int	lock_mask;
	int				rssi;

	unsigned int	ber_layer_A;
	unsigned int	ber_layer_B;

	unsigned int	per_layer_A;
	unsigned int	per_layer_B;

	unsigned int	cnr_layer_A;
	unsigned int	cnr_layer_B;

	unsigned int	ant_level_layer_A;
	unsigned int	ant_level_layer_B;
} IOCTL_ISDBT_SIGNAL_QUAL_INFO;


#define IOCTL_ISDBT_POWER_ON	_IOWR(ISDBT_IOC_MAGIC, 10, IOCTL_ISDBT_POWER_ON_INFO)
#define IOCTL_ISDBT_POWER_OFF	_IO(ISDBT_IOC_MAGIC, 11)
#define IOCTL_ISDBT_SCAN_CHANNEL _IOWR(ISDBT_IOC_MAGIC,12, IOCTL_ISDBT_SCAN_INFO)
#define IOCTL_ISDBT_SET_CHANNEL	_IOWR(ISDBT_IOC_MAGIC,13, IOCTL_ISDBT_SET_CH_INFO)
#define IOCTL_ISDBT_START_TS	_IO(ISDBT_IOC_MAGIC, 14)
#define IOCTL_ISDBT_STOP_TS	_IO(ISDBT_IOC_MAGIC, 15)
#define IOCTL_ISDBT_GET_LOCK_STATUS _IOR(ISDBT_IOC_MAGIC,16, unsigned int)
#define IOCTL_ISDBT_GET_SIGNAL_INFO _IOR(ISDBT_IOC_MAGIC,17, IOCTL_ISDBT_SIGNAL_INFO)
#define IOCTL_ISDBT_SUSPEND		_IO(ISDBT_IOC_MAGIC, 18)
#define IOCTL_ISDBT_RESUME		_IO(ISDBT_IOC_MAGIC, 19)

typedef struct {
	unsigned int	ber_layer_A;
	unsigned int	ber_layer_B;
	unsigned int	per_layer_A;
	unsigned int	per_layer_B;
} IOCTL_ISDBT_BER_PER_INFO;
#define IOCTL_ISDBT_GET_BER_PER_INFO _IOR(ISDBT_IOC_MAGIC, 20, IOCTL_ISDBT_BER_PER_INFO)
#define IOCTL_ISDBT_GET_RSSI _IOR(ISDBT_IOC_MAGIC, 21, int)
#define IOCTL_ISDBT_GET_CNR _IOR(ISDBT_IOC_MAGIC, 22, int)
#define IOCTL_ISDBT_GET_SIGNAL_QUAL_INFO _IOR(ISDBT_IOC_MAGIC, 23, IOCTL_ISDBT_SIGNAL_QUAL_INFO)

#ifdef __cplusplus 
} 
#endif 

#endif /* __ISDBT_IOCTL_H__*/




