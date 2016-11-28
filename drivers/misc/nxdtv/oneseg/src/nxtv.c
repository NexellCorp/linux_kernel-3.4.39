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
* TITLE 	  : NEXELL TV device driver API header file. 
*
* FILENAME    : nxtv.c
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
* 09/27/2010  Ko, Kevin        Creat for CS Realease
*             /Yang, Maverick  1.Reformating for CS API
*                              2.pll table, ADC clock switching, SCAN function, 
*								 FM function added..
********************************************************************************/

#include "nxtv_rf.h"

volatile BOOL g_fRtvChannelChange_1SEG;


volatile E_NXTV_ADC_CLK_FREQ_TYPE g_eRtvAdcClkFreqType_1SEG;
BOOL g_fRtvStreamEnabled_1SEG;

#if defined(NXTV_TDMB_ENABLE) || defined(NXTV_ISDBT_ENABLE)
	E_NXTV_COUNTRY_BAND_TYPE g_eRtvCountryBandType_1SEG;
#endif

#if defined(NXTV_TDMB_ENABLE) || defined(NXTV_DAB_ENABLE)
	BOOL g_fRtvFicOpened;
	#if defined(NXTV_IF_SPI) && defined(NXTV_CIF_MODE_ENABLED)
	U32 g_aOpenedCifSubChBits_MSC0[2]; /* Used sub channel ID bits. [0]: 0 ~ 31, [1]: 32 ~ 63 */
	#endif

	#if defined(NXTV_IF_SERIAL_TSIF) || defined(NXTV_IF_SPI_SLAVE)
	E_NXTV_FIC_OPENED_PATH_TYPE g_nRtvFicOpenedStatePath;
	#endif
#endif


#ifdef NXTV_DAB_ENABLE
	volatile E_NXTV_TV_MODE_TYPE g_curDabSetType;
#endif

UINT g_nRtvMscThresholdSize_1SEG; 
U8 g_bRtvIntrMaskRegL_1SEG;
#ifdef NXTV_DAB_ENABLE
	U8 g_bRtvIntrMaskRegH;
#endif

void nxtv_ConfigureHostIF_1SEG(void)
{
#if defined(NXTV_IF_TSIF) || defined(NXTV_IF_SPI_SLAVE)
	NXTV_REG_MAP_SEL(HOST_PAGE);
    NXTV_REG_SET(0x77, 0x15);   // TSIF Enable
    NXTV_REG_SET(0x22, 0x48);   

  #if defined(NXTV_IF_MPEG2_PARALLEL_TSIF)
  	#ifdef NXTV_FEC_SERIAL_ENABLE  /*Serial Out*/
        NXTV_REG_SET(0x04, 0x29);   // I2C + TSIF Mode Enable
	#else
		NXTV_REG_SET(0x04, 0x01);   // I2C + TSIF Mode Enable
	#endif
  #else
	NXTV_REG_SET(0x04, 0x29);   // I2C + TSIF Mode Enable
  #endif
  
	NXTV_REG_SET(0x0C, 0xF4);   // TSIF Enable

#elif defined(NXTV_IF_SPI) || defined(NXTV_IF_EBI2)
	NXTV_REG_MAP_SEL(HOST_PAGE);
	NXTV_REG_SET(0x77, 0x14);   //SPI Mode Enable
    NXTV_REG_SET(0x04, 0x28);   // SPI Mode Enable
	NXTV_REG_SET(0x0C, 0xF5);
 
#else
	#error "Code not present"
#endif
}

INT nxtv_InitSystem_1SEG(E_NXTV_TV_MODE_TYPE eTvMode, E_NXTV_ADC_CLK_FREQ_TYPE eAdcClkFreqType)
{
	INT nRet;
	int i;
	
	g_fRtvChannelChange_1SEG = FALSE;
	g_fRtvStreamEnabled_1SEG = FALSE;

	g_bRtvIntrMaskRegL_1SEG = 0xFF;
#ifdef NXTV_DAB_ENABLE
	g_bRtvIntrMaskRegH = 0xFF;
#endif

#if defined(NXTV_TDMB_ENABLE) || defined(NXTV_DAB_ENABLE)
	g_fRtvFicOpened = FALSE;
	#if defined(NXTV_IF_SPI) && defined(NXTV_CIF_MODE_ENABLED)
	g_aOpenedCifSubChBits_MSC0[0] = 0x0;
	g_aOpenedCifSubChBits_MSC0[1] = 0x0;
	#endif
#endif

	for(i=1; i<=100; i++)
	{
		NXTV_REG_MAP_SEL(HOST_PAGE);
		NXTV_REG_SET(0x7D, 0x06);
		if(NXTV_REG_GET(0x7D) == 0x06)
		{
			goto NXTV_POWER_ON_SUCCESS;
		}

		NXTV_DBGMSG1("[nxtv_InitSystem_1SEG] Power On wait: %d\n", i);

		NXTV_DELAY_MS(5);
	}

	NXTV_DBGMSG1("nxtv_InitSystem_1SEG: Power On Check error: %d\n", i);
	return NXTV_POWER_ON_CHECK_ERROR;

NXTV_POWER_ON_SUCCESS:
	
	nxtvRF_ConfigurePowerType_1SEG(eTvMode);

	if((nRet=nxtvRF_ConfigureAdcClock_1SEG(eTvMode, eAdcClkFreqType)) != NXTV_SUCCESS)
		return nRet;
		
	return NXTV_SUCCESS;
}

