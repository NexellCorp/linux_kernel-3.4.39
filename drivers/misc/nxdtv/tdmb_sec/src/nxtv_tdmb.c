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
* TITLE 	  : NEXELL TV T-DMB services source file. 
*
* FILENAME    : nxtv_tdmb.c
*
* DESCRIPTION : 
*		Library of routines to initialize, and operate on, the NEXELL T-DMB demod.
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

#include "nxtv_rf.h"
#if defined(NXTV_CIF_MODE_ENABLED) && defined(NXTV_BUILD_CIFDEC_WITH_DRIVER)
#include "nxtv_cif_dec.h"
#endif

#ifdef NXTV_TDMB_ENABLE

#undef OFDM_PAGE
#define OFDM_PAGE	0x6

#undef FEC_PAGE
#define FEC_PAGE	0x09

#if (defined(NXTV_IF_SERIAL_TSIF) || defined(NXTV_IF_SPI_SLAVE))\
&& defined(NXTV_CIF_MODE_ENABLED)
	/* Feature for NULL PID packet of error-tsp or not. */
	//#define NXTV_TDMB_NULL_PID_PKT_FOR_CIF_TSIF
#endif

#define MAX_NUM_TDMB_SUB_CH		64

/* Registered sub channel Table. */
typedef struct
{
	UINT 						nSubChID;
	UINT 						nHwSubChIdx;
	E_NXTV_SERVICE_TYPE 	eServiceType;	
	UINT						nThresholdSize;
} NXTV_TDMB_REG_SUBCH_INFO;


#if (NXTV_NUM_DAB_AVD_SERVICE == 1) /* Single Sub Channel */
	#define TDMB_MSC0_SUBCH_USE_MASK		0x00 /* NA */
	#define TDMB_MSC1_SUBCH_USE_MASK		0x01 /* SUBCH 0 */

#else /* Multi Sub Channel */
  #if defined(NXTV_IF_SPI) || defined(NXTV_IF_EBI2)
	#define TDMB_MSC0_SUBCH_USE_MASK		0x78 /* SUBCH 3,4,5,6 */
  #else
  	#define TDMB_MSC0_SUBCH_USE_MASK		0x70 /* SUBCH 3,4,5 */
  #endif
	#define TDMB_MSC1_SUBCH_USE_MASK		0x01 /* SUBCH 0 */
#endif


static NXTV_TDMB_REG_SUBCH_INFO g_atTdmbRegSubchInfo[NXTV_NUM_DAB_AVD_SERVICE];
static UINT g_nRegSubChArrayIdxBits;
static UINT g_nRtvUsedHwSubChIdxBits;
static U32 g_dwTdmbPrevChFreqKHz; 

#define DIV32(x)	((x) >> 5) // Divide by 32
#define MOD32(x)    ((x) & 31)
static U32 g_aRegSubChIdBits[2]; /* Used sub channel ID bits. [0]: 0 ~ 31, [1]: 32 ~ 63 */

static UINT g_nTdmbOpenedSubChNum;

/*==============================================================================
 * Replace the below code to eliminates the sqrt() and log10() functions.
 * In addtion, to eliminates floating operation, we multiplied by NXTV_TDMB_SNR_DIVIDER to the floating SNR.
 * SNR = (double)(100/(sqrt((double)data) - log10((double)data)*log10((double)data)) -7);
 *============================================================================*/
static const U16 g_awSNR_15_160[] = 
{
	33163/* 15 */, 32214/* 16 */, 31327/* 17 */, 30496/* 18 */, 29714/* 19 */, 
	28978/* 20 */, 28281/* 21 */, 27622/* 22 */, 26995/* 23 */, 26400/* 24 */, 
	25832/* 25 */, 25290/* 26 */, 24772/* 27 */, 24277/* 28 */, 23801/* 29 */, 
	23345/* 30 */, 22907/* 31 */, 22486/* 32 */, 22080/* 33 */, 21690/* 34 */, 
	21313/* 35 */, 20949/* 36 */, 20597/* 37 */, 20257/* 38 */, 19928/* 39 */, 
	19610/* 40 */, 19301/* 41 */, 19002/* 42 */, 18712/* 43 */, 18430/* 44 */, 
	18156/* 45 */, 17890/* 46 */, 17632/* 47 */, 17380/* 48 */, 17135/* 49 */, 
	16897/* 50 */, 16665/* 51 */, 16438/* 52 */, 16218/* 53 */, 16002/* 54 */, 
	15792/* 55 */, 15587/* 56 */, 15387/* 57 */, 15192/* 58 */, 15001/* 59 */, 
	14814/* 60 */, 14631/* 61 */, 14453/* 62 */, 14278/* 63 */, 14107/* 64 */, 
	13939/* 65 */, 13775/* 66 */, 13615/* 67 */, 13457/* 68 */, 13303/* 69 */, 
	13152/* 70 */, 13004/* 71 */, 12858/* 72 */, 12715/* 73 */, 12575/* 74 */, 
	12438/* 75 */, 12303/* 76 */, 12171/* 77 */, 12041/* 78 */, 11913/* 79 */, 
	11788/* 80 */, 11664/* 81 */, 11543/* 82 */, 11424/* 83 */, 11307/* 84 */, 
	11192/* 85 */, 11078/* 86 */, 10967/* 87 */, 10857/* 88 */, 10749/* 89 */, 
	10643/* 90 */, 10539/* 91 */, 10436/* 92 */, 10334/* 93 */, 10235/* 94 */, 
	10136/* 95 */, 10039/* 96 */, 9944/* 97 */, 9850/* 98 */, 9757/* 99 */, 
	9666/* 100 */, 9576/* 101 */, 9487/* 102 */, 9400/* 103 */, 9314/* 104 */, 
	9229/* 105 */, 9145/* 106 */, 9062/* 107 */, 8980/* 108 */, 8900/* 109 */, 
	8820/* 110 */, 8742/* 111 */, 8664/* 112 */, 8588/* 113 */, 8512/* 114 */, 
	8438/* 115 */, 8364/* 116 */, 8292/* 117 */, 8220/* 118 */, 8149/* 119 */, 
	8079/* 120 */, 8010/* 121 */, 7942/* 122 */, 7874/* 123 */, 7807/* 124 */, 
	7742/* 125 */, 7676/* 126 */, 7612/* 127 */, 7548/* 128 */, 7485/* 129 */, 
	7423/* 130 */, 7362/* 131 */, 7301/* 132 */, 7241/* 133 */, 7181/* 134 */, 
	7123/* 135 */, 7064/* 136 */, 7007/* 137 */, 6950/* 138 */, 6894/* 139 */, 
	6838/* 140 */, 6783/* 141 */, 6728/* 142 */, 6674/* 143 */, 6621/* 144 */, 
	6568/* 145 */, 6516/* 146 */, 6464/* 147 */, 6412/* 148 */, 6362/* 149 */, 
	6311/* 150 */, 6262/* 151 */, 6212/* 152 */, 6164/* 153 */, 6115/* 154 */, 
	6067/* 155 */, 6020/* 156 */, 5973/* 157 */, 5927/* 158 */, 5881/* 159 */, 
	5835/* 160 */
};



static void tdmb_InitTOP(void)
{
	NXTV_REG_MAP_SEL(OFDM_PAGE);
#if (NXTV_SRC_CLK_FREQ_KHz == 19200)
	NXTV_REG_SET(0x07, 0xF8);
#else
    NXTV_REG_SET(0x07, 0x08); 
#endif
	NXTV_REG_SET(0x05, 0x17); 
	NXTV_REG_SET(0x06, 0x10);	
	NXTV_REG_SET(0x0A, 0x00);   
}

//============================================================================
// Name    : tdmb_InitCOMM
// Action  : MAP SEL COMM Register Init
// Input   : Chip Address
// Output  : None
//============================================================================
static void tdmb_InitCOMM(void)
{
	NXTV_REG_MAP_SEL(COMM_PAGE);
	NXTV_REG_SET(0x10, 0x91);	  
	NXTV_REG_SET(0xE1, 0x00);

	NXTV_REG_SET(0x35, 0X8B);
	NXTV_REG_SET(0x3B, 0x3C);   

	NXTV_REG_SET(0x36, 0x67);   
	NXTV_REG_SET(0x3A, 0x0F);	   

	NXTV_REG_SET(0x3C,0x20); 
	NXTV_REG_SET(0x3D,0x0B);   
	NXTV_REG_SET(0x3D,0x09); 

#ifdef NXTV_TDMB_NULL_PID_PKT_FOR_CIF_TSIF
	// 0x30 ==>NO TSOUT@Error packet, 0x10 ==> NULL PID PACKET@Error packet
	NXTV_REG_SET(0xA6, 0x10);
#else
	NXTV_REG_SET(0xA6, 0x30);
#endif

	NXTV_REG_SET(0xAA, 0x01); // Enable 0x47 insertion to video frame.

	NXTV_REG_SET(0xAF, 0x07); // FEC
}

//============================================================================
// Name    : tdmb_InitHOST
// Action  : MAP SEL HOST Register Init
// Input   : Chip Address
// Output  : None
//============================================================================
static void tdmb_InitHOST(void)
{
	NXTV_REG_MAP_SEL(HOST_PAGE);
	NXTV_REG_SET(0x10, 0x00);
	NXTV_REG_SET(0x13,0x16);
	NXTV_REG_SET(0x14,0x00);

#if (NXTV_SRC_CLK_FREQ_KHz == 19200)
	NXTV_REG_SET(0x19,0x0B);
#else
    NXTV_REG_SET(0x19,0x0A);
#endif
	NXTV_REG_SET(0xF0,0x00);
	NXTV_REG_SET(0xF1,0x00);
	NXTV_REG_SET(0xF2,0x00);
	NXTV_REG_SET(0xF3,0x00);
	NXTV_REG_SET(0xF4,0x00);
	NXTV_REG_SET(0xF5,0x00);
	NXTV_REG_SET(0xF6,0x00);
	NXTV_REG_SET(0xF7,0x00);
	NXTV_REG_SET(0xF8,0x00);	
    NXTV_REG_SET(0xFB,0xFF);  

}


//============================================================================
// Name    : tdmb_InitOFDM
// Action  : MAP SEL OFDM Register Init
// Input   : Chip Address
// Output  : None
//============================================================================
static void tdmb_InitOFDM(void)
{
	U8 INV_MODE;       
	U8 PWM_COM;       
	U8 WAGC_COM;      
	U8 AGC_MODE;      
	U8 POST_INIT;     
	U8 AGC_CYCLE;     

	INV_MODE = 1;		
	PWM_COM = 0x08;
	WAGC_COM = 0x03;
	AGC_MODE = 0x06; 
	POST_INIT = 0x09;
	AGC_CYCLE = 0x10;

	NXTV_REG_MAP_SEL(OFDM_PAGE);

	if(g_eRtvCountryBandType_SEC == NXTV_COUNTRY_BAND_KOREA)
	{
		NXTV_REG_SET(0x11,0x8e); 
	}
	
	NXTV_REG_SET(0x12,0x04); 
	
	NXTV_REG_SET(0x13,0x72); 
	NXTV_REG_SET(0x14,0x63); 
	NXTV_REG_SET(0x15,0x64); 

	NXTV_REG_SET(0x16,0x6C); 

	NXTV_REG_SET(0x1A,0xB4); 

	NXTV_REG_SET(0x38,0x01);	

    NXTV_REG_SET(0x20,0x5B); 

    NXTV_REG_SET(0x25,0x09);

    NXTV_REG_SET(0x44,0x00 | (POST_INIT)); 

	NXTV_REG_SET(0x46,0xA0); 
	NXTV_REG_SET(0x47,0x0F);

	NXTV_REG_SET(0x48,0xB8); 
	NXTV_REG_SET(0x49,0x0B);  
	NXTV_REG_SET(0x54,0x58); 

	NXTV_REG_SET(0x55,0x06); 
	
	NXTV_REG_SET(0x56,0x00 | (AGC_CYCLE));         

	NXTV_REG_SET(0x59,0x51); 
                                            
	NXTV_REG_SET(0x5A,0x1C); 

	NXTV_REG_SET(0x6D,0x00); 
	NXTV_REG_SET(0x8B,0x34); 

	NXTV_REG_SET(0x6B,0x2D); 
	NXTV_REG_SET(0x85,0x32); 
	NXTV_REG_SET(0x8E,0x01); 

	NXTV_REG_SET(0x33, 0x00 | (INV_MODE<<1)); 
	NXTV_REG_SET(0x53,0x00 | (AGC_MODE)); 

	NXTV_REG_SET(0x6F,0x00 | (WAGC_COM)); 
	
	NXTV_REG_SET(0xBA,PWM_COM);

	switch( g_eRtvAdcClkFreqType_SEC )
	{
		case NXTV_ADC_CLK_FREQ_8_MHz: 
			NXTV_REG_MAP_SEL(COMM_PAGE);
			NXTV_REG_SET(0x6A,0x01); 
			   
			NXTV_REG_MAP_SEL(OFDM_PAGE);
			NXTV_REG_SET(0x3c,0x4B); 
			NXTV_REG_SET(0x3d,0x37); 
			NXTV_REG_SET(0x3e,0x89); 
			NXTV_REG_SET(0x3f,0x41);
			break;
			
		case NXTV_ADC_CLK_FREQ_8_192_MHz: 
			NXTV_REG_MAP_SEL(COMM_PAGE);
			NXTV_REG_SET(0x6A,0x01); 

			NXTV_REG_MAP_SEL(OFDM_PAGE);
			NXTV_REG_SET(0x3c,0x00); 
			NXTV_REG_SET(0x3d,0x00); 
			NXTV_REG_SET(0x3e,0x00); 
			NXTV_REG_SET(0x3f,0x40);
			break;
			
		case NXTV_ADC_CLK_FREQ_9_MHz: 
			NXTV_REG_MAP_SEL(COMM_PAGE);
			NXTV_REG_SET(0x6A,0x21); 

			NXTV_REG_MAP_SEL(OFDM_PAGE);
			NXTV_REG_SET(0x3c,0xB5); 
			NXTV_REG_SET(0x3d,0x14); 
			NXTV_REG_SET(0x3e,0x41); 
			NXTV_REG_SET(0x3f,0x3A);
			break;
			
		case NXTV_ADC_CLK_FREQ_9_6_MHz:
			NXTV_REG_MAP_SEL(COMM_PAGE);
			NXTV_REG_SET(0x6A,0x31); 

			NXTV_REG_MAP_SEL(OFDM_PAGE);
			NXTV_REG_SET(0x3c,0x69); 
			NXTV_REG_SET(0x3d,0x03); 
			NXTV_REG_SET(0x3e,0x9D); 
			NXTV_REG_SET(0x3f,0x36);
			break;
			
		default:
			NXTV_DBGMSG0("[tdmb_InitOFDM] Upsupport ADC clock type! \n");
			break;
	}
	
	NXTV_REG_SET(0x42,0x00); 
	NXTV_REG_SET(0x43,0x00); 

	NXTV_REG_SET(0x94,0x08); 

	NXTV_REG_SET(0x98,0x05); 
	NXTV_REG_SET(0x99,0x03); 
	NXTV_REG_SET(0x9B,0xCF); 
	NXTV_REG_SET(0x9C,0x10); 
	NXTV_REG_SET(0x9D,0x1C); 
	NXTV_REG_SET(0x9F,0x32); 
	NXTV_REG_SET(0xA0,0x90); 

	NXTV_REG_SET(0xA2,0xA0); 
	NXTV_REG_SET(0xA3,0x08);
	NXTV_REG_SET(0xA4,0x01); 

	NXTV_REG_SET(0xA8,0xF6); 
	NXTV_REG_SET(0xA9,0x89);
	NXTV_REG_SET(0xAA,0x0C); 
	NXTV_REG_SET(0xAB,0x32); 

	NXTV_REG_SET(0xAC,0x14); 
	NXTV_REG_SET(0xAD,0x09); 

	NXTV_REG_SET(0xAE,0xFF); 

    NXTV_REG_SET(0xEB,0x6B);

#ifdef NXTV_TII_PERIOD_1_FRAME
	NXTV_REG_SET(0x8D, 0x14);
	NXTV_REG_SET(0x8E, 0x00);
#endif
}

//============================================================================
// Name    : tdmb_InitFEC
// Action  : MAP SEL FEC Register Init
// Input   : Chip Address
// Output  : None
//============================================================================
static void tdmb_InitFEC(void)
{
	NXTV_REG_MAP_SEL(FEC_PAGE);

#if defined(NXTV_IF_SPI) || defined(NXTV_IF_EBI2) 
  #if (NXTV_NUM_DAB_AVD_SERVICE == 1) 
  	NXTV_REG_MASK_SET(0x7D, 0x10, 0x10); // 7KB memory use.
  #endif
#endif

	NXTV_REG_SET(0x80, 0x80);   
	NXTV_REG_SET(0x81, 0xFF);
#ifdef NXTV_TDMB_NULL_PID_PKT_FOR_CIF_TSIF
	NXTV_REG_SET(0x87, 0x00);
#else
	NXTV_REG_SET(0x87, 0x07);
#endif
	NXTV_REG_SET(0x45, 0xA1);
	NXTV_REG_SET(0xDD, 0xD0); 
	NXTV_REG_SET(0x39, 0x07);
	NXTV_REG_SET(0xE6, 0x00);
	NXTV_REG_SET(0xA5, 0xA0);
}


static void tdmb_InitDemod(void)
{
	tdmb_InitTOP();
	tdmb_InitCOMM();
	tdmb_InitHOST();
	tdmb_InitOFDM();
	tdmb_InitFEC();	

    nxtv_ResetMemory_FIC(); // Must disable before transmit con.
    
    /* Configure interrupt. */
	nxtvOEM_ConfigureInterrupt_SEC();

#ifdef NXTV_IF_TSIF
	nxtv_SetupThreshold_MSC1(188);
	nxtv_SetupMemory_MSC1(NXTV_TV_MODE_TDMB);
	#if (NXTV_MAX_NUM_DAB_DATA_SVC >= 1)
	nxtv_SetupThreshold_MSC0(188);
	nxtv_SetupMemory_MSC0(NXTV_TV_MODE_TDMB);
	#endif
#endif

	/* Configure CIF Header enable or disable for MSC0. */
#ifndef NXTV_CIF_MODE_ENABLED
	NXTV_REG_MAP_SEL(DD_PAGE);
	NXTV_REG_MASK_SET(0x31, 0x03, 0x00); // [0]:MSC0_HEAD_EN, [1]:MSC_HEAD_NBYTE : MSC0 Header OFF

#else /* CIF Header Enable */
   	NXTV_REG_MAP_SEL(DD_PAGE);
   #if defined(NXTV_IF_SPI) || defined(NXTV_IF_EBI2) 
	NXTV_REG_MASK_SET(0x31, 0x03, 0x03); // [0]:MSC0_HEAD_EN, [1]:MSC_HEAD_NBYTE	: MSC0 Header ON
   #else
	NXTV_REG_MASK_SET(0x31, 0x03, 0x00); // [0]:MSC0_HEAD_EN, [1]:MSC_HEAD_NBYTE : MSC0 Header OFF   
   #endif
#endif	

	/* Configure TSIF. */
#if defined(NXTV_IF_MPEG2_PARALLEL_TSIF)
  	nxtv_SetParallelTsif_TDMB_Only();

#elif defined(NXTV_IF_SERIAL_TSIF) || defined(NXTV_IF_SPI_SLAVE)
    nxtv_ConfigureTsifFormat(); 
   
	/* Configure TS memory and mode.
	[5] CIF_MODE_EN: TSI CIF transmit mode enable. 1 = CIF, 0 = Individual
	[4] MSC1_EN: MSC1 transmit enable
	[3] MSC0_EN: MSC0 transmit enable
	[2] FIC_EN: FIC transmit enable */
	NXTV_REG_MAP_SEL(COMM_PAGE);
  #ifndef NXTV_CIF_MODE_ENABLED /* Individual Mode */	
	NXTV_REG_SET(0x47, 0x1B|NXTV_COMM_CON47_CLK_SEL); //MSC0/MSC1 DD-TSI enable 
  #else  /* CIF Mode */ 
	NXTV_REG_SET(0x47, 0x3B|NXTV_COMM_CON47_CLK_SEL); // CIF/MSC0/MSC1 DD-TSI enable 

    NXTV_REG_MAP_SEL(DD_PAGE);
	NXTV_REG_SET(0xD6, 0xF4);
  #endif  
#endif		

	nxtv_ConfigureHostIF_SEC();
}


static void tdmb_SoftReset(void)
{
	NXTV_REG_MAP_SEL(OFDM_PAGE);
	NXTV_REG_SET(0x10, 0x48); // FEC reset enable
	NXTV_DELAY_MS(1);
	NXTV_REG_SET(0x10, 0xC9); // OFDM & FEC Soft reset
}


void nxtvTDMB_StandbyMode_SEC(int on)
{
	NXTV_GUARD_LOCK;
	
	if( on )
	{ 
		NXTV_REG_MAP_SEL(RF_PAGE); 
		NXTV_REG_MASK_SET(0x57,0x04, 0x04);  //SW PD ALL      
	}
	else
	{	  
		NXTV_REG_MAP_SEL(RF_PAGE); 
		NXTV_REG_MASK_SET(0x57,0x04, 0x00);  //SW PD ALL	
	}

	NXTV_GUARD_FREE;
}


UINT nxtvTDMB_GetLockStatus_SEC(void)
{
	U8 lock_stat;
	UINT lock_st = 0;
	
   	if(g_fRtvChannelChange_SEC) 
   	{
   		NXTV_DBGMSG0("[nxtvTDMB_GetLockStatus_SEC] NXTV Freqency change state! \n");	
		return 0x0;	 
	}

	NXTV_GUARD_LOCK;
		
    NXTV_REG_MAP_SEL(DD_PAGE);
	lock_stat = NXTV_REG_GET(0x37);	
	if( lock_stat & 0x01 )
        lock_st = NXTV_TDMB_OFDM_LOCK_MASK;

	lock_stat = NXTV_REG_GET(0xFB);	

	NXTV_GUARD_FREE;
	
	if((lock_stat & 0x03) == 0x03)
        lock_st |= NXTV_TDMB_FEC_LOCK_MASK;

	return lock_st;
}


U32 nxtvTDMB_GetPER_SEC(void)
{
    U8 rdata0, rdata1, rs_sync;

	if(g_fRtvChannelChange_SEC) 
	{
		NXTV_DBGMSG0("[nxtvTDMB_GetPER_SEC] NXTV Freqency change state! \n");
		return 0;	 
	}	

	NXTV_GUARD_LOCK;
	
   	NXTV_REG_MAP_SEL(FEC_PAGE);
	rdata0 = NXTV_REG_GET(0xD7);

	rs_sync = (rdata0 & 0x08) >> 3;
	if(rs_sync != 0x01)
	{
		NXTV_GUARD_FREE;
		return 0;//700;	
	}

	rdata1 = NXTV_REG_GET(0xB4);
	rdata0 = NXTV_REG_GET(0xB5);

	NXTV_GUARD_FREE;
	
	return  ((rdata1 << 8) | rdata0);
}


S32 nxtvTDMB_GetRSSI_SEC(void) 
{
	U8 RD00, GVBB, LNAGAIN, RFAGC;
	S32 nRssi = 0;
	
	if(g_fRtvChannelChange_SEC)
	{
		NXTV_DBGMSG0("[nxtvTDMB_GetRSSI_SEC] NXTV Freqency change state! \n");
		return 0;
	}	

	NXTV_GUARD_LOCK;

	NXTV_REG_MAP_SEL(RF_PAGE);
	RD00 = NXTV_REG_GET(0x00);
	GVBB = NXTV_REG_GET(0x05);
	
	NXTV_GUARD_FREE;

	LNAGAIN = ((RD00 & 0x30) >> 4);
	RFAGC = (RD00 & 0x0F);

	switch (LNAGAIN)
	{
	case 0:
		nRssi = -((RFAGC * (S32)(2.75*NXTV_TDMB_RSSI_DIVIDER) ) 
			+ (GVBB * (S32)(0.36*NXTV_TDMB_RSSI_DIVIDER))
			- (S32)(12*NXTV_TDMB_RSSI_DIVIDER));
		break;
	
	case 1:
		nRssi = -((RFAGC * (S32)(2.75*NXTV_TDMB_RSSI_DIVIDER))
			+ (GVBB * (S32)(0.36*NXTV_TDMB_RSSI_DIVIDER))
			+ (S32)(-2 * NXTV_TDMB_RSSI_DIVIDER));
		break;
	
	case 2:
		nRssi = -((RFAGC * (S32)(3*NXTV_TDMB_RSSI_DIVIDER))
			+ (GVBB * (S32)(0.365*NXTV_TDMB_RSSI_DIVIDER))
			+ (S32)(3*NXTV_TDMB_RSSI_DIVIDER));
		break;
	
	case 3:
		nRssi = -((RFAGC * (S32)(3*NXTV_TDMB_RSSI_DIVIDER))
			+ (GVBB * (S32)(0.5*NXTV_TDMB_RSSI_DIVIDER))
			+ (S32)(0.5*NXTV_TDMB_RSSI_DIVIDER));
		break;

	default:
		break;
	}

	if(((RD00&0xC0) == 0x40) && (GVBB > 123))
		nRssi += (S32)(0*NXTV_TDMB_RSSI_DIVIDER); 

	return  nRssi;
}


U32 nxtvTDMB_GetCNR_SEC(void)
{
	U8 data1=0, data2=0;
	U8 data=0;
	U32 SNR=0; 

	if(g_fRtvChannelChange_SEC) 
	{
		NXTV_DBGMSG0("[nxtvTDMB_GetCNR_SEC] NXTV Freqency change state! \n");
		return 0;	 
	}	

	NXTV_GUARD_LOCK;

	NXTV_REG_MAP_SEL(OFDM_PAGE); 

	NXTV_REG_SET(0x82, 0x01);	
	data1 = NXTV_REG_GET(0x7E);
	data2 = NXTV_REG_GET(0x7F);

	NXTV_GUARD_FREE;

	data = ((data2 & 0x1f) << 8) + data1;
	
	if(data == 0)
	{
		return 0;
	}
	else if((data > 0) && (data < 15))
	{
		SNR = (S32)(33 * NXTV_TDMB_CNR_DIVIDER);
	}
	else if((data >= 15) && (data <= 160))
	{
		SNR = g_awSNR_15_160[data-15];
	}
	else if(data > 160)
	{
		SNR = (S32)(5.44 * NXTV_TDMB_CNR_DIVIDER);
	}

	return SNR;
}


U32 nxtvTDMB_GetCER_SEC(void)
{
	U8 lock_stat, rcnt3=0, rcnt2=0, rcnt1=0, rcnt0=0;
	U32 cer_cnt, cer_period_cnt, ret_val;
#if defined(NXTV_IF_SERIAL_TSIF) || defined(NXTV_IF_SPI_SLAVE)
	U8 fec_sync;
#endif

	if(g_fRtvChannelChange_SEC) {
		NXTV_DBGMSG0("[nxtvTDMB_GetCER_SEC] NXTV Freqency change state! \n");
		return 2000;	 
	}	

	NXTV_GUARD_LOCK;
	
	NXTV_REG_MAP_SEL(FEC_PAGE);
	lock_stat = NXTV_REG_GET(0x37);	
	if( lock_stat & 0x01 ) {
		// MSC CER period counter for accumulation
		rcnt3 = NXTV_REG_GET(0x88);
		rcnt2 = NXTV_REG_GET(0x89);
		rcnt1 = NXTV_REG_GET(0x8A);
		rcnt0 = NXTV_REG_GET(0x8B);
		cer_period_cnt = (rcnt3 << 24) | (rcnt2 << 16) | (rcnt1 << 8) | rcnt0;
		
		rcnt3 = NXTV_REG_GET(0x8C);
		rcnt2 = NXTV_REG_GET(0x8D);
		rcnt1 = NXTV_REG_GET(0x8E);
		rcnt0 = NXTV_REG_GET(0x8F);
	}
	else
		cer_period_cnt = 0;

#if defined(NXTV_IF_SERIAL_TSIF) || defined(NXTV_IF_SPI_SLAVE)
	fec_sync = (NXTV_REG_GET(0xD7) >> 4) & 0x1;
#endif

	NXTV_GUARD_FREE;

	if(cer_period_cnt != 0) {
		cer_cnt = (rcnt3 << 24) | (rcnt2 << 16) | (rcnt1 << 8) | rcnt0;
		//NXTV_DBGMSG2("[nxtvTDMB_GetCER_SEC] cer_cnt: %u, cer_period_cnt: %u\n", cer_cnt, cer_period_cnt);

		ret_val = ((cer_cnt * 1000)/cer_period_cnt) * 10;
		if(ret_val > 1200)
			ret_val = 2000;

		if(ret_val <= 60)
			ret_val = 0;
	}
	else
		ret_val = 2000; // No service

#if defined(NXTV_IF_SERIAL_TSIF) || defined(NXTV_IF_SPI_SLAVE)
	if ((fec_sync == 0) || (ret_val == 2000)) {
		NXTV_GUARD_LOCK;
	#ifdef NXTV_TDMB_NULL_PID_PKT_FOR_CIF_TSIF
		NXTV_REG_SET(0x03, 0x06);
		NXTV_REG_SET(0x10, 0x48);

		NXTV_REG_SET(0x03, 0x09);
		NXTV_REG_SET(0x46, 0x14);

		NXTV_REG_SET(0x03, 0x04);
		NXTV_REG_SET(0x47, 0xe2);

		NXTV_REG_SET(0x03, 0x09);
		NXTV_REG_SET(0x46, 0x16);

		NXTV_REG_SET(0x03, 0x04);
		NXTV_REG_SET(0x47, 0xf7);

		NXTV_REG_SET(0x03, 0x06);
		NXTV_REG_SET(0x10, 0xc9);
	#else
		if (g_fRtvFicOpened_SEC == TRUE) {
			NXTV_REG_SET(0x03, 0x09);
			NXTV_REG_SET(0x46, 0x14);
			NXTV_REG_SET(0x46, 0x16);
		}
	
		NXTV_REG_SET(0x03, 0x06);
		NXTV_REG_SET(0x10, 0x49);
		NXTV_REG_SET(0x10, 0xC9);
	#endif
		NXTV_GUARD_FREE;
	}
#endif

	return ret_val;
}

U32 nxtvTDMB_GetBER_SEC(void)
{
	U8 rdata0=0, rdata1=0, rdata2=0;
	U8 rcnt0, rcnt1, rcnt2;
	U8 rs_sync;
	U32 val;
	U32 cnt;

	if(g_fRtvChannelChange_SEC) 
	{
		NXTV_DBGMSG0("[nxtvTDMB_GetBER_SEC] NXTV Freqency change state! \n");
		return NXTV_TDMB_BER_DIVIDER;	 
	}	

	NXTV_GUARD_LOCK;

	NXTV_REG_MAP_SEL(FEC_PAGE);
	rdata0 = NXTV_REG_GET(0xD7);

	rs_sync = (rdata0 & 0x08) >> 3;
	if(rs_sync != 0x01)
	{
		NXTV_GUARD_FREE;
		return NXTV_TDMB_BER_DIVIDER;;
	}
	
	rcnt2 = NXTV_REG_GET(0xA6);
	rcnt1 = NXTV_REG_GET(0xA7);
	rcnt0 = NXTV_REG_GET(0xA8);
	cnt = (rcnt2 << 16) | (rcnt1 << 8) | rcnt0;

	rdata2 = NXTV_REG_GET(0xA9);
	rdata1 = NXTV_REG_GET(0xAA);
	rdata0 = NXTV_REG_GET(0xAB);
	val = (rdata2 << 16) | (rdata1 << 8) | rdata0; // max 24 bit

	NXTV_GUARD_FREE;
	
	if(cnt == 0)
		return NXTV_TDMB_BER_DIVIDER;
	else
		return ((val * (U32)NXTV_TDMB_BER_DIVIDER) / cnt);
}


static UINT g_nTdmbPrevAntennaLevel;

#define TDMB_MAX_NUM_ANTENNA_LEVEL	7

UINT nxtvTDMB_GetAntennaLevel_SEC(U32 dwCER)
{
	UINT nCurLevel = 0;
	UINT nPrevLevel = g_nTdmbPrevAntennaLevel;
	static const UINT aAntLvlTbl[TDMB_MAX_NUM_ANTENNA_LEVEL]
		= {810, 700, 490, 400, 250, 180, 0};

	if(dwCER == 2000)
		return 0;

	do {
		if(dwCER >= aAntLvlTbl[nCurLevel]) /* Use equal for CER 0 */
			break;
	} while(++nCurLevel != TDMB_MAX_NUM_ANTENNA_LEVEL);

	if (nCurLevel != nPrevLevel) {
		if (nCurLevel < nPrevLevel)
			nPrevLevel--;
		else
			nPrevLevel++;

		g_nTdmbPrevAntennaLevel = nPrevLevel;
	}

	return nPrevLevel;
}

/* Because that TDMB has the sub channel, we checks the freq which new or the same when the changsing of channel */
U32 nxtvTDMB_GetPreviousFrequency_SEC(void)
{
	return g_dwTdmbPrevChFreqKHz;
}


// Interrupts are disabled for SPI
// TSIF stream disabled are for TSIF
static INT tdmb_CloseSubChannel(UINT nRegSubChArrayIdx)
{
	UINT nSubChID;
	INT nHwSubChIdx;

	if((g_nRegSubChArrayIdxBits & (1<<nRegSubChArrayIdx)) == 0)
		return NXTV_NOT_OPENED_SUB_CHANNEL_ID; // not opened! already closed!	

	nSubChID  = g_atTdmbRegSubchInfo[nRegSubChArrayIdx].nSubChID;
	nHwSubChIdx = g_atTdmbRegSubchInfo[nRegSubChArrayIdx].nHwSubChIdx;

	/* Delete a used sub channel index. */
	g_nRtvUsedHwSubChIdxBits &= ~(1<<nHwSubChIdx);

//printk("[tdmb_CloseSubChannel] g_fRtvFicOpened_SEC: %d, g_nRtvFicOpenedStatePath: %d",
//	g_fRtvFicOpened_SEC, g_nRtvFicOpenedStatePath);
	g_nTdmbOpenedSubChNum--;

	// Disable the specified SUB CH first.
#if (NXTV_NUM_DAB_AVD_SERVICE == 1) /* Single Sub Channel */
	nxtv_Set_MSC1_SUBCH0(nSubChID, FALSE, FALSE);

	#if defined(NXTV_IF_SPI) || defined(NXTV_IF_EBI2)
	NXTV_REG_MAP_SEL(HOST_PAGE);
	g_bRtvIntrMaskRegL_SEC |= MSC1_INTR_BITS;
	NXTV_REG_SET(0x62, g_bRtvIntrMaskRegL_SEC);
	#endif

#else /* Multi Sub Channel */
	switch( nHwSubChIdx ) {
	case 0: nxtv_Set_MSC1_SUBCH0(nSubChID, FALSE, FALSE); break;
	case 3: nxtv_Set_MSC0_SUBCH3(nSubChID, FALSE); break;
	case 4: nxtv_Set_MSC0_SUBCH4(nSubChID, FALSE); break;
	case 5: nxtv_Set_MSC0_SUBCH5(nSubChID, FALSE); break;
	case 6: nxtv_Set_MSC0_SUBCH6(nSubChID, FALSE); break;
	default: break;
	}

	if((g_nRtvUsedHwSubChIdxBits & TDMB_MSC1_SUBCH_USE_MASK) == 0)
	{
	#if defined(NXTV_IF_SPI) || defined(NXTV_IF_EBI2)
		NXTV_REG_MAP_SEL(HOST_PAGE); 
		g_bRtvIntrMaskRegL_SEC |= MSC1_INTR_BITS;
		NXTV_REG_SET(0x62, g_bRtvIntrMaskRegL_SEC);
	#endif
	}
	
	if((g_nRtvUsedHwSubChIdxBits & TDMB_MSC0_SUBCH_USE_MASK) == 0)
	{
	#if defined(NXTV_IF_SPI) || defined(NXTV_IF_EBI2)	
		NXTV_REG_MAP_SEL(HOST_PAGE); 
		g_bRtvIntrMaskRegL_SEC |= MSC0_INTR_BITS;
		NXTV_REG_SET(0x62, g_bRtvIntrMaskRegL_SEC);
	#endif
	}
#endif

#if defined(NXTV_IF_SPI) || defined(NXTV_IF_EBI2)
	#ifdef NXTV_CIF_MODE_ENABLED
	if (nHwSubChIdx != 0) { /* MSC0 only */
		nxtv_DeleteSpiCifSubChannelID_MSC0(nSubChID); /* for ISR */
		#ifdef NXTV_BUILD_CIFDEC_WITH_DRIVER
		nxtvCIFDEC_DeleteSubChannelID(nSubChID);
		#endif
	}
	#endif
#else /* TSIF */
	#if defined(NXTV_CIF_MODE_ENABLED) && defined(NXTV_BUILD_CIFDEC_WITH_DRIVER)
	nxtvCIFDEC_DeleteSubChannelID(nSubChID);
	#endif
#endif

#if defined(NXTV_CIF_MODE_ENABLED) && defined(NXTV_BUILD_CIFDEC_WITH_DRIVER)
	#if defined(NXTV_IF_SPI) || defined(NXTV_IF_EBI2)
	if ((g_nRtvUsedHwSubChIdxBits & TDMB_MSC0_SUBCH_USE_MASK) == 0)
		nxtvCIFDEC_Deinit(); // MSC0 Only
	#else
	if ((g_nTdmbOpenedSubChNum == 0) && (g_fRtvFicOpened_SEC == FALSE))
		nxtvCIFDEC_Deinit();
	#endif
#endif

	/* Delete a registered subch array index. */
	g_nRegSubChArrayIdxBits &= ~(1<<nRegSubChArrayIdx);

	/* Deregister a sub channel ID Bit. */
	g_aRegSubChIdBits[DIV32(nSubChID)] &= ~(1 << MOD32(nSubChID));

	return NXTV_SUCCESS;
}


#if (NXTV_NUM_DAB_AVD_SERVICE >= 2)
static void tdmb_CloseAllSubChannel(void)
{
	UINT i = 0;
	UINT nRegSubChArrayIdxBits = g_nRegSubChArrayIdxBits;
	
	while(nRegSubChArrayIdxBits != 0) {
		if(nRegSubChArrayIdxBits & 0x01)
			tdmb_CloseSubChannel(i);		

		nRegSubChArrayIdxBits >>= 1;
		i++;
	}
}
#endif


void nxtvTDMB_CloseAllSubChannels_SEC(void)
{
	NXTV_GUARD_LOCK;

#if (NXTV_NUM_DAB_AVD_SERVICE == 1) /* Single Sub Channel */
	tdmb_CloseSubChannel(0);
#elif (NXTV_NUM_DAB_AVD_SERVICE >= 2)
	tdmb_CloseAllSubChannel();
#endif	

	NXTV_GUARD_FREE;
}


INT nxtvTDMB_CloseSubChannel_SEC(UINT nSubChID)
{
	INT nRet = NXTV_SUCCESS;
#if (NXTV_NUM_DAB_AVD_SERVICE >= 2)
	UINT i = 0;
	UINT nRegSubChArrayIdxBits = g_nRegSubChArrayIdxBits;
#endif

	if(nSubChID > (MAX_NUM_TDMB_SUB_CH-1))
		return NXTV_INVAILD_SUB_CHANNEL_ID;

	NXTV_GUARD_LOCK;

#if (NXTV_NUM_DAB_AVD_SERVICE == 1) /* Single Sub Channel */
	nRet = tdmb_CloseSubChannel(0);

#elif (NXTV_NUM_DAB_AVD_SERVICE >= 2)
	while(nRegSubChArrayIdxBits != 0) {
		if(nRegSubChArrayIdxBits & 0x01) {
			if(nSubChID == g_atTdmbRegSubchInfo[i].nSubChID)
				tdmb_CloseSubChannel(i);		
		}		

		nRegSubChArrayIdxBits >>= 1;
		i++;
	}
#endif

	NXTV_GUARD_FREE;
			
	return nRet;
}



static void tdmb_OpenSubChannel(UINT nSubChID, E_NXTV_SERVICE_TYPE eServiceType, UINT nThresholdSize)
{
	INT nHwSubChIdx;
	UINT i = 0;

	if (g_nTdmbOpenedSubChNum == 0) { /* The first open */
#if defined(NXTV_IF_SERIAL_TSIF) || defined(NXTV_IF_SPI_SLAVE)
		if (g_fRtvFicOpened_SEC == TRUE) {
			/* Adjust FIC state when FIC was opened prior to this functtion. */
		//	NXTV_DBGMSG1("[tdmb_OpenSubChannel] Closing with FIC_state_path(%d)\n", g_nRtvFicOpenedStatePath);
			nxtv_CloseFIC(0, g_nRtvFicOpenedStatePath);

			/* Update the opened state and path to use open FIC. */
			g_nRtvFicOpenedStatePath = nxtv_OpenFIC(1/* Force play state */);
		//	NXTV_DBGMSG1("[tdmb_OpenSubChannel] Opened with FIC_state_path(%d)\n", g_nRtvFicOpenedStatePath);
		}

	#if defined(NXTV_CIF_MODE_ENABLED) && defined(NXTV_BUILD_CIFDEC_WITH_DRIVER)
		nxtvCIFDEC_Init();
	#endif
#else /* SPI or EBI2 */
	#if defined(NXTV_CIF_MODE_ENABLED) && defined(NXTV_BUILD_CIFDEC_WITH_DRIVER)
		if (eServiceType == NXTV_SERVICE_DATA)
			nxtvCIFDEC_Init(); /* Only MSC0 */
	#endif
#endif
	}

#if defined(NXTV_IF_SPI) || defined(NXTV_IF_EBI2)
	#ifdef NXTV_CIF_MODE_ENABLED
	if (eServiceType == NXTV_SERVICE_DATA) { /* MSC0 only */
		 nxtv_AddSpiCifSubChannelID_MSC0(nSubChID); /* for ISR */
		#ifdef NXTV_BUILD_CIFDEC_WITH_DRIVER
		nxtvCIFDEC_AddSubChannelID(nSubChID, eServiceType);
		#endif
	}
	#endif
#else /* TSIF */
	#if defined(NXTV_CIF_MODE_ENABLED) && defined(NXTV_BUILD_CIFDEC_WITH_DRIVER)
	nxtvCIFDEC_AddSubChannelID(nSubChID, eServiceType);
	#endif
#endif

#if (NXTV_NUM_DAB_AVD_SERVICE == 1) /* Single Subchannel */
	nHwSubChIdx = 0;		

	#ifndef NXTV_CIF_MODE_ENABLED
  	nxtv_SetupThreshold_MSC1(nThresholdSize);
	#endif

	#if defined(NXTV_IF_SPI) || defined(NXTV_IF_EBI2)	
	nxtv_SetupMemory_MSC1(NXTV_TV_MODE_TDMB);

  	NXTV_REG_MAP_SEL(DD_PAGE);  	
	NXTV_REG_SET(INT_E_UCLRL, MSC1_INTR_BITS); // MSC1 Interrupts status clear.

  	NXTV_REG_MAP_SEL(HOST_PAGE);
  	g_bRtvIntrMaskRegL_SEC &= ~(MSC1_INTR_BITS);
  	NXTV_REG_SET(0x62, g_bRtvIntrMaskRegL_SEC); /* Enable MSC1 interrupts. */ 
	#endif

	/* Set the sub-channel and enable MSC memory with the specified sub ID. */			
	if(eServiceType == NXTV_SERVICE_VIDEO)
		nxtv_Set_MSC1_SUBCH0(nSubChID, TRUE, TRUE); // RS enable
	else
		nxtv_Set_MSC1_SUBCH0(nSubChID, TRUE, FALSE); // RS Disable

#else 
	/* Multi sub channel enabled */	
	if((eServiceType == NXTV_SERVICE_VIDEO) || (eServiceType == NXTV_SERVICE_AUDIO))
	{
		nHwSubChIdx = 0;		

		if((g_nRtvUsedHwSubChIdxBits & TDMB_MSC1_SUBCH_USE_MASK) == 0)
		{	/* First enabled. */
		#ifndef NXTV_CIF_MODE_ENABLED
  			nxtv_SetupThreshold_MSC1(nThresholdSize);
  		#endif

		#if defined(NXTV_IF_SPI) || defined(NXTV_IF_EBI2)
			nxtv_SetupMemory_MSC1(NXTV_TV_MODE_TDMB);
			NXTV_REG_MAP_SEL(DD_PAGE);
			NXTV_REG_SET(INT_E_UCLRL, MSC1_INTR_BITS); // MSC1 Interrupts status clear.

			NXTV_REG_MAP_SEL(HOST_PAGE);	
			g_bRtvIntrMaskRegL_SEC &= ~(MSC1_INTR_BITS);
			NXTV_REG_SET(0x62, g_bRtvIntrMaskRegL_SEC); /* Enable MSC1 interrupts and restore FIC . */		
		#endif
		}

		if(eServiceType == NXTV_SERVICE_VIDEO)
			nxtv_Set_MSC1_SUBCH0(nSubChID, TRUE, TRUE); // RS enable
		else
			nxtv_Set_MSC1_SUBCH0(nSubChID, TRUE, FALSE); // RS disable
	}
	else /* Data */
	{
	#if defined(NXTV_IF_SPI) || defined(NXTV_IF_EBI2)
		/* Search an available SUBCH index for Audio/Data service. (3 ~ 6) */
		for(nHwSubChIdx=3/* MSC0 base */; nHwSubChIdx<=6; nHwSubChIdx++)
	#else
		for(nHwSubChIdx=3/* MSC0 base */; nHwSubChIdx<=5; nHwSubChIdx++)
	#endif
		{
			if((g_nRtvUsedHwSubChIdxBits & (1<<nHwSubChIdx)) == 0) 			
				break;			
		}			 	
		
		if((g_nRtvUsedHwSubChIdxBits & TDMB_MSC0_SUBCH_USE_MASK) == 0)
		{
		#ifndef NXTV_CIF_MODE_ENABLED
			nxtv_SetupThreshold_MSC0(nThresholdSize);
		#endif

	#if defined(NXTV_IF_SPI) || defined(NXTV_IF_EBI2)
			nxtv_SetupMemory_MSC0(NXTV_TV_MODE_TDMB);
			NXTV_REG_MAP_SEL(DD_PAGE);
			NXTV_REG_SET(INT_E_UCLRL, MSC0_INTR_BITS); // MSC0 Interrupt clear.			

			/* Enable MSC0 interrupts. */
			NXTV_REG_MAP_SEL(HOST_PAGE);	
			g_bRtvIntrMaskRegL_SEC &= ~(MSC0_INTR_BITS);
			NXTV_REG_SET(0x62, g_bRtvIntrMaskRegL_SEC);	 // restore FIC	
	#endif
		}

		/* Set sub channel. */ 
		switch(nHwSubChIdx) {
		case 3: nxtv_Set_MSC0_SUBCH3(nSubChID, TRUE); break;
		case 4: nxtv_Set_MSC0_SUBCH4(nSubChID, TRUE); break;
		case 5: nxtv_Set_MSC0_SUBCH5(nSubChID, TRUE); break;
	#if defined(NXTV_IF_SPI) || defined(NXTV_IF_EBI2)
		case 6: nxtv_Set_MSC0_SUBCH6(nSubChID, TRUE); break;
	#endif
		default: break;
		}
	}	
#endif 		

	/* To use when close .*/	
#if (NXTV_NUM_DAB_AVD_SERVICE >= 2)
	for(i=0; i<NXTV_NUM_DAB_AVD_SERVICE; i++)
	{
		if((g_nRegSubChArrayIdxBits & (1<<i)) == 0)		
		{
#else
	i = 0;
#endif		
			/* Register a array index of sub channel */
			g_nRegSubChArrayIdxBits |= (1<<i);
			
			g_atTdmbRegSubchInfo[i].nSubChID = nSubChID;

			/* Add the new sub channel index. */
			g_atTdmbRegSubchInfo[i].nHwSubChIdx  = nHwSubChIdx;
			g_atTdmbRegSubchInfo[i].eServiceType   = eServiceType;
			g_atTdmbRegSubchInfo[i].nThresholdSize = nThresholdSize;
#if (NXTV_NUM_DAB_AVD_SERVICE >= 2)			
			break;
		}
	}		
#endif	

	g_nTdmbOpenedSubChNum++;

	/* Add the HW sub channel index. */
	g_nRtvUsedHwSubChIdxBits |= (1<<nHwSubChIdx);	

	/* Register a new sub channel ID Bit. */
	g_aRegSubChIdBits[DIV32(nSubChID)] |= (1 << MOD32(nSubChID));

	//NXTV_DBGMSG2("[tdmb_OpenSubChannel] nSubChID(%d) use MSC_SUBCH%d\n", nSubChID, nHwSubChIdx);	
}


INT nxtvTDMB_OpenSubChannel_SEC(U32 dwChFreqKHz, UINT nSubChID, 
			E_NXTV_SERVICE_TYPE eServiceType, UINT nThresholdSize)
{
	INT nRet = NXTV_SUCCESS;
	
	if(nSubChID > (MAX_NUM_TDMB_SUB_CH-1))
		return NXTV_INVAILD_SUB_CHANNEL_ID;

	/* Check for threshold size. */
#ifndef NXTV_CIF_MODE_ENABLED /* Individual Mode */
   #if defined(NXTV_IF_SPI) || defined(NXTV_IF_EBI2)
	 if(nThresholdSize > (188*18))  
	 	return NXTV_INVAILD_THRESHOLD_SIZE;
   #endif
#endif

	/* Check the previous freq. */
	if(g_dwTdmbPrevChFreqKHz == dwChFreqKHz)
	{
		/* Is registerd sub ch ID? */
		if(g_aRegSubChIdBits[DIV32(nSubChID)] & (1<<MOD32(nSubChID)))
		{
			NXTV_GUARD_LOCK;
			nxtv_StreamRestore(NXTV_TV_MODE_TDMB);// To restore FIC stream.
			NXTV_GUARD_FREE;
		
			NXTV_DBGMSG1("[nxtvTDMB_OpenSubChannel_SEC] Already opened sub channed ID(%d)\n", nSubChID);
			
			return NXTV_ALREADY_OPENED_SUB_CHANNEL_ID;
		}

   #if (NXTV_NUM_DAB_AVD_SERVICE == 1)  /* Single Sub Channel Mode */
   		NXTV_GUARD_LOCK;
   		tdmb_CloseSubChannel(0); /* Max sub channel is 1. So, we close the previous sub ch. */
		NXTV_REG_MAP_SEL(OFDM_PAGE);
		NXTV_REG_SET(0x10, 0x48);
		NXTV_REG_SET(0x10, 0xC9);
   		tdmb_OpenSubChannel(nSubChID, eServiceType, nThresholdSize);
		NXTV_GUARD_FREE;
   #else
	/* Multi Sub Channel. */
		if((eServiceType == NXTV_SERVICE_VIDEO) || (eServiceType == NXTV_SERVICE_AUDIO))
		{
			/* Check if the SUBCH available for Video service ? */
			if((g_nRtvUsedHwSubChIdxBits & TDMB_MSC1_SUBCH_USE_MASK) == TDMB_MSC1_SUBCH_USE_MASK)
			{
				NXTV_GUARD_LOCK;
				nxtv_StreamRestore(NXTV_TV_MODE_TDMB);// To restore FIC stream.
				NXTV_GUARD_FREE;

				return NXTV_NO_MORE_SUB_CHANNEL; // Only 1 Video/Audio service. Close other video service.
			}
		}
		else /* Data */
		{
			/* Check if the SUBCH available for Audio/Data services for MSC0 ? */
			if((g_nRtvUsedHwSubChIdxBits & TDMB_MSC0_SUBCH_USE_MASK) == TDMB_MSC0_SUBCH_USE_MASK)
			{
				NXTV_GUARD_LOCK;
				nxtv_StreamRestore(NXTV_TV_MODE_TDMB);// To restore FIC stream.
				NXTV_GUARD_FREE;

				return NXTV_NO_MORE_SUB_CHANNEL; // 
			}
		}   

		NXTV_GUARD_LOCK;
		if (g_nTdmbOpenedSubChNum == 0) {
			NXTV_REG_MAP_SEL(OFDM_PAGE);
			NXTV_REG_SET(0x10, 0x48);
			NXTV_REG_SET(0x10, 0xC9);
		}
		tdmb_OpenSubChannel(nSubChID, eServiceType, nThresholdSize);    
		nxtv_StreamRestore(NXTV_TV_MODE_TDMB);// To restore FIC stream. This is NOT the first time.
		NXTV_GUARD_FREE;
   #endif
	}/* if(g_dwTdmbPrevChFreqKHz[RaonTvChipIdx] == dwChFreqKHz) */
	else
	{
		g_dwTdmbPrevChFreqKHz = dwChFreqKHz;

		g_fRtvChannelChange_SEC = TRUE; // To prevent get ber, cnr ...
		
		NXTV_GUARD_LOCK;

#if (NXTV_NUM_DAB_AVD_SERVICE == 1)
		tdmb_CloseSubChannel(0); // Cloes the previous sub channel because this channel is a new freq.
#elif (NXTV_NUM_DAB_AVD_SERVICE >= 2)	         
		tdmb_CloseAllSubChannel(); // Cloes the all sub channel because this channel is a new freq.	  
#endif	
				   
		nRet = nxtvRF_SetFrequency_SEC(NXTV_TV_MODE_TDMB, 0, dwChFreqKHz);

  		tdmb_OpenSubChannel(nSubChID, eServiceType, nThresholdSize);
	
		NXTV_GUARD_FREE;

		g_fRtvChannelChange_SEC = FALSE; 
	}

	return nRet;
}

INT nxtvTDMB_OpenSubChannelExt_SEC(U32 dwChFreqKHz, UINT nSubChID, 
			E_NXTV_SERVICE_TYPE eServiceType, UINT nThresholdSize)
{
	INT nRet = NXTV_SUCCESS;
	
	if(nSubChID > (MAX_NUM_TDMB_SUB_CH-1))
		return NXTV_INVAILD_SUB_CHANNEL_ID;

	/* Check for threshold size. */
#ifndef NXTV_CIF_MODE_ENABLED /* Individual Mode */
   #if defined(NXTV_IF_SPI) || defined(NXTV_IF_EBI2)
	 if(nThresholdSize > (188*18))  
	 	return NXTV_INVAILD_THRESHOLD_SIZE;
   #endif
#endif
	
	g_dwTdmbPrevChFreqKHz = dwChFreqKHz;

	g_fRtvChannelChange_SEC = TRUE; // To prevent get ber, cnr ...
	
	NXTV_GUARD_LOCK;

#if (NXTV_NUM_DAB_AVD_SERVICE == 1)
	tdmb_CloseSubChannel(0); // Cloes the previous sub channel because this channel is a new freq.
#elif (NXTV_NUM_DAB_AVD_SERVICE >= 2)	         
	tdmb_CloseAllSubChannel(); // Cloes the all sub channel because this channel is a new freq.	  
#endif
	NXTV_GUARD_FREE;

	nRet = nxtvTDMB_ScanFrequency_SEC(dwChFreqKHz);
	if (nRet == NXTV_SUCCESS) {
		NXTV_GUARD_LOCK;
		tdmb_OpenSubChannel(nSubChID, eServiceType, nThresholdSize);
		NXTV_GUARD_FREE;
	}

	g_fRtvChannelChange_SEC = FALSE; 

	return nRet;
}


/*
	nxtvTDMB_ReadFIC_SEC()
	
	This function reads a FIC data in manually. 
*/
INT nxtvTDMB_ReadFIC_SEC(U8 *pbBuf)
{
#ifdef NXTV_FIC_POLLING_MODE	
	U8 int_type_val1;	
	UINT cnt = 0;
	
	if(g_fRtvFicOpened_SEC == FALSE)
	{
		NXTV_DBGMSG0("[nxtvTDMB_ReadFIC_SEC] NOT OPEN FIC\n");
		return NXTV_NOT_OPENED_FIC;
	}

	NXTV_GUARD_LOCK;
	
	NXTV_REG_MAP_SEL(DD_PAGE);
	NXTV_REG_SET(INT_E_UCLRL, 0x01); // FIC interrupt status clear
	
	while(++cnt <= 408)
	{
		int_type_val1 = NXTV_REG_GET(INT_E_STATL);
		if(int_type_val1 & FIC_E_INT) // FIC interrupt
		{
//		    printk("FIC_E_INT occured!\n");

			NXTV_REG_MAP_SEL(FIC_PAGE);
	#if defined(NXTV_IF_TSIF) || defined(NXTV_IF_SPI_SLAVE)
			NXTV_REG_BURST_GET(0x10, pbBuf, 192);			
			NXTV_REG_BURST_GET(0x10, pbBuf+192, 192);	

	#elif defined(NXTV_IF_SPI) 
		#if defined(__KERNEL__) || defined(WINCE)
			#ifdef NXTV_IF_EBI2
			NXTV_REG_BURST_GET(0x10, pbBuf, 384);
			#else			
			NXTV_REG_BURST_GET(0x10, pbBuf, 384+1);
			#endif
		#else
			NXTV_REG_BURST_GET(0x10, pbBuf, 384);
		#endif
	#elif defined(NXTV_IF_EBI2)
		NXTV_REG_BURST_GET(0x10, pbBuf, 384);
	#endif	

			NXTV_REG_MAP_SEL(DD_PAGE);
			NXTV_REG_SET(INT_E_UCLRL, 0x01); // FIC interrupt status clear

			NXTV_GUARD_FREE;

			return 384; 
		}
		
		NXTV_DELAY_MS(12);
	} /* while() */	

	NXTV_GUARD_FREE;

	NXTV_DBGMSG0("[nxtvTDMB_ReadFIC_SEC] FIC read timeout\n");

	return 0;

#else
	NXTV_GUARD_LOCK; /* If the caller not is the ISR. */

	NXTV_REG_MAP_SEL(FIC_PAGE);
#if defined(NXTV_IF_TSIF) || defined(NXTV_IF_SPI_SLAVE)
	NXTV_REG_BURST_GET(0x10, pbBuf, 192);			
	NXTV_REG_BURST_GET(0x10, pbBuf+192, 192);	

#elif defined(NXTV_IF_SPI) 
	#if defined(__KERNEL__) || defined(WINCE)
	NXTV_REG_BURST_GET(0x10, pbBuf, 384+1);
	#else
	NXTV_REG_BURST_GET(0x10, pbBuf, 384);
	#endif
#endif	

	NXTV_REG_MAP_SEL(DD_PAGE);
	NXTV_REG_SET(INT_E_UCLRL, 0x01); // FIC interrupt status clear

	NXTV_GUARD_FREE;

	return 384;	
#endif		
}


void nxtvTDMB_CloseFIC_SEC(void)
{
	if(g_fRtvFicOpened_SEC == FALSE)
		return;

	NXTV_GUARD_LOCK;

#if defined(NXTV_IF_SERIAL_TSIF) || defined(NXTV_IF_SPI_SLAVE)
	//NXTV_DBGMSG1("[nxtvTDMB_CloseFIC_SEC] Closing with FIC_state_path(%d)\n", g_nRtvFicOpenedStatePath);
	nxtv_CloseFIC(g_nTdmbOpenedSubChNum, g_nRtvFicOpenedStatePath);

	/* To protect memory reset in nxtv_SetupMemory_FIC() */
	if ((g_nRtvFicOpenedStatePath == FIC_OPENED_PATH_TSIF_IN_SCAN)
	|| (g_nRtvFicOpenedStatePath == FIC_OPENED_PATH_TSIF_IN_PLAY))
		NXTV_DELAY_MS(NXTV_TS_STREAM_DISABLE_DELAY);

	#if defined(NXTV_CIF_MODE_ENABLED) && defined(NXTV_BUILD_CIFDEC_WITH_DRIVER)
	if (g_nTdmbOpenedSubChNum == 0) {
		if (g_nRtvFicOpenedStatePath == FIC_OPENED_PATH_TSIF_IN_PLAY)
			nxtvCIFDEC_Deinit(); /* From play state */
	}
	#endif
#else
	nxtv_CloseFIC(0, 0); /* Don't care */
#endif

	g_fRtvFicOpened_SEC = FALSE;

	NXTV_GUARD_FREE;
}


INT nxtvTDMB_OpenFIC_SEC(void)
{
	if (g_fRtvFicOpened_SEC == TRUE) {
		NXTV_DBGMSG0("[nxtvTDMB_OpenFIC_SEC] Must close FIC prior to opening FIC!\n");
		return NXTV_DUPLICATING_OPEN_FIC;
	}

	g_fRtvFicOpened_SEC = TRUE;

	NXTV_GUARD_LOCK;

#if defined(NXTV_IF_SERIAL_TSIF) || defined(NXTV_IF_SPI_SLAVE)
	g_nRtvFicOpenedStatePath = nxtv_OpenFIC(g_nTdmbOpenedSubChNum);
	//NXTV_DBGMSG1("[nxtvTDMB_OpenFIC_SEC] Opened with FIC_state_path(%d)\n", g_nRtvFicOpenedStatePath);
#else
	nxtv_OpenFIC(g_nTdmbOpenedSubChNum);
#endif

	NXTV_GUARD_FREE;

	return NXTV_SUCCESS;
}


/* When this function was called, all sub channel should closed to reduce scan-time !!!!! 
   FIC can enabled. Usally enabled.
 */
INT nxtvTDMB_ScanFrequency_SEC(U32 dwChFreqKHz)
{
	U8 scan_done, OFDM_L=0, ccnt = 0, NULL_C=0, SCV_C=0;
	U8 scan_pwr1=0, scan_pwr2=0, DAB_Mode=0xFF, DAB_Mode_Chk=0xFF;
	U8 pre_agc1=0, pre_agc2=0, pre_agc_mon=0, ASCV=0;
	INT scan_flag = 0;
	U16 SPower =0, PreGain=0, PreGainTH=0, PWR_TH = 0, ILoopTH =0; 
	U8 Cfreq_HTH = 0,Cfreq_LTH=0;
	U8 i=0,j=0, m=0;
	U8 varyLow=0,varyHigh=0;
	U16 varyMon=0;
	U8 MON_FSM=0, FsmCntChk=0;
	U8 test0=0,test1=0;
	U16 NullLengthMon=0;
	U16 fail = 0;
	U8  FecResetCh=0xff;
	U8 CoarseFreq=0xFF, NullTh=0xFF,NullChCnt=0;	
	U8 rdata0 =0, rdata1=0;
	U16 i_chk=0, q_chk=0;
	UINT nReTryCnt = 0;

	g_fRtvChannelChange_SEC = TRUE; // To prevent get ber, cnr ...

	NXTV_GUARD_LOCK;

	/* NOTE: When this rountine executed, all sub channel should closed 
			 and memory(MSC0, MSC1) interrupts are disabled. */
#if (NXTV_NUM_DAB_AVD_SERVICE == 1)
	tdmb_CloseSubChannel(0);
#elif (NXTV_NUM_DAB_AVD_SERVICE >= 2)
	tdmb_CloseAllSubChannel();	  
#endif	
	
	scan_flag = nxtvRF_SetFrequency_SEC(NXTV_TV_MODE_TDMB, 0, dwChFreqKHz);
	if(scan_flag != NXTV_SUCCESS)
		goto TDMB_SCAN_EXIT;

	NXTV_REG_MAP_SEL(OFDM_PAGE);
	NXTV_REG_SET( 0x54, 0x70); 

	tdmb_SoftReset();
		
	FecResetCh = 0xff;
	fail = 0xFFFF;

	while(1)
	{
		scan_flag = NXTV_CHANNEL_NOT_DETECTED;

		if(++nReTryCnt == 10000) /* Up to 400ms */
		{
			NXTV_DBGMSG0("[nxtvTDMB_ScanFrequency_SEC] Scan Timeout! \n");
			scan_flag = NXTV_CHANNEL_NOT_DETECTED;
			break;
		}
			
		NXTV_REG_MAP_SEL(OFDM_PAGE);
		scan_done = NXTV_REG_GET(0xCF); // Scan-done flag & scan-out flag check

		NXTV_REG_MAP_SEL(COMM_PAGE);		    // Scan-Power monitoring
		scan_pwr1 = NXTV_REG_GET(0x38);
		scan_pwr2 = NXTV_REG_GET(0x39);
		SPower = (scan_pwr2<<8)|scan_pwr1;

		NXTV_REG_MAP_SEL(OFDM_PAGE);

		if(scan_done != 0xff)
		{
			NULL_C = 0;
			SCV_C = 0;
			pre_agc_mon = NXTV_REG_GET(0x53);
			NXTV_REG_SET(0x53, (pre_agc_mon | 0x80));		// Pre-AGC Gain monitoring One-shot
			pre_agc1 = NXTV_REG_GET(0x66);
			pre_agc2 = NXTV_REG_GET(0x67);
			PreGain = (pre_agc2<<2)|(pre_agc1&0x03);

			DAB_Mode = NXTV_REG_GET(0x27);	// DAB TX Mode monitoring
			DAB_Mode = (DAB_Mode & 0x30)>>4;

			switch( g_eRtvAdcClkFreqType_SEC )
			{
				case NXTV_ADC_CLK_FREQ_8_MHz :
					PreGainTH = 405;					
					switch(DAB_Mode) // tr mode
					{
						case 0:
							PWR_TH = 30000;
							ILoopTH = 200;
							Cfreq_HTH = 206;
							Cfreq_LTH = 55;
							break;
						case 1:
							PWR_TH = 30000;
							ILoopTH = 180;
							Cfreq_HTH = 242;
							Cfreq_LTH = 14;
							break;
						case 2:
							PWR_TH = 1300;
							ILoopTH = 180;
							Cfreq_HTH = 248;
							Cfreq_LTH = 8;
							break;
						case 3:
							PWR_TH = 280;
							ILoopTH = 180;
							Cfreq_HTH = 230;
							Cfreq_LTH = 26;
							break;
						default:
							PWR_TH = 2400;
							ILoopTH = 200;
							Cfreq_HTH = 206;
							Cfreq_LTH = 55;
							break;
					}
					break;

				case NXTV_ADC_CLK_FREQ_8_192_MHz :
					PreGainTH = 405;
					
					switch(DAB_Mode)
					{
						case 0:
							PWR_TH = 30000;
							ILoopTH = 200;
							Cfreq_HTH = 206;
							Cfreq_LTH = 55;
							break;
						case 1:
							PWR_TH = 30000;
							ILoopTH = 180;
							Cfreq_HTH = 242;
							Cfreq_LTH = 14;
							break;
						case 2:
							PWR_TH = 1200;
							ILoopTH = 180;
							Cfreq_HTH = 248;
							Cfreq_LTH = 8;
							break;
						case 3:
							PWR_TH = 1900;
							ILoopTH = 180;
							Cfreq_HTH = 230;
							Cfreq_LTH = 26;
							break;
						default:
							PWR_TH = 1700;
							ILoopTH = 200;
							Cfreq_HTH = 206;
							Cfreq_LTH = 55;
							break;
					}				
					break;

				case NXTV_ADC_CLK_FREQ_9_MHz :
					PreGainTH = 380;
					switch(DAB_Mode)
					{
						case 0:
							PWR_TH = 30000;
							ILoopTH = 200;
							Cfreq_HTH = 206;
							Cfreq_LTH = 55;
							break;
						case 1:
							PWR_TH = 30000;
							ILoopTH = 180;
							Cfreq_HTH = 242;
							Cfreq_LTH = 14;
							break;
						case 2:
							PWR_TH = 1300;
							ILoopTH = 180;
							Cfreq_HTH = 248;
							Cfreq_LTH = 8;
							break;
						case 3:
							PWR_TH = 8000;
							ILoopTH = 180;
							Cfreq_HTH = 230;
							Cfreq_LTH = 26;
							break;
						default:
							PWR_TH = 8000;
							ILoopTH = 200;
							Cfreq_HTH = 206;
							Cfreq_LTH = 55;
							break;
					}
					break;

				case NXTV_ADC_CLK_FREQ_9_6_MHz :
					PreGainTH = 380;
					
					switch(DAB_Mode)
					{
		            	case 0:
							PWR_TH = 30000;
							ILoopTH = 200;
							Cfreq_HTH = 206;
							Cfreq_LTH = 55;
							break;
						case 1:
							PWR_TH = 30000;
							ILoopTH = 180;
							Cfreq_HTH = 242;
							Cfreq_LTH = 14;
							break;
						case 2:
							PWR_TH = 1300;
							ILoopTH = 180;
							Cfreq_HTH = 248;
							Cfreq_LTH = 8;
							break;
						case 3:
							PWR_TH = 8000;
							ILoopTH = 180;
							Cfreq_HTH = 230;
							Cfreq_LTH = 26;
							break;
						default:
							PWR_TH = 8000;
							ILoopTH = 200;
							Cfreq_HTH = 206;
							Cfreq_LTH = 55;
							break;
					}
				
					break;

				default:
					scan_flag = NXTV_UNSUPPORT_ADC_CLK;
					goto TDMB_SCAN_EXIT;
			}

			if(scan_done == 0x01)			 /* Not DAB signal channel */
			{
				scan_flag = NXTV_CHANNEL_NOT_DETECTED;
				fail = 0xEF01;	
				
				goto TDMB_SCAN_EXIT;		/* Auto-scan result return */
			}
			else if(scan_done == 0x03)	/* DAB signal channel */
			{
				NXTV_REG_MAP_SEL(OFDM_PAGE); 
				CoarseFreq = NXTV_REG_GET( 0x18);	/* coarse freq */
						
				if(g_eRtvCountryBandType_SEC == NXTV_COUNTRY_BAND_KOREA)
				{
					if(DAB_Mode > 0)   // Tr_mode detection miss for T-DMB [Only Tr_Mode 1ÀÎ °æ¿ì¿¡¸¸ »ç¿ë°¡´ÉÇÑ Á¶°Ç] 
					{
						scan_flag = NXTV_CHANNEL_NOT_DETECTED;
						fail = 0xE002;

						//NXTV_DBGMSG1("[_nxtvTDMB_ScanFrequency_DAB Fail] %d\n", DAB_Mode);
						goto TDMB_SCAN_EXIT;	 //Auto-scan result return  
					}
				}
			
				if((CoarseFreq < Cfreq_HTH) && (CoarseFreq  > Cfreq_LTH))
				{
					CoarseFreq = 0x33;					
					scan_flag = NXTV_CHANNEL_NOT_DETECTED;
					fail = 0xEF33;	
					goto TDMB_SCAN_EXIT;					
				}	

		#if 0
				///////////////////				
				// debug.. agc, lna gain, gvv,
				{
				U8 RD00, GVBB, LNAGAIN, RFAGC;

				NXTV_REG_MAP_SEL(RF_PAGE);
				RD00 = NXTV_REG_GET(0x00);
				GVBB = NXTV_REG_GET(0x05);
				LNAGAIN = ((RD00 & 0x30) >> 4);
				RFAGC = (RD00 & 0x0F);
				printk("SCAN: RD00(0x%02X), GVBB(0x%02X), LNAGAIN(0x%02X), RFAGC(0x%02X)\n",
						RD00, GVBB, LNAGAIN, RFAGC);

				NXTV_REG_MAP_SEL(OFDM_PAGE); // back
				}
		#endif

				if(SPower<PWR_TH)  /* Scan Power Threshold	*/
				{		
					scan_flag = NXTV_CHANNEL_NOT_DETECTED;
					fail = 0xEF03;	
					goto TDMB_SCAN_EXIT;  
				}
				else
				{
					if ((PreGain < PreGainTH)||(PreGain==0))   /* PreAGC Gain threshold check */
					{
						scan_flag = NXTV_CHANNEL_NOT_DETECTED;
						fail = 0xEF04;	 	
						goto TDMB_SCAN_EXIT;
					}
					else
					{
						for(m =0; m<16; m++)
						{
							NullTh = NXTV_REG_GET( 0x1C);
							NXTV_REG_SET( 0x1C, (NullTh | 0x10));	
							test0 = NXTV_REG_GET( 0x26);
							test1 = NXTV_REG_GET( 0x27);
							NullLengthMon = ((test1&0x0F)<<8)|test0;
							
							DAB_Mode_Chk = NXTV_REG_GET( 0x27);	 /* DAB TX Mode monitoring */
							DAB_Mode_Chk = (DAB_Mode_Chk & 0x30)>>4;				
							if(DAB_Mode != DAB_Mode_Chk)
							{
								scan_flag = NXTV_CHANNEL_NOT_DETECTED;
								fail = 0xE000;	
								goto TDMB_SCAN_EXIT; 
							}
							
							if((NullLengthMon == 0) || (NullLengthMon > 3000))
							{
								NullChCnt++;
							}
							if((NullChCnt > 10) && (m > 14)&& (PreGain < 400))	
							{
								scan_flag = NXTV_CHANNEL_NOT_DETECTED;
								fail = 0xEF05;	
								goto TDMB_SCAN_EXIT;							
							}
							else if(m>14)
							{
								fail = 0x1111;	
								scan_flag=NXTV_SUCCESS;
								break;
							}							

							ASCV = NXTV_REG_GET(0x30);  
							ASCV = ASCV & 0x0F;                
							if ((SCV_C > 0) && (ASCV > 7)) { 
								scan_flag = NXTV_CHANNEL_NOT_DETECTED;
								fail = 0xEF08;
								goto TDMB_SCAN_EXIT;
								/* Auto-scan result return */
							}
							if (ASCV > 7)
								SCV_C++;
							NXTV_DELAY_MS(10);	/* 10ms·Î ¸ÂÃçÁà¾ß ÇÔ */
						}
					}
					if(scan_flag == NXTV_SUCCESS)
					{						
						for(i=0; i</*ILoopTH*/100; i++)
						{
							NXTV_DELAY_MS(10);	/* 10ms·Î ¸ÂÃçÁà¾ß ÇÔ */
							
							NXTV_REG_MAP_SEL(OFDM_PAGE);
							ASCV = NXTV_REG_GET( 0x30);
							ASCV = ASCV&0x0F;											
							if((SCV_C > 0) && (ASCV > 7))		  // ASCV count
							{
								scan_flag = NXTV_CHANNEL_NOT_DETECTED;
								fail = 0xFF08;	
								goto TDMB_SCAN_EXIT;  /* Auto-scan result return */
							}								
							if(ASCV > 7)
								SCV_C++;		
							
							DAB_Mode_Chk = NXTV_REG_GET( 0x27);	 /* DAB TX Mode monitoring */
							DAB_Mode_Chk = (DAB_Mode_Chk & 0x30)>>4;				
							if(DAB_Mode != DAB_Mode_Chk)
							{
								scan_flag = NXTV_CHANNEL_NOT_DETECTED;
								fail = 0xF100;	
								goto TDMB_SCAN_EXIT; 
							}							

							NXTV_REG_MAP_SEL( COMM_PAGE); 
							NXTV_REG_MASK_SET(0x4D, 0x04, 0x00); 
							NXTV_REG_MASK_SET(0x4D, 0x04, 0x04); 
							rdata0 = NXTV_REG_GET( 0x4E);
							rdata1 = NXTV_REG_GET( 0x4F);
							i_chk = (rdata1 << 8) + rdata0;
							
							rdata0 = NXTV_REG_GET( 0x50);
							rdata1 = NXTV_REG_GET( 0x51);
							q_chk = (rdata1 << 8) + rdata0; 
							if((((i_chk>5) && (i_chk<65530)) || ((q_chk>5) && (q_chk<65530))) && (PreGain<500))
							{
								scan_flag = NXTV_CHANNEL_NOT_DETECTED;
								fail = 0xF200;	
								goto TDMB_SCAN_EXIT; 
							}

							/* //////////////////////// FSM Monitoring check//////////////////////////////// */
							NXTV_REG_MAP_SEL(OFDM_PAGE); 
							MON_FSM = NXTV_REG_GET( 0x37);
							MON_FSM = MON_FSM & 0x07;

							if((MON_FSM == 1) && (PreGain < 500))	
							{
								FsmCntChk++;
								if(NullChCnt > 14)
									FsmCntChk += 3;
							}
							if((MON_FSM == 1) && (FsmCntChk > 9) && (ccnt < 2))
							{
								scan_flag = NXTV_CHANNEL_NOT_DETECTED;									
								fail = 0xFF0A;	
								FsmCntChk = 0;
								
								goto TDMB_SCAN_EXIT;	/* Auto-scan result return */
							}	
							/* /////////////////////////////////////////////////////////////////////////////// */
							/* ///////////////////////// Coarse Freq. check/////////////////////////////////// */
							/* /////////////////////////////////////////////////////////////////////////////// */
							ccnt = NXTV_REG_GET( 0x17);	/* Coarse count check */
							ccnt &= 0x1F;
							if(ccnt > 1)
							{
								for(j=0;j<50;j++)
								{
									NXTV_DELAY_MS(10);	/* 5ms·Î ¸ÂÃçÁà¾ß ÇÔ */
									NXTV_REG_MAP_SEL( OFDM_PAGE);	
									OFDM_L = NXTV_REG_GET( 0x12);
									NXTV_REG_MASK_SET(0x82, 0x01, 0x01);	
									varyLow = NXTV_REG_GET( 0x7E);
									varyHigh = NXTV_REG_GET( 0x7F);			   
									varyMon = ((varyHigh & 0x1f) << 8) + varyLow;
									if((OFDM_L&0x80) && (varyMon > 0))
									{
										NXTV_REG_MAP_SEL(OFDM_PAGE);
										NXTV_REG_SET(0x54,0x58); 
										break;
									}
								}
								
								if(OFDM_L&0x80)
								{
									scan_flag = NXTV_SUCCESS;	   /* OFDM_Lock & FEC_Sync OK */
									fail = 0xFF7F;	
									goto TDMB_SCAN_EXIT;
								}
								else
								{
									scan_flag = NXTV_CHANNEL_NOT_DETECTED;	   /* OFDM_Unlock */
									fail = 0xFF0B;	
								}
								
								goto TDMB_SCAN_EXIT;						 /* Auto-scan result return */
							}
							else
							{
								scan_flag = NXTV_CHANNEL_NOT_DETECTED;	
							}
						}
						fail = 0xFF0C;	
						scan_flag = NXTV_CHANNEL_NOT_DETECTED;
						goto TDMB_SCAN_EXIT;
					}
				}
			}
		}		
		else
		{
			fail = 0xFF0D;	
			scan_flag = NXTV_CHANNEL_NOT_DETECTED;
			goto TDMB_SCAN_EXIT;
		}
	}

	fail = 0xFF0F;	
	
TDMB_SCAN_EXIT:

	NXTV_GUARD_FREE;
	
	g_fRtvChannelChange_SEC = FALSE; 

	g_dwTdmbPrevChFreqKHz = dwChFreqKHz;

	//NXTV_DBGMSG2("[nxtvTDMB_ScanFrequency_SEC] RF Freq = %d PreGain = %d ",dwChFreqKHz, PreGain);
	//NXTV_DBGMSG3(" 0x%04X SPower= %d CoarseFreq = %d ", fail,SPower, CoarseFreq);
	//NXTV_DBGMSG3("   m = %d i = %d j= %d\n",m,i,j);
	
	return scan_flag;	  /* Auto-scan result return */
}


void nxtvTDMB_DisableTiiInterrupt_SEC(void)
{
	NXTV_GUARD_LOCK;

  	NXTV_REG_MAP_SEL(HOST_PAGE);
  	NXTV_REG_MASK_SET(0x63, 0x40, 0x40);

	NXTV_GUARD_FREE;
}

void nxtvTDMB_EnableTiiInterrupt_SEC(void)
{
	NXTV_GUARD_LOCK;

  	NXTV_REG_MAP_SEL(HOST_PAGE);
  	NXTV_REG_MASK_SET(0x63, 0x40, 0x00);

	NXTV_GUARD_FREE;
}

void nxtvTDMB_GetTii_SEC(NXTV_TDMB_TII_INFO *ptTiiInfo)
{
	NXTV_GUARD_LOCK;

	nxtv_GetTii(ptTiiInfo);

	NXTV_GUARD_FREE;
}

INT nxtvTDMB_Initialize_SEC(E_NXTV_COUNTRY_BAND_TYPE eRtvCountryBandType)
{
	INT nRet;

	switch( eRtvCountryBandType )
	{
		case NXTV_COUNTRY_BAND_KOREA:
			break;
			
		default:
			return NXTV_INVAILD_COUNTRY_BAND;
	}
	g_eRtvCountryBandType_SEC = eRtvCountryBandType;

	g_nTdmbOpenedSubChNum = 0;
#if defined(NXTV_IF_SERIAL_TSIF) || defined(NXTV_IF_SPI_SLAVE)
	g_nRtvFicOpenedStatePath = FIC_NOT_OPENED;
#endif
	g_fRtvFicOpened_SEC = FALSE;	

	g_dwTdmbPrevChFreqKHz = 0;		
		
	g_nRtvUsedHwSubChIdxBits = 0x00;	

	g_nRegSubChArrayIdxBits = 0x00;
	
	g_aRegSubChIdBits[0] = 0x00000000;
	g_aRegSubChIdBits[1] = 0x00000000;

	g_nTdmbPrevAntennaLevel = 0;

	nRet = nxtv_InitSystem_SEC(NXTV_TV_MODE_TDMB, NXTV_ADC_CLK_FREQ_8_MHz);
	if(nRet != NXTV_SUCCESS)
		return nRet;

	/* Must after nxtv_InitSystem_SEC() to save ADC clock. */
	tdmb_InitDemod();
	
	nRet = nxtvRF_Initilize_SEC(NXTV_TV_MODE_TDMB);
	if(nRet != NXTV_SUCCESS)
		return nRet;

	NXTV_DELAY_MS(100);
	  
	NXTV_REG_MAP_SEL(RF_PAGE); 
	NXTV_REG_SET( 0x6b,  0xC5);

#ifdef NXTV_CIF_MODE_ENABLED
	NXTV_DBGMSG0("[nxtvTDMB_Initialize_SEC] CIF enabled\n");
#else
	NXTV_DBGMSG0("[nxtvTDMB_Initialize_SEC] SINGLE CHANNEL enabled\n");
#endif

	return NXTV_SUCCESS;
}

#endif /* #ifdef NXTV_TDMB_ENABLE */




