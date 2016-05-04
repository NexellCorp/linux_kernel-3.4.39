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

#include "tw9900.h"
#include "tw9900_preset.h"

#define TW9900_DEV_NAME "tw9900"

static struct dev_state tw9900;

/**
 *  util functions
 */
static inline struct dev_state *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct dev_state, sd);
}

static int _i2c_read_byte(struct i2c_client *client, u8 addr, u8 *data)
{
	s8 i = 0;
	s8 ret = 0;
	u8 buf =0;

	struct i2c_msg msg[2];

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len  = 1;
	msg[0].buf  = &addr;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = &buf;

	for (i=0; i<I2C_RETRY_CNT; i++) {
		ret = i2c_transfer(client->adapter, msg, 2);
		if (likely(ret == 2))
			break;
	}

	if (unlikely(ret != 2)) {
		dev_err(&client->dev, "%s failed reg:0x%02x\n", __func__, addr);
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
	u8 data = 0;

	struct i2c_msg msg;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = buf;

	buf[0] = addr;
	buf[1] = val;

	for (i=0; i<I2C_RETRY_CNT; i++) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (likely(ret == 1))
			break;
	}

	if (ret != 1) {
		printk(KERN_ERR "%s: failed to write addr 0x%x, val 0x%x\n",
		       __func__, addr, val);
		return -EIO;
	}

#if I2C_READ_CHECK
	_i2c_read_byte(client, addr, &data);
#endif
	return 0;
}

static int tw9900_initialize_ctrls(struct dev_state *me)
{
	return 0;
}

static int tw9900_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct dev_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int index = 0;

	printk("%s - enable : %d, state : %d\n",
			__func__, enable, (state->first == true) ? 1 : 0);

	if (enable) {
		if (!state->first) {

#if !defined(CONFIG_SLSIAP_BACKWARD_CAMERA)
			struct reg_val *reg_val = &tw9900_init_reg[0];

			for (index=0; index<TW9900_REGS; index++) {
#if 0
				printk("%s - reg : 0x%02X, val : 0x%02X\n",
					__func__, reg_val->reg, reg_val->val);
#endif
				_i2c_write_byte(client, reg_val->reg,
						reg_val->val);
				reg_val++;
			}
#endif

#if 0
			mdelay(1000);
			reg_val = &tw9900_init_reg[0];
			for (index=0; index<TW9900_REGS; index++) {
				//printk("%s - reg : 0x%02X, val : 0x%02X\n",
				//		__func__, reg_val->reg, reg_val->val);
				data = 0;
				ret = 0;

				ret = _i2c_read_byte(client, reg_val->reg,
						&data);

				mdelay(10);
				printk("%s : READ - reg : 0x%02X, val : 0x%02X, ret : %d\n",
					__func__, reg_val->reg, data, ret);
				reg_val++;
			}
#endif
		//	state->first = true;
		}
	} else {
	}

	return 0;
}

static int tw9900_g_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
			struct v4l2_subdev_format *fmt)
{
	return 0;
}

static int tw9900_s_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
			struct v4l2_subdev_format *fmt)
{
	int err = 0;
	struct v4l2_mbus_framefmt *_fmt = &fmt->format;
	struct dev_state *state = to_state(sd);

	vmsg("%s\n", __func__);

	state->width = _fmt->width;
	state->height = _fmt->height;

	printk("%s : mode %d, %dx%d\n", __func__,
	       state->mode, state->width, state->height);

	return err;
}

static int tw9900_s_power(struct v4l2_subdev *sd, int on)
{
	vmsg("%s: %d\n", __func__, on);
	return 0;
}

static const struct v4l2_subdev_core_ops tw9900_core_ops = {
	.s_power = tw9900_s_power,
};

static const struct v4l2_subdev_video_ops tw9900_video_ops = {
	.s_stream = tw9900_s_stream,
};

static const struct v4l2_subdev_pad_ops tw9900_pad_ops = {
	.set_fmt = tw9900_s_fmt,
	.get_fmt = tw9900_g_fmt,
};

static const struct v4l2_subdev_ops tw9900_ops = {
	.core = &tw9900_core_ops,
	.video = &tw9900_video_ops,
	.pad = &tw9900_pad_ops,
};

/**
 * media_entry_operations
 */
static int _link_setup(struct media_entity *entity,
		       const struct media_pad *local,
		       const struct media_pad *remote, u32 flags)
{
	return 0;
}

static const struct media_entity_operations tw9900_media_ops = {
	.link_setup = _link_setup,
};

static int tw9900_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct dev_state *state = &tw9900;
	struct v4l2_subdev *sd;
	int ret;

	sd = &state->sd;
	strcpy(sd->name, id->name);

	v4l2_i2c_subdev_init(sd, client, &tw9900_ops);

	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	state->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;
	sd->entity.ops = &tw9900_media_ops;
	ret = media_entity_init(&sd->entity, 1, &state->pad, 0);
	if (ret < 0) {
		dev_err(&client->dev, "%s: failed to media_entity_init()\n",
			__func__);
		return -ENOENT;
	}

	i2c_set_clientdata(client, sd);
	state->i2c_client = client;
	state->first = false;

	dev_info(&client->dev, "tw9900 has been probed\n");

	return 0;
}

static int tw9900_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	v4l2_device_unregister_subdev(sd);
	v4l2_ctrl_handler_free(sd->ctrl_handler);
	media_entity_cleanup(&sd->entity);
	return 0;
}

static const struct i2c_device_id tw9900_id[] = {
	{ TW9900_DEV_NAME, 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, tw9900_id);

static struct i2c_driver tw9900_i2c_driver = {
	.driver = {
		.name = TW9900_DEV_NAME,
		.owner = THIS_MODULE,
	},
	.probe = tw9900_probe,
	.remove = __devexit_p(tw9900_remove),
	.id_table = tw9900_id,
};

static int __init tw9900_init(void)
{
	int ret;
	ret = i2c_add_driver(&tw9900_i2c_driver);
	if (ret != 0) {
		printk(KERN_ERR "Failed to registe rtw9900 I2C Driver: %d\n",
			ret);
		return ret;
	}

	return 0;
}

static void __exit tw9900_exit(void)
{
	i2c_del_driver(&tw9900_i2c_driver);
}

module_init(tw9900_init);
module_exit(tw9900_exit);

MODULE_DESCRIPTION("TW9900 DECODER Sensor Driver");
MODULE_AUTHOR("<jkchoi@nexell.co.kr>");
MODULE_LICENSE("GPL");
