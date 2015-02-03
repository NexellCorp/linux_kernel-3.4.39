
/*
 * sp0718 Camera Driver
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
 * fixed by swpark@nexell.co.kr for compatibility with general v4l2 layer (remove soc-camera interface)
 */

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

#define MODULE_NAME "SP0718"

#ifdef SP0718_DEBUG
#define assert(expr) \
    if (unlikely(!(expr))) {				\
        pr_err("Assertion failed! %s,%s,%s,line=%d\n",	\
#expr, __FILE__, __func__, __LINE__);	\
    }

#define SP0718_DEBUG(fmt,args...) printk(KERN_ALERT fmt, ##args)
#else

#define assert(expr) do {} while (0)

#define SP0718_DEBUG(fmt,args...)
#endif
// I2c ADDR 0X42
#define PID                 0x02 /* Product ID Number  *///caichsh
#define SENSOR_ID           0x27
#define NUM_CTRLS           6
#define V4L2_IDENT_SP0718   64113

/* private ctrls */
#define V4L2_CID_SCENE_EXPOSURE         (V4L2_CTRL_CLASS_CAMERA | 0x1001)
#define V4L2_CID_PRIVATE_PREV_CAPT      (V4L2_CTRL_CLASS_CAMERA | 0x1002)

enum {
    WB_AUTO = 0 ,
    WB_INCANDESCENT,
    WB_FLUORESCENT,
    WB_DAYLIGHT,
    WB_CLOUDY,
    WB_TUNGSTEN,
    WB_MAX
};
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

/********************************************************************************
 * reg define
 */
#define SP0718 0x27 //pga

#define OUTTO_SENSO_CLOCK 24000000


/********************************************************************************
 * predefine reg values
 */
struct regval_list {
    unsigned char reg_num;
    unsigned char value;
};

#define ENDMARKER { 0xff, 0xff }

/*
 * init setting
 */
#define  Pre_Value_P0_0x30  0x00
//Filter en&dis
#define  Pre_Value_P0_0x56  0x73 //xg test
#define  Pre_Value_P0_0x57  0x10  //filter outdoor
#define  Pre_Value_P0_0x58  0x10  //filter indoor
#define  Pre_Value_P0_0x59  0x10  //filter night
#define  Pre_Value_P0_0x5a  0x06  //smooth outdoor
#define  Pre_Value_P0_0x5b  0x0a //smooth indoor  //0x02 xg 20121013
#define  Pre_Value_P0_0x5c  0x2a  //smooht night	//0x20 xg 20121013
//outdoor sharpness
#define  Pre_Value_P0_0x65  0x04  //0x03 xg 20121013
#define  Pre_Value_P0_0x66  0x01
#define  Pre_Value_P0_0x67  0x03
#define  Pre_Value_P0_0x68  0x43//0x46 xg 20121013
//indoor sharpness
#define  Pre_Value_P0_0x6b  0x06//0x04 xg 20121013
#define  Pre_Value_P0_0x6c  0x01
#define  Pre_Value_P0_0x6d  0x03
#define  Pre_Value_P0_0x6e  0x43//0x46 xg 20121013
//night sharpness
#define  Pre_Value_P0_0x71  0x07//0x05 xg 20121013
#define  Pre_Value_P0_0x72  0x01
#define  Pre_Value_P0_0x73  0x03
#define  Pre_Value_P0_0x74  0x43//0x46 xg 20121013
//color
#define  Pre_Value_P0_0x7f  0xd7  //R  0xd7
#define  Pre_Value_P0_0x87  0xf7  //B   0xf8
//satutation
#define  Pre_Value_P0_0xd8  0x68
#define  Pre_Value_P0_0xd9  0x58//0x48 xg 20121013 0x68
#define  Pre_Value_P0_0xda  0x58//0x48 xg 20121013 0x68
#define  Pre_Value_P0_0xdb  0x58//0x48 xg 20121013
//AE target
#define  Pre_Value_P0_0xf7  0x78
#define  Pre_Value_P0_0xf8  0x63
#define  Pre_Value_P0_0xf9  0x68
#define  Pre_Value_P0_0xfa  0x53
//HEQ
#define  Pre_Value_P0_0xdd  0x70
#define  Pre_Value_P0_0xde  0x98//0x98 xg 20121013
//AWB pre gain
#define  Pre_Value_P1_0x28  0x75
#define  Pre_Value_P1_0x29  0x4a//0x4e

//VBLANK
#define  Pre_Value_P0_0x05  0x00
#define  Pre_Value_P0_0x06  0x00
//HBLANK
#define  Pre_Value_P0_0x09  0x01
#define  Pre_Value_P0_0x0a  0x76

static struct regval_list sp0718_init_regs[] =
{
    {0xfd,0x00},
	{0x1C,0x28},
	{0x1F,0x20},
	{0x31,0x70},
	{0x27,0xb3},//0xb3	//2x gain
	{0x1b,0x17},
	{0x26,0xaa},
	{0x37,0x02},
	{0x28,0x8f},
	{0x1a,0x73},
	{0x1e,0x1b},
	{0x21,0x09},  //blackout voltage
	{0x22,0x2a},  //colbias
	{0x0f,0x3f},
	{0x10,0x3e},
	{0x11,0x00},
	{0x12,0x01},
	{0x13,0x3f},
	{0x14,0x04},
	{0x15,0x30},
	{0x16,0x31},
	{0x17,0x01},
	{0x69,0x31},
	{0x6a,0x2a},
	{0x6b,0x33},
	{0x6c,0x1a},
	{0x6d,0x32},
	{0x6e,0x28},
	{0x6f,0x29},
	{0x70,0x34},
	{0x71,0x18},
	{0x36,0x00},//02 delete badframe
	{0xfd,0x01},
	{0x5d,0x51},//position
	{0xf2,0x19},
	
	//Blacklevel
	{0x1f,0x10},
	{0x20,0x1f},
	//pregain 
	//{0xfd,0x02},
	//{0x00,0x88},
	//{0x01,0x88},
	
	//SI15_SP0718 24M 50Hz 12-8fps 
	//ae setting
{0xfd,0x00},
{0x03,0x01},
{0x04,0x6e},
{0x06,0x64},
{0x09,0x03},
{0x0a,0x15},
{0xfd,0x01},
{0xef,0x3d},
{0xf0,0x00},
{0x02,0x0f},
{0x03,0x01},
{0x06,0x37},
{0x07,0x00},
{0x08,0x01},
{0x09,0x00},
//Status                 
{0xfd,0x02},
{0xbe,0x93},
{0xbf,0x03},
{0xd0,0x93},
{0xd1,0x03},
{0xfd,0x01},
{0x5b,0x03},
{0x5c,0x93},

	//rpc
	{0xfd,0x01},
	{0xe0,0x40},//38//rpc_1base_max
	{0xe1,0x30},//2//rpc_2base_max
	{0xe2,0x2e},//26//rpc_3base_max
	{0xe3,0x2a},//22//rpc_4base_max
	{0xe4,0x2a},//22//rpc_5base_max
	{0xe5,0x28},//rpc_6base_max
	{0xe6,0x28},//rpc_7base_max
	{0xe7,0x26},//1//e//rpc_8base_max
	{0xe8,0x26},//1//e//rpc_9base_max
	{0xe9,0x26},//1//rpc_10base_max
	{0xea,0x26},//1//rpc_11base_max
	{0xf3,0x26},//1//rpc_12base_max
	{0xf4,0x26},//1//rpc_13base_max
	//ae gain &status
	{0xfd,0x01},
	{0x04,0xe0},//rpc_max_indr
	{0x05,0x26},//1//rpc_min_indr 
	{0x0a,0xa0},//rpc_max_outdr
	{0x0b,0x26},//rpc_min_outdr
	{0x5a,0x60},
	{0xfd,0x02}, 
	{0xbc,0xa0},//rpc_heq_low
	{0xbd,0x80},//rpc_heq_dummy
	{0xb8,0x80},//mean_normal_dummy
	{0xb9,0x90},//mean_dummy_normal
	
	//ae target
	{0xfd,0x01}, 
	{0xeb,0x78},//78 
	{0xec,0x78},//78
	{0xed,0x0a},	
	{0xee,0x10},
	
	//lsc       
	{0xfd,0x01},
	{0x26,0x30},
	{0x27,0x2c},
	{0x28,0x07},
	{0x29,0x08},
	{0x2a,0x00},
	{0x2b,0x03},
	{0x2c,0x00},
	{0x2d,0x00},
	
	{0xa1,0x2a},
	{0xa2,0x26},
	{0xa3,0x2d},
	{0xa4,0x24},
	{0xad,0x0d},
	{0xae,0x08},
	{0xaf,0x0a},
	{0xb0,0x03},
	//GGain 
	{0xa5,0x25},
	{0xa6,0x1d},
	{0xa7,0x25},
	{0xa8,0x20},
	{0xb1,0x02},
	{0xb2,0x00},
	{0xb3,0x00},
	{0xb4,0x00},
	//BGain
	{0xa9,0x1c},
	{0xaa,0x1a},
	{0xab,0x1f},
	{0xac,0x1c},
	{0xb5,0x00},
	{0xb6,0x00},
	{0xb7,0x00},
	{0xb8,0x00},
	 
	//DP       
	{0xfd,0x01},
	{0x48,0x00},
	{0x49,0x09},
	  
	//awb       
	{0xfd,0x01},
	{0x32,0x05},
	{0xfd,0x00},
	{0xe7,0x03},
	{0xfd,0x02},
	{0x26,0xc8},
	{0x27,0x8B},
	{0xfd,0x00},
	{0xe7,0x00},
	{0xfd,0x02},
	{0x1b,0x80},
	{0x1a,0x80},
	{0x18,0x27},
	{0x19,0x26},
	{0xfd,0x02},
	{0x2a,0x01},
	{0x2b,0x10},
	{0x28,0xef},
	{0x29,0x08},
	
	//d65 90  e2 93
	{0x66,0x40},
	{0x67,0x60},
	{0x68,0xD9},
	{0x69,0xFE},
	{0x6a,0xa5},
	//indoor 91
	{0x7c,0x34},
	{0x7d,0x56},
	{0x7e,0x0F},
	{0x7f,0x34},
	{0x80,0xaA},
	//cwf   92 
	{0x70,0x2B},
	{0x71,0x4F},
	{0x72,0x35},
	{0x73,0x5A},
	{0x74,0xaa},
	//tl84  93 
	{0x6b,0x08},
	{0x6c,0x2D},
	{0x6d,0x3F},
	{0x6e,0x63},
	{0x6f,0xAA},
	//f    94
	{0x61,0xDB},
	{0x62,0xFC},
	{0x63,0x6A},
	{0x64,0x8E},
	{0x65,0x5a},
	
	{0x75,0x80},
	{0x76,0x09},
	{0x77,0x02},
	{0x24,0x25},
	
	//?????,
	//0x20,0xd8},
	//0x21,0xb0},
	//0x22,0xb8},
	//0x23,0x9d},
	
	//outdoor r},
	// 0x78,0xc},
	// 0x79,0xa},
	// 0x7a,0xa},
	// 0x7b,0x8},
	//skin
	{0x0e,0x30},
	{0x09,0x07},
	//gw     
	{0x31,0x60},
	{0x32,0x60},
	{0x33,0xc0},
	{0x35,0x6f},
	{0x3b,0x09},
	
	//sharp   
	{0xfd,0x02},
	{0xde,0x0f},
	{0xd2,0x06},
	{0xd3,0x06},
	{0xd4,0x06},
	{0xd5,0x06},
	{0xd7,0x20},
	{0xd8,0x30},
	{0xd9,0x38},
	{0xda,0x38},
	{0xdb,0x08},
	{0xe8,0x48},
	{0xe9,0x48},
	{0xea,0x30},
	{0xeb,0x20},
	{0xec,0x80},
	{0xed,0x60},
	{0xee,0x40},
	{0xef,0x20},
	//?????,
	{0xf3,0x50},
	{0xf4,0x10},
	{0xf5,0x10},
	{0xf6,0x10},
	//dns       
	{0xfd,0x01},
	{0x64,0x44},
	{0x65,0x22},
	{0x6d,0x08},
	{0x6e,0x08},
	{0x6f,0x10},
	{0x70,0x10},
	{0x71,0x0d},
	{0x72,0x1b},
	{0x73,0x20},
	{0x74,0x24},
	{0x75,0x44},
	{0x76,0x02},
	{0x77,0x02},
	{0x78,0x02},
	{0x81,0x18},
	{0x82,0x30},
	{0x83,0x40},
	{0x84,0x50},
	{0x85,0x0c},
	{0xfd,0x02},
	{0xdc,0x0f},
	 
	//gamma    
	{0xfd,0x01},
	{0x8b,0x00},
	{0x8c,0x0e},
	{0x8d,0x1b},
	{0x8e,0x28},
	{0x8f,0x36},
	{0x90,0x4d},
	{0x91,0x61},
	{0x92,0x6f},
	{0x93,0x7e},
	{0x94,0x93},
	{0x95,0xa4},
	{0x96,0xb3},
	{0x97,0xbf},
	{0x98,0xca},
	{0x99,0xd2},
	{0x9a,0xda},
	{0x9b,0xe1},
	{0x9c,0xe7},
	{0x9d,0xec},
	{0x9e,0xf3},
	{0x9f,0xf9},
	{0xa0,0xff},
	//CCM      
	{0xfd,0x02},
	{0x15,0xE0},
	{0x16,0x95},
	
	//!F        
	{0xa0,0x80},
	{0xa1,0x00},
	{0xa2,0x00},
	{0xa3,0xfa},
	{0xa4,0x80},
	{0xa5,0x06},
	{0xa6,0x00},
	{0xa7,0xe7},
	{0xa8,0x99},
	{0xa9,0x00},
	{0xaa,0x03},
	{0xab,0x0c},
	//F        
	{0xac,0xa6},
	{0xad,0xe7},
	{0xae,0xf4},
	{0xaf,0xe7},
	{0xb0,0x99},
	{0xb1,0x00},
	{0xb2,0xe7},
	{0xb3,0xe7},
	{0xb4,0xb3},
	{0xb5,0x3c},
	{0xb6,0x03},
	{0xb7,0x0f},
	  
	//sat u     
	{0xfd,0x01},
	{0xd3,0x84},//0x88
	{0xd4,0x80},//0x88
	{0xd5,0x78},//0x88
	{0xd6,0x68},//0x78
	//sat v   
	{0xd7,0x84},//0x88
	{0xd8,0x80},//0x88
	{0xd9,0x78},//0x88
	{0xda,0x68},//0x78
	//auto_sat  
	{0xdd,0x30},
	{0xde,0x10},
	{0xd2,0x01},//autosa_en
	{0xdf,0xff},//a0//y_mean_th
	  
	//uv_th     
	{0xfd,0x01},
	{0xc2,0xaa},
	{0xc3,0xaa},
	{0xc4,0x66},
	{0xc5,0x66}, 
	
	//heq
	{0xfd,0x01},
	{0x0f,0xff},
	{0x10,0x00},
	{0x14,0x10},
	{0x11,0x00},
	{0x15,0x08},
	{0x16,0x0a},
	{0xd0,0x55},  
	//auto 
	{0xfd,0x01},
	{0xfb,0x33},
	{0x32,0x15},
	{0x33,0xff},
	{0x34,0xe7},
	
	{0x35,0x40},	  
    


    ENDMARKER,
};

static struct regval_list sp0830_enable_regs[] = {
    ENDMARKER,
};

static struct regval_list sp0830_disable_regs[] = {
    ENDMARKER,
};

/*
 * color code
 */
static struct regval_list sp0718_fmt_yuv422_yuyv[] =
{
    ENDMARKER,
};

static struct regval_list sp0718_fmt_yuv422_yvyu[] =
{
    ENDMARKER,
};

static struct regval_list sp0718_fmt_yuv422_vyuy[] =
{
    ENDMARKER,
};

static struct regval_list sp0718_fmt_yuv422_uyvy[] =
{
    ENDMARKER,
};

/* static struct regval_list sp0718_fmt_raw[] = */
/* { */
/*     {0xfd,0x00}, */
/*     ENDMARKER, */
/* }; */

/*
 *AWB
 */
static const struct regval_list sp0718_awb_regs_enable[] =
{


    ENDMARKER,
};
static const struct regval_list sp0718_awb_regs_diable[] =
{

    ENDMARKER,
};

static struct regval_list sp0718_wb_auto_regs[] =
{   
	{0xfd,0x01},
	{0x32,0x15},
	{0xfd,0x02},
	{0x26,0xc8},
	{0x27,0xb6},
	{0xfd,0x00},
    ENDMARKER,
};

static struct regval_list sp0718_wb_cloud_regs[] =
{  
	{0xfd,0x01},
	{0x32,0x05},
	{0xfd,0x02},
	{0x26,0xdc},
	{0x27,0x75},
	{0xfd,0x00},
    ENDMARKER,
};

static struct regval_list sp0718_wb_daylight_regs[] =
{
	{0xfd,0x01},
	{0x32,0x05},
	{0xfd,0x02},
	{0x26,0xc8},
	{0x27,0x89},
	{0xfd,0x00},

    ENDMARKER,
};

static struct regval_list sp0718_wb_incandescence_regs[] =
{
	{0xfd,0x01},
	{0x32,0x05},
	{0xfd,0x02},
	{0x26,0xaa},
	{0x27,0xce},
	{0xfd,0x00},
    ENDMARKER,
};

static struct regval_list sp0718_wb_fluorescent_regs[] =
{
	{0xfd,0x01},
	{0x32,0x05},
	{0xfd,0x02},
	{0x26,0x91},
	{0x27,0xc8},
	{0xfd,0x00},
    ENDMARKER,
};

static struct regval_list sp0718_wb_tungsten_regs[] =
{
	{0xfd,0x01},
	{0x32,0x05},
	{0xfd,0x02},
	{0x26,0x75},
	{0x27,0xe2},
	{0xfd,0x00},
    ENDMARKER,
};

/*
 * colorfx
 */
static struct regval_list sp0718_colorfx_none_regs[] =
{
	{0xfd, 0x01},
	{0x66, 0x00},
	{0x67, 0x80},
	{0x68, 0x80},
	{0xfd, 0x00},
    ENDMARKER,
};

static struct regval_list sp0718_colorfx_bw_regs[] =
{
	{0xfd, 0x01},
	{0x66, 0x20},
	{0x67, 0x80},
	{0x68, 0x80},
	{0xfd, 0x00},
    ENDMARKER,
};

static struct regval_list sp0718_colorfx_sepia_regs[] =
{
	{0xfd, 0x01},
	{0x66, 0x10},
	{0x67, 0xc0},
	{0x68, 0x20},
	{0xfd, 0x00},
    ENDMARKER,
};

static struct regval_list sp0718_colorfx_negative_regs[] =
{
	{0xfd, 0x01},
	{0x66, 0x08},
	{0x67, 0x80},
	{0x68, 0x80},
	{0xfd, 0x00},
    ENDMARKER,
};

/*
 * window size
 */
static const struct regval_list sp0718_vga_regs[] = {


    ENDMARKER,
};

static const struct regval_list sp0718_qvga_regs[] = {


    ENDMARKER,
};

/********************************************************************************
 * structures
 */
struct sp0718_win_size {
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
    unsigned int dummy_line;
    unsigned int dummy_pixel;
    unsigned int extra_line;
} exposure_param_t;

enum prev_capt {
    PREVIEW_MODE = 0,
    CAPTURE_MODE
};

struct sp0718_priv {
    struct v4l2_subdev                  subdev;
    struct media_pad                    pad;
    struct v4l2_ctrl_handler            hdl;
    const struct sp0718_color_format    *cfmt;
    const struct sp0718_win_size        *win;
    int                                 model;
    bool                                initialized;

    /**
     * ctrls
     */
    /* standard */
    struct v4l2_ctrl *auto_white_balance;
    struct v4l2_ctrl *exposure;
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

struct sp0718_color_format {
    enum v4l2_mbus_pixelcode code;
    enum v4l2_colorspace colorspace;
};

/********************************************************************************
 * tables
 */
static const struct sp0718_color_format sp0718_cfmts[] = {
    {
        .code       = V4L2_MBUS_FMT_YUYV8_2X8,
        .colorspace = V4L2_COLORSPACE_JPEG,
    },
    {
        .code       = V4L2_MBUS_FMT_UYVY8_2X8,
        .colorspace = V4L2_COLORSPACE_JPEG,
    },
    {
        .code       = V4L2_MBUS_FMT_YVYU8_2X8,
        .colorspace = V4L2_COLORSPACE_JPEG,
    },
    {
        .code       = V4L2_MBUS_FMT_VYUY8_2X8,
        .colorspace = V4L2_COLORSPACE_JPEG,
    },
};

/*
 * window size list
 */
#define VGA_WIDTH           640
#define VGA_HEIGHT          480
#define SP0718_MAX_WIDTH    VGA_WIDTH
#define SP0718_MAX_HEIGHT   VGA_HEIGHT
#define AHEAD_LINE_NUM      15    //10\D0\D0 = 50\B4\CEѭ\BB\B7(sp0718)
#define DROP_NUM_CAPTURE    3
#define DROP_NUM_PREVIEW    16


static unsigned int frame_rate_vga[] = {30,};

/* 640x480 */
static const struct sp0718_win_size sp0718_win_vga = {
    .name     = "VGA",
    .width    = VGA_WIDTH,
    .height   = VGA_HEIGHT,
    .win_regs = sp0718_vga_regs,
    .frame_rate_array = frame_rate_vga,
};

static const struct sp0718_win_size sp0718_win_qvga = {
    .name     = "QVGA",
    .width    = 320,
    .height   = 240,
    .win_regs = sp0718_qvga_regs,
    .frame_rate_array = frame_rate_vga,
};

static const struct sp0718_win_size *sp0718_win[] = {
    &sp0718_win_vga,
    &sp0718_win_qvga,
};

/********************************************************************************
 * general functions
 */
static inline struct sp0718_priv *to_priv(struct v4l2_subdev *subdev)
{
    return container_of(subdev, struct sp0718_priv, subdev);
}

static inline struct v4l2_subdev *ctrl_to_sd(struct v4l2_ctrl *ctrl)
{
    return &container_of(ctrl->handler, struct sp0718_priv, hdl)->subdev;
}

static bool check_id(struct i2c_client *client)
{
    u8 pid = i2c_smbus_read_byte_data(client, PID);
    if (pid == 0x71)// SENSOR_ID)
        return true;

    printk(KERN_ERR "failed to check id: 0x%x\n", pid);
    return false;
}

static int sp0718_write_array(struct i2c_client *client, const struct regval_list *vals)
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

static int sp0718_mask_set(struct i2c_client *client, u8 command, u8 mask, u8 set) __attribute__((unused));
static int sp0718_mask_set(struct i2c_client *client, u8 command, u8 mask, u8 set)
{
    s32 val = i2c_smbus_read_byte_data(client, command);
    if (val < 0)
        return val;

    val &= ~mask;
    val |= set & mask;

    return i2c_smbus_write_byte_data(client, command, val);
}

static const struct sp0718_win_size *sp0718_select_win(u32 width, u32 height)
{
    const struct sp0718_win_size *win;
    int i;

    for (i = 0; i < ARRAY_SIZE(sp0718_win); i++) {
        win = sp0718_win[i];
        if (width == win->width && height == win->height)
            return win;
    }

    printk(KERN_ERR "%s: unsupported width, height (%dx%d)\n", __func__, width, height);
    return NULL;
}

static int sp0718_set_mbusformat(struct i2c_client *client, const struct sp0718_color_format *cfmt) __attribute__((unused));
static int sp0718_set_mbusformat(struct i2c_client *client, const struct sp0718_color_format *cfmt)
{
    enum v4l2_mbus_pixelcode code;
    int ret = -1;
    code = cfmt->code;
    printk("%s: code 0x%x\n", __func__, code);
    switch (code) {
        case V4L2_MBUS_FMT_YUYV8_2X8:
            ret  = sp0718_write_array(client, sp0718_fmt_yuv422_yuyv);
            break;
        case V4L2_MBUS_FMT_UYVY8_2X8:
            ret  = sp0718_write_array(client, sp0718_fmt_yuv422_uyvy);
            break;
        case V4L2_MBUS_FMT_YVYU8_2X8:
            ret  = sp0718_write_array(client, sp0718_fmt_yuv422_yvyu);
            break;
        case V4L2_MBUS_FMT_VYUY8_2X8:
            ret  = sp0718_write_array(client, sp0718_fmt_yuv422_vyuy);
            break;
        default:
            printk(KERN_ERR "mbus code error in %s() line %d\n",__FUNCTION__, __LINE__);
    }
    return ret;
}

static int sp0718_set_params(struct v4l2_subdev *sd, u32 *width, u32 *height, enum v4l2_mbus_pixelcode code)
{
    struct sp0718_priv *priv = to_priv(sd);
    const struct sp0718_win_size *old_win, *new_win;
    int i;

    priv->cfmt = NULL;
    for (i = 0; i < ARRAY_SIZE(sp0718_cfmts); i++) {
        if (code == sp0718_cfmts[i].code) {
            priv->cfmt = sp0718_cfmts + i;
            break;
        }
    }
    if (!priv->cfmt) {
        printk(KERN_ERR "Unsupported sensor format.\n");
        return -EINVAL;
    }

    old_win = priv->win;
    new_win = sp0718_select_win(*width, *height);
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

/********************************************************************************
 * control functions
 */
static int sp0718_set_auto_white_balance(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl)
{
    struct i2c_client *client = v4l2_get_subdevdata(sd);
    int auto_white_balance = ctrl->val;
    int ret;

    printk("%s: val %d\n", __func__, auto_white_balance);
    if (auto_white_balance < 0 || auto_white_balance > 1) {
        dev_err(&client->dev, "set auto_white_balance over range, auto_white_balance = %d\n", auto_white_balance);
        return -ERANGE;
    }

    switch(auto_white_balance) {
        case 0:
            ret = sp0718_write_array(client, sp0718_awb_regs_diable);
            break;
        case 1:
            ret = sp0718_write_array(client, sp0718_awb_regs_enable);
            break;
    }

    assert(ret == 0);

    return 0;
}

static int sp0718_set_white_balance_temperature(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl)
{
    struct i2c_client *client = v4l2_get_subdevdata(sd);
    int white_balance_temperature = ctrl->val;
    int ret;

    printk("%s: val %d\n", __func__, ctrl->val);

    switch(white_balance_temperature) {
        case WB_INCANDESCENT:
            ret = sp0718_write_array(client, sp0718_wb_incandescence_regs);
            break;
        case WB_FLUORESCENT:
            ret = sp0718_write_array(client,sp0718_wb_fluorescent_regs);
            break;
        case WB_DAYLIGHT:
            ret = sp0718_write_array(client,sp0718_wb_daylight_regs);
            break;
        case WB_CLOUDY:
            ret = sp0718_write_array(client,sp0718_wb_cloud_regs);
            break;
        case WB_TUNGSTEN:
            ret = sp0718_write_array(client,sp0718_wb_tungsten_regs);
            break;
        case WB_AUTO:
            ret = sp0718_write_array(client, sp0718_wb_auto_regs);
            break;
        default:
            dev_err(&client->dev, "set white_balance_temperature over range, white_balance_temperature = %d\n", white_balance_temperature);
            return -ERANGE;
    }
    assert(ret == 0);

    return 0;
}

static int sp0718_set_colorfx(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl)
{
    struct i2c_client *client = v4l2_get_subdevdata(sd);
    int colorfx = ctrl->val;
    int ret;

    printk("%s: val %d\n", __func__, ctrl->val);

    switch (colorfx) {
        case V4L2_COLORFX_NONE: /* normal */
            ret = sp0718_write_array(client, sp0718_colorfx_none_regs);
            break;
        case V4L2_COLORFX_BW: /* black and white */
            ret = sp0718_write_array(client, sp0718_colorfx_bw_regs);
            break;
        case V4L2_COLORFX_SEPIA: /* antique ,\B8\B4\B9\C5*/
            ret = sp0718_write_array(client, sp0718_colorfx_sepia_regs);

            break;
        case V4L2_COLORFX_NEGATIVE: /* negative\A3\AC\B8\BAƬ */
            ret = sp0718_write_array(client, sp0718_colorfx_negative_regs);
            break;
        default:
            dev_err(&client->dev, "set colorfx over range, colorfx = %d\n", colorfx);
            return -ERANGE;
    }


    assert(ret == 0);

    return 0;
}

/* TODO */
static int sp0718_set_exposure_auto(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl)
{
    struct i2c_client *client = v4l2_get_subdevdata(sd);
    int exposure_auto = ctrl->val;

    if (exposure_auto < 0 || exposure_auto > 1) {
        dev_err(&client->dev, "set exposure_auto over range, exposure_auto = %d\n", exposure_auto);
        return -ERANGE;
    }

    return 0;
}

/* TODO */
static int sp0718_set_scene_exposure(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl)
{
    /* struct i2c_client *client = v4l2_get_subdevdata(sd); */
    /* int scene_exposure = ctrl->val; */

    return 0;
}

/* TODO */
static int sp0718_set_prev_capt_mode(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl)
{
    /* struct i2c_client *client = v4l2_get_subdevdata(sd); */
    /* int mode = ctrl->val; */
    /* sp0718_priv *priv = to_priv(sd); */

    /* switch(mode) { */
    /*     case PREVIEW_MODE: */
    /*         priv->prev_capt_mode = mode; */
    /*         break; */
    /*     case CAPTURE_MODE: */
    /*         priv->prev_capt_mode = mode; */
    /*         break; */
    /*     default: */
    /*         dev_err(&client->dev, "set_prev_capt_mode over range, prev_capt_mode = %d\n", mode); */
    /*         return -ERANGE; */
    /* } */

    return 0;
}


static int sp0718_set_brightness(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl)
{
    struct i2c_client *client = v4l2_get_subdevdata(sd);
    unsigned int reg_0xdb;
    int ret;

    int val = ctrl->val;

    printk("%s: val %d\n", __func__, val);

    if (val < 0 || val > 6) {
        dev_err(&client->dev, "set brightness over range, brightness = %d\n", val);
        return -ERANGE;
    }

    switch(val) {
        case 0:
            reg_0xdb = 0xd0;
            break;
        case 1:
			reg_0xdb = 0xe0;
            break;
        case 2:
            reg_0xdb = 0xf0;
            break;
        case 3:
            reg_0xdb = 0x00;
    
            break;
        case 4:
            reg_0xdb = 0x10;
        
            break;
        case 5:
            reg_0xdb = 0x20;
     
            break;
        case 6:
            reg_0xdb = 0x30;
        
            break;
    }

   ret = i2c_smbus_write_byte_data(client, 0xfd, 0x01);
   ret |= i2c_smbus_write_byte_data(client,0xdb, reg_0xdb);
   assert(ret == 0);

    return 0;
}

static int sp0718_s_ctrl(struct v4l2_ctrl *ctrl)
{
    struct v4l2_subdev *sd = ctrl_to_sd(ctrl);
    struct i2c_client *client = v4l2_get_subdevdata(sd);
    int ret = 0;

    printk("%s: val is %d \n",__func__,ctrl->id);

    switch (ctrl->id) {
        case V4L2_CID_BRIGHTNESS:
            sp0718_set_brightness(sd, ctrl);
            break;
        case V4L2_CID_AUTO_WHITE_BALANCE:
            sp0718_set_auto_white_balance(sd, ctrl);
            break;

        case V4L2_CID_WHITE_BALANCE_TEMPERATURE:
            sp0718_set_white_balance_temperature(sd, ctrl);
            break;

        case V4L2_CID_COLORFX:
            sp0718_set_colorfx(sd, ctrl);
            break;

        case V4L2_CID_EXPOSURE_AUTO:
            sp0718_set_exposure_auto(sd, ctrl);
            break;

        case V4L2_CID_SCENE_EXPOSURE:
            sp0718_set_scene_exposure(sd, ctrl);
            break;

        case V4L2_CID_PRIVATE_PREV_CAPT:
            sp0718_set_prev_capt_mode(sd, ctrl);
            break;

        default:
            dev_err(&client->dev, "%s: invalid control id %d\n", __func__, ctrl->id);
            return -EINVAL;
    }

    return ret;
}

static const struct v4l2_ctrl_ops sp0718_ctrl_ops = {
    .s_ctrl = sp0718_s_ctrl,
};

static const struct v4l2_ctrl_config sp0718_custom_ctrls[] = {
    {
        .ops    = &sp0718_ctrl_ops,
        .id     = V4L2_CID_SCENE_EXPOSURE,
        .type   = V4L2_CTRL_TYPE_INTEGER,
        .name   = "SceneExposure",
        .min    = 0,
        .max    = 1,
        .def    = 0,
        .step   = 1,
    }, {
        .ops    = &sp0718_ctrl_ops,
        .id     = V4L2_CID_PRIVATE_PREV_CAPT,
        .type   = V4L2_CTRL_TYPE_INTEGER,
        .name   = "PrevCapt",
        .min    = 0,
        .max    = 1,
        .def    = 0,
        .step   = 1,
    }
};

static int sp0718_initialize_ctrls(struct sp0718_priv *priv)
{
    v4l2_ctrl_handler_init(&priv->hdl, NUM_CTRLS);

    /* standard ctrls */
    priv->auto_white_balance = v4l2_ctrl_new_std(&priv->hdl, &sp0718_ctrl_ops,
            V4L2_CID_AUTO_WHITE_BALANCE, 0, 1, 1, 1);
    if (!priv->auto_white_balance) {
        printk(KERN_ERR "%s: failed to create auto_white_balance ctrl\n", __func__);
        return -ENOENT;
    }

    priv->white_balance_temperature = v4l2_ctrl_new_std(&priv->hdl, &sp0718_ctrl_ops,
            V4L2_CID_WHITE_BALANCE_TEMPERATURE, 0, 3, 1, 1);
    if (!priv->white_balance_temperature) {
        printk(KERN_ERR "%s: failed to create white_balance_temperature ctrl\n", __func__);
        return -ENOENT;
    }

    /* standard menus */
    priv->colorfx = v4l2_ctrl_new_std_menu(&priv->hdl, &sp0718_ctrl_ops,
            V4L2_CID_COLORFX, 3, 0, 0);
    if (!priv->colorfx) {
        printk(KERN_ERR "%s: failed to create colorfx ctrl\n", __func__);
        return -ENOENT;
    }

    priv->exposure_auto = v4l2_ctrl_new_std_menu(&priv->hdl, &sp0718_ctrl_ops,
            V4L2_CID_EXPOSURE_AUTO, 1, 0, 1);
    if (!priv->exposure_auto) {
        printk(KERN_ERR "%s: failed to create exposure_auto ctrl\n", __func__);
        return -ENOENT;
    }

    /* custom ctrls */
    priv->scene_exposure = v4l2_ctrl_new_custom(&priv->hdl, &sp0718_custom_ctrls[0], NULL);
    if (!priv->scene_exposure) {
        printk(KERN_ERR "%s: failed to create scene_exposure ctrl\n", __func__);
        return -ENOENT;
    }

    priv->prev_capt = v4l2_ctrl_new_custom(&priv->hdl, &sp0718_custom_ctrls[1], NULL);
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

/********************************************************************************
 * v4l2 subdev ops
 */

/**
 * core ops
 */
static int sp0718_g_chip_ident(struct v4l2_subdev *sd, struct v4l2_dbg_chip_ident *id)
{
    struct sp0718_priv *priv = to_priv(sd);
    id->ident    = priv->model;
    id->revision = 0;
    return 0;
}

static int sp0718_s_power(struct v4l2_subdev *sd, int on)
{
    /* used when suspending */
    /* printk("%s: on %d\n", __func__, on); */
    if (!on) {
        struct sp0718_priv *priv = to_priv(sd);
        priv->initialized = false;
    }
    return 0;
}

static const struct v4l2_subdev_core_ops sp0718_subdev_core_ops = {
    .g_chip_ident   = sp0718_g_chip_ident,
    .s_power        = sp0718_s_power,
    .s_ctrl         = v4l2_subdev_s_ctrl,
};

/**
 * video ops
 */
static int sp0718_s_stream(struct v4l2_subdev *sd, int enable)
{
    struct i2c_client *client = v4l2_get_subdevdata(sd);
    struct sp0718_priv *priv = to_priv(sd);
    int ret = 0;

    printk( "%s: enable %d, initialized %d\n", __func__, enable, priv->initialized);

    if (enable) {
        if (!priv->win || !priv->cfmt) {
            dev_err(&client->dev, "norm or win select error\n");
            return -EPERM;
        }

        if (!priv->initialized) {
            if (!check_id(client))
                return -EINVAL;

            ret = sp0718_write_array(client, sp0718_init_regs);
			udelay(10);
            if (ret < 0) {
                printk(KERN_ERR "%s: failed to sp0718_write_array init regs\n", __func__);
                return -EIO;
            }

            priv->initialized = true;
        }

        ret = sp0718_write_array(client, priv->win->win_regs);
        if (ret < 0) {
            printk(KERN_ERR "%s: failed to sp0718_write_array win regs\n", __func__);
            return -EIO;
        }

        sp0718_write_array(client, sp0718_fmt_yuv422_yuyv);
        if (ret < 0) {
            printk(KERN_ERR "%s: failed to sp0830_write_array format regs\n", __func__);
            return -EIO;
        }

        ret = sp0718_write_array(client, sp0830_enable_regs);
        if (ret < 0) {
            printk(KERN_ERR "%s: failed to sp0830_write_array enable regs\n", __func__);
            return -EIO;
        }
    } else {
        ret = sp0718_write_array(client, sp0830_disable_regs);
        if (ret < 0) {
            printk(KERN_ERR "%s: failed to sp0830_write_array disable regs\n", __func__);
            return -EIO;
        }
    }

    return 0;
}

static const struct v4l2_subdev_video_ops sp0718_subdev_video_ops= {
    .s_stream = sp0718_s_stream,
};

/**
 * pad ops
 */
static int sp0718_s_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
        struct v4l2_subdev_format *fmt)
{
    struct sp0718_priv *priv = to_priv(sd);
    struct v4l2_mbus_framefmt *mf = &fmt->format;
    int ret = 0;

    printk("%s: %dx%d\n", __func__, mf->width, mf->height);

    ret = sp0718_set_params(sd, &mf->width, &mf->height, mf->code);
    if(!ret)
        mf->colorspace = priv->cfmt->colorspace;

    return ret;
}

static const struct v4l2_subdev_pad_ops sp0718_subdev_pad_ops = {
    .set_fmt = sp0718_s_fmt,
};

/**
 * subdev ops
 */
static const struct v4l2_subdev_ops sp0718_subdev_ops = {
    .core   = &sp0718_subdev_core_ops,
    .video  = &sp0718_subdev_video_ops,
    .pad    = &sp0718_subdev_pad_ops,
};

/**
 * media_entity_operations
 */
static int sp0718_link_setup(struct media_entity *entity,
        const struct media_pad *local,
        const struct media_pad *remote, u32 flags)
{
    printk("%s\n", __func__);
    return 0;
}

static const struct media_entity_operations sp0718_media_ops = {
    .link_setup = sp0718_link_setup,
};

/********************************************************************************
 * initialize
 */
static void sp0718_priv_init(struct sp0718_priv * priv)
{
    priv->model = V4L2_IDENT_SP0718;
    priv->prev_capt_mode = PREVIEW_MODE;
    priv->timeperframe.denominator =12;//30;
    priv->timeperframe.numerator = 1;
    priv->win = &sp0718_win_vga;
}

static int sp0718_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct sp0718_priv *priv;
    struct v4l2_subdev *sd;
    int ret;

    priv = kzalloc(sizeof(struct sp0718_priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    sp0718_priv_init(priv);

    sd = &priv->subdev;
    strcpy(sd->name, MODULE_NAME);

    /* register subdev */
    v4l2_i2c_subdev_init(sd, client, &sp0718_subdev_ops);

    sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
    priv->pad.flags = MEDIA_PAD_FL_SOURCE;
    sd->entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;
    sd->entity.ops  = &sp0718_media_ops;
    if (media_entity_init(&sd->entity, 1, &priv->pad, 0)) {
        dev_err(&client->dev, "%s: failed to media_entity_init()\n", __func__);
        kfree(priv);
        return -ENOENT;
    }

    ret = sp0718_initialize_ctrls(priv);
    if (ret < 0) {
        printk(KERN_ERR "%s: failed to initialize controls\n", __func__);
        kfree(priv);
        return ret;
    }

    return 0;
}

static int sp0718_remove(struct i2c_client *client)
{
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    v4l2_device_unregister_subdev(sd);
    v4l2_ctrl_handler_free(sd->ctrl_handler);
    media_entity_cleanup(&sd->entity);
    kfree(to_priv(sd));
    return 0;
}

static const struct i2c_device_id sp0718_id[] = {
    { MODULE_NAME, 0 },
    { }
};

MODULE_DEVICE_TABLE(i2c, sp0718_id);

static struct i2c_driver sp0718_i2c_driver = {
    .driver = {
        .name = MODULE_NAME,
    },
    .probe    = sp0718_probe,
    .remove   = sp0718_remove,
    .id_table = sp0718_id,
};

module_i2c_driver(sp0718_i2c_driver);

MODULE_DESCRIPTION("SoC Camera driver for sp0718");
MODULE_AUTHOR("pangga(panggah@artekmicro.com)");
MODULE_LICENSE("GPL v2");
