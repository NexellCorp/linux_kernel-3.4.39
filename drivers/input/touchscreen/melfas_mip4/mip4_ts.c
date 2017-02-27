/*
 * MELFAS MIP4 Touchscreen
 *
 * Copyright (C) 2016 MELFAS Inc.
 *
 *
 * mip4_ts.c : Main functions
 *
 * Version : 2016.09.06
 */

#include "mip4_ts.h"

#ifdef CONFIG_ANDROID
#if USE_WAKEUP_GESTURE
struct wake_lock mip4_ts_wake_lock;
#endif
#endif

/*
* Reboot chip
*/
void mip4_ts_reboot(struct mip4_ts_info *info)
{
	struct i2c_adapter *adapter = to_i2c_adapter(info->client->dev.parent);

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	i2c_lock_adapter(adapter);

	mip4_ts_power_off(info);
	mip4_ts_power_on(info);

	i2c_unlock_adapter(adapter);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
}

/*
* I2C Read
*/
int mip4_ts_i2c_read(struct mip4_ts_info *info, char *write_buf, unsigned int write_len, char *read_buf, unsigned int read_len)
{
	int retry = I2C_RETRY_COUNT;
	int res;

	struct i2c_msg msg[] = {
		{
			.addr = info->client->addr,
			.flags = 0,
			.buf = write_buf,
			.len = write_len,
		}, {
			.addr = info->client->addr,
			.flags = I2C_M_RD,
			.buf = read_buf,
			.len = read_len,
		},
	};

	while (retry--) {
		res = i2c_transfer(info->client->adapter, msg, ARRAY_SIZE(msg));

		if (res == ARRAY_SIZE(msg)) {
			goto exit;
		} else if (res < 0) {
			dev_err(&info->client->dev, "%s [ERROR] i2c_transfer - errno[%d]\n", __func__, res);
		} else {
			dev_err(&info->client->dev, "%s [ERROR] i2c_transfer - size[%zu] result[%d]\n", __func__, ARRAY_SIZE(msg), res);
		}
	}

	goto error_reboot;

error_reboot:
#if RESET_ON_I2C_ERROR
	mip4_ts_reboot(info);
#endif
	return 1;

exit:
	return 0;
}

/*
* I2C Write
*/
int mip4_ts_i2c_write(struct mip4_ts_info *info, char *write_buf, unsigned int write_len)
{
	int retry = I2C_RETRY_COUNT;
	int res;

	while (retry--) {
		res = i2c_master_send(info->client, write_buf, write_len);

		if (res == write_len) {
			goto exit;
		} else if (res < 0) {
			dev_err(&info->client->dev, "%s [ERROR] i2c_master_send - errno [%d]\n", __func__, res);
		} else {
			dev_err(&info->client->dev, "%s [ERROR] i2c_master_send - write[%d] result[%d]\n", __func__, write_len, res);
		}
	}

	goto error_reboot;

error_reboot:
#if RESET_ON_I2C_ERROR
	mip4_ts_reboot(info);
#endif
	return 1;

exit:
	return 0;
}

/*
* Enable device
*/
int mip4_ts_enable(struct mip4_ts_info *info)
{
	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	if (info->enabled) {
		dev_err(&info->client->dev, "%s [ERROR] device already enabled\n", __func__);
		goto exit;
	}

#if USE_WAKEUP_GESTURE
	mip4_ts_set_power_state(info, MIP4_CTRL_POWER_ACTIVE);

	if (wake_lock_active(&mip4_ts_wake_lock)) {
		wake_unlock(&mip4_ts_wake_lock);
		dev_dbg(&info->client->dev, "%s - wake_unlock\n", __func__);
	}

	info->gesture_wakeup_mode = false;
	dev_dbg(&info->client->dev, "%s - gesture wake-up mode : off\n", __func__);
#else
	mip4_ts_power_on(info);
#endif

#if 1
	if (info->disable_esd == true) {
		//Disable ESD alert
		mip4_ts_disable_esd_alert(info);
	}
#endif

	mutex_lock(&info->lock);

	if (info->irq_enabled == false) {
		enable_irq(info->client->irq);
		info->irq_enabled = true;
	}

	info->enabled = true;

	mutex_unlock(&info->lock);

exit:
	dev_info(&info->client->dev, MIP4_TS_DEVICE_NAME" - Enabled\n");

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;
}

/*
* Disable device
*/
int mip4_ts_disable(struct mip4_ts_info *info)
{
	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	if (!info->enabled) {
		dev_err(&info->client->dev, "%s [ERROR] device already disabled\n", __func__);
		goto exit;
	}

#if USE_WAKEUP_GESTURE
	info->wakeup_gesture_code = 0;

	mip4_ts_set_wakeup_gesture_type(info, MIP4_EVENT_GESTURE_ALL);
	mip4_ts_set_power_state(info, MIP4_CTRL_POWER_LOW);

	info->gesture_wakeup_mode = true;
	dev_dbg(&info->client->dev, "%s - gesture wake-up mode : on\n", __func__);

	if (!wake_lock_active(&mip4_ts_wake_lock)) {
		wake_lock(&mip4_ts_wake_lock);
		dev_dbg(&info->client->dev, "%s - wake_lock\n", __func__);
	}
#else
	mutex_lock(&info->lock);

	disable_irq(info->client->irq);
	info->irq_enabled = false;

	mutex_unlock(&info->lock);

	mip4_ts_power_off(info);
#endif

	mip4_ts_clear_input(info);

	info->enabled = false;

exit:
	dev_info(&info->client->dev, MIP4_TS_DEVICE_NAME" - Disabled\n");

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;
}

#if USE_INPUT_OPEN_CLOSE
/*
* Open input device
*/
static int mip4_ts_input_open(struct input_dev *dev)
{
	struct mip4_ts_info *info = input_get_drvdata(dev);

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	if (info->init == true) {
		info->init = false;
	} else {
		mip4_ts_enable(info);
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;
}

/*
* Close input device
*/
static void mip4_ts_input_close(struct input_dev *dev)
{
	struct mip4_ts_info *info = input_get_drvdata(dev);

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	mip4_ts_disable(info);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return;
}
#endif

/*
* Get ready status
*/
int mip4_ts_get_ready_status(struct mip4_ts_info *info)
{
	u8 wbuf[16];
	u8 rbuf[16];
	int ret = 0;

	//dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	wbuf[0] = MIP4_R0_CTRL;
	wbuf[1] = MIP4_R1_CTRL_READY_STATUS;
	if (mip4_ts_i2c_read(info, wbuf, 2, rbuf, 1)) {
		dev_err(&info->client->dev, "%s [ERROR] mip4_ts_i2c_read\n", __func__);
		goto error;
	}
	ret = rbuf[0];

	//check status
	if ((ret == MIP4_CTRL_STATUS_NONE) || (ret == MIP4_CTRL_STATUS_LOG) || (ret == MIP4_CTRL_STATUS_READY)) {
		//dev_dbg(&info->client->dev, "%s - status[0x%02X]\n", __func__, ret);
	} else {
		dev_err(&info->client->dev, "%s [ERROR] Unknown status[0x%02X]\n", __func__, ret);
		goto error;
	}

	if (ret == MIP4_CTRL_STATUS_LOG) {
		//skip log event
		wbuf[0] = MIP4_R0_LOG;
		wbuf[1] = MIP4_R1_LOG_TRIGGER;
		wbuf[2] = 0;
		if (mip4_ts_i2c_write(info, wbuf, 3)) {
			dev_err(&info->client->dev, "%s [ERROR] mip4_ts_i2c_write\n", __func__);
		}
	}

	//dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return ret;

error:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return -1;
}

/*
* Read chip firmware version
*/
int mip4_ts_get_fw_version(struct mip4_ts_info *info, u8 *ver_buf)
{
	u8 rbuf[8];
	u8 wbuf[2];
	int i;

	wbuf[0] = MIP4_R0_INFO;
	wbuf[1] = MIP4_R1_INFO_VERSION_BOOT;
	if (mip4_ts_i2c_read(info, wbuf, 2, rbuf, 8)) {
		goto error;
	};

	for (i = 0; i < FW_MAX_SECT_NUM; i++) {
		ver_buf[0 + i * 2] = rbuf[1 + i * 2];
		ver_buf[1 + i * 2] = rbuf[0 + i * 2];
	}

	return 0;

error:
	for (i = 0; i < (FW_MAX_SECT_NUM * 2); i++) {
		ver_buf[i] = 0xFF;
	}

	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return 1;
}

/*
* Read chip firmware version for u16
*/
int mip4_ts_get_fw_version_u16(struct mip4_ts_info *info, u16 *ver_buf_u16)
{
	u8 rbuf[8];
	int i;

	if (mip4_ts_get_fw_version(info, rbuf)) {
		goto error;
	}

	for (i = 0; i < FW_MAX_SECT_NUM; i++) {
		ver_buf_u16[i] = (rbuf[0 + i * 2] << 8) | rbuf[1 + i * 2];
	}

	return 0;

error:
	for (i = 0; i < FW_MAX_SECT_NUM; i++) {
		ver_buf_u16[i] = 0xFFFF;
	}

	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return 1;
}

#if (CHIP_MODEL != CHIP_NONE)
/*
* Read bin(file) firmware version
*/
int mip4_ts_get_fw_version_from_bin(struct mip4_ts_info *info, u8 *ver_buf)
{
	const struct firmware *fw;
	int i;

	dev_dbg(&info->client->dev,"%s [START]\n", __func__);

	request_firmware(&fw, FW_PATH_INTERNAL, &info->client->dev);

	if (!fw) {
		dev_err(&info->client->dev,"%s [ERROR] request_firmware\n", __func__);
		goto error;
	}

	if (mip4_ts_bin_fw_version(info, fw->data, fw->size, ver_buf)) {
		for (i = 0; i < (FW_MAX_SECT_NUM * 2); i++) {
			ver_buf[i] = 0xFF;
		}
		dev_err(&info->client->dev,"%s [ERROR] mip4_ts_bin_fw_version\n", __func__);
		goto error;
	}

	release_firmware(fw);

	dev_dbg(&info->client->dev,"%s [DONE]\n", __func__);
	return 0;

error:
	dev_err(&info->client->dev,"%s [ERROR]\n", __func__);
	return 1;
}
#endif

/*
* Set power state
*/
int mip4_ts_set_power_state(struct mip4_ts_info *info, u8 mode)
{
	u8 wbuf[3];

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	dev_dbg(&info->client->dev, "%s - mode[%02X]\n", __func__, mode);

	wbuf[0] = MIP4_R0_CTRL;
	wbuf[1] = MIP4_R1_CTRL_POWER_STATE;
	wbuf[2] = mode;
	if (mip4_ts_i2c_write(info, wbuf, 3)) {
		dev_err(&info->client->dev, "%s [ERROR] mip4_ts_i2c_write\n", __func__);
		goto error;
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

error:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return 1;
}

/*
* Set wake-up gesture type
*/
int mip4_ts_set_wakeup_gesture_type(struct mip4_ts_info *info, u32 type)
{
	u8 wbuf[6];

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	dev_dbg(&info->client->dev, "%s - type[%08X]\n", __func__, type);

	wbuf[0] = MIP4_R0_CTRL;
	wbuf[1] = MIP4_R1_CTRL_GESTURE_TYPE;
	wbuf[2] = (type >> 24) & 0xFF;
	wbuf[3] = (type >> 16) & 0xFF;
	wbuf[4] = (type >> 8) & 0xFF;
	wbuf[5] = type & 0xFF;
	if (mip4_ts_i2c_write(info, wbuf, 6)) {
		dev_err(&info->client->dev, "%s [ERROR] mip4_ts_i2c_write\n", __func__);
		goto error;
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

error:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return 1;
}

/*
* Disable ESD alert
*/
int mip4_ts_disable_esd_alert(struct mip4_ts_info *info)
{
	u8 wbuf[4];
	u8 rbuf[4];

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	wbuf[0] = MIP4_R0_CTRL;
	wbuf[1] = MIP4_R1_CTRL_DISABLE_ESD_ALERT;
	wbuf[2] = 1;
	if (mip4_ts_i2c_write(info, wbuf, 3)) {
		dev_err(&info->client->dev, "%s [ERROR] mip4_ts_i2c_write\n", __func__);
		goto error;
	}

	if (mip4_ts_i2c_read(info, wbuf, 2, rbuf, 1)) {
		dev_err(&info->client->dev, "%s [ERROR] mip4_ts_i2c_read\n", __func__);
		goto error;
	}

	if (rbuf[0] != 1) {
		dev_dbg(&info->client->dev, "%s [ERROR] failed\n", __func__);
		goto error;
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

error:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return 1;
}

/*
* Alert event handler - ESD
*/
static int mip4_ts_alert_handler_esd(struct mip4_ts_info *info, u8 *rbuf)
{
	u8 frame_cnt = rbuf[1];

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	dev_dbg(&info->client->dev, "%s - frame_cnt[%d]\n", __func__, frame_cnt);

	if (frame_cnt == 0) {
		//sensor crack, not ESD
		info->esd_cnt++;
		dev_dbg(&info->client->dev, "%s - esd_cnt[%d]\n", __func__, info->esd_cnt);

		if (info->disable_esd == true) {
			mip4_ts_disable_esd_alert(info);
			info->esd_cnt = 0;
		} else if (info->esd_cnt > ESD_COUNT_FOR_DISABLE) {
			//Disable ESD alert
			if (!mip4_ts_disable_esd_alert(info)) {
				info->disable_esd = true;
				info->esd_cnt = 0;
			}
		} else {
			//Reset chip
			mip4_ts_reboot(info);
		}
	} else {
		//ESD detected
		//Reset chip
		mip4_ts_reboot(info);
		info->esd_cnt = 0;
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;
}

/*
* Alert event handler - Wake-up
*/
static int mip4_ts_alert_handler_wakeup(struct mip4_ts_info *info, u8 *rbuf)
{
	int gesture_code = rbuf[1];

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	if (mip4_ts_gesture_wakeup_event_handler(info, gesture_code)) {
		goto error;
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

error:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return 1;
}

/*
* Alert event handler - Input type
*/
static int mip4_ts_alert_handler_inputtype(struct mip4_ts_info *info, u8 *rbuf)
{
	u8 input_type = rbuf[1];

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	switch (input_type) {
	case 0:
		dev_dbg(&info->client->dev, "%s - Input type : Finger\n", __func__);
		break;
	case 1:
		dev_dbg(&info->client->dev, "%s - Input type : Glove\n", __func__);
		break;
	default:
		dev_err(&info->client->dev, "%s - Input type : Unknown[%d]\n", __func__, input_type);
		goto error;
		break;
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

error:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return 1;
}

/*
* Alert event handler - Image
*/
static int mip4_ts_alert_handler_image(struct mip4_ts_info *info, u8 *rbuf)
{
	u8 image_type = rbuf[1];

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	switch (image_type) {
#if USE_WAKEUP_GESTURE
	case MIP4_IMG_TYPE_GESTURE:
		if (mip4_ts_gesture_wakeup_event_handler(info, 1)) {
			dev_err(&info->client->dev, "%s [ERROR] mip4_ts_wakeup_event_handler\n", __func__);
			goto error;
		}
		if (mip4_ts_get_image(info, rbuf[1])) {
			dev_err(&info->client->dev, "%s [ERROR] mip4_ts_get_image\n", __func__);
			goto error;
		}
		break;
#endif
	default:
		goto error;
		break;
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

error:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return 1;
}

/*
* Alert event handler - 0xF1
*/
static int __maybe_unused mip4_ts_alert_handler_F1(struct mip4_ts_info *info, u8 *rbuf, u8 size)
{
#if USE_LPWG
	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	if (mip4_ts_lpwg_event_handler(info, rbuf, size)) {
		goto error;
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

error:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return 1;
#else
	return 0;
#endif
}

/*
* Interrupt handler
*/
static irqreturn_t mip4_ts_interrupt(int irq, void *dev_id)
{
	struct mip4_ts_info *info = dev_id;
	struct i2c_client *client = info->client;
	u8 wbuf[8];
	u8 rbuf[256];
	unsigned int size = 0;
	u8 category = 0;
	u8 alert_type = 0;

	dev_dbg(&client->dev, "%s [START]\n", __func__);

	//Read packet info
	wbuf[0] = MIP4_R0_EVENT;
	wbuf[1] = MIP4_R1_EVENT_PACKET_INFO;
	if (mip4_ts_i2c_read(info, wbuf, 2, rbuf, 1)) {
		dev_err(&client->dev, "%s [ERROR] Read packet info\n", __func__);
		goto error;
	}

	size = (rbuf[0] & 0x7F);
	category = ((rbuf[0] >> 7) & 0x1);
	dev_dbg(&client->dev, "%s - packet info : size[%d] category[%d]\n", __func__, size, category);

	//Check size
	if (size <= 0) {
		dev_err(&client->dev, "%s [ERROR] Packet size [%d]\n", __func__, size);
		goto exit;
	}

	//Read packet data
	wbuf[0] = MIP4_R0_EVENT;
	wbuf[1] = MIP4_R1_EVENT_PACKET_DATA;
	if (mip4_ts_i2c_read(info, wbuf, 2, rbuf, size)) {
		dev_err(&client->dev, "%s [ERROR] Read packet data\n", __func__);
		goto error;
	}

	//Event handler
	if (category == 0) {
		//Touch event
		info->esd_cnt = 0;

		mip4_ts_input_event_handler(info, size, rbuf);
	} else {
		//Alert event
		alert_type = rbuf[0];

		dev_dbg(&client->dev, "%s - alert type[%d]\n", __func__, alert_type);

		switch (alert_type) {
		case MIP4_ALERT_ESD:
			//ESD detection
			if (mip4_ts_alert_handler_esd(info, rbuf)) {
				goto error;
			}
			break;
		case MIP4_ALERT_WAKEUP:
			//Wake-up gesture
			if (mip4_ts_alert_handler_wakeup(info, rbuf)) {
				goto error;
			}
			break;
		case MIP4_ALERT_INPUT_TYPE:
			//Input type changed
			if (mip4_ts_alert_handler_inputtype(info, rbuf)) {
				goto error;
			}
			break;
		case MIP4_ALERT_IMAGE:
			//Image
			if (mip4_ts_alert_handler_image(info, rbuf)) {
				goto error;
			}
			break;
		case MIP4_ALERT_F1:
			//0xF1
			if (mip4_ts_alert_handler_F1(info, rbuf, size)) {
				goto error;
			}
			break;
		default:
			dev_err(&client->dev, "%s [ERROR] Unknown alert type[%d]\n", __func__, alert_type);
			goto error;
			break;
		}
	}

exit:
	dev_dbg(&client->dev, "%s [DONE]\n", __func__);
	return IRQ_HANDLED;

error:
	if (RESET_ON_EVENT_ERROR) {
		dev_info(&client->dev, "%s - Reset on error\n", __func__);

		mip4_ts_reboot(info);
	}

	dev_err(&client->dev, "%s [ERROR]\n", __func__);
	return IRQ_HANDLED;
}

/*
* Config module
*/
int mip4_ts_config(struct mip4_ts_info *info)
{
	u8 wbuf[4];
	u8 rbuf[16];
	int ret = 0;
	int retry = I2C_RETRY_COUNT;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	//Product name
	wbuf[0] = MIP4_R0_INFO;
	wbuf[1] = MIP4_R1_INFO_PRODUCT_NAME;
	ret = mip4_ts_i2c_read(info, wbuf, 2, rbuf, 16);
	if (ret) {
		dev_err(&info->client->dev, "%s [ERROR] mip4_ts_i2c_read\n", __func__);
		goto error;
	}

	memcpy(info->product_name, rbuf, 16);
	dev_dbg(&info->client->dev, "%s - product_name[%s]\n", __func__, info->product_name);

	//Firmware version
	ret = mip4_ts_get_fw_version(info, rbuf);
	if (ret) {
		dev_err(&info->client->dev, "%s [ERROR] mip4_ts_i2c_read\n", __func__);
		goto error;
	}

	memcpy(info->fw_version, rbuf, 8);
	dev_info(&info->client->dev, "%s - F/W Version : %02X.%02X %02X.%02X %02X.%02X %02X.%02X\n", __func__, info->fw_version[0], info->fw_version[1], info->fw_version[2], info->fw_version[3], info->fw_version[4], info->fw_version[5], info->fw_version[6], info->fw_version[7]);

	//Resolution
	wbuf[0] = MIP4_R0_INFO;
	wbuf[1] = MIP4_R1_INFO_RESOLUTION_X;
	ret = mip4_ts_i2c_read(info, wbuf, 2, rbuf, 14);
	if (ret) {
		dev_err(&info->client->dev, "%s [ERROR] mip4_ts_i2c_read\n", __func__);
		goto error;
	}

	//Set resolution using chip info
	info->max_x = (rbuf[0]) | (rbuf[1] << 8);
	info->max_y = (rbuf[2]) | (rbuf[3] << 8);
	dev_dbg(&info->client->dev, "%s - max_x[%d] max_y[%d]\n", __func__, info->max_x, info->max_y);

	info->ppm_x = rbuf[12];
	info->ppm_y = rbuf[13];
	dev_dbg(&info->client->dev, "%s - ppm_x[%d] ppm_y[%d]\n", __func__, info->ppm_x, info->ppm_y);

	//Node info
	info->node_x = rbuf[4];
	info->node_y = rbuf[5];
	info->node_key = rbuf[6];
	dev_dbg(&info->client->dev, "%s - node_x[%d] node_y[%d] node_key[%d]\n", __func__, info->node_x, info->node_y, info->node_key);

	//Key info
	if (info->node_key > 0) {
		//Enable touchkey
		info->key_enable = true;
		info->key_num = info->node_key;
	}

	//Protocol
	wbuf[0] = MIP4_R0_EVENT;
	wbuf[1] = MIP4_R1_EVENT_FORMAT;
	while (retry--) {
		if (mip4_ts_i2c_read(info, wbuf, 2, rbuf, 3)) {
			dev_err(&info->client->dev, "%s [ERROR] mip4_ts_i2c_read - event format\n", __func__);
		} else {
			info->event_format = rbuf[0] | (rbuf[1] << 8);
			info->event_size = rbuf[2];
			if (info->event_size <= 0) {
				dev_err(&info->client->dev, "%s [ERROR] event_size[%d]\n", __func__, info->event_size);
				goto error;
			}
			dev_dbg(&info->client->dev, "%s - event_format[%d] event_size[%d]\n", __func__, info->event_format, info->event_size);
			break;
		}
	}
	if (retry < 0) {
		dev_err(&info->client->dev, "%s [ERROR] event format - retry limit\n", __func__);
		goto error;
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

error:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return 1;
}

/*
* Config platform
*/
static int mip4_ts_config_platform(struct mip4_ts_info *info)
{
#ifdef CONFIG_ACPI
	struct acpi_device *a_dev;
	acpi_status a_status;
#endif
	int ret = 0;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

#ifdef CONFIG_ACPI
	a_status = acpi_bus_get_device(ACPI_HANDLE(&info->client->dev), &a_dev);
	if (ACPI_SUCCESS(a_status)) {
		if (strncmp(dev_name(&a_dev->dev), ACPI_ID, 8) != 0) {
			dev_err(&info->client->dev, "%s [ERROR] ACPI_ID mismatch [%s]\n", __func__, dev_name(&a_dev->dev));
			ret = -EINVAL;
			goto exit;
		}
	}
#else
	if (&info->client->dev.of_node) {
		dev_dbg(&info->client->dev, "%s - Devicetree\n", __func__);

		info->pdata = devm_kzalloc(&info->client->dev, sizeof(struct mip4_ts_platform_data), GFP_KERNEL);
		if (!info->pdata) {
			dev_err(&info->client->dev, "%s [ERROR] pdata devm_kzalloc\n", __func__);
			ret = -ENOMEM;
			goto exit;
		}

#ifdef CONFIG_OF
		ret = mip4_ts_parse_devicetree(&info->client->dev, info);
		if (ret) {
			dev_err(&info->client->dev, "%s [ERROR] mip4_ts_parse_devicetree\n", __func__);
			goto exit;
		}
#endif
	} else {
		dev_dbg(&info->client->dev, "%s - Platform data\n", __func__);

		info->pdata = dev_get_platdata(&info->client->dev);
		if (info->pdata == NULL) {
			ret = -EINVAL;
			dev_err(&info->client->dev, "%s [ERROR] dev_get_platdata\n", __func__);
			goto exit;
		}
	}
#endif

exit:
	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return ret;
}

#if (CHIP_MODEL != CHIP_NONE)
/*
* Update firmware from kernel built-in binary
*/
int mip4_ts_fw_update_from_kernel(struct mip4_ts_info *info)
{
	const char *fw_name = FW_PATH_INTERNAL;
	const struct firmware *fw;
	int retires = 3;
	int ret = fw_err_none;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	//Disable IRQ
	mutex_lock(&info->lock);
	disable_irq(info->client->irq);

	//Get firmware
	request_firmware(&fw, fw_name, &info->client->dev);

	if (!fw) {
		dev_err(&info->client->dev, "%s [ERROR] request_firmware\n", __func__);
		ret = fw_err_file_open;
		goto error;
	}

	//Update firmware
	do {
		ret = mip4_ts_flash_fw(info, fw->data, fw->size, false, true);
		if (ret >= fw_err_none) {
			break;
		}
	} while (--retires);

	if (!retires) {
		dev_err(&info->client->dev, "%s [ERROR] mip4_ts_flash_fw failed\n", __func__);
		ret = fw_err_download;
	}

	release_firmware(fw);

	//Enable IRQ
	enable_irq(info->client->irq);
	mutex_unlock(&info->lock);

	if (ret < fw_err_none) {
		goto error;
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

error:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return ret;
}

/*
* Update firmware from external storage
*/
int mip4_ts_fw_update_from_storage(struct mip4_ts_info *info, char *path, bool force)
{
	struct file *fp;
	mm_segment_t old_fs;
	size_t fw_size, nread;
	int ret = fw_err_none;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	//Disable IRQ
	mutex_lock(&info->lock);
 	disable_irq(info->client->irq);
	mip4_ts_clear_input(info);

	//Get firmware
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(path, O_RDONLY, S_IRUSR);
	if (IS_ERR(fp)) {
		dev_err(&info->client->dev, "%s [ERROR] file_open - path[%s]\n", __func__, path);
		ret = fw_err_file_open;
		goto error;
	}

 	fw_size = fp->f_path.dentry->d_inode->i_size;
	if (0 < fw_size) {
		//Read firmware
		unsigned char *fw_data;
		fw_data = kzalloc(fw_size, GFP_KERNEL);
		nread = vfs_read(fp, (char __user *)fw_data, fw_size, &fp->f_pos);
		dev_dbg(&info->client->dev, "%s - path[%s] size[%zu]\n", __func__, path, fw_size);

		if (nread != fw_size) {
			dev_err(&info->client->dev, "%s [ERROR] vfs_read - size[%zu] read[%zu]\n", __func__, fw_size, nread);
			ret = fw_err_file_read;
		} else {
			//Update firmware
			ret = mip4_ts_flash_fw(info, fw_data, fw_size, force, true);
		}

		kfree(fw_data);
	} else {
		dev_err(&info->client->dev, "%s [ERROR] fw_size[%zu]\n", __func__, fw_size);
		ret = fw_err_file_read;
	}

 	filp_close(fp, current->files);

error:
	set_fs(old_fs);

	//Enable IRQ
	enable_irq(info->client->irq);
	mutex_unlock(&info->lock);

	if (ret < fw_err_none) {
		dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	} else {
		dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	}

	return ret;
}

static ssize_t mip4_ts_sys_fw_update(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mip4_ts_info *info = i2c_get_clientdata(client);
	int result = 0;
	u8 data[255];
	int ret = 0;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	//Update firmware
	ret = mip4_ts_fw_update_from_storage(info, info->fw_path_ext, true);

	switch (ret) {
	case fw_err_none:
		sprintf(data, "F/W update success.\n");
		break;
	case fw_err_uptodate:
		sprintf(data, "F/W is already up-to-date.\n");
		break;
	case fw_err_download:
		sprintf(data, "F/W update failed : Download error\n");
		break;
	case fw_err_file_type:
		sprintf(data, "F/W update failed : File type error\n");
		break;
	case fw_err_file_open:
		sprintf(data, "F/W update failed : File open error[%s]\n", info->fw_path_ext);
		break;
	case fw_err_file_read:
		sprintf(data, "F/W update failed : File read error\n");
		break;
	default:
		sprintf(data, "F/W update failed.\n");
		break;
	}

	//Re-config driver
	mip4_ts_config(info);
	mip4_ts_config_input(info);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);

	result = snprintf(buf, 255, "%s\n", data);
	return result;
}
static DEVICE_ATTR(fw_update, S_IRUGO, mip4_ts_sys_fw_update, NULL);
#endif

/*
* Sysfs attr info
*/
static struct attribute *mip4_ts_attrs[] = {
#if (CHIP_MODEL != CHIP_NONE)
	&dev_attr_fw_update.attr,
#endif
	NULL,
};

/*
* Sysfs attr group info
*/
static const struct attribute_group mip4_ts_attr_group = {
	.attrs = mip4_ts_attrs,
};

/*
* Initialize driver
*/
static int mip4_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct mip4_ts_info *info;
	struct input_dev *input_dev;
	int ret = 0;

	dev_dbg(&client->dev, "%s [START]\n", __func__);

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "%s [ERROR] i2c_check_functionality\n", __func__);
		ret = -EIO;
		goto ERROR_I2C;
	}

	//Init info data
	info = devm_kzalloc(&client->dev, sizeof(struct mip4_ts_info), GFP_KERNEL);
#ifdef CONFIG_OF
	input_dev = devm_input_allocate_device(&client->dev);
#else
	input_dev = input_allocate_device();
#endif
	if (!info || !input_dev) {
		dev_err(&client->dev, "%s [ERROR]\n", __func__);
		ret = -ENOMEM;
		goto ERROR_INFO;
	}

	info->client = client;
	info->input_dev = input_dev;
	info->init = true;
	info->power = -1;
	info->irq_enabled = false;
	info->fw_path_ext = kstrdup(FW_PATH_EXTERNAL, GFP_KERNEL);
	mutex_init(&info->lock);

	//Config platform
	ret = mip4_ts_config_platform(info);
	if (ret) {
		dev_err(&client->dev, "%s [ERROR] mip4_ts_config_platform\n", __func__);
		goto ERROR_INFO;
	}

	//Init input device
	info->input_dev->name = "MELFAS_" CHIP_NAME "_Touchscreen";
	snprintf(info->phys, sizeof(info->phys), "%s/input0", info->input_dev->name);
	info->input_dev->phys = info->phys;
	info->input_dev->id.bustype = BUS_I2C;
	info->input_dev->dev.parent = &client->dev;
#if USE_INPUT_OPEN_CLOSE
	info->input_dev->open = mip4_ts_input_open;
	info->input_dev->close = mip4_ts_input_close;
#endif

	//Set info data
	input_set_drvdata(input_dev, info);
	i2c_set_clientdata(client, info);

	//Config regulator
	mip4_ts_config_regulator(info);

	//Power on
	mip4_ts_power_on(info);

	//Firmware update
#if USE_AUTO_FW_UPDATE
	ret = mip4_ts_fw_update_from_kernel(info);
	if (ret) {
		dev_err(&client->dev, "%s [ERROR] mip4_ts_fw_update_from_kernel\n", __func__);
	}
#endif

	//Config module
	ret = mip4_ts_config(info);
	if (ret) {
		dev_err(&client->dev, "%s [ERROR] mip4_ts_config\n", __func__);
		goto ERROR_DEVICE;
	}

	//Config input interface
	mip4_ts_config_input(info);

	//Register input device
	ret = input_register_device(input_dev);
	if (ret) {
		dev_err(&client->dev, "%s [ERROR] input_register_device\n", __func__);
		ret = -EIO;
		goto ERROR_DEVICE;
	}

#if USE_WAKEUP_GESTURE
	//Wake-lock for wake-up gesture mode
	wake_lock_init(&mip4_ts_wake_lock, WAKE_LOCK_SUSPEND, "mip4_ts_wake_lock");
#endif

	//Set interrupt handler
	ret = devm_request_threaded_irq(&client->dev, client->irq, NULL, mip4_ts_interrupt, IRQF_TRIGGER_LOW | IRQF_ONESHOT, MIP4_TS_DEVICE_NAME, info);
	//ret = devm_request_threaded_irq(&client->dev, client->irq, NULL, mip4_ts_interrupt, IRQF_TRIGGER_FALLING | IRQF_ONESHOT, MIP4_TS_DEVICE_NAME, info);
	if (ret) {
		dev_err(&client->dev, "%s [ERROR] request_threaded_irq\n", __func__);
		goto ERROR_IRQ;
	}

	disable_irq(client->irq);
	info->irq = client->irq;

	//Enable device
	mip4_ts_enable(info);

#if USE_DEV
	//Create dev node (optional)
	if (mip4_ts_dev_create(info)) {
		dev_err(&client->dev, "%s [ERROR] mip4_ts_dev_create\n", __func__);
	}

	//Create dev
	info->class = class_create(THIS_MODULE, MIP4_TS_DEVICE_NAME);
	device_create(info->class, NULL, info->mip4_ts_dev, NULL, MIP4_TS_DEVICE_NAME);
#endif

#if USE_SYS
	//Create sysfs for test mode (optional)
	if (mip4_ts_sysfs_create(info)) {
		dev_err(&client->dev, "%s [ERROR] mip4_ts_sysfs_create\n", __func__);
	}
#endif

#if USE_CMD
	//Create sysfs for command mode (optional)
	if (mip4_ts_sysfs_cmd_create(info)) {
		dev_err(&client->dev, "%s [ERROR] mip4_ts_sysfs_cmd_create\n", __func__);
	}
#endif

	//Create sysfs
	if (sysfs_create_group(&client->dev.kobj, &mip4_ts_attr_group)) {
		dev_err(&client->dev, "%s [ERROR] sysfs_create_group\n", __func__);
	}
	if (sysfs_create_link(NULL, &client->dev.kobj, MIP4_TS_DEVICE_NAME)) {
		dev_err(&client->dev, "%s [ERROR] sysfs_create_link\n", __func__);
	}

	dev_dbg(&client->dev, "%s [DONE]\n", __func__);
	dev_info(&client->dev, "MELFAS " CHIP_NAME " Touchscreen is initialized successfully.\n");
	return 0;

ERROR_IRQ:
	free_irq(info->irq, info);
ERROR_DEVICE:
	input_unregister_device(info->input_dev);
ERROR_INFO:
ERROR_I2C:
	dev_dbg(&client->dev, "%s [ERROR]\n", __func__);
	dev_err(&client->dev, "MELFAS " CHIP_NAME " Touchscreen initialization failed.\n");
	return ret;
}

/*
* Remove driver
*/
static int mip4_ts_remove(struct i2c_client *client)
{
	struct mip4_ts_info *info = i2c_get_clientdata(client);

#if USE_CMD
	mip4_ts_sysfs_cmd_remove(info);
#endif

#if USE_SYS
	mip4_ts_sysfs_remove(info);
#endif

	sysfs_remove_group(&info->client->dev.kobj, &mip4_ts_attr_group);
	sysfs_remove_link(NULL, MIP4_TS_DEVICE_NAME);

#if USE_DEV
	device_destroy(info->class, info->mip4_ts_dev);
	class_destroy(info->class);
#endif

	input_unregister_device(info->input_dev);

	return 0;
}

#ifdef CONFIG_PM
/*
* Device suspend event handler
*/
int mip4_ts_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mip4_ts_info *info = i2c_get_clientdata(client);

	dev_dbg(&client->dev, "%s [START]\n", __func__);

	mip4_ts_disable(info);

	dev_dbg(&client->dev, "%s [DONE]\n", __func__);

	return 0;
}

/*
* Device resume event handler
*/
int mip4_ts_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mip4_ts_info *info = i2c_get_clientdata(client);
	int ret = 0;

	dev_dbg(&client->dev, "%s [START]\n", __func__);

	mip4_ts_enable(info);

	dev_dbg(&client->dev, "%s [DONE]\n", __func__);

	return ret;
}

/*
* PM info
*/
const struct dev_pm_ops mip4_ts_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mip4_ts_suspend, mip4_ts_resume)
};
#endif

#ifdef CONFIG_OF
/*
* Device tree match table
*/
static const struct of_device_id mip4_ts_match_table[] = {
	{.compatible = "melfas,mip4_ts",},
	{},
};
MODULE_DEVICE_TABLE(of, mip4_ts_match_table);
#endif

#ifdef CONFIG_ACPI
/*
* ACPI match table
*/
static const struct acpi_device_id mip4_ts_acpi_match_table[] = {
	{ACPI_ID, 0},
	{},
};
MODULE_DEVICE_TABLE(acpi, mip4_ts_acpi_match_table);
#endif

/*
* I2C Device ID
*/
static const struct i2c_device_id mip4_ts_id[] = {
	{MIP4_TS_DEVICE_NAME, 0},
};
MODULE_DEVICE_TABLE(i2c, mip4_ts_id);

/*
* I2C driver info
*/
static struct i2c_driver mip4_ts_driver = {
	.id_table = mip4_ts_id,
	.probe = mip4_ts_probe,
	.remove = mip4_ts_remove,
	.driver = {
		.name = MIP4_TS_DEVICE_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(mip4_ts_match_table),
#endif
#ifdef CONFIG_ACPI
		.acpi_match_table = ACPI_PTR(mip4_ts_acpi_match_table),
#endif
#ifdef CONFIG_PM
		.pm = &mip4_ts_pm_ops,
#endif
	},
};

module_i2c_driver(mip4_ts_driver);

MODULE_DESCRIPTION("MELFAS MIP4 Touchscreen");
MODULE_VERSION("2016.10.04");
MODULE_AUTHOR("Sangwon Jee <jeesw@melfas.com>");
MODULE_LICENSE("GPL");

