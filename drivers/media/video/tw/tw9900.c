#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/switch.h>

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
};

static struct tw9900_state _state;

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
    /*printk("%s: val %d\n", __func__, ctrl->val);*/
    if (ctrl->val == 0) {
        // MUX 0
        _i2c_write_byte(me->i2c_client, 0x02, 0x40);
    } else {
        // MUX 1
        _i2c_write_byte(me->i2c_client, 0x02, 0x44);
    }

    return 0;
}

static int tw9900_get_status(struct v4l2_ctrl *ctrl)
{
    struct tw9900_state *me = ctrl_to_me(ctrl);
    u8 data = 0;
    u8 mux;
    u8 val = 0;

    _i2c_read_byte(me->i2c_client, 0x02, &data);
    mux = (data & 0x0c) >> 2;
    if (mux == 0) {
        _i2c_read_byte(me->i2c_client, 0x01, &data);
        if (!(data & 0x80))
            val |= 1 << 0;

        _i2c_read_byte(me->i2c_client, 0x16, &data);
        if (data & 0x40)
            val |= 1 << 1;
    } else {
        _i2c_read_byte(me->i2c_client, 0x01, &data);
        if (!(data & 0x80))
            val |= 1 << 1;
    }

    ctrl->val = val;

    return 0;
}

static int tw9900_s_ctrl(struct v4l2_ctrl *ctrl)
{
    switch (ctrl->id) {
    case V4L2_CID_MUX:
        return tw9900_set_mux(ctrl);
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

#define NUM_CTRLS 2
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

    me->sd.ctrl_handler = &me->handler;
    if (me->handler.error) {
        printk(KERN_ERR "%s: ctrl handler error(%d)\n", __func__, me->handler.error);
        v4l2_ctrl_handler_free(&me->handler);
        return -EINVAL;
    }

    return 0;
}

static irqreturn_t _irq_handler(int irq, void *devdata)
{
    int val = nxp_soc_gpio_get_in_value(PAD_GPIO_ALV + 4);
    vmsg("%s val %d\n", __func__, val);
    if (!val)
        switch_set_state(&_state.switch_dev, 1);
    else
        switch_set_state(&_state.switch_dev, 0);
    return IRQ_HANDLED;
}

static int tw9900_s_stream(struct v4l2_subdev *sd, int enable)
{
    if (enable) {
        if (_state.first) {
            int ret = request_irq(IRQ_ALIVE_4, _irq_handler, IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, "tw9900", &_state);
            if (ret) {
                pr_err("%s: failed to request_irq(irqnum %d)\n", __func__, IRQ_ALIVE_4);
                return -1;
            }
            _state.first = false;
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

    state->switch_dev.name = "tw9900";
    switch_dev_register(&state->switch_dev);
    switch_set_state(&state->switch_dev, 0);

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
