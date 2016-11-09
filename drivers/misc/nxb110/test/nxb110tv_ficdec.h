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
* TITLE 	  : NEXELL FIC Decoder API header file. 
*
* FILENAME    : nxb110tv_ficdec.h
*
* DESCRIPTION : 
*		This file contains types and declarations associated with the NEXELL 
*		FIC decoder.
*
********************************************************************************/

/******************************************************************************** 
* REVISION HISTORY
*
*    DATE	  	  NAME				REMARKS
* ----------  -------------    --------------------------------------------------
* 01/24/2011  Ko, Kevin        Creat for CS Realease
********************************************************************************/

#ifndef __NXB110TV_FICDEC_H__
#define __NXB110TV_FICDEC_H__

#ifdef __cplusplus 
extern "C"{ 
#endif  

#include "../src/nxb110tv.h"

#define RTV_MAX_NUM_SUB_CH		64
#define RTV_MAX_ENSEMBLE_LABEL_SIZE	16
#define RTV_MAX_SERVICE_LABEL_SIZE	16

/* Sub Channel Information */
typedef struct
{
	 unsigned char bSubChID; /* 6 bits */
	 unsigned short wBitRate; /* Service Component BitRate(kbit/s)  */
	 unsigned short wStartAddr; /* 10 bits */
	 unsigned char bTMId; /* 2 bits */
	 unsigned char bSvcType; /* 6 bits */
	 unsigned long nSvcID; /* 16/32 bits */
	 unsigned char szSvcLabel[RTV_MAX_SERVICE_LABEL_SIZE+1]; /* 16*8 bits */
} RTV_FIC_SUBCH_INFO;
 
typedef struct 
{
	 unsigned long dwEnsembleFreq; /* 4 bytes */
	 unsigned char bTotalSubCh; /* MAX: 64 */
	 unsigned short wEnsembleID;
	 unsigned char szEnsembleLabel[RTV_MAX_ENSEMBLE_LABEL_SIZE+1];
	 RTV_FIC_SUBCH_INFO tSubChInfo[RTV_MAX_NUM_SUB_CH];
} RTV_FIC_ENSEMBLE_INFO;


typedef enum
{
	RTV_FIC_RET_GOING = 0,
	RTV_FIC_RET_DONE,
	RTV_FIC_RET_CRC_ERR
} E_RTV_FIC_DEC_RET_TYPE;

UINT rtvFICDEC_GetEnsembleInfo(int demod_no, RTV_FIC_ENSEMBLE_INFO *ptEnsemble);
E_RTV_FIC_DEC_RET_TYPE rtvFICDEC_Decode(int demod_no, unsigned char *fic_buf, unsigned int fic_size);
void rtvFICDEC_Init(int demod_no);


 
#ifdef __cplusplus 
} 
#endif 

#endif /* __NXB110TV_FICDEC_H__ */

