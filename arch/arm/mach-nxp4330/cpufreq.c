/*
 * (C) Copyright 2009
 * jung hyun kim, Nexell Co, <jhkim@nexell.co.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <mach/platform.h>

#ifdef CONFIG_ARM_NXP4330_CPUFREQ_DEBUG
#define	DBGOUT(msg...)		printk(msg)
#else
#define	DBGOUT(msg...)
#endif

struct pll_pms {
	long rate;	/* unint Khz */
	int	P;
	int	M;
	int	S;
};

// PLL 0,1
static struct pll_pms pll0_1_pms [] =
{
	[ 0] = { .rate = 2000000, .P = 6, .M = 500, .S = 0, },
	[ 1] = { .rate = 1900000, .P = 6, .M = 475, .S = 0, },
	[ 2] = { .rate = 1800000, .P = 4, .M = 300, .S = 0, },
	[ 3] = { .rate = 1700000, .P = 6, .M = 425, .S = 0, },
	[ 4] = { .rate = 1600000, .P = 3, .M = 400, .S = 1, },
	[ 5] = { .rate = 1500000, .P = 3, .M = 375, .S = 1, },
	[ 6] = { .rate = 1400000, .P = 3, .M = 350, .S = 1, },
	[ 7] = { .rate = 1300000, .P = 3, .M = 325, .S = 1, },
	[ 8] = { .rate = 1200000, .P = 3, .M = 300, .S = 1, },
	[ 9] = { .rate = 1100000, .P = 6, .M = 550, .S = 1, },
	[10] = { .rate = 1000000, .P = 6, .M = 500, .S = 1, },
	[11] = { .rate =  900000, .P = 4, .M = 300, .S = 1, },
	[12] = { .rate =  800000, .P = 6, .M = 400, .S = 1, },
	[13] = { .rate =  780000, .P = 4, .M = 260, .S = 1, },
	[14] = { .rate =  760000, .P = 6, .M = 380, .S = 1, },
	[15] = { .rate =  740000, .P = 6, .M = 370, .S = 1, },
	[16] = { .rate =  720000, .P = 4, .M = 240, .S = 1, },
	[17] = { .rate =  562000, .P = 6, .M = 562, .S = 2, },
	[18] = { .rate =  533000, .P = 6, .M = 533, .S = 2, },
	[19] = { .rate =  490000, .P = 6, .M = 490, .S = 2, },
	[20] = { .rate =  470000, .P = 6, .M = 470, .S = 2, },
	[21] = { .rate =  460000, .P = 6, .M = 460, .S = 2, },
	[22] = { .rate =  450000, .P = 4, .M = 300, .S = 2, },
	[23] = { .rate =  440000, .P = 6, .M = 440, .S = 2, },
	[24] = { .rate =  430000, .P = 6, .M = 430, .S = 2, },
	[25] = { .rate =  420000, .P = 4, .M = 280, .S = 2, },
	[26] = { .rate =  410000, .P = 6, .M = 410, .S = 2, },
	[27] = { .rate =  400000, .P = 6, .M = 400, .S = 2, },
	[28] = { .rate =  399000, .P = 4, .M = 266, .S = 2, },
	[29] = { .rate =  390000, .P = 4, .M = 260, .S = 2, },
	[30] = { .rate =  384000, .P = 4, .M = 256, .S = 2, },
	[31] = { .rate =  350000, .P = 6, .M = 350, .S = 2, },
	[32] = { .rate =  330000, .P = 4, .M = 220, .S = 2, },
	[33] = { .rate =  300000, .P = 4, .M = 400, .S = 3, },
	[34] = { .rate =  266000, .P = 6, .M = 532, .S = 3, },
	[35] = { .rate =  250000, .P = 6, .M = 500, .S = 3, },
	[36] = { .rate =  220000, .P = 6, .M = 440, .S = 3, },
	[37] = { .rate =  200000, .P = 6, .M = 400, .S = 3, },
	[38] = { .rate =  166000, .P = 6, .M = 332, .S = 3, },
	[39] = { .rate =  147500, .P = 6, .M = 590, .S = 4, },	// 147.456000
	[40] = { .rate =  133000, .P = 6, .M = 532, .S = 4, },
	[41] = { .rate =  125000, .P = 6, .M = 500, .S = 4, },
	[42] = { .rate =  100000, .P = 6, .M = 400, .S = 4, },
	[43] = { .rate =   96000, .P = 4, .M = 256, .S = 4, },
	[44] = { .rate =   48000, .P = 3, .M =  96, .S = 4, },
};

// PLL 2,3
static struct pll_pms pll2_3_pms [] =
{
	[ 0] = { .rate = 2000000, .P = 3, .M = 250, .S = 0, },
	[ 1] = { .rate = 1900000, .P = 4, .M = 317, .S = 0, },
	[ 2] = { .rate = 1800000, .P = 3, .M = 225, .S = 0, },
	[ 3] = { .rate = 1700000, .P = 4, .M = 283, .S = 0, },
	[ 4] = { .rate = 1600000, .P = 3, .M = 200, .S = 0, },
	[ 5] = { .rate = 1500000, .P = 4, .M = 250, .S = 0, },
	[ 6] = { .rate = 1400000, .P = 3, .M = 175, .S = 0, },
	[ 7] = { .rate = 1300000, .P = 4, .M = 217, .S = 0, },
	[ 8] = { .rate = 1200000, .P = 3, .M = 150, .S = 0, },
	[ 9] = { .rate = 1100000, .P = 3, .M = 275, .S = 1, },
	[10] = { .rate = 1000000, .P = 3, .M = 250, .S = 1, },
	[11] = { .rate =  900000, .P = 3, .M = 225, .S = 1, },
	[12] = { .rate =  800000, .P = 3, .M = 200, .S = 1, },
	[13] = { .rate =  780000, .P = 3, .M = 195, .S = 1, },
	[14] = { .rate =  760000, .P = 3, .M = 190, .S = 1, },
	[15] = { .rate =  740000, .P = 3, .M = 185, .S = 1, },
	[16] = { .rate =  720000, .P = 3, .M = 180, .S = 1, },
	[17] = { .rate =  562000, .P = 4, .M = 187, .S = 1, },
	[18] = { .rate =  533000, .P = 4, .M = 355, .S = 2, },
	[19] = { .rate =  490000, .P = 3, .M = 245, .S = 2, },
	[20] = { .rate =  470000, .P = 3, .M = 235, .S = 2, },
	[21] = { .rate =  460000, .P = 3, .M = 230, .S = 2, },
	[22] = { .rate =  450000, .P = 3, .M = 225, .S = 2, },
	[23] = { .rate =  440000, .P = 3, .M = 220, .S = 2, },
	[24] = { .rate =  430000, .P = 3, .M = 215, .S = 2, },
	[25] = { .rate =  420000, .P = 3, .M = 210, .S = 2, },
	[26] = { .rate =  410000, .P = 3, .M = 205, .S = 2, },
	[27] = { .rate =  400000, .P = 3, .M = 200, .S = 2, },
	[28] = { .rate =  399000, .P = 4, .M = 266, .S = 2, },
	[29] = { .rate =  390000, .P = 3, .M = 195, .S = 2, },
	[30] = { .rate =  384000, .P = 3, .M = 192, .S = 2, },
	[31] = { .rate =  350000, .P = 3, .M = 175, .S = 2, },
	[32] = { .rate =  330000, .P = 3, .M = 165, .S = 2, },
	[33] = { .rate =  300000, .P = 3, .M = 150, .S = 2, },
	[34] = { .rate =  266000, .P = 3, .M = 266, .S = 3, },
	[35] = { .rate =  250000, .P = 3, .M = 250, .S = 3, },
	[36] = { .rate =  220000, .P = 3, .M = 220, .S = 3, },
	[37] = { .rate =  200000, .P = 3, .M = 200, .S = 3, },
	[38] = { .rate =  166000, .P = 3, .M = 166, .S = 3, },
	[39] = { .rate =  147500, .P = 3, .M = 147, .S = 3, },	// 147456
	[40] = { .rate =  133000, .P = 3, .M = 266, .S = 4, },
	[41] = { .rate =  125000, .P = 3, .M = 250, .S = 4, },
	[42] = { .rate =  100000, .P = 3, .M = 200, .S = 4, },
	[43] = { .rate =   96000, .P = 3, .M = 192, .S = 4, },
	[44] = { .rate =   48000, .P = 3, .M =  96, .S = 4, },
};

#define	PLL0_1_SIZE		ARRAY_SIZE(pll0_1_pms)
#define	PLL2_3_SIZE		ARRAY_SIZE(pll2_3_pms)

#define	PMS_RATE(p, i)	((&p[i])->rate)
#define	PMS_P(p, i)		((&p[i])->P)
#define	PMS_M(p, i)		((&p[i])->M)
#define	PMS_S(p, i)		((&p[i])->S)


#define PLL_S_BITPOS	0
#define PLL_M_BITPOS    8
#define PLL_P_BITPOS    18

static void core_pll_change(int PLL, int P, int M, int S)
{
	struct NX_CLKPWR_RegisterSet *clkpwr =
	(struct NX_CLKPWR_RegisterSet*)IO_ADDRESS(PHY_BASEADDR_CLKPWR_MODULE);

	// 1. change PLL0 clock to Oscillator Clock
	clkpwr->PLLSETREG[PLL] &= ~(1 << 28); 	// pll bypass on, xtal clock use
	clkpwr->CLKMODEREG0 = (1 << PLL); 		// update pll

	while(clkpwr->CLKMODEREG0 & (1<<31)); 		// wait for change update pll

	// 2. PLL Power Down & PMS value setting
	clkpwr->PLLSETREG[PLL] =((1UL << 29)			| // power down
							 (0UL << 28)    		| // clock bypass on, xtal clock use
							 (S   << PLL_S_BITPOS) 	|
							 (M   << PLL_M_BITPOS) 	|
							 (P   << PLL_P_BITPOS));

	clkpwr->CLKMODEREG0 = (1 << PLL); 				// update pll

	while(clkpwr->CLKMODEREG0 & (1<<31)); 			// wait for change update pll

	udelay(1);

	// 3. Update PLL & wait PLL locking
	clkpwr->PLLSETREG[PLL] &= ~((U32)(1UL<<29)); // pll power up

	clkpwr->CLKMODEREG0 = (1 << PLL); 			// update pll

	while(clkpwr->CLKMODEREG0 & (1<<31)); 		// wait for change update pll

	udelay(10);	// 1000us

	// 4. Change to PLL clock
	clkpwr->PLLSETREG[PLL] |= (1<<28); 			// pll bypass off, pll clock use
	clkpwr->CLKMODEREG0 = (1<<PLL); 				// update pll

	while(clkpwr->CLKMODEREG0 & (1<<31)); 				// wait for change update pll
}

unsigned long nxp_cpu_pll_change_frequency(int no, unsigned long rate)
{
	struct pll_pms *p;
	int len, i = 0, n = 0, l = 0;
	long freq = 0;

	rate /= 1000;
	DBGOUT("PLL.%d, %ld", no, rate);

	switch (no) {
	case 0 :
	case 1 : p = pll0_1_pms; len = PLL0_1_SIZE; break;
	case 2 :
	case 3 : p = pll2_3_pms; len = PLL2_3_SIZE; break;
	default: printk(KERN_ERR "Not support pll.%d (0~3)\n", no);
		return 0;
	}

	i = len/2;
	while (1) {
		l = n + i;
		freq = PMS_RATE(p, l);
		if (freq == rate)
			break;

		if (rate > freq)
			len -= i, i >>= 1;
		else
			n += i, i = (len-n-1)>>1;

		if (0 == i) {
			int k = l;
			if (abs(rate - freq) > abs(rate - PMS_RATE(p, k+1)))
				k += 1;
			if (abs(rate - PMS_RATE(p, k)) >= abs(rate - PMS_RATE(p, k-1)))
				k -= 1;
			l = k;
			break;
		}
	}

	/* change */
	core_pll_change(no, PMS_P(p, l), PMS_M(p, l), PMS_S(p, l));

	DBGOUT("(real %ld Khz, P=%d ,M=%3d, S=%d)\n",
		PMS_RATE(p, l), PMS_P(p, l), PMS_M(p, l), PMS_S(p, l));

	return PMS_RATE(p, l);
}
