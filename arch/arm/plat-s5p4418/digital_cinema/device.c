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
	0x1,            // Port0
	0xF             // Port1
};

const u8 g_DrexBRB_WR[2] = {
	0x1,            // Port0
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
        BOTBUS_SI_SLOT_DEINTERLACE,
        BOTBUS_SI_SLOT_1ST_CODA,
        BOTBUS_SI_SLOT_2ND_ARM,
        BOTBUS_SI_SLOT_SCALER,
        BOTBUS_SI_SLOT_TOP,
        BOTBUS_SI_SLOT_2ND_CODA
};
#endif

#if (CFG_BUS_RECONFIG_BOTTOMBUSQOS == 1)
const u8 g_BottomQoSSI[2] = {
	1,      // Tidemark
	(1<<BOTBUS_SI_SLOT_1ST_ARM) |   // Control
		(1<<BOTBUS_SI_SLOT_2ND_ARM) |
		(1<<BOTBUS_SI_SLOT_MALI) |
		(1<<BOTBUS_SI_SLOT_TOP) |
		(1<<BOTBUS_SI_SLOT_DEINTERLACE) |
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
	.pll_dev	   	= CONFIG_NXP_CPUFREQ_PLLDEV,
	.supply_name	= "vdd_arm_1.25V",	//refer to CONFIG_REGULATOR_NXE2000
//	.max_cpufreq	= 1400*1000,
//	.max_retention  =   20*1000,
//	.rest_cpufreq   =  400*1000,
//	.rest_retention =    1*1000,
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
		.temp  = 50,
		.freq  = 1200000,
		.period = 1000, /* Ms */
	} , {
		.temp  = 55,
		.freq  = 1000000,
		.period = 1000, /* Ms */
	} , {
		.temp  = 60,
		.freq  = 800000,
		.period = 1000, /* Ms */
	} , {
		.temp  = 65,
		.freq  = 400000,		/* freq = 0 :Set critical temp. Power off! */
		.period = 1000, /* Ms */
	},
};

struct nxp_adc_tmp_platdata adc_tmp_plat_data ={
	.channel	= 2,
	.tmp_offset	= 0,
	.duration	= 1000,
	.step_up 	= 1,
	.event 		= adc_tmp_event,
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
 * Network DM9000
 */
#if defined(CONFIG_DM9000) || defined(CONFIG_DM9000_MODULE)
#include <linux/dm9000.h>

static struct resource dm9000_resource[] = {
	[0] = {
		.start	= CFG_ETHER_EXT_PHY_BASEADDR,
		.end	= CFG_ETHER_EXT_PHY_BASEADDR + 1,		// 1 (8/16 BIT)
		.flags	= IORESOURCE_MEM
	},
	[1] = {
		.start	= CFG_ETHER_EXT_PHY_BASEADDR + 4,		// + 4 (8/16 BIT)
		.end	= CFG_ETHER_EXT_PHY_BASEADDR + 5,		// + 5 (8/16 BIT)
		.flags	= IORESOURCE_MEM
	},
	[2] = {
		.start	= CFG_ETHER_EXT_IRQ_NUM,
		.end	= CFG_ETHER_EXT_IRQ_NUM,
		.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL,
	}
};

static struct dm9000_plat_data eth_plat_data = {
	.flags		= DM9000_PLATF_8BITONLY,	// DM9000_PLATF_16BITONLY
};

static struct platform_device dm9000_plat_device = {
	.name			= "dm9000",
	.id				= 0,
	.num_resources	= ARRAY_SIZE(dm9000_resource),
	.resource		= dm9000_resource,
	.dev			= {
		.platform_data	= &eth_plat_data,
	}
};
#endif	/* CONFIG_DM9000 || CONFIG_DM9000_MODULE */

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
    udelay(100);
    gpio_set_value(CFG_ETHER_GMAC_PHY_RST_NUM, 0 );
    udelay(100);
    gpio_set_value(CFG_ETHER_GMAC_PHY_RST_NUM, 1 );

    gpio_free(CFG_ETHER_GMAC_PHY_RST_NUM);

    printk("NXP mac init ..................\n");
    return 0;
}

int gmac_phy_reset(void *priv)
{
    // Set GPIO nReset
    gpio_set_value(CFG_ETHER_GMAC_PHY_RST_NUM, 1 );
    udelay(100);
    gpio_set_value(CFG_ETHER_GMAC_PHY_RST_NUM, 0 );
    udelay(100);
    gpio_set_value(CFG_ETHER_GMAC_PHY_RST_NUM, 1 );
    msleep(30);

    return 0;
}

static struct stmmac_mdio_bus_data nxpmac0_mdio_bus = {
    .phy_reset = gmac_phy_reset,
    .phy_mask = ~(1<<3),	// 0
#if defined (CONFIG_PLAT_S5P4418_DC_NAP)
    .probed_phy_irq = CFG_ETHER_GMAC_PHY_IRQ_NUM,
#endif
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
 * DISPLAY (LVDS) / FB
 */
#if defined (CONFIG_FB_NXP)
#if defined (CONFIG_FB0_NXP)
static struct nxp_fb_plat_data fb0_plat_data = {
	.module			= CONFIG_FB0_NXP_DISPOUT,
	.layer			= CFG_DISP_PRI_SCREEN_LAYER,
	.format			= CFG_DISP_PRI_SCREEN_RGB_FORMAT,
	.bgcolor		= CFG_DISP_PRI_BACK_GROUND_COLOR,
	.bitperpixel	= CFG_DISP_PRI_SCREEN_PIXEL_BYTE * 8,
	.x_resol		= CFG_DISP_PRI_RESOL_WIDTH,
	.y_resol		= CFG_DISP_PRI_RESOL_HEIGHT,
	#ifdef CONFIG_ANDROID
	.buffers		= 3,
	.skip_pan_vsync	= 1,
	#else
	.buffers		= 2,
	#endif
	.lcd_with_mm	= CFG_DISP_PRI_LCD_WIDTH_MM,	/* 152.4 */
	.lcd_height_mm	= CFG_DISP_PRI_LCD_HEIGHT_MM,	/* 91.44 */
};

static struct platform_device fb0_device = {
	.name	= DEV_NAME_FB,
	.id		= 0,	/* FB device node num */
	.dev    = {
		.coherent_dma_mask 	= 0xffffffffUL,	/* for DMA allocate */
		.platform_data		= &fb0_plat_data
	},
};
#endif

static struct platform_device *fb_devices[] = {
	#if defined (CONFIG_FB0_NXP)
	&fb0_device,
	#endif
};
#endif /* CONFIG_FB_NXP */

/*------------------------------------------------------------------------------
 * backlight : generic pwm device
 */
#if defined(CONFIG_BACKLIGHT_PWM)
#include <linux/pwm_backlight.h>

static struct platform_pwm_backlight_data bl_plat_data = {
    .pwm_id         = CFG_LCD_PRI_PWM_CH,
    .max_brightness = 255,  /* 255 is 100%, set over 100% */
    .dft_brightness = 128,  /* 50% */
    .lth_brightness = 75,   /* about to 5% */
    .pwm_period_ns  = 1000000000/CFG_LCD_PRI_PWM_FREQ,
};

static struct platform_device bl_plat_device = {
	.name	= "pwm-backlight",
	.id		= -1,
	.dev	= {
		.platform_data	= &bl_plat_data,
	},
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

#if defined(CONFIG_SAMSUNG_DIGITAL_CINEMA_FPGA)
#include <linux/i2c.h>
#define SAMSUNG_FPGA_I2C_BUS    (1)
/* FPGA */
static struct i2c_board_info __initdata samsung_fpga_i2c_bdi = {
    .type   = "ditigal_cinema_fpga",
    .addr   = (0x20>>1),
};
#endif

/*------------------------------------------------------------------------------
 * Keypad platform device
 */
#if defined(CONFIG_KEYBOARD_NXP_KEY) || defined(CONFIG_KEYBOARD_NXP_KEY_MODULE)

#include <linux/input.h>

static unsigned int  button_gpio[] = CFG_KEYPAD_KEY_BUTTON;
static unsigned int  button_code[] = CFG_KEYPAD_KEY_CODE;

struct nxp_key_plat_data key_plat_data = {
	.bt_count	= ARRAY_SIZE(button_gpio),
	.bt_io		= button_gpio,
	.bt_code	= button_code,
	.bt_repeat	= CFG_KEYPAD_REPEAT,
};

static struct platform_device key_plat_device = {
	.name	= DEV_NAME_KEYPAD,
	.id		= -1,
	.dev    = {
		.platform_data	= &key_plat_data
	},
};
#endif	/* CONFIG_KEYBOARD_NXP_KEY || CONFIG_KEYBOARD_NXP_KEY_MODULE */

/*------------------------------------------------------------------------------
 * ASoC Codec platform device
 */
#if defined(CONFIG_SND_CODEC_WM8976) || defined(CONFIG_SND_CODEC_WM8976_MODULE)
#include <linux/i2c.h>

#define	WM8976_I2C_BUS		(0)

/* CODEC */
static struct i2c_board_info __initdata wm8976_i2c_bdi = {
	.type	= "wm8978",			// compatilbe with wm8976
	.addr	= (0x34>>1),		// 0x1A (7BIT), 0x34(8BIT)
};

/* DAI */
struct nxp_snd_dai_plat_data i2s_dai_data = {
	.i2s_ch	= 0,
	.sample_rate	= 48000,
	.hp_jack 		= {
		.support    	= 1,
		.detect_io		= PAD_GPIO_E + 8,
		.detect_level	= 1,
	},
};

static struct platform_device wm8976_dai = {
	.name			= "wm8976-audio",
	.id				= 0,
	.dev			= {
		.platform_data	= &i2s_dai_data,
	}
};
#endif

#if defined(CONFIG_SND_CODEC_ALC5623)
#include <linux/i2c.h>

#define	WM8976_I2C_BUS		(0)

/* CODEC */
static struct i2c_board_info __initdata alc5623_i2c_bdi = {
	.type	= "alc562x-codec",			// compatilbe with wm8976
	.addr	= (0x34>>1),		// 0x1A (7BIT), 0x34(8BIT)
};

/* DAI */
struct nxp_snd_dai_plat_data i2s_dai_data = {
	.i2s_ch	= 0,
	.sample_rate	= 48000,
	.pcm_format	 = SNDRV_PCM_FMTBIT_S16_LE,
	.hp_jack 		= {
		.support    	= 1,
		.detect_io		= PAD_GPIO_E + 8,
		.detect_level	= 1,
	},
};

static struct platform_device alc5623_dai = {
	.name			= "alc5623-audio",
	.id				= 0,
	.dev			= {
		.platform_data	= &i2s_dai_data,
	}
};
#endif


#if defined(CONFIG_SND_SPDIF_TRANSCIEVER) || defined(CONFIG_SND_SPDIF_TRANSCIEVER_MODULE)
static struct platform_device spdif_transciever = {
	.name	= "spdif-dit",
	.id		= -1,
};

struct nxp_snd_dai_plat_data spdif_trans_dai_data = {
	.sample_rate = 48000,
	.pcm_format	 = SNDRV_PCM_FMTBIT_S16_LE,
};

static struct platform_device spdif_trans_dai = {
	.name	= "spdif-transciever",
	.id		= -1,
	.dev	= {
		.platform_data	= &spdif_trans_dai_data,
	}
};
#endif

#if defined(CONFIG_SND_CODEC_NULL)
static struct platform_device snd_null = {
    .name = "snd-null",
    .id = -1,
};

struct nxp_snd_dai_plat_data snd_null_dai_data = {
    .i2s_ch = 0,
#if defined(CONFIG_SND_NXP_DFS)
    .sample_rate = SNDRV_PCM_RATE_8000_192000,
    .pcm_format = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
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

static struct platform_device snd_null_2 = {
    .name = "snd-null",
    .id = 1,
};

struct nxp_snd_dai_plat_data snd_null_dai_data_2 = {
    .i2s_ch = 2,
#if defined(CONFIG_SND_NXP_DFS)
    .sample_rate = SNDRV_PCM_RATE_8000_192000,
    .pcm_format = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
#else
    .sample_rate = 48000,
    .pcm_format = SNDRV_PCM_FMTBIT_S16_LE,
#endif
};

static struct platform_device snd_null_dai_2 = {
    .name = "snd-null-card",
    .id = 2,
    .dev = {
        .platform_data = &snd_null_dai_data_2 ,
    }
};
#endif

/*------------------------------------------------------------------------------
 *  * reserve mem
 *   */
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

#if defined(CONFIG_I2C_NXP)
#define I2CUDELAY(x)	1000000/x

/* gpio i2c 3 */
#define	I2C3_SCL	(PAD_GPIO_C + 9)
#define	I2C3_SDA	(PAD_GPIO_C + 10)
static struct i2c_gpio_platform_data nxp_i2c_gpio_port3 = {
	.sda_pin	= I2C3_SDA,
	.scl_pin	= I2C3_SCL,
	.udelay		= I2CUDELAY(CFG_I2C3_CLK),				/* Gpio_mode CLK Rate = 1/(udelay*2) * 1000000 */
	.timeout	= 10,
};

static struct platform_device i2c_device_ch3 = {
	.name	= "i2c-gpio",
	.id		= 3,
	.dev    = {
		.platform_data	= &nxp_i2c_gpio_port3,
	},
};

/* gpio i2c 4 */
#define	I2C4_SCL	(PAD_GPIO_B + 31)
#define	I2C4_SDA	(PAD_GPIO_B + 30)
static struct i2c_gpio_platform_data nxp_i2c_gpio_port4 = {
    .sda_pin    = I2C4_SDA,
    .scl_pin    = I2C4_SCL,
    .udelay     = I2CUDELAY(CFG_I2C4_CLK),              /* Gpio_mode CLK Rate = 1/(udelay*2) * 1000000 */
    .timeout    = 10,
};

static struct platform_device i2c_device_ch4 = {
    .name   = "i2c-gpio",
    .id     = 4,
    .dev    = {
        .platform_data  = &nxp_i2c_gpio_port4,
    },
};

/* gpio i2c 5 */
#define	I2C5_SCL	(PAD_GPIO_A + 26)
#define	I2C5_SDA	(PAD_GPIO_A + 27)
static struct i2c_gpio_platform_data nxp_i2c_gpio_port5 = {
    .sda_pin    = I2C5_SDA,
    .scl_pin    = I2C5_SCL,
    .udelay     = I2CUDELAY(CFG_I2C5_CLK),              /* Gpio_mode CLK Rate = 1/(udelay*2) * 1000000 */
    .timeout    = 10,
};

static struct platform_device i2c_device_ch5 = {
    .name   = "i2c-gpio",
    .id     = 5,
    .dev    = {
        .platform_data  = &nxp_i2c_gpio_port5,
    },
};

/* gpio i2c 6 */
#define I2C6_SCL    (PAD_GPIO_D + 27)
#define I2C6_SDA    (PAD_GPIO_D + 26)
static struct i2c_gpio_platform_data nxp_i2c_gpio_port6 = {
    .sda_pin    = I2C6_SDA,
    .scl_pin    = I2C6_SCL,
    .udelay     = I2CUDELAY(CFG_I2C6_CLK),              /* Gpio_mode CLK Rate = 1/(udelay*2) * 4000000 */
    .timeout    = 10,
};

static struct platform_device i2c_device_ch6 = {
    .name   = "i2c-gpio",
    .id     = 6,
    .dev    = {
        .platform_data  = &nxp_i2c_gpio_port6,
    },
};

/* gpio i2c 7 */
#define I2C7_SCL    (PAD_GPIO_D + 29)
#define I2C7_SDA    (PAD_GPIO_D + 28)
static struct i2c_gpio_platform_data nxp_i2c_gpio_port7 = {
    .sda_pin    = I2C7_SDA,
    .scl_pin    = I2C7_SCL,
    .udelay     = I2CUDELAY(CFG_I2C7_CLK),              /* Gpio_mode CLK Rate = 1/(udelay*2) * 1000000 */
    .timeout    = 10,
};

static struct platform_device i2c_device_ch7 = {
    .name   = "i2c-gpio",
    .id     = 7,
    .dev    = {
        .platform_data  = &nxp_i2c_gpio_port7,
    },
};

static struct platform_device *i2c_devices[] = {
	&i2c_device_ch3,
#if defined (CONFIG_PLAT_S5P4418_DC_NAP)	
	&i2c_device_ch4,
	&i2c_device_ch5,
	&i2c_device_ch6,
	&i2c_device_ch7,
#endif
};
#endif /* CONFIG_I2C_NXP */

#if defined(CONFIG_TOUCHSCREEN_TSC2007)
#include <linux/i2c.h>
#include <linux/i2c/tsc2007.h>

#define TSC2007_I2C_BUS     (6)

static int tsc2007_get_pendown_state(void) {
    return !gpio_get_value(CFG_IO_TOUCH_PENDOWN_DETECT);
}
#define MAX_12BIT           ((1 << 12) - 1)
#define MIN_12BIT           (0)

struct tsc2007_platform_data tsc2007_plat_data = {
        #if 1
        .x_plate_ohms   = 100,
        .min_x = MIN_12BIT,
        .min_y = MIN_12BIT,
        .max_x = MAX_12BIT,
        .max_y = MAX_12BIT,
        .fuzzx = 0,
        .fuzzy = 0,
        .fuzzz = 0,
        .get_pendown_state = &tsc2007_get_pendown_state,
        #else
        .x_plate_ohms   = 10,
        .poll_period    = 10,
        #endif
    };

static struct i2c_board_info __initdata tsc2007_i2c_bdi = {
    .type   = "tsc2007",
    .addr   = (0x90>>1),
    .irq    = PB_PIO_IRQ(CFG_IO_TOUCH_PENDOWN_DETECT),
    .platform_data = &tsc2007_plat_data,
};
#endif

#if defined(CONFIG_RTC_DRV_PCF8563)
#include <linux/i2c.h>

#define PCF8563_I2C_BUS		(7)

static struct i2c_board_info __initdata pcf8563_i2c_bdi = {
    .type   = "pcf8563",
    .addr   = (0xA2>>1),
	.irq    = PB_PIO_IRQ(CFG_IO_RTC_INT),
};
#endif

#if defined(CONFIG_USB_HUB_GL852G)
#define GL852G_I2C_BUS     (4)

static struct i2c_board_info __initdata gl852g_i2c_bdi = {
    .type   = "gl852g",
    .addr   = (0x3E>>1),
};
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

#define NXE2000_I2C_BUS		(3)
#define NXE2000_I2C_ADDR	(0x64 >> 1)

/* NXE2000 IRQs */
#define NXE2000_IRQ_BASE	(IRQ_SYSTEM_END)
#define NXE2000_GPIO_BASE	(ARCH_NR_GPIOS) //PLATFORM_NXE2000_GPIO_BASE
#define NXE2000_GPIO_IRQ	(NXE2000_GPIO_BASE + 8)

//#define CONFIG_NXE2000_RTC

static struct regulator_consumer_supply nxe2000_dc1_supply_0[] = {
	REGULATOR_SUPPLY("vdd_arm_1.25V", NULL),
};
static struct regulator_consumer_supply nxe2000_dc2_supply_0[] = {
	REGULATOR_SUPPLY("vdd_core_1.0V", NULL),
};
static struct regulator_consumer_supply nxe2000_dc3_supply_0[] = {
	REGULATOR_SUPPLY("vdd_sys_3.3V", NULL),
};
static struct regulator_consumer_supply nxe2000_dc4_supply_0[] = {
	REGULATOR_SUPPLY("vdd_ddr_1.5V", NULL),
};
static struct regulator_consumer_supply nxe2000_dc5_supply_0[] = {
	REGULATOR_SUPPLY("vdd_sys_1.5V", NULL),
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
	REGULATOR_SUPPLY("vsys_1.8V", NULL),
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
NXE2000_PDATA_INIT(dc1,      0,	 950000, 2000000, 1, 1, 1250000, 1,  4);	/* 1.2V ARM */
NXE2000_PDATA_INIT(dc2,      0,	1000000, 2000000, 1, 1, 1100000, 1,  4);	/* 1.1V CORE */
NXE2000_PDATA_INIT(dc3,      0,	1000000, 3500000, 1, 1, 3300000, 1,  0);	/* 3.3V SYS */
NXE2000_PDATA_INIT(dc4,      0,	1000000, 2000000, 1, 1, 1500000, 1, -1);	/* 1.5V DDR */
NXE2000_PDATA_INIT(dc5,      0,	1000000, 2000000, 1, 1, 1500000, 1,  4);	/* 1.5V SYS */
#if defined(CONFIG_RFKILL_NXP)
NXE2000_PDATA_INIT(ldo1,     0,	1000000, 3500000, 0, 0, 3300000, 0,  0);	/* 3.3V GPS */
#else
NXE2000_PDATA_INIT(ldo1,     0,	1000000, 3500000, 0, 0,      -1, 0,  0);	/* Not used */
#endif
NXE2000_PDATA_INIT(ldo2,     0,	1000000, 3500000, 0, 0,      -1, 0,  0);	/* Not used */
NXE2000_PDATA_INIT(ldo3,     0,	1000000, 3500000, 1, 0, 1800000, 1,  2);	/* 1.8V SYS1 */
NXE2000_PDATA_INIT(ldo4,     0,	1000000, 3500000, 1, 0, 1800000, 1,  2);	/* 1.8V SYS */
NXE2000_PDATA_INIT(ldo5,     0,	1000000, 3500000, 0, 0,      -1, 0,  0);	/* Not used */
NXE2000_PDATA_INIT(ldo6,     0,	1000000, 3500000, 1, 0, 3300000, 1, -1);	/* 3.3V ALIVE */
NXE2000_PDATA_INIT(ldo7,     0,	1000000, 3500000, 0, 0,      -1, 0,  0);	/* Not used */
NXE2000_PDATA_INIT(ldo8,     0,	1000000, 3500000, 0, 0,      -1, 0,  0);	/* Not used */
NXE2000_PDATA_INIT(ldo9,     0,	1000000, 3500000, 0, 0,      -1, 0,  0);	/* Not used */
NXE2000_PDATA_INIT(ldo10,    0,	1000000, 3500000, 1, 1, 1000000, 1,  4);	/* 1.0V CORE */
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

	.input_power_type	= INPUT_POWER_TYPE_ADP_UBC,

	.gpio_otg_usbid		= CFG_GPIO_OTG_USBID_DET,
	.gpio_otg_vbus		= CFG_GPIO_OTG_VBUS_DET,
	.gpio_pmic_vbus		= CFG_GPIO_PMIC_VUSB_DET,
	.gpio_pmic_lowbat	= CFG_GPIO_PMIC_LOWBAT_DET,

	.low_vbat_vol_mv	= 3600,
	.low_vsys_vol_mv	= 3600,
	.bat_impe			= 1891,	//1500,
	.slp_ibat			= 10,	//3600,
	.multiple			= 100,	//100%
	.monitor_time		= 60,

	/* some parameter is depend of battery type */
	.type[0] = {
		.ch_vfchg		= 0x03,	/* VFCHG	= 0 - 4 (4.05v, 4.10v, 4.15v, 4.20v, 4.35v) */
		.ch_vrchg		= 0x03,	/* VRCHG	= 0 - 4 (3.85v, 3.90v, 3.95v, 4.00v, 4.10v) */
		.ch_vbatovset	= 0xFF,	/* VBATOVSET	= 0 or 1 (0 : 4.38v(up)/3.95v(down) 1: 4.53v(up)/4.10v(down)) */
		.ch_ichg 		= 0x07,	/* ICHG		= 0 - 0x1D (100mA - 3000mA) */
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

/*------------------------------------------------------------------------------
 * v4l2 platform device
 */
#if defined(CONFIG_V4L2_NXP) || defined(CONFIG_V4L2_NXP_MODULE)
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <mach/nxp-v4l2-platformdata.h>
#include <mach/soc.h>

static struct nxp_capture_platformdata capture_plat_data[] = {
    { 0, NULL, 0, },
};
/* out platformdata */
static struct i2c_board_info hdmi_edid_i2c_boardinfo = {
    I2C_BOARD_INFO("nxp_edid", 0xA0>>1),
};

static struct nxp_v4l2_i2c_board_info edid = {
    .board_info = &hdmi_edid_i2c_boardinfo,
    .i2c_adapter_id = 0,
};

static struct nxp_out_platformdata out_plat_data = {
    .hdmi = {
        .edid = &edid,
    },
};

static struct nxp_v4l2_platformdata v4l2_plat_data = {
    .captures = &capture_plat_data[0],
    .out = &out_plat_data,
};

static struct platform_device nxp_v4l2_dev = {
    .name       = NXP_V4L2_DEV_NAME,
    .id         = 0,
    .dev        = {
        .platform_data = &v4l2_plat_data,
    },
};
#endif /* CONFIG_V4L2_NXP || CONFIG_V4L2_NXP_MODULE */

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
        .modalias        = "spidev",    /* fixup */
        .max_speed_hz    = 3125000,     /* max spi clock (SCK) speed in HZ */
        .bus_num         = 0,           /* Note> set bus num, must be smaller than ARRAY_SIZE(spi_plat_device) */
        .chip_select     = 0,           /* Note> set chip select num, must be smaller than spi cs_num */
        .controller_data = &spi0_info,
        .mode            = SPI_MODE_3 | SPI_CPOL | SPI_CPHA,
    },
};

#endif
/*------------------------------------------------------------------------------
 * DW MMC board config
 */
#if defined(CONFIG_MMC_DW)

#ifdef CONFIG_MMC_NXP_CH0
static struct dw_mci_board _dwmci0_data = {
    .quirks			= DW_MCI_QUIRK_BROKEN_CARD_DETECTION |
				  	  DW_MCI_QUIRK_HIGHSPEED |
				  	  DW_MMC_QUIRK_HW_RESET_PW |
				      DW_MCI_QUIRK_NO_DETECT_EBIT,
	.bus_hz			= 100 * 1000 * 1000,
	.hs_over_clk    = 50 * 1000 * 1000,
	.caps			= MMC_CAP_UHS_DDR50 |
					  MMC_CAP_NONREMOVABLE |
			 	  	  MMC_CAP_4_BIT_DATA | MMC_CAP_CMD23 |
				  	  MMC_CAP_ERASE | MMC_CAP_HW_RESET,
	.clk_dly        = DW_MMC_DRIVE_DELAY(0) | DW_MMC_SAMPLE_DELAY(0) | DW_MMC_DRIVE_PHASE(3) | DW_MMC_SAMPLE_PHASE(1),
	.desc_sz		= 4,
	.detect_delay_ms= 200,
#if defined (CONFIG_MMC_DW_IDMAC) && defined (CONFIG_MMC_NXP_CH0_USE_DMA)
    .mode       	= DMA_MODE,
#else
    .mode       	= PIO_MODE,
#endif
};
#endif

#ifdef CONFIG_MMC_NXP_CH1
static struct dw_mci_board _dwmci1_data = {
	.quirks				= DW_MCI_QUIRK_HIGHSPEED,
	.bus_hz				= 100 * 1000 * 1000,
	.caps 				= MMC_CAP_CMD23 | MMC_CAP_NONREMOVABLE,
	.detect_delay_ms	= 200,
	.cd_type 			= DW_MCI_CD_NONE,
	.pm_caps        	= MMC_PM_KEEP_POWER | MMC_PM_IGNORE_PM_NOTIFY,
	.clk_dly        	= DW_MMC_DRIVE_DELAY(0) | DW_MMC_SAMPLE_DELAY(0) | DW_MMC_DRIVE_PHASE(0) | DW_MMC_SAMPLE_PHASE(0),
#if defined (CONFIG_MMC_DW_IDMAC) && defined (CONFIG_MMC_NXP_CH1_USE_DMA)
    .mode       	= DMA_MODE,
#else
    .mode       	= PIO_MODE,
#endif
};
#endif

#ifdef CONFIG_MMC_NXP_CH2
static int _dwmci_ext_cd_init(void (*notify_func)(struct platform_device *, int state))
{
	return 0;
}

static int _dwmci_ext_cd_cleanup(void (*notify_func)(struct platform_device *, int state))
{
	return 0;
}

static int _dwmci_get_ro(u32 slot_id)
{
	return 0;
}

static int _dwmci2_init(u32 slot_id, irq_handler_t handler, void *data)
{
	struct dw_mci *host = (struct dw_mci *)data;
	int io  = CFG_SDMMC2_DETECT_IO;
	int irq = IRQ_GPIO_START + io;
	int id  = 2, ret = 0;

	printk("dw_mmc dw_mmc.%d: Using external card detect irq %3d (io %2d)\n", id, irq, io);

	ret  = request_irq(irq, handler, IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
				DEV_NAME_SDHC "2", (void*)host->slot[slot_id]);
	if (0 > ret)
		pr_err("dw_mmc dw_mmc.%d: fail request interrupt %d ...\n", id, irq);
	return 0;
}

static int _dwmci2_get_cd(u32 slot_id)
{
	int io = CFG_SDMMC2_DETECT_IO;
	return nxp_soc_gpio_get_in_value(io);
}

static struct dw_mci_board _dwmci2_data = {
	.quirks			= DW_MCI_QUIRK_HIGHSPEED,
	.bus_hz			= 100 * 1000 * 1000,
	.caps			= MMC_CAP_CMD23,
	.detect_delay_ms= 200,
	.cd_type		= DW_MCI_CD_EXTERNAL,
	.clk_dly        = DW_MMC_DRIVE_DELAY(0) | DW_MMC_SAMPLE_DELAY(0) | DW_MMC_DRIVE_PHASE(2) | DW_MMC_SAMPLE_PHASE(1),
	.init			= _dwmci2_init,
	.get_ro         = _dwmci_get_ro,
	.get_cd			= _dwmci2_get_cd,
	.ext_cd_init	= _dwmci_ext_cd_init,
	.ext_cd_cleanup	= _dwmci_ext_cd_cleanup,
#if defined (CONFIG_MMC_DW_IDMAC) && defined (CONFIG_MMC_NXP_CH2_USE_DMA)
    .mode       	= DMA_MODE,
#else
    .mode       	= PIO_MODE,
#endif
};
#endif

#endif /* CONFIG_MMC_DW */

/*------------------------------------------------------------------------------
 * RFKILL driver
 */
#if defined(CONFIG_RFKILL_NXP)
struct rfkill_dev_data  rfkill_dev_data =
{
	.supply_name 	= "vgps_3.3V",	// vwifi_3.3V, vgps_3.3V
	.module_name 	= "wlan",
	.initval		= RFKILL_INIT_SET | RFKILL_INIT_OFF,
    .delay_time_off	= 1000,
};

struct nxp_rfkill_plat_data rfkill_plat_data = {
	.name		= "WiFi-Rfkill",
	.type		= RFKILL_TYPE_WLAN,
	.rf_dev		= &rfkill_dev_data,
    .rf_dev_num	= 1,
};

static struct platform_device rfkill_device = {
	.name			= DEV_NAME_RFKILL,
	.dev			= {
		.platform_data	= &rfkill_plat_data,
	}
};
#endif	/* CONFIG_RFKILL_NXP */

/*------------------------------------------------------------------------------
 * USB HSIC power control.
 */
int nxp_hsic_phy_pwr_on(struct platform_device *pdev, bool on)
{
	return 0;
}
EXPORT_SYMBOL(nxp_hsic_phy_pwr_on);

#ifdef CONFIG_BATTERY_NXE2000
/*------------------------------------------------------------------------------
 * USB OTGVBUS power control.
 */
void nxp_otgvbus_pwr_set(int enable)
{
	nxp_soc_gpio_set_out_value(CFG_GPIO_OTG_VBUS_DET, enable);
}
EXPORT_SYMBOL(nxp_otgvbus_pwr_set);
#endif
/*------------------------------------------------------------------------------
 * HDMI CEC driver
 */
#if defined(CONFIG_NXP_HDMI_CEC)
static struct platform_device hdmi_cec_device = {
	.name = NXP_HDMI_CEC_DRV_NAME,
};
#endif /* CONFIG_NXP_HDMI_CEC */

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

#if defined (CONFIG_FB_NXP)
	printk("plat: add framebuffer\n");
	platform_add_devices(fb_devices, ARRAY_SIZE(fb_devices));
#endif

#if defined(CONFIG_MMC_DW)
	#ifdef CONFIG_MMC_NXP_CH0
	nxp_mmc_add_device(0, &_dwmci0_data);
	#endif
    #ifdef CONFIG_MMC_NXP_CH2
	nxp_mmc_add_device(2, &_dwmci2_data);
	#endif
    #ifdef CONFIG_MMC_NXP_CH1
	nxp_mmc_add_device(1, &_dwmci1_data);
	#endif
#endif

#if defined(CONFIG_DM9000) || defined(CONFIG_DM9000_MODULE)
	printk("plat: add device dm9000 net\n");
	platform_device_register(&dm9000_plat_device);
#endif

#if defined(CONFIG_BACKLIGHT_PWM)
	printk("plat: add backlight pwm device\n");
	platform_device_register(&bl_plat_device);
#endif

#if defined(CONFIG_MTD_NAND_NXP) || defined(CONFIG_NXP_FTL)
	platform_device_register(&nand_plat_device);
#endif

#if defined(CONFIG_KEYBOARD_NXP_KEY) || defined(CONFIG_KEYBOARD_NXP_KEY_MODULE)
	printk("plat: add device keypad\n");
	platform_device_register(&key_plat_device);
#endif

#if defined(CONFIG_I2C_NXP)
	printk("plat: add device i2c_gpio\n");
    platform_add_devices(i2c_devices, ARRAY_SIZE(i2c_devices));
#endif

#if defined(CONFIG_REGULATOR_NXE2000)
	printk("plat: add device nxe2000 pmic\n");
	i2c_register_board_info(NXE2000_I2C_BUS, nxe2000_i2c_boardinfo, ARRAY_SIZE(nxe2000_i2c_boardinfo));
#endif

#if defined(CONFIG_SND_CODEC_WM8976) || defined(CONFIG_SND_CODEC_WM8976_MODULE)
	printk("plat: add device asoc-wm8976\n");
	i2c_register_board_info(WM8976_I2C_BUS, &wm8976_i2c_bdi, 1);
	platform_device_register(&wm8976_dai);
#endif

#if defined(CONFIG_SND_CODEC_ALC5623) || defined(CONFIG_SND_CODEC_ALC5623_MODULE)
	printk("plat: add device asoc-alc5623\n");
	i2c_register_board_info(0, &alc5623_i2c_bdi, 1);
	platform_device_register(&alc5623_dai);
#endif

#if defined(CONFIG_SND_SPDIF_TRANSCIEVER) || defined(CONFIG_SND_SPDIF_TRANSCIEVER_MODULE)
	printk("plat: add device spdif playback\n");
	platform_device_register(&spdif_transciever);
	platform_device_register(&spdif_trans_dai);
#endif

#if defined(CONFIG_SND_CODEC_NULL)
    platform_device_register(&snd_null);
    platform_device_register(&snd_null_dai);
    platform_device_register(&snd_null_2);
    platform_device_register(&snd_null_dai_2);
#endif

#if defined(CONFIG_V4L2_NXP) || defined(CONFIG_V4L2_NXP_MODULE)
    printk("plat: add device nxp-v4l2\n");
    platform_device_register(&nxp_v4l2_dev);
#endif

#if defined(CONFIG_SPI_SPIDEV) || defined(CONFIG_SPI_SPIDEV_MODULE)
    spi_register_board_info(spi_plat_board, ARRAY_SIZE(spi_plat_board));
    printk("plat: register spidev\n");
#endif

#if defined(CONFIG_TOUCHSCREEN_TSC2007)
    printk("plat: add touch(tsc2007) device\n");
    i2c_register_board_info(TSC2007_I2C_BUS, &tsc2007_i2c_bdi, 1);
#endif

#if defined(CONFIG_RTC_DRV_PCF8563)
    printk("plat: add rtc (pcf8563) device\n");
    i2c_register_board_info(PCF8563_I2C_BUS, &pcf8563_i2c_bdi, 1);
#endif

#if defined(CONFIG_SAMSUNG_DIGITAL_CINEMA_FPGA)
    printk("plat: add device digital_cinema-fpga\n");
    i2c_register_board_info(SAMSUNG_FPGA_I2C_BUS, &samsung_fpga_i2c_bdi, 1);
#endif

#if defined(CONFIG_RFKILL_NXP)
    printk("plat: add device rfkill\n");
    platform_device_register(&rfkill_device);
#endif

#if defined(CONFIG_NXPMAC_ETH)
    printk("plat: add device nxp-gmac\n");
    platform_device_register(&nxp_gmac_dev);
#endif

#if defined(CONFIG_NXP_HDMI_CEC)
    printk("plat: add device hdmi-cec\n");
    platform_device_register(&hdmi_cec_device);
#endif

#if defined(CONFIG_USB_HUB_GL852G)
    printk("plat: add usb hub (gl852g) device\n");
    i2c_register_board_info(GL852G_I2C_BUS, &gl852g_i2c_bdi, 1);
#endif

	/* END */
	printk("\n");
}
