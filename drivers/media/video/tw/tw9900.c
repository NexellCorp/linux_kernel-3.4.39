#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/switch.h>

#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

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
};

static struct tw9900_state _state;

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

    i2c_set_clientdata(client, sd);

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
