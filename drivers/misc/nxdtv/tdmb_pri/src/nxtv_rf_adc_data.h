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
* TITLE 	  : NEXELL TV RF ADC data header file. 
*
* FILENAME    : nxtv_rf_adc_data.h
*
* DESCRIPTION : 
*		All the declarations and definitions necessary for the setting of RF ADC.
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

#if (NXTV_SRC_CLK_FREQ_KHz == 13000)
  static const U8 g_abAdcClkSynTbl[MAX_NUM_NXTV_ADC_CLK_FREQ_TYPE][7] =
  {
	{0x0D, 0x01, 0x1F, 0x27, 0x07, 0x80, 0xB9}, // Based 13MHz, 8MHz 
	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // Based 13MHz, 8.192MHz /* Unsupport Clock */
	{0x0D, 0x01, 0x1F, 0x27, 0x07, 0xB0, 0xB9}, // Based 13MHz, 9MHz 
	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}  // Based 13MHz, 9.6MHz /* Unsupport Clock */
  };

 #ifdef NXTV_ISDBT_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_ISDBT[] = {{0x37, 0x99}, {0x39, 0x9C}};

  static const E_NXTV_ADC_CLK_FREQ_TYPE g_aeAdcClkTypeTbl_ISDBT[] = 
  {	
	NXTV_ADC_CLK_FREQ_8_MHz,	
	NXTV_ADC_CLK_FREQ_9_MHz,
	NXTV_ADC_CLK_FREQ_9_MHz
  };
 #endif

 #ifdef NXTV_TDMB_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_TDMB[] = {{0x37, 0x70}, {0x39, 0x5C}};
 #endif

 #ifdef NXTV_DAB_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_DAB[] = {{0x37, 0x70}, {0x39, 0x5C}};
 #endif

 #ifdef NXTV_FM_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_FM[] = {{0x37, 0x70}, {0x39, 0x6C}};
 #endif

  
#elif (NXTV_SRC_CLK_FREQ_KHz == 16000)
  static const U8 g_abAdcClkSynTbl[MAX_NUM_NXTV_ADC_CLK_FREQ_TYPE][7] =
  {
	{0x04, 0x01, 0x0F, 0x27, 0x07, 0x60, 0xB8},	// Based 16MHz,	8MHz	   External Clock4
	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // Based 16MHz, 8.192MHz   /* Unsupport Clock */
	{0x04, 0x01, 0x0F, 0x27, 0x07, 0x6C, 0xB8}, // Based 16MHz, 9MHz	   External Clock5
	{0x05, 0x01, 0x1F, 0x27, 0x07, 0x90, 0xB8}	// Based 16MHz, 9.6MHz	   External Clock6
  };

 #ifdef NXTV_ISDBT_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_ISDBT[] = {{0x37, 0x7D}, {0x39, 0x7C}};

  static const E_NXTV_ADC_CLK_FREQ_TYPE g_aeAdcClkTypeTbl_ISDBT[] = 
  {	
	NXTV_ADC_CLK_FREQ_8_MHz,	
	NXTV_ADC_CLK_FREQ_9_6_MHz,
	NXTV_ADC_CLK_FREQ_9_MHz
  };
 #endif

 #ifdef NXTV_TDMB_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_TDMB[] = {{0x37, 0x5B}, {0x39, 0x4C}};
 #endif

 #ifdef NXTV_DAB_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_DAB[] = {{0x37, 0x5B}, {0x39, 0x4C}};
 #endif

 #ifdef NXTV_FM_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_FM[] = {{0x37, 0x5B}, {0x39, 0x5C}};
 #endif

	
#elif (NXTV_SRC_CLK_FREQ_KHz == 16384)
  static const U8 g_abAdcClkSynTbl[MAX_NUM_NXTV_ADC_CLK_FREQ_TYPE][7] =
  {
  	{0x10, 0x01, 0x1F, 0x27, 0x07, 0x77, 0xB9},	// Based 16.384MHz,	8MHz	   External Clock8
	{0x04, 0x01, 0x0F, 0x27, 0x07, 0x60, 0xB8},	// Based 16.384MHz,	8.192MHz   External Clock7
	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // Based 16.384MHz, 9MHz       /* Unsupport Clock */
	{0x08, 0x01, 0x1F, 0x27, 0x06, 0xE1, 0xB8}	// Based 16.384MHz,	9.6MHz	   External Clock9
  };

 #ifdef NXTV_ISDBT_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_ISDBT[] = {{0x37, 0x7A}, {0x39, 0x7C}};

  static const E_NXTV_ADC_CLK_FREQ_TYPE g_aeAdcClkTypeTbl_ISDBT[] = 
  {	
	NXTV_ADC_CLK_FREQ_8_MHz,	
	NXTV_ADC_CLK_FREQ_9_6_MHz,
	NXTV_ADC_CLK_FREQ_9_6_MHz
  };
 #endif

 #if defined(NXTV_TDMB_ENABLE) || defined(NXTV_DAB_ENABLE)
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_TDMB[] = {{0x37, 0x59}, {0x39, 0x4C}};
  
  static const E_NXTV_ADC_CLK_FREQ_TYPE g_aeAdcClkTypeTbl_TDMB[] = 
  {	
	NXTV_ADC_CLK_FREQ_8_192_MHz/* 175280: 7A */, NXTV_ADC_CLK_FREQ_8_MHz/* 177008: 7B */,	NXTV_ADC_CLK_FREQ_9_6_MHz/* 178736: 7C */,
	NXTV_ADC_CLK_FREQ_8_192_MHz/* 181280: 8A */,	NXTV_ADC_CLK_FREQ_8_MHz/* 183008: 8B */,	NXTV_ADC_CLK_FREQ_9_6_MHz/* 184736: 8C */,
	NXTV_ADC_CLK_FREQ_8_192_MHz/* 187280: 9A */,	NXTV_ADC_CLK_FREQ_8_MHz/* 189008: 9B */,	NXTV_ADC_CLK_FREQ_9_6_MHz/* 190736: 9C */,
	NXTV_ADC_CLK_FREQ_8_MHz/* 193280: 10A */, NXTV_ADC_CLK_FREQ_8_MHz/* 195008: 10B */, NXTV_ADC_CLK_FREQ_9_6_MHz/* 196736: 10C */,
	NXTV_ADC_CLK_FREQ_9_6_MHz/* 199280: 11A */, NXTV_ADC_CLK_FREQ_8_MHz/* 201008: 11B */, NXTV_ADC_CLK_FREQ_8_MHz/* 202736: 11C */,
	NXTV_ADC_CLK_FREQ_9_6_MHz/* 205280: 12A */, NXTV_ADC_CLK_FREQ_8_MHz/* 207008: 12B */,	NXTV_ADC_CLK_FREQ_9_6_MHz/* 208736: 12C */,
	NXTV_ADC_CLK_FREQ_8_192_MHz/* 211280: 13A */, NXTV_ADC_CLK_FREQ_8_MHz/* 213008: 13B */, NXTV_ADC_CLK_FREQ_8_MHz/* 214736: 13C */
  };	  
 #endif

 #ifdef NXTV_DAB_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_DAB[] = {{0x37, 0x59}, {0x39, 0x4C}};

 static const E_NXTV_ADC_CLK_FREQ_TYPE g_aeAdcClkTypeTbl_DAB_B3[] = 
  {	

  	NXTV_ADC_CLK_FREQ_8_192_MHz/*5A : 174928*/, NXTV_ADC_CLK_FREQ_9_6_MHz/*5B : 176640*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*5C : 178352*/, 
	NXTV_ADC_CLK_FREQ_9_6_MHz/*5D : 180064*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*6A : 181936*/, NXTV_ADC_CLK_FREQ_9_6_MHz/*6B : 183648*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*6C : 185360*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*6D : 187072*/, NXTV_ADC_CLK_FREQ_8_MHz/*7A : 188928*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*7B : 190640*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*7C : 192352*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*7D : 194064*/, 
	NXTV_ADC_CLK_FREQ_8_MHz/*8A : 195936*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*8B : 197648*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*8C : 199360*/, 
	NXTV_ADC_CLK_FREQ_8_MHz/*8D : 201072*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*9A : 202928*/, NXTV_ADC_CLK_FREQ_8_MHz/*9B : 204640*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*9C : 206352*/, NXTV_ADC_CLK_FREQ_9_6_MHz/*9D : 208064*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*10A: 209936*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*10N: 210096*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*10B: 211648*/, NXTV_ADC_CLK_FREQ_8_MHz/*10C: 213360*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*10D: 215072*/, NXTV_ADC_CLK_FREQ_9_6_MHz/*11A: 216928*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*11N: 217008*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*11B: 218640*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*11C: 220352*/, NXTV_ADC_CLK_FREQ_8_MHz/*11D: 222064*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*12A: 223936*/, NXTV_ADC_CLK_FREQ_8_MHz/*12N: 224096*/, NXTV_ADC_CLK_FREQ_8_MHz/*12B: 225648*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*12C: 227360*/, NXTV_ADC_CLK_FREQ_8_MHz/*12D: 229072*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*13A: 230784*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*13B: 232496*/, NXTV_ADC_CLK_FREQ_8_MHz/*13C: 234208*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*13D: 235776*/, 
	NXTV_ADC_CLK_FREQ_8_MHz/*13E: 237488*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*13F: 239200*/
	
  };	  

   static const E_NXTV_ADC_CLK_FREQ_TYPE g_aeAdcClkTypeTbl_DAB_L[] = 
  {	

  	NXTV_ADC_CLK_FREQ_8_192_MHz/*LA: 1452960*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LB: 1454672*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LC: 1456384*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*LD: 1458096*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LE: 1459808*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LF: 1461520*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*LG: 1463232*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LH: 1464944*/, NXTV_ADC_CLK_FREQ_9_6_MHz/*LI: 1466656*/, 
	NXTV_ADC_CLK_FREQ_9_6_MHz/*LJ: 1468368*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LK: 1470080*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LL: 1471792*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*LM: 1473504*/, NXTV_ADC_CLK_FREQ_9_6_MHz/*LN: 1475216*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LO: 1476928*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*LP: 1478640*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LQ: 1480352*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LR: 1482064*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*LS: 1483776*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LT: 1485488*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LU: 1487200*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*LV: 1488912*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LW: 1490624*/
	
  };	  
   
 #endif

 #ifdef NXTV_FM_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_FM[] = {{0x37, 0x59}, {0x39, 0x4C}};
 #endif


#elif (NXTV_SRC_CLK_FREQ_KHz == 18000)	
  static const U8 g_abAdcClkSynTbl[MAX_NUM_NXTV_ADC_CLK_FREQ_TYPE][7] = 
  {
	{0x06, 0x01, 0x13, 0x25, 0x06, 0x80, 0xB4},	// Based 18MHz,	8MHz	   External Clock10
	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // Based 18MHz, 8.192MHz   /* Unsupport Clock */
	{0x06, 0x01, 0x13, 0x25, 0x06, 0x90, 0xB4},	// Based 18MHz,	9MHz	   External Clock11
	{0x05, 0x01, 0x13, 0x25, 0x06, 0x80, 0xB4}	// Based 18MHz,	9.6MHz	   External Clock12
  };
  
 #ifdef NXTV_ISDBT_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_ISDBT[] = {{0x37, 0x6F}, {0x39, 0x6C}};

  static const E_NXTV_ADC_CLK_FREQ_TYPE g_aeAdcClkTypeTbl_ISDBT[] = 
  {	
	NXTV_ADC_CLK_FREQ_8_MHz,	
	NXTV_ADC_CLK_FREQ_9_6_MHz,
	NXTV_ADC_CLK_FREQ_9_MHz
  };
 #endif

 #ifdef NXTV_TDMB_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_TDMB[] = {{0x37, 0x51}, {0x39, 0x4C}};
 #endif

 #ifdef NXTV_DAB_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_DAB[] = {{0x37, 0x51}, {0x39, 0x4C}};
 #endif

 #ifdef NXTV_FM_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_FM[] = {{0x37, 0x51}, {0x39, 0x4C}};
 #endif
  

#elif (NXTV_SRC_CLK_FREQ_KHz == 19200)
  static const U8 g_abAdcClkSynTbl[MAX_NUM_NXTV_ADC_CLK_FREQ_TYPE][7] =
  {
	{0x04, 0x01, 0x0B, 0x23, 0x06, 0x50, 0xB0},	// Based 19.2MHz, 8MHz
	{0x19, 0x01, 0x1F, 0x3A, 0x0A, 0x00, 0xA2},	// Based 19.2MHz, 8.192MHz
	{0x04, 0x01, 0x0B, 0x23, 0x06, 0x5A, 0xB0},	// Based 19.2MHz, 9MHz
	{0x04, 0x01, 0x0B, 0x23, 0x06, 0x60, 0xB0}	// Based 19.2MHz, 9.6MHz
  };

 #ifdef NXTV_ISDBT_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_ISDBT[] = {{0x37, 0x68}, {0x39, 0x6C}};

  static const E_NXTV_ADC_CLK_FREQ_TYPE g_aeAdcClkTypeTbl_ISDBT[] = 
  {	
	NXTV_ADC_CLK_FREQ_8_MHz,	
	NXTV_ADC_CLK_FREQ_9_6_MHz,
	NXTV_ADC_CLK_FREQ_9_MHz
  };
 #endif

 #if defined(NXTV_TDMB_ENABLE) || defined(NXTV_DAB_ENABLE)
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_TDMB[] = {{0x37, 0x4C}, {0x39, 0x3C}};

  static const E_NXTV_ADC_CLK_FREQ_TYPE g_aeAdcClkTypeTbl_TDMB[] = 
  {	
	NXTV_ADC_CLK_FREQ_8_192_MHz/* 175280: 7A */, NXTV_ADC_CLK_FREQ_8_MHz/* 177008: 7B */,	NXTV_ADC_CLK_FREQ_9_MHz/* 178736: 7C */,
	NXTV_ADC_CLK_FREQ_9_MHz/* 181280: 8A */,	NXTV_ADC_CLK_FREQ_8_MHz/* 183008: 8B */,	NXTV_ADC_CLK_FREQ_9_6_MHz/* 184736: 8C */,
	NXTV_ADC_CLK_FREQ_9_MHz/* 187280: 9A */,	NXTV_ADC_CLK_FREQ_8_MHz/* 189008: 9B */,	NXTV_ADC_CLK_FREQ_9_6_MHz/* 190736: 9C */,
	NXTV_ADC_CLK_FREQ_8_192_MHz/* 193280: 10A */, NXTV_ADC_CLK_FREQ_8_MHz/* 195008: 10B */, NXTV_ADC_CLK_FREQ_9_6_MHz/* 196736: 10C */,
	NXTV_ADC_CLK_FREQ_9_MHz/* 199280: 11A */, NXTV_ADC_CLK_FREQ_8_MHz/* 201008: 11B */, NXTV_ADC_CLK_FREQ_8_MHz/* 202736: 11C */,
	NXTV_ADC_CLK_FREQ_9_6_MHz/* 205280: 12A */, NXTV_ADC_CLK_FREQ_8_MHz/* 207008: 12B */,	NXTV_ADC_CLK_FREQ_8_MHz/* 208736: 12C */,
	NXTV_ADC_CLK_FREQ_9_MHz/* 211280: 13A */, NXTV_ADC_CLK_FREQ_8_MHz/* 213008: 13B */, NXTV_ADC_CLK_FREQ_8_MHz/* 214736: 13C */
  };	
 #endif

 #ifdef NXTV_DAB_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_DAB[] = {{0x37, 0x4C}, {0x39, 0x3C}};

  static const E_NXTV_ADC_CLK_FREQ_TYPE g_aeAdcClkTypeTbl_DAB_B3[] = 
  {	

  	NXTV_ADC_CLK_FREQ_8_192_MHz/*5A : 174928*/, NXTV_ADC_CLK_FREQ_9_6_MHz/*5B : 176640*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*5C : 178352*/, 
	NXTV_ADC_CLK_FREQ_9_6_MHz/*5D : 180064*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*6A : 181936*/, NXTV_ADC_CLK_FREQ_9_6_MHz/*6B : 183648*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*6C : 185360*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*6D : 187072*/, NXTV_ADC_CLK_FREQ_8_MHz/*7A : 188928*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*7B : 190640*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*7C : 192352*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*7D : 194064*/, 
	NXTV_ADC_CLK_FREQ_8_MHz/*8A : 195936*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*8B : 197648*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*8C : 199360*/, 
	NXTV_ADC_CLK_FREQ_8_MHz/*8D : 201072*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*9A : 202928*/, NXTV_ADC_CLK_FREQ_8_MHz/*9B : 204640*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*9C : 206352*/, NXTV_ADC_CLK_FREQ_9_6_MHz/*9D : 208064*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*10A: 209936*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*10N: 210096*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*10B: 211648*/, NXTV_ADC_CLK_FREQ_8_MHz/*10C: 213360*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*10D: 215072*/, NXTV_ADC_CLK_FREQ_9_6_MHz/*11A: 216928*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*11N: 217008*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*11B: 218640*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*11C: 220352*/, NXTV_ADC_CLK_FREQ_8_MHz/*11D: 222064*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*12A: 223936*/, NXTV_ADC_CLK_FREQ_8_MHz/*12N: 224096*/, NXTV_ADC_CLK_FREQ_8_MHz/*12B: 225648*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*12C: 227360*/, NXTV_ADC_CLK_FREQ_8_MHz/*12D: 229072*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*13A: 230784*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*13B: 232496*/, NXTV_ADC_CLK_FREQ_8_MHz/*13C: 234208*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*13D: 235776*/, 
	NXTV_ADC_CLK_FREQ_8_MHz/*13E: 237488*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*13F: 239200*/
	
  };	  

   static const E_NXTV_ADC_CLK_FREQ_TYPE g_aeAdcClkTypeTbl_DAB_L[] = 
  {	

  	NXTV_ADC_CLK_FREQ_8_192_MHz/*LA: 1452960*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LB: 1454672*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LC: 1456384*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*LD: 1458096*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LE: 1459808*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LF: 1461520*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*LG: 1463232*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LH: 1464944*/, NXTV_ADC_CLK_FREQ_9_6_MHz/*LI: 1466656*/, 
	NXTV_ADC_CLK_FREQ_9_6_MHz/*LJ: 1468368*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LK: 1470080*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LL: 1471792*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*LM: 1473504*/, NXTV_ADC_CLK_FREQ_9_6_MHz/*LN: 1475216*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LO: 1476928*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*LP: 1478640*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LQ: 1480352*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LR: 1482064*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*LS: 1483776*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LT: 1485488*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LU: 1487200*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*LV: 1488912*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LW: 1490624*/
	
  };	  
   
 #endif

 #ifdef NXTV_FM_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_FM[] = {{0x37, 0x4C}, {0x39, 0x4C}};
 #endif


#elif (NXTV_SRC_CLK_FREQ_KHz == 24000)	
  static const U8 g_abAdcClkSynTbl[MAX_NUM_NXTV_ADC_CLK_FREQ_TYPE][7] =
  {
	{0x06, 0x01, 0x0F, 0x27, 0x07, 0x60, 0xB8},	// Based 24MHz,	8MHz	   External Clock17
	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // Based 24MHz, 8.192MHz   /* Unsupport Clock */
	{0x06, 0x01, 0x0F, 0x27, 0x07, 0x6C, 0xB8},	// Based 24MHz,	9MHz	   External Clock18
	{0x05, 0x01, 0x0B, 0x23, 0x06, 0x60, 0xB0}	// Based 24MHz,	9.6MHz	   External Clock19
  };

 #ifdef NXTV_ISDBT_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_ISDBT[] = {{0x37, 0x53}, {0x39, 0x5C}};

  static const E_NXTV_ADC_CLK_FREQ_TYPE g_aeAdcClkTypeTbl_ISDBT[] = 
  {	
	NXTV_ADC_CLK_FREQ_8_MHz,	
	NXTV_ADC_CLK_FREQ_9_6_MHz,
	NXTV_ADC_CLK_FREQ_9_MHz
  };
 #endif

 #ifdef NXTV_TDMB_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_TDMB[] = {{0x37, 0x3D}, {0x39, 0x3C}};
 #endif

 #ifdef NXTV_DAB_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_DAB[] = {{0x37, 0x3D}, {0x39, 0x3C}};
 #endif

 #ifdef NXTV_FM_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_FM[] = {{0x37, 0x3D}, {0x39, 0x3C}};
 #endif


#elif (NXTV_SRC_CLK_FREQ_KHz == 24576)		
  static const U8 g_abAdcClkSynTbl[MAX_NUM_NXTV_ADC_CLK_FREQ_TYPE][7] =
  {
  	{0x08, 0x01, 0x13, 0x25, 0x06, 0x7D, 0xB4},	// Based 24.576MHz,	8MHz	   External Clock21
	{0x06, 0x01, 0x0F, 0x27, 0x07, 0x60, 0xB8},	// Based 24.576MHz,	8.192MHz   External Clock20
	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // Based 24.576MHz, 9MHz       /* Unsupport Clock */
	{0x0C, 0x01, 0x1F, 0x27, 0x06, 0xE1, 0xB8}  // Based 24.576MHz,	9.6MHz	   External Clock22
  };

 #ifdef NXTV_ISDBT_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_ISDBT[] = {{0x37, 0x51}, {0x39, 0x4C}};

  static const E_NXTV_ADC_CLK_FREQ_TYPE g_aeAdcClkTypeTbl_ISDBT[] = 
  {	
	NXTV_ADC_CLK_FREQ_8_MHz,	
	NXTV_ADC_CLK_FREQ_9_6_MHz,
	NXTV_ADC_CLK_FREQ_8_192_MHz // 2011/04/22
  };
 #endif

 #if defined(NXTV_TDMB_ENABLE) || defined(NXTV_DAB_ENABLE)
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_TDMB[] = {{0x37, 0x3B}, {0x39, 0x2C}};

  static const E_NXTV_ADC_CLK_FREQ_TYPE g_aeAdcClkTypeTbl_TDMB[] = 
  {	
	NXTV_ADC_CLK_FREQ_8_192_MHz/* 175280: 7A */, NXTV_ADC_CLK_FREQ_8_MHz/* 177008: 7B */,	NXTV_ADC_CLK_FREQ_9_6_MHz/* 178736: 7C */,
	NXTV_ADC_CLK_FREQ_8_192_MHz/* 181280: 8A */,	NXTV_ADC_CLK_FREQ_8_MHz/* 183008: 8B */,	NXTV_ADC_CLK_FREQ_9_6_MHz/* 184736: 8C */,
	NXTV_ADC_CLK_FREQ_8_192_MHz/* 187280: 9A */,	NXTV_ADC_CLK_FREQ_8_MHz/* 189008: 9B */,	NXTV_ADC_CLK_FREQ_9_6_MHz/* 190736: 9C */,
	NXTV_ADC_CLK_FREQ_8_192_MHz/* 193280: 10A */, NXTV_ADC_CLK_FREQ_8_MHz/* 195008: 10B */, NXTV_ADC_CLK_FREQ_9_6_MHz/* 196736: 10C */,
	NXTV_ADC_CLK_FREQ_9_6_MHz/* 199280: 11A */, NXTV_ADC_CLK_FREQ_8_MHz/* 201008: 11B */, NXTV_ADC_CLK_FREQ_8_MHz/* 202736: 11C */,
	NXTV_ADC_CLK_FREQ_9_6_MHz/* 205280: 12A */, NXTV_ADC_CLK_FREQ_8_MHz/* 207008: 12B */,	NXTV_ADC_CLK_FREQ_8_192_MHz/* 208736: 12C */,
	NXTV_ADC_CLK_FREQ_8_192_MHz/* 211280: 13A */, NXTV_ADC_CLK_FREQ_8_MHz/* 213008: 13B */, NXTV_ADC_CLK_FREQ_8_MHz/* 214736: 13C */
  };	  
 #endif

 #ifdef NXTV_DAB_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_DAB[] = {{0x37, 0x3B}, {0x39, 0x2C}};

 static const E_NXTV_ADC_CLK_FREQ_TYPE g_aeAdcClkTypeTbl_DAB_B3[] = 
  {	

  	NXTV_ADC_CLK_FREQ_8_192_MHz/*5A : 174928*/, NXTV_ADC_CLK_FREQ_9_6_MHz/*5B : 176640*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*5C : 178352*/, 
	NXTV_ADC_CLK_FREQ_9_6_MHz/*5D : 180064*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*6A : 181936*/, NXTV_ADC_CLK_FREQ_9_6_MHz/*6B : 183648*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*6C : 185360*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*6D : 187072*/, NXTV_ADC_CLK_FREQ_8_MHz/*7A : 188928*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*7B : 190640*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*7C : 192352*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*7D : 194064*/, 
	NXTV_ADC_CLK_FREQ_8_MHz/*8A : 195936*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*8B : 197648*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*8C : 199360*/, 
	NXTV_ADC_CLK_FREQ_8_MHz/*8D : 201072*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*9A : 202928*/, NXTV_ADC_CLK_FREQ_8_MHz/*9B : 204640*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*9C : 206352*/, NXTV_ADC_CLK_FREQ_9_6_MHz/*9D : 208064*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*10A: 209936*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*10N: 210096*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*10B: 211648*/, NXTV_ADC_CLK_FREQ_8_MHz/*10C: 213360*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*10D: 215072*/, NXTV_ADC_CLK_FREQ_9_6_MHz/*11A: 216928*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*11N: 217008*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*11B: 218640*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*11C: 220352*/, NXTV_ADC_CLK_FREQ_8_MHz/*11D: 222064*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*12A: 223936*/, NXTV_ADC_CLK_FREQ_8_MHz/*12N: 224096*/, NXTV_ADC_CLK_FREQ_8_MHz/*12B: 225648*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*12C: 227360*/, NXTV_ADC_CLK_FREQ_8_MHz/*12D: 229072*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*13A: 230784*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*13B: 232496*/, NXTV_ADC_CLK_FREQ_8_MHz/*13C: 234208*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*13D: 235776*/, 
	NXTV_ADC_CLK_FREQ_8_MHz/*13E: 237488*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*13F: 239200*/
	
  };	  

   static const E_NXTV_ADC_CLK_FREQ_TYPE g_aeAdcClkTypeTbl_DAB_L[] = 
  {	

  	NXTV_ADC_CLK_FREQ_8_192_MHz/*LA: 1452960*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LB: 1454672*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LC: 1456384*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*LD: 1458096*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LE: 1459808*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LF: 1461520*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*LG: 1463232*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LH: 1464944*/, NXTV_ADC_CLK_FREQ_9_6_MHz/*LI: 1466656*/, 
	NXTV_ADC_CLK_FREQ_9_6_MHz/*LJ: 1468368*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LK: 1470080*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LL: 1471792*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*LM: 1473504*/, NXTV_ADC_CLK_FREQ_9_6_MHz/*LN: 1475216*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LO: 1476928*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*LP: 1478640*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LQ: 1480352*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LR: 1482064*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*LS: 1483776*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LT: 1485488*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LU: 1487200*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*LV: 1488912*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LW: 1490624*/
	
  };	  
   
 #endif

 #ifdef NXTV_FM_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_FM[] = {{0x37, 0x3B}, {0x39, 0x6C}};
 #endif

  
#elif (NXTV_SRC_CLK_FREQ_KHz == 26000)
  static const U8 g_abAdcClkSynTbl[MAX_NUM_NXTV_ADC_CLK_FREQ_TYPE][7] =
  {
	{0x0D, 0x01, 0x1F, 0x27, 0x06, 0xC0, 0xB8}, // Based 26MHz,	8MHz       External Clock23
	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // Based 26MHz, 8.192MHz   /* Unsupport Clock */
	{0x0D, 0x01, 0x1F, 0x27, 0x06, 0xD8, 0xB8}  // Based 26MHz,	9MHz	   External Clock24
	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // Based 26MHz, 9.6MHz     /* Unsupport Clock */
  };

 #ifdef NXTV_ISDBT_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_ISDBT[] = {{0x37, 0x4C}, {0x39, 0x4C}};

  static const E_NXTV_ADC_CLK_FREQ_TYPE g_aeAdcClkTypeTbl_ISDBT[] = 
  {	
	NXTV_ADC_CLK_FREQ_8_MHz,	
	NXTV_ADC_CLK_FREQ_9_MHz,
	NXTV_ADC_CLK_FREQ_9_MHz
  };
 #endif

 #ifdef NXTV_TDMB_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_TDMB[] = {{0x37, 0x38}, {0x39, 0x2C}};
 #endif

 #ifdef NXTV_DAB_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_DAB[] = {{0x37, 0x38}, {0x39, 0x2C}};
 #endif

 #ifdef NXTV_FM_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_FM[] = {{0x37, 0x38}, {0x39, 0x3C}};
 #endif
 
    
#elif (NXTV_SRC_CLK_FREQ_KHz == 27000)
  static const U8 g_abAdcClkSynTbl[MAX_NUM_NXTV_ADC_CLK_FREQ_TYPE][7] =
  {
	{0x09, 0x01, 0x13, 0x25, 0x06, 0x80, 0xB4}, // Based 27MHz,	8MHz	   External Clock25
	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // Based 27MHz, 8.192MHz   /* Unsupport Clock */
	{0x06, 0x01, 0x0F, 0x27, 0x07, 0x60, 0xB8}  // Based 27MHz,	9MHz	   External Clock26
	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // Based 27MHz, 9.6MHz     /* Unsupport Clock */
  };

 #ifdef NXTV_ISDBT_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_ISDBT[] = {{0x37, 0x4A}, {0x39, 0x4C}};

  static const E_NXTV_ADC_CLK_FREQ_TYPE g_aeAdcClkTypeTbl_ISDBT[] = 
  {	
	NXTV_ADC_CLK_FREQ_8_MHz,	
	NXTV_ADC_CLK_FREQ_9_MHz,
	NXTV_ADC_CLK_FREQ_9_MHz
  };
 #endif

 #ifdef NXTV_TDMB_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_TDMB[] = {{0x37, 0x36}, {0x39, 0x2C}};
 #endif

 #ifdef NXTV_DAB_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_DAB[] = {{0x37, 0x36}, {0x39, 0x2C}};
 #endif

 #ifdef NXTV_FM_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_FM[] = {{0x37, 0x36}, {0x39, 0x2C}};
 #endif


#elif (NXTV_SRC_CLK_FREQ_KHz == 32000)
  static const U8 g_abAdcClkSynTbl[MAX_NUM_NXTV_ADC_CLK_FREQ_TYPE][7] =
  {
	{0x08, 0x01, 0x0F, 0x27, 0x07, 0x60, 0xB8}, // Based 32MHz,	8MHz	   External Clock27
	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // Based 32MHz, 8.192MHz  /* Unsupport Clock */
	{0x08, 0x01, 0x0F, 0x27, 0x07, 0x6C, 0xB8}, // Based 32MHz,	9MHz	   External Clock28
	{0x0A, 0x01, 0x1F, 0x27, 0x07, 0x90, 0xB8}  // Based 32MHz,	9.6MHz	   External Clock29
  };

 #ifdef NXTV_ISDBT_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_ISDBT[] = {{0x37, 0x3E}, {0x39, 0x3C}};

  static const E_NXTV_ADC_CLK_FREQ_TYPE g_aeAdcClkTypeTbl_ISDBT[] = 
  {	
	NXTV_ADC_CLK_FREQ_8_MHz,	
	NXTV_ADC_CLK_FREQ_9_6_MHz,
	NXTV_ADC_CLK_FREQ_9_MHz
  };
 #endif

 #ifdef NXTV_TDMB_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_TDMB[] = {{0x37, 0x2D}, {0x39, 0x2C}};
 #endif

 #ifdef NXTV_DAB_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_DAB[] = {{0x37, 0x2D}, {0x39, 0x2C}};
 #endif

 #ifdef NXTV_FM_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_FM[] = {{0x37, 0x2D}, {0x39, 0x2C}};
 #endif


#elif (NXTV_SRC_CLK_FREQ_KHz == 32768)
  static const U8 g_abAdcClkSynTbl[MAX_NUM_NXTV_ADC_CLK_FREQ_TYPE][7] =
  {
	{0x20, 0x01, 0x1F, 0x27, 0x07, 0x77, 0xB9}, // Based 32.768MHz,	8MHz	   External Clock31
	{0x08, 0x01, 0x0F, 0x27, 0x07, 0x60, 0xB8}, // Based 32.768MHz,	8.192MHz   External Clock30
	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // Based 32.768MHz, 9MHz       /* Unsupport Clock */
	{0x10, 0x01, 0x1F, 0x27, 0x06, 0xE1, 0xB8}  // Based 32.768MHz,	9.6MHz	   External Clock32
  };

 #ifdef NXTV_ISDBT_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_ISDBT[] = {{0x37, 0x3D}, {0x39, 0x3C}};

   static const E_NXTV_ADC_CLK_FREQ_TYPE g_aeAdcClkTypeTbl_ISDBT[] = 
  {	
	NXTV_ADC_CLK_FREQ_8_MHz,	
	NXTV_ADC_CLK_FREQ_9_6_MHz,
	NXTV_ADC_CLK_FREQ_9_6_MHz
  };
 #endif

 #ifdef NXTV_TDMB_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_TDMB[] = {{0x37, 0x2C}, {0x39, 0x2C}};
 #endif

 #ifdef NXTV_DAB_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_DAB[] = {{0x37, 0x2C}, {0x39, 0x2C}};
 #endif

 #ifdef NXTV_FM_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_FM[] = {{0x37, 0x2C}, {0x39, 0x2C}};
 #endif


#elif (NXTV_SRC_CLK_FREQ_KHz == 36000)
  static const U8 g_abAdcClkSynTbl[MAX_NUM_NXTV_ADC_CLK_FREQ_TYPE][7] =
  {
	{0x09, 0x01, 0x0F, 0x27, 0x07, 0x60, 0xB8}, // Based 36MHz, 8MHz	   External Clock33
	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // Based 36MHz,	8.192MHz  /* Unsupport */
	{0x09, 0x01, 0x0F, 0x27, 0x07, 0x6C, 0xB8}, // Based 36MHz, 9MHz	   External Clock34
	{0x0A, 0x01, 0x13, 0x25, 0x06, 0x80, 0xB4}  // Based 36MHz, 9.6MHz	   External Clock35
  };
  
 #ifdef NXTV_ISDBT_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_ISDBT[] = {{0x37, 0x37}, {0x39, 0x3C}};

  static const E_NXTV_ADC_CLK_FREQ_TYPE g_aeAdcClkTypeTbl_ISDBT[] = 
  {	
	NXTV_ADC_CLK_FREQ_8_MHz,	
	NXTV_ADC_CLK_FREQ_9_6_MHz,
	NXTV_ADC_CLK_FREQ_9_MHz
  };
 #endif

 #if defined(NXTV_TDMB_ENABLE) || defined(NXTV_DAB_ENABLE)
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_TDMB[] = {{0x37, 0x28}, {0x39, 0x2C}};

  // Temp!!!
  static const E_NXTV_ADC_CLK_FREQ_TYPE g_aeAdcClkTypeTbl_TDMB[] = // temp 
  {  
    NXTV_ADC_CLK_FREQ_8_MHz/* 175280: 7A */, NXTV_ADC_CLK_FREQ_8_MHz/* 177008: 7B */,	NXTV_ADC_CLK_FREQ_9_6_MHz/* 178736: 7C */,
	NXTV_ADC_CLK_FREQ_8_MHz/* 181280: 8A */,	NXTV_ADC_CLK_FREQ_8_MHz/* 183008: 8B */,	NXTV_ADC_CLK_FREQ_9_6_MHz/* 184736: 8C */,
	NXTV_ADC_CLK_FREQ_8_MHz/* 187280: 9A */,	NXTV_ADC_CLK_FREQ_8_MHz/* 189008: 9B */,	NXTV_ADC_CLK_FREQ_9_6_MHz/* 190736: 9C */,
	NXTV_ADC_CLK_FREQ_8_MHz/* 193280: 10A */, NXTV_ADC_CLK_FREQ_8_MHz/* 195008: 10B */, NXTV_ADC_CLK_FREQ_9_6_MHz/* 196736: 10C */,
	NXTV_ADC_CLK_FREQ_9_6_MHz/* 199280: 11A */, NXTV_ADC_CLK_FREQ_8_MHz/* 201008: 11B */, NXTV_ADC_CLK_FREQ_8_MHz/* 202736: 11C */,
	NXTV_ADC_CLK_FREQ_9_6_MHz/* 205280: 12A */, NXTV_ADC_CLK_FREQ_8_MHz/* 207008: 12B */,	NXTV_ADC_CLK_FREQ_8_MHz/* 208736: 12C */,
	NXTV_ADC_CLK_FREQ_8_MHz/* 211280: 13A */, NXTV_ADC_CLK_FREQ_8_MHz/* 213008: 13B */, NXTV_ADC_CLK_FREQ_8_MHz/* 214736: 13C */
  }; 

 #endif

 #ifdef NXTV_DAB_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_DAB[] = {{0x37, 0x28}, {0x39, 0x2C}};


 static const E_NXTV_ADC_CLK_FREQ_TYPE g_aeAdcClkTypeTbl_DAB_B3[] = 
  {	

  	NXTV_ADC_CLK_FREQ_8_192_MHz/*5A : 174928*/, NXTV_ADC_CLK_FREQ_9_6_MHz/*5B : 176640*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*5C : 178352*/, 
	NXTV_ADC_CLK_FREQ_9_6_MHz/*5D : 180064*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*6A : 181936*/, NXTV_ADC_CLK_FREQ_9_6_MHz/*6B : 183648*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*6C : 185360*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*6D : 187072*/, NXTV_ADC_CLK_FREQ_8_MHz/*7A : 188928*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*7B : 190640*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*7C : 192352*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*7D : 194064*/, 
	NXTV_ADC_CLK_FREQ_8_MHz/*8A : 195936*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*8B : 197648*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*8C : 199360*/, 
	NXTV_ADC_CLK_FREQ_8_MHz/*8D : 201072*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*9A : 202928*/, NXTV_ADC_CLK_FREQ_8_MHz/*9B : 204640*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*9C : 206352*/, NXTV_ADC_CLK_FREQ_9_6_MHz/*9D : 208064*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*10A: 209936*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*10N: 210096*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*10B: 211648*/, NXTV_ADC_CLK_FREQ_8_MHz/*10C: 213360*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*10D: 215072*/, NXTV_ADC_CLK_FREQ_9_6_MHz/*11A: 216928*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*11N: 217008*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*11B: 218640*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*11C: 220352*/, NXTV_ADC_CLK_FREQ_8_MHz/*11D: 222064*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*12A: 223936*/, NXTV_ADC_CLK_FREQ_8_MHz/*12N: 224096*/, NXTV_ADC_CLK_FREQ_8_MHz/*12B: 225648*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*12C: 227360*/, NXTV_ADC_CLK_FREQ_8_MHz/*12D: 229072*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*13A: 230784*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*13B: 232496*/, NXTV_ADC_CLK_FREQ_8_MHz/*13C: 234208*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*13D: 235776*/, 
	NXTV_ADC_CLK_FREQ_8_MHz/*13E: 237488*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*13F: 239200*/
	
  };	  

   static const E_NXTV_ADC_CLK_FREQ_TYPE g_aeAdcClkTypeTbl_DAB_L[] = 
  {	

  	NXTV_ADC_CLK_FREQ_8_192_MHz/*LA: 1452960*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LB: 1454672*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LC: 1456384*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*LD: 1458096*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LE: 1459808*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LF: 1461520*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*LG: 1463232*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LH: 1464944*/, NXTV_ADC_CLK_FREQ_9_6_MHz/*LI: 1466656*/, 
	NXTV_ADC_CLK_FREQ_9_6_MHz/*LJ: 1468368*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LK: 1470080*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LL: 1471792*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*LM: 1473504*/, NXTV_ADC_CLK_FREQ_9_6_MHz/*LN: 1475216*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LO: 1476928*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*LP: 1478640*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LQ: 1480352*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LR: 1482064*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*LS: 1483776*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LT: 1485488*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LU: 1487200*/, 
	NXTV_ADC_CLK_FREQ_8_192_MHz/*LV: 1488912*/, NXTV_ADC_CLK_FREQ_8_192_MHz/*LW: 1490624*/
	
  };	  
   
 #endif

 #ifdef NXTV_FM_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_FM[] = {{0x37, 0x28}, {0x39, 0x2C}};
 #endif

  
#elif (NXTV_SRC_CLK_FREQ_KHz == 38400)
  static const U8 g_abAdcClkSynTbl[MAX_NUM_NXTV_ADC_CLK_FREQ_TYPE][7] =
  {
	{0x08, 0x01, 0x0B, 0x23, 0x06, 0x50, 0xB0}, // Based 38.4MHz, 8MHz	     External Clock36
	{0x19, 0x01, 0x1F, 0x27, 0x06, 0x00, 0xB9}, // Based 38.4MHz, 8.192MHz   External Clock37
	{0x08, 0x01, 0x0B, 0x23, 0x06, 0x5A, 0xB0}, // Based 38.4MHz, 9MHz	     External Clock38
	{0x0A, 0x01, 0x0F, 0x27, 0x07, 0x78, 0xB8}  // Based 38.4MHz, 9.6MHz	 External Clock39
  };

 #ifdef NXTV_ISDBT_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_ISDBT[] = {{0x37, 0x34}, {0x39, 0x3C}};

  static const E_NXTV_ADC_CLK_FREQ_TYPE g_aeAdcClkTypeTbl_ISDBT[] = 
  {	
	NXTV_ADC_CLK_FREQ_8_MHz,	
	NXTV_ADC_CLK_FREQ_9_6_MHz,
	NXTV_ADC_CLK_FREQ_9_MHz
  };
 #endif

 #ifdef NXTV_TDMB_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_TDMB[] = {{0x37, 0x26}, {0x39, 0x2C}};
 #endif

 #ifdef NXTV_DAB_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_DAB[] = {{0x37, 0x26}, {0x39, 0x2C}};
 #endif

 #ifdef NXTV_FM_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_FM[] = {{0x37, 0x26}, {0x39, 0x2C}};
 #endif


#elif (NXTV_SRC_CLK_FREQ_KHz == 40000)
  static const U8 g_abAdcClkSynTbl[MAX_NUM_NXTV_ADC_CLK_FREQ_TYPE][7] =
  {
	{0x0A, 0x01, 0x0F, 0x27, 0x07, 0x60, 0xB8}, // Based 40MHz,	8MHz	   External Clock40
	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // Based 40MHz, 8.192MHz   /* Unsupport */
	{0x0A, 0x01, 0x0F, 0x27, 0x07, 0x6C, 0xB8}, // Based 40MHz,	9MHz	   External Clock41
	{0x19, 0x01, 0x1F, 0x27, 0x06, 0x20, 0xB9}  // Based 40MHz,	9.6MHz	   External Clock42
  };

 #ifdef NXTV_ISDBT_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_ISDBT[] = {{0x37, 0x32}, {0x39, 0x3C}};

  static const E_NXTV_ADC_CLK_FREQ_TYPE g_aeAdcClkTypeTbl_ISDBT[] = 
  {	
	NXTV_ADC_CLK_FREQ_8_MHz,	
	NXTV_ADC_CLK_FREQ_9_6_MHz,
	NXTV_ADC_CLK_FREQ_9_MHz
  };
 #endif

 #ifdef NXTV_TDMB_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_TDMB[] = {{0x37, 0x24}, {0x39, 0x2C}};
 #endif

 #ifdef NXTV_DAB_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_DAB[] = {{0x37, 0x24}, {0x39, 0x2C}};
 #endif

 #ifdef NXTV_FM_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_FM[] = {{0x37, 0x24}, {0x39, 0x2C}};
 #endif


#elif (NXTV_SRC_CLK_FREQ_KHz == 48000)
  static const U8 g_abAdcClkSynTbl[MAX_NUM_NXTV_ADC_CLK_FREQ_TYPE][7] =
  {
	{0x0C, 0x01, 0x0F, 0x27, 0x07, 0x60, 0xB8}, // Based 48MHz,	8MHz       External Clock43
	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, // Based 48MHz, 8.192MHz   /* Unsupport */
	{0x0C, 0x01, 0x0F, 0x27, 0x07, 0x6C, 0xB8}, // Based 48MHz,	9MHz       External Clock44
	{0x0A, 0x01, 0x0B, 0x23, 0x06, 0x60, 0xB0}  // Based 48MHz,	9.6MHz     External Clock45
  };

 #ifdef NXTV_ISDBT_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_ISDBT[] = {{0x37, 0x29}, {0x39, 0x2C}};

  static const E_NXTV_ADC_CLK_FREQ_TYPE g_aeAdcClkTypeTbl_ISDBT[] = 
  {	
	NXTV_ADC_CLK_FREQ_8_MHz,	
	NXTV_ADC_CLK_FREQ_9_6_MHz,
	NXTV_ADC_CLK_FREQ_9_MHz
  };
 #endif

 #ifdef NXTV_TDMB_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_TDMB[] = {{0x37, 0x1E}, {0x39, 0x1C}};
 #endif

 #ifdef NXTV_DAB_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_DAB[] = {{0x37, 0x1E}, {0x39, 0x1C}};
 #endif

 #ifdef NXTV_FM_ENABLE
  static const NXTV_REG_INIT_INFO g_atAutoLnaInitData_FM[] = {{0x37, 0x1E}, {0x39, 0x2C}};
 #endif

#else
	#error "Unsupport external clock freqency!"
#endif




