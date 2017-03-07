/*
 * drivers/media/video/socionext/m10mo.c --  Video Decoder driver for the m10mo
 *
 * Copyright(C) 2017. Nexell Co.,
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
#include <linux/gpio.h>

#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>
#include <linux/videodev2_exynos_camera.h>

#include <mach/platform.h>
#include <mach/devices.h>
#include <mach/soc.h>

#include "m10mo_video.h"
#include "m10mo_preset.h"

#ifdef CONFIG_PLAT_S5P4418_RETRO
extern bool is_mipi_camera_power_state(void);
#endif

#define DEFAULT_SENSOR_WIDTH			1280
#define DEFAULT_SENSOR_HEIGHT			720
#define DEFAULT_SENSOR_CODE				(V4L2_MBUS_FMT_YUYV8_2X8)

#define FORMAT_FLAGS_COMPRESSED			0x3
#define SENSOR_JPEG_SNAPSHOT_MEMSIZE	0x410580

#define DEFAULT_PIX_FMT					V4L2_PIX_FMT_UYVY	/* YUV422 */
#define DEFAULT_MCLK					27000000	/* 24000000 */
#define POLL_TIME_MS					10
#define CAPTURE_POLL_TIME_MS    		1000

#define THINE_I2C_RETRY_CNT				1


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

#define M10MO_VERSION_1_1	0x11

enum m10mo_hw_power {
	M10MO_HW_POWER_OFF,
	M10MO_HW_POWER_ON,
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

enum m10mo_oprmode {
	M10MO_OPRMODE_VIDEO = 0,
	M10MO_OPRMODE_IMAGE = 1,
	M10MO_OPRMODE_MAX,
};

struct m10mo_resolution {
	u8			value;
	enum m10mo_oprmode	type;
	u16			width;
	u16			height;
};

/* M5MOLS default format (codes, sizes, preset values) */
static struct v4l2_mbus_framefmt default_fmt[M10MO_OPRMODE_MAX] = {
	[M10MO_OPRMODE_VIDEO] = {
		.width		= DEFAULT_SENSOR_WIDTH,
		.height		= DEFAULT_SENSOR_HEIGHT,
		.code		= DEFAULT_SENSOR_CODE,
		.field		= V4L2_FIELD_NONE,
		.colorspace	= V4L2_COLORSPACE_JPEG,
	},
	[M10MO_OPRMODE_IMAGE] = {
		.width		= 1920,
		.height		= 1080,
		.code		= V4L2_MBUS_FMT_JPEG_1X8,
		.field		= V4L2_FIELD_NONE,
		.colorspace	= V4L2_COLORSPACE_JPEG,
	},
};

#define SIZE_DEFAULT_FFMT	ARRAY_SIZE(default_fmt)
enum m10mo_preview_frame_size {
	M10MO_PREVIEW_VGA,			/* 640x480 */
	M10MO_PREVIEW_D1,			/* 720x480 */
	M10MO_PREVIEW_MAX,
};

enum m10mo_capture_frame_size {
	M10MO_CAPTURE_VGA = 0,		/* 640x480 */
	M10MO_CAPTURE_D1,
	M10MO_CAPTURE_MAX,
};

/* make look-up table */
static const struct m10mo_resolution m10mo_resolutions[] = {
	{M10MO_PREVIEW_D1  	, M10MO_OPRMODE_VIDEO, DEFAULT_SENSOR_WIDTH,  DEFAULT_SENSOR_HEIGHT  },

	{M10MO_CAPTURE_D1  , M10MO_OPRMODE_IMAGE, DEFAULT_SENSOR_WIDTH, DEFAULT_SENSOR_HEIGHT   },
};

struct m10mo_framesize {
	u32 index;
	u32 width;
	u32 height;
};

static const struct m10mo_framesize m10mo_preview_framesize_list[] = {
	{M10MO_PREVIEW_D1, 	DEFAULT_SENSOR_WIDTH,		DEFAULT_SENSOR_HEIGHT}
};

static const struct m10mo_framesize m10mo_capture_framesize_list[] = {
	{M10MO_CAPTURE_D1,		DEFAULT_SENSOR_WIDTH,		DEFAULT_SENSOR_HEIGHT},
};

struct m10mo_version {
	u32 major;
	u32 minor;
};

struct m10mo_date_info {
	u32 year;
	u32 month;
	u32 date;
};

enum m10mo_runmode {
	M10MO_RUNMODE_NOTREADY,
	M10MO_RUNMODE_IDLE,
	M10MO_RUNMODE_RUNNING,
	M10MO_RUNMODE_CAPTURE,
};

struct m10mo_firmware {
	u32 addr;
	u32 size;
};

struct m10mo_jpeg_param {
	u32 enable;
	u32 quality;
	u32 main_size;		/* Main JPEG file size */
	u32 thumb_size;		/* Thumbnail file size */
	u32 main_offset;
	u32 thumb_offset;
	u32 postview_offset;
};

struct m10mo_position {
	int x;
	int y;
};

struct gps_info_common {
	u32 direction;
	u32 dgree;
	u32 minute;
	u32 second;
};

struct m10mo_gps_info {
	unsigned char gps_buf[8];
	unsigned char altitude_buf[4];
	int gps_timeStamp;
};

struct m10mo_regset {
	u32 size;
	u8 *data;
};

struct m10mo_regset_table {
	const u32	*reg;
	int		array_size;
};

#define M10MO_REGSET(x, y)		\
	[(x)] = {					\
		.reg		= (y),			\
		.array_size	= ARRAY_SIZE((y)),	\
}

#define M10MO_REGSET_TABLE(y)		\
	{					\
		.reg		= (y),			\
		.array_size	= ARRAY_SIZE((y)),	\
}

struct m10mo_regs {
	struct m10mo_regset_table ev[EV_MAX];
	struct m10mo_regset_table metering[METERING_MAX];
	struct m10mo_regset_table iso[ISO_MAX];
	struct m10mo_regset_table effect[V4L2_IMAGE_EFFECT_MAX];
	struct m10mo_regset_table white_balance[V4L2_WHITE_BALANCE_MAX];
	struct m10mo_regset_table preview_size[M10MO_PREVIEW_MAX];
	struct m10mo_regset_table capture_size[M10MO_CAPTURE_MAX];
	struct m10mo_regset_table scene_mode[V4L2_SCENE_MODE_MAX];
	struct m10mo_regset_table saturation[V4L2_SATURATION_MAX];
	struct m10mo_regset_table contrast[V4L2_CONTRAST_MAX];
	struct m10mo_regset_table sharpness[V4L2_SHARPNESS_MAX];
	struct m10mo_regset_table fps[FRAME_RATE_MAX];
	struct m10mo_regset_table preview_return;
	struct m10mo_regset_table jpeg_quality_high;
	struct m10mo_regset_table jpeg_quality_normal;
	struct m10mo_regset_table jpeg_quality_low;
	struct m10mo_regset_table flash_start;
	struct m10mo_regset_table flash_end;
	struct m10mo_regset_table af_assist_flash_start;
	struct m10mo_regset_table af_assist_flash_end;
	struct m10mo_regset_table af_low_light_mode_on;
	struct m10mo_regset_table af_low_light_mode_off;
	struct m10mo_regset_table aeawb_lockunlock[V4L2_AE_AWB_MAX];
	//struct m10mo_regset_table ae_awb_lock_on;
	//struct m10mo_regset_table ae_awb_lock_off;
	struct m10mo_regset_table low_cap_on;
	struct m10mo_regset_table low_cap_off;
	struct m10mo_regset_table wdr_on;
	struct m10mo_regset_table wdr_off;
	struct m10mo_regset_table face_detection_on;
	struct m10mo_regset_table face_detection_off;
	struct m10mo_regset_table capture_start;
	struct m10mo_regset_table af_macro_mode_1;
	struct m10mo_regset_table af_macro_mode_2;
	struct m10mo_regset_table af_macro_mode_3;
	struct m10mo_regset_table af_normal_mode_1;
	struct m10mo_regset_table af_normal_mode_2;
	struct m10mo_regset_table af_normal_mode_3;
	struct m10mo_regset_table af_return_macro_position;
	struct m10mo_regset_table single_af_start;
	struct m10mo_regset_table single_af_off_1;
	struct m10mo_regset_table single_af_off_2;
	struct m10mo_regset_table continuous_af_on;
	struct m10mo_regset_table continuous_af_off;
	struct m10mo_regset_table dtp_start;
	struct m10mo_regset_table dtp_stop;
	struct m10mo_regset_table init_reg_1;
	struct m10mo_regset_table init_reg_2;
	struct m10mo_regset_table init_reg_3;
	struct m10mo_regset_table init_reg_4;
	struct m10mo_regset_table flash_init;
	struct m10mo_regset_table reset_crop;
	struct m10mo_regset_table get_ae_stable_status;
	struct m10mo_regset_table get_light_level;
	struct m10mo_regset_table get_1st_af_search_status;
	struct m10mo_regset_table get_2nd_af_search_status;
	struct m10mo_regset_table get_capture_status;
	struct m10mo_regset_table get_esd_status;
	struct m10mo_regset_table get_iso;
	struct m10mo_regset_table get_shutterspeed;
	struct m10mo_regset_table get_frame_count;
};


struct m10mo_state {
	struct m10mo_platform_data 	*pdata;
	struct media_pad	 	pad; /* for media deivce pad */
	struct v4l2_subdev 		sd;
    struct switch_dev       switch_dev;

	struct exynos_md		*mdev; /* for media deivce entity */
	struct v4l2_pix_format		pix;
	struct v4l2_mbus_framefmt	ffmt[2]; /* for media deivce fmt */
	struct v4l2_fract		timeperframe;
	struct m10mo_jpeg_param	jpeg;
	struct m10mo_version		fw;
	struct m10mo_version		prm;
	struct m10mo_date_info	dateinfo;
	struct m10mo_position	position;
	struct v4l2_streamparm		strm;
	struct v4l2_streamparm		stored_parm;
	struct m10mo_gps_info	gps_info;
	struct mutex			ctrl_lock;
	struct completion		af_complete;
	enum m10mo_runmode		runmode;
	enum m10mo_oprmode		oprmode;
	enum af_operation_status	af_status;
	enum v4l2_mbus_pixelcode	code; /* for media deivce code */
    int					irq;
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
	const struct 			m10mo_regs *regs;

    struct i2c_client *i2c_client;
    struct v4l2_ctrl_handler handler;
    struct v4l2_ctrl *ctrl_mux;
    struct v4l2_ctrl *ctrl_status;

    /* standard control */
    struct v4l2_ctrl *ctrl_brightness;
    char brightness;

    /* nexell: detect worker */
    struct delayed_work work;

	unsigned int init_wait_q;
	wait_queue_head_t wait;
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

struct m10mo_state *_state = NULL;

static inline struct m10mo_state *ctrl_to_me(struct v4l2_ctrl *ctrl)
{
    return container_of(ctrl->handler, struct m10mo_state, handler);
}

#if 0
static int m10mo_i2c_read_byte(struct i2c_client *client, u8 addr, u8 *data)
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

	for (i=0; i<THINE_I2C_RETRY_CNT; i++) {
		ret = i2c_transfer(client->adapter, msg, 2);
		if (likely(ret == 2))
			break;
		//mdelay(POLL_TIME_MS);
		//dev_err(&client->dev, "\e[31mm10mo_i2c_write_byte failed reg:0x%02x retry:%d\e[0m\n", addr, i);
	}

	if (unlikely(ret != 2)) {
		dev_err(&client->dev, "\e[31mm10mo_i2c_read_byte failed reg:0x%02x \e[0m\n", addr);
		return -EIO;
	}

	*data = buf;
	return 0;
}

static int m10mo_i2c_write_byte(struct i2c_client *client, u8 addr, u8 val)
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

	for (i=0; i<THINE_I2C_RETRY_CNT; i++) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (likely(ret == 1))
			break;
		//mdelay(POLL_TIME_MS);
		//dev_err(&client->dev, "\e[31mm10mo_i2c_write_byte failed reg:0x%02x write:0x%04x, retry:%d\e[0m\n", addr, val, i);
	}

	if (ret != 1) {
		m10mo_i2c_read_byte(client, addr, &read_val);
		dev_err(&client->dev, "\e[31mm10mo_i2c_write_byte failed reg:0x%02x write:0x%04x, read:0x%04x, retry:%d\e[0m\n", addr, val, read_val, i);
		return -EIO;
	}

	return 0;
}
#endif


static int m10mo_i2c_read_block(struct i2c_client *client, u8 *buf, int size)
{
	s8 i = 0;
	s8 ret = 0;
	struct i2c_msg msg[2];

	msg[0].addr = client->addr;
	msg[0].flags = I2C_M_RD;
	msg[0].len = size+1;
	msg[0].buf = buf;

#if 0
	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = size;
	msg[1].buf = &buf;
#endif

	for (i=0; i<THINE_I2C_RETRY_CNT; i++) {
		ret = i2c_transfer(client->adapter, msg, 1);
		if (likely(ret == 2))
			break;
		//mdelay(POLL_TIME_MS);
		//dev_err(&client->dev, "\e[31mm10mo_i2c_write_byte failed reg:0x%02x retry:%d\e[0m\n", addr, i);
	}

#if 0
	if (unlikely(ret != 2)) {
		dev_err(&client->dev, "\e[31mm10mo_i2c_read_block failed \e[0m\n");
		return -EIO;
	}
#endif
	return 0;
}


static int m10mo_i2c_write_block(struct i2c_client *client, u8 *buf, int size)
{
	s8 i = 0;
	s8 ret = 0;
	struct i2c_msg msg;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = size;
	msg.buf = buf;

	for (i=0; i<THINE_I2C_RETRY_CNT; i++) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (likely(ret == 1))
			break;
		msleep(POLL_TIME_MS);
	}

	if (ret != 1) {
		for (i=0; i<size; i++)
			dev_err(&client->dev, "\e[31mm10mo_i2c_write_block failed\e[0m buff[%d]:0x%02x \n", i, buf[i]);
		return -EIO;
	}

	return 0;
}


/**
 * psw0523 add private controls
 */
#define V4L2_CID_MUX        (V4L2_CTRL_CLASS_USER | 0x1001)
#define V4L2_CID_STATUS     (V4L2_CTRL_CLASS_USER | 0x1002)

static int m10mo_set_mux(struct v4l2_ctrl *ctrl)
{
    return 0;
}

static int m10mo_get_status(struct v4l2_ctrl *ctrl)
{
    return 0;
}

static int m10mo_set_brightness(struct v4l2_ctrl *ctrl)
{
    return 0;
}

static int m10mo_s_ctrl(struct v4l2_ctrl *ctrl)
{
    switch (ctrl->id) {
    case V4L2_CID_MUX:
        return m10mo_set_mux(ctrl);
    case V4L2_CID_BRIGHTNESS:
        printk("%s: brightness\n", __func__);
        return m10mo_set_brightness(ctrl);
    default:
        printk(KERN_ERR "%s: invalid control id 0x%x\n", __func__, ctrl->id);
        return -EINVAL;
    }
}

static int m10mo_g_volatile_ctrl(struct v4l2_ctrl *ctrl)
{
    switch (ctrl->id) {
    case V4L2_CID_STATUS:
        return m10mo_get_status(ctrl);
    default:
        printk(KERN_ERR "%s: invalid control id 0x%x\n", __func__, ctrl->id);
        return -EINVAL;
    }
}

static const struct v4l2_ctrl_ops m10mo_ctrl_ops = {
     .s_ctrl = m10mo_s_ctrl,
     .g_volatile_ctrl = m10mo_g_volatile_ctrl,
};

static const struct v4l2_ctrl_config m10mo_custom_ctrls[] = {
    {
        .ops  = &m10mo_ctrl_ops,
        .id   = V4L2_CID_MUX,
        .type = V4L2_CTRL_TYPE_INTEGER,
        .name = "MuxControl",
        .min  = 0,
        .max  = 1,
        .def  = 1,
        .step = 1,
    },
    {
        .ops  = &m10mo_ctrl_ops,
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
static int m10mo_initialize_ctrls(struct m10mo_state *me)
{
    v4l2_ctrl_handler_init(&me->handler, NUM_CTRLS);

    me->ctrl_mux = v4l2_ctrl_new_custom(&me->handler, &m10mo_custom_ctrls[0], NULL);
    if (!me->ctrl_mux) {
         printk(KERN_ERR "%s: failed to v4l2_ctrl_new_custom for mux\n", __func__);
         return -ENOENT;
    }

    me->ctrl_status = v4l2_ctrl_new_custom(&me->handler, &m10mo_custom_ctrls[1], NULL);
    if (!me->ctrl_status) {
         printk(KERN_ERR "%s: failed to v4l2_ctrl_new_custom for status\n", __func__);
         return -ENOENT;
    }

    me->ctrl_brightness = v4l2_ctrl_new_std(&me->handler, &m10mo_ctrl_ops,
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

static int m10mo_set_from_table(struct v4l2_subdev *sd,
				const char *setting_name,
				const struct m10mo_regset_table *table,
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
	return 0;//m10mo_write_regs(sd, table->reg, table->array_size);
}

static int m10mo_set_parameter(struct v4l2_subdev *sd,
				int *current_value_ptr,
				int new_value,
				const char *setting_name,
				const struct m10mo_regset_table *table,
				int table_size)
{
	int err;
/*
	if (*current_value_ptr == new_value)
		return 0;
		*/

	err = m10mo_set_from_table(sd, setting_name, table,
				table_size, new_value);

	if (!err)
		*current_value_ptr = new_value;
	return err;
}

static void m10mo_init_parameters(struct v4l2_subdev *sd)
{
	struct m10mo_state *state =
		container_of(sd, struct m10mo_state, sd);
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
	/* m10mo_stop_auto_focus(sd); */
}

static void m10mo_set_framesize(struct v4l2_subdev *sd,
				const struct m10mo_framesize *frmsize,
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
static void m10mo_set_framesize(struct v4l2_subdev *sd,
				const struct m10mo_framesize *frmsize,
				int frmsize_count, bool preview)
{
	struct m10mo_state *state =
		container_of(sd, struct m10mo_state, sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	const struct m10mo_framesize *last_frmsize =
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

		err = m10mo_set_from_table(sd, "set preview size",
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

		err = m10mo_set_from_table(sd, "set capture size",
					state->regs->capture_size,
					ARRAY_SIZE(state->regs->capture_size),
					state->capture_framesize_index);

		if (err < 0) {
			v4l_info(client, "%s: register set failed\n", __func__);
		}

	}

}

/*
 * s_config subdev ops
 * With camera device, we need to re-initialize
 * every single opening time therefor,
 * it is not necessary to be initialized on probe time.
 * except for version checking
 * NOTE: version checking is optional
 */

static int m10mo_s_power(struct v4l2_subdev *sd, int on)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct m10mo_state *state = container_of(sd, struct m10mo_state, sd);
	int ret = 0;

	dev_err(&client->dev, "%s() on:%d \n", __func__, on);

    if (on) {
		m10mo_init_parameters(sd);
		if(state->power_on == M10MO_HW_POWER_OFF)
			state->power_on = M10MO_HW_POWER_ON;
	} else {
		state->power_on = M10MO_HW_POWER_OFF;
		state->initialized = false;
	}

	return ret;
}

static int m10mo_init(struct v4l2_subdev *sd, u32 val)
{
    struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct m10mo_state *state = container_of(sd, struct m10mo_state, sd);

    int ret = 0;
	u8 buffer[16];

	if (!state->initialized) {
		msleep(100);

	    dev_err(&client->dev, "%s: start\n", __func__);

		buffer[0] = 0x00;
		buffer[1] = 0x04;
		buffer[2] = 0x13;
		buffer[3] = 0x00;
		buffer[4] = 0x00;
		buffer[5] = 0x05;
		buffer[6] = 0x00;
		buffer[7] = 0x01;
		buffer[8] = 0x7F;
		m10mo_i2c_write_block(client, buffer, 9);

		buffer[0] = 0x05; // N+4
		buffer[1] = 0x02;
		buffer[2] = 0x0F; // category
		buffer[3] = 0x12; // byte
		buffer[4] = 0x01; // N : data1
		m10mo_i2c_write_block(client, buffer, 5);

		state->init_wait_q = false;
		if (wait_event_interruptible_timeout(state->wait, state->init_wait_q, msecs_to_jiffies(1000)) == 0) {
			printk(KERN_ERR "##[%s():%s:%d\t] time out!!! \n", __FUNCTION__, strrchr(__FILE__, '/')+1, __LINE__);
		}

		buffer[0] = 0x05; // N+4
		buffer[1] = 0x02;
		buffer[2] = 0x01; // category
		buffer[3] = 0x06; // byte
		buffer[4] = 0x00; // N : data1
		m10mo_i2c_write_block(client, buffer, 5);

		buffer[0] = 0x05; // N+4
		buffer[1] = 0x02;
		buffer[2] = 0x01; // category
		buffer[3] = 0x01; // byte
		buffer[4] = 0x21; // N : data1
		m10mo_i2c_write_block(client, buffer, 5);

		buffer[0] = 0x05; // N+4
		buffer[1] = 0x02;
		buffer[2] = 0x00; // category
		buffer[3] = 0x0B; // byte
		buffer[4] = 0x02; // N : data1
		m10mo_i2c_write_block(client, buffer, 5);

		state->init_wait_q = false;
		if (wait_event_interruptible_timeout(state->wait, state->init_wait_q, msecs_to_jiffies(1000)) == 0) {
			printk(KERN_ERR "##[%s():%s:%d\t] time out!!! \n", __FUNCTION__, strrchr(__FILE__, '/')+1, __LINE__);
		}

		state->initialized = true;
	}

    return ret;
}

static const struct v4l2_subdev_core_ops m10mo_core_ops = {
	.s_power		= m10mo_s_power,
	.init 			= m10mo_init,/* initializing API */
};


static int m10mo_s_mbus_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *fmt)
{
	struct m10mo_state *state = container_of(sd, struct m10mo_state, sd);
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

	if (state->oprmode == M10MO_OPRMODE_IMAGE) {
		state->oprmode = M10MO_OPRMODE_IMAGE;
		/*
		 * In case of image capture mode,
		 * if the given image resolution is not supported,
		 * use the next higher image resolution. */
		m10mo_set_framesize(sd, m10mo_capture_framesize_list,
				ARRAY_SIZE(m10mo_capture_framesize_list),
				false);

	} else {
		state->oprmode = M10MO_OPRMODE_VIDEO;
		/*
		 * In case of video mode,
		 * if the given video resolution is not matching, use
		 * the default rate (currently M10MO_PREVIEW_WVGA).
		 */
		m10mo_set_framesize(sd, m10mo_preview_framesize_list,
				ARRAY_SIZE(m10mo_preview_framesize_list),
				true);
	}

	state->jpeg.enable = state->pix.pixelformat == V4L2_PIX_FMT_JPEG;

	return 0;
}

static int m10mo_enum_framesizes(struct v4l2_subdev *sd,
				  struct v4l2_frmsizeenum *fsize)
{
	struct m10mo_state *state = container_of(sd, struct m10mo_state, sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	dev_err(&client->dev, "%s() wid=%d\t height=%d\n", __func__,state->pix.width,state->pix.height);

	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->discrete.width = state->pix.width;
	fsize->discrete.height = state->pix.height;

	return 0;
}

static int m10mo_g_parm(struct v4l2_subdev *sd,
			struct v4l2_streamparm *param)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct m10mo_state *state = container_of(sd, struct m10mo_state, sd);

	dev_err(&client->dev, "%s() \n", __func__);

	memcpy(param, &state->strm, sizeof(param));
	return 0;
}

static int m10mo_s_parm(struct v4l2_subdev *sd,
			struct v4l2_streamparm *param)
{
	int err = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct m10mo_state *state = container_of(sd, struct m10mo_state, sd);
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
	err = m10mo_set_parameter(sd, &parms->contrast, new_parms->contrast,
				"contrast", state->regs->contrast,
				ARRAY_SIZE(state->regs->contrast));
	err |= m10mo_set_parameter(sd, &parms->effects, new_parms->effects,
				"effect", state->regs->effect,
				ARRAY_SIZE(state->regs->effect));
	err |= m10mo_set_parameter(sd, &parms->brightness,
				new_parms->brightness, "brightness",
				state->regs->ev, ARRAY_SIZE(state->regs->ev));
///	err |= m10mo_set_flash_mode(sd, new_parms->flash_mode);
///	err |= m10mo_set_focus_mode(sd, new_parms->focus_mode);
	err |= m10mo_set_parameter(sd, &parms->iso, new_parms->iso,
				"iso", state->regs->iso,
				ARRAY_SIZE(state->regs->iso));
	err |= m10mo_set_parameter(sd, &parms->metering, new_parms->metering,
				"metering", state->regs->metering,
				ARRAY_SIZE(state->regs->metering));
	err |= m10mo_set_parameter(sd, &parms->saturation,
				new_parms->saturation, "saturation",
				state->regs->saturation,
				ARRAY_SIZE(state->regs->saturation));
	err |= m10mo_set_parameter(sd, &parms->scene_mode,
				new_parms->scene_mode, "scene_mode",
				state->regs->scene_mode,
				ARRAY_SIZE(state->regs->scene_mode));
	err |= m10mo_set_parameter(sd, &parms->sharpness,
				new_parms->sharpness, "sharpness",
				state->regs->sharpness,
				ARRAY_SIZE(state->regs->sharpness));
	err |= m10mo_set_parameter(sd, &parms->aeawb_lockunlock,
				new_parms->aeawb_lockunlock, "aeawb_lockunlock",
				state->regs->aeawb_lockunlock,
				ARRAY_SIZE(state->regs->aeawb_lockunlock));
	err |= m10mo_set_parameter(sd, &parms->white_balance,
				new_parms->white_balance, "white balance",
				state->regs->white_balance,
				ARRAY_SIZE(state->regs->white_balance));
	err |= m10mo_set_parameter(sd, &parms->fps,
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

static int m10mo_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct m10mo_state *state = container_of(sd, struct m10mo_state, sd);
	int ret = 0;

	dev_err(&client->dev, "%s() \n", __func__);

	if (enable) {
		//enable_irq(state->irq);
		state->runmode = M10MO_RUNMODE_RUNNING;
		m10mo_init(sd, enable);
	} else {
		state->runmode = M10MO_RUNMODE_NOTREADY;
		//disable_irq(state->irq);
	}
	return ret;
}

static const struct v4l2_subdev_video_ops m10mo_video_ops = {
	//.g_mbus_fmt 		= m10mo_g_mbus_fmt,
	.s_mbus_fmt 		= m10mo_s_mbus_fmt,
	.enum_framesizes 	= m10mo_enum_framesizes,
	///.enum_fmt 		= m10mo_enum_fmt,
	///.try_fmt 		= m10mo_try_fmt,
	.g_parm 		= m10mo_g_parm,
	.s_parm 		= m10mo_s_parm,
	.s_stream 		= m10mo_s_stream,
};

/**
 * __find_oprmode - Lookup M10MO resolution type according to pixel code
 * @code: pixel code
 */
static enum m10mo_oprmode __find_oprmode(enum v4l2_mbus_pixelcode code)
{
	enum m10mo_oprmode type = M10MO_OPRMODE_VIDEO;

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
			     enum m10mo_oprmode *type,
			     u32 *resolution)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	const struct m10mo_resolution *fsize = &m10mo_resolutions[0];
	const struct m10mo_resolution *match = NULL;
	enum m10mo_oprmode stype = __find_oprmode(mf->code);
	int i = ARRAY_SIZE(m10mo_resolutions);
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

static struct v4l2_mbus_framefmt *__find_format(struct m10mo_state *state,
				struct v4l2_subdev_fh *fh,
				enum v4l2_subdev_format_whence which,
				enum m10mo_oprmode type)
{
	if (which == V4L2_SUBDEV_FORMAT_TRY)
		return fh ? v4l2_subdev_get_try_format(fh, 0) : NULL;

	return &state->ffmt[type];
}

/* enum code by flite video device command */
static int m10mo_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_fh *fh,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	if (!code || code->index >= SIZE_DEFAULT_FFMT)
		return -EINVAL;

	code->code = default_fmt[code->index].code;

	return 0;
}

/* get format by flite video device command */
static int m10mo_get_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
			  struct v4l2_subdev_format *fmt)
{
	struct m10mo_state *state =
		container_of(sd, struct m10mo_state, sd);
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
static int m10mo_set_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
			  struct v4l2_subdev_format *fmt)
{
	struct m10mo_state *state =
		container_of(sd, struct m10mo_state, sd);
	struct v4l2_mbus_framefmt *format = &fmt->format;
	struct v4l2_mbus_framefmt *sfmt;
	enum m10mo_oprmode type;
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
#ifndef CONFIG_VIDEO_M10MO_SENSOR_JPEG
		state->ffmt[type].code 		= V4L2_MBUS_FMT_YUYV8_2X8;
#else
		state->ffmt[type].code 		= format->code;
#endif

		/* find adaptable resolution */
		state->resolution 		= resolution;
#ifndef CONFIG_VIDEO_M10MO_SENSOR_JPEG
		state->code 			= V4L2_MBUS_FMT_YUYV8_2X8;
#else
		state->code 			= format->code;
#endif
		state->res_type 		= type;

		/* for set foramat */
		state->pix.width 		= format->width;
		state->pix.height 		= format->height;

		if (state->power_on == M10MO_HW_POWER_ON)
			m10mo_s_mbus_fmt(sd, sfmt);  /* set format */
	}

	return 0;
}


static struct v4l2_subdev_pad_ops m10mo_pad_ops = {
	.enum_mbus_code	= m10mo_enum_mbus_code,
	.get_fmt		= m10mo_get_fmt,
	.set_fmt		= m10mo_set_fmt,
};

static const struct v4l2_subdev_ops m10mo_ops = {
	.core = &m10mo_core_ops,
	.video = &m10mo_video_ops,
	.pad	= &m10mo_pad_ops,
};

static int m10mo_link_setup(struct media_entity *entity,
			    const struct media_pad *local,
			    const struct media_pad *remote, u32 flags)
{
	printk("%s\n", __func__);
	return 0;
}

static const struct media_entity_operations m10mo_media_ops = {
	.link_setup = m10mo_link_setup,
};

/* internal ops for media controller */
static int m10mo_init_formats(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
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

static int m10mo_subdev_close(struct v4l2_subdev *sd,
			      struct v4l2_subdev_fh *fh)
{
	return 0;
}

static int m10mo_subdev_registered(struct v4l2_subdev *sd)
{
	return 0;
}

static void m10mo_subdev_unregistered(struct v4l2_subdev *sd)
{
	return;
}

static const struct v4l2_subdev_internal_ops m10mo_v4l2_internal_ops = {
	.open = m10mo_init_formats,
	.close = m10mo_subdev_close,
	.registered = m10mo_subdev_registered,
	.unregistered = m10mo_subdev_unregistered,
};

static ssize_t m10mo_sys_fw_update(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct m10mo_state *info = i2c_get_clientdata(client);

	int result = 0;
	u8 data[255];
	int ret = 0;

	dev_dbg(&info->i2c_client->dev, "%s [START]\n", __func__);

	//Update firmware
	//ret = m10mo_fw_update_from_storage(info, info->fw_path_ext, true);

	switch (ret) {
	case 0:
		sprintf(data, "F/W update success.\n");
		break;

	default:
		sprintf(data, "F/W update failed.\n");
		break;
	}

	dev_dbg(&info->i2c_client->dev, "%s [DONE]\n", __func__);

	result = snprintf(buf, 255, "%s\n", data);
	return result;
}
static DEVICE_ATTR(fw_update, S_IRUGO, m10mo_sys_fw_update, NULL);

static struct attribute *m10mo_attrs[] = {
	&dev_attr_fw_update.attr,
	NULL,
};

static const struct attribute_group m10mo_attr_group = {
	.attrs = m10mo_attrs,
};

static irqreturn_t m10mo_irq_isr(int irq, void *data)
{
	struct m10mo_state *state = data;
	struct i2c_client *client = state->i2c_client;

	u8 buffer[5] = {0,};
	u8 read_buffer[128] = {0,};
	u8 read_buf[32] = {0,};
	int i;

	if (state->runmode == M10MO_RUNMODE_NOTREADY)
	{
		buffer[0] = 0x05;
		buffer[1] = 0x01;
		buffer[2] = 0x0F; // category
		buffer[3] = 0x0C; // byte
		buffer[4] = 0x04; // N

		m10mo_i2c_write_block(client, buffer, 5);
		m10mo_i2c_read_block(client, read_buffer, 5);

		for (i=0; i<5; i++)
			printk(KERN_ERR "## [%s():%s:%d\t] read_buffer[%d]:0x%x \n", __FUNCTION__, strrchr(__FILE__, '/')+1, __LINE__, i, read_buffer[i]);

	} else if (state->runmode == M10MO_RUNMODE_RUNNING) {

		buffer[0] = 0x05;
		buffer[1] = 0x01;
		buffer[2] = 0x00; // category
		buffer[3] = 0x1C; // byte
		buffer[4] = 0x01; // N

		m10mo_i2c_write_block(client, buffer, 5);
		m10mo_i2c_read_block(client, read_buf, 0x02);
	}

	state->init_wait_q = true;
	wake_up_interruptible(&state->wait);

    return IRQ_HANDLED;
}

/*
 * m10mo_probe
 * Fetching platform data is being done with s_config subdev call.
 * In probe routine, we just register subdev device
 */
static int m10mo_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct m10mo_state *state;
	int ret = 0;

	state = kzalloc(sizeof(struct m10mo_state), GFP_KERNEL);
	if (state == NULL)
		return -ENOMEM;

	mutex_init(&state->ctrl_lock);
	init_completion(&state->af_complete);

	state->power_on = M10MO_HW_POWER_OFF;

	state->runmode = M10MO_RUNMODE_NOTREADY;
	sd = &state->sd;
	strcpy(sd->name, M10MO_DRIVER_NAME);

	v4l2_i2c_subdev_init(sd, client, &m10mo_ops);
	state->pad.flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_init(&sd->entity, 1, &state->pad, 0);
	if (ret < 0) {
        dev_err(&client->dev, "%s: failed\n", __func__);
        return ret;
    }

	//m10mo_init_formats(sd, NULL);
	//m10mo_init_parameters(sd);

	sd->entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;
	sd->flags = V4L2_SUBDEV_FL_HAS_DEVNODE;
	sd->internal_ops = &m10mo_v4l2_internal_ops;
	sd->entity.ops = &m10mo_media_ops;

    ret = m10mo_initialize_ctrls(state);
    if (ret < 0) {
        printk(KERN_ERR "%s: failed to initialize controls\n", __func__);
        return ret;
    }

    state->i2c_client = client;
	state->irq = gpio_to_irq(CFG_IO_ISP_SINT0);

    ret = request_threaded_irq(state->irq, NULL, m10mo_irq_isr,
						/*IRQF_TRIGGER_FALLING |*/ IRQF_TRIGGER_RISING,
						"m10mo", state);
    if (ret<0) {
        pr_err("%s: failed to request_irq(irqnum %d), ret : %d\n", __func__, CFG_IO_ISP_SINT0, ret);
        return -1;
    }
	//disable_irq(state->irq);

	state->init_wait_q = false;
	init_waitqueue_head(&state->wait);

	//Create sysfs
	if (sysfs_create_group(&client->dev.kobj, &m10mo_attr_group)) {
		dev_err(&client->dev, "%s [ERROR] sysfs_create_group\n", __func__);
	}
	if (sysfs_create_link(NULL, &client->dev.kobj, "m10mo")) {
		dev_err(&client->dev, "%s [ERROR] sysfs_create_link\n", __func__);
	}

    return 0;
}

static int m10mo_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct m10mo_state *state =
		container_of(sd, struct m10mo_state, sd);

	v4l2_device_unregister_subdev(sd);
	mutex_destroy(&state->ctrl_lock);
	kfree(state);
	dev_dbg(&client->dev, "Unloaded camera sensor M10MO.\n");

	return 0;
}

#ifdef CONFIG_PM
static int m10mo_suspend(struct i2c_client *client, pm_message_t state)
{
	int ret = 0;

	return ret;
}

static int m10mo_resume(struct i2c_client *client)
{
	int ret = 0;

	return ret;
}

#endif

static const struct i2c_device_id m10mo_id[] = {
	{ M10MO_DRIVER_NAME, 0 },
	{}
};

MODULE_DEVICE_TABLE(i2c, m10mo_id);

static struct i2c_driver m10mo_i2c_driver = {
	.probe = m10mo_probe,
	.remove = __devexit_p(m10mo_remove),
#ifdef CONFIG_PM
	.suspend = m10mo_suspend,
	.resume = m10mo_resume,
#endif

	.driver = {
		.name = M10MO_DRIVER_NAME,
		.owner = THIS_MODULE,
	},
	.id_table = m10mo_id,
};

static int __init m10mo_mod_init(void)
{
	return i2c_add_driver(&m10mo_i2c_driver);
}

static void __exit m10mo_mod_exit(void)
{
	i2c_del_driver(&m10mo_i2c_driver);
}

module_init(m10mo_mod_init);
module_exit(m10mo_mod_exit);

MODULE_DESCRIPTION("M10MO Video driver");
MODULE_AUTHOR(" ");
MODULE_LICENSE("GPL");

