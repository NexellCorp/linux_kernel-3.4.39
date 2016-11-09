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
* TITLE		: NXB220 device driver API header file.
*
* FILENAME	: nxb220.h
*
* DESCRIPTION	:
*		This file contains types and declarations associated
*	        with the NEXELL TV Services.
*
******************************************************************************/
/******************************************************************************
* REVISION HISTORY
*
*    DATE         NAME          REMARKS
* ----------  -------------    ------------------------------------------------
* 03/03/2013  Yang, Maverick       Created.
******************************************************************************/

#ifndef __NXB220_H__
#define __NXB220_H__

#include "nxb220_port.h"

#ifdef __cplusplus
extern "C"{
#endif

/*=============================================================================
*
* Common definitions and types.
*
*===========================================================================*/
#define NXB220_SPI_CMD_SIZE		3

#ifndef NULL
	#define NULL		0
#endif

#ifndef FALSE
	#define FALSE		0
#endif

#ifndef TRUE
	#define TRUE		1
#endif

#ifndef MAX
	#define MAX(a, b)	(((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
	#define MIN(a, b)	(((a) < (b)) ? (a) : (b))
#endif

#ifndef ABS
	#define ABS(x)		(((x) < 0) ? -(x) : (x))
#endif


/* Error codes. */
#define NXTV_SUCCESS						 0
#define NXTV_POWER_ON_CHECK_ERROR		-1
#define NXTV_ADC_CLK_UNLOCKED			-2
#define NXTV_PLL_UNLOCKED			    -3
#define NXTV_CHANNEL_NOT_DETECTED		-4
#define NXTV_INVAILD_FREQUENCY_RANGE		-5
#define NXTV_INVAILD_RF_BAND             -6
#define NXTV_ERROR_LNATUNE               -7
#define NXTV_INVAILD_THRESHOLD_SIZE		-8
#define NXTV_INVAILD_SERVICE_TYPE		-9
#define NXTV_INVALID_DIVER_TYPE          -10

/* Do not modify the order and value! */
enum E_NXTV_SERVICE_TYPE {
	NXTV_SERVICE_INVALID = -1,
	NXTV_SERVICE_UHF_ISDBT_1seg = 0, /* ISDB-T 1seg */
	NXTV_SERVICE_UHF_ISDBT_13seg = 1, /* ISDB-T fullseg */
	NXTV_SERVICE_VHF_ISDBTmm_1seg = 2, /* ISDB-Tmm 1seg */
	NXTV_SERVICE_VHF_ISDBTmm_13seg = 3, /* ISDB-Tmm 13seg */
	NXTV_SERVICE_VHF_ISDBTsb_1seg = 4, /* ISDB-Tsb 1seg */
	NXTV_SERVICE_VHF_ISDBTsb_3seg = 5, /* ISDB-Tsb 3seg */
#if defined(NXTV_DVBT_ENABLE)
	NXTV_SERVICE_DVBT = 6, /* DVB-T */
#endif
	MAX_NUM_NXTV_SERVICE
};

enum E_NXTV_BANDWIDTH_TYPE {
	NXTV_BW_MODE_5MHZ = 0, /* DVB_T */
	NXTV_BW_MODE_6MHZ, /* DVB_T, FULLSEG, ISDB-Tmm */
	NXTV_BW_MODE_7MHZ, /* DVB_T, FULLSEG */
	NXTV_BW_MODE_8MHZ, /* DVB_T, FULLSEG */
	NXTV_BW_MODE_430KHZ, /* 1SEG at 6MHz BW */
	NXTV_BW_MODE_500KHZ, /* 1SEG at 7MHz BW */
	NXTV_BW_MODE_571KHZ, /* 1SEG at 8MHz BW */
	NXTV_BW_MODE_857KHZ, /* DAB */
	NXTV_BW_MODE_1290KHZ, /* 3SEG */
	MAX_NUM_NXTV_BW_MODE_TYPE
};

/*=============================================================================
*
* ISDB-T definitions, types and APIs.
*
*===========================================================================*/
#define NXTV_ISDBT_OFDM_LOCK_MASK	0x1
#define NXTV_ISDBT_TMCC_LOCK_MASK		0x2
#define NXTV_ISDBT_CHANNEL_LOCK_OK	\
	(NXTV_ISDBT_OFDM_LOCK_MASK|NXTV_ISDBT_TMCC_LOCK_MASK)

struct NXTV_LAYER_SIGNAL_INFO {
	U32 cnr;
	U32 ber;
	U32 per;
};

struct NXTV_Statistics {
	UINT lock;
	S32 rssi;
	U32 cnr;
	UINT antenna_level;
	UINT antenna_level_1seg;
	struct NXTV_LAYER_SIGNAL_INFO layerA; /*LP 1SEG, DVB-T, ISDBT Layer A*/
	struct NXTV_LAYER_SIGNAL_INFO layerB; /*ISDBT Layer B*/
	struct NXTV_LAYER_SIGNAL_INFO layerC; /*ISDBT Layer C*/
};

#define NXTV_ISDBT_BER_DIVIDER		100000
#define NXTV_ISDBT_CNR_DIVIDER		1000
#define NXTV_ISDBT_RSSI_DIVIDER		10

#if defined(NXTV_IF_SPI) || defined(NXTV_IF_EBI2)
extern UINT g_nRtvThresholdSize_FULLSEG;
static INLINE UINT nxtvNXB220_GetInterruptSize(void)
{
	return g_nRtvThresholdSize_FULLSEG;
}
#endif

enum E_NXTV_ISDBT_SEG_TYPE {
	NXTV_ISDBT_SEG_1 = 0,
	NXTV_ISDBT_SEG_2,
	NXTV_ISDBT_SEG_3,
	NXTV_ISDBT_SEG_4,
	NXTV_ISDBT_SEG_5,
	NXTV_ISDBT_SEG_6,
	NXTV_ISDBT_SEG_7,
	NXTV_ISDBT_SEG_8,
	NXTV_ISDBT_SEG_9,
	NXTV_ISDBT_SEG_10,
	NXTV_ISDBT_SEG_11,
	NXTV_ISDBT_SEG_12,
	NXTV_ISDBT_SEG_13
};

enum E_NXTV_ISDBT_MODE_TYPE {
	NXTV_ISDBT_MODE_1 = 0,
	NXTV_ISDBT_MODE_2,
	NXTV_ISDBT_MODE_3
};

enum E_NXTV_ISDBT_GUARD_TYPE {
	NXTV_ISDBT_GUARD_1_32 = 0,
	NXTV_ISDBT_GUARD_1_16,
	NXTV_ISDBT_GUARD_1_8,
	NXTV_ISDBT_GUARD_1_4
};

enum E_NXTV_ISDBT_INTERLV_TYPE {
	NXTV_ISDBT_INTERLV_0 = 0,
	NXTV_ISDBT_INTERLV_1,
	NXTV_ISDBT_INTERLV_2,
	NXTV_ISDBT_INTERLV_4,
	NXTV_ISDBT_INTERLV_8,
	NXTV_ISDBT_INTERLV_16,
	NXTV_ISDBT_INTERLV_32
};

enum E_NXTV_MODULATION_TYPE {
	NXTV_MOD_DQPSK = 0,
	NXTV_MOD_QPSK,
	NXTV_MOD_16QAM,
	NXTV_MOD_64QAM
};

enum E_NXTV_CODE_RATE_TYPE {
	NXTV_CODE_RATE_1_2 = 0,
	NXTV_CODE_RATE_2_3,
	NXTV_CODE_RATE_3_4,
	NXTV_CODE_RATE_5_6,
	NXTV_CODE_RATE_7_8
};

enum E_NXTV_LAYER_TYPE {
	NXTV_LAYER_A = 0,
	NXTV_LAYER_B,
	NXTV_LAYER_C
};

struct NXTV_ISDBT_LAYER_TMCC_INFO {
	enum E_NXTV_ISDBT_SEG_TYPE	eSeg; /* LP CodeRate @DVB-T */
	enum E_NXTV_MODULATION_TYPE	eModulation; /* Modulation @DVB-T */
	enum E_NXTV_CODE_RATE_TYPE	eCodeRate; /* Hierarchy Mode@DVB-T */
	enum E_NXTV_ISDBT_INTERLV_TYPE	eInterlv; /* HP CodeRate@DVB-T */
};

struct NXTV_ISDBT_TMCC_INFO {
	BOOL					fEWS;
	enum E_NXTV_ISDBT_MODE_TYPE		eTvMode;
	enum E_NXTV_ISDBT_GUARD_TYPE		eGuard;
	struct NXTV_ISDBT_LAYER_TMCC_INFO	eLayerA;
	struct NXTV_ISDBT_LAYER_TMCC_INFO	eLayerB;
	struct NXTV_ISDBT_LAYER_TMCC_INFO	eLayerC;
};

void nxtvNXB220_StandbyMode(int on);
void nxtvNXB220_GetSignalStatistics(struct NXTV_Statistics *ptSigInfo);
UINT nxtvNXB220_GetLockStatus(void);
UINT nxtvNXB220_GetAntennaLevel(U32 dwCNR);
UINT nxtvNXB220_GetAntennaLevel_1seg(U32 dwCNR);
U8   nxtvNXB220_GetAGC(void);
U32  nxtvNXB220_GetPER(void);
U32  nxtvNXB220_GetPER2(void);
U32  nxtvNXB220_GetPER3(void);

S32  nxtvNXB220_GetRSSI(void);

U32  nxtvNXB220_GetCNR(void);
U32  nxtvNXB220_GetCNR_LayerA(void);
U32  nxtvNXB220_GetCNR_LayerB(void);
U32  nxtvNXB220_GetCNR_LayerC(void);

U32  nxtvNXB220_GetBER(void);
U32  nxtvNXB220_GetBER2(void);
U32  nxtvNXB220_GetBER3(void);

U32  nxtvNXB220_GetPreviousFrequency(void);

void nxtvNXB220_GetTMCC(struct NXTV_ISDBT_TMCC_INFO *ptTmccInfo);

void nxtvNXB220_DisableStreamOut(void);
void nxtvNXB220_EnableStreamOut(void);

INT  nxtvNXB220_SetFrequency(U32 dwChFreqKHz, UINT nSubchID,
		enum E_NXTV_SERVICE_TYPE eServiceType,
		enum E_NXTV_BANDWIDTH_TYPE eBandwidthType, UINT nThresholdSize);

INT  nxtvNXB220_ScanFrequency(U32 dwChFreqKHz, UINT nSubchID,
		enum E_NXTV_SERVICE_TYPE eServiceType,
		enum E_NXTV_BANDWIDTH_TYPE eBandwidthType, UINT nThresholdSize);

INT  nxtvNXB220_Initialize(enum E_NXTV_BANDWIDTH_TYPE eBandwidthType);

#if !defined(NXTV_IF_SPI) && !defined(NXTV_IF_EBI2)
/* 0: A,B,C 1: A 2: B */
void nxtvNXB220_ISDBT_LayerSel(U8 layer);
#endif

#ifdef NXTV_DUAL_DIVERISTY_ENABLE
#define DIVERSITY_MASTER 0
#define DIVERSITY_SLAVE  1

INT nxtvNXB220_Diversity_Path_Select(BOOL bMS);

INT nxtvNXB220_Get_Diversity_Current_path(void);

INT nxtvNXB220_ConfigureDualDiversity(INT bMS);

void nxtvNXB220_ONOFF_DualDiversity(BOOL onoff);

/* return value : 0 :MIX, 1: Master 2: Slave */
INT nxtvNXB220_MonDualDiversity(void);

/* 0 : Auto 1 : Auto_NOT_Var_used 2: Force Master 3: Force Slave */
void nxtvNXB220_Diver_Manual_Set(U8 sel);

void nxtvNXB220_Diver_Update(void);
#endif /* #ifdef NXTV_DUAL_DIVERISTY_ENABLE */


#ifdef __cplusplus
}
#endif

#endif /* __NXB220_H__ */

