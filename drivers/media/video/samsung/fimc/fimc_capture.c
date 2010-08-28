/* linux/drivers/media/video/samsung/fimc_capture.c
 *
 * V4L2 Capture device support file for Samsung Camera Interface (FIMC) driver
 *
 * Dongsoo Kim, Copyright (c) 2009 Samsung Electronics
 * 	http://www.samsung.com/sec/
 * Jinsung Yang, Copyright (c) 2009 Samsung Electronics
 * 	http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/slab.h>
#include <linux/bootmem.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/videodev2.h>
#include <linux/videodev2_samsung.h>
#include <linux/clk.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <plat/media.h>
#include <plat/clock.h>
#include <plat/fimc.h>

#include "fimc.h"

/* subdev handling macro */
#define subdev_call(ctrl, o, f, args...) \
	v4l2_subdev_call(ctrl->cam->sd, o, f, ##args)

const static struct v4l2_fmtdesc capture_fmts[] = {
	{
		.index		= 0,
		.type		= V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.flags		= FORMAT_FLAGS_PACKED,
		.description	= "RGB-5-6-5",
		.pixelformat	= V4L2_PIX_FMT_RGB565,
	}, {
		.index		= 1,
		.type		= V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.flags		= FORMAT_FLAGS_PACKED,
		.description	= "RGB-8-8-8, unpacked 24 bpp",
		.pixelformat	= V4L2_PIX_FMT_RGB32,
	}, {
		.index		= 2,
		.type		= V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.flags		= FORMAT_FLAGS_PACKED,
		.description	= "YUV 4:2:2 packed, YCbYCr",
		.pixelformat	= V4L2_PIX_FMT_YUYV,
	}, {
		.index		= 3,
		.type		= V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.flags		= FORMAT_FLAGS_PACKED,
		.description	= "YUV 4:2:2 packed, CbYCrY",
		.pixelformat	= V4L2_PIX_FMT_UYVY,
	}, {
		.index		= 4,
		.type		= V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.flags		= FORMAT_FLAGS_PACKED,
		.description	= "YUV 4:2:2 packed, CrYCbY",
		.pixelformat	= V4L2_PIX_FMT_VYUY,
	}, {
		.index		= 5,
		.type		= V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.flags		= FORMAT_FLAGS_PACKED,
		.description	= "YUV 4:2:2 packed, YCrYCb",
		.pixelformat	= V4L2_PIX_FMT_YVYU,
	}, {
		.index		= 6,
		.type		= V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.flags		= FORMAT_FLAGS_PLANAR,
		.description	= "YUV 4:2:2 planar, Y/Cb/Cr",
		.pixelformat	= V4L2_PIX_FMT_YUV422P,
	}, {
		.index		= 7,
		.type		= V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.flags		= FORMAT_FLAGS_PLANAR,
		.description	= "YUV 4:2:0 planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV12,
	}, {
		.index		= 8,
		.type		= V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.flags		= FORMAT_FLAGS_PLANAR,
		.description	= "YUV 4:2:0 planar, Y/CbCr, Tiled",
		.pixelformat	= V4L2_PIX_FMT_NV12T,
	}, {
		.index		= 9,
		.type		= V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.flags		= FORMAT_FLAGS_PLANAR,
		.description	= "YUV 4:2:0 planar, Y/CrCb",
		.pixelformat	= V4L2_PIX_FMT_NV21,
	}, {
		.index		= 10,
		.type		= V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.flags		= FORMAT_FLAGS_PLANAR,
		.description	= "YUV 4:2:2 planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV16,
	}, {
		.index		= 11,
		.type		= V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.flags		= FORMAT_FLAGS_PLANAR,
		.description	= "YUV 4:2:2 planar, Y/CrCb",
		.pixelformat	= V4L2_PIX_FMT_NV61,
	}, {
		.index		= 12,
		.type		= V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.flags		= FORMAT_FLAGS_PLANAR,
		.description	= "YUV 4:2:0 planar, Y/Cb/Cr",
		.pixelformat	= V4L2_PIX_FMT_YUV420,
	},
};

#ifndef CONFIG_VIDEO_FIMC_MIPI
void s3c_csis_start(int lanes, int settle, int align, int width, int height) {}
#endif

static int fimc_init_camera(struct fimc_control *ctrl)
{
	struct fimc_global *fimc = get_fimc_dev();
	struct s3c_platform_fimc *pdata;
	struct s3c_platform_camera *cam;
	int ret;

	pdata = to_fimc_plat(ctrl->dev);
	if (pdata->default_cam >= FIMC_MAXCAMS) {
		fimc_err("%s: invalid camera index\n", __func__);
		return -EINVAL;
	}

	if (!fimc->camera[pdata->default_cam]) {
		fimc_err("no external camera device\n");
		return -ENODEV;
	}

	/*
	 * ctrl->cam may not be null if already s_input called,
	 * otherwise, that should be default_cam if ctrl->cam is null.
	*/
	if (!ctrl->cam)
		ctrl->cam = fimc->camera[pdata->default_cam];

	cam = ctrl->cam;

	/* do nothing if already initialized */
	if (cam->initialized)
		return 0;

	/* 
	 * WriteBack mode doesn't need to set clock and power,
	 * but it needs to set source width, height depend on LCD resolution.
	*/
	if (cam->id == CAMERA_WB) {
		s3cfb_direct_ioctl(0, S3CFB_GET_LCD_WIDTH, (unsigned long)&cam->width);	
		s3cfb_direct_ioctl(0, S3CFB_GET_LCD_HEIGHT, (unsigned long)&cam->height);
		cam->window.width = cam->width;
		cam->window.height = cam->height;
		cam->initialized = 1;
		return 0;
	}
	
	/* set rate for mclk */
	if (cam->clk->set_rate) {
		cam->clk->set_rate(cam->clk, cam->clk_rate);
		clk_enable(cam->clk);
		fimc_info1("clock for camera: %d\n", cam->clk_rate);
	}

	/* enable camera power if needed */
	if (cam->cam_power)
		cam->cam_power(1);

	/* subdev call for init */
	ret = v4l2_subdev_call(cam->sd, core, init, 0);
	if (ret == -ENOIOCTLCMD) {
		fimc_err("%s: init subdev api not supported\n", __func__);
		return ret;
	}

	if (cam->type == CAM_TYPE_MIPI) {
		/* 
		 * subdev call for sleep/wakeup:
		 * no error although no s_stream api support
		*/
		v4l2_subdev_call(cam->sd, video, s_stream, 0);
		s3c_csis_start(cam->mipi_lanes, cam->mipi_settle, \
				cam->mipi_align, cam->width, cam->height);
		v4l2_subdev_call(cam->sd, video, s_stream, 1);
	}

	cam->initialized = 1;

	return 0;
}

static int fimc_capture_scaler_info(struct fimc_control *ctrl)
{
	struct fimc_scaler *sc = &ctrl->sc;
	struct v4l2_rect *window = &ctrl->cam->window;
	int tx, ty, sx, sy;

	sx = window->width;
	sy = window->height;
	tx = ctrl->cap->fmt.width;
	ty = ctrl->cap->fmt.height;

	sc->real_width = sx;
	sc->real_height = sy;

	if (sx <= 0 || sy <= 0) {
		fimc_err("%s: invalid source size\n", __func__);
		return -EINVAL;
	}

	if (tx <= 0 || ty <= 0) {
		fimc_err("%s: invalid target size\n", __func__);
		return -EINVAL;
	}

	fimc_get_scaler_factor(sx, tx, &sc->pre_hratio, &sc->hfactor);
	fimc_get_scaler_factor(sy, ty, &sc->pre_vratio, &sc->vfactor);

	sc->pre_dst_width = sx / sc->pre_hratio;
	sc->pre_dst_height = sy / sc->pre_vratio;

	sc->main_hratio = (sx << 8) / (tx << sc->hfactor);
	sc->main_vratio = (sy << 8) / (ty << sc->vfactor);

	sc->scaleup_h = (tx >= sx) ? 1 : 0;
	sc->scaleup_v = (ty >= sy) ? 1 : 0;

	return 0;
}

static int fimc_add_inqueue(struct fimc_control *ctrl, int i)
{
	struct fimc_capinfo *cap = ctrl->cap;

	list_add_tail(&cap->bufs[i].list, &cap->inq);

	return 0;
}

static int fimc_add_outqueue(struct fimc_control *ctrl, int i)
{
	struct fimc_capinfo *cap = ctrl->cap;
	struct fimc_buf_set *buf;

	if (cap->nr_bufs > FIMC_PHYBUFS) {
		if (list_empty(&cap->inq))
			return -ENOENT;

		buf = list_first_entry(&cap->inq, struct fimc_buf_set, list);
		cap->outq[i] = buf->id;
		list_del(&buf->list);
	} else {
		buf = &cap->bufs[i];
	}

	fimc_hwset_output_address(ctrl, buf, i);

	return 0;
}

int fimc_g_parm(struct file *file, void *fh, struct v4l2_streamparm *a)
{
	struct fimc_control *ctrl = fh;
	int ret;

	fimc_dbg("%s\n", __func__);

	/* WriteBack doesn't have subdev_call */
	if (ctrl->cam->id == CAMERA_WB) {
		return 0;
	}
	
	mutex_lock(&ctrl->v4l2_lock);
	ret = subdev_call(ctrl, video, g_parm, a);
	mutex_unlock(&ctrl->v4l2_lock);

	return ret;
}

int fimc_s_parm(struct file *file, void *fh, struct v4l2_streamparm *a)
{
	struct fimc_control *ctrl = fh;
	int ret;

	fimc_dbg("%s\n", __func__);

	/* WriteBack doesn't have subdev_call */
	if (ctrl->cam->id == CAMERA_WB) {
		return 0;
	}
	
	mutex_lock(&ctrl->v4l2_lock);
	ret = subdev_call(ctrl, video, s_parm, a);
	mutex_unlock(&ctrl->v4l2_lock);

	return ret;
}

int fimc_enum_input(struct file *file, void *fh, struct v4l2_input *inp)
{
	struct fimc_global *fimc = get_fimc_dev();
	struct fimc_control *ctrl = fh;
	struct s3c_platform_camera *cam = NULL;
	int i, cam_count = 0;

	if (inp->index >= FIMC_MAXCAMS) {
		fimc_err("%s: invalid input index\n", __func__);
		return -EINVAL;
	}

	fimc_dbg("%s: index %d\n", __func__, inp->index);

	mutex_lock(&ctrl->v4l2_lock);

	/*
	 * External camera input devices are managed in fimc->camera[]
	 * but it aligned in the order of H/W camera interface's (A/B/C)
	 * Therefore it could be NULL if there is no actual camera to take
	 * place the index
	 * ie. if index is 1, that means that one camera has been selected
	 * before so choose second object it reaches
	 */
	for (i = 0; i < FIMC_MAXCAMS; i++) {
		/* increase index until get not NULL and upto FIMC_MAXCAMS */
		if (!fimc->camera[i])
			continue;

		if (fimc->camera[i]) {
			++cam_count;
			if (cam_count == inp->index + 1) {
				cam = fimc->camera[i];
				fimc_info1("%s:v4l2 input[%d] is %s",
						__func__, inp->index,
						fimc->camera[i]->info->type);
			} else
				continue;
		}
	}

	if (cam) {
		strcpy(inp->name, cam->info->type);
		inp->type = V4L2_INPUT_TYPE_CAMERA;
	} else {
		fimc_err("%s: no more camera input\n", __func__);
		mutex_unlock(&ctrl->v4l2_lock);
		return -EINVAL;
	}

	mutex_unlock(&ctrl->v4l2_lock);

	return 0;
}

int fimc_g_input(struct file *file, void *fh, unsigned int *i)
{
	struct fimc_control *ctrl = fh;

	/* In case of isueing g_input before s_input */
	if (!ctrl->cam) {
		dev_err(ctrl->dev,
				"no camera device selected yet!"
				"do VIDIOC_S_INPUT first\n");
		return -ENODEV;
	}

	*i = (unsigned int) ctrl->cam->id;

	fimc_dbg("%s: index %d\n", __func__, *i);

	return 0;
}

int fimc_s_input(struct file *file, void *fh, unsigned int i)
{
	struct fimc_global *fimc = get_fimc_dev();
	struct fimc_control *ctrl = fh;
	int index, dev_index = -1;

	if (i >= FIMC_MAXCAMS) {
		fimc_err("%s: invalid input index\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&ctrl->v4l2_lock);

	fimc_dbg("%s: index %d\n", __func__, i);

	/*
	 * Align mounted camera in v4l2 input order
	 * (handling NULL devices)
	 * dev_index represents the actual index number
	 */
	for (index = 0; index < FIMC_MAXCAMS; index++) {
		/* If there is no exact camera H/W for exact index */
		if (!fimc->camera[index])
			continue;

		/* Found actual device */
		if (fimc->camera[index]) {
			/* Count actual device number */
			++dev_index;
			/* If the index number matches the expecting input i */
			if (dev_index == i)
				ctrl->cam = fimc->camera[index];
			else
				continue;
		}
	}

	mutex_unlock(&ctrl->v4l2_lock);
	fimc_init_camera(ctrl);

	return 0;
}

int fimc_enum_fmt_vid_capture(struct file *file, void *fh,
					struct v4l2_fmtdesc *f)
{
	struct fimc_control *ctrl = fh;
	int i = f->index;

	fimc_dbg("%s\n", __func__);

	mutex_lock(&ctrl->v4l2_lock);

	memset(f, 0, sizeof(*f));
	memcpy(f, &capture_fmts[i], sizeof(*f));

	mutex_unlock(&ctrl->v4l2_lock);

	return 0;
}

int fimc_g_fmt_vid_capture(struct file *file, void *fh, struct v4l2_format *f)
{
	struct fimc_control *ctrl = fh;

	fimc_dbg("%s\n", __func__);

	if (!ctrl->cap) {
		fimc_err("%s: no capture device info\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&ctrl->v4l2_lock);

	memset(&f->fmt.pix, 0, sizeof(f->fmt.pix));
	memcpy(&f->fmt.pix, &ctrl->cap->fmt, sizeof(f->fmt.pix));

	mutex_unlock(&ctrl->v4l2_lock);

	return 0;
}

/*
 * Check for whether the requested format
 * can be streamed out from FIMC
 * depends on FIMC node
 */
static int fimc_fmt_avail(struct fimc_control *ctrl,
		struct v4l2_format *f)
{
	int i;

	/*
	 * TODO: check for which FIMC is used.
	 * Available fmt should be varied for each FIMC
	 */

	for (i = 0; i < ARRAY_SIZE(capture_fmts); i++) {
		if (capture_fmts[i].pixelformat == f->fmt.pix.pixelformat)
			return 0;
	}

	fimc_info1("Not supported pixelformat requested\n");

	return -1;
}

/*
 * figures out the depth of requested format
 */
static int fimc_fmt_depth(struct fimc_control *ctrl, struct v4l2_format *f)
{
	int err, depth = 0;

	/* First check for available format or not */
	err = fimc_fmt_avail(ctrl, f);
	if (err < 0)
		return -1;

	/* handles only supported pixelformats */
	switch (f->fmt.pix.pixelformat) {
	case V4L2_PIX_FMT_RGB32:
		depth = 32;
		fimc_dbg("32bpp\n");
		break;
	case V4L2_PIX_FMT_RGB565:
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_YUV422P:
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
		depth = 16;
		fimc_dbg("16bpp\n");
		break;
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV12T:
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_YUV420:
		depth = 12;
		fimc_dbg("12bpp\n");
		break;
	default:
		fimc_dbg("why am I here?\n");
		break;
	}

	return depth;
}

int fimc_s_fmt_vid_capture(struct file *file, void *fh, struct v4l2_format *f)
{
	struct fimc_control *ctrl = fh;
	struct fimc_capinfo *cap = ctrl->cap;
	int ret = 0;

	fimc_dbg("%s\n", __func__);

	/*
	 * The first time alloc for struct cap_info, and will be
	 * released at the file close.
	 * Anyone has better idea to do this?
	*/
	if (!cap) {
		cap = kzalloc(sizeof(*cap), GFP_KERNEL);
		if (!cap) {
			fimc_err("%s: no memory for "
				"capture device info\n", __func__);
			return -ENOMEM;
		}

		/* assign to ctrl */
		ctrl->cap = cap;
	} else {
		memset(cap, 0, sizeof(*cap));
	}

	mutex_lock(&ctrl->v4l2_lock);

	memset(&cap->fmt, 0, sizeof(cap->fmt));
	memcpy(&cap->fmt, &f->fmt.pix, sizeof(cap->fmt));

	/*
	 * Note that expecting format only can be with
	 * available output format from FIMC
	 * Following items should be handled in driver
	 * bytesperline = width * depth / 8
	 * sizeimage = bytesperline * height
	 */
	cap->fmt.bytesperline = (cap->fmt.width * fimc_fmt_depth(ctrl, f)) >> 3;
	cap->fmt.sizeimage = (cap->fmt.bytesperline * cap->fmt.height);

	if (cap->fmt.colorspace == V4L2_COLORSPACE_JPEG) {
		ctrl->sc.bypass = 1;
		cap->lastirq = 1;
	}
	else
	{
		ctrl->sc.bypass = 0;
		cap->lastirq = 0;

	}

	/* WriteBack doesn't have subdev_call */
	if (ctrl->cam->id == CAMERA_WB) {
		mutex_unlock(&ctrl->v4l2_lock);
		return 0;
	}
	
	ret = subdev_call(ctrl, video, s_fmt, f);

	mutex_unlock(&ctrl->v4l2_lock);

	return ret;
}

int fimc_try_fmt_vid_capture(struct file *file, void *fh, struct v4l2_format *f)
{
	return 0;
}

static int fimc_alloc_buffers(struct fimc_control *ctrl, 
				int plane, int size, int align)
{
	struct fimc_capinfo *cap = ctrl->cap;
	int i;

	for (i = 0; i < cap->nr_bufs; i++) {

		cap->bufs[i].length[plane] = PAGE_ALIGN(size);
		fimc_dma_alloc(ctrl, &cap->bufs[i], plane, align);

		if (!cap->bufs[i].base[plane])
			goto err_alloc;

		cap->bufs[i].state = VIDEOBUF_PREPARED;
	}


	return 0;

err_alloc:
	for (i = 0; i < cap->nr_bufs; i++) {
		if (cap->bufs[i].base[plane])
			fimc_dma_free(ctrl, &cap->bufs[i], plane);

		memset(&cap->bufs[i], 0, sizeof(cap->bufs[i]));
	}

	return -ENOMEM;
}

int fimc_reqbufs_capture(void *fh, struct v4l2_requestbuffers *b)
{
	struct fimc_control *ctrl = fh;
	struct fimc_capinfo *cap = ctrl->cap;
	int ret = 0, i;

	if (!cap) {
		fimc_err("%s: no capture device info\n", __func__);
		return -ENODEV;
	}

	mutex_lock(&ctrl->v4l2_lock);

	if (b->count < 1 || b->count == 3) {
		fimc_err("%s: invalid buffer count\n", __func__);
		return -EINVAL;
	}

	cap->nr_bufs = b->count;
	fimc_dbg("%s: requested %d buffers\n", __func__, b->count);

	INIT_LIST_HEAD(&cap->inq);
	for (i = 0; i < FIMC_CAPBUFS; i++) {
		/* free previous buffers */
		fimc_dma_free(ctrl, &cap->bufs[i], 0);
		cap->bufs[i].id = i;
		cap->bufs[i].state = VIDEOBUF_NEEDS_INIT;

		/* initialize list */
		INIT_LIST_HEAD(&cap->bufs[i].list);
	}

	switch (cap->fmt.pixelformat) {
	case V4L2_PIX_FMT_RGB32:	/* fall through */
	case V4L2_PIX_FMT_RGB565:	/* fall through */
	case V4L2_PIX_FMT_YUYV:		/* fall through */
	case V4L2_PIX_FMT_UYVY:		/* fall through */
	case V4L2_PIX_FMT_VYUY:		/* fall through */
	case V4L2_PIX_FMT_YVYU:		/* fall through */
	case V4L2_PIX_FMT_NV16:		/* fall through */
	case V4L2_PIX_FMT_NV61:		/* fall through */
	case V4L2_PIX_FMT_YUV422P:	/* fall through */
	case V4L2_PIX_FMT_YUV420:	/* fall through */
	case V4L2_PIX_FMT_NV21:
		ret = fimc_alloc_buffers(ctrl, 0, cap->fmt.sizeimage, 0);
		ctrl->mem.curr = ctrl->mem.base;
		break;

	case V4L2_PIX_FMT_NV12:		/* fall through */
	case V4L2_PIX_FMT_NV12T:
		ret = fimc_alloc_buffers(ctrl, 0,
			cap->fmt.width * cap->fmt.height, SZ_64K);
		ret = fimc_alloc_buffers(ctrl, 1,
			cap->fmt.width * cap->fmt.height / 2, SZ_64K);
		ctrl->mem.curr = ctrl->mem.base;
		break;
	default:
		break;
	}

	if (ret) {
		fimc_err("%s: no memory for capture buffer\n", __func__);
		mutex_unlock(&ctrl->v4l2_lock);
		return -ENOMEM;
	}

	for (i = cap->nr_bufs; i < FIMC_PHYBUFS; i++) {
		memcpy(&cap->bufs[i],
			&cap->bufs[i - cap->nr_bufs], sizeof(cap->bufs[i]));
	}

	mutex_unlock(&ctrl->v4l2_lock);

	return 0;
}

int fimc_querybuf_capture(void *fh, struct v4l2_buffer *b)
{
	struct fimc_control *ctrl = fh;

	if (ctrl->status != FIMC_STREAMOFF) {
		fimc_err("fimc is running\n");
		return -EBUSY;
	}

	mutex_lock(&ctrl->v4l2_lock);

	b->length = ctrl->cap->bufs[b->index].length[0];
	b->m.offset = b->index * PAGE_SIZE;

	ctrl->cap->bufs[b->index].state = VIDEOBUF_IDLE;

	fimc_dbg("%s: %d bytes with offset: %d\n",
		__func__, b->length, b->m.offset);

	mutex_unlock(&ctrl->v4l2_lock);

	return 0;
}

int fimc_g_ctrl_capture(void *fh, struct v4l2_control *c)
{
	struct fimc_control *ctrl = fh;
	int ret = 0;

	fimc_dbg("%s\n", __func__);

	mutex_lock(&ctrl->v4l2_lock);

	switch (c->id) {
	case V4L2_CID_ROTATION:
		ctrl->cap->rotate = c->value;
		break;

	case V4L2_CID_HFLIP:	/* fall through */
	case V4L2_CID_VFLIP:
		ctrl->cap->flip = c->id;
		break;

	default:
		/* get ctrl supported by subdev */
		/* WriteBack doesn't have subdev_call */
		if (ctrl->cam->id == CAMERA_WB)
			break;
		ret = subdev_call(ctrl, core, g_ctrl, c);
		break;
	}

	mutex_unlock(&ctrl->v4l2_lock);

	return ret;
}

int fimc_s_ctrl_capture(void *fh, struct v4l2_control *c)
{
	struct fimc_control *ctrl = fh;
	int ret = 0;

	fimc_dbg("%s\n", __func__);

	mutex_lock(&ctrl->v4l2_lock);

	switch (c->id) {
	case V4L2_CID_ROTATION:
		ctrl->cap->rotate = c->value;
		break;

	case V4L2_CID_HFLIP:	/* fall through */
	case V4L2_CID_VFLIP:
		ctrl->cap->flip = c->id;
		break;

	case V4L2_CID_PADDR_Y:
		c->value = ctrl->cap->bufs[c->value].base[FIMC_ADDR_Y];
		break;

	case V4L2_CID_PADDR_CB:		/* fall through */
	case V4L2_CID_PADDR_CBCR:
		c->value = ctrl->cap->bufs[c->value].base[FIMC_ADDR_CB];
		break;

	case V4L2_CID_PADDR_CR:
		c->value = ctrl->cap->bufs[c->value].base[FIMC_ADDR_CR];
		break;
        case V4L2_CID_STREAM_PAUSE:  
                fimc_hwset_stop_processing(ctrl);
                break;
	default:
		/* try on subdev */
		/* WriteBack doesn't have subdev_call */
		if (ctrl->cam->id == CAMERA_WB)
			break;
		ret = subdev_call(ctrl, core, s_ctrl, c);
		break;
	}

	mutex_unlock(&ctrl->v4l2_lock);

	return ret;
}

int fimc_cropcap_capture(void *fh, struct v4l2_cropcap *a)
{
	struct fimc_control *ctrl = fh;
	struct fimc_capinfo *cap = ctrl->cap;

	fimc_dbg("%s\n", __func__);

	mutex_lock(&ctrl->v4l2_lock);

	if (!ctrl->cam) {
		fimc_err("%s: s_input should be done before crop\n", __func__);
		mutex_unlock(&ctrl->v4l2_lock);
		return -ENODEV;
	}

	/* crop limitations */
	cap->cropcap.bounds.left = 0;
	cap->cropcap.bounds.top = 0;
	cap->cropcap.bounds.width = ctrl->cam->width;
	cap->cropcap.bounds.height = ctrl->cam->height;

	/* crop default values */
	cap->cropcap.defrect.left = 0;
	cap->cropcap.defrect.top = 0;
	cap->cropcap.defrect.width = ctrl->cam->width;
	cap->cropcap.defrect.height = ctrl->cam->height;

	a->bounds = cap->cropcap.bounds;
	a->defrect = cap->cropcap.defrect;

	mutex_unlock(&ctrl->v4l2_lock);

	return 0;
}

int fimc_g_crop_capture(void *fh, struct v4l2_crop *a)
{
	struct fimc_control *ctrl = fh;

	fimc_dbg("%s\n", __func__);

	mutex_lock(&ctrl->v4l2_lock);
	a->c = ctrl->cap->crop;
	mutex_unlock(&ctrl->v4l2_lock);

	return 0;
}

int fimc_s_crop_capture(void *fh, struct v4l2_crop *a)
{
	struct fimc_control *ctrl = fh;

	fimc_dbg("%s\n", __func__);

	mutex_lock(&ctrl->v4l2_lock);
	ctrl->cap->crop = a->c;
	mutex_unlock(&ctrl->v4l2_lock);

	return 0;
}

int fimc_start_capture(struct fimc_control *ctrl)
{
	fimc_dbg("%s\n", __func__);

	if (!ctrl->sc.bypass)
		fimc_hwset_start_scaler(ctrl);

	fimc_hwset_enable_capture(ctrl);

	return 0;
}

int fimc_stop_capture(struct fimc_control *ctrl)
{
	fimc_dbg("%s\n", __func__);

	if (ctrl->cap->lastirq) {
		fimc_hwset_enable_lastirq(ctrl);
		fimc_hwset_disable_capture(ctrl);
		fimc_hwset_disable_lastirq(ctrl);
	} else {
		fimc_hwset_disable_capture(ctrl);
	}

	fimc_hwset_stop_scaler(ctrl);

	return 0;
}

int fimc_streamon_capture(void *fh)
{
	struct fimc_control *ctrl = fh;
	struct fimc_capinfo *cap = ctrl->cap;
	int rot, i;

	fimc_dbg("%s\n", __func__);

	ctrl->status = FIMC_READY_ON;
	cap->irq = 0;

	fimc_hwset_enable_irq(ctrl, 0, 1);

	if (!ctrl->cam->initialized)
		fimc_init_camera(ctrl);

	/* Set FIMD to write back */
	if (ctrl->cam->id == CAMERA_WB)
		s3cfb_direct_ioctl(0, S3CFB_SET_WRITEBACK, 1);
	
	fimc_hwset_camera_source(ctrl);
	fimc_hwset_camera_offset(ctrl);
	fimc_hwset_camera_type(ctrl);
	fimc_hwset_camera_polarity(ctrl);
	fimc_capture_scaler_info(ctrl);
	fimc_hwset_prescaler(ctrl);
	fimc_hwset_scaler(ctrl);
	fimc_hwset_output_colorspace(ctrl, cap->fmt.pixelformat);
	fimc_hwset_output_addr_style(ctrl, cap->fmt.pixelformat);

	if (cap->fmt.pixelformat == V4L2_PIX_FMT_RGB32 ||
		cap->fmt.pixelformat == V4L2_PIX_FMT_RGB565)
		fimc_hwset_output_rgb(ctrl, cap->fmt.pixelformat);
	else
		fimc_hwset_output_yuv(ctrl, cap->fmt.pixelformat);

	fimc_hwset_output_size(ctrl, cap->fmt.width, cap->fmt.height);
	fimc_hwset_output_area(ctrl, cap->fmt.width, cap->fmt.height);
	fimc_hwset_output_scan(ctrl, &cap->fmt);

	fimc_hwset_output_rot_flip(ctrl, cap->rotate, cap->flip);
	rot = fimc_mapping_rot_flip(cap->rotate, cap->flip);

	if (rot & FIMC_ROT) {
		fimc_hwset_org_output_size(ctrl, cap->fmt.height,
						cap->fmt.width);
	} else {
		fimc_hwset_org_output_size(ctrl, cap->fmt.width,
						cap->fmt.height);
	}

	for (i = 0; i < FIMC_PHYBUFS; i++)
		fimc_add_outqueue(ctrl, i);

	fimc_start_capture(ctrl);

	ctrl->status = FIMC_STREAMON;

	return 0;
}

int fimc_streamoff_capture(void *fh)
{
	struct fimc_control *ctrl = fh;
	struct fimc_capinfo *cap = ctrl->cap;
	int i;

	fimc_dbg("%s\n", __func__);

	ctrl->status = FIMC_READY_OFF;
	fimc_stop_capture(ctrl);
	for(i = 0; i < FIMC_PHYBUFS; i++)
		fimc_add_inqueue(ctrl, cap->outq[i]);
	ctrl->status = FIMC_STREAMOFF;

	return 0;
}

int fimc_qbuf_capture(void *fh, struct v4l2_buffer *b)
{
	struct fimc_control *ctrl = fh;

	if (b->memory != V4L2_MEMORY_MMAP) {
		fimc_err("%s: invalid memory type\n", __func__);
		return -EINVAL;
	}

	if (ctrl->cap->nr_bufs > FIMC_PHYBUFS) {
		mutex_lock(&ctrl->v4l2_lock);
		fimc_add_inqueue(ctrl, b->index);
		mutex_unlock(&ctrl->v4l2_lock);
	}

	return 0;
}

int fimc_dqbuf_capture(void *fh, struct v4l2_buffer *b)
{
	struct fimc_control *ctrl = fh;
	struct fimc_capinfo *cap = ctrl->cap;
	int pp, ret = 0;

	if (b->memory != V4L2_MEMORY_MMAP) {
		fimc_err("%s: invalid memory type\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&ctrl->v4l2_lock);

	/* find out the real index */
	pp = ((fimc_hwget_frame_count(ctrl) + 2) % 4) % cap->nr_bufs;

	/* skip even frame: no data */
	if (cap->fmt.field == V4L2_FIELD_INTERLACED_TB)
		pp &= ~0x1;

	if (cap->nr_bufs > FIMC_PHYBUFS) {
		b->index = cap->outq[pp];
		ret = fimc_add_outqueue(ctrl, pp);
		if (ret) {
			b->index = -1;
			fimc_err("%s: no inqueue buffer\n", __func__);
		}
	} else {
		b->index = pp;
	}

	mutex_unlock(&ctrl->v4l2_lock);

	return ret;
}

