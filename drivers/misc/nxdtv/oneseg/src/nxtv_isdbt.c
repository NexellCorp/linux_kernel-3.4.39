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
* TITLE 	  : NEXELL TV ISDB-T services source file. 
*
* FILENAME    : nxtv_isdbt.c
*
* DESCRIPTION : 
*		Library of routines to initialize, and operate on, the NEXELL ISDB-T demod.
*
********************************************************************************/

/******************************************************************************** 
* REVISION HISTORY
*
*    DATE	  	  NAME				REMARKS
* ----------  -------------    --------------------------------------------------
* 10/01/2010  Ko, Kevin        Changed the debug macro name.
* 09/27/2010  Yang, Maverick   Changed the scan alg.
* 09/27/2010  Ko, Kevin        Creat for CS Realease
*             /Yang, Maverick  1.Reformating for CS API
*                              2.pll table, ADC clock switching, SCAN function, 
*								 FM function added..
* 04/09/2010  Yang, Maverick   REV1 SETTING 
* 01/25/2010  Yang, Maverick   Created.                                                              
********************************************************************************/

#include "nxtv_rf.h"

 
#ifdef NXTV_ISDBT_ENABLE

 static BOOL g_fIsdbtFastScanEnabled;
 
 static const NXTV_REG_INIT_INFO g_atOfdmInitData[] = 
 {
	{0x1d, 0x01},
	{0x1e, 0x4d},
	{0x1f, 0xd8},
	{0x20, 0x09},
	{0x21, 0x2c},
	{0x22, 0x01},
	{0x23, 0x2c},
	{0x24, 0x4c},
	{0x27, 0xff},
	{0x29, 0xff},
	{0x2a, 0x01},
	{0x2b, 0x01},
	{0x2c, 0x01},
	{0x2e, 0x48},
	{0x2f, 0x04},
	{0x30, 0x17},
	{0x31, 0xff},
	{0x35, 0x01},
	{0x36, 0x20},
	{0x37, 0x6e}, 
	{0x38, 0x51},
	{0x39, 0x78}, 
	{0x3a, 0x69},
	{0x3b, 0xa8},
	{0x3c, 0x61},
	{0x3d, 0x69},
	{0x3e, 0x91},
	{0x3f, 0x51},
	{0x40, 0xd0},
	{0x41, 0x10},
	{0x42, 0x90},
	{0x43, 0xe4},
	{0x44, 0x0a},
	{0x45, 0x00},
	{0x46, 0x00},
	{0x47, 0x00},
	{0x48, 0x00},
	{0x49, 0x00},
	{0x4a, 0x00},
	{0x4b, 0x00},
	{0x4c, 0x00},
	{0x4d, 0xb2},
	{0x4e, 0x96},
	{0x4f, 0x0a},
	{0x50, 0x14},
	{0x51, 0x14},
	{0x52, 0x14},
	{0x54, 0x76},
	{0x55, 0x09},
	{0x56, 0x14},
	{0x57, 0xa7},
	{0x59, 0xc0},
	{0x5a, 0x10},
	{0x5d, 0x13},
	{0x5e, 0x04},
	{0x5f, 0x04},
	{0x60, 0x80},
	{0x62, 0x90},
	{0x63, 0x30},
	{0x64, 0x05},
	{0x65, 0x0d},
	{0x66, 0x46},
	{0x69, 0x24},
	{0x6a, 0x88},
	{0x6b, 0x42},
	{0x6c, 0x31},
	{0x6d, 0x05},
	{0x70, 0xa9},
	{0x71, 0x02},
	{0x72, 0x04},
	{0x73, 0x06},
	{0x74, 0xe1},
	{0x75, 0x63},
	{0x76, 0x04},
	{0x77, 0x31},
	{0x78, 0x24},
	{0x79, 0x94},
	{0x80, 0x1c},
	{0xf4, 0x01},
	{0xf5, 0x69},
	{0xf6, 0xbe},
	{0xf7, 0x05},
	{0xd2, 0x50},
	{0xd3, 0x58},
	{0xd4, 0x10},
	{0xe8, 0x40},
	{0xe9, 0x2b},
	{0xea, 0x64},
	{0xeb, 0x2b},
	{0xec, 0x25},
	{0xed, 0x50},
	{0xee, 0x0a},
	{0xef, 0x0b}
 };

 static const NXTV_REG_INIT_INFO g_atFecInitData[] =
 {
	{0x71, 0x40},	// cer / ber / alarm period mothod {new}  && alarm occur calculation 
	{0x73, 0x00}, 	// Viterbi Period {MSB}
	{0x74, 0x0c}, 	// Viterbi Period {LSB}
	{0x75, 0x00}, 	// Byte Period {MSB}
	{0x76, 0x02}, 	// Byte Period {LSB}
	{0x77, 0x00}, 	// Alarm Period {MSB}
	{0x78, 0x01}, 	// Alarm Period {LSB}
	{0x79, 0x02}, 	// CER Period
	{0x13, 0x0d},	// 8d : test mode {old}, [4] Null mode [0] 16 QAM   //0x1d : error==>null packet, //0x0d ==> normal
	{0x21, 0x08},	// tmcc mix mode
	{0x20, 0x50},	// TMCC threshold	, 0x50 : 160, 0x60 : 170 {+80}
	{0x3b, 0xa0},	// TMCC moving sum skip 0
	{0x1d, 0x57},	// TMCC sync frame 
	{0x1e, 0x04},	// Manual byte sync 
	{0x6e, 0x10},	
	{0x12, 0x17}  // 0x1f ==> normanl, 0x17 ; error ==> 0x47.
 }; 


/*===============================================================================
 * isdbt_UpdateMonitoring
 *
 * DESCRIPTION : 
 *		This function returns 
 *		
 *
 * ARGUMENTS : none.
 * RETURN VALUE : none.
 *============================================================================*/
static void isdbt_UpdateMonitoring(void)
{	
	U8 R_Data;
	
	NXTV_REG_MAP_SEL(OFDM_PAGE);
	R_Data = NXTV_REG_GET(0x18) | 0x80;
	
	NXTV_REG_SET(0x18, R_Data);
	R_Data=R_Data&0x7f;
	NXTV_REG_SET(0x18, R_Data);
}


static void isdbt_Enable(BOOL ISDBT_EN)
{
	NXTV_REG_SET(0x07,0xF0|ISDBT_EN);
}


static void isdbt_InitOFDM(void)
{
	UINT nNumTblEntry;
	const NXTV_REG_INIT_INFO *ptOfdmInitData;
	
	NXTV_REG_MAP_SEL(OFDM_PAGE);
	
	NXTV_REG_SET(0x08,0x01);
	NXTV_REG_SET(0x08,0x00);

	NXTV_REG_SET(0x11,0x06);
	NXTV_REG_SET(0x13,0x3f);
	NXTV_REG_SET(0x15,0xf1);
	NXTV_REG_SET(0x16,0x0e);
	NXTV_REG_SET(0x17,0x00);
	
	switch( g_eRtvAdcClkFreqType_1SEG )
	{
		case NXTV_ADC_CLK_FREQ_8_MHz: 
			NXTV_REG_SET(0x19,0xff);
			NXTV_REG_SET(0x1a,0x08);
			NXTV_REG_SET(0x1b,0x82);
			NXTV_REG_SET(0x1c,0x20);
			break;
			
		case NXTV_ADC_CLK_FREQ_8_192_MHz: 
			NXTV_REG_SET(0x19,0xfd);
			NXTV_REG_SET(0x1a,0xfc);
			NXTV_REG_SET(0x1b,0xbe);
			NXTV_REG_SET(0x1c,0x1f);
			break;
			
		case NXTV_ADC_CLK_FREQ_9_MHz: 
			NXTV_REG_SET(0x19,0xe4);
			NXTV_REG_SET(0x1a,0x5c);
			NXTV_REG_SET(0x1b,0xe5);
			NXTV_REG_SET(0x1c,0x1c);
			break;
			
		case NXTV_ADC_CLK_FREQ_9_6_MHz:
			NXTV_REG_SET(0x19,0xd7);
			NXTV_REG_SET(0x1a,0x08);
			NXTV_REG_SET(0x1b,0x17);
			NXTV_REG_SET(0x1c,0x1b);
			break;
			
		default:
			// NXTV_DBGMSG0("[isdbt_InitOFDM] Upsupport ADC clock type! \n");
			break;
	}
	
	// Set the remained initial values.
	nNumTblEntry = sizeof(g_atOfdmInitData) / sizeof(NXTV_REG_INIT_INFO);
	ptOfdmInitData = g_atOfdmInitData;
		
	do
	{
		NXTV_REG_SET(ptOfdmInitData->bReg, ptOfdmInitData->bVal);
		ptOfdmInitData++;
	} while( --nNumTblEntry );
}


static void isdbt_InitFEC(void)
{
	UINT nNumTblEntry;
	const NXTV_REG_INIT_INFO *ptFecInitData;
	
	NXTV_REG_MAP_SEL(FEC_PAGE);
	
	nNumTblEntry = sizeof(g_atFecInitData) / sizeof(NXTV_REG_INIT_INFO);
	ptFecInitData = g_atFecInitData;

	do	
	{
		NXTV_REG_SET(ptFecInitData->bReg, ptFecInitData->bVal);
		ptFecInitData++;
	} while( --nNumTblEntry );
}



static void isdbt_SetTNCO_PNCO(void)
{
	U32 TNCO, PNCO;

	switch( g_eRtvAdcClkFreqType_1SEG )
	{
		case NXTV_ADC_CLK_FREQ_8_MHz: 
			TNCO = 0x10410410;		// TNCO
			PNCO = 0xF0000000;		// PNCO			
			break;
			
		case NXTV_ADC_CLK_FREQ_8_192_MHz: 
		    TNCO = 0x0FDF7DF7;		// TNCO
			PNCO = 0xF0600000;		// PNCO
			break;
			
		case NXTV_ADC_CLK_FREQ_9_MHz: 
		    TNCO = 0x0E72AE47;		// TNCO
			PNCO = 0xF1C71C72;		// PNCO
			break;
			
		case NXTV_ADC_CLK_FREQ_9_6_MHz:
		    TNCO = 0x0D8B8362;		// TNCO
			PNCO = 0xF2AAAAAB;		// PNCO
			break;
			
		default:
			NXTV_DBGMSG0("[isdbt_SetTNCO_PNCO] Upsupport ADC clock type! \n");
			return;
	}

	NXTV_REG_MAP_SEL(OFDM_PAGE);
	NXTV_REG_SET(0x45, (U8)(TNCO>>0)&0xff);
	NXTV_REG_SET(0x46, (U8)(TNCO>>8)&0xff);
	NXTV_REG_SET(0x47, (U8)(TNCO>>16)&0xff);
	NXTV_REG_SET(0x48, (U8)(TNCO>>24)&0xff);
	
	// PNCO
	NXTV_REG_SET(0x49, (U8)(PNCO>>0)&0xff);
	NXTV_REG_SET(0x4a, (U8)(PNCO>>8)&0xff);
	NXTV_REG_SET(0x4b, (U8)(PNCO>>16)&0xff);
	NXTV_REG_SET(0x4c, (U8)(PNCO>>24)&0xff);
}



static void isdbt_StartOFDM(void)
{
	NXTV_REG_MAP_SEL(OFDM_PAGE);
	NXTV_REG_SET(0x11,0x07);
	NXTV_REG_SET(0x11,0x06);
}
 

static void isdbt_SoftResetFEC(void)
{
	NXTV_REG_MAP_SEL(FEC_PAGE);
	NXTV_REG_SET(0x10,0x01);	
	NXTV_REG_SET(0x10,0x00); 
}



static void isdbt_InitDemod(void)
{
	g_fIsdbtFastScanEnabled = FALSE;
		
	// Initilize the OFDM
	isdbt_InitOFDM();
	
	isdbt_SetTNCO_PNCO();
	
	isdbt_Enable(TRUE);
		
	// Initilize the FEC
	isdbt_InitFEC();

	nxtvOEM_ConfigureInterrupt_1SEG();
	
#if defined(NXTV_IF_MPEG2_PARALLEL_TSIF)
	NXTV_REG_MAP_SEL(DD_PAGE);
	NXTV_REG_SET(0x46, 0x00);
	
	nxtv_SetParallelTsif_1SEG_Only();
	
#elif defined(NXTV_IF_SPI) || defined(NXTV_IF_EBI2)
	NXTV_REG_MAP_SEL(DD_PAGE);
	NXTV_REG_SET(0x3A, 0xC0);
	NXTV_REG_SET(0x45, 0xA0);
	NXTV_REG_SET(0x46, 0x00);
	
	
#elif defined(NXTV_IF_SERIAL_TSIF) || defined(NXTV_IF_SPI_SLAVE)
    NXTV_REG_MAP_SEL(DD_PAGE);
	NXTV_REG_SET(0x45, 0xA0);
	NXTV_REG_SET(0x46, 0x00);
	
	nxtv_Set_MSC1_SUBCH0(0, TRUE, TRUE);

	NXTV_REG_MAP_SEL(COMM_PAGE); 	
    nxtv_ConfigureTsifFormat(); 
    NXTV_REG_SET(0x47, 0x13|NXTV_COMM_CON47_CLK_SEL); // MSC1 DD-TSI enable 
  	 	
#else
	#error "Code not present"  	 	 
#endif
	
	nxtv_ConfigureHostIF_1SEG();
	
	isdbt_StartOFDM();
	
	isdbt_SoftResetFEC();
}


static void isdbt_SetFastScanMode(void)
{
	NXTV_REG_MAP_SEL(OFDM_PAGE);
	
	NXTV_REG_SET(0x08, 0x01);		
	NXTV_REG_SET(0x08, 0x00);		
	NXTV_REG_SET(0x13, 0x3c);
	NXTV_REG_SET(0x14, 0x00);
	NXTV_REG_SET(0x15, 0xf0);
	NXTV_REG_SET(0x38, 0x41);
	NXTV_REG_SET(0x40, 0x50);
	NXTV_REG_SET(0x1d, 0x01);

	NXTV_REG_SET(0x23, 0xac);
	NXTV_REG_SET(0x21, 0x2C);
	NXTV_REG_SET(0x22, 0x01);

	isdbt_SetTNCO_PNCO();

	isdbt_StartOFDM();
	
	g_fIsdbtFastScanEnabled = TRUE;
}


void nxtvISDBT_StandbyMode(int on)
{
	NXTV_GUARD_LOCK;

	if( on )
	{ 
		NXTV_REG_MAP_SEL(OFDM_PAGE); 
		NXTV_REG_SET(0x07, 0x01);

		NXTV_REG_MAP_SEL(RF_PAGE); 
		NXTV_REG_MASK_SET(0x57,0x04, 0x04);  //SW PD ALL      
	}
	else
	{	  
		NXTV_REG_MAP_SEL(RF_PAGE); 
		NXTV_REG_MASK_SET(0x57,0x04, 0x00);  //SW PD ALL	

		NXTV_REG_MAP_SEL(OFDM_PAGE); 
		NXTV_REG_SET(0x07, 0xF1);
	}

	NXTV_GUARD_FREE;
}

UINT nxtvISDBT_GetLockStatus(void)
{
    UINT OFDM_Lock;
	UINT TMCC_lock;
	UINT lock_st = 0;
	
   	if(g_fRtvChannelChange_1SEG) 
   	{
   		NXTV_DBGMSG0("[nxtvISDBT_GetLockStatus] NXTV Freqency change state! \n");	
		return 0x0;	 
	}

	NXTV_GUARD_LOCK;
		
	isdbt_UpdateMonitoring();

	///////////// OFDM lock /////////////     Bit3
	OFDM_Lock=((NXTV_REG_GET(0x81)>>5)&0x01);
	if(OFDM_Lock==0x01) 
		lock_st = NXTV_ISDBT_OFDM_LOCK_MASK;
	 
	NXTV_REG_MAP_SEL(FEC_PAGE);
	TMCC_lock = (NXTV_REG_GET(0x82) >>5) & 0x1;

	NXTV_GUARD_FREE;

	if (TMCC_lock)
		lock_st |= NXTV_ISDBT_TMCC_LOCK_MASK;
		
	return lock_st;
}


U8 nxtvISDBT_GetAGC(void)
{
	U8 bAgc;
	
	if(g_fRtvChannelChange_1SEG) 
	{
		NXTV_DBGMSG0("[nxtvISDBT_GetAGC] NXTV Freqency change state! \n");
		return 0;	 
	}
	
	NXTV_GUARD_LOCK;
	
	NXTV_REG_MAP_SEL(RF_PAGE);
	bAgc = NXTV_REG_GET(0x05);

	NXTV_GUARD_FREE;

	return bAgc;
}


S32 nxtvISDBT_GetRSSI(void) 
{
	U8 RD00;
	S32 nRssi;
	
	if(g_fRtvChannelChange_1SEG) 
	{
		NXTV_DBGMSG0("[nxtvISDBT_GetRSSI] NXTV Freqency change state! \n");
		return 0;	 
	}

	NXTV_GUARD_LOCK;
	
	NXTV_REG_MAP_SEL(RF_PAGE);	
	RD00 = NXTV_REG_GET(0x00); 

	nRssi = -((((RD00 & 0x30) >> 4) * (S32)(9.5 *NXTV_ISDBT_RSSI_DIVIDER)) 
			+ ((RD00 & 0x0F) * (S32)(3 *NXTV_ISDBT_RSSI_DIVIDER))  
			+ (( (NXTV_REG_GET(0x02) & 0x1E) >> 1 ) * (S32)(3 * NXTV_ISDBT_RSSI_DIVIDER)) 
			+ ((NXTV_REG_GET(0x04) &0x7F) * (S32)(0.5 * NXTV_ISDBT_RSSI_DIVIDER))); 

	NXTV_GUARD_FREE;
	
	if((RD00 & 0x30) == 0x30)
		nRssi += (S32)(15 * NXTV_ISDBT_RSSI_DIVIDER);
	else 
		nRssi += (S32)(20 * NXTV_ISDBT_RSSI_DIVIDER);

	return nRssi;
}



U32 nxtvISDBT_GetPER(void)
{
	U32 iPer;

	if(g_fRtvChannelChange_1SEG) 
	{
		NXTV_DBGMSG0("[nxtvISDBT_GetPER] NXTV Freqency change state! \n");
		return 0;	 
	}

	NXTV_GUARD_LOCK;
	
    NXTV_REG_MAP_SEL(FEC_PAGE);
	iPer = (NXTV_REG_GET(0x8d) << 16) | (NXTV_REG_GET(0x8e) << 8) | (NXTV_REG_GET(0x8f) << 0);

	NXTV_GUARD_FREE;

	return iPer;
}



/*===============================================================================
 * _nxtvISDBT_GetBER
 *
 * DESCRIPTION : 
 *		This function returns 
 *		
 *
 * ARGUMENTS : none.
 *
 * RETURN VALUE : 
 *		U32
 * NOTE: Must divied 1000 when using value.
 *============================================================================*/
U32 nxtvISDBT_GetBER(void)
{
	U32 iBer, iBerPer;

	if(g_fRtvChannelChange_1SEG) 
	{
		NXTV_DBGMSG0("[nxtvISDBT_GetBER] NXTV Freqency change state! \n"); 
		return 0;	 
	}

	NXTV_GUARD_LOCK;
    
	NXTV_REG_MAP_SEL(FEC_PAGE);
	iBerPer = ((NXTV_REG_GET(0x73) << 8) | (NXTV_REG_GET(0x74) << 0)) * 32 * 204 * 8;
		   
	iBer = (NXTV_REG_GET(0x87) << 16) | (NXTV_REG_GET(0x88) << 8) | (NXTV_REG_GET(0x89)<< 0);

	NXTV_GUARD_FREE;

	if(iBerPer == 0)
		return 0;
	else	
		return ((iBer * (U32)NXTV_ISDBT_BER_DIVIDER) / iBerPer);
}


U32 nxtvISDBT_GetCNR(void)
{
	U32 Mode;
	U32 LayerA;	
	U8 R_Data;
	U32 cn_val;
	U32 cn_a = 0;
	U32 cn_b = 0;
	S32 Constellation_val = 0;

    if(g_fRtvChannelChange_1SEG) 
    {
    	NXTV_DBGMSG0("[nxtvISDBT_GetCNR] NXTV Freqency change state! \n"); 
    	return 0;	
    }

	NXTV_GUARD_LOCK;
	
    NXTV_REG_MAP_SEL(OFDM_PAGE);
	R_Data = NXTV_REG_GET(0x80)|0x80;	
	NXTV_REG_SET(0x80, R_Data);
	
	isdbt_UpdateMonitoring();

	cn_val = (NXTV_REG_GET(0x89) + (NXTV_REG_GET(0x8a)<<8) 
		   + (NXTV_REG_GET(0x8b)<<16) + (NXTV_REG_GET(0x8c)<<24));

	Mode=(NXTV_REG_GET(0x81)>>2)&0x03;
	
	if(Mode ==MODE2)	cn_val = cn_val * 2;
	else if(Mode ==MODE1)	cn_val = cn_val * 4;
	
	NXTV_REG_MAP_SEL(FEC_PAGE);
	LayerA = NXTV_REG_GET(0x26) + ( (NXTV_REG_GET(0x27) & 0x1f) <<8);	

	NXTV_GUARD_FREE;
	
	Constellation_val = (LayerA >> 10) & 0x7;	


	if(Constellation_val == 1){
		///////////////////////// QPSK ////////////////////////
		if(cn_val > 305000){ 
		  cn_a = 0;
		  cn_b = 0;
		  return 0;
		}
		else if(cn_val > 289000) {	// 0~
		  cn_a = 0;
		  cn_b = (305000 - cn_val)/166;
		}
		else if(cn_val > 273000) {	// 1~
		  cn_a = 1;
		  cn_b = (289000 - cn_val)/166;
		}
		else if(cn_val > 254000) {	// 2~
		  cn_a = 2;
		  cn_b = (273000 - cn_val)/200;
		}
		else if(cn_val > 230500) {	// 3~
		  cn_a = 3;
		  cn_b = (254000 - cn_val)/233;
		}
		else if(cn_val > 205000) {	// 4~
		  cn_a = 4;
		  cn_b = (230500 - cn_val)/255;
		}
		else if(cn_val > 178000) {	// 5~
		  cn_a = 5;
		  cn_b = (205000 - cn_val)/277;
		}
		else if(cn_val > 152000) {	// 6~
		  cn_a = 6;
		  cn_b = (178000 - cn_val)/266;
		}
		else if(cn_val > 128000) {	// 7~
		  cn_a = 7;
		  cn_b = (152000 - cn_val)/250;
		}
		else if(cn_val > 104000) {	// 8~
		  cn_a = 8;
		  cn_b = (128000 - cn_val)/252;
		}
		else if(cn_val > 85500) {
		  cn_a = 9;
		  cn_b = (104000 - cn_val)/204;
		}
		else if(cn_val > 69200) {
		  cn_a = 10;
		  cn_b = (85500 - cn_val)/180;
		}
		else if(cn_val > 54500) {
		  cn_a = 11;
		  cn_b = (69200 - cn_val)/155;
		}
		else if(cn_val > 45000) {
		  cn_a = 12;
		  cn_b = (54500 - cn_val)/98;
		}
		else if(cn_val > 36400) {
		  cn_a = 13;
		  cn_b = (45000 - cn_val)/93;
		}
		else if(cn_val > 29700) {
		  cn_a = 14;
		  cn_b = (36400 - cn_val)/70;
		}
		else if(cn_val > 23750) {
		  cn_a = 15;
		  cn_b = (29700 - cn_val)/57;
		}
		else if(cn_val > 19350) {
		  cn_a = 16;
		  cn_b = (23750 - cn_val)/42;
		}
		else if(cn_val > 15750) {
		  cn_a = 17;
		  cn_b = (19350 - cn_val)/34;
		}
		else if(cn_val > 13050) {
		  cn_a = 18;
		  cn_b = (15750 - cn_val)/25;
		}
		else if(cn_val > 10920) {
		  cn_a = 19;
		  cn_b = (13050 - cn_val)/21;
		}
		else if(cn_val > 9250) {
		  cn_a = 20;
		  cn_b = (10920 - cn_val)/18;
		}
		else if(cn_val > 7800) {
		  cn_a = 21;
		  cn_b = (9250 - cn_val)/16;
		}
		else if(cn_val > 6770) {
		  cn_a = 22;
		  cn_b = (7800 - cn_val)/15;
		}
		else if(cn_val > 5830) {
		  cn_a = 23;
		  cn_b = (6770 - cn_val)/11;
		}
		else if(cn_val > 5140) {
		  cn_a = 24;
		  cn_b = (5830 - cn_val)/10;
		}
		else if(cn_val > 4620) {
		  cn_a = 25;
		  cn_b = (5140 - cn_val)/8;
		}
		else if(cn_val > 4190) {
		  cn_a = 26;
		  cn_b = (4620 - cn_val)/6;
		}
		else if(cn_val > 3830) {
		  cn_a = 27;
		  cn_b = (4190 - cn_val)/5;
		}
		else if(cn_val > 3550) {
		  cn_a = 28;
		  cn_b = (3830 - cn_val)/4;
		}
		else if(cn_val > 3320) {
		  cn_a = 29;
		  cn_b = (3550 - cn_val)/3;
		}
		else if(cn_val > 3160) {
		  cn_a = 30;
		  cn_b = (3320 - cn_val)/2;
		}
		else if(cn_val > 3005) {
		  cn_a = 31;
		  cn_b = (3160 - cn_val)/2;
		}
		else if(cn_val > 2890) {
		  cn_a = 32;
		  cn_b = (3005 - cn_val)/2;
		}
		else if(cn_val > 2840) {
		  cn_a = 33;
		  cn_b = (2890 - cn_val)/2;
		}
		else if(cn_val > 2750) {
		  cn_a = 34;
		  cn_b = (2840 - cn_val)/2;
		}
		else if(cn_val > 2680) {
		  cn_a = 35;
		  cn_b = (2750 - cn_val)/2;
		}
		else if(cn_val >  0) {	
		  cn_a = 36;
		  cn_b = (2680 - cn_val)/2;
		}
	}
	else if(Constellation_val == 2){
		///////////////////////// 16 QAM  ////////////////////////
		if(cn_val > 365000){ 
		  cn_a = 0;
		  cn_b = 0;
		}
		else if(cn_val > 353500) {	// 0~
		  cn_a = 0;
		  cn_b = (365000 - cn_val)/124;
		}
		else if(cn_val > 344200) {	// 1~
		  cn_a = 1;
		  cn_b = (353500 - cn_val)/101;
		}
		else if(cn_val > 333200) {	// 2~
		  cn_a = 2;
		  cn_b = (344200 - cn_val)/120;
		}
		else if(cn_val > 325000) {	// 3~
		  cn_a = 3;
		  cn_b = (333200 - cn_val)/90;
		}
		else if(cn_val > 316700) {	// 4~
		  cn_a = 4;
		  cn_b = (325000 - cn_val)/91;
		}
		else if(cn_val > 308200) {	// 5~
		  cn_a = 5;
		  cn_b = (316700 - cn_val)/93;
		}
		else if(cn_val > 299000) {	// 6~
		  cn_a = 6;
		  cn_b = (308200 - cn_val)/98;
		}
		else if(cn_val > 288600) {	// 7~
		  cn_a = 7;
		  cn_b = (299000 - cn_val)/108;
		}
		else if(cn_val > 274200) {	// 8~
		  cn_a = 8;
		  cn_b = (288600 - cn_val)/144;
		}
		else if(cn_val > 257300) {
		  cn_a = 9;
		  cn_b = (274200 - cn_val)/166;
		}
		else if(cn_val > 236800) {
		  cn_a = 10;
		  cn_b = (257300 - cn_val)/205;
		}
		else if(cn_val > 213200) {
		  cn_a = 11;
		  cn_b = (236800 - cn_val)/244;
		}
		else if(cn_val > 190000) {
		  cn_a = 12;
		  cn_b = (213200 - cn_val)/235;
		}
		else if(cn_val > 163900) {
		  cn_a = 13;
		  cn_b = (190000 - cn_val)/266;
		}
		else if(cn_val > 138600) {
		  cn_a = 14;
		  cn_b = (163900 - cn_val)/264;
		}
		else if(cn_val > 116500) {
		  cn_a = 15;
		  cn_b = (138600 - cn_val)/224;
		}
		else if(cn_val > 96600) {
		  cn_a = 16;
		  cn_b = (116500 - cn_val)/205;
		}
		else if(cn_val > 79700) {
		  cn_a = 17;
		  cn_b = (96600 - cn_val)/173;
		}
		else if(cn_val > 66550) {
		  cn_a = 18;
		  cn_b = (79700 - cn_val)/135;
		}
		else if(cn_val > 55500) {
		  cn_a = 19;
		  cn_b = (66550 - cn_val)/113;
		}
		else if(cn_val > 46950) {
		  cn_a = 20;
		  cn_b = (55500 - cn_val)/88;
		}
		else if(cn_val > 40300) {
		  cn_a = 21;
		  cn_b = (46950 - cn_val)/71;
		}
		else if(cn_val > 35050) {
		  cn_a = 22;
		  cn_b = (40300 - cn_val)/56;
		}
		else if(cn_val > 30400) {
		  cn_a = 23;
		  cn_b = (35050 - cn_val)/47;
		}
		else if(cn_val > 27000) {
		  cn_a = 24;
		  cn_b = (30400 - cn_val)/36;
		}
		else if(cn_val > 24300) {
		  cn_a = 25;
		  cn_b = (27000 - cn_val)/29;
		}
		else if(cn_val > 21900) {
		  cn_a = 26;
		  cn_b = (24300 - cn_val)/25;
		}
		else if(cn_val > 20300) {
		  cn_a = 27;
		  cn_b = (21900 - cn_val)/16;
		}
		else if(cn_val > 19050) {
		  cn_a = 28;
		  cn_b = (20300 - cn_val)/14;
		}
		else if(cn_val > 17850) {
		  cn_a = 29;
		  cn_b = (19050 - cn_val)/13;
		}
		else if(cn_val > 17150) {
		  cn_a = 30;
		  cn_b = (17850 - cn_val)/7;
		}
		else if(cn_val > 16300) {
		  cn_a = 31;
		  cn_b = (17150 - cn_val)/9;
		}
		else if(cn_val > 15800) {
		  cn_a = 32;
		  cn_b = (16300 - cn_val)/5;
		}
		else if(cn_val > 15450) {
		  cn_a = 33;
		  cn_b = (15800 - cn_val)/4;
		}
		else if(cn_val > 15050) {
		  cn_a = 34;
		  cn_b = (15450 - cn_val)/4;
		}
		else if(cn_val > 14800) {
		  cn_a = 35;
		  cn_b = (15050 - cn_val)/3;
		}
		else if(cn_val >  0) {	
		  cn_a = 36;
		  cn_b = (14800 - cn_val)/3;
		}

	}
	else {	
		cn_a = 0;
		cn_b = 0;
		
		return 0;
	}
	
	// kko. To avoid float-pointing.
	if(cn_b>1000)
	{
		return ((cn_a*(U32)NXTV_ISDBT_CNR_DIVIDER) + cn_b);
	}	
	else if (cn_b>100)
	{
		return ((cn_a*(U32)NXTV_ISDBT_CNR_DIVIDER) + (cn_b*10));
	}
	else
	{
		return ((cn_a*(U32)NXTV_ISDBT_CNR_DIVIDER) + (cn_b*100));
	}
}

static UINT g_nIsdbtPrevAntennaLevel;
#define ISDBT_MAX_NUM_ANTENNA_LEVEL	7

UINT nxtvISDBT_GetAntennaLevel(U32 dwCNR)
{
	UINT nCurLevel = ISDBT_MAX_NUM_ANTENNA_LEVEL-1;
	UINT nPrevLevel = g_nIsdbtPrevAntennaLevel;
	static const UINT aAntLvlTbl[ISDBT_MAX_NUM_ANTENNA_LEVEL-1]
		= {15*NXTV_ISDBT_CNR_DIVIDER, /* 6 */
			12*NXTV_ISDBT_CNR_DIVIDER, /* 5 */
			11*NXTV_ISDBT_CNR_DIVIDER, /* 4 */
			9*NXTV_ISDBT_CNR_DIVIDER, /* 3 */
			7*NXTV_ISDBT_CNR_DIVIDER, /* 2 */
			4*NXTV_ISDBT_CNR_DIVIDER /* 1 */
			};

	do {
		if(dwCNR >= aAntLvlTbl[6-nCurLevel])
			break;
	} while(--nCurLevel != 0);

	if (nCurLevel != nPrevLevel) {
		if (nCurLevel < nPrevLevel)
			nPrevLevel--;
		else
			nPrevLevel++;

		g_nIsdbtPrevAntennaLevel = nPrevLevel;
	}

	return nPrevLevel;
}


void nxtvISDBT_GetTMCC(NXTV_ISDBT_TMCC_INFO *ptTmccInfo)
{
	U32 LayerA;
	U8 R_Data;
	U32 dwInterlv;

	if(ptTmccInfo == NULL)
	{
		NXTV_DBGMSG0("[nxtvISDBT_GetTMCC] NXTV Invalid buffer pointer!\n"); 
		return;
	}
		
    if(g_fRtvChannelChange_1SEG) 
    {
    	NXTV_DBGMSG0("[nxtvISDBT_GetTMCC] NXTV Freqency change state! \n");
    	return;	
    }

	NXTV_GUARD_LOCK;
    
    NXTV_REG_MAP_SEL(OFDM_PAGE);
	R_Data = NXTV_REG_GET(0x81);
	
	switch( R_Data&0x03 )
	{
		case 0 : ptTmccInfo->eGuard = NXTV_ISDBT_GUARD_1_32; break;
		case 1 : ptTmccInfo->eGuard = NXTV_ISDBT_GUARD_1_16; break;
		case 2 : ptTmccInfo->eGuard = NXTV_ISDBT_GUARD_1_8; break;
		case 3 : ptTmccInfo->eGuard = NXTV_ISDBT_GUARD_1_4; break;
	}

	switch( (R_Data>>2)&0x03 )
	{
		case 0 : ptTmccInfo->eTvMode = NXTV_ISDBT_MODE_3; break;
		case 1 : ptTmccInfo->eTvMode = NXTV_ISDBT_MODE_2; break;
		case 2 : ptTmccInfo->eTvMode = NXTV_ISDBT_MODE_1; break;
	}	

	NXTV_REG_MAP_SEL(FEC_PAGE);
	R_Data = NXTV_REG_GET(0x25);

	switch( (R_Data>>1)&0x01 ) 
	{
		case 0 : ptTmccInfo->eSeg = NXTV_ISDBT_SEG_3; break;
		case 1 : ptTmccInfo->eSeg = NXTV_ISDBT_SEG_1; break;
	}

	ptTmccInfo->fEWS = R_Data& 0x01;

	LayerA = NXTV_REG_GET(0x26) + ( (NXTV_REG_GET(0x27) & 0x1f) <<8);	
	switch( (LayerA >> 10) & 0x7 )
	{
		case 0 : ptTmccInfo->eModulation = NXTV_MOD_DQPSK; break;
		case 1 : ptTmccInfo->eModulation = NXTV_MOD_QPSK; break;
		case 2 : ptTmccInfo->eModulation = NXTV_MOD_16QAM; break;
		case 3 : ptTmccInfo->eModulation = NXTV_MOD_64QAM; break;
	}
	
	switch( (LayerA >> 7) & 0x7 )
	{
		case 0 : ptTmccInfo->eCodeRate = NXTV_CODE_RATE_1_2; break;
		case 1 : ptTmccInfo->eCodeRate = NXTV_CODE_RATE_2_3; break;
		case 2 : ptTmccInfo->eCodeRate = NXTV_CODE_RATE_3_4; break;
		case 3 : ptTmccInfo->eCodeRate = NXTV_CODE_RATE_5_6; break;
		case 4 : ptTmccInfo->eCodeRate = NXTV_CODE_RATE_7_8; break;
	}

	dwInterlv = (LayerA >> 4) & 0x7;		
	switch( ptTmccInfo->eTvMode )
	{
		case NXTV_ISDBT_MODE_3:
			switch( dwInterlv )
			{
				case 0 : ptTmccInfo->eInterlv = NXTV_ISDBT_INTERLV_0; break;
				case 1 : ptTmccInfo->eInterlv = NXTV_ISDBT_INTERLV_1; break;
				case 2 : ptTmccInfo->eInterlv = NXTV_ISDBT_INTERLV_2; break;
				case 3 : ptTmccInfo->eInterlv = NXTV_ISDBT_INTERLV_4; break;
				case 4 : ptTmccInfo->eInterlv = NXTV_ISDBT_INTERLV_8; break;
			}
			break;

		case NXTV_ISDBT_MODE_1:
			switch( dwInterlv )
			{
				case 0 : ptTmccInfo->eInterlv = NXTV_ISDBT_INTERLV_0; break;
				case 1 : ptTmccInfo->eInterlv = NXTV_ISDBT_INTERLV_4; break;
				case 2 : ptTmccInfo->eInterlv = NXTV_ISDBT_INTERLV_8; break;
				case 3 : ptTmccInfo->eInterlv = NXTV_ISDBT_INTERLV_16; break;
				case 4 : ptTmccInfo->eInterlv = NXTV_ISDBT_INTERLV_32; break;
			}
			break;

		case NXTV_ISDBT_MODE_2:
			switch( dwInterlv )
			{
				case 0 : ptTmccInfo->eInterlv = NXTV_ISDBT_INTERLV_0; break;
				case 1 : ptTmccInfo->eInterlv = NXTV_ISDBT_INTERLV_2; break;
				case 2 : ptTmccInfo->eInterlv = NXTV_ISDBT_INTERLV_4; break;
				case 3 : ptTmccInfo->eInterlv = NXTV_ISDBT_INTERLV_8; break;
				case 4 : ptTmccInfo->eInterlv = NXTV_ISDBT_INTERLV_16; break;
			}
			break;
	}	

	NXTV_GUARD_FREE;
}


INT nxtvISDBT_ScanFrequency(UINT nChNum) 
{
	UINT i=5;
	INT nRet;	
	U8 Lock_Status =0;
	int OFDM_Lock_cnt = 20, TMCC_Lock_cnt=0;
	U8 RD82, TMCC_Flag;
	U8 Mon_FSM=0;
    U32 val1, val2;
    U8 RD00;

	NXTV_GUARD_LOCK;

	g_fRtvChannelChange_1SEG = TRUE;
		
	NXTV_REG_MAP_SEL(RF_PAGE);
	NXTV_REG_SET(0x6b, 0x85);//RFAGC CLK DIV = 0x6b[7:4]

	if((nRet=nxtvRF_SetFrequency_1SEG(NXTV_TV_MODE_1SEG, nChNum, 0)) != NXTV_SUCCESS)
		goto ISDBT_SCAN_FREQ_EXIT;

    NXTV_DELAY_MS(20);
    
    NXTV_REG_MAP_SEL(RF_PAGE);
	RD00 = NXTV_REG_GET(0x00);
        
	NXTV_REG_MASK_SET(0x7e, 0xc0, (RD00&0x0c)<<4); // RFAGC_I2C[3:2] = 0x7e[7:6]	
	NXTV_REG_MASK_SET(0x7f, 0xc0, (RD00&0x03)<<6); // RFAGC_I2C[1:0] = 0x7f[7:6]
	NXTV_REG_MASK_SET(0x80, 0xc0, (RD00&0x30)<<2); // LNAGAIN_I2C[1:0] = 0x80[7:6]
	
	NXTV_REG_MASK_SET(0x6a, 0x07, 0x07); // RFAGCSEL = 0x6a[2:0]	
	
	NXTV_REG_SET(0x6b, 0xc5); //RFAGC CLK DIV = 0x6b[7:4]		
	    	
    isdbt_SetFastScanMode();
	do 
	{    
        NXTV_DELAY_MS(20);
		isdbt_UpdateMonitoring();
		Mon_FSM = NXTV_REG_GET(0x85) & 0x07;		
		val1 = (NXTV_REG_GET(0xe3)<<16) + (NXTV_REG_GET(0xe2)<<8) +  NXTV_REG_GET(0xe1);
		if(Mon_FSM >= 2) break;          
	} while(--i);	
	
	//NXTV_DBGMSG2("[nxtvISDBT_ScanFrequency] Power Value : %d, FSM Count :%d\n", val1, 5-i);
	 
	if((val1<=100000)&(val1>10000)) 
	{
		isdbt_SetFastScanMode(); 
		i = 5;
		do
		{
			NXTV_DELAY_MS(20);
			isdbt_UpdateMonitoring();
			Mon_FSM = NXTV_REG_GET(0x85) & 0x07; 
			val2 = (NXTV_REG_GET(0xe3)<<16) + (NXTV_REG_GET(0xe2)<<8) +  NXTV_REG_GET(0xe1);
			if(Mon_FSM >=2) break;
		} while(--i);

		val1 = (val1 + val2)>>1;	
		//NXTV_DBGMSG3("[nxtvISDBT_ScanFrequency] Average Power Value : %d,  Power Value2 : %d, FSM Count :%d\n", val1,val2, 5-i);	
	}	
	
    if(val1 > 40000)
    {       
    	isdbt_InitDemod(); 

		NXTV_REG_MAP_SEL(RF_PAGE);
        NXTV_REG_MASK_SET(0x6a, 0x07, 0x00); // RFAGCSEL = 0x6a[2:0]

		for(OFDM_Lock_cnt = 0; OFDM_Lock_cnt < 20 ;OFDM_Lock_cnt ++ )
		{
			NXTV_DELAY_MS(100);
			isdbt_UpdateMonitoring();
			
			NXTV_REG_MAP_SEL(OFDM_PAGE);
			Lock_Status=NXTV_REG_GET(0x81)>>4;
			if(Lock_Status==0x03)
			{
				for(TMCC_Lock_cnt = 0 ; TMCC_Lock_cnt <20;TMCC_Lock_cnt++)
				{
					NXTV_DELAY_MS(100);  // delay 100ms
					isdbt_UpdateMonitoring();
					
					NXTV_REG_MAP_SEL(FEC_PAGE);
					RD82 = NXTV_REG_GET(0x82);
					TMCC_Flag = RD82>>5;
					if(TMCC_Flag) 
					{
						nRet = NXTV_SUCCESS; // Channel Exist
						goto ISDBT_SCAN_FREQ_EXIT; 
					}
				}
				
				if(TMCC_Lock_cnt >= 19)
				{
					nRet = NXTV_CHANNEL_NOT_DETECTED;
					goto ISDBT_SCAN_FREQ_EXIT;
				}
			}					
		}
	}
	else
	{	
		NXTV_REG_MAP_SEL(OFDM_PAGE);
		NXTV_REG_SET(0x23, 0x2c);
		
		NXTV_REG_MAP_SEL(RF_PAGE);
        NXTV_REG_MASK_SET(0x6a, 0x07, 0x00); // RFAGCSEL = 0x6a[2:0]
        
        isdbt_StartOFDM();
	}		

	nRet = NXTV_CHANNEL_NOT_DETECTED;

ISDBT_SCAN_FREQ_EXIT:

	g_fRtvChannelChange_1SEG = FALSE;

	NXTV_GUARD_FREE;
	
    NXTV_DBGMSG3("[nxtvISDBT_ScanFrequency] SCAN Result : %d, OFDM Lock Count :%d TMCC Lock Count  = %d\n", nRet, OFDM_Lock_cnt,TMCC_Lock_cnt);
	return nRet;
}


void nxtvISDBT_DisableStreamOut(void)
{
	NXTV_GUARD_LOCK;
	
	nxtv_StreamDisable(NXTV_TV_MODE_1SEG);

	NXTV_GUARD_FREE;
}

void nxtvISDBT_EnableStreamOut(void)
{
	NXTV_GUARD_LOCK;
	
	nxtv_StreamEnable();

	NXTV_GUARD_FREE;
}

INT nxtvISDBT_SetFrequency(UINT nChNum)
{
	INT nRet;

	NXTV_GUARD_LOCK;
	
	if(g_fIsdbtFastScanEnabled == TRUE)
		isdbt_InitDemod();
    
	nRet = nxtvRF_SetFrequency_1SEG(NXTV_TV_MODE_1SEG, nChNum, 0);

	NXTV_GUARD_FREE;
	
	return nRet;
}

void nxtvISDBT_SwReset(void)
{
	NXTV_GUARD_LOCK;

	NXTV_REG_MAP_SEL(OFDM_PAGE);
	NXTV_REG_SET(0x11,0x07);
	NXTV_REG_SET(0x11,0x06);

	NXTV_REG_MAP_SEL(FEC_PAGE);
	NXTV_REG_SET(0x10,0x01); 
	NXTV_REG_SET(0x10,0x00); 

	NXTV_GUARD_FREE;
}

INT nxtvISDBT_Initialize(E_NXTV_COUNTRY_BAND_TYPE eRtvCountryBandType, UINT nThresholdSize)
{
	INT nRet;

	switch( eRtvCountryBandType )
	{
		case NXTV_COUNTRY_BAND_JAPAN:
		case NXTV_COUNTRY_BAND_BRAZIL:
		case NXTV_COUNTRY_BAND_ARGENTINA: 
			break;
			
		default:
			return NXTV_INVAILD_COUNTRY_BAND;
	}
	g_eRtvCountryBandType_1SEG = eRtvCountryBandType;

	if((nThresholdSize % NXTV_TS_PACKET_SIZE) != 0) 
	{
		NXTV_DBGMSG0("[nxtvISDBT_Initialize] The size of TS data should 188 align!\n");
		return NXTV_INVAILD_THRESHOLD_SIZE;
	}

#if defined(NXTV_IF_SPI) || defined(NXTV_IF_EBI2)
	if(nThresholdSize > (NXTV_TS_PACKET_SIZE * 10))
	{
		NXTV_DBGMSG0("[nxtvISDBT_Initialize] Maximum TSP size must be less than (188 * 10) bytes\n");
		return NXTV_INVAILD_THRESHOLD_SIZE;
	}
#else
	/* TSIF */
	if(nThresholdSize > (NXTV_TS_PACKET_SIZE * 16))
	{
		NXTV_DBGMSG0("[nxtvISDBT_Initialize] Maximum TSP size must be less than (188 * 16) bytes\n");
		return NXTV_INVAILD_THRESHOLD_SIZE;
	}
#endif

	g_nRtvMscThresholdSize_1SEG = nThresholdSize;

	g_nIsdbtPrevAntennaLevel = 0;
	
	nRet = nxtv_InitSystem_1SEG(NXTV_TV_MODE_1SEG, NXTV_ADC_CLK_FREQ_8_MHz);
	if(nRet != NXTV_SUCCESS)
		return nRet;

	/* Must after nxtv_InitSystem_1SEG() */
	isdbt_InitDemod();

	nRet = nxtvRF_Initilize_1SEG(NXTV_TV_MODE_1SEG);
	if(nRet != NXTV_SUCCESS)
		return nRet;
 
	NXTV_DELAY_MS(100);

	NXTV_REG_MAP_SEL(RF_PAGE); 
	NXTV_REG_SET( 0x6b,  0xC5);

	return NXTV_SUCCESS;
}
#endif // NXTV_ISDBT_ENABLE


