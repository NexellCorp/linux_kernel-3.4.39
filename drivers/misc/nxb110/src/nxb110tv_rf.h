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
* TITLE 	  : NEXELL TV RF services header file. 
*
* FILENAME    : nxb110tv_rf.h
*
* DESCRIPTION : 
*		Library of routines to initialize, and operate on, the NEXELL RF chip.
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

#ifndef __NXB110TV_RF_H__
#define __NXB110TV_RF_H__

#ifdef __cplusplus 
extern "C"{ 
#endif  

#include "nxb110tv_internal.h"

INT  rtvRF_SetFrequency(int demod_no, E_RTV_TV_MODE_TYPE eTvMode, UINT nChNum, U32 dwFreqKHz);
INT  rtvRF_ChangeAdcClock(int demod_no, E_RTV_TV_MODE_TYPE eTvMode, E_RTV_ADC_CLK_FREQ_TYPE eAdcClkFreqType);
INT  rtvRF_ConfigureAdcClock(int demod_no, E_RTV_TV_MODE_TYPE eTvMode, E_RTV_ADC_CLK_FREQ_TYPE eAdcClkFreqType);
void rtvRF_ConfigurePowerType(int demod_no, E_RTV_TV_MODE_TYPE eTvMode);
INT  rtvRF_Initilize(int demod_no, E_RTV_TV_MODE_TYPE eTvMode);

#ifdef __cplusplus 
} 
#endif 

#endif /* __NXB110TV_RF_H__ */

