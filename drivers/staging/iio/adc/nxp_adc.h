/*
 * Copyright (C) 2016  Nexell Co., Ltd.
 * Author: Bon-gyu, KOO <freestyle@nexell.co.kr>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * ADC definitions
 */
#ifdef CONFIG_ARCH_S5P4418
#define	ADC_MAX_SAMPLE_RATE		150000
#else
#define	ADC_MAX_SAMPLE_RATE		1000000
#endif
#define	ADC_MAX_SAMPLE_BITS		6
#define	ADC_MAX_PRESCALE		256			// 8bit
#define	ADC_MIN_PRESCALE		20			// 8bit

#define ADC_TIMEOUT			(msecs_to_jiffies(100))


/* Register definitions for ADC_V1 */
#define ADC_V1_CON(x)			((x) + 0x00)
#define ADC_V1_DAT(x)			((x) + 0x04)
#define ADC_V1_INTENB(x)		((x) + 0x08)
#define ADC_V1_INTCLR(x)		((x) + 0x0c)

/* Bit definitions for ADC_V1 */
#define ADC_V1_CON_APEN			(1u << 14)
#define ADC_V1_CON_APSV(x)		(((x) & 0xff) << 6)
#define ADC_V1_CON_ASEL(x)		(((x) & 0x7) << 3)
#define ADC_V1_CON_STBY			(1u << 2)
#define ADC_V1_CON_ADEN			(1u << 0)
#define ADC_V1_INTENB_ENB		(1u << 0)
#define ADC_V1_INTCLR_CLR		(1u << 0)

/* Register definitions for ADC_V2 */
#define ADC_V2_CON(x)			((x) + 0x00)
#define ADC_V2_DAT(x)			((x) + 0x04)
#define ADC_V2_INTENB(x)		((x) + 0x08)
#define ADC_V2_INTCLR(x)		((x) + 0x0c)
#define ADC_V2_PRESCON(x)		((x) + 0x10)

/* Bit definitions for ADC_V2 */
#define ADC_V2_CON_DATA_SEL(x)		(((x) & 0xf) << 10)
#define ADC_V2_CON_CLK_CNT(x)		(((x) & 0xf) << 6)
#define ADC_V2_CON_ASEL(x)		(((x) & 0x7) << 3)
#define ADC_V2_CON_STBY			(1u << 2)
#define ADC_V2_CON_ADEN			(1u << 0)
#define ADC_V2_INTENB_ENB		(1u << 0)
#define ADC_V2_INTCLR_CLR		(1u << 0)
#define ADC_V2_PRESCON_APEN		(1u << 15)
#define ADC_V2_PRESCON_PRES(x)		(((x) & 0x3ff) << 0)

#define ADC_V2_DATA_SEL_VAL		(0)	/* 0:5clk, 1:4clk, 2:3clk, 3:2clk */
						/* 4:1clk: 5:not delayed, else: 4clk */
#define ADC_V2_CLK_CNT_VAL		(6)	/* 28nm ADC */
