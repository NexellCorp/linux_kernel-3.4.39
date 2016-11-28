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
* FILENAME    : nxtv.h
*
* DESCRIPTION : 
*		This file contains types and declarations associated with the NEXELL
*		TV Services.
*
********************************************************************************/

/******************************************************************************** 
* REVISION HISTORY
*
*    DATE	  	  NAME				REMARKS
* ----------  -------------    --------------------------------------------------
* 10/14/2010  Ko, Kevin        Added the NXTV_FM_CH_STEP_FREQ_KHz defintion.
* 10/06/2010  Ko, Kevin        Added NXTV_ISDBT_FREQ2CHNUM macro for ISDB-T.
* 09/27/2010  Ko, Kevin        Creat for CS Realease
*             /Yang, Maverick  1.Reformating for CS API
*                              2.pll table, ADC clock switching, SCAN function, 
*								 FM function added..
********************************************************************************/

#ifndef __NXTV_H__
#define __NXTV_H__

#ifdef __cplusplus 
extern "C"{ 
#endif  

#include "nxtv_port.h"

#define NXTV_CHIP_ID		0x8A

/*==============================================================================
 *
 * Common definitions and types.
 *
 *============================================================================*/
#ifndef NULL
	#define NULL    	0
#endif

#ifndef FALSE
	#define FALSE		0
#endif

#ifndef TRUE
	#define TRUE		1
#endif

#ifndef MAX
	#define MAX(a, b)    (((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
	#define MIN(a, b)    (((a) < (b)) ? (a) : (b))
#endif

#ifndef ABS
	#define ABS(x) 		 (((x) < 0) ? -(x) : (x))
#endif


#define	NXTV_TS_PACKET_SIZE		188


/* Error codes. */
#define NXTV_SUCCESS						0
#define NXTV_INVAILD_COUNTRY_BAND		-1
#define NXTV_UNSUPPORT_ADC_CLK			-2
#define NXTV_INVAILD_TV_MODE				-3
#define NXTV_CHANNEL_NOT_DETECTED		-4
#define NXTV_INSUFFICIENT_CHANNEL_BUF	-5
#define NXTV_INVAILD_FREQ				-6
#define NXTV_INVAILD_SUB_CHANNEL_ID		-7 // for T-DMB and DAB
#define NXTV_NO_MORE_SUB_CHANNEL			-8 // for T-DMB and DAB
#define NXTV_ALREADY_OPENED_SUB_CHANNEL_ID	-9 // for T-DMB and DAB
#define NXTV_NOT_OPENED_SUB_CHANNEL_ID	-10 // for T-DMB and DAB
#define NXTV_INVAILD_THRESHOLD_SIZE		-11 
#define NXTV_POWER_ON_CHECK_ERROR		-12 
#define NXTV_PLL_UNLOCKED				-13 
#define NXTV_ADC_CLK_UNLOCKED			-14 
#define NXTV_DUPLICATING_OPEN_FIC		-15 // for T-DMB and DAB
#define NXTV_NOT_OPENED_FIC				-16 // for T-DMB and DAB
#define NXTV_INVALID_FIC_READ_SIZE		-17 // for T-DMB and DAB

typedef enum
{
	NXTV_COUNTRY_BAND_JAPAN = 0,
	NXTV_COUNTRY_BAND_KOREA,		
	NXTV_COUNTRY_BAND_BRAZIL,
	NXTV_COUNTRY_BAND_ARGENTINA 
} E_NXTV_COUNTRY_BAND_TYPE;


// Do not modify the order!
typedef enum
{
	NXTV_ADC_CLK_FREQ_8_MHz = 0,
	NXTV_ADC_CLK_FREQ_8_192_MHz,
	NXTV_ADC_CLK_FREQ_9_MHz,
	NXTV_ADC_CLK_FREQ_9_6_MHz,
	MAX_NUM_NXTV_ADC_CLK_FREQ_TYPE
} E_NXTV_ADC_CLK_FREQ_TYPE;


// Modulation
typedef enum
{
	NXTV_MOD_DQPSK = 0,
	NXTV_MOD_QPSK,
	NXTV_MOD_16QAM,
	NXTV_MOD_64QAM
} E_NXTV_MODULATION_TYPE;

typedef enum
{
	NXTV_CODE_RATE_1_2 = 0,
	NXTV_CODE_RATE_2_3,
	NXTV_CODE_RATE_3_4,
	NXTV_CODE_RATE_5_6,
	NXTV_CODE_RATE_7_8
} E_NXTV_CODE_RATE_TYPE;

// Do not modify the value!
typedef enum
{
	NXTV_SERVICE_VIDEO = 0x01,
	NXTV_SERVICE_AUDIO = 0x02,
	NXTV_SERVICE_DATA = 0x04
} E_NXTV_SERVICE_TYPE;


/*==============================================================================
 *
 * ISDB-T definitions, types and APIs.
 *
 *============================================================================*/
static INLINE UINT NXTV_ISDBT_FREQ2CHNUM(E_NXTV_COUNTRY_BAND_TYPE eRtvCountryBandType, U32 dwFreqKHz)
{
	switch( eRtvCountryBandType )
	{
		case NXTV_COUNTRY_BAND_JAPAN:
			return ((dwFreqKHz - 395143) / 6000);
			
		case NXTV_COUNTRY_BAND_BRAZIL:
		case NXTV_COUNTRY_BAND_ARGENTINA: 
			return (((dwFreqKHz - 395143) / 6000) + 1);
			
		default:
			return 0xFFFF;
	}
}


#define NXTV_ISDBT_OFDM_LOCK_MASK	0x1
#define NXTV_ISDBT_TMCC_LOCK_MASK	0x2
#define NXTV_ISDBT_CHANNEL_LOCK_OK	(NXTV_ISDBT_OFDM_LOCK_MASK|NXTV_ISDBT_TMCC_LOCK_MASK)

#define NXTV_ISDBT_BER_DIVIDER		100000
#define NXTV_ISDBT_CNR_DIVIDER		10000
#define NXTV_ISDBT_RSSI_DIVIDER		10


typedef enum
{
	NXTV_ISDBT_SEG_1 = 0,
	NXTV_ISDBT_SEG_3
} E_NXTV_ISDBT_SEG_TYPE;

typedef enum
{
	NXTV_ISDBT_MODE_1 = 0, // 2048
	NXTV_ISDBT_MODE_2,	  // 4096
	NXTV_ISDBT_MODE_3      // 8192 fft
} E_NXTV_ISDBT_MODE_TYPE;

typedef enum
{
	NXTV_ISDBT_GUARD_1_32 = 0, /* 1/32 */
	NXTV_ISDBT_GUARD_1_16,     /* 1/16 */
	NXTV_ISDBT_GUARD_1_8,      /* 1/8 */
	NXTV_ISDBT_GUARD_1_4       /* 1/4 */
} E_NXTV_ISDBT_GUARD_TYPE;


typedef enum
{
	NXTV_ISDBT_INTERLV_0 = 0,
	NXTV_ISDBT_INTERLV_1,
	NXTV_ISDBT_INTERLV_2,
	NXTV_ISDBT_INTERLV_4,
	NXTV_ISDBT_INTERLV_8,
	NXTV_ISDBT_INTERLV_16,
	NXTV_ISDBT_INTERLV_32
} E_NXTV_ISDBT_INTERLV_TYPE;


// for Layer A.
typedef struct
{
	E_NXTV_ISDBT_SEG_TYPE		eSeg;
	E_NXTV_ISDBT_MODE_TYPE		eTvMode;
	E_NXTV_ISDBT_GUARD_TYPE		eGuard;
	E_NXTV_MODULATION_TYPE		eModulation;
	E_NXTV_CODE_RATE_TYPE		eCodeRate;
	E_NXTV_ISDBT_INTERLV_TYPE	eInterlv;
	int						fEWS;	
} NXTV_ISDBT_TMCC_INFO;

void nxtvISDBT_StandbyMode(int on);
UINT nxtvISDBT_GetLockStatus(void); 
U8   nxtvISDBT_GetAGC(void);
S32  nxtvISDBT_GetRSSI(void);
U32  nxtvISDBT_GetPER(void);
U32  nxtvISDBT_GetCNR(void);
U32  nxtvISDBT_GetBER(void);
UINT nxtvISDBT_GetAntennaLevel(U32 dwCNR);
void nxtvISDBT_GetTMCC(NXTV_ISDBT_TMCC_INFO *ptTmccInfo);
void nxtvISDBT_DisableStreamOut(void);
void nxtvISDBT_EnableStreamOut(void);
INT  nxtvISDBT_SetFrequency(UINT nChNum);
INT  nxtvISDBT_ScanFrequency(UINT nChNum);
void nxtvISDBT_SwReset(void);
INT  nxtvISDBT_Initialize(E_NXTV_COUNTRY_BAND_TYPE eRtvCountryBandType, UINT nThresholdSize);


/*==============================================================================
 *
 * FM definitions, types and APIs.
 *
 *============================================================================*/
#define NXTV_FM_PILOT_LOCK_MASK	0x1
#define NXTV_FM_RDS_LOCK_MASK		0x2
#define NXTV_FM_CHANNEL_LOCK_OK      (NXTV_FM_PILOT_LOCK_MASK|NXTV_FM_RDS_LOCK_MASK)

#define NXTV_FM_RSSI_DIVIDER		10

typedef enum
{
	NXTV_FM_OUTPUT_MODE_AUTO = 0,
	NXTV_FM_OUTPUT_MODE_MONO = 1,
	NXTV_FM_OUTPUT_MODE_STEREO = 2
} E_NXTV_FM_OUTPUT_MODE_TYPE;

void nxtvFM_StandbyMode(int on);
void nxtvFM_GetLockStatus(UINT *pLockVal, UINT *pLockCnt);
S32 nxtvFM_GetRSSI(void);
void nxtvFM_SetOutputMode(E_NXTV_FM_OUTPUT_MODE_TYPE eOutputMode);
void nxtvFM_DisableStreamOut(void);
void nxtvFM_EnableStreamOut(void);
INT  nxtvFM_SetFrequency(U32 dwChFreqKHz);
INT  nxtvFM_SearchFrequency(U32 *pDetectedFreqKHz, U32 dwStartFreqKHz, U32 dwEndFreqKHz);
INT  nxtvFM_ScanFrequency(U32 *pChBuf, UINT nNumChBuf, U32 dwStartFreqKHz, U32 dwEndFreqKHz);
INT  nxtvFM_Initialize(E_NXTV_ADC_CLK_FREQ_TYPE eAdcClkFreqType, UINT nThresholdSize); 


/*==============================================================================
 *
 * TDMB definitions, types and APIs.
 *
 *============================================================================*/
#define NXTV_TDMB_OFDM_LOCK_MASK		0x1
#define NXTV_TDMB_FEC_LOCK_MASK		0x2
#define NXTV_TDMB_CHANNEL_LOCK_OK    (NXTV_TDMB_OFDM_LOCK_MASK|NXTV_TDMB_FEC_LOCK_MASK)

#define NXTV_TDMB_BER_DIVIDER		100000
#define NXTV_TDMB_CNR_DIVIDER		1000
#define NXTV_TDMB_RSSI_DIVIDER		1000

// TII Main id = pattern(7bit), TII sub ID = combo(5bit) 
typedef struct
{
	int tii_combo;
	int tii_pattern;
} NXTV_TDMB_TII_INFO;

void nxtvTDMB_DisableTiiInterrupt_SEC(void);
void nxtvTDMB_EnableTiiInterrupt_SEC(void);
void nxtvTDMB_GetTii_SEC(NXTV_TDMB_TII_INFO *ptTiiInfo);

void nxtvTDMB_StandbyMode_SEC(int on);
UINT nxtvTDMB_GetLockStatus_SEC(void);
U32  nxtvTDMB_GetPER_SEC(void);
S32  nxtvTDMB_GetRSSI_SEC(void);
U32  nxtvTDMB_GetCNR_SEC(void);
U32  nxtvTDMB_GetCER_SEC(void);
U32  nxtvTDMB_GetBER_SEC(void);
UINT nxtvTDMB_GetAntennaLevel_SEC(U32 dwCER);
U32  nxtvTDMB_GetPreviousFrequency_SEC(void);
INT  nxtvTDMB_OpenSubChannel_SEC(U32 dwChFreqKHz, UINT nSubChID, E_NXTV_SERVICE_TYPE eServiceType, UINT nThresholdSize);
INT  nxtvTDMB_CloseSubChannel_SEC(UINT nSubChID);
void nxtvTDMB_CloseAllSubChannels_SEC(void);
INT  nxtvTDMB_ScanFrequency_SEC(U32 dwChFreqKHz);
INT	 nxtvTDMB_ReadFIC_SEC(U8 *pbBuf);
void nxtvTDMB_CloseFIC_SEC(void);
INT  nxtvTDMB_OpenFIC_SEC(void);
INT  nxtvTDMB_Initialize_SEC(E_NXTV_COUNTRY_BAND_TYPE eRtvCountryBandType); 


/*==============================================================================
 *
 * DAB definitions, types and APIs.
 *
 *============================================================================*/
#define NXTV_DAB_OFDM_LOCK_MASK		0x1
#define NXTV_DAB_FEC_LOCK_MASK		0x2
#define NXTV_DAB_CHANNEL_LOCK_OK    (NXTV_TDMB_OFDM_LOCK_MASK|NXTV_TDMB_FEC_LOCK_MASK)

#define NXTV_DAB_BER_DIVIDER		100000
#define NXTV_DAB_CNR_DIVIDER		1000
#define NXTV_DAB_RSSI_DIVIDER	1000

// TII Main id = pattern(7bit), TII sub ID = combo(5bit) 
typedef struct
{
	int tii_combo;
	int tii_pattern;
} NXTV_DAB_TII_INFO;


typedef enum
{
	NXTV_DAB_TRANSMISSION_MODE1 = 0,
	NXTV_DAB_TRANSMISSION_MODE2,
	NXTV_DAB_TRANSMISSION_MODE3,	
	NXTV_DAB_TRANSMISSION_MODE4,
	NXTV_DAB_INVALID_TRANSMISSION_MODE
} E_NXTV_DAB_TRANSMISSION_MODE;

void nxtvDAB_StandbyMode(int on);
UINT nxtvDAB_GetLockStatus(void);
U32  nxtvDAB_GetPER(void);
S32  nxtvDAB_GetRSSI(void);
U32  nxtvDAB_GetCNR(void);
U32  nxtvDAB_GetCER(void);
U32  nxtvDAB_GetBER(void);
UINT nxtvDAB_GetAntennaLevel(U32 dwCER);
U32  nxtvDAB_GetPreviousFrequency(void);
INT  nxtvDAB_OpenSubChannel(U32 dwChFreqKHz, UINT nSubChID,
	E_NXTV_SERVICE_TYPE eServiceType, UINT nThresholdSize);
INT  nxtvDAB_CloseSubChannel(UINT nSubChID);
void nxtvDAB_CloseAllSubChannels(void);
INT  nxtvDAB_ScanFrequency(U32 dwChFreqKHz);
void nxtvDAB_DisableReconfigInterrupt(void);
void nxtvDAB_EnableReconfigInterrupt(void);
INT  nxtvDAB_ReadFIC(U8 *pbBuf, UINT nFicSize);
UINT nxtvDAB_GetFicSize(void);
void nxtvDAB_CloseFIC(void);
INT  nxtvDAB_OpenFIC(void);
INT  nxtvDAB_Initialize(void); 

 
#ifdef __cplusplus 
} 
#endif 

#endif /* __NXTV_H__ */

