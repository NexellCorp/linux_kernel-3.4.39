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

#include "max9272.h"
#include "max9272_preset.h"

#define DSER_DEV_NAME	"MAX9272"
#define SER_DEV_NAME	"MAX9271"

#define DES_I2CBUS	0

static struct dev_state max9272;

static struct i2c_board_info sensor_i2c_boardinfo[] __initdata = {
	{
		I2C_BOARD_INFO("PC6060KCT", 0X66>>1),
	},
};

static struct i2c_board_info ser_i2c_boardinfo[] __initdata = {
	{
		I2C_BOARD_INFO(SER_DEV_NAME, 0x80>>1),
	},
};

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
	if (_i2c_read_byte(client, addr, &data) == 0)
		printk("read data - addr: 0x%x, val 0x%x\n", addr, data);
#endif
	return 0;
}

static bool check_id(struct i2c_client *client)
{
	u8 pid = 0;

	_i2c_read_byte(client, 0x1E, &pid);
	if( pid == 0x0A )
		return true;

	printk(KERN_ERR "fail to check id: 0x%02X\n", pid);
	return false;
}

static int serdes_check_lock(struct i2c_client *client)
{
	int locked = 0;
	int ret = 0;
	u8 data = 0;

	ret = _i2c_read_byte(client, MAX9272_I2C_CHECK_LOCK, &data);
	if (ret != 0) {
		printk(KERN_ERR "%s serdes check lock failed! "
		                "(0x%x, ret: %d)\n", __func__,
				MAX9272_I2C_CHECK_LOCK, ret);
		return -EIO;
	} else
		locked = (data >> 7);

	return locked;

}

static int register_i2c(int i2c_idx, struct i2c_board_info *board_info,
			int board_idx, struct i2c_client **client) {

	struct i2c_adapter *adap = i2c_get_adapter(i2c_idx);
	if (adap == NULL) {
		printk(KERN_ERR "i2c_get_adapter fail\n");
		return -1;
	}

	*client = i2c_new_device(adap, &board_info[board_idx]);
	if (client == NULL) {
		printk(KERN_ERR "i2c_new_device fail!\n");
		return -1;
	}

	i2c_put_adapter(adap);

	return 0;
}

static int sensor_init(void)
{
	struct reg_val *reg_val = &sensor_init_reg[0];
	int index = 0;

	struct i2c_client *client = max9272.sensor_client;

	for (index=0; index<SENSOR_REGS; index++) {
		if(reg_val->reg == DELAY) {
			mdelay(reg_val->val);
			reg_val++;
			continue;
		}

		printk("%s - reg : 0x%02X, val : 0x%02X\n",
		       __func__, reg_val->reg, reg_val->val);

		_i2c_write_byte(client, reg_val->reg,
				reg_val->val);
		reg_val++;
	}

	return 0;
}

static int ser_init(void)
{
	struct reg_val *reg_val = &ser_init_reg[0];
	int index = 0;

	struct i2c_client *client = max9272.ser_client;

	for (index=0; index<SER_REGS; index++) {
		if(reg_val->reg == DELAY) {
			mdelay(reg_val->val);
			reg_val++;
			continue;
		}

		printk("%s - reg : 0x%02X, val : 0x%02X\n",
		       __func__, reg_val->reg, reg_val->val);

		_i2c_write_byte(client, reg_val->reg,
				reg_val->val);
		reg_val++;
	}

	return 0;
}

static int des_init(void)
{
	struct reg_val *reg_val = &des_init_reg[0];
	int index = 0;

	struct i2c_client *client = max9272.des_client;

	for (index=0; index<DES_REGS; index++) {
		if(reg_val->reg == DELAY) {
			mdelay(reg_val->val);
			reg_val++;
			continue;
		}

		printk("%s - reg : 0x%02X, val : 0x%02X\n",
		       __func__, reg_val->reg, reg_val->val);

		_i2c_write_byte(client, reg_val->reg,
				reg_val->val);
		reg_val++;
	}

	return 0;
}

static int max9272_initialize_ctrls(struct dev_state *me)
{
	return 0;
}

static int max9272_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct dev_state *state = to_state(sd);

	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct i2c_client *sensor_client = max9272.sensor_client;

	int regval = 0;

	printk("%s - enable : %d, state : %d\n",
	       __func__, enable, (state->first == true) ? 1 : 0);

	if (enable) {
		if (!state->first) {
			if(!check_id(client))
				return -EINVAL;

			_i2c_write_byte(sensor_client, 0x03, 0x00);
			mdelay(5);
			_i2c_write_byte(sensor_client, 0x29, 0x99);
			mdelay(5);

			regval	= serdes_check_lock(client);
			if (!regval) {
				printk(KERN_ERR "%s - SerDes lock status : %d\n"
				       , __func__, regval);
				return -EIO;
			}

			des_init();
			ser_init();
			sensor_init();

			//	state->first = true;
		}
	} else {
	}

	return 0;
}

static int max9272_g_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
			struct v4l2_subdev_format *fmt)
{
	return 0;
}

static int max9272_s_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
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

static int max9272_s_power(struct v4l2_subdev *sd, int on)
{
	vmsg("%s: %d\n", __func__, on);
	return 0;
}

static const struct v4l2_subdev_core_ops max9272_core_ops = {
	.s_power = max9272_s_power,
};

static const struct v4l2_subdev_video_ops max9272_video_ops = {
	.s_stream = max9272_s_stream,
};

static const struct v4l2_subdev_pad_ops max9272_pad_ops = {
	.set_fmt = max9272_s_fmt,
	.get_fmt = max9272_g_fmt,
};

static const struct v4l2_subdev_ops max9272_ops = {
	.core = &max9272_core_ops,
	.video = &max9272_video_ops,
	.pad = &max9272_pad_ops,
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

static const struct media_entity_operations max9272_media_ops = {
	.link_setup = _link_setup,
};

static int max9272_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct dev_state *state = &max9272;
	struct v4l2_subdev *sd;
	int ret;

	sd = &state->sd;
	strcpy(sd->name, id->name);

	v4l2_i2c_subdev_init(sd, client, &max9272_ops);

	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	state->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;
	sd->entity.ops = &max9272_media_ops;
	ret = media_entity_init(&sd->entity, 1, &state->pad, 0);
	if (ret < 0) {
		dev_err(&client->dev, "%s: failed to media_entity_init()\n",
			__func__);
		return -ENOENT;
	}

	i2c_set_clientdata(client, sd);
	state->des_client = client;
	state->first = false;

	/* register serializer i2c */
	if (register_i2c(DES_I2CBUS, ser_i2c_boardinfo, 0 , &state->ser_client)
		< 0) {
		printk(KERN_ERR "Serializer registratiron is fail!\n");
		return -1;
	}

	/* register sensor i2c */
	if (register_i2c(DES_I2CBUS, sensor_i2c_boardinfo, 0,
		&state->sensor_client)<0) {
		printk(KERN_ERR "Sensor registeration is fail!\n");
		return -1;
	}

	dev_info(&client->dev, "max9272 has been probed\n");

	return 0;
}

static int max9272_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct dev_state *state = &max9272;

	v4l2_device_unregister_subdev(sd);
	v4l2_ctrl_handler_free(sd->ctrl_handler);
	media_entity_cleanup(&sd->entity);

	i2c_set_clientdata(state->ser_client, NULL);
	i2c_set_clientdata(state->sensor_client, NULL);

	return 0;
}

static const struct i2c_device_id max9272_id[] = {
	{ DSER_DEV_NAME, 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, max9272_id);

static struct i2c_driver max9272_i2c_driver = {
	.driver = {
		.name = DSER_DEV_NAME,
		.owner = THIS_MODULE,
	},
	.probe = max9272_probe,
	.remove = __devexit_p(max9272_remove),
	.id_table = max9272_id,
};

static int __init max9272_init(void)
{
	int ret;
	ret = i2c_add_driver(&max9272_i2c_driver);
	if (ret != 0) {
		printk(KERN_ERR "Failed to registe rmax9272 I2C Driver: %d\n",
			ret);
		return ret;
	}

	return 0;
}

static void __exit max9272_exit(void)
{
	i2c_del_driver(&max9272_i2c_driver);
}

module_init(max9272_init);
module_exit(max9272_exit);

MODULE_DESCRIPTION("MAX9272 SerDes Sensor Driver");
MODULE_AUTHOR("<jkchoi@nexell.co.kr>");
MODULE_LICENSE("GPL");
