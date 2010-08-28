/* linux/drivers/media/video/samsung/fimc_cfg.c
 *
 * V4L2 Overlay device support file for Samsung Camera Interface (FIMC) driver
 *
 * Jonghun Han, Copyright (c) 2009 Samsung Electronics
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
#include <linux/io.h>
#include <linux/uaccess.h>
#include <plat/media.h>

#include "fimc.h"

int fimc_try_fmt_overlay(struct file *filp, void *fh, struct v4l2_format *f)
{
	struct fimc_control *ctrl = (struct fimc_control *) fh;
	u32 is_rotate = 0;

	fimc_info1("%s: called. " \
			"top(%d), left(%d), width(%d), height(%d)\n", \
			__func__, f->fmt.win.w.top, f->fmt.win.w.left, \
			f->fmt.win.w.width, f->fmt.win.w.height);

	if (ctrl->out->overlay.mode == FIMC_OVERLAY_NONE)
		return 0;

	/* Check Overlay Size : Overlay size must be smaller than LCD size. */
	is_rotate = fimc_mapping_rot_flip(ctrl->out->rotate, ctrl->out->flip);
	if (is_rotate & FIMC_ROT) {	/* Landscape mode */
		if (f->fmt.win.w.width > ctrl->fb.lcd_vres) {
			fimc_warn("The width is changed %d -> %d\n",
				f->fmt.win.w.width, ctrl->fb.lcd_vres);
			f->fmt.win.w.width = ctrl->fb.lcd_vres;
		}

		if (f->fmt.win.w.height > ctrl->fb.lcd_hres) {
			fimc_warn("The height is changed %d -> %d\n",
				f->fmt.win.w.height, ctrl->fb.lcd_hres);
			f->fmt.win.w.height = ctrl->fb.lcd_hres;
		}
	} else {			/* Portrait mode */
		if (f->fmt.win.w.width > ctrl->fb.lcd_hres) {
			fimc_warn("The width is changed %d -> %d\n",
				f->fmt.win.w.width, ctrl->fb.lcd_hres);
			f->fmt.win.w.width = ctrl->fb.lcd_hres;
		}

		if (f->fmt.win.w.height > ctrl->fb.lcd_vres) {
			fimc_warn("The height is changed %d -> %d\n",
				f->fmt.win.w.height, ctrl->fb.lcd_vres);
			f->fmt.win.w.height = ctrl->fb.lcd_vres;
		}
	}

	return 0;
}

int fimc_g_fmt_vid_overlay(struct file *file, void *fh, struct v4l2_format *f)
{
	struct fimc_control *ctrl = (struct fimc_control *) fh;

	fimc_info1("%s: called\n", __func__);

	f->fmt.win = ctrl->out->win;

	return 0;
}

static int fimc_check_pos(struct fimc_control *ctrl, struct v4l2_format *f)
{
	if(ctrl->out->win.w.width != f->fmt.win.w.width) {
		fimc_err("%s: cannot change width\n", __func__);
		return -EINVAL;
	} else if (ctrl->out->win.w.height != f->fmt.win.w.height) {
		fimc_err("%s: cannot change height\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static int fimc_change_fifo_position(struct fimc_control *ctrl)
{
	struct v4l2_rect fimd_rect;
	struct s3cfb_user_window window;
	int ret = -1;

	memset(&fimd_rect, 0, sizeof(struct v4l2_rect));

	ret = fimc_fimd_rect(ctrl, &fimd_rect);
	if (ret < 0) {
		fimc_err("fimc_fimd_rect fail\n");
		return -EINVAL;
	}

	/* Update WIN position */
	window.x = fimd_rect.left;
	window.y = fimd_rect.top;
	ret = s3cfb_direct_ioctl(ctrl->id, S3CFB_WIN_POSITION,
			(unsigned long)&window);
	if (ret < 0) {
		fimc_err("direct_ioctl(S3CFB_WIN_POSITION) fail\n");
		return -EINVAL;
	}

	return 0;
}

int fimc_s_fmt_vid_overlay(struct file *file, void *fh, struct v4l2_format *f)
{
	struct fimc_control *ctrl = (struct fimc_control *) fh;
	int ret = -1;

	fimc_info1("%s: called\n", __func__);

	switch (ctrl->status) {
	case FIMC_STREAMON:
		ret = fimc_check_pos(ctrl, f);
		if (ret < 0) {
			fimc_err("When FIMC is running, "
					"you can only move the position.\n");
			return -EBUSY;
		}

		ret = fimc_try_fmt_overlay(file, fh, f);
		if (ret < 0)
			return ret;

		ctrl->out->win = f->fmt.win;
		fimc_change_fifo_position(ctrl);

		break;
	case FIMC_STREAMOFF:
		ret = fimc_try_fmt_overlay(file, fh, f);
		if (ret < 0)
			return ret;
		ctrl->out->win = f->fmt.win;

		break;

	default:
		fimc_err("FIMC is running\n");
		return -EBUSY;
	}

	return ret;
}

int fimc_g_fbuf(struct file *filp, void *fh, struct v4l2_framebuffer *fb)
{
	struct fimc_control *ctrl = (struct fimc_control *) fh;
	u32 bpp = 1;
	u32 format = ctrl->out->fbuf.fmt.pixelformat;

	fimc_info1("%s: called\n", __func__);

	fb->capability = ctrl->out->fbuf.capability;
	fb->flags = 0;
	fb->base = ctrl->out->fbuf.base;

	fb->fmt.width = ctrl->out->fbuf.fmt.width;
	fb->fmt.height = ctrl->out->fbuf.fmt.height;
	fb->fmt.pixelformat = ctrl->out->fbuf.fmt.pixelformat;

	if (format == V4L2_PIX_FMT_NV12)
		bpp = 1;
	else if (format == V4L2_PIX_FMT_RGB32)
		bpp = 4;
	else if (format == V4L2_PIX_FMT_RGB565)
		bpp = 2;

	ctrl->out->fbuf.fmt.bytesperline = fb->fmt.width * bpp;
	fb->fmt.bytesperline = ctrl->out->fbuf.fmt.bytesperline;
	fb->fmt.sizeimage = ctrl->out->fbuf.fmt.sizeimage;
	fb->fmt.colorspace = V4L2_COLORSPACE_SMPTE170M;
	fb->fmt.priv = 0;

	return 0;
}

int fimc_s_fbuf(struct file *filp, void *fh, struct v4l2_framebuffer *fb)
{
	struct fimc_control *ctrl = (struct fimc_control *) fh;
	u32 bpp = 1;
	u32 format = fb->fmt.pixelformat;

	fimc_info1("%s: called. width(%d), height(%d)\n", 
				__func__, fb->fmt.width, fb->fmt.height);

	ctrl->out->fbuf.capability = V4L2_FBUF_CAP_EXTERNOVERLAY;
	ctrl->out->fbuf.flags = 0;
	ctrl->out->fbuf.base = fb->base;

	if (fb->base) {
		ctrl->out->fbuf.fmt.width = fb->fmt.width;
		ctrl->out->fbuf.fmt.height = fb->fmt.height;
		ctrl->out->fbuf.fmt.pixelformat	= fb->fmt.pixelformat;

		if (format == V4L2_PIX_FMT_NV12)
			bpp = 1;
		else if (format == V4L2_PIX_FMT_RGB32)
			bpp = 4;
		else if (format == V4L2_PIX_FMT_RGB565)
			bpp = 2;

		ctrl->out->fbuf.fmt.bytesperline = fb->fmt.width * bpp;
		ctrl->out->fbuf.fmt.sizeimage = fb->fmt.sizeimage;
		ctrl->out->fbuf.fmt.colorspace = V4L2_COLORSPACE_SMPTE170M;
		ctrl->out->fbuf.fmt.priv = 0;

		ctrl->out->overlay.mode = FIMC_OVERLAY_NONE;
	} else {
		ctrl->out->overlay.mode = FIMC_OVERLAY_MODE;
	}

	return 0;
}

