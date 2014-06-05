#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/export.h>
#include <linux/atomic.h>
#include <linux/completion.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#include <mach/platform.h>
#include <mach/soc.h>
#include <mach/nxp-dfs-bclk.h>

#define BCLK_MIN    120000000
/*#define BCLK_MIN    100000000*/
#define BCLK_MEDIUM 200000000
#define BCLK_MAX    400000000

static int default_dfs_bclk_func(uint32_t pll_num, uint32_t counter, uint32_t user_bitmap, uint32_t current_bclk);

static struct dfs_bclk_manager {
    uint32_t bclk_pll_num;
    atomic_t counter;
    atomic_t user_bitmap;
    uint32_t current_bclk;

    dfs_bclk_func func;
} dfs_bclk_manager = {
#ifdef CONFIG_NXP4330_DFS_BCLK_PLL_0
    .bclk_pll_num = 0,
#else
    .bclk_pll_num = 1,
#endif
    .counter = ATOMIC_INIT(0),
    .user_bitmap = ATOMIC_INIT(0),
    .current_bclk = BCLK_MAX,
    .func = default_dfs_bclk_func
};

/**
 * sysfs attributes
 */
static uint32_t bclk_min = BCLK_MIN;
/*static uint32_t bclk_medium = BCLK_MEDIUM;*/
static uint32_t bclk_max = BCLK_MAX;
static uint32_t enable = true;

#define ATOMIC_SET_MASK(PTR, MASK)  \
    do { \
        unsigned long oldval = atomic_read(PTR); \
        unsigned long newval = oldval | MASK; \
        atomic_cmpxchg(PTR, oldval, newval); \
    } while (0)

#define ATOMIC_CLEAR_MASK(PTR, MASK) \
        atomic_clear_mask(MASK, (unsigned long *)&((PTR)->counter))

struct vsync_callback_ctx {
    struct completion completion;
    uint32_t pll_num;
    uint32_t bclk;
};

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

#include <linux/clocksource.h>

extern CBOOL    NX_MLC_GetTopDirtyFlag( U32 ModuleIndex  );
/*extern cycle_t timer_source_read(struct clocksource *cs);*/

static void core_pll_change(int PLL, int P, int M, int S)
{
    // for debugging
    /*bool dirty1, dirty2, dirty3, dirty4, dirty5;*/
    /*uint32_t ctrl1, ctrl2, ctrl3, ctrl4, ctrl5;*/
    /*u64 jiffies1, jiffies2, jiffies3, jiffies4, jiffies5;*/

	struct NX_CLKPWR_RegisterSet *clkpwr =
	(struct NX_CLKPWR_RegisterSet*)IO_ADDRESS(PHY_BASEADDR_CLKPWR_MODULE);

    /*dirty1 = NX_MLC_GetTopDirtyFlag(0);*/
    /*ctrl1 = NX_MLC_GetControlReg(0);*/
    /*jiffies1 = timer_source_read(NULL);*/

	// 1. change PLL0 clock to Oscillator Clock
	clkpwr->PLLSETREG[PLL] &= ~(1 << 28); 	// pll bypass on, xtal clock use
	clkpwr->CLKMODEREG0 = (1 << PLL); 		// update pll

	while(clkpwr->CLKMODEREG0 & (1<<31)); 		// wait for change update pll
    /*dirty2 = NX_MLC_GetTopDirtyFlag(0);*/
    /*ctrl2 = NX_MLC_GetControlReg(0);*/
    /*jiffies2 = timer_source_read(NULL);*/

	// 2. PLL Power Down & PMS value setting
	clkpwr->PLLSETREG[PLL] =((1UL << 29)			| // power down
							 (0UL << 28)    		| // clock bypass on, xtal clock use
							 (S   << PLL_S_BITPOS) 	|
							 (M   << PLL_M_BITPOS) 	|
							 (P   << PLL_P_BITPOS));

	clkpwr->CLKMODEREG0 = (1 << PLL); 				// update pll

	while(clkpwr->CLKMODEREG0 & (1<<31)); 			// wait for change update pll
    /*dirty3 = NX_MLC_GetTopDirtyFlag(0);*/
    /*ctrl3 = NX_MLC_GetControlReg(0);*/
    /*jiffies3 = timer_source_read(NULL);*/

	/*udelay(1);*/

	// 3. Update PLL & wait PLL locking
	clkpwr->PLLSETREG[PLL] &= ~((U32)(1UL<<29)); // pll power up

	clkpwr->CLKMODEREG0 = (1 << PLL); 			// update pll

	while(clkpwr->CLKMODEREG0 & (1<<31)); 		// wait for change update pll
    /*dirty4 = NX_MLC_GetTopDirtyFlag(0);*/
    /*ctrl4 = NX_MLC_GetControlReg(0);*/
    /*jiffies4 = timer_source_read(NULL);*/

	/*udelay(10);	// 1000us*/
    /*dirty5 = NX_MLC_GetTopDirtyFlag(0);*/
    /*ctrl5 = NX_MLC_GetControlReg(0);*/

	// 4. Change to PLL clock
	clkpwr->PLLSETREG[PLL] |= (1<<28); 			// pll bypass off, pll clock use
	clkpwr->CLKMODEREG0 = (1<<PLL); 				// update pll

	while(clkpwr->CLKMODEREG0 & (1<<31)); 				// wait for change update pll
    /*dirty5 = NX_MLC_GetTopDirtyFlag(0);*/
    /*ctrl5 = NX_MLC_GetControlReg(0);*/
    /*jiffies5 = timer_source_read(NULL);*/

    /*printk("%s: dirty %d,%d,%d,%d,%d\n", __func__, dirty1, dirty2, dirty3, dirty4, dirty5);*/
    /*printk("%s: ctrl %08x,%08x,%08x,%08x,%08x\n", __func__, ctrl1, ctrl2, ctrl3, ctrl4, ctrl5);*/
    /*printk("%s: jiffies %llu,%llu,%llu,%llu,%llu\n", __func__, jiffies1, jiffies2, jiffies3, jiffies4, jiffies5);*/
}

static struct pll_pms *s_p;
static int s_l;

static unsigned long _cpu_pll_change_frequency(int no, unsigned long rate)
{
	struct pll_pms *p;
	int len, i = 0, n = 0, l = 0;
	long freq = 0;
    /*bool dirty1, dirty2;*/

    /*dirty1 = NX_MLC_GetTopDirtyFlag(0);*/
	rate /= 1000;
	printk("PLL.%d, %ld\n", no, rate);

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
    /*dirty2 = NX_MLC_GetTopDirtyFlag(0);*/

	/* change */
#if 0
	core_pll_change(no, PMS_P(p, l), PMS_M(p, l), PMS_S(p, l));

    printk("%s: %d, %d\n", __func__, dirty1, dirty2);
	DBGOUT("(real %ld Khz, P=%d ,M=%3d, S=%d)\n",
		PMS_RATE(p, l), PMS_P(p, l), PMS_M(p, l), PMS_S(p, l));

	return PMS_RATE(p, l);
#else
    s_p = p;
    s_l = l;
    return 0;
#endif
}
extern void	NX_MLC_SetTopDirtyFlag( U32 ModuleIndex );
extern CBOOL	NX_MLC_GetTopDirtyFlag( U32 ModuleIndex );

static void _pre_work(void *data)
{
    struct vsync_callback_ctx *ctx = data;
    NX_MLC_SetTopDirtyFlag(0);
    /*printk("DirtyFlag: %d\n", NX_MLC_GetTopDirtyFlag(0));*/
    complete(&ctx->completion);
}

static void _vsync_irq_callback(void *data)
{
    struct vsync_callback_ctx *ctx = data;
    uint32_t dirty = NX_MLC_GetTopDirtyFlag(0);
    if (dirty)
        nxp_cpu_pll_change_frequency(ctx->pll_num, ctx->bclk);
    complete(&ctx->completion);
}

// for debugging
extern uint32_t NX_MLC_GetControlReg( U32 ModuleIndex);
static inline void _set_and_wait(uint32_t pll_num, uint32_t bclk)
{
    struct vsync_callback_ctx ctx;
    printk("%s: pll %d, bclk %d\n", __func__, pll_num, bclk);

    init_completion(&ctx.completion);
    nxp_soc_disp_register_irq_callback(0, _pre_work, &ctx);
    wait_for_completion(&ctx.completion);
    nxp_soc_disp_unregister_irq_callback(0);

    init_completion(&ctx.completion);
    ctx.pll_num = pll_num;
    ctx.bclk = bclk;
    nxp_soc_disp_register_irq_callback(0, _vsync_irq_callback, &ctx);
    wait_for_completion(&ctx.completion);
    nxp_soc_disp_unregister_irq_callback(0);
    printk("MLC ControlReg 0x%x\n", NX_MLC_GetControlReg(0));
}

extern CBOOL	NX_DPC_GetInterruptPendingAll( U32 ModuleIndex );
extern void	NX_DPC_ClearInterruptPendingAll( U32 ModuleIndex );
extern void	NX_MLC_SetMLCEnable( U32 ModuleIndex, CBOOL bEnb );
extern void	NX_DPC_SetInterruptEnableAll( U32 ModuleIndex, CBOOL Enable );
static u64 tint[5];
static volatile struct NX_CLKPWR_RegisterSet *clkpwr;

// u32 pll_data : (p << 24) | (m << 16) | (s << 8) | pll_num
#if 1
        /*u32 pll_data = (P << 24) | (M << 8) | (S << 2) | pll_num;*/
void _real_change_pll(volatile u32 *clkpwr_reg, u32 *sram_base, u32 pll_data)
{
    // here : block other ip bus access
    uint32_t pll_num = pll_data & 0x00000003;
    uint32_t s       = (pll_data & 0x000000fc) >> 2;
    uint32_t m       = (pll_data & 0x00ffff00) >> 8;
    uint32_t p       = (pll_data & 0xff000000) >> 24;
    volatile u32 *pllset_reg = (clkpwr_reg + 2 + pll_num);
    /*printk("clkpwr reg %p, pllset reg %p, plldata 0x%x, p %d, m %d, s %d\n", clkpwr_reg, pllset_reg, pll_data, p, m, s);*/

    *pllset_reg &= ~(1 << 28);
    *clkpwr_reg  = (1 << pll_num);
    while(*clkpwr_reg & (1<<31));

    *pllset_reg  = ((1UL << 29) |
                    (0UL << 28) |
                    (s   << 0)  |
                    (m   << 8)  |
                    (p   << 18));
    *clkpwr_reg  = (1 << pll_num);
    while(*clkpwr_reg & (1<<31));

    *pllset_reg &= ~((u32)(1UL<<29));
    *clkpwr_reg  = (1 << pll_num);
    while(*clkpwr_reg & (1<<31));

    *pllset_reg |= (1 << 28);
    *clkpwr_reg  = (1 << pll_num);
    while(*clkpwr_reg & (1<<31));
    // here : unblock other ip bus access
}
#endif

static inline void _disable_irq_and_set(uint32_t pll_num, uint32_t bclk)
{
    bool pending = false;
    /*u64 t1, t2, t3, t4, t5;*/
    /*u32 line;*/
    /*uint32_t dirty1, dirty2;*/
    local_irq_disable();
    _cpu_pll_change_frequency(pll_num, bclk);
    /*printk("===>\n");*/
    NX_DPC_SetInterruptEnableAll(0, false);

#if 0
    /*printk("MLC ControlReg 0x%x\n", NX_MLC_GetControlReg(0));*/
    /*disable_irq(287);*/
    do {
        pending = NX_DPC_GetInterruptPendingAll(0);
    } while (!pending);
    NX_DPC_ClearInterruptPendingAll(0);
    {
        int i;
        for (i = 0; i < 50; i++)
            udelay(100);
    }
#endif

    /*NX_MLC_SetMLCEnable(0, false);*/
    /*NX_MLC_SetMLCEnable(0, true);*/
    /*NX_MLC_SetTopDirtyFlag(0);*/
    do {
        /*NX_MLC_SetTopDirtyFlag(0);*/
        pending = NX_DPC_GetInterruptPendingAll(0);
        /*tint[0] = tint[1];*/
        /*tint[1] = tint[2];*/
        /*tint[2] = timer_source_read(NULL);*/
    } while (!pending);
    /*tint[3] = timer_source_read(NULL);*/
    /*tint[4] = 0;*/

    /*while(NX_MLC_GetTopDirtyFlag(0) == 1);*/
    /*tint[4] = timer_source_read(NULL);*/
    /*dirty1 = NX_MLC_GetTopDirtyFlag(0);*/
#if 0
    {
        int i;
        for (i = 0; i < 200; i++)
            udelay(100);
    }
#else
    /*nxp_cpu_pll_change_frequency(pll_num, bclk);*/
    /*t1 = timer_source_read(NULL);*/
	/*core_pll_change(pll_num, PMS_P(s_p, s_l), PMS_M(s_p, s_l), PMS_S(s_p, s_l));*/
#if 0
    {
        uint32_t PLL = pll_num;
        uint32_t P = PMS_P(s_p, s_l);
        uint32_t M = PMS_M(s_p, s_l);
        uint32_t S = PMS_S(s_p, s_l);
        volatile uint32_t *modreg = (volatile uint32_t *)clkpwr;
        /*uint32_t counter1 = 0, counter2 = 0;*/
        uint32_t tmp;

        // 1. change PLL0 clock to Oscillator Clock
        clkpwr->PLLSETREG[PLL] &= ~(1 << 28); 	// pll bypass on, xtal clock use
        clkpwr->CLKMODEREG0 = (1 << PLL); 		// update pll
        tmp = clkpwr->CLKMODEREG0;
        /*while(*modreg & (1<<31));*/
        /*t2 = timer_source_read(NULL);*/

        // 2. PLL Power Down & PMS value setting
        clkpwr->PLLSETREG[PLL] =((1UL << 29)			| // power down
                (0UL << 28)    		| // clock bypass on, xtal clock use
                (S   << PLL_S_BITPOS) 	|
                (M   << PLL_M_BITPOS) 	|
                (P   << PLL_P_BITPOS));
        clkpwr->CLKMODEREG0 = (1 << PLL); 				// update pll
        tmp = clkpwr->CLKMODEREG0;
        /*while(*modreg & (1<<31));*/
        /*t3 = timer_source_read(NULL);*/

        // 3. Update PLL & wait PLL locking
        clkpwr->PLLSETREG[PLL] &= ~((U32)(1UL<<29)); // pll power up
        clkpwr->CLKMODEREG0 = (1<<PLL); 				// update pll
        while(*modreg & (1<<31)); 		// wait for change update pll
        /*t4 = timer_source_read(NULL);*/
        /*[>tmp = clkpwr->CLKMODEREG0;<]*/
        /*udelay(50);*/

        // 4. Change to PLL clock
        /*udelay(30);*/
        clkpwr->PLLSETREG[PLL] |= (1 << 28); 			// pll bypass off, pll clock use
        clkpwr->CLKMODEREG0 = (1<<PLL); 				// update pll
        /*tmp = clkpwr->CLKMODEREG0;*/
        /*while(clkpwr->CLKMODEREG0 & (1<<31)); 		// wait for change update pll*/
        while(*modreg & (1<<31)); 		// wait for change update pll

        /*printk("counter: %d, %d\n", counter1, counter2);*/
    }
#else
    {
        extern void (*do_suspend)(ulong, ulong);
        void (*real_change_pll)(u32*, u32*, u32*, u32) = (void (*)(u32 *, u32 *, u32 *, u32))((ulong)do_suspend + 0x224);
        uint32_t P = PMS_P(s_p, s_l);
        uint32_t M = PMS_M(s_p, s_l);
        uint32_t S = PMS_S(s_p, s_l);
        u32 pll_data = (P << 24) | (M << 8) | (S << 2) | pll_num;
        /*printk("before call: p %d, m %d, s %d\n", P, M, S);*/
        real_change_pll((u32 *)clkpwr, (u32 *)do_suspend, (u32 *)(IO_ADDRESS(PHY_BASEADDR_DREX)), pll_data);
        /*_real_change_pll((u32 *)clkpwr, NULL, pll_data);*/
    }
#endif
    /*t5 = timer_source_read(NULL);*/
#endif
    /*dirty2 = NX_MLC_GetTopDirtyFlag(0);*/
    NX_DPC_ClearInterruptPendingAll(0);
    local_irq_enable();
    NX_DPC_SetInterruptEnableAll(0, true);
    /*enable_irq(287);*/
    /*printk("dirty: %d, %d\n", dirty1, dirty2);*/
    /*printk("MLC ControlReg %d -> 0x%x\n", __LINE__, NX_MLC_GetControlReg(0));*/
    /*printk("dirty clear line: %d\n", line);*/
    /*printk("t : %llu, %llu, %llu, %llu, %llu\n", tint[0], tint[1], tint[2], tint[3], tint[4]);*/
    /*printk("===> %llu ns, %llu ns, %llu ns, %llu ns\n",*/
            /*100*(t2-t1), 100*(t3-t2), 100*(t4-t3), 100*(t5-t4));*/
}

static int default_dfs_bclk_func(uint32_t pll_num, uint32_t counter, uint32_t user_bitmap, uint32_t current_bclk)
{
    uint32_t bclk = current_bclk;
    if (counter > 0) {
        if (user_bitmap & ((1 << BCLK_USER_MPEG) | (1 << BCLK_USER_OGL))) {
            if (bclk != bclk_max) {
                bclk = bclk_max;
            }
        } else if (user_bitmap & (1 << BCLK_USER_DMA)) {
            bclk = BCLK_MEDIUM;
        }
    } else {
        bclk = bclk_min;
    }

    if (bclk != current_bclk)
        _disable_irq_and_set(pll_num, bclk);
    return bclk;
}

extern void nxp_pm_data_save(void *mem);
extern void nxp_pm_data_restore(void *mem);

int bclk_get(uint32_t user)
{
    if (enable) {
        printk("%s: %d, %d\n", __func__, user, atomic_read(&dfs_bclk_manager.counter));
        atomic_inc(&dfs_bclk_manager.counter);
        ATOMIC_SET_MASK(&dfs_bclk_manager.user_bitmap, 1<<user);
        if (user == BCLK_USER_MPEG)
            nxp_pm_data_save(NULL);
        dfs_bclk_manager.current_bclk =
            dfs_bclk_manager.func(
                    dfs_bclk_manager.bclk_pll_num,
                    atomic_read(&dfs_bclk_manager.counter),
                    atomic_read(&dfs_bclk_manager.user_bitmap),
                    dfs_bclk_manager.current_bclk
                    );
    }
    return 0;
}

int bclk_put(uint32_t user)
{
    if (enable) {
        printk("%s: %d, %d\n", __func__, user, atomic_read(&dfs_bclk_manager.counter));
        atomic_dec(&dfs_bclk_manager.counter);
        ATOMIC_CLEAR_MASK(&dfs_bclk_manager.user_bitmap, 1<<user);
        if (user == BCLK_USER_MPEG)
            nxp_pm_data_restore(NULL);
        dfs_bclk_manager.current_bclk =
            dfs_bclk_manager.func(
                    dfs_bclk_manager.bclk_pll_num,
                    atomic_read(&dfs_bclk_manager.counter),
                    atomic_read(&dfs_bclk_manager.user_bitmap),
                    dfs_bclk_manager.current_bclk
                    );
    }
    return 0;
}

int register_dfs_bclk_func(dfs_bclk_func func)
{
    dfs_bclk_manager.func = func;
    return 0;
}

EXPORT_SYMBOL(bclk_get);
EXPORT_SYMBOL(bclk_put);
EXPORT_SYMBOL(register_dfs_bclk_func);

static ssize_t max_show(struct device *pdev, struct device_attribute *attr, char *buf)
{
    return scnprintf(buf, PAGE_SIZE, "%d\n", bclk_max);
}

static ssize_t max_store(struct device *pdev, struct device_attribute *attr, const char *buf, size_t n)
{
    uint32_t val;
    sscanf(buf, "%d", &val);
    if (val >= bclk_min && val <= BCLK_MAX) {
        printk("%s: bclk_max set to %d\n", __func__, val);
        bclk_max = val;
    } else {
        printk("%s: invalid value %d(%d-%d)\n", __func__, val, bclk_min, BCLK_MAX);
    }
    return n;
}

static ssize_t min_show(struct device *pdev, struct device_attribute *attr, char *buf)
{
    return scnprintf(buf, PAGE_SIZE, "%d\n", bclk_min);
}

static ssize_t min_store(struct device *pdev, struct device_attribute *attr, const char *buf, size_t n)
{
    uint32_t val;
    sscanf(buf, "%d", &val);
    if (val >= BCLK_MIN && val <= bclk_max) {
        printk("%s: bclk_min set to %d\n", __func__, val);
        bclk_min = val;
    } else {
        printk("%s: invalid value %d(%d-%d)\n", __func__, val, BCLK_MIN, bclk_max);
    }
    return n;
}

static ssize_t enable_show(struct device *pdev, struct device_attribute *attr, char *buf)
{
    return scnprintf(buf, PAGE_SIZE, "%d\n", enable);
}

static ssize_t enable_store(struct device *pdev, struct device_attribute *attr, const char *buf, size_t n)
{
    uint32_t val;
    sscanf(buf, "%d", &val);
    if (val > 0)
        enable = 1;
    else
        enable = 0;
    return n;
}

static ssize_t force_store(struct device *pdev, struct device_attribute *attr, const char *buf, size_t n)
{
    uint32_t val;
    sscanf(buf, "%d", &val);
    /*_set_and_wait(dfs_bclk_manager.bclk_pll_num, val);*/
    _disable_irq_and_set(dfs_bclk_manager.bclk_pll_num, val);
    return n;
}

static struct device_attribute max_attr =
    __ATTR(max, S_IRUGO | S_IWUSR, max_show, max_store);
static struct device_attribute min_attr =
    __ATTR(min, S_IRUGO | S_IWUSR, min_show, min_store);
static struct device_attribute enable_attr =
    __ATTR(enable, S_IRUGO | S_IWUSR, enable_show, enable_store);
static struct device_attribute force_attr =
    __ATTR(force, S_IRUGO | S_IWUSR, NULL, force_store);

static struct attribute *attrs[] = {
    &max_attr.attr,
    &min_attr.attr,
    &enable_attr.attr,
    &force_attr.attr,
    NULL,
};

static struct attribute_group attr_group = {
     .attrs = (struct attribute **)attrs,
};

static int __init dfs_bclk_init(void)
{
     struct kobject *kobj = NULL;
     int ret = 0;

     kobj = kobject_create_and_add("dfs-bclk", &platform_bus.kobj);
     if (!kobj) {
         printk(KERN_ERR "%s: Failed to create kobject for dfs-bclk\n", __func__);
         return -EINVAL;
     }

     ret = sysfs_create_group(kobj, &attr_group);
     if (ret) {
         printk(KERN_ERR "%s: Failed to sysfs_create_group for dfs-bclk\n", __func__);
         kobject_del(kobj);
         return -ret;
     }

     clkpwr = (struct NX_CLKPWR_RegisterSet*)IO_ADDRESS(PHY_BASEADDR_CLKPWR_MODULE);

     return 0;
}
module_init(dfs_bclk_init);

MODULE_AUTHOR("swpark <swpark@nexell.co.kr>");
MODULE_DESCRIPTION("DFS BCLK Manger for NXP4330");
MODULE_LICENSE("GPL");
