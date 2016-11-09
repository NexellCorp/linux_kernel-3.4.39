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
* TITLE		: NXB220 porting source file.
*
* FILENAME	: nxb220_port.c
*
* DESCRIPTION	: User-supplied Routines for NEXELL TV Services.
*
******************************************************************************/
/******************************************************************************
* REVISION HISTORY
*
*    DATE         NAME          REMARKS
* ----------  -------------    ------------------------------------------------
* 07/12/2013  Ko, Kevin        Created.
******************************************************************************/

#include "nxb220.h"
#include "nxb220_internal.h"


/* Declares a variable of gurad object if neccessry. */
#if defined(NXTV_IF_SPI) || defined(NXTV_IF_EBI2)
	#if defined(__KERNEL__)
	struct mutex nxtv_guard_fullseg;
	#elif defined(WINCE) || defined(WINDOWS) || defined(WIN32)
        CRITICAL_SECTION nxtv_guard_fullseg;
    #else
	/* non-OS and RTOS. */
	#endif
#endif

#include "isdbt.h"
#include "isdbt_gpio.h"

void nxtvOEM_PowerOn_FULLSEG(int on)
{
printk("%s() fullseg power onoff(%d)\n", __func__, on);
	if (on) {
		/* Set the GPIO of MTV_EN pin to low. */
		gpio_set_value(MTV_PWR_EN, 0);
		NXTV_DELAY_MS(1);

		/* Set the GPIO of MTV_EN pin to high. */
		gpio_set_value(MTV_PWR_EN, 1);
		NXTV_DELAY_MS(1);

		NXTV_GUARD_INIT;
	} else {
		/* Set the GPIO of MTV_EN pin to low. */
		gpio_set_value(MTV_PWR_EN, 0);

		NXTV_GUARD_DEINIT;
	}
}

