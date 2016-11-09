/******************************************************************************
* (c) COPYRIGHT 2013 NEXELL, Inc. ALL RIGHTS RESERVED.
*
* This software is the property of NEXELL and is furnished under license
* by NEXELL.
* This software may be used only in accordance with the terms of said license.
* This copyright notice may not be removed, modified or obliterated without
* the prior written permission of NEXELL, Inc.
*
* This software may not be copied, transmitted, provided to or otherwise
* made available to any other person, company, corporation or other entity
* except as specified in the terms of said license.
*
* No right, title, ownership or other interest in the software is hereby
* granted or transferred.
*
* The information contained herein is subject to change without notice
* and should not be construed as a commitment by NEXELL, Inc.
*
* TITLE		: NXB220 configuration header file.
*
* FILENAME	: nxb220_port.h
*
* DESCRIPTION	:
*		Configuration for NEXELL NXB220 Services.
*
******************************************************************************/
/******************************************************************************
* REVISION HISTORY
*
*    DATE         NAME          REMARKS
* ----------  -------------    ------------------------------------------------
* 03/03/2013  Yang, Maverick       Created.
******************************************************************************/

#ifndef __NXB220_PORT_H__
#define __NXB220_PORT_H__

/*=============================================================================
* Includes the user header files if neccessry.
*===========================================================================*/
#if defined(__KERNEL__) /* Linux kernel */
	#include <linux/io.h>
	#include <linux/kernel.h>
	#include <linux/delay.h>
	#include <linux/mm.h>
	#include <linux/mutex.h>
	#include <linux/uaccess.h>
	#include <linux/string.h>
	#include <linux/jiffies.h>

#elif defined(WINCE) || defined(WINDOWS) || defined(WIN32)
	#include <stdio.h>
	#include <windows.h>
	#include <winbase.h>
	#include <string.h>
	#ifdef WINCE
		#include <drvmsg.h>
	#endif
#else
	#include <stdio.h>
	#include <string.h>
#endif

#ifdef __cplusplus
extern "C"{
#endif

/*############################################################################
#
# COMMON configurations
#
############################################################################*/
/*============================================================================
* The slave address for I2C and SPI.
*===========================================================================*/
#define NXTV_CHIP_ADDR	0x43  //7bit I2C Address ID

/*============================================================================
* Modifies the basic data types if necessary.
*===========================================================================*/
typedef int					BOOL;
typedef signed char			SS8;
typedef unsigned char		U8;
typedef signed short		S16;
typedef unsigned short		U16;
typedef signed int			S32;
typedef unsigned int		U32;

typedef int			INT;
typedef unsigned int		UINT;
typedef long			LONG;
typedef unsigned long		ULONG;
 
typedef volatile U8		VU8;
typedef volatile U16		VU16;
typedef volatile U32		VU32;


#if defined(__GNUC__)
    #define INLINE   inline
#elif defined(WINCE) || defined(WINDOWS) || defined(WIN32)
    #define INLINE    __inline
#elif defined(__ARMCC_VERSION)
    #define INLINE    __inline
#else
    /* Need to modified */
    #define INLINE    inline
#endif

/*==============================================================================
* Selects the TV mode(s) to target product.
*============================================================================*/
#define NXTV_ISDBT_ENABLE
//#define NXTV_DVBT_ENABLE

#if defined(NXTV_ISDBT_ENABLE)
//	#define NXTV_ISDBT_1SEG_LPMODE_ENABLE
#endif 

/*============================================================================
* Defines the Dual Diversity Enable
*===========================================================================*/
//#define NXTV_DUAL_DIVERISTY_ENABLE

#ifdef NXTV_DUAL_DIVERISTY_ENABLE
	extern U8 g_div_i2c_chip_id;
	#define NXTV_CHIP_ADDR_SLAVE 0x44
	//	#define NXTV_DIVER_TWO_XTAL_USED //define for two X-TAL using both M,S 
#endif

/*============================================================================
* Defines the power Mode
*===========================================================================*/
//#define NXTV_EXT_POWER_MODE

/*============================================================================
* Defines the package type of chip to target product.
*===========================================================================*/
//#define NXTV_CHIP_PKG_CSP
#define NXTV_CHIP_PKG_QFN

/*============================================================================
* Defines the external source frequency in KHz.
* Ex> #define NXTV_SRC_CLK_FREQ_KHz	32000 // 32MHz
*===========================================================================*/
#define NXTV_SRC_CLK_FREQ_KHz			32000//32MHz
//#define NXTV_SRC_CLK_FREQ_KHz			19200//19.MHz
 
/*============================================================================
* Defines the Host interface.
*===========================================================================*/
//#define NXTV_IF_SPI /* AP: SPI Master Mode */
//#define NXTV_IF_TSIF_0 /* I2C + TSIF0<GPDx pinout> for Serial Out Master Mode*/
#define NXTV_IF_TSIF_1 /* I2C + TSIF1<GDDx pinout> For Serial/Parallel Out Master Mode*/
//#define NXTV_IF_SPI_SLAVE /* AP: SPI Slave Mode: control: I2C, data: SPI  */
//#define NXTV_IF_SPI_TSIFx /* AP: SPI Master Mode for Control and please */
                           /*    define NXTV_IF_TSIF_0  or NXTV_IF_TSIF_1 for TSIF */

#if defined(NXTV_IF_SPI_TSIFx)
    #if defined(NXTV_IF_TSIF_0)
	    #error "TSIF0 can't use in case of SPI used for Register Control"
    #endif
#endif

#if defined(NXTV_IF_TSIF_0) || defined(NXTV_IF_TSIF_1) || defined(NXTV_IF_SPI_SLAVE)
//#define NXTV_IF_CSI656_RAW_8BIT_ENABLE /*Sync signal pre-move(4clock) + 1 clock add Mode*/
#endif

#ifdef NXTV_DUAL_DIVERISTY_ENABLE
	#if defined(NXTV_IF_TSIF_1) || defined(NXTV_IF_SPI) || defined(NXTV_IF_SPI_SLAVE)
		 #error "Diversity function is not supported for defined interface"  
	#endif
#endif

/*============================================================================
* Defines the feature of SPI speed(> 30MHz) for SPI interface.
*===========================================================================*/
#if defined(NXTV_IF_SPI)
	//#define NXTV_SPI_HIGH_SPEED_ENABLE
#endif

/* Determine if the output of error-tsp is disable. */
#define NXTV_ERROR_TSP_OUTPUT_DISABLE
//#define NXTV_NULL_PID_TSP_OUTPUT_DISABLE

#ifndef NXTV_ERROR_TSP_OUTPUT_DISABLE
	/* Determine if the NULL PID will generated for error-tsp. */
	//#define NXTV_NULL_PID_GENERATE
#endif /* NXTV_ERROR_TSP_OUTPUT_DISABLE */

#if defined(NXTV_IF_TSIF_0) && defined(NXTV_IF_TSIF_1)
/*If TSIF0 & TSIF1 is Enabled, TS0= LayerA, TS1 = LayerB out..*/
//	#define DUAL_PORT_TSOUT_ENABLE
#endif

/*============================================================================
* Defines the polarity of interrupt if necessary.
*===========================================================================*/
#define NXTV_INTR_POLARITY_LOW_ACTIVE
//#define NXTV_INTR_POLARITY_HIGH_ACTIVE

/*============================================================================
* Defines the delay macro in milliseconds.
*===========================================================================*/
#if defined(__KERNEL__) /* Linux kernel */
	static INLINE void NXTV_DELAY_MS(UINT ms)
	{
		unsigned long start_jiffies, end_jiffies;
		UINT diff_time;
		UINT _1ms_cnt = ms;

		start_jiffies = get_jiffies_64();

		do {
			end_jiffies = get_jiffies_64();
			
			diff_time = jiffies_to_msecs(end_jiffies - start_jiffies);
			if (diff_time >= ms)
				break;

			mdelay(1);		
		} while (--_1ms_cnt);
	}

#elif defined(WINCE)
	#define NXTV_DELAY_MS(ms)    Sleep(ms)

#else
	void mtv_delay_ms(int ms);
	#define NXTV_DELAY_MS(ms)    mtv_delay_ms(ms) /* TODO */
#endif

/*============================================================================
* Defines the debug message macro.
*===========================================================================*/
#if 1
    #define NXTV_DBGMSG0(fmt)                   printk(fmt)
    #define NXTV_DBGMSG1(fmt, arg1)             printk(fmt, arg1)
    #define NXTV_DBGMSG2(fmt, arg1, arg2)       printk(fmt, arg1, arg2)
    #define NXTV_DBGMSG3(fmt, arg1, arg2, arg3) printk(fmt, arg1, arg2, arg3)
#else
    /* To eliminates the debug messages. */
    #define NXTV_DBGMSG0(fmt)			do {} while (0)
    #define NXTV_DBGMSG1(fmt, arg1)		do {} while (0)
    #define NXTV_DBGMSG2(fmt, arg1, arg2)	do {} while (0)
    #define NXTV_DBGMSG3(fmt, arg1, arg2, arg3)	do {} while (0)
#endif
/*#### End of Common ###########*/

/*############################################################################
#
# ISDB-T specific configurations
#
############################################################################*/

/*############################################################################
#
# Host Interface specific configurations
#
############################################################################*/
#if defined(NXTV_IF_SPI) || defined(NXTV_IF_SPI_TSIFx)
    /*=================================================================
    * Defines the register I/O macros.
    *================================================================*/
	U8 isdbt_spi_read(U8 page, U8 reg);
	void isdbt_spi_read_burst(U8 page, U8 reg, U8 *buf, int size);
	void isdbt_spi_write(U8 page, U8 reg, U8 val);
	void isdbt_spi_recover(unsigned char *buf, unsigned int size);
	extern U8 g_bRtvPage;

	static INLINE U8 NXTV_REG_GET(U8 reg)
	{
		return (U8)isdbt_spi_read(g_bRtvPage, (U8)(reg));
	}

	#define NXTV_REG_BURST_GET(reg, buf, size)\
		isdbt_spi_read_burst(g_bRtvPage, (U8)(reg), buf, (size))

	#define NXTV_REG_SET(reg, val)\
		isdbt_spi_write(g_bRtvPage, (U8)(reg), (U8)(val))

	#define NXTV_REG_MASK_SET(reg, mask, val)\
	do {					\
		U8 tmp;				\
		tmp = (NXTV_REG_GET(reg)|(U8)(mask))\
				& (U8)((~(mask))|(val));\
		NXTV_REG_SET(reg, tmp);		\
	} while (0)

	#define NXTV_TSP_XFER_SIZE	188
#endif

#if defined(NXTV_IF_TSIF_0) || defined(NXTV_IF_TSIF_1) ||  defined(NXTV_IF_SPI_SLAVE)
	/*=================================================================
	* Defines the TS format.
	*================================================================*/
	#define NXTV_TSIF_FORMAT_0 /* Serial: EN_high, CLK_rising */
	//#define NXTV_TSIF_FORMAT_1 /*   Serial: EN_high, CLK_falling */ //
	//#define NXTV_TSIF_FORMAT_2 /* Serial: EN_low, CLK_rising */
	//#define NXTV_TSIF_FORMAT_3 /* Serial: EN_low, CLK_falling */
	//#define NXTV_TSIF_FORMAT_4 /* Serial: EN_high, CLK_rising + 1CLK add */
	//#define NXTV_TSIF_FORMAT_5 /* Serial: EN_high, CLK_falling + 1CLK add */
	//#define NXTV_TSIF_FORMAT_6 /* Parallel: EN_high, CLK_rising */
	//#define NXTV_TSIF_FORMAT_7 /* Parallel: EN_high, CLK_falling */

	/*=================================================================
	* Defines the TSIF speed.
	*================================================================*/
	//#define NXTV_TSIF_SPEED_500_kbps  
	//#define NXTV_TSIF_SPEED_1_Mbps  
	//#define NXTV_TSIF_SPEED_2_Mbps 
	//#define NXTV_TSIF_SPEED_4_Mbps
	//#define NXTV_TSIF_SPEED_7_Mbps 
	//#define NXTV_TSIF_SPEED_15_Mbps 
	#define NXTV_TSIF_SPEED_30_Mbps 
	//#define NXTV_TSIF_SPEED_60_Mbps 

	/*=================================================================
	* Defines the TSP size. 188 or 204
	*================================================================*/
	#define NXTV_TSP_XFER_SIZE	188

	#ifndef NXTV_IF_SPI_TSIFx
	/*=================================================================
	* Defines the register I/O macros.
	*================================================================*/
	U8 isdbt_i2c_read(U8 i2c_chip_addr, U8 reg);
	void isdbt_i2c_write(U8 i2c_chip_addr, U8 reg, U8 val);
	void isdbt_i2c_read_burst(U8 i2c_chip_addr, U8 reg, U8 *buf, int size);

	#ifdef NXTV_DUAL_DIVERISTY_ENABLE		
		#define	NXTV_REG_GET(reg)            		isdbt_i2c_read((U8)g_div_i2c_chip_id,(U8)reg)
		#define	NXTV_REG_SET(reg, val)       		isdbt_i2c_write((U8)g_div_i2c_chip_id,(U8)reg, (U8)val)
		#define NXTV_REG_BURST_GET(reg, buf, size)\
					isdbt_i2c_read_burst(g_div_i2c_chip_id, (U8)(reg), buf, (size))

	#else
		#define	NXTV_REG_GET(reg)         isdbt_i2c_read(NXTV_CHIP_ADDR, (U8)reg)
		#define	NXTV_REG_SET(reg, val)      isdbt_i2c_write(NXTV_CHIP_ADDR, (U8)reg, (U8)val)
		#define NXTV_REG_BURST_GET(reg, buf, size)\
			isdbt_i2c_read_burst(NXTV_CHIP_ADDR, (U8)(reg), buf, size)
	#endif

	#define	NXTV_REG_MASK_SET(reg, mask, val) 								\
		do {																\
			U8 tmp;															\
			tmp = (NXTV_REG_GET(reg)|(U8)(mask)) & (U8)((~(mask))|(val));	\
			NXTV_REG_SET(reg, tmp);											\
		} while(0)
	#endif

#endif

#if defined(NXTV_IF_EBI2)

	#define NXTV_EBI2_BUS_WITDH_16 // 
	//#define NXTV_EBI2_BUS_WITDH_32 //

	/*=================================================================
	* Defines the register I/O macros.
	*================================================================*/
	U8 isdbt_ebi2_read(U8 page, U8 reg);
	void isdbt_ebi2_read_burst(U8 page, U8 reg, U8 *buf, int size);
	void isdbt_ebi2_write(U8 page, U8 reg, U8 val);	
	extern U8 g_bRtvPage;

	static INLINE U8 NXTV_REG_GET(U8 reg)
	{
		return (U8)isdbt_ebi2_read(g_bRtvPage, (U8)(reg));
	}

	#define NXTV_REG_BURST_GET(reg, buf, size)\
		isdbt_ebi2_read_burst(g_bRtvPage, (U8)(reg), buf, (size))

	#define NXTV_REG_SET(reg, val)\
		isdbt_ebi2_write(g_bRtvPage, (U8)(reg), (U8)(val))

	#define NXTV_REG_MASK_SET(reg, mask, val)\
	do {					\
		U8 tmp;				\
		tmp = (NXTV_REG_GET(reg)|(U8)(mask))\
				& (U8)((~(mask))|(val));\
		NXTV_REG_SET(reg, tmp);		\
	} while (0)

	#define NXTV_TSP_XFER_SIZE	188
#endif


/*############################################################################
#
# Pre-definintion by NEXELL.
#
############################################################################*/


/*############################################################################
#
# Defines the critical object and macros.
#
############################################################################*/
#if defined(NXTV_IF_SPI) || defined(NXTV_IF_EBI2)
    #if defined(__KERNEL__)
	extern struct mutex nxtv_guard_fullseg;
	#define NXTV_GUARD_INIT		mutex_init(&nxtv_guard_fullseg)
	#define NXTV_GUARD_LOCK		mutex_lock(&nxtv_guard_fullseg)
	#define NXTV_GUARD_FREE		mutex_unlock(&nxtv_guard_fullseg)
	#define NXTV_GUARD_DEINIT	((void)0)

    #elif defined(WINCE) || defined(WINDOWS) || defined(WIN32)
	extern CRITICAL_SECTION		nxtv_guard_fullseg;
	#define NXTV_GUARD_INIT		InitializeCriticalSection(&nxtv_guard_fullseg)
	#define NXTV_GUARD_LOCK		EnterCriticalSection(&nxtv_guard_fullseg)
	#define NXTV_GUARD_FREE		LeaveCriticalSection(&nxtv_guard_fullseg)
	#define NXTV_GUARD_DEINIT	DeleteCriticalSection(&nxtv_guard_fullseg)
    #else
	/* temp: TODO */
	#define NXTV_GUARD_INIT		((void)0)
	#define NXTV_GUARD_LOCK		((void)0)
	#define NXTV_GUARD_FREE		((void)0)
	#define NXTV_GUARD_DEINIT	((void)0)
    #endif
#else
	#define NXTV_GUARD_INIT		((void)0)
	#define NXTV_GUARD_LOCK		((void)0)
	#define NXTV_GUARD_FREE		((void)0)
	#define NXTV_GUARD_DEINIT	((void)0)
#endif

/*############################################################################
#
# Check erros by user-configurations.
#
############################################################################*/
#if !defined(NXTV_CHIP_PKG_CSP) && !defined(NXTV_CHIP_PKG_QFN)
	#error "Must define the package type !"
#endif

#if defined(NXTV_IF_TSIF_0) || defined(NXTV_IF_TSIF_1) ||  defined(NXTV_IF_SPI_SLAVE)\
|| defined(NXTV_IF_SPI)
    #if (NXTV_CHIP_ADDR >= 0xFF)
	#error "Invalid chip address"
    #endif

#elif defined(NXTV_IF_EBI2)

#else
	#error "Must define the interface definition !"
#endif



#ifndef NXTV_TSP_XFER_SIZE
	#error "Must define the NXTV_TSP_XFER_SIZE definition !"
#endif

#if (NXTV_TSP_XFER_SIZE != 188) && (NXTV_TSP_XFER_SIZE != 204)
	#error "Must 188 or 204 for TS size"
#endif

#if defined(NXTV_IF_SPI) || defined(NXTV_IF_EBI2)
extern BOOL g_bRtvSpiHighSpeed;
#endif

void nxtvOEM_PowerOn_FULLSEG(int on);

#ifdef __cplusplus
}
#endif

#endif /* __NXB220_PORT_H__ */

