#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/switch.h>
#include <linux/delay.h>

#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>

#include <mach/platform.h>
#include <mach/soc.h>

/*#define DEBUG_TW9900*/
#ifdef DEBUG_TW9900
#define vmsg(a...)  printk(a)
#else
#define vmsg(a...)
#endif

//#define BRIGHTNESS_TEST
#define DEFAULT_BRIGHTNESS  0x1e
struct tw9900_state {
    struct media_pad pad;
    struct v4l2_subdev sd;
    struct switch_dev switch_dev;
    bool first;

    struct i2c_client *i2c_client;

    struct v4l2_ctrl_handler handler;
    /* custom control */
    struct v4l2_ctrl *ctrl_mux;
    struct v4l2_ctrl *ctrl_status;
    /* standard control */
    struct v4l2_ctrl *ctrl_brightness;
    char brightness;

    /* nexell: detect worker */
    /*struct work_struct work;*/
    struct delayed_work work;
};

struct reg_val {
    uint8_t reg;
    uint8_t val;
};

#define END_MARKER {0xff, 0xff}

static struct reg_val _sensor_init_data[] =
{
    {0x02, 0x40}, //MUX0
//    {0x02, 0x44}, //MUX1
    {0x03, 0xa2},
    {0x07, 0x02},
    {0x08, 0x12},
    {0x09, 0xf0},
    {0x0a, 0x1c},
    /*{0x0b, 0xd0}, // 720 */
    {0x0b, 0xc0}, // 704
    {0x1b, 0x00},
    /*{0x10, 0xfa},*/
    {0x10, 0x1e},
    {0x11, 0x64},
    {0x2f, 0xe6},
    {0x55, 0x00},
#if 1
    /*{0xb1, 0x20},*/
    /*{0xb1, 0x02},*/
    {0xaf, 0x00},
    {0xb1, 0x20},
    {0xb4, 0x20},
    /*{0x06, 0x80},*/
#endif
    /*{0xaf, 0x40},*/
    /*{0xaf, 0x00},*/
    /*{0xaf, 0x80},*/
    END_MARKER
};

#define MUX0					0
#define MUX1 					1
#define SV_DET					0x16
//#define REARCAM_BACKDETECT_TIME	300
#define REARCAM_BACKDETECT_TIME	10
#define REARCAM_MPOUT_TIME		300

/* this value is used to detect on/off for back gear switch by uevent. */
struct switch_dev *backgear_switch = NULL;

static struct tw9900_state _state;

#ifdef CONFIG_SLSIAP_BACKWARD_CAMERA
extern int get_backward_module_num(void);
#endif

static int rearcam_brightness_tbl[12] = {30, -15, -12, -9, -6, -3, 0, 10, 20, 30, 35, 40};

void tw9900_external_set_brightness(char brightness) {
    _state.brightness = brightness;
}
EXPORT_SYMBOL(tw9900_external_set_brightness);


static irqreturn_t _irq_handler(int, void *);

#if defined(CONFIG_SLSIAP_BACKWARD_CAMERA)
int register_backward_irq_tw9900(void)
{
    int ret=0;

#if 0
    _state.switch_dev.name = "rearcam";
    switch_dev_register(&_state.switch_dev);
    switch_set_state(&_state.switch_dev, 0);

    backgear_switch = &_state.switch_dev;
#endif

    ret = request_irq(IRQ_GPIO_START + CFG_BACKWARD_GEAR, _irq_handler, IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, "tw9900", &_state);

    if (ret<0) {
        pr_err("%s: failed to request_irq(irqnum %d), ret : %d\n", __func__, IRQ_ALIVE_1, ret);
        return -1;
    }

    return 0;
}
#endif

/**
 * util functions
 */
static inline struct tw9900_state *ctrl_to_me(struct v4l2_ctrl *ctrl)
{
    return container_of(ctrl->handler, struct tw9900_state, handler);
}

#define THINE_I2C_RETRY_CNT				3
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

static int _i2c_write_byte(struct i2c_client *client, u8 addr, u8 val)
{
	s8 i = 0;
	s8 ret = 0;
	u8 buf[2];
	struct i2c_msg msg;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = buf;

	buf[0] = addr;
	buf[1] = val ;

	for(i=0; i<THINE_I2C_RETRY_CNT; i++) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (likely(ret == 1))
			break;
	}

	if (ret != 1) {
        printk(KERN_ERR "%s: failed to write addr 0x%x, val 0x%x\n", __func__, addr, val);
		return -EIO;
	}

	return 0;
}

#define V4L2_CID_MUX        (V4L2_CTRL_CLASS_USER | 0x1001)
#define V4L2_CID_STATUS     (V4L2_CTRL_CLASS_USER | 0x1002)

static int tw9900_set_mux(struct v4l2_ctrl *ctrl)
{
    struct tw9900_state *me = ctrl_to_me(ctrl);
    if (ctrl->val == 0) {
        // MUX 0
        if (rearcam_brightness_tbl[me->brightness] != DEFAULT_BRIGHTNESS)
            _i2c_write_byte(me->i2c_client, 0x10, DEFAULT_BRIGHTNESS);
        _i2c_write_byte(me->i2c_client, 0x02, 0x40);
    } else {
        // MUX 1
        if (rearcam_brightness_tbl[me->brightness] != DEFAULT_BRIGHTNESS)
            _i2c_write_byte(me->i2c_client, 0x10, rearcam_brightness_tbl[me->brightness]);
        _i2c_write_byte(me->i2c_client, 0x02, 0x44);
    }

    return 0;
}

static int tw9900_set_brightness(struct v4l2_ctrl *ctrl)
{
    struct tw9900_state *me = ctrl_to_me(ctrl);

#ifdef BRIGHTNESS_TEST
		printk(KERN_ERR "%s: brightness = %d\n", __func__, ctrl->val);
		_i2c_write_byte(me->i2c_client, 0x10, ctrl->val);
#else	
    if (ctrl->val != me->brightness) {
		printk(KERN_ERR "%s: brightness = %d\n", __func__, rearcam_brightness_tbl[ctrl->val]);
        _i2c_write_byte(me->i2c_client, 0x10, rearcam_brightness_tbl[ctrl->val]);

        me->brightness = ctrl->val;
    }
#endif
    return 0;
}

static bool _is_backgear_on(void);
static bool _is_camera_on(void);

static int tw9900_get_status(struct v4l2_ctrl *ctrl)
{
    struct tw9900_state *me = ctrl_to_me(ctrl);
    u8 data = 0;
    u8 mux;
    u8 val = 0;

    _i2c_read_byte(me->i2c_client, 0x02, &data);
    mux = (data & 0x0c) >> 2;

    if (mux == 0) {
        // AV IN
        _i2c_read_byte(me->i2c_client, 0x01, &data);
        if (!(data & 0x80))
            val |= 1 << 0;
        // Rearcam IN
        if (_is_backgear_on()) {
            _i2c_read_byte(me->i2c_client, 0x16, &data);
            if (data & 0x40)
                val |= 1 << 1;
        }
        //printk("%s: mux 0 --> 0x%x\n", __func__, val);
    } else {
        // Rearcam IN
#if 0
        if (switch_get_state(&me->switch_dev))
            val |= 1 << 1;
#else
//        if (_is_backgear_on()) {
            _i2c_read_byte(me->i2c_client, 0x01, &data);
            if (!(data & 0x80))
                val |= 1 << 1;
//        }
#endif
        //printk("%s: mux 1 --> 0x%x\n", __func__, val);
    }

    ctrl->val = val;

    return 0;
}

static int tw9900_s_ctrl(struct v4l2_ctrl *ctrl)
{
    switch (ctrl->id) {
    case V4L2_CID_MUX:
        return tw9900_set_mux(ctrl);
    case V4L2_CID_BRIGHTNESS:
        return tw9900_set_brightness(ctrl);
    default:
        printk(KERN_ERR "%s: invalid control id 0x%x\n", __func__, ctrl->id);
        return -EINVAL;
    }
}

static int tw9900_g_volatile_ctrl(struct v4l2_ctrl *ctrl)
{
    switch (ctrl->id) {
    case V4L2_CID_STATUS:
        return tw9900_get_status(ctrl);
    default:
        printk(KERN_ERR "%s: invalid control id 0x%x\n", __func__, ctrl->id);
        return -EINVAL;
    }
}

static const struct v4l2_ctrl_ops tw9900_ctrl_ops = {
     .s_ctrl = tw9900_s_ctrl,
     .g_volatile_ctrl = tw9900_g_volatile_ctrl,
};

static const struct v4l2_ctrl_config tw9900_custom_ctrls[] = {
    {
        .ops  = &tw9900_ctrl_ops,
        .id   = V4L2_CID_MUX,
        .type = V4L2_CTRL_TYPE_INTEGER,
        .name = "MuxControl",
        .min  = 0,
        .max  = 1,
        .def  = 1,
        .step = 1,
    },
    {
        .ops  = &tw9900_ctrl_ops,
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
static int tw9900_initialize_ctrls(struct tw9900_state *me)
{
    v4l2_ctrl_handler_init(&me->handler, NUM_CTRLS);

    me->ctrl_mux = v4l2_ctrl_new_custom(&me->handler, &tw9900_custom_ctrls[0], NULL);
    if (!me->ctrl_mux) {
         printk(KERN_ERR "%s: failed to v4l2_ctrl_new_custom for mux\n", __func__);
         return -ENOENT;
    }

    me->ctrl_status = v4l2_ctrl_new_custom(&me->handler, &tw9900_custom_ctrls[1], NULL);
    if (!me->ctrl_status) {
         printk(KERN_ERR "%s: failed to v4l2_ctrl_new_custom for status\n", __func__);
         return -ENOENT;
    }

    me->ctrl_brightness = v4l2_ctrl_new_std(&me->handler, &tw9900_ctrl_ops,
            V4L2_CID_BRIGHTNESS, -128, 127, 1, 0x1e);
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

static inline bool _is_backgear_on(void)
{
    int val = nxp_soc_gpio_get_in_value(CFG_BACKWARD_GEAR);

    if (!val)
    {
    	//NX_GPIO_SetOutputValue(PAD_GET_GROUP(CFG_IO_CAM_PWR_EN), PAD_GET_BITNO(CFG_IO_CAM_PWR_EN), 1);
		//_i2c_write_byte(_state.i2c_client, 0x02, 0x44);
        return true;
    }
	else
    	return false;
}

static inline bool _is_camera_on(void)
{
    // read status
	u8 data;
	u8 cin0;

#if 0
	extern int mux_status;
#endif
	
    _i2c_read_byte(_state.i2c_client, 0x01, &data);
    //printk(KERN_INFO "%s: data 0x%x\n", __func__, data);

#if 0
	NX_GPIO_SetOutputValue(PAD_GET_GROUP(CFG_IO_CAM_PWR_EN), PAD_GET_BITNO(CFG_IO_CAM_PWR_EN), 1);
	_i2c_write_byte(_state.i2c_client, 0x02, 0x44);

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
	printk(KERN_ERR "%s : switch value : %d, backgear on : %d\n", __func__, switch_get_state(&_state.switch_dev), _is_backgear_on());
#if 0
    __cancel_delayed_work(&_state.work);
    if (switch_get_state(&_state.switch_dev) && !_is_backgear_on()) {
       switch_set_state(&_state.switch_dev, 0);
    } else if (!switch_get_state(&_state.switch_dev) && _is_backgear_on()) {
        schedule_delayed_work(&_state.work, msecs_to_jiffies(REARCAM_BACKDETECT_TIME));
    }
#else
    __cancel_delayed_work(&_state.work);
    schedule_delayed_work(&_state.work, msecs_to_jiffies(REARCAM_BACKDETECT_TIME));
#endif

    return IRQ_HANDLED;
}

static irqreturn_t _irq_handler3(int irq, void *devdata)
{
    if (switch_get_state(&_state.switch_dev)/* && !_is_camera_on()*/) {
        switch_set_state(&_state.switch_dev, 0);
    } else if (!switch_get_state(&_state.switch_dev) && _is_backgear_on()) {
        schedule_delayed_work(&_state.work, msecs_to_jiffies(REARCAM_MPOUT_TIME));
    }

    return IRQ_HANDLED;
}

static void _work_handler(struct work_struct *work)
{
    //printk(KERN_ERR "+++ %s +++\n", __func__);
    if (_is_backgear_on() && _is_camera_on()) {
		if (rearcam_brightness_tbl[_state.brightness] != DEFAULT_BRIGHTNESS)
		{
			//printk(KERN_ERR "brightness = %d\n",_state.brightness);
        	_i2c_write_byte(_state.i2c_client, 0x10, rearcam_brightness_tbl[_state.brightness]);
		}
        switch_set_state(&_state.switch_dev, 1);
		printk(KERN_ERR "%s : backward camera on!!\n", __func__);
        return;
    } else if (switch_get_state(&_state.switch_dev)) {
        switch_set_state(&_state.switch_dev, 0);
		printk(KERN_ERR "%s : backward camera off!!\n", __func__);
    }
	else if(_is_backgear_on())
	{
		schedule_delayed_work(&_state.work, msecs_to_jiffies(REARCAM_BACKDETECT_TIME));
	}
    //printk(KERN_ERR "--- %s ---\n", __func__);
}

static int tw9900_s_stream(struct v4l2_subdev *sd, int enable)
{
	//printk("%s : enable : %d, state : %d\n", __func__, enable, _state.first);
    if (enable) {
        if (_state.first) 
		{
#if 0
      	 	int ret = request_irq(IRQ_GPIO_START + CFG_BACKWARD_GEAR, _irq_handler, IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, "tw9900", &_state);
        	if (ret<0) {
         		pr_err("%s: failed to request_irq(irqnum %d), ret : %d\n", __func__, IRQ_ALIVE_1, ret);
         		return -1;
          	}
#endif
#if 0
			ret = request_irq(IRQ_ALIVE_4, _irq_handler3, IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, "tw9900-mpout", &_state);
			if (ret) {
				pr_err("%s: failed to request_irq (irqnum %d)\n", __func__, IRQ_ALIVE_4);
				return -1;
			}
#endif

#if !defined(CONFIG_SLSIAP_BACKWARD_CAMERA)

			int  i=0;
			struct tw9900_state *me = &_state;
			struct reg_val *reg_val = _sensor_init_data;

			while( reg_val->reg != 0xff)
			{
				//printk("%s : reg : 0x%02X, val : 0x%02X\n", __func__, reg_val->reg, reg_val->val);

				_i2c_write_byte(me->i2c_client, reg_val->reg, reg_val->val);
				mdelay(10);
				i++;
				reg_val++;
			}
#endif
     		_state.first = false;

			if (_is_backgear_on()){
				if (_is_camera_on())	{
					switch_set_state(&_state.switch_dev, 1);
				} else {
					schedule_delayed_work(&_state.work, msecs_to_jiffies(REARCAM_BACKDETECT_TIME));
				}
			}
		}
    }

    return 0;
}

static int tw9900_s_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
        struct v4l2_subdev_format *fmt)
{
    vmsg("%s\n", __func__);
    return 0;
}

static int tw9900_s_power(struct v4l2_subdev *sd, int on)
{
    vmsg("%s: %d\n", __func__, on);
    return 0;
}

static const struct v4l2_subdev_core_ops tw9900_subdev_core_ops = {
    .s_power = tw9900_s_power,
};

static const struct v4l2_subdev_pad_ops tw9900_subdev_pad_ops = {
    .set_fmt = tw9900_s_fmt,
};

static const struct v4l2_subdev_video_ops tw9900_subdev_video_ops = {
    .s_stream = tw9900_s_stream,
};

static const struct v4l2_subdev_ops tw9900_ops = {
    .core  = &tw9900_subdev_core_ops,
    .video = &tw9900_subdev_video_ops,
    .pad   = &tw9900_subdev_pad_ops,
};

static int tw9900_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct v4l2_subdev *sd;
    struct tw9900_state *state = &_state;
    int ret;

    vmsg("%s entered\n", __func__);

    sd = &state->sd;
    strcpy(sd->name, "tw9900");

    v4l2_i2c_subdev_init(sd, client, &tw9900_ops);

    sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
    state->pad.flags = MEDIA_PAD_FL_SOURCE;
    sd->entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;
    ret = media_entity_init(&sd->entity, 1, &state->pad, 0);
    if (ret < 0) {
        dev_err(&client->dev, "%s: failed to media_entity_init()\n", __func__);
        return ret;
    }

    ret = tw9900_initialize_ctrls(state);
    if (ret < 0) {
        printk(KERN_ERR "%s: failed to initialize controls\n", __func__);
        return ret;
    }

    i2c_set_clientdata(client, sd);
    state->i2c_client = client;

#if defined(CONFIG_SLSIAP_BACKWARD_CAMERA)
    state->switch_dev.name = "rearcam";
    switch_dev_register(&state->switch_dev);
    switch_set_state(&state->switch_dev, 0);

    backgear_switch = &state->switch_dev;

    INIT_DELAYED_WORK(&_state.work, _work_handler);
#endif

    state->first = true;

    vmsg("%s exit\n", __func__);

    return 0;
}

static int tw9900_remove(struct i2c_client *client)
{
    struct tw9900_state *state = &_state;
    v4l2_device_unregister_subdev(&state->sd);
    return 0;
}

static const struct i2c_device_id tw9900_id[] = {
    { "tw9900", 0 },
    {}
};

MODULE_DEVICE_TABLE(i2c, tw9900_id);

static struct i2c_driver tw9900_i2c_driver = {
    .driver = {
        .name = "tw9900",
    },
    .probe = tw9900_probe,
    .remove = __devexit_p(tw9900_remove),
    .id_table = tw9900_id,
};

static int __init tw9900_init(void)
{
    return i2c_add_driver(&tw9900_i2c_driver);
}

static void __exit tw9900_exit(void)
{
    i2c_del_driver(&tw9900_i2c_driver);
}

module_init(tw9900_init);
module_exit(tw9900_exit);

MODULE_DESCRIPTION("TW9900 Camera Sensor Driver for only FINE");
MODULE_AUTHOR("<swpark@nexell.co.kr>");
MODULE_LICENSE("GPL");
