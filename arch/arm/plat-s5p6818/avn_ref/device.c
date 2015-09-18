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






/*------------------------------------------------------------------------------
 * CPU Frequence
 */
#if defined(CONFIG_ARM_NXP_CPUFREQ)

static unsigned long dfs_freq_table[][2] = {
//	{ 1400000, 1300000 },
//	{ 1300000, 1300000 },
	{ 1200000, 1200000, },
	{ 1100000, 1200000, },
	{ 1000000, 1100000, },
	{  900000, 1100000, },
	{  800000, 1100000, },
	{  700000, 1000000, },
	{  666000, 1000000, },
	{  600000, 1000000, },
	{  533000, 1000000, },
	{  500000, 1000000, },
	{  400000, 1000000, },
};

struct nxp_cpufreq_plat_data dfs_plat_data = {
	.pll_dev	   	= CONFIG_NXP_CPUFREQ_PLLDEV,
	.supply_name	= "vdd_arm_1.3V",	//refer to CONFIG_REGULATOR_NXE2000
	.supply_delay_us = 0,
	.freq_table	   	= dfs_freq_table,
	.table_size	   	= ARRAY_SIZE(dfs_freq_table),
	.max_cpufreq    = 1200*1000,
	.max_retention  =   20*1000,
	.rest_cpufreq   =  400*1000,
	.rest_retention =    1*1000,
};

static struct platform_device dfs_plat_device = {
	.name			= DEV_NAME_CPUFREQ,
	.dev			= {
		.platform_data	= &dfs_plat_data,
	}
};
#endif

#if defined(CONFIG_SENSORS_NXP_TMU)
struct nxp_tmu_trigger tmu_triggers[] = {
       {
               .trig_degree    =  85,
               .trig_duration  =  10,
               .trig_cpufreq   =  400*1000,    /* Khz */
       },
};
static struct nxp_tmu_platdata tmu_data = {
       .channel  = 0,
       .triggers = tmu_triggers,
       .trigger_size = ARRAY_SIZE(tmu_triggers),
       .poll_duration = 100,
};
static struct platform_device tmu_device = {
       .name                   = "nxp-tmu",
			 .dev			= {
					.platform_data  = &tmu_data,
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
	.max_brightness = 350,	/* 255 is 100%, set over 100% */
	.dft_brightness = 128,	/* 50% */
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
#if defined(CONFIG_MTD_NAND_NEXELL)
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
#endif	/* CONFIG_MTD_NAND_NEXELL */


#if defined(CONFIG_TOUCHSCREEN_TSC2007)
#include <linux/i2c.h>
#include <linux/i2c/tsc2007.h>

#define TSC2007_I2C_BUS     (2)

static int tsc2007_get_pendown_state(struct device *dev){
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

#if defined(CONFIG_SND_CODEC_RT5640)
#include <linux/i2c.h>

#define	RT5640_I2C_BUS		(1)

/* CODEC */
static struct i2c_board_info __initdata rt5640_i2c_bdi = {
	.type	= "rt5640",			// compatilbe with wm8976
	.addr	= (0x38>>1),		// 0x1A (7BIT), 0x34(8BIT)
};

/* DAI */
struct nxp_snd_dai_plat_data rt5640_i2s_dai_data = {
	.i2s_ch	= 0,
	.sample_rate	= 48000,
#if 0
   	.hp_jack 		= {
		.support    	= 1,
		.detect_io		= PAD_GPIO_E + 8,
		.detect_level	= 1,
	},
#endif	
};

static struct platform_device rt5640_dai = {
	.name			= "rt5640-audio",
	.id				= 0,
	.dev			= {
		.platform_data	= &rt5640_i2s_dai_data,
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
#if 0
   	.hp_jack 		= {
		.support    	= 1,
		.detect_io		= PAD_GPIO_E + 8,
		.detect_level	= 1,
	},
#endif	
};

static struct platform_device alc5623_dai = {
	.name			= "alc5623-audio",
	.id				= 1,
	.dev			= {
		.platform_data	= &alc5623_i2s_dai_data,
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

#if defined(CONFIG_I2C_NXP) || defined (CONFIG_I2C_SLSI)
#define I2CUDELAY(x)	1000000/x

/* gpio i2c 3 */ 
#define	I2C3_SCL	PAD_GPIO_B + 18 
#define	I2C3_SDA	PAD_GPIO_B + 16
#define I2C3_CLK    CFG_I2C3_CLK

/* GPIO I2C4 : USB HUB */
#if defined(CONFIG_USB_HUB_USB2514)
#define	I2C4_SCL	PAD_GPIO_C + 25
#define	I2C4_SDA	PAD_GPIO_C + 27
#define I2C4_CLK    CFG_I2C4_CLK
#endif

#define	I2C5_SCL	PAD_GPIO_D + 22
#define	I2C5_SDA	PAD_GPIO_D + 23
#define I2C5_CLK    CFG_I2C7_CLK

#define	I2C6_SCL	PAD_GPIO_D + 26
#define	I2C6_SDA	PAD_GPIO_D + 27
#define I2C6_CLK    CFG_I2C8_CLK

/* GPIO I2C7: PMIC VDDA */
#define I2C7_SDA    PAD_GPIO_E + 8
#define I2C7_SCL    PAD_GPIO_E + 9
#define I2C7_CLK    CFG_I2C7_CLK

/* GPIO I2C8 : PMIC VDDB */
#define I2C8_SDA    PAD_GPIO_E + 10
#define I2C8_SCL    PAD_GPIO_E + 11
#define I2C8_CLK    CFG_I2C8_CLK


static struct i2c_gpio_platform_data nxp_i2c_gpio_port3 = {
	.sda_pin	= I2C3_SDA,
	.scl_pin	= I2C3_SCL,
	.udelay		= I2CUDELAY(CFG_I2C3_CLK),				/* Gpio_mode CLK Rate = 1/( udelay*2) * 1000000 */

	.timeout	= 10,
};

#if defined(CONFIG_USB_HUB_USB2514)
static struct i2c_gpio_platform_data nxp_i2c_gpio_port4 = {
    .sda_pin    = I2C4_SDA,
    .scl_pin    = I2C4_SCL,
    .udelay     = I2CUDELAY(I2C4_CLK),              /* Gpio_mode CLK Rate = 1/( udelay*2) * 1000000 */
    .timeout    = 10,
};
#endif

static struct i2c_gpio_platform_data nxp_i2c_gpio_port5 = {
    .sda_pin    = I2C5_SDA,
    .scl_pin    = I2C5_SCL,
    .udelay     = I2CUDELAY(CFG_I2C5_CLK),              /* Gpio_mode CLK Rate = 1/( udelay*2) * 1000000 */

    .timeout    = 10,
};

static struct i2c_gpio_platform_data nxp_i2c_gpio_port6 = {
    .sda_pin    = I2C6_SDA,
    .scl_pin    = I2C6_SCL,
    .udelay     = I2CUDELAY(CFG_I2C6_CLK),              /* Gpio_mode CLK Rate = 1/( udelay*2) * 1000000 */

    .timeout    = 10,
};


static struct i2c_gpio_platform_data nxp_i2c_gpio_port7 = {
    .sda_pin    = I2C7_SDA,
    .scl_pin    = I2C7_SCL,
    .udelay     = I2CUDELAY(I2C7_CLK),              /* Gpio_mode CLK Rate = 1/( udelay*2) * 1000000 */
    .timeout    = 10,
};

static struct i2c_gpio_platform_data nxp_i2c_gpio_port8 = {
    .sda_pin    = I2C8_SDA,
    .scl_pin    = I2C8_SCL,
    .udelay     = I2CUDELAY(I2C8_CLK),              /* Gpio_mode CLK Rate = 1/( udelay*2) * 1000000 */
    .timeout    = 10,
};


static struct platform_device i2c_device_ch3 = {
	.name	= "i2c-gpio",
	.id		= 3,
	.dev    = {
		.platform_data	= &nxp_i2c_gpio_port3,
	},
};

#if defined(CONFIG_USB_HUB_USB2514)
static struct platform_device i2c_device_ch4 = {
    .name   = "i2c-gpio",
    .id     = 4,
    .dev    = {
        .platform_data  = &nxp_i2c_gpio_port4,
    },
};
#endif

static struct platform_device i2c_device_ch5 = {
    .name   = "i2c-gpio",
    .id     = 5,
    .dev    = {
        .platform_data  = &nxp_i2c_gpio_port5,
    },
};

static struct platform_device i2c_device_ch6 = {
    .name   = "i2c-gpio",
    .id     = 6,
    .dev    = {
        .platform_data  = &nxp_i2c_gpio_port6,
    },
};

static struct platform_device i2c_device_ch7 = {
    .name   = "i2c-gpio",
    .id     = 7,
    .dev    = {
        .platform_data  = &nxp_i2c_gpio_port7,
    },
};

static struct platform_device i2c_device_ch8 = {
    .name   = "i2c-gpio",
    .id     = 8,
    .dev    = {
        .platform_data  = &nxp_i2c_gpio_port8,
    },
};

static struct platform_device *i2c_devices[] = {
	&i2c_device_ch3,
#if defined(CONFIG_USB_HUB_USB2514)
    &i2c_device_ch4,
#endif
    &i2c_device_ch5,
    &i2c_device_ch6,
    &i2c_device_ch7,
    &i2c_device_ch8,
};
#endif /* CONFIG_I2C_NXP */

/*------------------------------------------------------------------------------
 * PMIC platform device
 */

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
MP8845C_PDATA_INIT(vout, 0, 600000, 1500000, 1, 1, 1225000, 1, -1);	/* ARM */
MP8845C_PDATA_INIT(vout, 1, 600000, 1500000, 1, 1, 1135000, 1, -1);	/* CORE */
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
 * USB HUB platform device
 */
#if defined(CONFIG_USB_HUB_USB2514)
#define USB2514_I2C_BUS     (4)

static struct i2c_board_info __initdata usb2514_i2c_bdi = {
    .type   = "usb2514",
    .addr   = 0x58>>1,
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

static int camera_common_set_clock(ulong clk_rate)
{
    PM_DBGOUT("%s: %d\n", __func__, (int)clk_rate);
    if (clk_rate > 0)
        nxp_soc_pwm_set_frequency(1, clk_rate, 50);
    else
        nxp_soc_pwm_set_frequency(1, 0, 0);
    msleep(1);
    return 0;
}

static bool is_camera_port_configured = false;
static void camera_common_vin_setup_io(int module, bool force)
{
    if (!force && is_camera_port_configured)
        return;
    else {
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

        printk("%s\n", __func__);

        pad = (u_int *)port;
        len = sizeof(port)/sizeof(port[0]);

        for (i = 0; i < len; i++) {
            io = *pad++;
            fn = *pad++;
            nxp_soc_gpio_set_io_dir(io, 0);
            nxp_soc_gpio_set_io_func(io, fn);
        }

        is_camera_port_configured = true;
    }
}

static bool camera_power_enabled = false;
static void camera_power_control(int enable)
{
    struct regulator *cam_io_28V = NULL;
    struct regulator *cam_core_18V = NULL;
    struct regulator *cam_io_33V = NULL;

    if (enable && camera_power_enabled)
        return;
    if (!enable && !camera_power_enabled)
        return;

    cam_core_18V = regulator_get(NULL, "vcam1_1.8V");
    if (IS_ERR(cam_core_18V)) {
        printk(KERN_ERR "%s: failed to regulator_get() for vcam1_1.8V", __func__);
        return;
    }

    cam_io_28V = regulator_get(NULL, "vcam_2.8V");
    if (IS_ERR(cam_io_28V)) {
        printk(KERN_ERR "%s: failed to regulator_get() for vcam_2.8V", __func__);
        return;
    }

    cam_io_33V = regulator_get(NULL, "vcam_3.3V");
    if (IS_ERR(cam_io_33V)) {
        printk(KERN_ERR "%s: failed to regulator_get() for vcam_3.3V", __func__);
        return;
    }

    printk("%s: %d\n", __func__, enable);
    if (enable) {
        regulator_enable(cam_core_18V);
        regulator_enable(cam_io_28V);
        regulator_enable(cam_io_33V);
    } else {
        regulator_disable(cam_io_33V);
        regulator_disable(cam_io_28V);
        regulator_disable(cam_core_18V);
    }

    regulator_put(cam_io_28V);
    regulator_put(cam_core_18V);
    regulator_put(cam_io_33V);

    camera_power_enabled = enable ? true : false;
}

static bool is_back_camera_enabled = false;
static bool is_back_camera_power_state_changed = false;
static bool is_front_camera_enabled = false;
static bool is_front_camera_power_state_changed = false;

static int front_camera_power_enable(bool on);
static int back_camera_power_enable(bool on)
{
#if 0
    unsigned int io = CFG_IO_CAMERA_BACK_POWER_DOWN;
    unsigned int reset_io = CFG_IO_CAMERA_RESET;
    PM_DBGOUT("%s: is_back_camera_enabled %d, on %d\n", __func__, is_back_camera_enabled, on);
    if (on) {
        front_camera_power_enable(0);
        if (!is_back_camera_enabled) {
            camera_power_control(1);
            /* PD signal */
            nxp_soc_gpio_set_out_value(io, 0);
            nxp_soc_gpio_set_io_dir(io, 1);
            nxp_soc_gpio_set_io_func(io, nxp_soc_gpio_get_altnum(io));
            nxp_soc_gpio_set_out_value(io, 1);
            camera_common_set_clock(24000000);
            /* mdelay(10); */
            mdelay(1);
            nxp_soc_gpio_set_out_value(io, 0);
            /* RST signal */
            nxp_soc_gpio_set_out_value(reset_io, 1);
            nxp_soc_gpio_set_io_dir(reset_io, 1);
            nxp_soc_gpio_set_io_func(reset_io, nxp_soc_gpio_get_altnum(io));
            nxp_soc_gpio_set_out_value(reset_io, 0);
            /* mdelay(100); */
            mdelay(1);
            nxp_soc_gpio_set_out_value(reset_io, 1);
            /* mdelay(100); */
            mdelay(1);
            is_back_camera_enabled = true;
            is_back_camera_power_state_changed = true;
        } else {
            is_back_camera_power_state_changed = false;
        }
    } else {
        if (is_back_camera_enabled) {
            nxp_soc_gpio_set_out_value(io, 1);
            nxp_soc_gpio_set_out_value(reset_io, 0);
            is_back_camera_enabled = false;
            is_back_camera_power_state_changed = true;
        } else {
            nxp_soc_gpio_set_out_value(io, 1);
            nxp_soc_gpio_set_io_dir(io, 1);
            nxp_soc_gpio_set_io_func(io, nxp_soc_gpio_get_altnum(io));
            nxp_soc_gpio_set_out_value(io, 1);
            is_back_camera_power_state_changed = false;
        }

        if (!(is_back_camera_enabled || is_front_camera_enabled)) {
            camera_power_control(0);
        }
    }
#endif
    return 0;
}

static bool back_camera_power_state_changed(void)
{
    return is_back_camera_power_state_changed;
}

static struct i2c_board_info back_camera_i2c_boardinfo[] = {
    {
        I2C_BOARD_INFO("SP2518", 0x60>>1),
    },
};

static int front_camera_power_enable(bool on)
{
#if 0
    unsigned int io = CFG_IO_CAMERA_FRONT_POWER_DOWN;
    unsigned int reset_io = CFG_IO_CAMERA_RESET;
    PM_DBGOUT("%s: is_front_camera_enabled %d, on %d\n", __func__, is_front_camera_enabled, on);
    if (on) {
        back_camera_power_enable(0);
        if (!is_front_camera_enabled) {
            camera_power_control(1);
            /* First RST signal to low */
            nxp_soc_gpio_set_out_value(reset_io, 1);
            nxp_soc_gpio_set_io_dir(reset_io, 1);
            nxp_soc_gpio_set_io_func(reset_io, nxp_soc_gpio_get_altnum(io));
            nxp_soc_gpio_set_out_value(reset_io, 0);
            mdelay(1);

            /* PWDN signal High to Low */
            nxp_soc_gpio_set_out_value(io, 0);
            nxp_soc_gpio_set_io_dir(io, 1);
            nxp_soc_gpio_set_io_func(io, nxp_soc_gpio_get_altnum(io));
            nxp_soc_gpio_set_out_value(io, 1);
            camera_common_set_clock(24000000);
            mdelay(10);
            /* mdelay(1); */
            nxp_soc_gpio_set_out_value(io, 0);
            /* mdelay(10); */
            mdelay(10);

            /* RST signal  to High */
            nxp_soc_gpio_set_out_value(reset_io, 1);
            /* mdelay(100); */
            mdelay(5);

            is_front_camera_enabled = true;
            is_front_camera_power_state_changed = true;
        } else {
            is_front_camera_power_state_changed = false;
        }
    } else {
        if (is_front_camera_enabled) {
            nxp_soc_gpio_set_out_value(io, 1);
            is_front_camera_enabled = false;
            is_front_camera_power_state_changed = true;
        } else {
            nxp_soc_gpio_set_out_value(io, 1);
            is_front_camera_power_state_changed = false;
        }
        if (!(is_back_camera_enabled || is_front_camera_enabled)) {
            camera_power_control(0);
        }
    }
#endif
    return 0;
}

static bool front_camera_power_state_changed(void)
{
    return is_front_camera_power_state_changed;
}

static struct i2c_board_info front_camera_i2c_boardinfo[] = {
    {
        I2C_BOARD_INFO("SP0838", 0x18),
    },
};

static struct nxp_v4l2_i2c_board_info sensor[] = {
    {
        .board_info = &back_camera_i2c_boardinfo[0],
        .i2c_adapter_id = 3,
    },
    {
        .board_info = &front_camera_i2c_boardinfo[0],
        .i2c_adapter_id = 3,
    },
};


static struct nxp_capture_platformdata capture_plat_data[] = {
    {
        /* back_camera 656 interface */
        // for 5430
        /*.module = 1,*/
        .module = 0,
        .sensor = &sensor[0],
        .type = NXP_CAPTURE_INF_PARALLEL,
        .parallel = {
            /* for 656 */
            .is_mipi        = false,
            .external_sync  = false, /* 656 interface */
            .h_active       = 800,
            .h_frontporch   = 7,
            .h_syncwidth    = 1,
            .h_backporch    = 10,
            .v_active       = 600,
            .v_frontporch   = 0,
            .v_syncwidth    = 2,
            .v_backporch    = 3,
            .clock_invert   = true,
            .port           = 0,
            .data_order     = NXP_VIN_Y0CBY1CR,
            .interlace      = false,
            .clk_rate       = 24000000,
            .late_power_down = true,
            .power_enable   = back_camera_power_enable,
            .power_state_changed = back_camera_power_state_changed,
            .set_clock      = camera_common_set_clock,
            .setup_io       = camera_common_vin_setup_io,
        },
        .deci = {
            .start_delay_ms = 0,
            .stop_delay_ms  = 0,
        },
    },
    {
        /* front_camera 601 interface */
        // for 5430
        /*.module = 1,*/
        .module = 0,
        .sensor = &sensor[1],
        .type = NXP_CAPTURE_INF_PARALLEL,
        .parallel = {
            .is_mipi        = false,
            .external_sync  = true,
            .h_active       = 640,
            .h_frontporch   = 0,
            .h_syncwidth    = 0,
            .h_backporch    = 2,
            .v_active       = 480,
            .v_frontporch   = 0,
            .v_syncwidth    = 0,
            .v_backporch    = 2,
            .clock_invert   = false,
            .port           = 0,
            .data_order     = NXP_VIN_CBY0CRY1,
            .interlace      = false,
            .clk_rate       = 24000000,
            .late_power_down = true,
            .power_enable   = front_camera_power_enable,
            .power_state_changed = front_camera_power_state_changed,
            .set_clock      = camera_common_set_clock,
            .setup_io       = camera_common_vin_setup_io,
        },
        .deci = {
            .start_delay_ms = 0,
            .stop_delay_ms  = 0,
        },
    },
    { 0, NULL, 0, },
};
/* out platformdata */
static struct i2c_board_info hdmi_edid_i2c_boardinfo = {
    I2C_BOARD_INFO("nxp_edid", 0xA0>>1),
};

static struct nxp_v4l2_i2c_board_info edid = {
    .board_info = &hdmi_edid_i2c_boardinfo,
    .i2c_adapter_id = 5,
};

static struct i2c_board_info hdmi_hdcp_i2c_boardinfo = {
    I2C_BOARD_INFO("nxp_hdcp", 0x74>>1),
};

static struct nxp_v4l2_i2c_board_info hdcp = {
    .board_info = &hdmi_hdcp_i2c_boardinfo,
    .i2c_adapter_id = 5,
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

#if defined(CONFIG_SLSIAP_BACKWARD_CAMERA)
#include <mach/nxp-backward-camera.h>
static struct reg_val _sensor_init_data[] = {

	{0xfd,0x00},
	{0x1b,0x1a},//maximum drv ability
	{0x0e,0x01},
	{0x0f,0x2f},
	{0x10,0x2e},
	{0x11,0x00},
	{0x12,0x4f},
	{0x14,0x40},//20
	{0x16,0x02},
	{0x17,0x10},
	{0x1a,0x1f},
	{0x1e,0x81},
	{0x21,0x00},
	{0x22,0x1b},
	{0x25,0x10},
	{0x26,0x25},
	{0x27,0x6d},
	{0x2c,0x23},//31 Ronlus remove balck dot0x45},
	{0x2d,0x75},
	{0x2e,0x38},//sxga 0x18
    // psw0523 fix
#ifndef CONFIG_VIDEO_SP2518_FIXED_FRAMERATE
	{0x31,0x10},//mirror upside down
    // fix for 30frame
	/* {0x31,0x00},//mirror upside down */
#else
	{0x31,0x18},//mirror upside down
#endif // CONFIG_VIDEO_SP2518_FIXED_FRAMERATE
    // end psw0523
    // psw0523 add for 656
    /* {0x36, 0x1f}, // bit1: ccir656 output enable */
    // end psw0523
	{0x44,0x03},
	{0x6f,0x00},
	{0xa0,0x04},
	{0x5f,0x01},
	{0x32,0x00},
	{0xfd,0x01},
	{0x2c,0x00},
	{0x2d,0x00},
	{0xfd,0x00},
	{0xfb,0x83},
	{0xf4,0x09},
	//Pregain
	{0xfd,0x01},
	{0xc6,0x90},
	{0xc7,0x90},
	{0xc8,0x90},
	{0xc9,0x90},
	//blacklevel
	{0xfd,0x00},
	{0x65,0x08},
	{0x66,0x08},
	{0x67,0x08},
	{0x68,0x08},

	//bpc
	{0x46,0xff},
	//rpc
	{0xfd,0x00},
	{0xe0,0x6c},
	{0xe1,0x54},
	{0xe2,0x48},
	{0xe3,0x40},
	{0xe4,0x40},
	{0xe5,0x3e},
	{0xe6,0x3e},
	{0xe8,0x3a},
	{0xe9,0x3a},
	{0xea,0x3a},
	{0xeb,0x38},
	{0xf5,0x38},
	{0xf6,0x38},
	{0xfd,0x01},
	{0x94,0xcC},//f8 C0
	{0x95,0x38},
	{0x9c,0x74},//6C
	{0x9d,0x38},
#ifndef CONFIG_VIDEO_SP2518_FIXED_FRAMERATE
	/*24*3pll 8~13fps 50hz*/
	{0xfd , 0x00},
	{0x03 , 0x03},
	{0x04 , 0xf6},
	{0x05 , 0x00},
	{0x06 , 0x00},
	{0x07 , 0x00},
	{0x08 , 0x00},
	{0x09 , 0x00},
	{0x0a , 0x8b},
	{0x2f , 0x00},
	{0x30 , 0x08},
	{0xf0 , 0xa9},
	{0xf1 , 0x00},
	{0xfd , 0x01},
	{0x90 , 0x0c},
	{0x92 , 0x01},
	{0x98 , 0xa9},
	{0x99 , 0x00},
	{0x9a , 0x01},
	{0x9b , 0x00},
	//Status
	{0xfd , 0x01},
	{0xce , 0xec},
	{0xcf , 0x07},
	{0xd0 , 0xec},
	{0xd1 , 0x07},
	{0xd7 , 0xab},
	{0xd8 , 0x00},
	{0xd9 , 0xaf},
	{0xda , 0x00},
	{0xfd , 0x00},


	{0xfd , 0x00},
	{0x03 , 0x07},
	{0x04 , 0x9e},
	{0x05 , 0x00},
	{0x06 , 0x00},
	{0x07 , 0x00},
	{0x08 , 0x00},
	{0x09 , 0x00},
	{0x0a , 0xd4},
	{0x2f , 0x00},
	{0x30 , 0x0c},
	{0xf0 , 0x45},
	{0xf1 , 0x01},
	{0xfd , 0x01},
	{0x90 , 0x04},
	{0x92 , 0x01},
	{0x98 , 0x45},
	{0x99 , 0x01},
	{0x9a , 0x01},
	{0x9b , 0x00},
	//Status
	{0xfd , 0x01},
	{0xce , 0x14},
	{0xcf , 0x05},
	{0xd0 , 0x14},
	{0xd1 , 0x05},
	{0xd7 , 0x41},
	{0xd8 , 0x01},
	{0xd9 , 0x45},
	{0xda , 0x01},
	{0xfd , 0x00},
#else
    /* 24M Fixed 10frame */
	{0xfd , 0x00},
	{0x03 , 0x03},
	{0x04 , 0x0C},
	{0x05 , 0x00},
	{0x06 , 0x00},
	{0x07 , 0x00},
	{0x08 , 0x00},
	{0x09 , 0x00},
	{0x0a , 0xE4},
	{0x2f , 0x00},
	{0x30 , 0x11},
	{0xf0 , 0x82},
	{0xf1 , 0x00},
	{0xfd , 0x01},
	{0x90 , 0x0A},
	{0x92 , 0x01},
	{0x98 , 0x82},
	{0x99 , 0x01},
	{0x9a , 0x01},
	{0x9b , 0x00},
	//Status
	{0xfd , 0x01},
	{0xce , 0x14},
	{0xcf , 0x05},
	{0xd0 , 0x14},
	{0xd1 , 0x05},
	{0xd7 , 0x7E},
	{0xd8 , 0x00},
	{0xd9 , 0x82},
	{0xda , 0x00},
	{0xfd , 0x00},
#endif /* CONFIG_VIDEO_SP2518_FIXED_FRAMERATE */

	{0xfd,0x01},
	{0xca,0x30},//mean dummy2low
	{0xcb,0x50},//mean low2dummy
	{0xcc,0xc0},//f8,rpc low
	{0xcd,0xc0},//rpc dummy
	{0xd5,0x80},//mean normal2dummy
	{0xd6,0x90},//mean dummy2normal
	{0xfd,0x00},
	//lens shading for Ë´Ì©979C-171A\181A
	{0xfd,0x00},
	{0xa1,0x20},
	{0xa2,0x20},
	{0xa3,0x20},
	{0xa4,0xff},
	{0xa5,0x80},
	{0xa6,0x80},
	{0xfd,0x01},
	{0x64,0x1e},//28
	{0x65,0x1c},//25
	{0x66,0x1c},//2a
	{0x67,0x16},//25
	{0x68,0x1c},//25
	{0x69,0x1c},//29
	{0x6a,0x1a},//28
	{0x6b,0x16},//20
	{0x6c,0x1a},//22
	{0x6d,0x1a},//22
	{0x6e,0x1a},//22
	{0x6f,0x16},//1c
	{0xb8,0x04},//0a
	{0xb9,0x13},//0a
	{0xba,0x00},//23
	{0xbb,0x03},//14
	{0xbc,0x03},//08
	{0xbd,0x11},//08
	{0xbe,0x00},//12
	{0xbf,0x02},//00
	{0xc0,0x04},//05
	{0xc1,0x0e},//05
	{0xc2,0x00},//18
	{0xc3,0x05},//08
	//raw filter
	{0xfd,0x01},
	{0xde,0x0f},
	{0xfd,0x00},
	{0x57,0x08},//raw_dif_thr
	{0x58,0x08},//a
	{0x56,0x08},//a
	{0x59,0x10},
	//R\BÍ¨µÀ¼äÆ½»¬
	{0x5a,0xa0},//raw_rb_fac_outdoor
	{0xc4,0xa0},//60raw_rb_fac_indoor
	{0x43,0xa0},//40raw_rb_fac_dummy
	{0xad,0x40},//raw_rb_fac_low
	//Gr¡¢Gb Í¨µÀÄÚ²¿Æ½»¬
	{0x4f,0xa0},//raw_gf_fac_outdoor
	{0xc3,0xa0},//60raw_gf_fac_indoor
	{0x3f,0xa0},//40raw_gf_fac_dummy
	{0x42,0x40},//raw_gf_fac_low
	{0xc2,0x15},
	//Gr¡¢GbÍ¨µÀ¼äÆ½»¬
	{0xb6,0x80},//raw_gflt_fac_outdoor
	{0xb7,0x80},//60raw_gflt_fac_normal
	{0xb8,0x40},//40raw_gflt_fac_dummy
	{0xb9,0x20},//raw_gflt_fac_low
	//Gr¡¢GbÍ¨µÀãÐÖµ
	{0xfd,0x01},
	{0x50,0x0c},//raw_grgb_thr
	{0x51,0x0c},
	{0x52,0x10},
	{0x53,0x10},
	{0xfd,0x00},
	// awb1
	{0xfd,0x01},
	{0x11,0x10},
	{0x12,0x1f},
	{0x16,0x1c},
	{0x18,0x00},
	{0x19,0x00},
	{0x1b,0x96},
	{0x1a,0x9a},//95
	{0x1e,0x2f},
	{0x1f,0x29},
	{0x20,0xff},
	{0x22,0xff},
	{0x28,0xce},
	{0x29,0x8a},
	{0xfd,0x00},
	{0xe7,0x03},
	{0xe7,0x00},
	{0xfd,0x01},
	{0x2a,0xf0},
	{0x2b,0x10},
	{0x2e,0x04},
	{0x2f,0x18},
	{0x21,0x60},
	{0x23,0x60},
	{0x8b,0xab},
	{0x8f,0x12},
	//awb2
	{0xfd,0x01},
	{0x1a,0x80},
	{0x1b,0x80},
	{0x43,0x80},
	 //outdoor
    {0x00,0xd4},
    {0x01,0xb0},
    {0x02,0x90},
    {0x03,0x78},
	//d65
	{0x35,0xd6},//d6,b0
	{0x36,0xf0},//f0,d1,e9
	{0x37,0x7a},//8a,70
	{0x38,0x9a},//dc,9a,af
	//indoor
	{0x39,0xab},
	{0x3a,0xca},
	{0x3b,0xa3},
	{0x3c,0xc1},
	//f
	{0x31,0x82},//7d
	{0x32,0xa5},//a0,74
	{0x33,0xd6},//d2
	{0x34,0xec},//e8
	{0x3d,0xa5},//a7,88
	{0x3e,0xc2},//be,bb
	{0x3f,0xa7},//b3,ad
	{0x40,0xc5},//c5,d0
	//Color Correction
	{0xfd,0x01},
	{0x1c,0xc0},
	{0x1d,0x95},
	{0xa0,0xa6},//b8
	{0xa1,0xda},//,d5
	{0xa2,0x00},//,f2
	{0xa3,0x06},//,e8
	{0xa4,0xb2},//,95
	{0xa5,0xc7},//,03
	{0xa6,0x00},//,f2
	{0xa7,0xce},//,c4
	{0xa8,0xb2},//,ca
	{0xa9,0x0c},//,3c
	{0xaa,0x30},//,03
	{0xab,0x0c},//,0f
	{0xac,0xc0},//b8
	{0xad,0xc0},//d5
	{0xae,0x00},//f2
	{0xaf,0xf2},//e8
	{0xb0,0xa6},//95
	{0xb1,0xe8},//03
	{0xb2,0x00},//f2
	{0xb3,0xe7},//c4
	{0xb4,0x99},//ca
	{0xb5,0x0c},//3c
	{0xb6,0x33},//03
	{0xb7,0x0c},//0f
	//Saturation
	{0xfd,0x00},
	{0xbf,0x01},
	{0xbe,0xbb},
	{0xc0,0xb0},
	{0xc1,0xf0},
	{0xd3,0x77},
	{0xd4,0x77},
	{0xd6,0x77},
	{0xd7,0x77},
	{0xd8,0x77},
	{0xd9,0x77},
	{0xda,0x77},
	{0xdb,0x77},
	//uv_dif
	{0xfd,0x00},
	{0xf3,0x03},
	{0xb0,0x00},
	{0xb1,0x23},
	//gamma1
	{0xfd,0x00},//
	{0x8b,0x0 },//0 ,0
	{0x8c,0xA },//14,A
	{0x8d,0x13},//24,13
	{0x8e,0x25},//3a,25
	{0x8f,0x43},//59,43
	{0x90,0x5D},//6f,5D
	{0x91,0x74},//84,74
	{0x92,0x88},//95,88
	{0x93,0x9A},//a3,9A
	{0x94,0xA9},//b1,A9
	{0x95,0xB5},//be,B5
	{0x96,0xC0},//c7,C0
	{0x97,0xCA},//d1,CA
	{0x98,0xD4},//d9,D4
	{0x99,0xDD},//e1,DD
	{0x9a,0xE6},//e9,E6
	{0x9b,0xEF},//f1,EF
	{0xfd,0x01},//01,01
	{0x8d,0xF7},//f9,F7
	{0x8e,0xFF},//ff,FF
	//gamma2
	{0xfd,0x00},//
	{0x78,0x0 },//0
	{0x79,0xA },//14
	{0x7a,0x13},//24
	{0x7b,0x25},//3a
	{0x7c,0x43},//59
	{0x7d,0x5D},//6f
	{0x7e,0x74},//84
	{0x7f,0x88},//95
	{0x80,0x9A},//a3
	{0x81,0xA9},//b1
	{0x82,0xB5},//be
	{0x83,0xC0},//c7
	{0x84,0xCA},//d1
	{0x85,0xD4},//d9
	{0x86,0xDD},//e1
	{0x87,0xE6},//e9
	{0x88,0xEF},//f1
	{0x89,0xF7},//f9
	{0x8a,0xFF},//ff

	//gamma_ae
	{0xfd,0x01},
	{0x96,0x46},
	{0x97,0x14},
	{0x9f,0x06},
	//HEQ
	{0xfd,0x00},//
	{0xdd,0x80},//
	{0xde,0x95},//a0
	{0xdf,0x80},//
	//Ytarget
	{0xfd,0x00},//
	{0xec,0x70},//6a
	{0xed,0x86},//7c
	{0xee,0x70},//65
	{0xef,0x86},//78
	{0xf7,0x80},//78
	{0xf8,0x74},//6e
	{0xf9,0x80},//74
	{0xfa,0x74},//6a
	//sharpen
	{0xfd,0x01},
	{0xdf,0x0f},
	{0xe5,0x10},
	{0xe7,0x10},
	{0xe8,0x20},
	{0xec,0x20},
	{0xe9,0x20},
	{0xed,0x20},
	{0xea,0x10},
	{0xef,0x10},
	{0xeb,0x10},
	{0xf0,0x10},
	//,gw
	{0xfd,0x01},//
	{0x70,0x76},//
	{0x7b,0x40},//
	{0x81,0x30},//
	//,Y_offset
	{0xfd,0x00},
	{0xb2,0x10},
	{0xb3,0x1f},
	{0xb4,0x30},
	{0xb5,0x50},
	//,CNR
	{0xfd,0x00},
	{0x5b,0x20},
	{0x61,0x80},
	{0x77,0x80},
	{0xca,0x80},
	//,YNR
	{0xab,0x00},
	{0xac,0x02},
	{0xae,0x08},
	{0xaf,0x20},
	{0xfd,0x00},
	{0x31,0x10},
	{0x32,0x0d},
	{0x33,0xcf},//ef
	{0x34,0x7f},//3f
	{0xe7,0x03},
	{0xe7,0x00},

    // 704x480
	{0xfd,0x00},
    {0x36, 0x1f}, // bit1: ccir656 output enable

	{0x47,0x01},
	{0x48,0x68},
	{0x49,0x01},
	{0x4a,0xe0},
	{0x4b,0x01},
	{0x4c,0xc0},
	{0x4d,0x02},
	{0x4e,0xc0},
	{0xfd,0x01},
	{0x06,0x00},
	{0x07,0x25},
	{0x08,0x00},
	{0x09,0x28},
	{0x0a,0x04},
	{0x0b,0x00},
	{0x0c,0x05},
	{0x0d,0x00},
    {0x0e,0x00},

    // CrYCbY
    {0xfd,0x00},
    /*{0x35,0x01},*/
    /*{0x35,0x40},*/
    {0x35,0x00},

    END_MARKER
};

static void _draw_rgb_overlay(struct nxp_backward_camera_platform_data *plat_data)
{
    void *mem = (void *)plat_data->rgb_addr;
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
}

#define BACKWARD_CAM_WIDTH  704
#define BACKWARD_CAM_HEIGHT 480

static struct nxp_backward_camera_platform_data backward_camera_plat_data = {
    .backgear_gpio_num  = CFG_BACKGEAR_GPIO_NUM,
    .active_high        = false,
    .vip_module_num     = 1,
    .mlc_module_num     = 0,

    // sensor
    .i2c_bus            = 0,
    .chip_addr          = 0x60 >> 1,
    .reg_val            = _sensor_init_data,
    .power_enable       = back_camera_power_enable,
    .set_clock          = camera_common_set_clock,
    .setup_io           = camera_common_vin_setup_io,
    .clk_rate           = 24000000,

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
    .interlace          = false,

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

/*static struct platform_device backward_camera_device = {*/
struct platform_device backward_camera_device = {
    .name           = "nxp-backward-camera",
    .dev			= {
        .platform_data	= &backward_camera_plat_data,
    }
};
#endif /*CONFIG_SLSIAP_BACKWARD_CAMERA */

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
        .fb_delay   = 0x2,
    },
};
struct spi_board_info spi0_board_info[] __initdata = {
    {
        .modalias       = "spidev",
        .platform_data  = NULL,
        .max_speed_hz   = 10 * 1000 * 1000,
        .bus_num        = 0,
        .chip_select    = 0,
        .mode           = SPI_MODE_0,
        .controller_data    = &spi0_csi[0],
    }
};
#endif




/*------------------------------------------------------------------------------
 * DW MMC board config
 */
#if defined(CONFIG_MMC_DW)
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
    .quirks         = DW_MCI_QUIRK_HIGHSPEED,
    .bus_hz         = 100 * 1000 * 1000,
    .caps           = MMC_CAP_CMD23,
    .detect_delay_ms= 200,
    .cd_type        = DW_MCI_CD_EXTERNAL,
    .clk_dly        = DW_MMC_DRIVE_DELAY(0) | DW_MMC_SAMPLE_DELAY(0) | DW_MMC_DRIVE_PHASE(2) | DW_MMC_SAMPLE_PHASE(1),
    .init           = _dwmci0_init,
    .get_ro         = _dwmci_get_ro,
    .get_cd         = _dwmci0_get_cd,
    .ext_cd_init    = _dwmci_ext_cd_init,
    .ext_cd_cleanup = _dwmci_ext_cd_cleanup,
#if defined (CONFIG_MMC_DW_IDMAC) && defined (CONFIG_MMC_NXP_CH0_USE_DMA)
    .mode           = DMA_MODE,
#else
    .mode           = PIO_MODE,
#endif

};
#endif

#ifdef CONFIG_MMC_NXP_CH1
static struct dw_mci_board _dwmci1_data = {
	.quirks			= DW_MCI_QUIRK_HIGHSPEED,
	.bus_hz			= 100 * 1000 * 1000,
	.caps = MMC_CAP_CMD23|MMC_CAP_NONREMOVABLE,
	.detect_delay_ms= 200,
	.cd_type = DW_MCI_CD_NONE,
	.pm_caps        = MMC_PM_KEEP_POWER | MMC_PM_IGNORE_PM_NOTIFY,
	.clk_dly        = DW_MMC_DRIVE_DELAY(0) | DW_MMC_SAMPLE_DELAY(0) | DW_MMC_DRIVE_PHASE(0) | DW_MMC_SAMPLE_PHASE(0),
#if defined (CONFIG_MMC_DW_IDMAC) && defined (CONFIG_MMC_NXP_CH1_USE_DMA)
    .mode           = DMA_MODE,
#else
    .mode           = PIO_MODE,
#endif

};
#endif

#ifdef CONFIG_MMC_NXP_CH2
static struct dw_mci_board _dwmci2_data = {
    .quirks         = DW_MCI_QUIRK_BROKEN_CARD_DETECTION |
                      DW_MCI_QUIRK_HIGHSPEED |
                      DW_MMC_QUIRK_HW_RESET_PW |
                      DW_MCI_QUIRK_NO_DETECT_EBIT,
    .bus_hz         = 200 * 1000 * 1000, /*200*/
    .caps           = MMC_CAP_UHS_DDR50 |
                      MMC_CAP_NONREMOVABLE |
                      MMC_CAP_8_BIT_DATA | MMC_CAP_CMD23 |
                      MMC_CAP_HW_RESET,
    .clk_dly        = DW_MMC_DRIVE_DELAY(0x0) | DW_MMC_SAMPLE_DELAY(0x0) | DW_MMC_DRIVE_PHASE(2) | DW_MMC_SAMPLE_PHASE(1),

    .desc_sz        = 4,
    .detect_delay_ms= 200,
    .sdr_timing     = 0x01010001,
    .ddr_timing     = 0x03030002,
#if defined (CONFIG_MMC_DW_IDMAC) && defined (CONFIG_MMC_NXP_CH2_USE_DMA)
    .mode           = DMA_MODE,
#else
    .mode           = PIO_MODE,
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

#if defined(CONFIG_NXP_HDMI_CEC)
static struct platform_device hdmi_cec_device = {
	.name			= NXP_HDMI_CEC_DRV_NAME,
};
#endif /* CONFIG_NXP_HDMI_CEC */

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


#if defined(CONFIG_KEYBOARD_NXP_KEY) || defined(CONFIG_KEYBOARD_NXP_KEY_MODULE)
	printk("plat: add device keypad\n");
	platform_device_register(&key_plat_device);
#endif

#if defined(CONFIG_I2C_NXP) || defined (CONFIG_I2C_SLSI)
    platform_add_devices(i2c_devices, ARRAY_SIZE(i2c_devices));
#endif

#if defined(CONFIG_REGULATOR_MP8845C)
	printk("plat: add device mp8845c ARM\n");
	i2c_register_board_info(MP8845C_I2C_BUS0, &mp8845c_regulators[0], 1);

	printk("plat: add device mp8845c CORE\n");
	i2c_register_board_info(MP8845C_I2C_BUS1, &mp8845c_regulators[1], 1);
#endif

#if defined(CONFIG_SND_CODEC_RT5640) || defined(CONFIG_SND_CODEC_RT5640_MODULE)
	printk("plat: add device asoc-rt5640\n");
	i2c_register_board_info(RT5640_I2C_BUS, &rt5640_i2c_bdi, 1);
	platform_device_register(&rt5640_dai);
#endif

#if defined(CONFIG_SND_CODEC_ALC5623) || defined(CONFIG_SND_CODEC_ALC5623_MODULE)
	printk("plat: add device asoc-alc5623\n");
	i2c_register_board_info(ALC5623_I2C_BUS, &alc5623_i2c_bdi, 1);
	platform_device_register(&alc5623_dai);
#endif

#if defined(CONFIG_SND_SPDIF_TRANSCIEVER) || defined(CONFIG_SND_SPDIF_TRANSCIEVER_MODULE)
	printk("plat: add device spdif playback\n");
	platform_device_register(&spdif_transciever);
	platform_device_register(&spdif_trans_dai);
#endif

#if defined(CONFIG_V4L2_NXP) || defined(CONFIG_V4L2_NXP_MODULE)
    printk("plat: add device nxp-v4l2\n");
    platform_device_register(&nxp_v4l2_dev);
#endif

#if defined(CONFIG_SPI_SPIDEV) || defined(CONFIG_SPI_SPIDEV_MODULE)
    spi_register_board_info(spi0_board_info, ARRAY_SIZE(spi0_board_info));
    printk("plat: register spidev\n");
#endif

#if defined(CONFIG_NXP_HDMI_CEC)
    printk("plat: add device hdmi-cec\n");
    platform_device_register(&hdmi_cec_device);
#endif

#if defined(CONFIG_TOUCHSCREEN_TSC2007)
    printk("plat: add touch(tsc2007) device\n");
    i2c_register_board_info(TSC2007_I2C_BUS, &tsc2007_i2c_bdi, 1);
#endif


#if defined(CONFIG_USB_HUB_USB2514)
    printk("plat: add device usb2514\n");
    i2c_register_board_info(USB2514_I2C_BUS, &usb2514_i2c_bdi, 1);
#endif

	/* END */
	printk("\n");
}
