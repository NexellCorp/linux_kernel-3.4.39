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
#include <linux/syscalls.h>
#include <linux/fs.h>

/* nexell soc headers */
#include <mach/platform.h>
#include <mach/devices.h>
#include <mach/soc.h>

/*------------------------------------------------------------------------------
 * CPU Frequence
 */
#if defined(CONFIG_ARM_NXP_CPUFREQ)

struct nxp_cpufreq_plat_data dfs_plat_data = {
	.pll_dev	   	= CONFIG_NXP_CPUFREQ_PLLDEV,
		.supply_name 	= "vdd_arm_1.3V",
};

static struct platform_device dfs_plat_device = {
	.name			= DEV_NAME_CPUFREQ,
	.dev			= {
		.platform_data	= &dfs_plat_data,
	}
};
#endif

/*------------------------------------------------------------------------------
 * DW GMAC board config
 */
#if defined(CONFIG_NXPMAC_ETH)
#include <linux/phy.h>
#include <linux/nxpmac.h>
#include <linux/delay.h>
#include <linux/gpio.h>
int  nxpmac_init(struct platform_device *pdev)
{
    u32 addr;

	// Clock control
	NX_CLKGEN_Initialize();
	addr = NX_CLKGEN_GetPhysicalAddress(CLOCKINDEX_OF_DWC_GMAC_MODULE);
	NX_CLKGEN_SetBaseAddress( CLOCKINDEX_OF_DWC_GMAC_MODULE, (void*)IO_ADDRESS(addr) );

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


    gpio_request(CFG_ETHER_GMAC_PHY_RST_NUM,"Ethernet Rst pin");
	gpio_direction_output(CFG_ETHER_GMAC_PHY_RST_NUM, 1 );
	udelay( 100 );
	gpio_set_value(CFG_ETHER_GMAC_PHY_RST_NUM, 0 );
	udelay( 100 );
	gpio_set_value(CFG_ETHER_GMAC_PHY_RST_NUM, 1 );

    gpio_free(CFG_ETHER_GMAC_PHY_RST_NUM);

	printk("NXP mac init ..................\n");
	return 0;
}

int gmac_phy_reset(void *priv)
{
	// Set GPIO nReset
	gpio_set_value(CFG_ETHER_GMAC_PHY_RST_NUM, 1 );
	udelay( 100 );
	gpio_set_value(CFG_ETHER_GMAC_PHY_RST_NUM, 0 );
	udelay( 100 );
	gpio_set_value(CFG_ETHER_GMAC_PHY_RST_NUM, 1 );
	msleep( 30 );

	return 0;
}

static struct stmmac_mdio_bus_data nxpmac0_mdio_bus = {
	.phy_reset = gmac_phy_reset,
	.phy_mask = 0,
	.probed_phy_irq = CFG_ETHER_GMAC_PHY_IRQ_NUM,
};

static struct plat_stmmacenet_data nxpmac_plat_data = {
    .phy_addr = 3,		/* hw config */
    .clk_csr = 0x4,		/* PCLK 150~250 Mhz */
    .interface = PHY_INTERFACE_MODE_RGMII,
	.autoneg = AUTONEG_ENABLE, /* AUTONEG_ENABLE or AUTONEG_DISABLE */
    .speed = SPEED_1000,/* speed & duplex settings apply only when AUTONEG_DISABLE */
	.duplex = DUPLEX_FULL,
	.pbl = 16,          /* burst 16 */
	.has_gmac = 1,      /* GMAC ethernet */
	.enh_desc = 1,
	.mdio_bus_data = &nxpmac0_mdio_bus,
	//.init = &nxpmac_init,
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

/*
 * Alsa sound platform device (I2S-resmple)
 */
#if defined(CONFIG_SND_NXP_EC)

#ifndef CFG_AUDIO_I2S0_MASTER_CLOCK_IN
#define CFG_AUDIO_I2S0_MASTER_CLOCK_IN		0
#endif

static struct nxp_i2s_plat_data i2s_data_ch0 = {
	.master_mode		= CFG_AUDIO_I2S0_MASTER_MODE,
	.master_clock_in	= CFG_AUDIO_I2S0_MASTER_CLOCK_IN,
	.trans_mode			= CFG_AUDIO_I2S0_TRANS_MODE,
	.frame_bit			= CFG_AUDIO_I2S0_FRAME_BIT,
	.sample_rate		= CFG_AUDIO_I2S0_SAMPLE_RATE,
	.pre_supply_mclk 	= CFG_AUDIO_I2S0_PRE_SUPPLY_MCLK,
	/* DMA */
	.dma_filter			= pl08x_filter_id,
	.dma_play_ch		= DMA_PERIPHERAL_NAME_I2S0_TX,
	.dma_capt_ch		= DMA_PERIPHERAL_NAME_I2S0_RX,
};

static struct platform_device i2s_device_ch0 = {
	.name	= "nxp-i2s-ec",
	.id		= 0,	/* channel */
	.dev    = {
		.platform_data	= &i2s_data_ch0
	},
};

#ifndef CFG_AUDIO_I2S1_MASTER_CLOCK_IN
#define CFG_AUDIO_I2S1_MASTER_CLOCK_IN		0
#endif

static struct nxp_i2s_plat_data i2s_data_ch1 = {
	.master_mode		= CFG_AUDIO_I2S1_MASTER_MODE,
	.master_clock_in	= CFG_AUDIO_I2S1_MASTER_CLOCK_IN,
	.trans_mode			= CFG_AUDIO_I2S1_TRANS_MODE,
	.frame_bit			= CFG_AUDIO_I2S1_FRAME_BIT,
	.sample_rate		= CFG_AUDIO_I2S1_SAMPLE_RATE,
	.pre_supply_mclk 	= CFG_AUDIO_I2S1_PRE_SUPPLY_MCLK,
	/* DMA */
	.dma_filter			= pl08x_filter_id,
	.dma_play_ch		= DMA_PERIPHERAL_NAME_I2S1_TX,
	.dma_capt_ch		= DMA_PERIPHERAL_NAME_I2S1_RX,
};

static struct platform_device i2s_device_ch1 = {
	.name	= "nxp-i2s-ec",
	.id		= 1,	/* channel */
	.dev    = {
		.platform_data	= &i2s_data_ch1
	},
};

static struct platform_device *i2s_devices[] __initdata = {
	&i2s_device_ch0,
	&i2s_device_ch1,
};
#endif

/*
 * I2S/PCM : NULL CODEC
 */

#if defined(CONFIG_SND_NXP_EC)
static struct platform_device snd_null = {
	.name = "snd-null",
	.id = -1,
};

struct nxp_snd_dai_plat_data snd_null_dai_data = {
	.i2s_ch = 0,
#if (1)
	.sample_rate = SNDRV_PCM_RATE_8000_192000,
	.pcm_format = SNDRV_PCM_FMTBIT_S16_LE,
#else
	.sample_rate = 48000,
	.pcm_format = SNDRV_PCM_FMTBIT_S16_LE,
#endif
};

static struct platform_device snd_null_dai = {
	.name = "snd-null-card",
	.id = -1,
	.dev = {
		.platform_data = &snd_null_dai_data,
	}
};

static struct platform_device snd_null_1 = {
	.name = "snd-null",
	.id = 1,
};

struct nxp_snd_dai_plat_data snd_null_dai_data_1 = {
	.i2s_ch = 1,
#if (1)
	.sample_rate = SNDRV_PCM_RATE_8000_192000,
	.pcm_format = SNDRV_PCM_FMTBIT_S16_LE,
#else
	.sample_rate = 48000,
	.pcm_format = SNDRV_PCM_FMTBIT_S16_LE,
#endif
};

static struct platform_device snd_null_dai_1 = {
	.name = "snd-null-card",
	.id = 1,
	.dev = {
		.platform_data = &snd_null_dai_data_1 ,
	}
};
#endif


/*
 * PDM
 */
#if defined(CONFIG_SND_NXP_EC_PDM_VIRT)
static struct nxp_pdm_plat_data pdm_virt_data = {
	.sample_rate = CFG_AUDIO_PDM_SAMPLE_RATE,
};

static struct platform_device pdm_virt_device = {
	.name	= "nxp-pdm-virt",
	.id		= -1,
	.dev    = {
		.platform_data	= &pdm_virt_data
	},
};

static struct platform_device pdm_virt_recorder = {
	.name	= "pdm-virt-dit-recorder",
	.id		= -1,
};

struct nxp_snd_dai_plat_data pdm_virt_rec_dai_data = {
	.sample_rate = 16000,
	.pcm_format	 = SNDRV_PCM_FMTBIT_S16_LE,
};

static struct platform_device pdm_virt_rec_dai = {
	.name	= "pdm-virt-recorder",
	.id		= -1,
	.dev	= {
		.platform_data	= &pdm_virt_rec_dai_data,
	}
};
#endif

#if defined(CONFIG_SND_NXP_EC_PDM_SPI)
static struct nxp_pdm_plat_data pdm_spi_data = {
	.sample_rate = CFG_AUDIO_PDM_SAMPLE_RATE,
	.channel = 2,
/* DMA */
	.dma_filter	= pl08x_filter_id,
	.dma_ch		= DMA_PERIPHERAL_NAME_SSP2_RX, // change spi dma ch name
};

static struct platform_device pdm_spi_device = {
	.name	= "nxp-pdm-spi",
	.id		= -1, // change spi ch
	.dev    = {
		.platform_data = &pdm_spi_data
	},
};

static struct platform_device pdm_spi_recorder = {
	.name	= "pdm-spi-dit-recorder",
	.id		= -1,
};

struct nxp_snd_dai_plat_data pdm_spi_rec_dai_data = {
	.sample_rate = 16000,
	.pcm_format	 = SNDRV_PCM_FMTBIT_S16_LE,
};

static struct platform_device pdm_spi_rec_dai = {
	.name	= "pdm-spi-recorder",
	.id		= -1,
	.dev	= {
		.platform_data	= &pdm_spi_rec_dai_data,
	}
};
#endif

/*------------------------------------------------------------------------------
 * DW MMC board config
 */
#if defined(CONFIG_MMC_DW)
#include <linux/mmc/dw_mmc.h>

int _dwmci_ext_cd_init(void (*notify_func)(struct platform_device *, int state))
{
	return 0;
}

int _dwmci_ext_cd_cleanup(void (*notify_func)(struct platform_device *, int state))
{
	return 0;
}

static int _dwmci_get_ro(u32 slot_id)
{
	return 0;
}

#ifdef CONFIG_MMC_NXP_CH0
static int _dwmci0_init(u32 slot_id, irq_handler_t handler, void *data)
{
	struct dw_mci *host = (struct dw_mci *)data;
	int io  = CFG_SDMMC0_DETECT_IO;
	int irq = IRQ_GPIO_START + io;
	int id  = 0, ret = 0;

	printk("dw_mmc dw_mmc.%d: Using external card detect irq %3d (io %2d)\n", id, irq, io);

	ret  = request_irq(irq, handler, IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
				DEV_NAME_SDHC "0", (void*)host->slot[slot_id]);
	if (0 > ret)
		pr_err("dw_mmc dw_mmc.%d: fail request interrupt %d ...\n", id, irq);
	return 0;
}

static int _dwmci_get_cd(u32 slot_id)
{
	return 0;
}
static int _dwmci0_get_cd(u32 slot_id)
{
	int io = CFG_SDMMC0_DETECT_IO;
	return nxp_soc_gpio_get_in_value(io);
}

static struct dw_mci_board _dwmci0_data = {
	.quirks			= DW_MCI_QUIRK_BROKEN_CARD_DETECTION |DW_MCI_QUIRK_HIGHSPEED,
	.bus_hz			= 80 * 1000 * 1000,
	.caps			= MMC_CAP_CMD23,
	.detect_delay_ms= 200,
	.clk_dly        = DW_MMC_DRIVE_DELAY(0) | DW_MMC_SAMPLE_DELAY(0) | DW_MMC_DRIVE_PHASE(2) | DW_MMC_SAMPLE_PHASE(1),
	.cd_type		= DW_MCI_CD_EXTERNAL,
	.init			= _dwmci0_init,
	.get_ro			= _dwmci_get_ro,
	.get_cd			= _dwmci0_get_cd,
	.ext_cd_init	= _dwmci_ext_cd_init,
	.ext_cd_cleanup	= _dwmci_ext_cd_cleanup,
#if defined (CONFIG_MMC_DW_IDMAC) && defined (CONFIG_MMC_NXP_CH0_USE_DMA)
    .mode       	= DMA_MODE,
#else
    .mode       	= PIO_MODE,
#endif

};
#endif

#ifdef CONFIG_MMC_NXP_CH1
static struct dw_mci_board _dwmci1_data = {
	.quirks			= DW_MCI_QUIRK_BROKEN_CARD_DETECTION |
					  DW_MCI_QUIRK_HIGHSPEED,
	.bus_hz			= 50 * 1000 * 1000,
	.caps			= MMC_CAP_CMD23,
	.desc_sz		= 4,
	.detect_delay_ms= 200,
	.clk_dly        = DW_MMC_DRIVE_DELAY(0) | DW_MMC_SAMPLE_DELAY(0) | DW_MMC_DRIVE_PHASE(2) | DW_MMC_SAMPLE_PHASE(0),
	.get_ro         = _dwmci_get_ro,
	.get_cd			= _dwmci_get_cd,
#if defined (CONFIG_MMC_DW_IDMAC) && defined (CONFIG_MMC_NXP_CH1_USE_DMA)
    .mode       	= DMA_MODE,
#else
    .mode       	= PIO_MODE,
#endif
};
#endif

#ifdef CONFIG_MMC_NXP_CH2
static struct dw_mci_board _dwmci2_data = {
	.quirks			= DW_MCI_QUIRK_BROKEN_CARD_DETECTION |
					  DW_MCI_QUIRK_HIGHSPEED,
	.bus_hz			= 50 * 1000 * 1000,
	.caps			=	MMC_CAP_CMD23,
	.desc_sz		= 4,
	.detect_delay_ms= 200,
	.clk_dly        = DW_MMC_DRIVE_DELAY(0) | DW_MMC_SAMPLE_DELAY(0) | DW_MMC_DRIVE_PHASE(2) | DW_MMC_SAMPLE_PHASE(0),
	.get_ro			= _dwmci_get_ro,
	.get_cd			= _dwmci_get_cd,
#if defined (CONFIG_MMC_DW_IDMAC) && defined (CONFIG_MMC_NXP_CH2_USE_DMA)
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
#define SOC_PA_EHCI		PHY_BASEADDR_EHCI
#define SOC_VA_EHCI		IO_ADDRESS(SOC_PA_EHCI)

int nxp_hsic_phy_pwr_on(struct platform_device *pdev, bool on)
{
	printk("++%s %d\n", __func__, on);

	if(on)
		writel(0x1 << 1, SOC_VA_EHCI + 0xB0);
	else
		writel(0x0 << 1, SOC_VA_EHCI + 0xB0);

	return 0;
}
EXPORT_SYMBOL(nxp_hsic_phy_pwr_on);

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

#define NXE2000_I2C_BUS		(2)
#define NXE2000_I2C_ADDR	(0x64 >> 1)

/* NXE2000 IRQs */
#define NXE2000_IRQ_BASE	(IRQ_SYSTEM_END)
#define NXE2000_GPIO_BASE	(ARCH_NR_GPIOS) //PLATFORM_NXE2000_GPIO_BASE
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
	REGULATOR_SUPPLY("vwifi_3.3V", NULL),
};
static struct regulator_consumer_supply nxe2000_ldo9_supply_0[] = {
	REGULATOR_SUPPLY("vhub_3.3V", NULL),
};
static struct regulator_consumer_supply nxe2000_ldo10_supply_0[] = {
	REGULATOR_SUPPLY("vhsic_1.2V", NULL),
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
NXE2000_PDATA_INIT(dc1,      0,  950000, 2000000, 1, 1, 1125000, 1,  4); /* 1.1V ARM */
NXE2000_PDATA_INIT(dc2,      0, 1000000, 2000000, 1, 1, 1100000, 1,  4); /* 1.0V CORE */
NXE2000_PDATA_INIT(dc3,      0, 1000000, 3500000, 1, 1, 3300000, 1,  0); /* 3.3V SYS */
NXE2000_PDATA_INIT(dc4,      0, 1000000, 2000000, 1, 1, 1500000, 1, -1); /* 1.5V DDR */
NXE2000_PDATA_INIT(dc5,      0, 1000000, 2000000, 1, 1, 1500000, 1,  4); /* 1.5V SYS */

NXE2000_PDATA_INIT(ldo1,     0, 1000000, 3500000, 1, 0, 3300000, 1,  0); /* 3.3V GPS */
NXE2000_PDATA_INIT(ldo2,     0, 1000000, 3500000, 0, 0, 1800000, 0,  0); /* 1.8V CAM1 */
NXE2000_PDATA_INIT(ldo3,     0, 1000000, 3500000, 1, 0, 1800000, 1,  2); /* 1.8V SYS1 */
NXE2000_PDATA_INIT(ldo4,     0, 1000000, 3500000, 1, 0, 1800000, 1,  2); /* 1.8V SYS */
NXE2000_PDATA_INIT(ldo5,     0, 1000000, 3500000, 0, 0, 2800000, 0,  0); /* 2.8V VCAM */
NXE2000_PDATA_INIT(ldo6,     0, 1000000, 3500000, 1, 0, 3300000, 1, -1); /* 3.3V ALIVE */
NXE2000_PDATA_INIT(ldo7,     0, 1000000, 3500000, 1, 0, 2800000, 1,  1); /* 2.8V VID */
#if defined(CONFIG_RFKILL_NXP)
NXE2000_PDATA_INIT(ldo8,     0, 1000000, 3500000, 0, 0, 3300000, 0,  0); /* 3.3V WIFI */
#else
NXE2000_PDATA_INIT(ldo8,     0, 1000000, 3500000, 0, 0, 3300000, 1,  0); /* 3.3V WIFI */
#endif
NXE2000_PDATA_INIT(ldo9,     0, 1000000, 3500000, 1, 0, 3300000, 1,  0); /* 3.3V HUB */
NXE2000_PDATA_INIT(ldo10,    0, 1000000, 3500000, 1, 0, 1200000, 0,  0); /* 1.2V HSIC */
NXE2000_PDATA_INIT(ldortc1,  0, 1700000, 3500000, 1, 0, 1800000, 1, -1); /* 1.8V ALIVE */
NXE2000_PDATA_INIT(ldortc2,  0, 1000000, 3500000, 1, 0, 1000000, 1, -1); /* 1.0V ALIVE */


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

#define NXE2000_BATTERY_REG	\
{	\
    .id		= -1,	\
    .name	= "nxe2000-battery",	\
    .platform_data	= &nxe2000_battery_data,	\
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


static struct nxe2000_battery_platform_data nxe2000_battery_data = {
	.irq 				= NXE2000_IRQ_BASE,

//	.input_power_type	= INPUT_POWER_TYPE_ADP_UBC_LINKED,
	.input_power_type	= INPUT_POWER_TYPE_UBC,

	.gpio_otg_usbid		= CFG_GPIO_OTG_USBID_DET,
	.gpio_otg_vbus		= CFG_GPIO_OTG_VBUS_DET,
	.gpio_pmic_vbus		= CFG_GPIO_PMIC_VUSB_DET,
	.gpio_pmic_lowbat	= CFG_GPIO_PMIC_LOWBAT_DET,

	.low_vbat_vol_mv	= 3600,
	.low_vsys_vol_mv	= 3600,
	.bat_impe			= 1500,
	.slp_ibat			= 10,
//	.adc_channel		= NXE2000_ADC_CHANNEL_VBAT,
	.multiple			= 100,	//100%
	.monitor_time		= 60,
		/* some parameter is depend of battery type */
	.type[0] = {
		.ch_vfchg		= 0x03,	/* VFCHG	= 0 - 4 (4.05v, 4.10v, 4.15v, 4.20v, 4.35v) */
		.ch_vrchg		= 0x03,	/* VRCHG	= 0 - 4 (3.85v, 3.90v, 3.95v, 4.00v, 4.10v) */
		.ch_vbatovset	= 0xFF,	/* VBATOVSET	= 0 or 1 (0 : 4.38v(up)/3.95v(down) 1: 4.53v(up)/4.10v(down)) */
		.ch_ichg 		= 0x0E,	/* ICHG		= 0 - 0x1D (100mA - 3000mA) */
		.ch_ichg_slp	= 0x0E,	/* SLEEP  ICHG	= 0 - 0x1D (100mA - 3000mA) */
		.ch_ilim_adp 	= 0x18,	/* ILIM_ADP	= 0 - 0x1D (100mA - 3000mA) */
		.ch_ilim_usb 	= 0x04,	/* ILIM_USB	= 0 - 0x1D (100mA - 3000mA) */
		.ch_icchg		= 0x00,	/* ICCHG	= 0 - 3 (50mA 100mA 150mA 200mA) */
		.fg_target_vsys	= 3250,	/* This value is the target one to DSOC=0% */
		.fg_target_ibat	= 1000,	/* This value is the target one to DSOC=0% */
		.fg_poff_vbat	= 3350,	/* setting value of 0 per Vbat */
		.jt_en			= 0,	/* JEITA Enable	  = 0 or 1 (1:enable, 0:disable) */
		.jt_hw_sw		= 1,	/* JEITA HW or SW = 0 or 1 (1:HardWare, 0:SoftWare) */
		.jt_temp_h		= 50,	/* degree C */
		.jt_temp_l		= 12,	/* degree C */
		.jt_vfchg_h 	= 0x03,	/* VFCHG High  	= 0 - 4 (4.05v, 4.10v, 4.15v, 4.20v, 4.35v) */
		.jt_vfchg_l 	= 0,	/* VFCHG Low  	= 0 - 4 (4.05v, 4.10v, 4.15v, 4.20v, 4.35v) */
		.jt_ichg_h		= 0x07,	/* ICHG High  	= 0 - 0x1D (100mA - 3000mA) */
		.jt_ichg_l		= 0x04,	/* ICHG Low   	= 0 - 0x1D (100mA - 3000mA) */
	},
	/*
	.type[1] = {
		.ch_vfchg		= 0x0,
		.ch_vrchg		= 0x0,
		.ch_vbatovset	= 0x0,
		.ch_ichg		= 0x0,
		.ch_ilim_adp	= 0x0,
		.ch_ilim_usb	= 0x0,
		.ch_icchg		= 0x00,
		.fg_target_vsys	= 3300,//3000,
		.fg_target_ibat	= 1000,//1000,
		.jt_en			= 0,
		.jt_hw_sw		= 1,
		.jt_temp_h		= 40,
		.jt_temp_l		= 10,
		.jt_vfchg_h		= 0x0,
		.jt_vfchg_l		= 0,
		.jt_ichg_h		= 0x01,
		.jt_ichg_l		= 0x01,
	},
	*/

/*  JEITA Parameter
*
*          VCHG
*            |
* jt_vfchg_h~+~~~~~~~~~~~~~~~~~~~+
*            |                   |
* jt_vfchg_l-| - - - - - - - - - +~~~~~~~~~~+
*            |    Charge area    +          |
*  -------0--+-------------------+----------+--- Temp
*            !                   +
*          ICHG
*            |                   +
*  jt_ichg_h-+ - -+~~~~~~~~~~~~~~+~~~~~~~~~~+
*            +    |              +          |
*  jt_ichg_l-+~~~~+   Charge area           |
*            |    +              +          |
*         0--+----+--------------+----------+--- Temp
*            0   jt_temp_l      jt_temp_h   55
*/
};



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
	NXE2000_BATTERY_REG,
	NXE2000_PWRKEY_REG,
#ifdef CONFIG_NXE2000_RTC
	NXE2000_RTC_REG,
#endif
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

/******************************************************************************/
/*------------------------------------------------------------------------------
 * SSP/SPI
 */
#if defined(CONFIG_SPI_SPIDEV) || defined(CONFIG_SPI_SPIDEV_MODULE)
#include <linux/spi/spi.h>

#if (CFG_SPI2_CS_GPIO_MODE)
static void spi2_cs(u32 chipselect)
{
	if(nxp_soc_gpio_get_io_func( CFG_SPI2_CS )!= nxp_soc_gpio_get_altnum( CFG_SPI2_CS))
		nxp_soc_gpio_set_io_func( CFG_SPI2_CS, nxp_soc_gpio_get_altnum( CFG_SPI2_CS));

	nxp_soc_gpio_set_io_dir( CFG_SPI2_CS,1);
	nxp_soc_gpio_set_out_value(	 CFG_SPI2_CS , chipselect);
}
#endif
struct pl022_config_chip spi2_info = {
    /* available POLLING_TRANSFER, INTERRUPT_TRANSFER, DMA_TRANSFER */
    .com_mode = CFG_SPI2_COM_MODE,
    .iface = SSP_INTERFACE_MOTOROLA_SPI,
    /* We can only act as master but SSP_SLAVE is possible in theory */
    .hierarchy = SSP_SLAVE,
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
#if (CFG_SPI2_CS_GPIO_MODE)
    .cs_control = spi2_cs,
#endif
	.clkdelay = SSP_FEEDBACK_CLK_DELAY_1T,

};

static struct spi_board_info spi_plat_board[] __initdata = {
    [0] = {
        .modalias        = "spidev",    /* fixup */
        .max_speed_hz    = 3125000,     /* max spi clock (SCK) speed in HZ */
        .bus_num         = 2,           /* Note> set bus num, must be smaller than ARRAY_SIZE(spi_plat_device) */
        .chip_select     = 0,           /* Note> set chip select num, must be smaller than spi cs_num */
        .controller_data = &spi2_info,
        .mode            = SPI_MODE_3 | SPI_CPOL | SPI_CPHA,
    },
};

#endif

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

#if defined(CONFIG_MMC_DW)
	#ifdef CONFIG_MMC_NXP_CH0
	nxp_mmc_add_device(0, &_dwmci0_data);
	#endif
	#ifdef CONFIG_MMC_NXP_CH1
	nxp_mmc_add_device(1, &_dwmci1_data);
	#endif
	#ifdef CONFIG_MMC_NXP_CH2
	nxp_mmc_add_device(2, &_dwmci2_data);
	#endif
#endif

#if defined(CONFIG_SND_NXP_EC)
    printk("plat: add device i2s (array:%d) \n", ARRAY_SIZE(i2s_devices));
    platform_add_devices(i2s_devices, ARRAY_SIZE(i2s_devices));
    printk("plat: add device null codec\n");
	platform_device_register(&snd_null);
	platform_device_register(&snd_null_dai);
	platform_device_register(&snd_null_1);
	platform_device_register(&snd_null_dai_1);
#if defined(CONFIG_SND_NXP_EC_PDM_VIRT)
	printk("plat: add device pdm-virt capture\n");
	platform_device_register(&pdm_virt_device);
	platform_device_register(&pdm_virt_recorder);
	platform_device_register(&pdm_virt_rec_dai);
#endif
#if defined(CONFIG_SND_NXP_EC_PDM_SPI)
	printk("plat: add device pdm-spi capture\n");
	platform_device_register(&pdm_spi_device);
	platform_device_register(&pdm_spi_recorder);
	platform_device_register(&pdm_spi_rec_dai);
#endif
#endif

#if defined(CONFIG_SND_CODEC_RT5631) || defined(CONFIG_SND_CODEC_RT5631_MODULE)
	printk("plat: add device asoc-rt5631\n");
	i2c_register_board_info(RT5631_I2C_BUS, &rt5631_i2c_bdi, 1);
	platform_device_register(&rt5631_dai);
#endif

#if defined(CONFIG_REGULATOR_NXE2000)
	printk("plat: add device nxe2000 pmic\n");
	i2c_register_board_info(NXE2000_I2C_BUS, nxe2000_i2c_boardinfo, ARRAY_SIZE(nxe2000_i2c_boardinfo));
#endif

#if defined(CONFIG_NXPMAC_ETH)
    printk("plat: add device nxp-gmac\n");
    platform_device_register(&nxp_gmac_dev);
#endif

#if defined(CONFIG_SPI_SPIDEV) || defined(CONFIG_SPI_SPIDEV_MODULE)
    spi_register_board_info(spi_plat_board, ARRAY_SIZE(spi_plat_board));
    printk("plat: register spidev\n");
#endif
	/* END */
	printk("\n");
}
