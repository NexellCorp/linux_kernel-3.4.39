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
* TITLE 	  : NEXELL TV DAB services source file. 
*
* FILENAME    : nxb110tv_dab.c
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

#include "nxb110tv_rf.h"
#if defined(RTV_CIF_MODE_ENABLED) && defined(RTV_BUILD_CIFDEC_WITH_DRIVER)
#include "nxb110tv_cif_dec.h"
#endif


#ifdef RTV_DAB_ENABLE

#undef OFDM_PAGE
#define OFDM_PAGE	0x6

#undef FEC_PAGE
#define FEC_PAGE	0x09

#if (defined(RTV_IF_SERIAL_TSIF) || defined(RTV_IF_SPI_SLAVE))\
&& defined(RTV_CIF_MODE_ENABLED)
	/* Feature for NULL PID packet of error-tsp or not. */
	//#define RTV_TDMB_NULL_PID_PKT_FOR_CIF_TSIF
#endif

#define MAX_NUM_DAB_SUB_CH		64


/* Registered sub channel Table. */
typedef struct
{
	UINT 			nSubChID;
	UINT 			nHwSubChIdx;
	E_RTV_SERVICE_TYPE	eServiceType;	
	UINT			nThresholdSize;
} RTV_DAB_REG_SUBCH_INFO;


#if (RTV_NUM_DAB_AVD_SERVICE == 1) /* Single Sub Channel */
	#define DAB_MSC0_SUBCH_USE_MASK		0x00 /* NA */
	#define DAB_MSC1_SUBCH_USE_MASK		0x01 /* SUBCH 0 */

#else /* Multi Sub Channel */
  #if defined(RTV_IF_SPI) || defined(RTV_IF_EBI2)
	#define DAB_MSC0_SUBCH_USE_MASK		0x78 /* SUBCH 3,4,5,6 */
#else
	#define DAB_MSC0_SUBCH_USE_MASK		0x70 /* SUBCH 3,4,5 */
#endif
		
	#define DAB_MSC1_SUBCH_USE_MASK		0x01 /* SUBCH 0 */
#endif


static RTV_DAB_REG_SUBCH_INFO g_atDabRegSubchInfo[RTV_NUM_DAB_AVD_SERVICE];
static UINT g_nDabRegSubChArrayIdxBits;
static UINT g_nDabRtvUsedHwSubChIdxBits;
static U32 g_dwDabPrevChFreqKHz; 
BOOL g_fDabEnableReconfigureIntr;

#define DIV32(x)	((x) >> 5) // Divide by 32
#define MOD32(x)    ((x) & 31)
static U32 g_aDabRegSubChIdBits[2]; /* Used sub channel ID bits. [0]: 0 ~ 31, [1]: 32 ~ 63 */
	
static UINT g_nDabOpenedSubChNum;
/*==============================================================================
 * Replace the below code to eliminates the sqrt() and log10() functions.
 * In addtion, to eliminates floating operation, we multiplied by RTV_dab_SNR_DIVIDER to the floating SNR.
 * SNR = (double)(100/(sqrt((double)data) - log10((double)data)*log10((double)data)) -7);
 *============================================================================*/
static const U16 g_DAB_awSNR_15_160[] = 
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



static void dab_InitTOP(void)
{
	RTV_REG_MAP_SEL(OFDM_PAGE);
    RTV_REG_SET(0x07, 0x08); 
	RTV_REG_SET(0x05, 0x17); 
	RTV_REG_SET(0x06, 0x10);	
	RTV_REG_SET(0x0A, 0x00);   
}

//============================================================================
// Name    : dab_InitCOMM
// Action  : MAP SEL COMM Register Init
// Input   : Chip Address
// Output  : None
//============================================================================
static void dab_InitCOMM(void)
{
	RTV_REG_MAP_SEL(COMM_PAGE);
	RTV_REG_SET(0x10, 0x91);	  
	RTV_REG_SET(0xE1, 0x00);

	RTV_REG_SET(0x35, 0x7B);
	RTV_REG_SET(0x3B, 0x3C);   

	RTV_REG_SET(0x36, 0x68);   
	RTV_REG_SET(0x3A, 0x44);

	RTV_REG_SET(0x3C,0x20); 
	RTV_REG_SET(0x3D,0x0B);   
	RTV_REG_SET(0x3D,0x09); 

#ifdef RTV_TDMB_NULL_PID_PKT_FOR_CIF_TSIF
	// 0x30 ==>NO TSOUT@Error packet, 0x10 ==> NULL PID PACKET@Error packet
	RTV_REG_SET(0xA6, 0x10);
#else
	RTV_REG_SET(0xA6, 0x30);
#endif

	RTV_REG_SET(0xAA, 0x01); // Enable 0x47 insertion to video frame.

	RTV_REG_SET(0xAF, 0x07); // FEC
}

//============================================================================
// Name    : dab_InitHOST
// Action  : MAP SEL HOST Register Init
// Input   : Chip Address
// Output  : None
//============================================================================
static void dab_InitHOST(void)
{
	RTV_REG_MAP_SEL(HOST_PAGE);
	RTV_REG_SET(0x10, 0x00);
	RTV_REG_SET(0x13,0x16);
	RTV_REG_SET(0x14,0x00);
	RTV_REG_SET(0x19,0x0A);
	RTV_REG_SET(0xF0,0x00);
	RTV_REG_SET(0xF1,0x00);
	RTV_REG_SET(0xF2,0x00);
	RTV_REG_SET(0xF3,0x00);
	RTV_REG_SET(0xF4,0x00);
	RTV_REG_SET(0xF5,0x00);
	RTV_REG_SET(0xF6,0x00);
	RTV_REG_SET(0xF7,0x00);
	RTV_REG_SET(0xF8,0x00);	
    RTV_REG_SET(0xFB,0xFF);
}


//============================================================================
// Name    : dab_InitOFDM
// Action  : MAP SEL OFDM Register Init
// Input   : Chip Address
// Output  : None
//============================================================================
static void dab_InitOFDM(void)
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

	RTV_REG_MAP_SEL(OFDM_PAGE);

	RTV_REG_SET(0x12,0x04);
	
	RTV_REG_SET(0x13,0x72); 
	RTV_REG_SET(0x14,0x63); 
	RTV_REG_SET(0x15,0x64); 

	RTV_REG_SET(0x16,0x6C); 

	RTV_REG_SET(0x38,0x01);	

	RTV_REG_SET(0x1A, 0xB4);
    RTV_REG_SET(0x20,0x5B);

    RTV_REG_SET(0x25,0x09);

    RTV_REG_SET(0x44,0x00 | (POST_INIT)); 

	RTV_REG_SET(0x46,0xA0); 
	RTV_REG_SET(0x47,0x0F);

	RTV_REG_SET(0x48,0xB8); 
	RTV_REG_SET(0x49,0x0B);  
	RTV_REG_SET(0x54,0x58); 

	RTV_REG_SET(0x55,0x06); 
	
	RTV_REG_SET(0x56,0x00 | (AGC_CYCLE));         

	RTV_REG_SET(0x59,0x51); 
                                            
	RTV_REG_SET(0x5A,0x1C); 

	RTV_REG_SET(0x6D,0x00); 
	RTV_REG_SET(0x8B,0x34); 

	RTV_REG_SET(0x6A,0x1C);
	RTV_REG_SET(0x6B,0x2D);

	RTV_REG_SET(0x85,0x32);

	RTV_REG_SET(0x8D,0x0C);
	RTV_REG_SET(0x8E,0x01); 

	RTV_REG_SET(0x33, 0x00 | (INV_MODE<<1)); 
	RTV_REG_SET(0x53,0x00 | (AGC_MODE)); 

	RTV_REG_SET(0x6F,0x00 | (WAGC_COM)); 
	
	RTV_REG_SET(0xBA,PWM_COM);

	switch( g_eRtvAdcClkFreqType )
	{
		case RTV_ADC_CLK_FREQ_8_MHz: 
			RTV_REG_MAP_SEL(COMM_PAGE);
			RTV_REG_SET(0x6A,0x01); 
			   
			RTV_REG_MAP_SEL(OFDM_PAGE);
			RTV_REG_SET(0x3c,0x4B); 
			RTV_REG_SET(0x3d,0x37); 
			RTV_REG_SET(0x3e,0x89); 
			RTV_REG_SET(0x3f,0x41);

			RTV_REG_SET(0x40,0x8F); //PNCO
			RTV_REG_SET(0x41,0xC2); //PNCO
			RTV_REG_SET(0x42,0xF5); //PNCO
			RTV_REG_SET(0x43,0x00); //PNCO
			break;
			
		case RTV_ADC_CLK_FREQ_8_192_MHz: 
			RTV_REG_MAP_SEL(COMM_PAGE);
			RTV_REG_SET(0x6A,0x01); 

			RTV_REG_MAP_SEL(OFDM_PAGE);
			RTV_REG_SET(0x3c,0x00); 
			RTV_REG_SET(0x3d,0x00); 
			RTV_REG_SET(0x3e,0x00); 
			RTV_REG_SET(0x3f,0x40);

			RTV_REG_SET(0x40,0x00); //PNCO
			RTV_REG_SET(0x41,0x00); //PNCO
			RTV_REG_SET(0x42,0xF0); //PNCO
			RTV_REG_SET(0x43,0x00); //PNCO
			break;
			
		case RTV_ADC_CLK_FREQ_9_MHz: 
			RTV_REG_MAP_SEL(COMM_PAGE);
			RTV_REG_SET(0x6A,0x21); 

			RTV_REG_MAP_SEL(OFDM_PAGE);
			RTV_REG_SET(0x3c,0xB5); 
			RTV_REG_SET(0x3d,0x14); 
			RTV_REG_SET(0x3e,0x41); 
			RTV_REG_SET(0x3f,0x3A);

			RTV_REG_SET(0x40,0x0D); //PNCO
			RTV_REG_SET(0x41,0x74); //PNCO
			RTV_REG_SET(0x42,0xDA); //PNCO
			RTV_REG_SET(0x43,0x00); //PNCO
			break;
			
		case RTV_ADC_CLK_FREQ_9_6_MHz:
			RTV_REG_MAP_SEL(COMM_PAGE);
			RTV_REG_SET(0x6A,0x31); 

			RTV_REG_MAP_SEL(OFDM_PAGE);
			RTV_REG_SET(0x3c,0x69); 
			RTV_REG_SET(0x3d,0x03); 
			RTV_REG_SET(0x3e,0x9D); 
			RTV_REG_SET(0x3f,0x36);

			RTV_REG_SET(0x40,0xCC); //PNCO
			RTV_REG_SET(0x41,0xCC); //PNCO
			RTV_REG_SET(0x42,0xCC); //PNCO
			RTV_REG_SET(0x43,0x00); //PNCO
			break;
			
		default:
			RTV_DBGMSG0("[dab_InitOFDM] Upsupport ADC clock type! \n");
			break;
	}
	
	RTV_REG_SET(0x94,0x08); 

	RTV_REG_SET(0x98,0x05); 
	RTV_REG_SET(0x99,0x03); 
	RTV_REG_SET(0x9B,0xCF); 
	RTV_REG_SET(0x9C,0x10); 
	RTV_REG_SET(0x9D,0x1C); 
	RTV_REG_SET(0x9F,0x32); 
	RTV_REG_SET(0xA0,0x90); 

	RTV_REG_SET(0xA2,0xA0); 
	RTV_REG_SET(0xA3,0x08);
	RTV_REG_SET(0xA4,0x01); 

	RTV_REG_SET(0xA8,0xF6); 
	RTV_REG_SET(0xA9,0x89);
	RTV_REG_SET(0xAA,0x0C); 
	RTV_REG_SET(0xAB,0x32); 

	RTV_REG_SET(0xAC,0x14); 
	RTV_REG_SET(0xAD,0x09); 

	RTV_REG_SET(0xAE,0xFF); 

    RTV_REG_SET(0xEB,0x6B);

	RTV_REG_SET(0x93,0x10);
	RTV_REG_SET(0x94,0x29);
	RTV_REG_SET(0xA2,0x50);
	RTV_REG_SET(0xA4,0x02);
	RTV_REG_SET(0xAF,0x01);
}

//============================================================================
// Name    : dab_InitFEC
// Action  : MAP SEL FEC Register Init
// Input   : Chip Address
// Output  : None
//============================================================================
static void dab_InitFEC(void)
{
	RTV_REG_MAP_SEL(FEC_PAGE);

#if defined(RTV_IF_SPI) || defined(RTV_IF_EBI2) 
  #if (RTV_NUM_DAB_AVD_SERVICE == 1) 
  	RTV_REG_MASK_SET(0x7D, 0x10, 0x10); // 7KB memory use.
  #endif
#endif

	RTV_REG_SET(0x80, 0x80);   
	RTV_REG_SET(0x81, 0xFF);
#ifdef RTV_TDMB_NULL_PID_PKT_FOR_CIF_TSIF
	RTV_REG_SET(0x87, 0x00);
#else
	RTV_REG_SET(0x87, 0x07);
#endif
	RTV_REG_SET(0x45, 0xA1);
	RTV_REG_SET(0xDD, 0xD0); 
	RTV_REG_SET(0x39, 0x07);
	RTV_REG_SET(0xE6, 0x10);    
	RTV_REG_SET(0xA5, 0xA0);
}


static void dab_InitDemod(void)
{
	dab_InitTOP();
	dab_InitCOMM();
	dab_InitHOST();
	dab_InitOFDM();
	dab_InitFEC();	

    rtv_ResetMemory_FIC(); // Must disable before transmit con.

    /* Configure interrupt. */
	rtvOEM_ConfigureInterrupt(); 

#ifdef RTV_IF_TSIF
	rtv_SetupThreshold_MSC1(4096);
	rtv_SetupMemory_MSC1(RTV_TV_MODE_DAB_B3);
	#if (RTV_MAX_NUM_DAB_DATA_SVC >= 1)
	rtv_SetupThreshold_MSC0(3072);
	rtv_SetupMemory_MSC0(RTV_TV_MODE_DAB_B3);
	#endif
#endif

	/* Configure CIF Header enable or disable for MSC0. */
#ifndef RTV_CIF_MODE_ENABLED
	RTV_REG_MAP_SEL(DD_PAGE);
	RTV_REG_MASK_SET(0x31, 0x03, 0x00); // [0]:MSC0_HEAD_EN, [1]:MSC_HEAD_NBYTE : MSC0 Header OFF

#else /* CIF Header Enable */
   	RTV_REG_MAP_SEL(DD_PAGE);
   #if defined(RTV_IF_SPI) || defined(RTV_IF_EBI2) 
	RTV_REG_MASK_SET(0x31, 0x03, 0x03); // [0]:MSC0_HEAD_EN, [1]:MSC_HEAD_NBYTE	: MSC0 Header ON
   #else
	RTV_REG_MASK_SET(0x31, 0x03, 0x00); // [0]:MSC0_HEAD_EN, [1]:MSC_HEAD_NBYTE : MSC0 Header OFF   
   #endif
#endif	

	/* Configure TSIF. */
#if defined(RTV_IF_SERIAL_TSIF) || defined(RTV_IF_SPI_SLAVE)
    rtv_ConfigureTsifFormat(); 
   
	/* Configure TS memory and mode.
	[5] CIF_MODE_EN: TSI CIF transmit mode enable. 1 = CIF, 0 = Individual
	[4] MSC1_EN: MSC1 transmit enable
	[3] MSC0_EN: MSC0 transmit enable
	[2] FIC_EN: FIC transmit enable */
	RTV_REG_MAP_SEL(COMM_PAGE);
  #ifndef RTV_CIF_MODE_ENABLED /* Individual Mode */	
	RTV_REG_SET(0x47, 0x1B|RTV_COMM_CON47_CLK_SEL); //MSC0/MSC1 DD-TSI enable 
  #else  /* CIF Mode */ 
	RTV_REG_SET(0x47, 0x3B|RTV_COMM_CON47_CLK_SEL); // CIF/MSC0/MSC1 DD-TSI enable 

    RTV_REG_MAP_SEL(DD_PAGE);
	RTV_REG_SET(0xD6, 0xF4);
  #endif
  
#elif defined(RTV_IF_MPEG2_PARALLEL_TSIF)
  	rtv_SetParallelTsif_TDMB_Only();
#endif		

	rtv_ConfigureHostIF();
}


static void dab_SoftReset(void)
{
	RTV_REG_MAP_SEL(OFDM_PAGE);
	RTV_REG_SET(0x10, 0x48); // FEC reset enable
	RTV_DELAY_MS(1);
	RTV_REG_SET(0x10, 0xC9); // OFDM & FEC Soft reset
}

void rtvDAB_StandbyMode(int on)
{
	RTV_GUARD_LOCK(demod_no);
	
	if( on )
	{ 
		RTV_REG_MAP_SEL(RF_PAGE); 
		RTV_REG_MASK_SET(0x57,0x04, 0x04);  //SW PD ALL      
	}
	else
	{	  
		RTV_REG_MAP_SEL(RF_PAGE); 
		RTV_REG_MASK_SET(0x57,0x04, 0x00);  //SW PD ALL	
	}

	RTV_GUARD_FREE(demod_no);
}


UINT rtvDAB_GetLockStatus(void)
{
	U8 lock_stat;
	UINT lock_st = 0;
	
   	if(g_fRtvChannelChange) 
   	{
   		RTV_DBGMSG0("[rtvdab_GetLockStatus] RTV Freqency change state! \n");	
		return 0x0;	 
	}

	RTV_GUARD_LOCK(demod_no);
		
    RTV_REG_MAP_SEL(DD_PAGE);
	lock_stat = RTV_REG_GET(0x37);
	if( lock_stat & 0x01 )
        lock_st = RTV_DAB_OFDM_LOCK_MASK;

	lock_stat = RTV_REG_GET(0xFB);	

	RTV_GUARD_FREE(demod_no);
	
	if((lock_stat & 0x03) == 0x03)
        lock_st |= RTV_DAB_FEC_LOCK_MASK;

	return lock_st;
}


U32 rtvDAB_GetPER(void)
{
    U8 rdata0, rdata1, rs_sync;

	if(g_fRtvChannelChange) 
	{
		RTV_DBGMSG0("[rtvdab_GetPER] RTV Freqency change state! \n");
		return 0;	 
	}	

	RTV_GUARD_LOCK(demod_no);
	
   	RTV_REG_MAP_SEL(FEC_PAGE);
	rdata0 = RTV_REG_GET(0xD7);

	rs_sync = (rdata0 & 0x08) >> 3;
	if(rs_sync != 0x01)
	{
		RTV_GUARD_FREE(demod_no);
		return 0;//700;	
	}

	rdata1 = RTV_REG_GET(0xB4);
	rdata0 = RTV_REG_GET(0xB5);

	RTV_GUARD_FREE(demod_no);
	
	return  ((rdata1 << 8) | rdata0);
}


S32 rtvDAB_GetRSSI(void) 
{
	U8 RD00, GVBB, LNAGAIN, RFAGC;
	S32 nRssi = 0;
	
	if(g_fRtvChannelChange) 
	{
		RTV_DBGMSG0("[rtvdab_GetRSSI] RTV Freqency change state! \n");
		return 0;	 
	}	

	RTV_GUARD_LOCK(demod_no);

	RTV_REG_MAP_SEL(RF_PAGE);	
	RD00 = RTV_REG_GET(0x00); 
	GVBB = RTV_REG_GET(0x05);

#ifdef RTV_DAB_LBAND_ENABLED
	if((g_curDabSetType ==RTV_TV_MODE_DAB_B3) || (g_curDabSetType ==RTV_TV_MODE_TDMB))
#endif		
	{
		RTV_GUARD_FREE(demod_no);
		
		LNAGAIN = ((RD00 & 0x30) >> 4);
		RFAGC = (RD00 & 0x0F);
		
		switch (LNAGAIN)
		{
		case 0:
			nRssi = -((RFAGC * (S32)(2.75*RTV_TDMB_RSSI_DIVIDER) ) 
				+ (GVBB * (S32)(0.36*RTV_TDMB_RSSI_DIVIDER))
				- (S32)(12*RTV_TDMB_RSSI_DIVIDER));
			break;
		
		case 1:
			nRssi = -((RFAGC * (S32)(2.75*RTV_TDMB_RSSI_DIVIDER))
				+ (GVBB *(S32)(0.36*RTV_TDMB_RSSI_DIVIDER))
				+ (S32)(-2*RTV_TDMB_RSSI_DIVIDER));
			break;
		
		case 2:
			nRssi = -((RFAGC * (S32)(3*RTV_TDMB_RSSI_DIVIDER))
				+ (GVBB * (S32)(0.365*RTV_TDMB_RSSI_DIVIDER))
				+ (S32)(3*RTV_TDMB_RSSI_DIVIDER));
			break;
		
		case 3:
			nRssi = -((RFAGC *(S32)(3*RTV_TDMB_RSSI_DIVIDER))
				+ (GVBB * (S32)(0.5*RTV_TDMB_RSSI_DIVIDER))
				+ (S32)(0.5*RTV_TDMB_RSSI_DIVIDER));
			break;
		
		default:
			break;
		}
		
		if(((RD00&0xC0) == 0x40) && ( GVBB > 123)) 
			nRssi += (S32)(0*RTV_TDMB_RSSI_DIVIDER); 
	}
#ifdef RTV_DAB_LBAND_ENABLED	
	else if(g_curDabSetType == RTV_TV_MODE_DAB_L)
	{

		nRssi = -( (((RD00 & 0x30) >> 4) * (S32)(12*RTV_DAB_RSSI_DIVIDER)) 
				+ ((RD00 & 0x0F) * (S32)(3*RTV_DAB_RSSI_DIVIDER))	
				+ (( (RTV_REG_GET(0x02) & 0x1E) >> 1 ) * (S32)(2.8*RTV_DAB_RSSI_DIVIDER)) 
				+ ((RTV_REG_GET(0x04) &0x7F) * (S32)(0.5*RTV_DAB_RSSI_DIVIDER) ) - (S32)(25*RTV_DAB_RSSI_DIVIDER) ); 

		RTV_GUARD_FREE(demod_no);
	}
#endif
	else 
		RTV_DBGMSG0("[rtvDAB_GetRSSI] g_curDabSetType error! \n");

	return  nRssi;
}


U32 rtvDAB_GetCNR(void)
{
	U8 data1=0, data2=0;
	U8 data=0;
	U32 SNR=0; 

	if(g_fRtvChannelChange) 
	{
		RTV_DBGMSG0("[rtvdab_GetCNR] RTV Freqency change state! \n");
		return 0;	 
	}	

	RTV_GUARD_LOCK(demod_no);

	RTV_REG_MAP_SEL(OFDM_PAGE); 

	RTV_REG_SET(0x82, 0x01);	
	data1 = RTV_REG_GET(0x7E);
	data2 = RTV_REG_GET(0x7F);

	RTV_GUARD_FREE(demod_no);

	data = ((data2 & 0x1f) << 8) + data1;
	
	if(data == 0)
	{
		return 0;
	}
	else if((data > 0) && (data < 15))
	{
		SNR = (S32)(33 * RTV_DAB_CNR_DIVIDER);
	}
	else if((data >= 15) && (data <= 160))
	{
		SNR = g_DAB_awSNR_15_160[data-15];
	}
	else if(data > 160)
	{
		SNR = (S32)(5.44 * RTV_DAB_CNR_DIVIDER);
	}

	return SNR;
}


U32 rtvDAB_GetCER(void)
{
	U8 lock_stat, rcnt3=0, rcnt2=0, rcnt1=0, rcnt0=0;
	U32 cer_cnt, cer_period_cnt, ret_val;
#if defined(RTV_IF_SERIAL_TSIF) || defined(RTV_IF_SPI_SLAVE)
	U8 fec_sync;
#endif

	if(g_fRtvChannelChange) 
	{
		RTV_DBGMSG0("[rtvDAB_GetCER] RTV Freqency change state! \n");
		return 2000;	 
	}	

	RTV_GUARD_LOCK(demod_no);
	
	RTV_REG_MAP_SEL(FEC_PAGE);

	lock_stat = RTV_REG_GET(0x37);	
	if( lock_stat & 0x01 ) {
		// MSC CER period counter for accumulation
		rcnt3 = RTV_REG_GET(0x88);
		rcnt2 = RTV_REG_GET(0x89);
		rcnt1 = RTV_REG_GET(0x8A);
		rcnt0 = RTV_REG_GET(0x8B);
		cer_period_cnt = (rcnt3 << 24) | (rcnt2 << 16) | (rcnt1 << 8) | rcnt0; // 442368
		
		rcnt3 = RTV_REG_GET(0x8C);
		rcnt2 = RTV_REG_GET(0x8D);
		rcnt1 = RTV_REG_GET(0x8E);
		rcnt0 = RTV_REG_GET(0x8F);
	}
	else
		cer_period_cnt = 0;

#if defined(RTV_IF_SERIAL_TSIF) || defined(RTV_IF_SPI_SLAVE)
	fec_sync = (RTV_REG_GET(0xD7) >> 4) & 0x1;
#endif

	RTV_GUARD_FREE(demod_no);

	if(cer_period_cnt != 0) {
		cer_cnt = (rcnt3 << 24) | (rcnt2 << 16) | (rcnt1 << 8) | rcnt0;
		//RTV_DBGMSG2("[rtvDAB_GetCER] cer_cnt: %u, cer_period_cnt: %u\n", cer_cnt, cer_period_cnt);
		if(cer_cnt <= 4000)
			ret_val = 0;
		else {
			ret_val = ((cer_cnt * 1000)/cer_period_cnt) * 10;
			if(ret_val > 1200)
				ret_val = 2000;
		}
	}
	else
		ret_val = 2000; // No service

#if defined(RTV_IF_SERIAL_TSIF) || defined(RTV_IF_SPI_SLAVE)
	if ((fec_sync == 0) || (ret_val == 2000)) {
		RTV_GUARD_LOCK(demod_no);
	#ifdef RTV_TDMB_NULL_PID_PKT_FOR_CIF_TSIF
		RTV_REG_SET(0x03, 0x06);
		RTV_REG_SET(0x10, 0x48);

		RTV_REG_SET(0x03, 0x09);
		RTV_REG_SET(0x46, 0x14);

		RTV_REG_SET(0x03, 0x04);
		RTV_REG_SET(0x47, 0xe2);

		RTV_REG_SET(0x03, 0x09);
		RTV_REG_SET(0x46, 0x16);

		RTV_REG_SET(0x03, 0x04);
		RTV_REG_SET(0x47, 0xf7);

		RTV_REG_SET(0x03, 0x06);
		RTV_REG_SET(0x10, 0xc9);
	#else
		RTV_REG_SET(0x03, 0x09);
		RTV_REG_SET(0x46, 0x1E);
		RTV_REG_SET(0x35, 0x01);
		RTV_REG_SET(0x46, 0x16);
	#endif
		RTV_GUARD_FREE(demod_no);
	}
#endif

	return ret_val;
}

U32 rtvDAB_GetBER(void)
{
	U8 rdata0=0, rdata1=0, rdata2=0;
	U8 rcnt0, rcnt1, rcnt2;
	U8 rs_sync;
	U32 val;
	U32 cnt;

	if(g_fRtvChannelChange) 
	{
		RTV_DBGMSG0("[rtvdab_GetBER] RTV Freqency change state! \n");
		return RTV_DAB_BER_DIVIDER;	 
	}	

	RTV_GUARD_LOCK(demod_no);

	RTV_REG_MAP_SEL(FEC_PAGE);
	rdata0 = RTV_REG_GET(0xD7);

	rs_sync = (rdata0 & 0x08) >> 3;
	if(rs_sync != 0x01)
	{
		RTV_GUARD_FREE(demod_no);
		return RTV_DAB_BER_DIVIDER;
	}
	
	rcnt2 = RTV_REG_GET(0xA6);
	rcnt1 = RTV_REG_GET(0xA7);
	rcnt0 = RTV_REG_GET(0xA8);
	cnt = (rcnt2 << 16) | (rcnt1 << 8) | rcnt0;

	rdata2 = RTV_REG_GET(0xA9);
	rdata1 = RTV_REG_GET(0xAA);
	rdata0 = RTV_REG_GET(0xAB);
	val = (rdata2 << 16) | (rdata1 << 8) | rdata0; // max 24 bit

	RTV_GUARD_FREE(demod_no);
	
	if(cnt == 0)
		return RTV_DAB_BER_DIVIDER;
	else
		return ((val * (U32)RTV_DAB_BER_DIVIDER) / cnt);
}


static UINT g_nDabPrevAntennaLevel;

#define DAB_MAX_NUM_ANTENNA_LEVEL	7

UINT rtvDAB_GetAntennaLevel(U32 dwCER)
{
	UINT nCurLevel = 0;
	UINT nPrevLevel = g_nDabPrevAntennaLevel;
	static const UINT aAntLvlTbl[DAB_MAX_NUM_ANTENNA_LEVEL]
		= {810, 700, 490, 400, 250, 180, 0};

	if(dwCER == 2000)
		return 0;
		
	do {
		if(dwCER >= aAntLvlTbl[nCurLevel]) /* Use equal for CER 0 */
			break;
	} while(++nCurLevel != DAB_MAX_NUM_ANTENNA_LEVEL);

	if (nCurLevel != nPrevLevel) {
		if (nCurLevel < nPrevLevel)
			nPrevLevel--;
		else
			nPrevLevel++;

		g_nDabPrevAntennaLevel = nPrevLevel;
	}

	return nPrevLevel;
}


/* Because that dab has the sub channel, we checks the freq which new or the same when the changsing of channel */
U32 rtvDAB_GetPreviousFrequency(void)
{
	return g_dwDabPrevChFreqKHz;
}

// Interrupts are disabled for SPI
// TSIF stream disabled are for TSIF
static INT dab_CloseSubChannel(UINT nRegSubChArrayIdx)
{
	UINT nSubChID;
	INT nHwSubChIdx;

	if((g_nDabRegSubChArrayIdxBits & (1<<nRegSubChArrayIdx)) == 0)
		return RTV_NOT_OPENED_SUB_CHANNEL_ID; // not opened! already closed!

	nSubChID  = g_atDabRegSubchInfo[nRegSubChArrayIdx].nSubChID;
	nHwSubChIdx = g_atDabRegSubchInfo[nRegSubChArrayIdx].nHwSubChIdx;

	/* Delete a used sub channel index. */
	g_nDabRtvUsedHwSubChIdxBits &= ~(1<<nHwSubChIdx);
	g_nDabOpenedSubChNum--;

	// Disable the specified SUB CH first.
#if (RTV_NUM_DAB_AVD_SERVICE == 1) /* Single Sub Channel */
	rtv_Set_MSC1_SUBCH0(nSubChID, FALSE, FALSE);

	#if defined(RTV_IF_SPI) || defined(RTV_IF_EBI2)
	RTV_REG_MAP_SEL(HOST_PAGE); 
	g_bRtvIntrMaskRegL |= MSC1_INTR_BITS;
	RTV_REG_SET(0x62, g_bRtvIntrMaskRegL);
	#endif
#else /* Multi Sub Channel */
	switch( nHwSubChIdx )
	{
		case 0: rtv_Set_MSC1_SUBCH0(nSubChID, FALSE, FALSE); break;			
		case 3: rtv_Set_MSC0_SUBCH3(nSubChID, FALSE); break;
		case 4: rtv_Set_MSC0_SUBCH4(nSubChID, FALSE); break;
		case 5: rtv_Set_MSC0_SUBCH5(nSubChID, FALSE); break;
		case 6: rtv_Set_MSC0_SUBCH6(nSubChID, FALSE); break;
		default: break;
	}

	if((g_nDabRtvUsedHwSubChIdxBits & DAB_MSC1_SUBCH_USE_MASK) == 0)
	{
	#if defined(RTV_IF_SPI) || defined(RTV_IF_EBI2)
		RTV_REG_MAP_SEL(HOST_PAGE); 
		g_bRtvIntrMaskRegL |= MSC1_INTR_BITS;
		RTV_REG_SET(0x62, g_bRtvIntrMaskRegL);
	#endif
	}
	
	if((g_nDabRtvUsedHwSubChIdxBits & DAB_MSC0_SUBCH_USE_MASK) == 0)
	{
	#if defined(RTV_IF_SPI) || defined(RTV_IF_EBI2)	
		RTV_REG_MAP_SEL(HOST_PAGE); 
		g_bRtvIntrMaskRegL |= MSC0_INTR_BITS;
		RTV_REG_SET(0x62, g_bRtvIntrMaskRegL);
	#endif
	}
#endif

#if defined(RTV_IF_SPI) || defined(RTV_IF_EBI2)
	#ifdef RTV_CIF_MODE_ENABLED
	if (nHwSubChIdx != 0) { /* MSC0 only */
		 rtv_DeleteSpiCifSubChannelID_MSC0(nSubChID); /* for ISR */
		#ifdef RTV_BUILD_CIFDEC_WITH_DRIVER
		rtvCIFDEC_DeleteSubChannelID(nSubChID);
		#endif
	}
	#endif
#else /* TSIF */
	#if defined(RTV_CIF_MODE_ENABLED) && defined(RTV_BUILD_CIFDEC_WITH_DRIVER)
	rtvCIFDEC_DeleteSubChannelID(nSubChID);
	#endif
#endif

#if defined(RTV_CIF_MODE_ENABLED) && defined(RTV_BUILD_CIFDEC_WITH_DRIVER)
	#if defined(RTV_IF_SPI) || defined(RTV_IF_EBI2)
	if((g_nDabRtvUsedHwSubChIdxBits & DAB_MSC0_SUBCH_USE_MASK) == 0)
		rtvCIFDEC_Deinit(); // MSC0 Only
	#else
	if ((g_nDabOpenedSubChNum == 0) && (g_fRtvFicOpened == FALSE))
		rtvCIFDEC_Deinit();
	#endif
#endif

#if defined(RTV_IF_SERIAL_TSIF) || defined(RTV_IF_SPI_SLAVE)
	/* Must setuped before the opening of new ansemble. */
	RTV_REG_SET(0x03, 0x09);
	RTV_REG_SET(0x46, 0x1E);
	RTV_REG_SET(0x35, 0x01);
	RTV_REG_SET(0x46, 0x16);
#endif

	/* Delete a registered subch array index. */
	g_nDabRegSubChArrayIdxBits &= ~(1<<nRegSubChArrayIdx);

	/* Deregister a sub channel ID Bit. */
	g_aDabRegSubChIdBits[DIV32(nSubChID)] &= ~(1 << MOD32(nSubChID));

	return RTV_SUCCESS;
}


#if (RTV_NUM_DAB_AVD_SERVICE >= 2)
static void dab_CloseAllSubChannel(void)
{
	UINT i = 0;
	UINT nRegSubChArrayIdxBits = g_nDabRegSubChArrayIdxBits;
	
	while(nRegSubChArrayIdxBits != 0)
	{
		if( nRegSubChArrayIdxBits & 0x01 ) 	
		{
			dab_CloseSubChannel(i);		
		}		

		nRegSubChArrayIdxBits >>= 1;
		i++;
	}
}
#endif


void rtvDAB_CloseAllSubChannels(void)
{
#if (RTV_NUM_DAB_AVD_SERVICE >= 2)
	UINT i = 0;
	UINT nRegSubChArrayIdxBits = g_nDabRegSubChArrayIdxBits;
#endif

	RTV_GUARD_LOCK(demod_no);

#if (RTV_NUM_DAB_AVD_SERVICE == 1) /* Single Sub Channel */
	dab_CloseSubChannel(0);
#elif (RTV_NUM_DAB_AVD_SERVICE >= 2)	
	while(nRegSubChArrayIdxBits != 0)
	{
		if( nRegSubChArrayIdxBits & 0x01 )	
		{
			dab_CloseSubChannel(i);		
		}		

		nRegSubChArrayIdxBits >>= 1;
		i++;
	}
#endif	

	RTV_GUARD_FREE(demod_no);
}


INT rtvDAB_CloseSubChannel(UINT nSubChID)
{
	INT nRet = RTV_SUCCESS;
#if (RTV_NUM_DAB_AVD_SERVICE >= 2)
	UINT i = 0;
	UINT nRegSubChArrayIdxBits = g_nDabRegSubChArrayIdxBits;
#endif

	if(nSubChID > (MAX_NUM_DAB_SUB_CH-1))
		return RTV_INVAILD_SUB_CHANNEL_ID;

	RTV_GUARD_LOCK(demod_no);

#if (RTV_NUM_DAB_AVD_SERVICE == 1) /* Single Sub Channel */
	nRet = dab_CloseSubChannel(0);
#elif (RTV_NUM_DAB_AVD_SERVICE >= 2)	
	while(nRegSubChArrayIdxBits != 0)
	{
		if( nRegSubChArrayIdxBits & 0x01 )	
		{
			if(nSubChID == g_atDabRegSubchInfo[i].nSubChID)
				dab_CloseSubChannel(i);		
		}		

		nRegSubChArrayIdxBits >>= 1;
		i++;
	}
#endif	

	RTV_GUARD_FREE(demod_no);
			
	return RTV_SUCCESS;
}



static void dab_OpenSubChannel(UINT nSubChID, E_RTV_SERVICE_TYPE eServiceType,
				UINT nThresholdSize)
{
	INT nHwSubChIdx;
	UINT i = 0;

	if (g_nDabOpenedSubChNum == 0) { /* The first open */
#if defined(RTV_IF_SERIAL_TSIF) || defined(RTV_IF_SPI_SLAVE)
		if (g_fRtvFicOpened == TRUE) {
			/* Adjust FIC state when FIC was opened prior to this functtion. */
		//	RTV_DBGMSG1("[dab_OpenSubChannel] Closing with FIC_state_path(%d)\n", g_nRtvFicOpenedStatePath);
			rtv_CloseFIC(0, g_nRtvFicOpenedStatePath);

			/* Update the opened state and path to use open FIC. */
			g_nRtvFicOpenedStatePath = rtv_OpenFIC(1/* Force play state */);
			RTV_DBGMSG1("[tdmb_OpenSubChannel] Opened with FIC_state_path(%d)\n", g_nRtvFicOpenedStatePath);
		}

	#if defined(RTV_CIF_MODE_ENABLED) && defined(RTV_BUILD_CIFDEC_WITH_DRIVER)
		rtvCIFDEC_Init();
	#endif
#else /* SPI or EBI2 */
	#if defined(RTV_CIF_MODE_ENABLED) && defined(RTV_BUILD_CIFDEC_WITH_DRIVER)
		if (eServiceType == RTV_SERVICE_DATA)
			rtvCIFDEC_Init(); /* Only MSC0 */
	#endif
#endif
	}

#if defined(RTV_IF_SPI) || defined(RTV_IF_EBI2)
	#ifdef RTV_CIF_MODE_ENABLED
	if (eServiceType == RTV_SERVICE_DATA) { /* MSC0 only */
		 rtv_AddSpiCifSubChannelID_MSC0(nSubChID); /* for ISR */
		#ifdef RTV_BUILD_CIFDEC_WITH_DRIVER
		rtvCIFDEC_AddSubChannelID(nSubChID, eServiceType);
		#endif
	}
	#endif
#else /* TSIF */
	#if defined(RTV_CIF_MODE_ENABLED) && defined(RTV_BUILD_CIFDEC_WITH_DRIVER)
	rtvCIFDEC_AddSubChannelID(nSubChID, eServiceType);
	#endif
#endif

#if (RTV_NUM_DAB_AVD_SERVICE == 1) /* Single Subchannel */
	nHwSubChIdx = 0;		

  	rtv_SetupThreshold_MSC1(nThresholdSize);

	#if defined(RTV_IF_SPI) || defined(RTV_IF_EBI2)
	rtv_SetupMemory_MSC1(RTV_TV_MODE_DAB_B3);
	RTV_REG_MAP_SEL(DD_PAGE);
	RTV_REG_SET(INT_E_UCLRL, MSC1_INTR_BITS); /* MSC1 Interrupt status clear. */

	RTV_REG_MAP_SEL(HOST_PAGE);	
	g_bRtvIntrMaskRegL &= ~(MSC1_INTR_BITS);
	RTV_REG_SET(0x62, g_bRtvIntrMaskRegL); /* Enable MSC1 interrupts. */ 	
	#endif

	/* Set the sub-channel and enable MSC memory with the specified sub ID. */			
	if(eServiceType == RTV_SERVICE_VIDEO)
	{
		rtv_Set_MSC1_SUBCH0(nSubChID, TRUE, TRUE); // RS enable
	}
	else
	{
		rtv_Set_MSC1_SUBCH0(nSubChID, TRUE, FALSE); // RS Disable
	}
  
#else /* Multi sub channel enabled */	

	if((eServiceType == RTV_SERVICE_VIDEO) || (eServiceType == RTV_SERVICE_AUDIO))
	{
		nHwSubChIdx = 0;		

		if((g_nDabRtvUsedHwSubChIdxBits & DAB_MSC1_SUBCH_USE_MASK) == 0)
		{	/* First enabled. */
			rtv_SetupThreshold_MSC1(nThresholdSize); 

		#if defined(RTV_IF_SPI) || defined(RTV_IF_EBI2)
			rtv_SetupMemory_MSC1(RTV_TV_MODE_DAB_B3);
			RTV_REG_MAP_SEL(DD_PAGE);
			RTV_REG_SET(INT_E_UCLRL, MSC1_INTR_BITS); // MSC1 Interrupts status clear.

			RTV_REG_MAP_SEL(HOST_PAGE);	
			g_bRtvIntrMaskRegL &= ~(MSC1_INTR_BITS);
			RTV_REG_SET(0x62, g_bRtvIntrMaskRegL); /* Enable MSC1 interrupts and restore FIC . */		
		#endif
		}

		if(eServiceType == RTV_SERVICE_VIDEO)
		{
			rtv_Set_MSC1_SUBCH0(nSubChID, TRUE, TRUE); // RS enable
		}
		else
		{
			rtv_Set_MSC1_SUBCH0(nSubChID, TRUE, FALSE); // RS disable
		}
	}
	else /* Data */
	{
	#if defined(RTV_IF_SPI) || defined(RTV_IF_EBI2)
		/* Search an available SUBCH index for Audio/Data service. (3 ~ 6) */
		for(nHwSubChIdx=3/* MSC0 base */; nHwSubChIdx<=6; nHwSubChIdx++)
	#else
		for(nHwSubChIdx=3/* MSC0 base */; nHwSubChIdx<=5; nHwSubChIdx++)
	#endif
		{
			if((g_nDabRtvUsedHwSubChIdxBits & (1<<nHwSubChIdx)) == 0) 			
				break;			
		}			 
	
		if((g_nDabRtvUsedHwSubChIdxBits & DAB_MSC0_SUBCH_USE_MASK) == 0)
		{
	#ifndef RTV_CIF_MODE_ENABLED
			rtv_SetupThreshold_MSC0(nThresholdSize); /* CIF mode. */
	#endif

	#if defined(RTV_IF_SPI) || defined(RTV_IF_EBI2)
			rtv_SetupMemory_MSC0(RTV_TV_MODE_DAB_B3);
			RTV_REG_MAP_SEL(DD_PAGE);
			RTV_REG_SET(INT_E_UCLRL, MSC0_INTR_BITS); // MSC0 Interrupt clear.			

			/* Enable MSC0 interrupts. */
			RTV_REG_MAP_SEL(HOST_PAGE);	
			g_bRtvIntrMaskRegL &= ~(MSC0_INTR_BITS);
			RTV_REG_SET(0x62, g_bRtvIntrMaskRegL);	 // restore FIC	
	#endif
		}

		/* Set sub channel. */ 
		switch(nHwSubChIdx) {
		case 3: rtv_Set_MSC0_SUBCH3(nSubChID, TRUE); break;
		case 4: rtv_Set_MSC0_SUBCH4(nSubChID, TRUE); break;
		case 5: rtv_Set_MSC0_SUBCH5(nSubChID, TRUE); break;
	#if defined(RTV_IF_SPI) || defined(RTV_IF_EBI2)
		case 6: rtv_Set_MSC0_SUBCH6(nSubChID, TRUE); break;
	#endif
		default: break;
		}
	}	
#endif 		

	/* To use when close .*/	
#if (RTV_NUM_DAB_AVD_SERVICE >= 2)
	for(i=0; i<RTV_NUM_DAB_AVD_SERVICE; i++)
	{
		if((g_nDabRegSubChArrayIdxBits & (1<<i)) == 0)		
		{
#else
	i = 0;
#endif		
			/* Register a array index of sub channel */
			g_nDabRegSubChArrayIdxBits |= (1<<i);
			
			g_atDabRegSubchInfo[i].nSubChID = nSubChID;

			/* Add the new sub channel index. */
			g_atDabRegSubchInfo[i].nHwSubChIdx  = nHwSubChIdx;
			g_atDabRegSubchInfo[i].eServiceType   = eServiceType;
			g_atDabRegSubchInfo[i].nThresholdSize = nThresholdSize;
#if (RTV_NUM_DAB_AVD_SERVICE >= 2)
			break;
		}
	}		
#endif	

	g_nDabOpenedSubChNum++;
	/* Add the HW sub channel index. */
	g_nDabRtvUsedHwSubChIdxBits |= (1<<nHwSubChIdx);	

	/* Register a new sub channel ID Bit. */
	g_aDabRegSubChIdBits[DIV32(nSubChID)] |= (1 << MOD32(nSubChID)); 

	//RTV_DBGMSG2("[dab_OpenSubChannel] nSubChID(%d) use MSC_SUBCH%d\n", nSubChID, nHwSubChIdx);	
}


INT rtvDAB_OpenSubChannel(U32 dwChFreqKHz, UINT nSubChID,
	E_RTV_SERVICE_TYPE eServiceType, UINT nThresholdSize)
{
	INT nRet = RTV_SUCCESS;

	if(nSubChID > (MAX_NUM_DAB_SUB_CH-1))
		return RTV_INVAILD_SUB_CHANNEL_ID;

	/* Check for threshold size. */
#ifndef RTV_CIF_MODE_ENABLED /* Individual Mode */
   #if defined(RTV_IF_SPI) || defined(RTV_IF_EBI2)
	 if(nThresholdSize > (188*18))  
	 	return RTV_INVAILD_THRESHOLD_SIZE;
   #endif
#endif

	/* Check the previous freq. */
	if(g_dwDabPrevChFreqKHz == dwChFreqKHz)
	{
		/* Is registerd sub ch ID? */
		if(g_aDabRegSubChIdBits[DIV32(nSubChID)] & (1<<MOD32(nSubChID)))
		{
			RTV_GUARD_LOCK(demod_no);
			rtv_StreamRestore(RTV_TV_MODE_DAB_B3);// To restore FIC stream.
			RTV_GUARD_FREE(demod_no);
		
			RTV_DBGMSG1("[rtvdab_OpenSubChannel] Already opened sub channed ID(%d)\n", nSubChID);
			
			return RTV_ALREADY_OPENED_SUB_CHANNEL_ID;
		}
	
   #if (RTV_NUM_DAB_AVD_SERVICE == 1)  /* Single Sub Channel Mode */
   		RTV_GUARD_LOCK(demod_no);
   		dab_CloseSubChannel(0); /* Max sub channel is 1. So, we close the previous sub ch. */
		RTV_REG_MAP_SEL(OFDM_PAGE);
		RTV_REG_SET(0x10, 0x48);
		RTV_REG_SET(0x10, 0xC9);
   		dab_OpenSubChannel(nSubChID, eServiceType, nThresholdSize);
		RTV_GUARD_FREE(demod_no);
   #else
   /* Multi Sub Channel. */ 
		if((eServiceType == RTV_SERVICE_VIDEO) || (eServiceType == RTV_SERVICE_AUDIO))
		{
			/* Check if the SUBCH available for Video service ? */
			if((g_nDabRtvUsedHwSubChIdxBits & DAB_MSC1_SUBCH_USE_MASK) == DAB_MSC1_SUBCH_USE_MASK)
			{
				RTV_GUARD_LOCK(demod_no);
				rtv_StreamRestore(RTV_TV_MODE_DAB_B3);// To restore FIC stream.
				RTV_GUARD_FREE(demod_no);

				return RTV_NO_MORE_SUB_CHANNEL; // Only 1 Video/Audio service. Close other video service.
			}
		}
		else /* Data */
		{
			/* Check if the SUBCH available for Audio/Data services ? */
			if((g_nDabRtvUsedHwSubChIdxBits & DAB_MSC0_SUBCH_USE_MASK) == DAB_MSC0_SUBCH_USE_MASK)
			{
				RTV_GUARD_LOCK(demod_no);
				rtv_StreamRestore(RTV_TV_MODE_DAB_B3);// To restore FIC stream.
				RTV_GUARD_FREE(demod_no);

				return RTV_NO_MORE_SUB_CHANNEL; // Not available SUBCH for Audio/Data.
			}
		}   
	
		RTV_GUARD_LOCK(demod_no);
		if (g_nDabOpenedSubChNum == 0) {
			RTV_REG_MAP_SEL(OFDM_PAGE);
			RTV_REG_SET(0x10, 0x48);
			RTV_REG_SET(0x10, 0xC9);
		}
		dab_OpenSubChannel(nSubChID, eServiceType, nThresholdSize);    
		rtv_StreamRestore(RTV_TV_MODE_DAB_B3);// To restore FIC stream. This is NOT the first time.
		RTV_GUARD_FREE(demod_no);		
   #endif
	}/* if(g_adabPrevChFreqKHz[RaonTvChipIdx] == dwChFreqKHz) */
	else
	{
		g_dwDabPrevChFreqKHz = dwChFreqKHz;

		g_fRtvChannelChange = TRUE; // To prevent get ber, cnr ...
		
		RTV_GUARD_LOCK(demod_no);

#if (RTV_NUM_DAB_AVD_SERVICE == 1)
		dab_CloseSubChannel(0); // Cloes the previous sub channel because this channel is a new freq.
#elif (RTV_NUM_DAB_AVD_SERVICE >= 2)
		dab_CloseAllSubChannel(); // Cloes the all sub channel because this channel is a new freq.	  
#endif	
		nRet = rtvRF_SetFrequency(rtv_GetDabTvMode(dwChFreqKHz), 0, dwChFreqKHz);

  		dab_OpenSubChannel(nSubChID, eServiceType, nThresholdSize);

		RTV_GUARD_FREE(demod_no);

		g_fRtvChannelChange = FALSE; 
	}

	return nRet;
}


void rtvDAB_DisableReconfigInterrupt(void)
{
	RTV_GUARD_LOCK(demod_no);

	rtv_DisableReconfigInterrupt();

	RTV_GUARD_FREE(demod_no); 
}

void rtvDAB_EnableReconfigInterrupt(void)
{
	RTV_GUARD_LOCK(demod_no);

	rtv_EnableReconfigInterrupt();

	RTV_GUARD_FREE(demod_no);	
}

/*
	rtvDAB_ReadFIC()
	
	This function reads a FIC data in manually. 
*/
INT rtvDAB_ReadFIC(U8 *pbBuf, UINT nFicSize)
{
#ifdef RTV_FIC_POLLING_MODE	
	U8 int_type_val1;	
	UINT cnt = 0;		
	
	if(g_fRtvFicOpened == FALSE)
	{
		RTV_DBGMSG0("[rtvDAB_ReadFIC] NOT OPEN FIC\n");
		return RTV_NOT_OPENED_FIC;
	}

	if(nFicSize == 0)
	{
		RTV_DBGMSG1("[rtvDAB_ReadFIC] Invalid FIC read size: %u\n", nFicSize);
		return RTV_INVALID_FIC_READ_SIZE;
	}

	RTV_GUARD_LOCK(demod_no);

	RTV_REG_MAP_SEL(DD_PAGE);
	RTV_REG_SET(INT_E_UCLRL, 0x01); // FIC interrupt status clear

	while(++cnt <= 40)
	{
		int_type_val1 = RTV_REG_GET(INT_E_STATL);
		if(int_type_val1 & FIC_E_INT)
		{
		    //printk("FIC_E_INT occured : %u\n", cnt);

			RTV_REG_MAP_SEL(FIC_PAGE);
	#if defined(RTV_IF_TSIF) || defined(RTV_IF_SPI_SLAVE)
			RTV_REG_BURST_GET(0x10, pbBuf, nFicSize/2);			
			RTV_REG_BURST_GET(0x10, pbBuf+(nFicSize/2), (nFicSize/2));	

	#elif defined(RTV_IF_SPI) 
        #if defined(__KERNEL__) || defined(WINCE)
			RTV_REG_BURST_GET(0x10, pbBuf, nFicSize+1);
		#else
			RTV_REG_BURST_GET(0x10, pbBuf, nFicSize);
		#endif
	#endif	
			RTV_REG_MAP_SEL(DD_PAGE);
			RTV_REG_SET(INT_E_UCLRL, 0x01); // FIC interrupt status clear

			RTV_GUARD_FREE(demod_no);

			return nFicSize; 

		}

		RTV_DELAY_MS(12);
	} /* while() */

	RTV_GUARD_FREE(demod_no);

	RTV_DBGMSG0("[rtvdab_ReadFIC] FIC read timeout\n");

	return 0;

#else
	RTV_GUARD_LOCK(demod_no); /* If the caller not is the ISR. */

		RTV_REG_MAP_SEL(FIC_PAGE);
	#if defined(RTV_IF_TSIF) || defined(RTV_IF_SPI_SLAVE)
		RTV_REG_BURST_GET(0x10, pbBuf, (nFicSize/2));			
		RTV_REG_BURST_GET(0x10, pbBuf+(nFicSize/2), (nFicSize/2));	

	#elif defined(RTV_IF_SPI) 
		#if defined(__KERNEL__) || defined(WINCE)
		    RTV_REG_BURST_GET(0x10, pbBuf, nFicSize+1);
		#else
		    RTV_REG_BURST_GET(0x10, pbBuf, nFicSize);
		#endif
	#endif	
	
		RTV_REG_MAP_SEL(DD_PAGE);
		RTV_REG_SET(INT_E_UCLRL, 0x01); // FIC interrupt status clear
	
	RTV_GUARD_FREE(demod_no);
		return nFicSize;	
#endif		
}



/* This function should called after CHANNEL LOCKed. */
UINT rtvDAB_GetFicSize(void)
{
	UINT nFicSize;

	RTV_GUARD_LOCK(demod_no);

	nFicSize = rtv_GetFicSize();
	
	RTV_GUARD_FREE(demod_no);
	
	return nFicSize;
}

void rtvDAB_CloseFIC(void)
{
	if(g_fRtvFicOpened == FALSE)
		return;

	RTV_GUARD_LOCK(demod_no);

#if defined(RTV_IF_SERIAL_TSIF) || defined(RTV_IF_SPI_SLAVE)
	//RTV_DBGMSG1("[rtvTDMB_CloseFIC] Closing with FIC_state_path(%d)\n", g_nRtvFicOpenedStatePath);
	rtv_CloseFIC(g_nDabOpenedSubChNum, g_nRtvFicOpenedStatePath);

	/* To protect memory reset in rtv_SetupMemory_FIC() */
	if ((g_nRtvFicOpenedStatePath == FIC_OPENED_PATH_TSIF_IN_SCAN)
	|| (g_nRtvFicOpenedStatePath == FIC_OPENED_PATH_TSIF_IN_PLAY))
		RTV_DELAY_MS(RTV_TS_STREAM_DISABLE_DELAY);

	#if defined(RTV_CIF_MODE_ENABLED) && defined(RTV_BUILD_CIFDEC_WITH_DRIVER)
	if (g_nDabOpenedSubChNum == 0) {
		if (g_nRtvFicOpenedStatePath == FIC_OPENED_PATH_TSIF_IN_PLAY)
			rtvCIFDEC_Deinit(); /* From play state */
	}
	#endif
#else
	rtv_CloseFIC(0, 0); /* Don't care */
#endif

	g_fRtvFicOpened = FALSE;

	RTV_GUARD_FREE(demod_no);
}


INT rtvDAB_OpenFIC(void)
{
	if (g_fRtvFicOpened == TRUE) {
		RTV_DBGMSG0("[rtvDAB_OpenFIC] Must close FIC prior to opening FIC!\n");
		return RTV_DUPLICATING_OPEN_FIC;
	}

	g_fRtvFicOpened = TRUE;

	RTV_GUARD_LOCK(demod_no);

#if defined(RTV_IF_SERIAL_TSIF) || defined(RTV_IF_SPI_SLAVE)
	g_nRtvFicOpenedStatePath = rtv_OpenFIC(g_nDabOpenedSubChNum);
	RTV_DBGMSG1("[rtvDAB_OpenFIC] Opened with FIC_state_path(%d)\n", g_nRtvFicOpenedStatePath);
#else
	rtv_OpenFIC(g_nDabOpenedSubChNum);
#endif

	RTV_GUARD_FREE(demod_no);

	return RTV_SUCCESS;
}

/* When this function was called, all sub channel should closed to reduce scan-time !!!!! 
   FIC can enabled. Usally enabled.
 */
INT rtvDAB_ScanFrequency(U32 dwChFreqKHz)
{
	U8 scan_done, OFDM_L=0, ccnt = 0, NULL_C=0, SCV_C=0;
	U8 scan_pwr1=0, scan_pwr2=0, DAB_Mode=0xFF;
	U8 pre_agc1=0, pre_agc2=0, pre_agc_mon=0, ASCV=0;
	INT scan_flag = RTV_SUCCESS;
	UINT SPower =0, PreGain=0, PreGainTH=0, PWR_TH = 0, ILoopTH =0;
	U8 Cfreq_HTH = 0,Cfreq_LTH=0;
	U8 i=0,j=0, m=0;
	U8 varyLow=0,varyHigh=0;
	U16 varyMon=0;
	U8 MON_FSM=0, FsmCntChk=0;
	U8 RF00 = 0;
	U8 CoCH_Chk = 0;
	U16 fail = 0xFFFF;
	U8 FEC_SYNC=0xFF, CoarseFreq=0xFF, NullChCnt=0;
	U8 rdata0 =0, rdata1=0, rdata2=0, rdata3=0;
	U32 fic_cer_chk = 0;
	U8 MonFsm6Cnt = 0;
	U16 NullLenMon=0, PostAgcMon=0;
	UINT nReTryCnt = 0;

	g_fRtvChannelChange = TRUE; // To prevent get ber, cnr ...

	RTV_GUARD_LOCK(demod_no);

	/* NOTE: When this rountine executed, all sub channel should closed 
			 and memory(MSC0, MSC1) interrupts are disabled. */
#if (RTV_NUM_DAB_AVD_SERVICE == 1)
	dab_CloseSubChannel(0);
#elif (RTV_NUM_DAB_AVD_SERVICE >= 2)
	dab_CloseAllSubChannel();	  
#endif

	scan_flag = rtvRF_SetFrequency(rtv_GetDabTvMode(dwChFreqKHz), 0, dwChFreqKHz);
	if (scan_flag != RTV_SUCCESS)
		goto DAB_SCAN_EXIT;

	RTV_REG_MAP_SEL(COMM_PAGE);
	RTV_REG_SET(0x55,0x0D); // CO_DC_DEPTH

	RTV_REG_MAP_SEL(OFDM_PAGE);
	if ((110000 < dwChFreqKHz) && ( dwChFreqKHz < 246000)) { /* Band-III */
		switch (g_eRtvAdcClkFreqType) {
		case RTV_ADC_CLK_FREQ_8_MHz:
			RTV_REG_SET(0x54, 0x7B);
			break;
		case RTV_ADC_CLK_FREQ_8_192_MHz:
			RTV_REG_SET(0x54, 0x78);
			break;
		case RTV_ADC_CLK_FREQ_9_6_MHz:
			RTV_REG_SET(0x54, 0x7A);
			break;
		default: /* 9 MHz */
			RTV_REG_SET(0x54, 0x78);
			break;
		}
		RTV_REG_SET(0x5A, 0x1B);

		RTV_REG_MAP_SEL(RF_PAGE); /* BPF BW */
		RTV_REG_SET(0x4B, 0x8F);
	}
	else { /* L-Band */
		RTV_REG_SET(0x5A, 0x1C);
		RTV_REG_SET(0x54, 0x78);

		RTV_REG_MAP_SEL(RF_PAGE);  /* BPF BW */
		RTV_REG_SET(0x4B, 0x8F);
	}

	dab_SoftReset();

	while (1) {
		scan_flag = RTV_CHANNEL_NOT_DETECTED; /* Reset */

		if (++nReTryCnt == 10000) { /* Up to 400ms */
			RTV_DBGMSG0("[rtvDAB_ScanFrequency] Scan Timeout! \n");
			break;
		}

		RTV_REG_MAP_SEL(OFDM_PAGE);
		scan_done = RTV_REG_GET(0xCF);
		if (scan_done == 0xFF) { /* Interface error */
			fail = 0xFF0C;
			goto DAB_SCAN_EXIT;
		}

		RTV_REG_MAP_SEL(COMM_PAGE);	/* Scan-Power monitoring */
		scan_pwr1 = RTV_REG_GET(0x38);
		scan_pwr2 = RTV_REG_GET(0x39);
		SPower = (scan_pwr2<<8)|scan_pwr1;

		NULL_C = 0;
		SCV_C = 0;
		RTV_REG_MAP_SEL(OFDM_PAGE);
		pre_agc_mon = RTV_REG_GET(0x53);
		RTV_REG_SET(0x53, (pre_agc_mon | 0x80)); /* Pre-AGC Gain monitoring One-shot */
		pre_agc1 = RTV_REG_GET(0x66);
		pre_agc2 = RTV_REG_GET(0x67);
		PreGain = (pre_agc2<<2)|(pre_agc1&0x03);

		DAB_Mode = RTV_REG_GET(0x27); /* DAB TX Mode monitoring */
		DAB_Mode = (DAB_Mode & 0x30)>>4;
		switch (DAB_Mode) {
		case 0:
				RTV_REG_SET(0x1A, 0xB4);
				break;
		case 1:
				RTV_REG_SET(0x1A, 0x44);
				break;
		case 2:
				RTV_REG_SET(0x1A, 0x04);
				break;
		case 3:
				RTV_REG_SET(0x1A, 0x84);
				break;
		default:
				RTV_REG_SET(0x1A, 0xB4);
				break;
		}

		switch (g_eRtvAdcClkFreqType) {
		case RTV_ADC_CLK_FREQ_8_MHz :
			if (dwChFreqKHz > 250000)
				PreGainTH = 355;
			else
				PreGainTH = 390;

			switch (DAB_Mode) { /* tr mode */
			case 0:
				PWR_TH = 6500;
				ILoopTH = 75;
				Cfreq_HTH = 206;
				Cfreq_LTH = 55;
				break;
			case 1:
				PWR_TH = 950;
				ILoopTH = 75;
				Cfreq_HTH = 242;
				Cfreq_LTH = 14;
				break;
			case 2:
				PWR_TH = 320;
				ILoopTH = 75;
				Cfreq_HTH = 248;
				Cfreq_LTH = 8;
				break;
			case 3:
				PWR_TH = 4000;
				ILoopTH = 75;
				Cfreq_HTH = 230;
				Cfreq_LTH = 26;
				break;
			default:
				PWR_TH = 6500;
				ILoopTH = 75;
				Cfreq_HTH = 206;
				Cfreq_LTH = 55;
				break;
			}
			break;

		case RTV_ADC_CLK_FREQ_8_192_MHz :
			if (dwChFreqKHz > 250000)
				PreGainTH = 355;		 
			else
				PreGainTH = 390;
				
			switch (DAB_Mode) {
            case 0:
				PWR_TH = 6500;
				ILoopTH = 75;
				Cfreq_HTH = 206;
				Cfreq_LTH = 55;
				break;
			case 1:
				PWR_TH = 950;
				ILoopTH = 75;
				Cfreq_HTH = 242;
				Cfreq_LTH = 14;
				break;
			case 2:
				PWR_TH = 320;
				ILoopTH = 75;
				Cfreq_HTH = 248;
				Cfreq_LTH = 8;
				break;
			case 3:
				PWR_TH = 4000;
				ILoopTH = 75;
				Cfreq_HTH = 230;
				Cfreq_LTH = 26;
				break;
			default:
				PWR_TH = 6500;
				ILoopTH = 75;
				Cfreq_HTH = 206;
				Cfreq_LTH = 55;
				break;
			}				
			break;

		case RTV_ADC_CLK_FREQ_9_MHz :
			if (dwChFreqKHz > 250000)
				PreGainTH = 355;		 
			else
				PreGainTH = 380;
				
			switch (DAB_Mode) {
			case 0:
				PWR_TH = 6500;
				ILoopTH = 75;
				Cfreq_HTH = 206;
				Cfreq_LTH = 55;
				break;
			case 1:
				PWR_TH = 950;
				ILoopTH = 75;
				Cfreq_HTH = 242;
				Cfreq_LTH = 14;
				break;
			case 2:
				PWR_TH = 400;
				ILoopTH = 75;
				Cfreq_HTH = 248;
				Cfreq_LTH = 8;
				break;
			case 3:
				PWR_TH = 5000;
				ILoopTH = 75;
				Cfreq_HTH = 230;
				Cfreq_LTH = 26;
				break;
			default:
				PWR_TH = 6500;
				ILoopTH = 75;
				Cfreq_HTH = 206;
				Cfreq_LTH = 55;
				break;
			}
			break;

		case RTV_ADC_CLK_FREQ_9_6_MHz :
			if (dwChFreqKHz > 250000)
				PreGainTH = 355;		 
			else
				PreGainTH = 380;
				
			switch (DAB_Mode) {
			case 0:
				PWR_TH = 6500;
				ILoopTH = 75;
				Cfreq_HTH = 206;
				Cfreq_LTH = 55;
				break;
			case 1:
				PWR_TH = 950;
				ILoopTH = 75;
				Cfreq_HTH = 242;
				Cfreq_LTH = 14;
				break;
			case 2:
				PWR_TH = 400;
				ILoopTH = 75;
				Cfreq_HTH = 248;
				Cfreq_LTH = 8;
				break;
			case 3:
				PWR_TH = 5000;
				ILoopTH = 75;
				Cfreq_HTH = 230;
				Cfreq_LTH = 26;
				break;
			default:
				PWR_TH = 6500;
				ILoopTH = 75;
				Cfreq_HTH = 206;
				Cfreq_LTH = 55;
				break;
			}
			break;

		default:
			goto DAB_SCAN_EXIT;
		}

		/* Scan-done flag & scan-out flag check */
		if (scan_done != 0x03) {
			if (scan_done == 0x01) { /* Not DAB signal channel */
				fail = 0xEF01;
				RTV_REG_MAP_SEL(OFDM_PAGE);
				RTV_REG_SET(0x5A, 0x1C);
				goto DAB_SCAN_EXIT;
			}
			else /* Not scan finished */
				continue;
		}

		RTV_REG_MAP_SEL(OFDM_PAGE);
		RTV_REG_SET(0x5A, 0x1C);
		CoarseFreq = RTV_REG_GET(0x18);	/* coarse freq */

		RTV_REG_MAP_SEL(RF_PAGE);
		RF00 = RTV_REG_GET(0x00);
		RF00 = (RF00 & 0xC0)>>6;

		if (g_curDabSetType == RTV_TV_MODE_TDMB) {
			if (DAB_Mode > 0) {
				fail = 0xE002;
				goto DAB_SCAN_EXIT;
			}
		}

		if (RF00 == 0x02)
			PreGainTH = PreGainTH * (UINT)(0.7 * 10); /* Pre-multiplied with 10 */

		/* PreAGC Gain threshold check */
		if ((PreGain*10 < PreGainTH) && (PreGain != 0)) {
			fail = 0xFF04;
			goto DAB_SCAN_EXIT;
		}
		else {
			for (m = 0; m < 16; m++) {
				RTV_REG_MAP_SEL(RF_PAGE);	
				RF00 = RTV_REG_GET(0x00);
				RF00 = (RF00 & 0xC0)>>6;
				if ((RF00 == 0x02) && (m>10)) { /* ACR */
					if ((SPower < (PWR_TH/4)) && (SPower != 0)) {
						fail = 0xF303;
						goto DAB_SCAN_EXIT;
					}
				}
				else { /* Normal And Sensitivity */
					if ((SPower<PWR_TH) && (SPower != 0)) {
						fail = 0xFF03;
						goto DAB_SCAN_EXIT;
					}
				}					

				RTV_REG_MAP_SEL(RF_PAGE);	
				RF00 = RTV_REG_GET(0x00);
				RF00 = (RF00 & 0xC0)>>6;
				
				RTV_REG_MAP_SEL(OFDM_PAGE); 
				rdata0 = RTV_REG_GET(0xB1);		
				rdata1 = RTV_REG_GET(0xB5);
				rdata2 = RTV_REG_GET(0xB6);
				if ((rdata0&0x01) 
					&& ((((rdata2 & 0x07)<<8) | (rdata1 & 0xFF))==0)
					&& (CoCH_Chk > 14) && (RF00 == 2) && (m>14)) {
					fail = 0x8888;
					goto DAB_SCAN_EXIT;
				}

				if ((rdata0&0x01) && ((((rdata2 & 0x07)<<8) | (rdata1 & 0xFF))==0))
					CoCH_Chk++;
				
				RTV_REG_MAP_SEL(RF_PAGE);
				RF00 = RTV_REG_GET(0x00);
				RF00 = (RF00 & 0xC0)>>6;
				if ((SPower<PWR_TH) && (RF00 != 0x02) && (SPower != 0) && (m>13)) { /* C/N channel */
					fail = 0xFF33;	
					goto DAB_SCAN_EXIT;
				}
	
				if (m > 14) {
					scan_flag = RTV_SUCCESS; /* Set */
					break;
				}							

				RTV_DELAY_MS(10);
			}

			if (scan_flag == RTV_SUCCESS) {
				for (i = 0; i < ILoopTH; i++) {
					RTV_DELAY_MS(10);

					RTV_REG_MAP_SEL(OFDM_PAGE);
					CoarseFreq = RTV_REG_GET(0x18);
				
					RTV_REG_MAP_SEL(RF_PAGE); 
					RF00 = RTV_REG_GET(0x00);
					RF00 = (RF00 & 0xC0)>>6;

					/* Scan Power Threshold	*/
					if ((SPower<PWR_TH) && (SPower>0) 
						&& (RF00 != 0x02) && (i > 25)
						&& (((CoarseFreq  > Cfreq_HTH) || (CoarseFreq  < Cfreq_LTH)))) {		
						scan_flag = RTV_CHANNEL_NOT_DETECTED;
						fail = 0xFF13;
						goto DAB_SCAN_EXIT;
					}
					else if ((SPower<(PWR_TH*2)) && (SPower != 0)
						&& (RF00 == 0x02) && (i > 25)
						&& (((CoarseFreq  > Cfreq_HTH) || (CoarseFreq  < Cfreq_LTH)))) {
						scan_flag = RTV_CHANNEL_NOT_DETECTED;
						fail = 0xF403;
						goto DAB_SCAN_EXIT;
					}
					else
						scan_flag = RTV_SUCCESS;

					RTV_REG_MAP_SEL(OFDM_PAGE);
					ASCV = RTV_REG_GET(0x30);
					ASCV = ASCV & 0x0F;
					if ((SCV_C > 4) && (ASCV > 9)) {
						scan_flag = RTV_CHANNEL_NOT_DETECTED;
						fail = 0xFF98;
						goto DAB_SCAN_EXIT;
					}
					else if ((SCV_C > 6) && (ASCV > 8)) {
						scan_flag = RTV_CHANNEL_NOT_DETECTED;
						fail = 0xFF88;
						goto DAB_SCAN_EXIT;
					}										
					else if (((SCV_C > 8) && (ASCV > 7)) && (i > 15)) { /* ASCV count */
						scan_flag = RTV_CHANNEL_NOT_DETECTED;
						fail = 0xFF08;
						goto DAB_SCAN_EXIT;
					}								

					if (ASCV > 7)
						SCV_C++;		

					/* //////////////////////// FSM Monitoring check//////////////////////////////// */
					RTV_REG_MAP_SEL(OFDM_PAGE); 
					MON_FSM = RTV_REG_GET(0x37);
					MON_FSM = MON_FSM & 0x07;

					if ((MON_FSM == 1) && (PreGain < 550) 
						&& (SPower < 3000)) {
						FsmCntChk++;
						if ((NullChCnt > 14) && (dwChFreqKHz< 250000))
							FsmCntChk += 3;
					}

					if ((MON_FSM == 1) && (FsmCntChk > 20) 
						&& (ccnt < 2) &&(i > 30)) {
						scan_flag = RTV_CHANNEL_NOT_DETECTED;									
						fail = 0xFF0A;
						goto DAB_SCAN_EXIT;
					}	

					/* /////////////////////////////////////////////////////////////////////////////// */
					/* ///////////////////////// Coarse Freq. check/////////////////////////////////// */
					/* /////////////////////////////////////////////////////////////////////////////// */
					ccnt = RTV_REG_GET(0x17);	/* Coarse count check */
					ccnt &= 0x1F;

					CoarseFreq = RTV_REG_GET(0x18); 
					
					RTV_REG_MASK_SET(0x82, 0x01, 0x01);
					varyLow = RTV_REG_GET(0x7E);
					varyHigh = RTV_REG_GET(0x7F);
					varyMon = ((varyHigh & 0x1f) << 8) + varyLow; 

					RTV_REG_MASK_SET(0x1C, 0x10, 0x10);
					rdata1 = RTV_REG_GET(0x26);
					rdata2 = RTV_REG_GET(0x27);
					NullLenMon = ((rdata2 & 0x0f) << 8) + rdata1;    
					
					RTV_REG_MASK_SET(0x44, 0x80, 0x80); 	
					rdata0 = RTV_REG_GET(0x4C);
					rdata1 = RTV_REG_GET(0x4D);
					PostAgcMon = ((rdata1 & 0x01)<<8) + rdata0;

					if ((MON_FSM == 6) && (ccnt == 1)
						&& (MonFsm6Cnt > 1) && (i > 15)) {
						scan_flag = RTV_CHANNEL_NOT_DETECTED;
						fail = 0xFFF0;
						goto DAB_SCAN_EXIT;
					}

					if ((MON_FSM == 6) && (ccnt == 1)
						&& ((NullLenMon == 0) || (PostAgcMon > 170) || (PostAgcMon < 65)))
						MonFsm6Cnt++;

					if (ccnt > 1) {
						if (((CoarseFreq  > Cfreq_HTH) || (CoarseFreq  < Cfreq_LTH))
							&& (varyMon < 140))
						{
							for (j=0;j<40;j++) {
								RTV_DELAY_MS(10);
								RTV_REG_MAP_SEL(OFDM_PAGE);	
								OFDM_L = RTV_REG_GET(0x12);
								RTV_REG_MASK_SET(0x82, 0x01, 0x01);	
								varyLow = RTV_REG_GET(0x7E);
								varyHigh = RTV_REG_GET(0x7F);			   
								varyMon = ((varyHigh & 0x1f) << 8) + varyLow; 
								if ((OFDM_L&0x80) && (varyMon > 0)) {
									RTV_REG_MAP_SEL(OFDM_PAGE);
									RTV_REG_SET(0x54,0x58); 
									RTV_REG_MAP_SEL(RF_PAGE);  // RF_AGC speed 
									RTV_REG_SET(0x4B, 0x80);  // BPF BW
									RTV_REG_MAP_SEL(COMM_PAGE);
									RTV_REG_SET(0x55,0x15);   // CO_DC_DEPTH 
									break;
								}
							}
						
							if (OFDM_L&0x80) {
								RTV_REG_MAP_SEL(FEC_PAGE);
								FEC_SYNC = RTV_REG_GET(0xFB);
								rdata0 = RTV_REG_GET(0x83);	
								rdata1 = RTV_REG_GET(0x84);	
								rdata2 = RTV_REG_GET(0x85);	
								rdata3 = RTV_REG_GET(0x86);	
								
								fic_cer_chk = (((rdata2 << 8) + rdata3) * 1000) / ((rdata0<<8) + rdata1);
							//	printk("[1] fic_cer_chk(%u), varyMon(%u), fic_cer_chk_1(%u), fic_cer_chk_2(%u)\n",
							//				fic_cer_chk, varyMon, ((rdata2 << 8) + rdata3), ((rdata0<<8) + rdata1));
								
								FEC_SYNC = FEC_SYNC & 0x03;
								if (FEC_SYNC == 0x03) {		
									if((fic_cer_chk > 195) && (varyMon < 180)) { /* fic cer threshold fix */
										scan_flag = RTV_CHANNEL_NOT_DETECTED;
										fail = 0xF770; 
									}
									else {
										scan_flag = RTV_SUCCESS; /* OFDM_Lock & FEC_Sync OK */
										fail = 0xFF70;	
									}

									goto DAB_SCAN_EXIT;
								}
								else {
									RTV_REG_MAP_SEL(OFDM_PAGE);	
									CoarseFreq= RTV_REG_GET(0x18);
									
									RTV_REG_MAP_SEL(FEC_PAGE);
									rdata0 = RTV_REG_GET(0x83);	
									rdata1 = RTV_REG_GET(0x84);	
									rdata2 = RTV_REG_GET(0x85);	
									rdata3 = RTV_REG_GET(0x86);

									fic_cer_chk = (((rdata2 << 8) + rdata3) * 1000) / ((rdata0<<8) + rdata1);
								//	printk("[2] fic_cer_chk(%u), varyMon(%u), fic_cer_chk_1(%u), fic_cer_chk_2(%u)\n",
								//				fic_cer_chk, varyMon, ((rdata2 << 8) + rdata3), ((rdata0<<8) + rdata1));
							
									scan_flag = RTV_CHANNEL_NOT_DETECTED;
									fail = 0xFF72; /* FEC_Sync miss */
									
									if ((fic_cer_chk < 195) && (varyMon < 180)) { /* fic cer threshold fix */
										scan_flag = RTV_SUCCESS;
										fail = 0xF772; 
										goto DAB_SCAN_EXIT;
									}
								}
							}
							else {
								RTV_REG_MAP_SEL(OFDM_PAGE);	
								CoarseFreq = RTV_REG_GET(0x18);
								scan_flag = RTV_CHANNEL_NOT_DETECTED; /* OFDM_Unlock */
								fail = 0xFF0B;	
							}

							goto DAB_SCAN_EXIT;
						}
					}
				}

				fail = 0xFF0C;
				scan_flag = RTV_CHANNEL_NOT_DETECTED;
				goto DAB_SCAN_EXIT;
			}
		}
	}

	fail = 0xFF0D;

DAB_SCAN_EXIT:

	RTV_GUARD_FREE(demod_no);

	g_fRtvChannelChange = FALSE;

	g_dwDabPrevChFreqKHz = dwChFreqKHz;

	RTV_DBGMSG2("[rtvDAB_ScanFrequency] RF Freq = %d PreGain = %d ",dwChFreqKHz, PreGain);
	RTV_DBGMSG3(" 0x%04X SPower= %d CoarseFreq = %d ", fail,SPower, CoarseFreq);
	RTV_DBGMSG3("   m = %d i = %d j= %d\n",m,i,j);

	return scan_flag;	  /* Auto-scan result return */
}




INT rtvDAB_Initialize(void)
{
	INT nRet;

	g_nDabOpenedSubChNum = 0;
#if defined(RTV_IF_SERIAL_TSIF) || defined(RTV_IF_SPI_SLAVE)
	g_nRtvFicOpenedStatePath = FIC_NOT_OPENED;
#endif
	g_fRtvFicOpened = FALSE;	
	g_dwDabPrevChFreqKHz = 0;		
		
	g_nDabRtvUsedHwSubChIdxBits = 0x00;	
	
	g_fDabEnableReconfigureIntr = FALSE;

	g_nDabRegSubChArrayIdxBits = 0x00;
	
	g_aDabRegSubChIdBits[0] = 0x00000000;
	g_aDabRegSubChIdBits[1] = 0x00000000;

	g_nDabPrevAntennaLevel = 0;

	nRet = rtv_InitSystem(RTV_TV_MODE_DAB_B3, RTV_ADC_CLK_FREQ_8_MHz);
	if(nRet != RTV_SUCCESS)
		return nRet;

	/* Must after rtv_InitSystem() to save ADC clock. */
	dab_InitDemod();
	
	nRet = rtvRF_Initilize(RTV_TV_MODE_DAB_B3);
	if(nRet != RTV_SUCCESS)
		return nRet;

      RTV_DELAY_MS(100);
	  
	RTV_REG_MAP_SEL(RF_PAGE); 
	RTV_REG_SET( 0x6b,  0xC5);

#ifdef RTV_CIF_MODE_ENABLED
	RTV_DBGMSG0("[rtvDAB_Initialize] CIF enabled\n");
#else
	RTV_DBGMSG0("[rtvDAB_Initialize] SINGLE CHANNEL enabled\n");
#endif
	return RTV_SUCCESS;
}

#endif /* #ifdef RTV_DAB_ENABLE */


