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
#include <linux/firmware.h>

#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>
#include <linux/videodev2_exynos_camera.h>

#include <mach/platform.h>
#include <mach/devices.h>
#include <mach/soc.h>

#include "m10mo.h"
#include "m10mo_video.h"
#include "m10mo_preset.h"

#ifdef CONFIG_PLAT_S5P4418_RETRO
extern int mipi_camera_power_enable(bool on);
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
	M10MO_FIRMWAREMODE,
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

	struct jc_isp isp;

	int op_mode;
	int dtp_mode;
	int cam_mode;
	int vtcall_mode;
	int started;
	int flash_mode;
	int lowLight;
	int dtpTest;
	int af_mode;
	//int af_status;
	int preview_size;
	int capture_size;
	unsigned int lux;
	int awb_mode;
	int samsungapp;
	u8 sensor_ver[10];
	u8 phone_ver[10];
	u8 isp_ver[10];
	u8 sensor_ver_str[12];
	u8 isp_ver_str[12];
	u8 phone_ver_str[12];
	u8 sensor_type[25];
	bool burst_mode;
	bool stream_on;
	bool touch_af_mode;
	bool hw_vdis_mode;
	bool torch_mode;
	bool fw_update;
	int system_rev;
	bool movie_mode;
	int shot_mode;
	bool anti_stream_off;
	int sensor_combination;
	bool need_restart_caf;
	bool is_isp_null;
	bool isp_null_read_sensor_fw;
	bool samsung_app;
	bool factory_bin;
	int fw_retry_cnt;
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
	msg[1].flags = I2C_M_RD;// | I2C_M_NO_RD_ACK;
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
	msg[0].len = size + 1;
	msg[0].buf = buf;

#if 0
	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;// | I2C_M_NO_RD_ACK;
	msg[1].len = size;
	msg[1].buf = &buf;
#endif

	for (i=0; i<THINE_I2C_RETRY_CNT; i++) {
		ret = i2c_transfer(client->adapter, msg, 1);
		if (likely(ret == 1))
			break;
		mdelay(POLL_TIME_MS);
		dev_err(&client->dev, "\e[31m m10mo_i2c_read_block failed, retry:%d\e[0m\n", i);
	}

#if 1
	if (ret != 1) {
		dev_err(&client->dev, "\e[31m m10mo_i2c_read_block failed \e[0m\n");
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

#define jc_readb(g, b, v) jc_read(__LINE__, 1, g, b, v, true)
#define jc_readw(g, b, v) jc_read(__LINE__, 2, g, b, v, true)
#define jc_readl(g, b, v) jc_read(__LINE__, 4, g, b, v, true)

#define jc_writeb(g, b, v) jc_write(__LINE__, 1, g, b, v, true)
#define jc_writew(g, b, v) jc_write(__LINE__, 2, g, b, v, true)
#define jc_writel(g, b, v) jc_write(__LINE__, 4, g, b, v, true)

#define jc_readb2(g, b, v) jc_read(__LINE__, 1, g, b, v, false)
#define jc_readw2(g, b, v) jc_read(__LINE__, 2, g, b, v, false)
#define jc_readl2(g, b, v) jc_read(__LINE__, 4, g, b, v, false)

#define jc_writeb2(g, b, v) jc_write(__LINE__, 1, g, b, v, false)
#define jc_writew2(g, b, v) jc_write(__LINE__, 2, g, b, v, false)
#define jc_writel2(g, b, v) jc_write(__LINE__, 4, g, b, v, false)

static struct m10mo_state *jc_ctrl = NULL;
static struct i2c_client *jc_client;
static struct device jc_dev;

#if JC_LOAD_FW_MAIN
bool firmware_update;
bool firmware_update_sdcard;
bool version_checked;
int isp_version = -1;

bool firmware_update_sdcard = false;
bool version_checked = false;
#endif

#if JC_CHECK_FW
static u8 sysfs_sensor_fw[10] = {0,};
static u8 sysfs_phone_fw[10] = {0,};
static u8 sysfs_sensor_fw_str[12] = {0,};
static u8 sysfs_isp_fw_str[12] = {0,};
static u8 sysfs_phone_fw_str[12] = {0,};
static u8 sysfs_sensor_type[25] = {0,};
#endif

static u32 jc_wait_interrupt(unsigned int timeout);
static int jc_check_chip_erase(void);
static int jc_set_sizes(void);


static int jc_read(int _line, u8 len,
	u8 category, u8 byte, int *val, bool log)
{
	struct i2c_msg msg;
	unsigned char data[5];
	unsigned char recv_data[len + 1];
	int i, err = 0;

	if (!jc_client->adapter)
		return -ENODEV;

	if (len != 0x01 && len != 0x02 && len != 0x04)
		return -EINVAL;

	msg.addr = jc_client->addr;
	msg.flags = 0;
	msg.len = sizeof(data);
	msg.buf = data;

	/* high byte goes out first */
	data[0] = msg.len;
	data[1] = 0x01;			/* Read category parameters */
	data[2] = category;
	data[3] = byte;
	data[4] = len;

	for (i = JC_I2C_RETRY; i; i--) {
		err = i2c_transfer(jc_client->adapter, &msg, 1);
		if (err == 1)
			break;
		msleep(20);
	}

	if (err != 1) {
		cam_err("RD category %#x, byte %#x, err %d\n",
			category, byte, err);
		return err;
	}

	msg.flags = I2C_M_RD;// | I2C_M_NO_RD_ACK;
	msg.len = sizeof(recv_data);
	msg.buf = recv_data;
	for (i = JC_I2C_RETRY; i; i--) {
		err = i2c_transfer(jc_client->adapter, &msg, 1);
		if (err == 1)
			break;
		msleep(20);
	}

	if (err != 1) {
		cam_err("RD category %#x, byte %#x, err %d\n",
			category, byte, err);
		return err;
	}

	if (recv_data[0] != sizeof(recv_data))
		cam_i2c_dbg("expected length %d, but return length %d\n",
				 sizeof(recv_data), recv_data[0]);

	if (len == 0x01)
		*val = recv_data[1];
	else if (len == 0x02)
		*val = recv_data[1] << 8 | recv_data[2];
	else
		*val = recv_data[1] << 24 | recv_data[2] << 16 |
				recv_data[3] << 8 | recv_data[4];

	if (log)
		cam_i2c_dbg("[ %4d ] Read %s %#02x, byte %#x, value %#x\n",
			_line, (len == 4 ? "L" : (len == 2 ? "W" : "B")),
			category, byte, *val);

	return err;
}

static int jc_write(int _line, u8 len,
	u8 category, u8 byte, int val, bool log)
{
	struct i2c_msg msg;
	unsigned char data[len + 4];
	int i, err;

	if (!jc_client->adapter)
		return -ENODEV;

	if (len != 0x01 && len != 0x02 && len != 0x04)
		return -EINVAL;

	msg.addr = jc_client->addr;
	msg.flags = 0;
	msg.len = sizeof(data);
	msg.buf = data;

	data[0] = msg.len;
	data[1] = 0x02;			/* Write category parameters */
	data[2] = category;
	data[3] = byte;
	if (len == 0x01) {
		data[4] = (val & 0xFF);
	} else if (len == 0x02) {
		data[4] = ((val >> 8) & 0xFF);
		data[5] = (val & 0xFF);
	} else {
		data[4] = ((val >> 24) & 0xFF);
		data[5] = ((val >> 16) & 0xFF);
		data[6] = ((val >> 8) & 0xFF);
		data[7] = (val & 0xFF);
	}

	if (log)
		cam_i2c_dbg("[ %4d ] Write %s %#x, byte %#x, value %#x\n",
			_line, (len == 4 ? "L" : (len == 2 ? "W" : "B")),
			category, byte, val);

	for (i = JC_I2C_RETRY; i; i--) {
		err = i2c_transfer(jc_client->adapter, &msg, 1);
		if (err == 1)
			break;
		msleep(20);
	}

	if (err != 1) {
		cam_err("WR category %#x, byte %#x, err %d\n",
			category, byte, err);
		return err;
	}

	return err;
}

static int jc_verify_writedata(unsigned char category,
					unsigned char byte, u32 value)
{
	u32 val = 0;
	unsigned char i;

	for (i = 0; i < JC_I2C_VERIFY; i++) {
		jc_readb(category, byte, &val);
		if (val == value) {
			CAM_DEBUG("### Read %#x, byte %#x, value %#x "\
				"(try = %d)\n", category, byte, value, i);
			return 0;
		}
		msleep(20);/*20ms*/
	}

	cam_err("Failed !!");
	return -EBUSY;
}

static int jc_set_mode(u32 mode)
{
	int i, err;
	u32 old_mode, val;
	u32 int_factor;

	err = jc_readb(JC_CATEGORY_SYS, JC_SYS_MODE, &old_mode);
	if (err < 0)
		return err;

	if (old_mode == mode) {
		cam_info("Same mode : %d\n", mode);
		return old_mode;
	}

	CAM_DEBUG("E: ### %#x -> %#x", old_mode, mode);

	switch (mode) {
	case JC_SYSINIT_MODE:
		cam_info("sensor is initializing\n");
		err = -EBUSY;
		break;

	case JC_PARMSET_MODE:
		cam_info("stop AF\n");
		jc_writeb(JC_CATEGORY_LENS,
				0x02, 0x00);
		msleep(30);
		err = jc_writeb(JC_CATEGORY_SYS, JC_SYS_MODE, mode);
		int_factor = jc_wait_interrupt(JC_ISP_TIMEOUT);
		if (!(int_factor & JC_INT_MODE)) {
			cam_err("parameter mode interrupt fail %#x\n",
				int_factor);
			return -ENOSYS;
		}
		jc_ctrl->need_restart_caf = true;
		jc_ctrl->stream_on = false;
		break;

	case JC_MONITOR_MODE:
		err = jc_writeb(JC_CATEGORY_SYS, JC_SYS_MODE, mode);

		int_factor = jc_wait_interrupt(JC_ISP_TIMEOUT);
		if (!(int_factor & JC_INT_MODE)) {
			cam_err("monitor mode interrupt fail %#x\n",
				int_factor);
			return -ENOSYS;
		}
		jc_ctrl->stream_on = true;
		if (jc_ctrl->need_restart_caf == true) {
			jc_ctrl->need_restart_caf = false;
			if (jc_ctrl->shot_mode == 4 ||jc_ctrl->shot_mode == 15) { //drama, cine photo
				cam_info("Restart auto focus\n");
				if (jc_ctrl->af_mode == 3) {
					cam_info("start CAF\n");
					jc_writeb(JC_CATEGORY_LENS,
							0x01, 0x03);
					jc_writeb(JC_CATEGORY_LENS,
							0x02, 0x01);
				} else if (jc_ctrl->af_mode == 4) {
					cam_info("start macro CAF\n");
					jc_writeb(JC_CATEGORY_LENS,
							0x01, 0x07);
					jc_writeb(JC_CATEGORY_LENS,
							0x02, 0x01);
				} else if (jc_ctrl->af_mode == 5) {
					msleep(50);
					cam_info("start Movie CAF\n");
					jc_writeb(JC_CATEGORY_LENS,
							0x01, 0x04);
					jc_writeb(JC_CATEGORY_LENS,
							0x02, 0x01);
				} else if (jc_ctrl->af_mode == 6) {
					msleep(50);
					cam_info("FD CAF\n");
					jc_writeb(JC_CATEGORY_LENS,
							0x01, 0x05);
					jc_writeb(JC_CATEGORY_LENS,
							0x02, 0x01);
				}
			}
		}
		break;

	default:
		cam_err("current mode is unknown, %d\n", mode);
		err = 0;/* -EINVAL; */
	}

	if (err < 0)
		return err;

	for (i = JC_I2C_VERIFY; i; i--) {
		err = jc_readb(JC_CATEGORY_SYS, JC_SYS_MODE, &val);
		if (val == mode)
			break;
		msleep(20);/*20ms*/
	}

	CAM_DEBUG("X");
	return mode;
}

void jc_set_preview(void)
{
	CAM_DEBUG("E");

	CAM_DEBUG("X");
}

void jc_set_capture(void)
{
	CAM_DEBUG("E");

	CAM_DEBUG("X");
}

static int jc_set_capture_size(int width, int height)
{
	u32 isp_mode;
	int err;

	CAM_DEBUG("Entered, width %d, height %d\n", width, height);

	if (width == 4128 && height == 3096)
		jc_ctrl->capture_size = 0x2C;
	else if (width == 4128 && height == 2322)
		jc_ctrl->capture_size = 0x2B;
	else if (width == 4096 && height == 3072)
		jc_ctrl->capture_size = 0x38;
	else if (width == 4096 && height == 2304)
		jc_ctrl->capture_size = 0x37;
	else if (width == 3264 && height == 2448)
		jc_ctrl->capture_size = 0x25;
	else if (width == 3200 && height == 1920)
		jc_ctrl->capture_size = 0x24;
	else if (width == 3264 && height == 1836)
		jc_ctrl->capture_size = 0x21;
	else if (width == 2560 && height == 1920)
		jc_ctrl->capture_size = 0x1F;
	else if (width == 2048 && height == 1536)
		jc_ctrl->capture_size = 0x1B;
	else if (width == 2048 && height == 1152)
		jc_ctrl->capture_size = 0x1A;
	else if (width == 1280 && height == 720)
		jc_ctrl->capture_size = 0x10;
	else if (width == 640 && height == 480)
		jc_ctrl->capture_size = 0X09;
	else
		jc_ctrl->capture_size = 0x2C;

	err = jc_readb(JC_CATEGORY_SYS, JC_SYS_MODE, &isp_mode);

	if (err < 0) {
		CAM_DEBUG("isp mode check error : %d", err);
		return err;
	}

	if( isp_mode == JC_MONITOR_MODE ) {
		jc_set_sizes();

		err = jc_readb(JC_CATEGORY_SYS, JC_SYS_MODE, &isp_mode);

		if (err < 0) {
			CAM_DEBUG("isp mode check error : %d", err);
			return err;
		}

		if( isp_mode == JC_PARMSET_MODE ) {
			jc_set_mode(JC_MONITOR_MODE);
		}
	}
	return 0;
}

static int jc_set_preview_size(int32_t width, int32_t height)
{
	CAM_DEBUG("Entered, width %d, height %d\n", width, height);

	if (width == 1728 && height == 1296)
		jc_ctrl->preview_size = 0x40;
	else if (width == 384 && height == 288)
		jc_ctrl->preview_size = 0x3F;
	else if (width == 768 && height == 576)
		jc_ctrl->preview_size = 0x3E;
	else if (width == 864 && height == 576)
		jc_ctrl->preview_size = 0x3D;
	else if (width == 1536 && height == 864)
		jc_ctrl->preview_size = 0x3C;
	else if (width == 2304 && height == 1296)
		jc_ctrl->preview_size = 0x3B;
	else if (width == 1440 && height == 1080)
		jc_ctrl->preview_size = 0x37;
	else if (width == 960 && height == 720)
		jc_ctrl->preview_size = 0x34;
	else if (width == 1920 && height == 1080)
		jc_ctrl->preview_size = 0x28;
	else if (width == 1152 && height == 864)
		jc_ctrl->preview_size = 0x23;
	else if (width == 1280 && height == 720)
		jc_ctrl->preview_size = 0x21;
	else if (width == 720 && height == 480)
		jc_ctrl->preview_size = 0x18;
	else if (width == 640 && height == 480)
		jc_ctrl->preview_size = 0x17;
	else if (width == 320 && height == 240)
		jc_ctrl->preview_size = 0x09;
	else if (width == 176 && height == 144)
		jc_ctrl->preview_size = 0x05;
	else if (width == 4128 && height == 3096)
		jc_ctrl->preview_size = 0x27;
	else if (width == 800 && height == 480)
		jc_ctrl->preview_size = 0x41;
	else if (width == 800 && height == 450)
		jc_ctrl->preview_size = 0x44;
	else if (width == 3264 && height == 1836)
		jc_ctrl->preview_size = 0x43;
	else if (width == 1056 && height == 864)
		jc_ctrl->preview_size = 0x47;
	else
		jc_ctrl->preview_size = 0x17;

	return 0;
}

static int jc_set_sizes(void)
{
	int prev_preview_size, prev_capture_size;
	CAM_DEBUG("Entered");

	jc_readb(JC_CATEGORY_PARM,
			JC_PARM_MON_SIZE, &prev_preview_size);
	jc_readb(JC_CATEGORY_CAPPARM,
			JC_CAPPARM_MAIN_IMG_SIZE, &prev_capture_size);

	/* set monitor(preview) size */
	if (jc_ctrl->preview_size != prev_preview_size) {
		cam_info("### set monitor(preview) size : 0x%x\n",
				 jc_ctrl->preview_size);

		/* change to parameter mode */
		jc_set_mode(JC_PARMSET_MODE);

		jc_writeb(JC_CATEGORY_PARM,
				JC_PARM_MON_SIZE,
				jc_ctrl->preview_size);

		if (jc_ctrl->preview_size == 0x43 ||
			jc_ctrl->preview_size == 0x44 || jc_ctrl->preview_size == 0x45) {
			msleep(10);
			jc_writeb(JC_CATEGORY_CAPCTRL, 0x0, 0x0);
			msleep(10);
			jc_writeb(0x1, 0x5, 0x1);
			msleep(10);
			jc_writeb(0x1, 0x6, 0x1);
		} else
			jc_writeb(JC_CATEGORY_CAPCTRL, 0x0, 0x0f);
		jc_verify_writedata(JC_CATEGORY_PARM,
				JC_PARM_MON_SIZE,
				jc_ctrl->preview_size);
	} else
		cam_info("### preview size same as previous size : " \
					"0x%x\n", prev_preview_size);

	/* set capture size */
	if (jc_ctrl->capture_size != prev_capture_size) {
		cam_info("### set capture size : 0x%x\n",
				 jc_ctrl->capture_size);
		jc_writeb(JC_CATEGORY_CAPPARM,
				JC_CAPPARM_MAIN_IMG_SIZE,
				jc_ctrl->capture_size);
		jc_verify_writedata(JC_CATEGORY_CAPPARM,
				JC_CAPPARM_MAIN_IMG_SIZE,
				jc_ctrl->capture_size);
	} else
		cam_info("### capture size same as previous size : " \
					"0x%x\n", prev_capture_size);
	return 0;
}

static int jc_set_snapshot_mode(int mode)
{
	int32_t rc = 0;
	int val = -1;
	int retries = 0;

	cam_info("Entered, shot mode %d\n", mode);

	if (jc_ctrl->burst_mode == true
		&& mode == 1) {
		cam_info("Now Burst shot capturing");
		return rc;
	}

	if (mode == 0) {
		cam_info("Single Capture");
		do {
			jc_readb(JC_CATEGORY_CAPCTRL,
				        0x1F, &val);
			cam_info("capture status : %d", val);
		} while (val != 0 && retries++ < JC_I2C_VERIFY);
		if ((jc_ctrl->shot_mode == 0 ||jc_ctrl->shot_mode == 1)
			&& (jc_ctrl->movie_mode == false)){
			cam_info("1Sec Burst");
			jc_writeb(JC_CATEGORY_CAPCTRL,
					JC_CAPCTRL_START_DUALCAP, 0x07);
		} else {
			jc_writeb(JC_CATEGORY_CAPCTRL,
					JC_CAPCTRL_START_DUALCAP, 0x01);
		}
	} else if (mode == 1) {
		cam_info("Burst Capture On");
		jc_ctrl->burst_mode = true;
		jc_writeb(JC_CATEGORY_CAPCTRL,
				JC_CAPCTRL_START_DUALCAP, 0x04);
	} else if (mode == 2) {
		cam_info("Burst Capture Off");
		jc_ctrl->burst_mode = false;
		jc_writeb(JC_CATEGORY_CAPCTRL,
				JC_CAPCTRL_START_DUALCAP, 0x05);
	}
	return rc;
}

static int jc_set_zoom(int level)
{
	int32_t rc = 0;

	cam_info("Entered, zoom %d\n", level);

	if (level < 1 || level > 31) {
		cam_err("invalid value, %d\n", level);
		return rc;
	}
	jc_writeb(JC_CATEGORY_MON,
			JC_MON_ZOOM, level);
	return rc;
}

static int jc_set_autofocus(int status)
{
	int32_t rc = 0;

	cam_info("Entered, auto focus %d\n", status);

	if (status < 0 || status > 1) {
		cam_err("invalid value, %d\n", status);
		return rc;
	}
	jc_writeb(JC_CATEGORY_LENS,
			0x02, status);
	return rc;
}

static int jc_set_af_mode(int status)
{
	int32_t rc = 0;

	cam_info("Entered, af mode %d\n", status);

	if (status < 1 || status > 6) {
		cam_err("invalid value, %d\n", status);
		return rc;
	}
	jc_ctrl->af_mode = status;

	if (jc_ctrl->touch_af_mode == true
		&& (status == 1 || status == 2)) {
		cam_info("Touch af focus\n");
		jc_ctrl->touch_af_mode = false;
		return rc;
	}
	jc_ctrl->touch_af_mode = false;

	if (status == 1) {
		cam_info("Macro focus\n");
		jc_writeb(JC_CATEGORY_LENS,
				0x01, 0x01);
	} else if (status == 2) {
		cam_info("Auto focus\n");
		jc_writeb(JC_CATEGORY_LENS,
				0x01, 0x00);
	} else if (status == 3) {
		cam_info("CAF\n");
		jc_writeb(JC_CATEGORY_LENS,
				0x01, 0x03);
		if (jc_ctrl->stream_on == true) {
			cam_info("start CAF\n");
			jc_writeb(JC_CATEGORY_LENS,
					0x02, 0x01);
		}
	} else if (status == 4) {
		cam_info("Macro CAF\n");
		jc_writeb(JC_CATEGORY_LENS,
				0x01, 0x07);
		if (jc_ctrl->stream_on == true) {
			cam_info("start Macro CAF\n");
			jc_writeb(JC_CATEGORY_LENS,
					0x02, 0x01);
		}
	} else if (status == 5) {
		cam_info("Movie CAF\n");
		jc_writeb(JC_CATEGORY_LENS,
				0x01, 0x04);
		msleep(50);
		if (jc_ctrl->stream_on == true) {
			cam_info("start Movie CAF\n");
			jc_writeb(JC_CATEGORY_LENS,
					0x02, 0x01);
		}
	} else if (status == 6) {
		cam_info("FD CAF\n");
		jc_writeb(JC_CATEGORY_LENS,
				0x01, 0x05);
		if (jc_ctrl->stream_on == true) {
			cam_info("start FD CAF\n");
			jc_writeb(JC_CATEGORY_LENS,
					0x02, 0x01);
		}
	}

	return rc;
}

static int jc_set_touch_af_pos(int x, int y)
{
	int32_t rc = 0;
	int h_x, l_x, h_y, l_y;

	cam_info("Entered, touch af pos (%x, %x)\n", x, y);

	if ((jc_ctrl->af_mode >= 3
		&& jc_ctrl->af_mode <=6)
		&& jc_ctrl->touch_af_mode == true) {
		cam_info("Now CAF mode. Return touch position setting!\n");
		return rc;
	}

	jc_ctrl->touch_af_mode = true;

	h_x = x >> 8;
	l_x = x & 0xFF;
	h_y = y >> 8;
	l_y = y & 0xFF;

	cam_info("(%x %x, %x %x)", h_x, l_x, h_y, l_y);
	if (jc_ctrl->movie_mode == true) {
		cam_info("Movie touch af\n");
		jc_writeb(JC_CATEGORY_LENS,
				0x01, 0x07);
	} else {
		cam_info("Normal touch af\n");
		jc_writeb(JC_CATEGORY_LENS,
				0x01, 0x02);
	}

	jc_writeb(JC_CATEGORY_LENS,
			0x30, h_x);
	jc_writeb(JC_CATEGORY_LENS,
			0x31, l_x);
	jc_writeb(JC_CATEGORY_LENS,
			0x32, h_y);
	jc_writeb(JC_CATEGORY_LENS,
			0x33, l_y);

	return rc;
}

static int jc_set_metering(int mode)
{
	int32_t rc = 0;

	cam_info("Entered, metering %d\n", mode);

	if (mode < 0 || mode > 2) {
		cam_err("invalid value, %d\n", mode);
		return rc;
	}
	if (mode == 0) {
		cam_info("average\n");
		jc_writeb(JC_CATEGORY_AE,
				JC_AE_MODE, 0x02);
	} else if (mode == 1) {
		cam_info("center-weight\n");
		jc_writeb(JC_CATEGORY_AE,
				JC_AE_MODE, 0x00);
	} else if (mode == 2) {
		cam_info("spot\n");
		jc_writeb(JC_CATEGORY_AE,
				JC_AE_MODE, 0x01);
	}
	return rc;
}

static int jc_set_wb(int mode)
{
	int32_t rc = 0;

	cam_info("Entered, wb %d\n", mode);

	if (mode < 1 || mode > 6) {
		cam_err("invalid value, %d\n", mode);
		return rc;
	}
	if (mode == 1) {
		cam_info("auto\n");
		jc_writeb(JC_CATEGORY_WB,
				JC_WB_AWB_MODE, 0x01);
	} else {
		cam_info("manual WB\n");
		jc_writeb(JC_CATEGORY_WB,
				JC_WB_AWB_MODE, 0x02);

		if (mode == 3) {
			cam_info("Incandescent\n");
			jc_writeb(JC_CATEGORY_WB,
					JC_WB_AWB_MANUAL, 0x01);
		} else if (mode == 4) {
			cam_info("Fluorescent\n");
			jc_writeb(JC_CATEGORY_WB,
					JC_WB_AWB_MANUAL, 0x02);
		} else if (mode == 5) {
			cam_info("Daylight\n");
			jc_writeb(JC_CATEGORY_WB,
					JC_WB_AWB_MANUAL, 0x04);
		} else if (mode == 6) {
			cam_info("Cloudy\n");
			jc_writeb(JC_CATEGORY_WB,
					JC_WB_AWB_MANUAL, 0x05);
		}
	}
	return rc;
}

static int jc_set_effect(int mode)
{
	int32_t rc = 0;

	cam_info("Entered, effect %d\n", mode);

	if (mode == 0) {
		cam_info("Off\n");
		jc_writeb(JC_CATEGORY_MON,
				JC_MON_COLOR_EFFECT, 0x00);
	} else if (mode == 1) {
		cam_info("Sepia\n");
		cam_info("Enable CFIXB and CFIXR\n");
		jc_writeb(JC_CATEGORY_MON,
				JC_MON_COLOR_EFFECT, 0x01);
		jc_writeb(JC_CATEGORY_MON,
				JC_MON_CFIXB, 0x00);
		jc_writeb(JC_CATEGORY_MON,
				JC_MON_CFIXR, 0x00);
	} else if (mode == 2) {
		cam_info("Negative\n");
		jc_writeb(JC_CATEGORY_MON,
				JC_MON_COLOR_EFFECT, 0x02);
	} else if (mode == 4) {
		cam_info("Mono\n");
		cam_info("Enable CFIXB and CFIXR\n");
		jc_writeb(JC_CATEGORY_MON,
				JC_MON_COLOR_EFFECT, 0x01);
		jc_writeb(JC_CATEGORY_MON,
				JC_MON_CFIXB, 0xD8);
		jc_writeb(JC_CATEGORY_MON,
				JC_MON_CFIXR, 0x18);
	} else if (mode == 12) {
		cam_info("warm\n");
		jc_writeb(JC_CATEGORY_MON,
				JC_MON_COLOR_EFFECT, 0x04);
	} else if (mode == 13) {
		cam_info("cold\n");
		jc_writeb(JC_CATEGORY_MON,
				JC_MON_COLOR_EFFECT, 0x05);
	} else if (mode == 14) {
		cam_info("washed\n");
		jc_writeb(JC_CATEGORY_MON,
				JC_MON_COLOR_EFFECT, 0x06);
	}
	return rc;
}

static int jc_set_quality(int mode)
{
	int32_t rc = 0;

	cam_info("Entered, quality %d\n", mode);

	if (mode < 0 || mode > 2) {
		cam_err("invalid value, %d\n", mode);
		return rc;
	}

	if (mode == 0) {
		cam_info("Super Fine\n");
		jc_writeb(JC_CATEGORY_CAPPARM,
				JC_CAPPARM_JPEG_RATIO_OFS, 0x02);
	} else if (mode == 1) {
		cam_info("Fine\n");
		jc_writeb(JC_CATEGORY_CAPPARM,
				JC_CAPPARM_JPEG_RATIO_OFS, 0x09);
	} else if (mode == 2) {
		cam_info("Normal\n");
		jc_writeb(JC_CATEGORY_CAPPARM,
				JC_CAPPARM_JPEG_RATIO_OFS, 0x14);
	}
	return rc;
}

static int jc_set_iso(int mode)
{
	int32_t rc = 0;

	cam_info("Entered, iso %d\n", mode);

	if (mode < 0 || mode > 6) {
		cam_err("invalid value, %d\n", mode);
		return rc;
	}

	if (mode == 0) {
		cam_info("auto\n");
		jc_writeb(JC_CATEGORY_AE,
			JC_AE_ISOSEL, 0x00);
	} else if (mode == 2) {
		cam_info("iso100\n");
		jc_writeb(JC_CATEGORY_AE,
			JC_AE_ISOSEL, 0x01);
	} else if (mode == 3) {
		cam_info("iso200\n");
		jc_writeb(JC_CATEGORY_AE,
			JC_AE_ISOSEL, 0x02);
	} else if (mode == 4) {
		cam_info("iso400\n");
		jc_writeb(JC_CATEGORY_AE,
			JC_AE_ISOSEL, 0x03);
	} else if (mode == 5) {
		cam_info("iso800\n");
		jc_writeb(JC_CATEGORY_AE,
			JC_AE_ISOSEL, 0x04);
	} else if (mode == 6) {
		cam_info("iso1600\n");
		jc_writeb(JC_CATEGORY_AE,
			JC_AE_ISOSEL, 0x05);
	}
	return rc;
}

static int jc_set_ev(int mode)
{
	int32_t rc = 0;

	cam_info("Entered, ev %d\n", mode);

	if (mode < 0 || mode > 8) {
		cam_err("invalid value, %d\n", mode);
		return rc;
	}

	if (mode == 0) {
		cam_info("-2\n");
		jc_writeb(JC_CATEGORY_AE,
				JC_AE_INDEX, 0x00);
	} else if (mode == 1) {
		cam_info("-1.5\n");
		jc_writeb(JC_CATEGORY_AE,
				JC_AE_INDEX, 0x01);
	} else if (mode == 2) {
		cam_info("-1.0\n");
		jc_writeb(JC_CATEGORY_AE,
				JC_AE_INDEX, 0x02);
	} else if (mode == 3) {
		cam_info("-0.5\n");
		jc_writeb(JC_CATEGORY_AE,
				JC_AE_INDEX, 0x03);
	} else if (mode == 4) {
		cam_info("0\n");
		jc_writeb(JC_CATEGORY_AE,
				JC_AE_INDEX, 0x04);
	} else if (mode == 5) {
		cam_info("+0.5\n");
		jc_writeb(JC_CATEGORY_AE,
				JC_AE_INDEX, 0x05);
	} else if (mode == 6) {
		cam_info("+1\n");
		jc_writeb(JC_CATEGORY_AE,
				JC_AE_INDEX, 0x06);
	} else if (mode == 7) {
		cam_info("+1.5\n");
		jc_writeb(JC_CATEGORY_AE,
				JC_AE_INDEX, 0x07);
	} else if (mode == 8) {
		cam_info("+2\n");
		jc_writeb(JC_CATEGORY_AE,
				JC_AE_INDEX, 0x08);
	}
	return rc;
}

static int jc_set_hjr(int mode)
{
	int32_t rc = 0;

	cam_info("Entered, hjr %d\n", mode);

	if (mode < 0 || mode > 1) {
		cam_err("invalid value, %d\n", mode);
		return rc;
	}

	if (mode == 0) {
		cam_info("HJR Off\n");
		jc_writeb(JC_CATEGORY_CAPCTRL,
				0x0B, 0x00);
	} else if (mode == 1) {
		cam_info("HJR On\n");
		jc_writeb(JC_CATEGORY_CAPCTRL,
				0x0B, 0x01);
	}

	return rc;
}

static int jc_set_wdr(int mode)
{
	int32_t rc = 0;

	cam_info("Entered, wdr %d\n", mode);

	if (mode < 0 || mode > 1) {
		cam_err("invalid value, %d\n", mode);
		return rc;
	}

	if (mode == 0) {
		cam_info("WDR Off\n");
		jc_writeb(JC_CATEGORY_CAPPARM,
				JC_CAPPARM_WDR_EN, 0x00);
	} else if (mode == 1) {
		cam_info("WDR On\n");
		jc_writeb(JC_CATEGORY_CAPPARM,
				JC_CAPPARM_WDR_EN, 0x01);
	}
	return rc;
}

static int jc_set_movie_mode(int mode)
{
	int32_t rc = 0;

	cam_info("Entered, movie mode %d\n", mode);

	jc_set_mode(JC_PARMSET_MODE);

	if (mode == 0) {
		cam_info("Capture mode\n");
		jc_ctrl->movie_mode = false;
		jc_writeb(JC_CATEGORY_PARM,
				JC_PARM_MON_MOVIE_SELECT, 0x00);

		cam_info("Zsl mode\n");
		jc_writeb(0x02, 0xCF, 0x01); /*zsl mode*/
	} else if (mode == 1) {
		cam_info("Movie mode\n");
		jc_ctrl->movie_mode = true;
		jc_writeb(JC_CATEGORY_PARM,
				JC_PARM_MON_MOVIE_SELECT, 0x01);
	}
	return rc;
}

static int jc_set_antibanding(int mode)
{
	int32_t rc = 0;

	cam_info("Entered, antibanding %d\n", mode);

	if (mode == 0) {
		cam_info("flicker off\n");
		jc_writeb(JC_CATEGORY_AE,
				0x06, 0x05);
	} if (mode == 1) {
		cam_info("60Hz auto flicker mode\n");
		jc_writeb(JC_CATEGORY_AE,
				0x06, 0x02);
	} if (mode == 2) {
		cam_info("50Hz auto flicker mode\n");
		jc_writeb(JC_CATEGORY_AE,
				0x06, 0x01);
	} if (mode == 3) {
		cam_info("auto flicker mode\n");
		jc_writeb(JC_CATEGORY_AE,
				0x06, 0x00);
	}

	return rc;
}

static int jc_set_shot_mode(int mode)
{
	int32_t rc = 0;
	u32 isp_mode;

	jc_readb(JC_CATEGORY_SYS, JC_SYS_MODE, &isp_mode);

	cam_info("Entered, shot mode %d / %d\n", mode, isp_mode);

	jc_ctrl->shot_mode = mode;

	if (isp_mode == JC_MONITOR_MODE) {
		cam_info("monitor mode\n");

		jc_set_mode(JC_PARMSET_MODE);

		if (mode == 10) { //night mode
			cam_info("HJR Off\n");
			jc_writeb(JC_CATEGORY_CAPCTRL,
					0x0B, 0x00);
		}

		jc_writeb(JC_CATEGORY_PARM,
				0x0E, mode);

		jc_set_mode(JC_MONITOR_MODE);
	} else {
		cam_info("parameter mode\n");

		if (mode == 10) { //night mode
			cam_info("HJR Off\n");
			jc_writeb(JC_CATEGORY_CAPCTRL,
					0x0B, 0x00);
		}

		jc_writeb(JC_CATEGORY_PARM,
				0x0E, mode);
	}

	return rc;
}

static int jc_set_ocr_focus_mode(int mode)
{
	int32_t rc = 0;

	cam_info("Entered, ocr focus mode %d\n", mode);

	jc_writeb(JC_CATEGORY_LENS, 0x18, mode);

	return rc;
}

static int jc_set_different_ratio_capture(int mode)
{
	int32_t rc = 0;

	cam_info("Entered, different ratio capture mode %d\n", mode);

	if (mode == 1)
		jc_writeb(JC_CATEGORY_CAPPARM, 0x77, 0x1);
	else
		jc_writeb(JC_CATEGORY_CAPPARM, 0x77, 0x0);

	return rc;
}

static int jc_set_af_window(int mode)
{
	int32_t rc = 0;

	cam_info("Entered, af window %d\n", mode);

	jc_writeb(JC_CATEGORY_TEST, 0x8D, mode);

	return rc;
}

static int jc_set_softlanding(void)
{
	int32_t rc = 0;
	int check_softlanding = 0;
	int retry = 0;

	cam_info("Entered, softlanding\n");

	jc_writeb(JC_CATEGORY_LENS, 0x16, 0x01);

	for (retry = 2; retry > 0; retry--) {
		jc_readb(JC_CATEGORY_LENS,
					0x17, &check_softlanding);
		if (check_softlanding == 0x30) {
			cam_info("Finish softlanding");
			break;
		} else {
			cam_info("softlanding status : 0x%x", check_softlanding);
			msleep(33);
		}
	}

	return rc;
}

static int jc_set_fps(int mode, int min, int max)
{
	int32_t rc = 0;

	cam_info("Entered, fps mode : %d, min : %d, max : %d\n",
		mode, min, max);

	if (mode == 0) {
		cam_info("Auto fps mode\n");
		jc_writeb(0x02, 0xCF, 0x01);	/*zsl mode*/

		if (min == 10000 && max == 30000) { /*LLS*/
			jc_writeb(JC_CATEGORY_AE,
					JC_AE_EP_MODE_CAP, 0x15);
		} else if (max == 24000) {
			jc_writeb(JC_CATEGORY_AE,
					JC_AE_EP_MODE_CAP, 0x0C);
		} else {
			jc_writeb(JC_CATEGORY_AE,
					JC_AE_EP_MODE_CAP, 0x00);
		}
	} else if (mode == 1) {
		cam_info("Fixed fps mode\n");
		jc_writeb(0x02, 0xCF, 0x00);	/*non-zsl mode*/

		if (max == 7000) {
			jc_writeb(JC_CATEGORY_AE,
					JC_AE_EP_MODE_CAP, 0x09);
		} else if (max == 15000) {
			if (jc_ctrl->shot_mode == 0x0F) {/*CinePic*/
				cam_info("CinePic FPS\n");
				jc_writeb(JC_CATEGORY_AE,
						JC_AE_EP_MODE_CAP, 0x17);
			} else {
				jc_writeb(JC_CATEGORY_AE,
						JC_AE_EP_MODE_CAP, 0x04);
			}
		} else if (max == 24000) {
			jc_writeb(JC_CATEGORY_AE,
					JC_AE_EP_MODE_CAP, 0x05);
		} else if (max == 30000) {
			jc_writeb(JC_CATEGORY_AE,
					JC_AE_EP_MODE_CAP, 0x02);
		} else if (max == 60000) {
			jc_writeb(JC_CATEGORY_AE,
					JC_AE_EP_MODE_CAP, 0x07);
		} else if (max == 90000) {
			jc_writeb(JC_CATEGORY_AE,
					JC_AE_EP_MODE_CAP, 0x0B);
		} else if (max == 120) { /*120fps*/
			jc_writeb(JC_CATEGORY_AE,
					JC_AE_EP_MODE_CAP, 0x08);
		}
	} else if (mode == 2) {
		cam_info("Drama shot mode\n");
		jc_writeb(JC_CATEGORY_AE,
				JC_AE_EP_MODE_CAP, 0x16);
	}
	return rc;
}

static int jc_set_flash(int mode, int mode2)
{
	int32_t rc = 0;

	cam_info("Entered, flash %d / %d\n", mode, mode2);

	if (jc_ctrl->torch_mode == true
		&& mode == 1
		&& mode2 == 0) {
		/*torch off on camera */
		mode = 0;
	}

	if (mode == 0) {
		cam_info("Preview LED\n");
		if (mode2 == 0) {
			cam_info("Torch Off\n");
			jc_ctrl->torch_mode = false;
			jc_writeb(JC_CATEGORY_TEST,
					0x29, 0x00);
		} else if (mode2 == 1) {
			cam_info("Torch On");
			jc_ctrl->torch_mode = true;
			jc_writeb(JC_CATEGORY_TEST,
					0x29, 0x01);
		}
	} else if (mode == 1) {
		jc_ctrl->torch_mode = false;
		if (mode2 == 0) {
			cam_info("Flash Off\n");
			jc_writeb(JC_CATEGORY_TEST,
					0xB6, 0x00);
		} else if (mode2 == 1) {
			cam_info("Flash Auto");
			jc_writeb(JC_CATEGORY_TEST,
					0xB6, 0x02);
		} else if (mode2 == 2) {
			cam_info("Flash On");
			jc_writeb(JC_CATEGORY_TEST,
					0xB6, 0x01);
		}
	}
	return rc;
}

static int jc_set_hw_vdis(void)
{
	int32_t rc = 0, prev_vdis_mode;

	cam_info("Entered, hw vdis mode : %d\n", jc_ctrl->hw_vdis_mode);

	jc_readb(JC_CATEGORY_MON,
			0x00, &prev_vdis_mode);

	if (jc_ctrl->hw_vdis_mode != prev_vdis_mode) {
		if (jc_ctrl->hw_vdis_mode == 1)
			jc_writeb(JC_CATEGORY_MON,
					0x00, 0x01);
		else
			jc_writeb(JC_CATEGORY_MON,
					0x00, 0x00);
  	} else
		cam_info("hw vdis mode same as previous mode : %d",
			prev_vdis_mode);
	return rc;
}


#if JC_DUMP_FW
static int jc_mem_dump(u16 len, u32 addr, u8 *val)
{
	struct i2c_msg msg;
	unsigned char data[8];
	unsigned char recv_data[len + 3];
	int i, err = 0;

	if (!jc_client->adapter)
		return -ENODEV;

	if (len <= 0)
		return -EINVAL;

	msg.addr = jc_client->addr;
	msg.flags = 0;
	msg.len = sizeof(data);
	msg.buf = data;

	/* high byte goes out first */
	data[0] = 0x00;
	data[1] = 0x03;
	data[2] = 0x18;
	data[3] = (addr >> 16) & 0xFF;
	data[4] = (addr >> 8) & 0xFF;
	data[5] = addr & 0xFF;
	data[6] = (len >> 8) & 0xFF;
	data[7] = len & 0xFF;

	for (i = JC_I2C_RETRY; i; i--) {
		err = i2c_transfer(jc_client->adapter, &msg, 1);
		if (err == 1)
			break;
		else
			cam_i2c_dbg("i2c write error\n");

		msleep(20);
	}

	if (err != 1)
		return err;

	msg.flags = I2C_M_RD;// | I2C_M_NO_RD_ACK;
	msg.len = sizeof(recv_data);
	msg.buf = recv_data;
	for (i = JC_I2C_RETRY; i; i--) {
		err = i2c_transfer(jc_client->adapter, &msg, 1);
		if (err == 1)
			break;
		else
			cam_i2c_dbg("i2c read error\n");

		msleep(20);
	}

	if (err != 1)
		return err;

	if (len != (recv_data[1] << 8 | recv_data[2]))
		cam_i2c_dbg("expected length %d, but return length %d\n",
			len, recv_data[1] << 8 | recv_data[2]);

	memcpy(val, recv_data + 3, len);

	/*cam_i2c_dbg("address %#x, length %d\n", addr, len);*/	/*for debug*/
	return err;
}
#endif

#if JC_MEM_READ
static int jc_mem_read(u16 len, u32 addr, u8 *val)
{
	struct i2c_msg msg;
	unsigned char data[8];
	unsigned char recv_data[len + 3];
	int i, err = 0;

	if (!jc_client->adapter)
		return -ENODEV;

	if (len <= 0)
		return -EINVAL;

	msg.addr = jc_client->addr;
	msg.flags = 0;
	msg.len = sizeof(data);
	msg.buf = data;

	/* high byte goes out first */
	data[0] = 0x00;
	data[1] = 0x03;
	data[2] = (addr >> 24) & 0xFF;
	data[3] = (addr >> 16) & 0xFF;
	data[4] = (addr >> 8) & 0xFF;
	data[5] = addr & 0xFF;
	data[6] = (len >> 8) & 0xFF;
	data[7] = len & 0xFF;

	for (i = JC_I2C_RETRY; i; i--) {
		err = i2c_transfer(jc_client->adapter, &msg, 1);
		if (err == 1)
			break;
		msleep(20);
	}

	if (err != 1)
		return err;

	msg.flags = I2C_M_RD;// | I2C_M_NO_RD_ACK;
	msg.len = sizeof(recv_data);
	msg.buf = recv_data;
	for (i = JC_I2C_RETRY; i; i--) {
		err = i2c_transfer(jc_client->adapter, &msg, 1);
		if (err == 1)
			break;
		msleep(20);
	}

	if (err != 1)
		return err;

	if (len != (recv_data[1] << 8 | recv_data[2]))
		cam_i2c_dbg("expected length %d, but return length %d\n",
			len, recv_data[1] << 8 | recv_data[2]);

	memcpy(val, recv_data + 3, len);

	cam_i2c_dbg("address %#x, length %d\n", addr, len);
	return err;
}
#endif

#if JC_LOAD_FW_MAIN || JC_DUMP_FW
static int jc_mem_write(u8 cmd,
		u16 len, u32 addr, u8 *val)
{
	struct i2c_msg msg;
	unsigned char data[len + 8];
	int i, err = 0;

	if (!jc_client->adapter)
		return -ENODEV;

	msg.addr = jc_client->addr;
	msg.flags = 0;
	msg.len = sizeof(data);
	msg.buf = data;

	/* high byte goes out first */
	data[0] = 0x00;
	data[1] = cmd;
	data[2] = ((addr >> 24) & 0xFF);
	data[3] = ((addr >> 16) & 0xFF);
	data[4] = ((addr >> 8) & 0xFF);
	data[5] = (addr & 0xFF);
	data[6] = ((len >> 8) & 0xFF);
	data[7] = (len & 0xFF);
	memcpy(data + 2 + sizeof(addr) + sizeof(len), val, len);

	cam_i2c_dbg("address %#x, length %d\n", addr, len);
	for (i = JC_I2C_RETRY; i; i--) {
		err = i2c_transfer(jc_client->adapter, &msg, 1);
		if (err == 1)
			break;
		msleep(20);
	}

	return err;
}
#endif


#if JC_DUMP_FW
static int jc_dump_fw(void)
{
	struct file *fp;
	mm_segment_t old_fs;
	u8 *buf/*, val*/;
	u32 addr, unit, count, intram_unit = 0x1000;
	int i, /*j,*/ err;

	cam_err("start\n");

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(JC_FW_DUMP_PATH,
		O_WRONLY|O_CREAT|O_TRUNC, S_IRUGO|S_IWUGO|S_IXUSR);
	if (IS_ERR(fp)) {
		cam_err("failed to open %s, err %ld\n",
			JC_FW_DUMP_PATH, PTR_ERR(fp));
		err = -ENOENT;
		goto file_out;
	}

	buf = kmalloc(intram_unit, GFP_KERNEL);
	if (!buf) {
		cam_err("failed to allocate memory\n");
		err = -ENOMEM;
		goto out;
	}

	cam_err("start, file path %s\n", JC_FW_DUMP_PATH);

	err = jc_mem_write(0x04, SZ_64,
				0x90001200 , buf_port_seting0);
	msleep(10);

	err = jc_mem_write(0x04, SZ_64,
				0x90001000 , buf_port_seting1);
	msleep(10);

	err = jc_mem_write(0x04, SZ_64,
				0x90001100 , buf_port_seting2);
	msleep(10);

	err = jc_writel(JC_CATEGORY_FLASH,
				0x1C, 0x0247036D);

	err = jc_writeb(JC_CATEGORY_FLASH,
				0x57, 0x01);

	addr = JC_FLASH_READ_BASE_ADDR;
	unit = 0x80;
	count = 1024*16;
	for (i = 0; i < count; i++) {
			err = jc_mem_dump(unit, addr + (i * unit), buf);
			/*cam_err("dump ~~ count : %d\n", i);*/
			if (err < 0) {
				cam_i2c_dbg("dump i2c error\n");
				cam_err("i2c falied, err %d\n", err);
				goto out;
			}
			vfs_write(fp, buf, unit, &fp->f_pos);
	}

	cam_err("end\n");

out:
	if (buf)
		kfree(buf);

	if (!IS_ERR(fp))
		filp_close(fp, current->files);

file_out:
	set_fs(old_fs);

	return err;
}
#endif

#ifdef JC_SPI_WRITE
static int jc_SIO_loader(void)
{
	u8 *buf_m9mo = NULL;
	unsigned int count = 0;
	/*int offset;*/
	int err = 0;

	struct file *fp;
	mm_segment_t old_fs;
	long fsize, nread;

	CAM_DEBUG("E");

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(JC_SIO_LOADER_PATH_M9MO, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		cam_err("failed to open %s, err %ld\n",
			JC_SIO_LOADER_PATH_M9MO, PTR_ERR(fp));
		goto out;
	}

	fsize = fp->f_path.dentry->d_inode->i_size;
	count = fsize / SZ_4K;

	cam_err("start, file path %s, size %ld Bytes, count %d\n",
		JC_SIO_LOADER_PATH_M9MO, fsize, count);

	buf_m9mo = vmalloc(fsize);
	if (!buf_m9mo) {
		cam_err("failed to allocate memory\n");
		err = -ENOMEM;
		goto out;
	}

	nread = vfs_read(fp, (char __user *)buf_m9mo, fsize, &fp->f_pos);
	if (nread != fsize) {
		cam_err("failed to read firmware file, %ld Bytes\n", nread);
		err = -EIO;
		goto out;
	}

	err = jc_mem_write(0x08, SZ_64,
			0x90001200 , buf_port_seting0);

	cam_err("err : %d", err);

	msleep(10);

	err = jc_mem_write(0x08, SZ_64,
			0x90001000 , buf_port_seting1);
	msleep(10);

	cam_err("err : %d", err);

	err = jc_mem_write(0x08, SZ_64,
			0x90001100 , buf_port_seting2);
	msleep(10);

	cam_err("err : %d", err);

	/* program FLASH ROM */
	err = jc_mem_write(0x04, SZ_4K,
			0x01000100 , buf_m9mo);

	cam_err("err : %d", err);

	err = jc_mem_write(0x04, SZ_4K,
			0x01001100 , buf_m9mo+SZ_4K);

	cam_err("err : %d", err);

	err = jc_mem_write(0x04, SZ_4K,
			0x01002100 , buf_m9mo+SZ_8K);

	cam_err("err : %d", err);

	err = jc_mem_write(0x04, fsize-(SZ_4K|SZ_8K),
			0x01003100 , buf_m9mo+(SZ_4K|SZ_8K));

	cam_err("err : %d", err);

	err = jc_writel(JC_CATEGORY_FLASH,
			0x0C, 0x01000100);

	cam_err("err : %d", err);

	/* Start Programming */
		err = jc_writeb(JC_CATEGORY_FLASH, 0x12, 0x02);

	cam_err("err : %d", err);

out:
	if (buf_m9mo)
		vfree(buf_m9mo);

	if (!IS_ERR(fp))
		filp_close(fp, current->files);

	set_fs(old_fs);

	return err;
}
#endif


#if JC_LOAD_FW_MAIN
static int jc_load_fw_main(void)
{
#ifndef JC_SPI_WRITE
	u8 *buf_m9mo = NULL;
#else
	int txSize = 0;
#endif
	unsigned int count = 0;
	/*int offset;*/
	int err = 0;
	struct file *fp = NULL;
	mm_segment_t old_fs;
	long fsize, nread;
	u8 *buf_m10mo = NULL;
	int i;

	CAM_DEBUG("E");

	old_fs = get_fs();
	set_fs(KERNEL_DS);

#if 1
	fp = filp_open(JC_M10MO_FW_PATH_SD, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		cam_err("failed to open %s, err %ld\n", JC_M10MO_FW_PATH_SD, PTR_ERR(fp));
		goto out;
	}
#else
	if (firmware_update_sdcard == true){
		fp = filp_open(JC_M10MO_FW_PATH_SD, O_RDONLY, 0);
		if (IS_ERR(fp)) {
			cam_err("failed to open %s, err %ld\n", JC_M10MO_FW_PATH_SD, PTR_ERR(fp));
			goto out;
		}
	} else {
		if (jc_ctrl->sensor_combination == 0)
			fp = filp_open(JC_M10MO_FW_PATH_SS, O_RDONLY, 0);
		else if (jc_ctrl->sensor_combination == 1)
			fp = filp_open(JC_M10MO_FW_PATH_OS, O_RDONLY, 0);
		else if (jc_ctrl->sensor_combination == 2)
			fp = filp_open(JC_M10MO_FW_PATH_SL, O_RDONLY, 0);
		else if (jc_ctrl->sensor_combination == 3)
			fp = filp_open(JC_M10MO_FW_PATH_OL, O_RDONLY, 0);
		else
			fp = filp_open(JC_M10MO_FW_PATH_SS, O_RDONLY, 0);

		if (IS_ERR(fp)) {
			if (jc_ctrl->sensor_combination == 0)
				cam_err("failed to open %s, err %ld\n", JC_M10MO_FW_PATH_SS, PTR_ERR(fp));
			else if (jc_ctrl->sensor_combination == 1)
				cam_err("failed to open %s, err %ld\n", JC_M10MO_FW_PATH_OS, PTR_ERR(fp));
			else if (jc_ctrl->sensor_combination == 2)
				cam_err("failed to open %s, err %ld\n", JC_M10MO_FW_PATH_SL, PTR_ERR(fp));
			else if (jc_ctrl->sensor_combination == 3)
				cam_err("failed to open %s, err %ld\n", JC_M10MO_FW_PATH_OL, PTR_ERR(fp));
			else
				cam_err("failed to open %s, err %ld\n", JC_M10MO_FW_PATH_SS, PTR_ERR(fp));

			goto out;
		}
	}
#endif

	fsize = fp->f_path.dentry->d_inode->i_size;
	count = fsize / SZ_4K;

#if 0
	if (firmware_update_sdcard == true) {
		cam_err("start, file path %s, size %ld Bytes, count %d\n",
			JC_M10MO_FW_PATH_SD, fsize, count);
	} else {
		if (jc_ctrl->sensor_combination == 0)
			cam_err("start, file path %s, size %ld Bytes, count %d\n",
				JC_M10MO_FW_PATH_SS, fsize, count);
		else if (jc_ctrl->sensor_combination == 1)
			cam_err("start, file path %s, size %ld Bytes, count %d\n",
				JC_M10MO_FW_PATH_OS, fsize, count);
		else if (jc_ctrl->sensor_combination == 2)
			cam_err("start, file path %s, size %ld Bytes, count %d\n",
				JC_M10MO_FW_PATH_SL, fsize, count);
		else if (jc_ctrl->sensor_combination == 3)
			cam_err("start, file path %s, size %ld Bytes, count %d\n",
				JC_M10MO_FW_PATH_OL, fsize, count);
		else
			cam_err("start, file path %s, size %ld Bytes, count %d\n",
				JC_M10MO_FW_PATH_SS, fsize, count);
	}
#endif

#ifndef JC_SPI_WRITE
	buf_m9mo = vmalloc(fsize);
	if (!buf_m9mo) {
		cam_err("failed to allocate memory\n");
		err = -ENOMEM;
		goto out;
	}

	nread = vfs_read(fp, (char __user *)buf_m9mo, fsize, &fp->f_pos);
	if (nread != fsize) {
		cam_err("failed to read firmware file, %ld Bytes\n", nread);
		err = -EIO;
		goto out;
	}

	filp_close(fp, current->files);

	err = jc_mem_write(0x04, SZ_64,
			0x90001200 , buf_port_seting0);
	msleep(10);

	err = jc_mem_write(0x04, SZ_64,
			0x90001000 , buf_port_seting1);
	msleep(10);

	err = jc_mem_write(0x04, SZ_64,
			0x90001100 , buf_port_seting2);
	msleep(10);

	err = jc_writel(JC_CATEGORY_FLASH,
			0x1C, 0x0247036D);

	err = jc_writeb(JC_CATEGORY_FLASH,
			0x4A, 0x01);
	msleep(10);

	/* program FLASH ROM */
	err = jc_program_fw(buf_m9mo, JC_FLASH_BASE_ADDR, SZ_4K, count);
	if (err < 0)
		goto out;

	cam_err("end\n");
	jc_ctrl->isp.bad_fw = 0;

out:
	if (buf_m9mo)
		vfree(buf_m9mo);

	if (!IS_ERR(fp))
		filp_close(fp, current->files);

	set_fs(old_fs);

	CAM_DEBUG("X");
#else

	txSize = FW_WRITE_SIZE;
	count = fsize / txSize;

	buf_m10mo = vmalloc(txSize);

	if (!buf_m10mo) {
		cam_err("failed to allocate memory\n");
		err = -ENOMEM;
		goto out;
	}

	for (i = 0 ; i < count ; i++) {
		nread = vfs_read(fp, (char __user *)buf_m10mo, txSize, &fp->f_pos);
		cam_err("nread : %ld\n", nread);
		err = spi_xmit(buf_m10mo, txSize);
	}

	if (buf_m10mo)
		vfree(buf_m10mo);

	filp_close(fp, current->files);

out:
	if (!IS_ERR(fp))
		filp_close(fp, current->files);
	set_fs(old_fs);
#endif

	return err;
}
#endif

#ifdef JC_SPI_WRITE
static int jc_init_fw_load(void)
{
	u32 val;
	int i;
	int err = 0;
	int erase = 0x01;
	int retries = 0;

	for (i = 0; i < 512; i++) {
		/* Set Flash ROM memory address */
		err = jc_writel(JC_CATEGORY_FLASH,
			JC_FLASH_ADDR, i * 0x0001000);

		/* Erase FLASH ROM entire memory */
		err = jc_writeb(JC_CATEGORY_FLASH,
			JC_FLASH_ERASE, erase);

		/* Response while sector-erase is operating */
		retries = 0;
		do {
			msleep(10);
			err = jc_readb(JC_CATEGORY_FLASH,
				JC_FLASH_ERASE, &val);
		} while (val == erase && retries++ < JC_I2C_VERIFY);

		if (val != 0) {
			cam_err("failed to erase sector\n");
			return -EFAULT;
		}
	}

	err = jc_writel(JC_CATEGORY_FLASH,
			0x1C, 0x0247036D);

	err = jc_writeb(JC_CATEGORY_FLASH,
			0x4A, 0x02);

	msleep(3);

	err = jc_writel(JC_CATEGORY_FLASH,
			0x14, 0x20000000);

	err = jc_writel(JC_CATEGORY_FLASH,
			0x18, 0x00200000);

	err = jc_load_fw_main();

	err = jc_writel(JC_CATEGORY_FLASH,
			0x00, 0x00);

	err = jc_writel(JC_CATEGORY_FLASH,
			0x18, 0x00200000);

	err = jc_writeb(JC_CATEGORY_FLASH,
			JC_FLASH_WR, 0x01);

	retries = 0;
	do {
		msleep(300);
		err = jc_readb(JC_CATEGORY_FLASH,
			JC_FLASH_WR, &val);
	} while (val == 0x01 && retries++ < JC_I2C_VERIFY);


	return 0;
}
#endif

#if JC_CHECK_FW
static int jc_get_sensor_fw_version(void)
{
	int err;
	int fw_ver = 0x00;
	u8 sensor_type[] = "13M_SONY";/* temporarily */

	cam_err("E\n");

	/* read F/W version */
	err = jc_readw(JC_CATEGORY_SYS,
		JC_SYS_VER_FW, &fw_ver);

	cam_info("f/w version = %x\n", fw_ver);
	cam_info("sensor_type = %s\n", sensor_type);

	sprintf(jc_ctrl->sensor_ver, "FU%x", fw_ver);
	sprintf(jc_ctrl->sensor_type, "%s", sensor_type);
	memcpy(sysfs_sensor_fw, jc_ctrl->sensor_ver,
			sizeof(jc_ctrl->sensor_ver));
	memcpy(sysfs_sensor_type, jc_ctrl->sensor_type,
			sizeof(jc_ctrl->sensor_type));

	cam_info("sensor fw : %s\n", sysfs_sensor_fw);
	cam_info("sensor type : %s\n", sysfs_sensor_type);
	return 0;
}

static int jc_get_phone_fw_version(void)
{
	const struct firmware *fw;
	int err = 0;

	struct file *fp = NULL;
	mm_segment_t old_fs;
	long nread;
	int fw_requested = 1;
	int ver_tmp;

	cam_info("E\n");

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	if (isp_version == 0x00) {
		fp = filp_open(JC_M9MO_FW_PATH_SD, O_RDONLY, 0);
		if (IS_ERR(fp)) {
			cam_err("failed to open %s, err %ld\n",
				JC_M9MO_FW_PATH_SD, PTR_ERR(fp));
			firmware_update_sdcard = false;

			fp = filp_open(JC_M9MO_FW_PATH, O_RDONLY, 0);
			if (IS_ERR(fp)) {
				cam_err("failed to open %s, err %ld\n",
					JC_M9MO_FW_PATH, PTR_ERR(fp));
				goto request_fw;
			} else {
				cam_info("FW File(phone) opened.\n");
			}
		} else {
			cam_info("FW File(SD card) opened.\n");
			firmware_update_sdcard = true;
		}
	} else if (isp_version == 0x10) {
		fp = filp_open(JC_M10MO_FW_PATH_SD, O_RDONLY, 0);
		if (IS_ERR(fp)) {
			cam_err("failed to open %s, err %ld\n",
				JC_M10MO_FW_PATH_SD, PTR_ERR(fp));
			firmware_update_sdcard = false;

			fp = filp_open(JC_M10MO_FW_PATH, O_RDONLY, 0);
			if (IS_ERR(fp)) {
				cam_err("failed to open %s, err %ld\n",
					JC_M10MO_FW_PATH, PTR_ERR(fp));
				goto request_fw;
			} else {
				cam_info("FW File(phone) opened.\n");
			}
		} else {
			cam_info("FW File(SD card) opened.\n");
			firmware_update_sdcard = true;
		}
	}

	fw_requested = 0;

	if (!IS_ERR(fp)) {
		err = vfs_llseek(fp, JC_FW_VER_NUM, SEEK_SET);
		if (err < 0) {
			cam_err("failed to fseek, %d\n", err);
			goto out;
		}

		nread = vfs_read(fp, (char __user *)jc_ctrl->phone_ver,
				JC_FW_VER_LEN, &fp->f_pos);
		cam_err("phone ver : %02x%02x\n",
			*jc_ctrl->phone_ver, *(jc_ctrl->phone_ver+1));

		if (nread != JC_FW_VER_LEN) {
			cam_err("failed to read firmware file, %ld Bytes\n", nread);
			err = -EIO;
			goto out;
		}
	}

request_fw:
	if (fw_requested) {
		set_fs(old_fs);

		cam_info("Firmware Path = %s\n", JC_FW_REQ_PATH);
		err = request_firmware(&fw, JC_FW_REQ_PATH, &jc_dev);

		if (err != 0) {
			cam_err("request_firmware falied\n");
			err = -EINVAL;
			goto out;
		}
		cam_info("%s: fw->data[0] = %x, fw->data[1] = %x\n", __func__,
				(int)fw->data[JC_FW_VER_NUM],
				(int)fw->data[JC_FW_VER_NUM + 1]);
		ver_tmp = (int)fw->data[JC_FW_VER_NUM] * 16 * 16;
		ver_tmp += (int)fw->data[JC_FW_VER_NUM + 1];
		cam_info("ver_tmp = %x\n", ver_tmp);
		sprintf(jc_ctrl->phone_ver, "FU%x", ver_tmp);
		memcpy(sysfs_phone_fw, jc_ctrl->phone_ver,
				sizeof(jc_ctrl->phone_ver));

	}
out:
	if (err != 0)
		sprintf(sysfs_phone_fw, "FU%02x%02x",
		*jc_ctrl->phone_ver, *(jc_ctrl->phone_ver+1));

	if (!fw_requested) {
		if (!IS_ERR(fp))
			filp_close(fp, current->files);
		set_fs(old_fs);
	} else {
		release_firmware(fw);
	}

	cam_err("phone ver : %s\n", sysfs_phone_fw);
	return 0;
}

static int jc_check_fw(void)
{
	CAM_DEBUG("E\n");

	/* F/W version */
	/* temporarily changed func call order - orig : phone -> sensor */
	jc_get_sensor_fw_version();
	jc_get_phone_fw_version();

	cam_info("phone ver = %s, sensor_ver = %s\n",
			sysfs_phone_fw, sysfs_sensor_fw);

	CAM_DEBUG("X\n");
	return strcmp(sysfs_sensor_fw, sysfs_phone_fw);
}
#endif

static int jc_check_sum(void)
{
	int err;
	int retries;
	int isp_val, factarea_val;
	u8 flash_controller[] = {0x7f};

	err = jc_mem_write(0x04, 1,
		    0x13000005 , flash_controller);
	cam_err("err : %d", err);
	msleep(1);

	/* Checksum */
	err = jc_writeb(JC_CATEGORY_FLASH, 0x09, 0x04);
	cam_err("err : %d", err);

	retries = 0;
	do {
		msleep(300);
		err = jc_readb(JC_CATEGORY_FLASH,
			0x09, &isp_val);

		if (isp_val == 0)
			break;
	} while (isp_val == 0x04 && retries++ < JC_I2C_VERIFY);

	/* Get Checksum of ISP */
	err = jc_readw(JC_CATEGORY_FLASH, 0x0A, &isp_val);
	cam_err("ISP Checksum : %x", isp_val);

	if (jc_ctrl->isp_null_read_sensor_fw == true) {
		err = jc_writel(JC_CATEGORY_FLASH,
				0x0, 0x001E7000);
		err = jc_writel(JC_CATEGORY_FLASH,
				0x5C, 0x00018000);

		/* Checksum */
		err = jc_writeb(JC_CATEGORY_FLASH, 0x09, 0x02);

		retries = 0;
		do {
			msleep(300);
			err = jc_readb(JC_CATEGORY_FLASH,
				0x09, &factarea_val);

			if (factarea_val == 0)
				break;
		} while (factarea_val == 0x02 && retries++ < JC_I2C_VERIFY);

		/* Get Checksum of FactArea */
		err = jc_readw(JC_CATEGORY_FLASH, 0x0A, &factarea_val);
		cam_err("FactArea Checksum : %x", factarea_val);
		cam_err("ISP - FactArea Checksum : %x", isp_val-factarea_val);

		return isp_val-factarea_val;
	}

	return isp_val;
}

static int jc_phone_fw_to_isp(void)
{
	int err = 0;
	int retries;
	int val;
	int chip_erase;

retry:
	/* Set SIO receive mode : 0x4C, rising edge*/
	err = jc_writeb(JC_CATEGORY_FLASH, 0x4B, 0x4C);
	cam_err("err : %d", err);

	err = jc_readb(JC_CATEGORY_FLASH,
			0x4B, &val);
	cam_err("rising edge : %d", val);

	/* SIO mode start */
	err = jc_writeb(JC_CATEGORY_FLASH, 0x4A, 0x02);
	cam_err("err : %d", err);

	retries = 0;
	do {
		msleep(300);
		cam_err("read SIO mode start!!");
		err = jc_readb(JC_CATEGORY_FLASH,
			0x4A, &val);
	} while (val == 0x02 && retries++ < JC_I2C_VERIFY);

	msleep(30);

	/* Send firmware by SIO transmission */
	err = jc_load_fw_main();

	chip_erase = jc_check_chip_erase();

	if (chip_erase == ISP_FROM_ERASED) {
	/* Erase Flash ROM */
		err = jc_writeb(JC_CATEGORY_FLASH, 0x06, 0x02);

		retries = 0;
		do {
			msleep(300);
			err = jc_readb(JC_CATEGORY_FLASH,
				0x06, &val);
		} while (val == 0x02 && retries++ < JC_I2C_VERIFY);
	}

	/* Start Programming */
	err = jc_writeb(JC_CATEGORY_FLASH, 0x07, 0x01);
	retries = 0;
	do {
		msleep(300);
		err = jc_readb(JC_CATEGORY_FLASH,
			0x07, &val);
	} while (val == 0x01 && retries++ < JC_I2C_VERIFY);

	err = jc_check_sum();

	if (err != 0 && jc_ctrl->fw_retry_cnt < 2) {
		cam_err("checksum error!! retry fw write!!: %d", jc_ctrl->fw_retry_cnt);
		jc_ctrl->fw_retry_cnt++;
		goto retry;
	}

	return 0;
}

static int jc_read_from_sensor_fw(void)
{
	int err = 0;
	int retries;
	int val = 0;
	int chip_erase;

retry:
	/* Read Sensor Flash */
	err = jc_writeb(JC_CATEGORY_FLASH, 0x63, 0x01);
	retries = 0;
	do {
		msleep(300);
		err = jc_readb(JC_CATEGORY_FLASH,
			0x63, &val);
	} while (val == 0xff && retries++ < JC_I2C_VERIFY);

	chip_erase = jc_check_chip_erase();

	if (chip_erase == ISP_FROM_ERASED) {
		/* Chip Erase */
		err = jc_writeb(JC_CATEGORY_FLASH, 0x06, 0x02);

		retries = 0;
		do {
			msleep(300);
			err = jc_readb(JC_CATEGORY_FLASH,
				0x06, &val);
		} while (val == 0x02 && retries++ < JC_I2C_VERIFY);
	}

	/* Start Programming */
	err = jc_writeb(JC_CATEGORY_FLASH, 0x07, 0x01);
	retries = 0;
	do {
		msleep(300);
		err = jc_readb(JC_CATEGORY_FLASH,
			0x07, &val);
	} while (val == 0x01 && retries++ < JC_I2C_VERIFY);

	err = jc_check_sum();

	if (err != 0 && jc_ctrl->fw_retry_cnt < 2) {
		cam_err("checksum error!! retry fw write!!: %d", jc_ctrl->fw_retry_cnt);
		jc_ctrl->fw_retry_cnt++;
		goto retry;
	}

	return 0;
}

static int jc_load_SIO_fw(void)
{
	struct file *fp;
	mm_segment_t old_fs;
	long fsize, nread;

	u8 *buf_m9mo = NULL;
	int err = 0;
	unsigned int count = 0;

	/* Port Setting */
	err = jc_mem_write(0x08, SZ_64,
			0x90001200 , buf_port_seting0_m10mo);

	cam_err("err : %d", err);

	msleep(10);

	err = jc_mem_write(0x08, SZ_64,
			0x90001000 , buf_port_seting1_m10mo);
	msleep(10);

	cam_err("err : %d", err);

	err = jc_mem_write(0x08, SZ_64,
			0x90001100 , buf_port_seting2_m10mo);

	CAM_DEBUG("E");

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(JC_SIO_LOADER_PATH_M10MO, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		cam_err("failed to open %s, err %ld\n",
			JC_SIO_LOADER_PATH_M10MO, PTR_ERR(fp));
		goto out;
	}

	fsize = fp->f_path.dentry->d_inode->i_size;
	count = fsize / SZ_4K;

	cam_err("start, file path %s, size %ld Bytes, count %d\n",
		JC_SIO_LOADER_PATH_M10MO, fsize, count);

	buf_m9mo = vmalloc(fsize);
	if (!buf_m9mo) {
		cam_err("failed to allocate memory\n");
		err = -ENOMEM;
		goto out;
	}

	nread = vfs_read(fp, (char __user *)buf_m9mo, fsize, &fp->f_pos);
	if (nread != fsize) {
		cam_err("failed to read firmware file, %ld Bytes\n", nread);
		err = -EIO;
		goto out;
	}

	/* Send SIO Loader Program */
	err = jc_mem_write(0x04, SZ_4K,
			0x01000100 , buf_m9mo);

	cam_err("err : %d", err);

	err = jc_mem_write(0x04, SZ_4K,
			0x01001100 , buf_m9mo+SZ_4K);

	cam_err("err : %d", err);

	err = jc_mem_write(0x04, SZ_4K,
			0x01002100 , buf_m9mo+SZ_8K);

	cam_err("err : %d", err);

	err = jc_mem_write(0x04, fsize-(SZ_4K|SZ_8K),
			0x01003100 , buf_m9mo+(SZ_4K|SZ_8K));

	/* Set start address of SIO Loader Program */
	err = jc_writel(JC_CATEGORY_FLASH,
			0x0C, 0x01000100);

	cam_err("err : %d", err);

	/* Start SIO Loader Program */
	err = jc_writeb(JC_CATEGORY_FLASH, 0x12, 0x02);

	cam_err("err : %d", err);

out:

	if (buf_m9mo)
		vfree(buf_m9mo);

	msleep(3);

	if (!IS_ERR(fp))
		filp_close(fp, current->files);

	set_fs(old_fs);
	return 0;
}

static int jc_get_sensor_version(void)
{
	int i;
	int err;
	int sensor_ver[11];
	u8 sensor_type[] = "M10MO";

	/* Read Sensor Ver String */

	cam_err("Read Sensor Ver String : 11 byte");

	if (jc_ctrl->is_isp_null == true) {
		for (i = 0 ; i < 11 ; i++) {
			err = jc_readb(JC_CATEGORY_FLASH, 0x2C + i, &sensor_ver[i]);
		}
	} else {
		for (i = 0 ; i < 11 ; i++) {
			err = jc_readb(0x00, 0x32 + i, &sensor_ver[i]);
		}
	}

	sprintf(sysfs_sensor_fw_str, "%c%c%c%c%c%c%c%c%c%c%c",
		sensor_ver[0], sensor_ver[1], sensor_ver[2], sensor_ver[3],
		sensor_ver[4], sensor_ver[5], sensor_ver[6], sensor_ver[7],
		sensor_ver[8], sensor_ver[9], sensor_ver[10]);

	sprintf(sysfs_sensor_type, "%s", sensor_type);

	cam_err("Sensor version : %s\n", sysfs_sensor_fw_str);
	cam_err("Sensor type : %s\n", sysfs_sensor_type);

	return 0;
}

static int jc_check_sensor_phone(void)
{
	cam_err("Compare Sensor with Phone FW version");

	if ((strncmp(sysfs_sensor_fw_str, "S13F0", 5) != 0)
	    && (strncmp(sysfs_sensor_fw_str, "O13F0", 5) != 0)) {
		cam_err("sensor version is wrong, skip!");
		return -1;
	} else if (strncmp(sysfs_sensor_fw_str, sysfs_phone_fw_str, 6) != 0) {
		cam_err("Another sensor module is detected.");
		return 1;
	} else
		return strcmp(sysfs_sensor_fw_str, sysfs_phone_fw_str);
}

static int jc_check_sensor_isp(void)
{
	cam_err("Compare Sensor with ISP FW version\n");

	if ((strncmp(sysfs_isp_fw_str, "S13F0", 5) != 0)
	    && (strncmp(sysfs_isp_fw_str, "O13F0", 5) != 0)) {
		cam_err("isp version is wrong, skip!");
		return 1;
	} else if ((strncmp(sysfs_sensor_fw_str, "S13F0", 5) != 0)
	    && (strncmp(sysfs_sensor_fw_str, "O13F0", 5) != 0)) {
		cam_err("sensor version is wrong, skip!");
		return -1;
	} else if (strncmp(sysfs_sensor_fw_str, sysfs_isp_fw_str, 6) != 0) {
		cam_err("Another sensor module is detected.");
		return 1;
	} else
		return strcmp(sysfs_sensor_fw_str, sysfs_isp_fw_str);
}

static int jc_check_isp_phone(void)
{
	cam_err("Compare ISP with Phone FW version\n");

	if ((strncmp(sysfs_isp_fw_str, "S13F0", 5) != 0)
	    && (strncmp(sysfs_isp_fw_str, "O13F0", 5) != 0)) {
		cam_err("isp version is wrong, skip!");
		return -1;
	} else if (strncmp(sysfs_isp_fw_str, sysfs_phone_fw_str, 6) != 0) {
		cam_err("Another sensor module is detected.");
		return -1;
	} else
		return strcmp(sysfs_isp_fw_str, sysfs_phone_fw_str);
}

static int jc_check_chip_erase(void)
{
      int val, chip_erase;
      int ret = -1;
      int retries;


	cam_info("Check chip erased or not");

	jc_writeb(JC_CATEGORY_FLASH,
			0x5B, 0x01);

	/* Checksum */
	jc_writeb(JC_CATEGORY_FLASH, 0x09, 0x02);

	retries = 0;
	do {
		msleep(300);
		jc_readb(JC_CATEGORY_FLASH,
			0x09, &val);

		if (val == 0)
			break;
	} while (val == 0x02 && retries++ < JC_I2C_VERIFY);

	/* Get Checksum of ISP */
	jc_readw(JC_CATEGORY_FLASH, 0x0A, &val);

      jc_readb(JC_CATEGORY_FLASH,
			0x13, &chip_erase);

      if (chip_erase == 0x00) {
	  	cam_info("FlashROM is blank");
		ret = 0;
      	} else if (chip_erase == 0x01) {
	  	cam_info("FlashROM is NOT blank");
		ret = 1;
      	}

      return ret;
}

static int jc_check_sensor_validation(void)
{
      cam_info("Check sensor validation JF sensor or NOT");

	if ((strncmp(sysfs_sensor_fw_str, "S13L0", 5) == 0)
	    || (strncmp(sysfs_sensor_fw_str, "O13L0", 5) == 0)) {
             cam_err("JA sensor is detected, fail!!");
		return 0;
	} else
	      return 1;
}

static int jc_get_isp_version(void)
{
	int err;
	u8 isp_ver[22] = {0, };

	/* Read Sensor Ver String */

	cam_err("Read Sensor Ver String : 11 byte");

	err = jc_writeb(JC_CATEGORY_FLASH, 0x57, 0x01);

	err = jc_mem_read(11, 0x181EF080, isp_ver);

	sprintf(sysfs_isp_fw_str, "%s", isp_ver);

	err = jc_writeb(JC_CATEGORY_FLASH, 0x57, 0x00);

	if ((strncmp(sysfs_isp_fw_str, "S13F0", 5) != 0)
	    && (strncmp(sysfs_isp_fw_str, "O13F0", 5) != 0))  {
		cam_err("isp version is wrong, skip!");
		jc_ctrl->is_isp_null = true;
		return -1;
	} else {
		jc_ctrl->is_isp_null = false;
		cam_err("ISP version : %s\n", sysfs_isp_fw_str);
		return 0;
	}
}

static int jc_get_phone_version(void)
{
	int err = 0;
	int ret = -1;

	struct file *fp = NULL;
	mm_segment_t old_fs;
	long nread;

	cam_info("E\n");

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(JC_M10MO_FW_PATH_SD, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		cam_err("failed to open %s, err %ld\n", JC_M10MO_FW_PATH_SD, PTR_ERR(fp));
		firmware_update_sdcard = false;
#if 1
		goto open_fail;
#else
		if (strncmp(sysfs_sensor_fw_str, "S13F0S", 6) == 0) {
			jc_ctrl->sensor_combination = 0;
			fp = filp_open(JC_M10MO_FW_PATH_SS, O_RDONLY, 0);
		} else if (strncmp(sysfs_sensor_fw_str, "O13F0S", 6) == 0) {
			jc_ctrl->sensor_combination = 1;
			fp = filp_open(JC_M10MO_FW_PATH_OS, O_RDONLY, 0);
		} else if (strncmp(sysfs_sensor_fw_str, "S13F0L", 6) == 0) {
			jc_ctrl->sensor_combination = 2;
			fp = filp_open(JC_M10MO_FW_PATH_SL, O_RDONLY, 0);
		} else if (strncmp(sysfs_sensor_fw_str, "O13F0L", 6) == 0) {
			jc_ctrl->sensor_combination = 3;
			fp = filp_open(JC_M10MO_FW_PATH_OL, O_RDONLY, 0);
		} else {
			jc_ctrl->sensor_combination = 0;
			fp = filp_open(JC_M10MO_FW_PATH_SS, O_RDONLY, 0);
		}

		if (IS_ERR(fp)) {
			if (jc_ctrl->sensor_combination == 0)
				cam_err("failed to open %s, err %ld\n", JC_M10MO_FW_PATH_SS, PTR_ERR(fp));
			else if (jc_ctrl->sensor_combination == 1)
				cam_err("failed to open %s, err %ld\n", JC_M10MO_FW_PATH_OS, PTR_ERR(fp));
			else if (jc_ctrl->sensor_combination == 2)
				cam_err("failed to open %s, err %ld\n", JC_M10MO_FW_PATH_SL, PTR_ERR(fp));
			else if (jc_ctrl->sensor_combination == 3)
				cam_err("failed to open %s, err %ld\n", JC_M10MO_FW_PATH_OL, PTR_ERR(fp));
			else
				cam_err("failed to open %s, err %ld\n", JC_M10MO_FW_PATH_SS, PTR_ERR(fp));

			goto out;
		} else {
			cam_info("FW File(phone) opened.\n");
		}
#endif
	} else {
		cam_info("FW File(SD card) opened.\n");
		firmware_update_sdcard = true;
	}

	/* Read Version String addr */
	err = vfs_llseek(fp, JC_FW_VER_STR, SEEK_SET);
	if (err < 0) {
		cam_err("failed to fseek, %d\n", err);
		goto out;
	}

	nread = vfs_read(fp, (char __user *)jc_ctrl->phone_ver_str,
			JC_FW_VER_STR_LEN, &fp->f_pos);

	if (nread != JC_FW_VER_STR_LEN) {
		cam_err("failed to read firmware file, %ld Bytes\n", nread);
		err = -EIO;
		goto out;
	}

	sprintf(sysfs_phone_fw_str, "%c%c%c%c%c%c%c%c%c%c%c",
		*jc_ctrl->phone_ver_str, *(jc_ctrl->phone_ver_str+1),
		*(jc_ctrl->phone_ver_str+2), *(jc_ctrl->phone_ver_str+3),
		*(jc_ctrl->phone_ver_str+4), *(jc_ctrl->phone_ver_str+5),
		*(jc_ctrl->phone_ver_str+6), *(jc_ctrl->phone_ver_str+7),
		*(jc_ctrl->phone_ver_str+8), *(jc_ctrl->phone_ver_str+9),
		*(jc_ctrl->phone_ver_str+10));

	cam_err("Phone version : %s\n", sysfs_phone_fw_str);

	filp_close(fp, current->files);

	set_fs(old_fs);

	ret = 0;
	return ret;

out:
	filp_close(fp, current->files);

open_fail:
	set_fs(old_fs);

	return ret;
}

static int jc_isp_boot(void)
{
	int err;
	u32 int_factor;
	u8 flash_controller[] = {0x7f};

	if (isp_version == 0x00) {
		err = jc_writel(JC_CATEGORY_FLASH, 0x0C, 0x27C00020);
	} else if (isp_version == 0x10) {
		err = jc_mem_write(0x04, 1, 0x13000005 , flash_controller);
		msleep(1);
	}

	/* start camera program(parallel FLASH ROM) */
	cam_info("start camera program\n");
	err = jc_writeb(JC_CATEGORY_FLASH, JC_FLASH_CAM_START, 0x01);

	int_factor = jc_wait_interrupt(JC_ISP_TIMEOUT);
	if (!(int_factor & JC_INT_MODE)) {
		cam_err("firmware was erased?\n");
		jc_ctrl->isp.bad_fw = 1;
		/* return -ENOSYS; */
	}
	cam_info("ISP boot complete\n");
	return 0;
}

void jc_isp_reset(void)
{
	CAM_DEBUG("E");

	mipi_camera_power_enable(0);

	msleep(3);

	mipi_camera_power_enable(1);

	jc_ctrl->isp.issued = false;
	if (wait_event_interruptible_timeout(jc_ctrl->isp.wait, jc_ctrl->isp.issued, msecs_to_jiffies(1000)) == 0) {
		printk(KERN_ERR "##[%s():%s:%d\t] time out!!! \n", __FUNCTION__, strrchr(__FILE__, '/')+1, __LINE__);
	}
}



static u32 jc_wait_interrupt(unsigned int timeout)
{
	int i = 0;

	CAM_DEBUG("E");

	if (wait_event_interruptible_timeout(jc_ctrl->isp.wait,
		jc_ctrl->isp.issued == 1,
		msecs_to_jiffies(timeout)) == 0) {
		cam_err("timeout ~~~~~~~~~~~~~~~~~~~~~\n");
		return 0;
	}

	jc_ctrl->isp.issued = 0;

	jc_readw(JC_CATEGORY_SYS, JC_SYS_INT_FACTOR, &jc_ctrl->isp.int_factor);
	cam_err("jc_wait_interrupt : jc_ctrl->isp.int_factor = %x\n", jc_ctrl->isp.int_factor);
	for (i = 0 ; i < 10 ; i++) {
		if (jc_ctrl->isp.int_factor != 0x100) {
			msleep(30);
			jc_readw(JC_CATEGORY_SYS, JC_SYS_INT_FACTOR, &jc_ctrl->isp.int_factor);
			cam_err("jc_wait_interrupt retry[%d] : jc_ctrl->isp.int_factor = %x\n", i, jc_ctrl->isp.int_factor);
		} else {
			cam_err("jc_wait_interrupt : jc_ctrl->isp.int_factor = %x\n", jc_ctrl->isp.int_factor);
			break;
		}
	}

	CAM_DEBUG("X");
	return jc_ctrl->isp.int_factor;
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

		state->isp.issued = false;
		if (wait_event_interruptible_timeout(state->isp.wait, state->isp.issued, msecs_to_jiffies(1000)) == 0) {
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

		state->isp.issued = false;
		if (wait_event_interruptible_timeout(state->isp.wait, state->isp.issued, msecs_to_jiffies(1000)) == 0) {
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

static int m10mo_power_reset(void)
{
	int ret = 0;

	mipi_camera_power_enable(0);
	mipi_camera_power_enable(1);

	jc_ctrl->isp.issued = false;
	if (wait_event_interruptible_timeout(jc_ctrl->isp.wait, jc_ctrl->isp.issued, msecs_to_jiffies(1000)) == 0) {
		printk(KERN_ERR "##[%s():%s:%d\t] time out!!! \n", __FUNCTION__, strrchr(__FILE__, '/')+1, __LINE__);
	}

	return ret;
}

static ssize_t m10mo_sys_fw_update(struct device *dev, struct device_attribute *attr, char *buf)
{
	//struct i2c_client *client = to_i2c_client(dev);
	//struct m10mo_state *state = jc_ctrl;

	int result = 0;
	u8 data[255];
	int ret = 0;

	int rc = 0;
	int err;
	int result_sensor_phone, result_isp_phone, result_sensor_isp;
	int isp_revision = 0;
	int isp_ret;

	dev_dbg(&jc_ctrl->i2c_client->dev, "%s [START]\n", __func__);

	//Update firmware
	//ret = m10mo_fw_update_from_storage(jc_ctrl, jc_ctrl->fw_path_ext, true);

	firmware_update = false;
	firmware_update_sdcard = false;
	jc_ctrl->burst_mode = false;
	jc_ctrl->stream_on = false;
	jc_ctrl->torch_mode = false;
	jc_ctrl->movie_mode = false;
	jc_ctrl->anti_stream_off = false;
	jc_ctrl->need_restart_caf = false;
	jc_ctrl->is_isp_null = false;
	jc_ctrl->isp_null_read_sensor_fw = false;
	jc_ctrl->touch_af_mode = false;
	jc_ctrl->fw_retry_cnt = 0;

	//jc_ctrl->runmode = M10MO_FIRMWAREMODE;
#ifdef CONFIG_PLAT_S5P4418_RETRO
	mipi_camera_power_enable(1);
#endif

	jc_ctrl->isp.issued = false;
	if (wait_event_interruptible_timeout(jc_ctrl->isp.wait, jc_ctrl->isp.issued, msecs_to_jiffies(1000)) == 0) {
		printk(KERN_ERR "##[%s():%s:%d\t] time out!!! \n", __FUNCTION__, strrchr(__FILE__, '/')+1, __LINE__);
	}

#if 0//ndef JC_SPI_WRITE
	jc_load_fw_main();
#endif

	printk(KERN_ERR "## \e[31m PJSMSG \e[0m [%s():%s:%d\t] version_checked:%d, fw_update:%d \n", __FUNCTION__, strrchr(__FILE__, '/')+1, __LINE__, version_checked, jc_ctrl->fw_update);

#if 1// PJSIN 20170314 add-- [ 1

	jc_load_SIO_fw();

#else
	if (version_checked == false && jc_ctrl->fw_update == true) {
		isp_ret = jc_get_isp_version();
		jc_isp_boot();
		jc_get_sensor_version();
		jc_get_phone_version();

		if(!jc_check_sensor_validation() /*|| !jc_check_sensor_spi_port()*/) {
			mipi_camera_power_enable(0);
			return -ENOSYS;
		}

		cam_info("isp_ret: %d, samsung app: %d, factory bin: %d\n",
		    isp_ret, jc_ctrl->samsung_app, jc_ctrl->factory_bin);

		if (isp_ret == 0 && jc_ctrl->samsung_app == false && jc_ctrl->factory_bin == false) {
		    cam_err("3rd party app. skip ISP FW update\n");
		    goto start;
		}

		jc_ctrl->fw_update = false;

		if (firmware_update_sdcard == true) {
			cam_info("FW in sd card is higher priority than others!\n");

			result_isp_phone = jc_check_isp_phone();

			if (result_isp_phone != 0) {
				cam_info("ISP FW and SD card FW are different, Force Write!\n");
				jc_isp_reset();
				jc_load_SIO_fw();
				jc_phone_fw_to_isp();
				jc_isp_reset();
				jc_get_isp_version();
				jc_isp_boot();
			} else
				cam_info("ISP FW and SD card FW are same, Do not anything!\n");
		} else if (isp_ret < 0) {
                       cam_info("ISP FW is wrong, need to write from sensor or phone\n");
			    jc_isp_reset();
                       jc_load_SIO_fw();
                       jc_get_sensor_version();

                       if(!jc_check_sensor_validation() /*|| !jc_check_sensor_spi_port()*/) {
                           mipi_camera_power_enable(0);
                           return -ENOSYS;
                       }
                       result_sensor_phone = jc_check_sensor_phone();

                       if (result_sensor_phone > 0) {
                           cam_info("Sensor > Phone, update from sensor\n");
						jc_ctrl->isp_null_read_sensor_fw = true;
                           jc_read_from_sensor_fw();
                           jc_isp_reset();
                           jc_get_isp_version();
                           jc_isp_boot();

                       } else {
                           cam_info("Sensor < Phone, update from Phone\n");
                           jc_phone_fw_to_isp();
                           jc_isp_reset();
                           jc_get_isp_version();
                           jc_isp_boot();
                       }
                   }else {
			cam_info("ISP FW is normal, need to compare with sensor/phone");
			result_sensor_phone = jc_check_sensor_phone();

			if (result_sensor_phone > 0) {
				cam_info("Sensor > Phone, compare with ISP\n");
				result_sensor_isp = jc_check_sensor_isp();

				if (result_sensor_isp > 0) {
					cam_info("Sensor > ISP, update from sensor\n");
					jc_isp_reset();
					jc_load_SIO_fw();
					jc_read_from_sensor_fw();
					jc_isp_reset();
					jc_get_isp_version();
					jc_isp_boot();
				} else
					cam_info("Sensor <= ISP, Do not anything\n");
			} else {
				cam_info("Sensor < Phone, compare with ISP\n");

			result_isp_phone = jc_check_isp_phone();

			if (result_isp_phone < 0) {
				cam_info("Phone > ISP, update from Phone\n");
				jc_isp_reset();
				jc_load_SIO_fw();
				jc_phone_fw_to_isp();
				jc_isp_reset();
				jc_get_isp_version();
				jc_isp_boot();
			} else
				cam_info("Phone <= ISP, Do not anything\n");
			}
		}
	}else {
		jc_isp_boot();

		if(!jc_check_sensor_validation() /*|| !jc_check_sensor_spi_port()*/) {
			mipi_camera_power_enable(0);
			return -ENOSYS;
		}
	}

start:
	cam_info("nv12 output setting\n");
	err = jc_writeb(JC_CATEGORY_CAPCTRL,
			0x0, 0x0f);

	if (jc_ctrl->samsung_app != 1) {
		cam_info("Set different ratio capture mode\n");
		jc_set_different_ratio_capture(1);
	}

	err = jc_readb(0x01, 0x3F, &isp_revision);
	cam_info("isp revision : 0x%x\n", isp_revision);

	cam_info("Sensor version : %s\n", sysfs_sensor_fw_str);
	cam_info("ISP version : %s\n", sysfs_isp_fw_str);
	cam_info("Phone version : %s\n", sysfs_phone_fw_str);
	CAM_DEBUG("X");

	//return rc;
#endif// ]-- end

#if 0// PJSIN 20170314 add-- [ 1
	jc_get_isp_version();

	jc_get_sensor_version();

	ret = jc_get_phone_version();

#endif// ]-- end

	jc_ctrl->runmode = M10MO_RUNMODE_NOTREADY;

#ifdef CONFIG_PLAT_S5P4418_RETRO
	mipi_camera_power_enable(0);
#endif

	switch (ret) {
	case 0:
		sprintf(data, "F/W update success.\n");
		break;

	default:
		sprintf(data, "F/W update failed.\n");
		break;
	}

	dev_dbg(&jc_ctrl->i2c_client->dev, "%s [DONE]\n", __func__);

	result = snprintf(buf, 255, "%s\n", data);
	return result;
}

static ssize_t m10mo_sys_fw_path(struct device *dev, struct device_attribute *attr, char *buf)
{
	//struct i2c_client *client = to_i2c_client(dev);
	//struct m10mo_state *state = jc_ctrl;
	int result = 0;
	u8 data[255];

	dev_dbg(&jc_ctrl->i2c_client->dev, "%s [START]\n", __func__);

	sprintf(data, "Firmware Path : %s \n", JC_M10MO_FW_PATH_SD);

	dev_dbg(&jc_ctrl->i2c_client->dev, "%s [DONE]\n", __func__);

	result = snprintf(buf, 255, "%s\n", data);
	return result;
}

static DEVICE_ATTR(fw_update, S_IRUGO, m10mo_sys_fw_update, NULL);
static DEVICE_ATTR(fw_path, S_IRUGO, m10mo_sys_fw_path, NULL);


static struct attribute *m10mo_attrs[] = {
	&dev_attr_fw_path.attr,
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

	printk(KERN_ERR "## \e[31m PJSMSG \e[0m [%s():%s:%d\t] state->runmode:%d \n", __FUNCTION__, strrchr(__FILE__, '/')+1, __LINE__, state->runmode);

	if (state->runmode == M10MO_FIRMWAREMODE) {
		;
	} else if (state->runmode == M10MO_RUNMODE_NOTREADY) {
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

	state->isp.issued = true;
	wake_up_interruptible(&state->isp.wait);

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

	jc_ctrl = state;

	jc_client = client;
	jc_dev = client->dev;

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

	state->isp.issued = false;
	init_waitqueue_head(&state->isp.wait);

	//Create sysfs
	if (sysfs_create_group(&client->dev.kobj, &m10mo_attr_group)) {
		dev_err(&client->dev, "%s [ERROR] sysfs_create_group\n", __func__);
	}
	if (sysfs_create_link(NULL, &client->dev.kobj, "m10mo")) {
		dev_err(&client->dev, "%s [ERROR] sysfs_create_link\n", __func__);
	}

	jc_ctrl->fw_update = true;


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

