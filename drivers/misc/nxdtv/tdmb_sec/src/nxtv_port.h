/******************************************************************************** 
* (c) COPYRIGHT 2010 NEXELL, Inc. ALL RIGHTS RESERVED.
* 
* This software is the property of NEXELL and is furnished under license by NEXELL.                
* This software may be used only in accordance with the terms of said license.                         
* This copyright noitce may not be remoced, modified or obliterated without the prior                  
* written permission of NEXELL, Inc.                                                                 
*                                                                                                      
* This software may not be copied, transmitted, provided to or otherwise made available                
* to any other person, company, corporation or other entity except as specified in the                 
* terms of said license.                                                                               
*                                                                                                      
* No right, title, ownership or other interest in the software is hereby granted or transferred.       
*                                                                                                      
* The information contained herein is subject to change without notice and should 
* not be construed as a commitment by NEXELL, Inc.                                                                    
* 
* TITLE 	  : NEXELL TV configuration header file. 
*
* FILENAME    : nxtv_port.h
*
* DESCRIPTION : 
*		Configuration for NEXELL TV Services.
*
********************************************************************************/

/******************************************************************************** 
* REVISION HISTORY
*
*    DATE	  	  NAME				REMARKS
* ----------  -------------    --------------------------------------------------
* 10/12/2010  Ko, Kevin        Added the code of conutry for NXTV_CONUTRY_ARGENTINA.
* 10/01/2010  Ko, Kevin        Changed the debug message macro names.
* 09/27/2010  Ko, Kevin        Creat for CS Realease
*             /Yang, Maverick  1.Reformating for CS API
*                              2.pll table, ADC clock switching, SCAN function, 
*								 FM function added..
* 04/09/2010  Yang, Maverick   REV1 SETTING 
* 01/25/2010  Yang, Maverick   Created.                                                   
********************************************************************************/

#ifndef __NXTV_PORT_H__
#define __NXTV_PORT_H__

/*=============================================================================
 * Includes the user header files if neccessry.
 *============================================================================*/ 
#if defined(__KERNEL__) /* Linux kernel */
    #include <linux/io.h>
    #include <linux/kernel.h>
    #include <linux/delay.h>
    #include <linux/mm.h>
    #include <linux/mutex.h>
    #include <linux/uaccess.h>
	
#elif defined(WINCE)
    #include <windows.h>
    #include <drvmsg.h>
    
#else
	#include <stdio.h>   
#endif

#ifdef __cplusplus 
extern "C"{ 
#endif

#define NXTV_MAX_NUM_DEMOD_CHIP	1

/*############################################################################
#
# COMMON configurations
#
############################################################################*/
/*============================================================================
* The slave address for I2C and SPI, the base address for EBI2.
*===========================================================================*/
#define NXTV_CHIP_ADDR	0x86 

/*============================================================================
* Modifies the basic data types if neccessry.
*===========================================================================*/
typedef int					BOOL;
typedef signed char			SS8;
typedef unsigned char		U8;
typedef signed short		S16;
typedef unsigned short		U16;
typedef signed int			S32;
typedef unsigned int		U32;

typedef int                 INT;
typedef unsigned int        UINT;
typedef long                LONG;
typedef unsigned long       ULONG;
 
typedef volatile U8			VU8;
typedef volatile U16		VU16;
typedef volatile U32		VU32;

#if defined(__GNUC__)
	#define INLINE		inline
#elif defined(_WIN32)
	#define INLINE		__inline
#elif defined(__ARMCC_VERSION)
	#define INLINE		__inline
#else
	/* Need to modified */
	#define INLINE		inline
#endif

/*============================================================================
* Selects the TV mode(s) to target product.
*===========================================================================*/
//#define NXTV_ISDBT_ENABLE
#define NXTV_TDMB_ENABLE
//#define NXTV_FM_ENABLE
//#define NXTV_DAB_ENABLE


/*============================================================================
* Defines the package type of chip to target product.
*===========================================================================*/
///TEST
//#define NXTV_CHIP_PKG_WLCSP	// NXB220/318
#define NXTV_CHIP_PKG_QFN		// NXB110
//#define NXTV_CHIP_PKG_LGA	// NXB250/350


/*============================================================================
* Defines the external source freqenecy in KHz.
* Ex> #define NXTV_SRC_CLK_FREQ_KHz	36000 // 36MHz
*=============================================================================
* MTV250 : #define NXTV_SRC_CLK_FREQ_KHz  32000  //must be defined 
* MTV350 : #define NXTV_SRC_CLK_FREQ_KHz  24576  //must be defined 
*===========================================================================*/
#define NXTV_SRC_CLK_FREQ_KHz			24576//36000//24576
//#define NXTV_SRC_CLK_FREQ_KHz			19200
	

/*============================================================================
* Define the power type.
*============================================================================*/  
//#define NXTV_PWR_EXTERNAL
#define NXTV_PWR_LDO
//#define NXTV_PWR_DCDC


/*============================================================================
* Defines the I/O voltage.
*===========================================================================*/
#define NXTV_IO_1_8V
//#define NXTV_IO_2_5V
//#define NXTV_IO_3_3V


#if defined(NXTV_IO_2_5V) || defined(NXTV_IO_3_3V)
	#error "If VDDIO pin is connected with IO voltage, NXTV_IO_1_8V must be defined,please check the HW pin connection"
	#error "If VDDIO pin is connected with GND, IO voltage must be defined same as AP IO voltage"
#endif


/*============================================================================
* Defines the Host interface.
*===========================================================================*/
//#define NXTV_IF_MPEG2_SERIAL_TSIF // I2C + TSIF Master Mode. 
//#define NXTV_IF_MPEG2_PARALLEL_TSIF // I2C + TSIF Master Mode. Support only 1seg &TDMB Application!
//#define NXTV_IF_QUALCOMM_TSIF // I2C + TSIF Master Mode
#define NXTV_IF_SPI // AP: SPI Master Mode
//#define NXTV_IF_SPI_SLAVE // AP: SPI Slave Mode
//#define NXTV_IF_EBI2 // External Bus Interface Slave Mode

//#if defined(NXTV_ISDBT_ENABLE)  //Do not use
//	#ifdef NXTV_IF_MPEG2_PARALLEL_TSIF
	//	#define NXTV_FEC_SERIAL_ENABLE  
//	#endif
//#endif

/*#################################
# Pre-definintion by NEXELL.
###################################*/
#if defined(NXTV_IF_MPEG2_SERIAL_TSIF) || defined(NXTV_IF_QUALCOMM_TSIF)\
|| defined(NXTV_IF_MPEG2_PARALLEL_TSIF)
	#define NXTV_IF_TSIF /* All TSIF */
#endif

#if defined(NXTV_IF_MPEG2_SERIAL_TSIF) || defined(NXTV_IF_QUALCOMM_TSIF)
	#define NXTV_IF_SERIAL_TSIF /* Serial TSIF */
#endif

/*============================================================================
* Defines the clear mode of interrupts for EBI/SPI interfaces.
* 1. NXTV_MSC_INTR_ISTAUS_ACC_CLR_MODE
*		In case, the ISR of platform implemented as the nested ISR.
*
* 2. NXTV_MSC_INTR_MEM_ACC_CLR_MODE
*		In case, the ISR of platform implemented as NOT the nested ISR.
*===========================================================================*/
#if defined(NXTV_IF_SPI) || defined(NXTV_IF_EBI2)
	#define NXTV_MSC_INTR_ISTAUS_ACC_CLR_MODE
	//#define NXTV_MSC_INTR_MEM_ACC_CLR_MODE
#endif


/*============================================================================
* Defines the delay macro in milliseconds.
*===========================================================================*/
#if defined(__KERNEL__) /* Linux kernel */
	#define NXTV_DELAY_MS(ms)    mdelay(ms) 

#elif defined(WINCE)
	#define NXTV_DELAY_MS(ms)    Sleep(ms) 

#else
	extern void mtv_delay_ms(int ms);
	#define NXTV_DELAY_MS(ms) 	mtv_delay_ms(ms) // TODO
#endif

/*============================================================================
* Defines the debug message macro.
*===========================================================================*/
#if 1
	#define NXTV_DBGMSG0(fmt)					printk(fmt)
	#define NXTV_DBGMSG1(fmt, arg1)				printk(fmt, arg1) 
	#define NXTV_DBGMSG2(fmt, arg1, arg2)		printk(fmt, arg1, arg2) 
	#define NXTV_DBGMSG3(fmt, arg1, arg2, arg3)	printk(fmt, arg1, arg2, arg3) 
#else
	/* To eliminates the debug messages. */
	#define NXTV_DBGMSG0(fmt)					((void)0) 
	#define NXTV_DBGMSG1(fmt, arg1)				((void)0) 
	#define NXTV_DBGMSG2(fmt, arg1, arg2)		((void)0) 
	#define NXTV_DBGMSG3(fmt, arg1, arg2, arg3)	((void)0) 
#endif
/*#### End of Common ###########*/


/*############################################################################
#
# ISDB-T specific configurations
#
############################################################################*/
/*============================================================================
* Defines the NOTCH FILTER setting Enable.
* In order to reject GSM/CDMA blocker, NOTCH FILTER must be defined.
* This feature used for module company in the JAPAN.
*===========================================================================*/
//#if defined(NXTV_ISDBT_ENABLE)  //Do not use
//	#ifdef NXTV_CHIP_PKG_WLCSP
	//	#define NXTV_NOTCH_FILTER_ENABLE  
//	#endif
//#endif

#if defined(NXTV_NOTCH_FILTER_ENABLE)
	#error "NXTV_NOTCH_FILTER_ENABLE must be confirmed by NEXELL"
#endif

/*##############################################################################
#
# T-DMB/DAB specific configurations
#
############################################################################*/
#if defined(NXTV_TDMB_ENABLE) || defined(NXTV_DAB_ENABLE)
	/* Determine if the FIC is not handled by interrupt. */
	#define NXTV_FIC_POLLING_MODE

	/* TII configuration */
	#define NXTV_TII_PERIOD_1_FRAME

	/* Defines the number of AV service. (0 ~ 1) */
	#define NXTV_MAX_NUM_DAB_AV_SVC	0

	/* Defines the number of DATA services. (TSIF: 0 ~ 3, SPI: 0 ~ 4) */
	#define NXTV_MAX_NUM_DAB_DATA_SVC	1

	#if defined(NXTV_IF_SERIAL_TSIF) || defined(NXTV_IF_SPI_SLAVE) || defined(NXTV_IF_TSIF)
		/*========================================================*/
		/* Selects FIC transmission path when SCAN and PLAY state */
		/*========================================================*/
		#define NXTV_FIC__SCAN_I2C__PLAY_NA /* Polling or Intrrupt. Not applicable in PLAY state */
		//#define NXTV_FIC__SCAN_I2C__PLAY_I2C /* Polling or Intrrupt */
		//#define NXTV_FIC__SCAN_I2C__PLAY_TSIF /* Polling or Intrrupt */
		//#define NXTV_FIC__SCAN_TSIF__PLAY_NA /* NXTV_FIC_POLLING_MODE meaningless */
		//#define NXTV_FIC__SCAN_TSIF__PLAY_I2C /* Polling or Intrrupt */
		//#define NXTV_FIC__SCAN_TSIF__PLAY_TSIF /* NXTV_FIC_POLLING_MODE meaningless */
	#endif

	/* Determine if the CIF decoder is compiled with NEXELL driver. */
	#define NXTV_BUILD_CIFDEC_WITH_DRIVER

	/* Select the copying method of CIF decoded data(FIC and MSC) which copy_to_user()
	or memcpy() to fast operation when the CIF decoder was bulid with LINUX Kernel. */
	#if defined(__KERNEL__)
		//#define NXTV_CIF_LINUX_USER_SPACE_COPY_USED
	#endif

	#ifdef NXTV_DAB_ENABLE
		/* Determine whether or not DAB L-BAND is enabled. */
		#define NXTV_DAB_LBAND_ENABLED
	#endif
#endif /* #if defined(NXTV_TDMB_ENABLE) || defined(NXTV_DAB_ENABLE) */

/*############################################################################
#
# FM specific configurations
#
############################################################################*/
#ifdef NXTV_FM_ENABLE
	#define NXTV_FM_CH_MIN_FREQ_KHz		76000
	#define NXTV_FM_CH_MAX_FREQ_KHz		108000
	#define NXTV_FM_CH_STEP_FREQ_KHz		100 // in KHz

	//#define NXTV_FM_RDS_ENABLED

	#if defined(__KERNEL__) /* Linux kernel */
		//#define NXTV_FM_SCAN_LINUX_USER_SPACE_COPY_USED
	#endif
#endif


/*############################################################################
#
# Host Interface specific configurations
#
############################################################################*/
#if defined(NXTV_IF_TSIF) || defined(NXTV_IF_SPI_SLAVE)
	/*=================================================================
	* Defines the TSIF interface for MPEG2 or QUALCOMM TSIF.	 
	*================================================================*/
	//#define NXTV_TSIF_FORMAT_1
	//#define NXTV_TSIF_FORMAT_2
	#define NXTV_TSIF_FORMAT_3
	//#define NXTV_TSIF_FORMAT_4
	//#define NXTV_TSIF_FORMAT_5

	//#define NXTV_TSIF_CLK_SPEED_DIV_2 // Host Clk/2
	#define NXTV_TSIF_CLK_SPEED_DIV_4 // Host Clk/4
	//#define NXTV_TSIF_CLK_SPEED_DIV_6 // Host Clk/6
	//#define NXTV_TSIF_CLK_SPEED_DIV_8 // Host Clk/8

	/*=================================================================
	* Defines the register I/O macros.
	*================================================================*/
	unsigned char mtv_i2c_read(U8 reg);
	void mtv_i2c_read_burst(U8 reg, U8 *buf, int size);
	void mtv_i2c_write(U8 reg, U8 val);
	#define	NXTV_REG_GET(reg)	mtv_i2c_read((U8)(reg))
	#define	NXTV_REG_BURST_GET(reg, buf, size)	mtv_i2c_read_burst((U8)(reg), buf, size)
	#define	NXTV_REG_SET(reg, val)	mtv_i2c_write((U8)(reg), (U8)(val))
	#define	NXTV_REG_MASK_SET(reg, mask, val)\
		do {					\
			U8 tmp;				\
			tmp = (NXTV_REG_GET(reg)|(U8)(mask)) & (U8)((~(mask))|(val));\
			NXTV_REG_SET(reg, tmp);		\
		} while(0)

#elif defined(NXTV_IF_SPI)
	/*=================================================================
	* Defines the register I/O macros.
	*================================================================*/
	unsigned char mtv_spi_read_SEC(unsigned char reg);
	void mtv_spi_read_burst_SEC(unsigned char reg, unsigned char *buf, int size);
	void mtv_spi_write_SEC(unsigned char reg, unsigned char val);

	#define	NXTV_REG_GET(reg)            			(U8)mtv_spi_read_SEC((U8)(reg))
	#define	NXTV_REG_BURST_GET(reg, buf, size) 		mtv_spi_read_burst_SEC((U8)(reg), buf, (size))
	#define	NXTV_REG_SET(reg, val)       			mtv_spi_write_SEC((U8)(reg), (U8)(val))       
	#define	NXTV_REG_MASK_SET(reg, mask, val)\
		do {					\
			U8 tmp;				\
			tmp = (NXTV_REG_GET(reg)|(U8)(mask)) & (U8)((~(mask))|(val));\
			NXTV_REG_SET(reg, tmp);		\
		} while(0)
    
#elif defined(NXTV_IF_EBI2)
	unsigned char mtv_ebi2_read(unsigned char reg);
	void mtv_ebi2_read_burst(unsigned char reg, unsigned char *buf, int size);
	void mtv_ebi2_write(unsigned char reg, unsigned char val);

#define	NXTV_REG_GET(reg)  (U8)mtv_ebi2_read((U8)(reg))
#define	NXTV_REG_BURST_GET(reg, buf, size) mtv_ebi2_read_burst((U8)(reg), buf, size)
#define	NXTV_REG_SET(reg, val) mtv_ebi2_write((U8)(reg), (U8)(val))
#define	NXTV_REG_MASK_SET(reg, mask, val) \
	do { \
		U8 tmp; \
		tmp = (NXTV_REG_GET(reg)|(U8)(mask)) & (U8)((~(mask))|(val)); \
		NXTV_REG_SET(reg, tmp); \
	} while (0)

#else
	#error "Must define the interface definition !"
#endif

/*############################################################################
#
# Pre-definintion by NEXELL.
# Assume that FM only project was not exist.
#
############################################################################*/
#if defined(NXTV_TDMB_ENABLE) || defined(NXTV_DAB_ENABLE)
	#if defined(NXTV_IF_SPI) || defined(NXTV_IF_EBI2)
		#ifdef NXTV_FIC_POLLING_MODE
			#define NXTV_NUM_FIC_SVC 	0 /* FIC polling mode */
		#else
			#define NXTV_FIC_SPI_INTR_ENABLED /* FIC SPI Interrupt mode. */
			#define NXTV_NUM_FIC_SVC 	1 /* For Multi service with MSC */
		#endif

	#elif defined(NXTV_IF_SERIAL_TSIF) || defined(NXTV_IF_SPI_SLAVE)
		#if !defined(NXTV_FIC_POLLING_MODE)\
		&& !defined(NXTV_FIC__SCAN_TSIF__PLAY_NA)\
		&& !defined(NXTV_FIC__SCAN_TSIF__PLAY_TSIF)
			#define NXTV_FIC_I2C_INTR_ENABLED /* FIC Interrupt use. */
		#endif

		#if defined(NXTV_FIC__SCAN_I2C__PLAY_TSIF)\
		|| defined(NXTV_FIC__SCAN_TSIF__PLAY_TSIF)
			#define NXTV_NUM_FIC_SVC 	1 /*  */
		#else
			#define NXTV_NUM_FIC_SVC 	0
		#endif

	#elif defined(NXTV_IF_MPEG2_PARALLEL_TSIF)
		#if 0
		#if !defined(NXTV_FIC_POLLING_MODE)\
		&& !defined(NXTV_FIC__SCAN_TSIF__PLAY_NA)\
		&& !defined(NXTV_FIC__SCAN_TSIF__PLAY_TSIF)
			#define NXTV_FIC_I2C_INTR_ENABLED /* FIC Interrupt use. */
		#else
			#error "Not support"
		#endif
		#endif
		#define NXTV_NUM_FIC_SVC 	0
	#endif
	
	/* Defines the number of Sub Channel to be open simultaneously. (AV + DATA) */
	#define NXTV_NUM_DAB_AVD_SERVICE	(NXTV_MAX_NUM_DAB_AV_SVC+NXTV_MAX_NUM_DAB_DATA_SVC)
	
	#if ((NXTV_NUM_DAB_AVD_SERVICE + NXTV_NUM_FIC_SVC) >= 2)
		#define NXTV_MULTI_SERVICE_MODE
	#endif

	#if defined(NXTV_IF_SPI) || defined(NXTV_IF_EBI2)
		/* Defines CIF mode by number of subchannel. */
		#if (NXTV_MAX_NUM_DAB_DATA_SVC >= 2) /* more 1 DATA service */
			#define NXTV_CIF_MODE_ENABLED
		#endif
	#else
		#if ((NXTV_NUM_DAB_AVD_SERVICE + NXTV_NUM_FIC_SVC) >= 2)
			#define NXTV_CIF_MODE_ENABLED
		#endif
	#endif
#endif /* #if defined(NXTV_TDMB_ENABLE) || defined(NXTV_DAB_ENABLE) */

#if defined(NXTV_FM_ENABLE) && defined(NXTV_FM_RDS_ENABLED)
	#ifndef NXTV_MULTI_SERVICE_MODE
		//#define NXTV_MULTI_SERVICE_MODE !! not yet
	#endif
#endif

/* Define the state of MSC1 memory usage for MTV chip. */
#if defined(NXTV_IF_SPI) || defined(NXTV_IF_EBI2)
	#if defined(NXTV_ISDBT_ENABLE)\
	|| (defined(NXTV_FM_ENABLE) && !defined(NXTV_FM_RDS_ENABLED))\
	|| ((defined(NXTV_TDMB_ENABLE)||defined(NXTV_DAB_ENABLE))\
		&& (((NXTV_MAX_NUM_DAB_AV_SVC == 0) && (NXTV_MAX_NUM_DAB_DATA_SVC == 1))\
			|| (NXTV_MAX_NUM_DAB_AV_SVC >= 1)))
		/* TDMB/DAB: (Av:0 ea, DATA: 1ea) or (AV >=1) */
		#define NXTV_SPI_MSC1_ENABLED
	#endif
#endif /* #if defined(NXTV_IF_SPI) */

/* Define the state of MSC0 memory usage for MTV chip. */
#if defined(NXTV_IF_SPI)
	#if (defined(NXTV_FM_ENABLE) && defined(NXTV_FM_RDS_ENABLED))\
	|| ((defined(NXTV_TDMB_ENABLE)||defined(NXTV_DAB_ENABLE))\
		&& (((NXTV_MAX_NUM_DAB_AV_SVC == 1) && (NXTV_MAX_NUM_DAB_DATA_SVC == 1))\
			|| (NXTV_MAX_NUM_DAB_DATA_SVC >= 2)))
		/* TDMB/DAB: (Av:1 ea, DATA: 1ea) or (AV: 0, DATA >=2) */
		#define NXTV_SPI_MSC0_ENABLED
	#endif
#endif /* #if defined(NXTV_IF_SPI) */


#if (defined(NXTV_TDMB_ENABLE)||defined(NXTV_DAB_ENABLE))\
&& !(defined(NXTV_ISDBT_ENABLE)||defined(NXTV_FM_ENABLE)) 
	/* Only TDMB or DAB enabled. */
	#define NXTV_TDMBorDAB_ONLY_ENABLED

#elif !(defined(NXTV_TDMB_ENABLE)||defined(NXTV_DAB_ENABLE) || defined(NXTV_FM_ENABLE))\
&& defined(NXTV_ISDBT_ENABLE)
	/* Only 1SEG enabled. */
	#define NXTV_ISDBT_ONLY_ENABLED

#elif (defined(NXTV_TDMB_ENABLE)||defined(NXTV_DAB_ENABLE)) && defined(NXTV_FM_ENABLE)\
&& !defined(NXTV_ISDBT_ENABLE)
		/* Only TDMB/DAB and FM  enabled. */
	#define NXTV_TDMBorDAB_FM_ENABLED
#endif


/*############################################################################
# Define the critical object.
############################################################################*/
#if defined(NXTV_IF_SPI) || defined(NXTV_FIC_I2C_INTR_ENABLED)
	#if defined(__KERNEL__)	
		extern struct mutex nxtv_guard_SEC;
		#define NXTV_GUARD_INIT		mutex_init(&nxtv_guard_SEC)
		#define NXTV_GUARD_LOCK		mutex_lock(&nxtv_guard_SEC)
		#define NXTV_GUARD_FREE		mutex_unlock(&nxtv_guard_SEC)
		#define NXTV_GUARD_DEINIT 	((void)0)
		
    #elif defined(WINCE)        
	        extern CRITICAL_SECTION		nxtv_guard_SEC;
	        #define NXTV_GUARD_INIT		InitializeCriticalSection(&nxtv_guard_SEC)
	        #define NXTV_GUARD_LOCK		EnterCriticalSection(&nxtv_guard_SEC)
	        #define NXTV_GUARD_FREE		LeaveCriticalSection(&nxtv_guard_SEC)
	        #define NXTV_GUARD_DEINIT	DeleteCriticalSection(&nxtv_guard_SEC)
	#else
		// temp: TODO
		#define NXTV_GUARD_INIT		((void)0)
		#define NXTV_GUARD_LOCK		((void)0)
		#define NXTV_GUARD_FREE 	((void)0)
		#define NXTV_GUARD_DEINIT 	((void)0)
	#endif
	
#else
	#define NXTV_GUARD_INIT		((void)0)
	#define NXTV_GUARD_LOCK		((void)0)
	#define NXTV_GUARD_FREE 	((void)0)
	#define NXTV_GUARD_DEINIT 	((void)0)
#endif


/*############################################################################
#
# Check erros by user-configurations.
#
############################################################################*/
#if !defined(NXTV_CHIP_PKG_WLCSP) && !defined(NXTV_CHIP_PKG_QFN)\
&& !defined(NXTV_CHIP_PKG_LGA)
	#error "Must define the package type !"
#endif

#if !defined(NXTV_PWR_EXTERNAL) && !defined(NXTV_PWR_LDO)  && !defined(NXTV_PWR_DCDC)
	#error "Must define the power type !"
#endif

#if !defined(NXTV_IO_1_8V) && !defined(NXTV_IO_2_5V)  && !defined(NXTV_IO_3_3V)
	#error "Must define I/O voltage!"
#endif

 
#if defined(NXTV_IF_TSIF) || defined(NXTV_IF_SPI_SLAVE) ||defined(NXTV_IF_SPI)
    #if (NXTV_CHIP_ADDR >= 0xFF)
        #error "Invalid chip address"
    #endif
#elif defined(NXTV_IF_EBI2)
    #if (NXTV_CHIP_ADDR <= 0xFF)
        #error "Invalid chip address"
    #endif
    
#else
	#error "Must define the interface definition !"
#endif


#if defined(NXTV_DAB_ENABLE) && defined(NXTV_TDMB_ENABLE)
	#error "Should select one of NXTV_DAB_ENABLE or NXTV_TDMB_ENABLE"
#endif

#if defined(NXTV_TDMB_ENABLE) || defined(NXTV_DAB_ENABLE)
	#if !defined(NXTV_MAX_NUM_DAB_AV_SVC) && !defined(NXTV_MAX_NUM_DAB_DATA_SVC)
		#error "Should be define both!"
	#endif

	#if (NXTV_NUM_DAB_AVD_SERVICE == 0)
		#error " No Audio/Video/Data service defined!"
	#endif

	#if (NXTV_MAX_NUM_DAB_AV_SVC < 0) || (NXTV_MAX_NUM_DAB_AV_SVC > 1)
		#error "Must 0 or 1 for AV service"
	#endif

	#if defined(NXTV_IF_SPI) || defined(NXTV_IF_EBI2)
		#if (NXTV_MAX_NUM_DAB_DATA_SVC < 0) || (NXTV_MAX_NUM_DAB_DATA_SVC > 4)
			#error "Must from 0 to 4 for DATA service"
		#endif

	#else
		#if (NXTV_MAX_NUM_DAB_DATA_SVC < 0) || (NXTV_MAX_NUM_DAB_DATA_SVC > 3)
			#error "Must from 0 to 3 for DATA service"
		#endif
	#endif

	#if defined(NXTV_IF_SERIAL_TSIF) || defined(NXTV_IF_SPI_SLAVE)
		#if !defined(NXTV_FIC__SCAN_I2C__PLAY_NA)\
		&& !defined(NXTV_FIC__SCAN_I2C__PLAY_I2C)\
		&& !defined(NXTV_FIC__SCAN_I2C__PLAY_TSIF)\
		&& !defined(NXTV_FIC__SCAN_TSIF__PLAY_NA )\
		&& !defined(NXTV_FIC__SCAN_TSIF__PLAY_I2C)\
		&& !defined(NXTV_FIC__SCAN_TSIF__PLAY_TSIF)
			#error "No FIC path was defined for TSIF!"
		#endif

		#if defined(NXTV_FIC__SCAN_I2C__PLAY_NA)\
			&& defined(NXTV_FIC__SCAN_I2C__PLAY_I2C)\
			&& defined(NXTV_FIC__SCAN_I2C__PLAY_TSIF)\
			&& defined(NXTV_FIC__SCAN_TSIF__PLAY_NA )\
			&& defined(NXTV_FIC__SCAN_TSIF__PLAY_I2C)\
			&& defined(NXTV_FIC__SCAN_TSIF__PLAY_TSIF)
			#error "Should select only one FIC path for TSIF!"
		#endif		
	#endif
#endif


#if !defined(NXTV_TDMB_ENABLE) && !defined(NXTV_DAB_ENABLE)
	/* To prevent the compile error. */
	#define NXTV_NUM_DAB_AVD_SERVICE		1
	#define NXTV_MAX_NUM_DAB_DATA_SVC 	1
#endif


#ifdef NXTV_IF_MPEG2_PARALLEL_TSIF
	#if defined(NXTV_FM_ENABLE) || defined(NXTV_DAB_ENABLE)\
	|| defined(NXTV_CHIP_PKG_WLCSP)  || defined(NXTV_CHIP_PKG_LGA)
		#error "Not support parallel TSIF!"
	#endif
	
	#if defined(NXTV_TDMB_ENABLE) && (NXTV_NUM_DAB_AVD_SERVICE > 1)
		#error "Not support T-DMB multi sub channel mode!"
	#endif

	#if defined(NXTV_DAB_ENABLE) && (NXTV_NUM_DAB_AVD_SERVICE > 1)
		#error "Not support DAB multi sub channel mode!"
	#endif
#endif



#if defined(NXTV_IF_SPI) || defined(NXTV_IF_EBI2)
	#if !defined(NXTV_MSC_INTR_MEM_ACC_CLR_MODE)\
	&& !defined(NXTV_MSC_INTR_ISTAUS_ACC_CLR_MODE)
		#error " Should selects an interrupt clear mode"
	#endif

	#if defined(NXTV_MSC_INTR_MEM_ACC_CLR_MODE)\
	&& defined(NXTV_MSC_INTR_ISTAUS_ACC_CLR_MODE)
		#error " Should selects an interrupt clear mode"
	#endif
#endif

void nxtvOEM_ConfigureInterrupt_SEC(void);
void nxtvOEM_PowerOn_SEC(int on);

#ifdef __cplusplus 
} 
#endif 

#endif /* __NXTV_PORT_H__ */

