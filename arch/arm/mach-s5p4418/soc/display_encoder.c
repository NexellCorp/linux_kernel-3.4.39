/*
 * (C) Copyright 2009
 * jung hyun kim, Nexell Co, <jhkim@nexell.co.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <mach/platform.h>
#include <linux/platform_device.h>

#include <mach/devices.h>
#include <mach/soc.h>
#include "display_4418.h"

#if (0)
#define DBGOUT(msg...)		{ printk(KERN_INFO msg); }
#else
#define DBGOUT(msg...)		do {} while (0)
#endif
#define ERROUT(msg...)		{ printk(KERN_ERR msg); }

static void _release_hdmi_reset(void)
{
	NX_RSTCON_SetBaseAddress((void*)IO_ADDRESS(NX_RSTCON_GetPhysicalAddress()));
	NX_RSTCON_SetRST(RESETINDEX_OF_DISPLAYTOP_MODULE_i_HDMI_nRST, 1);
	NX_RSTCON_SetRST(RESETINDEX_OF_DISPLAYTOP_MODULE_i_HDMI_VIDEO_nRST, 1);
	NX_RSTCON_SetRST(RESETINDEX_OF_DISPLAYTOP_MODULE_i_HDMI_SPDIF_nRST, 1);
	NX_RSTCON_SetRST(RESETINDEX_OF_DISPLAYTOP_MODULE_i_HDMI_TMDS_nRST, 1);
	NX_RSTCON_SetRST(RESETINDEX_OF_DISPLAYTOP_MODULE_i_HDMI_PHY_nRST, 1);
}

static void _set_hdmi_clk_27MHz(void)
{
	NX_HDMI_SetBaseAddress(0, (void*)IO_ADDRESS(NX_HDMI_GetPhysicalAddress(0)));

	NX_TIEOFF_Initialize();
	NX_TIEOFF_SetBaseAddress((void*)IO_ADDRESS(NX_TIEOFF_GetPhysicalAddress()));
	NX_TIEOFF_Set(TIEOFFINDEX_OF_DISPLAYTOP0_i_HDMI_PHY_REFCLK_SEL, 1);

	// HDMI PCLK Enable
	NX_DISPTOP_CLKGEN_SetBaseAddress(HDMI_CLKGEN, (void*)IO_ADDRESS(NX_DISPTOP_CLKGEN_GetPhysicalAddress(HDMI_CLKGEN)));
	NX_DISPTOP_CLKGEN_SetClockPClkMode(HDMI_CLKGEN, NX_PCLKMODE_ALWAYS);

	// Enter Reset
	NX_RSTCON_SetRST  (NX_HDMI_GetResetNumber(0, i_nRST_PHY) , 0);
	NX_RSTCON_SetRST  (NX_HDMI_GetResetNumber(0, i_nRST)     , 0); // APB

	// Release Reset
	NX_RSTCON_SetRST  (NX_HDMI_GetResetNumber(0, i_nRST_PHY) , 1);
	NX_RSTCON_SetRST  (NX_HDMI_GetResetNumber(0, i_nRST)     , 1); // APB

	NX_DISPTOP_CLKGEN_SetClockPClkMode      (HDMI_CLKGEN, NX_PCLKMODE_ALWAYS);

	pr_info("%s - HDMI Address : 0x%x\n", __func__, HDMI_PHY_Reg7C);

	NX_HDMI_SetReg( 0, HDMI_PHY_Reg7C, (0<<7) );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg7C, (0<<7) );    /// MODE_SET_DONE : APB Set
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg04, (0<<4) );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg04, (0<<4) );    ///CLK_SEL : REF OSC or INT_CLK
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg24, (1<<7) );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg24, (1<<7) );    // INT REFCLK : ³»ºÎÀÇ syscon¿¡¼­ ¹Þ´Â clock
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg04, 0xD1   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg04, 0xD1   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg08, 0x22   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg08, 0x22   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg0C, 0x51   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg0C, 0x51   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg10, 0x40   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg10, 0x40   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg14, 0x8    );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg14, 0x8    );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg18, 0xFC   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg18, 0xFC   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg1C, 0xE0   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg1C, 0xE0   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg20, 0x98   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg20, 0x98   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg24, 0xE8   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg24, 0xE8   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg28, 0xCB   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg28, 0xCB   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg2C, 0xD8   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg2C, 0xD8   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg30, 0x45   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg30, 0x45   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg34, 0xA0   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg34, 0xA0   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg38, 0xAC   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg38, 0xAC   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg3C, 0x80   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg3C, 0x80   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg40, 0x6    );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg40, 0x6    );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg44, 0x80   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg44, 0x80   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg48, 0x9    );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg48, 0x9    );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg4C, 0x84   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg4C, 0x84   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg50, 0x5    );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg50, 0x5    );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg54, 0x22   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg54, 0x22   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg58, 0x24   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg58, 0x24   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg5C, 0x86   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg5C, 0x86   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg60, 0x54   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg60, 0x54   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg64, 0xE4   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg64, 0xE4   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg68, 0x24   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg68, 0x24   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg6C, 0x0    );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg6C, 0x0    );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg70, 0x0    );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg70, 0x0    );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg74, 0x0    );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg74, 0x0    );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg78, 0x1    );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg78, 0x1    );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg7C, 0x80   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg7C, 0x80   );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg7C, (1<<7) );
	NX_HDMI_SetReg( 0, HDMI_PHY_Reg7C, (1<<7) );    /// MODE_SET_DONE : APB Set Done

	// wait phy ready
	{
		U32 Is_HDMI_PHY_READY = CFALSE;
		while(Is_HDMI_PHY_READY == CFALSE) {
			if(NX_HDMI_GetReg( 0, HDMI_LINK_PHY_STATUS_0 ) & 0x01) {
				Is_HDMI_PHY_READY = CTRUE;
			}
		}
	}
}

static int encoder_set_vsync(struct disp_process_dev *pdev, struct disp_vsync_info *psync)
{
	RET_ASSERT_VAL(pdev && psync, -EINVAL);
	DBGOUT("%s: %s\n", __func__, dev_to_str(pdev->dev_id));

	pdev->status |= PROC_STATUS_READY;
	memcpy(&pdev->vsync, psync , sizeof(*psync));

	return 0;
}

static int encoder_get_vsync(struct disp_process_dev *pdev, struct disp_vsync_info *psync)
{
	printk("%s: %s\n", __func__, dev_to_str(pdev->dev_id));
	RET_ASSERT_VAL(pdev, -EINVAL);

	if (psync)
		memcpy(psync, &pdev->vsync, sizeof(*psync));

	return 0;
}

static int  encoder_prepare(struct disp_process_dev *pdev)
{
	struct disp_lcd_param *plcd = pdev->dev_param;
	int input = pdev->dev_in;
	int mpu = plcd->lcd_mpu_type;
	int rsc = 0, sel = 0;

	switch (input) {
	case DISP_DEVICE_SYNCGEN0:	input = 0; break;
	case DISP_DEVICE_SYNCGEN1:	input = 1; break;
	case DISP_DEVICE_RESCONV  :	input = 2; break;
	default:
		return -EINVAL;
	}

	switch (input) {
	case 0:	sel = mpu ? 1 : 0; break;
	case 1:	sel = rsc ? 3 : 2; break;
	default:
		printk("Fail, %s nuknown module %d\n", __func__, input);
		return -1;
	}

	NX_DISPLAYTOP_SetPrimaryMUX(sel);
	return 0;
}

static int  encoder_enable(struct disp_process_dev *pdev, int enable)
{
	struct disp_vsync_info *psync = &pdev->vsync;	
	int clk_src_lv0 = psync->clk_src_lv0;

	PM_DBGOUT("%s %s, %s\n", __func__, dev_to_str(pdev->dev_id), enable?"ON":"OFF");

	if (! enable) {
  		pdev->status &= ~PROC_STATUS_ENABLE;
	} else {
		encoder_prepare(pdev);

		if (clk_src_lv0 == 4) {	
			_release_hdmi_reset();
			_set_hdmi_clk_27MHz();
		}

		pdev->status |=  PROC_STATUS_ENABLE;
  	}
  	return 0;
}

static int  encoder_stat_enable(struct disp_process_dev *pdev)
{
	CBOOL ret = CFALSE;

	switch (pdev->dev_in) {
	case DISP_DEVICE_SYNCGEN0: ret = NX_DPC_GetDPCEnable(0); break;
	case DISP_DEVICE_SYNCGEN1: ret = NX_DPC_GetDPCEnable(1); break;
	case DISP_DEVICE_RESCONV : break;
	default:
		break;
	}

	if (CTRUE == ret)
		pdev->status |=  PROC_STATUS_ENABLE;
	else
		pdev->status &= ~PROC_STATUS_ENABLE;

	return pdev->status & PROC_STATUS_ENABLE ? 1 : 0;
}

static int  encoder_suspend(struct disp_process_dev *pdev)
{
	PM_DBGOUT("%s\n", __func__);
	return encoder_enable(pdev, 0);
}

static void encoder_resume(struct disp_process_dev *pdev)
{
	PM_DBGOUT("%s\n", __func__);
	encoder_enable(pdev, 1);
}

static struct disp_process_ops encoder_ops = {
	.set_vsync 	= encoder_set_vsync,
	.get_vsync  = encoder_get_vsync,
	.enable 	= encoder_enable,
	.stat_enable= encoder_stat_enable,
	.suspend	= encoder_suspend,
	.resume	  	= encoder_resume,
};

static int encoder_probe(struct platform_device *pdev)
{
	struct nxp_lcd_plat_data *plat = pdev->dev.platform_data;
	struct disp_lcd_param *plcd;
	struct disp_vsync_info *psync;
	struct disp_syncgen_par *sgpar;
	int device = DISP_DEVICE_LCD;
	int input;

	RET_ASSERT_VAL(plat, -EINVAL);
	RET_ASSERT_VAL(plat->display_in == DISP_DEVICE_SYNCGEN0 ||
				   plat->display_in == DISP_DEVICE_SYNCGEN1 ||
				   plat->display_dev == DISP_DEVICE_LCD ||
				   plat->display_in == DISP_DEVICE_RESCONV, -EINVAL);
	RET_ASSERT_VAL(plat->vsync, -EINVAL);

	plcd = kzalloc(sizeof(*plcd), GFP_KERNEL);
	RET_ASSERT_VAL(plcd, -EINVAL);

	if (plat->dev_param)
		memcpy(plcd, plat->dev_param, sizeof(*plcd));

	sgpar = plat->sync_gen;
	psync = plat->vsync;
	input = plat->display_in;

	nxp_soc_disp_register_proc_ops(device, &encoder_ops);
	nxp_soc_disp_device_connect_to(device, input, psync);
	nxp_soc_disp_device_set_dev_param(device, plcd);

	if (sgpar &&
		(input == DISP_DEVICE_SYNCGEN0 ||
		 input == DISP_DEVICE_SYNCGEN1))
		nxp_soc_disp_device_set_sync_param(input, sgpar);

	printk("Encoder : [%d]=%s connect to [%d]=%s\n",
		device, dev_to_str(device), input, dev_to_str(input));

	return 0;
}

static struct platform_driver encoder_driver = {
	.driver	= {
	.name	= DEV_NAME_LCD,
	.owner	= THIS_MODULE,
	},
	.probe	= encoder_probe,
};
module_platform_driver(encoder_driver);

MODULE_AUTHOR("jhkim <jhkim@nexell.co.kr>");
MODULE_DESCRIPTION("Display RGB driver for the Nexell");
MODULE_LICENSE("GPL");
