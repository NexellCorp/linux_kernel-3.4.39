/*
 * (C) Copyright 2009
 * jung hyun kim, Nexell Co, <jhkim@nexell.co.kr>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/delay.h>	/* mdelay */
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>

/* nexell soc headers */
#include <mach/platform.h>
#include <mach/devices.h>

#if (0)	/* default on */
#define DBGOUT(msg...)		{ printk("cpu: " msg); }
#else
#define DBGOUT(msg...)		do {} while (0)
#endif

static void cpu_base_init(void)
{
	U32 tie_reg, val;
	int i = 0;

	NX_RSTCON_Initialize();
	NX_RSTCON_SetBaseAddress((U32)IO_ADDRESS(NX_RSTCON_GetPhysicalAddress()));

	NX_TIEOFF_Initialize();
	NX_TIEOFF_SetBaseAddress((U32)IO_ADDRESS(NX_TIEOFF_GetPhysicalAddress()));

	NX_GPIO_Initialize();
	for (i = 0; NX_GPIO_GetNumberOfModule() > i; i++) {
		NX_GPIO_SetBaseAddress(i, (U32)IO_ADDRESS(NX_GPIO_GetPhysicalAddress(i)));
		NX_GPIO_OpenModule(i);
	}

	NX_ALIVE_Initialize();
	NX_ALIVE_SetBaseAddress((U32)IO_ADDRESS(NX_ALIVE_GetPhysicalAddress()));
	NX_ALIVE_OpenModule();

	NX_CLKPWR_Initialize();
	NX_CLKPWR_SetBaseAddress((U32)IO_ADDRESS(NX_CLKPWR_GetPhysicalAddress()));
	NX_CLKPWR_OpenModule();

	/*
	 * NOTE> ALIVE Power Gate must enable for RTC register access.
	 */
	NX_ALIVE_SetWriteEnable(CTRUE);

	/*
	 * NOTE> Control for ACP register access.
	 */
	tie_reg = (U32)IO_ADDRESS(NX_TIEOFF_GetPhysicalAddress());

	val = __raw_readl(tie_reg + 0x70) & ~((3 << 30) | (3 << 10));
	writel(val, (tie_reg + 0x70));

	val = __raw_readl(tie_reg + 0x80) & ~(3 << 3);
	writel(val, (tie_reg + 0x80));
}

static void cpu_bus_init(void)
{
	/* MCUS for Static Memory. */
	NX_MCUS_Initialize();
	NX_MCUS_SetBaseAddress((U32)IO_ADDRESS(NX_MCUS_GetPhysicalAddress()));
	NX_MCUS_OpenModule();

	/*
	 * MCU-Static config: Static Bus #0 ~ #1
	 */
	#define STATIC_BUS_CONFIGUTATION( _n_ )								\
	NX_MCUS_SetStaticBUSConfig											\
	( 																	\
		NX_MCUS_SBUSID_STATIC ## _n_, 									\
		CFG_SYS_STATIC ## _n_ ## _BW, 									\
		CFG_SYS_STATIC ## _n_ ## _TACS, 								\
		CFG_SYS_STATIC ## _n_ ## _TCAH, 								\
		CFG_SYS_STATIC ## _n_ ## _TCOS, 								\
		CFG_SYS_STATIC ## _n_ ## _TCOH, 								\
		CFG_SYS_STATIC ## _n_ ## _TACC, 								\
		CFG_SYS_STATIC ## _n_ ## _TSACC,								\
		(NX_MCUS_WAITMODE ) CFG_SYS_STATIC ## _n_ ## _WAITMODE, 		\
		(NX_MCUS_BURSTMODE) CFG_SYS_STATIC ## _n_ ## _RBURST, 			\
		(NX_MCUS_BURSTMODE) CFG_SYS_STATIC ## _n_ ## _WBURST			\
	);

	STATIC_BUS_CONFIGUTATION( 0);
	STATIC_BUS_CONFIGUTATION( 1);
}

/*
 * 	cpu core shutdown/reset
 */
void (*nxp_board_shutdown)(void) = NULL;
void (*nxp_board_reset)(char str, const char *cmd) = NULL;

static unsigned int core_power[][2] = {
	[0] = { TIEOFFINDEX_OF_CORTEXA9MP_TOP_QUADL2C_CLAMPCPU0,
		    TIEOFFINDEX_OF_CORTEXA9MP_TOP_QUADL2C_CPU0PWRDOWN },
	[1] = { TIEOFFINDEX_OF_CORTEXA9MP_TOP_QUADL2C_CLAMPCPU1,
		    TIEOFFINDEX_OF_CORTEXA9MP_TOP_QUADL2C_CPU1PWRDOWN },
	[2] = { TIEOFFINDEX_OF_CORTEXA9MP_TOP_QUADL2C_CLAMPCPU2,
		    TIEOFFINDEX_OF_CORTEXA9MP_TOP_QUADL2C_CPU2PWRDOWN },
	[3] = { TIEOFFINDEX_OF_CORTEXA9MP_TOP_QUADL2C_CLAMPCPU3,
		    TIEOFFINDEX_OF_CORTEXA9MP_TOP_QUADL2C_CPU3PWRDOWN },
};

void nxp_cpu_core_shutdown(int core)
{
	printk(KERN_INFO "cpu.%d shutdown ...\n", core);
	NX_TIEOFF_Set(core_power[core][0], 1);
	NX_TIEOFF_Set(core_power[core][1], 1);
}

void nxp_cpu_shutdown(void)
{
	int cpu, cur = smp_processor_id();

	if (nxp_board_shutdown)
		nxp_board_shutdown();

	for_each_present_cpu(cpu) {
		if (cpu == cur)
			continue;
		nxp_cpu_core_shutdown(cpu);
	}

	printk(KERN_INFO "cpu.%d shutdown ...\n", cur);
	NX_ALIVE_SetVDDPWRON(CFALSE, CFALSE);	/* Core power down */
}

void nxp_cpu_reset(char str, const char *cmd)
{
	printk(KERN_INFO "system reset: %s ...\n", cmd);

	if (nxp_board_reset)
		nxp_board_reset(str, cmd);

    NX_CLKPWR_SetSoftwareResetEnable(CTRUE);
    NX_CLKPWR_DoSoftwareReset();
}

/*
 * Notify cpu version
 *
 * /sys/devices/platform/cpu/version
 */
unsigned int cpu_vers_no = -1;

static ssize_t version_show(struct device *pdev,
			struct device_attribute *attr, char *buf)
{
	char *s = buf;
	s += sprintf(s, "%d\n", nxp_cpu_version());
	if (s != buf)
		*(s-1) = '\n';

	return (s - buf);
}
static struct device_attribute vers_attr = __ATTR(version, 0664, version_show, NULL);

static struct attribute *attrs[] = {
	&vers_attr.attr, NULL,
};

static struct attribute_group attr_group = {
	.attrs = (struct attribute **)attrs,
};

static int __init cpu_sys_init(void)
{
	struct kobject *kobj = NULL;
	int ret = 0;

	/* create attribute interface */
	kobj = kobject_create_and_add("cpu", &platform_bus.kobj);
	if (! kobj) {
		printk(KERN_ERR "Fail, create kobject for cpu\n");
		return -ret;
	}

	ret = sysfs_create_group(kobj, &attr_group);
	if (ret) {
		printk(KERN_ERR "Fail, create sysfs group for cpu\n");
		kobject_del(kobj);
		return -ret;
	}
	return ret;
}
module_init(cpu_sys_init);

unsigned int nxp_cpu_version(void)
{
	return cpu_vers_no;
}

/*
 * 	cpu func.
 */
#if defined(CONFIG_PROTOTYPE_RELEASE)
#define	DEBUG_PROTOTYPE		0
#else
#define	DEBUG_PROTOTYPE		1
#endif

void nxp_cpu_base_init(void)
{
	unsigned int  rev, ver = 0;

	cpu_base_init();
	cpu_bus_init();

#ifdef CONFIG_SMP
	//writel(0x0018, __PB_IO_MAP_REGS_VIRT + 0x11080);	// ACP Bus Enable
	writel(0x0000, __PB_IO_MAP_REGS_VIRT + 0x11080);	// ACP Bus Disable
	writel(0xffff, __PB_IO_MAP_MPPR_VIRT + 0x0c);		// SCU
	writel(0x000f, __PB_IO_MAP_MPPR_VIRT + 0x50);		// SCU
	writel(0x0009, __PB_IO_MAP_MPPR_VIRT + 0x00);		// SCU L2 Spec... Enable.
#endif
	/* Check version */
	if (-1 != cpu_vers_no)
		return;

	rev = __raw_readl(__PB_IO_MAP_IROM_VIRT + 0x0100);
	switch(rev) {
	case 0xe153000a:	ver = 1; break;
	default:			ver = 0; break;
	}
	cpu_vers_no = ver;
	printk(KERN_INFO "CPU : VERSION = %u (0x%X)\n", cpu_vers_no, rev);
}

