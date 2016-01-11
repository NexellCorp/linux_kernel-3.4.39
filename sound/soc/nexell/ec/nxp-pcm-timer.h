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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <asm/io.h>

#include <mach/platform.h>

#ifndef __NXP_PCM_TIMER_H__
#define __NXP_PCM_TIMER_H__

#define	LC_TIMER_CHANNEL	3
#define	LC_TIMER_TCOUNT		100000000					// PCLK 200/2

/***************************************************************************************/
struct pcm_timer_data {
	int channel;
	int irq;
	int mux;
	int prescale;
	unsigned long tcount;
	unsigned long nsec;
	struct timespec ts;
	spinlock_t lock;
	void *data;
};
static struct pcm_timer_data pcm_timer;

/***************************************************************************************/
/* Timer HW */
/***************************************************************************************/
#define	__TIMER_BASE	IO_ADDRESS(PHY_BASEADDR_TIMER)
#define	TIMER_CFG0		(__TIMER_BASE + 0x00)
#define	TIMER_CFG1		(__TIMER_BASE + 0x04)
#define	TIMER_TCON		(__TIMER_BASE + 0x08)
#define	TIMER_STAT		(__TIMER_BASE + 0x44)
#define	TIMER_CNTB(ch)	((__TIMER_BASE + 0x0C) + (0xc*ch))
#define	TIMER_CMPB(ch)	((__TIMER_BASE + 0x10) + (0xc*ch))
#define	TIMER_CNTO(ch)	((__TIMER_BASE + 0x14) + (0xc*ch))

#define	TCON_AUTO		(1<<3)
#define	TCON_INVT		(1<<2)
#define	TCON_UP			(1<<1)
#define	TCON_RUN		(1<<0)
#define TCFG0(c)		(c == 0 || c == 1 ? 0 : 8)
#define TCFG1(c)		(c * 4)
#define TCON(c)		(c ? c * 4  + 4 : 0)
#define TINTON(c)		(c)
#define TINTST(c)		(c + 5)
#define	TINTMASK		(0x1F)

#define	T_MUX_1_1		0
#define	T_MUX_1_2		1
#define	T_MUX_1_4		2
#define	T_MUX_1_8		3
#define	T_MUX_1_16		4
#define	T_MUX_TCLK		5

#define	T_IRQ_ON		(1<<0)

static inline void timer_clock(int ch, int mux, int scl)
{
	__raw_writel((__raw_readl(TIMER_CFG0) & ~(0xFF << TCFG0(ch))) |
			((scl-1)<< TCFG0(ch)), TIMER_CFG0);
	__raw_writel((__raw_readl(TIMER_CFG1) & ~(0xF << TCFG1(ch))) |
			(mux << TCFG1(ch)), TIMER_CFG1);
}

static inline void timer_count(int ch, unsigned int cnt)
{
	__raw_writel(cnt-1, TIMER_CNTB(ch)), __raw_writel(cnt-1, TIMER_CMPB(ch));
}

static inline void timer_start(int ch, unsigned int flag)
{
	u32 val = (__raw_readl(TIMER_STAT) & ~(TINTMASK<<5 | 0x1 << TINTON(ch))) |
		   (0x1 << TINTST(ch) | ((flag|T_IRQ_ON)?1:0) << TINTON(ch));
	__raw_writel(val, TIMER_STAT);

	val = (__raw_readl(TIMER_TCON) & ~(0xE << TCON(ch))) | (TCON_UP << TCON(ch));
	__raw_writel(val, TIMER_TCON);

	val = (val & ~(TCON_UP << TCON(ch))) | ((TCON_AUTO | TCON_RUN) << TCON(ch));
	__raw_writel(val, TIMER_TCON);
}

static inline void timer_stop(int ch, unsigned int flag)
{
	u32 val = (__raw_readl(TIMER_STAT) & ~(TINTMASK<<5 | 0x1 << TINTON(ch))) |
		   (0x1 << TINTST(ch) | ((flag|T_IRQ_ON)?1:0) << TINTON(ch));
	__raw_writel(val, TIMER_STAT);

	val = __raw_readl(TIMER_TCON) & ~(TCON_RUN << TCON(ch));
	__raw_writel(val, TIMER_TCON);
}

static inline void timer_clear(int ch)
{
	u32 val = (__raw_readl(TIMER_STAT) & ~(TINTMASK<<5)) | (0x1 << TINTST(ch));
	__raw_writel(val, TIMER_STAT);
}
#endif