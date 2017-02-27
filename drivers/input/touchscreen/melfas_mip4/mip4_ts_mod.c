/*
 * MELFAS MIP4 Touchscreen
 *
 * Copyright (C) 2016 MELFAS Inc.
 *
 *
 * mip4_ts_mod.c : Model dependent functions
 *
 * Version : 2016.09.06
 */

#include "mip4_ts.h"

/*
* Config regulator
*/
int mip4_ts_config_regulator(struct mip4_ts_info *info)
{
	int ret = 0;

#ifdef CONFIG_REGULATOR
	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	info->regulator_vd33 = regulator_get(&info->client->dev, "tsp_3p3v");

	if (IS_ERR_OR_NULL(info->regulator_vd33)) {
		dev_err(&info->client->dev, "%s [ERROR] regulator_get : tsp_3p3v\n", __func__);
		ret = PTR_ERR(info->regulator_vd33);
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
#endif

	return ret;
}

/*
* Control regulator
*/
int mip4_ts_regulator_control(struct mip4_ts_info *info, int enable)
{
	//////////////////////////
	// PLEASE MODIFY HERE !!!
	//

#ifdef CONFIG_REGULATOR
	int ret = 0;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);
	dev_dbg(&info->client->dev, "%s - switch : %d\n", __func__, enable);

	if (info->power == enable) {
		dev_dbg(&info->client->dev, "%s - skip\n", __func__);
		goto exit;
	}

	if (IS_ERR_OR_NULL(info->regulator_vd33)) {
		dev_err(&info->client->dev, "%s [ERROR] vd33 not found\n", __func__);
		goto exit;
	}

	if (enable) {
		ret = regulator_enable(info->regulator_vd33);
		if(ret){
			dev_err(&info->client->dev, "%s [ERROR] regulator_enable : vd33\n", __func__);
			goto error;
		}
#if 0// PJSIN 20170224 add-- [ 1
		if (!IS_ERR_OR_NULL(info->pinctrl)) {
			ret = pinctrl_select_state(info->pinctrl, info->pins_enable);
			if (ret < 0) {
				dev_err(&info->client->dev, "%s [ERROR] pinctrl_select_state : pins_enable\n", __func__);
			}
		}
#endif// ]-- end
	} else {
		if (regulator_is_enabled(info->regulator_vd33)) {
			regulator_disable(info->regulator_vd33);
		}

#if 0// PJSIN 20170224 add-- [ 1
		if (!IS_ERR_OR_NULL(info->pinctrl)) {
			ret = pinctrl_select_state(info->pinctrl, info->pins_disable);
			if (ret < 0) {
				dev_err(&info->client->dev, "%s [ERROR] pinctrl_select_state : pins_disable\n", __func__);
			}
		}
#endif// ]-- end
	}

	info->power = enable;

	goto exit;

	//
	//////////////////////////

error:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return ret;

exit:
	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
#endif

	return 0;
}

/*
* Turn off power supply
*/
int mip4_ts_power_off(struct mip4_ts_info *info)
{
	//int ret = 0;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	//////////////////////////
	// PLEASE MODIFY HERE !!!
	//

	//Use CE pin
	//gpio_direction_output(info->gpio_ce, 0);

	//Use VD33 regulator
	mip4_ts_regulator_control(info, 0);

	//Use pinctrl
	/*
	if (!IS_ERR_OR_NULL(info->pinctrl)) {
		ret = pinctrl_select_state(info->pinctrl, info->pins_disable);
		if (ret < 0) {
			dev_err(&info->client->dev, "%s [ERROR] pinctrl_select_state : pins_disable\n", __func__);
			goto ERROR;
		}
	}
	*/

	//
	//////////////////////////

	msleep(1);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

//ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return -1;
}

/*
* Turn on power supply
*/
int mip4_ts_power_on(struct mip4_ts_info *info)
{
	//int ret = 0;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	//////////////////////////
	// PLEASE MODIFY HERE !!!
	//

	//Use VD33 regulator
	mip4_ts_regulator_control(info, 1);

	//Use CE pin
	//gpio_direction_output(info->gpio_ce, 1);

	//Use pinctrl
	/*
	if (!IS_ERR_OR_NULL(info->pinctrl)) {
		ret = pinctrl_select_state(info->pinctrl, info->pins_enable);
		if (ret < 0) {
			dev_err(&info->client->dev, "%s [ERROR] pinctrl_select_state : pins_enable\n", __func__);
			goto ERROR;
		}
	}
	*/

	//
	//////////////////////////

	//Booting delay
	msleep(200);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

//ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return -1;
}

/*
* Clear touch input event status
*/
void mip4_ts_clear_input(struct mip4_ts_info *info)
{
	int i;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	//Screen
	for(i = 0; i < MAX_FINGER_NUM; i++){
		/////////////////////////////////
		// PLEASE MODIFY HERE !!!
		//

#if INPUT_REPORT_TYPE
		input_mt_slot(info->input_dev, i);
		input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 0);
#else
		input_report_key(info->input_dev, BTN_TOUCH, 0);
		//input_report_key(info->input_dev, BTN_TOOL_FINGER, 0);
		//input_mt_sync(info->input_dev);
#endif

		info->touch_state[i] = 0;

		//
		/////////////////////////////////
	}

	//Key
	if (info->key_enable == true) {
		for (i = 0; i < info->key_num; i++) {
			input_report_key(info->input_dev, info->key_code[i], 0);
		}
	}

	input_sync(info->input_dev);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);

	return;
}

/*
* Input event handler - Report input event
*/
void mip4_ts_input_event_handler(struct mip4_ts_info *info, u8 sz, u8 *buf)
{
	int i;
	int type;
	int id;
	int hover = 0;
	int palm = 0;
	int state = 0;
	int x, y, z;
	int size = 0;
	int pressure_stage = 0;
	int pressure = 0;
	int touch_major = 0;
	int touch_minor = 0;
#if !INPUT_REPORT_TYPE
	int finger_id = 0;
	int finger_cnt = 0;
#endif

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);
	//print_hex_dump(KERN_ERR, MIP4_TS_DEVICE_NAME " Event Packet : ", DUMP_PREFIX_OFFSET, 16, 1, buf, sz, false);

	for (i = 0; i < sz; i += info->event_size) {
		u8 *packet = &buf[i];

		//Event format & type
		if ((info->event_format == 0) || (info->event_format == 1)) {
			type = (packet[0] & 0x40) >> 6;
		} else if (info->event_format == 3) {
			type = (packet[0] & 0xF0) >> 4;
		} else {
			dev_err(&info->client->dev, "%s [ERROR] Unknown event format [%d]\n", __func__, info->event_format);
			goto ERROR;
		}
		dev_dbg(&info->client->dev, "%s - Type[%d]\n", __func__, type);

		//Report input event
		if (type == MIP4_EVENT_INPUT_TYPE_KEY) {
			//Key event
			if ((info->event_format == 0) || (info->event_format == 1)) {
				id = packet[0] & 0x0F;
				state = (packet[0] & 0x80) >> 7;
			} else if (info->event_format == 3) {
				id = packet[0] & 0x0F;
				state = (packet[1] & 0x01);
			} else {
				dev_err(&info->client->dev, "%s [ERROR] Unknown event format [%d]\n", __func__, info->event_format);
				goto ERROR;
			}

			//Report key event
			if ((id >= 1) && (id <= info->key_num)) {
				/////////////////////////////////
				// PLEASE MODIFY HERE !!!
				//

				int keycode = info->key_code[id - 1];

				input_report_key(info->input_dev, keycode, state);

				dev_dbg(&info->client->dev, "%s - Key : ID[%d] Code[%d] Event[%d]\n", __func__, id, keycode, state);

				//
				//
				/////////////////////////////////
			} else {
				dev_err(&info->client->dev, "%s [ERROR] Unknown Key ID [%d]\n", __func__, id);
				continue;
			}
		} else if (type == MIP4_EVENT_INPUT_TYPE_SCREEN) {
			//Screen event
			if (info->event_format == 0) {
				//Touch only
				state = (packet[0] & 0x80) >> 7;
				hover = (packet[0] & 0x20) >> 5;
				palm = (packet[0] & 0x10) >> 4;
				id = (packet[0] & 0x0F) - 1;
				x = ((packet[1] & 0x0F) << 8) | packet[2];
				y = (((packet[1] >> 4) & 0x0F) << 8) | packet[3];
				pressure = packet[4];
				size = packet[5];
				touch_major = packet[5];
				touch_minor = packet[5];
			} else if (info->event_format == 1) {
				//Touch only
				state = (packet[0] & 0x80) >> 7;
				hover = (packet[0] & 0x20) >> 5;
				palm = (packet[0] & 0x10) >> 4;
				id = (packet[0] & 0x0F) - 1;
				x = ((packet[1] & 0x0F) << 8) | packet[2];
				y = (((packet[1] >> 4) & 0x0F) << 8) | packet[3];
				pressure = packet[4];
				size = packet[5];
				touch_major = packet[6];
				touch_minor = packet[7];
			} else if (info->event_format == 3) {
				//Touch + Force(Pressure)
				id = (packet[0] & 0x0F) - 1;
				hover = (packet[1] & 0x04) >> 2;
				palm = (packet[1] & 0x02) >> 1;
				state = (packet[1] & 0x01);
				x = ((packet[2] & 0x0F) << 8) | packet[3];
				y = (((packet[2] >> 4) & 0x0F) << 8) | packet[4];
				z = packet[5];
				size = packet[6];
				pressure_stage = (packet[7] & 0xF0) >> 4;
				pressure = ((packet[7] & 0x0F) << 8) | packet[8];
				touch_major = packet[9];
				touch_minor = packet[10];
			} else {
				dev_err(&info->client->dev, "%s [ERROR] Unknown event format [%d]\n", __func__, info->event_format);
				goto ERROR;
			}

			/////////////////////////////////
			// PLEASE MODIFY HERE !!!
			//

			//Report screen event
			if (state == 1) {
				//Press or Move event
#if INPUT_REPORT_TYPE
				input_mt_slot(info->input_dev, id);
				input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, true);
				input_report_abs(info->input_dev, ABS_MT_POSITION_X, x);
				input_report_abs(info->input_dev, ABS_MT_POSITION_Y, y);
				input_report_abs(info->input_dev, ABS_MT_PRESSURE, pressure);
				input_report_abs(info->input_dev, ABS_MT_TOUCH_MAJOR, touch_major);
				input_report_abs(info->input_dev, ABS_MT_TOUCH_MINOR, touch_minor);
#else
				input_report_abs(info->input_dev, ABS_MT_PRESSURE, pressure);
				input_report_abs(info->input_dev, ABS_MT_TOUCH_MAJOR, touch_major);
				input_report_abs(info->input_dev, ABS_MT_TOUCH_MINOR, touch_minor);
				input_report_abs(info->input_dev, ABS_MT_TRACKING_ID, id);
				input_report_key(info->input_dev, BTN_TOUCH, 1);
				//input_report_key(info->input_dev, BTN_TOOL_FINGER, 1);
				input_report_abs(info->input_dev, ABS_MT_POSITION_X, x);
				input_report_abs(info->input_dev, ABS_MT_POSITION_Y, y);
				input_mt_sync(info->input_dev);
#endif
				info->touch_state[id] = 1;

				dev_dbg(&info->client->dev, "%s - Screen : ID[%d] X[%d] Y[%d] Z[%d] Major[%d] Minor[%d] Size[%d] Pressure[%d] Palm[%d] Hover[%d]\n", __func__, id, x, y, pressure, touch_major, touch_minor, size, pressure, palm, hover);
			} else if (state == 0) {
				//Release event
#if INPUT_REPORT_TYPE
				input_mt_slot(info->input_dev, id);
				input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 0);
#else
				//input_report_key(info->input_dev, BTN_TOUCH, 0);
				//input_report_key(info->input_dev, BTN_TOOL_FINGER, 0);
				//input_mt_sync(info->input_dev);
#endif
				info->touch_state[id] = 0;

				dev_dbg(&info->client->dev, "%s - Screen : ID[%d] Release\n", __func__, id);

#if !INPUT_REPORT_TYPE
				//Final release event
				finger_cnt = 0;
				for (finger_id = 0; finger_id < MAX_FINGER_NUM; finger_id++) {
					if (info->touch_state[finger_id] != 0) {
						finger_cnt++;
						break;
					}
				}
				if (finger_cnt == 0) {
					input_report_key(info->input_dev, BTN_TOUCH, 0);
					//input_report_key(info->input_dev, BTN_TOOL_FINGER, 0);

					dev_dbg(&info->client->dev, "%s - Screen : Release\n", __func__);
				}
#endif
			} else {
				dev_err(&info->client->dev, "%s [ERROR] Unknown event state [%d]\n", __func__, state);
				goto ERROR;
			}

			//
			/////////////////////////////////
		}
		else if (type == MIP4_EVENT_INPUT_TYPE_PROXIMITY) {
			//Proximity event

			/////////////////////////////////
			// PLEASE MODIFY HERE !!!
			//

			state = (packet[1] & 0x01);
			z = packet[5];

			dev_dbg(&info->client->dev, "%s - Proximity : State[%d] Value[%d]\n", __func__, state, z);

			//
			/////////////////////////////////
		} else {
			dev_err(&info->client->dev, "%s [ERROR] Unknown event type [%d]\n", __func__, type);
			goto ERROR;
		}
	}

	input_sync(info->input_dev);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return;

ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return;
}

/*
* Wake-up gesture event handler
*/
int mip4_ts_gesture_wakeup_event_handler(struct mip4_ts_info *info, int gesture_code)
{
	u8 wbuf[4];

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	/////////////////////////////////
	// PLEASE MODIFY HERE !!!
	//

	//Report wake-up event

	dev_dbg(&info->client->dev, "%s - gesture[%d]\n", __func__, gesture_code);

	info->wakeup_gesture_code = gesture_code;

	switch (gesture_code) {
		case MIP4_EVENT_GESTURE_C:
		case MIP4_EVENT_GESTURE_W:
		case MIP4_EVENT_GESTURE_V:
		case MIP4_EVENT_GESTURE_M:
		case MIP4_EVENT_GESTURE_S:
		case MIP4_EVENT_GESTURE_Z:
		case MIP4_EVENT_GESTURE_O:
		case MIP4_EVENT_GESTURE_E:
		case MIP4_EVENT_GESTURE_V_90:
		case MIP4_EVENT_GESTURE_V_180:
		case MIP4_EVENT_GESTURE_FLICK_RIGHT:
		case MIP4_EVENT_GESTURE_FLICK_DOWN:
		case MIP4_EVENT_GESTURE_FLICK_LEFT:
		case MIP4_EVENT_GESTURE_FLICK_UP:
		case MIP4_EVENT_GESTURE_DOUBLE_TAP:
			//Example : emulate power key
			input_report_key(info->input_dev, KEY_POWER, 1);
			input_sync(info->input_dev);
			input_report_key(info->input_dev, KEY_POWER, 0);
			input_sync(info->input_dev);
			break;
		default:
			//Re-enter gesture wake-up mode
			wbuf[0] = MIP4_R0_CTRL;
			wbuf[1] = MIP4_R1_CTRL_POWER_STATE;
			wbuf[2] = MIP4_CTRL_POWER_LOW;
			if (mip4_ts_i2c_write(info, wbuf, 3)) {
				dev_err(&info->client->dev, "%s [ERROR] mip4_ts_i2c_write\n", __func__);
				goto ERROR;
			}
			break;
	}

	//
	//
	/////////////////////////////////

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

ERROR:
	return 1;
}

#ifdef CONFIG_OF
/*
* Parse device tree
*/
int mip4_ts_parse_devicetree(struct device *dev, struct mip4_ts_info *info)
{
	//struct i2c_client *client = to_i2c_client(dev);
	//struct mip4_ts_info *info = i2c_get_clientdata(client);
	struct device_node *np = dev->of_node;
	int ret;

	dev_dbg(dev, "%s [START]\n", __func__);

	/////////////////////////////////
	// PLEASE MODIFY HERE !!!
	//

	//Get GPIO
	ret = of_get_named_gpio(np, MIP4_TS_DEVICE_NAME",ce-gpio", 0);
	if (!gpio_is_valid(ret)) {
		dev_err(dev, "%s [ERROR] of_get_named_gpio : ce-gpio\n", __func__);
		goto error;
	} else {
		info->gpio_ce = ret;
	}

	//Request GPIO
	ret = gpio_request(info->gpio_ce, "ce-gpio");
	if (ret < 0) {
		dev_err(dev, "%s [ERROR] gpio_request : ce-gpio\n", __func__);
		goto error;
	}
	gpio_direction_output(info->gpio_ce, 0);

	//Get Pinctrl
	info->pinctrl = devm_pinctrl_get(&info->client->dev);
	if (IS_ERR(info->pinctrl)) {
		dev_err(dev, "%s [ERROR] devm_pinctrl_get\n", __func__);
		goto exit;
	}

	info->pins_enable = pinctrl_lookup_state(info->pinctrl, "enable");
	if (IS_ERR(info->pins_enable)) {
		dev_err(dev, "%s [ERROR] pinctrl_lookup_state enable\n", __func__);
	}

	info->pins_disable = pinctrl_lookup_state(info->pinctrl, "disable");
	if (IS_ERR(info->pins_disable)) {
		dev_err(dev, "%s [ERROR] pinctrl_lookup_state disable\n", __func__);
	}

	//
	/////////////////////////////////

exit:
	dev_dbg(dev, "%s [DONE]\n", __func__);
	return 0;

error:
	dev_err(dev, "%s [ERROR]\n", __func__);
	return 1;
}
#endif

/*
* Config input interface
*/
void mip4_ts_config_input(struct mip4_ts_info *info)
{
	struct input_dev *input_dev = info->input_dev;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	/////////////////////////////
	// PLEASE MODIFY HERE !!!
	//

	set_bit(EV_SYN, input_dev->evbit);
	set_bit(EV_ABS, input_dev->evbit);
	set_bit(EV_KEY, input_dev->evbit);

	//Screen
	set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

#if INPUT_REPORT_TYPE
	//input_mt_init_slots(input_dev, MAX_FINGER_NUM);
	input_mt_init_slots(input_dev, MAX_FINGER_NUM);//, INPUT_MT_DIRECT);
#else
	set_bit(BTN_TOUCH, input_dev->keybit);
	//set_bit(BTN_TOOL_FINGER, input_dev->keybit);
    input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0, MAX_FINGER_NUM, 0, 0);
#endif

	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, info->max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, info->max_y, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, INPUT_PRESSURE_MAX, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, INPUT_TOUCH_MAJOR_MAX, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MINOR, 0, INPUT_TOUCH_MINOR_MAX, 0, 0);

	//Key
	set_bit(KEY_BACK, input_dev->keybit);
	set_bit(KEY_MENU, input_dev->keybit);

	info->key_code[0] = KEY_BACK;
	info->key_code[1] = KEY_MENU;

#if USE_WAKEUP_GESTURE
	set_bit(KEY_POWER, input_dev->keybit);
#endif

	//
	/////////////////////////////

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return;
}

