/*------------------------------------------------------------------------------
 *
 *	Copyright (C) 2009 Nexell Co., Ltd All Rights Reserved
 *	Nexell Co. Proprietary & Confidential
 *
 *	NEXELL INFORMS THAT THIS CODE AND INFORMATION IS PROVIDED "AS IS" BASE
 *  AND	WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING
 *  BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS
 *  FOR A PARTICULAR PURPOSE.
 *
 *	Module     : System memory config
 *	Description:
 *	Author     : Platform Team
 *	Export     :
 *	History    :
 *	   2009/05/13 first implementation
 ------------------------------------------------------------------------------*/
#ifndef __CFG_GPIO_H__
#define __CFG_GPIO_H__

/*------------------------------------------------------------------------------
 *
 *	(GROUP_A)
 *
 *	0 bit           8 bit                   12 bit          16 bit              20 bit
 *	| PAD_MODE_XXX  | PAD_FUNC_ALT(0,1,2,3) | PAD_LEVEL_XXX | PAD_PULL_UP,OFF | PAD_STRENGTH_0,1,2,3
 *
 -----------------------------------------------------------------------------*/
#define PAD_GPIOA0      (PAD_MODE_ALT  | PAD_FUNC_ALT1 | PAD_LEVEL_LOW   | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: PVCLK               ,2:_                    ,3: TESTMODE[4]         =MCU_DISY_CLK
#define PAD_GPIOA1      (PAD_MODE_ALT  | PAD_FUNC_ALT1 | PAD_LEVEL_LOW   | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: PDRGB24[0]          ,2:_                    ,3:_                    =MCU_DISD0
#define PAD_GPIOA2      (PAD_MODE_ALT  | PAD_FUNC_ALT1 | PAD_LEVEL_LOW   | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: PDRGB24[1]          ,2:_                    ,3: TESTMODE[0]         =MCU_DISD1
#define PAD_GPIOA3      (PAD_MODE_ALT  | PAD_FUNC_ALT1 | PAD_LEVEL_LOW   | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: PDRGB24[2]          ,2:_                    ,3: TESTMODE[1]         =MCU_DISD2
#define PAD_GPIOA4      (PAD_MODE_ALT  | PAD_FUNC_ALT1 | PAD_LEVEL_LOW   | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: PDRGB24[3]          ,2:_                    ,3: TESTMODE[2]         =MCU_DISD3
#define PAD_GPIOA5      (PAD_MODE_ALT  | PAD_FUNC_ALT1 | PAD_LEVEL_LOW   | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: PDRGB24[4]          ,2:_                    ,3: TESTMODE[3]         =MCU_DISD4
#define PAD_GPIOA6      (PAD_MODE_ALT  | PAD_FUNC_ALT1 | PAD_LEVEL_LOW   | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: PDRGB24[5]          ,2:_                    ,3:_                    =MCU_DISD5
#define PAD_GPIOA7      (PAD_MODE_ALT  | PAD_FUNC_ALT1 | PAD_LEVEL_LOW   | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: PDRGB24[6]          ,2:_                    ,3:_                    =MCU_DISD6
#define PAD_GPIOA8      (PAD_MODE_ALT  | PAD_FUNC_ALT1 | PAD_LEVEL_LOW   | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: PDRGB24[7]          ,2:_                    ,3:_                    =MCU_DISD7
#define PAD_GPIOA9      (PAD_MODE_ALT  | PAD_FUNC_ALT1 | PAD_LEVEL_LOW   | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: PDRGB24[8]          ,2:_                    ,3:_                    =MCU_DISD8
#define PAD_GPIOA10     (PAD_MODE_ALT  | PAD_FUNC_ALT1 | PAD_LEVEL_LOW   | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: PDRGB24[9]          ,2:_                    ,3:_                    =MCU_DISD9
#define PAD_GPIOA11     (PAD_MODE_ALT  | PAD_FUNC_ALT1 | PAD_LEVEL_LOW   | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: PDRGB24[10]         ,2:_                    ,3:_                    =MCU_DISD10
#define PAD_GPIOA12     (PAD_MODE_ALT  | PAD_FUNC_ALT1 | PAD_LEVEL_LOW   | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: PDRGB24[11]         ,2:_                    ,3:_                    =MCU_DISD11
#define PAD_GPIOA13     (PAD_MODE_OUT  | PAD_FUNC_ALT0 | PAD_LEVEL_LOW   | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: PDRGB24[12]         ,2:_                    ,3:_                    =TUNER_CTL0
#define PAD_GPIOA14     (PAD_MODE_OUT  | PAD_FUNC_ALT0 | PAD_LEVEL_LOW   | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: PDRGB24[13]         ,2:_                    ,3:_                    =TUNER_CTL1
#define PAD_GPIOA15     (PAD_MODE_OUT  | PAD_FUNC_ALT0 | PAD_LEVEL_LOW   | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: PDRGB24[14]         ,2:_                    ,3:_                    =TUNER_CTL2
#define PAD_GPIOA16     (PAD_MODE_OUT  | PAD_FUNC_ALT0 | PAD_LEVEL_LOW   | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: PDRGB24[15]         ,2:_                    ,3:-                    =TUNER_CTL3
#define PAD_GPIOA17     (PAD_MODE_OUT  | PAD_FUNC_ALT0 | PAD_LEVEL_LOW   | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: PDRGB24[16]         ,2:_                    ,3:_                    =TUNER_CTL4
#define PAD_GPIOA18     (PAD_MODE_IN   | PAD_FUNC_ALT0 | PAD_LEVEL_LOW   | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: PDRGB24[17]         ,2:_                    ,3:_                    =TOUCH_INT
#define PAD_GPIOA19     (PAD_MODE_OUT  | PAD_FUNC_ALT0 | PAD_LEVEL_LOW   | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: PDRGB24[18]         ,2:_                    ,3:_                    =TOUCH_RST
#define PAD_GPIOA20     (PAD_MODE_OUT  | PAD_FUNC_ALT0 | PAD_LEVEL_LOW   | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: PDRGB24[19]         ,2:_                    ,3:_                    =TUNER_RST
#define PAD_GPIOA21     (PAD_MODE_OUT  | PAD_FUNC_ALT0 | PAD_LEVEL_LOW   | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: PDRGB24[20]         ,2:_                    ,3:_                    =USBHUB_RST
#define PAD_GPIOA22     (PAD_MODE_OUT  | PAD_FUNC_ALT0 | PAD_LEVEL_LOW   | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: PDRGB24[21]         ,2:_                    ,3:_                    =REST_VIDEO_ENCODER
#define PAD_GPIOA23     (PAD_MODE_OUT  | PAD_FUNC_ALT0 | PAD_LEVEL_LOW   | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: PDRGB24[22]         ,2:_                    ,3:_                    =WL_RST
#define PAD_GPIOA24     (PAD_MODE_OUT  | PAD_FUNC_ALT0 | PAD_LEVEL_LOW   | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: PDRGB24[23]         ,2:_                    ,3:_                    =WIFI_PWN
#define PAD_GPIOA25     (PAD_MODE_ALT  | PAD_FUNC_ALT1 | PAD_LEVEL_LOW   | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: PDVSYNC             ,2:_                    ,3:_                    =MCU_DISY_VSYNC
#define PAD_GPIOA26     (PAD_MODE_ALT  | PAD_FUNC_ALT1 | PAD_LEVEL_LOW   | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: PDHSYNC             ,2:_                    ,3:_                    =MCU_DISY_HSYNC
#define PAD_GPIOA27     (PAD_MODE_OUT  | PAD_FUNC_ALT0 | PAD_LEVEL_LOW   | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: PDDE                ,2:_                    ,3:_                    =MCU_VG_EN
#define PAD_GPIOA28     (PAD_MODE_IN   | PAD_FUNC_ALT0 | PAD_LEVEL_LOW   | PAD_PULL_DN  | PAD_STRENGTH_0)     // 0: GPIO          ,1: VP0_EXTCLK          ,2: I2S2_CLK            ,3: I2S1_CLK            =NC
#define PAD_GPIOA29     (PAD_MODE_ALT  | PAD_FUNC_ALT1 | PAD_LEVEL_LOW   | PAD_PULL_OFF | PAD_STRENGTH_1)     // 0: GPIO          ,1: SDMMC0_CCLK         ,2:_                    ,3:_                    =MCU_EMMC_SD0_CLK
#define PAD_GPIOA30     (PAD_MODE_IN   | PAD_FUNC_ALT0 | PAD_LEVEL_LOW   | PAD_PULL_DN  | PAD_STRENGTH_0)     // 0: GPIO          ,1: VIP0_VD[0]          ,2: SDEX[0]             ,3: I2S1_BCLK           =VID1_0 (NC)
#define PAD_GPIOA31     (PAD_MODE_ALT  | PAD_FUNC_ALT1 | PAD_LEVEL_LOW   | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: SDMMC0_CMD          ,2:_                    ,3:_                    =MCU_EMMC_SD0_CMD

/*------------------------------------------------------------------------------
 *	(GROUP_B)
 *
 *	0 bit           8 bit                   12 bit          16 bit              20 bit
 *	| PAD_MODE_XXX  | PAD_FUNC_ALT(0,1,2,3) | PAD_LEVEL_XXX | PAD_PULL_UP,OFF | PAD_STRENGTH_0,1,2,3
 *
 -----------------------------------------------------------------------------*/
#define PAD_GPIOB0      (PAD_MODE_IN  | PAD_FUNC_ALT0 | PAD_LEVEL_LOW  | PAD_PULL_DN  | PAD_STRENGTH_0)     // 0: GPIO          ,1: VIP0_VD[1]          ,2: SDEX[1]             ,3: I2S1_LRCLK          =VID1_1	(NC)
#define PAD_GPIOB1      (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: SDMMC0_CDATA[0]     ,2:_                    ,3:_                    =MCU_EMMC_SD0_D0
#define PAD_GPIOB2      (PAD_MODE_IN  | PAD_FUNC_ALT0 | PAD_LEVEL_LOW  | PAD_PULL_DN  | PAD_STRENGTH_0)     // 0: GPIO          ,1: VIP0_VD[2]          ,2: SDEX[2]             ,3: I2S2_BCLK           =VID1_2	(NC)
#define PAD_GPIOB3      (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: SDMMC0_CDATA[1]     ,2:_                    ,3:_                    =MCU_EMMC_SD0_D1
#define PAD_GPIOB4      (PAD_MODE_IN  | PAD_FUNC_ALT0 | PAD_LEVEL_LOW  | PAD_PULL_DN  | PAD_STRENGTH_0)     // 0: GPIO          ,1: VIP0_VD[3]          ,2: SDEX[3]             ,3: I2S2_LRCLK          =VID1_3	(NC)
#define PAD_GPIOB5      (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: SDMMC0_CDATA[2]     ,2:_                    ,3:_                    =MCU_EMMC_SD0_D2
#define PAD_GPIOB6      (PAD_MODE_IN  | PAD_FUNC_ALT0 | PAD_LEVEL_LOW  | PAD_PULL_DN  | PAD_STRENGTH_0)     // 0: GPIO          ,1: VIP0_VD[4]          ,2: SDEX[4]             ,3: I2S1SDO             =MCU_ETHIRQ
#define PAD_GPIOB7      (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: SDMMC0_CDATA[3]     ,2:_                    ,3:_                    =MCU_EMMC_SD0_D3
#define PAD_GPIOB8      (PAD_MODE_OUT | PAD_FUNC_ALT0 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: VIP0_VD[5]          ,2: SDEX[5]             ,3: I2S2SDO             =MCU_nETHRST
#define PAD_GPIOB9      (PAD_MODE_IN  | PAD_FUNC_ALT0 | PAD_LEVEL_LOW  | PAD_PULL_DN  | PAD_STRENGTH_0)     // 0: GPIO          ,1: VIP0_VD[6]          ,2: SDEX[6]             ,3: I2S1SDI             =MCU_EMMC_RESET1 (NC)
#define PAD_GPIOB10     (PAD_MODE_IN  | PAD_FUNC_ALT0 | PAD_LEVEL_LOW  | PAD_PULL_DN  | PAD_STRENGTH_0)     // 0: GPIO          ,1: VIP0_VD[7]          ,2: SDEX[7]             ,3: I2S2SDI             =VID1_7 (NC)
#define PAD_GPIOB11     (PAD_MODE_ALT | PAD_FUNC_ALT0 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: CLE           ,1: CLE1                ,2: GPIO                ,3:_                    =MCU_CLE (NAND)
#define PAD_GPIOB12     (PAD_MODE_ALT | PAD_FUNC_ALT0 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: ALE           ,1: ALE1                ,2: GPIO                ,3:_                    =MCU_ALE (NAND)
#define PAD_GPIOB13     (PAD_MODE_ALT | PAD_FUNC_ALT0 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: MCUS_SD[0]    ,1: GPIO                ,2:_                    ,3:_                    =MCU_SD0 (NAND)
#define PAD_GPIOB14     (PAD_MODE_ALT | PAD_FUNC_ALT0 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: MCUS_RnB      ,1: MCUS_RnB1           ,2: GPIO                ,3:_                    =MCU_RnB (NAND)
#define PAD_GPIOB15     (PAD_MODE_ALT | PAD_FUNC_ALT0 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: MCUS_SD[1]    ,1: GPIO                ,2:_                    ,3:_                    =MCU_SD1 (NAND)
#define PAD_GPIOB16     (PAD_MODE_ALT | PAD_FUNC_ALT0 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: MCUS_nNOFE    ,1: MCUS_nNOFE1         ,2: GPIO                ,3:_                    =MCU_nNFOE (NAND)
#define PAD_GPIOB17     (PAD_MODE_ALT | PAD_FUNC_ALT0 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: MCUS_SD[2]    ,1: GPIO                ,2:_                    ,3:_                    =MCU_SD2 (NAND)
#define PAD_GPIOB18     (PAD_MODE_ALT | PAD_FUNC_ALT0 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: MCUS_nNFWE    ,1: MCUS_nNFWE1         ,2: GPIO                ,3:_                    =MCU_nNFWE (NAND)
#define PAD_GPIOB19     (PAD_MODE_ALT | PAD_FUNC_ALT0 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: MCUS_SD[3]    ,1: GPIO                ,2:_                    ,3:_                    =MCU_SD3 (NAND)
#define PAD_GPIOB20     (PAD_MODE_ALT | PAD_FUNC_ALT0 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: MCUS_SD[4]    ,1: GPIO                ,2:_                    ,3:_                    =MCU_SD4 (NAND)
#define PAD_GPIOB21     (PAD_MODE_ALT | PAD_FUNC_ALT0 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: MCUS_SD[5]    ,1: GPIO                ,2:_                    ,3:_                    =MCU_SD5 (NAND)
#define PAD_GPIOB22     (PAD_MODE_ALT | PAD_FUNC_ALT0 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: MCUS_SD[6]    ,1: GPIO                ,2:_                    ,3:_                    =MCU_SD6 (NAND)
#define PAD_GPIOB23     (PAD_MODE_ALT | PAD_FUNC_ALT0 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: MCUS_SD[7]    ,1: GPIO                ,2:_                    ,3:_                    =MCU_SD7 (NAND)
#define PAD_GPIOB24     (PAD_MODE_ALT | PAD_FUNC_ALT2 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: MCUS_SD[8]    ,1: GPIO                ,2: MPEGTSI0_TDATA[0]   ,3:_                    =TS0_D0
#define PAD_GPIOB25     (PAD_MODE_ALT | PAD_FUNC_ALT2 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: MCUS_SD[9]    ,1: GPIO                ,2: MPEGTSI0_TDATA[1]   ,3:_                    =TS0_D1
#define PAD_GPIOB26     (PAD_MODE_ALT | PAD_FUNC_ALT2 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: MCUS_SD[10]   ,1: GPIO                ,2: MPEGTSI0_TDATA[2]   ,3: ECID_BONDING_ID[2]  =TS0_D2
#define PAD_GPIOB27     (PAD_MODE_ALT | PAD_FUNC_ALT2 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: MCUS_SD[11]   ,1: GPIO                ,2: MPEGTSI0_TDATA[3]   ,3:_                    =TS0_D3
#define PAD_GPIOB28     (PAD_MODE_ALT | PAD_FUNC_ALT2 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: MCUS_SD[12]   ,1: GPIO                ,2: MPEGTSI0_TDATA[4]   ,3: UART4_RXD           =TS0_D4
#define PAD_GPIOB29     (PAD_MODE_ALT | PAD_FUNC_ALT2 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: MCUS_SD[13]   ,1: GPIO                ,2: MPEGTSI0_TDATA[5]   ,3: UART4_TXD           =TS0_D5
#define PAD_GPIOB30     (PAD_MODE_ALT | PAD_FUNC_ALT2 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: MCUS_SD[14]   ,1: GPIO                ,2: MPEGTSI0_TDATA[6]   ,3: UART5_RXD           =TS0_D6
#define PAD_GPIOB31     (PAD_MODE_ALT | PAD_FUNC_ALT2 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: MCUS_SD[15]   ,1: GPIO                ,2: MPEGTSI0_TDATA[7]   ,3: UART5_TXD           =TS0_D7

/*------------------------------------------------------------------------------
 *	(GROUP_C)
 *
 *	0 bit           8 bit                   12 bit          16 bit              20 bit
 *	| PAD_MODE_XXX  | PAD_FUNC_ALT(0,1,2,3) | PAD_LEVEL_XXX | PAD_PULL_UP,OFF | PAD_STRENGTH_0,1,2,3
 *
 -----------------------------------------------------------------------------*/
#define PAD_GPIOC0      (PAD_MODE_ALT | PAD_FUNC_ALT2 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: MCUS_ADDR[0]  ,1: GPIO                ,2: MPEGTSI0_TSERR      ,3:_                    =TS0_ERR
#define PAD_GPIOC1      (PAD_MODE_ALT | PAD_FUNC_ALT2 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: MCUS_ADDR[1]  ,1: GPIO                ,2: MPEGTSI1_TSERR      ,3:_                    =TS1_ERR
#define PAD_GPIOC2      (PAD_MODE_IN  | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_DN  | PAD_STRENGTH_0)     // 0: MCUS_ADDR[2]  ,1: GPIO                ,2:_                    ,3:_                    =NC
#define PAD_GPIOC3      (PAD_MODE_ALT | PAD_FUNC_ALT2 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: MCUS_ADDR[3]  ,1: GPIO                ,2: HDMI_CEC            ,3: SDMMC0_nRST         =MCU_HDMI_CEC
#define PAD_GPIOC4      (PAD_MODE_OUT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: MCUS_ADDR[4]  ,1: GPIO                ,2: UART1_DCD           ,3: SDMMC0_CARD_nint    =MCU_SMC_VCCB
#define PAD_GPIOC5      (PAD_MODE_OUT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: MCUS_ADDR[5]  ,1: GPIO                ,2: UART1_CTS           ,3: SDMMC0_WP           =PMIC_SDA
#define PAD_GPIOC6      (PAD_MODE_OUT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: MCUS_ADDR[6]  ,1: GPIO                ,2: UART1_RTS           ,3: SDMMC0_DETECT       =PMIC_SCL
#define PAD_GPIOC7      (PAD_MODE_OUT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: MCUS_ADDR[7]  ,1: GPIO                ,2: UART1_DSR           ,3: SDMMC1_nRST         =MCU_SMC_RSTB
#define PAD_GPIOC8      (PAD_MODE_OUT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: MCUS_ADDR[8]  ,1: GPIO                ,2: UART1_DTR           ,3: SDMMC1_CARD_nint    =MCU_SMC_PRES
#define PAD_GPIOC9      (PAD_MODE_ALT | PAD_FUNC_ALT2 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: MCUS_ADDR[9]  ,1: GPIO                ,2: SSP2_CLK_IO         ,3: PDM_STROBE          =MCU_SSPCLK2
#define PAD_GPIOC10     (PAD_MODE_ALT | PAD_FUNC_ALT2 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: MCUS_ADDR[10] ,1: GPIO                ,2: SSP2_FSS            ,3: MCUS_nNCS[2]        =MCU_SSPFRM2
#define PAD_GPIOC11     (PAD_MODE_ALT | PAD_FUNC_ALT2 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: MCUS_ADDR[11] ,1: GPIO                ,2: SSP2_RXD            ,3: USB2.0OTG0_DRVBUS   =MCU_SSPRXD2
#define PAD_GPIOC12     (PAD_MODE_ALT | PAD_FUNC_ALT2 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: MCUS_ADDR[12] ,1: GPIO                ,2: SSP2_TXD            ,3: SDMMC2_nRST         =MCU_SSPTXD2
#define PAD_GPIOC13     (PAD_MODE_ALT | PAD_FUNC_ALT2 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: MCUS_ADDR[13] ,1: GPIO                ,2: PWM1_OUT            ,3: SDMMC2_CARD_nint    =MCU_SMC_CLK (PWM)
#define PAD_GPIOC14     (PAD_MODE_OUT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: MCUS_ADDR[14] ,1: GPIO                ,2: PWM2_OUT            ,3: VIP0_ExtCLK2        =5V_PWR_EN
#define PAD_GPIOC15     (PAD_MODE_ALT | PAD_FUNC_ALT2 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: MCUS_ADDR[15] ,1: GPIO                ,2: MPEGTSI0_TSCLK      ,3: VIP0_HSYNC2         =TSI0_CLK
#define PAD_GPIOC16     (PAD_MODE_ALT | PAD_FUNC_ALT2 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: MCUS_ADDR[16] ,1: GPIO                ,2: MPEGTSI0_TSYNC0     ,3: VIP0_VSYNC2         =TSI0_SYNC
#define PAD_GPIOC17     (PAD_MODE_ALT | PAD_FUNC_ALT2 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: MCUS_ADDR[17] ,1: GPIO                ,2: MPEGTSI0_TDP0       ,3: VIP0_VD2[0]         =TSI0_VALID
#define PAD_GPIOC18     (PAD_MODE_ALT | PAD_FUNC_ALT2 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_2)     // 0: MCUS_ADDR[18] ,1: GPIO                ,2: SDMMC2_CCLK         ,3: VIP0_VD2[1]         =MCU_SD2_CLK
#define PAD_GPIOC19     (PAD_MODE_ALT | PAD_FUNC_ALT2 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: MCUS_ADDR[19] ,1: GPIO                ,2: SDMMC2_CMD          ,3: VIP0_VD2[2]         =MCU_SD2_CMD
#define PAD_GPIOC20     (PAD_MODE_ALT | PAD_FUNC_ALT2 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: MCUS_ADDR[20] ,1: GPIO                ,2: SDMMC2_CDATA[0]     ,3: VIP0_VD2[3]         =MCU_SD2_D0
#define PAD_GPIOC21     (PAD_MODE_ALT | PAD_FUNC_ALT2 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: MCUS_ADDR[21] ,1: GPIO                ,2: SDMMC2_CDATA[1]     ,3: VIP0_VD2[4]         =MCU_SD2_D1
#define PAD_GPIOC22     (PAD_MODE_ALT | PAD_FUNC_ALT2 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: MCUS_ADDR[22] ,1: GPIO                ,2: SDMMC2_CDATA[2]     ,3: VIP0_VD2[5]         =MCU_SD2_D2
#define PAD_GPIOC23     (PAD_MODE_ALT | PAD_FUNC_ALT2 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: MCUS_ADDR[23] ,1: GPIO                ,2: SDMMC2_CDATA[3]     ,3: VIP0_VD2[6]         =MCU_SD2_D3
#define PAD_GPIOC24     (PAD_MODE_IN  | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_DN  | PAD_STRENGTH_0)     // 0: MCUS_LATADDR  ,1: GPIO                ,2: SPDIFIN             ,3: VIP0_VD2[7]         =NC
#define PAD_GPIOC25     (PAD_MODE_ALT | PAD_FUNC_ALT2 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: MCUS_nSWAIT   ,1: GPIO                ,2: SPDIF_DATA          ,3:_                    =MCU_SPDIF
#define PAD_GPIOC26     (PAD_MODE_IN  | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: MCUS_RDnWR    ,1: GPIO                ,2: PDM_DATA0           ,3:_                    =NC
#define PAD_GPIOC27     (PAD_MODE_IN  | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_DN  | PAD_STRENGTH_0)     // 0: MCUS_nSDQM1   ,1: GPIO                ,2: PDM_DATA1           ,3:_                    =MCU_NAND_WP
#define PAD_GPIOC28     (PAD_MODE_OUT | PAD_FUNC_ALT0 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: MCUS_nSCS[1]        ,2: UART1_TRI           ,3:_                    =MCU_LED_STATE
#define PAD_GPIOC29     (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: SSP0_CLKIO          ,2:_                    ,3:_                    =MCU_SSPCLK
#define PAD_GPIOC30     (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: SSP0_FSS            ,2:_                    ,3:_                    =MCU_SSPFRM
#define PAD_GPIOC31     (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: SSP0_TXD            ,2:_                    ,3:_                    =MCU_SSPTXD

/*------------------------------------------------------------------------------
 *	(GROUP_D)
 *
 *	0 bit           8 bit                   12 bit          16 bit              20 bit
 *	| PAD_MODE_XXX  | PAD_FUNC_ALT(0,1,2,3) | PAD_LEVEL_XXX | PAD_PULL_UP,OFF | PAD_STRENGTH_0,1,2,3
 *
 -----------------------------------------------------------------------------*/
#define PAD_GPIOD0      (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: SSP0_RXD            ,2: PWM3_OUT            ,3:_                    =MCU_SSPRXD
#define PAD_GPIOD1      (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: PWM0_OUT            ,2: MCUS_ADDR[25]       ,3:_                    =MCU_BACKLIGHT_PWM
#define PAD_GPIOD2      (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_1)     // 0: GPIO          ,1: I2C0_SCL            ,2: UART4_SMCAYEN       ,3:_                    =MCU_SCL_0
#define PAD_GPIOD3      (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_1)     // 0: GPIO          ,1: I2C0_SDA            ,2: UART5_SMCAYEN       ,3:_                    =MCU_SDA_0
#define PAD_GPIOD4      (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: I2C1_SCL            ,2:_                    ,3:_                    =MCU_SCL_1
#define PAD_GPIOD5      (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: I2C1_SDA            ,2:_                    ,3:_                    =MCU_SDA_1
#define PAD_GPIOD6      (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: I2C2_SCL            ,2:_                    ,3:_                    =MCU_SCL_2
#define PAD_GPIOD7      (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: I2C2_SDA            ,2:_                    ,3:_                    =MCU_SDA_2
#define PAD_GPIOD8      (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: PPM_IN              ,2:_                    ,3:_                    =MCU_IR_INT
#define PAD_GPIOD9      (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: I2S0_SDO            ,2: AC97_ACSDATAOUT     ,3:_                    =MCU_I2S_SDOUT
#define PAD_GPIOD10     (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: I2S0_BCLK           ,2: AC97_ACBITCLK       ,3:_                    =MCU_I2S_BCK
#define PAD_GPIOD11     (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: I2S0_SDI            ,2: AC97_ACSDATAIN      ,3:_                    =MCU_I2S_SDIN
#define PAD_GPIOD12     (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: I2S0_LRCLK          ,2: AC97_ACSYNC         ,3:_                    =MCU_I2S_LRCK
#define PAD_GPIOD13     (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: I2S0_CODCLK         ,2: AC97_nACRESET       ,3:_                    =MCU_I2S_MCLK
#define PAD_GPIOD14     (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_UP  | PAD_STRENGTH_0)     // 0: GPIO          ,1: UART0RXD            ,2: UART1_SMCAYEN       ,3:_                    =MCU_UART0_RX
#define PAD_GPIOD15     (PAD_MODE_ALT | PAD_FUNC_ALT0 | PAD_LEVEL_LOW  | PAD_PULL_UP  | PAD_STRENGTH_0)     // 0: GPIO          ,1: UART1RXD            ,2: UART2_SMCAYEN       ,3:_                    =MCU_UART1_RX
#define PAD_GPIOD16     (PAD_MODE_ALT | PAD_FUNC_ALT0 | PAD_LEVEL_LOW  | PAD_PULL_UP  | PAD_STRENGTH_0)     // 0: GPIO          ,1: UART2RXD            ,2: CAN0_TX             ,3:_                    =MCU_UART2_RX
#define PAD_GPIOD17     (PAD_MODE_ALT | PAD_FUNC_ALT0 | PAD_LEVEL_LOW  | PAD_PULL_UP  | PAD_STRENGTH_0)     // 0: GPIO          ,1: UART3RXD            ,2: CAN1_TX             ,3:_                    =MCU_UART3_RX
#define PAD_GPIOD18     (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: UART0TXD            ,2:_                    ,3: SDnCD2              =MCU_UART0_TX
#define PAD_GPIOD19     (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: UART1TXD            ,2:_                    ,3:_                    =MCU_UART1_TX
#define PAD_GPIOD20     (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: UART2TXD            ,2:_                    ,3:_                    =MCU_UART2_TX
#define PAD_GPIOD21     (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: UART3TXD            ,2:_                    ,3:_                    =MCU_UART3_TX
#define PAD_GPIOD22     (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: SDMMC1_CCLK         ,2:_                    ,3:_                    =MCU_WIFI_SD1_CLK
#define PAD_GPIOD23     (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: SDMMC1_CMD          ,2:_                    ,3:_                    =MCU_WIFI_SD1_CMD
#define PAD_GPIOD24     (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: SDMMC1_CDATA[0]     ,2:_                    ,3:_                    =MCU_WIFI_SD1_D0
#define PAD_GPIOD25     (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: SDMMC1_CDATA[1]     ,2:_                    ,3:_                    =MCU_WIFI_SD1_D1
#define PAD_GPIOD26     (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: SDMMC1_CDATA[3]     ,2:_                    ,3:_                    =MCU_WIFI_SD1_D2
#define PAD_GPIOD27     (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: SDMMC1_CDATA[3]     ,2:_                    ,3:_                    =MCU_WIFI_SD1_D3
#define PAD_GPIOD28     (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: VIP1_VD[0]          ,2: MPEGTSI_TDATA1[0]   ,3: MCUS_ADDR[24]       =TSI1_D0
#define PAD_GPIOD29     (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: VIP1_VD[1]          ,2: MPEGTSI_TDATA1[1]   ,3:_                    =TSI1_D1
#define PAD_GPIOD30     (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: VIP1_VD[2]          ,2: MPEGTSI_TDATA1[2]   ,3:_                    =TSI1_D2
#define PAD_GPIOD31     (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          ,1: VIP1_VD[3]          ,2: MPEGTSI_TDATA1[3]   ,3:_                    =TSI1_D3

/*------------------------------------------------------------------------------
 *	(GROUP_E)
 *
 *	0 bit           8 bit                   12 bit          16 bit              20 bit
 *	| PAD_MODE_XXX  | PAD_FUNC_ALT(0,1,2,3) | PAD_LEVEL_XXX | PAD_PULL_UP,OFF | PAD_STRENGTH_0,1,2,3
 *
 -----------------------------------------------------------------------------*/
#define PAD_GPIOE0      (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          1: VIP1_VD[4]           ,2: MPEGTSI_TDATA1[0]   ,3:_                    =TSI1_D4
#define PAD_GPIOE1      (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          1: VIP1_VD[5]           ,2: MPEGTSI_TDATA1[0]   ,3:_                    =TSI1_D5
#define PAD_GPIOE2      (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          1: VIP1_VD[6]           ,2: MPEGTSI_TDATA1[0]   ,3:_                    =TSI1_D6
#define PAD_GPIOE3      (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          1: VIP1_VD[7]           ,2: MPEGTSI_TDATA1[0]   ,3:_                    =TSI1_D7
#define PAD_GPIOE4      (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          1: VIP1_ExtCLK          ,2: MPEGTSI_TCLK1       ,3:_                    =TSI1_CLK
#define PAD_GPIOE5      (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          1: VIP1_HSYNC           ,2: MPEGTSI_TSYNC1      ,3:_                    =TSI1_SYNC
#define PAD_GPIOE6      (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          1: VIP_VSYNC            ,2: MPEGTSI_TDP1        ,3:_                    =TSI1_DP
#define PAD_GPIOE7      (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          1: GMAC0_PHY_TXD[0]     ,2: VIP0_Ext_VSYNC      ,3:_                    =GMAC_TXD0
#define PAD_GPIOE8      (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          1: GMAC0_PHY_TXD[1]     ,2:_                    ,3:_                    =GMAC_TXD1
#define PAD_GPIOE9      (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          1: GMAC0_PHY_TXD[2]     ,2:_                    ,3:_                    =GMAC_TXD2
#define PAD_GPIOE10     (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          1: GMAC0_PHY_TXD[3]     ,2:_                    ,3:_                    =GMAC_TXD3
#define PAD_GPIOE11     (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          1: GMAC0_PHY_TXEN       ,2:_                    ,3:_                    =GMAC_TXEN
#define PAD_GPIOE12     (PAD_MODE_IN  | PAD_FUNC_ALT0 | PAD_LEVEL_LOW  | PAD_PULL_DN  | PAD_STRENGTH_0)     // 0: GPIO          1: GMAC0_PHY_TXER       ,2:_                    ,3:_                    =NC
#define PAD_GPIOE13     (PAD_MODE_IN  | PAD_FUNC_ALT0 | PAD_LEVEL_LOW  | PAD_PULL_DN  | PAD_STRENGTH_0)     // 0: GPIO          1: GMAC0_PHY_COL        ,2: VIP0_Ext_HSYNC      ,3:_                    =NC
#define PAD_GPIOE14     (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          1: GMAC0_PHY_RXD[0]     ,2: SSP1_CLKIO          ,3:_                    =GMAC_RXD0
#define PAD_GPIOE15     (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          1: GMAC0_PHY_RXD[1]     ,2: SSP1_FSS            ,3:_                    =GMAC_RXD1
#define PAD_GPIOE16     (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          1: GMAC0_PHY_RXD[2]     ,2:_                    ,3:_                    =GMAC_RXD2
#define PAD_GPIOE17     (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          1: GMAC0_PHY_RXD[3]     ,2:_                    ,3:_                    =GMAC_RXD3
#define PAD_GPIOE18     (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          1: GMAC0_CLK_RX         ,2: SSP1_RXD            ,3:_                    =GMAC_RXCLK
#define PAD_GPIOE19     (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          1: GMAC0_PHY_RX_DV      ,2: SSP1_TXD            ,3:_                    =GMAC_RXDV
#define PAD_GPIOE20     (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          1: GMAC0_GMII_MDC       ,2:_                    ,3:_                    =GMAC_MDC
#define PAD_GPIOE21     (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          1: GMAC0_GMII_MDI       ,2:_                    ,3:_                    =GMAC_MDIO
#define PAD_GPIOE22     (PAD_MODE_IN  | PAD_FUNC_ALT0 | PAD_LEVEL_LOW  | PAD_PULL_DN  | PAD_STRENGTH_0)     // 0: GPIO          1: GMAC0_PHY_RXER       ,2:_                    ,3:_                    =NC
#define PAD_GPIOE23     (PAD_MODE_IN  | PAD_FUNC_ALT0 | PAD_LEVEL_LOW  | PAD_PULL_DN  | PAD_STRENGTH_0)     // 0: GPIO          1: GMAC0_PHY_CRS        ,2:_                    ,3:_                    =NC
#define PAD_GPIOE24     (PAD_MODE_ALT | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: GPIO          1: GMAC0_GTX_CLK        ,2:_                    ,3:_                    =GMAC_TXCLK
#define PAD_GPIOE25     (PAD_MODE_ALT | PAD_FUNC_ALT0 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: nTRST         1: GPIO                 ,2:_                    ,3:_                    =JTAG_INTRST(NC)
#define PAD_GPIOE26     (PAD_MODE_ALT | PAD_FUNC_ALT0 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: TMS           1: GPIO                 ,2:_                    ,3:_                    =JTAG_TMS(NC)
#define PAD_GPIOE27     (PAD_MODE_ALT | PAD_FUNC_ALT0 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: TDI           1: GPIO                 ,2:_                    ,3:_                    =JTAG_TDI(NC)
#define PAD_GPIOE28     (PAD_MODE_ALT | PAD_FUNC_ALT0 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: TCLK          1: GPIO                 ,2:_                    ,3:_                    =JTAG_TCLK(NC)
#define PAD_GPIOE29     (PAD_MODE_ALT | PAD_FUNC_ALT0 | PAD_LEVEL_LOW  | PAD_PULL_OFF | PAD_STRENGTH_0)     // 0: TDO           1: GPIO                 ,2:_                    ,3:_                    =JTAG_TDO(NC)
#define PAD_GPIOE30     (PAD_MODE_IN  | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_DN  | PAD_STRENGTH_0)     // 0: MCUS_nSOE     1: GPIO                 ,2:_                    ,3:_                    =NC
#define PAD_GPIOE31     (PAD_MODE_IN  | PAD_FUNC_ALT1 | PAD_LEVEL_LOW  | PAD_PULL_DN  | PAD_STRENGTH_0)     // 0: MCUS_nSWE     1: GPIO                 ,2:_                    ,3:_                    =NC

/*------------------------------------------------------------------------------
 *	(GROUPALV)
 *	0                     4                             8        12
 *	| MODE(IN/OUT/DETECT) | ALIVE OUT or ALIVE DETMODE0 | PullUp |
 *
 -----------------------------------------------------------------------------*/
#define PAD_GPIOALV0    (PAD_MODE_IN  | PAD_LEVEL_LOW  | PAD_PULL_UP )				//MCU_NVDDPOWERTOGGLE
#define PAD_GPIOALV1    (PAD_MODE_IN  | PAD_LEVEL_LOW  | PAD_PULL_UP )				//MCU_SD2_CD
#define PAD_GPIOALV2    (PAD_MODE_IN  | PAD_LEVEL_LOW  | PAD_PULL_UP )				//
#define PAD_GPIOALV3    (PAD_MODE_IN  | PAD_LEVEL_LOW  | PAD_PULL_UP )				//
#define PAD_GPIOALV4    (PAD_MODE_IN  | PAD_LEVEL_LOW  | PAD_PULL_UP )				//MCU_PMIC_INT
#define PAD_GPIOALV5    (PAD_MODE_IN  | PAD_LEVEL_LOW  | PAD_PULL_UP )				//RT_PMEB(NC)

/*------------------------------------------------------------------------------
 *	TOUCH
 */
#define	CFG_IO_TOUCH_PENDOWN_DETECT			(PAD_GPIO_A + 18)
#define	CFG_IO_TOUCH_RESET_PIN				(PAD_GPIO_A + 19)						/* for aw5306 */


/*------------------------------------------------------------------------------
 *	AUDIO AMP for wm8976
 */
#define CFG_IO_AUDIO_AMP_POWER				(PAD_GPIO_E + 31)		/* GPIO */


#endif	/* __CFG_GPIO_H__ */

