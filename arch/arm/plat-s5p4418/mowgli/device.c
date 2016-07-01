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
	4,      // Tidemark
	(1<<BOTBUS_SI_SLOT_1ST_ARM) |   // Control
		(1<<BOTBUS_SI_SLOT_2ND_ARM) |
		(0<<BOTBUS_SI_SLOT_MALI) |
		(1<<BOTBUS_SI_SLOT_TOP) |
		(0<<BOTBUS_SI_SLOT_DEINTERLACE) |
		(0<<BOTBUS_SI_SLOT_1ST_CODA)
};
#endif

#if (CFG_BUS_RECONFIG_DISPBUSSI == 1)
const u8 g_DispBusSI[3] = {
	DISBUS_SI_SLOT_1ST_DISPLAY,
	DISBUS_SI_SLOT_2ND_DISPLAY,
	DISBUS_SI_SLOT_2ND_DISPLAY		// DISBUS_SI_SLOT_GMAC
};
#endif
#endif	/* #if (CFG_BUS_RECONFIG_ENB == 1) */

/*------------------------------------------------------------------------------
 * CPU Frequence
 */
#if defined(CONFIG_ARM_NXP_CPUFREQ)

struct nxp_cpufreq_plat_data dfs_plat_data = {
	.pll_dev	   	= CONFIG_NXP_CPUFREQ_PLLDEV,
	.supply_name	= "vdd_arm_1.3V",	//refer to CONFIG_REGULATOR_NXE2000
	//.max_cpufreq	= 1400*1000,
	//.max_retention  =   20*1000,
	//.rest_cpufreq   =  800*1000,
	//.rest_retention =    1*1000,
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
#if 0		
	{              
        .temp  = 44,		/* thermal camera: 75 */
		.freq  = 1200000,
		.period = 1000, 	/* Ms */
	} , 
	{
		.temp  = 50,		/* thermal camera: 80 */
		.freq  = 1000000,
		.period = 1000, 	/* Ms */
	} , 
	{
		.temp  = 52, 		/* thermal camera: 82 */
		.freq  = 800000,
		.period = 1000, 	/* Ms */
	} ,
#endif
	{
		.temp  = 101,  		/* thermal camera: 85 */
		.freq  = 800000,    /* freq = 0 :Set critical temp. Power off! */
		.period = 1000, 	/* Ms */
	}
};                                                                              
                                                                                
struct nxp_adc_tmp_platdata adc_tmp_plat_data ={                                
    .channel    = 2,                                                            
    .tmp_offset = 0,                                                            
    .duration   = 1000,                                                         
    .step_up    = 0,                                                            
    .event      = adc_tmp_event,                                                
    .eventsize = ARRAY_SIZE( adc_tmp_event),                                    
};                                                                              
                                                                                
static struct platform_device adc_temp_plat_device = {                          
    .name           = "nxp-adc-tmp",                                            
    .dev            = {                                                         
        .platform_data  = &adc_tmp_plat_data,                                   
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
 * ASoC Codec platform device
 */
#if defined(CONFIG_SND_CODEC_WM8985) || defined(CONFIG_SND_CODEC_WM8985_MODULE)
#include <linux/i2c.h>

#define	WM8985_I2C_BUS		(2)

/* CODEC */

#if 1
static struct i2c_board_info __initdata wm8985_i2c_bdi = {
	.type	= "wm8985",			// compatible 8758
	.addr	= (0x34>>1),		// 0x1A (7BIT), 0x34(8BIT)
};
/* DAI */
struct nxp_snd_dai_plat_data i2s_dai_data = {
	.i2s_ch	= 0,
	.sample_rate	= 44100,
	.hp_jack 		= {
		.support    	= 1,
		.detect_io		= PAD_GPIO_B + 24,
		.detect_level	= 1,
	},
};

static struct platform_device wm8985_dai = {
	.name			= "wm8985-audio",
	.id				= 0,
	.dev			= {
		.platform_data	= &i2s_dai_data,
	}
};
#else
static struct i2c_board_info __initdata wm8985_board_info = {
    I2C_BOARD_INFO("wm8985", 0x3A),
};

#endif

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
#define	I2C3_SCL	PAD_GPIO_E + 14
#define	I2C3_SDA	PAD_GPIO_E + 15

static struct i2c_gpio_platform_data nxp_i2c_gpio_port3 = {
	.sda_pin	= I2C3_SDA,
	.scl_pin	= I2C3_SCL,
	.udelay		= I2CUDELAY(CFG_I2C2_CLK),				/* Gpio_mode CLK Rate = 1/( udelay*2) * 1000000 */

	.timeout	= 10,
};


static struct platform_device i2c_device_ch3 = {
	.name	= "i2c-gpio",
	.id		= 3,
	.dev    = {
		.platform_data	= &nxp_i2c_gpio_port3,
	},
};

static struct platform_device *i2c_devices[] = {
	&i2c_device_ch3,
};
#endif /* CONFIG_I2C_NXP */

#if defined(CONFIG_TOUCHSCREEN_TSC2007)
#include <linux/i2c.h>
#include <linux/i2c/tsc2007.h>

#define TSC2007_I2C_BUS     (1)

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
    .bt_count   = ARRAY_SIZE(button_gpio),
    .bt_io      = button_gpio,
    .bt_code    = button_code,
    .bt_repeat  = CFG_KEYPAD_REPEAT,
};

static struct platform_device key_plat_device = {
    .name   = DEV_NAME_KEYPAD,
    .id     = -1,
    .dev    = {
        .platform_data  = &key_plat_data
    },
};
#endif  /* CONFIG_KEYBOARD_NXP_KEY || CONFIG_KEYBOARD_NXP_KEY_MODULE */

#if defined(CONFIG_KEYBOARD_GPIO_POLLED)
#include <linux/gpio_keys.h>
#include <linux/input.h>

//static unsigned int  button_gpio[] = CFG_KEYPAD_KEY_BUTTON;

static struct gpio_keys_button keys_polled_table[] = {
    {
        .code = KEY_BATTERY,
        .gpio = PAD_GPIO_D + 25,
        .active_low = 0,
        .desc = "gpio-keys-polled: PWR_DET",
        .type = EV_KEY,
        .debounce_interval = 30,
    },
    {
        .code = KEY_POWER,
        .gpio = PAD_GPIO_ALV + 0,
        .active_low = 0,
        .desc = "gpio-keys-polled: PWR_KEY",
        .type = EV_KEY,
        .debounce_interval = 30,
    },
    {
        .code = KEY_CONTROL_BOX,
        .gpio = PAD_GPIO_E + 30,
        .active_low = 0,
        .desc = "gpio-keys-polled: CB_BOX_KEY",
        .type = EV_KEY,
        .debounce_interval = 30,
    },
};

static struct gpio_keys_platform_data keys_polled_data = {
    .buttons = keys_polled_table,
    .nbuttons = ARRAY_SIZE(keys_polled_table),
    .poll_interval = 25,
};

static struct platform_device keys_polled = {
    .name = "gpio-keys-polled",
    .dev = {
        .platform_data = &keys_polled_data,
    },
};
#endif

#if defined(CONFIG_KEYBOARD_GPIO)
#include <linux/gpio_keys.h>
#include <linux/input.h>
static struct gpio_keys_button mowgli_keys[] = {
    {
        .code           = KEY_POWER,
        .gpio           = CFG_KEYPAD_KEY_BUTTON,
        .desc           = "power_key",
        .active_low     = 1,
        .type       = EV_KEY,
        .wakeup     = 1,
        .debounce_interval = 15,
    },
};

static struct gpio_keys_platform_data mowgli_keys_data = {
    .buttons        = mowgli_keys,
    .nbuttons       = ARRAY_SIZE(mowgli_keys),
};

static struct platform_device mowgli_kp_pdev = {
    .name           = "gpio-keys",
    .id             = -1,
    .dev            = {
        .platform_data  = &mowgli_keys_data,
    },
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
	REGULATOR_SUPPLY("vlcd_3.3V", NULL),
};
static struct regulator_consumer_supply nxe2000_ldo6_supply_0[] = {
	REGULATOR_SUPPLY("valive_3.3V", NULL),
};
static struct regulator_consumer_supply nxe2000_ldo7_supply_0[] = {
	REGULATOR_SUPPLY("vvid_2.8V", NULL),
};
static struct regulator_consumer_supply nxe2000_ldo8_supply_0[] = {
	REGULATOR_SUPPLY("vtouch_3.3V", NULL),
};
static struct regulator_consumer_supply nxe2000_ldo9_supply_0[] = {
	REGULATOR_SUPPLY("vcam2_1.8V", NULL),
};
static struct regulator_consumer_supply nxe2000_ldo10_supply_0[] = {
	REGULATOR_SUPPLY("vdumy2_1.2V", NULL),
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
NXE2000_PDATA_INIT(dc1,      0,	 950000, 2000000, 1, 1, 1125000, 1,  4);	/* 1.125V ARM */
NXE2000_PDATA_INIT(dc2,      0,	1000000, 2000000, 1, 1, 1100000, 1,  4);	/* 1.1V CORE */
NXE2000_PDATA_INIT(dc3,      0,	1000000, 3500000, 1, 1, 3300000, 1,  0);	/* 3.3V SYS */
NXE2000_PDATA_INIT(dc4,      0,	1000000, 2000000, 1, 1, 1500000, 1, -1);	/* 1.5V DDR */
NXE2000_PDATA_INIT(dc5,      0,	1000000, 2000000, 1, 1, 1500000, 1,  4);	/* 1.5V SYS */
NXE2000_PDATA_INIT(ldo1,     0,	1000000, 3500000, 0, 0, 3000000, 1,  0);	/* 3.0V GPS */
NXE2000_PDATA_INIT(ldo2,     0,	1000000, 3500000, 1, 1, 1800000, 1,  0);	/* 1.8V AR */
NXE2000_PDATA_INIT(ldo3,     0,	1000000, 3500000, 1, 0, 1800000, 1,  2);	/* 1.8V SYS1 */
NXE2000_PDATA_INIT(ldo4,     0,	1000000, 3500000, 1, 0, 1800000, 1,  2);	/* 1.8V SYS */
NXE2000_PDATA_INIT(ldo5,     0,	1000000, 3500000, 1, 1, 3300000, 1,  0);	/* 3.3V LCD */
NXE2000_PDATA_INIT(ldo6,     0,	1000000, 3500000, 1, 0, 3300000, 1, -1);	/* 3.3V ALIVE */
NXE2000_PDATA_INIT(ldo7,     0,	1000000, 3500000, 1, 0, 3300000, 1,  1);	/* 3.3V VID */
NXE2000_PDATA_INIT(ldo8,     0,	1000000, 3500000, 1, 1, 3300000, 1,  0);	/* 3.3 Touch */
NXE2000_PDATA_INIT(ldo9,     0,	1000000, 3500000, 1, 1, 1800000, 1,  0);	/* 1.8V CIF */
NXE2000_PDATA_INIT(ldo10,    0,	1000000, 3500000, 0, 0,      -1, 0,  0);	/* Not Use */
NXE2000_PDATA_INIT(ldortc1,  0, 1700000, 3500000, 1, 0, 1800000, 1, -1);    /* 1.8V ALIVE */
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

static void back_camera_vin_setup_io(int module, bool force)
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
		{ PAD_GPIO_D + 28, NX_GPIO_PADFUNC_1 },
		{ PAD_GPIO_D + 29, NX_GPIO_PADFUNC_1 },
		{ PAD_GPIO_D + 30, NX_GPIO_PADFUNC_1 },
		{ PAD_GPIO_D + 31, NX_GPIO_PADFUNC_1 },
		{ PAD_GPIO_E +  0, NX_GPIO_PADFUNC_1 },
		{ PAD_GPIO_E +  1, NX_GPIO_PADFUNC_1 },
		{ PAD_GPIO_E +  2, NX_GPIO_PADFUNC_1 },
		{ PAD_GPIO_E +  3, NX_GPIO_PADFUNC_1 },
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
}

static void front_camera_vin_setup_io(int module, bool force)
{
	u_int *pad;
	int i, len;
	u_int io, fn;

	/* VIP1:0 = VCLK, VID0 ~ 7 */
	const u_int port[][2] = {
		/* VCLK, HSYNC, VSYNC */
		{ PAD_GPIO_A + 28, NX_GPIO_PADFUNC_1 },
		{ PAD_GPIO_E + 13, NX_GPIO_PADFUNC_2 },
		{ PAD_GPIO_E +  7, NX_GPIO_PADFUNC_2 },
		/* DATA */
		{ PAD_GPIO_A + 30, NX_GPIO_PADFUNC_1 },
		{ PAD_GPIO_B +  0, NX_GPIO_PADFUNC_1 },
		{ PAD_GPIO_B +  2, NX_GPIO_PADFUNC_1 },
		{ PAD_GPIO_B +  4, NX_GPIO_PADFUNC_1 },
		{ PAD_GPIO_B +  6, NX_GPIO_PADFUNC_1 },
		{ PAD_GPIO_B +  8, NX_GPIO_PADFUNC_1 },
		{ PAD_GPIO_B +  9, NX_GPIO_PADFUNC_1 },
		{ PAD_GPIO_B + 10, NX_GPIO_PADFUNC_1 },
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

static int back_camera_power_enable(bool on)
{
	return 0;
}

static bool back_camera_power_state_changed(void)
{
    return is_back_camera_power_state_changed;
}

static struct i2c_board_info back_camera_i2c_boardinfo[] = {
    {
        I2C_BOARD_INFO("tw9900", 0x88>>1),
    },
};

static struct nxp_v4l2_i2c_board_info sensor[] = {
    {
        .board_info = &back_camera_i2c_boardinfo[0],
        .i2c_adapter_id = 0,
    },
};


static struct nxp_capture_platformdata capture_plat_data[] = {
	{
		/* back_camera 656 interface */
		.module = 1,
		//..module = 0,
		.sensor = &sensor[0],
		.type = NXP_CAPTURE_INF_PARALLEL,
		.parallel = {
			/* for 656 */
			.is_mipi        = false,
			.external_sync  = false, /* 656 interface */
			.h_active       = 704,
			.h_frontporch   = 7,
			.h_syncwidth    = 1,
			.h_backporch    = 10,
			.v_active       = 480,
			.v_frontporch   = 0,
			.v_syncwidth    = 2,
			.v_backporch    = 3,
			.clock_invert   = true,
			.port           = 0,
			.data_order     = NXP_VIN_CBY0CRY1,	//NXP_VIN_CRY1CBY0, NXP_VIN_Y0CBY1CR, NXP_VIN_Y1CRY0CB,
			.interlace      = true,
			.clk_rate       = 27000000,
			.late_power_down = false,
			.power_enable   = back_camera_power_enable,
			.power_state_changed = back_camera_power_state_changed,
			.set_clock      = NULL,
			.setup_io       = back_camera_vin_setup_io,
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
        .modalias        = "fc8080_spi",    /* fixup */
        //.max_speed_hz    = 3125000,     /* max spi clock (SCK) speed in HZ */
        //.platform_data   = NULL,
        .max_speed_hz    = 15 * 1000 * 1000,     /* max spi clock (SCK) speed in HZ */
        .bus_num         = 0,           /* Note> set bus num, must be smaller than ARRAY_SIZE(spi_plat_device) */
        .chip_select     = 0,           /* Note> set chip select num, must be smaller than spi cs_num */
        .controller_data = &spi0_info,
        .mode            = SPI_MODE_0, //SPI_MODE_3 | SPI_CPOL | SPI_CPHA,
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
	.caps			= MMC_CAP_UHS_DDR50 |
					  MMC_CAP_NONREMOVABLE |
			 	  	  MMC_CAP_4_BIT_DATA | MMC_CAP_CMD23 |
				  	  MMC_CAP_HW_RESET,
	.clk_dly        = DW_MMC_DRIVE_DELAY(0) | DW_MMC_SAMPLE_DELAY(0x1c) | DW_MMC_DRIVE_PHASE(2) | DW_MMC_SAMPLE_PHASE(1),

	.desc_sz		= 4,
	.detect_delay_ms= 200,
	.sdr_timing		= 0x01010001,
	.ddr_timing		= 0x03030002,
#if defined (CONFIG_MMC_DW_IDMAC) && defined (CONFIG_MMC_NXP_CH0_USE_DMA)
    .mode       	= DMA_MODE,
#else
    .mode       	= PIO_MODE,
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
	int id  = 0, ret = 0;

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

static struct dw_mci_board _dwmci2_ext_cb_data = {
	.quirks			= DW_MCI_QUIRK_HIGHSPEED,
	.bus_hz			= 100 * 1000 * 1000,
	.caps			= MMC_CAP_CMD23,
	.detect_delay_ms= 200,
	.cd_type		= DW_MCI_CD_EXTERNAL,
	.clk_dly        = DW_MMC_DRIVE_DELAY(0) | DW_MMC_SAMPLE_DELAY(0) | DW_MMC_DRIVE_PHASE(3) | DW_MMC_SAMPLE_PHASE(2),
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
	//{0x02, 0x40},                                                           
	{0x02, 0x44}, //Mux1 selected
	{0x03, 0xa2},
	{0x07, 0x02},
	{0x08, 0x12},
	{0x09, 0xf0},
	{0x0a, 0x1c},
	{0x0b, 0xc0}, // 704
	{0x1b, 0x00},
	{0x10, 0x1e},
	{0x11, 0x64},
	{0x2f, 0xe6},
	{0x55, 0x00},
	{0xaf, 0x00},
	{0xb1, 0x20},
	{0xb4, 0x20},
	END_MARKER
};

#define CAMERA_PWDN	((PAD_GPIO_E + 10) | PAD_FUNC_ALT0)
#define CAMERA_RESET	((PAD_GPIO_E +  9) | PAD_FUNC_ALT0)
static int _sensor_power_enable(bool enable)
{
	unsigned int io = CAMERA_PWDN;
	unsigned int reset_io = CAMERA_RESET;
		
	printk("%s: Enter +++ enable = %d \n", __FUNCTION__, enable);

	if (enable) {
		/* Power Down : Active High*/
		nxp_soc_gpio_set_out_value(io, 0);
		nxp_soc_gpio_set_io_dir(io, 1);
		nxp_soc_gpio_set_io_func(io,
					 nxp_soc_gpio_get_altnum(io));
		mdelay(1);
		nxp_soc_gpio_set_out_value(io, 0);
		mdelay(200);

		/* Reset : Active Low */
		nxp_soc_gpio_set_out_value(reset_io, 1);
		nxp_soc_gpio_set_io_dir(reset_io, 1);
		nxp_soc_gpio_set_io_func(reset_io,
					 nxp_soc_gpio_get_altnum(io));
		nxp_soc_gpio_set_out_value(reset_io, 0);
		mdelay(1);
		nxp_soc_gpio_set_out_value(reset_io, 1);
		mdelay(200);
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
		{ PAD_GPIO_D + 28, NX_GPIO_PADFUNC_1 },
		{ PAD_GPIO_D + 29, NX_GPIO_PADFUNC_1 },
		{ PAD_GPIO_D + 30, NX_GPIO_PADFUNC_1 },
		{ PAD_GPIO_D + 31, NX_GPIO_PADFUNC_1 },
		{ PAD_GPIO_E +  0, NX_GPIO_PADFUNC_1 },
		{ PAD_GPIO_E +  1, NX_GPIO_PADFUNC_1 },
		{ PAD_GPIO_E +  2, NX_GPIO_PADFUNC_1 },
		{ PAD_GPIO_E +  3, NX_GPIO_PADFUNC_1 },
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
				pbuffer[i * plat_data->width + j] = color;
			}
		}
	}
	printk("%s exit\n", __func__);
}

#define BACKWARD_CAM_WIDTH  704
#define BACKWARD_CAM_HEIGHT 480
#define CFG_BACKGEAR_GPIO_NUM               ((PAD_GPIO_E + 8) | PAD_FUNC_ALT0)
#define CFG_BACKGEAR_IRQ_NUM                (IRQ_GPIO_E_START + 8)

static struct nxp_backward_camera_platform_data backward_camera_plat_data = {
	.backgear_irq_num   = CFG_BACKGEAR_IRQ_NUM,
	.backgear_gpio_num  = CFG_BACKGEAR_GPIO_NUM,
	.active_high        = false,
	.vip_module_num     = 1,
	.mlc_module_num     = 0,

	// sensor
	.i2c_bus            = 0,
	.chip_addr          = 0x88>>1,
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

	.lu_stride          = 0,
	.cb_stride          = 0,
	.cr_stride          = 0,

	.rgb_format         = MLC_RGBFMT_A8R8G8B8,
	.width              = 800,
	.height             = 480,
	.rgb_addr           = 0,
//	.draw_rgb_overlay   = _draw_rgb_overlay,
};

//static struct platform_device backward_camera_device = {
struct platform_device backward_camera_device = {
	.name           = "nxp-backward-camera",
	.dev			= {
		.platform_data	= &backward_camera_plat_data,
	}
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
    printk("nxp_mmc_add_device\n");




    if(0 != nxp_soc_gpio_get_in_value(CFG_SDMMC2_DETECT_CONTROL_BOX)){

        printk("Don't detect Control Box\n");
        nxp_mmc_add_device(2, &_dwmci2_data);

    }else {

        printk("Detect control Box\n");
        //change drive's streng value: 1
        nxp_soc_gpio_set_io_drv( (PAD_GPIO_C + 17), 2 );     // PAD_GPIOC17    SD_WP 
        nxp_soc_gpio_set_io_drv( (PAD_GPIO_C + 18), 2 );     // PAD_GPIOC18    SD_CLK
        nxp_soc_gpio_set_io_drv( (PAD_GPIO_C + 19), 2 );     // PAD_GPIOC19    SD_CMD
        nxp_soc_gpio_set_io_drv( (PAD_GPIO_C + 20), 2 );     // PAD_GPIOC20    SD_DATA0
        nxp_soc_gpio_set_io_drv( (PAD_GPIO_C + 21), 2 );     // PAD_GPIOC21    SD_DAT1
        nxp_soc_gpio_set_io_drv( (PAD_GPIO_C + 22), 2 );     // PAD_GPIOC22    SD_DAT2
        nxp_soc_gpio_set_io_drv( (PAD_GPIO_C + 23), 2 );     // PAD_GPIOC23    SD_DAT3

        nxp_mmc_add_device(2, &_dwmci2_ext_cb_data);
    }
	#endif
    #ifdef CONFIG_MMC_NXP_CH1
	nxp_mmc_add_device(1, &_dwmci1_data);
	#endif
#endif

#if defined(CONFIG_BACKLIGHT_PWM)
	printk("plat: add backlight pwm device\n");
	platform_device_register(&bl_plat_device);
#endif

#if defined(CONFIG_I2C_NXP)
    platform_add_devices(i2c_devices, ARRAY_SIZE(i2c_devices));
#endif

#if defined(CONFIG_KEYBOARD_NXP_KEY) || defined(CONFIG_KEYBOARD_NXP_KEY_MODULE)
    printk("plat: add device keypad\n");
    platform_device_register(&key_plat_device);
#endif

#if defined(CONFIG_KEYBOARD_GPIO_POLLED)
    printk("plat: add device keys_polled\n");
    platform_device_register(&keys_polled);
#endif    

#if defined(CONFIG_REGULATOR_NXE2000)
	printk("plat: add device nxe2000 pmic\n");
	i2c_register_board_info(NXE2000_I2C_BUS, nxe2000_i2c_boardinfo, ARRAY_SIZE(nxe2000_i2c_boardinfo));
#endif

#if defined(CONFIG_SND_CODEC_WM8985) || defined(CONFIG_SND_CODEC_WM8985_MODULE)
    printk("plat: add device asoc-wm8985\n");
    i2c_register_board_info(WM8985_I2C_BUS, &wm8985_i2c_bdi, 1);
    platform_device_register(&wm8985_dai);
#endif

#if defined(CONFIG_V4L2_NXP) || defined(CONFIG_V4L2_NXP_MODULE)
    printk("plat: add device nxp-v4l2\n");
    platform_device_register(&nxp_v4l2_dev);
#endif

#if defined(CONFIG_SPI_SPIDEV) || defined(CONFIG_SPI_SPIDEV_MODULE)
    spi_register_board_info(spi_plat_board, ARRAY_SIZE(spi_plat_board));
    printk("plat: register spidev\n");
#endif

#if defined(CONFIG_RFKILL_NXP)
    printk("plat: add device rfkill\n");
    platform_device_register(&rfkill_device);
#endif

#if defined(CONFIG_TOUCHSCREEN_TSC2007)
    printk("plat: add touch(tsc2007) device\n");
    i2c_register_board_info(TSC2007_I2C_BUS, &tsc2007_i2c_bdi, 1);
#endif

/*
#if defined(CONFIG_SLSIAP_BACKWARD_CAMERA)
    printk("plat: register device backward-camera platform device to fine-boot\n");
    platform_device_register(&backward_camera_device);
#endif
*/

	/* END */
	printk("\n");
}
