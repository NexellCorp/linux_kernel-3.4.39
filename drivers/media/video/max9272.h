#ifndef __MAX9272_H__
#define __MAX9272_H__

#include <mach/soc.h>

#define I2C_RETRY_CNT 5

struct reg_val {
	uint8_t reg;
	uint8_t val;
};

//#define DEBUG_MAX9272
#define I2C_READ_CHECK 0

#ifdef DEBUG_MAX9272
#define vmsg(a...)  printk(KERN_ERR a)
#else
#define vmsg(a...)
#endif

#define DELAY	0xFE
#define WQ_DELAY_DETECTION 1000

#define MAX9272_I2C_CHECK_LOCK 0x04
#define MAX9272_I2C_CHECK_ERROR 0x10

struct dev_state    {
	struct media_pad pad;
	struct v4l2_subdev sd;
	struct v4l2_ctrl_handler handler;

	bool first;

	int mode;
	int width;
	int height;

	struct i2c_client *sensor_client;
	struct i2c_client *ser_client;
	struct i2c_client *des_client;

	/* common control */
	struct v4l2_ctrl *ctrl_status;

	/* standard control */
	struct v4l2_ctrl *ctrl_brightness;
	char brightness;

	/* worker */
	struct work_struct work;
};

#endif
