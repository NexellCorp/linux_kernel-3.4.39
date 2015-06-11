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
#include <mach/s5p6818_bus.h>

const u16 g_DrexQoS[2] = {
	0x100,		// S0
	0xFFF		// S1, Default value
};

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

const u8 g_BottomQoSSI[2] = {
	1,	// Tidemark
	(1<<BOTBUS_SI_SLOT_1ST_ARM) |	// Control
	(1<<BOTBUS_SI_SLOT_2ND_ARM) |
	(1<<BOTBUS_SI_SLOT_MALI) |
	(1<<BOTBUS_SI_SLOT_TOP) |
	(1<<BOTBUS_SI_SLOT_DEINTERLACE) |
	(1<<BOTBUS_SI_SLOT_1ST_CODA)
};

const u8 g_DispBusSI[3] = {
	DISBUS_SI_SLOT_1ST_DISPLAY,
	DISBUS_SI_SLOT_2ND_DISPLAY,
	DISBUS_SI_SLOT_2ND_DISPLAY  //DISBUS_SI_SLOT_GMAC
};
#endif	/* #if (CFG_BUS_RECONFIG_ENB == 1) */

/*------------------------------------------------------------------------------
 * CPU Frequence
 */
#if defined(CONFIG_ARM_NXP_CPUFREQ)

static unsigned long dfs_freq_table[][2] = {
	{ 1600000, 1340000, },
	{ 1500000, 1280000, },
	{ 1400000, 1240000, },
	{ 1300000, 1180000, },
	{ 1200000, 1140000, },
	{ 1100000, 1100000, },
	{ 1000000, 1060000, },
	{  900000, 1040000, },
	{  800000, 1000000, },
	{  700000,  940000, },
	{  600000,  940000, },
	{  500000,  940000, },
	{  400000,  940000, },
};

struct nxp_cpufreq_plat_data dfs_plat_data = {
	.pll_dev	   	= CONFIG_NXP_CPUFREQ_PLLDEV,
	.supply_name	= "vdd_arm_1.3V",
	.supply_delay_us = 0,
	.freq_table	   	= dfs_freq_table,
	.table_size	   	= ARRAY_SIZE(dfs_freq_table),
};

static struct platform_device dfs_plat_device = {
	.name			= DEV_NAME_CPUFREQ,
	.dev			= {
		.platform_data	= &dfs_plat_data,
	}
};

#endif

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
	.pwm_id			= CFG_LCD_PRI_PWM_CH,
	.max_brightness = 255,//	/* 255 is 100%, set over 100% */
	.dft_brightness = 100,//	/* 99% */
	.pwm_period_ns	= 1000000000/CFG_LCD_PRI_PWM_FREQ,
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
#endif	/* CONFIG_MTD_NAND_NXP */

#if defined(CONFIG_TOUCHSCREEN_GSLX680)
#include <linux/i2c.h>
#define	GSLX680_I2C_BUS		(1)

static struct i2c_board_info __initdata gslX680_i2c_bdi = {
	.type	= "gslX680",
	.addr	= (0x40),
    	.irq    = PB_PIO_IRQ(CFG_IO_TOUCH_PENDOWN_DETECT),
};
#endif


#if defined(CONFIG_TOUCHSCREEN_TSC2007)
#include <linux/i2c.h>
#include <linux/i2c/tsc2007.h>

#define	TSC2007_I2C_BUS		(2)

struct tsc2007_platform_data tsc2007_plat_data = {
		//.touch_points	= 10,
		//.x_resol	   	= CFG_DISP_PRI_RESOL_WIDTH,
		//.y_resol	   	= CFG_DISP_PRI_RESOL_HEIGHT,
		//.rotate			= 90,
		.x_plate_ohms	= 1000,
		//.max_rt			= 1400,//1200,
	};

static struct i2c_board_info __initdata tsc2007_i2c_bdi = {
	.type	= "tsc2007",
	.addr	= (0x90>>1),
    .irq    = PB_PIO_IRQ(CFG_IO_TOUCH_PENDOWN_DETECT),
    .platform_data = &tsc2007_plat_data,
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

#if defined(CONFIG_SND_CODEC_RT5631) || defined(CONFIG_SND_CODEC_RT5631_MODULE)
#include <linux/i2c.h>

#define	RT5631_I2C_BUS		(0)

/* CODEC */
static struct i2c_board_info __initdata rt5631_i2c_bdi = {
	.type	= "rt5631",
	.addr	= (0x34>>1),		// 0x1A (7BIT), 0x34(8BIT)
};

/* DAI */
struct nxp_snd_dai_plat_data rt5631_i2s_dai_data = {
	.i2s_ch	= 0,
	.sample_rate	= 48000,
	.pcm_format = SNDRV_PCM_FMTBIT_S16_LE,
	.hp_jack 		= {
		.support    	= 1,
		.detect_io		= CFG_IO_HP_DET,
		.detect_level	= 0,
	},
};

static struct platform_device rt5631_dai = {
	.name			= "rt5631-audio",
	.id				= 0,
	.dev			= {
		.platform_data	= &rt5631_i2s_dai_data,
	}
};
#endif

#if defined(CONFIG_SND_CODEC_ALC5623)
#include <linux/i2c.h>

#define	ALC5623_I2C_BUS		(3)

/* CODEC */
static struct i2c_board_info __initdata alc5623_i2c_bdi = {
	.type	= "alc562x-codec",			// compatilbe with wm8976
	.addr	= (0x34>>1),		// 0x1A (7BIT), 0x34(8BIT)
};

/* DAI */
struct nxp_snd_dai_plat_data alc5623_i2s_dai_data = {
	.i2s_ch	= 1,
	.sample_rate	= 48000,
	.pcm_format	 = SNDRV_PCM_FMTBIT_S16_LE,
/*
	.hp_jack 		= {
		.support    	= 1,
		.detect_io		= PAD_GPIO_E + 8,
		.detect_level	= 1,
	},
*/
};

static struct platform_device alc5623_dai = {
	.name			= "alc5623-audio",
	.id				= 0,
	.dev			= {
		.platform_data	= &alc5623_i2s_dai_data,
	}
};
#endif

#if defined(CONFIG_SND_CODEC_RT5631_FDONE) || defined(CONFIG_SND_CODEC_RT5631_FDONE_MODULE)
#include <linux/i2c.h>

#define	RT5631_I2C_BUS		(0)

/* CODEC */
static struct i2c_board_info __initdata rt5631_i2c_bdi = {
	.type	= "rt5631",
	.addr	= (0x34>>1),		// 0x1A (7BIT), 0x34(8BIT)
};

/* DAI */
struct nxp_snd_dai_plat_data rt5631_i2s_dai_data = {
	.i2s_ch	= 0,
	.sample_rate	= 48000,
	.pcm_format = SNDRV_PCM_FMTBIT_S16_LE,
#if 0
	.hp_jack 		= {
		.support    	= 1,
		.detect_io		= PAD_GPIO_A + 0,
		.detect_level	= 1,
	},
#endif
};

static struct platform_device rt5631_dai = {
	.name			= "rt5631-audio",
	.id				= 0,
	.dev			= {
		.platform_data	= &rt5631_i2s_dai_data,
	}
};
#endif

#if defined(CONFIG_SND_CODEC_RT5623_FDONE)
#include <linux/i2c.h>

#define	RT5623_I2C_BUS		(3)

/* CODEC */
static struct i2c_board_info __initdata rt5623_i2c_bdi = {
	.type	= "rt5623",			// compatilbe with wm8976
	.addr	= (0x34>>1),		// 0x1A (7BIT), 0x34(8BIT)
};

/* DAI */
struct nxp_snd_dai_plat_data rt5623_i2s_dai_data = {
	.i2s_ch	= 1,
	.sample_rate	= 48000,
	.pcm_format	 = SNDRV_PCM_FMTBIT_S16_LE,
/*
	.hp_jack 		= {
		.support    	= 1,
		.detect_io		= PAD_GPIO_E + 8,
		.detect_level	= 1,
	},
*/
};

static struct platform_device rt5623_dai = {
	.name			= "rt5623-audio",
	.id				= 0,
	.dev			= {
		.platform_data	= &rt5623_i2s_dai_data,
	}
};
#endif


/*------------------------------------------------------------------------------
 * G-Sensor platform device
 */
#if defined(CONFIG_SENSORS_MMA865X) || defined(CONFIG_SENSORS_MMA865X_MODULE)
#include <linux/i2c.h>

#define	MMA865X_I2C_BUS		(2)

/* CODEC */
static struct i2c_board_info __initdata mma865x_i2c_bdi = {
	.type	= "mma8653",
	.addr	= 0x1D//(0x3a),
};

#endif

#if defined(CONFIG_SENSORS_STK831X) || defined(CONFIG_SENSORS_STK831X_MODULE)
#include <linux/i2c.h>

#define	STK831X_I2C_BUS		(2)

/* CODEC */
static struct i2c_board_info __initdata stk831x_i2c_bdi = {
#if   defined CONFIG_SENSORS_STK8312
	.type	= "stk8312",
	.addr	= (0x3d),
#elif defined CONFIG_SENSORS_STK8313
	.type	= "stk8313",
	.addr	= (0x22),
#endif
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
#ifdef CONFIG_ION_NXP_RESERVEHEAP_SIZE
        {
            .name = "ion-reserve",
            .size = CONFIG_ION_NXP_RESERVEHEAP_SIZE * SZ_1K,
            {
                .alignment = PAGE_SIZE,
            }
        },
#endif
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
        "ion-nxp/ion-reserve=ion-reserve;"
        "ion-nxp/ion-nxp=ion;"
        "nx_vpu=ion;";

#ifdef CONFIG_ION_NXP_RESERVEHEAP_SIZE
    printk("%s: reserve CMA: size %d\n", __func__, CONFIG_ION_NXP_RESERVEHEAP_SIZE * SZ_1K);
#endif

#ifdef CONFIG_ION_NXP_CONTIGHEAP_SIZE
    printk("%s: reserve CMA: size %d\n", __func__, CONFIG_ION_NXP_CONTIGHEAP_SIZE * SZ_1K);
#endif

    nxp_cma_region_reserve(regions, map);
}
#endif

#if defined(CONFIG_I2C_NXP) || defined (CONFIG_I2C_SLSI)
#define I2CUDELAY(x)	1000000/x/2

/* GPIO I2C3 : CODEC2 */
#define	I2C3_SDA	PAD_GPIO_C + 9
#define	I2C3_SCL	PAD_GPIO_C + 10
/* GPIO I2C4 : DEC2 */
#define	I2C4_SDA	PAD_GPIO_C + 12
#define	I2C4_SCL	PAD_GPIO_C + 11

#if defined(CONFIG_USB_HUB_USB2514)
/* GPIO I2C5 : USB HUB */
#define	I2C5_SDA	PAD_GPIO_C + 5
#define	I2C5_SCL	PAD_GPIO_C + 6
#define	I2C5_CLK	CFG_I2C3_CLK
#endif

/* GPIO I2C6 : TDMB */
#define	I2C6_SDA	PAD_GPIO_C + 7
#define	I2C6_SCL	PAD_GPIO_C + 8
#define	I2C6_CLK	CFG_I2C3_CLK

/* GPIO I2C7 : PMIC VDDA */
#define	I2C7_SDA	PAD_GPIO_E + 8
#define	I2C7_SCL	PAD_GPIO_E + 9
#define	I2C7_CLK	CFG_I2C3_CLK

/* GPIO I2C8 : PMIC VDDB */
#define	I2C8_SDA	PAD_GPIO_E + 10
#define	I2C8_SCL	PAD_GPIO_E + 11
#define	I2C8_CLK	CFG_I2C3_CLK

static struct i2c_gpio_platform_data nxp_i2c_gpio_port3 = {
	.sda_pin	= I2C3_SDA,
	.scl_pin	= I2C3_SCL,
	.udelay		= I2CUDELAY(CFG_I2C3_CLK),				/* Gpio_mode CLK Rate = 1/( udelay*2) * 1000000 */
	.timeout	= 10,
};

static struct i2c_gpio_platform_data nxp_i2c_gpio_port4 = {
	.sda_pin	= I2C4_SDA,
	.scl_pin	= I2C4_SCL,
	.udelay		= I2CUDELAY(CFG_I2C4_CLK),				/* Gpio_mode CLK Rate = 1/( udelay*2) * 1000000 */
	.timeout	= 10,
};

#if defined(CONFIG_USB_HUB_USB2514)
static struct i2c_gpio_platform_data nxp_i2c_gpio_port5 = {
	.sda_pin	= I2C5_SDA,
	.scl_pin	= I2C5_SCL,
	.udelay		= I2CUDELAY(I2C5_CLK),				/* Gpio_mode CLK Rate = 1/( udelay*2) * 1000000 */
	.timeout	= 10,
};
#endif

static struct i2c_gpio_platform_data nxp_i2c_gpio_port6 = {
	.sda_pin	= I2C6_SDA,
	.scl_pin	= I2C6_SCL,
	.udelay		= I2CUDELAY(I2C6_CLK),				/* Gpio_mode CLK Rate = 1/( udelay*2) * 1000000 */
	.timeout	= 10,
};

static struct i2c_gpio_platform_data nxp_i2c_gpio_port7 = {
	.sda_pin	= I2C8_SDA,
	.scl_pin	= I2C8_SCL,
	.udelay		= I2CUDELAY(I2C7_CLK),				/* Gpio_mode CLK Rate = 1/( udelay*2) * 1000000 */
	.timeout	= 10,
};

static struct i2c_gpio_platform_data nxp_i2c_gpio_port8 = {
	.sda_pin	= I2C7_SDA,
	.scl_pin	= I2C7_SCL,
	.udelay		= I2CUDELAY(I2C8_CLK),				/* Gpio_mode CLK Rate = 1/( udelay*2) * 1000000 */
	.timeout	= 10,
};

static struct platform_device i2c_device_ch3 = {
	.name	= "i2c-gpio",
	.id		= 3,
	.dev    = {
		.platform_data	= &nxp_i2c_gpio_port3,
	},
};

static struct platform_device i2c_device_ch4 = {
	.name	= "i2c-gpio",
	.id		= 4,
	.dev    = {
		.platform_data	= &nxp_i2c_gpio_port4,
	},
};

#if defined(CONFIG_USB_HUB_USB2514)
static struct platform_device i2c_device_ch5 = {
	.name	= "i2c-gpio",
	.id		= 5,
	.dev    = {
		.platform_data	= &nxp_i2c_gpio_port5,
	},
};
#endif

static struct platform_device i2c_device_ch6 = {
	.name	= "i2c-gpio",
	.id		= 6,
	.dev    = {
		.platform_data	= &nxp_i2c_gpio_port6,
	},
};

static struct platform_device i2c_device_ch7 = {
	.name	= "i2c-gpio",
	.id		= 7,
	.dev    = {
		.platform_data	= &nxp_i2c_gpio_port7,
	},
};

static struct platform_device i2c_device_ch8 = {
	.name	= "i2c-gpio",
	.id		= 8,
	.dev    = {
		.platform_data	= &nxp_i2c_gpio_port8,
	},
};

static struct platform_device *i2c_devices[] = {
	&i2c_device_ch3,
	&i2c_device_ch4,
#if defined(CONFIG_USB_HUB_USB2514)
	&i2c_device_ch5,
#endif
	&i2c_device_ch6,
	&i2c_device_ch7,
	&i2c_device_ch8,
};
#endif /* CONFIG_I2C_NXP || CONFIG_I2C_SLSI */

/*------------------------------------------------------------------------------
 * USB HUB platform device
 */
#if defined(CONFIG_USB_HUB_USB2514)
#define	USB2514_I2C_BUS		(5)

static struct i2c_board_info __initdata usb2514_i2c_bdi = {
	.type	= "usb2514",
	.addr	= 0x58>>1,
};
#endif /* CONFIG_USB_HUB_USB2514 */

/*------------------------------------------------------------------------------
 * v4l2 platform device
 */
#if defined(CONFIG_V4L2_NXP) || defined(CONFIG_V4L2_NXP_MODULE)
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <mach/nxp-v4l2-platformdata.h>
#include <mach/soc.h>

#ifdef CONFIG_VIDEO_TW9992
static bool is_front_camera_enabled = false;
static bool is_front_camera_power_state_changed = false;

static int front_camera_power_enable(bool on);

static void front_vin_setup_io(int module, bool force)
{
    /* do nothing */
	return;
}

static int front_phy_enable(bool en)
{
    return 0;
}

static int front_camera_set_clock(ulong clk_rate)
{
    /* do nothing */
    return 0;
}

static int front_camera_power_enable(bool on)
{
    unsigned int reset_io = (PAD_GPIO_ALV + 0);

    printk("%s: is_front_camera_enabled %d, on %d\n", __func__, is_front_camera_enabled, on);

    if (on)
	{
        if (!is_front_camera_enabled)
		{
	        nxp_soc_gpio_set_io_func(reset_io, nxp_soc_gpio_get_altnum(reset_io));
	        nxp_soc_gpio_set_io_dir(reset_io, 1);
	        mdelay(1);
	        nxp_soc_gpio_set_out_value(reset_io, 0);
	        mdelay(10);
	        nxp_soc_gpio_set_out_value(reset_io, 1);
	        mdelay(10);
            is_front_camera_enabled = true;
            is_front_camera_power_state_changed = true;
        } else {
            is_front_camera_power_state_changed = false;
        }
    }
#if 0
    else
    {
        if (is_front_camera_enabled) {
            nxp_soc_gpio_set_out_value(reset_io, 0);
            is_front_camera_enabled = false;
            is_front_camera_power_state_changed = true;
        }
    }
#endif

    return 0;
}

static bool front_camera_power_state_changed(void)
{
    return is_front_camera_power_state_changed;
}

#define FRONT_CAM_WIDTH  720
#define FRONT_CAM_HEIGHT 480

struct nxp_mipi_csi_platformdata front_plat_data = {
    .module     = 0,
    .clk_rate   = 27000000, // 27MHz
    .lanes      = 1,
    .alignment = 0,
    .hs_settle  = 0,
    .width      = FRONT_CAM_WIDTH,
    .height     = FRONT_CAM_HEIGHT,
    .fixed_phy_vdd = false,
    .irq        = 0, /* not used */
    .base       = 0, /* not used */
    .phy_enable = front_phy_enable,
};
#endif /* CONFIG_VIDEO_TW9992 */

static struct i2c_board_info back_camera_i2c_boardinfo[] = {
    {
        I2C_BOARD_INFO("tw9900", 0x8a>>1),
    },
};

static struct i2c_board_info front_camera_i2c_boardinfo[] = {
    {
        I2C_BOARD_INFO("tw9992", 0x7a>>1),
    },
};

static struct nxp_v4l2_i2c_board_info sensor[] = {
    {
        .board_info = &back_camera_i2c_boardinfo[0],
        .i2c_adapter_id = 1,
    },
    {
        .board_info = &front_camera_i2c_boardinfo[0],
        .i2c_adapter_id = 4,
    },
};

static struct nxp_capture_platformdata capture_plat_data[] = {
#ifdef CONFIG_VIDEO_TW9900
    {
        .module = 2,
        .sensor = &sensor[0],
        .type = NXP_CAPTURE_INF_PARALLEL,
        .parallel = {
            .is_mipi        = false,
            .external_sync  = false,
            .h_active       = 720,
            .h_frontporch   = 7,
            .h_syncwidth    = 1,
            .h_backporch    = 10,
            .v_active       = 480,
            .v_frontporch   = 0,
            .v_syncwidth    = 2,
            .v_backporch    = 3,
            .clock_invert   = false,
            .port           = 0,
            .data_order     = 0,
            .interlace      = true,
            .clk_rate       = 24000000,
            .late_power_down = false,
        },
    },
#endif
#ifdef CONFIG_VIDEO_TW9992
    {
        .module = 0,
        .sensor = &sensor[1],
        .type = NXP_CAPTURE_INF_CSI,
        .parallel = {
            .is_mipi        = true,
            .external_sync  = true,
            .h_active       = FRONT_CAM_WIDTH,
            .h_frontporch   = 4,
            .h_syncwidth    = 4,
            .h_backporch    = 4,
            .v_active       = FRONT_CAM_HEIGHT,
            .v_frontporch   = 1,
            .v_syncwidth    = 1,
            .v_backporch    = 1,
            .clock_invert   = false,
            .port           = NX_VIP_INPUTPORT_B,
            .data_order     = NXP_VIN_CBY0CRY1,
            .interlace      = false,
            .clk_rate       = 24000000,
            .late_power_down = true,
            .power_enable   = front_camera_power_enable,
            .power_state_changed = front_camera_power_state_changed,
            .set_clock      = front_camera_set_clock,
            .setup_io       = front_vin_setup_io,
        },
        .deci = {
            .start_delay_ms = 0,
            .stop_delay_ms  = 0,
        },
		.csi = &front_plat_data,
    },
#endif /* CONFIG_VIDEO_TW9992 */
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

static struct i2c_board_info hdmi_hdcp_i2c_boardinfo = {
    I2C_BOARD_INFO("nxp_hdcp", 0x74>>1),
};

static struct nxp_v4l2_i2c_board_info hdcp = {
    .board_info = &hdmi_hdcp_i2c_boardinfo,
    .i2c_adapter_id = 0,
};


static void hdmi_set_int_external(int gpio)
{
    nxp_soc_gpio_set_int_enable(gpio, 0);
    nxp_soc_gpio_set_int_mode(gpio, 1); /* high level */
    nxp_soc_gpio_set_int_enable(gpio, 1);
    nxp_soc_gpio_clr_int_pend(gpio);
}

static void hdmi_set_int_internal(int gpio)
{
    nxp_soc_gpio_set_int_enable(gpio, 0);
    nxp_soc_gpio_set_int_mode(gpio, 0); /* low level */
    nxp_soc_gpio_set_int_enable(gpio, 1);
    nxp_soc_gpio_clr_int_pend(gpio);
}

static int hdmi_read_hpd_gpio(int gpio)
{
    return nxp_soc_gpio_get_in_value(gpio);
}

static struct nxp_out_platformdata out_plat_data = {
    .hdmi = {
        .internal_irq = 0,
        .external_irq = 0,//PAD_GPIO_A + 19,
        .set_int_external = hdmi_set_int_external,
        .set_int_internal = hdmi_set_int_internal,
        .read_hpd_gpio = hdmi_read_hpd_gpio,
        .edid = &edid,
        .hdcp = &hdcp,
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
#include <mach/slsi-spi.h>
#include <linux/spi/spi.h>
#include <linux/gpio.h>

static struct s3c64xx_spi_csinfo spi0_csi[] = {
    [0] = {
        .line       = CFG_SPI0_CS,
        .set_level  = gpio_set_value,
        //.fb_delay   = 0x2,
    },
};

struct spi_board_info spi0_board_info[] __initdata = {
    {
        //.modalias       = "spidev",
        .modalias       = "INC_SPI",
        .platform_data  = NULL,
        //.max_speed_hz   = 10 * 1000 * 1000,
        .max_speed_hz   = 4 * 1000 * 1000,
        .bus_num        = 0,
        .chip_select    = 0,
        .mode           = SPI_MODE_0,
        .controller_data    = &spi0_csi[0],
    }
};

static struct s3c64xx_spi_csinfo spi1_csi[] = {
    [0] = {
        .line       = CFG_SPI1_CS,
        .set_level  = gpio_set_value,
        .fb_delay   = 0x2,
    },
};
struct spi_board_info spi1_board_info[] __initdata = {
    {
        .modalias       = "xr20m1172", // "spidev",
        .platform_data  = NULL,
        .max_speed_hz   = 8 * 1000 * 1000,
        .bus_num        = 1,
        .chip_select    = 0,
        .mode           = SPI_MODE_0,
        .controller_data    = &spi1_csi[0],
    }
};

static struct s3c64xx_spi_csinfo spi2_csi[] = {
    [0] = {
        .line       = CFG_SPI2_CS,
        .set_level  = gpio_set_value,
        .fb_delay   = 0x2,
    },
};
struct spi_board_info spi2_board_info[] __initdata = {
    {
        .modalias       = "spidev",
        .platform_data  = NULL,
        .max_speed_hz   = 10 * 1000 * 1000,
        .bus_num        = 2,
        .chip_select    = 0,
        .mode           = SPI_MODE_0,
        .controller_data    = &spi2_csi[0],
    }
};
#endif

/*------------------------------------------------------------------------------
 * DW MMC board config
 */

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
static int _dwmci0_get_cd(u32 slot_id)
{
	int io = CFG_SDMMC0_DETECT_IO;
	return nxp_soc_gpio_get_in_value(io);
}

#ifdef CONFIG_MMC_NXP_CH0
static struct dw_mci_board _dwmci0_data = {
	.quirks			= DW_MCI_QUIRK_HIGHSPEED,
	.bus_hz			= 100 * 1000 * 1000,
	.caps			= MMC_CAP_CMD23,
	.detect_delay_ms= 200,
	.cd_type		= DW_MCI_CD_EXTERNAL,
	.clk_dly        = DW_MMC_DRIVE_DELAY(0) | DW_MMC_SAMPLE_DELAY(0) | DW_MMC_DRIVE_PHASE(2) | DW_MMC_SAMPLE_PHASE(1),
	.init			= _dwmci0_init,
	.get_ro         = _dwmci_get_ro,
	.get_cd			= _dwmci0_get_cd,
	.ext_cd_init	= _dwmci_ext_cd_init,
	.ext_cd_cleanup	= _dwmci_ext_cd_cleanup,
};
#endif

#ifdef CONFIG_MMC_NXP_CH2
static struct dw_mci_board _dwmci2_data = {
    .quirks			= DW_MCI_QUIRK_BROKEN_CARD_DETECTION |
				  	  DW_MCI_QUIRK_HIGHSPEED |
				  	  DW_MMC_QUIRK_HW_RESET_PW |
				      DW_MCI_QUIRK_NO_DETECT_EBIT,
	.bus_hz			= 200 * 1000 * 1000, /*200*/
	.caps			= MMC_CAP_UHS_DDR50 | MMC_CAP_1_8V_DDR |
					  MMC_CAP_NONREMOVABLE |
			 	  	  MMC_CAP_8_BIT_DATA | MMC_CAP_CMD23 |
				  	  MMC_CAP_ERASE | MMC_CAP_HW_RESET,
	//.clk_dly        = DW_MMC_DRIVE_DELAY(0) | DW_MMC_SAMPLE_DELAY(0x0) | DW_MMC_DRIVE_PHASE(2) | DW_MMC_SAMPLE_PHASE(1),
	.clk_dly        = DW_MMC_DRIVE_DELAY(0x0) | DW_MMC_SAMPLE_DELAY(0x0) | DW_MMC_DRIVE_PHASE(3) | DW_MMC_SAMPLE_PHASE(1),

	.desc_sz		= 4,
	.detect_delay_ms= 200,
	.sdr_timing		= 0x01010001,
	.ddr_timing		= 0x03030002,
};
#endif

/*------------------------------------------------------------------------------
 * RFKILL driver
 */
#if defined(CONFIG_NXP_RFKILL)

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

#if defined(CONFIG_REGULATOR_MP8845C)
#include <linux/i2c.h>
#include <linux/regulator/machine.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/regulator/fixed.h>
#include <linux/regulator/mp8845c-regulator.h>

#define MP8845C_PDATA_INIT(_name, _sname, _minuv, _maxuv, _always_on, _boot_on, _init_uv, _init_enable, _slp_slots) \
	static struct mp8845c_regulator_platform_data pdata_##_name##_##_sname = \
	{									\
		.regulator = {					\
			.constraints = {			\
				.min_uV		= _minuv,	\
				.max_uV		= _maxuv,	\
				.valid_modes_mask	= (REGULATOR_MODE_NORMAL |	\
									REGULATOR_MODE_STANDBY),	\
				.valid_ops_mask		= (REGULATOR_CHANGE_MODE |	\
									REGULATOR_CHANGE_STATUS |	\
									REGULATOR_CHANGE_VOLTAGE),	\
				.always_on	= _always_on,	\
				.boot_on	= _boot_on,		\
				.apply_uV	= 1,				\
			},								\
			.num_consumer_supplies =		\
				ARRAY_SIZE(mp8845c_##_name##_##_sname),			\
			.consumer_supplies	= mp8845c_##_name##_##_sname, 	\
			.supply_regulator	= 0,			\
		},									\
		.init_uV		= _init_uv,			\
		.init_enable	= _init_enable,		\
		.sleep_slots	= _slp_slots,		\
	}

#define MP8845C_REGULATOR(_dev_id, _name, _sname)	\
{												\
	.id		= MP8845C_##_dev_id##_VOUT,				\
	.name	= "mp8845c-regulator",				\
	.platform_data	= &pdata_##_name##_##_sname,\
}

#define I2C_FLATFORM_INFO(dev_type, dev_addr, dev_data)\
{										\
	.type = dev_type, 					\
	.addr = (dev_addr),					\
	.platform_data = dev_data,			\
}

static struct regulator_consumer_supply mp8845c_vout_0[] = {
	REGULATOR_SUPPLY("vdd_arm_1.3V", NULL),
};

static struct regulator_consumer_supply mp8845c_vout_1[] = {
	REGULATOR_SUPPLY("vdd_core_1.2V", NULL),
};

MP8845C_PDATA_INIT(vout, 0, 600000, 1500000, 1, 1, 1200000, 1, -1);	/* ARM */
MP8845C_PDATA_INIT(vout, 1, 600000, 1500000, 1, 1, 1200000, 1, -1);	/* CORE */

static struct mp8845c_platform_data __initdata mp8845c_platform[] = {
	MP8845C_REGULATOR(0, vout, 0),
	MP8845C_REGULATOR(1, vout, 1),
};

#define MP8845C_I2C_BUS0		(7)
#define MP8845C_I2C_BUS1		(8)
#define MP8845C_I2C_ADDR		(0x1c)

static struct i2c_board_info __initdata mp8845c_regulators[] = {
	I2C_FLATFORM_INFO("mp8845c", MP8845C_I2C_ADDR, &mp8845c_platform[0]),
	I2C_FLATFORM_INFO("mp8845c", MP8845C_I2C_ADDR, &mp8845c_platform[1]),
};
#endif  /* CONFIG_REGULATOR_MP8845C */


/*------------------------------------------------------------------------------
 * HDMI CEC driver
 */
#if defined(CONFIG_NXP_HDMI_CEC)
static struct platform_device hdmi_cec_device = {
	.name			= NXP_HDMI_CEC_DRV_NAME,
};
#endif /* CONFIG_NXP_HDMI_CEC */

/*------------------------------------------------------------------------------
 * Backward Camera driver
 */
#if defined(CONFIG_SLSIAP_BACKWARD_CAMERA)
#include <mach/nxp-backward-camera.h>

static struct reg_val _sensor_init_data[] =
{
    {0x02, 0x44},		// MUX0 :  0x40,		MUX1 :  0x44
    {0x03, 0xa2},
    {0x07, 0x02},
    {0x08, 0x12},
    {0x09, 0xf0},
    {0x0a, 0x1c},
    /*{0x0b, 0xd0}, // 720 */
    {0x0b, 0xc0}, // 704
    {0x1b, 0x00},
    {0x10, 0xfa},
    {0x11, 0x64},
    {0x2f, 0xe6},
    {0x55, 0x00},
#if 1
    /*{0xb1, 0x20},*/
    /*{0xb1, 0x02},*/
    {0xaf, 0x00},
    {0xb1, 0x20},
    {0xb4, 0x20},
    /*{0x06, 0x80},*/
#endif
    /*{0xaf, 0x40},*/
    /*{0xaf, 0x00},*/
    /*{0xaf, 0x80},*/
    END_MARKER
};

#define CAMERA_RESET        ((PAD_GPIO_D + 23) | PAD_FUNC_ALT1)
static int _sensor_power_enable(bool enable)
{
    u32 reset_io = CAMERA_RESET;

    if (enable) {
        // reset to high
        nxp_soc_gpio_set_out_value(reset_io & 0xff, 1);
        nxp_soc_gpio_set_io_dir(reset_io & 0xff, 1);
        nxp_soc_gpio_set_io_func(reset_io & 0xff, nxp_soc_gpio_get_altnum(reset_io));
        mdelay(1);

        // reset to low
        nxp_soc_gpio_set_out_value(reset_io & 0xff, 0);
        mdelay(10);

        // reset to high
        nxp_soc_gpio_set_out_value(reset_io & 0xff, 1);
        mdelay(10);
    }

    return 0;
}

static void _sensor_setup_io(void)
{
    u_int *pad;
    int i, len;
    u_int io, fn;

    /* VIP0:0 = VCLK, VID0 ~ 7 */
    const u_int port[][2] = {
        /* VCLK, HSYNC, VSYNC */
        { PAD_GPIO_E +  4, NX_GPIO_PADFUNC_1 },
        { PAD_GPIO_E +  5, NX_GPIO_PADFUNC_1 },
        { PAD_GPIO_E +  6, NX_GPIO_PADFUNC_1 },
        /* DATA */
        { PAD_GPIO_D + 28, NX_GPIO_PADFUNC_1 }, { PAD_GPIO_D + 29, NX_GPIO_PADFUNC_1 },
        { PAD_GPIO_D + 30, NX_GPIO_PADFUNC_1 }, { PAD_GPIO_D + 31, NX_GPIO_PADFUNC_1 },
        { PAD_GPIO_E +  0, NX_GPIO_PADFUNC_1 }, { PAD_GPIO_E +  1, NX_GPIO_PADFUNC_1 },
        { PAD_GPIO_E +  2, NX_GPIO_PADFUNC_1 }, { PAD_GPIO_E +  3, NX_GPIO_PADFUNC_1 },
    };

    pad = (u_int *)port;
    len = sizeof(port)/sizeof(port[0]);

    for (i = 0; i < len; i++) {
        io = *pad++;
        fn = *pad++;
        nxp_soc_gpio_set_io_dir(io, 0);
        nxp_soc_gpio_set_io_func(io, fn);
    }
}

// This is callback function for rgb overlay drawing
static void _draw_rgb_overlay(struct nxp_backward_camera_platform_data *plat_data, void *mem)
{
    printk("%s entered\n", __func__);
    memset(mem, 0, plat_data->width*plat_data->height*4);
    /* draw redbox at (0, 0) -- (50, 50) */
    {
        u32 color = 0xFFFF0000; // red
        int i, j;
        u32 *pbuffer = (u32 *)mem;
        for (i = 0; i < 50; i++) {
            for (j = 0; j < 50; j++) {
                pbuffer[i * 1024 + j] = color;
            }
        }
    }
    printk("%s exit\n", __func__);
}

#define BACKWARD_CAM_WIDTH  704
#define BACKWARD_CAM_HEIGHT 480

static struct nxp_backward_camera_platform_data backward_camera_plat_data = {
    .backgear_irq_num   = CFG_BACKGEAR_IRQ_NUM,
    .backgear_gpio_num  = CFG_BACKGEAR_GPIO_NUM,
    .active_high        = false,
    .vip_module_num     = 2,
    .mlc_module_num     = 0,

    // sensor
    .i2c_bus            = 1,
    .chip_addr          = 0x8a >> 1,
    .reg_val            = _sensor_init_data,
    .power_enable       = _sensor_power_enable,
    .set_clock          = NULL,
    .setup_io           = _sensor_setup_io,

    // vip
    .port               = 0,
    .external_sync      = false,
    .is_mipi            = false,
    .h_active           = BACKWARD_CAM_WIDTH,
    .h_frontporch       = 7,
    .h_syncwidth        = 1,
    .h_backporch        = 10,
    .v_active           = BACKWARD_CAM_HEIGHT,
    .v_frontporch       = 0,
    .v_syncwidth        = 2,
    .v_backporch        = 3,
    .data_order         = 0,
    .interlace          = true,

    .lu_addr            = 0,
    .cb_addr            = 0,
    .cr_addr            = 0,

    .lu_stride          = BACKWARD_CAM_WIDTH,
    .cb_stride          = 384,
    .cr_stride          = 384,

    .rgb_format         = MLC_RGBFMT_A8R8G8B8,
    .width              = 1024,
    .height             = 600,
    .rgb_addr           = 0,
    .draw_rgb_overlay   = _draw_rgb_overlay,
};

static struct platform_device backward_camera_device = {
    .name           = "nxp-backward-camera",
    .dev			= {
        .platform_data	= &backward_camera_plat_data,
    }
};

#ifdef CONFIG_SLSIAP_FINEBOOT
extern void register_backward_camera(struct platform_device *device);
#endif
#endif

/*------------------------------------------------------------------------------
 * SLsiAP Thermal Unit
 */
#if defined(CONFIG_SENSORS_NXP_TMU)

struct nxp_tmu_trigger tmu_triggers[] = {
	{
		.trig_degree	=  85,
		.trig_duration	=  100,
		.trig_cpufreq	=  800*1000,	/* Khz */
	},
};

static struct nxp_tmu_platdata tmu_data = {
	.channel  = 0,
	.triggers = tmu_triggers,
	.trigger_size = ARRAY_SIZE(tmu_triggers),
	.poll_duration = 100,
};

static struct platform_device tmu_device = {
	.name			= "nxp-tmu",
	.dev			= {
		.platform_data	= &tmu_data,
	}
};
#endif

/*------------------------------------------------------------------------------
 * register board platform devices
 */
void __init nxp_board_devs_register(void)
{
	printk("[Register board platform devices]\n");

#if defined(CONFIG_ARM_NXP_CPUFREQ)
	printk("plat: add dynamic frequency (pll.%d)\n", dfs_plat_data.pll_dev);
	platform_device_register(&dfs_plat_device);
#endif

#if defined(CONFIG_SENSORS_NXP_TMU)
	printk("plat: add device TMU\n");
	platform_device_register(&tmu_device);
#endif

#if defined (CONFIG_FB_NXP)
	printk("plat: add framebuffer\n");
	platform_add_devices(fb_devices, ARRAY_SIZE(fb_devices));
#endif

#if defined(CONFIG_MMC_DW)
	#ifdef CONFIG_MMC_NXP_CH2
	nxp_mmc_add_device(2, &_dwmci2_data);
	#endif
	#ifdef CONFIG_MMC_NXP_CH0
	nxp_mmc_add_device(0, &_dwmci0_data);
	#endif
    //#ifdef CONFIG_MMC_NXP_CH1
	//nxp_mmc_add_device(1, &_dwmci1_data);
	//#endif
#endif

#if defined(CONFIG_DM9000) || defined(CONFIG_DM9000_MODULE)
	printk("plat: add device dm9000 net\n");
	platform_device_register(&dm9000_plat_device);
#endif

#if defined(CONFIG_BACKLIGHT_PWM)
	printk("plat: add backlight pwm device\n");
	platform_device_register(&bl_plat_device);
#endif

#if defined(CONFIG_MTD_NAND_NXP)
	platform_device_register(&nand_plat_device);
#endif

#if defined(CONFIG_KEYBOARD_NXP_KEY) || defined(CONFIG_KEYBOARD_NXP_KEY_MODULE)
	printk("plat: add device keypad\n");
	platform_device_register(&key_plat_device);
#endif

#if defined(CONFIG_I2C_NXP)
    platform_add_devices(i2c_devices, ARRAY_SIZE(i2c_devices));
#endif

#if defined(CONFIG_REGULATOR_MP8845C)
	printk("plat: add device mp8845c ARM\n");
	i2c_register_board_info(MP8845C_I2C_BUS0, &mp8845c_regulators[0], 1);

	printk("plat: add device mp8845c CORE\n");
	i2c_register_board_info(MP8845C_I2C_BUS1, &mp8845c_regulators[1], 1);
#endif

#if defined(CONFIG_SND_CODEC_RT5631) || defined(CONFIG_SND_CODEC_RT5631_MODULE)
	printk("plat: add device asoc-rt5631\n");
	i2c_register_board_info(RT5631_I2C_BUS, &rt5631_i2c_bdi, 1);
	platform_device_register(&rt5631_dai);
#endif

#if defined(CONFIG_SND_CODEC_ALC5623) || defined(CONFIG_SND_CODEC_ALC5623_MODULE)
	printk("plat: add device asoc-alc5623\n");
	i2c_register_board_info(ALC5623_I2C_BUS, &alc5623_i2c_bdi, 1);
	platform_device_register(&alc5623_dai);
#endif

#if defined(CONFIG_SND_CODEC_RT5631_FDONE) || defined(CONFIG_SND_CODEC_RT5631_FDONE_MODULE)
	printk("plat: add device asoc-rt5631\n");
	i2c_register_board_info(RT5631_I2C_BUS, &rt5631_i2c_bdi, 1);
	platform_device_register(&rt5631_dai);
#endif

#if defined(CONFIG_SND_CODEC_RT5623_FDONE) || defined(CONFIG_SND_CODEC_RT5623_FDONE_MODULE)
	printk("plat: add device asoc-rt5623\n");
	i2c_register_board_info(RT5623_I2C_BUS, &rt5623_i2c_bdi, 1);
	platform_device_register(&rt5623_dai);
#endif

#if defined(CONFIG_V4L2_NXP) || defined(CONFIG_V4L2_NXP_MODULE)
    printk("plat: add device nxp-v4l2\n");
    platform_device_register(&nxp_v4l2_dev);
#endif

#if defined(CONFIG_SPI_SPIDEV) || defined(CONFIG_SPI_SPIDEV_MODULE)
 	spi_register_board_info(spi0_board_info, ARRAY_SIZE(spi0_board_info));
	spi_register_board_info(spi1_board_info, ARRAY_SIZE(spi1_board_info));
    printk("plat: register spidev\n");
#endif

#if defined(CONFIG_TOUCHSCREEN_GSLX680)
	printk("plat: add touch(gslX680) device\n");
	i2c_register_board_info(GSLX680_I2C_BUS, &gslX680_i2c_bdi, 1);
#endif


#if defined(CONFIG_TOUCHSCREEN_TSC2007)
	printk("plat: add touch(tsc2007) device\n");
	i2c_register_board_info(TSC2007_I2C_BUS, &tsc2007_i2c_bdi, 1);
#endif


#if defined(CONFIG_SENSORS_MMA865X) || defined(CONFIG_SENSORS_MMA865X_MODULE)
	printk("plat: add g-sensor mma865x\n");
	i2c_register_board_info(2, &mma865x_i2c_bdi, 1);
#elif defined(CONFIG_SENSORS_MMA7660) || defined(CONFIG_SENSORS_MMA7660_MODULE)
	printk("plat: add g-sensor mma7660\n");
	i2c_register_board_info(MMA7660_I2C_BUS, &mma7660_i2c_bdi, 1);
#endif

#if defined(CONFIG_RFKILL_NXP)
    printk("plat: add device rfkill\n");
    platform_device_register(&rfkill_device);
#endif

#if defined(CONFIG_NXP_HDMI_CEC)
    printk("plat: add device hdmi-cec\n");
    platform_device_register(&hdmi_cec_device);
#endif

#if defined(CONFIG_SLSIAP_BACKWARD_CAMERA)
    printk("plat: register device backward-camera platform device to fine-boot\n");
#ifdef CONFIG_SLSIAP_FINEBOOT
    register_backward_camera(&backward_camera_device);
#else
    platform_device_register(&backward_camera_device);
#endif
#endif

#if defined(CONFIG_USB_HUB_USB2514)
	printk("plat: add device usb2514\n");
	i2c_register_board_info(USB2514_I2C_BUS, &usb2514_i2c_bdi, 1);
#endif

	/* END */
	printk("\n");
}
