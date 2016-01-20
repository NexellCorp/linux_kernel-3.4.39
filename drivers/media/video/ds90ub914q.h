#ifndef __DS90UB914Q_H__
#define	__DS90UB914Q_H__

#include <mach/soc.h>
#include <linux/regulator/machine.h>

#include "ds90ub914q-preset.h"

#define PREVIEW_MODE	0
#define CAPTURE_MODE	1

#define I2C_RETRY_CNT	5

struct reg_val {
    uint8_t reg;
    uint8_t val;
};

#define END_MARKER  {0xff, 0xff}


#if 0
struct ds90ub914q_state {
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_ctrl_handler handler;
	
	/* standard */
	struct v4l2_ctrl *focus;
	struct v4l2_ctrl *wb;
	struct v4l2_ctrl *color_effect;
	struct v4l2_ctrl *exposure;

	/* custom */
	struct v4l2_ctrl *scene_mode;
	struct v4l2_ctrl *anti_shake;
	struct v4l2_ctlr *mode_change;

	bool inited;
	int width;
	int height;
	int mode;	//PREVIEW or CAPTURE

	/* for zoom */
	struct v4l2_rect crop;
	
	struct regulator *cam_core_18V;
	struct regulator *cam_io_28V;

	struct workqueue_struct *init_wq;
	struct work_struct init_work;	
};
#else
struct dev_state {
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_ctrl_handler handler;

	struct i2c_client *camera_client;
	struct i2c_client *ser_client;
	struct i2c_client *des_client;
	
	/* standard */
	struct v4l2_ctrl *ctrl_brightness;
#if 0 
	struct v4l2_ctrl *focus;
	struct v4l2_ctrl *wb;
	struct v4l2_ctrl *color_effect;
	struct v4l2_ctrl *exposure;
#endif

	/* custom */
	struct v4l2_ctrl *ctrl_mux;
	struct v4l2_ctrl *ctrl_status;
#if 0
	struct v4l2_ctrl *scene_mode;
	struct v4l2_ctrl *anti_shake;
	struct v4l2_ctlr *mode_change;
#endif

	bool inited;
	int width;
	int height;
	int mode;	//PREVIEW or CAPTURE

#if 0
	/* for zoom */
	struct v4l2_rect crop;
	
	struct regulator *cam_core_18V;
	struct regulator *cam_io_28V;

	struct workqueue_struct *init_wq;
	struct work_struct init_work;	
#endif
};

#endif

#endif
