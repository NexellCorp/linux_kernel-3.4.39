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

#include "ds90ub914q.h"

#define DS90UB914Q_USE_DBG	0	

#ifdef DS90UB914Q_USE_DBG
#define vmsg(a...) 	printk(a)
#else
#define vmsg(a...)
#endif

#define SENSOR_LOG  0

#define NEWONE_ALIAS_TEST 0
//#define SERDES_I2C_TEST

#define DBG_READ_TEST	0

#define DES_I2CBUS 7

#define DS90UB914Q 0xC0
#define PID	0x00

#define DSER_DEV_NAME	"DS90UB914Q"
#define SER_DEV_NAME	"DS90UB913Q"

static struct dev_state ds90ub914q;

static struct i2c_board_info serdes_camera_i2c_boardinfo[] __initdata = {
   	{
		I2C_BOARD_INFO("ZA03W10", 0xB8>>1), 
	},
};

static struct i2c_board_info ser_i2c_boardinfo[] __initdata = {
    {
		I2C_BOARD_INFO(SER_DEV_NAME, 0xB0>>1), 
	},
};

static inline struct dev_state *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct dev_state, sd);
}

static bool check_id(struct i2c_client *client)
{
	u8 pid = i2c_smbus_read_byte_data(client, PID);
	if(pid == DS90UB914Q)
		return true;

	printk(KERN_ERR "failed to check id: 0x%02X\n", pid);
	return false;
}

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

	for(i=0; i<I2C_RETRY_CNT; i++) {
		ret = i2c_transfer(client->adapter, msg, 2);
		if (likely(ret == 2))
			break;
	}

	if (unlikely(ret != 2)) {
		dev_err(&client->dev, "%s fail reg:0x%02x\n", __func__, addr);
		return -EIO;
	}
	
	*data = buf;
	return 0;
}

static int _i2c_write_byte(struct i2c_client *client, u8 addr, u8 val)
{
	s8	i = 0;
	s8 ret = 0;
	u8 buf[2];
	struct i2c_msg msg;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = buf;

	buf[0] = addr;
	buf[1] = val;

	for(i=0; i<I2C_RETRY_CNT; i++) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (likely(ret == 1))
			break;
	}

	if (ret != 1) {
		printk(KERN_ERR "%s: failed to wirte addr 0x%x, val 0x%x\n", __func__, addr, val);
		return -EIO;
	}

	return 0;
}

static int i2c_read(struct i2c_client *client, u8 addr, void *data)
{
	char buf[2];
	int	ret;

	buf[0] = (addr & 0xff);

	if((ret = i2c_master_send(client, buf, 1)) < 0)
		return 1;

	if((ret = i2c_master_recv(client, data, 1)) < 0)
		return 2;

	return 0;
}

static int i2c_write(struct i2c_client *client, u8 addr, u8 data)
{
	char buf[2];
	int ret;

	buf[0] = (addr & 0xff);
	buf[1] = data;

	if((ret = i2c_master_send(client, buf, 2)) < 0)
	{
		//printk("%s addr:0x%02x, data:0x%02x, ret:%3d err!\n", __func__, addr, data, ret);
		return ret;
	}	

	return 0;
} 

static int write_reg_data(struct i2c_client *client, struct reg_val *reg_val)
{
	while( reg_val->reg != 0xff)
	{
		//printk("%s - reg : 0x%02x, val : 0x%02x\n", __func__, reg_val->reg, reg_val->val);
		_i2c_write_byte(client, reg_val->reg, reg_val->val);
		mdelay(10);
		reg_val++;
	}

	return 0;
}

static int update_preset(struct i2c_client *client, unsigned short *p)
{
	int pos=0;
	int retry=0;
	int ret;

	unsigned short *p_data;

	p_data = p;

	for(;;)
	{
		if(*p_data == 0xffff)
			break;
		else
		{
#if 0
			printk("0x%04X,", *p_data);
			if((pos+1) % 10 == 0) printk("\n");
#endif
			ret = i2c_write(client, ((*p_data) >> 8) & 0xff, ((*p_data) & 0xff));
			if(ret)
			{
				if( ++retry == 3)
				{
					printk("[MDAS] %s failed @ 0x%x\n", __func__, pos);
					break;
				}
				else
					continue;
			}
			else
				retry = 0;
		
			p_data++;
			pos++;
		}
	}

	return 0;
}

static int serialize_init(void)
{
	struct i2c_client *ser_client	 = ds90ub914q.ser_client;
	struct i2c_client *des_client    = ds90ub914q.des_client;

#if SENSOR_LOG
	struct i2c_client *camera_client = ds90ub914q.camera_client;

	unsigned char data = 0; 
#endif

	struct reg_val des_init_data[] ={
		//{0x03, 0xed},
		{0x03, 0xfd},
		{0x06, 0xb0},
		{0x07, 0xb0},
		END_MARKER
	};

	struct reg_val ser_init_data[] = {
		{0x11, 0x12},
		{0x12, 0x12},
		
		/* GPO Setting*/

		END_MARKER
	};

	struct reg_val des_config_data[] = {
		{0x21, 0x97},
		{0x4d, 0xc0},
		{0x04, 0x03},
		END_MARKER
	};	

	struct reg_val des_camera_config_data[] = {
		{0x06, (0xb8 | 0x1)},
		{0x07, 0xb8},
		END_MARKER
	};	

	write_reg_data(des_client, des_init_data);
	write_reg_data(ser_client, ser_init_data);

#if SENSOR_LOG
	if (_i2c_read_byte(ser_client, 0x0d, &data) == 0 )
	{
		_i2c_write_byte(ser_client, 0x0d, 0x99);

		if (_i2c_read_byte(ser_client, 0x0d, &data) == 0 )
			printk("4. ds90ub913q reg = 0x0d, data = 0x%02X\n", data);
	}

	if (_i2c_read_byte(ser_client, 0x0d, &data) == 0 )
	{
		printk("5. ds90ub913q reg = 0x0d, data = 0x%02X\n", data);
	}

	printk("DeSerializer GPO1,0 Status !!!\n");
	if (_i2c_read_byte(des_client, 0x1d, &data) == 0 )
		printk("ds90ub914q reg = 0x1d, data = 0x%02X\n", data);

	_i2c_write_byte(des_client, 0x1d, 0x99);

	if (_i2c_read_byte(des_client, 0x1d, &data) == 0 )
		printk("ds90ub914q reg = 0x1d, data = 0x%02X\n", data);
#else
    _i2c_write_byte(ser_client, 0x0d, 0x99);
	_i2c_write_byte(des_client, 0x1d, 0x99);
#endif


#if SENSOR_LOG 
	printk("Serializer Status !!!\n");
	if (_i2c_read_byte(ser_client, 0x0d, &data) == 0 )
		printk("ds90ub913q reg = 0x0d, data = 0x%02X\n", data);

	//_i2c_write_byte(ser_client, 0x0d, 0xdd);

	if (_i2c_read_byte(ser_client, 0x0d, &data) == 0 )
		printk("ds90ub913q reg = 0x0d, data = 0x%02X\n", data);
#endif

#if SENSOR_LOG
	if (_i2c_read_byte(ser_client, 0x11, &data) == 0 )
		printk("ds90ub913q reg = 0x11, data = 0x%02X\n", data);
	else
		return 1;

	if (_i2c_read_byte(ser_client, 0x12, &data) == 0 )
		printk("ds90ub913q reg = 0x12, data = 0x%02X\n", data);
	else
		return 2;

	if (_i2c_read_byte(ser_client, 0x0e, &data) == 0 )
		printk("ds90ub913q reg = 0x0e, data = 0x%02X\n", data);
#endif

	_i2c_write_byte(ser_client, 0x0e, 0x99);
	_i2c_write_byte(ser_client, 0x0e, 0x99);

#if SENSOR_LOG 
	if (_i2c_read_byte(ser_client, 0x03, &data) == 0 )
	{
		printk("ds90ub913q reg = 0x03, data = 0x%02X, CRC Error Reset = 0x%02X\n", data, (data | 0x20));
	
		//_i2c_write_byte(ser_client, 0x03, (data | 0x20));
		//_i2c_write_byte(ser_client, 0x03, data);
	}

	if(_i2c_read_byte(ser_client, 0x0c, &data) == 0)
	{
		printk("ds90ub913q reg = 0x0c, data = 0x%x\n", data);	
		printk("RevId:%d, Rx Locked:%d, BIST CRC ERR:%d, PCLK Detect:%d, DES Err:%d, Link Detect:%d\n",
			(data>>5), (data & 0x10) ? 1 : 0, (data & 0x08) ? 1 : 0, (data & 0x04) ? 1: 0, (data & 0x02) ? 1 : 0, (data & 0x01) ? 1 : 0);
		printk("* CAMERA B: ON *\n");
	}
	else
	{
		printk("ds90ub913q_read(0x12) fail!!\n");
		printk("* CAMERA B: OFF *\n");
		return -1;
	}
#endif
	
	write_reg_data(des_client, des_config_data);
	write_reg_data(des_client, des_camera_config_data);

#if SENSOR_LOG
	if(_i2c_read_byte(des_client, 0x1c, &data) == 0)
	{
		printk("ds90ub914q reg = 0x1c, data = 0x%x\n", data);	
		printk("RevId:%d, Locked:%d, Signal detect:%d, Parity Error:%d\n",
			(data>>4), (data & 0x01) ? 1 : 0, (data & 0x02) ? 1 : 0, (data & 0x04) ? 1: 0);
	}
	else
	{
		printk("ds90ub914q_read(0x1c) fail!!\n");
		return -1;
	}

	printk("camera client = 0x%p\n", camera_client);	

	int i=0;
	for(i=0; i<=0x03; i++)
	{
		if(_i2c_read_byte(camera_client, i, &data) == 0)
			printk("camera reg = 0x%02x, data = 0x%02x\n", i, data);	
	}
#endif

	return 0;
}

static int register_i2c(int i2c_idx, struct i2c_board_info *board_info, int board_idx, struct i2c_client **client)
{
	struct i2c_adapter *adap = i2c_get_adapter(i2c_idx);

	if(adap == NULL)
	{
		printk("i2c_get_adapter fail\n");
		return -1;
	}	

	*client = i2c_new_device(adap, &board_info[board_idx]);
	if(client == NULL)
	{
		printk("i2c_new_device fail!\n");
		return -1;
	}

	i2c_put_adapter(adap);

	return 0;
}

static int setZa03(eZa03w10_page page, i2c_data *p, char *str)
{
	u8 buf[2];
	int ret=0;
	struct i2c_client *camera_client = ds90ub914q.camera_client;

	ret = i2c_write(camera_client, 0x00, page);
	if (ret) {
		ret = 1;
	}

	ret = i2c_read(camera_client, p->addr, buf);
	if (ret) {
		ret = 2;
	}

#if 0
	printk("%s()B Page%c %s: data: 0x%0x, mask:0x%0x, data:0x%0x \n",
			 __func__, ('A' + (unsigned char)page), str, buf[0], p->mask, p->data);
#endif

	msleep(1);

	ret = i2c_write(camera_client, p->addr, (buf[0] & p->mask) | p->data);
	if (ret) {
		ret = 3;
	}

	ret = i2c_read(camera_client, p->addr, (buf+1));
	if (ret) {
		ret = 4;
	}	
#if 0
	printk("%s()A Page%c %s: 0x%0x -> 0x%0x \n", __func__, ('A' + (unsigned char)page), str, buf[0], buf[1]);
#endif

	return ret;
}

static int zaSet(eZa03w10_page page, unsigned char addr, unsigned char data, unsigned char mask, char *s)
{
	int i;
	int ret;
	i2c_data d;

	d.addr = addr;
	d.data = data;
	d.mask = mask;

	for (i=0 ; i<3 ; i++)
	{
		ret = setZa03(page, &d, s);
		if (!ret)
			return 0;
	}

#if 0

	printk("[MDAS %s PAGE_%c addr: %d, data: %d, mask: 0x%x, %s Failed!\n",
						 __func__, 'A'+page, addr, data, mask, s);
#endif

	return -1;
}

static void ds90ub914q_reset(struct i2c_client *client)
{
	unsigned char data=0;

	//_i2c_write_byte(client, 0x01, 0x02); //Digital reset except regitsers. self-clearing.
	_i2c_write_byte(client, 0x01, 0x01); 
	mdelay(50);
	if( _i2c_read_byte(client, 0x01, &data) != 0)
		printk("%s : Deserializer i2c error!\n", __func__);

	if((data & 0x02) > 0 || (data & 0x01) > 0)
		printk("%s : Deserializer status is reset!!\n", __func__);
}

static int ds90ub914q_s_power(struct v4l2_subdev *sd, int on)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct dev_state *state = to_state(sd);
	int i2c_id = i2c_adapter_id(client->adapter);
	static bool is_first = true;

	switch (i2c_id) {
		case 1:
			break;
		case 2:
			break;
		default:
			return 0;
	}

	if (on)
	{
		if( is_first )
		{
			is_first = false;
		}

	}
	else
	{
		state->inited = false;	
		is_first = true;
	}

	return 0;
}

static int ds90ub914q_set_fmt(struct v4l2_subdev *sd, 
									struct v4l2_subdev_fh *fh, struct v4l2_subdev_format *fmt)
{
	int err = 0;
	struct v4l2_mbus_framefmt *_fmt = &fmt->format;
	struct dev_state *state = to_state(sd);
	
	state->width = _fmt->width;
	state->height = _fmt->height;

	//printk("%s : mode %d, %dx%d\n", __func__, state->mode, state->width, state->height);

	return err;	
}

static int ds90ub914q_set_crop(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh, struct v4l2_subdev_crop *crop)
{
	return 0;
}

static int ds90ub914q_get_crop(struct v4l2_subdev *sd,
									struct v4l2_subdev_fh *fh, struct v4l2_subdev_crop *crop)
{
	return 0;	
}

static int ds90ub914q_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct dev_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	struct i2c_client *camera_client = ds90ub914q.camera_client;	

#if DBG_READ_TEST
	unsigned char data = 0;
	char buf[4];
	char *regName[] = {"Page", "ID_H", "ID_L", "REVISION"};
#endif

	if( enable ) {
		if( !state->inited ) 
		{
			if(!check_id(client))
				return -EINVAL;

#if 0
			int i=0;

			for(i=0 ; i<5 ; i++)
			{
				ret = i2c_read(camera_client, 0x1, &d);

				if (!ret && d == 0x3)
				{
					printk("Error initialization error\n");
					break;
				}
				else
				{
					serialize_init();
					printk("[MDAS] ZA03W10 CAMERA INIT. %d TIMES! d: %d, ret: %d\n", (i+1), d, ret);
				}	

			}
#else
			ds90ub914q_reset(client);
			serialize_init();
#endif

#if DBG_READ_TEST
			memset(buf, 0x0, sizeof(buf));
			for(i=0 ; i<=0x03 ; i++)
			{
				i2c_read(camera_client, i, buf);
				printk("%s() reading addr %s:0x%02x, data:0x%02x\n", __func__, regName[i], i, buf[0]);
			}	
#endif

			update_preset(camera_client, init_values);
	
			zaSet(ZA03W10_PAGE_B, 0x06, 0x48, (u8)~0xc8, "sync Polarity");

			zaSet(ZA03W10_PAGE_B, 0x2E, 0x80, (u8)~0xff, "BLANK_A");
			zaSet(ZA03W10_PAGE_B, 0x2F, 0x10, (u8)~0xff, "BLANK_B");

			zaSet(ZA03W10_PAGE_B, 0x0B, 0xC0, 0, "BT656 Em-sync");
			printk("%s - Embedded Sync Mode.\n", __func__);	
			
			zaSet(ZA03W10_PAGE_B, 0x12, 0x0, 0x8, "Output Format");

			state->inited = true;	
		}	
	} else {

	}

#ifdef SERDES_I2C_TEST
	int i=0;
	for(i=0; i<=0x26; i++)
	{
		_i2c_read_byte(client, i, &data);
		printk("DES. @ 0x%02x : %02xh \n", i, data);
	}

	_i2c_read_byte(client, 0x4d, &data);
	printk("DES. @ 0x%02x : %02xh \n", i, data);
#endif

	return 0;
}

static const struct v4l2_subdev_core_ops ds90ub914q_core_ops = {
	.s_power = ds90ub914q_s_power,
	.s_ctrl = v4l2_subdev_s_ctrl,
};

static const struct v4l2_subdev_pad_ops ds90ub914q_pad_ops = {
	.set_fmt =	ds90ub914q_set_fmt,
	.set_crop = ds90ub914q_set_crop,
	.get_crop = ds90ub914q_get_crop,
};

static const struct v4l2_subdev_video_ops ds90ub914q_video_ops = {
	.s_stream = ds90ub914q_s_stream, 
};

static const struct v4l2_subdev_ops ds90ub914q_ops = {
	.core = &ds90ub914q_core_ops,
	.video = &ds90ub914q_video_ops,
	.pad = &ds90ub914q_pad_ops,
};

static int _link_setup(struct media_entity *entity, 
						const struct media_pad *local,
						const struct media_pad *remote, u32 flags)
{
	return 0;
}

static const struct media_entity_operations ds90ub914q_media_ops = {
	.link_setup = _link_setup, 
}; 

static int ds90ub914q_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct dev_state *state = &ds90ub914q;
	struct v4l2_subdev *sd;
	int ret;

	sd = &state->sd;
	strcpy(sd->name, id->name); 

	v4l2_i2c_subdev_init(sd, client, &ds90ub914q_ops);

	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	state->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;
	sd->entity.ops = &ds90ub914q_media_ops;
	ret = media_entity_init(&sd->entity, 1, &state->pad, 0);
	if (ret < 0) {
		dev_err(&client->dev, "%s: failed to media_entity_init()\n", __func__);
		return -ENOENT;
	}

	//initialize........!
#if 0
	ret = ds90ub914q_initialize_ctrls(state);
	if (ret < 0) {
		printk(KERN_ERR "%s: failed to initialize controls\n", __func__);
		kfree(state);
		return ret;
	}

	state->init_wq = create_singlethread_workqueue("ds90ub914q-init");
	if (!state->init_wq) {
		pr_err("%s: error create work queue for init\n", __func__);
		return -ENOMEM;
	}
	INIT_WORK(&state->init_work, ds90ub914q_init_work);
#endif

	i2c_set_clientdata(client, sd);
	state->des_client = client;
	state->inited = false;

	/* register serializer i2c */
	if(register_i2c(DES_I2CBUS, ser_i2c_boardinfo, 0, &state->ser_client) < 0)
	{
		printk(KERN_ERR "Serializer registration is fail!\n");
		return -1;
	}

	/* register camera i2c */
	if(register_i2c(DES_I2CBUS, serdes_camera_i2c_boardinfo, 0, &state->camera_client) < 0)
	{
		printk(KERN_ERR "SerDes camera registration is fail!\n");
		return -1;
	}

	dev_info(&client->dev, "ds90ub914q has been probed\n");

	return 0;
}

static int ds90ub914q_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct dev_state *state = &ds90ub914q;

	v4l2_device_unregister_subdev(sd);
	v4l2_ctrl_handler_free(sd->ctrl_handler);
	media_entity_cleanup(&sd->entity);

	i2c_set_clientdata(state->ser_client, NULL);
	i2c_set_clientdata(state->camera_client, NULL);

	return 0;
}

static const struct i2c_device_id ds90ub914q_id[] = {
	{ DSER_DEV_NAME, 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, ds90ub914q_id);

static struct i2c_driver ds90ub914q_i2c_driver = {
	.driver = {
		.name = DSER_DEV_NAME,
		.owner = THIS_MODULE,
	},
	.probe = ds90ub914q_probe, 
	.remove = __devexit_p(ds90ub914q_remove),
	.id_table = ds90ub914q_id,
};

static int __init ds90ub914q_init(void)
{
	int ret;

	ret = i2c_add_driver(&ds90ub914q_i2c_driver);
	if (ret != 0)
	{
		printk(KERN_ERR "Failed to register ds90ub914q I2C Driver: %d\n", ret);
		return ret;
	}

	return 0;
}

static void __exit ds90ub914q_exit(void)
{
	i2c_del_driver(&ds90ub914q_i2c_driver);
}

module_init(ds90ub914q_init);
module_exit(ds90ub914q_exit);

MODULE_DESCRIPTION("DS90UB914Q SerDes Sensor Driver");
MODULE_AUTHOR("<jkchoi@nexell.co.kr>");
MODULE_LICENSE("GPL");
