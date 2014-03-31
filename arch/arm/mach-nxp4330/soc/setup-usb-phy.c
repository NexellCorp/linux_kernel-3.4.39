/*
 * Copyright (C) 2013 Nexell Co.Ltd
 * Author: bongKwan Kook <kook@nexell.co.kr>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/platform_device.h>

#include <mach/usb-phy.h>

#include <mach/platform.h>
#include <mach/iomap.h>
#include <mach/nxp4330.h>
#include <mach/nxp4330_irq.h>
#include <nx_tieoff.h>

#define SOC_PA_RSTCON		PHY_BASEADDR_RSTCON
#define	SOC_VA_RSTCON		IO_ADDRESS(SOC_PA_RSTCON)

#define SOC_PA_TIEOFF		PHY_BASEADDR_TIEOFF
#define	SOC_VA_TIEOFF		IO_ADDRESS(SOC_PA_TIEOFF)


int nxp_usb_phy_init(struct platform_device *pdev, int type)
{
	PM_DBGOUT("++ %s\n", __func__);

	if (!pdev)
		return -EINVAL;

	if( type == NXP_USB_PHY_HOST )
	{
		// 1. Release common reset of host controller
		writel(readl(SOC_VA_RSTCON + 0x04) & ~(1<<24), SOC_VA_RSTCON + 0x04);			// reset on
		udelay(1);
		writel(readl(SOC_VA_RSTCON + 0x04) |  (1<<24), SOC_VA_RSTCON + 0x04);			// reset off

		// 2. Program AHB Burst type
		//writel(readl(SOC_VA_TIEOFF + 0x1c) & ~(7<<25), SOC_VA_TIEOFF + 0x1c);
		writel(readl(SOC_VA_TIEOFF + 0x1c) |  (7<<25), SOC_VA_TIEOFF + 0x1c);		// INCR16
		//writel(readl(SOC_VA_TIEOFF + 0x1c) |  (6<<25), SOC_VA_TIEOFF + 0x1c);		// INCR8
		//writel(readl(SOC_VA_TIEOFF + 0x1c) |  (4<<25), SOC_VA_TIEOFF + 0x1c);		// INCR4

		// 3. Select word interface and enable word interface selection
		//writel(readl(SOC_VA_TIEOFF + 0x14) & ~(3<<25), SOC_VA_TIEOFF + 0x14);
		writel(readl(SOC_VA_TIEOFF + 0x14) |  (3<<25), SOC_VA_TIEOFF + 0x14);		// 2'b01 8bit, 2'b11 16bit word

		//writel(readl(SOC_VA_TIEOFF + 0x24) & ~(3<<8), SOC_VA_TIEOFF + 0x24);
		writel(readl(SOC_VA_TIEOFF + 0x24) |  (3<<8), SOC_VA_TIEOFF + 0x24);		// 2'b01 8bit, 2'b11 16bit word

		// 4. POR of PHY
		writel(readl(SOC_VA_TIEOFF + 0x20) & ~(3<<7), SOC_VA_TIEOFF + 0x20);
		writel(readl(SOC_VA_TIEOFF + 0x20) |  (1<<7), SOC_VA_TIEOFF + 0x20);

		// Wait clock of PHY - about 40 micro seconds
		udelay(100); // 40us delay need.

		// 5. Release utmi reset
		//writel(readl(SOC_VA_TIEOFF + 0x14) & ~(3<<20), SOC_VA_TIEOFF + 0x14);
		writel(readl(SOC_VA_TIEOFF + 0x14) |  (3<<20), SOC_VA_TIEOFF + 0x14);

		//6. Release ahb reset of EHCI, OHCI
		//writel(readl(SOC_VA_TIEOFF + 0x14) & ~(7<<17), SOC_VA_TIEOFF + 0x14);
		writel(readl(SOC_VA_TIEOFF + 0x14) |  (7<<17), SOC_VA_TIEOFF + 0x14);
	}
	else if( type == NXP_USB_PHY_OTG )
	{
		u32 temp;

		// 1. Release otg common reset
		writel(readl(SOC_VA_RSTCON + 0x04) & ~(1<<25), SOC_VA_RSTCON + 0x04);     // reset on
		udelay(1);
		writel(readl(SOC_VA_RSTCON + 0x04) |  (1<<25), SOC_VA_RSTCON + 0x04);     // reset off

		// 2. Program scale mode to real mode
		writel(readl(SOC_VA_TIEOFF + 0x30) & ~(3<<0), SOC_VA_TIEOFF + 0x30);

		// 3. Select word interface and enable word interface selection
		//writel(readl(SOC_VA_TIEOFF + 0x38) & ~(3<<8), SOC_VA_TIEOFF + 0x38);
		writel(readl(SOC_VA_TIEOFF + 0x38) |  (3<<8), SOC_VA_TIEOFF + 0x38);        // 2'b01 8bit, 2'b11 16bit word

		// 4. Select VBUS
		temp    = readl(SOC_VA_TIEOFF + 0x34);
		temp   &= ~(3<<24);     /* Analog 5V */
//		temp   |=  (3<<24);     /* Digital 3.3V */
		writel(temp, SOC_VA_TIEOFF + 0x34);

		// 5. POR of PHY
		temp   &= ~(3<<7);
		temp   |=  (1<<7);
		writel(temp, SOC_VA_TIEOFF + 0x34);
		udelay(40); // 40us delay need.

		// 6. UTMI reset
		temp   |=  (1<<3);
		writel(temp, SOC_VA_TIEOFF + 0x34);
		udelay(1); // 10 clock need

		// 7. AHB reset
		temp   |=  (1<<2);
		writel(temp, SOC_VA_TIEOFF + 0x34);
		udelay(1); // 10 clock need
	}

	PM_DBGOUT("-- %s\n", __func__);

	return 0;
}

int nxp_usb_phy_exit(struct platform_device *pdev, int type)
{
	PM_DBGOUT("++ %s\n", __func__);

	if (!pdev)
		return -EINVAL;

	if (type == NXP_USB_PHY_HOST)
	{
#if 0
		// EHCI, OHCI reset on
		writel(readl(SOC_VA_RSTCON + 0x04) & ~(1<<24), SOC_VA_RSTCON + 0x04);
#else
		// EHCI, OHCI reset on
		writel(readl(SOC_VA_RSTCON + 0x04) & ~(1<<24), SOC_VA_RSTCON + 0x04);

		// 6. Release ahb reset of EHCI, OHCI
		writel(readl(SOC_VA_TIEOFF + 0x14) & ~(7<<17), SOC_VA_TIEOFF + 0x14);

		// 5. Release utmi reset
		writel(readl(SOC_VA_TIEOFF + 0x14) & ~(3<<20), SOC_VA_TIEOFF + 0x14);

		// 4. POR of PHY
		writel(readl(SOC_VA_TIEOFF + 0x20) |  (3<<7), SOC_VA_TIEOFF + 0x20);

		// EHCI, OHCI reset on
//		writel(readl(SOC_VA_RSTCON + 0x04) & ~(1<<24), SOC_VA_RSTCON + 0x04);
#endif
	}
	else if( type == NXP_USB_PHY_OTG )
	{
		u32 temp;

		temp    = readl(SOC_VA_TIEOFF + 0x34);

		temp   &= ~(1<<3);                      //nUtmiResetSync = 0
		writel(temp, SOC_VA_TIEOFF + 0x34);

		temp   &= ~(1<<2);                      //nResetSync = 0
		writel(temp, SOC_VA_TIEOFF + 0x34);

		temp   |=  (3<<7);                      //POR_ENB=1, POR=1
		writel(temp, SOC_VA_TIEOFF + 0x34);

		// OTG reset on
		writel(readl(SOC_VA_RSTCON + 0x04) & ~(1<<25), SOC_VA_RSTCON + 0x04);
	}

	PM_DBGOUT("-- %s\n", __func__);

	return 0;
}
