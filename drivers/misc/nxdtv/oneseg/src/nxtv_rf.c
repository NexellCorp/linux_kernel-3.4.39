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
* TITLE 	  : NEXELL TV RF services source file. 
*
* FILENAME    : nxtv_rf.c
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
* 10/01/2010  Ko, Kevin        Added the auto channel setting for TDMB.
* 09/29/2010  Ko, Kevin        Modified the auto pll index comments.
* 09/27/2010  Ko, Kevin        Creat for CS Realease
*             /Yang, Maverick  1.Reformating for CS API
*                              2.pll table, ADC clock switching, SCAN function, 
*								 FM function added..
* 04/09/2010  Yang, Maverick   REV1 SETTING 
* 01/25/2010  Yang, Maverick   Created.                                                              
********************************************************************************/

#include "nxtv_rf.h"

// Include only once.
#if defined(NXTV_ISDBT_ENABLE)
	#include "nxtv_rf_pll_data_isdbt.h"
#endif
	
#if defined(NXTV_TDMB_ENABLE)
	#include "nxtv_rf_pll_data_tdmb.h"
#endif

#if defined(NXTV_FM_ENABLE)
    #include "nxtv_rf_pll_data_fm.h"
#endif
	
#if defined(NXTV_DAB_ENABLE)	
	#include "nxtv_rf_pll_data_dab.h"
	#include "nxtv_rf_pll_data_tdmb.h"
#endif

#include "nxtv_rf_adc_data.h"


/* Down conversion Signal Monitoring. */
//#define DEBUG_A_TEST_ZERO


#ifdef NXTV_ISDBT_ENABLE

 static const NXTV_REG_INIT_INFO t_ISDBT_INIT[] =   
 {
	{  0x27,  0x6a  },
	{  0x2a,  0x07  },
	{  0x2B,  0x88  },
	{  0x2d,  0xec  },
	{  0x2e,  0xb0  },
	{  0x31,  0x04  },
	{  0x34,  0xc0  },
	{  0x3a,  0x77  },
	{  0x3b,  0xff  },
	{  0x3c,  0x79  },
	{  0x3e,  0x67  },
	{  0x3f,  0x00  },
	{  0x42,  0x14  },
	{  0x44,  0x40  },
	{  0x47,  0xc0  },
	{  0x49,  0x4f  },
	{  0x4a,  0x10  },
	{  0x4b,  0x80  },
#ifdef DEBUG_A_TEST_ZERO
	{  0x50,  0x88  },	
#endif
	{  0x53,  0x20  },	
	{  0x55,  0xfc  },
	{  0x57,  0x10  },
	{  0x5a,  0x83  },
	{  0x60,  0x13  },
	{  0x6b,  0x85  },
 #if defined(NXTV_CHIP_PKG_QFN)
	{  0x6c,  0x8d  },  
	{  0x6d,  0x7d  },  

 #elif defined (NXTV_CHIP_PKG_WLCSP)
	{  0x6c,  0x80  },  
	{  0x6d,  0x70  },  

 #elif defined (NXTV_CHIP_PKG_LGA)
	{  0x6c,  0x80  },  
	{  0x6d,  0x70  },  
 #else
	#error "Code not present"
 #endif	
	{  0x72,  0xb0  },
	{  0x73,  0xca  },
	{  0x77,  0x89  },
	{  0x84,  0x87  },
	{  0x85,  0x95  },
	{  0x86,  0x42  },
	{  0x87,  0x60  },
	{  0x8a,  0xf6  },
	{  0x8b,  0x89  },
	{  0x8c,  0x78  },
	{  0x90,  0x07  },
	{  0x93,  0x2F  },
	{  0xb5,  0x1b  },
	{  0xae,  0x37  },
	{  0xc0,  0x31  },
	{  0xc1,  0xe8  },
	{  0xc3,  0xa2  },
	{  0xc4,  0xac  },
	{  0xc6,  0xeb  },
	{  0xca,  0x38  },
	{  0xcb,  0x8c  },
	{  0xcd,  0xa1  },
	{  0xce,  0xfc  },
	{  0xd0,  0x3f  },
	{  0xd4,  0x13  },
	{  0xd5,  0xf9  },
	{  0xd7,  0xa6  },
	{  0xd8,  0xac  },
	{  0xd9,  0x16  },
	{  0xda,  0x79  },
	{  0xde,  0x37  },
	{  0xdf,  0x3d  },
	{  0xe1,  0xa0  },
	{  0xe2,  0x0c  },
	{  0xe4,  0x3a  },
	{  0xa5,  0x00  },
	{  0xe9,  0xc1  }, // CKO_D24_EN = 0, CK_16M clock, SET_IIR clock disable
	{  0xae,  0x77  },
	{  0xe9,  0xd1  } // CKO_D24_EN = 1, CK_16M clock, SET_IIR clock enable
 };	
#endif /* NXTV_ISDBT_ENABLE */


#ifdef NXTV_FM_ENABLE
 static const NXTV_REG_INIT_INFO t_FM_INIT[] =   
 {
	{  0x27,  0xc2  },
	{  0x2B,  0x88  },
	{  0x2d,  0xec  },
	{  0x2e,  0xb0  },
	{  0x31,  0x04  },
	{  0x34,  0xf8  },
	{  0x3a,  0x55  },
	{  0x3b,  0x73  },
	{  0x3c,  0x00  },
	{  0x3e,  0x00  },
	{  0x3f,  0x00  },
	{  0x40,  0x10  },
	{  0x42,  0x40  },
	{  0x44,  0x64  },
	{  0x47,  0xb1  },
	{  0x48,  0x10  },
	{  0x4b,  0x80  },
#ifdef DEBUG_A_TEST_ZERO
	{  0x50,  0x88	},	
#endif
	{  0x53,  0x20  },
	{  0x55,  0xd6  },
	{  0x5a,  0x83  },
	{  0x57,  0x10  },
	{  0x60,  0x11  },
	{  0x6b,  0xc5  },
	{  0x72,  0xf0  },
	{  0x73,  0xc8  },
	{  0x74,  0x70  },
	{  0x77,  0x80  },
	{  0x78,  0x47  },
	{  0x86,  0x80  },
	{  0x87,  0x98  },
	{  0x8a,  0xf6  },
	{  0x8b,  0x80  },
	{  0x8c,  0x74  },
	{  0xaf,  0x01  },
	{  0xb5,  0x5b  },
	{  0xae,  0x37  },
	{  0xbd,  0x3f  },
	{  0xbe,  0x37  },
	{  0xbf,  0x3c  },
	{  0xc0,  0x3f  },
	{  0xc1,  0x39  },
	{  0xc3,  0xdb  },
	{  0xc4,  0x3c  },
	{  0xc5,  0x1e  },
	{  0xc6,  0x6c  },
	{  0xc8,  0x7b  },
	{  0xc9,  0xec  },
	{  0xca,  0x39  },
	{  0xcb,  0x03  },
	{  0xcd,  0xb4  },
	{  0xce,  0xec  },
	{  0xcf,  0x1d  },
	{  0xd0,  0xa6  },
	{  0xd1,  0x41  },
	{  0xd2,  0x16  },
	{  0xd3,  0xec  },
	{  0xd4,  0x3f  },
	{  0xd5,  0xc3  },
	{  0xd7,  0xbd  },
	{  0xd8,  0xbc  },
	{  0xd9,  0x1b  },
	{  0xda,  0x4b  },
	{  0xdb,  0x3b  },
	{  0xdc,  0x55  },
	{  0xdd,  0xdc  },
	{  0xde,  0x1a  },
	{  0xdf,  0xed  },
	{  0xe1,  0xa7  },
	{  0xe2,  0xcc  },
	{  0xe3,  0x1a  },
	{  0xe4,  0x55  },
	{  0xe5,  0x43  },
	{  0xe6,  0x00  },
	{  0xe7,  0x0f  },
	{  0xa5,  0x00  },
	{  0xe9,  0x41  },
	{  0xae,  0x77  },
	{  0xe9,  0x51  }
 };	
#endif /* NXTV_FM_ENABLE */


#ifdef NXTV_DAB_ENABLE
static const NXTV_REG_INIT_INFO t_DAB_INIT[] =   
 {
	{  0x27,  0x96  },
	{  0x2B,  0x88  },
	{  0x2d,  0xec  },
	{  0x2e,  0xb0  },
	{  0x31,  0x04  },
	{  0x34,  0xf8  },
	{  0x35,  0x14  },
	{  0x3a,  0x62  },	
	{  0x3b,  0x74  },
	{  0x3c,  0xa8  },	
	{  0x43,  0x36  },
	{  0x44,  0x70  },
	{  0x46,  0x07  },
	{  0x49,  0x1f  },
	{  0x4a,  0x50  },
	{  0x4b,  0x80  },
#ifdef DEBUG_A_TEST_ZERO
	{  0x50,  0x88	},	
#endif
	{  0x53,  0x20  },
	{  0x55,  0xd6  },
	{  0x5a,  0x83  },
	{  0x60,  0x13  },
	{  0x6b,  0x85  },
	{  0x6e,  0x88  },
	{  0x6f,  0x78  },
	{  0x72,  0xf0  },
	{  0x73,  0xca  },
	{  0x77,  0x80  },
	{  0x78,  0x47  },
	{  0x84,  0x80  },
	{  0x85,  0x90  },
	{  0x86,  0x80  },
	{  0x87,  0x98  },
	{  0x8a,  0xf6  },
	{  0x8b,  0x80  },
	{  0x8c,  0x75  },
	{  0x8f,  0xf9  },
	{  0x90,  0x91  },
	{  0x93,  0x2d  },
	{  0x94,  0x23  },
	{  0x99,  0x77  },
	{  0x9a,  0x2d  },
	{  0x9b,  0x1e  },
	{  0x9c,  0x47  },
	{  0x9d,  0x3a  },
	{  0x9e,  0x03  },
	{  0x9f,  0x1e  },
	{  0xa0,  0x22  },
	{  0xa1,  0x33  },
	{  0xa2,  0x51  },
	{  0xa3,  0x36  },
	{  0xa4,  0x0c  },
	{  0xae,  0x37  },
	{  0xb5,  0x9b  },
	{  0xbd,  0x45  },	 
	{  0xbe,  0x68  },	
	{  0xbf,  0x5c  },	
	{  0xc0,  0x33  },	 
	{  0xc1,  0xca  },	
	{  0xc2,  0x43  },	
	{  0xc3,  0x90  },	 
	{  0xc4,  0xec  },   
	{  0xc5,  0x17  },   
	{  0xc6,  0xda  },   
	{  0xc7,  0x41  },   
	{  0xc8,  0x49  }, 
	{  0xc9,  0xbc  }, 
	{  0xca,  0x3a  },   
	{  0xcb,  0x20  },   
	{  0xcc,  0x43  },   
	{  0xcd,  0xa6  },   
	{  0xce,  0x4c  },   
	{  0xcf,  0x1f  },   
	{  0xd0,  0x8d  },   
	{  0xd1,  0x3d  },   
	{  0xd2,  0xdf  }, 
	{  0xd3,  0xa4  }, 
	{  0xd4,  0x30  },   
	{  0xd5,  0x00  }, 
	{  0xd6,  0x41  },
	{  0xd7,  0x81  },  
	{  0xd8,  0xd0  }, 
	{  0xd9,  0x00  },
	{  0xda,  0x00  }, 
	{  0xdb,  0x41  },
	{  0xdc,  0x6d  },
	{  0xdd,  0xac  },
	{  0xde,  0x39  },
	{  0xdf,  0x4e  },
	{  0xe0,  0x43  },
	{  0xe1,  0xa0  },
	{  0xe2,  0x4c  },
	{  0xe3,  0x1d  },
	{  0xe4,  0x99  },
	{  0xe5,  0x3b  },
	{  0xe6,  0x00  },
	{  0xe7,  0x0d  },
	{  0xa5,  0x00  },
	{  0xe9,  0x41  },
	{  0xae,  0x77  },
	{  0xe9,  0x51  }
 }; 

 static const NXTV_REG_INIT_INFO t_BAND3_INIT[] =   
 {
	{  0x27,	0x96  },
	{  0x34,	0xf8  },
	{  0x3b,	0x74  },
	{  0x43,	0x36  },
	{  0x44,	0x70  },
	{  0x46,	0x07  },
	{  0x49,	0x1f  },
	{  0x4a,	0x50  },
#ifdef DEBUG_A_TEST_ZERO
	{  0x50,  0x88	},	
#endif
	{  0x55,	0xd6  },
	{  0x60,	0x13  },
	{  0x6e,	0x88  },
	{  0x6f,	0x78  },
	{  0x84,	0x80  },
	{  0x85,	0x90  },
	{  0x86,	0x80  },
	{  0x87,	0x98  },
	{  0x8b,	0x80  },
	{  0x8c,	0x75  },
	{  0x8f,	0xf9  },
	{  0x90,	0x91  },
	{  0x93,	0x2d  },
	{  0x94,	0x23  }
 };	

#ifdef NXTV_DAB_LBAND_ENABLED
 static const NXTV_REG_INIT_INFO t_LBAND_INIT[] =   
 {
	{  0x27,  0x3e  },
	{  0x34,  0x70  },
	{  0x3b,  0xf7  },
	{  0x43,  0x34  },
	{  0x44,  0x00  },
	{  0x46,  0x03  },
	{  0x49,  0xbf  },
	{  0x4a,  0x18  },
#ifdef DEBUG_A_TEST_ZERO
	{  0x50,  0x88	},	
#endif
	{  0x55,  0xfa  },
	{  0x60,  0x43  },
	{  0x6e,  0x80  },
	{  0x6f,  0x70  },
	{  0x84,  0x70  },
	{  0x85,  0x98  },
	{  0x86,  0x70  },
	{  0x87,  0x88  },
	{  0x8b,  0x86  },
	{  0x8c,  0x7a  },
	{  0x8f,  0xfa  },
	{  0x90,  0x05  },
	{  0x93,  0x2f  },
	{  0x94,  0x26  }
 };
#endif /* NXTV_DAB_LBAND_ENABLED */

 static const NXTV_REG_INIT_INFO t_DAB_TDMB_INIT[] =
 {
	{  0x27,	0x96  },
	{  0x34,	0xf8  },
	{  0x3a,    0x62  },
	{  0x3b,	0x74  },
	{  0x3c,    0x81  },
	{  0x3d,    0x48  },
	{  0x43,	0x36  },
	{  0x44,	0x70  },
	{  0x46,	0x07  },
	{  0x49,	0x1f  },
	{  0x4a,	0x50  },
#ifdef DEBUG_A_TEST_ZERO
	{  0x50,  0x88	},	
#endif
	{  0x55,	0xd6  },
	{  0x60,	0x13  },
	{  0x6e,	0x88  },
	{  0x6f,	0x78  },
	{  0x84,	0x80  },
	{  0x85,	0x90  },
	{  0x86,	0x80  },
	{  0x87,	0x98  },
	{  0x8b,	0x80  },
	{  0x8c,	0x75  },
	{  0x8f,	0xf9  },
	{  0x90,	0x91  },
	{  0x93,	0x2d  },
	{  0x94,	0x23  }
	

 };	 
#endif /* NXTV_DAB_ENABLE */


#ifdef NXTV_TDMB_ENABLE
 static const NXTV_REG_INIT_INFO t_TDMB_INIT[] =   
 {
	{  0x2B,  0x88  },
	{  0x2d,  0xec  },
	{  0x2e,  0xb0  },
	{  0x31,  0x04  },
	{  0x34,  0xf8  },
	{  0x3a,  0x62  },
	{  0x3b,  0x74  },
	{  0x3c,  0x81  },
	{  0x3d,  0x48  },
	{  0x4b,  0x80  },
#ifdef DEBUG_A_TEST_ZERO
	{  0x50,  0x88	},	
#endif
	{  0x53,  0x20  },
	{  0x55,  0xd6  },
	{  0x5a,  0x83  },
	{  0x60,  0x13  },
	{  0x6b,  0x85  },
	{  0x72,  0x70  },
	{  0x73,  0xca  },
	{  0x77,  0x80  },
	{  0x78,  0x47  },
	{  0x84,  0x00  },
	{  0x85,  0x00  },
	{  0x86,  0x80  },
	{  0x87,  0x98  },
	{  0x8a,  0xf6  },
	{  0x8b,  0x80  },
	{  0x8c,  0x75  },
	{  0x8f,  0xf9  },
	{  0x90,  0x91  },
	{  0x94,  0x23  },
	{  0x99,  0x77  },
	{  0x9a,  0x2d  },
	{  0x9b,  0x1e  },
	{  0x9c,  0x47  },
	{  0x9d,  0x3a  },
	{  0x9e,  0x03  },
	{  0x9f,  0x1e  },
	{  0xa0,  0x22  },
	{  0xa1,  0x33  },
	{  0xa2,  0x51  },
	{  0xa3,  0x36  },
	{  0xa4,  0x0c  },
	{  0xae,  0x37  },
	{  0xb5,  0x9b  },
	{  0xbd,  0x45  },	 
	{  0xbe,  0x68  },	
	{  0xbf,  0x5c  },	
	{  0xc0,  0x33  },	 
	{  0xc1,  0xca  },	
	{  0xc2,  0x43  },	
	{  0xc3,  0x90  },	 
	{  0xc4,  0xec  },   
	{  0xc5,  0x17  },   
	{  0xc6,  0xda  },   
	{  0xc7,  0x41  },   
	{  0xc8,  0x49  }, 
	{  0xc9,  0xbc  }, 
	{  0xca,  0x3a  },   
	{  0xcb,  0x20  },   
	{  0xcc,  0x43  },   
	{  0xcd,  0xa6  },   
	{  0xce,  0x4c  },   
	{  0xcf,  0x1f  },   
	{  0xd0,  0x8d  },   
	{  0xd1,  0x3d  },   
	{  0xd2,  0xdf  }, 
	{  0xd3,  0xa4  }, 
	{  0xd4,  0x30  },   
	{  0xd5,  0x00  }, 
	{  0xd6,  0x41  },
	{  0xd7,  0x81  },  
	{  0xd8,  0xd0  }, 
	{  0xd9,  0x00  },
	{  0xda,  0x00  }, 
	{  0xdb,  0x41  },
	{  0xdc,  0x6d  },
	{  0xdd,  0xac  },
	{  0xde,  0x39  },
	{  0xdf,  0x4e  },
	{  0xe0,  0x43  },
	{  0xe1,  0xa0  },
	{  0xe2,  0x4c  },
	{  0xe3,  0x1d  },
	{  0xe4,  0x99  },
	{  0xe5,  0x3b  },
	{  0xe6,  0x00  },
	{  0xe7,  0x0d  },
	{  0xa5,  0x00  },
	{  0xe9,  0x41  },
	{  0xae,  0x77  },
	{  0xe9,  0x51  }
 }; 
#endif /* NXTV_TDMB_ENABLE */



/*===============================================================================
 * nxtvRF_ConfigurePowerType_1SEG
 *
 * DESCRIPTION : 
 *		This function returns 
 *		
 *
 * ARGUMENTS : none.
 * RETURN VALUE : none.
 *============================================================================*/
void nxtvRF_ConfigurePowerType_1SEG(E_NXTV_TV_MODE_TYPE eTvMode)
{
#if defined(NXTV_IO_2_5V)
	U8 io_type = 0;
#elif defined(NXTV_IO_3_3V)	
	U8 io_type = 1;
#elif defined(NXTV_IO_1_8V)	
	U8 io_type = 2;
#else
	#error "Code not present"
#endif
	U8 REG2F = 0x61;   //DCDC_OUTSEL = 0x03,
	U8 REG30 = 0xF2 & 0xF0;  //IOLDOCON__REG
	U8 REG52 = 0x07;   //LDODIG_HT = 0x07;
	U8 REG54 = 0x0C;
	
	switch( eTvMode )
	{
#ifdef NXTV_ISDBT_ENABLE
		case NXTV_TV_MODE_1SEG : REG54 = 0x1C; break;
#endif

#ifdef NXTV_FM_ENABLE
		case NXTV_TV_MODE_FM :   REG54 = 0x1C; break;
#endif

#ifdef NXTV_TDMB_ENABLE
		case NXTV_TV_MODE_TDMB : REG54 = 0x1C; break;
#endif

#ifdef NXTV_DAB_ENABLE
		case NXTV_TV_MODE_DAB_B3 :
	#ifdef NXTV_DAB_LBAND_ENABLED
		case NXTV_TV_MODE_DAB_L :
	#endif
		case NXTV_TV_MODE_TDMB:  REG54 = 0x1C; break;
#endif
		default: return;
	}

	REG30 = REG30 | (io_type<<1);   //IO Type Select.

#if defined(NXTV_PWR_EXTERNAL)		
    REG2F = REG2F | 0x14; //PDDCDC_I2C = 1, PDLDO12_I2C = 1 ; 
#elif defined(NXTV_PWR_LDO)             
    REG2F = REG2F | 0x10; //PDDCDC_I2C = 1, PDLDO12_I2C = 0 ; 
#elif defined(NXTV_PWR_DCDC)            
    REG2F = REG2F | 0x04; //PDDCDC_I2C = 0, PDLDO12_I2C = 1 ; 
#else
	#error "Code not present"
#endif    

	/* Below Power Up sequence is very important.*/
	NXTV_REG_MAP_SEL(RF_PAGE);
	NXTV_REG_SET(0x54, REG54);
	NXTV_REG_SET(0x52, REG52);
	NXTV_REG_SET(0x30, REG30);
	NXTV_REG_SET(0x2F, REG2F);	
}



#define REGE8 (0x46 & 0xC0) 
#define REGEA (0x07 & 0xC0)  
#define REGEB (0x27 & 0xC0)  
#define REGEC (0x1E & 0xC0)  
#define REGED (0x18 & 0x00)  
#define REGEE (0xB8 & 0x00)  

INT nxtvRF_ConfigureAdcClock_1SEG(E_NXTV_TV_MODE_TYPE eTvMode, E_NXTV_ADC_CLK_FREQ_TYPE eAdcClkFreqType)
{
	U8 REGE9,RD15;
	INT i;
	const U8 *pbAdcClkSynTbl = (const U8 *)&g_abAdcClkSynTbl[eAdcClkFreqType];

	if(pbAdcClkSynTbl[0] == 0xFF)
	{
	        NXTV_DBGMSG1("[nxtvRF_ConfigureAdcClock_1SEG] Unsupport ADC clock type: %d\n", eAdcClkFreqType);
	 	 return NXTV_UNSUPPORT_ADC_CLK;
	 }
		
	switch( eTvMode )
	{
#ifdef NXTV_ISDBT_ENABLE
		case NXTV_TV_MODE_1SEG : REGE9 = (0xD4 & 0xF0); break;
#endif		

#ifdef NXTV_FM_ENABLE
		case NXTV_TV_MODE_FM : REGE9 = (0x54 & 0xF0); break;
#endif

#ifdef NXTV_TDMB_ENABLE
		case NXTV_TV_MODE_TDMB : REGE9 = (0x54 & 0xF0); break;
#endif

#ifdef NXTV_DAB_ENABLE
		case NXTV_TV_MODE_DAB_B3 : 
		case NXTV_TV_MODE_TDMB: 
	#ifdef NXTV_DAB_LBAND_ENABLED
		case NXTV_TV_MODE_DAB_L :
	#endif
			REGE9 = (0x54 & 0xF0); break;
#endif
		default: return NXTV_INVAILD_TV_MODE;
	}
		
	NXTV_REG_MAP_SEL(RF_PAGE);
	NXTV_REG_SET(0xE8, (REGE8 | pbAdcClkSynTbl[0]));
	NXTV_REG_SET(0xE9, (REGE9 | pbAdcClkSynTbl[1]));				
	NXTV_REG_SET(0xEA, (REGEA | pbAdcClkSynTbl[2]));				
	NXTV_REG_SET(0xEB, (REGEB | pbAdcClkSynTbl[3]));				
	NXTV_REG_SET(0xEC, (REGEC | pbAdcClkSynTbl[4]));				
	NXTV_REG_SET(0xED, (REGED | pbAdcClkSynTbl[5]));				
	NXTV_REG_SET(0xEE, (REGEE | pbAdcClkSynTbl[6]));	
	
	for(i=0; i<10;i++)
	{
		RD15 = NXTV_REG_GET(0x15) & 0x01; 
		if(RD15) 
		{
			 break;
		}
		else
		{
			NXTV_DBGMSG0("[nxtvRF_ConfigureAdcClock_1SEG] CLOCK SYNTH 1st step  UnLock..\n");
		}
		
		NXTV_DELAY_MS(1);
	}

	if(i == 10)
	{
		NXTV_DBGMSG0("[nxtvRF_ConfigureAdcClock_1SEG] ADC clock unlocked!Check the power supply.\n");
		return NXTV_ADC_CLK_UNLOCKED;
	}
	
#ifdef NXTV_ISDBT_ENABLE 
	if(eTvMode == NXTV_TV_MODE_1SEG)
	{
		NXTV_REG_MAP_SEL(HOST_PAGE);
		NXTV_REG_SET(0x1A, 0x8B);
		NXTV_REG_SET(0x18, 0xC0);
		NXTV_REG_SET(0x19, 0x03);
		NXTV_REG_SET(0x07, 0xF0); 
	}
#endif

	
	// Save the ADC clock type.
	g_eRtvAdcClkFreqType_1SEG = eAdcClkFreqType;

	//NXTV_DBGMSG1("[nxtvRF_ConfigureAdcClock_1SEG] ADC clk Type: %d\n", g_eRtvAdcClkFreqType_1SEG[RaonTvChipIdx]);

	return NXTV_SUCCESS;
}


INT nxtvRF_ChangeAdcClock_1SEG(E_NXTV_TV_MODE_TYPE eTvMode, E_NXTV_ADC_CLK_FREQ_TYPE eAdcClkFreqType)
{	
	U8 REGE9, RD15;
#ifdef NXTV_PWR_DCDC	
	U8 RD2F;
#endif
	INT i;
	const U8 *pbAdcClkSynTbl = (const U8 *)&g_abAdcClkSynTbl[eAdcClkFreqType];

	if(eAdcClkFreqType == g_eRtvAdcClkFreqType_1SEG)
		return NXTV_SUCCESS;
		
	if(pbAdcClkSynTbl[0] == 0xFF)
    {
		NXTV_DBGMSG1("[nxtvRF_ChangeAdcClock_1SEG] Unsupport ADC clock type: %d\n", eAdcClkFreqType);
		return NXTV_UNSUPPORT_ADC_CLK;
    }
		
	switch( eTvMode )
	{
#ifdef NXTV_ISDBT_ENABLE
		case NXTV_TV_MODE_1SEG : REGE9 = (0xD4 & 0xF0); break;
#endif		

#ifdef NXTV_FM_ENABLE
		case NXTV_TV_MODE_FM : REGE9 = (0x54 & 0xF0); break;
#endif

#ifdef NXTV_TDMB_ENABLE
		case NXTV_TV_MODE_TDMB : REGE9 = (0x54 & 0xF0); break;
#endif

#ifdef NXTV_DAB_ENABLE
		case NXTV_TV_MODE_DAB_B3 :
	#ifdef NXTV_DAB_LBAND_ENABLED
		case NXTV_TV_MODE_DAB_L :
	#endif
		case NXTV_TV_MODE_TDMB:  REGE9 = (0x54 & 0xF0); break;
#endif
		default: return NXTV_INVAILD_TV_MODE;
	}
		
	NXTV_REG_MAP_SEL(RF_PAGE);

#ifdef NXTV_PWR_DCDC
	RD2F = NXTV_REG_GET(0x2F);     
	NXTV_REG_SET(0x2F, (RD2F & 0xF7));
#endif	
	
	NXTV_REG_SET(0xE8, (REGE8 | pbAdcClkSynTbl[0]));                   
	NXTV_REG_SET(0xE9, (REGE9 | pbAdcClkSynTbl[1]));				
	NXTV_REG_SET(0xEA, (REGEA | pbAdcClkSynTbl[2]));				
	NXTV_REG_SET(0xEB, (REGEB | pbAdcClkSynTbl[3]));				
	NXTV_REG_SET(0xEC, (REGEC | pbAdcClkSynTbl[4]));				
	NXTV_REG_SET(0xED, (REGED | pbAdcClkSynTbl[5]));				
	NXTV_REG_SET(0xEE, (REGEE | pbAdcClkSynTbl[6]));	
	
	for(i=0; i<10;i++)
	{
		RD15 = NXTV_REG_GET(0x15) & 0x01; 
		if(RD15) 
		{
			 break;
		}
		else
		{
			NXTV_DBGMSG0("[nxtvRF_ChangeAdcClock_1SEG] CLOCK SYNTH 1st step  UnLock..\n");
		}
		
		NXTV_DELAY_MS(1);
	}

	if(i == 10)
	{
		NXTV_DBGMSG0("[nxtvRF_ChangeAdcClock_1SEG] ADC clock unlocked!Check the power supply.\n");
		return NXTV_ADC_CLK_UNLOCKED;
	}

    switch(eAdcClkFreqType)
	{
		case NXTV_ADC_CLK_FREQ_8_MHz:     
            switch( eTvMode )
            {
#ifdef NXTV_ISDBT_ENABLE
                case NXTV_TV_MODE_1SEG :	
                    NXTV_REG_MAP_SEL(OFDM_PAGE);
                    NXTV_REG_SET(0x19,0xff);
                    NXTV_REG_SET(0x1a,0x08);
                    NXTV_REG_SET(0x1b,0x82);
                    NXTV_REG_SET(0x1c,0x20);
                    
                    NXTV_REG_SET(0x45,0x10);
                    NXTV_REG_SET(0x46,0x04);
                    NXTV_REG_SET(0x47,0x41);
                    NXTV_REG_SET(0x48,0x10);
    
                    NXTV_REG_SET(0x49,0x00);
                    NXTV_REG_SET(0x4a,0x00);
                    NXTV_REG_SET(0x4b,0x00);
                    NXTV_REG_SET(0x4c,0xF0);
                    break;
#endif

#if defined(NXTV_TDMB_ENABLE)  ||  defined(NXTV_DAB_ENABLE)
				case NXTV_TV_MODE_TDMB :
				case NXTV_TV_MODE_DAB_B3 :
			#ifdef NXTV_DAB_LBAND_ENABLED
				case NXTV_TV_MODE_DAB_L :
			#endif
                    NXTV_REG_MAP_SEL(COMM_PAGE);
                    NXTV_REG_SET(0x6A,0x01); 
                    
                    NXTV_REG_MAP_SEL(0x06);
                    NXTV_REG_SET(0x3c,0x4B); 
                    NXTV_REG_SET(0x3d,0x37); 
                    NXTV_REG_SET(0x3e,0x89); 
                    NXTV_REG_SET(0x3f,0x41); 
		      		NXTV_REG_SET(0x54, 0x58);

					NXTV_REG_SET(0x40,0x8F); //PNCO
					NXTV_REG_SET(0x41,0xC2); //PNCO
					NXTV_REG_SET(0x42,0xF5); //PNCO
					NXTV_REG_SET(0x43,0x00); //PNCO
                    break;
#endif	
                default: break;	
            } /* End of switch(eTvMode) */
			break;
                
		case NXTV_ADC_CLK_FREQ_8_192_MHz:	
            switch( eTvMode )
            {
#ifdef NXTV_ISDBT_ENABLE
                case NXTV_TV_MODE_1SEG :	
                    NXTV_REG_MAP_SEL(OFDM_PAGE);
                    NXTV_REG_SET(0x19,0xfd);
                    NXTV_REG_SET(0x1a,0xfc);
                    NXTV_REG_SET(0x1b,0xbe);
                    NXTV_REG_SET(0x1c,0x1f);
                                     
                    NXTV_REG_SET(0x45,0xF7);
                    NXTV_REG_SET(0x46,0x7D);
                    NXTV_REG_SET(0x47,0xDF);
                    NXTV_REG_SET(0x48,0x0F);
                                     
                    NXTV_REG_SET(0x49,0x00);
                    NXTV_REG_SET(0x4a,0x00);
                    NXTV_REG_SET(0x4b,0x60);
                    NXTV_REG_SET(0x4c,0xF0);
                    break;
#endif


#if defined(NXTV_TDMB_ENABLE)  ||  defined(NXTV_DAB_ENABLE)
	case NXTV_TV_MODE_TDMB :	
	case NXTV_TV_MODE_DAB_B3 :
	#ifdef NXTV_DAB_LBAND_ENABLED
	case NXTV_TV_MODE_DAB_L :
	#endif
                    NXTV_REG_MAP_SEL(COMM_PAGE);
                    NXTV_REG_SET(0x6A,0x01); 
                       
                    NXTV_REG_MAP_SEL( 0x06);
                    NXTV_REG_SET(0x3c,0x00); 
                    NXTV_REG_SET(0x3d,0x00); 
                    NXTV_REG_SET(0x3e,0x00); 
                    NXTV_REG_SET(0x3f,0x40); 
			      	NXTV_REG_SET(0x54, 0x58); 

					NXTV_REG_SET(0x40,0x00); //PNCO
					NXTV_REG_SET(0x41,0x00); //PNCO
					NXTV_REG_SET(0x42,0xF0); //PNCO
					NXTV_REG_SET(0x43,0x00); //PNCO
#endif
                default: break;
            } /* End of switch(eTvMode) */
			break;
		
        case NXTV_ADC_CLK_FREQ_9_MHz:	
            switch( eTvMode )
            {
#ifdef NXTV_ISDBT_ENABLE
                case NXTV_TV_MODE_1SEG :	
                    NXTV_REG_MAP_SEL(OFDM_PAGE);
                    NXTV_REG_SET(0x19,0xe4);
                    NXTV_REG_SET(0x1a,0x5c);
                    NXTV_REG_SET(0x1b,0xe5);
                    NXTV_REG_SET(0x1c,0x1c);
                                     
                    NXTV_REG_SET(0x45,0x47);
                    NXTV_REG_SET(0x46,0xAE);
                    NXTV_REG_SET(0x47,0x72);
                    NXTV_REG_SET(0x48,0x0E);
                                     
                    NXTV_REG_SET(0x49,0x72);
                    NXTV_REG_SET(0x4a,0x1C);
                    NXTV_REG_SET(0x4b,0xC7);
                    NXTV_REG_SET(0x4c,0xF1);
                    break;
#endif
			

#if defined(NXTV_TDMB_ENABLE)  ||  defined(NXTV_DAB_ENABLE)
				case NXTV_TV_MODE_TDMB :
				case NXTV_TV_MODE_DAB_B3 :
		#ifdef NXTV_DAB_LBAND_ENABLED
				case NXTV_TV_MODE_DAB_L :
		#endif
                    NXTV_REG_MAP_SEL(COMM_PAGE);
                    NXTV_REG_SET(0x6A,0x21);
    
                    NXTV_REG_MAP_SEL( 0x06);
                    NXTV_REG_SET(0x3c,0xB5); 
                    NXTV_REG_SET(0x3d,0x14); 
                    NXTV_REG_SET(0x3e,0x41); 
                    NXTV_REG_SET(0x3f,0x3A); 
		      NXTV_REG_SET(0x54, 0x58); 

					NXTV_REG_SET(0x40,0x0D); //PNCO
					NXTV_REG_SET(0x41,0x74); //PNCO
					NXTV_REG_SET(0x42,0xDA); //PNCO
					NXTV_REG_SET(0x43,0x00); //PNCO
                    break;
#endif
                default: break;
            } /* End of switch(eTvMode) */
			break;	
                
		case NXTV_ADC_CLK_FREQ_9_6_MHz:	
            switch( eTvMode )
            {
#ifdef NXTV_ISDBT_ENABLE
                case NXTV_TV_MODE_1SEG :	
                    NXTV_REG_MAP_SEL(OFDM_PAGE);
                    NXTV_REG_SET(0x19,0xd7);
                    NXTV_REG_SET(0x1a,0x08);
                    NXTV_REG_SET(0x1b,0x17);
                    NXTV_REG_SET(0x1c,0x1b);
                    
                    NXTV_REG_SET(0x45,0x62);
                    NXTV_REG_SET(0x46,0x83);
                    NXTV_REG_SET(0x47,0x8B);
                    NXTV_REG_SET(0x48,0x0D);
    
                    NXTV_REG_SET(0x49,0xAB);
                    NXTV_REG_SET(0x4a,0xAA);
                    NXTV_REG_SET(0x4b,0xAA);
                    NXTV_REG_SET(0x4c,0xF2);
                    break;
#endif
			
#if defined(NXTV_TDMB_ENABLE)  ||  defined(NXTV_DAB_ENABLE)
				case NXTV_TV_MODE_TDMB :
				case NXTV_TV_MODE_DAB_B3 :
		#ifdef NXTV_DAB_LBAND_ENABLED
				case NXTV_TV_MODE_DAB_L :
				#endif
                    NXTV_REG_MAP_SEL(COMM_PAGE);
                    NXTV_REG_SET(0x6A,0x31);
                           
                    NXTV_REG_MAP_SEL(0x06);
                    NXTV_REG_SET(0x3c,0x69); 
                    NXTV_REG_SET(0x3d,0x03); 
                    NXTV_REG_SET(0x3e,0x9D); 
                    NXTV_REG_SET(0x3f,0x36); 
		      NXTV_REG_SET(0x54, 0x58); 

					NXTV_REG_SET(0x40,0xCC); //PNCO
					NXTV_REG_SET(0x41,0xCC); //PNCO
					NXTV_REG_SET(0x42,0xCC); //PNCO
					NXTV_REG_SET(0x43,0x00); //PNCO
                    break;
#endif			
                default: break;
            } /* End of switch(eTvMode) */
			break;	

		default:
			break;
	} /* End of switch(eAdcClkFreqType) */
	
#ifdef NXTV_PWR_DCDC
	NXTV_REG_MAP_SEL(RF_PAGE);
	RD15 = NXTV_REG_GET(0x15) & 0x01; 
	if(RD15) 
	{
		 NXTV_REG_SET(0x2F, RD2F);
	}
	else
	{
		NXTV_DBGMSG0("[nxtvRF_ChangeAdcClock_1SEG] DCDC CLOCK SYNTH  UnLock..\n");
	}
#endif

	// Save the ADC clock type.
	g_eRtvAdcClkFreqType_1SEG = eAdcClkFreqType;

//	NXTV_DBGMSG1("[nxtvRF_ChangeAdcClock_1SEG] ADC clk Type: %d\n", g_eRtvAdcClkFreqType_1SEG[RaonTvChipIdx]);

	return NXTV_SUCCESS;	
}


INT nxtvRF_SetFrequency_1SEG(E_NXTV_TV_MODE_TYPE eTvMode, UINT nChNum, U32 dwChFreqKHz)
{
#if defined(NXTV_TDMB_ENABLE) || defined(NXTV_DAB_ENABLE)
	int nIdx;
#endif

#ifdef NXTV_DAB_ENABLE
	UINT nNumTblEntry=0;
	const NXTV_REG_INIT_INFO *ptInitTbl = NULL;
#endif
#ifdef NXTV_NOTCH_FILTER_ENABLE
	U8 notch_nIdx;
    U8 lnacaphg_nIdx;
#endif
	INT nRet = NXTV_SUCCESS;
	E_NXTV_ADC_CLK_FREQ_TYPE eAdcClkFreqType = NXTV_ADC_CLK_FREQ_8_MHz;
	U32 dwPllNF=0,rd_dwPllNF_Verify_val,wr_dwPllNF_Verify_val;
	U8 RD15; 
	U32 PLL_Verify_cnt=10;

	g_fRtvChannelChange_1SEG = TRUE;

    /* Get the PLLNF and ADC clock type. */
	switch( eTvMode )
	{
#ifdef NXTV_ISDBT_ENABLE
		case NXTV_TV_MODE_1SEG :
			if((g_eRtvCountryBandType_1SEG==NXTV_COUNTRY_BAND_BRAZIL) || (g_eRtvCountryBandType_1SEG==NXTV_COUNTRY_BAND_ARGENTINA))		
				nChNum -= 1;
				
			switch( nChNum )
			{
				//case 14: case 22: case 38: case 46: case 54: case 57: case 62: // 57channel added.
				case 14: case 22: case 38: case 46: case 54: 
					eAdcClkFreqType = g_aeAdcClkTypeTbl_ISDBT[1];	
					break;
				//case 30:
				case 30:  case 61:
					eAdcClkFreqType = g_aeAdcClkTypeTbl_ISDBT[2];	
					break;
				default: 
					eAdcClkFreqType = g_aeAdcClkTypeTbl_ISDBT[0]; 
					break;
			}	
			dwPllNF = ISDBT_AUTO_PLLNF;
			
    #ifdef NXTV_NOTCH_FILTER_ENABLE // => NXTV_1SEG_NOTCH_FILTER_ENABLE ??
			switch( nChNum )  // //notch filter value need to tuning
			{
				case 26: case 34: case 35: case 36:
					notch_nIdx = 1; 
					lnacaphg_nIdx = 0;
					break;									
				default: 
					notch_nIdx = 0; 
					lnacaphg_nIdx = 7;
					break;
			}
    #endif
			break;
#endif		

#ifdef NXTV_FM_ENABLE
		case NXTV_TV_MODE_FM :							
			dwPllNF = g_atPllNF_FM[(dwChFreqKHz - FM_MIN_FREQ_KHz)/FM_SCAN_STEP_FREQ_KHz];	
			eAdcClkFreqType = g_eRtvAdcClkFreqType_1SEG; /* Only init adc clock type. */
			break;
#endif

#ifdef NXTV_TDMB_ENABLE
		case NXTV_TV_MODE_TDMB :
			if(g_eRtvCountryBandType_1SEG == NXTV_COUNTRY_BAND_KOREA)
			{
				nIdx = (dwChFreqKHz - TDMB_CH_FREQ_START__KOREA) / TDMB_CH_FREQ_STEP__KOREA;
				
				if(dwChFreqKHz >= 205280)		nIdx -= 2;
				else if(dwChFreqKHz >= 193280)	nIdx -= 1; 
		
	            eAdcClkFreqType = g_aeAdcClkTypeTbl_TDMB[nIdx];
	            dwPllNF = g_atPllNF_TDMB_Korea[nIdx];
	        }
			break;
#endif

#ifdef NXTV_DAB_ENABLE
		case NXTV_TV_MODE_DAB_B3 :			
			if(g_curDabSetType != NXTV_TV_MODE_DAB_B3)
			{
				ptInitTbl = t_BAND3_INIT;
				nNumTblEntry = sizeof(t_BAND3_INIT) / sizeof(NXTV_REG_INIT_INFO);
				
				NXTV_REG_MAP_SEL(RF_PAGE);
				do
				{
					NXTV_REG_SET(ptInitTbl->bReg, ptInitTbl->bVal);
					ptInitTbl++;						
				} while( --nNumTblEntry );

				g_curDabSetType = NXTV_TV_MODE_DAB_B3;
			}

			nIdx = (dwChFreqKHz - DAB_CH_BAND3_START_FREQ_KHz) / DAB_CH_BAND3_STEP_FREQ_KHz;

			if(dwChFreqKHz >= 224096)			nIdx += 3;
			else if(dwChFreqKHz >= 217008)		nIdx += 2; 
			else if(dwChFreqKHz >= 210096)		nIdx += 1;			
			
           		eAdcClkFreqType = g_aeAdcClkTypeTbl_DAB_B3[nIdx];
	        	dwPllNF = g_atPllNF_DAB_BAND3[nIdx];
			break;

		case NXTV_TV_MODE_TDMB : 	
			if(g_curDabSetType != NXTV_TV_MODE_TDMB)
			{
				ptInitTbl = t_DAB_TDMB_INIT;
				nNumTblEntry = sizeof(t_DAB_TDMB_INIT) / sizeof(NXTV_REG_INIT_INFO);
				NXTV_REG_MAP_SEL(RF_PAGE);
				do
				{
					NXTV_REG_SET(ptInitTbl->bReg, ptInitTbl->bVal);
					ptInitTbl++;						
				} while( --nNumTblEntry );											
			}
			
			//if(g_eRtvCountryBandType_1SEG == NXTV_COUNTRY_BAND_KOREA)
			{
				nIdx = (dwChFreqKHz - TDMB_CH_FREQ_START__KOREA) / TDMB_CH_FREQ_STEP__KOREA;
				
				if(dwChFreqKHz >= 205280)		nIdx -= 2;
				else if(dwChFreqKHz >= 193280)	nIdx -= 1; 
		
				eAdcClkFreqType = g_aeAdcClkTypeTbl_TDMB[nIdx];
				dwPllNF = g_atPllNF_TDMB_Korea[nIdx];
				g_curDabSetType =NXTV_TV_MODE_TDMB;
			}
			break;
			
	#ifdef NXTV_DAB_LBAND_ENABLED
		case NXTV_TV_MODE_DAB_L :
			if(g_curDabSetType != NXTV_TV_MODE_DAB_L)
			{
				ptInitTbl = t_LBAND_INIT;
				nNumTblEntry = sizeof(t_LBAND_INIT) / sizeof(NXTV_REG_INIT_INFO);
				NXTV_REG_MAP_SEL(RF_PAGE);
				do
				{
					NXTV_REG_SET(ptInitTbl->bReg, ptInitTbl->bVal);
					ptInitTbl++;						
				} while( --nNumTblEntry );				
				
				g_curDabSetType = NXTV_TV_MODE_DAB_L;
			}
			
			nIdx = (dwChFreqKHz - DAB_CH_LBAND_START_FREQ_KHz) / DAB_CH_LBAND_STEP_STEP_FREQ_KHz;
			
     			eAdcClkFreqType = g_aeAdcClkTypeTbl_DAB_L[nIdx];
	        	dwPllNF = g_atPllNF_DAB_LBAND[nIdx];
			break;
	#endif /* #ifdef NXTV_DAB_LBAND_ENABLED */
#endif

		default:
			nRet = NXTV_INVAILD_TV_MODE;
			goto RF_SET_FREQ_EXIT;
	}
		
	if((nRet=nxtvRF_ChangeAdcClock_1SEG(eTvMode, eAdcClkFreqType)) != NXTV_SUCCESS)
		goto RF_SET_FREQ_EXIT;
	
	NXTV_REG_MAP_SEL(RF_PAGE);

    /* Set the PLLNF and channel. */
	switch( eTvMode )
	{
#ifdef NXTV_ISDBT_ENABLE
		case NXTV_TV_MODE_1SEG :			
	#ifdef NXTV_NOTCH_FILTER_ENABLE
			NXTV_REG_SET(0x21, (notch_nIdx << 4 )| 0x01); 	
			NXTV_REG_SET(0x3A, (lnacaphg_nIdx << 4 )| 0x07); 	
	#endif			
  
			NXTV_REG_SET(0x20, nChNum);  
            NXTV_DELAY_MS(2);	//2ms Delay 	

            wr_dwPllNF_Verify_val = (dwPllNF + (((ISDBT_AUTO_PLL_NFSTEP * 8) & 0xFFFFF0) * (nChNum >> 3)) 
                                    + ((nChNum & 0x07) * (ISDBT_AUTO_PLL_NFSTEP &0xFFFFF0)))>>1;

			do{
				RD15 = NXTV_REG_GET(0x15);
				rd_dwPllNF_Verify_val = (NXTV_REG_GET(0x12) << 22) + (NXTV_REG_GET(0x13) << 14) +(NXTV_REG_GET(0x14) << 6) +(RD15 >> 2);

				if((wr_dwPllNF_Verify_val == rd_dwPllNF_Verify_val ) || (wr_dwPllNF_Verify_val == (U32)(rd_dwPllNF_Verify_val>>1)))
				{
					if((RD15 & 0x02) == 0x02) 				
						break;
					else
					{
						NXTV_REG_SET(0x20, nChNum);	
						NXTV_DELAY_MS(2);	//2ms Delay 
					}
				}
				else
				{ 
					NXTV_REG_SET(0x20, nChNum);	
					NXTV_DELAY_MS(2);	//2ms Delay 
				//	NXTV_DBGMSG3("[nxtvRF_SetFrequency_1SEG] 1SEG PLL verify Re-Try!!!  PLLNF_I2C = 0x%0x  RD_PLLNF = 0x%0x RD_PLLNF*2 = 0x%0x\n",wr_dwPllNF_Verify_val,rd_dwPllNF_Verify_val,rd_dwPllNF_Verify_val>>1);
				}
			}while(--PLL_Verify_cnt);

			NXTV_REG_MAP_SEL(OFDM_PAGE);
			NXTV_REG_SET(0x11,0x07);
			NXTV_REG_SET(0x11,0x06);
			NXTV_REG_MAP_SEL(FEC_PAGE);
			NXTV_REG_SET(0x10,0x01); 
			NXTV_REG_SET(0x10,0x00);
			break;
#endif		

#ifdef NXTV_FM_ENABLE
		case NXTV_TV_MODE_FM :		
			NXTV_REG_SET(0x23, (dwPllNF>>22)&0xFF);
			NXTV_REG_SET(0x24, (dwPllNF>>14)&0xFF);
			NXTV_REG_SET(0x25, (dwPllNF>>6)&0xFF);
			NXTV_REG_SET(0x26, (((dwPllNF&0x0000003F)<<2)) | 0x00);	
			NXTV_DELAY_MS(1);  //1ms Delay	
			NXTV_REG_SET(0x20, 0x00);	

		    NXTV_DELAY_MS(2);	//2ms Delay 	 
			wr_dwPllNF_Verify_val = dwPllNF>>1;

			do{
				RD15 = NXTV_REG_GET(0x15);
				rd_dwPllNF_Verify_val = (NXTV_REG_GET(0x12) << 22) + (NXTV_REG_GET(0x13) << 14) +(NXTV_REG_GET(0x14) << 6) +(RD15 >> 2);

				if(((dwPllNF & ~0x1) == (rd_dwPllNF_Verify_val & ~0x1) ) || (wr_dwPllNF_Verify_val == rd_dwPllNF_Verify_val))
				{
					if((RD15 & 0x02) == 0x02) 				
						break;
					else
					{
						NXTV_REG_SET(0x20, 0x00);	
						NXTV_DELAY_MS(2);	//2ms Delay 
					}
				}
				else
				{ 
					NXTV_REG_SET(0x20, 0x00);	
					NXTV_DELAY_MS(2);	//2ms Delay 
				//	NXTV_DBGMSG3("[nxtvRF_SetFrequency_1SEG] FM PLL verify Re-Try!!!  PLLNF_I2C = 0x%0x, PLLNF_I2C/2 = 0x%0x,   RD_PLLNF = 0x%0x\n", dwPllNF & ~0x1, wr_dwPllNF_Verify_val, rd_dwPllNF_Verify_val & ~0x1);
				}
			}while(--PLL_Verify_cnt);

            NXTV_REG_MAP_SEL(0x06); // ofdm
			NXTV_REG_SET(0x10,0x48); 
			NXTV_REG_SET(0x10,0xC9);
			break;
#endif

#ifdef NXTV_TDMB_ENABLE
		case NXTV_TV_MODE_TDMB :	
            NXTV_REG_SET(0x23, (dwPllNF>>22)&0xFF);
			NXTV_REG_SET(0x24, (dwPllNF>>14)&0xFF);
			NXTV_REG_SET(0x25, (dwPllNF>>6)&0xFF);
			NXTV_REG_SET(0x26, (((dwPllNF&0x0000003F)<<2)) | 0x00);	
			NXTV_DELAY_MS(1);  //1ms Delay	
			NXTV_REG_SET(0x20, 0x00);
                        
            NXTV_DELAY_MS(2);	//2ms Delay 	 
			wr_dwPllNF_Verify_val = dwPllNF>>1;

			do{
				RD15 = NXTV_REG_GET(0x15);
				rd_dwPllNF_Verify_val = (NXTV_REG_GET(0x12) << 22) + (NXTV_REG_GET(0x13) << 14) +(NXTV_REG_GET(0x14) << 6) +(RD15 >> 2);

				//NXTV_DBGMSG3("[nxtvRF_SetFrequency_1SEG] TDMB PLL PLLNF_I2C = 0x%0x, PLLNF_I2C/2 = 0x%0x,   RD_PLLNF = 0x%0x\n", dwPllNF & ~0x1, wr_dwPllNF_Verify_val, rd_dwPllNF_Verify_val & ~0x1);
				
				if(((dwPllNF & ~0x1) == (rd_dwPllNF_Verify_val & ~0x1) ) || (wr_dwPllNF_Verify_val == rd_dwPllNF_Verify_val))
				{
					if((RD15 & 0x02) == 0x02) 
						break;
					else
					{
						NXTV_REG_SET(0x20, 0x00);	
						NXTV_DELAY_MS(2);	//2ms Delay 
					}
				}
				else
				{ 
					NXTV_REG_SET(0x20, 0x00);	
					NXTV_DELAY_MS(2);	//2ms Delay 
					NXTV_DBGMSG3("[nxtvRF_SetFrequency_1SEG] TDMB PLL verify Re-Try!!!  PLLNF_I2C = 0x%0x, PLLNF_I2C/2 = 0x%0x,   RD_PLLNF = 0x%0x\n", dwPllNF & ~0x1, wr_dwPllNF_Verify_val, rd_dwPllNF_Verify_val & ~0x1);
				}
			}while(--PLL_Verify_cnt);

			NXTV_REG_MAP_SEL(0x06); // ofdm
			NXTV_REG_SET(0x10,0x48); 
			NXTV_REG_SET(0x10,0xC9);
			break;
#endif

#ifdef NXTV_DAB_ENABLE
		case NXTV_TV_MODE_DAB_B3 :
	#ifdef NXTV_DAB_LBAND_ENABLED			
		case NXTV_TV_MODE_DAB_L :
	#endif
		case NXTV_TV_MODE_TDMB:
			NXTV_REG_SET(0x23, (dwPllNF>>22)&0xFF);
			NXTV_REG_SET(0x24, (dwPllNF>>14)&0xFF);
			NXTV_REG_SET(0x25, (dwPllNF>>6)&0xFF);
			NXTV_REG_SET(0x26, (((dwPllNF&0x0000003F)<<2)) | eTvMode);	
			NXTV_DELAY_MS(1);  //1ms Delay	
			NXTV_REG_SET(0x20, 0x00);
                        
            NXTV_DELAY_MS(2);	//2ms Delay 	
			wr_dwPllNF_Verify_val = dwPllNF>>1;

			do{
				RD15 = NXTV_REG_GET(0x15);
				
				rd_dwPllNF_Verify_val = (NXTV_REG_GET(0x12) << 22) + (NXTV_REG_GET(0x13) << 14) +(NXTV_REG_GET(0x14) << 6) +(RD15 >> 2);

				if(((dwPllNF & ~0x1) == (rd_dwPllNF_Verify_val & ~0x1) ) || (wr_dwPllNF_Verify_val == rd_dwPllNF_Verify_val))
				{
					if((RD15 & 0x02) == 0x02) 				
						break;
					else
					{
						NXTV_REG_SET(0x20, 0x00);	
						NXTV_DELAY_MS(2);	//2ms Delay 
					}
				}
				else
				{ 
					NXTV_REG_SET(0x20, 0x00);	
					NXTV_DELAY_MS(2);	//2ms Delay 
				//	NXTV_DBGMSG3("[nxtvRF_SetFrequency_1SEG] DAB PLL verify Re-Try!!!  PLLNF_I2C = 0x%0x, PLLNF_I2C/2 = 0x%0x,   RD_PLLNF = 0x%0x\n", dwPllNF & ~0x1, wr_dwPllNF_Verify_val, rd_dwPllNF_Verify_val & ~0x1);
				}
			}while(--PLL_Verify_cnt);
			
			NXTV_REG_MAP_SEL(0x06); // ofdm
			NXTV_REG_SET(0x10,0x48); 
			NXTV_REG_SET(0x10,0xC9); 
			break;
#endif

		default: break;
	}

	if(PLL_Verify_cnt == 0)
	{
		NXTV_DBGMSG0("[nxtvRF_SetFrequency_1SEG] PLL unlocked!\n");
		nRet = NXTV_PLL_UNLOCKED;
		goto RF_SET_FREQ_EXIT;
	}

	NXTV_DELAY_MS(1);  //1ms Delay

RF_SET_FREQ_EXIT:
	g_fRtvChannelChange_1SEG = FALSE; 
		 
	return nRet;
}


INT nxtvRF_Initilize_1SEG(E_NXTV_TV_MODE_TYPE eTvMode)
{
	UINT nNumTblEntry=0, nNumAutoInit=0;
	const NXTV_REG_INIT_INFO *ptInitTbl = NULL;
	const NXTV_REG_INIT_INFO *ptLNA = NULL;
	const NXTV_REG_MASK_INFO *ptAutoCh = NULL;
    U32 dwAutoPllNF=0, dwAutoPllNFSTEP=0;
			
	g_fRtvChannelChange_1SEG = FALSE;
	
	switch( eTvMode )
	{
#ifdef NXTV_ISDBT_ENABLE
		case NXTV_TV_MODE_1SEG :
			ptInitTbl = t_ISDBT_INIT;
			nNumTblEntry = sizeof(t_ISDBT_INIT) / sizeof(NXTV_REG_INIT_INFO);
			ptLNA = g_atAutoLnaInitData_ISDBT;
			ptAutoCh = g_atAutoChInitData_ISDBT;
			nNumAutoInit = sizeof(g_atAutoChInitData_ISDBT) / sizeof(NXTV_REG_MASK_INFO);	
			dwAutoPllNF = ISDBT_AUTO_PLLNF;
			dwAutoPllNFSTEP = ISDBT_AUTO_PLL_NFSTEP;
			break;
#endif		

#ifdef NXTV_FM_ENABLE
		case NXTV_TV_MODE_FM :
			ptInitTbl = t_FM_INIT;
			nNumTblEntry = sizeof(t_FM_INIT) / sizeof(NXTV_REG_INIT_INFO);
			ptLNA = g_atAutoLnaInitData_FM;
			break;
#endif

#ifdef NXTV_TDMB_ENABLE
		case NXTV_TV_MODE_TDMB :
			ptInitTbl = t_TDMB_INIT;
			nNumTblEntry = sizeof(t_TDMB_INIT) / sizeof(NXTV_REG_INIT_INFO);
			ptLNA = g_atAutoLnaInitData_TDMB;		
			break;
#endif

#ifdef NXTV_DAB_ENABLE
		case NXTV_TV_MODE_DAB_B3:
			ptInitTbl = t_DAB_INIT;
			nNumTblEntry = sizeof(t_DAB_INIT) / sizeof(NXTV_REG_INIT_INFO);
			ptLNA = g_atAutoLnaInitData_DAB;
			g_curDabSetType= NXTV_TV_MODE_DAB_B3;
			break;
#endif

		default:
			return NXTV_INVAILD_TV_MODE;
	}
	
	NXTV_REG_MAP_SEL(RF_PAGE);
		
	do
	{
		NXTV_REG_SET(ptInitTbl->bReg, ptInitTbl->bVal);
		ptInitTbl++;						
	} while( --nNumTblEntry );
	
	// Auto LNA
	NXTV_REG_SET(ptLNA[0].bReg, ptLNA[0].bVal);
	NXTV_REG_SET(ptLNA[1].bReg, ptLNA[1].bVal);
	
	// Auto channel setting.
	if(eTvMode == NXTV_TV_MODE_1SEG)
	{
		do
		{		
			NXTV_REG_MASK_SET(ptAutoCh->bReg, ptAutoCh->bMask, ptAutoCh->bVal);
			ptAutoCh++;	
		} while(--nNumAutoInit );

		NXTV_REG_SET(0x23,(dwAutoPllNF>>22)&0xFF);
		NXTV_REG_SET(0x24,(dwAutoPllNF>>14)&0xFF);
		NXTV_REG_SET(0x25,(dwAutoPllNF>>6)&0xFF);

		NXTV_REG_SET(0x26,((dwAutoPllNF&0x0000003F) << 2) | eTvMode);

		NXTV_REG_SET(0x62, (dwAutoPllNFSTEP >> 16) & 0xFF);
		NXTV_REG_SET(0x63, (dwAutoPllNFSTEP >> 8 ) & 0xFF);
		NXTV_REG_SET(0x64, (dwAutoPllNFSTEP & 0xFF));

#ifdef NXTV_CHIP_PKG_WLCSP
       if(NXTV_REG_GET(0x01) == 0x09)
       	{
       		NXTV_REG_SET(0x21,0xF1);
       	}
#endif 
	}

	
#ifdef NXTV_PWR_DCDC
	{
		U8 RD15 = NXTV_REG_GET(0x15) & 0x01; 
		U8 RD2F = NXTV_REG_GET(0x2F);       
		if(RD15)
		{
			RD2F |=0x08;
			NXTV_REG_SET(0x2F, RD2F);
		}
		else
		{
			NXTV_DBGMSG0("[nxtvRF_Initilize_1SEG] Clock Unlock \n");
		}
	}
#endif
//////////////////////////////CODE PATCH FOR OTP////////////////////////////////////
    if(((U16)(NXTV_REG_GET(0x10) << 8) | (U16)(NXTV_REG_GET(0x11))) != 0xFFFF)
    {

		switch( eTvMode )
		{
#ifdef NXTV_ISDBT_ENABLE
			case NXTV_TV_MODE_1SEG :
				NXTV_REG_SET(0x2C,0x48);
				NXTV_REG_SET(0x47,0xE0);
				break;
#endif		

#ifdef NXTV_FM_ENABLE
			case NXTV_TV_MODE_FM :
				NXTV_REG_SET(0x2C,0x48);
				NXTV_REG_SET(0x47,0xE1);
				NXTV_REG_SET(0x35,0x14);
				break;
#endif

#ifdef NXTV_TDMB_ENABLE
			case NXTV_TV_MODE_TDMB :
				NXTV_REG_SET(0x2C,0x48);
				NXTV_REG_SET(0x47,0xC0);
				break;
#endif

#ifdef NXTV_DAB_ENABLE
			case NXTV_TV_MODE_DAB_B3:
				NXTV_REG_SET(0x2C,0x48);
				NXTV_REG_SET(0x47,0xE0);
				NXTV_REG_SET(0x35,0x04);
				break;
#endif
			default:
			return NXTV_INVAILD_TV_MODE;
		}

		NXTV_REG_SET(0x2A,0x05);
		NXTV_REG_SET(0x2D,0x8c);
		NXTV_REG_SET(0x61,0x25);
    }
	else
    {
		switch( eTvMode )
		{
#ifdef NXTV_ISDBT_ENABLE
			case NXTV_TV_MODE_1SEG :
				NXTV_REG_SET(0x2C,0xC8);
				NXTV_REG_SET(0x47,0xC0);
				break;
#endif		

#ifdef NXTV_FM_ENABLE
			case NXTV_TV_MODE_FM :
				NXTV_REG_SET(0x2C,0xC8);
				NXTV_REG_SET(0x47,0xB1);
				NXTV_REG_SET(0x35,0x14);
				break;
#endif

#ifdef NXTV_TDMB_ENABLE
			case NXTV_TV_MODE_TDMB :
				NXTV_REG_SET(0x2C,0xC8);
				NXTV_REG_SET(0x47,0xB0);
				break;
#endif

#ifdef NXTV_DAB_ENABLE
			case NXTV_TV_MODE_DAB_B3:
				NXTV_REG_SET(0x2C,0xC8);
				NXTV_REG_SET(0x47,0xB0);
				NXTV_REG_SET(0x35,0x04);
				break;
#endif
			default:
			return NXTV_INVAILD_TV_MODE;
		}

		NXTV_REG_SET(0x2A,0x07);
		NXTV_REG_SET(0x2D,0xec);
		NXTV_REG_SET(0x61,0x2a);
    }
////////////////////////////////////////////////////////////////////////////////////	

	return NXTV_SUCCESS;
}

