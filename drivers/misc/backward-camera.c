#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/i2c.h>
#include <linux/switch.h>

#include <linux/dma-buf.h>
#include <linux/nxp_ion.h>
#include <linux/ion.h>
#include <linux/cma.h>

#include <mach/platform.h>
#include <mach/soc.h>
#include <mach/nxp-backward-camera.h>

#include <../drivers/gpu/ion/ion_priv.h>

#ifndef MLC_LAYER_RGB_OVERLAY
#define MLC_LAYER_RGB_OVERLAY 0
#endif

extern struct switch_dev *backgear_switch;

static struct nxp_backward_camera_context {
    struct nxp_backward_camera_platform_data *plat_data;
    int irq;
    bool running;
    bool backgear_on;
    bool is_first;
    struct work_struct work;
    struct i2c_client *client;
#ifdef CONFIG_PM
    struct delayed_work resume_work;
#endif

    /* ion allocation */
    struct ion_client *ion_client;
    /* video */
    struct ion_handle *ion_handle_video;
    struct dma_buf    *dma_buf_video;
    void              *virt_video;

    /* rgb */
    struct ion_handle *ion_handle_rgb;
    struct dma_buf    *dma_buf_rgb;
    void              *virt_rgb;

    /* for remove */
    struct platform_device *my_device;
	bool is_on;
	bool removed;
} _context;

static void _mlc_dump_register(int module)
{
#define DBGOUT(args...)  printk(args)
    struct NX_MLC_RegisterSet *pREG = 
    (struct NX_MLC_RegisterSet*)NX_MLC_GetBaseAddress(module);

    DBGOUT("BASE ADDRESS: %p\n", pREG);
#if defined(CONFIG_ARCH_S5P4418)
    DBGOUT(" MLC_MLCCONTROLT    = 0x%04x\r\n", pREG->MLCCONTROLT);
#endif

}


static void _vip_dump_register(int module)
{
#define DBGOUT(args...)  printk(args)
    NX_VIP_RegisterSet *pREG =
        (NX_VIP_RegisterSet*)NX_VIP_GetBaseAddress(module);

    DBGOUT("BASE ADDRESS: %p\n", pREG);
#if defined(CONFIG_ARCH_S5P4418)
    DBGOUT(" VIP_CONFIG     = 0x%04x\r\n", pREG->VIP_CONFIG);
    DBGOUT(" VIP_HVINT      = 0x%04x\r\n", pREG->VIP_HVINT);
    DBGOUT(" VIP_SYNCCTRL   = 0x%04x\r\n", pREG->VIP_SYNCCTRL);
    DBGOUT(" VIP_SYNCMON    = 0x%04x\r\n", pREG->VIP_SYNCMON);
    DBGOUT(" VIP_VBEGIN     = 0x%04x\r\n", pREG->VIP_VBEGIN);
    DBGOUT(" VIP_VEND       = 0x%04x\r\n", pREG->VIP_VEND);
    DBGOUT(" VIP_HBEGIN     = 0x%04x\r\n", pREG->VIP_HBEGIN);
    DBGOUT(" VIP_HEND       = 0x%04x\r\n", pREG->VIP_HEND);
    DBGOUT(" VIP_FIFOCTRL   = 0x%04x\r\n", pREG->VIP_FIFOCTRL);
    DBGOUT(" VIP_HCOUNT     = 0x%04x\r\n", pREG->VIP_HCOUNT);
    DBGOUT(" VIP_VCOUNT     = 0x%04x\r\n", pREG->VIP_VCOUNT);
    DBGOUT(" VIP_CDENB      = 0x%04x\r\n", pREG->VIP_CDENB);
    DBGOUT(" VIP_ODINT      = 0x%04x\r\n", pREG->VIP_ODINT);
    DBGOUT(" VIP_IMGWIDTH   = 0x%04x\r\n", pREG->VIP_IMGWIDTH);
    DBGOUT(" VIP_IMGHEIGHT  = 0x%04x\r\n", pREG->VIP_IMGHEIGHT);
    DBGOUT(" CLIP_LEFT      = 0x%04x\r\n", pREG->CLIP_LEFT);
    DBGOUT(" CLIP_RIGHT     = 0x%04x\r\n", pREG->CLIP_RIGHT);
    DBGOUT(" CLIP_TOP       = 0x%04x\r\n", pREG->CLIP_TOP);
    DBGOUT(" CLIP_BOTTOM    = 0x%04x\r\n", pREG->CLIP_BOTTOM);
    DBGOUT(" DECI_TARGETW   = 0x%04x\r\n", pREG->DECI_TARGETW);
    DBGOUT(" DECI_TARGETH   = 0x%04x\r\n", pREG->DECI_TARGETH);
    DBGOUT(" DECI_DELTAW    = 0x%04x\r\n", pREG->DECI_DELTAW);
    DBGOUT(" DECI_DELTAH    = 0x%04x\r\n", pREG->DECI_DELTAH);
    DBGOUT(" DECI_CLEARW    = 0x%04x\r\n", pREG->DECI_CLEARW);
    DBGOUT(" DECI_CLEARH    = 0x%04x\r\n", pREG->DECI_CLEARH);
    DBGOUT(" DECI_LUSEG     = 0x%04x\r\n", pREG->DECI_LUSEG);
    DBGOUT(" DECI_CRSEG     = 0x%04x\r\n", pREG->DECI_CRSEG);
    DBGOUT(" DECI_CBSEG     = 0x%04x\r\n", pREG->DECI_CBSEG);
    DBGOUT(" DECI_FORMAT    = 0x%04x\r\n", pREG->DECI_FORMAT);
    DBGOUT(" DECI_ROTFLIP   = 0x%04x\r\n", pREG->DECI_ROTFLIP);
    DBGOUT(" DECI_LULEFT    = 0x%04x\r\n", pREG->DECI_LULEFT);
    DBGOUT(" DECI_CRLEFT    = 0x%04x\r\n", pREG->DECI_CRLEFT);
    DBGOUT(" DECI_CBLEFT    = 0x%04x\r\n", pREG->DECI_CBLEFT);
    DBGOUT(" DECI_LURIGHT   = 0x%04x\r\n", pREG->DECI_LURIGHT);
    DBGOUT(" DECI_CRRIGHT   = 0x%04x\r\n", pREG->DECI_CRRIGHT);
    DBGOUT(" DECI_CBRIGHT   = 0x%04x\r\n", pREG->DECI_CBRIGHT);
    DBGOUT(" DECI_LUTOP     = 0x%04x\r\n", pREG->DECI_LUTOP);
    DBGOUT(" DECI_CRTOP     = 0x%04x\r\n", pREG->DECI_CRTOP);
    DBGOUT(" DECI_CBTOP     = 0x%04x\r\n", pREG->DECI_CBTOP);
    DBGOUT(" DECI_LUBOTTOM  = 0x%04x\r\n", pREG->DECI_LUBOTTOM);
    DBGOUT(" DECI_CRBOTTOM  = 0x%04x\r\n", pREG->DECI_CRBOTTOM);
    DBGOUT(" DECI_CBBOTTOM  = 0x%04x\r\n", pREG->DECI_CBBOTTOM);
    DBGOUT(" CLIP_LUSEG     = 0x%04x\r\n", pREG->CLIP_LUSEG);
    DBGOUT(" CLIP_CRSEG     = 0x%04x\r\n", pREG->CLIP_CRSEG);
    DBGOUT(" CLIP_CBSEG     = 0x%04x\r\n", pREG->CLIP_CBSEG);
    DBGOUT(" CLIP_FORMAT    = 0x%04x\r\n", pREG->CLIP_FORMAT);
    DBGOUT(" CLIP_ROTFLIP   = 0x%04x\r\n", pREG->CLIP_ROTFLIP);
    DBGOUT(" CLIP_LULEFT    = 0x%04x\r\n", pREG->CLIP_LULEFT);
    DBGOUT(" CLIP_CRLEFT    = 0x%04x\r\n", pREG->CLIP_CRLEFT);
    DBGOUT(" CLIP_CBLEFT    = 0x%04x\r\n", pREG->CLIP_CBLEFT);
    DBGOUT(" CLIP_LURIGHT   = 0x%04x\r\n", pREG->CLIP_LURIGHT);
    DBGOUT(" CLIP_CRRIGHT   = 0x%04x\r\n", pREG->CLIP_CRRIGHT);
    DBGOUT(" CLIP_CBRIGHT   = 0x%04x\r\n", pREG->CLIP_CBRIGHT);
    DBGOUT(" CLIP_LUTOP     = 0x%04x\r\n", pREG->CLIP_LUTOP);
    DBGOUT(" CLIP_CRTOP     = 0x%04x\r\n", pREG->CLIP_CRTOP);
    DBGOUT(" CLIP_CBTOP     = 0x%04x\r\n", pREG->CLIP_CBTOP);
    DBGOUT(" CLIP_LUBOTTOM  = 0x%04x\r\n", pREG->CLIP_LUBOTTOM);
    DBGOUT(" CLIP_CRBOTTOM  = 0x%04x\r\n", pREG->CLIP_CRBOTTOM);
    DBGOUT(" CLIP_CBBOTTOM  = 0x%04x\r\n", pREG->CLIP_CBBOTTOM);
    DBGOUT(" VIP_SCANMODE   = 0x%04x\r\n", pREG->VIP_SCANMODE);
    DBGOUT(" CLIP_YUYVENB   = 0x%04x\r\n", pREG->CLIP_YUYVENB);
    DBGOUT(" CLIP_BASEADDRH = 0x%04x\r\n", pREG->CLIP_BASEADDRH);
    DBGOUT(" CLIP_BASEADDRL = 0x%04x\r\n", pREG->CLIP_BASEADDRL);
    DBGOUT(" CLIP_STRIDEH   = 0x%04x\r\n", pREG->CLIP_STRIDEH);
    DBGOUT(" CLIP_STRIDEL   = 0x%04x\r\n", pREG->CLIP_STRIDEL);
    DBGOUT(" VIP_VIP1       = 0x%04x\r\n", pREG->VIP_VIP1);
#elif defined(CONFIG_ARCH_S5P6818)
    DBGOUT(" VIP_CONFIG     = 0x%04x\r\n", pREG->VIP_CONFIG);
    DBGOUT(" VIP_HVINT      = 0x%04x\r\n", pREG->VIP_HVINT);
    DBGOUT(" VIP_SYNCCTRL   = 0x%04x\r\n", pREG->VIP_SYNCCTRL);
    DBGOUT(" VIP_SYNCMON    = 0x%04x\r\n", pREG->VIP_SYNCMON);
    DBGOUT(" VIP_VBEGIN     = 0x%04x\r\n", pREG->VIP_VBEGIN);
    DBGOUT(" VIP_VEND       = 0x%04x\r\n", pREG->VIP_VEND);
    DBGOUT(" VIP_HBEGIN     = 0x%04x\r\n", pREG->VIP_HBEGIN);
    DBGOUT(" VIP_HEND       = 0x%04x\r\n", pREG->VIP_HEND);
    DBGOUT(" VIP_FIFOCTRL   = 0x%04x\r\n", pREG->VIP_FIFOCTRL);
    DBGOUT(" VIP_HCOUNT     = 0x%04x\r\n", pREG->VIP_HCOUNT);
    DBGOUT(" VIP_VCOUNT     = 0x%04x\r\n", pREG->VIP_VCOUNT);
    DBGOUT(" VIP_PADCLK_SEL = 0x%04x\r\n", pREG->VIP_PADCLK_SEL);
    DBGOUT(" VIP_INFIFOCLR  = 0x%04x\r\n", pREG->VIP_INFIFOCLR);
    DBGOUT(" VIP_CDENB      = 0x%04x\r\n", pREG->VIP_CDENB);
    DBGOUT(" VIP_ODINT      = 0x%04x\r\n", pREG->VIP_ODINT);
    DBGOUT(" VIP_IMGWIDTH   = 0x%04x\r\n", pREG->VIP_IMGWIDTH);
    DBGOUT(" VIP_IMGHEIGHT  = 0x%04x\r\n", pREG->VIP_IMGHEIGHT);
    DBGOUT(" CLIP_LEFT      = 0x%04x\r\n", pREG->CLIP_LEFT);
    DBGOUT(" CLIP_RIGHT     = 0x%04x\r\n", pREG->CLIP_RIGHT);
    DBGOUT(" CLIP_TOP       = 0x%04x\r\n", pREG->CLIP_TOP);
    DBGOUT(" CLIP_BOTTOM    = 0x%04x\r\n", pREG->CLIP_BOTTOM);
    DBGOUT(" DECI_TARGETW   = 0x%04x\r\n", pREG->DECI_TARGETW);
    DBGOUT(" DECI_TARGETH   = 0x%04x\r\n", pREG->DECI_TARGETH);
    DBGOUT(" DECI_DELTAW    = 0x%04x\r\n", pREG->DECI_DELTAW);
    DBGOUT(" DECI_DELTAH    = 0x%04x\r\n", pREG->DECI_DELTAH);
    DBGOUT(" DECI_CLEARW    = 0x%04x\r\n", pREG->DECI_CLEARW);
    DBGOUT(" DECI_CLEARH    = 0x%04x\r\n", pREG->DECI_CLEARH);
    DBGOUT(" DECI_FORMAT    = 0x%04x\r\n", pREG->DECI_FORMAT);
    DBGOUT(" DECI_LUADDR    = 0x%04x\r\n", pREG->DECI_LUADDR);
    DBGOUT(" DECI_LUSTRIDE  = 0x%04x\r\n", pREG->DECI_LUSTRIDE);
    DBGOUT(" DECI_CRADDR    = 0x%04x\r\n", pREG->DECI_CRADDR);
    DBGOUT(" DECI_CRSTRIDE  = 0x%04x\r\n", pREG->DECI_CRSTRIDE);
    DBGOUT(" DECI_CBADDR    = 0x%04x\r\n", pREG->DECI_CBADDR);
    DBGOUT(" DECI_CBSTRIDE  = 0x%04x\r\n", pREG->DECI_CBSTRIDE);
    DBGOUT(" CLIP_FORMAT    = 0x%04x\r\n", pREG->CLIP_FORMAT);
    DBGOUT(" CLIP_LUADDR    = 0x%04x\r\n", pREG->CLIP_LUADDR);
    DBGOUT(" CLIP_LUSTRIDE  = 0x%04x\r\n", pREG->CLIP_LUSTRIDE);
    DBGOUT(" CLIP_CRADDR    = 0x%04x\r\n", pREG->CLIP_CRADDR);
    DBGOUT(" CLIP_CRSTRIDE  = 0x%04x\r\n", pREG->CLIP_CRSTRIDE);
    DBGOUT(" CLIP_CBADDR    = 0x%04x\r\n", pREG->CLIP_CBADDR);
    DBGOUT(" CLIP_CBSTRIDE  = 0x%04x\r\n", pREG->CLIP_CBSTRIDE);
    DBGOUT(" VIP_SCANMODE   = 0x%04x\r\n", pREG->VIP_SCANMODE);
    DBGOUT(" VIP_VIP1       = 0x%04x\r\n", pREG->VIP_VIP1);
#endif
}

static void _vip_hw_set_clock(int module, struct nxp_backward_camera_platform_data *param, bool on)
{
    if (on) {
        volatile void *clkgen_base = (volatile void *)IO_ADDRESS(NX_CLKGEN_GetPhysicalAddress(NX_VIP_GetClockNumber(module)));
        NX_CLKGEN_SetBaseAddress(NX_VIP_GetClockNumber(module), (void *)clkgen_base);
        NX_CLKGEN_SetClockDivisorEnable(NX_VIP_GetClockNumber(module), CTRUE);
        NX_CLKGEN_SetClockBClkMode(NX_VIP_GetClockNumber(module), NX_BCLKMODE_DYNAMIC);
#if defined(CONFIG_ARCH_S5P4418)
        NX_RSTCON_SetnRST(NX_VIP_GetResetNumber(module), RSTCON_nDISABLE);
        NX_RSTCON_SetnRST(NX_VIP_GetResetNumber(module), RSTCON_nENABLE);
#elif defined(CONFIG_ARCH_S5P6818)
        NX_RSTCON_SetRST(NX_VIP_GetResetNumber(module), RSTCON_ASSERT);
        NX_RSTCON_SetRST(NX_VIP_GetResetNumber(module), RSTCON_NEGATE);
#endif

        if (param->is_mipi) {
            NX_CLKGEN_SetClockSource(NX_VIP_GetClockNumber(module), 0, 2); /* external PCLK */
            NX_CLKGEN_SetClockDivisor(NX_VIP_GetClockNumber(module), 0, 2);
            NX_CLKGEN_SetClockDivisorEnable(NX_VIP_GetClockNumber(module), CTRUE);
        } else {
            NX_CLKGEN_SetClockSource(NX_VIP_GetClockNumber(module), 0, 4 + param->port); /* external PCLK */
            NX_CLKGEN_SetClockDivisor(NX_VIP_GetClockNumber(module), 0, 1);
            NX_CLKGEN_SetClockDivisorEnable(NX_VIP_GetClockNumber(module), CTRUE);
        }
        NX_VIP_SetBaseAddress(module, (void *)IO_ADDRESS(NX_VIP_GetPhysicalAddress(module)));
    }
}

static void _vip_hw_set_sensor_param(int module, struct nxp_backward_camera_platform_data *param)
{
    if (param->is_mipi) {
        NX_VIP_SetInputPort(module, NX_VIP_INPUTPORT_B);
        NX_VIP_SetDataMode(module, NX_VIP_DATAORDER_CBY0CRY1, 16);
        NX_VIP_SetFieldMode(module, CFALSE, NX_VIP_FIELDSEL_BYPASS, CFALSE, CFALSE);
        NX_VIP_SetDValidMode(module, CTRUE, CTRUE, CTRUE);
        NX_VIP_SetFIFOResetMode(module, NX_VIP_FIFORESET_ALL);

        NX_VIP_SetHVSyncForMIPI(module,
                param->h_active * 2,
                param->v_active,
                param->h_syncwidth,
                param->h_frontporch,
                param->h_backporch,
                param->v_syncwidth,
                param->v_frontporch,
                param->v_backporch);
    } else {
        NX_VIP_SetDataMode(module, param->data_order, 8);
        NX_VIP_SetFieldMode(module,
                CFALSE,
                0,
                param->interlace,
                CFALSE);
        {
            NX_VIP_RegisterSet *pREG =
                (NX_VIP_RegisterSet*)NX_VIP_GetBaseAddress(module);
        }

        NX_VIP_SetDValidMode(module,
                CFALSE,
                CFALSE,
                CFALSE);
        NX_VIP_SetFIFOResetMode(module, NX_VIP_FIFORESET_ALL);
        NX_VIP_SetInputPort(module, (NX_VIP_INPUTPORT)param->port);

        NX_VIP_SetHVSync(module,
                param->external_sync,
                param->h_active*2,
                param->interlace ? param->v_active >> 1 : param->v_active,
                param->h_syncwidth,
                param->h_frontporch,
                param->h_backporch,
                param->v_syncwidth,
                param->v_frontporch,
                param->v_backporch);
    }

#if defined(CONFIG_ARCH_S5P4418)
    NX_VIP_SetClipperFormat(module, NX_VIP_FORMAT_420, 0, 0, 0);
#else
    NX_VIP_SetClipperFormat(module, NX_VIP_FORMAT_420);
#endif

    NX_VIP_SetClipRegion(module,
            0,
            0,
            param->h_active,
            param->interlace ? param->v_active >> 1 : param->v_active);
}

static void _vip_hw_set_addr(int module, struct nxp_backward_camera_platform_data *param, u32 lu_addr, u32 cb_addr, u32 cr_addr)
{
#if 0
    NX_VIP_SetClipperAddr(module, NX_VIP_FORMAT_420, param->h_active, param->v_active,
            lu_addr, cb_addr, cr_addr,
            param->interlace ? ALIGN(param->h_active, 64)   : param->h_active,
            param->interlace ? ALIGN(param->h_active/2, 64) : param->h_active/2);
#else
	//printk("%s : width : %d, height : %d, lu_stride : %d, cb_stride : %d\n", __func__, param->h_active, param->v_active, param->lu_stride, param->cb_stride);
    NX_VIP_SetClipperAddr(module, NX_VIP_FORMAT_420, param->h_active, param->v_active,
            lu_addr, cb_addr, cr_addr,
            param->lu_stride,
            param->cb_stride);
            /*param->interlace ? ALIGN(param->h_active, 64)   : param->h_active,*/
            /*param->interlace ? ALIGN(param->h_active/2, 64) : param->h_active/2);*/
#endif
}

static void _vip_run(int module)
{
    struct nxp_backward_camera_context *me = &_context;

	printk("+++ %s +++\n", __func__);

    u32 lu_addr = me->plat_data->lu_addr;
    u32 cb_addr = me->plat_data->cb_addr;
    u32 cr_addr = me->plat_data->cr_addr;

    _vip_hw_set_clock(module, me->plat_data, true);
    _vip_hw_set_sensor_param(module, me->plat_data);
    _vip_hw_set_addr(module, me->plat_data, lu_addr, cb_addr, cr_addr);

    NX_VIP_SetVIPEnable(module, CTRUE, CTRUE, CTRUE, CFALSE);
    //_vip_dump_register(module);

	printk("--- %s ---\n", __func__);
}

static void _vip_stop(int module)
{
#if defined(CONFIG_ARCH_S5P6818)
    {
        int intnum = 0;
        /*int intnum = 2; ODINT*/
        NX_VIP_SetInterruptEnable( module, intnum, CTRUE );
        while (CFALSE == NX_VIP_GetInterruptPending( module, intnum ));
        NX_VIP_ClearInterruptPendingAll( module );
    }
#endif
    NX_VIP_SetVIPEnable(module, CFALSE, CFALSE, CFALSE, CFALSE);
#if defined(CONFIG_ARCH_S5P6818)
    NX_RSTCON_SetRST(NX_VIP_GetResetNumber(module), RSTCON_ASSERT);
    NX_RSTCON_SetRST(NX_VIP_GetResetNumber(module), RSTCON_NEGATE);
#endif
}

static void _mlc_video_run(int module)
{
    NX_MLC_SetTopDirtyFlag(module);
    NX_MLC_SetVideoLayerLineBufferPowerMode(module, CTRUE);
    NX_MLC_SetVideoLayerLineBufferSleepMode(module, CFALSE);
    NX_MLC_SetLayerEnable(module, MLC_LAYER_VIDEO, CTRUE);
    NX_MLC_SetDirtyFlag(module, MLC_LAYER_VIDEO);
}

static void _mlc_video_stop(int module)
{
    NX_MLC_SetTopDirtyFlag(module);
    NX_MLC_SetVideoLayerLineBufferPowerMode(module, CFALSE);
    NX_MLC_SetVideoLayerLineBufferSleepMode(module, CTRUE);
    NX_MLC_SetLayerEnable(module, MLC_LAYER_VIDEO, CFALSE);
    NX_MLC_SetDirtyFlag(module, MLC_LAYER_VIDEO);

	//_mlc_dump_register(module);
}

static void _mlc_overlay_run(int module)
{
    u32 layer = MLC_LAYER_RGB_OVERLAY;
    NX_MLC_SetLayerEnable(module, layer, CTRUE);
    NX_MLC_SetDirtyFlag(module, layer);
}

static void _mlc_overlay_stop(int module)
{
    u32 layer = MLC_LAYER_RGB_OVERLAY;
    NX_MLC_SetLayerEnable(module, layer, CFALSE);
    NX_MLC_SetDirtyFlag(module, layer);
}

#ifndef MLC_LAYER_VIDEO
#define MLC_LAYER_VIDEO     3
#endif

static void _mlc_video_set_param(int module, struct nxp_backward_camera_platform_data *param)
{
    int srcw = param->h_active;
    int srch = param->v_active;
    int dstw, dsth, hf, vf;

    NX_MLC_GetScreenSize(module, &dstw, &dsth);

    hf = 1, vf = 1;

    if (srcw == dstw && srch == dsth)
        hf = 0, vf = 0;

    NX_MLC_SetFormatYUV(module, NX_MLC_YUVFMT_420);
    NX_MLC_SetVideoLayerScale(module, srcw, srch, dstw, dsth,
            (CBOOL)hf, (CBOOL)hf, (CBOOL)vf, (CBOOL)vf);
    NX_MLC_SetPosition(module, MLC_LAYER_VIDEO,
            0, 0, dstw - 1, dsth - 1);
#if 0 //keun 2015. 08. 17
    NX_MLC_SetLayerPriority(module, 0);
#else
    NX_MLC_SetLayerPriority(module, 1);
#endif
    NX_MLC_SetDirtyFlag(module, MLC_LAYER_VIDEO);
}

static void _mlc_video_set_addr(int module, u32 lu_a, u32 cb_a, u32 cr_a, u32 lu_s, u32 cb_s, u32 cr_s)
{
    NX_MLC_SetVideoLayerStride (module, lu_s, cb_s, cr_s);
    NX_MLC_SetVideoLayerAddress(module, lu_a, cb_a, cr_a);
    NX_MLC_SetVideoLayerLineBufferPowerMode(module, CTRUE);
    NX_MLC_SetVideoLayerLineBufferSleepMode(module, CFALSE);
    NX_MLC_SetDirtyFlag(module, MLC_LAYER_VIDEO);
}

static inline u32 _get_pixel_byte(u32 nxp_rgb_format)
{
    switch (nxp_rgb_format) {
        case NX_MLC_RGBFMT_R5G6B5:
        case NX_MLC_RGBFMT_B5G6R5:
        case NX_MLC_RGBFMT_X1R5G5B5:
        case NX_MLC_RGBFMT_X1B5G5R5:
        case NX_MLC_RGBFMT_X4R4G4B4:
        case NX_MLC_RGBFMT_X4B4G4R4:
        case NX_MLC_RGBFMT_X8R3G3B2:
        case NX_MLC_RGBFMT_X8B3G3R2:
        case NX_MLC_RGBFMT_A1R5G5B5:
        case NX_MLC_RGBFMT_A1B5G5R5:
        case NX_MLC_RGBFMT_A4R4G4B4:
        case NX_MLC_RGBFMT_A4B4G4R4:
        case NX_MLC_RGBFMT_A8R3G3B2:
        case NX_MLC_RGBFMT_A8B3G3R2:
            return 2;

        case NX_MLC_RGBFMT_R8G8B8:
        case NX_MLC_RGBFMT_B8G8R8:
            return 3;

        case NX_MLC_RGBFMT_A8R8G8B8:
        case NX_MLC_RGBFMT_A8B8G8R8:
            return 4;

        default:
            pr_err("%s: invalid nxp_rgb_format(0x%x)\n", __func__, nxp_rgb_format);
            return 0;
    }
}

static void _mlc_rgb_overlay_set_param(int module, struct nxp_backward_camera_platform_data *param)
{
    u32 format = param->rgb_format;
    u32 pixelbyte = _get_pixel_byte(format);
    u32 stride = param->width * pixelbyte;
    u32 layer = MLC_LAYER_RGB_OVERLAY;
    CBOOL EnAlpha = CFALSE;

    if (format == MLC_RGBFMT_A1R5G5B5 ||
        format == MLC_RGBFMT_A1B5G5R5 ||
        format == MLC_RGBFMT_A4R4G4B4 ||
        format == MLC_RGBFMT_A4B4G4R4 ||
        format == MLC_RGBFMT_A8R3G3B2 ||
        format == MLC_RGBFMT_A8B3G3R2 ||
        format == MLC_RGBFMT_A8R8G8B8 ||
        format == MLC_RGBFMT_A8B8G8R8)
        EnAlpha = CTRUE;

    NX_MLC_SetColorInversion(module, layer, CFALSE, 0);
    NX_MLC_SetAlphaBlending(module, layer, EnAlpha, 0);
    NX_MLC_SetFormatRGB(module, layer, (NX_MLC_RGBFMT)format);
    NX_MLC_SetRGBLayerInvalidPosition(module, layer, 0, 0, 0, 0, 0, CFALSE);
    NX_MLC_SetRGBLayerInvalidPosition(module, layer, 1, 0, 0, 0, 0, CFALSE);
    NX_MLC_SetPosition(module, layer, 0, 0, param->width-1, param->height-1);

    NX_MLC_SetRGBLayerStride (module, layer, pixelbyte, stride);
    NX_MLC_SetRGBLayerAddress(module, layer, param->rgb_addr);
    NX_MLC_SetDirtyFlag(module, layer);
}

static void _mlc_rgb_overlay_draw(int module, struct nxp_backward_camera_platform_data *me, void *mem)
{
    if (me->draw_rgb_overlay)
        me->draw_rgb_overlay(me, mem);
}

static int _get_i2c_client(struct nxp_backward_camera_context *me)
{
    struct i2c_client *client;
    struct i2c_adapter *adapter = i2c_get_adapter(me->plat_data->i2c_bus);

printk("+++ %s +++\n", __func__);
    if (!adapter) {
        pr_err("%s: unable to get i2c adapter %d\n", __func__, me->plat_data->i2c_bus);
        return -EINVAL;
    }

    client = kzalloc(sizeof *client, GFP_KERNEL);
    if (!client) {
        pr_err("%s: can't allocate i2c_client\n", __func__);
        return -ENOMEM;
    }

    client->adapter = adapter;
    client->addr = me->plat_data->chip_addr;
    client->flags = 0;

    me->client = client;

printk("--- %s ---\n", __func__);
    return 0;
}

static int _camera_sensor_run(struct nxp_backward_camera_context *me)
{
    struct reg_val *reg_val;

printk("+++ %s +++\n", __func__);
    if (me->plat_data->power_enable)
        me->plat_data->power_enable(true);

    if (me->plat_data->setup_io)
        me->plat_data->setup_io();

    if (me->plat_data->set_clock)
        me->plat_data->set_clock(me->plat_data->clk_rate);

    reg_val = me->plat_data->reg_val;
    while (reg_val->reg != 0xff) {
		printk("%s : reg : 0x%02X, val : 0x%02X\n", __func__, reg_val->reg, reg_val->val);
        i2c_smbus_write_byte_data(me->client, reg_val->reg, reg_val->val);
        reg_val++;
    }


printk("--- %s ---\n", __func__);
    return 0;
}

static void _turn_on(struct nxp_backward_camera_context *me)
{
    if (me->is_first == true) {
        //_vip_run(me->plat_data->vip_module_num);
        _mlc_video_set_param(me->plat_data->mlc_module_num, me->plat_data);
        _mlc_video_set_addr(me->plat_data->mlc_module_num,
                me->plat_data->lu_addr,
                me->plat_data->cb_addr,
                me->plat_data->cr_addr,
                me->plat_data->lu_stride,
                me->plat_data->cb_stride,
                me->plat_data->cr_stride);

        _mlc_rgb_overlay_set_param(me->plat_data->mlc_module_num, me->plat_data);
        _mlc_rgb_overlay_draw(me->plat_data->mlc_module_num, me->plat_data, me->virt_rgb);
        me->is_first = false;
    }

//	_mlc_dump_register(me->plat_data->mlc_module_num);
    _mlc_video_run(me->plat_data->mlc_module_num);
   	_mlc_overlay_run(me->plat_data->mlc_module_num);
	me->is_on = true;
}

static void _turn_off(struct nxp_backward_camera_context *me)
{
    _mlc_overlay_stop(me->plat_data->mlc_module_num);
    _mlc_video_stop(me->plat_data->mlc_module_num);
	me->is_on = false;
}

#define THINE_I2C_RETRY_CNT             3
static int _i2c_read_byte(struct i2c_client *client, u8 addr, u8 *data)
{
    s8 i = 0;
    s8 ret = 0;
    u8 buf = 0;
    struct i2c_msg msg[2];

    msg[0].addr = client->addr;
    msg[0].flags = 0;
    msg[0].len = 1;
    msg[0].buf = &addr;

    msg[1].addr = client->addr;
    msg[1].flags = I2C_M_RD;
    msg[1].len = 1;
    msg[1].buf = &buf;

    for(i=0; i<THINE_I2C_RETRY_CNT; i++) {
        ret = i2c_transfer(client->adapter, msg, 2); 
        if (likely(ret == 2)) 
            break;
    }   

    if (unlikely(ret != 2)) {
        dev_err(&client->dev, "_i2c_read_byte failed reg:0x%02x\n", addr);
        return -EIO;
    }   

    *data = buf;
    return 0;
}

#if 1
static inline bool _is_backgear_on(struct nxp_backward_camera_platform_data *pdata)
#else
static inline bool _is_backgear_on(struct nxp_backward_camera_context *me)
#endif
{
#if 1
    bool is_on = nxp_soc_gpio_get_in_value(pdata->backgear_gpio_num);
    if (!pdata->active_high) is_on ^= 1;
    return is_on;
#else
	//tw9900 read status
	u8 data = 0;
	_i2c_read_byte(me->client, 0x01, &data);
	if( data & 0x80 )
		return 0;
	return 1;
#endif
}

static inline bool _is_running(struct nxp_backward_camera_context *me)
{
#if 0
    CBOOL vipenb, sepenb, clipenb, decenb;
    bool mlcenb;

    NX_VIP_GetVIPEnable(me->plat_data->vip_module_num, &vipenb, &sepenb, &clipenb, &decenb);
    mlcenb = NX_MLC_GetLayerEnable(me->plat_data->mlc_module_num, 3);

    return mlcenb && vipenb && sepenb && clipenb;
#else
    return NX_MLC_GetLayerEnable(me->plat_data->mlc_module_num, 3);
#endif
}

static void _backgear_switch(int on)
{
	if( backgear_switch != NULL )
		switch_set_state(backgear_switch, on);	
	else
		printk("%s - backgear switch is NULL!!!\n", __func__);
}

static void _decide(struct nxp_backward_camera_context *me)
{
    /*me->running = NX_MLC_GetLayerEnable(me->plat_data->mlc_module_num, 3); // video layer*/
    me->running = _is_running(me);
    me->backgear_on = _is_backgear_on(me->plat_data);
	//printk("%s: running %d, backgear on %d\n", __func__, me->running, me->backgear_on);
    if (me->backgear_on && !me->running)
	{
        _turn_on(me);
		_backgear_switch(1);
	}
    else if (me->running && !me->backgear_on)
	{
        _turn_off(me);
		_backgear_switch(0);
	}
}

static irqreturn_t _irq_handler(int irq, void *devdata)
{
    struct nxp_backward_camera_context *me = devdata;
    schedule_work(&me->work);
    return IRQ_HANDLED;
}

static void _work_handler(struct work_struct *work)
{
    _decide(&_context);
}

extern struct ion_device *get_global_ion_device(void);
static int _allocate_memory(struct nxp_backward_camera_context *me)
{
	printk("+++ %s +++\n", __func__);
    struct ion_device *ion_dev = get_global_ion_device();
    if (me->plat_data->lu_addr && me->plat_data->rgb_addr)
        return 0;

    me->ion_client = ion_client_create(ion_dev, "backward-camera");
    if (IS_ERR(me->ion_client)) {
        pr_err("%s: failed to ion_client_create()\n", __func__);
        return -EINVAL;
    }


    if (!me->plat_data->lu_addr) {
        int size = me->plat_data->lu_stride * me->plat_data->v_active
            + me->plat_data->cb_stride * (me->plat_data->v_active / 2)
            + me->plat_data->cr_stride * (me->plat_data->v_active / 2);
        struct ion_buffer *ion_buffer;
        size = PAGE_ALIGN(size);

        me->ion_handle_video = ion_alloc(me->ion_client, size, 0, ION_HEAP_NXP_CONTIG_MASK, 0);
        if (IS_ERR(me->ion_handle_video)) {
             pr_err("%s: failed to ion_alloc() for video, size %d\n", __func__, size);
             return -ENOMEM;
        }
        me->dma_buf_video = ion_share_dma_buf(me->ion_client, me->ion_handle_video);
        if (IS_ERR_OR_NULL(me->dma_buf_video)) {
            pr_err("%s: failed to ion_share_dma_buf() for video\n", __func__);
            return -EINVAL;
        }
        ion_buffer = me->dma_buf_video->priv;
        me->plat_data->lu_addr = ion_buffer->priv_phys;
        me->plat_data->cb_addr = me->plat_data->lu_addr + me->plat_data->lu_stride * me->plat_data->v_active;
        me->plat_data->cr_addr = me->plat_data->cb_addr + me->plat_data->cb_stride * (me->plat_data->v_active / 2);
        me->virt_video = cma_get_virt(me->plat_data->lu_addr, size, 1);

#if 0
        printk("%s: lu 0x%x, cb 0x%x, cr 0x%x, virt %p\n",
                __func__,
                me->plat_data->lu_addr,
                me->plat_data->cb_addr,
                me->plat_data->cr_addr,
                me->virt_video);
#endif
    }

    if (!me->plat_data->rgb_addr) {
        u32 format = me->plat_data->rgb_format;
        u32 pixelbyte = _get_pixel_byte(format);
        int size = me->plat_data->width * me->plat_data->height * pixelbyte;
        struct ion_buffer *ion_buffer;
        size = PAGE_ALIGN(size);

        me->ion_handle_rgb = ion_alloc(me->ion_client, size, 0, ION_HEAP_NXP_CONTIG_MASK, 0);
        if (IS_ERR(me->ion_handle_rgb)) {
             pr_err("%s: failed to ion_alloc() for rgb, size %d\n", __func__, size);
             return -ENOMEM;
        }
        me->dma_buf_rgb = ion_share_dma_buf(me->ion_client, me->ion_handle_rgb);
        if (IS_ERR_OR_NULL(me->dma_buf_rgb)) {
            pr_err("%s: failed to ion_share_dma_buf() for rgb\n", __func__);
            return -EINVAL;
        }
        ion_buffer = me->dma_buf_rgb->priv;
        me->plat_data->rgb_addr = ion_buffer->priv_phys;
        me->virt_rgb = cma_get_virt(me->plat_data->rgb_addr, size, 1);

#if 0
        printk("%s: rgb 0x%x, virt %p\n",
                __func__,
                me->plat_data->rgb_addr,
                me->virt_rgb);
#endif
    }

	printk("--- %s ---\n", __func__);

    return 0;
}

static void _free_buffer(struct nxp_backward_camera_context *me)
{
    if (me->dma_buf_video != NULL) {
        dma_buf_put(me->dma_buf_video);
        me->dma_buf_video = NULL;
        ion_free(me->ion_client, me->ion_handle_video);
        me->ion_handle_video = NULL;
    }

    if (me->dma_buf_rgb != NULL) {
        dma_buf_put(me->dma_buf_rgb);
        me->dma_buf_rgb = NULL;
        ion_free(me->ion_client, me->ion_handle_rgb);
        me->ion_handle_rgb = NULL;
    }
}


#ifdef CONFIG_PM
#define RESUME_CAMERA_ON_DELAY_MS   300
static void _resume_work(struct work_struct *work)
{
    struct nxp_backward_camera_context *me = &_context;
    int vip_module_num = me->plat_data->vip_module_num;
    int mlc_module_num = me->plat_data->mlc_module_num;
    u32 lu_addr = me->plat_data->lu_addr;
    u32 cb_addr = me->plat_data->cb_addr;
    u32 cr_addr = me->plat_data->cr_addr;
    u32 lu_stride = me->plat_data->lu_stride;
    u32 cb_stride = me->plat_data->cb_stride;
    u32 cr_stride = me->plat_data->cr_stride;

    _camera_sensor_run(me);
    _vip_hw_set_clock(vip_module_num, me->plat_data, true);
    _vip_hw_set_sensor_param(vip_module_num, me->plat_data);
    _vip_hw_set_addr(vip_module_num, me->plat_data, lu_addr, cb_addr, cr_addr);

    _mlc_video_set_param(mlc_module_num, me->plat_data);
    _mlc_video_set_addr(mlc_module_num, lu_addr, cb_addr, cr_addr, lu_stride, cb_stride, cr_stride);

    _mlc_rgb_overlay_set_param(mlc_module_num, me->plat_data);
    _mlc_rgb_overlay_draw(mlc_module_num, me->plat_data, me->virt_rgb);

    _decide(me);
}

static int nxp_backward_camera_suspend(struct device *dev)
{
    struct nxp_backward_camera_context *me = &_context;
    PM_DBGOUT("+%s\n", __func__);
    me->running = false;
    PM_DBGOUT("-%s\n", __func__);
    return 0;
}

static int nxp_backward_camera_resume(struct device *dev)
{
    struct nxp_backward_camera_context *me = &_context;
    PM_DBGOUT("+%s\n", __func__);
    if (!me->client)
        _get_i2c_client(me);

    queue_delayed_work(system_nrt_wq, &me->resume_work, msecs_to_jiffies(RESUME_CAMERA_ON_DELAY_MS));

    PM_DBGOUT("-%s\n", __func__);
    return 0;
}
#endif

#ifdef CONFIG_PM
static const struct dev_pm_ops nxp_backward_camera_pm_ops = {
    SET_SYSTEM_SLEEP_PM_OPS(nxp_backward_camera_suspend, nxp_backward_camera_resume)
};
#define NXP_BACKWARD_CAMERA_PMOPS       (&nxp_backward_camera_pm_ops)
#else
#define NXP_BACKWARD_CAMERA_PMOPS       NULL
#endif

static int nxp_backward_camera_probe(struct platform_device *pdev)
{
    int ret;
    struct nxp_backward_camera_platform_data *pdata = pdev->dev.platform_data;
    struct nxp_backward_camera_context *me = &_context;

    me->plat_data = pdata;
    me->irq = IRQ_GPIO_START + pdata->backgear_gpio_num;

	NX_MLC_SetBaseAddress(pdata->mlc_module_num, (void *)IO_ADDRESS(NX_MLC_GetPhysicalAddress(pdata->mlc_module_num)));
	NX_VIP_SetBaseAddress(pdata->vip_module_num, (void *)IO_ADDRESS(NX_VIP_GetPhysicalAddress(pdata->vip_module_num)));

    _get_i2c_client(me);
    _camera_sensor_run(me);
    _allocate_memory(me);
	_vip_run(me->plat_data->vip_module_num);

    INIT_WORK(&me->work, _work_handler);
#if 1
    ret = request_irq(me->irq, _irq_handler, IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, "backward-camera", me);
    if (ret) {
        pr_err("%s: failed to request_irq (irqnum %d)\n", __func__, me->irq);
        return -1;
    }
#endif

    me->is_first = true;

	me->removed = false;

    _decide(me);

    me->my_device = pdev;

#ifdef CONFIG_PM
    INIT_DELAYED_WORK(&me->resume_work, _resume_work);
#endif

    return 0;
}

#if 0
static int nxp_backward_camera_remove(struct platform_device *pdev)
{
    struct nxp_backward_camera_context *me = &_context;
    _free_buffer(me);
    free_irq(_context.irq, &_context);
    return 0;
}
#else
static int nxp_backward_camera_remove(struct platform_device *pdev)
{
	struct nxp_backward_camera_context *me = &_context;
	if( me->removed == false ) {
		printk(KERN_ERR "%s\n", __func__);
		if( me->is_on ) {
			_mlc_overlay_stop(me->plat_data->mlc_module_num);
			_mlc_video_stop(me->plat_data->mlc_module_num);
			_vip_stop(me->plat_data->vip_module_num);	
		}
		_free_buffer(me);
		free_irq(_context.irq, &_context);
		me->removed = true;
	}
	return 0;
}


#endif

static struct platform_driver backward_camera_driver = {
    .probe  = nxp_backward_camera_probe,
    .remove = nxp_backward_camera_remove,
    .driver = {
        .name  = "nxp-backward-camera",
        .owner = THIS_MODULE,
        .pm    = NXP_BACKWARD_CAMERA_PMOPS,
    },
};

bool is_backward_camera_on(void)
{
    struct nxp_backward_camera_context *me = &_context;
#if 0
    return _is_backgear_on(me);
#else
	return me->is_on;
#endif
}

void backward_camera_remove(void)
{
    struct nxp_backward_camera_context *me = &_context;
    nxp_backward_camera_remove(me->my_device);
}

EXPORT_SYMBOL(is_backward_camera_on);
EXPORT_SYMBOL(backward_camera_remove);

static int __init backward_camera_init(void)
{
    return platform_driver_register(&backward_camera_driver);
}

subsys_initcall(backward_camera_init);

MODULE_AUTHOR("swpark <swpark@nexell.co.kr>");
MODULE_DESCRIPTION("Backward Camera Driver for Nexell");
MODULE_LICENSE("GPL");
