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
#include <linux/delay.h>
#include <mach/platform.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>

#include <mach/devices.h>
#include <mach/soc.h>
#include "display_4330.h"

#include <nx_hdmi.h>
#include <nx_rstcon.h>
#include <nx_displaytop.h>
#include <nx_disptop_clkgen.h>
#include <nx_ecid.h>
#include <nx_tieoff.h>

#include "regs-hdmi.h"
#include "hdmi-priv.h"

#if (0)
#define DBGOUT(msg...)		{ printk(KERN_INFO msg); }
#else
#define DBGOUT(msg...)		do {} while (0)
#endif

#define DEFAULT_SAMPLE_RATE             48000
#define DEFAULT_BITS_PER_SAMPLE         16
#define DEFAULT_AUDIO_CODEC             HDMI_AUDIO_PCM

#define NXP_HDMIPHY_PRESET_TABLE_SIZE   30

static const u8 hdmiphy_preset74_25[32] = {
    0xd1, 0x1f, 0x10, 0x40, 0x40, 0xf8, 0xc8, 0x81,
    0xe8, 0xba, 0xd8, 0x45, 0xa0, 0xac, 0x80, 0x56,
    0x80, 0x09, 0x84, 0x05, 0x22, 0x24, 0x86, 0x54,
    0xa5, 0x24, 0x01, 0x00, 0x00, 0x01, 0x10, 0x80,
};

static const u8 hdmiphy_preset148_5[32] = {
    0xd1, 0x1f, 0x00, 0x40, 0x40, 0xf8, 0xc8, 0x81,
    0xe8, 0xba, 0xd8, 0x45, 0xa0, 0xac, 0x80, 0x66,
    0x80, 0x09, 0x84, 0x05, 0x22, 0x24, 0x86, 0x54,
    0x4b, 0x25, 0x03, 0x00, 0x00, 0x01, 0x80,
};

enum NXP_HDMI_PRESET {
    NXP_HDMI_PRESET_720P = 0,   /* 1280 x 720 */
    NXP_HDMI_PRESET_1080P,      /* 1920 x 1080 */
    NXP_HDMI_PRESET_MAX
};

#define HDMI_PLUG_READY		(0<<0)
#define HDMI_PLUG_START		(1<<0)
#define HDMI_PLUG_DONE		(1<<1)

struct nxp_hdmi_context {
    bool initialized;
    unsigned int plug_in;

    u32 audio_codec;
    bool audio_enable;
    u32 audio_channel_count;
    int sample_rate;
    int bits_per_sample;

    u32 aspect;
    int color_range;
    bool is_dvi;
    int vic;

    enum NXP_HDMI_PRESET cur_preset;

    struct mutex mutex;

    u32 v_sync_start;
    u32 h_active_start;
    u32 h_active_end;
    u32 v_sync_hs_start_end0;
    u32 v_sync_hs_start_end1;

    u32 source_dpc_module_num;
    u32 width;
    u32 height;
    int source_device;
    int internal_irq;
    int external_irq;
    int connect;
    struct delayed_work hpd_work;
    enum disp_dev_type input;
};

#define	HDMI_PLUG(s)	(s==HDMI_PLUG_READY?"READY":(s==HDMI_PLUG_START?"START":"DONE"))

static struct nxp_hdmi_context *_context = NULL;

static int _hdmi_phy_enable(struct nxp_hdmi_context *me, int enable)
{
	const u8 *table;
	int size = 0;
	u32 addr, i = 0;

    if (enable) {
        switch (me->cur_preset) {
    	case NXP_HDMI_PRESET_720P:	table = hdmiphy_preset74_25; size = 32; break;
    	case NXP_HDMI_PRESET_1080P: table = hdmiphy_preset148_5; size = 31; break;
    	default: printk("hdmi: phy not support preset %d\n", me->cur_preset);
    	    return -EINVAL;
		}

    	NX_HDMI_SetReg(0, HDMI_PHY_Reg7C, (0<<7));
    	NX_HDMI_SetReg(0, HDMI_PHY_Reg7C, (0<<7));
    	NX_HDMI_SetReg(0, HDMI_PHY_Reg04, (0<<4));
    	NX_HDMI_SetReg(0, HDMI_PHY_Reg04, (0<<4));
    	NX_HDMI_SetReg(0, HDMI_PHY_Reg24, (1<<7));
    	NX_HDMI_SetReg(0, HDMI_PHY_Reg24, (1<<7));

    	for (i=0, addr=HDMI_PHY_Reg04; size > i; i++, addr+=4) {
    	    NX_HDMI_SetReg(0, addr, table[i]);
    	    NX_HDMI_SetReg(0, addr, table[i]);
		}

    	NX_HDMI_SetReg(0, HDMI_PHY_Reg7C, 0x80);
    	NX_HDMI_SetReg(0, HDMI_PHY_Reg7C, 0x80);
    	NX_HDMI_SetReg(0, HDMI_PHY_Reg7C, (1<<7));
    	NX_HDMI_SetReg(0, HDMI_PHY_Reg7C, (1<<7));
    	DBGOUT("hdmi phy %s (preset = %d)\n", enable?"ON":"OFF", me->cur_preset);
    }

    return 0;
}

static inline bool _wait_for_ecid_ready(void)
{
    int retry_count = 100;
    bool is_key_ready = false;

    NX_ECID_SetBaseAddress((U32)IO_ADDRESS(NX_ECID_GetPhysicalAddress()));

    do {
        is_key_ready = NX_ECID_GetKeyReady();
        if (is_key_ready) break;
        msleep(1);
        retry_count--;
    } while (retry_count > 0);

    return is_key_ready;
}

static inline void _hdmi_reset(void)
{
    NX_RSTCON_SetnRST(NX_HDMI_GetResetNumber(0, i_nRST_VIDEO), RSTCON_nDISABLE);
    NX_RSTCON_SetnRST(NX_HDMI_GetResetNumber(0, i_nRST_SPDIF), RSTCON_nDISABLE);
    NX_RSTCON_SetnRST(NX_HDMI_GetResetNumber(0, i_nRST_TMDS), RSTCON_nDISABLE);
    NX_RSTCON_SetnRST(NX_HDMI_GetResetNumber(0, i_nRST_VIDEO), RSTCON_nENABLE);
    NX_RSTCON_SetnRST(NX_HDMI_GetResetNumber(0, i_nRST_SPDIF), RSTCON_nENABLE);
    NX_RSTCON_SetnRST(NX_HDMI_GetResetNumber(0, i_nRST_TMDS), RSTCON_nENABLE);
}

static inline bool _wait_for_hdmiphy_ready(void)
{
    int retry_count = 500;
    do {
        u32 regval = NX_HDMI_GetReg(0, HDMI_LINK_PHY_STATUS_0);
        if (regval & 0x01) {
            DBGOUT("HDMI PHY Ready!!!\n");
            return true;
        }
        mdelay(10);
        retry_count--;
    } while (retry_count > 0);

    return false;
}

static inline int _get_vsync_info(int preset, int device,
				struct disp_vsync_info *vsync, struct disp_syncgen_par *par)
{
	if (vsync) {
		switch (preset) {
    	case NXP_HDMI_PRESET_720P:
    	    /* 720p: 1280x720 */
    	    vsync->h_active_len = 1280;
    	    vsync->h_sync_width = 40;
    	    vsync->h_back_porch = 220;
    	    vsync->h_front_porch = 110;
    	    vsync->h_sync_invert = 0;
    	    vsync->v_active_len = 720;
    	    vsync->v_sync_width = 5;
    	    vsync->v_back_porch = 20;
    	    vsync->v_front_porch = 5;
    	    vsync->v_sync_invert = 0;
    	    vsync->pixel_clock_hz = 74250000;
    	    break;

    	case NXP_HDMI_PRESET_1080P:
    	    /* 1080p: 1920x1080 */
    	    vsync->h_active_len = 1920;
    	    vsync->h_sync_width = 44;
    	    vsync->h_back_porch = 148;
    	    vsync->h_front_porch = 88;
    	    vsync->h_sync_invert = 0;
    	    vsync->v_active_len = 1080;
    	    vsync->v_sync_width = 5;
    	    vsync->v_back_porch = 36;
    	    vsync->v_front_porch = 4;
    	    vsync->v_sync_invert = 0;
    	    vsync->pixel_clock_hz = 148500000;
    	    break;

    	default:
    	    printk(KERN_ERR "%s: invalid preset value 0x%x\n", __func__, preset);
    	    return -EINVAL;
    	}

    	vsync->clk_src_lv0 = 4;
    	vsync->clk_div_lv0 = 1;
    	vsync->clk_src_lv1 = 7;
    	vsync->clk_div_lv1 = 1;
	}

	if (par) {
		nxp_soc_disp_device_get_sync_param(device, (void*)par);

		par->out_format	= OUTPUTFORMAT_RGB888;
		par->delay_mask = (DISP_SYNCGEN_DELAY_RGB_PVD | DISP_SYNCGEN_DELAY_HSYNC_CP1 |
						   DISP_SYNCGEN_DELAY_VSYNC_FRAM | DISP_SYNCGEN_DELAY_DE_CP);
		par->d_rgb_pvd = 0;
		par->d_hsync_cp1 = 0;
		par->d_vsync_fram = 0;
		par->d_de_cp2 = 7;

		//	HFP + HSW + HBP + AVWidth-VSCLRPIXEL- 1;
		par->vs_start_offset = (vsync->h_front_porch + vsync->h_sync_width +
						vsync->h_back_porch + vsync->h_active_len - 1);
		par->vs_end_offset = 0;
		// HFP + HSW + HBP + AVWidth-EVENVSCLRPIXEL- 1
		par->ev_start_offset = (vsync->h_front_porch + vsync->h_sync_width +
						vsync->h_back_porch + vsync->h_active_len - 1);
		par->ev_end_offset = 0;
	}

    return 0;
}

static int _hdmi_mux(struct nxp_hdmi_context *me)
{
    struct disp_vsync_info vsync;
    struct disp_syncgen_par sgpar;
    U32 HDMI_SEL = 0;
    int source_device = me->source_device;
    int ret;

	DBGOUT("%s from %s\n", __func__, dev_to_str(source_device));
    switch (source_device) {
		case DISP_DEVICE_SYNCGEN0: HDMI_SEL = PrimaryMLC;  		break;
		case DISP_DEVICE_SYNCGEN1: HDMI_SEL = SecondaryMLC; 	break;
   		case DISP_DEVICE_RESCONV:  HDMI_SEL = ResolutionConv; 	break;
    	default:
        	printk(KERN_ERR "%s: invalid source device %d\n", __func__, source_device);
        	return -EINVAL;
    }

    ret = _get_vsync_info(me->cur_preset, source_device, &vsync, &sgpar);
    if (ret) {
        printk(KERN_ERR "%s: failed to _get_vsync_info()\n", __func__);
        return ret;
    }

    /* vsync.interlace_scan = me->cur_conf->mbus_fmt.field == V4L2_FIELD_INTERLACED; */
    vsync.interlace = 0;

	ret = nxp_soc_disp_device_set_vsync_info(source_device, &vsync);
    ret = nxp_soc_disp_device_set_sync_param(source_device, (void*)&sgpar);
    if (ret) {
        pr_err("%s: failed to display parameter....\n", __func__);
        return ret;
    }

	NX_DISPLAYTOP_SetHDMIMUX(CTRUE, HDMI_SEL);

	return ret;
}

static void _hdmi_clock(void)
{
    NX_DISPTOP_CLKGEN_SetBaseAddress(ToMIPI_CLKGEN,
            (U32)IO_ADDRESS(NX_DISPTOP_CLKGEN_GetPhysicalAddress(ToMIPI_CLKGEN)));
    NX_DISPTOP_CLKGEN_SetClockDivisorEnable(ToMIPI_CLKGEN, CFALSE);
    NX_DISPTOP_CLKGEN_SetClockPClkMode(ToMIPI_CLKGEN, NX_PCLKMODE_ALWAYS);
    NX_DISPTOP_CLKGEN_SetClockSource(ToMIPI_CLKGEN, HDMI_SPDIF_CLKOUT, 2); // pll2
    NX_DISPTOP_CLKGEN_SetClockDivisor(ToMIPI_CLKGEN, HDMI_SPDIF_CLKOUT, 2);
    NX_DISPTOP_CLKGEN_SetClockSource(ToMIPI_CLKGEN, 1, 7);
    NX_DISPTOP_CLKGEN_SetClockDivisorEnable(ToMIPI_CLKGEN, CTRUE);

	// must initialize this !!
	NX_DISPLAYTOP_HDMI_SetVSyncHSStartEnd(0, 0);
	NX_DISPLAYTOP_HDMI_SetVSyncStart(0); // from posedge VSync
	NX_DISPLAYTOP_HDMI_SetHActiveStart(0); // from posedge HSync
	NX_DISPLAYTOP_HDMI_SetHActiveEnd(0); // from posedge HSync
}

static int _hdmi_setup(struct nxp_hdmi_context *me)
{
    u32 width, height;
    u32 hfp, hsw, hbp;
    u32 vfp, vsw, vbp;

    u32 h_blank, v_blank;
    u32 v_actline, v2_blank;
    u32 v_line, h_line;
    u32 h_sync_start, h_sync_end;
    u32 v_sync_line_bef_1;
    u32 v_sync_line_bef_2;

    u32 fixed_ffff = 0xffff;

    switch (me->cur_preset) {
    case NXP_HDMI_PRESET_720P:
        DBGOUT("%s: 720p\n", __func__);
        width = 1280;
        height = 720;
        hfp = 110;
        hsw = 40;
        hbp = 220;
        vfp = 5;
        vsw = 5;
        vbp = 20;
        break;
    case NXP_HDMI_PRESET_1080P:
        DBGOUT("%s: 1080p\n", __func__);
        width = 1920;
        height = 1080;
        hfp = 88;
        hsw = 44;
        hbp = 148;
        vfp = 4;
        vsw = 5;
        vbp = 36;
        break;
    default:
        printk(KERN_ERR "%s: invalid preset %d\n", __func__, me->cur_preset);
        return -EINVAL;
    }

    me->width = width;
    me->height = height;

    /**
     * calculate sync variables
     */
    h_blank = hfp + hsw + hbp;
    v_blank = vfp + vsw + vbp;
    v_actline = height;
    v2_blank = height + vfp + vsw + vbp;
    v_line = height + vfp + vsw + vbp; /* total v */
    h_line = width + hfp + hsw + hbp;  /* total h */
    h_sync_start = hfp;
    h_sync_end = hfp + hsw;
    v_sync_line_bef_1 = vfp;
    v_sync_line_bef_2 = vfp + vsw;

    /* for debugging */
    /* printk("h_blank %d, v_blank %d, v_actline %d, v2_blank %d, v_line %d, h_line %d, h_sync_start %d, h_sync_end %d, v_sync_line_bef_1 %d, v_sync_line_bef_2 %d\n", */
    /*         h_blank, v_blank, v_actline, v2_blank, v_line, h_line, h_sync_start, h_sync_end, v_sync_line_bef_1, v_sync_line_bef_2); */

    /* no blue screen mode, encoding order as it is */
    NX_HDMI_SetReg(0, HDMI_LINK_HDMI_CON_0, (0<<5)|(1<<4));

    /* set HDMI_LINK_BLUE_SCREEN_* to 0x0 */
    NX_HDMI_SetReg(0, HDMI_LINK_BLUE_SCREEN_R_0, 0x5555);
    NX_HDMI_SetReg(0, HDMI_LINK_BLUE_SCREEN_R_1, 0x5555);
    NX_HDMI_SetReg(0, HDMI_LINK_BLUE_SCREEN_G_0, 0x5555);
    NX_HDMI_SetReg(0, HDMI_LINK_BLUE_SCREEN_G_1, 0x5555);
    NX_HDMI_SetReg(0, HDMI_LINK_BLUE_SCREEN_B_0, 0x5555);
    NX_HDMI_SetReg(0, HDMI_LINK_BLUE_SCREEN_B_1, 0x5555);

    /* set HDMI_CON_1 to 0x0 */
    NX_HDMI_SetReg(0, HDMI_LINK_HDMI_CON_1, 0x0);
    NX_HDMI_SetReg(0, HDMI_LINK_HDMI_CON_2, 0x0);

    /* set interrupt : enable hpd_plug, hpd_unplug */
    NX_HDMI_SetReg(0, HDMI_LINK_INTC_CON_0, (1<<6)|(1<<3)|(1<<2));

    /* set STATUS_EN to 0x17 */
    NX_HDMI_SetReg(0, HDMI_LINK_STATUS_EN, 0x17);

    /* TODO set HDP to 0x0 : later check hpd */
    NX_HDMI_SetReg(0, HDMI_LINK_HPD, 0x0);

    /* set MODE_SEL to 0x02 */
    NX_HDMI_SetReg(0, HDMI_LINK_MODE_SEL, 0x2);

    /* set H_BLANK_*, V1_BLANK_*, V2_BLANK_*, V_LINE_*, H_LINE_*, H_SYNC_START_*, H_SYNC_END_ *
     * V_SYNC_LINE_BEF_1_*, V_SYNC_LINE_BEF_2_*
     */
    NX_HDMI_SetReg(0, HDMI_LINK_H_BLANK_0, h_blank%256);
    NX_HDMI_SetReg(0, HDMI_LINK_H_BLANK_1, h_blank>>8);
    NX_HDMI_SetReg(0, HDMI_LINK_V1_BLANK_0, v_blank%256);
    NX_HDMI_SetReg(0, HDMI_LINK_V1_BLANK_1, v_blank>>8);
    NX_HDMI_SetReg(0, HDMI_LINK_V2_BLANK_0, v2_blank%256);
    NX_HDMI_SetReg(0, HDMI_LINK_V2_BLANK_1, v2_blank>>8);
    NX_HDMI_SetReg(0, HDMI_LINK_V_LINE_0, v_line%256);
    NX_HDMI_SetReg(0, HDMI_LINK_V_LINE_1, v_line>>8);
    NX_HDMI_SetReg(0, HDMI_LINK_H_LINE_0, h_line%256);
    NX_HDMI_SetReg(0, HDMI_LINK_H_LINE_1, h_line>>8);

    if (width == 1280) {
        NX_HDMI_SetReg(0, HDMI_LINK_HSYNC_POL, 0x1);
        NX_HDMI_SetReg(0, HDMI_LINK_VSYNC_POL, 0x1);
    } else {
        NX_HDMI_SetReg(0, HDMI_LINK_HSYNC_POL, 0x0);
        NX_HDMI_SetReg(0, HDMI_LINK_VSYNC_POL, 0x0);
    }

    NX_HDMI_SetReg(0, HDMI_LINK_INT_PRO_MODE, 0x0);

    NX_HDMI_SetReg(0, HDMI_LINK_H_SYNC_START_0, (h_sync_start%256)-2);
    NX_HDMI_SetReg(0, HDMI_LINK_H_SYNC_START_1, h_sync_start>>8);
    NX_HDMI_SetReg(0, HDMI_LINK_H_SYNC_END_0, (h_sync_end%256)-2);
    NX_HDMI_SetReg(0, HDMI_LINK_H_SYNC_END_1, h_sync_end>>8);
    NX_HDMI_SetReg(0, HDMI_LINK_V_SYNC_LINE_BEF_1_0, v_sync_line_bef_1%256);
    NX_HDMI_SetReg(0, HDMI_LINK_V_SYNC_LINE_BEF_1_1, v_sync_line_bef_1>>8);
    NX_HDMI_SetReg(0, HDMI_LINK_V_SYNC_LINE_BEF_2_0, v_sync_line_bef_2%256);
    NX_HDMI_SetReg(0, HDMI_LINK_V_SYNC_LINE_BEF_2_1, v_sync_line_bef_2>>8);

    /* Set V_SYNC_LINE_AFT*, V_SYNC_LINE_AFT_PXL*, VACT_SPACE* */
    NX_HDMI_SetReg(0, HDMI_LINK_V_SYNC_LINE_AFT_1_0, fixed_ffff%256);
    NX_HDMI_SetReg(0, HDMI_LINK_V_SYNC_LINE_AFT_1_1, fixed_ffff>>8);
    NX_HDMI_SetReg(0, HDMI_LINK_V_SYNC_LINE_AFT_2_0, fixed_ffff%256);
    NX_HDMI_SetReg(0, HDMI_LINK_V_SYNC_LINE_AFT_2_1, fixed_ffff>>8);
    NX_HDMI_SetReg(0, HDMI_LINK_V_SYNC_LINE_AFT_3_0, fixed_ffff%256);
    NX_HDMI_SetReg(0, HDMI_LINK_V_SYNC_LINE_AFT_3_1, fixed_ffff>>8);
    NX_HDMI_SetReg(0, HDMI_LINK_V_SYNC_LINE_AFT_4_0, fixed_ffff%256);
    NX_HDMI_SetReg(0, HDMI_LINK_V_SYNC_LINE_AFT_4_1, fixed_ffff>>8);
    NX_HDMI_SetReg(0, HDMI_LINK_V_SYNC_LINE_AFT_5_0, fixed_ffff%256);
    NX_HDMI_SetReg(0, HDMI_LINK_V_SYNC_LINE_AFT_5_1, fixed_ffff>>8);
    NX_HDMI_SetReg(0, HDMI_LINK_V_SYNC_LINE_AFT_6_0, fixed_ffff%256);
    NX_HDMI_SetReg(0, HDMI_LINK_V_SYNC_LINE_AFT_6_1, fixed_ffff>>8);

    NX_HDMI_SetReg(0, HDMI_LINK_V_SYNC_LINE_AFT_PXL_1_0, fixed_ffff%256);
    NX_HDMI_SetReg(0, HDMI_LINK_V_SYNC_LINE_AFT_PXL_1_1, fixed_ffff>>8);
    NX_HDMI_SetReg(0, HDMI_LINK_V_SYNC_LINE_AFT_PXL_2_0, fixed_ffff%256);
    NX_HDMI_SetReg(0, HDMI_LINK_V_SYNC_LINE_AFT_PXL_2_1, fixed_ffff>>8);
    NX_HDMI_SetReg(0, HDMI_LINK_V_SYNC_LINE_AFT_PXL_3_0, fixed_ffff%256);
    NX_HDMI_SetReg(0, HDMI_LINK_V_SYNC_LINE_AFT_PXL_3_1, fixed_ffff>>8);
    NX_HDMI_SetReg(0, HDMI_LINK_V_SYNC_LINE_AFT_PXL_4_0, fixed_ffff%256);
    NX_HDMI_SetReg(0, HDMI_LINK_V_SYNC_LINE_AFT_PXL_4_1, fixed_ffff>>8);
    NX_HDMI_SetReg(0, HDMI_LINK_V_SYNC_LINE_AFT_PXL_5_0, fixed_ffff%256);
    NX_HDMI_SetReg(0, HDMI_LINK_V_SYNC_LINE_AFT_PXL_5_1, fixed_ffff>>8);
    NX_HDMI_SetReg(0, HDMI_LINK_V_SYNC_LINE_AFT_PXL_6_0, fixed_ffff%256);
    NX_HDMI_SetReg(0, HDMI_LINK_V_SYNC_LINE_AFT_PXL_6_1, fixed_ffff>>8);

    NX_HDMI_SetReg(0, HDMI_LINK_VACT_SPACE1_0, fixed_ffff%256);
    NX_HDMI_SetReg(0, HDMI_LINK_VACT_SPACE1_1, fixed_ffff>>8);
    NX_HDMI_SetReg(0, HDMI_LINK_VACT_SPACE2_0, fixed_ffff%256);
    NX_HDMI_SetReg(0, HDMI_LINK_VACT_SPACE2_1, fixed_ffff>>8);
    NX_HDMI_SetReg(0, HDMI_LINK_VACT_SPACE3_0, fixed_ffff%256);
    NX_HDMI_SetReg(0, HDMI_LINK_VACT_SPACE3_1, fixed_ffff>>8);
    NX_HDMI_SetReg(0, HDMI_LINK_VACT_SPACE4_0, fixed_ffff%256);
    NX_HDMI_SetReg(0, HDMI_LINK_VACT_SPACE4_1, fixed_ffff>>8);
    NX_HDMI_SetReg(0, HDMI_LINK_VACT_SPACE5_0, fixed_ffff%256);
    NX_HDMI_SetReg(0, HDMI_LINK_VACT_SPACE5_1, fixed_ffff>>8);
    NX_HDMI_SetReg(0, HDMI_LINK_VACT_SPACE6_0, fixed_ffff%256);
    NX_HDMI_SetReg(0, HDMI_LINK_VACT_SPACE6_1, fixed_ffff>>8);

    NX_HDMI_SetReg(0, HDMI_LINK_CSC_MUX, 0x0);
    NX_HDMI_SetReg(0, HDMI_LINK_SYNC_GEN_MUX, 0x0);

    NX_HDMI_SetReg(0, HDMI_LINK_SEND_START_0, 0xfd);
    NX_HDMI_SetReg(0, HDMI_LINK_SEND_START_1, 0x01);
    NX_HDMI_SetReg(0, HDMI_LINK_SEND_END_0, 0x0d);
    NX_HDMI_SetReg(0, HDMI_LINK_SEND_END_1, 0x3a);
    NX_HDMI_SetReg(0, HDMI_LINK_SEND_END_2, 0x08);

    /* Set DC_CONTROL to 0x00 */
    NX_HDMI_SetReg(0, HDMI_LINK_DC_CONTROL, 0x0);

    /* Set VIDEO_PATTERN_GEN to 0x00 */
#if (1)
    NX_HDMI_SetReg(0, HDMI_LINK_VIDEO_PATTERN_GEN, 0x0);
#else
    NX_HDMI_SetReg(0, HDMI_LINK_VIDEO_PATTERN_GEN, 0x1);
#endif

    NX_HDMI_SetReg(0, HDMI_LINK_GCP_CON, 0x0a);

    /* Set HDMI Sync Control parameters */
    me->v_sync_start = vsw + vbp + height - 1;
    me->h_active_start = hsw + hbp;
    me->h_active_end = width + hsw + hbp;
    me->v_sync_hs_start_end0 = hsw + hbp + 1;
    me->v_sync_hs_start_end1 = hsw + hbp + 2;

    return 0;
}

static void _hdmi_reg_infoframe(struct nxp_hdmi_context *me,
        struct hdmi_infoframe *infoframe,
        const struct hdmi_3d_info *info_3d)
{
    u32 hdr_sum;
    u8 chksum;
    u32 aspect_ratio;
    u32 vic;

    DBGOUT("%s: infoframe type = 0x%x\n", __func__, infoframe->type);

    if (me->is_dvi) {
        hdmi_writeb(HDMI_VSI_CON, HDMI_VSI_CON_DO_NOT_TRANSMIT);
        hdmi_writeb(HDMI_AVI_CON, HDMI_AVI_CON_DO_NOT_TRANSMIT);
        hdmi_write(HDMI_AUI_CON, HDMI_AUI_CON_NO_TRAN);
        return;
    }

    switch (infoframe->type) {
    case HDMI_PACKET_TYPE_VSI:
        hdmi_writeb(HDMI_VSI_CON, HDMI_VSI_CON_EVERY_VSYNC);
        hdmi_writeb(HDMI_VSI_HEADER0, infoframe->type);
        hdmi_writeb(HDMI_VSI_HEADER1, infoframe->ver);
        /* 0x000C03 : 24bit IEEE Registration Identifier */
        hdmi_writeb(HDMI_VSI_DATA(1), 0x03);
        hdmi_writeb(HDMI_VSI_DATA(2), 0x0c);
        hdmi_writeb(HDMI_VSI_DATA(3), 0x00);
        hdmi_writeb(HDMI_VSI_DATA(4),
                HDMI_VSI_DATA04_VIDEO_FORMAT(info_3d->is_3d));
        hdmi_writeb(HDMI_VSI_DATA(5),
                HDMI_VSI_DATA05_3D_STRUCTURE(info_3d->fmt_3d));
        if (info_3d->fmt_3d == HDMI_3D_FORMAT_SB_HALF) {
            infoframe->len += 1;
            hdmi_writeb(HDMI_VSI_DATA(6),
                    (u8)HDMI_VSI_DATA06_3D_EXT_DATA(HDMI_H_SUB_SAMPLE));
        }
        hdmi_writeb(HDMI_VSI_HEADER2, infoframe->len);
        hdr_sum = infoframe->type + infoframe->ver + infoframe->len;
        chksum = soc_hdmi_chksum(HDMI_VSI_DATA(1), infoframe->len, hdr_sum);
        DBGOUT("%s: VSI checksum = 0x%x\n", __func__, chksum);
        hdmi_writeb(HDMI_VSI_DATA(0), chksum);
        break;

    case HDMI_PACKET_TYPE_AVI:
        hdmi_writeb(HDMI_AVI_CON, HDMI_AVI_CON_EVERY_VSYNC);
        hdmi_writeb(HDMI_AVI_HEADER0, infoframe->type);
        hdmi_writeb(HDMI_AVI_HEADER1, infoframe->ver);
        hdmi_writeb(HDMI_AVI_HEADER2, infoframe->len);
        hdr_sum = infoframe->type + infoframe->ver + infoframe->len;
        hdmi_writeb(HDMI_AVI_BYTE(1), HDMI_OUTPUT_RGB888 << 5 |
                AVI_ACTIVE_FORMAT_VALID | AVI_UNDERSCAN);
        aspect_ratio = AVI_PIC_ASPECT_RATIO_16_9;
        vic = me->vic;

        hdmi_writeb(HDMI_AVI_BYTE(2), aspect_ratio |
                AVI_SAME_AS_PIC_ASPECT_RATIO | AVI_ITU709);
        if (me->color_range == 0 || me->color_range == 2)
            hdmi_writeb(HDMI_AVI_BYTE(3), AVI_FULL_RANGE);
        else
            hdmi_writeb(HDMI_AVI_BYTE(3), AVI_LIMITED_RANGE);
        DBGOUT("%s: VIC code = %d\n", __func__, vic);
        hdmi_writeb(HDMI_AVI_BYTE(4), vic);
        chksum = soc_hdmi_chksum(HDMI_AVI_BYTE(1), infoframe->len, hdr_sum);
        DBGOUT("%s: AVI checksum = 0x%x\n", __func__, chksum);
        hdmi_writeb(HDMI_AVI_CHECK_SUM, chksum);
        break;

    case HDMI_PACKET_TYPE_AUI:
        hdmi_write(HDMI_AUI_CON, HDMI_AUI_CON_TRANS_EVERY_VSYNC);
        hdmi_writeb(HDMI_AUI_HEADER0, infoframe->type);
        hdmi_writeb(HDMI_AUI_HEADER1, infoframe->ver);
        hdmi_writeb(HDMI_AUI_HEADER2, infoframe->len);
        hdr_sum = infoframe->type + infoframe->ver + infoframe->len;
        /* speaker placement */
        if (me->audio_channel_count == 6)
            hdmi_writeb(HDMI_AUI_BYTE(4), 0x0b);
        else if (me->audio_channel_count == 8)
            hdmi_writeb(HDMI_AUI_BYTE(4), 0x13);
        else
            hdmi_writeb(HDMI_AUI_BYTE(4), 0x00);
        chksum = soc_hdmi_chksum(HDMI_AUI_BYTE(1), infoframe->len, hdr_sum);
        DBGOUT("%s: AUI checksum = 0x%x\n", __func__, chksum);
        hdmi_writeb(HDMI_AUI_CHECK_SUM, chksum);
        break;

    default:
        pr_err("%s: unknown type(0x%x)\n", __func__, infoframe->type);
        break;
    }
}

static int _hdmi_set_infoframe(struct nxp_hdmi_context *me)
{
    struct hdmi_infoframe infoframe;
    struct hdmi_3d_info info_3d;

    info_3d.is_3d = 0;

    soc_hdmi_stop_vsi();

    infoframe.type = HDMI_PACKET_TYPE_AVI;
    infoframe.ver  = HDMI_AVI_VERSION;
    infoframe.len  = HDMI_AVI_LENGTH;
    _hdmi_reg_infoframe(me, &infoframe, &info_3d);

    if (me->audio_enable) {
        infoframe.type = HDMI_PACKET_TYPE_AUI;
        infoframe.ver  = HDMI_AUI_VERSION;
        infoframe.len  = HDMI_AUI_LENGTH;
        _hdmi_reg_infoframe(me, &infoframe, &info_3d);
    }

    return 0;
}

static void _hdmi_set_packets(struct nxp_hdmi_context *me)
{
    soc_hdmi_set_acr(me->sample_rate, me->is_dvi);
}

static int _hdmi_set_preset(int preset)
{
    struct nxp_hdmi_context *me = _context;

    if (!me) {
        printk(KERN_ERR "hdmi soc driver not probed!!!\n");
        return -ENODEV;
    }

    if (preset >= NXP_HDMI_PRESET_MAX) {
        printk(KERN_ERR "%s: invalid preset %d\n", __func__, preset);
        return -EINVAL;
    }

    return 0;
}

void _hdmi_initialize(void)
{
    struct nxp_hdmi_context *me = _context;

    if (!me) {
        printk(KERN_ERR "hdmi soc driver not probed!!!\n");
        return;
    }

    if (!me->initialized) {
        /**
         * [SEQ 1] release the reset of DisplayTop.i_Top_nRst)
         */
	#if 0
        NX_RSTCON_SetnRST(NX_DISPLAYTOP_GetResetNumber(), RSTCON_nDISABLE);
        NX_RSTCON_SetnRST(NX_DISPLAYTOP_GetResetNumber(), RSTCON_nENABLE);
	#endif

        /**
         * [SEQ 2] set the HDMI CLKGEN's PCLKMODE to always enabled
         */
        NX_DISPTOP_CLKGEN_SetBaseAddress(HDMI_CLKGEN,
                (U32)IO_ADDRESS(NX_DISPTOP_CLKGEN_GetPhysicalAddress(HDMI_CLKGEN)));
        NX_DISPTOP_CLKGEN_SetClockPClkMode(HDMI_CLKGEN, NX_PCLKMODE_ALWAYS);

        NX_HDMI_SetBaseAddress(0, (U32)IO_ADDRESS(NX_HDMI_GetPhysicalAddress(0)));
        NX_HDMI_Initialize();

        /**
         * [SEQ 3] set the 0xC001100C[0] to 1
         */
        /* NX_TIEOFF_Initialize(); */
        NX_TIEOFF_SetBaseAddress((U32)IO_ADDRESS(NX_TIEOFF_GetPhysicalAddress()));
        NX_TIEOFF_Set(TIEOFFINDEX_OF_DISPLAYTOP0_i_HDMI_PHY_REFCLK_SEL, 1);

        /**
         * [SEQ 4] release the resets of HDMI.i_PHY_nRST and HDMI.i_nRST
         */
        NX_RSTCON_SetnRST(NX_HDMI_GetResetNumber(0, i_nRST_PHY), RSTCON_nDISABLE);
        NX_RSTCON_SetnRST(NX_HDMI_GetResetNumber(0, i_nRST), RSTCON_nDISABLE);
        NX_RSTCON_SetnRST(NX_HDMI_GetResetNumber(0, i_nRST_PHY), RSTCON_nENABLE);
        NX_RSTCON_SetnRST(NX_HDMI_GetResetNumber(0, i_nRST), RSTCON_nENABLE);

        me->initialized = true;

        /**
         * Next sequence start in _hdmi_prepare()
         */
    }
}

/* int nxp_soc_disp_hdmi_streamon(struct nxp_hdmi_context *me) */
static int _hdmi_prepare(struct nxp_hdmi_context *me)
{
    int ret = 0;

	mutex_lock(&me->mutex);

	me->plug_in = HDMI_PLUG_START;
	DBGOUT("hdmi prepare plugin=%d, plug=%s\n", me->connect, HDMI_PLUG(me->plug_in));

    /**
     * [SEQ 5] set up the HDMI PHY to specific video clock.
     */
    ret = _hdmi_phy_enable(me, 1);
    if (ret < 0) {
        printk(KERN_ERR "%s: _hdmi_phy_enable() failed\n", __func__);
    	goto _end_prepare;
    }

    /**
     * [SEQ 6] I2S (or SPDIFTX) configuration for the source audio data
     * this is done in another user app  - ex> Android Audio HAL
     */

    /**
     * [SEQ 7] Wait for ECID ready
     */
    if(false == _wait_for_ecid_ready()) {
        printk(KERN_ERR "%s: failed to wait for ecid ready\n", __func__);
        _hdmi_phy_enable(me, 0);
        ret = -EIO;
        goto _end_prepare;
    }

    /**
     * [SEQ 8] release the resets of HDMI.i_VIDEO_nRST and HDMI.i_SPDIF_nRST and HDMI.i_TMDS_nRST
     */
    _hdmi_reset();

    /**
     * [SEQ 9] Wait for HDMI PHY ready (wait until 0xC0200020.[0], 1)
     */
    if (false == _wait_for_hdmiphy_ready()) {
        printk(KERN_ERR "%s: failed to wait for hdmiphy ready\n", __func__);
        _hdmi_phy_enable(me, 0);
        ret = -EIO;
        goto _end_prepare;
    }

	/*set mux */
    _hdmi_mux(me);

    /**
     * [SEC 10] Set the DPC CLKGEN’s Source Clock to HDMI_CLK & Set Sync Parameter
     */
    _hdmi_clock(); /* set hdmi link clk to clkgen  vs default is hdmi phy clk */

    /**
     * [SEQ 11] Set up the HDMI Converter parameters
     */
    _hdmi_setup(me);
    _hdmi_set_infoframe(me);
    _hdmi_set_packets(me);
    soc_hdmi_audio_spdif_init(me->audio_codec, me->bits_per_sample);

    if (me->audio_enable)
        soc_hdmi_audio_enable(true);


_end_prepare:

	if (0 > ret)
		me->plug_in = HDMI_PLUG_READY;

	mutex_unlock(&me->mutex);

    return ret;
}

static void _hdmi_start(struct nxp_hdmi_context *me)
{
    u32 regval;

    mutex_lock(&me->mutex);

	DBGOUT("hdmi start connect=%d, plug=%s\n", me->connect, HDMI_PLUG(me->plug_in));
	if (HDMI_PLUG_START != me->plug_in) {
		printk("Fail, hdmi not prepare... (%d, %s)\n", me->connect, HDMI_PLUG(me->plug_in));
		goto _end_start;
	}

    regval = NX_HDMI_GetReg(0, HDMI_LINK_HDMI_CON_0)| 0x01;
    NX_HDMI_SetReg(0, HDMI_LINK_HDMI_CON_0, regval);

    NX_DISPLAYTOP_HDMI_SetVSyncStart(me->v_sync_start);
    NX_DISPLAYTOP_HDMI_SetHActiveStart(me->h_active_start);
    NX_DISPLAYTOP_HDMI_SetHActiveEnd(me->h_active_end);
    NX_DISPLAYTOP_HDMI_SetVSyncHSStartEnd(me->v_sync_hs_start_end0, me->v_sync_hs_start_end1);

	mdelay(5);
    me->plug_in = HDMI_PLUG_DONE;

_end_start:
    mutex_unlock(&me->mutex);
}

static void _hdmi_stop(struct nxp_hdmi_context *me)
{
	mutex_lock(&me->mutex);
	DBGOUT("hdmi stop plugin=%d, plug=%s\n", me->connect, HDMI_PLUG(me->plug_in));

   	_hdmi_phy_enable(me, 0);
   	me->plug_in = HDMI_PLUG_READY;

	mutex_unlock(&me->mutex);
}

static int _hdmi_in_enable(int enable)
{
	struct nxp_hdmi_context *me = _context;
	return nxp_soc_disp_device_enable(me->source_device, enable);
}

static void _hdmi_hpd_work(struct work_struct *work)
{
    struct nxp_hdmi_context *me = _context;
	DBGOUT("hdmi detect plugin=%d, plug=%s\n", me->connect, HDMI_PLUG(me->plug_in));

	if (me->connect) {
		if (HDMI_PLUG_READY != me->plug_in)
			return;

		_hdmi_prepare(me);
		_hdmi_in_enable(1);
		_hdmi_start(me);
	} else {
		_hdmi_stop(me);
		_hdmi_in_enable(0);
	}
}

static irqreturn_t _hdmi_irq_handler(int irq, void *dev_data)
{
    struct nxp_hdmi_context *me = _context;
    u32 flag;

    /* flag = NX_HDMI_GetReg(0, HDMI_LINK_INTC_FLAG_0); */
    flag = hdmi_read(HDMI_LINK_INTC_FLAG_0);
    printk("hdmi [%s]\n", flag&HDMI_INTC_FLAG_HPD_UNPLUG?"UN PLUG":"PLUG IN");

    if (flag & HDMI_INTC_FLAG_HPD_UNPLUG) {
		hdmi_write_mask(HDMI_LINK_INTC_FLAG_0, ~0, HDMI_INTC_FLAG_HPD_UNPLUG);
        me->connect = 0;
    }
    if (flag & HDMI_INTC_FLAG_HPD_PLUG) {
        /* ignore plug in interrupt */
		hdmi_write_mask(HDMI_LINK_INTC_FLAG_0, ~0, HDMI_INTC_FLAG_HPD_PLUG);
        me->connect = 1;
    }

    /* queue_work(system_nrt_wq, &me->hpd_work); */
    queue_delayed_work(system_nrt_wq, &me->hpd_work, msecs_to_jiffies(1000));

    return IRQ_HANDLED;
}

static int hdmi_get_vsync(struct disp_process_dev *pdev, struct disp_vsync_info *psync)
{
	struct nxp_hdmi_context *me = _context;
	int source_device = me->source_device;
	RET_ASSERT_VAL(pdev && psync, -EINVAL);
	DBGOUT("%s: %s\n", __func__, dev_to_str(pdev->dev_id));

	return _get_vsync_info(me->cur_preset, source_device, psync, NULL);
}

static int  hdmi_prepare(struct disp_process_dev *dev)
{
	struct nxp_hdmi_context *me = _context;
	int ret = -1;
	DBGOUT("%s %s\n", __func__, dev_to_str(dev->dev_id));

	if (me->connect)
		ret = _hdmi_prepare(me);

	return ret;
}

static int  hdmi_enable(struct disp_process_dev *dev, int enable)
{
    struct nxp_hdmi_context *me = _context;
	DBGOUT("%s %s, %s\n", __func__, dev_to_str(dev->dev_id), enable?"ON":"OFF");

	if (enable && me->connect)
        _hdmi_start(me);
 	else
 		_hdmi_stop(me);

	return 0;
}

static int  hdmi_stat_enable(struct disp_process_dev *pdev)
{
	return pdev->status & PROC_STATUS_ENABLE ? 1 : 0;
}

static int  hdmi_suspend(struct disp_process_dev *pdev)
{
	struct nxp_hdmi_context *me = _context;
	PM_DBGOUT("%s\n", __func__);

	_hdmi_stop(me);

	return 0;
}

static void hdmi_pre_resume(struct disp_process_dev *pdev)
{
	struct nxp_hdmi_context *me = _context;
	PM_DBGOUT("%s\n", __func__);

	me->initialized = false;
	_hdmi_initialize();
	_hdmi_set_preset(me->cur_preset);

	disable_irq(me->internal_irq);
    NX_HDMI_SetReg(0, HDMI_LINK_INTC_CON_0, (1<<6)|(1<<3)|(1<<2));	/* irq */

	if (me->connect)
		_hdmi_prepare(me);
}

static void hdmi_resume(struct disp_process_dev *pdev)
{
	struct nxp_hdmi_context *me = _context;
	PM_DBGOUT("%s\n", __func__);

	enable_irq(me->internal_irq);

	if (me->connect)
		_hdmi_start(me);
	else
		_hdmi_stop(me);
}

static struct disp_process_ops hdmi_ops = {
	.get_vsync 		= hdmi_get_vsync,
	.prepare 		= hdmi_prepare,
	.enable 		= hdmi_enable,
	.stat_enable 	= hdmi_stat_enable,
	.suspend 		= hdmi_suspend,
	.pre_resume		= hdmi_pre_resume,
	.resume 		= hdmi_resume,
};

static int hdmi_probe(struct platform_device *pdev)
{
	struct nxp_lcd_plat_data *plat = pdev->dev.platform_data;
	struct disp_hdmi_param *phdmi;
    struct nxp_hdmi_context *ctx = NULL;
    struct disp_vsync_info vsync;
    struct disp_syncgen_par sgpar;
    int device = DISP_DEVICE_HDMI;
	int ret = 0;

	RET_ASSERT_VAL(plat, -EINVAL);
	RET_ASSERT_VAL(plat->display_in == DISP_DEVICE_SYNCGEN0 ||
				   plat->display_in == DISP_DEVICE_SYNCGEN1 ||
				   plat->display_dev == DISP_DEVICE_HDMI ||
				   plat->display_in == DISP_DEVICE_RESCONV, -EINVAL);

    ctx = (struct nxp_hdmi_context *)kzalloc(sizeof(struct nxp_hdmi_context), GFP_KERNEL);
    if (!ctx) {
        printk(KERN_ERR "failed to allocation nxp_hdmi_context\n");
        return -ENOMEM;
    }

	phdmi = kzalloc(sizeof(*phdmi), GFP_KERNEL);
	RET_ASSERT_VAL(phdmi, -EINVAL);

	if (plat->dev_param)
		memcpy(phdmi, plat->dev_param, sizeof(*phdmi));

    hdmi_set_base((void *)IO_ADDRESS(NX_HDMI_GetPhysicalAddress(0)));

    _context = ctx;
    _context->cur_preset = phdmi ? phdmi->preset : 0;
    _context->source_device = plat->display_in;

    mutex_init(&_context->mutex);

    /* audio */
    _context->audio_enable = true;
    _context->audio_channel_count = 2;
    _context->sample_rate = DEFAULT_SAMPLE_RATE;
    _context->color_range = 3;
    _context->bits_per_sample = DEFAULT_BITS_PER_SAMPLE;
    _context->audio_codec = DEFAULT_AUDIO_CODEC;
    _context->aspect = HDMI_ASPECT_RATIO_16_9;
    _context->internal_irq = NX_HDMI_GetInterruptNumber(0);
    _context->external_irq = (phdmi ? (phdmi->external_irq != -1 ? phdmi->external_irq : -1): -1);
	_context->plug_in = HDMI_PLUG_READY;

    ret = request_irq(_context->internal_irq, _hdmi_irq_handler, 0,
            "hdmi-soc-int", _context);
    if (0 > ret) {
        pr_err("Fail: HDMI request_irq(%d)\n", _context->internal_irq);
        goto _irq_fail;
    }
    disable_irq(_context->internal_irq);

	INIT_DELAYED_WORK(&_context->hpd_work, _hdmi_hpd_work);

	_get_vsync_info(_context->cur_preset, _context->source_device, &vsync, &sgpar);

	_hdmi_initialize();
	_hdmi_set_preset(_context->cur_preset);

    if (_context->cur_preset == NXP_HDMI_PRESET_720P) {
        _context->vic = 4;
    } else if (_context->cur_preset == NXP_HDMI_PRESET_1080P) {
        _context->vic = 16;
    }

	nxp_soc_disp_register_proc_ops(DISP_DEVICE_HDMI, &hdmi_ops);
	nxp_soc_disp_device_connect_to(DISP_DEVICE_HDMI, _context->source_device, &vsync);

	printk("HDMI: [%d]=%s connect to [%d]=%s (%s)\n",
		device, dev_to_str(device), plat->display_in, dev_to_str(plat->display_in),
			HDMI_PLUG(_context->plug_in));

    NX_HDMI_SetReg(0, HDMI_LINK_INTC_CON_0, (1<<6)|(1<<3)|(1<<2));
    enable_irq(_context->internal_irq);

	return 0;

_irq_fail:
	if (phdmi)
    	kfree(phdmi);
	return ret;
}

static struct platform_driver hdmi_driver = {
	.driver	= {
	.name	= DEV_NAME_HDMI,
	.owner	= THIS_MODULE,
	},
	.probe	= hdmi_probe,
};
module_platform_driver(hdmi_driver);

MODULE_AUTHOR("jhkim <jhkim@nexell.co.kr>");
MODULE_DESCRIPTION("Display HDMI driver for the Nexell");
MODULE_LICENSE("GPL");
