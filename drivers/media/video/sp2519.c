
/*
 * sp2519 Camera Driver
 *
 * Copyright (C) 2011 Actions Semiconductor Co.,LTD
 * Wang Xin <wangxin@actions-semi.com>
 *
 * Based on ov227x driver
 *
 * Copyright (C) 2008 Renesas Solutions Corp.
 * Kuninori Morimoto <morimoto.kuninori@renesas.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * fixed by swpark@nexell.co.kr for compatibility with general v4l2 layer (not using soc camera interface)
 */


#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/videodev2.h>
#include <linux/v4l2-subdev.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>

#define MODULE_NAME "SP2519"

// TODO : move this to PLAT/device.c
#if 0
static struct i2c_board_info asoc_i2c_camera = {
    I2C_BOARD_INFO(MODULE_NAME, 0x30),//caichsh
};
#endif

#ifdef SP2519_DEBUG
#define assert(expr) \
    if (unlikely(!(expr))) {				\
        pr_err("Assertion failed! %s,%s,%s,line=%d\n",	\
#expr, __FILE__, __func__, __LINE__);	\
    }

#define SP2519_DEBUG(fmt,args...) printk(KERN_ALERT fmt, ##args)
#else

#define assert(expr) do {} while (0)

#define SP2519_DEBUG(fmt,args...)
#endif

#define PID                 0x02 /* Product ID Number  *///caichsh
#define SP2519              0x53
#define OUTTO_SENSO_CLOCK   24000000
#define NUM_CTRLS           11
#define V4L2_IDENT_SP2519   64112

/* private ctrls */
#define V4L2_CID_SCENE_EXPOSURE         (V4L2_CTRL_CLASS_CAMERA | 0x1001)
#define V4L2_CID_PRIVATE_PREV_CAPT      (V4L2_CTRL_CLASS_CAMERA | 0x1002)

#if 0
enum {
    V4L2_WHITE_BALANCE_INCANDESCENT = 0,
    V4L2_WHITE_BALANCE_FLUORESCENT,
    V4L2_WHITE_BALANCE_DAYLIGHT,
    V4L2_WHITE_BALANCE_CLOUDY_DAYLIGHT,
    V4L2_WHITE_BALANCE_TUNGSTEN
};
#else
enum {
    V4L2_WHITE_BALANCE_INCANDESCENT = 0,
    /*V4L2_WHITE_BALANCE_FLUORESCENT,*/
    V4L2_WHITE_BALANCE_DAYLIGHT,
    V4L2_WHITE_BALANCE_CLOUDY_DAYLIGHT,
    /*V4L2_WHITE_BALANCE_TUNGSTEN*/
};
#endif

struct regval_list {
    unsigned char reg_num;
    unsigned char value;
};

/****************************************************************************************
 * predefined reg values
 */
#define ENDMARKER { 0xff, 0xff }

static struct regval_list sp2519_fmt_yuv422_yuyv[] =
{
    //YCbYCr
    ENDMARKER,
};

static struct regval_list sp2519_fmt_yuv422_yvyu[] =
{
    //YCrYCb
    ENDMARKER,
};

static struct regval_list sp2519_fmt_yuv422_vyuy[] =
{
    //CrYCbY
    ENDMARKER,
};

static struct regval_list sp2519_fmt_yuv422_uyvy[] =
{
    //CbYCrY
    ENDMARKER,
};

static struct regval_list sp2519_fmt_raw[] __attribute__((unused)) =
{
    ENDMARKER,
};

/*
 *AWB
 */
static const struct regval_list sp2519_awb_regs_enable[] =
{
    ENDMARKER,
};

static const struct regval_list sp2519_awb_regs_diable[] =
{
    ENDMARKER,
};

static struct regval_list sp2519_wb_cloud_regs[] =
{
    ENDMARKER,
};

static struct regval_list sp2519_wb_daylight_regs[] =
{
    ENDMARKER,
};

static struct regval_list sp2519_wb_incandescence_regs[] =
{
    ENDMARKER,
};

static struct regval_list sp2519_wb_fluorescent_regs[] =
{
    ENDMARKER,
};

static struct regval_list sp2519_wb_tungsten_regs[] =
{
    ENDMARKER,
};

static struct regval_list sp2519_colorfx_none_regs[] =
{
    ENDMARKER,
};

static struct regval_list sp2519_colorfx_bw_regs[] =
{
    ENDMARKER,
};

static struct regval_list sp2519_colorfx_sepia_regs[] =
{
    ENDMARKER,
};

static struct regval_list sp2519_colorfx_negative_regs[] =
{
    ENDMARKER,
};

static struct regval_list sp2519_whitebance_auto[] __attribute__((unused)) =
{
	ENDMARKER,
};

static struct regval_list sp2519_whitebance_cloudy[] __attribute__((unused)) =
{
	ENDMARKER,
};

static  struct regval_list sp2519_whitebance_sunny[] __attribute__((unused)) =
{

	ENDMARKER,
};
/* Office Colour Temperature : 3500K - 5000K ,荧光灯 */
static  struct regval_list sp2519_whitebance_fluorescent[] __attribute__((unused)) =
{
	ENDMARKER,

};
/* Home Colour Temperature : 2500K - 3500K ，白炽灯 */
static  struct regval_list sp2519_whitebance_incandescent[] __attribute__((unused)) =
{
	ENDMARKER,
};

/*正常模式*/
static  struct regval_list sp2519_effect_normal[] __attribute__((unused)) =
{
    ENDMARKER,
};
/*单色，黑白照片*/
static  struct regval_list sp2519_effect_white_black[] __attribute__((unused)) =
{

  ENDMARKER,
};

/* Effect */
static  struct regval_list sp2519_effect_negative[] __attribute__((unused)) =
{
    //Negative
	ENDMARKER,
};
/*复古效果*/
static  struct regval_list sp2519_effect_antique[] __attribute__((unused)) =
{

	ENDMARKER,
};

/* Scene */
static  struct regval_list sp2519_scene_auto[] __attribute__((unused)) =
{

	ENDMARKER,
};

static  struct regval_list sp2519_scene_night[] __attribute__((unused)) =
{

	ENDMARKER,
};

/*
 * register setting for window size
 */
// mediatech
static const struct regval_list sp2519_svga_init_regs[] =
{
    //sp2519 v10.13
{0xfd,0x01},
{0x36,0x02},
{0xfd,0x00},
{0x0c,0x55},
{0x11,0x40},
{0x18,0x00},
//{0x19,0x00},//20 fs
{0x1a,0x49},
{0x1c,0x1f},//07 fs
{0x1d,0x00},//0x11
{0x1e,0x01},
{0x2e,0x8f},
{0x20,0x2f},
{0x21,0x10},
{0x22,0x2a},
{0x25,0xad},
{0x27,0xa1},
{0x1f,0xf0},
{0x28,0x0b},
{0x2b,0x8c},
{0x26,0x09},
{0x2c,0x45},
{0x37,0x00},
{0x16,0x01},
{0x17,0x2f},
{0x69,0x01},
{0x6a,0x2d},
{0x13,0x4f},
{0x6b,0x50},
{0x6c,0x50},
{0x6f,0x50},
{0x73,0x51},

{0x7a,0x41},
{0x70,0x41},
{0x7d,0x40},
{0x74,0x40},
{0x75,0x40},

{0x14,0x01},
{0x15,0x20},
{0x71,0x22},
{0x76,0x22},
{0x7c,0x22},

{0x7e,0x21},
{0x72,0x21},
{0x77,0x20},

{0xfd,0x00},
{0x1b,0x10},
{0x92,0x00},

{0xfd,0x01},
{0x32,0x00},
{0xfb,0x21},//21//hanlei//25 fs
{0xfd,0x02},
{0x85,0x00},
{0x00,0x82},
{0x01,0x82},

	 {0xfd,0x00},
	 {0x2f,0x04},
	 {0x03,0x02},
	 {0x04,0xa0},
	 {0x05,0x00},
	 {0x06,0x00},
	 {0x07,0x00},
	 {0x08,0x00},
	 {0x09,0x00},
	 {0x0a,0x95},
	 {0xfd,0x01},
	 {0xf0,0x00},
	 {0xf7,0x70},
	 {0xf8,0x5d},
	 {0x02,0x0b},
	 {0x03,0x01},
	 {0x06,0x70},
	 {0x07,0x00},
	 {0x08,0x01},
	 {0x09,0x00},
	 {0xfd,0x02},
	 {0x3d,0x0d},
	 {0x3e,0x5d},
	 {0x3f,0x00},
	 {0x88,0x92},
	 {0x89,0x81},
	 {0x8a,0x54},
	 {0xfd,0x02},
	 {0xbe,0xd0},
	 {0xbf,0x04},
	 {0xd0,0xd0},
	 {0xd1,0x04},
	 {0xc9,0xd0},
	 {0xca,0x04},

{0xb8,0x70},  //mean_nr_dummy
{0xb9,0x80},  //mean_dummy_nr
{0xba,0x30},  //mean_dummy_low
{0xbb,0x45},  //mean_low_dummy
{0xbc,0x90},  //rpc_heq_low
{0xbd,0x70},  //rpc_heq_dummy
{0xfd,0x03},
{0x77,0x48},	//rpc_heq_nr2

//rpc
{0xfd,0x01},
{0xe0,0x48},
{0xe1,0x38},
{0xe2,0x30},
{0xe3,0x2c},
{0xe4,0x2c},
{0xe5,0x2a},
{0xe6,0x2a},
{0xe7,0x28},
{0xe8,0x28},
{0xe9,0x28},
{0xea,0x26},
{0xf3,0x26},
{0xf4,0x26},
{0xfd,0x01},//ae min gain
{0x04,0x80},//rpc_max_indr//90 fs
{0x05,0x26},//c  //rpc_min_indr
{0x0a,0x48},//rpc_max_outdr
{0x0b,0x26},//rpc_min_outdr

{0xfd,0x01},//ae target
{0xeb,0x70},//target_indr
{0xec,0x70},//target_outdr
{0xed,0x06},//lock_range
{0xee,0x0a},//hold_range

//????
{0xfd,0x03},

{0x52,0xff},//dpix_wht_ofst_outdoor
{0x53,0x60},//dpix_wht_ofst_normal1
{0x94,0x20},//dpix_wht_ofst_normal2//00 fs
{0x54,0x00},//dpix_wht_ofst_dummy
{0x55,0x00},//dpix_wht_ofst_low

{0x56,0x80},//dpix_blk_ofst_outdoor
{0x57,0x80},//dpix_blk_ofst_normal1
{0x95,0x80},//dpix_blk_ofst_normal2//00 fs
{0x58,0x00},//dpix_blk_ofst_dummy
{0x59,0x00},//dpix_blk_ofst_low

{0x5a,0xf6},//dpix_wht_ratio
{0x5b,0x00},
{0x5c,0x88},//dpix_blk_ratio
{0x5d,0x00},
{0x96,0x68},//dpix_wht/blk_ratio_nr2//00 fs

{0xfd,0x03},
{0x8a,0x00},
{0x8b,0x00},
{0x8c,0xff},

{0x22,0xff},//dem_gdif_thr_outdoor
{0x23,0xff},//dem_gdif_thr_normal
{0x24,0xff},//dem_gdif_thr_dummy
{0x25,0xff},//dem_gdif_thr_low

{0x5e,0xff},//dem_gwnd_wht_outdoor
{0x5f,0xff},//dem_gwnd_wht_normal
{0x60,0xff},//dem_gwnd_wht_dummy
{0x61,0xff},//dem_gwnd_wht_low
{0x62,0x00},//dem_gwnd_blk_outdoor
{0x63,0x00},//dem_gwnd_blk_normal
{0x64,0x00},//dem_gwnd_blk_dummy
{0x65,0x00},//dem_gwnd_blk_low

//lsc
{0xfd,0x01},
{0x21,0x00},//lsc_sig_ru lsc_sig_lu
{0x22,0x00},//lsc_sig_rd lsc_sig_ld
{0x26,0x60},//lsc_gain_thr
{0x27,0x14},//lsc_exp_thrl
{0x28,0x05},//lsc_exp_thrh
{0x29,0x00},//lsc_dec_fac     ?dummy??shading ?????????
{0x2a,0x01},//lsc_rpc_en lens?????

{0xfd,0x01},
{0xa1,0x1d},
{0xa2,0x20},
{0xa3,0x20},
{0xa4,0x1d},
{0xa5,0x1d},
{0xa6,0x1d},
{0xa7,0x20},
{0xa8,0x1b},
{0xa9,0x1c},
{0xaa,0x1e},
{0xab,0x1e},
{0xac,0x1c},
{0xad,0x0a},
{0xae,0x09},
{0xaf,0x05},
{0xb0,0x05},
{0xb1,0x0a},
{0xb2,0x0a},
{0xb3,0x05},
{0xb4,0x07},
{0xb5,0x0a},//0x09 fs
{0xb6,0x0a},//0x0a fs
{0xb7,0x04},//0x0a fs
{0xb8,0x07}, //0x07

//AWB
{0xfd,0x02},

{0x26,0xac},//Red channel gain
{0x27,0x91},//Blue channel gain

{0xfd,0x00},
{0xe7,0x03},
{0xe7,0x00},
{0xfd,0x02},


{0x28,0xcc},//Y top value limit
{0x29,0x01},//Y bot value limit
{0x2a,0x02},//rg_limit_log
{0x2b,0x16},//bg_limit_log
{0x2c,0x20},//Awb image center row start
{0x2d,0xdc},//Awb image center row end
{0x2e,0x20},//Awb image center col start
{0x2f,0x96},//Awb image center col end
{0x1b,0x80},//b,g mult a constant for detect white pixel
{0x1a,0x80},//r,g mult a constant for detect white pixel
{0x18,0x16},//wb_fine_gain_step,wb_rough_gain_step
{0x19,0x26},//wb_dif_fine_th, wb_dif_rough_th
{0x10,0x0a},

//d65
{0x66,0x3E},//41;3a
{0x67,0x66},//6d;6a
{0x68,0xC5},//a9;b3
{0x69,0xE3},//d1;de
{0x6a,0xa5},//

//indoor
{0x7c,0x33},//b
{0x7d,0x58},//51
{0x7e,0xe7},//e7
{0x7f,0x0c},//06
{0x80,0xa6},//

//cwf
{0x70,0x21},//21;1f
{0x71,0x45},//b;49
{0x72,0x0C},//fd;05
{0x73,0x2E},//22;2e
{0x74,0xaa},//a6

//tl84
{0x6b,0x00},//09
{0x6c,0x20},//32
{0x6d,0x12},//0b
{0x6e,0x36},//34
{0x6f,0xaa},//aa

//f
{0x61,0xeE},//f3;e9
{0x62,0x11},//e;16
{0x63,0x2D},//20;2a
{0x64,0x56},//48;4c
{0x65,0x6a},

{0x75,0x00},
{0x76,0x09},
{0x77,0x02},
{0x0e,0x16},
{0x3b,0x09},//awb
{0xfd,0x02},//awb outdoor mode
{0x02,0x00},//outdoor exp 5msb
{0x03,0x10},//outdoor exp 8lsb
{0x04,0xf0},//outdoor rpc
{0xf5,0xb3},//outdoor rgain top
{0xf6,0x80},//outdoor rgain bot
{0xf7,0xe0},//outdoor bgain top
{0xf8,0x89},//outdoor bgain bot

//skin detec
{0xfd,0x02},
{0x08,0x00},
{0x09,0x04},

{0xfd,0x02},
{0xdd,0x0f},//raw smooth en
{0xde,0x0f},//sharpen en

{0xfd,0x02},// sharp
//{0x57,0x28},//raw_sharp_y_base
{0x57,0x1a}, //hanlei//30 fs
{0x58,0x10},//raw_sharp_y_min
{0x59,0xe0},//raw_sharp_y_max
//{0x5a,0x20},//30  ;raw_sharp_rangek_neg
{0x5a,0x20}, //hanlei//00 fs
//{0x5b,0x20},//raw_sharp_rangek_pos
{0x5b,0x20},//hanlei//0d fs

{0xcb,0x04},//18//raw_sharp_range_base_outdoor//04 fs
{0xcc,0x0b},//18//raw_sharp_range_base_nr //0b fs
{0xcd,0x10},//18//raw_sharp_range_base_dummy//10 fs
{0xce,0x1a},//18//raw_sharp_range_base_low//1a fs

{0xfd,0x03},
{0x87,0x04},//raw_sharp_range_ofst1	4x
{0x88,0x08},//raw_sharp_range_ofst2	8x
{0x89,0x10},//raw_sharp_range_ofst3	16x

{0xfd,0x02},
{0xe8,0x68},//sharpness gain for increasing pixel?s Y, in outdoor//30//50 fs
{0xec,0x70},//sharpness gain for decreasing pixel?s Y, in outdoor//30//60 fs
{0xe9,0x68},//sharpness gain for increasing pixel?s Y, in normal
{0xed,0x70},//sharpness gain for decreasing pixel?s Y, in normal
{0xea,0x68},//sharpness gain for increasing pixel?s Y,in dummy //20//48 fs
{0xee,0x70},//sharpness gain for decreasing pixel?s Y, in dummy//30//50 fs
{0xeb,0x38},//sharpness gain for increasing pixel?s Y,in lowlight //20 fs
{0xef,0x40},//sharpness gain for decreasing pixel?s Y, in low light//30 fs

{0xfd,0x02},//skin sharpen
{0xdc,0x04},//skin_sharp_sel?????
{0x05,0x6f},//skin_num_th2?????????????????

//????
{0xfd,0x02},
{0xf4,0x30},//raw_ymin
{0xfd,0x03},//

{0x97,0x80},//raw_ymax_outdoor
{0x98,0x80},//raw_ymax_normal
{0x99,0x80},//raw_ymax_dummy
{0x9a,0x80},//raw_ymax_low
{0xfd,0x02},
{0xe4,0xff},//40//raw_yk_fac_outdoor
{0xe5,0xff},//40//raw_yk_fac_normal
{0xe6,0xff},//40//raw_yk_fac_dummy
{0xe7,0xff},//40//raw_yk_fac_low

/*{0xfd,0x03},
{0x72,0x00},//raw_lsc_fac_outdoor
{0x73,0x20},//raw_lsc_fac_normal
{0x74,0x20},//raw_lsc_fac_dummy
{0x75,0x20},//raw_lsc_fac_low  */

{0xfd,0x03},
{0x72,0x00},//raw_lsc_fac_outdoor
{0x73,0x08},//raw_lsc_fac_normal
{0x74,0x0c},//raw_lsc_fac_dummy
{0x75,0x10},//raw_lsc_fac_low

//???????
{0xfd,0x02},//  //raw_dif_thr
{0x78,0x20},//20//raw_dif_thr_outdoor
{0x79,0x20},//18//0a
{0x7a,0x14},//10//10
{0x7b,0x08},//08//20

//Gr?Gb????
{0x81,0x20},//18//10;raw_grgb_thr_outdoor
{0x82,0x20},//10//10
{0x83,0x08},//08//10
{0x84,0x08},//08//10

/*{0xfd,0x03},
{0x7e,0x10},//raw_noise_base_outdoor
{0x7f,0x18},//raw_noise_base_normal
{0x80,0x20},//raw_noise_base_dummy
{0x81,0x30},//raw_noise_base_low  */

{0xfd,0x03},
{0x7e,0x10},//raw_noise_base_outdoor
{0x7f,0x20},//raw_noise_base_normal
{0x80,0x20},//raw_noise_base_dummy
{0x81,0x28},//raw_noise_base_low

{0x7c,0xff},//raw_noise_base_dark

/*{0x82,0x44},//{raw_dns_fac_outdoor,raw_dns_fac_normal}
{0x83,0x22},//{raw_dns_fac_dummy,raw_dns_fac_low}    */

{0x82,0x54},//{raw_dns_fac_outdoor,raw_dns_fac_normal}
{0x83,0x43},//{raw_dns_fac_dummy,raw_dns_fac_low}


{0x84,0x08},//raw_noise_ofst1 	4x
{0x85,0x20},//raw_noise_ofst2	8x
{0x86,0x48},//raw_noise_ofst3	16x

//?????
{0xfd,0x03},//
{0x66,0x18},//pf_bg_thr_normal b-g>thr
{0x67,0x28},//pf_rg_thr_normal r-g<thr
{0x68,0x20},//pf_delta_thr_normal |val|>thr
{0x69,0x88},//pf_k_fac val/16
{0x9b,0x18},//pf_bg_thr_outdoor
{0x9c,0x28},//pf_rg_thr_outdoor
{0x9d,0x20},//pf_delta_thr_outdoor

//Gamma
{0xfd,0x01},
{0x8b,0x00},//00
{0x8c,0x13},//0f
{0x8d,0x21},//21
{0x8e,0x32},//2c
{0x8f,0x43},//37
{0x90,0x5c},//46
{0x91,0x6c},//53
{0x92,0x7a},//5e
{0x93,0x87},//6a
{0x94,0x9c},//7d
{0x95,0xaa},//8d
{0x96,0xba},//9e
{0x97,0xc5},//ac
{0x98,0xce},//ba
{0x99,0xd4},//c6
{0x9a,0xdc},//d1
{0x9b,0xe3},//da
{0x9c,0xea},//e4
{0x9d,0xef},//eb
{0x9e,0xf5},//f2
{0x9f,0xfb},//f9
{0xa0,0xff},//ff


//CCM
{0xfd,0x02},
{0x15,0xa8},//b0 fs//a8 fs
{0x16,0x95}, //95 fs
/*//!F
{0xa0,0x99},//8c
{0xa1,0xf4},//fa
{0xa2,0xf4},//fa
{0xa3,0xf4},//ed
{0xa4,0x99},//a6
{0xa5,0xf4},//ed
{0xa6,0xf4},//e7
{0xa7,0xda},//f4
{0xa8,0xb3},//a6
{0xa9,0x3c},//c
{0xaa,0x33},//33
{0xab,0x0f},//0f */

{0xa0,0x97},//8c
{0xa1,0xea},//fa
{0xa2,0xff},//fa
{0xa3,0x0e},//ed
{0xa4,0x77},//a6//77 fs
{0xa5,0xfa},//ed
{0xa6,0x08},//e7
{0xa7,0xcb},//f4
{0xa8,0xad},//a6
{0xa9,0x3c},//c
{0xaa,0x30},//33
{0xab,0x0c},//0f

//F
{0xac,0x99},//0x8c
{0xad,0xf4},//0xf4
{0xae,0xf4},//0x00
{0xaf,0xf4},//0xe7
{0xb0,0x99},//0xc0
{0xb1,0xf4},//0xda
{0xb2,0xf4},//0xf4
{0xb3,0xda},//0xcd
{0xb4,0xb3},//0xc0
{0xb5,0x3c},//0x0c
{0xb6,0x33},//0x33
{0xb7,0x0f},//0x0f

{0xfd,0x01},//auto_sat
{0xd2,0x3d},//autosa_en[0]  //2d fs
{0xd1,0x38},//lum thr in green enhance
{0xdd,0x3f},
{0xde,0x3f},

//auto sat
{0xfd,0x02},
{0xc1,0x40},
{0xc2,0x40},
{0xc3,0x40},
{0xc4,0x40},
{0xc5,0x80},
{0xc6,0x80},
{0xc7,0x80},
{0xc8,0x80},

//sat u
{0xfd,0x01},
{0xd3,0x98}, //90 fs
{0xd4,0x98},
{0xd5,0x88},  //80 fs
{0xd6,0x80},


//sat v
{0xd7,0x98},//90 fs
{0xd8,0x98},
{0xd9,0x88}, //80 fs
{0xda,0x80},


{0xfd,0x03},
{0x76,0x10},
{0x7a,0x40},
{0x7b,0x40},
//auto_sat
{0xfd,0x01},
{0xc2,0xcc},//aa ;u_v_th_outdoor???????????????
{0xc3,0xaa},//aa ;u_v_th_nr
{0xc4,0xaa},//44 ;u_v_th_dummy
{0xc5,0xaa},//44 ;u_v_th_low

//low_lum_offset
{0xfd,0x01},
{0xcd,0x10},
{0xce,0x10},

//gw
{0xfd,0x02},
{0x35,0x6f},//v_fix_dat
{0x37,0x13},

//heq
{0xfd,0x01},//
{0xdb,0x00},//buf_heq_offset

{0x10,0x80},//ku_outdoor
{0x11,0xb0},//ku_nr
{0x12,0x80},//ku_dummy
{0x13,0xb0},//ku_low
{0x14,0x80},//kl_outdoor
{0x15,0xa0},//kl_nr
{0x16,0x80},//kl_dummy
{0x17,0xa0},//kl_low

{0xfd,0x03},
{0x00,0x80},//ctf_heq_mean
{0x03,0x70},//ctf_range_thr   ???????????
{0x06,0xd8},//ctf_reg_max
{0x07,0x28},//ctf_reg_min
{0x0a,0xfd},//ctf_lum_ofst
{0x01,0x00},//ctf_posk_fac_outdoor
{0x02,0x00},//ctf_posk_fac_nr
{0x04,0x00},//ctf_posk_fac_dummy
{0x05,0x00},//ctf_posk_fac_low
{0x0b,0x00},//ctf_negk_fac_outdoor
{0x0c,0x00},//ctf_negk_fac_nr
{0x0d,0x00},//ctf_negk_fac_dummy
{0x0e,0x00},//ctf_negk_fac_low
{0x08,0x10},

//CNR


{0xfd,0x02},//cnr
{0x8e,0x0a},//cnr_uv_grad_thr
{0x8f,0x03},//[0]0 vertical,1 horizontal
{0x90,0x40},//cnrH_thr_outdoor
{0x91,0x40},//cnrH_thr_nr
{0x92,0x60},//cnrH_thr_dummy
{0x93,0x80},//cnrH_thr_low
{0x94,0x80},//cnrV_thr_outdoor
{0x95,0x80},//cnrV_thr_nr
{0x96,0x80},//cnrV_thr_dummy
{0x97,0x80},//cnrV_thr_low

{0x98,0x80},//cnr_grad_thr_outdoor
{0x99,0x80},//cnr_grad_thr_nr
{0x9a,0x80},//cnr_grad_thr_dummy
{0x9b,0x80},//cnr_grad_thr_low

{0x9e,0x44},
{0x9f,0x44},

{0xfd,0x02},//auto
{0x85,0x00},//enable 50Hz/60Hz function[4]  [3:0] interval_line
{0xfd,0x01},//
{0x00,0x00},//fix mode
{0x32,0x15},//ae en
{0x33,0xef},//ef		;lsc\bpc en
{0x34,0xef},//ynr[4]\cnr[0]\gamma[2]\colo[1]
{0x35,0x40},//YUYV
{0xfd,0x00},//
//{0x31,0x10},//size
//{0x33,0x00},
{0x3f,0x00},//mirror/flip

{0xfd,0x01},//
{0x50,0x00},//heq_auto_mode ???
{0x66,0x00},//effect

{0xfd,0x02},
{0xd6,0x0f},

{0xfd,0x00},
{0x1b,0x30},
{0xfd,0x01},
{0x36,0x00},

//UXGA
{0xfd,0x00},
{0x19,0x00},
{0x30,0x00},//00
{0x31,0x00},
{0x33,0x00},

    {0xff,0xff}//The end flag
};

static const struct regval_list sp2519_svga_regs[] = {
  /*  {0xfd, 0x00},
    {0x36, 0x1f}, // bit1: ccir656 output enable
    {0x4b, 0x00},
    {0x4c, 0x00},
    {0x47, 0x00},
    {0x48, 0x00},
    {0xfd, 0x01},
    {0x06, 0x00},
    {0x07, 0x40},
    {0x08, 0x00},
    {0x09, 0x40},
    {0x0a, 0x02},
    {0x0b, 0x58},
    {0x0c, 0x03},
    {0x0d, 0x20},
    {0x0e, 0x21},
    {0xfd, 0x00},*/
    ENDMARKER
};

static const struct regval_list sp2519_uxga_regs[] = {//caichsh
    //Resolution Setting : 1600*1200
   /* {0xfd, 0x00},
    {0x36, 0x1f}, // bit1: ccir656 output enable
    {0x47, 0x00},
    {0x48, 0x00},
    {0x49, 0x04},
    {0x4a, 0xb0},
    {0x4b, 0x00},
    {0x4c, 0x00},
    {0x4d, 0x06},
    {0x4e, 0x40},
    {0xfd, 0x01},
    {0x0e, 0x00},
    {0x0f, 0x00},
    {0xfd, 0x00},*/
    ENDMARKER,
};

static const struct regval_list sp2519_disable_regs[] = {
    {0xfd, 0x00},
    {0x36, 0x00},
    {0xfd, 0x00},
    ENDMARKER,
};


/****************************************************************************************
 * structures
 */
struct sp2519_win_size {
    char                        *name;
    __u32                       width;
    __u32                       height;
    __u32                       exposure_line_width;
    __u32                       capture_maximum_shutter;
    const struct regval_list    *win_regs;
    const struct regval_list    *lsc_regs;
    unsigned int                *frame_rate_array;
};

typedef struct {
    unsigned int max_shutter;
    unsigned int shutter;
    unsigned int gain;
    unsigned int dummy_line;
    unsigned int dummy_pixel;
    unsigned int extra_line;
} exposure_param_t;

enum prev_capt {
    PREVIEW_MODE = 0,
    CAPTURE_MODE
};

struct sp2519_priv {
    struct v4l2_subdev                  subdev;
    struct media_pad                    pad;
    struct v4l2_ctrl_handler            hdl;
    const struct sp2519_color_format    *cfmt;
    const struct sp2519_win_size        *win;
    int                                 model;
    bool                                initialized;

    /**
     * ctrls
    */
    /* standard */
    struct v4l2_ctrl *brightness;
    struct v4l2_ctrl *contrast;
    struct v4l2_ctrl *auto_white_balance;
    struct v4l2_ctrl *exposure;
    struct v4l2_ctrl *gain;
    struct v4l2_ctrl *hflip;
    struct v4l2_ctrl *white_balance_temperature;
    /* menu */
    struct v4l2_ctrl *colorfx;
    struct v4l2_ctrl *exposure_auto;
    /* custom */
    struct v4l2_ctrl *scene_exposure;
    struct v4l2_ctrl *prev_capt;

    struct v4l2_rect rect; /* Sensor window */
    struct v4l2_fract timeperframe;
    enum prev_capt prev_capt_mode;
    exposure_param_t preview_exposure_param;
    exposure_param_t capture_exposure_param;
};

struct sp2519_color_format {
    enum v4l2_mbus_pixelcode code;
    enum v4l2_colorspace colorspace;
};

/****************************************************************************************
 * tables
 */
static const struct sp2519_color_format sp2519_cfmts[] = {
    {
        .code		= V4L2_MBUS_FMT_YUYV8_2X8,
        .colorspace	= V4L2_COLORSPACE_JPEG,
    },
    {
        .code		= V4L2_MBUS_FMT_UYVY8_2X8,
        .colorspace	= V4L2_COLORSPACE_JPEG,
    },
    {
        .code		= V4L2_MBUS_FMT_YVYU8_2X8,
        .colorspace	= V4L2_COLORSPACE_JPEG,
    },
    {
        .code		= V4L2_MBUS_FMT_VYUY8_2X8,
        .colorspace	= V4L2_COLORSPACE_JPEG,
    },
};

/*
 * window size list
 */
#define VGA_WIDTH           640
#define VGA_HEIGHT          480
#define UXGA_WIDTH          1600
#define UXGA_HEIGHT         1200
#define SVGA_WIDTH          800
#define SVGA_HEIGHT         600
#define SP2519_MAX_WIDTH    UXGA_WIDTH
#define SP2519_MAX_HEIGHT   UXGA_HEIGHT
#define AHEAD_LINE_NUM		15    //10行 = 50次循环(sp2519)
#define DROP_NUM_CAPTURE			0
#define DROP_NUM_PREVIEW			0

static unsigned int frame_rate_svga[] = {12,};
static unsigned int frame_rate_uxga[] = {12,};

/* 800X600 */
static const struct sp2519_win_size sp2519_win_svga = {
    .name     = "SVGA",
    .width    = SVGA_WIDTH,
    .height   = SVGA_HEIGHT,
    .win_regs = sp2519_svga_regs,
    .frame_rate_array = frame_rate_svga,
};

/* 1600X1200 */
static const struct sp2519_win_size sp2519_win_uxga = {
    .name     = "UXGA",
    .width    = UXGA_WIDTH,
    .height   = UXGA_HEIGHT,
    .win_regs = sp2519_uxga_regs,
    .frame_rate_array = frame_rate_uxga,
};

static const struct sp2519_win_size *sp2519_win[] = {
    &sp2519_win_svga,
    &sp2519_win_uxga,
};

/****************************************************************************************
 * general functions
 */
static inline struct sp2519_priv *to_priv(struct v4l2_subdev *subdev)
{
    return container_of(subdev, struct sp2519_priv, subdev);
}

static inline struct v4l2_subdev *ctrl_to_sd(struct v4l2_ctrl *ctrl)
{
    return &container_of(ctrl->handler, struct sp2519_priv, hdl)->subdev;
}

static bool check_id(struct i2c_client *client)
{
    u8 pid = i2c_smbus_read_byte_data(client, PID);
    if (pid == 0x25)//SP2519)
        return true;

    printk(KERN_ERR "failed to check id: 0x%x\n", pid);
    return false;
}

static int sp2519_write_array(struct i2c_client *client, const struct regval_list *vals)
{
    int ret;
    while (vals->reg_num != 0xff) {
        ret = i2c_smbus_write_byte_data(client, vals->reg_num, vals->value);
        if (ret < 0)
            return ret;
        vals++;
    }
    return 0;
}

static int sp2519_mask_set(struct i2c_client *client, u8 command, u8 mask, u8 set) __attribute__((unused));
static int sp2519_mask_set(struct i2c_client *client, u8 command, u8 mask, u8 set)
{
    s32 val = i2c_smbus_read_byte_data(client, command);
    if (val < 0)
        return val;

    val &= ~mask;
    val |= set & mask;

    return i2c_smbus_write_byte_data(client, command, val);
}

static const struct sp2519_win_size *sp2519_select_win(u32 width, u32 height)
{
	const struct sp2519_win_size *win;
    int i;

    printk(KERN_ERR "%s:  width, height (%dx%d)\n", __func__, width, height);

    for (i = 0; i < ARRAY_SIZE(sp2519_win); i++) {
        win = sp2519_win[i];
        if (width == win->width && height == win->height)
            return win;
    }

    win = &sp2519_win_uxga;
    printk(KERN_ERR "%s: unsupported width, height (%dx%d)\n", __func__, width, height);
    return win;
}

static int sp2519_set_mbusformat(struct i2c_client *client, const struct sp2519_color_format *cfmt)
{
    enum v4l2_mbus_pixelcode code;
    int ret = -1;
    code = cfmt->code;
    switch (code) {
        case V4L2_MBUS_FMT_YUYV8_2X8:
            ret  = sp2519_write_array(client, sp2519_fmt_yuv422_yuyv);
            break;
        case V4L2_MBUS_FMT_UYVY8_2X8:
            ret  = sp2519_write_array(client, sp2519_fmt_yuv422_uyvy);
            break;
        case V4L2_MBUS_FMT_YVYU8_2X8:
            ret  = sp2519_write_array(client, sp2519_fmt_yuv422_yvyu);
            break;
        case V4L2_MBUS_FMT_VYUY8_2X8:
            ret  = sp2519_write_array(client, sp2519_fmt_yuv422_vyuy);
            break;
        default:
            printk(KERN_ERR "mbus code error in %s() line %d\n",__FUNCTION__, __LINE__);
    }
    return ret;
}

static int sp2519_set_params(struct v4l2_subdev *sd, u32 *width, u32 *height, enum v4l2_mbus_pixelcode code)
{
    struct sp2519_priv *priv = to_priv(sd);
    const struct sp2519_win_size *old_win, *new_win;
    int i;

    /*
     * select format
     */
    priv->cfmt = NULL;
    for (i = 0; i < ARRAY_SIZE(sp2519_cfmts); i++) {
        if (code == sp2519_cfmts[i].code) {
            priv->cfmt = sp2519_cfmts + i;
            break;
        }
    }
    if (!priv->cfmt) {
        printk(KERN_ERR "Unsupported sensor format.\n");
        return -EINVAL;
    }

    /*
     * select win
     */
    old_win = priv->win;
    new_win = sp2519_select_win(*width, *height);
    if (!new_win) {
        printk(KERN_ERR "Unsupported win size\n");
        return -EINVAL;
    }
    priv->win = new_win;

    priv->rect.left = 0;
    priv->rect.top = 0;
    priv->rect.width = priv->win->width;
    priv->rect.height = priv->win->height;

    *width = priv->win->width;
    *height = priv->win->height;

    return 0;
}

/****************************************************************************************
 * control functions
 */
static int sp2519_set_brightness(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl)
{
    struct i2c_client *client = v4l2_get_subdevdata(sd);
    unsigned int reg_0xb5, reg_0xd3;
    int ret;

    int val = ctrl->val;

    printk("%s: val %d\n", __func__, val);

    if (val < 0 || val > 6) {
        dev_err(&client->dev, "set brightness over range, brightness = %d\n", val);
        return -ERANGE;
    }

    switch(val) {
        case 0:
            reg_0xb5 = 0xd0;
            reg_0xd3 = 0x68;
            break;
        case 1:
            reg_0xb5 = 0xe0;
            reg_0xd3 = 0x70;
            break;
        case 2:
            reg_0xb5 = 0xf0;
            reg_0xd3 = 0x78;
            break;
        case 3:
            reg_0xb5 = 0x10;
            reg_0xd3 = 0x88;//80
            break;
        case 4:
            reg_0xb5 = 0x20;
            reg_0xd3 = 0x88;
            break;
        case 5:
            reg_0xb5 = 0x30;
            reg_0xd3 = 0x90;
            break;
        case 6:
            reg_0xb5 = 0x40;
            reg_0xd3 = 0x98;
            break;
    }

   // ret = i2c_smbus_write_byte_data(client, 0xb5, reg_0xb5);
   // ret |= i2c_smbus_write_byte_data(client, 0xd3, reg_0xd3);
  //  assert(ret == 0);

    return 0;
}

static int sp2519_set_contrast(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl)
{
    /* TODO */
    int contrast = ctrl->val;
    printk("%s: val %d\n", __func__, contrast);

    return 0;
}

static int sp2519_set_auto_white_balance(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl)
{
    struct i2c_client *client = v4l2_get_subdevdata(sd);
    /* struct sp2519_priv *priv = to_priv(sd); */
    int auto_white_balance = ctrl->val;
    int ret;

    printk("%s: val %d\n", __func__, auto_white_balance);
    if (auto_white_balance < 0 || auto_white_balance > 1) {
        dev_err(&client->dev, "set auto_white_balance over range, auto_white_balance = %d\n", auto_white_balance);
        return -ERANGE;
    }

    switch(auto_white_balance) {
        case 0:
            SP2519_DEBUG(KERN_ERR "===awb disable===\n");
            ret = sp2519_write_array(client, sp2519_awb_regs_diable);
            break;
        case 1:
            SP2519_DEBUG(KERN_ERR "===awb enable===\n");
            ret = sp2519_write_array(client, sp2519_awb_regs_enable);
            break;
    }

    assert(ret == 0);

    return 0;
}

/* TODO : exposure */
static int sp2519_set_exposure(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl)
{
    /* struct i2c_client *client = v4l2_get_subdevdata(sd); */
    /* struct sp2519_priv *priv = to_priv(sd); */
    printk("%s: val %d\n", __func__, ctrl->val);
#if 0
    unsigned int reg_0x13, reg_0x02, reg_0x03;
    int ret;

    if (exposure < 0 || exposure > 0xFFFFU) {
        dev_err(&client->dev, "set exposure over range, exposure = %d\n", exposure);
        return -ERANGE;
    }

    reg_0x13 = i2c_smbus_read_byte_data(client, 0x13);
    assert(reg_0x13 >= 0);
    reg_0x13 &= ~(1U << 1); // AUTO1[1]: AEC_en

    reg_0x03 = exposure & 0xFFU;
    reg_0x02 = (exposure >> 8) & 0xFFU;

    ret = i2c_smbus_write_byte_data(client, 0x13, reg_0x13);
    ret |= i2c_smbus_write_byte_data(client, 0x02, reg_0x02);
    ret |= i2c_smbus_write_byte_data(client, 0x03, reg_0x03);
    assert(ret == 0);

    priv->exposure_auto = 0;
    priv->exposure = exposure;
    ctrl->cur.val = exposure;
#endif

    return 0;
}

/* TODO */
static int sp2519_set_gain(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl)
{
    /* struct i2c_client *client = v4l2_get_subdevdata(sd); */
    /* struct sp2519_priv *priv = to_priv(sd); */
    printk("%s: val %d\n", __func__, ctrl->val);
    return 0;
}

/* TODO */
static int sp2519_set_hflip(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl)
{
    /* struct i2c_client *client = v4l2_get_subdevdata(sd); */
    /* struct sp2519_priv *priv = to_priv(sd); */
    printk("%s: val %d\n", __func__, ctrl->val);
    return 0;
}

static int sp2519_set_white_balance_temperature(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl)
{
    struct i2c_client *client = v4l2_get_subdevdata(sd);
    /* struct sp2519_priv *priv = to_priv(sd); */
    int white_balance_temperature = ctrl->val;
    int ret;

    printk("%s: val %d\n", __func__, ctrl->val);

    switch(white_balance_temperature) {
        case V4L2_WHITE_BALANCE_INCANDESCENT:
            ret = sp2519_write_array(client, sp2519_wb_incandescence_regs);
            break;
        case V4L2_WHITE_BALANCE_FLUORESCENT:
            ret = sp2519_write_array(client, sp2519_wb_fluorescent_regs);
            break;
        case V4L2_WHITE_BALANCE_DAYLIGHT:
            ret = sp2519_write_array(client, sp2519_wb_daylight_regs);
            break;
        case V4L2_WHITE_BALANCE_CLOUDY_DAYLIGHT:
            ret = sp2519_write_array(client, sp2519_wb_cloud_regs);
            break;
        case V4L2_WHITE_BALANCE_TUNGSTEN:
            ret = sp2519_write_array(client, sp2519_wb_tungsten_regs);
            break;
        default:
            dev_err(&client->dev, "set white_balance_temperature over range, white_balance_temperature = %d\n", white_balance_temperature);
            return -ERANGE;
    }

    assert(ret == 0);

    return 0;
}

static int sp2519_set_colorfx(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl)
{
    struct i2c_client *client = v4l2_get_subdevdata(sd);
    /* struct sp2519_priv *priv = to_priv(sd); */
    int colorfx = ctrl->val;
    int ret;

    printk("%s: val %d\n", __func__, ctrl->val);

    switch (colorfx) {
        case V4L2_COLORFX_NONE: /* normal */
            ret = sp2519_write_array(client, sp2519_colorfx_none_regs);
            break;
        case V4L2_COLORFX_BW: /* black and white */
            ret = sp2519_write_array(client, sp2519_colorfx_bw_regs);
            break;
        case V4L2_COLORFX_SEPIA: /* antique ,复古*/
            ret = sp2519_write_array(client, sp2519_colorfx_sepia_regs);
            break;
        case V4L2_COLORFX_NEGATIVE: /* negative，负片 */
            ret = sp2519_write_array(client, sp2519_colorfx_negative_regs);
            break;
        default:
            dev_err(&client->dev, "set colorfx over range, colorfx = %d\n", colorfx);
            return -ERANGE;
    }

    assert(ret == 0);
    return 0;
}

static int sp2519_set_exposure_auto(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl)
{
    struct i2c_client *client = v4l2_get_subdevdata(sd);
    /* struct sp2519_priv *priv = to_priv(sd); */
    int exposure_auto = ctrl->val;

    /* unsigned int reg_0xec; */
    /* int ret; */

    printk("%s: val %d\n", __func__, ctrl->val);

    if (exposure_auto < 0 || exposure_auto > 1) {
        dev_err(&client->dev, "set exposure_auto over range, exposure_auto = %d\n", exposure_auto);
        return -ERANGE;
    }

    return 0;
}

/* TODO */
static int sp2519_set_scene_exposure(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl)
{
#if 0
    struct i2c_client *client = v4l2_get_subdevdata(sd);
    struct sp2519_priv *priv = to_priv(sd);
    int scene_exposure = ctrl->val;

    unsigned int reg_0xec;
    int ret;

    switch(scene_exposure) {
        case V4L2_SCENE_MODE_HOUSE:  //室内
            reg_0xec = 0x30;
            break;
        case V4L2_SCENE_MODE_SUNSET:  //室外
            reg_0xec = 0x20;
            break;
        default:
            dev_err(&client->dev, "set scene_exposure over range, scene_exposure = %d\n", scene_exposure);
            return -ERANGE;
    }

    ret = i2c_smbus_write_byte_data(client, 0xec, reg_0xec);
    assert(ret == 0);
#endif

    printk("%s: val %d\n", __func__, ctrl->val);
    return 0;
}

static int sp2519_set_prev_capt_mode(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl)
{
    struct i2c_client *client = v4l2_get_subdevdata(sd);
    struct sp2519_priv *priv = to_priv(sd);
    printk("%s: val %d\n", __func__, ctrl->val);

    switch(ctrl->val) {
        case PREVIEW_MODE:
            priv->prev_capt_mode = ctrl->val;
            break;
        case CAPTURE_MODE:
            priv->prev_capt_mode = ctrl->val;
            break;
        default:
            dev_err(&client->dev, "set_prev_capt_mode over range, prev_capt_mode = %d\n", ctrl->val);
            return -ERANGE;
    }

    return 0;
}

static int sp2519_s_ctrl(struct v4l2_ctrl *ctrl)
{
    struct v4l2_subdev *sd = ctrl_to_sd(ctrl);
    struct i2c_client *client = v4l2_get_subdevdata(sd);
    int ret = 0;

    switch (ctrl->id) {
        case V4L2_CID_BRIGHTNESS:
            sp2519_set_brightness(sd, ctrl);
            break;

        case V4L2_CID_CONTRAST:
            sp2519_set_contrast(sd, ctrl);
            break;

        case V4L2_CID_AUTO_WHITE_BALANCE:
            sp2519_set_auto_white_balance(sd, ctrl);
            break;

        case V4L2_CID_EXPOSURE:
            sp2519_set_exposure(sd, ctrl);
            break;

        case V4L2_CID_GAIN:
            sp2519_set_gain(sd, ctrl);
            break;

        case V4L2_CID_HFLIP:
            sp2519_set_hflip(sd, ctrl);
            break;

        case V4L2_CID_WHITE_BALANCE_TEMPERATURE:
            sp2519_set_white_balance_temperature(sd, ctrl);
            break;

        case V4L2_CID_COLORFX:
            sp2519_set_colorfx(sd, ctrl);
            break;

        case V4L2_CID_EXPOSURE_AUTO:
            sp2519_set_exposure_auto(sd, ctrl);
            break;

        case V4L2_CID_SCENE_EXPOSURE:
            sp2519_set_scene_exposure(sd, ctrl);
            break;

        case V4L2_CID_PRIVATE_PREV_CAPT:
            sp2519_set_prev_capt_mode(sd, ctrl);
            break;

        default:
            dev_err(&client->dev, "%s: invalid control id %d\n", __func__, ctrl->id);
            return -EINVAL;
    }

    return ret;
}

static const struct v4l2_ctrl_ops sp2519_ctrl_ops = {
    .s_ctrl = sp2519_s_ctrl,
};

static const struct v4l2_ctrl_config sp2519_custom_ctrls[] = {
    {
        .ops    = &sp2519_ctrl_ops,
        .id     = V4L2_CID_SCENE_EXPOSURE,
        .type   = V4L2_CTRL_TYPE_INTEGER,
        .name   = "SceneExposure",
        .min    = 0,
        .max    = 1,
        .def    = 0,
        .step   = 1,
    }, {
        .ops    = &sp2519_ctrl_ops,
        .id     = V4L2_CID_PRIVATE_PREV_CAPT,
        .type   = V4L2_CTRL_TYPE_INTEGER,
        .name   = "PrevCapt",
        .min    = 0,
        .max    = 1,
        .def    = 0,
        .step   = 1,
    }
};

static int sp2519_initialize_ctrls(struct sp2519_priv *priv)
{
    v4l2_ctrl_handler_init(&priv->hdl, NUM_CTRLS);

    /* standard ctrls */
    priv->brightness = v4l2_ctrl_new_std(&priv->hdl, &sp2519_ctrl_ops,
            V4L2_CID_BRIGHTNESS, 0, 6, 1, 0);
    if (!priv->brightness) {
        printk(KERN_ERR "%s: failed to create brightness ctrl\n", __func__);
        return -ENOENT;
    }

    priv->contrast = v4l2_ctrl_new_std(&priv->hdl, &sp2519_ctrl_ops,
            V4L2_CID_CONTRAST, -6, 6, 1, 0);
    if (!priv->contrast) {
        printk(KERN_ERR "%s: failed to create contrast ctrl\n", __func__);
        return -ENOENT;
    }

    priv->auto_white_balance = v4l2_ctrl_new_std(&priv->hdl, &sp2519_ctrl_ops,
            V4L2_CID_AUTO_WHITE_BALANCE, 0, 1, 1, 1);
    if (!priv->auto_white_balance) {
        printk(KERN_ERR "%s: failed to create auto_white_balance ctrl\n", __func__);
        return -ENOENT;
    }

#if 0
    priv->exposure = v4l2_ctrl_new_std(&priv->hdl, &sp2519_ctrl_ops,
            V4L2_CID_EXPOSURE, 0, 0xFFFF, 1, 500);
    if (!priv->exposure) {
        printk(KERN_ERR "%s: failed to create exposure ctrl\n", __func__);
        return -ENOENT;
    }
#endif

    priv->gain = v4l2_ctrl_new_std(&priv->hdl, &sp2519_ctrl_ops,
            V4L2_CID_GAIN, 0, 0xFF, 1, 128);
    if (!priv->gain) {
        printk(KERN_ERR "%s: failed to create gain ctrl\n", __func__);
        return -ENOENT;
    }

    priv->hflip = v4l2_ctrl_new_std(&priv->hdl, &sp2519_ctrl_ops,
            V4L2_CID_HFLIP, 0, 1, 1, 0);
    if (!priv->hflip) {
        printk(KERN_ERR "%s: failed to create hflip ctrl\n", __func__);
        return -ENOENT;
    }

    priv->white_balance_temperature = v4l2_ctrl_new_std(&priv->hdl, &sp2519_ctrl_ops,
            V4L2_CID_WHITE_BALANCE_TEMPERATURE, 0, 3, 1, 1);
    if (!priv->white_balance_temperature) {
        printk(KERN_ERR "%s: failed to create white_balance_temperature ctrl\n", __func__);
        return -ENOENT;
    }

    /* standard menus */
    priv->colorfx = v4l2_ctrl_new_std_menu(&priv->hdl, &sp2519_ctrl_ops,
            V4L2_CID_COLORFX, 3, 0, 0);
    if (!priv->colorfx) {
        printk(KERN_ERR "%s: failed to create colorfx ctrl\n", __func__);
        return -ENOENT;
    }

    priv->exposure_auto = v4l2_ctrl_new_std_menu(&priv->hdl, &sp2519_ctrl_ops,
            V4L2_CID_EXPOSURE_AUTO, 1, 0, 1);
    if (!priv->exposure_auto) {
        printk(KERN_ERR "%s: failed to create exposure_auto ctrl\n", __func__);
        return -ENOENT;
    }

    /* custom ctrls */
    priv->scene_exposure = v4l2_ctrl_new_custom(&priv->hdl, &sp2519_custom_ctrls[0], NULL);
    if (!priv->scene_exposure) {
        printk(KERN_ERR "%s: failed to create scene_exposure ctrl\n", __func__);
        return -ENOENT;
    }

    priv->prev_capt = v4l2_ctrl_new_custom(&priv->hdl, &sp2519_custom_ctrls[1], NULL);
    if (!priv->prev_capt) {
        printk(KERN_ERR "%s: failed to create prev_capt ctrl\n", __func__);
        return -ENOENT;
    }

    priv->subdev.ctrl_handler = &priv->hdl;
    if (priv->hdl.error) {
        printk(KERN_ERR "%s: ctrl handler error(%d)\n", __func__, priv->hdl.error);
        v4l2_ctrl_handler_free(&priv->hdl);
        return -EINVAL;
    }

    return 0;
}

static int sp2519_save_exposure_param(struct v4l2_subdev *sd)
{
    struct i2c_client *client = v4l2_get_subdevdata(sd);
    struct sp2519_priv *priv = to_priv(sd);
    int ret = 0;
    unsigned int reg_0x03 = 0x20;
    unsigned int reg_0x80;
    unsigned int reg_0x81;
    unsigned int reg_0x82;

    i2c_smbus_write_byte_data(client, 0x03, reg_0x03); //page 20
    reg_0x80 = i2c_smbus_read_byte_data(client, 0x80);
    reg_0x81 = i2c_smbus_read_byte_data(client, 0x81);
    reg_0x82 = i2c_smbus_read_byte_data(client, 0x82);

    priv->preview_exposure_param.shutter = (reg_0x80 << 16)|(reg_0x81 << 8)|reg_0x82;
    priv->capture_exposure_param.shutter = (priv->preview_exposure_param.shutter)/2;

    return ret;
}

static int sp2519_set_exposure_param(struct v4l2_subdev *sd) __attribute__((unused));
static int sp2519_set_exposure_param(struct v4l2_subdev *sd)
{
    struct i2c_client *client = v4l2_get_subdevdata(sd);
    struct sp2519_priv *priv = to_priv(sd);
    int ret;
    unsigned int reg_0x03 = 0x20;
    unsigned int reg_0x83;
    unsigned int reg_0x84;
    unsigned int reg_0x85;

    if(priv->capture_exposure_param.shutter < 1)
        priv->capture_exposure_param.shutter = 1;

    reg_0x83 = (priv->capture_exposure_param.shutter)>>16;
    reg_0x84 = ((priv->capture_exposure_param.shutter)>>8) & 0x000000FF;
    reg_0x85 = (priv->capture_exposure_param.shutter) & 0x000000FF;

    ret = i2c_smbus_write_byte_data(client, 0x03, reg_0x03); //page 20
    ret |= i2c_smbus_write_byte_data(client, 0x83, reg_0x83);
    ret |= i2c_smbus_write_byte_data(client, 0x84, reg_0x84);
    ret |= i2c_smbus_write_byte_data(client, 0x85, reg_0x85);

    return 0;
}

/****************************************************************************************
 * v4l2 subdev ops
 */

/**
 * core ops
 */
static int sp2519_g_chip_ident(struct v4l2_subdev *sd, struct v4l2_dbg_chip_ident *id)
{
    struct sp2519_priv *priv = to_priv(sd);
    id->ident    = priv->model;
    id->revision = 0;
    return 0;
}

static int sp2519_s_power(struct v4l2_subdev *sd, int on)
{
    /* used when suspending */
    /* printk("%s: on %d\n", __func__, on); */
    if (!on) {
        struct sp2519_priv *priv = to_priv(sd);
        priv->initialized = false;
    }
    return 0;
}

static const struct v4l2_subdev_core_ops sp2519_subdev_core_ops = {
    .g_chip_ident	= sp2519_g_chip_ident,
    .s_power        = sp2519_s_power,
    .s_ctrl         = v4l2_subdev_s_ctrl,
};

/**
 * video ops
 */
static int sp2519_s_stream(struct v4l2_subdev *sd, int enable)
{
    struct i2c_client *client = v4l2_get_subdevdata(sd);
    struct sp2519_priv *priv = to_priv(sd);
    int ret = 0;

    printk("%s: enable %d, initialized %d\n", __func__, enable, priv->initialized);
    if (enable) {
        if (!priv->win || !priv->cfmt) {
            dev_err(&client->dev, "norm or win select error\n");
            return -EPERM;
        }
         /* write init regs */
        if (!priv->initialized) {
            if (!check_id(client))
                return -EINVAL;

            ret = sp2519_write_array(client, sp2519_svga_init_regs);
            if (ret < 0) {
                printk(KERN_ERR "%s: failed to sp2519_write_array init regs\n", __func__);
                return -EIO;
            }
            priv->initialized = true;
        }

        ret = sp2519_write_array(client, priv->win->win_regs);
        if (ret < 0) {
            printk(KERN_ERR "%s: failed to sp2519_write_array win regs\n", __func__);
            return -EIO;
        }

        ret = sp2519_set_mbusformat(client, priv->cfmt);
        if (ret < 0) {
            printk(KERN_ERR "%s: failed to sp2519_set_mbusformat()\n", __func__);
            return -EIO;
        }
    } else {
        sp2519_write_array(client, sp2519_disable_regs);
    }

    return ret;
}

static int sp2519_enum_framesizes(struct v4l2_subdev *sd, struct v4l2_frmsizeenum *fsize)
{
    struct i2c_client *client = v4l2_get_subdevdata(sd);

    if (fsize->index >= ARRAY_SIZE(sp2519_win)) {
        dev_err(&client->dev, "index(%d) is over range %d\n", fsize->index, ARRAY_SIZE(sp2519_win));
        return -EINVAL;
    }

    switch (fsize->pixel_format) {
        case V4L2_PIX_FMT_YUV420:
        case V4L2_PIX_FMT_YUV422P:
        case V4L2_PIX_FMT_NV12:
        case V4L2_PIX_FMT_YUYV:
            fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
            fsize->discrete.width = sp2519_win[fsize->index]->width;
            fsize->discrete.height = sp2519_win[fsize->index]->height;
            break;
        default:
            dev_err(&client->dev, "pixel_format(%d) is Unsupported\n", fsize->pixel_format);
            return -EINVAL;
    }

    dev_info(&client->dev, "type %d, width %d, height %d\n", V4L2_FRMSIZE_TYPE_DISCRETE, fsize->discrete.width, fsize->discrete.height);
    return 0;
}

static int sp2519_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned int index,
        enum v4l2_mbus_pixelcode *code)
{
    if (index >= ARRAY_SIZE(sp2519_cfmts))
        return -EINVAL;

    *code = sp2519_cfmts[index].code;
    return 0;
}

static int sp2519_g_mbus_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
    struct i2c_client *client = v4l2_get_subdevdata(sd);
    struct sp2519_priv *priv = to_priv(sd);
    if (!priv->win || !priv->cfmt) {
        u32 width = SVGA_WIDTH;
        u32 height = SVGA_HEIGHT;
        int ret = sp2519_set_params(sd, &width, &height, V4L2_MBUS_FMT_UYVY8_2X8);
        if (ret < 0) {
            dev_info(&client->dev, "%s, %d\n", __func__, __LINE__);
            return ret;
        }
    }

    mf->width   = priv->win->width;
    mf->height  = priv->win->height;
    mf->code    = priv->cfmt->code;
    mf->colorspace  = priv->cfmt->colorspace;
    mf->field   = V4L2_FIELD_NONE;
    dev_info(&client->dev, "%s, %d\n", __func__, __LINE__);
    return 0;
}

static int sp2519_try_mbus_fmt(struct v4l2_subdev *sd,
        struct v4l2_mbus_framefmt *mf)
{
    /* struct i2c_client *client = v4l2_get_subdevdata(sd); */
    struct sp2519_priv *priv = to_priv(sd);
    const struct sp2519_win_size *win;
    int i;

    /*
     * select suitable win
     */
    win = sp2519_select_win(mf->width, mf->height);
    if (!win)
        return -EINVAL;

    mf->width   = win->width;
    mf->height  = win->height;
    mf->field   = V4L2_FIELD_NONE;


    for (i = 0; i < ARRAY_SIZE(sp2519_cfmts); i++)
        if (mf->code == sp2519_cfmts[i].code)
            break;

    if (i == ARRAY_SIZE(sp2519_cfmts)) {
        /* Unsupported format requested. Propose either */
        if (priv->cfmt) {
            /* the current one or */
            mf->colorspace = priv->cfmt->colorspace;
            mf->code = priv->cfmt->code;
        } else {
            /* the default one */
            mf->colorspace = sp2519_cfmts[0].colorspace;
            mf->code = sp2519_cfmts[0].code;
        }
    } else {
        /* Also return the colorspace */
        mf->colorspace	= sp2519_cfmts[i].colorspace;
    }

    return 0;
}

static int sp2519_s_mbus_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
    /* struct i2c_client *client = v4l2_get_subdevdata(sd); */
    struct sp2519_priv *priv = to_priv(sd);

    int ret = sp2519_set_params(sd, &mf->width, &mf->height, mf->code);
    if (!ret)
        mf->colorspace = priv->cfmt->colorspace;

    return ret;
}

static const struct v4l2_subdev_video_ops sp2519_subdev_video_ops = {
    .s_stream               = sp2519_s_stream,
    .enum_framesizes        = sp2519_enum_framesizes,
    .enum_mbus_fmt          = sp2519_enum_mbus_fmt,
    .g_mbus_fmt             = sp2519_g_mbus_fmt,
    .try_mbus_fmt           = sp2519_try_mbus_fmt,
    .s_mbus_fmt             = sp2519_s_mbus_fmt,
};

/**
 * pad ops
 */
static int sp2519_s_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
        struct v4l2_subdev_format *fmt)
{
    struct v4l2_mbus_framefmt *mf = &fmt->format;
    printk("%s: %dx%d\n", __func__, mf->width, mf->height);
    return sp2519_s_mbus_fmt(sd, mf);
}

static const struct v4l2_subdev_pad_ops sp2519_subdev_pad_ops = {
    .set_fmt    = sp2519_s_fmt,
};

/**
 * subdev ops
 */
static const struct v4l2_subdev_ops sp2519_subdev_ops = {
    .core   = &sp2519_subdev_core_ops,
    .video  = &sp2519_subdev_video_ops,
    .pad    = &sp2519_subdev_pad_ops,
};

/**
 * media_entity_operations
 */
static int sp2519_link_setup(struct media_entity *entity,
        const struct media_pad *local,
        const struct media_pad *remote, u32 flags)
{
    printk("%s\n", __func__);
    return 0;
}

static const struct media_entity_operations sp2519_media_ops = {
    .link_setup = sp2519_link_setup,
};

/****************************************************************************************
 * initialize
 */
static void sp2519_priv_init(struct sp2519_priv * priv)
{
    priv->model = V4L2_IDENT_SP2519;
    priv->prev_capt_mode = PREVIEW_MODE;
    priv->timeperframe.denominator = 12;//30;
    priv->timeperframe.numerator = 1;
    priv->win = &sp2519_win_svga;
}

static int sp2519_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct sp2519_priv *priv;
    struct v4l2_subdev *sd;
    int ret;

    priv = kzalloc(sizeof(struct sp2519_priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    sp2519_priv_init(priv);

    sd = &priv->subdev;
    strcpy(sd->name, MODULE_NAME);

    /* register subdev */
    v4l2_i2c_subdev_init(sd, client, &sp2519_subdev_ops);

    sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
    priv->pad.flags = MEDIA_PAD_FL_SOURCE;
    sd->entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;
    sd->entity.ops  = &sp2519_media_ops;
    if (media_entity_init(&sd->entity, 1, &priv->pad, 0)) {
        dev_err(&client->dev, "%s: failed to media_entity_init()\n", __func__);
        kfree(priv);
        return -ENOENT;
    }

    ret = sp2519_initialize_ctrls(priv);
    if (ret < 0) {
        printk(KERN_ERR "%s: failed to initialize controls\n", __func__);
        kfree(priv);
        return ret;
    }

    return 0;
}

static int sp2519_remove(struct i2c_client *client)
{
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    v4l2_device_unregister_subdev(sd);
    v4l2_ctrl_handler_free(sd->ctrl_handler);
    media_entity_cleanup(&sd->entity);
    kfree(to_priv(sd));
    return 0;
}

static const struct i2c_device_id sp2519_id[] = {
    { MODULE_NAME, 0 },
    { }
};

MODULE_DEVICE_TABLE(i2c, sp2519_id);

static struct i2c_driver sp2519_i2c_driver = {
    .driver = {
        .name = MODULE_NAME,
    },
    .probe    = sp2519_probe,
    .remove   = sp2519_remove,
    .id_table = sp2519_id,
};

module_i2c_driver(sp2519_i2c_driver);

MODULE_DESCRIPTION("SoC Camera driver for sp2519");
MODULE_AUTHOR("caichsh(caichsh@artekmicro.com)");
MODULE_LICENSE("GPL v2");
