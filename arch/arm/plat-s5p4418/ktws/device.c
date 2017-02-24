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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/irq.h>
#include <linux/amba/pl022.h>
#include <linux/gpio.h>

/* nexell soc headers */
#include <mach/platform.h>
#include <mach/devices.h>
#include <mach/soc.h>

#if defined(CONFIG_NXP_HDMI_CEC)
#include <mach/nxp-hdmi-cec.h>
#endif

/*------------------------------------------------------------------------------
 * BUS Configure
 */
#if (CFG_BUS_RECONFIG_ENB == 1)
#include <mach/s5p4418_bus.h>

const u8 g_DrexBRB_RD[2] = {
	0x4,            // Port0
	0xF             // Port1
};

const u8 g_DrexBRB_WR[2] = {
	0x4,            // Port0
	0xF             // Port1
};

const u16 g_DrexQoS[2] = {
	0x100,          // S0
	0xFFF           // S1, Default value
};


#if (CFG_BUS_RECONFIG_TOPBUSSI == 1)
const u8 g_TopBusSI[8] = {
	TOPBUS_SI_SLOT_DMAC0,
	TOPBUS_SI_SLOT_USBOTG,
	TOPBUS_SI_SLOT_USBHOST0,
	TOPBUS_SI_SLOT_DMAC1,
	TOPBUS_SI_SLOT_SDMMC,
	TOPBUS_SI_SLOT_USBOTG,
	TOPBUS_SI_SLOT_USBHOST1,
	TOPBUS_SI_SLOT_USBOTG
};
#endif

#if (CFG_BUS_RECONFIG_TOPBUSQOS == 1)
const u8 g_TopQoSSI[2] = {
	1,      // Tidemark
	(1<<TOPBUS_SI_SLOT_DMAC0) |     // Control
	(1<<TOPBUS_SI_SLOT_MP2TS) |
	(1<<TOPBUS_SI_SLOT_DMAC1) |
	(1<<TOPBUS_SI_SLOT_SDMMC) |
	(1<<TOPBUS_SI_SLOT_USBOTG) |
	(1<<TOPBUS_SI_SLOT_USBHOST0) |
	(1<<TOPBUS_SI_SLOT_USBHOST1)
};
#endif

#if (CFG_BUS_RECONFIG_BOTTOMBUSSI == 1)
const u8 g_BottomBusSI[8] = {
        BOTBUS_SI_SLOT_1ST_ARM,
        BOTBUS_SI_SLOT_MALI,
        BOTBUS_SI_SLOT_TOP,
        BOTBUS_SI_SLOT_1ST_CODA,
        BOTBUS_SI_SLOT_2ND_ARM,
        BOTBUS_SI_SLOT_TOP,
        BOTBUS_SI_SLOT_TOP,
        BOTBUS_SI_SLOT_2ND_CODA
};
#endif

#if (CFG_BUS_RECONFIG_BOTTOMBUSQOS == 1)
const u8 g_BottomQoSSI[2] = {
	4,      // Tidemark
	(1<<BOTBUS_SI_SLOT_1ST_ARM) |   // Control
		(1<<BOTBUS_SI_SLOT_2ND_ARM) |
		(0<<BOTBUS_SI_SLOT_MALI) |
		(1<<BOTBUS_SI_SLOT_TOP) |
		(0<<BOTBUS_SI_SLOT_DEINTERLACE) |
		(1<<BOTBUS_SI_SLOT_1ST_CODA)
};
#endif

#if (CFG_BUS_RECONFIG_DISPBUSSI == 1)
const u8 g_DispBusSI[3] = {
	DISBUS_SI_SLOT_1ST_DISPLAY,
	DISBUS_SI_SLOT_2ND_DISPLAY,
	DISBUS_SI_SLOT_GMAC
};
#endif
#endif	/* #if (CFG_BUS_RECONFIG_ENB == 1) */

/*------------------------------------------------------------------------------
 * CPU Frequence
 */
#if defined(CONFIG_ARM_NXP_CPUFREQ)

struct nxp_cpufreq_plat_data dfs_plat_data = {
	.pll_dev	= CONFIG_NXP_CPUFREQ_PLLDEV,
	.supply_name	= "vdd_arm_1.3V",	// Refer to CONFIG_REGULATOR_NXE2000
//	.max_cpufreq	= 1400*1000,
//	.max_retention  = 20*1000,
//	.rest_cpufreq   = 400*1000,
//	.rest_retention = 1*1000,
};

static struct platform_device dfs_plat_device = {
	.name			= DEV_NAME_CPUFREQ,
	.dev			= {
		.platform_data	= &dfs_plat_data,
	}
};
#endif

#if defined (CONFIG_SENSORS_NXP_ADC_TEMP)
struct nxp_adc_tmp_trigger adc_tmp_event[] = {
	{
		// TMU Policy
		.temp  = 52,
		.freq  = 600000,
		.period = 1000, /* Ms */
	} , {
		.temp  = 56,
		.freq  = 400000,
		.period = 1000, /* Ms */
	}
};

struct nxp_adc_tmp_platdata adc_tmp_plat_data ={
	.channel	= 2,
	.tmp_offset	= 0,
	.duration	= 1000,
	.step_up	= 1,
	.event		= adc_tmp_event,
	.eventsize = ARRAY_SIZE( adc_tmp_event),
};

static struct platform_device adc_temp_plat_device = {
	.name			= "nxp-adc-tmp",
	.dev			= {
		.platform_data	= &adc_tmp_plat_data,
	}
};
#endif //SENSORS_NXP_ADC_TEMP

/*------------------------------------------------------------------------------
 * DW GMAC board config
 */
#if defined(CONFIG_NXPMAC_ETH)
#include <linux/phy.h>
#include <linux/nxpmac.h>
#include <linux/delay.h>
#include <linux/gpio.h>
int nxpmac_init(struct platform_device *pdev)
{
    u32 addr;

    // Clock control
    NX_CLKGEN_Initialize();
    addr = NX_CLKGEN_GetPhysicalAddress(CLOCKINDEX_OF_DWC_GMAC_MODULE);
    NX_CLKGEN_SetBaseAddress( CLOCKINDEX_OF_DWC_GMAC_MODULE, (void*)IO_ADDRESS(addr));

    NX_CLKGEN_SetClockSource( CLOCKINDEX_OF_DWC_GMAC_MODULE, 0, 4);     // Sync mode for 100 & 10Base-T : External RX_clk
    NX_CLKGEN_SetClockDivisor( CLOCKINDEX_OF_DWC_GMAC_MODULE, 0, 1);    // Sync mode for 100 & 10Base-T

    NX_CLKGEN_SetClockOutInv( CLOCKINDEX_OF_DWC_GMAC_MODULE, 0, CFALSE);    // TX Clk invert off : 100 & 10Base-T

    NX_CLKGEN_SetClockDivisorEnable( CLOCKINDEX_OF_DWC_GMAC_MODULE, CTRUE);

    // Reset control
    NX_RSTCON_Initialize();
    addr = NX_RSTCON_GetPhysicalAddress();
    NX_RSTCON_SetBaseAddress( (void*)IO_ADDRESS(addr) );
    NX_RSTCON_SetnRST(RESETINDEX_OF_DWC_GMAC_MODULE_aresetn_i, RSTCON_ENABLE);
    udelay(100);
    NX_RSTCON_SetnRST(RESETINDEX_OF_DWC_GMAC_MODULE_aresetn_i, RSTCON_DISABLE);
    udelay(100);
    NX_RSTCON_SetnRST(RESETINDEX_OF_DWC_GMAC_MODULE_aresetn_i, RSTCON_ENABLE);
    udelay(100);


    gpio_request(CFG_ETHER_GMAC_PHY_RST_NUM, "Ethernet Rst pin");
    gpio_direction_output(CFG_ETHER_GMAC_PHY_RST_NUM, 1);
    udelay(100);
    gpio_set_value(CFG_ETHER_GMAC_PHY_RST_NUM, 0);
    udelay(100);
    gpio_set_value(CFG_ETHER_GMAC_PHY_RST_NUM, 1);

    gpio_free(CFG_ETHER_GMAC_PHY_RST_NUM);

    printk("NXP mac init ..................\n");
    return 0;
}

int gmac_phy_reset(void *priv)
{
    // Set GPIO nReset
    gpio_set_value(CFG_ETHER_GMAC_PHY_RST_NUM, 1);
    udelay(100);
    gpio_set_value(CFG_ETHER_GMAC_PHY_RST_NUM, 0);
    udelay(100);
    gpio_set_value(CFG_ETHER_GMAC_PHY_RST_NUM, 1);
    msleep(30);

    return 0;
}

static struct stmmac_mdio_bus_data nxpmac0_mdio_bus = {
    .phy_reset = gmac_phy_reset,
    .phy_mask = 0,
    .probed_phy_irq = CFG_ETHER_GMAC_PHY_IRQ_NUM,
};

static struct plat_stmmacenet_data nxpmac_plat_data = {
    .phy_addr = 3,      /* hw config */
    .clk_csr = 0x4,     /* PCLK 150~250 Mhz */
    .interface = PHY_INTERFACE_MODE_RGMII,
    .autoneg = AUTONEG_ENABLE, /* AUTONEG_ENABLE or AUTONEG_DISABLE */
    .speed = SPEED_1000,/* speed & duplex settings apply only when AUTONEG_DISABLE */
    .duplex = DUPLEX_FULL,
    .pbl = 16,          /* burst 16 */
    .has_gmac = 1,      /* GMAC ethernet */
    .enh_desc = 1,
    .mdio_bus_data = &nxpmac0_mdio_bus,
    .init = &nxpmac_init,
};

/* DWC GMAC Controller registration */

static struct resource nxpmac_resource[] = {
    [0] = DEFINE_RES_MEM(PHY_BASEADDR_GMAC, SZ_8K),
    [1] = DEFINE_RES_IRQ_NAMED(IRQ_PHY_GMAC, "macirq"),
};

static u64 nxpmac_dmamask = DMA_BIT_MASK(32);

struct platform_device nxp_gmac_dev = {
    .name           = "stmmaceth",  //"s5p4418-gmac",
    .id             = -1,
    .num_resources  = ARRAY_SIZE(nxpmac_resource),
    .resource       = nxpmac_resource,
    .dev            = {
        .dma_mask           = &nxpmac_dmamask,
        .coherent_dma_mask  = DMA_BIT_MASK(32),
        .platform_data      = &nxpmac_plat_data,
    }
};
#endif

/*------------------------------------------------------------------------------
 * NAND device
 */
#if defined(CONFIG_MTD_NAND_NXP)
#include <linux/mtd/partitions.h>
#include <asm-generic/sizes.h>

static struct mtd_partition nxp_nand_parts[] = {
#if 0
	{
		.name           = "root",
		.offset         =   0 * SZ_1M,
	},
#else
	{
		.name		= "system",
		.offset		=  64 * SZ_1M,
		.size		= 512 * SZ_1M,
	}, {
		.name		= "cache",
		.offset		= MTDPART_OFS_APPEND,
		.size		= 256 * SZ_1M,
	}, {
		.name		= "userdata",
		.offset		= MTDPART_OFS_APPEND,
		.size		= MTDPART_SIZ_FULL,
	}
#endif
};

static struct nxp_nand_plat_data nand_plat_data = {
	.parts		= nxp_nand_parts,
	.nr_parts	= ARRAY_SIZE(nxp_nand_parts),
	.chip_delay = 10,
};

static struct platform_device nand_plat_device = {
	.name	= DEV_NAME_NAND,
	.id		= -1,
	.dev	= {
		.platform_data	= &nand_plat_data,
	},
};
#elif defined(CONFIG_NXP_FTL)
static struct resource nand_resource =
{
};

static struct platform_device nand_plat_device = {
	.name	= DEV_NAME_NAND,
	.id		= -1,
	.dev	= {
	},
};
#endif	/* CONFIG_NXP_FTL */

/*------------------------------------------------------------------------------
 * Reserved memory
 */
#ifdef CONFIG_CMA
#include <linux/cma.h>
extern void nxp_cma_region_reserve(struct cma_region *, const char *);

void __init nxp_reserve_mem(void)
{
    static struct cma_region regions[] = {
        {
            .name = "ion",
#ifdef CONFIG_ION_NXP_CONTIGHEAP_SIZE
            .size = CONFIG_ION_NXP_CONTIGHEAP_SIZE * SZ_1K,
#else
			.size = 0,
#endif
            {
                .alignment = PAGE_SIZE,
            }
        },
        {
            .size = 0
        }
    };

    static const char map[] __initconst =
        "ion-nxp=ion;"
        "nx_vpu=ion;";

#ifdef CONFIG_ION_NXP_CONTIGHEAP_SIZE
    printk("%s: reserve CMA: size %d\n", __func__, CONFIG_ION_NXP_CONTIGHEAP_SIZE * SZ_1K);
#endif
    nxp_cma_region_reserve(regions, map);
}
#endif

/*------------------------------------------------------------------------------
 * PMIC platform device
 */
#if defined(CONFIG_REGULATOR_NXE2000)

#include <linux/i2c.h>
#include <linux/regulator/machine.h>
#include <linux/mfd/nxe2000.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/regulator/fixed.h>
#include <linux/regulator/nxe2000-regulator.h>
#include <linux/power/nxe2000_battery.h>
//#include <linux/rtc/rtc-nxe2000.h>
//#include <linux/rtc.h>

#define NXE2000_I2C_BUS		(0)
#define NXE2000_I2C_ADDR	(0x64 >> 1)

/* NXE2000 IRQs */
#define NXE2000_IRQ_BASE	(IRQ_SYSTEM_END)
#define NXE2000_GPIO_BASE	(ARCH_NR_GPIOS)
#define NXE2000_GPIO_IRQ	(NXE2000_GPIO_BASE + 8)

//#define CONFIG_NXE2000_RTC

static struct regulator_consumer_supply nxe2000_dc1_supply_0[] = {
	REGULATOR_SUPPLY("vdd_arm_1.3V", NULL),
};
static struct regulator_consumer_supply nxe2000_dc2_supply_0[] = {
	REGULATOR_SUPPLY("vdd_core_1.2V", NULL),
};
static struct regulator_consumer_supply nxe2000_dc3_supply_0[] = {
	REGULATOR_SUPPLY("vdd_sys_3.3V", NULL),
};
static struct regulator_consumer_supply nxe2000_dc4_supply_0[] = {
	REGULATOR_SUPPLY("vdd_ddr_1.6V", NULL),
};
static struct regulator_consumer_supply nxe2000_dc5_supply_0[] = {
	REGULATOR_SUPPLY("vdd_sys_1.6V", NULL),
};

static struct regulator_consumer_supply nxe2000_ldo1_supply_0[] = {
	REGULATOR_SUPPLY("vgps_3.3V", NULL),
};
static struct regulator_consumer_supply nxe2000_ldo2_supply_0[] = {
	REGULATOR_SUPPLY("vcam1_1.8V", NULL),
};
static struct regulator_consumer_supply nxe2000_ldo3_supply_0[] = {
	REGULATOR_SUPPLY("vsys1_1.8V", NULL),
};
static struct regulator_consumer_supply nxe2000_ldo4_supply_0[] = {
	REGULATOR_SUPPLY("vsys_1.9V", NULL),
};
static struct regulator_consumer_supply nxe2000_ldo5_supply_0[] = {
	REGULATOR_SUPPLY("vcam_2.8V", NULL),
};
static struct regulator_consumer_supply nxe2000_ldo6_supply_0[] = {
	REGULATOR_SUPPLY("valive_3.3V", NULL),
};
static struct regulator_consumer_supply nxe2000_ldo7_supply_0[] = {
	REGULATOR_SUPPLY("vvid_2.8V", NULL),
};
static struct regulator_consumer_supply nxe2000_ldo8_supply_0[] = {
	REGULATOR_SUPPLY("vdumy0_3.3V", NULL),
};
static struct regulator_consumer_supply nxe2000_ldo9_supply_0[] = {
	REGULATOR_SUPPLY("vcam_3.3V", NULL),
};
static struct regulator_consumer_supply nxe2000_ldo10_supply_0[] = {
	REGULATOR_SUPPLY("vcore_1.0V", NULL),
};
static struct regulator_consumer_supply nxe2000_ldortc1_supply_0[] = {
	REGULATOR_SUPPLY("valive_1.8V", NULL),
};
static struct regulator_consumer_supply nxe2000_ldortc2_supply_0[] = {
	REGULATOR_SUPPLY("valive_1.0V", NULL),
};

#define NXE2000_PDATA_INIT(_name, _sname, _minuv, _maxuv, _always_on, _boot_on, \
	_init_uv, _init_enable, _slp_slots) \
	static struct nxe2000_regulator_platform_data pdata_##_name##_##_sname = \
	{	\
		.regulator = {	\
			.constraints = {	\
				.min_uV		= _minuv,	\
				.max_uV		= _maxuv,	\
				.valid_modes_mask	= (REGULATOR_MODE_NORMAL |	\
									REGULATOR_MODE_STANDBY),	\
				.valid_ops_mask		= (REGULATOR_CHANGE_MODE |	\
									REGULATOR_CHANGE_STATUS |	\
									REGULATOR_CHANGE_VOLTAGE),	\
				.always_on	= _always_on,	\
				.boot_on	= _boot_on,		\
				.apply_uV	= 1,			\
			},	\
			.num_consumer_supplies =		\
				ARRAY_SIZE(nxe2000_##_name##_supply_##_sname),	\
			.consumer_supplies	= nxe2000_##_name##_supply_##_sname, \
			.supply_regulator	= 0,	\
		},	\
		.init_uV		= _init_uv,		\
		.init_enable	= _init_enable,	\
		.sleep_slots	= _slp_slots,	\
	}
/* min_uV/max_uV : Please set the appropriate value for the devices that the power supplied within a*/
/*                 range from min to max voltage according to NXE2000 specification. */
NXE2000_PDATA_INIT(dc1,      0,	 950000, 2000000, 1, 1, 1125000, 1,  4);	/* 1.2V ARM */
NXE2000_PDATA_INIT(dc2,      0,	1000000, 2000000, 1, 1, 1000000, 1,  4);	/* 1.1V CORE */
NXE2000_PDATA_INIT(dc3,      0,	1000000, 3500000, 1, 1, 3300000, 1,  0);	/* 3.3V SYS */
NXE2000_PDATA_INIT(dc4,      0,	1000000, 2000000, 1, 1, 1500000, 1, -1);	/* 1.5V DDR */
NXE2000_PDATA_INIT(dc5,      0,	1000000, 2000000, 1, 1, 1500000, 1,  4);	/* 1.5V SYS */
NXE2000_PDATA_INIT(ldo1,     0,	1000000, 3500000, 0, 0,      -1, 0,  0);	/* 3.3V GPS */
NXE2000_PDATA_INIT(ldo2,     0,	1000000, 3500000, 0, 0,      -1, 0,  0);	/* 1.8V CAM1 */
NXE2000_PDATA_INIT(ldo3,     0,	1000000, 3500000, 1, 0, 1800000, 1,  2);	/* 1.8V SYS1 */
NXE2000_PDATA_INIT(ldo4,     0,	1000000, 3500000, 0, 0,      -1, 0,  2);	/* 1.8V SYS (option) */
NXE2000_PDATA_INIT(ldo5,     0,	1000000, 3500000, 0, 0,      -1, 0,  0);	/* 2.8V VCAM */
NXE2000_PDATA_INIT(ldo6,     0,	1000000, 3500000, 1, 0, 3300000, 1, -1);	/* 3.3V ALIVE */
NXE2000_PDATA_INIT(ldo7,     0,	1000000, 3500000, 1, 0,      -1, 0,  0);	/* 2.8V VID (option) */
NXE2000_PDATA_INIT(ldo8,     0,	1000000, 3500000, 0, 0,      -1, 0,  0);	/* Not Use */
NXE2000_PDATA_INIT(ldo9,     0,	1000000, 3500000, 0, 0,      -1, 0,  0);	/* 2.8V VCAM */
NXE2000_PDATA_INIT(ldo10,    0,	1000000, 3500000, 1, 1, 1000000, 1,  4);	/* 1.0V VCC1P0 CORE */
NXE2000_PDATA_INIT(ldortc1,  0,	1700000, 3500000, 1, 0, 1800000, 1, -1);	/* 1.8V ALIVE */
NXE2000_PDATA_INIT(ldortc2,  0,	1000000, 3500000, 1, 0, 1000000, 1, -1);	/* 1.0V ALIVE */

/*-------- if nxe2000 RTC exists -----------*/
#ifdef CONFIG_NXE2000_RTC
static struct nxe2000_rtc_platform_data rtc_data = {
	.irq	= NXE2000_IRQ_BASE,
	.time	= {
		.tm_year	= 1970,
		.tm_mon		= 0,
		.tm_mday	= 1,
		.tm_hour	= 0,
		.tm_min		= 0,
		.tm_sec		= 0,
	},
};

#define NXE2000_RTC_REG	\
{	\
	.id		= 0,	\
	.name	= "rtc_nxe2000",	\
	.platform_data	= &rtc_data,	\
}
#endif
/*-------- if Nexell RTC exists -----------*/

#define NXE2000_REG(_id, _name, _sname)	\
{	\
	.id		= NXE2000_ID_##_id,	\
	.name	= "nxe2000-regulator",	\
	.platform_data	= &pdata_##_name##_##_sname,	\
}

//==========================================
//NXE2000 Power_Key device data
//==========================================
static struct nxe2000_pwrkey_platform_data nxe2000_pwrkey_data = {
	.irq 		= NXE2000_IRQ_BASE,
	.delay_ms 	= 20,
};
#define NXE2000_PWRKEY_REG		\
{	\
	.id 	= -1,	\
	.name 	= "nxe2000-pwrkey",	\
	.platform_data 	= &nxe2000_pwrkey_data,	\
}

#define NXE2000_DEV_REG 		\
	NXE2000_REG(DC1, dc1, 0),	\
	NXE2000_REG(DC2, dc2, 0),	\
	NXE2000_REG(DC3, dc3, 0),	\
	NXE2000_REG(DC4, dc4, 0),	\
	NXE2000_REG(DC5, dc5, 0),	\
	NXE2000_REG(LDO1, ldo1, 0),	\
	NXE2000_REG(LDO2, ldo2, 0),	\
	NXE2000_REG(LDO3, ldo3, 0),	\
	NXE2000_REG(LDO4, ldo4, 0),	\
	NXE2000_REG(LDO5, ldo5, 0),	\
	NXE2000_REG(LDO6, ldo6, 0),	\
	NXE2000_REG(LDO7, ldo7, 0),	\
	NXE2000_REG(LDO8, ldo8, 0),	\
	NXE2000_REG(LDO9, ldo9, 0),	\
	NXE2000_REG(LDO10, ldo10, 0),	\
	NXE2000_REG(LDORTC1, ldortc1, 0),	\
	NXE2000_REG(LDORTC2, ldortc2, 0)

static struct nxe2000_subdev_info nxe2000_devs_dcdc[] = {
	NXE2000_DEV_REG,
	NXE2000_PWRKEY_REG,
};


#define NXE2000_GPIO_INIT(_init_apply, _output_mode, _output_val, _led_mode, _led_func) \
	{									\
		.output_mode_en = _output_mode,	\
		.output_val		= _output_val,	\
		.init_apply		= _init_apply,	\
		.led_mode		= _led_mode,	\
		.led_func		= _led_func,	\
	}
struct nxe2000_gpio_init_data nxe2000_gpio_data[] = {
	NXE2000_GPIO_INIT(false, false, 0, 0, 0),
	NXE2000_GPIO_INIT(false, false, 0, 0, 0),
	NXE2000_GPIO_INIT(false, false, 0, 0, 0),
	NXE2000_GPIO_INIT(false, false, 0, 0, 0),
	NXE2000_GPIO_INIT(false, false, 0, 0, 0),
};

static struct nxe2000_platform_data nxe2000_platform = {
	.num_subdevs		= ARRAY_SIZE(nxe2000_devs_dcdc),
	.subdevs			= nxe2000_devs_dcdc,
	.irq_base			= NXE2000_IRQ_BASE,
	.irq_type			= IRQ_TYPE_EDGE_FALLING,
	.gpio_base			= NXE2000_GPIO_BASE,
	.gpio_init_data		= nxe2000_gpio_data,
	.num_gpioinit_data	= ARRAY_SIZE(nxe2000_gpio_data),
	.enable_shutdown_pin	= true,
};

static struct i2c_board_info __initdata nxe2000_i2c_boardinfo[] = {
	{
		I2C_BOARD_INFO("nxe2000", NXE2000_I2C_ADDR),
		.irq			= CFG_GPIO_PMIC_INTR,
		.platform_data	= &nxe2000_platform,
	},
};
#endif  /* CONFIG_REGULATOR_NXE2000 */

/*------------------------------------------------------------------------------
 * SSP/SPI
 */
#if defined(CONFIG_SPI_SPIDEV) || defined(CONFIG_SPI_SPIDEV_MODULE)
#include <linux/spi/spi.h>
static void spi0_cs(u32 chipselect)
{
#if (CFG_SPI0_CS_GPIO_MODE)
	if(nxp_soc_gpio_get_io_func( CFG_SPI0_CS )!= nxp_soc_gpio_get_altnum( CFG_SPI0_CS))
		nxp_soc_gpio_set_io_func( CFG_SPI0_CS, nxp_soc_gpio_get_altnum( CFG_SPI0_CS));

	nxp_soc_gpio_set_io_dir( CFG_SPI0_CS,1);
	nxp_soc_gpio_set_out_value(	 CFG_SPI0_CS , chipselect);
#else
	;
#endif
}

struct pl022_config_chip spi0_info = {
    /* available POLLING_TRANSFER, INTERRUPT_TRANSFER, DMA_TRANSFER */
    .com_mode = CFG_SPI0_COM_MODE,
    .iface = SSP_INTERFACE_MOTOROLA_SPI,
    /* We can only act as master but SSP_SLAVE is possible in theory */
    .hierarchy = SSP_MASTER,
    /* 0 = drive TX even as slave, 1 = do not drive TX as slave */
    .slave_tx_disable = 1,
    .rx_lev_trig = SSP_RX_4_OR_MORE_ELEM,
    .tx_lev_trig = SSP_TX_4_OR_MORE_EMPTY_LOC,
    .ctrl_len = SSP_BITS_8,
    .wait_state = SSP_MWIRE_WAIT_ZERO,
    .duplex = SSP_MICROWIRE_CHANNEL_FULL_DUPLEX,
    /*
     * This is where you insert a call to a function to enable CS
     * (usually GPIO) for a certain chip.
     */
#if (CFG_SPI0_CS_GPIO_MODE)
    .cs_control = spi0_cs,
#endif
	.clkdelay = SSP_FEEDBACK_CLK_DELAY_1T,

};

static struct spi_board_info spi_plat_board[] __initdata = {
    [0] = {
        .modalias        = "spidev",    	/* fixup */
        .max_speed_hz    = 10*1000*1000,    /* max spi clock (SCK) speed in HZ */
        .bus_num         = 0,           	/* Note> set bus num, must be smaller than ARRAY_SIZE(spi_plat_device) */
        .chip_select     = 0,           	/* Note> set chip select num, must be smaller than spi cs_num */
        .controller_data = &spi0_info,
        .mode            = SPI_MODE_0,
    },
};

#endif

/*------------------------------------------------------------------------------
 * DW MMC board config
 */
#if defined(CONFIG_MMC_DW)
int _dwmci_ext_cd_init(void (*notify_func)(struct platform_device *, int state)) {
	return 0;
}

int _dwmci_ext_cd_cleanup(void (*notify_func)(struct platform_device *, int state)) {
	return 0;
}

#ifdef CONFIG_MMC_NXP_CH1
static struct dw_mci_board _dwmci1_data = {
    .quirks         = DW_MCI_QUIRK_HIGHSPEED,
    .bus_hz         = 100 * 1000 * 1000,
    .caps           = MMC_CAP_CMD23,
    .detect_delay_ms= 200,
    .cd_type        = DW_MCI_CD_EXTERNAL,
    .ext_cd_init    = _dwmci_ext_cd_init,
    .ext_cd_cleanup = _dwmci_ext_cd_cleanup,
	.pm_caps		= MMC_PM_KEEP_POWER | MMC_PM_IGNORE_PM_NOTIFY,
    .clk_dly        = DW_MMC_DRIVE_DELAY(0) | DW_MMC_SAMPLE_DELAY(0) | DW_MMC_DRIVE_PHASE(2) | DW_MMC_SAMPLE_PHASE(1),
#if defined (CONFIG_MMC_DW_IDMAC) && defined (CONFIG_MMC_NXP_CH0_USE_DMA)
    .mode           = DMA_MODE,
#else
    .mode           = PIO_MODE,
#endif

};
#endif

#ifdef CONFIG_MMC_NXP_CH0
static struct dw_mci_board _dwmci0_data = {
	.quirks			= DW_MCI_QUIRK_BROKEN_CARD_DETECTION |
						DW_MCI_QUIRK_HIGHSPEED |
						DW_MMC_QUIRK_HW_RESET_PW |
						DW_MCI_QUIRK_NO_DETECT_EBIT,
	.bus_hz			= 100 * 1000 * 1000,
	.hs_over_clk	= 50 * 1000 * 1000,
	.caps			= MMC_CAP_UHS_DDR50 |
						MMC_CAP_NONREMOVABLE |
						MMC_CAP_4_BIT_DATA | MMC_CAP_CMD23 |
						MMC_CAP_ERASE | MMC_CAP_HW_RESET,
	.desc_sz		= 4,
	.detect_delay_ms= 200,
	.clk_dly		= DW_MMC_DRIVE_DELAY(0) | DW_MMC_SAMPLE_DELAY(0) | DW_MMC_DRIVE_PHASE(2) | DW_MMC_SAMPLE_PHASE(1),
#if defined (CONFIG_MMC_DW_IDMAC) && defined (CONFIG_MMC_NXP_CH0_USE_DMA)
	.mode			= DMA_MODE,
#else
	.mode			= PIO_MODE,
#endif
};
#endif

#endif /* CONFIG_MMC_DW */

/*------------------------------------------------------------------------------
 * USB HSIC power control.
 */
int nxp_hsic_phy_pwr_on(struct platform_device *pdev, bool on)
{
    return 0;
}
EXPORT_SYMBOL(nxp_hsic_phy_pwr_on);

/*------------------------------------------------------------------------------
 * register board platform devices
 */
void __init nxp_board_devices_register(void)
{
	printk("[Register board platform devices]\n");

#if defined(CONFIG_ARM_NXP_CPUFREQ)
	printk("plat: add dynamic frequency (pll.%d)\n", dfs_plat_data.pll_dev);
	platform_device_register(&dfs_plat_device);
#endif

#ifdef CONFIG_SENSORS_NXP_ADC_TEMP
	printk("plat: add device adc temp\n");
	platform_device_register(&adc_temp_plat_device);
#endif

#if defined(CONFIG_MMC_DW)
	#ifdef CONFIG_MMC_NXP_CH0
	nxp_mmc_add_device(0, &_dwmci0_data);
	#endif

	#ifdef CONFIG_MMC_NXP_CH1
	nxp_mmc_add_device(1, &_dwmci1_data);
	#endif
#endif

#if defined(CONFIG_REGULATOR_NXE2000)
	printk("plat: add device nxe2000 pmic\n");
	i2c_register_board_info(NXE2000_I2C_BUS, nxe2000_i2c_boardinfo, ARRAY_SIZE(nxe2000_i2c_boardinfo));
#endif

#if defined(CONFIG_SPI_SPIDEV) || defined(CONFIG_SPI_SPIDEV_MODULE)
    spi_register_board_info(spi_plat_board, ARRAY_SIZE(spi_plat_board));
    printk("plat: register spidev\n");
#endif

#if defined(CONFIG_NXPMAC_ETH)
	printk("plat: add device nxp-gmac\n");
	platform_device_register(&nxp_gmac_dev);
#endif

	/* END */
	printk("\n");
}
