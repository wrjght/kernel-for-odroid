/*
 * Driver for S5K6AA (SXGA camera) from Samsung Electronics
 * 
 * 1/6" 1.3Mp CMOS Image Sensor SoC with an Embedded Image Processor
 * supporting MIPI CSI-2
 *
 * Copyright (C) 2009, Dongsoo Nathaniel Kim<dongsoo45.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-i2c-drv.h>
#include <media/s5k6aa_platform.h>

#ifdef CONFIG_VIDEO_SAMSUNG_V4L2
#include <linux/videodev2_samsung.h>
#endif

#include "s5k6aa.h"

#define S5K6AA_DRIVER_NAME	"S5K6AA"

/* Default resolution & pixelformat. plz ref s5k6aa_platform.h */
#define DEFAULT_RES		WVGA	/* Index of resoultion */
#define DEFAUT_FPS_INDEX	S5K6AA_15FPS
#define DEFAULT_FMT		V4L2_PIX_FMT_UYVY	/* YUV422 */

/*
 * Specification
 * Parallel : ITU-R. 656/601 YUV422, RGB565, RGB888 (Up to VGA), RAW10 
 * Serial : MIPI CSI2 (single lane) YUV422, RGB565, RGB888 (Up to VGA), RAW10
 * Resolution : 1280 (H) x 1024 (V)
 * Image control : Brightness, Contrast, Saturation, Sharpness, Glamour
 * Effect : Mono, Negative, Sepia, Aqua, Sketch
 * FPS : 15fps @full resolution, 30fps @VGA, 24fps @720p
 * Max. pixel clock frequency : 48MHz(upto)
 * Internal PLL (6MHz to 27MHz input frequency)
 */

/* Camera functional setting values configured by user concept */
struct s5k6aa_userset {
	signed int exposure_bias;	/* V4L2_CID_EXPOSURE */
	unsigned int ae_lock;
	unsigned int awb_lock;
	unsigned int auto_wb;	/* V4L2_CID_AUTO_WHITE_BALANCE */
	unsigned int manual_wb;	/* V4L2_CID_WHITE_BALANCE_PRESET */
	unsigned int wb_temp;	/* V4L2_CID_WHITE_BALANCE_TEMPERATURE */
	unsigned int effect;	/* Color FX (AKA Color tone) */
	unsigned int contrast;	/* V4L2_CID_CONTRAST */
	unsigned int saturation;	/* V4L2_CID_SATURATION */
	unsigned int sharpness;		/* V4L2_CID_SHARPNESS */
	unsigned int glamour;
};

struct s5k6aa_state {
	struct s5k6aa_platform_data *pdata;
	struct v4l2_subdev sd;
	struct v4l2_pix_format pix;
	struct v4l2_fract timeperframe;
	struct s5k6aa_userset userset;
	int freq;	/* MCLK in KHz */
	int is_mipi;
	int isize;
	int ver;
	int fps;
};

static inline struct s5k6aa_state *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct s5k6aa_state, sd);
}

/*
 * S5K6AA register structure : 2bytes address, 2bytes value
 * retry on write failure up-to 5 times
 */
static inline int s5k6aa_write(struct v4l2_subdev *sd, u16 addr, u16 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct i2c_msg msg[1];
	unsigned char reg[4];
	int err = 0;
	int retry = 0;


	if (!client->adapter)
		return -ENODEV;

again:
	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 4;
	msg->buf = reg;

	reg[0] = addr >> 8;
	reg[1] = addr & 0xff;
	reg[2] = val >> 8;
	reg[3] = val & 0xff;

	err = i2c_transfer(client->adapter, msg, 1);
	if (err >= 0)
		return err;	/* Returns here on success */

	/* abnormal case: retry 5 times */
	if (retry < 5) {
		dev_err(&client->dev, "%s: address: 0x%02x%02x, " \
			"value: 0x%02x%02x\n", __func__, \
			reg[0], reg[1], reg[2], reg[3]);
		retry++;
		goto again;
	}

	return err;
}

static int s5k6aa_i2c_write(struct v4l2_subdev *sd, unsigned char i2c_data[],
				unsigned char length)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char buf[length], i;
	struct i2c_msg msg = {client->addr, 0, length, buf};

	for (i = 0; i < length; i++) {
		buf[i] = i2c_data[i];
	}
	return i2c_transfer(client->adapter, &msg, 1) == 1 ? 0 : -EIO;
}

static const char *s5k6aa_querymenu_wb_preset[] = {
	"WB Tungsten", "WB Fluorescent", "WB sunny", "WB cloudy", NULL
};

static const char *s5k6aa_querymenu_effect_mode[] = {
	"Effect Sepia", "Effect Aqua", "Effect Monochrome",
	"Effect Negative", "Effect Sketch", NULL
};

static const char *s5k6aa_querymenu_ev_bias_mode[] = {
	"-3EV",	"-2,1/2EV", "-2EV", "-1,1/2EV",
	"-1EV", "-1/2EV", "0", "1/2EV",
	"1EV", "1,1/2EV", "2EV", "2,1/2EV",
	"3EV", NULL
};

static struct v4l2_queryctrl s5k6aa_controls[] = {
	{
		/*
		 * For now, we just support in preset type
		 * to be close to generic WB system,
		 * we define color temp range for each preset
		 */
		.id = V4L2_CID_WHITE_BALANCE_TEMPERATURE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "White balance in kelvin",
		.minimum = 0,
		.maximum = 10000,
		.step = 1,
		.default_value = 0,	/* FIXME */
	},
	{
		.id = V4L2_CID_WHITE_BALANCE_PRESET,
		.type = V4L2_CTRL_TYPE_MENU,
		.name = "White balance preset",
		.minimum = 0,
		.maximum = ARRAY_SIZE(s5k6aa_querymenu_wb_preset) - 2,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_AUTO_WHITE_BALANCE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "Auto white balance",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_EXPOSURE,
		.type = V4L2_CTRL_TYPE_MENU,
		.name = "Exposure bias",
		.minimum = 0,
		.maximum = ARRAY_SIZE(s5k6aa_querymenu_ev_bias_mode) - 2,
		.step = 1,
		.default_value = (ARRAY_SIZE(s5k6aa_querymenu_ev_bias_mode) - 2) / 2,	/* 0 EV */
	},
	{
		.id = V4L2_CID_COLORFX,
		.type = V4L2_CTRL_TYPE_MENU,
		.name = "Image Effect",
		.minimum = 0,
		.maximum = ARRAY_SIZE(s5k6aa_querymenu_effect_mode) - 2,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_CONTRAST,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Contrast",
		.minimum = 0,
		.maximum = 4,
		.step = 1,
		.default_value = 2,
	},
	{
		.id = V4L2_CID_SATURATION,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Saturation",
		.minimum = 0,
		.maximum = 4,
		.step = 1,
		.default_value = 2,
	},
	{
		.id = V4L2_CID_SHARPNESS,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Sharpness",
		.minimum = 0,
		.maximum = 4,
		.step = 1,
		.default_value = 2,
	},
};

const char **s5k6aa_ctrl_get_menu(u32 id)
{
	switch (id) {
	case V4L2_CID_WHITE_BALANCE_PRESET:
		return s5k6aa_querymenu_wb_preset;

	case V4L2_CID_COLORFX:
		return s5k6aa_querymenu_effect_mode;

	case V4L2_CID_EXPOSURE:
		return s5k6aa_querymenu_ev_bias_mode;

	default:
		return v4l2_ctrl_get_menu(id);
	}
}

static inline struct v4l2_queryctrl const *s5k6aa_find_qctrl(int id)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(s5k6aa_controls); i++)
		if (s5k6aa_controls[i].id == id)
			return &s5k6aa_controls[i];

	return NULL;
}

static int s5k6aa_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(s5k6aa_controls); i++) {
		if (s5k6aa_controls[i].id == qc->id) {
			memcpy(qc, &s5k6aa_controls[i], \
				sizeof(struct v4l2_queryctrl));
			return 0;
		}
	}

	return -EINVAL;
}

static int s5k6aa_querymenu(struct v4l2_subdev *sd, struct v4l2_querymenu *qm)
{
	struct v4l2_queryctrl qctrl;

	qctrl.id = qm->id;
	s5k6aa_queryctrl(sd, &qctrl);

	return v4l2_ctrl_query_menu(qm, &qctrl, s5k6aa_ctrl_get_menu(qm->id));
}

/*
 * Clock configuration
 * Configure expected MCLK from host and return EINVAL if not supported clock
 * frequency is expected
 * 	freq : in Hz
 * 	flag : not supported for now
 */
static int s5k6aa_s_crystal_freq(struct v4l2_subdev *sd, u32  freq, u32 flags)
{
	int err = -EINVAL;

	return err;
}

static int s5k6aa_g_fmt(struct v4l2_subdev *sd, struct v4l2_format *fmt)
{
	int err = 0;

	return err;
}

static int s5k6aa_s_fmt(struct v4l2_subdev *sd, struct v4l2_format *fmt)
{
	int err = 0;

	return err;
}
static int s5k6aa_enum_framesizes(struct v4l2_subdev *sd, 
					struct v4l2_frmsizeenum *fsize)
{
	int err = 0;

	return err;
}

static int s5k6aa_enum_frameintervals(struct v4l2_subdev *sd, 
					struct v4l2_frmivalenum *fival)
{
	int err = 0;

	return err;
}

static int s5k6aa_enum_fmt(struct v4l2_subdev *sd, 
				struct v4l2_fmtdesc *fmtdesc)
{
	int err = 0;

	return err;
}

static int s5k6aa_try_fmt(struct v4l2_subdev *sd, struct v4l2_format *fmt)
{
	int err = 0;

	return err;
}

static int s5k6aa_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	int err = 0;

	return err;
}

static int s5k6aa_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	int err = 0;

	return err;
}

static int s5k6aa_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k6aa_state *state = to_state(sd);
	struct s5k6aa_userset userset = state->userset;
	int err = -EINVAL;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		ctrl->value = userset.exposure_bias;
		err = 0;
		break;
	case V4L2_CID_AUTO_WHITE_BALANCE:
		ctrl->value = userset.auto_wb;
		err = 0;
		break;
	case V4L2_CID_WHITE_BALANCE_PRESET:
		ctrl->value = userset.manual_wb;
		err = 0;
		break;
	case V4L2_CID_WHITE_BALANCE_TEMPERATURE:
		ctrl->value = userset.wb_temp;
		err = 0;
		break;
	case V4L2_CID_COLORFX:
		ctrl->value = userset.effect;
		err = 0;
		break;
	case V4L2_CID_CONTRAST:
		ctrl->value = userset.contrast;
		err = 0;
		break;
	case V4L2_CID_SATURATION:
		ctrl->value = userset.saturation;
		err = 0;
		break;
	case V4L2_CID_SHARPNESS:
		ctrl->value = userset.saturation;
		err = 0;
		break;
	default:
		dev_err(&client->dev, "%s: no such ctrl\n", __func__);
		break;
	}
	
	return err;
}

static int s5k6aa_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
#ifdef S5K6AA_COMPLETE
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k6aa_state *state = to_state(sd);
	struct s5k6aa_userset userset = state->userset;
	int err = -EINVAL;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		dev_dbg(&client->dev, "%s: V4L2_CID_EXPOSURE\n", \
			__func__);
		err = s5k6aa_write_regs(sd, s5k6aa_regs_ev_bias[ctrl->value]);
		break;
	case V4L2_CID_AUTO_WHITE_BALANCE:
		dev_dbg(&client->dev, "%s: V4L2_CID_AUTO_WHITE_BALANCE\n", \
			__func__);
		err = s5k6aa_write_regs(sd, s5k6aa_regs_awb_enable[ctrl->value]);
		break;
	case V4L2_CID_WHITE_BALANCE_PRESET:
		dev_dbg(&client->dev, "%s: V4L2_CID_WHITE_BALANCE_PRESET\n", \
			__func__);
		err = s5k6aa_write_regs(sd, s5k6aa_regs_wb_preset[ctrl->value]);
		break;
	case V4L2_CID_WHITE_BALANCE_TEMPERATURE:
		dev_dbg(&client->dev, "%s: V4L2_CID_WHITE_BALANCE_TEMPERATURE\n", \
			__func__);
		err = s5k6aa_write_regs(sd, s5k6aa_regs_wb_temperature[ctrl->value]);
		break;
	case V4L2_CID_COLORFX:
		dev_dbg(&client->dev, "%s: V4L2_CID_COLORFX\n", __func__);
		err = s5k6aa_write_regs(sd, s5k6aa_regs_color_effect[ctrl->value]);
		break;
	case V4L2_CID_CONTRAST:
		dev_dbg(&client->dev, "%s: V4L2_CID_CONTRAST\n", __func__);
		err = s5k6aa_write_regs(sd, s5k6aa_regs_contrast_bias[ctrl->value]);
		break;
	case V4L2_CID_SATURATION:
		dev_dbg(&client->dev, "%s: V4L2_CID_SATURATION\n", __func__);
		err = s5k6aa_write_regs(sd, s5k6aa_regs_saturation_bias[ctrl->value]);
		break;
	case V4L2_CID_SHARPNESS:
		dev_dbg(&client->dev, "%s: V4L2_CID_SHARPNESS\n", __func__);
		err = s5k6aa_write_regs(sd, s5k6aa_regs_sharpness_bias[ctrl->value]);
		break;
	default:
		dev_err(&client->dev, "%s: no such control\n", __func__);
		break;
	}

	if (err < 0)
		goto out;
	else
		return 0;

out:
	dev_dbg(&client->dev, "%s: vidioc_s_ctrl failed\n", __func__);
	return err;
#else
	return 0;
#endif
}

static int s5k6aa_init(struct v4l2_subdev *sd, u32 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = -EINVAL, i;

	v4l_info(client, "%s: camera initialization start\n", __func__);

	for (i = 0; i < S5K6AA_INIT_REGS; i++) {
		if (s5k6aa_init_reg[i][0] == REG_DELAY) {
			mdelay(s5k6aa_init_reg[i][1]);
			err = 0;
		} else {
			err = s5k6aa_i2c_write(sd, s5k6aa_init_reg[i], \
						sizeof(s5k6aa_init_reg[i]));
		}

		if (err < 0)
			v4l_info(client, "%s: register set failed\n", \
			__func__);
	}

	if (err < 0) {
		v4l_err(client, "%s: camera initialization failed\n", \
			__func__);
		return -EIO;	/* FIXME */
	}

	return 0;
}

/*
 * s_config subdev ops
 * With camera device, we need to re-initialize every single opening time therefor,
 * it is not necessary to be initialized on probe time. except for version checking
 * NOTE: version checking is optional
 */
static int s5k6aa_s_config(struct v4l2_subdev *sd, int irq, void *platform_data)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k6aa_state *state = to_state(sd);
	struct s5k6aa_platform_data *pdata;

	dev_info(&client->dev, "fetching platform data\n");

	pdata = client->dev.platform_data;

	if (!pdata) {
		dev_err(&client->dev, "%s: no platform data\n", __func__);
		return -ENODEV;
	}

	/*
	 * Assign default format and resolution
	 * Use configured default information in platform data
	 * or without them, use default information in driver
	 */
	if (!(pdata->default_width && pdata->default_height)) {
		/* TODO: assign driver default resolution */
	} else {
		state->pix.width = pdata->default_width;
		state->pix.height = pdata->default_height;
	}

	if (!pdata->pixelformat)
		state->pix.pixelformat = DEFAULT_FMT;
	else
		state->pix.pixelformat = pdata->pixelformat;

	if (!pdata->freq)
		state->freq = 24000000;	/* 24MHz default */
	else
		state->freq = pdata->freq;

	if (!pdata->is_mipi) {
		state->is_mipi = 0;
		dev_info(&client->dev, "parallel mode\n");
	} else
		state->is_mipi = pdata->is_mipi;

	return 0;
}

static int s5k6aa_sleep(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = -EINVAL, i;

	v4l_info(client, "%s: sleep mode\n", __func__);

	for (i = 0; i < S5K6AA_SLEEP_REGS; i++) {
		if (s5k6aa_sleep_reg[i][0] == REG_DELAY) {
			mdelay(s5k6aa_sleep_reg[i][1]);
			err = 0;
		} else {
			err = s5k6aa_write(sd, s5k6aa_sleep_reg[i][0], \
				s5k6aa_sleep_reg[i][1]);
		}

		if (err < 0)
			v4l_info(client, "%s: register set failed\n", __func__);
	}

	if (err < 0) {
		v4l_err(client, "%s: sleep failed\n", __func__);
		return -EIO;
	}

	return 0;
}

static int s5k6aa_wakeup(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = -EINVAL, i;

	v4l_info(client, "%s: wakeup mode\n", __func__);

	for (i = 0; i < S5K6AA_WAKEUP_REGS; i++) {
		if (s5k6aa_wakeup_reg[i][0] == REG_DELAY) {
			mdelay(s5k6aa_wakeup_reg[i][1]);
			err = 0;
		} else {
			err = s5k6aa_write(sd, s5k6aa_wakeup_reg[i][0], \
				s5k6aa_wakeup_reg[i][1]);
		}

		if (err < 0)
			v4l_info(client, "%s: register set failed\n", __func__);
	}

	if (err < 0) {
		v4l_err(client, "%s: wake up failed\n", __func__);
		return -EIO;
	}

	return 0;
}

static int s5k6aa_s_stream(struct v4l2_subdev *sd, int enable)
{
	return (enable ? s5k6aa_wakeup(sd) : s5k6aa_sleep(sd));
}

static const struct v4l2_subdev_core_ops s5k6aa_core_ops = {
	.init = s5k6aa_init,	/* initializing API */
	.s_config = s5k6aa_s_config,	/* Fetch platform data */
	.queryctrl = s5k6aa_queryctrl,
	.querymenu = s5k6aa_querymenu,
	.g_ctrl = s5k6aa_g_ctrl,
	.s_ctrl = s5k6aa_s_ctrl,
};

static const struct v4l2_subdev_video_ops s5k6aa_video_ops = {
	.s_crystal_freq = s5k6aa_s_crystal_freq,
	.g_fmt = s5k6aa_g_fmt,
	.s_fmt = s5k6aa_s_fmt,
	.enum_framesizes = s5k6aa_enum_framesizes,
	.enum_frameintervals = s5k6aa_enum_frameintervals,
	.enum_fmt = s5k6aa_enum_fmt,
	.try_fmt = s5k6aa_try_fmt,
	.g_parm = s5k6aa_g_parm,
	.s_parm = s5k6aa_s_parm,
	.s_stream = s5k6aa_s_stream,
};

static const struct v4l2_subdev_ops s5k6aa_ops = {
	.core = &s5k6aa_core_ops,
	.video = &s5k6aa_video_ops,
};

/*
 * s5k6aa_probe
 * Fetching platform data is being done with s_config subdev call.
 * In probe routine, we just register subdev device
 */
static int s5k6aa_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct s5k6aa_state *state;
	struct v4l2_subdev *sd;

	state = kzalloc(sizeof(struct s5k6aa_state), GFP_KERNEL);
	if (state == NULL)
		return -ENOMEM;

	sd = &state->sd;
	strcpy(sd->name, S5K6AA_DRIVER_NAME);

	/* Registering subdev */
	v4l2_i2c_subdev_init(sd, client, &s5k6aa_ops);

	dev_info(&client->dev, "s5k6aa has been probed\n");
	return 0;
}


static int s5k6aa_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);

	v4l2_device_unregister_subdev(sd);
	kfree(to_state(sd));
	return 0;
}

static const struct i2c_device_id s5k6aa_id[] = {
	{ S5K6AA_DRIVER_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, s5k6aa_id);

static struct v4l2_i2c_driver_data v4l2_i2c_data = {
	.name = S5K6AA_DRIVER_NAME,
	.probe = s5k6aa_probe,
	.remove = s5k6aa_remove,
	.id_table = s5k6aa_id,
};

MODULE_DESCRIPTION("Samsung Electronics S5K6AA SXGA camera driver");
MODULE_AUTHOR("Dongsoo Nathaniel Kim<dongsoo45.kim@samsung.com>");
MODULE_LICENSE("GPL");

