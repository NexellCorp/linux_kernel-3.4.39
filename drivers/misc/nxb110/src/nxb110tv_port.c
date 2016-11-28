/******************************************************************************** 
* (c) COPYRIGHT 2015 NEXELL, Inc. ALL RIGHTS RESERVED.
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
* TITLE 	  : TV OEM source file. 
*
* FILENAME    : nxb110tv_port.c
*
* DESCRIPTION : 
*		User-supplied Routines for TV Services.
*
********************************************************************************/

/******************************************************************************** 
* REVISION HISTORY
*
*    DATE	  	  NAME				REMARKS
* ----------  -------------    --------------------------------------------------
* 09/27/2010  Ko, Kevin        Creat for CS Realease
*             /Yang, Maverick  1.Reformating for CS API
*                              2.pll table, ADC clock switching, SCAN function, 
*								 FM function added..
* 04/09/2010  Yang, Maverick   REV1 SETTING 
* 01/25/2010  Yang, Maverick   Created.                                                   
********************************************************************************/

#include "nxb110tv.h"
#include "nxb110tv_internal.h"


/* Declares a variable of gurad object if neccessry. */
#if defined(RTV_IF_SPI) || defined(RTV_FIC_I2C_INTR_ENABLED)
	#if defined(__KERNEL__)	
		struct mutex nxb110tv_guard[NXB110TV_MAX_NUM_DEMOD_CHIP];
    #elif defined(WINCE)
        CRITICAL_SECTION nxb110tv_guard[NXB110TV_MAX_NUM_DEMOD_CHIP];

    #else
	/* non-OS and RTOS. */
	#endif
#endif


void rtvOEM_ConfigureInterrupt(int demod_no) 
{
#if defined(RTV_IF_SPI) || defined(RTV_IF_EBI2)\
|| defined(RTV_FIC_I2C_INTR_ENABLED)
	RTV_REG_SET(demod_no, 0x09, 0x00); // [6]INT1 [5]INT0 - 1: Input mode, 0: Output mode
	RTV_REG_SET(demod_no, 0x0B, 0x00); // [2]INT1 PAD disable [1]INT0 PAD disable

	RTV_REG_MAP_SEL(demod_no, HOST_PAGE);
	RTV_REG_SET(demod_no, 0x28, 0x01); // [5:3]INT1 out sel [2:0] INI0 out sel -  0:Toggle 1:Level,, 2:"0", 3:"1"

	RTV_REG_SET(demod_no, 0x2A, 0x13); // [5]INT1 pol [4]INT0 pol - 0:Active High, 1:Active Low [3:0] Period = (INT_TIME+1)/8MHz
#endif

	RTV_REG_MAP_SEL(demod_no, HOST_PAGE);
#if defined(RTV_IF_SPI) || defined(RTV_IF_EBI2)
	#if defined(RTV_MSC_INTR_ISTAUS_ACC_CLR_MODE)
 	RTV_REG_SET(demod_no, 0x29, 0x08); // [3] Interrupt status register clear condition - 0:read data by memory access 1:status register access
	#else
	RTV_REG_SET(demod_no, 0x29, 0x00); // [3] Interrupt status register clear condition - 0:read data by memory access 1:status register access
	#endif
#else
	RTV_REG_SET(demod_no, 0x29, 0x00); // [3] auto&uclr, 1: user set only
#endif

}

#include "./../mtv.h"
#include "./../mtv_gpio.h"

void rtvOEM_PowerOn(int demod_no, int on)
{
	int pwr_gpio_pin;
	int spi_cs_gpio_pin;

//#if (NXB110TV_MAX_NUM_DEMOD_CHIP == 2)
//	if (demod_no == 0) {
		pwr_gpio_pin = MTV_PWR_EN;
		spi_cs_gpio_pin = NXB110TV_CS_SPI;
//		}
//	else {
//		pwr_gpio_pin = MTV_PWR_EN_1;
//		spi_cs_gpio_pin = NXB110TV_CS_SPI1;
//	}
//#else
//	pwr_gpio_pin = MTV_PWR_EN;
//#endif
	
	if( on )
	{
		/* Set the GPIO of MTV_EN pin to low. */
		gpio_set_value(pwr_gpio_pin, 0);
		RTV_DELAY_MS(10);
		
		/* Set the GPIO of MTV_EN pin to high. */
		gpio_set_value(pwr_gpio_pin, 1);
		RTV_DELAY_MS(20);	



		gpio_set_value(spi_cs_gpio_pin, 1);
		RTV_DELAY_MS(1);	
		
		/* In case of SPI interface or FIC interrupt mode for T-DMB, we should lock the register page. */
		RTV_GUARD_INIT(demod_no); 			
	}
	else
	{
		/* Set the GPIO of MTV_EN pin to low. */		
		gpio_set_value(pwr_gpio_pin, 0);

		gpio_set_value(spi_cs_gpio_pin, 0);

		RTV_GUARD_DEINIT(demod_no);
	}
}



void mtv_delay_ms(int ms)
{
 mdelay(ms);
	//#define RTV_DELAY_MS(ms) 	mtv_delay_ms(ms) // TODO
}
