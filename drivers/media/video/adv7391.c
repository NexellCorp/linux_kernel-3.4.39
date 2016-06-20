/*
 * adv7391 - ADV7391 Video Encoder Driver
 *
 * The encoder hardware does not support SECAM.
 *
 * Copyright (C) 2010-2012 ADVANSEE - http://www.advansee.com/
 * Benoît Thébaudeau <benoit.thebaudeau@advansee.com>
 *
 * Based on ADV7343 driver,
 *
 * Copyright (C) 2009 Texas Instruments Incorporated - http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed .as is. WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/ctype.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/videodev2.h>
#include <linux/uaccess.h>

#include <media/adv7391.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ctrls.h>

#include <mach/platform.h>
#include <mach/soc.h>

#include "adv7391.h"
#include "adv7391_regs.h"

MODULE_DESCRIPTION("ADV7391 video encoder driver");
MODULE_LICENSE("GPL");

static bool debug;
module_param(debug, bool, 0644);
MODULE_PARM_DESC(debug, "Debug level 0-1");

struct adv7391_state {
	struct v4l2_subdev sd;
	struct v4l2_ctrl_handler hdl;
	u8 reg00;
	u8 reg01;
	u8 reg02;
	u8 reg35;
	u8 reg80;
	u8 reg82;
	u32 output;
	v4l2_std_id std;
};

#if 1
#define DUMP_REGISTER 0
void dump_register_dpc(int module)
{
#if (DUMP_REGISTER)
#define DBGOUT(args...)  printk(args)
		NX_DPC_SetBaseAddress(module, (U32)IO_ADDRESS(NX_DPC_GetPhysicalAddress(module)));

		struct NX_DPC_RegisterSet *pREG =
			(struct NX_DPC_RegisterSet*)NX_DPC_GetBaseAddress(module);
		DBGOUT("DPC%d BASE ADDRESS: %p\n", module, pREG);
		DBGOUT(" DPCCTRL0     = 0x%04x\r\n", pREG->DPCCTRL0);
		DBGOUT(" DPCHTOTAL		=	0x%04x\r\n", pREG->DPCHTOTAL);
		DBGOUT(" DPCHSWIDTH		=	0x%04x\r\n", pREG->DPCHSWIDTH);
		DBGOUT(" DPCHASTART		=	0x%04x\r\n", pREG->DPCHASTART);
		DBGOUT(" DPCHAEND		=	0x%04x\r\n", pREG->DPCHAEND);
		DBGOUT(" DPCVTOTAL		=	0x%04x\r\n", pREG->DPCVTOTAL);
		DBGOUT(" DPCVSWIDTH		=	0x%04x\r\n", pREG->DPCVSWIDTH);
		DBGOUT(" DPCVASTART		=	0x%04x\r\n", pREG->DPCVASTART);
		DBGOUT(" DPCVAEND		=	0x%04x\r\n", pREG->DPCVAEND);
		DBGOUT(" DPCCTRL0		=	0x%04x\r\n", pREG->DPCCTRL0);
		DBGOUT(" DPCCTRL1		=	0x%04x\r\n", pREG->DPCCTRL1);
		DBGOUT(" DPCEVTOTAL		=	0x%04x\r\n", pREG->DPCEVTOTAL);
		DBGOUT(" DPCEVSWIDTH		=	0x%04x\r\n", pREG->DPCEVSWIDTH);
		DBGOUT(" DPCEVASTART		=	0x%04x\r\n", pREG->DPCEVASTART);
		DBGOUT(" DPCEVAEND		=	0x%04x\r\n", pREG->DPCEVAEND);
		DBGOUT(" DPCCTRL2		=	0x%04x\r\n", pREG->DPCCTRL2);
		DBGOUT(" DPCVSEOFFSET		=	0x%04x\r\n", pREG->DPCVSEOFFSET);
		DBGOUT(" DPCVSSOFFSET		=	0x%04x\r\n", pREG->DPCVSSOFFSET);
		DBGOUT(" DPCEVSEOFFSET		=	0x%04x\r\n", pREG->DPCEVSEOFFSET);
		DBGOUT(" DPCEVSSOFFSET		=	0x%04x\r\n", pREG->DPCEVSSOFFSET);
		DBGOUT(" DPCDELAY0		=	0x%04x\r\n", pREG->DPCDELAY0);
		DBGOUT(" DPCUPSCALECON0 	=	0x%04x\r\n", pREG->DPCUPSCALECON0);
		DBGOUT(" DPCUPSCALECON1 	=	0x%04x\r\n", pREG->DPCUPSCALECON1);
		DBGOUT(" DPCUPSCALECON2 	=	0x%04x\r\n", pREG->DPCUPSCALECON2);
		DBGOUT(" DPCRNUMGENCON0 	= 	0x%04x\r\n", pREG->DPCRNUMGENCON0);
		DBGOUT(" DPCRNUMGENCON1 	=	0x%04x\r\n", pREG->DPCRNUMGENCON1);
		DBGOUT(" DPCRNUMGENCON2 	=	0x%04x\r\n", pREG->DPCRNUMGENCON2);
		DBGOUT(" DPCRNDCONFORMULA_L	=	0x%04x\r\n", pREG->DPCRNDCONFORMULA_L);
		DBGOUT(" DPCRNDCONFORMULA_H	=	0x%04x\r\n", pREG->DPCRNDCONFORMULA_H);
		DBGOUT(" DPCFDTAddr		=	0x%04x\r\n", pREG->DPCFDTAddr);
		DBGOUT(" DPCFRDITHERVALUE	=	0x%04x\r\n", pREG->DPCFRDITHERVALUE);
		DBGOUT(" DPCFGDITHERVALUE	=	0x%04x\r\n", pREG->DPCFGDITHERVALUE);
		DBGOUT(" DPCFBDITHERVALUE	=	0x%04x\r\n", pREG->DPCFBDITHERVALUE);
		DBGOUT(" DPCDELAY1       	=	0x%04x\r\n", pREG->DPCDELAY1);
		DBGOUT(" DPCCMDBUFFERDATAL	=	0x%04x\r\n", pREG->DPCCMDBUFFERDATAL);
		DBGOUT(" DPCCMDBUFFERDATAH	=	0x%04x\r\n", pREG->DPCCMDBUFFERDATAH);
		DBGOUT(" DPCPOLCTRL      	=	0x%04x\r\n", pREG->DPCPOLCTRL);
		DBGOUT(" DPCRGBSHIFT     	=	0x%04x\r\n", pREG->DPCRGBSHIFT);
		DBGOUT(" DPCDATAFLUSH    	=	0x%04x\r\n", pREG->DPCDATAFLUSH);
		DBGOUT(" DPCCLKENB		=	0x%04x\r\n", pREG->DPCCLKENB);
#endif
}
#endif

static inline struct adv7391_state *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct adv7391_state, sd);
}

static inline struct v4l2_subdev *to_sd(struct v4l2_ctrl *ctrl)
{
	return &container_of(ctrl->handler, struct adv7391_state, hdl)->sd;
}

static inline int adv7391_write(struct v4l2_subdev *sd, u8 reg, u8 value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return i2c_smbus_write_byte_data(client, reg, value);
}

static const u8 adv7391_init_reg_val[] = {
	ADV7391_SOFT_RESET, ADV7391_SOFT_RESET_DEFAULT,
	ADV7391_POWER_MODE_REG, ADV7391_POWER_MODE_REG_DEFAULT,

	ADV7391_HD_MODE_REG1, ADV7391_HD_MODE_REG1_DEFAULT,
	ADV7391_HD_MODE_REG2, ADV7391_HD_MODE_REG2_DEFAULT,
	ADV7391_HD_MODE_REG3, ADV7391_HD_MODE_REG3_DEFAULT,
	ADV7391_HD_MODE_REG4, ADV7391_HD_MODE_REG4_DEFAULT,
	ADV7391_HD_MODE_REG5, ADV7391_HD_MODE_REG5_DEFAULT,
	ADV7391_HD_MODE_REG6, ADV7391_HD_MODE_REG6_DEFAULT,
	ADV7391_HD_MODE_REG7, ADV7391_HD_MODE_REG7_DEFAULT,

	ADV7391_SD_MODE_REG1, ADV7391_SD_MODE_REG1_DEFAULT,
	ADV7391_SD_MODE_REG2, ADV7391_SD_MODE_REG2_DEFAULT,
	ADV7391_SD_MODE_REG3, ADV7391_SD_MODE_REG3_DEFAULT,
	ADV7391_SD_MODE_REG4, ADV7391_SD_MODE_REG4_DEFAULT,
	ADV7391_SD_MODE_REG5, ADV7391_SD_MODE_REG5_DEFAULT,
	ADV7391_SD_MODE_REG6, ADV7391_SD_MODE_REG6_DEFAULT,
	ADV7391_SD_MODE_REG7, ADV7391_SD_MODE_REG7_DEFAULT,
	ADV7391_SD_MODE_REG8, ADV7391_SD_MODE_REG8_DEFAULT,

	ADV7391_SD_TIMING_REG0, ADV7391_SD_TIMING_REG0_DEFAULT,

	ADV7391_SD_HUE_ADJUST, ADV7391_SD_HUE_ADJUST_DEFAULT,
	ADV7391_SD_CGMS_WSS0, ADV7391_SD_CGMS_WSS0_DEFAULT,
	ADV7391_SD_BRIGHTNESS_WSS, ADV7391_SD_BRIGHTNESS_WSS_DEFAULT,
};

/*
 * 			    2^32
 * FSC(reg) =  FSC (HZ) * --------
 *			  27000000
 */
static const struct adv7391_std_info stdinfo[] = {
	{
		/* FSC(Hz) = 4,433,618.75 Hz */
		SD_STD_NTSC, 705268427, V4L2_STD_NTSC_443,
	}, {
		/* FSC(Hz) = 3,579,545.45 Hz */
		SD_STD_NTSC, 569408542, V4L2_STD_NTSC,
	}, {
		/* FSC(Hz) = 3,575,611.00 Hz */
		SD_STD_PAL_M, 568782678, V4L2_STD_PAL_M,
	}, {
		/* FSC(Hz) = 3,582,056.00 Hz */
		SD_STD_PAL_N, 569807903, V4L2_STD_PAL_Nc,
	}, {
		/* FSC(Hz) = 4,433,618.75 Hz */
		SD_STD_PAL_N, 705268427, V4L2_STD_PAL_N,
	}, {
		/* FSC(Hz) = 4,433,618.75 Hz */
		SD_STD_PAL_M, 705268427, V4L2_STD_PAL_60,
	}, {
		/* FSC(Hz) = 4,433,618.75 Hz */
		SD_STD_PAL_BDGHI, 705268427, V4L2_STD_PAL,
	},
};

static int adv7391_setstd(struct v4l2_subdev *sd, v4l2_std_id std)
{
	struct adv7391_state *state = to_state(sd);
	const struct adv7391_std_info *std_info;
	int num_std;
	u8 reg;
	u32 val;
	int err = 0;
	int i;

	num_std = ARRAY_SIZE(stdinfo);

	for (i = 0; i < num_std; i++) {
		if (stdinfo[i].stdid & std)
			break;
	}

	if (i == num_std) {
		v4l2_dbg(1, debug, sd,
				"Invalid std or std is not supported: %llx\n",
						(unsigned long long)std);
		return -EINVAL;
	}

	std_info = &stdinfo[i];

	/* Set the standard */
	val = state->reg80 & ~SD_STD_MASK;
	val |= std_info->standard_val3;
	pr_info("%s - ADV7391_SD_MODE_REG1: 0x%X\n", __func__, val);
	err = adv7391_write(sd, ADV7391_SD_MODE_REG1, val);
	if (err < 0)
		goto setstd_exit;

	state->reg80 = val;

	/* Configure the input mode register */
	val = state->reg01 & ~INPUT_MODE_MASK;
	val |= SD_INPUT_MODE;
	pr_info("%s - ADV7391_MODE_SELECT_REG: 0x%X\n", __func__, val);
	err = adv7391_write(sd, ADV7391_MODE_SELECT_REG, val);
	if (err < 0)
		goto setstd_exit;

	state->reg01 = val;

	/* Program the sub carrier frequency registers */
	val = std_info->fsc_val;
	for (reg = ADV7391_FSC_REG0; reg <= ADV7391_FSC_REG3; reg++) {
		pr_info("%s - ADV7391_FSC_REG0 ~ 3 0x[%X]: 0x%X\n", __func__, reg, val);
		err = adv7391_write(sd, reg, val);
		if (err < 0)
			goto setstd_exit;
		val >>= 8;
	}

	val = state->reg82;

	/* Pedestal settings */
	if (std & (V4L2_STD_NTSC | V4L2_STD_NTSC_443))
		val |= SD_PEDESTAL_EN;
	else
		val &= SD_PEDESTAL_DI;

	err = adv7391_write(sd, ADV7391_SD_MODE_REG2, val);
	if (err < 0)
		goto setstd_exit;

	pr_info("%s - ADV7391_SD_MODE_REG2: 0x%X\n", __func__, val);
	state->reg82 = val;

setstd_exit:
	if (err != 0)
		v4l2_err(sd, "Error setting std, write failed\n");

	return err;
}

static int adv731_test_pattern(struct v4l2_subdev *sd, v4l2_std_id std)
{
	struct adv7391_state *state = to_state(sd);
	int err = 0;

	err = adv7391_write(sd, 0x00, 0x1C);
	if (err < 0)
		v4l2_err(sd, "Error setting power mode, write failed\n");

	//err = adv7391_write(sd, 0x82, 0xC9); //Component
	err = adv7391_write(sd, 0x82, 0xCB); //composite 
	if (err < 0)
		v4l2_err(sd, "Error setting sd mode register 2, write failed\n");

	err = adv7391_write(sd, 0x84, 0x40);
	if (err < 0)
		v4l2_err(sd, "Error setting color bar, write failed\n");
#if 0
	err = adv7391_write(sd, 0x02, 0x24);
	if (err < 0)
		v4l2_err(sd, "Error setting selection color bar, write failed\n");
#endif

	return err;
}

static int adv7391_setoutput(struct v4l2_subdev *sd, u32 output_type)
{
	struct adv7391_state *state = to_state(sd);
	u8 val;
	int err = 0;

	if (output_type > ADV7391_SVIDEO_ID) {
		v4l2_dbg(1, debug, sd,
			"Invalid output type or output type not supported:%d\n",
								output_type);
		return -EINVAL;
	}

	/* Enable Appropriate DAC */
	val = state->reg00 & 0x03;

	if (output_type == ADV7391_COMPOSITE_ID)
		val |= ADV7391_COMPOSITE_POWER_VALUE;
	else if (output_type == ADV7391_COMPONENT_ID)
		val |= ADV7391_COMPONENT_POWER_VALUE;
	else
		val |= ADV7391_SVIDEO_POWER_VALUE;

	pr_info("%s - ADV7391_POWER_MODE_REG: 0x%X\n", __func__, val);
	err = adv7391_write(sd, ADV7391_POWER_MODE_REG, val);
	if (err < 0)
		goto setoutput_exit;

	state->reg00 = val;

	/* Enable YUV output */
	val = state->reg02 | YUV_OUTPUT_SELECT;
	pr_info("%s - ADV7391_MODE_REG0 : 0x%X\n", __func__, val);
	err = adv7391_write(sd, ADV7391_MODE_REG0, val);
	if (err < 0)
		goto setoutput_exit;

	state->reg02 = val;

	/* configure SD DAC Output 1 bit */
	val = state->reg82;
	if (output_type == ADV7391_COMPONENT_ID)
		val &= SD_DAC_OUT1_DI;
	else
		val |= SD_DAC_OUT1_EN;

	pr_info("%s - ADV7391_SD_MODE_REG2 : 0x%X\n", __func__, val);
	err = adv7391_write(sd, ADV7391_SD_MODE_REG2, val);
	if (err < 0)
		goto setoutput_exit;

	state->reg82 = val;

	/* configure ED/HD Color DAC Swap bit to zero */
	val = state->reg35 & HD_DAC_SWAP_DI;
	pr_info("%s - ADV7391_HD_MODE_REG6 : 0x%X\n", __func__, val);
	err = adv7391_write(sd, ADV7391_HD_MODE_REG6, val);
	if (err < 0)
		goto setoutput_exit;

	state->reg35 = val;

setoutput_exit:
	if (err != 0)
		v4l2_err(sd, "Error setting output, write failed\n");

	return err;
}

static int adv7391_log_status(struct v4l2_subdev *sd)
{
	struct adv7391_state *state = to_state(sd);

	v4l2_info(sd, "Standard: %llx\n", (unsigned long long)state->std);
	v4l2_info(sd, "Output: %s\n", (state->output == 0) ? "Composite" :
			((state->output == 1) ? "Component" : "S-Video"));
	return 0;
}

static int adv7391_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct v4l2_subdev *sd = to_sd(ctrl);

	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		return adv7391_write(sd, ADV7391_SD_BRIGHTNESS_WSS,
					ctrl->val & SD_BRIGHTNESS_VALUE_MASK);

	case V4L2_CID_HUE:
		return adv7391_write(sd, ADV7391_SD_HUE_ADJUST,
					ctrl->val - ADV7391_HUE_MIN);

	case V4L2_CID_GAIN:
		return adv7391_write(sd, ADV7391_DAC123_OUTPUT_LEVEL,
					ctrl->val);
	}
	return -EINVAL;
}

static const struct v4l2_ctrl_ops adv7391_ctrl_ops = {
	.s_ctrl = adv7391_s_ctrl,
};

static const struct v4l2_subdev_core_ops adv7391_core_ops = {
	.log_status = adv7391_log_status,
	.g_ext_ctrls = v4l2_subdev_g_ext_ctrls,
	.try_ext_ctrls = v4l2_subdev_try_ext_ctrls,
	.s_ext_ctrls = v4l2_subdev_s_ext_ctrls,
	.g_ctrl = v4l2_subdev_g_ctrl,
	.s_ctrl = v4l2_subdev_s_ctrl,
	.queryctrl = v4l2_subdev_queryctrl,
	.querymenu = v4l2_subdev_querymenu,
};

static int adv7391_s_std_output(struct v4l2_subdev *sd, v4l2_std_id std)
{
	struct adv7391_state *state = to_state(sd);
	int err = 0;

	if (state->std == std)
		return 0;

	err = adv7391_setstd(sd, std);
	if (!err)
		state->std = std;

	return err;
}

static int adv7391_s_routing(struct v4l2_subdev *sd,
		u32 input, u32 output, u32 config)
{
	struct adv7391_state *state = to_state(sd);
	int err = 0;

	if (state->output == output)
		return 0;

	err = adv7391_setoutput(sd, output);
	if (!err)
		state->output = output;

	return err;
}

static const struct v4l2_subdev_video_ops adv7391_video_ops = {
	.s_std_output	= adv7391_s_std_output,
	.s_routing	= adv7391_s_routing,
};

static const struct v4l2_subdev_ops adv7391_ops = {
	.core	= &adv7391_core_ops,
	.video	= &adv7391_video_ops,
};

static int adv7391_initialize(struct v4l2_subdev *sd)
{
	struct adv7391_state *state = to_state(sd);
	int err = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(adv7391_init_reg_val); i += 2) {

		err = adv7391_write(sd, adv7391_init_reg_val[i],
					adv7391_init_reg_val[i+1]);
		if (err) {
			v4l2_err(sd, "Error initializing\n");
			return err;
		}
	}

	/* Configure for default video standard */
	err = adv7391_setoutput(sd, state->output);
	if (err < 0) {
		v4l2_err(sd, "Error setting output during init\n");
		return -EINVAL;
	}

	err = adv7391_setstd(sd, state->std);
	if (err < 0) {
		v4l2_err(sd, "Error setting std during init\n");
		return -EINVAL;
	}
#if 0
	//Init test pattern
	err = adv731_test_pattern(sd, state->std);
	if (err < 0) {
		v4l2_err(sd, "Error setting test pattern\n");
		return -EINVAL;
	}
#endif

	return err;
}

static int adv7391_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct adv7391_state *state;
	int err;

	pr_info("+++ %s +++\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -ENODEV;

	v4l_info(client, "chip found @ 0x%x (%s)\n",
			client->addr << 1, client->adapter->name);

	state = devm_kzalloc(&client->dev, sizeof(*state), GFP_KERNEL);
	if (state == NULL)
		return -ENOMEM;

	state->reg00	= ADV7391_POWER_MODE_REG_DEFAULT;
	state->reg01	= 0x00;
	state->reg02	= 0x20;
	state->reg35	= ADV7391_HD_MODE_REG6_DEFAULT;
	state->reg80	= ADV7391_SD_MODE_REG1_DEFAULT;
	state->reg82	= ADV7391_SD_MODE_REG2_DEFAULT;

	state->std = V4L2_STD_NTSC;

	v4l2_i2c_subdev_init(&state->sd, client, &adv7391_ops);

	v4l2_ctrl_handler_init(&state->hdl, 3);
	v4l2_ctrl_new_std(&state->hdl, &adv7391_ctrl_ops,
			V4L2_CID_BRIGHTNESS, ADV7391_BRIGHTNESS_MIN,
					     ADV7391_BRIGHTNESS_MAX, 1,
					     ADV7391_BRIGHTNESS_DEF);
	v4l2_ctrl_new_std(&state->hdl, &adv7391_ctrl_ops,
			V4L2_CID_HUE, ADV7391_HUE_MIN,
				      ADV7391_HUE_MAX, 1,
				      ADV7391_HUE_DEF);
	v4l2_ctrl_new_std(&state->hdl, &adv7391_ctrl_ops,
			V4L2_CID_GAIN, ADV7391_GAIN_MIN,
				       ADV7391_GAIN_MAX, 1,
				       ADV7391_GAIN_DEF);
	state->sd.ctrl_handler = &state->hdl;
	if (state->hdl.error) {
		int err = state->hdl.error;

		v4l2_ctrl_handler_free(&state->hdl);
		return err;
	}
	v4l2_ctrl_handler_setup(&state->hdl);

#if 0 //reset
	unsigned int reset = 0;

	reset = (PAD_GPIO_B + 8);

	nxp_soc_gpio_set_out_value(reset, 1);
	nxp_soc_gpio_set_io_dir(reset, 1);
	nxp_soc_gpio_set_io_func(reset, nxp_soc_gpio_get_altnum(reset));
	mdelay(1);
	nxp_soc_gpio_set_out_value(reset, 0);
	mdelay(1);
	nxp_soc_gpio_set_out_value(reset, 1);
	mdelay(1000);
#endif

	err = adv7391_initialize(&state->sd);
	if (err)
		v4l2_ctrl_handler_free(&state->hdl);

	pr_info("--- %s ---\n", __func__);

	return err;
}

static int adv7391_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct adv7391_state *state = to_state(sd);

	v4l2_device_unregister_subdev(sd);
	v4l2_ctrl_handler_free(&state->hdl);

	return 0;
}

static const struct i2c_device_id adv7391_id[] = {
	{"adv7391", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, adv7391_id);

static struct i2c_driver adv7391_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "adv7391",
	},
	.probe		= adv7391_probe,
	.remove		= adv7391_remove,
	.id_table	= adv7391_id,
};
module_i2c_driver(adv7391_driver);
