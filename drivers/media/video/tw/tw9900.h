#ifndef __TW9900_H__
#define __TW9900_H__

#include <mach/soc.h>

#define I2C_RETRY_CNT 5

struct reg_val {
    uint8_t reg;
    uint8_t val;
};

#define DEBUG_TW9900
#define I2C_READ_CHECK 1

#ifdef DEBUG_TW9900
    #define vmsg(a...)  printk(KERN_ERR a)
#else
    #define vmsg(a...)
#endif

struct dev_state    {
    struct media_pad pad;
    struct v4l2_subdev sd;

    bool first;

    int mode;
    int width;
    int height;

    struct i2c_client *i2c_client;
    struct v4l2_ctrl_handler handler;

    /* common control */
    struct v4l2_ctrl *ctrl_status;

    /* standard control */
    struct v4l2_ctrl *ctrl_brightness;
    char brightness;

    /* worker */
    struct work_struct work;
};

#endif /*__TW9900_PRESET_H__ */
