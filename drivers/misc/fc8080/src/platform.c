#include <linux/io.h>

#include "platform.h"
#include "fc8080.h"
#include "fci_oal.h"

#ifdef TAURUS
#include <linux/gpio.h>
#include <mach/gpio.h>
#include <plat/gpio-cfg.h>
#include <linux/delay.h>
#include <linux/regulator/machine.h>
#endif

#ifdef S5PV210
#include <mach/gpio.h>
#include <mach/regs-gpio.h>
#include <mach/gpio-bank.h>
#endif


#include <mach/platform.h>
#include <mach/devices.h>
#include <mach/soc.h>



void platform_hw_setting(void)
{




  /* pwr */
  nxp_soc_gpio_set_io_dir(CFG_GPIO_DMB_POWER, 1);      //output mode
  nxp_soc_gpio_set_io_pull_enb(CFG_GPIO_DMB_POWER, 0); //pull disable



#ifdef TAURUS

  /* irq */
  gpio_request(EXYNOS4_GPX1(1), "DMB_IRQ");
  gpio_direction_input(EXYNOS4_GPX1(1));
  s3c_gpio_setpull(EXYNOS4_GPX1(1), S3C_GPIO_PULL_UP);
  gpio_free(EXYNOS4_GPX1(1));

  /* pwr */
  gpio_request(EXYNOS4212_GPV2(7), "DMB_PWR_EN");
  gpio_direction_output(EXYNOS4212_GPV2(7), GPIOF_DIR_OUT);
  s3c_gpio_setpull(EXYNOS4212_GPV2(7), S3C_GPIO_PULL_NONE);
  gpio_free(EXYNOS4212_GPV2(7));


#endif
#ifdef S5PV210
#if 0
	s32 val;

	val = readl(S5PV210_EINT32MASK);
	val = val & ~(0x1<<6);
	writel(val, S5PV210_EINT32MASK);

	val = readl(S5PV210_EINT32PEND);
	val = val & ~(0x1<<6);
	writel(val, S5PV210_EINT32PEND);

	val = readl(S5PV210_EINT32CON);
	val = (val & ~(0x7 << 24)) | (0x2<<24);
	writel(val, S5PV210_EINT32CON);

	val = readl(S5PV210_GPH2CON);
	val = (val & ~(0xf << 24)) | (0xf<<24);
	writel(val, S5PV210_GPH2CON);
#endif
#if 0
	val = readl(S5PV210_GPH2CON);
	printk(KERN_DEBUG"S5PV210_EINT32MASK : 0x%x\n", val);

	val = readl(S5PV210_EINT32CON);
	printk(KERN_DEBUG"S5PV210_EINT32CON : 0x%x\n", val);

	val = readl(S5PV210_EINT32PEND);
	printk(KERN_DEBUG"S5PV210_EINT32PEND : 0x%x\n", val);

	val = readl(S5PV210_EINT32MASK);
	printk(KERN_DEBUG"S5PV210_GPH2CON : 0x%x\n", val);
#endif
#endif
}

void platform_hw_init(u32 power_pin, u32 reset_pin)
{

 
   nxp_soc_gpio_set_out_value(CFG_GPIO_DMB_POWER, CTRUE);
   ms_wait(1);

#ifdef TAURUS
  gpio_set_value(power_pin, 1);
  ms_wait(1);
#endif
#ifdef S5PV210
#ifndef EVB
	gpio_set_value(power_pin, 1);
	ms_wait(1);

	gpio_set_value(reset_pin, 1);
	ms_wait(1);
	gpio_set_value(reset_pin, 0);
	ms_wait(1);
	gpio_set_value(reset_pin, 1);
	ms_wait(1);
#endif
#endif
}

void platform_hw_deinit(u32 power_pin)
{
#ifdef TAURUS
  ms_wait(1);
  gpio_set_value(power_pin, 0);
#endif
#ifdef S5PV210
#ifndef EVB
	ms_wait(1);
	gpio_set_value(power_pin, 0);
#endif
#endif
}

void platform_hw_reset(u32 reset_pin)
{
#ifdef TAURUS
  /* UB */
#endif
#ifdef S5PV210
	gpio_set_value(reset_pin, 1);
	ms_wait(1);
	gpio_set_value(reset_pin, 0);
	ms_wait(1);
	gpio_set_value(reset_pin, 1);
	ms_wait(1);
#endif
}
