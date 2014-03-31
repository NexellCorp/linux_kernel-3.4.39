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
#include <linux/init.h>
#include <linux/module.h>
#include <linux/suspend.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <asm/cacheflush.h>
#include <asm/suspend.h>
#include <asm/memory.h>
#include <asm/system.h>
#include <asm/sections.h> 	/*_stext, _end*/
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/hardware/pl080.h>
#include <mach/platform.h>
#include <mach/pm.h>

#define SRAM_SAVE_SIZE		(0x4000)

static unsigned int  sramsave[SRAM_SAVE_SIZE/4];
static unsigned int *sramptr;

#ifndef CONFIG_SUSPEND_IDLE
void (*nxp_board_suspend_mark)(struct suspend_mark_up *mark, int suspend) = NULL;
static void (*do_suspend)(ulong, ulong) = NULL;
#endif

struct save_gpio {
	unsigned long data;			/* 0x00 */
	unsigned long output;		/* 0x04 */
	unsigned long alfn[2];		/* 0x20, 0x24 */
	unsigned long mode[3];		/* 0x08, 0x0C, 0x28 */
	unsigned long mask;			/* 0x10, 0x3C */
	unsigned long reg_val[10]; /* 0x40 ~ 0x64 */
};

struct save_l2c {
	unsigned long tieoff;
	unsigned long tag_latency;
	unsigned long data_latency;
	unsigned long filter_start;
	unsigned long filter_end;
	unsigned long prefetch_ctrl;
	unsigned long pwr_ctrl;
	unsigned long aux_ctrl;
	unsigned long l2x0_way_mask;
};

struct save_alive {
	unsigned long  detmod[6];
	unsigned long  detenb;
	unsigned long  irqenb;
	unsigned long  output;
};

struct pm_saved_regs {
	struct save_l2c   l2c;
	struct save_gpio  gpio[5];	/* A,B,C,D,E */
	struct save_alive alive;
};

static struct pm_saved_regs saved_regs;
static struct board_suspend_ops *board_suspend = NULL;

#if (0)
#define	PM_SAVE_ADDR	(U32)virt_to_phys(&saved_regs)
#define	PM_SAVE_SIZE	SUSPEND_SAVE_SIZE
#else
#define	PM_SAVE_ADDR	(U32)__pa(_stext)
#define	PM_SAVE_SIZE	SUSPEND_SAVE_SIZE
#endif

#define	SUSPEND_STATUS(s)	(SUSPEND_SUSPEND == s ? "suspend" : "resume")

unsigned int __wake_event_bits = 0;	/* VDDTOGLE, RTC, ALIVE0, 1, ... */
static const char * __wake_event_name [] = {
	[0] = "VDDPWRTOGGLE",
	[1] = "RTC",
	[2] = "ALIVE 0",
	[3] = "ALIVE 1",
	[4] = "ALIVE 2",
	[5] = "ALIVE 3",
	[6] = "ALIVE 4",
	[7] = "ALIVE 5",
	[8] = "ALIVE 6",
	[9] = "ALIVE 7",
};
#define	WAKE_EVENT_NUM	ARRAY_SIZE(__wake_event_name)

#define	POWER_KEY_MASK		(0x3FC)
#define	RTC_ALARM_INTENB	(0x010)
#define	RTC_ALARM_INTPND	(0x014)

static int suspend_machine(void)
{
	const U32 pads[][2] = {
		{ CFG_PWR_WAKEUP_SRC_ALIVE0, CFG_PWR_WAKEUP_MOD_ALIVE0 },
		{ CFG_PWR_WAKEUP_SRC_ALIVE1, CFG_PWR_WAKEUP_MOD_ALIVE1 },
		{ CFG_PWR_WAKEUP_SRC_ALIVE2, CFG_PWR_WAKEUP_MOD_ALIVE2 },
		{ CFG_PWR_WAKEUP_SRC_ALIVE3, CFG_PWR_WAKEUP_MOD_ALIVE3 },
		{ CFG_PWR_WAKEUP_SRC_ALIVE4, CFG_PWR_WAKEUP_MOD_ALIVE4 },
		{ CFG_PWR_WAKEUP_SRC_ALIVE5, CFG_PWR_WAKEUP_MOD_ALIVE5 },
		{ CFG_PWR_WAKEUP_SRC_ALIVE6, CFG_PWR_WAKEUP_MOD_ALIVE6 },
		{ CFG_PWR_WAKEUP_SRC_ALIVE7, CFG_PWR_WAKEUP_MOD_ALIVE7 },
	};

	u32 rtc = IO_ADDRESS(PHY_BASEADDR_RTC);
	int ret = 0, i = 0, n = 0;
	CBOOL IRQ, MODE;

	lldebugout("%s\n", __func__);

	NX_ALIVE_SetWriteEnable(CTRUE);
	NX_ALIVE_ClearInterruptPendingAll();
	NX_ALIVE_ClearWakeUpStatus();

	/*
	 * set wakeup device
	 */
	for (i = 0; 8 > i; i++) {
		IRQ = pads[i][0] ? CTRUE : CFALSE;
		NX_ALIVE_SetDetectEnable(i, IRQ);
		NX_ALIVE_SetInterruptEnable(i, IRQ);
		NX_ALIVE_SetOutputValue(i, IRQ?CTRUE:CFALSE);
		for (n = 0; 6 > n; n++) {
			MODE = (CTRUE == pads[i][0] && n == pads[i][1]) ? CTRUE : CFALSE;
			NX_ALIVE_SetDetectMode(n, i, MODE);
		}

		/* both edge */
		if (CTRUE == pads[i][0] && PWR_DECT_BOTHEDGE == pads[i][1]) {
			NX_ALIVE_SetDetectMode(PWR_DECT_FALLINGEDGE, i, CTRUE);
			NX_ALIVE_SetDetectMode(PWR_DECT_RISINGEDGE , i, CTRUE);
		}
	}

	/* disable alarm wakeup */
#if !(PM_RTC_WAKE)
	writel(readl(rtc + RTC_ALARM_INTENB) & ~(3<<0),
		(rtc + RTC_ALARM_INTENB));
	writel(readl(rtc + RTC_ALARM_INTPND) & ~(3<<0),
		(rtc + RTC_ALARM_INTPND));
#endif

	/*
	 * wakeup from board.
	 */
    if (board_suspend && board_suspend->poweroff)
       ret = board_suspend->poweroff();

	if (ret)
		return ret;

	/*
	 * check wakeup event(alive, alarm)
	 * before enter sleep mode
	 */
	if (NX_ALIVE_GetInterruptPending32())
		return -EINVAL;

	if (readl(rtc + RTC_ALARM_INTPND) &
		readl(rtc + RTC_ALARM_INTENB) & (1<<1))
		return -EINVAL;

	return 0;
}

static int resume_machine(void)
{
	u32 status, alive = 0, alarm = 0;
	PM_DBGOUT("%s\n", __func__);

	NX_ALIVE_SetWriteEnable(CTRUE);

	status = NX_ALIVE_GetWakeUpStatus();
	status = 0;	/* TEST */

	/* recheck */
	if (!status) {
		u32 rtc = IO_ADDRESS(PHY_BASEADDR_RTC);
		alive = NX_ALIVE_GetInterruptPending32();
		alarm = readl(rtc + RTC_ALARM_INTPND) &
				readl(rtc + RTC_ALARM_INTENB) & (1<<1);
		status = (alive << 2) | (alarm ? (1<<1) : 0);
	}

	/* set wake event */
	__wake_event_bits = status & ((1<<WAKE_EVENT_NUM) - 1);

	/* reset machine */
    nxp_cpu_base_init();
    if (board_suspend && board_suspend->poweron)
        board_suspend->poweron();

	return 0;
}

static void print_wake_event(void)
{
	int i = 0;
	for (i = 0; WAKE_EVENT_NUM > i; i++) {
		if (__wake_event_bits & 1<<i)
			printk("%s WAKE [%s]\n", __func__, __wake_event_name[i]);
	}
}

static void suspend_cores(suspend_state_t stat)
{
	unsigned int core = 0, clamp = 0;
	unsigned int reset = 0;
	int cpu = 1, num = nr_cpu_ids;

	for (; num > cpu; cpu++) {
		switch (cpu) {
		case 1:	clamp  = TIEOFFINDEX_OF_CORTEXA9MP_TOP_QUADL2C_CLAMPCPU1;
				core  = TIEOFFINDEX_OF_CORTEXA9MP_TOP_QUADL2C_CPU1PWRDOWN;
				reset = RESETINDEX_OF_nx01301_CORTEXA9MP_TOP_QUADL2C_MODULE_nCPURESET1;
				break;
		case 2:	clamp  = TIEOFFINDEX_OF_CORTEXA9MP_TOP_QUADL2C_CLAMPCPU2;
				core  = TIEOFFINDEX_OF_CORTEXA9MP_TOP_QUADL2C_CPU2PWRDOWN;
				reset = RESETINDEX_OF_nx01301_CORTEXA9MP_TOP_QUADL2C_MODULE_nCPURESET2;
				break;
		case 3:	clamp  = TIEOFFINDEX_OF_CORTEXA9MP_TOP_QUADL2C_CLAMPCPU3;
				core  = TIEOFFINDEX_OF_CORTEXA9MP_TOP_QUADL2C_CPU3PWRDOWN;
				reset = RESETINDEX_OF_nx01301_CORTEXA9MP_TOP_QUADL2C_MODULE_nCPURESET3;
				break;
		}

		if (SUSPEND_SUSPEND == stat) {
			NX_RSTCON_SetBaseAddress(IO_ADDRESS(NX_RSTCON_GetPhysicalAddress()));
			NX_RSTCON_SetnRST(reset, RSTCON_nDISABLE);

			NX_TIEOFF_Set(clamp, 1);
			mdelay(1);
			NX_TIEOFF_Set(core, 1);
			PM_DBGOUT("Power off cpu.%d\n", cpu);
		}
#if (0)
		/* reset core */
		else {
			unsigned int clock = TIEOFFINDEX_OF_CORTEXA9MP_TOP_QUADL2C_CPUCLKOFF;
			clock += cpu;

			NX_TIEOFF_Set(core, 0);
			mdelay(1);
			NX_TIEOFF_Set(clamp, 0);
			NX_TIEOFF_Set(clock, 0);
			mdelay(1);

			NX_RSTCON_SetBaseAddress(IO_ADDRESS(NX_RSTCON_GetPhysicalAddress()));
			NX_RSTCON_SetnRST(reset, RSTCON_nDISABLE);
			mdelay(1);
			NX_TIEOFF_Set(clock, 1);
			NX_RSTCON_SetnRST(reset, RSTCON_nENABLE);
			mdelay(1);
			NX_TIEOFF_Set(clock, 0);
			PM_DBGOUT("Power on cpu.%d\n", cpu);
		}
#endif
	}
}

#define CHKSTRIDE	(8)
static inline unsigned int __calc_crc(void *addr, int len)
{
	u32 *c = (u32*)addr;
	u32 crc = 0, chkcnt = ((len+3)/4);
	int i, n;

	for (i = 0; chkcnt > i; i += CHKSTRIDE, c += CHKSTRIDE) {
		u32 dat = *c;
		crc ^= dat;
		for(n = 0; 32 > n; n++) {
			if(crc & 0x01) crc ^= (0x04C11DB7L);
			crc >>= 1;
		}
	}
	return crc;
}

#ifndef CONFIG_SUSPEND_IDLE
static void suspend_mark(suspend_state_t stat)
{
	struct suspend_mark_up mark = {
		.resume_fn = (U32)virt_to_phys(cpu_resume),
		.signature = SUSPEND_SIGNATURE,
		.save_phy_addr = PM_SAVE_ADDR,
		.save_phy_len = PM_SAVE_SIZE,
	};

	writel((-1UL), SCR_WAKE_FN_RESET);
	writel((-1UL), SCR_CRC_PHY_RESET);
	writel((-1UL), SCR_CRC_RET_RESET);
	writel((-1UL), SCR_CRC_LEN_RESET);
	writel((-1UL), SCR_SIGNAGURE_RESET);

	if (SUSPEND_SUSPEND == stat) {
		uint phy = mark.save_phy_addr;
		uint len = mark.save_phy_len;
		mark.save_crc_ret = __calc_crc(__va(phy), len);
	}

	if (nxp_board_suspend_mark) {
		nxp_board_suspend_mark(&mark, (SUSPEND_SUSPEND == stat ? 1: 0));
		return;
	}

	if (SUSPEND_SUSPEND == stat) {
		PM_DBGOUT("%s Suspend CRC [phy=0x%08x, calc=0x%08x, len=%d]\n",
			__func__, mark.save_phy_addr, mark.save_crc_ret, mark.save_phy_len);
		writel(mark.signature, SCR_SIGNAGURE_SET);
		writel(mark.resume_fn , SCR_WAKE_FN_SET);
		writel(mark.save_phy_addr, SCR_CRC_PHY_SET);
		writel(mark.save_crc_ret, SCR_CRC_RET_SET);
		writel(mark.save_phy_len, SCR_CRC_LEN_SET);
	}
}
#endif

#ifdef CONFIG_CACHE_L2X0
static inline void __l2x0_flush_all(void __iomem *base)
{
	struct save_l2c *pl2c = &saved_regs.l2c;
	writel(pl2c->l2x0_way_mask, base + L2X0_CLEAN_INV_WAY);

	/* wait for cache operation by line or way to complete */
	while (readl(base + L2X0_CLEAN_INV_WAY) & pl2c->l2x0_way_mask)
		cpu_relax();

	#ifdef CONFIG_PL310_ERRATA_753970
	writel(0, base + L2X0_DUMMY_REG);
	#else
	writel(0, base + L2X0_CACHE_SYNC);
	#endif
}

static inline void __l2x0_invalidate_all(void __iomem *base)
{
	struct save_l2c *pl2c = &saved_regs.l2c;

	/* Invalidating when L2 is enabled is a nono */
	BUG_ON(readl(base + L2X0_CTRL) & 1);
	writel(pl2c->l2x0_way_mask, base + L2X0_INV_WAY);

	/* wait for cache operation by line or way to complete */
	while (readl(base + L2X0_CLEAN_INV_WAY) & pl2c->l2x0_way_mask)
		cpu_relax();

	#ifdef CONFIG_PL310_ERRATA_753970
	writel(0, base + L2X0_DUMMY_REG);
	#else
	writel(0, base + L2X0_CACHE_SYNC);
	#endif
}
#endif

static void suspend_l2cache(suspend_state_t stat)
{
#ifdef CONFIG_CACHE_L2X0
	struct save_l2c *pl2c = &saved_regs.l2c;
	void __iomem *base = (void __iomem *)__PB_IO_MAP_L2C_VIRT;

	if (SUSPEND_SUSPEND == stat) {
		pl2c->tag_latency = readl_relaxed(base + L2X0_TAG_LATENCY_CTRL);
    	pl2c->data_latency = readl_relaxed(base + L2X0_DATA_LATENCY_CTRL);
    	pl2c->prefetch_ctrl = readl_relaxed(base + L2X0_PREFETCH_CTRL);
    	pl2c->filter_start = readl_relaxed(base + L2X0_ADDR_FILTER_START);
    	pl2c->filter_end = readl_relaxed(base + L2X0_ADDR_FILTER_END);
		pl2c->pwr_ctrl = readl_relaxed(base + L2X0_POWER_CTRL);
		pl2c->aux_ctrl = readl_relaxed(base + L2X0_AUX_CTRL);
		pl2c->tieoff = readl_relaxed(IO_ADDRESS(PHY_BASEADDR_TIEOFF));
		pl2c->l2x0_way_mask = pl2c->aux_ctrl & (1 << 16) ? (1 << 16) - 1 : (1 << 8) - 1 ;
	} else {
		int i = 0, lockregs = 8;

		if ((readl_relaxed(base + L2X0_CTRL) & 1))
			return;

		/* TIEOFF */
		writel_relaxed(pl2c->tieoff|0x3000, IO_ADDRESS(PHY_BASEADDR_TIEOFF));

		/* restore */
		writel_relaxed(pl2c->tag_latency, (base + L2X0_TAG_LATENCY_CTRL));
    	writel_relaxed(pl2c->data_latency, (base + L2X0_DATA_LATENCY_CTRL));
    	writel_relaxed(pl2c->filter_start, (base + L2X0_ADDR_FILTER_START));
    	writel_relaxed(pl2c->filter_end, (base + L2X0_ADDR_FILTER_END));
		writel_relaxed(pl2c->prefetch_ctrl, (base + L2X0_PREFETCH_CTRL));
		writel_relaxed(pl2c->pwr_ctrl, (base + L2X0_POWER_CTRL));

		/*
		 * l2x0_unlock, aux, enable
		 */
		for (i = 0; i < lockregs; i++) {
			writel_relaxed(0x0, base + L2X0_LOCKDOWN_WAY_D_BASE + i*L2X0_LOCKDOWN_STRIDE);
			writel_relaxed(0x0, base + L2X0_LOCKDOWN_WAY_I_BASE + i*L2X0_LOCKDOWN_STRIDE);
		}
		writel_relaxed(pl2c->aux_ctrl, (base + L2X0_AUX_CTRL));
		__l2x0_invalidate_all(base);
		writel_relaxed(1, (base + L2X0_CTRL));
	}
#endif
}

static void suspend_gpio(suspend_state_t stat)
{
	struct save_gpio *gpio = saved_regs.gpio;
	unsigned int base = IO_ADDRESS(PHY_BASEADDR_GPIOA);
	int i = 0, j = 0, size = 5;

	if (SUSPEND_SUSPEND == stat) {
		for (i = 0; size > i; i++, gpio++, base += 0x1000) {
			gpio->data    = readl(base+0x00);
			gpio->output  = readl(base+0x04);
			gpio->alfn[0] = readl(base+0x20);
			gpio->alfn[1] = readl(base+0x24);
			gpio->mode[0] = readl(base+0x08);
			gpio->mode[1] = readl(base+0x0C);
			gpio->mode[2] = readl(base+0x28);
			gpio->mask = readl(base+0x10);

			for (j = 0; j < 10; j++)
				gpio->reg_val[j] = readl(base+0x40+(j<<2));


			writel((-1UL), (base+0x14));	/* clear pend */
		}
	} else {
		for (i = 0; size > i; i++, gpio++, base += 0x1000) {

			for (j = 0; j < 10; j++)
				writel(gpio->reg_val[j], (base+0x40+(j<<2)));

			writel(gpio->output, (base+0x04));
			writel(gpio->data  , (base+0x00));
			writel(gpio->alfn[0],(base+0x20));
			writel(gpio->alfn[1],(base+0x24));
			writel(gpio->mode[0],(base+0x08));
			writel(gpio->mode[1],(base+0x0C)),
			writel(gpio->mode[2],(base+0x28));
			writel(gpio->mask, (base+0x10));
			writel(gpio->mask, (base+0x3C));
			writel((-1UL), (base+0x14));	/* clear pend */
		}
	}
}

static void suspend_alive(suspend_state_t stat)
{
	struct save_alive *alive = &saved_regs.alive;
	unsigned int base = IO_ADDRESS(PHY_BASEADDR_ALIVE);
	int i = 0;

	NX_ALIVE_SetWriteEnable(CTRUE);
	if (SUSPEND_SUSPEND == stat) {
		for (i = 0; 6 > i; i++)
			alive->detmod[i] = readl(base + (i*0x0C) + 0x0C);

		alive->detenb = readl(base + 0x54);
		alive->irqenb = readl(base + 0x60);
		alive->output = readl(base + 0x7C);
	} else {
		for (i = 0; 6 > i; i++) {
			writel((-1UL), base + (i*0x0C) + 0x04);			/* reset */
			writel(alive->detmod[i], base + (i*0x0C) + 0x08);	/* set */
		}
		writel((-1UL), base + 0x4C);		/* reset */
		writel((-1UL), base + 0x58);		/* reset */
		writel((-1UL), base + 0x74);		/* reset */
		writel(alive->detenb, base + 0x50);	/* set */
		writel(alive->irqenb, base + 0x5C);	/* set */
		writel(alive->output, base + 0x78);	/* set */
	}
}

static void suspend_clock(suspend_state_t stat)
{
	if (SUSPEND_RESUME == stat)
		nxp_cpu_clock_resume();
}

static int __powerdown(unsigned long arg)
{
	int ret = suspend_machine();
	int i;
	if (0 == ret) {
		for(i=0; ARRAY_SIZE(sramsave) > i; i++)
			sramptr[i] = sramsave[i];
	}

#ifdef CONFIG_SUSPEND_IDLE
	lldebugout("Go to IDLE...\n");
#endif

	flush_cache_all();
	outer_flush_all();

	if (ret)
		return ret;	/* wake up */

#ifdef CONFIG_SUSPEND_IDLE
	cpu_do_idle();
#else
	if(do_suspend == NULL) {
		lldebugout("Fail, inavalid suspend callee\n");
		return 0;
	}

	do_suspend = (void (*)(ulong, ulong))((ulong)do_suspend + 0x220);
	do_suspend(IO_ADDRESS(PHY_BASEADDR_ALIVE), IO_ADDRESS(PHY_BASEADDR_DREX));

    while (1) { ; }
#endif
	return 0;
}

/*
 * Suspend ops functions
 */

/* return : 0 = wake up, 1 = goto suspend */
static int suspend_valid(suspend_state_t state)
{
	int ret = 1;
	/* clear */
	__wake_event_bits = 0;

#ifdef CONFIG_SUSPEND
    if (!suspend_valid_only_mem(state)) {
        printk(KERN_ERR "%s: not supported state(%d)\n", __func__, state);
        return 0;
    }
#endif
    if (board_suspend && board_suspend->valid)
        ret = board_suspend->valid(state);

  	PM_DBGOUT("%s %s\n", __func__, ret ? "DONE":"WAKE");
    return ret;
}

/* return : 0 = goto suspend, 1 = wake up */
static int suspend_begin(suspend_state_t state)
{
	int ret = 0;
    if (board_suspend && board_suspend->begin)
        ret = board_suspend->begin(state);

  	PM_DBGOUT("%s %s\n", __func__, ret ? "WAKE":"DONE");
    return 0;
}

/* return : 0 = goto suspend, 1 = wake up */
static int suspend_prepare(void)
{
	int ret = 0;
    if (board_suspend && board_suspend->prepare)
        ret = board_suspend->prepare();

  	PM_DBGOUT("%s %s\n", __func__, ret ? "WAKE":"DONE");
    return ret;
}

/* return : 0 = goto suspend, 1 = wake up */
static int suspend_enter(suspend_state_t state)
{
    int ret = 0;
  	lldebugout("%s enter\n", __func__);

    if (board_suspend && board_suspend->enter) {
		if ((ret = board_suspend->enter(state)))
            return ret;
	}

	suspend_clock(SUSPEND_SUSPEND);
	suspend_gpio(SUSPEND_SUSPEND);
	suspend_alive(SUSPEND_SUSPEND);
	suspend_l2cache(SUSPEND_SUSPEND);
#ifndef CONFIG_SUSPEND_IDLE
	suspend_mark(SUSPEND_SUSPEND);
#endif

	/* SMP power down */
	suspend_cores(SUSPEND_SUSPEND);

    /*
     * goto sleep
     */
    cpu_suspend(0, __powerdown);

	/*
	 * Wakeup status
	 */
#ifndef CONFIG_SUSPEND_IDLE
    suspend_mark(SUSPEND_RESUME);
#endif
	suspend_l2cache(SUSPEND_RESUME);

	resume_machine();

	suspend_alive(SUSPEND_RESUME);
	suspend_gpio(SUSPEND_RESUME);
	suspend_clock(SUSPEND_RESUME);

	suspend_cores(SUSPEND_RESUME);	/* last */

	/* print wakeup evnet */
	print_wake_event();

    return 0;
}

static void suspend_finish(void)
{
	PM_DBGOUT("%s\n", __func__);
    if (board_suspend && board_suspend->finish)
        board_suspend->finish();
}

static void suspend_end(void)
{
	PM_DBGOUT("%s\n", __func__);
    if (board_suspend && board_suspend->end)
        board_suspend->end();
}

static struct platform_suspend_ops suspend_ops = {
    .valid      = suspend_valid,     /* first suspend call */
    .begin      = suspend_begin,     /* before driver suspend */
    .prepare    = suspend_prepare,   /* after driver suspend */
    .enter      = suspend_enter,     /* goto suspend */
    .finish     = suspend_finish,    /* before driver resume */
    .end        = suspend_end,       /* after driver resume */
};

static int __init suspend_ops_init(void)
{
	int i = 0;
	sramptr = (unsigned int*)ioremap(0xFFFF0000, SRAM_SAVE_SIZE);

	pr_debug("%s sram save\r\n", __func__);
	for(i=0; ARRAY_SIZE(sramsave) > i; i++)
		sramsave[i] = sramptr[i];

	suspend_set_ops(&suspend_ops);

#ifndef CONFIG_SUSPEND_IDLE
	do_suspend = __arm_ioremap_exec(0xffff0000, 0x10000, 0);
	if (!do_suspend)
		printk("Fail, ioremap for suspend callee\n");
#endif
	return 0;
}
core_initcall(suspend_ops_init);

/*
 * 	cpu board suspend fn
 */
void nxp_board_suspend_register(struct board_suspend_ops *ops)
{
    board_suspend = ops;
}

/*
 * 	cpu wakeup source
 */
int nxp_check_pm_wakeup_alive(int num)
{
	int grp = PAD_GET_GROUP(num);
	int io  = PAD_GET_BITNO(num);

	if (PAD_GET_GROUP(PAD_GPIO_ALV) != grp)
		return 0;

	return (__wake_event_bits & 1<<(io+2)) ? 1 : 0;
}
EXPORT_SYMBOL(nxp_check_pm_wakeup_alive);

static int pm_check_wakeup_dev(char *dev, int io)
{
	printk("Check PM wakeup : %s, io[%d]\n", dev, io);
	return nxp_check_pm_wakeup_alive(io);
}

int (*nxp_check_pm_wakeup_dev)(char *dev, int io) = pm_check_wakeup_dev;
EXPORT_SYMBOL(nxp_check_pm_wakeup_dev);

