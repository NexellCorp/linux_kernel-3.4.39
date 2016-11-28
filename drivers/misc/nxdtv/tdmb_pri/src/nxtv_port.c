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
* TITLE 	  : NEXELL TV OEM source file. 
*
* FILENAME    : nxtv_port.c
*
* DESCRIPTION : 
*		User-supplied Routines for NEXELL TV Services.
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

#include "nxtv.h"
#include "nxtv_internal.h"


/* Declares a variable of gurad object if neccessry. */
#if defined(NXTV_IF_SPI) || defined(NXTV_FIC_I2C_INTR_ENABLED)
	#if defined(__KERNEL__)	
		struct mutex nxtv_guard;
    #elif defined(WINCE)
        CRITICAL_SECTION nxtv_guard;

    #else
	/* non-OS and RTOS. */
	#endif
#endif


void nxtvOEM_ConfigureInterrupt(void) 
{
#if defined(NXTV_IF_SPI) || defined(NXTV_IF_EBI2)\
|| defined(NXTV_FIC_I2C_INTR_ENABLED)
	NXTV_REG_SET(0x09, 0x00); // [6]INT1 [5]INT0 - 1: Input mode, 0: Output mode
	NXTV_REG_SET(0x0B, 0x00); // [2]INT1 PAD disable [1]INT0 PAD disable

	NXTV_REG_MAP_SEL(HOST_PAGE);
	NXTV_REG_SET(0x28, 0x01); // [5:3]INT1 out sel [2:0] INI0 out sel -  0:Toggle 1:Level,, 2:"0", 3:"1"

	NXTV_REG_SET(0x2A, 0x13); // [5]INT1 pol [4]INT0 pol - 0:Active High, 1:Active Low [3:0] Period = (INT_TIME+1)/8MHz
#endif

	NXTV_REG_MAP_SEL(HOST_PAGE);
#if defined(NXTV_IF_SPI) || defined(NXTV_IF_EBI2)
	#if defined(NXTV_MSC_INTR_ISTAUS_ACC_CLR_MODE)
 	NXTV_REG_SET(0x29, 0x08); // [3] Interrupt status register clear condition - 0:read data by memory access 1:status register access
	#else
	NXTV_REG_SET(0x29, 0x00); // [3] Interrupt status register clear condition - 0:read data by memory access 1:status register access
	#endif
#else
	NXTV_REG_SET(0x29, 0x00); // [3] auto&uclr, 1: user set only
#endif

}

#include "./../mtv.h"
#include "./../mtv_gpio.h"

void nxtvOEM_PowerOn(int on)
{
	int pwr_gpio_pin;
	int spi_cs_gpio_pin;

	pwr_gpio_pin = MTV_PWR_EN;
	spi_cs_gpio_pin = NXTV_CS_SPI1;
	
	if( on )
	{
		/* Set the GPIO of MTV_EN pin to low. */
		gpio_set_value(pwr_gpio_pin, 0);
		NXTV_DELAY_MS(10);
		
		/* Set the GPIO of MTV_EN pin to high. */
		gpio_set_value(pwr_gpio_pin, 1);
		NXTV_DELAY_MS(20);	

		gpio_set_value(spi_cs_gpio_pin, 1);
		NXTV_DELAY_MS(1);	
		
		/* In case of SPI interface or FIC interrupt mode for T-DMB, we should lock the register page. */
		NXTV_GUARD_INIT; 			
	}
	else
	{
		/* Set the GPIO of MTV_EN pin to low. */		
		gpio_set_value(pwr_gpio_pin, 0);

		gpio_set_value(spi_cs_gpio_pin, 0);

		NXTV_GUARD_DEINIT;
	}
}




