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
* FILENAME    : nxtv_rf.h
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

#ifndef __NXTV_RF_H__
#define __NXTV_RF_H__

#ifdef __cplusplus 
extern "C"{ 
#endif  

#include "nxtv_internal.h"

INT  nxtvRF_SetFrequency_SEC(E_NXTV_TV_MODE_TYPE eTvMode, UINT nChNum, U32 dwFreqKHz);
INT  nxtvRF_ChangeAdcClock_SEC(E_NXTV_TV_MODE_TYPE eTvMode, E_NXTV_ADC_CLK_FREQ_TYPE eAdcClkFreqType);
INT  nxtvRF_ConfigureAdcClock_SEC(E_NXTV_TV_MODE_TYPE eTvMode, E_NXTV_ADC_CLK_FREQ_TYPE eAdcClkFreqType);
void nxtvRF_ConfigurePowerType_SEC(E_NXTV_TV_MODE_TYPE eTvMode);
INT  nxtvRF_Initilize_SEC(E_NXTV_TV_MODE_TYPE eTvMode);

#ifdef __cplusplus 
} 
#endif 

#endif /* __NXTV_RF_H__ */

