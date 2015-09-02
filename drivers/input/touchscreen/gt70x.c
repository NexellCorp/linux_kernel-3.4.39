#include <linux/irq.h>
#include "gt70x.h"

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/err.h>

#include <linux/time.h>
#include <linux/platform_device.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/kthread.h>



#define SCREEN_MAX_HEIGHT	1024
#define SCREEN_MAX_WIDTH	600

#define TOUCH_MAX_HEIGHT	4096
#define TOUCH_MAX_WIDTH	4096


#define GTP_INT_TRIGGER  1
#define	DELAY_WORK_TIME 		10	// ms


static struct workqueue_struct *msd_tp_wq;

s32 gtp_i2c_read(struct i2c_client *client, u8 *buf, s32 len)
{
		struct i2c_msg msgs[2];
		s32 ret = -1;
		s32 retries = 0;

		//发送写地址
		msgs[0].flags = !I2C_M_RD;
		msgs[0].addr = client->addr;
		msgs[0].len = GTP_ADDR_LENGTH;
		msgs[0].buf = &buf[0];

		//接收数据
		msgs[1].flags = I2C_M_RD;
		msgs[1].addr = client->addr;
		msgs[1].len = len - GTP_ADDR_LENGTH;
		msgs[1].buf = &buf[GTP_ADDR_LENGTH];

		while(retries < 5)
		{
				ret = i2c_transfer(client->adapter, msgs, 2);
				if(ret == 2) break;
					retries++;
		}

		if(retries >= 5)
		{
				GTP_ERROR("error:	I2C retry timeout.\n");
		}

		return ret;
}


s32 gtp_i2c_write(struct i2c_client *client, u8 *buf, s32 len)
{
		struct i2c_msg msg;
		s32 ret = -1;
		s32 retries = 0;

		msg.flags = !I2C_M_RD;
		msg.addr = client->addr;
		msg.len = len;
		msg.buf = buf;

		while(retries < 5)
		{
				ret = i2c_transfer(client->adapter, &msg, 1);
				if(ret == 1)	break;

				retries++;
		}

		if(retries >= 5)
		{
				GTP_ERROR("error:I2C write retry timeout.\n");
		}
		return ret;
}



void gtp_irq_enable(struct goodix_ts_data *ts)
{
		unsigned long irqflags = 0;
		spin_lock_irqsave(&ts->irq_lock, irqflags);
		if(ts->irq_is_disable)
		{
				enable_irq(ts->client->irq);
				ts->irq_is_disable = 0;
		}
		spin_unlock_irqrestore(&ts->irq_lock, irqflags);
}


void gtp_irq_disable(struct goodix_ts_data *ts)
{
		unsigned long irqflags = 0;
		spin_lock_irqsave(&ts->irq_lock, irqflags);
		if(!ts->irq_is_disable)
		{
				disable_irq_nosync(ts->client->irq);
				ts->irq_is_disable = 1;
		}
		spin_unlock_irqrestore(&ts->irq_lock, irqflags);
}







static void goodix_ts_work_func(struct work_struct *work)
{

		static BOOL TouchDown = FALSE;
//		static int BackCarMode = FORWARD;
		static u32 ret = 0;
		static u8 finger, touch_num, i;

		static s32 input_x = 0;
	    static s32 input_y = 0;
	    static s32 input_w = 0;
	    static s32 id = 0;

		static u8* coor_data = NULL;

		static u8 point_data[2 + 1 + 8 * GTP_MAX_TOUCH +1] = {GTP_READ_COOR_ADDR >> 8, GTP_READ_COOR_ADDR & 0xFF};
		static u8  end_cmd[3] = {GTP_READ_COOR_ADDR >> 8, GTP_READ_COOR_ADDR & 0xFF, 0};

		 static struct goodix_ts_data *ts = NULL;

		 ts = container_of(work, struct goodix_ts_data, work.work);

//		 printk("goodix_ts_work_func \n");

//		if(touch_check_qstart_rtk()==BACK)
//		{
//			BackCarMode = BACK;
//			if(!TouchDown) goto DEAL_END;
//		}
//		else
//			BackCarMode = FORWARD;

		ret = gtp_i2c_read(ts->client, point_data, sizeof(point_data)/sizeof(point_data[0]));
		if(ret < 0)
		{
				GTP_ERROR("I2C transfer error. errno: %d\n", ret);
				goto DEAL_END;
		}

		finger = point_data[GTP_ADDR_LENGTH];
		if((finger & 0x80) == 0) //表示坐标未准备好
		{
//			printk("\n TCP NO ready \n");
			goto DEAL_END;
		}

		touch_num = finger & 0x0f;
		if(touch_num > GTP_MAX_TOUCH)
		{
			GTP_ERROR("touch_num > GTP_MAX_TOUCH \n");
			goto DEAL_END;
		}

		if(touch_num)
		{
//			if(BackCarMode == BACK && TouchDown) goto DEAL_END;
			for(i = 0; i < touch_num; i++)
			{
//						printk("\n get touch piont \n");
						coor_data = &point_data[i * 8 + 3];
						id = coor_data[0] & 0x0F;
						input_x = coor_data[1] | coor_data[2] << 8;
						input_y = coor_data[3] | coor_data[4] << 8;
						input_w = coor_data[5] | coor_data[6] << 8;

						if(input_x < SCREEN_MAX_HEIGHT)
							input_x = SCREEN_MAX_HEIGHT - input_x;

						//printk("\n--input_x[%d] = %d\n", i,input_x);
						//printk("\n--input_y[%d] = %d\n", i, input_y);

						input_report_key(ts->input_dev, BTN_TOUCH, 1);
						input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, id);
						input_report_abs(ts->input_dev, ABS_MT_POSITION_X, input_x);
						input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, input_y);
						input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 15);
						input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, input_w);

						input_mt_sync(ts->input_dev);

						TouchDown = TRUE;
				}
		}else {
				//printk("\n touch up \n");
				input_report_key(ts->input_dev, BTN_TOUCH, 0);
				input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
				input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0);
				input_mt_sync(ts->input_dev);

				 TouchDown = false;
		}

		input_sync(ts->input_dev);

DEAL_END:
	ret = gtp_i2c_write(ts->client, end_cmd, 3);
	enable_irq(ts->client->irq);
	return;


}

static irqreturn_t guitar_ts_irq_handler(s32 irq, void *dev_id)
{
   struct goodix_ts_data *ts = (struct goodix_ts_data*)dev_id;

//  printk("guitar_ts_irq_handler \n");
    disable_irq_nosync(ts->client->irq);
    queue_delayed_work(msd_tp_wq, &ts->work, msecs_to_jiffies(DELAY_WORK_TIME));

    return IRQ_HANDLED;
}

static s8 gtp_request_irq(struct goodix_ts_data *ts)
{
		s32 ret = -1;

	 ts->client->irq=TS_INT;
    ret = request_irq(ts->client->irq, guitar_ts_irq_handler,
    					IRQF_DISABLED | IRQ_TYPE_EDGE_FALLING,//IRQ_TYPE_EDGE_RISING,
                      ts->client->name, ts);
    if (ret)
    {
        dev_err(&ts->client->dev,"Cannot allocate ts INT!ERROR:%d\n", ret);
    }
    else
    {
        disable_irq(ts->client->irq);
    }

    return ret;
}



static s8 gtp_request_input_dev(struct goodix_ts_data *ts)
{
		s8 ret = -1;

		ts->input_dev = input_allocate_device();
		if(ts->input_dev == NULL)
		{
				GTP_ERROR("Failed to allocate input device.\n");
				return -ENOMEM;
		}

		ts->input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
		ts->input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);

		input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, SCREEN_MAX_HEIGHT, 0, 0);
		input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, SCREEN_MAX_WIDTH, 0, 0);

		input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);
		input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
		input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0, 255, 0, 0);


		sprintf(ts->phys, "input/ts");
		ts->input_dev->name ="Goodix-TS";
	    ts->input_dev->phys = ts->phys;
	    ts->input_dev->id.bustype = BUS_I2C;
	    ts->input_dev->id.vendor = 0xDEAD;
	    ts->input_dev->id.product = 0xBEEF;
	    ts->input_dev->id.version = 10427;

    ret = input_register_device(ts->input_dev);

    if (ret)
    {
        dev_err(&ts->client->dev,"Probe: Unable to register %s input device\n", ts->input_dev->name);
        input_free_device(ts->input_dev);
        return -ENODEV;
    }

    return 0;
}




static s32 gtp_init_panel(struct goodix_ts_data *ts)
{
		s32 ret = 0;
		u8 check_sum, i;

	u8 config[]={0x80,0x47,0x42,(u8)TOUCH_MAX_HEIGHT, (u8)(TOUCH_MAX_HEIGHT>>8),(u8)TOUCH_MAX_WIDTH, (u8)(TOUCH_MAX_WIDTH>>8),
		0x05,0x0C,0x00,0x01,0x08,0x19,0x05,0x50,0x3C,0x03,
		0x05,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x87,0x28,0x0A,0x48,0x46,
		0x46,0x08,0x00,0x00,0x00,0x99,0x02,0x25,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x46,0x8C,0x94,0x05,0x02,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x04,0x06,0x08,0x0A,
		0x0C,0x0E,0x10,0x12,0x14,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x04,0x06,0x08,0x0A,0x0C,0x1D,0x1E,
		0x1F,0x20,0x21,0x22,0x24,0x26,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x95,0x01};



	  check_sum = 0;
    for (i = GTP_ADDR_LENGTH; i < (sizeof(config)/sizeof(config[0])-2); i++)
    {
        check_sum += config[i];
    }
    config[(sizeof(config)/sizeof(config[0])) - 2] = (~check_sum) + 1;




	ret = gtp_i2c_write(ts->client, config , sizeof(config)/sizeof(config[0]));
	if (ret < 0){
		printk("\n--gtp_init_panel fail-\n");
		goto error_i2c_transfer;
	}
	else
		printk("\n--gtp_init_panel success \n");


	msleep(1);
	return 0;

error_i2c_transfer:
	return ret;
}



void gtp_reset_guitar(struct i2c_client *client, s32 ms)
{

    GTP_GPIO_REQUEST(GTP_RST_PORT, "TS_RST");    //Request IO
	GTP_GPIO_REQUEST(GTP_INT_PORT, "TS_INT");    //Request IO


    GTP_GPIO_OUTPUT(GTP_RST_PORT, 0);   //begin select I2C slave addr
    msleep(ms);
    GTP_GPIO_OUTPUT(GTP_INT_PORT, client->addr == 0x14);

    msleep(2);
    GTP_GPIO_OUTPUT(GTP_RST_PORT, 1);

    msleep(6);                          //must > 3ms

    GTP_GPIO_OUTPUT(GTP_INT_PORT, 0);
    msleep(50);
    GTP_GPIO_AS_INT(GTP_INT_PORT);


}

static s32 msd_tp_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
		s32 ret = 0;
		//u16 version_info;
		u8 count;
		struct goodix_ts_data *ts;

		//do NOT remove these output log
    printk("GTP Driver Version:%s",GTP_DRIVER_VERSION);
    printk("GTP Driver build@%s,%s", __TIME__,__DATE__);
    printk("GTP I2C Address:0x%02x", client->addr);

    //Check I2C function
    if(!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
    {
    		dev_err(&client->dev, "i2c check failed!\n");
    		return -ENODEV;
    }

    ts = kzalloc(sizeof(*ts), GFP_KERNEL);
    if(ts == NULL)
    {
    		GTP_ERROR("alloc GFP_KERNEL memory failed.\n");
    		return -ENOMEM;
    }

    memset(ts, 0, sizeof(*ts));

    ts->client = client;
    i2c_set_clientdata(client, ts);

//    ret = gtp_request_irq(ts);
//    if(ret < 0)
//    {
//    		GTP_ERROR("GTP request irq failed.\n");
//    		goto tp_err;
//    }
//
    gtp_reset_guitar(ts->client, 20);

    ret = gtp_request_input_dev(ts);
    if (ret < 0)
    {
    		GTP_ERROR("GTP request input dev failed.\n");
    }



    for(count = 0; count < 3; count++){

    		  ret = gtp_init_panel(ts);
				  if(ret < 0)
				  {
				    GTP_ERROR("TP init panel failed.\n");
				    continue;
				  } else {
//				  		sg_pMbxRegs = (PREG_MBX_st)IO_ADDRESS(REG_MBX_BASE);
							printk("goodix_init_panel OK ");
							break;
				  }
    }

    if(count >= 3)
    {
    		printk("\n---tp init failed----\n");
    		goto tp_err;
    }


   /* ret = gtp_read_version(client, &version_info);
    if(ret < 0)
    {
    		GTP_ERROR("Read version failed.\n");
    }*/

    // psw0523 fix for quickboot

    INIT_DELAYED_WORK(&ts->work, goodix_ts_work_func);
    ret = gtp_request_irq(ts);
    if(ret < 0)
    {
    		GTP_ERROR("GTP request irq failed.\n");
    		goto tp_err;
    }

    enable_irq(ts->client->irq);

    return ret;

tp_err:
	i2c_set_clientdata(client, NULL);
	kfree(ts);
return ret;

}

static s32 msd_tp_remove(struct i2c_client *client)
{
		return 0;
}


static const struct i2c_device_id msd_tp_id[]={
    { MSD_I2C_NAME, 0 },
		{}
};


static struct i2c_driver msd_tp_driver = {
		.probe = msd_tp_probe,
		.remove = msd_tp_remove,
		.id_table = msd_tp_id,
		.driver = {
        .name   = MSD_I2C_NAME,
				.owner = THIS_MODULE,
			},
};




static s32 __devinit msd_tp_init(void)
{
		printk("msd_tp_init.\n");

		msd_tp_wq = create_singlethread_workqueue("msd_tp_wq");
		if(!msd_tp_wq){
				GTP_ERROR("create workqueue failed\n");
				return -ENOMEM;
		}

		return i2c_add_driver(&msd_tp_driver);
}


static void __exit msd_tp_exit(void)
{
		GTP_DEBUG(KERN_ALERT"Touchscreen driver of GPT exited.\n");

		i2c_del_driver(&msd_tp_driver);
		if(msd_tp_wq)
			destroy_workqueue(msd_tp_wq);
}


module_init(msd_tp_init);

module_exit(msd_tp_exit);


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GTP Series Driver");

