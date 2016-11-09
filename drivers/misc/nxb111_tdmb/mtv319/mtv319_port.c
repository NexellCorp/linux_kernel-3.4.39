/******************************************************************************
* (c) COPYRIGHT 2012 RAONTECH, Inc. ALL RIGHTS RESERVED.
*
* This software is the property of RAONTECH and is furnished under license
* by RAONTECH.
* This software may be used only in accordance with the terms of said license.
* This copyright noitce may not be remoced, modified or obliterated without
* the prior written permission of RAONTECH, Inc.
*
* This software may not be copied, transmitted, provided to or otherwise
* made available to any other person, company, corporation or other entity
* except as specified in the terms of said license.
*
* No right, title, ownership or other interest in the software is hereby
* granted or transferred.
*
* The information contained herein is subject to change without notice
* and should not be construed as a commitment by RAONTECH, Inc.
*
* TITLE		: MTV319 porting source file.
*
* FILENAME	: mtv319_port.c
*
* DESCRIPTION	: User-supplied Routines for RAONTECH TV Services.
*
******************************************************************************/
/******************************************************************************
* REVISION HISTORY
*
*    DATE         NAME          REMARKS
* ----------  -------------    ------------------------------------------------
* 07/12/2012  Ko, Kevin        Created.
******************************************************************************/

#include "mtv319.h"
#include "mtv319_internal.h"
#include "mtv319_cifdec.h"

#include "tdmb.h"
#include "tdmb_gpio.h"


/* Declares a variable of gurad object if neccessry. */
#if defined(RTV_IF_SPI) || defined(RTV_FIC_I2C_INTR_ENABLED)
#if defined(__KERNEL__)
	struct mutex raontv_guard;
#elif defined(WINCE) || defined(WINDOWS) || defined(WIN32)
	CRITICAL_SECTION raontv_guard;
#else
	/* non-OS and RTOS. */
#endif
#endif

int mtv319_assemble_fic(unsigned char *fic_buf, const unsigned char *ts_data,
			unsigned int ts_size)
{
	struct RTV_CIF_DEC_INFO cifdec;

	cifdec.fic_buf_ptr = fic_buf;

	rtvCIFDEC_Decode(&cifdec, ts_data, ts_size);

	return (int)cifdec.fic_size;
}

void rtvOEM_PowerOn(int on)
{
	int pwr_gpio_pin;
	int spi_cs_gpio_pin;

	pwr_gpio_pin = MTV_PWR_EN;
	spi_cs_gpio_pin = RAONTV_CS_SPI1;

	if (on) {
		/* Set the GPIO of MTV_EN pin to low. */
		gpio_set_value(pwr_gpio_pin, 0);
		RTV_DELAY_MS(10);
		
		/* Set the GPIO of MTV_EN pin to high. */
		gpio_set_value(pwr_gpio_pin, 1);
		RTV_DELAY_MS(20);	

		gpio_set_value(spi_cs_gpio_pin, 1);
		RTV_DELAY_MS(1);

		/* In case of SPI interface or FIC interrupt mode for T-DMB,
		we should lock the register page. */
		RTV_GUARD_INIT;
	} else {
		/* Set the GPIO of MTV_EN pin to low. */
		gpio_set_value(pwr_gpio_pin, 0);

		gpio_set_value(spi_cs_gpio_pin, 0);

		RTV_GUARD_DEINIT;
	}
}




