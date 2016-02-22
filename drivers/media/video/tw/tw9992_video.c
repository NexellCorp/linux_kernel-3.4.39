/*
 * drivers/media/video/tw/tw9992.c --  Video Decoder driver for the tw9992
 *
 * Copyright(C) 2009. Nexell Co., <pjsin865@nexell.co.kr>
 *
 * See file CREDITS for list of people who contributed to this project.
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

#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/completion.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/switch.h>

#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>
#include <linux/videodev2_exynos_camera.h>

#include <mach/platform.h>
#include <mach/devices.h>
#include <mach/soc.h>

#include "tw9992_video.h"
#include "tw9992_preset.h"


#define DEFAULT_SENSOR_WIDTH			720
#define DEFAULT_SENSOR_HEIGHT			480
#define DEFAULT_SENSOR_CODE				(V4L2_MBUS_FMT_YUYV8_2X8)

#define FORMAT_FLAGS_COMPRESSED			0x3
#define SENSOR_JPEG_SNAPSHOT_MEMSIZE	0x410580

#define DEFAULT_PIX_FMT					V4L2_PIX_FMT_UYVY	/* YUV422 */
#define DEFAULT_MCLK					27000000	/* 24000000 */
#define POLL_TIME_MS					10
#define CAPTURE_POLL_TIME_MS    		1000

#define THINE_I2C_RETRY_CNT				3


/* maximum time for one frame at minimum fps (15fps) in normal mode */
#define NORMAL_MODE_MAX_ONE_FRAME_DELAY_MS     67

/* maximum time for one frame at minimum fps (4fps) in night mode */
#define NIGHT_MODE_MAX_ONE_FRAME_DELAY_MS     250

/* time to move lens to target position before last af mode register write */
#define LENS_MOVE_TIME_MS       100

/* level at or below which we need to enable flash when in auto mode */
#define LOW_LIGHT_LEVEL		0x1D

/* level at or below which we need to use low light capture mode */
#define HIGH_LIGHT_LEVEL	0x80

#define FIRST_AF_SEARCH_COUNT   80
#define SECOND_AF_SEARCH_COUNT  80
#define AE_STABLE_SEARCH_COUNT  4

#define FIRST_SETTING_FOCUS_MODE_DELAY_MS	100
#define SECOND_SETTING_FOCUS_MODE_DELAY_MS	200

#ifdef CONFIG_VIDEO_TW9992_DEBUG
enum {
	TW9992_DEBUG_I2C		= 1U << 0,
	TW9992_DEBUG_I2C_BURSTS	= 1U << 1,
};
static uint32_t tw9992_debug_mask = TW9992_DEBUG_I2C_BURSTS;
module_param_named(debug_mask, tw9992_debug_mask, uint, S_IWUSR | S_IRUGO);

#define tw9992_debug(mask, x...) \
	do { \
		if (tw9992_debug_mask & mask) \
			pr_info(x);	\
	} while (0)
#else

#define tw9992_debug(mask, x...)

#endif

#define TW9992_VERSION_1_1	0x11

enum tw9992_hw_power {
	TW9992_HW_POWER_OFF,
	TW9992_HW_POWER_ON,
};

/* result values returned to HAL */
enum {
	AUTO_FOCUS_FAILED,
	AUTO_FOCUS_DONE,
	AUTO_FOCUS_CANCELLED,
};

enum af_operation_status {
	AF_NONE = 0,
	AF_START,
	AF_CANCEL,
};

enum tw9992_oprmode {
	TW9992_OPRMODE_VIDEO = 0,
	TW9992_OPRMODE_IMAGE = 1,
	TW9992_OPRMODE_MAX,
};

struct tw9992_resolution {
	u8			value;
	enum tw9992_oprmode	type;
	u16			width;
	u16			height;
};

/* M5MOLS default format (codes, sizes, preset values) */
static struct v4l2_mbus_framefmt default_fmt[TW9992_OPRMODE_MAX] = {
	[TW9992_OPRMODE_VIDEO] = {
		.width		= DEFAULT_SENSOR_WIDTH,
		.height		= DEFAULT_SENSOR_HEIGHT,
		.code		= DEFAULT_SENSOR_CODE,
		.field		= V4L2_FIELD_NONE,
		.colorspace	= V4L2_COLORSPACE_JPEG,
	},
	[TW9992_OPRMODE_IMAGE] = {
		.width		= 1920,
		.height		= 1080,
		.code		= V4L2_MBUS_FMT_JPEG_1X8,
		.field		= V4L2_FIELD_NONE,
		.colorspace	= V4L2_COLORSPACE_JPEG,
	},
};

#define SIZE_DEFAULT_FFMT	ARRAY_SIZE(default_fmt)
enum tw9992_preview_frame_size {
	TW9992_PREVIEW_VGA,			/* 640x480 */
	TW9992_PREVIEW_D1,			/* 720x480 */
	TW9992_PREVIEW_MAX,
};

enum tw9992_capture_frame_size {
	TW9992_CAPTURE_VGA = 0,		/* 640x480 */
	TW9992_CAPTURE_D1,
	TW9992_CAPTURE_MAX,
};

/* make look-up table */
static const struct tw9992_resolution tw9992_resolutions[] = {
	{TW9992_PREVIEW_D1  	, TW9992_OPRMODE_VIDEO, DEFAULT_SENSOR_WIDTH,  DEFAULT_SENSOR_HEIGHT  },

	{TW9992_CAPTURE_D1  , TW9992_OPRMODE_IMAGE, DEFAULT_SENSOR_WIDTH, DEFAULT_SENSOR_HEIGHT   },
};

struct tw9992_framesize {
	u32 index;
	u32 width;
	u32 height;
};

static const struct tw9992_framesize tw9992_preview_framesize_list[] = {
	{TW9992_PREVIEW_D1, 	DEFAULT_SENSOR_WIDTH,		DEFAULT_SENSOR_HEIGHT}
};

static const struct tw9992_framesize tw9992_capture_framesize_list[] = {
	{TW9992_CAPTURE_D1,		DEFAULT_SENSOR_WIDTH,		DEFAULT_SENSOR_HEIGHT},
};

struct tw9992_version {
	u32 major;
	u32 minor;
};

struct tw9992_date_info {
	u32 year;
	u32 month;
	u32 date;
};

enum tw9992_runmode {
	TW9992_RUNMODE_NOTREADY,
	TW9992_RUNMODE_IDLE,
	TW9992_RUNMODE_RUNNING,
	TW9992_RUNMODE_CAPTURE,
};

struct tw9992_firmware {
	u32 addr;
	u32 size;
};

struct tw9992_jpeg_param {
	u32 enable;
	u32 quality;
	u32 main_size;		/* Main JPEG file size */
	u32 thumb_size;		/* Thumbnail file size */
	u32 main_offset;
	u32 thumb_offset;
	u32 postview_offset;
};

struct tw9992_position {
	int x;
	int y;
};

struct gps_info_common {
	u32 direction;
	u32 dgree;
	u32 minute;
	u32 second;
};

struct tw9992_gps_info {
	unsigned char gps_buf[8];
	unsigned char altitude_buf[4];
	int gps_timeStamp;
};

struct tw9992_regset {
	u32 size;
	u8 *data;
};

struct tw9992_regset_table {
	const u32	*reg;
	int		array_size;
};

#define TW9992_REGSET(x, y)		\
	[(x)] = {					\
		.reg		= (y),			\
		.array_size	= ARRAY_SIZE((y)),	\
}

#define TW9992_REGSET_TABLE(y)		\
	{					\
		.reg		= (y),			\
		.array_size	= ARRAY_SIZE((y)),	\
}

struct tw9992_regs {
	struct tw9992_regset_table ev[EV_MAX];
	struct tw9992_regset_table metering[METERING_MAX];
	struct tw9992_regset_table iso[ISO_MAX];
	struct tw9992_regset_table effect[V4L2_IMAGE_EFFECT_MAX];
	struct tw9992_regset_table white_balance[V4L2_WHITE_BALANCE_MAX];
	struct tw9992_regset_table preview_size[TW9992_PREVIEW_MAX];
	struct tw9992_regset_table capture_size[TW9992_CAPTURE_MAX];
	struct tw9992_regset_table scene_mode[V4L2_SCENE_MODE_MAX];
	struct tw9992_regset_table saturation[V4L2_SATURATION_MAX];
	struct tw9992_regset_table contrast[V4L2_CONTRAST_MAX];
	struct tw9992_regset_table sharpness[V4L2_SHARPNESS_MAX];
	struct tw9992_regset_table fps[FRAME_RATE_MAX];
	struct tw9992_regset_table preview_return;
	struct tw9992_regset_table jpeg_quality_high;
	struct tw9992_regset_table jpeg_quality_normal;
	struct tw9992_regset_table jpeg_quality_low;
	struct tw9992_regset_table flash_start;
	struct tw9992_regset_table flash_end;
	struct tw9992_regset_table af_assist_flash_start;
	struct tw9992_regset_table af_assist_flash_end;
	struct tw9992_regset_table af_low_light_mode_on;
	struct tw9992_regset_table af_low_light_mode_off;
	struct tw9992_regset_table aeawb_lockunlock[V4L2_AE_AWB_MAX];
	//struct tw9992_regset_table ae_awb_lock_on;
	//struct tw9992_regset_table ae_awb_lock_off;
	struct tw9992_regset_table low_cap_on;
	struct tw9992_regset_table low_cap_off;
	struct tw9992_regset_table wdr_on;
	struct tw9992_regset_table wdr_off;
	struct tw9992_regset_table face_detection_on;
	struct tw9992_regset_table face_detection_off;
	struct tw9992_regset_table capture_start;
	struct tw9992_regset_table af_macro_mode_1;
	struct tw9992_regset_table af_macro_mode_2;
	struct tw9992_regset_table af_macro_mode_3;
	struct tw9992_regset_table af_normal_mode_1;
	struct tw9992_regset_table af_normal_mode_2;
	struct tw9992_regset_table af_normal_mode_3;
	struct tw9992_regset_table af_return_macro_position;
	struct tw9992_regset_table single_af_start;
	struct tw9992_regset_table single_af_off_1;
	struct tw9992_regset_table single_af_off_2;
	struct tw9992_regset_table continuous_af_on;
	struct tw9992_regset_table continuous_af_off;
	struct tw9992_regset_table dtp_start;
	struct tw9992_regset_table dtp_stop;
	struct tw9992_regset_table init_reg_1;
	struct tw9992_regset_table init_reg_2;
	struct tw9992_regset_table init_reg_3;
	struct tw9992_regset_table init_reg_4;
	struct tw9992_regset_table flash_init;
	struct tw9992_regset_table reset_crop;
	struct tw9992_regset_table get_ae_stable_status;
	struct tw9992_regset_table get_light_level;
	struct tw9992_regset_table get_1st_af_search_status;
	struct tw9992_regset_table get_2nd_af_search_status;
	struct tw9992_regset_table get_capture_status;
	struct tw9992_regset_table get_esd_status;
	struct tw9992_regset_table get_iso;
	struct tw9992_regset_table get_shutterspeed;
	struct tw9992_regset_table get_frame_count;
};


struct tw9992_state {
	struct tw9992_platform_data 	*pdata;
	struct media_pad	 	pad; /* for media deivce pad */
	struct v4l2_subdev 		sd;
    struct switch_dev       switch_dev;

	struct exynos_md		*mdev; /* for media deivce entity */
	struct v4l2_pix_format		pix;
	struct v4l2_mbus_framefmt	ffmt[2]; /* for media deivce fmt */
	struct v4l2_fract		timeperframe;
	struct tw9992_jpeg_param	jpeg;
	struct tw9992_version		fw;
	struct tw9992_version		prm;
	struct tw9992_date_info	dateinfo;
	struct tw9992_position	position;
	struct v4l2_streamparm		strm;
	struct v4l2_streamparm		stored_parm;
	struct tw9992_gps_info	gps_info;
	struct mutex			ctrl_lock;
	struct completion		af_complete;
	enum tw9992_runmode		runmode;
	enum tw9992_oprmode		oprmode;
	enum af_operation_status	af_status;
	enum v4l2_mbus_pixelcode	code; /* for media deivce code */
	int 				res_type;
	u8 				resolution;
	int				preview_framesize_index;
	int				capture_framesize_index;
	int				sensor_version;
	int				freq;		/* MCLK in Hz */
	int				check_dataline;
	int				check_previewdata;
	bool 				flash_on;
	bool 				torch_on;
	bool 				power_on;
	bool 				sensor_af_in_low_light_mode;
	bool 				flash_state_on_previous_capture;
	bool 				initialized;
	bool 				restore_preview_size_needed;
	int 				one_frame_delay_ms;
	const struct 			tw9992_regs *regs;

    struct i2c_client *i2c_client;
    struct v4l2_ctrl_handler handler;
    struct v4l2_ctrl *ctrl_mux;
    struct v4l2_ctrl *ctrl_status;

    /* standard control */
    struct v4l2_ctrl *ctrl_brightness;
    char brightness;

    /* nexell: detect worker */
    struct delayed_work work;
};

static const struct v4l2_fmtdesc capture_fmts[] = {
	{
		.index		= 0,
		.type		= V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.flags		= FORMAT_FLAGS_COMPRESSED,
		.description	= "JPEG + Postview",
		.pixelformat	= V4L2_PIX_FMT_JPEG,
	},
};

struct tw9992_state *_state = NULL;
#if defined(CONFIG_SLSIAP_BACKWARD_CAMERA)

#define REARCAM_BACKDETECT_TIME 10
#define REARCAM_MPOUT_TIME      300

#ifdef CONFIG_SLSIAP_BACKWARD_CAMERA
extern int get_backward_module_num(void);
#endif


static int tw9992_i2c_read_byte(struct i2c_client *, u8, u8 *);

static inline bool _is_backgear_on(void)
{
    int val = nxp_soc_gpio_get_in_value(CFG_BACKWARD_GEAR);
    if (!val) {

        //NX_GPIO_SetOutputValue(PAD_GET_GROUP(CFG_IO_CAM_PWR_EN), PAD_GET_BITNO(CFG_IO_CAM_PWR_EN), 1);
        //_i2c_write_byte(_state.i2c_client, 0x02, 0x44);

        return true;
    } else {
        return false;
    }
}

static inline bool _is_camera_on(void)
{
    u8 data;
    u8 cin;
    extern int mux_status;

    tw9992_i2c_read_byte(_state->i2c_client, 0x03, &data);
    //printk(KERN_ERR "%s: data 0x%x\n", __func__, data);

#if 0
    NX_GPIO_SetOutputValue(PAD_GET_GROUP(CFG_IO_CAM_PWR_EN), PAD_GET_BITNO(CFG_IO_CAM_PWR_EN), 1);
    tw9992_i2c_write_byte(me->i2c_client, 0x02, 0x40);

    if(mux_status == MUX0)
    {   
        _i2c_read_byte(_state.i2c_client, SV_DET, &cin0);
        printk(KERN_ERR "%s: cin0 0x%x\n", __func__, cin0);
        cin0 &= (1<<6);
        if(cin0) {
            return true;
        } else {
            return false;
        }
    }
#endif

    if (data & 0x80)
        return false;

    if ((data & 0x40) && (data & 0x08))
        return true;

    return false;
}

static irqreturn_t _irq_handler(int irq, void *devdata)
{
    printk("IRQ1\n");
    __cancel_delayed_work(&_state->work);
    if (switch_get_state(&_state->switch_dev) && !_is_backgear_on()) {
        printk(KERN_ERR "BACKGEAR OFF\n");
        switch_set_state(&_state->switch_dev, 0);
    } else if (!switch_get_state(&_state->switch_dev) && _is_backgear_on()) {
        schedule_delayed_work(&_state->work, msecs_to_jiffies(REARCAM_BACKDETECT_TIME));
    }

    return IRQ_HANDLED;
}


int register_backward_irq_tw9992(void)
{
    int ret=0;

#if 0
    _state->switch_dev.name = "rearcam";
    switch_dev_register(&_state->switch_dev);
    switch_set_state(&_state->switch_dev, 0);
#endif

    ret = request_irq(IRQ_GPIO_START + CFG_BACKWARD_GEAR, _irq_handler, IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, "tw9900", &_state);
    if (ret<0) {
        pr_err("%s: failed to request_irq(irqnum %d), ret : %d\n", __func__, IRQ_ALIVE_1, ret);
        return -1; 
    }   

    return 0;
}

static void _work_handler(struct work_struct *work) 
{ 
    if (_is_backgear_on() && _is_camera_on()) { 
        printk(KERN_ERR "BACK GEAR ON && CAMERA ON\n"); 
        switch_set_state(&_state->switch_dev, 1); 
        return; 
    } else if (switch_get_state(&_state->switch_dev)) { 
        printk(KERN_ERR "BACKGEAR OFF\n"); 
        switch_set_state(&_state->switch_dev, 0); 
    } 
    else if (_is_backgear_on()) 
    { 
        schedule_delayed_work(&_state->work, msecs_to_jiffies(REARCAM_BACKDETECT_TIME)); 
        printk(KERN_ERR "Rearcam check\n"); 
    } 
} 
#endif

static inline struct tw9992_state *ctrl_to_me(struct v4l2_ctrl *ctrl)
{
    return container_of(ctrl->handler, struct tw9992_state, handler);
}

static int tw9992_i2c_read_byte(struct i2c_client *client, u8 addr, u8 *data)
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

	for(i=0; i<THINE_I2C_RETRY_CNT; i++)
	{
		ret = i2c_transfer(client->adapter, msg, 2);
		if (likely(ret == 2))
			break;
		//mdelay(POLL_TIME_MS);
		//dev_err(&client->dev, "\e[31mtw9992_i2c_write_byte failed reg:0x%02x retry:%d\e[0m\n", addr, i);
	}

	if (unlikely(ret != 2))
	{
		dev_err(&client->dev, "\e[31mtw9992_i2c_read_byte failed reg:0x%02x \e[0m\n", addr);
		return -EIO;
	}

	*data = buf;
	return 0;
}

static int tw9992_i2c_write_byte(struct i2c_client *client, u8 addr, u8 val)
{
	s8 i = 0;
	s8 ret = 0;
	u8 buf[2];
	u8 read_val = 0;
	struct i2c_msg msg;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = buf;

	buf[0] = addr;
	buf[1] = val ;

	for(i=0; i<THINE_I2C_RETRY_CNT; i++)
	{
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (likely(ret == 1))
			break;
		//mdelay(POLL_TIME_MS);
		//dev_err(&client->dev, "\e[31mtw9992_i2c_write_byte failed reg:0x%02x write:0x%04x, retry:%d\e[0m\n", addr, val, i);
	}

	if (ret != 1)
	{
		tw9992_i2c_read_byte(client, addr, &read_val);
		dev_err(&client->dev, "\e[31mtw9992_i2c_write_byte failed reg:0x%02x write:0x%04x, read:0x%04x, retry:%d\e[0m\n", addr, val, read_val, i);
		return -EIO;
	}

	return 0;
}

static int tw9992_i2c_write_block(struct v4l2_subdev *sd, u8 *buf, int size)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	s8 i = 0;
	s8 ret = 0;
	struct i2c_msg msg;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = size;
	msg.buf = buf;

	for(i=0; i<THINE_I2C_RETRY_CNT; i++)
	{
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (likely(ret == 1))
			break;
		msleep(POLL_TIME_MS);
	}

	if (ret != 1)
	{
		dev_err(&client->dev, "\e[31mtw9992_i2c_write_block failed size:%d \e[0m\n", size);
		return -EIO;
	}

	return 0;
}


static int tw9992_reg_set_write(struct i2c_client *client, u8 *RegSet)
{
	u8 index, val;
	int ret = 0;

	while (( RegSet[0] != 0xFF ) || ( RegSet[1]!= 0xFF )) {			// 0xff, 0xff is end of data
		index = *RegSet;
		val = *(RegSet+1);

		ret = tw9992_i2c_write_byte(client, index, val);
		if(ret < 0)
			return ret;

		RegSet+=2;
	}

	return 0;
}

/**
 * psw0523 add private controls
 */
#define V4L2_CID_MUX        (V4L2_CTRL_CLASS_USER | 0x1001)
#define V4L2_CID_STATUS     (V4L2_CTRL_CLASS_USER | 0x1002)

static int tw9992_set_mux(struct v4l2_ctrl *ctrl)
{
    struct tw9992_state *me = ctrl_to_me(ctrl);
    /*printk("%s: val %d\n", __func__, ctrl->val);*/
    if (ctrl->val == 0) {
        // MUX 0 : Black Box
        tw9992_i2c_write_byte(me->i2c_client, 0x02, 0x44);
        tw9992_i2c_write_byte(me->i2c_client, 0x3b, 0x30);
    } else {
        // MUX 1 : Front Camera
        tw9992_i2c_write_byte(me->i2c_client, 0x02, 0x46);
        tw9992_i2c_write_byte(me->i2c_client, 0x3b, 0x0c);
    }


    return 0;
}

static int tw9992_get_status(struct v4l2_ctrl *ctrl)
{
    struct tw9992_state *me = ctrl_to_me(ctrl);
    u8 data = 0;
    u8 mux;
    u8 val = 0;

    tw9992_i2c_read_byte(me->i2c_client, 0x02, &data);
    data = data & 0x0f;
    if (data == 0x4)
        mux = 0;
    else
        mux = 1;

    if (mux == 0) {
        // black box
        /*printk("mux ==> blackbox\n");*/
        printk("mux ==> blackbox\n");
        tw9992_i2c_read_byte(me->i2c_client, 0x03, &data);
        if (!(data & 0x80))
            val |= 1 << 0;

        tw9992_i2c_write_byte(me->i2c_client, 0x52, 0x03);
        tw9992_i2c_read_byte(me->i2c_client, 0x52, &data);
        if (data == 0x03)
            val |= 1 << 1;
    } else {
        // front camera
        /*printk("mux ==> frontcamera\n");*/
        printk("mux ==> frontcamera\n");
        tw9992_i2c_read_byte(me->i2c_client, 0x03, &data);
        if (!(data & 0x80))
            val |= 1 << 1;

        tw9992_i2c_write_byte(me->i2c_client, 0x52, 0x03);
        tw9992_i2c_read_byte(me->i2c_client, 0x52, &data);
        if (data == 0x03)
            val |= 1 << 0;
    }

    /*printk("status: 0x%x\n", val);*/
    ctrl->val = val;
    return 0;
}

static int tw9992_set_brightness(struct v4l2_ctrl *ctrl)
{
    struct tw9992_state *me = ctrl_to_me(ctrl);
    if (me->runmode != TW9992_RUNMODE_RUNNING) {
        me->brightness = ctrl->val;
    } else {
        if (ctrl->val != me->brightness) {
            printk(KERN_ERR "%s: set brightness %d\n", __func__, ctrl->val);
            tw9992_i2c_write_byte(me->i2c_client, 0x10, ctrl->val);
            me->brightness = ctrl->val;
        }
    }
    return 0;
}

static int tw9992_s_ctrl(struct v4l2_ctrl *ctrl)
{
    switch (ctrl->id) {
    case V4L2_CID_MUX:
        return tw9992_set_mux(ctrl);
    case V4L2_CID_BRIGHTNESS:
        printk("%s: brightness\n", __func__);
        return tw9992_set_brightness(ctrl);
    default:
        printk(KERN_ERR "%s: invalid control id 0x%x\n", __func__, ctrl->id);
        return -EINVAL;
    }
}

static int tw9992_g_volatile_ctrl(struct v4l2_ctrl *ctrl)
{
    switch (ctrl->id) {
    case V4L2_CID_STATUS:
        return tw9992_get_status(ctrl);
    default:
        printk(KERN_ERR "%s: invalid control id 0x%x\n", __func__, ctrl->id);
        return -EINVAL;
    }
}

static const struct v4l2_ctrl_ops tw9992_ctrl_ops = {
     .s_ctrl = tw9992_s_ctrl,
     .g_volatile_ctrl = tw9992_g_volatile_ctrl,
};

static const struct v4l2_ctrl_config tw9992_custom_ctrls[] = {
    {
        .ops  = &tw9992_ctrl_ops,
        .id   = V4L2_CID_MUX,
        .type = V4L2_CTRL_TYPE_INTEGER,
        .name = "MuxControl",
        .min  = 0,
        .max  = 1,
        .def  = 1,
        .step = 1,
    },
    {
        .ops  = &tw9992_ctrl_ops,
        .id   = V4L2_CID_STATUS,
        .type = V4L2_CTRL_TYPE_INTEGER,
        .name = "Status",
        .min  = 0,
        .max  = 1,
        .def  = 1,
        .step = 1,
        .flags = V4L2_CTRL_FLAG_VOLATILE,
    }
};

#define NUM_CTRLS 3
static int tw9992_initialize_ctrls(struct tw9992_state *me)
{
    v4l2_ctrl_handler_init(&me->handler, NUM_CTRLS);

    me->ctrl_mux = v4l2_ctrl_new_custom(&me->handler, &tw9992_custom_ctrls[0], NULL);
    if (!me->ctrl_mux) {
         printk(KERN_ERR "%s: failed to v4l2_ctrl_new_custom for mux\n", __func__);
         return -ENOENT;
    }

    me->ctrl_status = v4l2_ctrl_new_custom(&me->handler, &tw9992_custom_ctrls[1], NULL);
    if (!me->ctrl_status) {
         printk(KERN_ERR "%s: failed to v4l2_ctrl_new_custom for status\n", __func__);
         return -ENOENT;
    }

    me->ctrl_brightness = v4l2_ctrl_new_std(&me->handler, &tw9992_ctrl_ops,
            V4L2_CID_BRIGHTNESS, -128, 127, 1, -112);
    if (!me->ctrl_brightness) {
        printk(KERN_ERR "%s: failed to v4l2_ctrl_new_std for brightness\n", __func__);
        return -ENOENT;
    }

    me->sd.ctrl_handler = &me->handler;
    if (me->handler.error) {
        printk(KERN_ERR "%s: ctrl handler error(%d)\n", __func__, me->handler.error);
        v4l2_ctrl_handler_free(&me->handler);
        return -EINVAL;
    }

    return 0;
}


/*
 * Parse the init_reg2 array into a number of register sets that
 * we can send over as i2c burst writes instead of writing each
 * entry of init_reg2 as a single 4 byte write.  Write the
 * new data structures and then free them.
 */
static int tw9992_write_init_reg2_burst(struct v4l2_subdev *sd) __attribute__((unused));
static int tw9992_write_init_reg2_burst(struct v4l2_subdev *sd)
{
	struct tw9992_state *state =
		container_of(sd, struct tw9992_state, sd);
	struct tw9992_regset *regset_table;
	struct tw9992_regset *regset;
	struct tw9992_regset *end_regset;
	u8 *regset_data;
	u8 *dst_ptr;
	const u32 *end_src_ptr;
	bool flag_copied;
	int init_reg_2_array_size = state->regs->init_reg_2.array_size;
	int init_reg_2_size = init_reg_2_array_size * sizeof(u32);
	const u32 *src_ptr = state->regs->init_reg_2.reg;
	u32 src_value;
	int err;

	pr_debug("%s : start\n", __func__);

	regset_data = vmalloc(init_reg_2_size);
	if (regset_data == NULL)
		return -ENOMEM;
	regset_table = vmalloc(sizeof(struct tw9992_regset) *
			init_reg_2_size);
	if (regset_table == NULL) {
		kfree(regset_data);
		return -ENOMEM;
	}

	dst_ptr = regset_data;
	regset = regset_table;
	end_src_ptr = &state->regs->init_reg_2.reg[init_reg_2_array_size];

	src_value = *src_ptr++;
	while (src_ptr <= end_src_ptr) {
		/* initial value for a regset */
		regset->data = dst_ptr;
		flag_copied = false;
		*dst_ptr++ = src_value >> 24;
		*dst_ptr++ = src_value >> 16;
		*dst_ptr++ = src_value >> 8;
		*dst_ptr++ = src_value;

		/* check subsequent values for a data flag (starts with
		   0x0F12) or something else */
		do {
			src_value = *src_ptr++;
			if ((src_value & 0xFFFF0000) != 0x0F120000) {
				/* src_value is start of next regset */
				regset->size = dst_ptr - regset->data;
				regset++;
				break;
			}
			/* copy the 0x0F12 flag if not done already */
			if (!flag_copied) {
				*dst_ptr++ = src_value >> 24;
				*dst_ptr++ = src_value >> 16;
				flag_copied = true;
			}
			/* copy the data part */
			*dst_ptr++ = src_value >> 8;
			*dst_ptr++ = src_value;
		} while (src_ptr < end_src_ptr);
	}
	pr_debug("%s : finished creating table\n", __func__);

	end_regset = regset;
	pr_debug("%s : first regset = %p, last regset = %p, count = %d\n",
		__func__, regset_table, regset, end_regset - regset_table);
	pr_debug("%s : regset_data = %p, end = %p, dst_ptr = %p\n", __func__,
		regset_data, regset_data + (init_reg_2_size * sizeof(u32)),
		dst_ptr);

#ifdef CONFIG_VIDEO_TW9992_DEBUG
	if (tw9992_debug_mask & TW9992_DEBUG_I2C_BURSTS) {
		int last_regset_end_addr = 0;
		regset = regset_table;
		do {
			tw9992_dump_regset(regset);
			if (regset->size > 4) {
				int regset_addr = (regset->data[2] << 8 |
						regset->data[3]);
				if (last_regset_end_addr == regset_addr)
					pr_info("%s : this regset can be"
						" combined with previous\n",
						__func__);
				last_regset_end_addr = (regset_addr
							+ regset->size - 6);
			}
			regset++;
		} while (regset < end_regset);
	}
#endif
	regset = regset_table;
	pr_debug("%s : start writing init reg 2 bursts\n", __func__);
	do {
		if (regset->size > 4) {
			/* write the address packet */
			err = tw9992_i2c_write_block(sd, regset->data, 4);
			if (err)
				break;
			/* write the data in a burst */
			err = tw9992_i2c_write_block(sd, regset->data+4,
						regset->size-4);

		} else
			err = tw9992_i2c_write_block(sd, regset->data,
						regset->size);
		if (err)
			break;
		regset++;
	} while (regset < end_regset);

	pr_debug("%s : finished writing init reg 2 bursts\n", __func__);

	vfree(regset_data);
	vfree(regset_table);

	return err;
}

static int tw9992_set_from_table(struct v4l2_subdev *sd,
				const char *setting_name,
				const struct tw9992_regset_table *table,
				int table_size, int index)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	/* return if table is not initilized */
	if ((unsigned int)table < (unsigned int)0xc0000000)
		return 0;

	//dev_err(&client->dev, "%s: set %s index %d\n",
	//	__func__, setting_name, index);
	if ((index < 0) || (index >= table_size)) {
		dev_err(&client->dev, "%s: index(%d) out of range[0:%d] for table for %s\n",
							__func__, index, table_size, setting_name);
		return -EINVAL;
	}
	table += index;
	if (table->reg == NULL)
		return -EINVAL;
	return 0;//tw9992_write_regs(sd, table->reg, table->array_size);
}

static int tw9992_set_parameter(struct v4l2_subdev *sd,
				int *current_value_ptr,
				int new_value,
				const char *setting_name,
				const struct tw9992_regset_table *table,
				int table_size)
{
	int err;
/*
	if (*current_value_ptr == new_value)
		return 0;
		*/

	err = tw9992_set_from_table(sd, setting_name, table,
				table_size, new_value);

	if (!err)
		*current_value_ptr = new_value;
	return err;
}

static void tw9992_init_parameters(struct v4l2_subdev *sd)
{
	struct tw9992_state *state =
		container_of(sd, struct tw9992_state, sd);
	struct sec_cam_parm *parms =
		(struct sec_cam_parm *)&state->strm.parm.raw_data;
	struct sec_cam_parm *stored_parms =
		(struct sec_cam_parm *)&state->stored_parm.parm.raw_data;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	dev_err(&client->dev, "%s: \n", __func__);
	state->strm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	parms->capture.capturemode = 0;
	parms->capture.timeperframe.numerator = 1;
	parms->capture.timeperframe.denominator = 30;
	parms->contrast = V4L2_CONTRAST_DEFAULT;
	parms->effects = V4L2_IMAGE_EFFECT_NORMAL;
	parms->brightness = V4L2_BRIGHTNESS_DEFAULT;
	parms->flash_mode = FLASH_MODE_AUTO;
	parms->focus_mode = V4L2_FOCUS_MODE_AUTO;
	parms->iso = V4L2_ISO_AUTO;
	parms->metering = V4L2_METERING_CENTER;
	parms->saturation = V4L2_SATURATION_DEFAULT;
	parms->scene_mode = V4L2_SCENE_MODE_NONE;
	parms->sharpness = V4L2_SHARPNESS_DEFAULT;
	parms->white_balance = V4L2_WHITE_BALANCE_AUTO;
	parms->aeawb_lockunlock = V4L2_AE_UNLOCK_AWB_UNLOCK;

	stored_parms->effects = V4L2_IMAGE_EFFECT_NORMAL;
	stored_parms->brightness = V4L2_BRIGHTNESS_DEFAULT;
	stored_parms->iso = V4L2_ISO_AUTO;
	stored_parms->metering = V4L2_METERING_CENTER;
	stored_parms->scene_mode = V4L2_SCENE_MODE_NONE;
	stored_parms->white_balance = V4L2_WHITE_BALANCE_AUTO;

	state->jpeg.enable = 0;
	state->jpeg.quality = 100;
	state->jpeg.main_offset = 0;
	state->jpeg.main_size = 0;
	state->jpeg.thumb_offset = 0;
	state->jpeg.thumb_size = 0;
	state->jpeg.postview_offset = 0;

	state->fw.major = 1;

	state->one_frame_delay_ms = NORMAL_MODE_MAX_ONE_FRAME_DELAY_MS;

    /* psw0523 block this */
	/* tw9992_stop_auto_focus(sd); */
}

static void tw9992_set_framesize(struct v4l2_subdev *sd,
				const struct tw9992_framesize *frmsize,
				int frmsize_count, bool exact_match);



/* This function is called from the g_ctrl api
 *
 * This function should be called only after the s_fmt call,
 * which sets the required width/height value.
 *
 * It checks a list of available frame sizes and sets the
 * most appropriate frame size.
 *
 * The list is stored in an increasing order (as far as possible).
 * Hence the first entry (searching from the beginning) where both the
 * width and height is more than the required value is returned.
 * In case of no perfect match, we set the last entry (which is supposed
 * to be the largest resolution supported.)
 */
static void tw9992_set_framesize(struct v4l2_subdev *sd,
				const struct tw9992_framesize *frmsize,
				int frmsize_count, bool preview)
{
	struct tw9992_state *state =
		container_of(sd, struct tw9992_state, sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	const struct tw9992_framesize *last_frmsize =
		&frmsize[frmsize_count - 1];
	int err;

	dev_err(&client->dev, "%s: Requested Res: %dx%d\n", __func__,
		state->pix.width, state->pix.height);

	do {
		/*
		 * In case of image capture mode,
		 * if the given image resolution is not supported,
		 * return the next higher image resolution. */
		if (preview) {
			if (frmsize->width == state->pix.width &&
				frmsize->height == state->pix.height) {
				break;
			}
		} else {
			dev_err(&client->dev,
				"%s: compare frmsize %dx%d to %dx%d\n",
				__func__,
				frmsize->width, frmsize->height,
				state->pix.width, state->pix.height);
			if (frmsize->width >= state->pix.width &&
				frmsize->height >= state->pix.height) {
				dev_err(&client->dev,
					"%s: select frmsize %dx%d, index=%d\n",
					__func__,
					frmsize->width, frmsize->height,
					frmsize->index);
				break;
			}
		}

		frmsize++;
	} while (frmsize <= last_frmsize);

	if (frmsize > last_frmsize)
		frmsize = last_frmsize;

	state->pix.width = frmsize->width;
	state->pix.height = frmsize->height;
	if (preview) {
		state->preview_framesize_index = frmsize->index;
		dev_err(&client->dev, "%s: Preview Res Set: %dx%d, index %d\n",
			__func__, state->pix.width, state->pix.height,
			state->preview_framesize_index);

		err = tw9992_set_from_table(sd, "set preview size",
					state->regs->preview_size,
					ARRAY_SIZE(state->regs->preview_size),
					state->preview_framesize_index);
		if (err < 0) {
			v4l_info(client, "%s: register set failed\n", __func__);
		}

	} else {
		state->capture_framesize_index = frmsize->index;
		dev_err(&client->dev, "%s: Capture Res Set: %dx%d, index %d\n",
			__func__, state->pix.width, state->pix.height,
			state->capture_framesize_index);

		err = tw9992_set_from_table(sd, "set capture size",
					state->regs->capture_size,
					ARRAY_SIZE(state->regs->capture_size),
					state->capture_framesize_index);

		if (err < 0) {
			v4l_info(client, "%s: register set failed\n", __func__);
		}

	}

}

static void tw9992_get_esd_int(struct v4l2_subdev *sd, struct v4l2_control *ctrl) __attribute__((unused));
static void tw9992_get_esd_int(struct v4l2_subdev *sd,
				struct v4l2_control *ctrl)
{
	struct tw9992_state *state =
		container_of(sd, struct tw9992_state, sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u16 read_value;
	int err;

	if ((TW9992_RUNMODE_RUNNING == state->runmode) &&
		(state->af_status != AF_START)) {
		err = tw9992_set_from_table(sd, "get esd status",
					&state->regs->get_esd_status,
					1, 0);
		//err |= tw9992_i2c_read_word(client, 0x0F12, &read_value);
		dev_dbg(&client->dev,
			"%s: read_value == 0x%x\n", __func__, read_value);
		/* return to write mode */
		//err |= tw9992_i2c_write_word(client, 0x0028, 0x7000);

		if (err < 0) {
			v4l_info(client,
				"Failed I2C for getting ESD information\n");
			ctrl->value = 0x01;
		} else {
			if (read_value != 0x0000) {
				v4l_info(client, "ESD interrupt happened!!\n");
				ctrl->value = 0x01;
			} else {
				dev_dbg(&client->dev,
					"%s: No ESD interrupt!!\n", __func__);
				ctrl->value = 0x00;
			}
		}
	} else
		ctrl->value = 0x00;
}

#ifdef CONFIG_VIDEO_TW9992_DEBUG
static void tw9992_dump_regset(struct tw9992_regset *regset)
{
	if ((regset->data[0] == 0x00) && (regset->data[1] == 0x2A)) {
		if (regset->size <= 6)
			pr_err("odd regset size %d\n", regset->size);
		pr_info("regset: addr = 0x%02X%02X, data[0,1] = 0x%02X%02X,"
			" total data size = %d\n",
			regset->data[2], regset->data[3],
			regset->data[6], regset->data[7],
			regset->size-6);
	} else {
		pr_info("regset: 0x%02X%02X%02X%02X\n",
			regset->data[0], regset->data[1],
			regset->data[2], regset->data[3]);
		if (regset->size != 4)
			pr_err("odd regset size %d\n", regset->size);
	}
}
#endif

/*
 * s_config subdev ops
 * With camera device, we need to re-initialize
 * every single opening time therefor,
 * it is not necessary to be initialized on probe time.
 * except for version checking
 * NOTE: version checking is optional
 */
static int tw9992_power(int flag)
{
    u32 reset = (PAD_GPIO_B + 23);

	printk("%s: sensor is power %s\n", __func__, flag == 1 ?"on":"off");

    if (flag) {
        nxp_soc_gpio_set_out_value(reset, 1);
        nxp_soc_gpio_set_io_dir(reset, 1);
        nxp_soc_gpio_set_io_func(reset, nxp_soc_gpio_get_altnum(reset));
        mdelay(1);

        nxp_soc_gpio_set_out_value(reset, 0);
        mdelay(10);

        nxp_soc_gpio_set_out_value(reset, 1);
        mdelay(10);
    }
	else
	{
        nxp_soc_gpio_set_out_value(reset, 0);
	}
	return 0;

}

static int tw9992_s_power(struct v4l2_subdev *sd, int on)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct tw9992_state *state = container_of(sd, struct tw9992_state, sd);
	int ret = 0;

	dev_err(&client->dev, "%s() on:%d \n", __func__, on);

    if (on)
	{
		tw9992_init_parameters(sd);

		if(state->power_on == TW9992_HW_POWER_OFF)
		{
			//ret = tw9992_power(TW9992_HW_POWER_ON);

			//ret = tw9992_reg_set_write(client, TW9992_DataSet);
			if(ret < 0) {
				dev_err(&client->dev, "\e[31mTW9992_DataSet0 error\e[0m, ret = %d\n", ret);
				return ret;
			}
			state->power_on = TW9992_HW_POWER_ON;
		}
	}
	else
	{
		//ret = tw9992_power(TW9992_HW_POWER_OFF);
		state->power_on = TW9992_HW_POWER_OFF;
        state->initialized = false;
	}
		return ret;
}

u8 tw9992_get_color_system(struct i2c_client *client)
{
	u8 reg_val;

	mdelay(100);
	tw9992_i2c_read_byte(client, 0x1C, &reg_val);

	if((reg_val & 0x70) != 0x00)
		return 1;	// PAL
	else
		return 0;	// NTSC
}

int tw9992_decoder_lock(struct i2c_client *client)
{
	u16	i;
	u8	Status03, Status1C;

    /*unsigned int video_loss = ((PAD_GPIO_C + 4) | PAD_FUNC_ALT1);*/
    unsigned int video_det = ((PAD_GPIO_B + 12) | PAD_FUNC_ALT0);

	//nxp_soc_gpio_set_io_func(video_loss, nxp_soc_gpio_get_altnum(video_loss));
	//nxp_soc_gpio_set_io_dir(video_loss, 0);

	nxp_soc_gpio_set_io_func(video_det, nxp_soc_gpio_get_altnum(video_det));
	nxp_soc_gpio_set_io_dir(video_det, 0);

	tw9992_i2c_write_byte(client, 0x1d, 0x83);		// start detect

	for ( i=0; i<800; i++ ) {						// LOOP for 800ms

		while(nxp_soc_gpio_get_out_value(video_det) == 1){;}

		tw9992_i2c_read_byte(client, 0x03, &Status03);
		tw9992_i2c_read_byte(client, 0x1C, &Status1C);

		if ( Status1C & 0x80 ) 	continue;			// chwck end of detect
		if ( Status03 & 0x80 )	continue;			// VDLOSS
		if (( Status03 & 0x68 ) != 0x68) 	continue;		// VLOCK, SLOCK, HLOCK check

		if ( Status03 & 0x10 )		continue;		// if ODD field wait until it goes to EVEN field
		//if (( Status03 & 0x10 ) == 0)		continue;		// if EVEN field wait until it goes to ODD field
		else {
			mdelay(5);										// give some delay
			tw9992_i2c_write_byte(client, 0x70, 0x01);		//			// MIPI out
			break;			//
		}
	}

	//printk("===== video_loss : 0x%x \n", nxp_soc_gpio_get_out_value(video_loss));
	printk("===== video_det : 0x%x \n", nxp_soc_gpio_get_out_value(video_det));

	tw9992_i2c_read_byte(client, 0x00, &Status03);
	printk("===== Product ID code : 0x%x \n", Status03);

	tw9992_i2c_read_byte(client, 0x03, &Status03);
	printk("===== Reg 0x03 : 0x%x.\n", Status03);

	if(Status03 & 0x80)	printk("===== Video not present.\n");
	else				printk("===== Video Detected.\n");

	if(Status03 & 0x40)	printk("===== Horizontal sync PLL is locked to the incoming video source.\n");
	else				printk("===== Horizontal sync PLL is not locked.\n");

	if(Status03 & 0x20)	printk("===== Sub-carrier PLL is locked to the incoming video source.\n");
	else				printk("===== Sub-carrier PLL is not locked.\n");

	if(Status03 & 0x10)	printk("===== Odd field is being decoded.\n");
	else				printk("===== Even field is being decoded.\n");

	if(Status03 & 0x8)	printk("===== Vertical logic is locked to the incoming video source.\n");
	else				printk("===== Vertical logic is not locked.\n");

	if(Status03 & 0x2)	printk("===== No color burst signal detected.\n");
	else				printk("===== Color burst signal detected.\n");

	if(Status03 & 0x1)	printk("===== 50Hz source detected.\n");
	else				printk("===== 60Hz source detected.\n");

	tw9992_i2c_read_byte(client, 0x1C, &Status1C);
	printk("\n===== Reg 0x1C : 0x%x.\n", Status1C);
	if(Status1C & 0x80)	printk("===== Detection in progress.\n");
	else				printk("===== Idle.\n");


	return 0;
}

static int tw9992_init(struct v4l2_subdev *sd, u32 val)
{
    struct i2c_client *client = v4l2_get_subdevdata(sd);

    int ret = 0;
    dev_err(&client->dev, "%s: start\n", __func__);

//    mdelay(500);
    ret = tw9992_reg_set_write(client, TW9992_DataSet);
    if(ret < 0) {
        dev_err(&client->dev, "\e[31mTW9992_DataSet0 error\e[0m, ret = %d\n", ret);
        return ret;
    }

    if(ret < 0) {
        dev_err(&client->dev, "\e[31mcolor_system() error\e[0m, ret = %d\n", ret);
        return ret;
    }

#if 0
	u8 data = 0;
	tw9992_i2c_read_byte(client, 0x02, &data);
    data = data & 0x0f;

	printk("%s - data : %02X\n", __func__, data);	
    if (data == 0x4)
		printk("%s - mux == 0\n", __func__);
    else
		printk("%s - mux == 1\n", __func__);
#endif

    return ret;
}

static const struct v4l2_subdev_core_ops tw9992_core_ops = {
	.s_power		= tw9992_s_power,
	.init 			= tw9992_init,/* initializing API */
};


static int tw9992_s_mbus_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *fmt)
{
	struct tw9992_state *state = container_of(sd, struct tw9992_state, sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	dev_err(&client->dev, "%s() \n", __func__);

	if (fmt->width < fmt->height) {
		int temp;
		temp  = fmt->width;
		fmt->width = fmt->height;
		fmt->height = temp;
	}

	state->pix.width = fmt->width;
	state->pix.height = fmt->height;
	//state->pix.pixelformat = fmt->fmt.pix.pixelformat;

	if (state->oprmode == TW9992_OPRMODE_IMAGE) {
		state->oprmode = TW9992_OPRMODE_IMAGE;
		/*
		 * In case of image capture mode,
		 * if the given image resolution is not supported,
		 * use the next higher image resolution. */
		tw9992_set_framesize(sd, tw9992_capture_framesize_list,
				ARRAY_SIZE(tw9992_capture_framesize_list),
				false);

	} else {
		state->oprmode = TW9992_OPRMODE_VIDEO;
		/*
		 * In case of video mode,
		 * if the given video resolution is not matching, use
		 * the default rate (currently TW9992_PREVIEW_WVGA).
		 */
		tw9992_set_framesize(sd, tw9992_preview_framesize_list,
				ARRAY_SIZE(tw9992_preview_framesize_list),
				true);
	}

	state->jpeg.enable = state->pix.pixelformat == V4L2_PIX_FMT_JPEG;

	return 0;
}

static int tw9992_enum_framesizes(struct v4l2_subdev *sd,
				  struct v4l2_frmsizeenum *fsize)
{
	struct tw9992_state *state = container_of(sd, struct tw9992_state, sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	dev_err(&client->dev, "%s() wid=%d\t height=%d\n", __func__,state->pix.width,state->pix.height);

	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->discrete.width = state->pix.width;
	fsize->discrete.height = state->pix.height;

	return 0;
}

static int tw9992_g_parm(struct v4l2_subdev *sd,
			struct v4l2_streamparm *param)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct tw9992_state *state = container_of(sd, struct tw9992_state, sd);

	dev_err(&client->dev, "%s() \n", __func__);

	memcpy(param, &state->strm, sizeof(param));
	return 0;
}

static int tw9992_s_parm(struct v4l2_subdev *sd,
			struct v4l2_streamparm *param)
{
	int err = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct tw9992_state *state = container_of(sd, struct tw9992_state, sd);
	struct sec_cam_parm *new_parms = (struct sec_cam_parm *)&param->parm.raw_data;
	struct sec_cam_parm *parms = (struct sec_cam_parm *)&state->strm.parm.raw_data;

	dev_err(&client->dev, "%s() \n", __func__);

	if (param->parm.capture.timeperframe.numerator !=
		parms->capture.timeperframe.numerator ||
		param->parm.capture.timeperframe.denominator !=
		parms->capture.timeperframe.denominator) {

		int fps = 0;
		int fps_max = 30;

		if (param->parm.capture.timeperframe.numerator &&
			param->parm.capture.timeperframe.denominator)
			fps =
			    (int)(param->parm.capture.timeperframe.denominator /
				  param->parm.capture.timeperframe.numerator);
		else
			fps = 0;

		if (fps <= 0 || fps > fps_max) {
			dev_err(&client->dev,
				"%s: Framerate %d not supported,"
				" setting it to %d fps.\n",
				__func__, fps, fps_max);
			fps = fps_max;
		}

		/*
		 * Don't set the fps value, just update it in the state
		 * We will set the resolution and
		 * fps in the start operation (preview/capture) call
		 */
		parms->capture.timeperframe.numerator = 1;
		parms->capture.timeperframe.denominator = fps;
	}

	/* we return an error if one happened but don't stop trying to
	 * set all parameters passed
	 */
	err = tw9992_set_parameter(sd, &parms->contrast, new_parms->contrast,
				"contrast", state->regs->contrast,
				ARRAY_SIZE(state->regs->contrast));
	err |= tw9992_set_parameter(sd, &parms->effects, new_parms->effects,
				"effect", state->regs->effect,
				ARRAY_SIZE(state->regs->effect));
	err |= tw9992_set_parameter(sd, &parms->brightness,
				new_parms->brightness, "brightness",
				state->regs->ev, ARRAY_SIZE(state->regs->ev));
///	err |= tw9992_set_flash_mode(sd, new_parms->flash_mode);
///	err |= tw9992_set_focus_mode(sd, new_parms->focus_mode);
	err |= tw9992_set_parameter(sd, &parms->iso, new_parms->iso,
				"iso", state->regs->iso,
				ARRAY_SIZE(state->regs->iso));
	err |= tw9992_set_parameter(sd, &parms->metering, new_parms->metering,
				"metering", state->regs->metering,
				ARRAY_SIZE(state->regs->metering));
	err |= tw9992_set_parameter(sd, &parms->saturation,
				new_parms->saturation, "saturation",
				state->regs->saturation,
				ARRAY_SIZE(state->regs->saturation));
	err |= tw9992_set_parameter(sd, &parms->scene_mode,
				new_parms->scene_mode, "scene_mode",
				state->regs->scene_mode,
				ARRAY_SIZE(state->regs->scene_mode));
	err |= tw9992_set_parameter(sd, &parms->sharpness,
				new_parms->sharpness, "sharpness",
				state->regs->sharpness,
				ARRAY_SIZE(state->regs->sharpness));
	err |= tw9992_set_parameter(sd, &parms->aeawb_lockunlock,
				new_parms->aeawb_lockunlock, "aeawb_lockunlock",
				state->regs->aeawb_lockunlock,
				ARRAY_SIZE(state->regs->aeawb_lockunlock));
	err |= tw9992_set_parameter(sd, &parms->white_balance,
				new_parms->white_balance, "white balance",
				state->regs->white_balance,
				ARRAY_SIZE(state->regs->white_balance));
	err |= tw9992_set_parameter(sd, &parms->fps,
				new_parms->fps, "fps",
				state->regs->fps,
				ARRAY_SIZE(state->regs->fps));

	if (parms->scene_mode == SCENE_MODE_NIGHTSHOT)
		state->one_frame_delay_ms = NIGHT_MODE_MAX_ONE_FRAME_DELAY_MS;
	else
		state->one_frame_delay_ms = NORMAL_MODE_MAX_ONE_FRAME_DELAY_MS;

	dev_dbg(&client->dev, "%s: returning %d\n", __func__, err);
	return err;
}

static int tw9992_s_stream(struct v4l2_subdev *sd, int enable)
{
    struct i2c_client *client = v4l2_get_subdevdata(sd);
    struct tw9992_state *state = container_of(sd, struct tw9992_state, sd);
    int ret = 0;

    dev_err(&client->dev, "%s() \n", __func__);

    if (enable && state->runmode != TW9992_RUNMODE_RUNNING) {
        tw9992_init(sd, enable);
        tw9992_i2c_write_byte(state->i2c_client, 0x10, state->brightness);
        state->runmode = TW9992_RUNMODE_RUNNING;
    }
    return ret;
}

static const struct v4l2_subdev_video_ops tw9992_video_ops = {
	//.g_mbus_fmt 		= tw9992_g_mbus_fmt,
	.s_mbus_fmt 		= tw9992_s_mbus_fmt,
	.enum_framesizes 	= tw9992_enum_framesizes,
	///.enum_fmt 		= tw9992_enum_fmt,
	///.try_fmt 		= tw9992_try_fmt,
	.g_parm 		= tw9992_g_parm,
	.s_parm 		= tw9992_s_parm,
	.s_stream 		= tw9992_s_stream,
};

/**
 * __find_oprmode - Lookup TW9992 resolution type according to pixel code
 * @code: pixel code
 */
static enum tw9992_oprmode __find_oprmode(enum v4l2_mbus_pixelcode code)
{
	enum tw9992_oprmode type = TW9992_OPRMODE_VIDEO;

	do {
		if (code == default_fmt[type].code)
			return type;
	} while (type++ != SIZE_DEFAULT_FFMT);

	return 0;
}

/**
 * __find_resolution - Lookup preset and type of M-5MOLS's resolution
 * @mf: pixel format to find/negotiate the resolution preset for
 * @type: M-5MOLS resolution type
 * @resolution:	M-5MOLS resolution preset register value
 *
 * Find nearest resolution matching resolution preset and adjust mf
 * to supported values.
 */
static int __find_resolution(struct v4l2_subdev *sd,
			     struct v4l2_mbus_framefmt *mf,
			     enum tw9992_oprmode *type,
			     u32 *resolution)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	const struct tw9992_resolution *fsize = &tw9992_resolutions[0];
	const struct tw9992_resolution *match = NULL;
	enum tw9992_oprmode stype = __find_oprmode(mf->code);
	int i = ARRAY_SIZE(tw9992_resolutions);
	unsigned int min_err = ~0;
	int err;

	while (i--) {
		if (stype == fsize->type) {
			err = abs(fsize->width - mf->width)
				+ abs(fsize->height - mf->height);

			if (err < min_err) {
				min_err = err;
				match = fsize;
				stype = fsize->type;
			}
		}
		fsize++;
	}
	dev_err(&client->dev, "LINE(%d): mf width: %d, mf height: %d, mf code: %d\n", __LINE__,
		mf->width, mf->height, stype);
	dev_err(&client->dev, "LINE(%d): match width: %d, match height: %d, match code: %d\n", __LINE__,
		match->width, match->height, stype);
	if (match) {
		mf->width  = match->width;
		mf->height = match->height;
		*resolution = match->value;
		*type = stype;
		return 0;
	}
	dev_err(&client->dev, "LINE(%d): mf width: %d, mf height: %d, mf code: %d\n", __LINE__,
		mf->width, mf->height, stype);

	return -EINVAL;
}

static struct v4l2_mbus_framefmt *__find_format(struct tw9992_state *state,
				struct v4l2_subdev_fh *fh,
				enum v4l2_subdev_format_whence which,
				enum tw9992_oprmode type)
{
	if (which == V4L2_SUBDEV_FORMAT_TRY)
		return fh ? v4l2_subdev_get_try_format(fh, 0) : NULL;

	return &state->ffmt[type];
}

/* enum code by flite video device command */
static int tw9992_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_fh *fh,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	if (!code || code->index >= SIZE_DEFAULT_FFMT)
		return -EINVAL;

	code->code = default_fmt[code->index].code;

	return 0;
}

/* get format by flite video device command */
static int tw9992_get_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
			  struct v4l2_subdev_format *fmt)
{
	struct tw9992_state *state =
		container_of(sd, struct tw9992_state, sd);
	struct v4l2_mbus_framefmt *format;

	if (fmt->pad != 0)
		return -EINVAL;

	format = __find_format(state, fh, fmt->which, state->res_type);
	if (!format)
		return -EINVAL;

	fmt->format = *format;
	return 0;
}

/* set format by flite video device command */
static int tw9992_set_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
			  struct v4l2_subdev_format *fmt)
{
	struct tw9992_state *state =
		container_of(sd, struct tw9992_state, sd);
	struct v4l2_mbus_framefmt *format = &fmt->format;
	struct v4l2_mbus_framefmt *sfmt;
	enum tw9992_oprmode type;
	u32 resolution = 0;
	int ret;

	if (fmt->pad != 0)
		return -EINVAL;

	ret = __find_resolution(sd, format, &type, &resolution);
	if (ret < 0)
		return ret;

	sfmt = __find_format(state, fh, fmt->which, type);
	if (!sfmt)
		return 0;

	sfmt		= &default_fmt[type];
	sfmt->width	= format->width;
	sfmt->height	= format->height;

	if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE) {
		/* for enum size of entity by flite */
		state->oprmode  		= type;
		state->ffmt[type].width 	= format->width;
		state->ffmt[type].height 	= format->height;
#ifndef CONFIG_VIDEO_TW9992_SENSOR_JPEG
		state->ffmt[type].code 		= V4L2_MBUS_FMT_YUYV8_2X8;
#else
		state->ffmt[type].code 		= format->code;
#endif

		/* find adaptable resolution */
		state->resolution 		= resolution;
#ifndef CONFIG_VIDEO_TW9992_SENSOR_JPEG
		state->code 			= V4L2_MBUS_FMT_YUYV8_2X8;
#else
		state->code 			= format->code;
#endif
		state->res_type 		= type;

		/* for set foramat */
		state->pix.width 		= format->width;
		state->pix.height 		= format->height;

		if (state->power_on == TW9992_HW_POWER_ON)
			tw9992_s_mbus_fmt(sd, sfmt);  /* set format */
	}

	return 0;
}


static struct v4l2_subdev_pad_ops tw9992_pad_ops = {
	.enum_mbus_code	= tw9992_enum_mbus_code,
	.get_fmt		= tw9992_get_fmt,
	.set_fmt		= tw9992_set_fmt,
};

static const struct v4l2_subdev_ops tw9992_ops = {
	.core = &tw9992_core_ops,
	.video = &tw9992_video_ops,
	.pad	= &tw9992_pad_ops,
};

static int tw9992_link_setup(struct media_entity *entity,
			    const struct media_pad *local,
			    const struct media_pad *remote, u32 flags)
{
	printk("%s\n", __func__);
	return 0;
}

static const struct media_entity_operations tw9992_media_ops = {
	.link_setup = tw9992_link_setup,
};

/* internal ops for media controller */
static int tw9992_init_formats(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct v4l2_subdev_format format;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	dev_err(&client->dev, "%s: \n", __func__);
	memset(&format, 0, sizeof(format));
	format.pad = 0;
	format.which = fh ? V4L2_SUBDEV_FORMAT_TRY : V4L2_SUBDEV_FORMAT_ACTIVE;
	format.format.code = DEFAULT_SENSOR_CODE;
	format.format.width = DEFAULT_SENSOR_WIDTH;
	format.format.height = DEFAULT_SENSOR_HEIGHT;

	return 0;
}

static int tw9992_subdev_close(struct v4l2_subdev *sd,
			      struct v4l2_subdev_fh *fh)
{
	tw9992_debug(TW9992_DEBUG_I2C, "%s", __func__);
	printk("%s", __func__);
	return 0;
}

static int tw9992_subdev_registered(struct v4l2_subdev *sd)
{
	tw9992_debug(TW9992_DEBUG_I2C, "%s", __func__);
	return 0;
}

static void tw9992_subdev_unregistered(struct v4l2_subdev *sd)
{
	tw9992_debug(TW9992_DEBUG_I2C, "%s", __func__);
	return;
}

static const struct v4l2_subdev_internal_ops tw9992_v4l2_internal_ops = {
	.open = tw9992_init_formats,
	.close = tw9992_subdev_close,
	.registered = tw9992_subdev_registered,
	.unregistered = tw9992_subdev_unregistered,
};

/*
 * tw9992_probe
 * Fetching platform data is being done with s_config subdev call.
 * In probe routine, we just register subdev device
 */
static int tw9992_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct tw9992_state *state;
	int ret = 0;

	state = kzalloc(sizeof(struct tw9992_state), GFP_KERNEL);
	if (state == NULL)
		return -ENOMEM;

	mutex_init(&state->ctrl_lock);
	init_completion(&state->af_complete);

	state->power_on = TW9992_HW_POWER_OFF;

	state->runmode = TW9992_RUNMODE_NOTREADY;
	sd = &state->sd;
	strcpy(sd->name, TW9992_DRIVER_NAME);

	v4l2_i2c_subdev_init(sd, client, &tw9992_ops);
	state->pad.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_init(&sd->entity, 1, &state->pad, 0);
	if (ret < 0) {
        dev_err(&client->dev, "%s: failed\n", __func__);
        return ret;
    }

	//tw9992_init_formats(sd, NULL);

	//tw9992_init_parameters(sd);

	sd->entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;
	sd->flags = V4L2_SUBDEV_FL_HAS_DEVNODE;
	sd->internal_ops = &tw9992_v4l2_internal_ops;
	sd->entity.ops = &tw9992_media_ops;

    // psw0523 add
    ret = tw9992_initialize_ctrls(state);
    if (ret < 0) {
        printk(KERN_ERR "%s: failed to initialize controls\n", __func__);
        return ret;
    }
    state->i2c_client = client;

#if defined(CONFIG_SLSIAP_BACKWARD_CAMERA)
    state->switch_dev.name = "rearcam_tw9992";
    switch_dev_register(&state->switch_dev);
    switch_set_state(&state->switch_dev, 0);

    INIT_DELAYED_WORK(&state->work, _work_handler);

    _state = state;
#endif


    return 0;
}

static int tw9992_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct tw9992_state *state =
		container_of(sd, struct tw9992_state, sd);

	v4l2_device_unregister_subdev(sd);
	mutex_destroy(&state->ctrl_lock);
	kfree(state);
	tw9992_power(0);
	dev_dbg(&client->dev, "Unloaded camera sensor TW9992.\n");

	return 0;
}

#ifdef CONFIG_PM
static int tw9992_suspend(struct i2c_client *client, pm_message_t state)
{
	int ret = 0;

	return ret;
}

static int tw9992_resume(struct i2c_client *client)
{
	int ret = 0;

	return ret;
}

#endif

static const struct i2c_device_id tw9992_id[] = {
	{ TW9992_DRIVER_NAME, 0 },
	{}
};

MODULE_DEVICE_TABLE(i2c, tw9992_id);

static struct i2c_driver tw9992_i2c_driver = {
	.probe = tw9992_probe,
	.remove = __devexit_p(tw9992_remove),
#ifdef CONFIG_PM
	.suspend = tw9992_suspend,
	.resume = tw9992_resume,
#endif

	.driver = {
		.name = TW9992_DRIVER_NAME,
		.owner = THIS_MODULE,
	},
	.id_table = tw9992_id,
};

static int __init tw9992_mod_init(void)
{
	return i2c_add_driver(&tw9992_i2c_driver);
}

static void __exit tw9992_mod_exit(void)
{
	i2c_del_driver(&tw9992_i2c_driver);
}

module_init(tw9992_mod_init);
module_exit(tw9992_mod_exit);

MODULE_DESCRIPTION("TW9992 Video driver");
MODULE_AUTHOR("<pjsin865@nexell.co.kr>");
MODULE_LICENSE("GPL");

