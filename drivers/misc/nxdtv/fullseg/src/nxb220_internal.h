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
* TITLE		: NXB220 internal header file.
*
* FILENAME	: nxb220_internal.h
*
* DESCRIPTION	:
*		All the declarations and definitions necessary for
*		the NXB220 TV driver.
*
******************************************************************************/
/******************************************************************************
* REVISION HISTORY
*
*    DATE         NAME          REMARKS
* ----------  -------------    ------------------------------------------------
* 03/03/2013  Yang, Maverick       Created.
******************************************************************************/

#ifndef __NXB220_INTERNAL_H__
#define __NXB220_INTERNAL_H__

#include "nxb220.h"

#ifdef __cplusplus
extern "C"{
#endif

#if defined(NXTV_IF_SPI) || defined(NXTV_IF_SPI_TSIFx) || defined(NXTV_IF_EBI2)
	#if defined(NXTV_INTR_POLARITY_LOW_ACTIVE)
		#define SPI_INTR_POL_ACTIVE	0x00
	#elif defined(NXTV_INTR_POLARITY_HIGH_ACTIVE)
		#define SPI_INTR_POL_ACTIVE	(1<<3)
	#endif
#endif

struct NXTV_REG_INIT_INFO {
	U8	bReg;
	U8	bVal;
};

struct NXTV_REG_MASK_INFO {
	U8	bReg;
	U8  bMask;
	U8	bVal;
};

struct NXTV_ADC_CFG_INFO {
	U8 bData2A;
	U8 bData6E;
	U8 bData70;
	U8 bData71;
	U8 bData75;
	U32 dwTNCO;
	U32 dwPNCO;
	U32 dwCFREQGAIN;
	U16 dwGAIN;
};

#if defined(NXTV_IF_TSIF_0) || defined(NXTV_IF_TSIF_1)\
|| defined(NXTV_IF_SPI_SLAVE)
	#if defined(NXTV_TSIF_SPEED_500_kbps)
		#define NXTV_FEC_TSIF_OUT_SPEED	7
	#elif defined(NXTV_TSIF_SPEED_1_Mbps)
		#define NXTV_FEC_TSIF_OUT_SPEED	6
	#elif defined(NXTV_TSIF_SPEED_2_Mbps)
		#define NXTV_FEC_TSIF_OUT_SPEED	5
	#elif defined(NXTV_TSIF_SPEED_4_Mbps)
		#define NXTV_FEC_TSIF_OUT_SPEED	4
	#elif defined(NXTV_TSIF_SPEED_7_Mbps)
		#define NXTV_FEC_TSIF_OUT_SPEED	3
	#elif defined(NXTV_TSIF_SPEED_15_Mbps)
		#define NXTV_FEC_TSIF_OUT_SPEED	2
	#elif defined(NXTV_TSIF_SPEED_30_Mbps)
		#define NXTV_FEC_TSIF_OUT_SPEED	1
	#elif defined(NXTV_TSIF_SPEED_60_Mbps)
		#define NXTV_FEC_TSIF_OUT_SPEED	0
	#else
		#error "Code not present"
	#endif
#endif

#if defined(NXTV_IF_SPI) || defined(NXTV_IF_EBI2)
	#if (NXTV_SRC_CLK_FREQ_KHz == 4000)
		#define NXTV_SPI_INTR_DEACT_PRD_VAL	0x51

	#elif (NXTV_SRC_CLK_FREQ_KHz == 13000)
		#define NXTV_SPI_INTR_DEACT_PRD_VAL	((6<<4)|3)

	#elif (NXTV_SRC_CLK_FREQ_KHz == 16000)
		#define NXTV_SPI_INTR_DEACT_PRD_VAL	((6<<4)|3)

	#elif (NXTV_SRC_CLK_FREQ_KHz == 16384)
		#define NXTV_SPI_INTR_DEACT_PRD_VAL	((6<<4)|3)

	#elif (NXTV_SRC_CLK_FREQ_KHz == 18000)
		#define NXTV_SPI_INTR_DEACT_PRD_VAL	((6<<4)|3)

	#elif (NXTV_SRC_CLK_FREQ_KHz == 19200) /* 1 clk: 52.08ns */
		#define NXTV_SPI_INTR_DEACT_PRD_VAL	((6<<4)|3)

	#elif (NXTV_SRC_CLK_FREQ_KHz == 24000)
		#define NXTV_SPI_INTR_DEACT_PRD_VAL	((6<<4)|3)

	#elif (NXTV_SRC_CLK_FREQ_KHz == 24576) /* 1 clk: 40.7ns */
		#define NXTV_SPI_INTR_DEACT_PRD_VAL	((7<<4)|2)/*about 10us*/

	#elif (NXTV_SRC_CLK_FREQ_KHz == 26000)
		#define NXTV_SPI_INTR_DEACT_PRD_VAL	((6<<4)|3)

	#elif (NXTV_SRC_CLK_FREQ_KHz == 27000)
		#define NXTV_SPI_INTR_DEACT_PRD_VAL	((6<<4)|3)

	#elif (NXTV_SRC_CLK_FREQ_KHz == 32000)
		#define NXTV_SPI_INTR_DEACT_PRD_VAL	((7<<4)|3)

	#elif (NXTV_SRC_CLK_FREQ_KHz == 32768)
		#define NXTV_SPI_INTR_DEACT_PRD_VAL	((6<<4)|3)

	#elif (NXTV_SRC_CLK_FREQ_KHz == 36000) /* 1clk: 27.7 ns */
		#define NXTV_SPI_INTR_DEACT_PRD_VAL	((7<<4)|3)

	#elif (NXTV_SRC_CLK_FREQ_KHz == 38400)
		#define NXTV_SPI_INTR_DEACT_PRD_VAL	((6<<4)|3)

	#elif (NXTV_SRC_CLK_FREQ_KHz == 40000)
		#define NXTV_SPI_INTR_DEACT_PRD_VAL	((6<<4)|3)

	#elif (NXTV_SRC_CLK_FREQ_KHz == 48000) /* 1clk: 20.8 ns */
		#define NXTV_SPI_INTR_DEACT_PRD_VAL	((9<<4)|0)
	#else
		#error "Code not present"
	#endif
#endif /* #if defined(NXTV_IF_SPI) */

#if (NXTV_TSP_XFER_SIZE == 188)
	#define N_DATA_LEN_BITVAL	0x02
	#define ONE_DATA_LEN_BITVAL	0x00
#elif (NXTV_TSP_XFER_SIZE == 204)
	#define N_DATA_LEN_BITVAL	0x03
	#define ONE_DATA_LEN_BITVAL	(1<<5)
#endif

#define SPI_OVERFLOW_INTR       0x02
#define SPI_UNDERFLOW_INTR      0x20
#define SPI_THRESHOLD_INTR      0x08
#define SPI_INTR_BITS (SPI_THRESHOLD_INTR|SPI_UNDERFLOW_INTR|SPI_OVERFLOW_INTR)

#define TOP_PAGE	0x00
#define HOST_PAGE	0x00
#define OFDM_PAGE	0x01
#define SHAD_PAGE	0x02
#define FEC_PAGE	0x03
#define DATA_PAGE	0x04
#define FEC2_PAGE	0x06
#define LPOFDM_PAGE	0x07
#define SPI_CTRL_PAGE	0x0E
#define RF_PAGE		0x0F
#define SPI_MEM_PAGE	0xFF /* Temp value. > 15 */

#define MAP_SEL_REG	0x03
#define MAP_SEL_VAL(page)		(page)

#if defined(NXTV_IF_SPI) ||  defined(NXTV_IF_SPI_TSIFx) || defined(NXTV_IF_EBI2)
	#define NXTV_REG_MAP_SEL(page)	g_bRtvPage = page
	#define NXTV_REG_GET_MAP_SEL	g_bRtvPage
#else
	#define NXTV_REG_MAP_SEL(page) \
		do  {\
			NXTV_REG_SET(MAP_SEL_REG, MAP_SEL_VAL(page));\
		} while (0)

	#define NXTV_REG_GET_MAP_SEL \
		(NXTV_REG_GET(MAP_SEL_REG))
#endif

extern U8 g_bRtvIntrMaskReg;
extern UINT g_nRtvThresholdSize_FULLSEG;

/*==============================================================================
 *
 * Common inline functions.
 *
 *============================================================================*/

static INLINE void nxtv_SetupInterruptThreshold(void)
{
#if defined(NXTV_IF_SPI) || defined(NXTV_IF_EBI2)
	NXTV_REG_MAP_SEL(SPI_CTRL_PAGE);
	NXTV_REG_SET(0x23, (g_nRtvThresholdSize_FULLSEG/188)/4);
#endif
}

/* Forward prototype. */
static INLINE void nxtv_DisableTSIF(void)
{
	NXTV_REG_MAP_SEL(FEC_PAGE);
	NXTV_REG_SET(0xA8, 0x80);

#if defined(NXTV_IF_SPI) || defined(NXTV_IF_EBI2)
	NXTV_REG_SET(0xAA, 0x80);

	NXTV_REG_MAP_SEL(SPI_CTRL_PAGE);
	/* Disable interrupts. */
	g_bRtvIntrMaskReg |= SPI_INTR_BITS;
	NXTV_REG_SET(0x24, g_bRtvIntrMaskReg);

	/* To clear interrupt and data. */
	NXTV_REG_SET(0x2A, 1);
	NXTV_REG_SET(0x2A, 0);
#else
	#if defined(NXTV_IF_TSIF_0)
	NXTV_REG_SET(0xAA, 0x80);
	#endif

	#if defined(NXTV_IF_TSIF_1)
	NXTV_REG_SET(0xAB, 0x80);
	#endif
#endif

}

static INLINE void nxtv_EnableTSIF(void)
{
#if defined(NXTV_IF_SPI) || defined(NXTV_IF_EBI2)
	nxtv_SetupInterruptThreshold();

	/* To clear interrupt and data. */
	NXTV_REG_SET(0x2A, 1);
	NXTV_REG_SET(0x2A, 0);

	/* Enable SPI interrupts */
	g_bRtvIntrMaskReg &= ~(SPI_INTR_BITS);
	NXTV_REG_SET(0x24, g_bRtvIntrMaskReg);

	NXTV_REG_MAP_SEL(FEC_PAGE);
	NXTV_REG_SET(0xA8, 0x87);
	NXTV_REG_SET(0xAA, 0x87);
#else
	NXTV_REG_MAP_SEL(FEC_PAGE);
	NXTV_REG_SET(0xA8, 0x87);

	#if defined(DUAL_PORT_TSOUT_ENABLE)
		NXTV_REG_SET(0xAA, 0x82); /* TS0 Layer A only */
		NXTV_REG_SET(0xAB, 0x85); /* TS1 Layer B,C only */
	#else
		#if defined(NXTV_IF_TSIF_0)
		NXTV_REG_SET(0xAA, 0x87);
		#endif

		#if defined(NXTV_IF_TSIF_1)
		NXTV_REG_SET(0xAB, 0x87);
		#endif
	#endif
#endif

}

/* #define PRE_EXTEND_VALID_SIGNAL */
/* #define TSOUT_LSB_FIRST */
#if defined(NXTV_IF_TSIF_0) || defined(NXTV_IF_SPI_SLAVE)
static INLINE void nxtv_ConfigureTsif0Format(void)
{
	U8 REG9F;
	NXTV_REG_MAP_SEL(FEC_PAGE);
	REG9F = NXTV_REG_GET(0x9F) & 0xAA;

#if defined(NXTV_TSIF_FORMAT_0) /* EN_high, CLK_rising */
	NXTV_REG_SET(0x9F, (REG9F | 0x00));
	NXTV_REG_SET(0xA5, 0x08);
#elif defined(NXTV_TSIF_FORMAT_1) /* EN_high, CLK_falling */
	NXTV_REG_SET(0x9F, (REG9F | 0x00));
	NXTV_REG_SET(0xA5, 0x00);
#elif defined(NXTV_TSIF_FORMAT_2) /* EN_low, CLK_rising */
	NXTV_REG_SET(0x9F, (REG9F | 0x10));
	NXTV_REG_SET(0xA5, 0x08);
#elif defined(NXTV_TSIF_FORMAT_3) /* EN_low, CLK_falling */
	NXTV_REG_SET(0x9F, (REG9F | 0x10));
	NXTV_REG_SET(0xA5, 0x00);
#elif defined(NXTV_TSIF_FORMAT_4) /* EN_high, CLK_rising + 1CLK add */
	NXTV_REG_SET(0x9F, (REG9F | 0x00));
	NXTV_REG_SET(0xA5, 0x0C);
#elif defined(NXTV_TSIF_FORMAT_5) /* EN_high, CLK_falling + 1CLK add */
	NXTV_REG_SET(0x9F, (REG9F | 0x00));
	NXTV_REG_SET(0xA5, 0x04);
#elif defined(NXTV_TSIF_FORMAT_6) || defined(NXTV_TSIF_FORMAT_7)
	#error "NXTV_TSIF_FORMAT_6/7 is not suported at NXTV_IF_TSIF_0 Mode"
#else
	#error "Code not present"
#endif
	NXTV_REG_SET(0xA4, 0x89);
	NXTV_REG_SET(0xA8, 0x87);
	NXTV_REG_SET(0xA9, (0xB8|NXTV_FEC_TSIF_OUT_SPEED));

#if defined(NXTV_ERROR_TSP_OUTPUT_DISABLE)
	NXTV_REG_MASK_SET(0xA5, 0x40, 0x40);
#endif
#if defined(NXTV_NULL_PID_TSP_OUTPUT_DISABLE)
	NXTV_REG_MASK_SET(0xA5, 0x20, 0x20);
#endif
#if defined(NXTV_NULL_PID_GENERATE)
	NXTV_REG_MASK_SET(0xA4, 0x02, 0x02);
#endif

#if defined(NXTV_IF_CSI656_RAW_8BIT_ENABLE)
	NXTV_REG_MASK_SET(0x9F, 0x0F, 0x05); /* One clock pre-add. */
	NXTV_REG_SET(0x9D, 0x01); /* Sync signal One pre-move. */
#endif

#if defined(PRE_EXTEND_VALID_SIGNAL)
	NXTV_REG_MASK_SET(0xA5, 0x10, 0x10);
#endif

#if defined(TSOUT_LSB_FIRST)
	NXTV_REG_MASK_SET(0xA4, 0x04, 0x04);
#endif


#if defined(DUAL_PORT_TSOUT_ENABLE)
	NXTV_REG_SET(0xAA, 0x82); /* TS0 Layer A only */
#else
	NXTV_REG_SET(0xAA, 0x87);
#endif
}
#endif /* #elif defined(NXTV_IF_TSIF_0) || defined(NXTV_IF_SPI_SLAVE) */

#if defined(NXTV_IF_TSIF_1) || defined(NXTV_IF_SPI_SLAVE)
static INLINE void nxtv_ConfigureTsif1Format(void)
{
	 U8 REG9F;
	NXTV_REG_MAP_SEL(FEC_PAGE);
	REG9F = NXTV_REG_GET(0x9F) & 0x55;
#if defined(NXTV_TSIF_FORMAT_0) /* EN_high, CLK_rising */
	NXTV_REG_SET(0x9F, (REG9F | 0x00));
	NXTV_REG_SET(0xA6, 0x88);
	NXTV_REG_SET(0xA7, 0x49);//0x48);

#elif defined(NXTV_TSIF_FORMAT_1) /* EN_high, CLK_falling */
	NXTV_REG_SET(0x9F, (REG9F | 0x00));
	NXTV_REG_SET(0xA6, 0x88);
	NXTV_REG_SET(0xA7, 0x40);

#elif defined(NXTV_TSIF_FORMAT_2) /* EN_low, CLK_rising */
	NXTV_REG_SET(0x9F, (REG9F | 0x20));
	NXTV_REG_SET(0xA6, 0x88);
	NXTV_REG_SET(0xA7, 0x48);

#elif defined(NXTV_TSIF_FORMAT_3) /* EN_low, CLK_falling */
	NXTV_REG_SET(0x9F, (REG9F | 0x20));
	NXTV_REG_SET(0xA6, 0x88);
	NXTV_REG_SET(0xA7, 0x40);

#elif defined(NXTV_TSIF_FORMAT_4) /* EN_high, CLK_rising + 1CLK add */
	NXTV_REG_SET(0x9F, (REG9F | 0x00));
	NXTV_REG_SET(0xA6, 0x88);
	NXTV_REG_SET(0xA7, 0x4C);

#elif defined(NXTV_TSIF_FORMAT_5) /* EN_high, CLK_falling + 1CLK add */
	NXTV_REG_SET(0x9F, (REG9F | 0x00));
	NXTV_REG_SET(0xA6, 0x88);
	NXTV_REG_SET(0xA7, 0x44);

#elif defined(NXTV_TSIF_FORMAT_6) /* Parallel: EN_high, CLK_rising*/
	#if defined(NXTV_IF_SPI_SLAVE)
	#error "NXTV_TSIF_FORMAT_6 is not suported at NXTV_IF_SPI_SLAVE Mode"
	#else
	NXTV_REG_SET(0x9F, (REG9F | 0x00));
	NXTV_REG_SET(0xA6, 0x80);
	NXTV_REG_SET(0xA7, 0x48);
	#endif
#elif defined(NXTV_TSIF_FORMAT_7) /* Parallel: EN_high, CLK_falling */
	#if defined(NXTV_IF_SPI_SLAVE)
	#error "NXTV_TSIF_FORMAT_7 is not suported at NXTV_IF_SPI_SLAVE Mode"
	#else
	NXTV_REG_SET(0x9F, (REG9F | 0x00));
	NXTV_REG_SET(0xA6, 0x80);
	NXTV_REG_SET(0xA7, 0x40);
	#endif
#else
	#error "Code not present"
#endif

	NXTV_REG_MASK_SET(0xA4, 0x01, 0x01); /* TEI Enable */

#if defined(NXTV_ERROR_TSP_OUTPUT_DISABLE)
	NXTV_REG_MASK_SET(0xA5, 0x40, 0x40);
#endif

#if defined(NXTV_NULL_PID_TSP_OUTPUT_DISABLE)
	NXTV_REG_MASK_SET(0xA5, 0x20, 0x20);
#endif

#if defined(NXTV_NULL_PID_GENERATE)
	NXTV_REG_MASK_SET(0xA4, 0x02, 0x02);
#endif
	NXTV_REG_SET(0xA8, 0x87);
	NXTV_REG_SET(0xA9, (0xB8|NXTV_FEC_TSIF_OUT_SPEED));

#if defined(NXTV_IF_CSI656_RAW_8BIT_ENABLE)
	NXTV_REG_MASK_SET(0x9F, 0x0F, 0x0A); /* One clock pre-add. */
	#if defined(NXTV_TSIF_FORMAT_6)  || defined(NXTV_TSIF_FORMAT_7)
	NXTV_REG_SET(0x9E, 0x04); /* 4bit clock pre-move. */
	#else
	NXTV_REG_SET(0x9E, 0x01); /* 8bit clock pre-move. */
	#endif
#endif

#if defined(PRE_EXTEND_VALID_SIGNAL)
	NXTV_REG_MASK_SET(0xA7, 0x10, 0x10);
#endif

#if defined(TSOUT_LSB_FIRST)
	NXTV_REG_MASK_SET(0xA6, 0x04, 0x04);
#endif


#if defined(DUAL_PORT_TSOUT_ENABLE)
	NXTV_REG_SET(0xAB, 0x85); /* TS1 Layer B,C only */
#else
	NXTV_REG_SET(0xAB, 0x87);
#endif
}
#endif /* #elif defined(NXTV_IF_TSIF_1) || defined(NXTV_IF_SPI_SLAVE) */

static INLINE int nxtvRF_LockCheck(U8 bCheckBlock)
{
	INT i = 0;
	INT nRet = NXTV_SUCCESS;
	U8 nLockCheck = 0;

	NXTV_REG_MAP_SEL(RF_PAGE);

	switch (bCheckBlock) {
	case 0: /* O == RF Lock Check */
		for (i = 0; i < 10; i++) {
			nLockCheck = NXTV_REG_GET(0x1B) & 0x02;
			if (nLockCheck)
				break;
			else
				NXTV_DBGMSG1("[nxtvRF_LockCheck]VCheck(%d)\n", i);

			NXTV_DELAY_MS(1);
		}

		if (i == 10) {
			NXTV_DBGMSG0("[nxtvRF_LockCheck] VCO Pll unlocked!\n");
			nRet =  NXTV_PLL_UNLOCKED;
		}
		break;

	case 1: /* CLK Synth Lock Check */
		for (i = 0; i < 10; i++) {
			nLockCheck = NXTV_REG_GET(0x1B) & 0x01;
			if (nLockCheck)
				break;
			else
				NXTV_DBGMSG1("[nxtvRF_LockCheck]SCheck(%d)\n", i);

			NXTV_DELAY_MS(1);
		}

		if (i == 10) {
			NXTV_DBGMSG0("[nxtvRF_LockCheck] ADC clock unlocked!\n");
			nRet =  NXTV_ADC_CLK_UNLOCKED;
		}
		break;
	}

	return nRet;
}

extern BOOL g_fRtv1segLpMode;
extern enum E_NXTV_SERVICE_TYPE g_eRtvServiceType;

#ifdef NXTV_DUAL_DIVERISTY_ENABLE
extern enum E_NXTV_SERVICE_TYPE g_eRtvServiceType_slave;
#endif

static INLINE void nxtv_UpdateMon(void)
{
	if (g_fRtv1segLpMode) {
		NXTV_REG_MAP_SEL(LPOFDM_PAGE);
		NXTV_REG_MASK_SET(0x13, 0x80, 0x80);
		NXTV_REG_MASK_SET(0x13, 0x80, 0x00);
	} else {
		NXTV_REG_MAP_SEL(OFDM_PAGE);
		NXTV_REG_MASK_SET(0x1B, 0x80, 0x80);
		NXTV_REG_MASK_SET(0x1B, 0x80, 0x00);
	}

	NXTV_REG_MAP_SEL(FEC_PAGE);
	NXTV_REG_MASK_SET(0x11, 0x04, 0x04);
	NXTV_REG_MASK_SET(0x11, 0x04, 0x00);
}

static INLINE void nxtv_SoftReset(void)
{
	if (g_fRtv1segLpMode)
		NXTV_REG_MAP_SEL(LPOFDM_PAGE);
	else
		NXTV_REG_MAP_SEL(OFDM_PAGE);

	NXTV_REG_MASK_SET(0x10, 0x01, 0x01);
	NXTV_REG_MASK_SET(0x10, 0x01, 0x00);

	NXTV_REG_MAP_SEL(FEC_PAGE);
	NXTV_REG_MASK_SET(0xFB, 0x01, 0x01);
	NXTV_REG_MASK_SET(0xFB, 0x01, 0x00);
}

static INLINE INT nxtv_ServiceTypeSelect(enum E_NXTV_SERVICE_TYPE eServiceType)
{
	INT nRet = NXTV_SUCCESS;

	switch (eServiceType) {
#if defined(NXTV_ISDBT_ENABLE)
	case NXTV_SERVICE_UHF_ISDBT_1seg:
	case NXTV_SERVICE_VHF_ISDBTmm_1seg:
	case NXTV_SERVICE_VHF_ISDBTsb_1seg:
		NXTV_REG_MAP_SEL(HOST_PAGE);
		NXTV_REG_SET(0x0B, 0x36);

		NXTV_REG_SET(0x12, 0x08);
		NXTV_REG_SET(0x21, 0x01);
		NXTV_REG_SET(0x26, 0x00);

		NXTV_REG_MAP_SEL(FEC_PAGE);
		NXTV_REG_SET(0x20, 0x0C);

		NXTV_REG_SET(0x23, 0xF0); /* Layer A */
#if !defined(NXTV_IF_SPI) && !defined(NXTV_IF_EBI2)
		NXTV_REG_SET(0x24, 0x31);
		NXTV_REG_SET(0x4F, 0x1F);
#endif
		NXTV_REG_SET(0x44, 0x68);
		NXTV_REG_SET(0x47, 0x40);

		NXTV_REG_SET(0x53, 0x3E);
		NXTV_REG_SET(0x21, 0x00);
		NXTV_REG_SET(0x22, 0x00);
		NXTV_REG_SET(0x5C, 0x10);
		NXTV_REG_SET(0x5F, 0x10);
		NXTV_REG_SET(0x77, 0x40);
		NXTV_REG_SET(0x7A, 0x20);
		NXTV_REG_SET(0x83, 0x10);
		NXTV_REG_SET(0x96, 0x00);
		NXTV_REG_SET(0xAE, 0x00);

		NXTV_REG_SET(0xFC, 0x83);
		NXTV_REG_SET(0xFF, 0x03);

#if 0
		NXTV_REG_SET(0x44, 0x48);
		NXTV_REG_SET(0x47, 0x00);
#endif
		g_fRtv1segLpMode = 1;
		break;
	case NXTV_SERVICE_VHF_ISDBTsb_3seg:
		NXTV_DBGMSG0("[nxtvRF_SelectService] 3seg is not implemented\n");
		break;

	case NXTV_SERVICE_UHF_ISDBT_13seg:
	case NXTV_SERVICE_VHF_ISDBTmm_13seg:
		NXTV_REG_MAP_SEL(HOST_PAGE);
		NXTV_REG_SET(0x0B, 0x96);

		NXTV_REG_SET(0x12, 0x00);
		NXTV_REG_SET(0x21, 0x00);
		NXTV_REG_SET(0x26, 0xB8);

		NXTV_REG_MAP_SEL(OFDM_PAGE);
		NXTV_REG_SET(0x10, 0xD4);

		NXTV_REG_MAP_SEL(FEC_PAGE);
		NXTV_REG_SET(0x20, 0x00);
#ifdef NXTV_DUAL_DIVERISTY_ENABLE
		NXTV_REG_SET(0x21, 0x22);
		NXTV_REG_SET(0x22, 0x22);
#else
		NXTV_REG_SET(0x21, 0x21);
		NXTV_REG_SET(0x22, 0x21);
#endif

#if 0
		NXTV_REG_SET(0x23, 0x84);
		NXTV_REG_SET(0x24, 0x31);
		NXTV_REG_SET(0x4F, 0x1F);
#endif

		NXTV_REG_SET(0x23, 0x90);
#if !defined(NXTV_IF_SPI) && !defined(NXTV_IF_EBI2)
		NXTV_REG_SET(0x24, 0x01);
		NXTV_REG_SET(0x4F, 0x00);
#endif
		NXTV_REG_SET(0x44, 0x68);
		NXTV_REG_SET(0x47, 0x40);

		NXTV_REG_SET(0x53, 0x1E);
		NXTV_REG_SET(0x5C, 0x11);
		NXTV_REG_SET(0x5F, 0x11);
		NXTV_REG_SET(0x77, 0x00);
		NXTV_REG_SET(0x7A, 0x00);
		NXTV_REG_SET(0x83, 0x00);
		NXTV_REG_SET(0x96, 0x20);
		NXTV_REG_SET(0xAE, 0x02);

		NXTV_REG_SET(0xFC, 0x83);
		NXTV_REG_SET(0xFF, 0x03);

#if 0
		NXTV_REG_SET(0x44, 0xE8);
		NXTV_REG_SET(0x47, 0x40);
#endif
		g_fRtv1segLpMode = 0;
		break;
#endif
#if defined(NXTV_DVBT_ENABLE)
	case NXTV_SERVICE_DVBT:
		NXTV_REG_MAP_SEL(HOST_PAGE);
		NXTV_REG_SET(0x0B, 0x96);
		NXTV_REG_SET(0x12, 0x00);
		NXTV_REG_SET(0x21, 0x00);
		NXTV_REG_SET(0x26, 0xB8);

		NXTV_REG_MAP_SEL(DATA_PAGE);
		NXTV_REG_SET(0xA2, 0x0E);
		NXTV_REG_SET(0xA3, 0x0E);
		NXTV_REG_SET(0xA7, 0x0D);
		NXTV_REG_SET(0xA6, 0x0D);

		NXTV_REG_MAP_SEL(OFDM_PAGE);
		NXTV_REG_SET(0x10, 0xD6);

		NXTV_REG_MAP_SEL(FEC_PAGE);
		NXTV_REG_SET(0x20, 0x00);
#ifdef NXTV_DUAL_DIVERISTY_ENABLE
		NXTV_REG_SET(0x21, 0x22);
		NXTV_REG_SET(0x22, 0x22);
		NXTV_REG_SET(0x53, 0x03); //
#else
		NXTV_REG_SET(0x21, 0x21);
		NXTV_REG_SET(0x22, 0x21);
		NXTV_REG_SET(0x53, 0x1E);
#endif

		NXTV_REG_SET(0x23, 0xF0); /* Layer A */
#if !defined(NXTV_IF_SPI) && !defined(NXTV_IF_EBI2)
		NXTV_REG_SET(0x24, 0x11);
		NXTV_REG_SET(0x4F, 0x07);
#endif
		NXTV_REG_SET(0x44, 0xE8);
		NXTV_REG_SET(0x47, 0x40);

		NXTV_REG_SET(0x5C, 0x10);
		NXTV_REG_SET(0x5F, 0x10);
		NXTV_REG_SET(0x77, 0x00);
		NXTV_REG_SET(0x7A, 0x00);
		NXTV_REG_SET(0x83, 0x00);
		NXTV_REG_SET(0x96, 0x20);
		NXTV_REG_SET(0xAE, 0x02);

		NXTV_REG_SET(0xFC, 0x83);
		NXTV_REG_SET(0xFF, 0x03);
		
#if 0
		NXTV_REG_SET(0x44, 0xE8);
		NXTV_REG_SET(0x47, 0x40);
#endif
		g_fRtv1segLpMode = 0;
		break;
#endif
	default:
		nRet = NXTV_INVAILD_SERVICE_TYPE;
	}

	return nRet;
}

/*=============================================================================
* External functions for NXTV driver core.
*============================================================================*/
INT nxtv_InitSystem_FULLSEG(void);

#ifdef __cplusplus
}
#endif

#endif /* __NXB220_INTERNAL_H__ */


