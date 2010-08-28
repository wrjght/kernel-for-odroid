/* linux/drivers/media/video/samsung/fimc_output.c
 *
 * V4L2 Output device support file for Samsung Camera Interface (FIMC) driver
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
#include <linux/mm.h>
#include <linux/videodev2.h>
#include <linux/videodev2_samsung.h>
#include <media/videobuf-core.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/mman.h>
#include <plat/media.h>

#include "fimc.h"
#include "fimc-ipc.h"

void fimc_outdev_set_src_addr(struct fimc_control *ctrl, dma_addr_t *base)
{
	fimc_hwset_addr_change_disable(ctrl);
	fimc_hwset_input_address(ctrl, base);
	fimc_hwset_addr_change_enable(ctrl);
}

int fimc_outdev_start_camif(void *param)
{
	struct fimc_control *ctrl = (struct fimc_control *)param;

	fimc_hwset_start_scaler(ctrl);
	fimc_hwset_enable_capture(ctrl);
	fimc_hwset_start_input_dma(ctrl);

	return 0;
}

static int fimc_outdev_stop_camif(void *param)
{
	struct fimc_control *ctrl = (struct fimc_control *)param;

	fimc_hwset_stop_input_dma(ctrl);
	fimc_hwset_disable_autoload(ctrl);
	fimc_hwset_stop_scaler(ctrl);
	fimc_hwset_disable_capture(ctrl);

	return 0;
}

static int fimc_outdev_stop_dma(struct fimc_control *ctrl)
{
	struct s3cfb_user_window window;
	int ret = -1;

	fimc_dbg("%s: called\n", __func__);

	ret = wait_event_timeout(ctrl->wq, \
			(ctrl->status == FIMC_STREAMOFF), \
			FIMC_ONESHOT_TIMEOUT);
	if (ret == 0)
		fimc_err("Fail : %s ctrl->status = %d\n", __func__, ctrl->status);

	fimc_outdev_stop_camif(ctrl);

	if (ctrl->out->overlay.mode == FIMC_OVERLAY_DMA_MANUAL)
		return 0;

	ret = s3cfb_direct_ioctl(ctrl->id, S3CFB_SET_WIN_OFF, (unsigned long)NULL);
	if (ret < 0) {
		fimc_err("direct_ioctl(S3CFB_SET_WIN_OFF) fail\n");
		return -EINVAL;
	}

	/* reset WIN position */
	memset(&window, 0, sizeof(window));
	ret = s3cfb_direct_ioctl(ctrl->id, S3CFB_WIN_POSITION,
			(unsigned long)&window);
	if (ret < 0) {
		fimc_err("direct_ioctl(S3CFB_WIN_POSITION) fail\n");
		return -EINVAL;
	}

	ret = s3cfb_direct_ioctl(ctrl->id, S3CFB_SET_WIN_ADDR, 0x00000000);
	if (ret < 0) {
		fimc_err("direct_ioctl(S3CFB_SET_WIN_ADDR) fail\n");
		return -EINVAL;
	}

	ret = s3cfb_direct_ioctl(ctrl->id, S3CFB_SET_WIN_MEM, DMA_MEM_NONE);
	if (ret < 0) {
		fimc_err("direct_ioctl(S3CFB_SET_WIN_MEM) fail\n");
		return -EINVAL;
	}

	ctrl->fb.is_enable = 0;

	return 0;
}

static int fimc_outdev_stop_fifo(struct fimc_control *ctrl)
{
	struct s3c_platform_fimc *pdata = to_fimc_plat(ctrl->dev);
	int ret = -1;

	fimc_dbg("%s: called\n", __func__);

	if (pdata->hw_ver == 0x40) {		/* to support C100 */
		ret = ctrl->fb.close_fifo(ctrl->id, fimc_outdev_stop_camif,
				(void *)ctrl);
		if (ret < 0)
			fimc_err("FIMD FIFO close fail\n");
	} else if ((pdata->hw_ver == 0x43) || (pdata->hw_ver == 0x50)) {	/* to support C110/6442 */
		ret = ctrl->fb.close_fifo(ctrl->id, NULL, NULL);
		if (ret < 0)
			fimc_err("FIMD FIFO close fail\n");
		fimc_hw_wait_winoff(ctrl);
		fimc_outdev_stop_camif(ctrl);
		fimc_hw_wait_stop_input_dma(ctrl);
#if defined (CONFIG_VIDEO_IPC)
		if (ctrl->out->pix.field == V4L2_FIELD_INTERLACED_TB)
			ipc_stop();
#endif
	} 

	return 0;
}

int fimc_outdev_stop_streaming(struct fimc_control *ctrl)
{
	int ret = 0;

	fimc_dbg("%s: called\n", __func__);

	switch (ctrl->out->overlay.mode) {
	case FIMC_OVERLAY_NONE:
		if (ctrl->status == FIMC_STREAMON_IDLE)
			ctrl->status = FIMC_STREAMOFF;
		else 
			ctrl->status = FIMC_READY_OFF;

		ret = wait_event_timeout(ctrl->wq, \
				(ctrl->status == FIMC_STREAMOFF), \
				FIMC_ONESHOT_TIMEOUT);
		if (ret == 0)
			fimc_err("Fail : %s\n", __func__);

		fimc_outdev_stop_camif(ctrl);

		break;
	case FIMC_OVERLAY_DMA_AUTO:
	case FIMC_OVERLAY_DMA_MANUAL:
		if (ctrl->status == FIMC_STREAMON_IDLE)
			ctrl->status = FIMC_STREAMOFF;
		else 
			ctrl->status = FIMC_READY_OFF;

		fimc_outdev_stop_dma(ctrl);

		break;
	case FIMC_OVERLAY_FIFO:
		ctrl->status = FIMC_READY_OFF;
		fimc_outdev_stop_fifo(ctrl);

		break;
	}

	return 0;
}

int fimc_outdev_resume_dma(struct fimc_control *ctrl)
{
	struct v4l2_rect fimd_rect;
	struct fb_var_screeninfo var;
	struct s3cfb_user_window window;
	int ret = -1, idx;
	u32 id = ctrl->id;

	memset(&fimd_rect, 0, sizeof(struct v4l2_rect));
	ret = fimc_fimd_rect(ctrl, &fimd_rect);
	if (ret < 0) {
		fimc_err("fimc_fimd_rect fail\n");
		return -EINVAL;
	}

	/* Get WIN var_screeninfo  */
	ret = s3cfb_direct_ioctl(id, FBIOGET_VSCREENINFO, (unsigned long)&var);
	if (ret < 0) {
		fimc_err("direct_ioctl(FBIOGET_VSCREENINFO) fail\n");
		return -EINVAL;
	}

	/* window path : DMA */
	ret = s3cfb_direct_ioctl(id, S3CFB_SET_WIN_PATH, DATA_PATH_DMA);
	if (ret < 0) {
		fimc_err("direct_ioctl(S3CFB_SET_WIN_PATH) fail\n");
		return -EINVAL;
	}

	/* Don't allocate the memory. */
	ret = s3cfb_direct_ioctl(id, S3CFB_SET_WIN_MEM, DMA_MEM_OTHER);
	if (ret < 0) {
		fimc_err("direct_ioctl(S3CFB_SET_WIN_MEM) fail\n");
		return -EINVAL;
	}

	/* Update WIN size  */
	var.xres_virtual = fimd_rect.width;
	var.yres_virtual = fimd_rect.height;
	var.xres = fimd_rect.width;
	var.yres = fimd_rect.height;

	ret = s3cfb_direct_ioctl(id, FBIOPUT_VSCREENINFO, (unsigned long)&var);
	if (ret < 0) {
		fimc_err("direct_ioctl(FBIOPUT_VSCREENINFO) fail\n");
		return -EINVAL;
	}

	/* Update WIN position */
	window.x = fimd_rect.left;
	window.y = fimd_rect.top;
	ret = s3cfb_direct_ioctl(id, S3CFB_WIN_POSITION,
			(unsigned long)&window);
	if (ret < 0) {
		fimc_err("direct_ioctl(S3CFB_WIN_POSITION) fail\n");
		return -EINVAL;
	}

	idx = ctrl->out->out_queue[0];
	if (idx == -1) {
		fimc_err("out going queue is empty.\n");
		return -EINVAL;
	}
		
	ret = s3cfb_direct_ioctl(ctrl->id, S3CFB_SET_WIN_ADDR, \
			(unsigned long)ctrl->out->dst[idx].base[FIMC_ADDR_Y]);
	if (ret < 0) {
		fimc_err("direct_ioctl(S3CFB_SET_WIN_ADDR) fail\n");
		return -EINVAL;
	}

	ret = s3cfb_direct_ioctl(ctrl->id, S3CFB_SET_WIN_ON, \
						(unsigned long)NULL);
	if (ret < 0) {
		fimc_err("direct_ioctl(S3CFB_SET_WIN_ON) fail\n");
		return -EINVAL;
	}

	ctrl->fb.is_enable = 1;

	return 0;
}

static void fimc_init_out_buf(struct fimc_control *ctrl)
{
	int i;

	for (i = 0; i < FIMC_OUTBUFS; i++) {
		ctrl->out->src[i].state = VIDEOBUF_IDLE;
		ctrl->out->src[i].flags = 0x0;

		ctrl->out->in_queue[i] = -1;
		ctrl->out->out_queue[i] = -1;
	}
}

static 
int fimc_outdev_set_src_buf(struct fimc_control *ctrl)
{
	u32 width = ctrl->out->pix.width; 
	u32 height = ctrl->out->pix.height;
	u32 format = ctrl->out->pix.pixelformat;
	u32 y_size = width * height;
	u32 c_size = (width * height >> 1);
	u32 i, size;
	dma_addr_t *curr = &ctrl->mem.curr;

	switch (format) {
	case V4L2_PIX_FMT_RGB32:
		size = PAGE_ALIGN(width * height * 4);
		break;
	case V4L2_PIX_FMT_YUYV:		/* fall through */
	case V4L2_PIX_FMT_RGB565:	/* fall through */
		size = PAGE_ALIGN(width * height * 2);
		break;
	case V4L2_PIX_FMT_NV12:		/* fall through */
	case V4L2_PIX_FMT_NV12T:
		size = PAGE_ALIGN(y_size + c_size);
		break;

	default: 
		fimc_err("%s: Invalid pixelformt : %d\n", __func__, format);
		return -EINVAL;
	}

	if ((size * FIMC_OUTBUFS) > ctrl->mem.size) {
		fimc_err("Reserved memory is not sufficient\n");
		return -EINVAL;
	}

	/* Initialize source buffer addr */
	switch (format) {
	case V4L2_PIX_FMT_YUYV:		/* fall through */
	case V4L2_PIX_FMT_RGB565:	/* fall through */
	case V4L2_PIX_FMT_RGB32:
		for (i = 0; i < FIMC_OUTBUFS; i++) {
			ctrl->out->src[i].base[FIMC_ADDR_Y] = *curr;
			ctrl->out->src[i].length[FIMC_ADDR_Y] = size;
			ctrl->out->src[i].base[FIMC_ADDR_CB] = 0;
			ctrl->out->src[i].length[FIMC_ADDR_CB] = 0;
			ctrl->out->src[i].base[FIMC_ADDR_CR] = 0;
			ctrl->out->src[i].length[FIMC_ADDR_CR] = 0;
			*curr += size;
		}

		break;
	case V4L2_PIX_FMT_NV12:		/* fall through */
	case V4L2_PIX_FMT_NV12T:
		for (i = 0; i < FIMC_OUTBUFS; i++) {
			ctrl->out->src[i].base[FIMC_ADDR_Y] = *curr;
			ctrl->out->src[i].base[FIMC_ADDR_CB] = *curr + y_size;
			ctrl->out->src[i].length[FIMC_ADDR_Y] = y_size;
			ctrl->out->src[i].length[FIMC_ADDR_CB] = c_size;
			ctrl->out->src[i].base[FIMC_ADDR_CR] = 0;
			ctrl->out->src[i].length[FIMC_ADDR_CR] = 0;
			*curr += size;
		}

		break;

	default: 
		fimc_err("%s: Invalid pixelformt : %d\n", __func__, format);
		return -EINVAL;
	}

	return 0;
}


static 
int fimc_outdev_set_dst_buf(struct fimc_control *ctrl)
{
	dma_addr_t *curr = &ctrl->mem.curr;
	dma_addr_t end;
	u32 width = ctrl->fb.lcd_hres; 
	u32 height = ctrl->fb.lcd_vres; 
	u32 i, size;

	end = ctrl->mem.base + ctrl->mem.size;
	size = PAGE_ALIGN(width * height * 4);

	if ((*curr + (size * FIMC_OUTBUFS)) > end) {
		fimc_err("Reserved memory is not sufficient\n");
		return -EINVAL;
	}

	/* Initialize destination buffer addr */
	for (i = 0; i < FIMC_OUTBUFS; i++) {
		ctrl->out->dst[i].base[FIMC_ADDR_Y] = *curr;
		ctrl->out->dst[i].length[FIMC_ADDR_Y] = size;
		ctrl->out->dst[i].base[FIMC_ADDR_CB] = 0;
		ctrl->out->dst[i].length[FIMC_ADDR_CB] = 0;
		ctrl->out->dst[i].base[FIMC_ADDR_CR] = 0;
		ctrl->out->dst[i].length[FIMC_ADDR_CR] = 0;
		*curr += size;
	}

	return 0;
}


static int fimc_set_rot_degree(struct fimc_control *ctrl, int degree)
{
	switch (degree) {
	case 0:		/* fall through */
	case 90:	/* fall through */
	case 180:	/* fall through */
	case 270:
		ctrl->out->rotate = degree;
		break;

	default:
		fimc_err("Invalid rotate value : %d\n", degree);
		return -EINVAL;
	}

	return 0;
}

static int fimc_outdev_check_param(struct fimc_control *ctrl)
{
	struct v4l2_rect dst, bound;
	u32 rot = 0;
	int ret = 0;

	rot = fimc_mapping_rot_flip(ctrl->out->rotate, ctrl->out->flip);
	dst.top = ctrl->out->win.w.top;
	dst.left = ctrl->out->win.w.left;
	dst.width = ctrl->out->win.w.width;
	dst.height = ctrl->out->win.w.height;

	if (ctrl->out->overlay.mode == FIMC_OVERLAY_NONE) {
		bound.width = ctrl->out->fbuf.fmt.width;
		bound.height = ctrl->out->fbuf.fmt.height;
	} else {	/* Non-Destructive OVERLAY device */
		if (rot & FIMC_ROT) {
			bound.width = ctrl->fb.lcd_vres;
			bound.height = ctrl->fb.lcd_hres;
		} else {
			bound.width = ctrl->fb.lcd_hres;
			bound.height = ctrl->fb.lcd_vres;
		}
	}

	if ((dst.left + dst.width) > bound.width) {
		fimc_err("Horizontal position setting is failed\n");
		fimc_err("\tleft = %d, width = %d, bound width = %d,\n",
				dst.left, dst.width, bound.width);
		ret = -EINVAL;
	} else if ((dst.top + dst.height) > bound.height) {
		fimc_err("Vertical position setting is failed\n");
		fimc_err("\ttop = %d, height = %d, bound height = %d, \n",
			dst.top, dst.height, bound.height);
		ret = -EINVAL;
	}

	return ret;
}

static void fimc_outdev_set_src_format(struct fimc_control *ctrl, u32 pixfmt, \
							enum v4l2_field field)
{
	fimc_hwset_input_burst_cnt(ctrl, 4);
	fimc_hwset_input_colorspace(ctrl, pixfmt);
	fimc_hwset_input_yuv(ctrl, pixfmt);
	fimc_hwset_input_rgb(ctrl, pixfmt);
	fimc_hwset_intput_field(ctrl, field);
	fimc_hwset_ext_rgb(ctrl, 1);
	fimc_hwset_input_addr_style(ctrl, pixfmt);
}

static void fimc_outdev_set_dst_format(struct fimc_control *ctrl, \
						struct v4l2_pix_format *pixfmt)
{
	fimc_hwset_output_colorspace(ctrl, pixfmt->pixelformat);
	fimc_hwset_output_yuv(ctrl, pixfmt->pixelformat);
	fimc_hwset_output_rgb(ctrl, pixfmt->pixelformat);
	fimc_hwset_output_scan(ctrl, pixfmt);
	fimc_hwset_output_addr_style(ctrl, pixfmt->pixelformat);
}

static void fimc_outdev_set_format(struct fimc_control *ctrl)
{
	struct v4l2_pix_format pixfmt;
	memset(&pixfmt, 0, sizeof(pixfmt));

	fimc_outdev_set_src_format(ctrl, ctrl->out->pix.pixelformat, \
							ctrl->out->pix.field);

	if (ctrl->out->overlay.mode == FIMC_OVERLAY_NONE) {
		pixfmt.pixelformat = ctrl->out->fbuf.fmt.pixelformat;
		pixfmt.field = V4L2_FIELD_NONE;
	} else {	/* Non-destructive overlay mode */
		if (ctrl->out->pix.field == V4L2_FIELD_NONE) {
			pixfmt.pixelformat = V4L2_PIX_FMT_RGB32;
			pixfmt.field = V4L2_FIELD_NONE;
		} else if (ctrl->out->pix.field == V4L2_FIELD_INTERLACED_TB) {
			pixfmt.pixelformat = V4L2_PIX_FMT_YUV444;
			pixfmt.field = V4L2_FIELD_INTERLACED_TB;
		}
	}

	fimc_outdev_set_dst_format(ctrl, &pixfmt);
}

static void fimc_outdev_set_path(struct fimc_control *ctrl)
{
	/* source path */
	fimc_hwset_input_source(ctrl, FIMC_SRC_MSDMA);

	if (ctrl->out->overlay.mode == FIMC_OVERLAY_FIFO) {
		fimc_hwset_enable_lcdfifo(ctrl);
		fimc_hwset_enable_autoload(ctrl);
	} else {
		fimc_hwset_disable_lcdfifo(ctrl);
		fimc_hwset_disable_autoload(ctrl);
	}
}

static void fimc_outdev_set_rot(struct fimc_control *ctrl)
{
	u32 rot = ctrl->out->rotate;
	u32 flip = ctrl->out->flip;

	if (ctrl->out->overlay.mode == FIMC_OVERLAY_FIFO) {
		fimc_hwset_input_rot(ctrl, rot, flip);
		fimc_hwset_input_flip(ctrl, rot, flip);
		fimc_hwset_output_rot_flip(ctrl, 0, 0);
	} else {
		fimc_hwset_input_rot(ctrl, 0, 0);
		fimc_hwset_input_flip(ctrl, 0, 0);
		fimc_hwset_output_rot_flip(ctrl, rot, flip);
	}
}

static void fimc_outdev_set_src_dma_offset(struct fimc_control *ctrl)
{
	struct v4l2_rect bound, crop;
	u32 pixfmt = ctrl->out->pix.pixelformat;

	bound.width = ctrl->out->pix.width;
	bound.height = ctrl->out->pix.height;

	crop.left = ctrl->out->crop.left;
	crop.top = ctrl->out->crop.top;
	crop.width = ctrl->out->crop.width;
	crop.height = ctrl->out->crop.height;

	fimc_hwset_input_offset(ctrl, pixfmt, &bound, &crop);
}

static int fimc_outdev_check_src_size(struct fimc_control *ctrl, \
				struct v4l2_rect *real, struct v4l2_rect *org)
{
	u32 rot = ctrl->out->rotate;
	u32 pixelformat = ctrl->out->pix.pixelformat;
	struct s3c_platform_fimc *pdata = to_fimc_plat(ctrl->dev);
	if ((ctrl->out->overlay.mode == FIMC_OVERLAY_FIFO) && ((rot == 90) || (rot == 270))) {
		/* Input Rotator */
		if (pdata->hw_ver == 0x50) {
			switch (pixelformat) {
			case V4L2_PIX_FMT_YUV422P:	/* fall through */
			case V4L2_PIX_FMT_YVU420:
				if (real->height % 2) {		
					fimc_err("SRC Real_H: multiple of 2\n");
					return -EINVAL;
				}
			}
		} else {
			if (real->height % 16) {
				fimc_err("SRC Real_H: multiple of 16 !\n");
				return -EINVAL;
			}

			if (ctrl->sc.pre_hratio) {
				if (real->height % (ctrl->sc.pre_hratio*4)) {
					fimc_err("SRC Real_H: multiple of \
							4*pre_hratio !\n");
					return -EINVAL;
				}
			}

			if (ctrl->sc.pre_vratio) {
				if (real->width % ctrl->sc.pre_vratio) {
					fimc_err("SRC Real_W: multiple of \
							pre_vratio!\n");
					return -EINVAL;
				}
			}
		}

		if (real->height < 16) {
			fimc_err("SRC Real_H: Min 16\n");
			return -EINVAL;
		}
		if (real->width < 8) {
			fimc_err("SRC Real_W: Min 8\n");
			return -EINVAL;
		}
	} else {
		/* No Input Rotator */
		if (real->height < 8) {
			fimc_err("SRC Real_H: Min 8\n");
			return -EINVAL;
		}

		if (real->width < 16) {
			fimc_err("SRC Real_W: Min 16\n");
			return -EINVAL;
		}

		if (real->width > ctrl->limit->real_w_no_rot) {
			fimc_err("SRC REAL_W: Real_W <= %d\n", \
					ctrl->limit->real_w_no_rot);
			return -EINVAL;
		}
	}

	if (org->height < real->height) {
		fimc_err("SRC Org_H: larger than Real_H\n");
		return -EINVAL;
	}

	if (org->width < real->width) {
		fimc_err("SRC Org_W: Org_W >= Real_W\n");
		return -EINVAL;
	}
	
	if (pdata->hw_ver == 0x50) {
		if (ctrl->out->pix.field == V4L2_FIELD_INTERLACED_TB) {
			switch (pixelformat) {
			case V4L2_PIX_FMT_YUV444:	/* fall through */
			case V4L2_PIX_FMT_RGB32:
				if (real->height % 2) {		
					fimc_err("SRC Real_H: multiple of 2\n");
					return -EINVAL;
				}
			case V4L2_PIX_FMT_YUV422P:
				if (real->height % 2) {		
					fimc_err("SRC Real_H: multiple of 2\n");
					return -EINVAL;
				} else if (real->width % 2) {
					fimc_err("SRC Real_H: multiple of 2\n");
					return -EINVAL;
				}
			case V4L2_PIX_FMT_YVU420:
				if (real->height % 4) {		
					fimc_err("SRC Real_H: multiple of 4\n");
					return -EINVAL;
				} else if (real->width % 2) {
					fimc_err("SRC Real_H: multiple of 2\n");
					return -EINVAL;
				}

			}
		} else if (ctrl->out->pix.field == V4L2_FIELD_NONE) {
			switch (pixelformat) {
			case V4L2_PIX_FMT_YUV422P:
				if (real->height % 2) { 		
					fimc_err("SRC Real_H: multiple of 2\n");
					return -EINVAL;
				} else if(real->width % 2) {
					fimc_err("SRC Real_H: multiple of 2\n");
					return -EINVAL;
				}
			}
		}	 
	} else {
		if (ctrl->sc.pre_vratio) {
			if (real->height % ctrl->sc.pre_vratio) {
				fimc_err("SRC Real_H: multiple of \
						pre_vratio!\n");
				return -EINVAL;
			}
		}

		if (real->width % 16) {
			fimc_err("SRC Real_W: multiple of 16 !\n");
			return -EINVAL;
		}

		if (ctrl->sc.pre_hratio) {
			if (real->width % (ctrl->sc.pre_hratio*4)) {
				fimc_err("SRC Real_W: multiple of 4*pre_hratio!\n");
				return -EINVAL;
			}
		}

		if (org->width % 16) {
			fimc_err("SRC Org_W: multiple of 16\n");
			return -EINVAL;
		}
		
		if (org->height < 8) {
			fimc_err("SRC Org_H: Min 8\n");
			return -EINVAL;
		}

	}
	
	return 0;
}

static int fimc_outdev_set_src_dma_size(struct fimc_control *ctrl)
{
	struct v4l2_rect real, org;
	int ret = 0;

	real.width = ctrl->out->crop.width;
	real.height = ctrl->out->crop.height;
	org.width = ctrl->out->pix.width;
	org.height = ctrl->out->pix.height;

	ret = fimc_outdev_check_src_size(ctrl, &real, &org);
	if (ret < 0)
		return ret;

	fimc_hwset_org_input_size(ctrl, org.width, org.height);
	fimc_hwset_real_input_size(ctrl, real.width, real.height);

	return 0;
}

static void fimc_outdev_set_dst_dma_offset(struct fimc_control *ctrl)
{
	struct v4l2_rect bound, win;
	u32 pixfmt = ctrl->out->fbuf.fmt.pixelformat;
	memset(&bound, 0, sizeof(bound));
	memset(&win, 0, sizeof(win));

	switch (ctrl->out->rotate) {
	case 0:
		bound.width = ctrl->out->fbuf.fmt.width;
		bound.height = ctrl->out->fbuf.fmt.height;

		win.left = ctrl->out->win.w.left;
		win.top = ctrl->out->win.w.top;
		win.width = ctrl->out->win.w.width;
		win.height = ctrl->out->win.w.height;

		break;

	case 90:
		bound.width = ctrl->out->fbuf.fmt.height;
		bound.height = ctrl->out->fbuf.fmt.width;

		win.left = ctrl->out->fbuf.fmt.height -
				(ctrl->out->win.w.height +
				 ctrl->out->win.w.top);
		win.top = ctrl->out->win.w.left;
		win.width = ctrl->out->win.w.height;
		win.height = ctrl->out->win.w.width;

		break;

	case 180:
		bound.width = ctrl->out->fbuf.fmt.width;
		bound.height = ctrl->out->fbuf.fmt.height;

		win.left = ctrl->out->fbuf.fmt.width -
				(ctrl->out->win.w.left +
				 ctrl->out->win.w.width);
		win.top = ctrl->out->fbuf.fmt.height -
				(ctrl->out->win.w.top +
				 ctrl->out->win.w.height);
		win.width = ctrl->out->win.w.width;
		win.height = ctrl->out->win.w.height;

		break;

	case 270:
		bound.width = ctrl->out->fbuf.fmt.height;
		bound.height = ctrl->out->fbuf.fmt.width;

		win.left = ctrl->out->win.w.top;
		win.top = ctrl->out->fbuf.fmt.width -
				(ctrl->out->win.w.left +
				 ctrl->out->win.w.width);
		win.width = ctrl->out->win.w.height;
		win.height = ctrl->out->win.w.width;

		break;

	default:
		fimc_err("Rotation degree is invalid\n");
		break;
	}


	fimc_dbg("bound.width(%d), bound.height(%d)\n",
				bound.width, bound.height);
	fimc_dbg("win.width(%d), win.height(%d)\n",
				win.width, win.height);
	fimc_dbg("win.top(%d), win.left(%d)\n", win.top, win.left);

	fimc_hwset_output_offset(ctrl, pixfmt, &bound, &win);
}

static int fimc_outdev_check_dst_size(struct fimc_control *ctrl, \
				struct v4l2_rect *real, struct v4l2_rect *org)
{
	u32 rot = ctrl->out->rotate;

	if (real->height % 2) {
		fimc_err("DST Real_H: even number\n");
		return -EINVAL;
	}

	if ((ctrl->out->overlay.mode != FIMC_OVERLAY_FIFO) && ((rot == 90) || (rot == 270))) {
		/* Use Output Rotator */
		if (org->height < real->width) {
			fimc_err("DST Org_H: Org_H(%d) >= Real_W(%d)\n", org->height, real->width);
			return -EINVAL;
		}

		if (org->width < real->height) {
			fimc_err("DST Org_W: Org_W(%d) >= Real_H(%d)\n", 
						org->width, real->height);
			return -EINVAL;
		}

		if (real->height > ctrl->limit->trg_h_rot) {
			fimc_err("DST REAL_H: Real_H <= %d\n", \
						ctrl->limit->trg_h_rot);
			return -EINVAL;
		}
	} else if (ctrl->out->overlay.mode != FIMC_OVERLAY_FIFO) {
		/* No Output Rotator */
		if (org->height < 8) {
			fimc_err("DST Org_H: Min 8\n");
			return -EINVAL;
		}

		if (org->height < real->height) {
			fimc_err("DST Org_H: Org_H >= Real_H\n");
			return -EINVAL;
		}

		if (org->width % 8) {
			fimc_err("DST Org_W: multiple of 8\n");
			return -EINVAL;
		}

		if (org->width < real->width) {
			fimc_err("DST Org_W: Org_W >= Real_W\n");
			return -EINVAL;
		}

		if (real->height > ctrl->limit->trg_h_no_rot) {
			fimc_err("DST REAL_H: Real_H <= %d\n", \
						ctrl->limit->trg_h_no_rot);
			return -EINVAL;
		}
	}

	return 0;
}

static int fimc_outdev_set_dst_dma_size(struct fimc_control *ctrl)
{
	struct v4l2_rect org, real;
	int ret = -1;

	memset(&org, 0, sizeof(org));
	memset(&real, 0, sizeof(real));

	switch (ctrl->out->overlay.mode) {
	case FIMC_OVERLAY_NONE:
		real.width = ctrl->out->win.w.width;
		real.height = ctrl->out->win.w.height;

		switch (ctrl->out->rotate) {
		case 0:
		case 180:
			org.width = ctrl->out->fbuf.fmt.width;
			org.height = ctrl->out->fbuf.fmt.height;

			break;

		case 90:
		case 270:
			org.width = ctrl->out->fbuf.fmt.height;
			org.height = ctrl->out->fbuf.fmt.width;

			break;

		default:
			fimc_err("Rotation degree is invalid\n");
			break;
		}

		break;

	case FIMC_OVERLAY_DMA_MANUAL:	/* fall through */
	case FIMC_OVERLAY_DMA_AUTO:
		real.width = ctrl->out->win.w.width;
		real.height = ctrl->out->win.w.height;

		switch (ctrl->out->rotate) {
		case 0:
		case 180:
			org.width = ctrl->out->win.w.width;
			org.height = ctrl->out->win.w.height;

			break;

		case 90:
		case 270:
			org.width = ctrl->out->win.w.height;
			org.height = ctrl->out->win.w.width;

			break;

		default:
			fimc_err("Rotation degree is invalid\n");
			break;
		}

		break;
	case FIMC_OVERLAY_FIFO:
		switch (ctrl->out->rotate) {
		case 0:
		case 180:
			real.width = ctrl->out->win.w.width;
			real.height = ctrl->out->win.w.height;
			org.width = ctrl->fb.lcd_hres;
			org.height = ctrl->fb.lcd_vres;

			break;

		case 90:
		case 270:
			real.width = ctrl->out->win.w.height;
			real.height = ctrl->out->win.w.width;

			org.width = ctrl->fb.lcd_vres;
			org.height = ctrl->fb.lcd_hres;

			break;

		default:
			fimc_err("Rotation degree is invalid\n");
			break;
		}

		break;
	}

	fimc_dbg("DST : org.width(%d), org.height(%d)\n", \
				org.width, org.height);
	fimc_dbg("DST : real.width(%d), real.height(%d)\n", \
				real.width, real.height);

	ret = fimc_outdev_check_dst_size(ctrl, &real, &org);
	if (ret < 0)
		return ret;

	fimc_hwset_output_size(ctrl, real.width, real.height);
	fimc_hwset_output_area(ctrl, real.width, real.height);
	fimc_hwset_org_output_size(ctrl, org.width, org.height);
	fimc_hwset_ext_output_size(ctrl, real.width, real.height);

	return 0;
}

static void fimc_outdev_calibrate_scale_info(struct fimc_control *ctrl, \
				struct v4l2_rect *src, struct v4l2_rect *dst)
{
	if (ctrl->out->overlay.mode != FIMC_OVERLAY_FIFO) {	/* OUTPUT ROTATOR */
		src->width = ctrl->out->crop.width;
		src->height = ctrl->out->crop.height;
		dst->width = ctrl->out->win.w.width;
		dst->height = ctrl->out->win.w.height;
	} else {					/* INPUT ROTATOR */
		switch (ctrl->out->rotate) {
		case 0:		/* fall through */
		case 180:
			src->width = ctrl->out->crop.width;
			src->height = ctrl->out->crop.height;
			dst->width = ctrl->out->win.w.width;
			dst->height = ctrl->out->win.w.height;

			break;

		case 90:	/* fall through */
		case 270:
			src->width = ctrl->out->crop.height;
			src->height = ctrl->out->crop.width;
			dst->width = ctrl->out->win.w.height;
			dst->height = ctrl->out->win.w.width;

			break;

		default:
			fimc_err("Rotation degree is invalid\n");
			break;
		}
	}

	fimc_dbg("src->width(%d), src->height(%d)\n", \
				src->width, src->height);
	fimc_dbg("dst->width(%d), dst->height(%d)\n", \
				dst->width, dst->height);
}

static int fimc_outdev_check_scaler(struct fimc_control *ctrl, \
				struct v4l2_rect *src, struct v4l2_rect *dst)
{
	u32 pixels = 0, dstfmt = 0;

	/* Check scaler limitation */
	if (ctrl->sc.pre_dst_width > ctrl->limit->pre_dst_w) {
		fimc_err("FIMC%d : MAX PreDstWidth is %d\n",
					ctrl->id, ctrl->limit->pre_dst_w);
		return -EDOM;
	}

	/* SRC width double boundary check */
	switch (ctrl->out->pix.pixelformat) {
	case V4L2_PIX_FMT_RGB32:
		pixels = 1;		
		break;
	case V4L2_PIX_FMT_YUYV:		/* fall through */
	case V4L2_PIX_FMT_RGB565:
		pixels = 2;
		break;		
	case V4L2_PIX_FMT_NV12:		/* fall through */
	case V4L2_PIX_FMT_NV12T:
		pixels = 8;		
		break;
	default:
		fimc_err("Invalid color format\n");
		return -EINVAL;
	}

	if (src->width % pixels) {
		fimc_err("source width multiple of %d pixels\n", pixels);
		return -EDOM;
	}

	/* DST width double boundary check */
	if (ctrl->out->overlay.mode == FIMC_OVERLAY_NONE)
		dstfmt = ctrl->out->fbuf.fmt.pixelformat;
	else
		dstfmt = V4L2_PIX_FMT_RGB32;

	switch (dstfmt) {
	case V4L2_PIX_FMT_RGB32:
		pixels = 1;
		break;
	case V4L2_PIX_FMT_RGB565:
		pixels = 2;
		break;
	default:
		fimc_err("Invalid color format\n");
		return -EINVAL;
	}

	if (dst->width % pixels) {
		fimc_err("source width multiple of %d pixels\n", pixels);
		return -EDOM;
	}

	return 0;
}

static int fimc_outdev_set_scaler(struct fimc_control *ctrl)
{
	struct v4l2_rect src, dst;
	int ret = 0;
	struct s3c_platform_fimc *pdata = to_fimc_plat(ctrl->dev);

	memset(&src, 0, sizeof(src));
	memset(&dst, 0, sizeof(dst));

	fimc_outdev_calibrate_scale_info(ctrl, &src, &dst);

	ret = fimc_get_scaler_factor(src.width, dst.width, \
			&ctrl->sc.pre_hratio, &ctrl->sc.hfactor);
	if (ret < 0) {
		fimc_err("Fail : Out of Width scale range\n");
		return ret;
	}

	ret = fimc_get_scaler_factor(src.height, dst.height, \
			&ctrl->sc.pre_vratio, &ctrl->sc.vfactor);
	if (ret < 0) {
		fimc_err("Fail : Out of Height scale range\n");
		return ret;
	}

	ctrl->sc.pre_dst_width = src.width / ctrl->sc.pre_hratio;
	ctrl->sc.pre_dst_height = src.height / ctrl->sc.pre_vratio;

	if (pdata->hw_ver == 0x50) {
		ctrl->sc.main_hratio = (src.width << 14) / (dst.width << ctrl->sc.hfactor);
		ctrl->sc.main_vratio = (src.height << 14) / 
			(dst.height << ctrl->sc.vfactor);
	} else {
		ctrl->sc.main_hratio = (src.width << 8) / (dst.width<<ctrl->sc.hfactor);
		ctrl->sc.main_vratio = (src.height << 8) /
			(dst.height<<ctrl->sc.vfactor);
	}

	fimc_dbg("pre_hratio(%d), hfactor(%d), \
			pre_vratio(%d), vfactor(%d)\n", \
			ctrl->sc.pre_hratio, ctrl->sc.hfactor, \
			ctrl->sc.pre_vratio, ctrl->sc.vfactor);


	fimc_dbg("pre_dst_width(%d), main_hratio(%d),\
			pre_dst_height(%d), main_vratio(%d)\n", \
			ctrl->sc.pre_dst_width, ctrl->sc.main_hratio, \
			ctrl->sc.pre_dst_height, ctrl->sc.main_vratio);

	/* Input DMA cannot support scaler bypass. */
	ctrl->sc.bypass = 0;

	ctrl->sc.scaleup_h = (dst.width >= src.width) ? 1 : 0;
	ctrl->sc.scaleup_v = (dst.height >= src.height) ? 1 : 0;

	ctrl->sc.shfactor = 10 - (ctrl->sc.hfactor + ctrl->sc.vfactor);

	if (pdata->hw_ver != 0x50) {
		ret = fimc_outdev_check_scaler(ctrl, &src, &dst);
		if (ret < 0)
			return ret;
	}

	fimc_hwset_prescaler(ctrl);
	fimc_hwset_scaler(ctrl);

	return 0;
}

int fimc_outdev_set_param(struct fimc_control *ctrl)
{
	int ret = -1;
#if defined (CONFIG_VIDEO_IPC)
	u32 use_ipc = 0;
	struct v4l2_rect src, dst;
	memset(&src, 0, sizeof(src));
	memset(&dst, 0, sizeof(dst));
#endif

	if ((ctrl->status != FIMC_STREAMOFF) && \
			(ctrl->status != FIMC_READY_ON) && \
			(ctrl->status != FIMC_ON_IDLE_SLEEP)) {
		fimc_err("FIMC is running\n");
		return -EBUSY;
	}

	fimc_hwset_enable_irq(ctrl, 0, 1);
	fimc_outdev_set_format(ctrl);
	fimc_outdev_set_path(ctrl);
	fimc_outdev_set_rot(ctrl);

	fimc_outdev_set_src_dma_offset(ctrl);
	ret = fimc_outdev_set_src_dma_size(ctrl);
	if (ret < 0)
		return ret;

	if (ctrl->out->overlay.mode == FIMC_OVERLAY_NONE)
		fimc_outdev_set_dst_dma_offset(ctrl);

	ret = fimc_outdev_set_dst_dma_size(ctrl);
	if (ret < 0)
		return ret;

	ret = fimc_outdev_set_scaler(ctrl);
	if (ret < 0)
		return ret;

#if defined (CONFIG_VIDEO_IPC)
	if (ctrl->out->overlay.mode == FIMC_OVERLAY_FIFO)
		if (ctrl->out->pix.field == V4L2_FIELD_INTERLACED_TB)
			use_ipc = 1;

	if (use_ipc) {
		fimc_outdev_calibrate_scale_info(ctrl, &src, &dst);
		ret = ipc_init(dst.width, dst.height/2, IPC_2D);
		if (ret < 0)
			return ret;
	} 
#endif

	return 0;
}

int fimc_fimd_rect(const struct fimc_control *ctrl,
		struct v4l2_rect *fimd_rect)
{
	switch (ctrl->out->rotate) {
	case 0:
		fimd_rect->left = ctrl->out->win.w.left;
		fimd_rect->top = ctrl->out->win.w.top;
		fimd_rect->width = ctrl->out->win.w.width;
		fimd_rect->height = ctrl->out->win.w.height;

		break;

	case 90:
		fimd_rect->left = ctrl->fb.lcd_hres -
					(ctrl->out->win.w.top \
						+ ctrl->out->win.w.height);
		fimd_rect->top = ctrl->out->win.w.left;
		fimd_rect->width = ctrl->out->win.w.height;
		fimd_rect->height = ctrl->out->win.w.width;

		break;

	case 180:
		fimd_rect->left = ctrl->fb.lcd_hres -
					(ctrl->out->win.w.left \
						+ ctrl->out->win.w.width);
		fimd_rect->top = ctrl->fb.lcd_vres -
					(ctrl->out->win.w.top \
						+ ctrl->out->win.w.height);
		fimd_rect->width = ctrl->out->win.w.width;
		fimd_rect->height = ctrl->out->win.w.height;

		break;

	case 270:
		fimd_rect->left = ctrl->out->win.w.top;
		fimd_rect->top = ctrl->fb.lcd_vres -
					(ctrl->out->win.w.left \
						+ ctrl->out->win.w.width);
		fimd_rect->width = ctrl->out->win.w.height;
		fimd_rect->height = ctrl->out->win.w.width;

		break;

	default:
		fimc_err("Rotation degree is invalid\n");
		return -EINVAL;

		break;
	}

	return 0;
}

int fimc_start_fifo(struct fimc_control *ctrl)
{
	struct v4l2_rect fimd_rect;
	struct fb_var_screeninfo var;
	struct s3cfb_user_window window;
	int ret = -1;
	u32 id = ctrl->id;

	memset(&fimd_rect, 0, sizeof(struct v4l2_rect));
	ret = fimc_fimd_rect(ctrl, &fimd_rect);
	if (ret < 0) {
		fimc_err("fimc_fimd_rect fail\n");
		return -EINVAL;
	}

	/* Get WIN var_screeninfo  */
	ret = s3cfb_direct_ioctl(id, FBIOGET_VSCREENINFO, (unsigned long)&var);
	if (ret < 0) {
		fimc_err("direct_ioctl(FBIOGET_VSCREENINFO) fail\n");
		return -EINVAL;
	}

        /* Don't allocate the memory. */
        if (ctrl->out->pix.field == V4L2_FIELD_NONE)
                ret = s3cfb_direct_ioctl(id, S3CFB_SET_WIN_PATH, DATA_PATH_FIFO);
        else if (ctrl->out->pix.field == V4L2_FIELD_INTERLACED_TB)
                ret = s3cfb_direct_ioctl(id, S3CFB_SET_WIN_PATH, DATA_PATH_IPC);
        if (ret < 0) {
                fimc_err("direct_ioctl(S3CFB_SET_WIN_MEM) fail\n");
                return -EINVAL;
        }

	ret = s3cfb_direct_ioctl(id, S3CFB_SET_WIN_MEM, DMA_MEM_NONE);
	if (ret < 0) {
		fimc_err("direct_ioctl(S3CFB_SET_WIN_MEM) fail\n");
		return -EINVAL;
	}

	ret = s3cfb_direct_ioctl(id, S3CFB_SET_WIN_ADDR, 0x00000000);
	if (ret < 0) {
		fimc_err("direct_ioctl(S3CFB_SET_WIN_ADDR) fail\n");
		return -EINVAL;
	}

	/* Update WIN size  */
	var.xres_virtual = fimd_rect.width;
	var.yres_virtual = fimd_rect.height;
	var.xres = fimd_rect.width;
	var.yres = fimd_rect.height;
	ret = s3cfb_direct_ioctl(id, FBIOPUT_VSCREENINFO, (unsigned long)&var);
	if (ret < 0) {
		fimc_err("direct_ioctl(FBIOPUT_VSCREENINFO) fail\n");
		return -EINVAL;
	}

	/* Update WIN position */
	window.x = fimd_rect.left;
	window.y = fimd_rect.top;
	ret = s3cfb_direct_ioctl(id, S3CFB_WIN_POSITION,
			(unsigned long)&window);
	if (ret < 0) {
		fimc_err("direct_ioctl(S3CFB_WIN_POSITION) fail\n");
		return -EINVAL;
	}

	/* Open WIN FIFO */
	ret = ctrl->fb.open_fifo(id, 0, fimc_outdev_start_camif, (void *)ctrl);
	if (ret < 0) {
		fimc_err("FIMD FIFO close fail\n");
		return -EINVAL;
	}

	return 0;
}

int fimc_outdev_overlay_buf(struct file *filp, struct fimc_control *ctrl)
{
	int ret = 0, i;
	struct fimc_overlay_buf * buf;
	buf = &ctrl->out->overlay.buf;

	for (i = 0; i < FIMC_OUTBUFS; i++) {
		ctrl->out->overlay.req_idx = i;
		buf->size[i] = ctrl->out->dst[i].length[0];
		buf->phy_addr[i] = ctrl->out->dst[i].base[0];
		buf->vir_addr[i] = do_mmap(filp, 0, buf->size[i], PROT_READ|PROT_WRITE, MAP_SHARED, 0);
		if (buf->vir_addr[i] == -EINVAL) {
			fimc_err("fimc_outdev_overlay_buf fail\n");
			return -EINVAL;
		}

		fimc_dbg("idx : %d, size(0x%08x), phy_addr(0x%08x), "
				"vir_addr(0x%08x)\n", i, buf->size[i], 
				buf->phy_addr[i], buf->vir_addr[i]);
	}
	
	ctrl->out->overlay.req_idx = -1;

	return ret;
}

int fimc_reqbufs_output(void *fh, struct v4l2_requestbuffers *b)
{
	struct fimc_control *ctrl = (struct fimc_control *) fh;
	struct fimc_overlay_buf *buf = &ctrl->out->overlay.buf;
	enum fimc_overlay_mode mode = ctrl->out->overlay.mode;
	struct mm_struct *mm = current->mm;
	int ret = -1, i;

	fimc_info1("%s: called\n", __func__);

	if (ctrl->status != FIMC_STREAMOFF) {
		fimc_err("FIMC is running\n");
		return -EBUSY;
	}

	if (ctrl->out->is_requested == 1 && b->count != 0) {
		fimc_err("Buffers were already requested\n");
		return -EBUSY;
	}

	if (b->count > FIMC_OUTBUFS) {
		fimc_warn("The buffer count is modified by driver \
				from %d to %d\n", b->count, FIMC_OUTBUFS);
		b->count = FIMC_OUTBUFS;
	}

	fimc_init_out_buf(ctrl);
	ctrl->out->is_requested = 0;
	
	if (b->count == 0) {
		ctrl->mem.curr = ctrl->mem.base;
		
		for (i = 0; i < FIMC_OUTBUFS; i++) {
			if (buf->vir_addr[i]) {
				ret = do_munmap(mm, buf->vir_addr[i], buf->size[i]);
				if (ret < 0)
					fimc_err("%s: do_munmap fail. vir_addr[%d](0x%08x)\n", __func__, i, buf->vir_addr[i]);
			}
		}
	} else {
		/* initialize source buffers */
		if (b->memory == V4L2_MEMORY_MMAP) {
			ret = fimc_outdev_set_src_buf(ctrl);
			if (ret)
				return ret;
		}

		/* initialize destination buffers */
		if ((mode == FIMC_OVERLAY_DMA_MANUAL) || (mode == FIMC_OVERLAY_DMA_AUTO)) {
			ret = fimc_outdev_set_dst_buf(ctrl);
			if (ret)
				return ret;
		}

		ctrl->out->is_requested = 1;
	}

	ctrl->out->buf_num = b->count;

	return 0;
}

int fimc_querybuf_output(void *fh, struct v4l2_buffer *b)
{
	struct fimc_control *ctrl = (struct fimc_control *) fh;
	u32 buf_length = 0;

	fimc_info1("%s: called\n", __func__);

	if (ctrl->status != FIMC_STREAMOFF) {
		fimc_err("FIMC is running\n");
		return -EBUSY;
	}

	if (b->index > ctrl->out->buf_num) {
		fimc_err("The index is out of bounds. \n \
			You requested %d buffers. \
			But you set the index as %d\n",
			ctrl->out->buf_num, b->index);
		return -EINVAL;
	}

	b->flags = ctrl->out->src[b->index].flags;
	b->m.offset = b->index * PAGE_SIZE;
	buf_length = ctrl->out->src[b->index].length[FIMC_ADDR_Y] + \
			ctrl->out->src[b->index].length[FIMC_ADDR_CB] + \
			ctrl->out->src[b->index].length[FIMC_ADDR_CR];
 	b->length = buf_length;

	return 0;
}

int fimc_g_ctrl_output(void *fh, struct v4l2_control *c)
{
	struct fimc_control *ctrl = (struct fimc_control *) fh;
	struct s3c_platform_fimc *pdata = to_fimc_plat(ctrl->dev);

	if (ctrl->status != FIMC_STREAMOFF) {
		fimc_err("FIMC is running\n");
		return -EBUSY;
	}

	switch (c->id) {
	case V4L2_CID_ROTATION:
		c->value = ctrl->out->rotate;
		break;

#if 1
	// added by jamie (2009.08.25)
	case V4L2_CID_RESERVED_MEM_BASE_ADDR:
		c->value = ctrl->mem.base;
		break;

	case V4L2_CID_FIMC_VERSION:
		c->value = pdata->hw_ver;
		break;
#endif

	case V4L2_CID_HFLIP:
		if (ctrl->out->flip & V4L2_CID_HFLIP)
			c->value = 1;
		else
			c->value = 0;
		break;

	case V4L2_CID_VFLIP:
		if (ctrl->out->flip & V4L2_CID_VFLIP)
			c->value = 1;
		else
			c->value = 0;
		break;

	case V4L2_CID_OVERLAY_VADDR0:
		c->value = ctrl->out->overlay.buf.vir_addr[0];
		break;

	case V4L2_CID_OVERLAY_VADDR1:
		c->value = ctrl->out->overlay.buf.vir_addr[1];
		break;

	case V4L2_CID_OVERLAY_VADDR2:
		c->value = ctrl->out->overlay.buf.vir_addr[2];
		break;

	case V4L2_CID_OVERLAY_AUTO:
		if (ctrl->out->overlay.mode == FIMC_OVERLAY_DMA_AUTO)
			c->value = 1;
		else
			c->value = 0;
		break;

	default:
		fimc_err("Invalid control id: %d\n", c->id);
		return -EINVAL;
	}

	return 0;
}

int fimc_s_ctrl_output(struct file *filp, void *fh, struct v4l2_control *c)
{
	struct fimc_control *ctrl = (struct fimc_control *) fh;
	int ret = 0;

	if (ctrl->status != FIMC_STREAMOFF) {
		fimc_err("FIMC is running\n");
		return -EBUSY;
	}

	switch (c->id) {
	case V4L2_CID_ROTATION:
		ret = fimc_set_rot_degree(ctrl, c->value);
		break;

	case V4L2_CID_HFLIP:
		if (c->value)
			ctrl->out->flip = V4L2_CID_HFLIP;
		else
			ctrl->out->flip &= ~V4L2_CID_HFLIP;
		break;

	case V4L2_CID_VFLIP:
		if (c->value)
			ctrl->out->flip = V4L2_CID_VFLIP;
		else
			ctrl->out->flip &= ~V4L2_CID_VFLIP;

		break;

	case V4L2_CID_OVERLAY_AUTO:
		if (c->value == 1) {
			ctrl->out->overlay.mode = FIMC_OVERLAY_DMA_AUTO;
		} else {
			ctrl->out->overlay.mode = FIMC_OVERLAY_DMA_MANUAL;
			fimc_outdev_overlay_buf(filp, ctrl);
		}
		
		break;

	default:
		fimc_err("Invalid control id: %d\n", c->id);
		ret = -EINVAL;
	}

	return ret;
}

int fimc_cropcap_output(void *fh, struct v4l2_cropcap *a)
{
	struct fimc_control *ctrl = (struct fimc_control *) fh;
	struct fimc_outinfo *out = ctrl->out;
	u32 pixelformat = ctrl->out->pix.pixelformat;
	u32 is_rotate = 0;
	u32 max_w = 0, max_h = 0;

	fimc_info1("%s: called\n", __func__);

	if (ctrl->status != FIMC_STREAMOFF) {
		fimc_err("FIMC is running\n");
		return -EBUSY;
	}

	is_rotate = fimc_mapping_rot_flip(ctrl->out->rotate, ctrl->out->flip);

	printk("fimc_cropcap_output : pixelformat = 0x%x\n", pixelformat);

	switch (pixelformat) {
	case V4L2_PIX_FMT_NV12:		/* fall through */
	case V4L2_PIX_FMT_NV12T:	/* fall through */
	case V4L2_PIX_FMT_YUYV:		/* fall through */
		max_w = FIMC_SRC_MAX_W;
		max_h = FIMC_SRC_MAX_H;		
	case V4L2_PIX_FMT_RGB32:	/* fall through */
	case V4L2_PIX_FMT_RGB565:	/* fall through */
		if (is_rotate & FIMC_ROT) {		/* Landscape mode */
			max_w = ctrl->fb.lcd_vres;
			max_h = ctrl->fb.lcd_hres;
		} else {				/* Portrait */
			max_w = ctrl->fb.lcd_hres;
			max_h = ctrl->fb.lcd_vres;
		}

		break;
	default: 
		fimc_warn("Supported format : V4L2_PIX_FMT_YUYV, V4L2_PIX_FMT_NV12, V4L2_PIX_FMT_NV12T, V4L2_PIX_FMT_RGB32, V4L2_PIX_FMT_RGB565\n");
		return -EINVAL;
	}

	/* crop bounds */
	out->cropcap.bounds.left = 0;
	out->cropcap.bounds.top = 0;
	out->cropcap.bounds.width = max_w;
	out->cropcap.bounds.height = max_h;

	/* crop default values */
	out->cropcap.defrect.left = 0;
	out->cropcap.defrect.top = 0;
	out->cropcap.defrect.width = max_w;
	out->cropcap.defrect.height = max_h;

	/* crop pixel aspec values */
	/* To Do : Have to modify but I don't know the meaning. */
	out->cropcap.pixelaspect.numerator = 16;
	out->cropcap.pixelaspect.denominator = 9;

	a->bounds = out->cropcap.bounds;
	a->defrect = out->cropcap.defrect;
	a->pixelaspect = out->cropcap.pixelaspect;

	return 0;
}

int fimc_s_crop_output(void *fh, struct v4l2_crop *a)
{
	struct fimc_control *ctrl = (struct fimc_control *) fh;

	fimc_info1("%s: called: left(%d), top(%d), width(%d), height(%d), \n",
		__func__, a->c.left, a->c.top, a->c.width, a->c.height);

	if (ctrl->status != FIMC_STREAMOFF) {
		fimc_err("FIMC is running\n");
		return -EBUSY;
	}

	/* Check arguments : widht and height */
	if ((a->c.width < 0) || (a->c.height < 0)) {
		fimc_err("The crop rect must be bigger than 0\n");
		return -EINVAL;
	}

	if ((a->c.width > FIMC_SRC_MAX_W) || (a->c.height > FIMC_SRC_MAX_H)) {
		fimc_err("The crop width/height must be smaller than \
				%d and %d\n", FIMC_SRC_MAX_W, FIMC_SRC_MAX_H);
		return -EINVAL;
	}

	/* Check arguments : left and top */
	if ((a->c.left < 0) || (a->c.top < 0)) {
		fimc_err("The crop rect left and top must be \
				bigger than zero\n");
		return -EINVAL;
	}

	if ((a->c.left > FIMC_SRC_MAX_W) || (a->c.top > FIMC_SRC_MAX_H)) {
		fimc_err("The crop left/top must be smaller than \
				%d, %d\n", FIMC_SRC_MAX_W, FIMC_SRC_MAX_H);
		return -EINVAL;
	}

	if ((a->c.left + a->c.width) > FIMC_SRC_MAX_W) {
		fimc_err("The crop rect must be in bound rect\n");
		return -EINVAL;
	}

	if ((a->c.top + a->c.height) > FIMC_SRC_MAX_H) {
		fimc_err("The crop rect must be in bound rect\n");
		return -EINVAL;
	}

	ctrl->out->crop.left = a->c.left;
	ctrl->out->crop.top = a->c.top;
	ctrl->out->crop.width = a->c.width;
	ctrl->out->crop.height = a->c.height;

	return 0;
}

int fimc_streamon_output(void *fh)
{
	struct fimc_control *ctrl = (struct fimc_control *) fh;
	int ret = -1;

	fimc_info1("%s: called\n", __func__);

	ret = fimc_outdev_check_param(ctrl);
	if (ret < 0) {
		fimc_err("Fail: fimc_check_param\n");
		return ret;
	}

	ret = fimc_outdev_set_param(ctrl);
	if (ret < 0) {
		fimc_err("Fail: fimc_outdev_set_param\n");
		return ret;
	}

	ctrl->status = FIMC_READY_ON;

	return ret;
}

int fimc_streamoff_output(void *fh)
{
	struct fimc_control *ctrl = (struct fimc_control *) fh;
	u32 i = 0;
	int ret = -1;

	fimc_info1("%s: called\n", __func__);

	ret = fimc_outdev_stop_streaming(ctrl);
	if (ret < 0) {
		fimc_err("Fail: fimc_outdev_stop_streaming\n");
		return -EINVAL;
	}

	ret = fimc_init_in_queue(ctrl);
	if (ret < 0) {
		fimc_err("Fail: fimc_init_in_queue\n");
		return -EINVAL;
	}

	ret = fimc_init_out_queue(ctrl);
	if (ret < 0) {
		fimc_err("Fail: fimc_init_out_queue\n");
		return -EINVAL;
	}

	/* Make all buffers DQUEUED state. */
	for (i = 0; i < FIMC_OUTBUFS; i++) {
		ctrl->out->src[i].state	= VIDEOBUF_IDLE;
		ctrl->out->src[i].flags = V4L2_BUF_FLAG_MAPPED;
	}

	ctrl->out->idx.prev = -1;
	ctrl->out->idx.active = -1;
	ctrl->out->idx.next = -1;

	ctrl->status = FIMC_STREAMOFF;

	return 0;
}

static int fimc_qbuf_output_none(struct fimc_control *ctrl)
{
	struct fimc_buf_set buf_set;
	u32 index = 0;
	int ret = -1;
	u32 i = 0;

	if ((ctrl->status == FIMC_READY_ON) || \
		(ctrl->status == FIMC_STREAMON_IDLE)) {
		ret =  fimc_detach_in_queue(ctrl, &index);
		if (ret < 0) {
			fimc_err("Fail: fimc_detach_in_queue\n");
			return -EINVAL;
		}

		fimc_outdev_set_src_addr(ctrl, ctrl->out->src[index].base);

		memset(&buf_set, 0x00, sizeof(buf_set));
		buf_set.base[FIMC_ADDR_Y] = (dma_addr_t)ctrl->out->fbuf.base;

		for (i = 0; i < FIMC_PHYBUFS; i++)
			fimc_hwset_output_address(ctrl, &buf_set, i);

		ret = fimc_outdev_start_camif(ctrl);
		if (ret < 0) {
			fimc_err("Fail: fimc_start_camif\n");
			return -EINVAL;
		}

		ctrl->out->idx.active = index;
		ctrl->status = FIMC_STREAMON;
	}

	return 0;
}

static int fimc_qbuf_output_dma_auto(struct fimc_control *ctrl)
{
	struct fb_var_screeninfo var;
	struct s3cfb_user_window window;
	struct v4l2_rect fimd_rect;
	struct fimc_buf_set buf_set;

	u32 index = 0, i = 0;
	u32 id = ctrl->id;
	int ret = -1;

	switch (ctrl->status) {
	case FIMC_READY_ON:
		memset(&fimd_rect, 0, sizeof(struct v4l2_rect));
		ret = fimc_fimd_rect(ctrl, &fimd_rect);
		if (ret < 0) {
			fimc_err("fimc_fimd_rect fail\n");
			return -EINVAL;
		}

		/* Get WIN var_screeninfo  */
		ret = s3cfb_direct_ioctl(id, FBIOGET_VSCREENINFO, (unsigned long)&var);
		if (ret < 0) {
			fimc_err("direct_ioctl(FBIOGET_VSCREENINFO) fail\n");
			return -EINVAL;
		}

	        /* window path : DMA */
		ret = s3cfb_direct_ioctl(id, S3CFB_SET_WIN_PATH, DATA_PATH_DMA);
		if (ret < 0) {
			fimc_err("direct_ioctl(S3CFB_SET_WIN_PATH) fail\n");
			return -EINVAL;
		}

	        /* Don't allocate the memory. */
		ret = s3cfb_direct_ioctl(id, S3CFB_SET_WIN_MEM, DMA_MEM_OTHER);
		if (ret < 0) {
			fimc_err("direct_ioctl(S3CFB_SET_WIN_MEM) fail\n");
			return -EINVAL;
		}

		/* Update WIN size  */
		var.xres_virtual = fimd_rect.width;
		var.yres_virtual = fimd_rect.height;
		var.xres = fimd_rect.width;
		var.yres = fimd_rect.height;

		ret = s3cfb_direct_ioctl(id, FBIOPUT_VSCREENINFO, (unsigned long)&var);
		if (ret < 0) {
			fimc_err("direct_ioctl(FBIOPUT_VSCREENINFO) fail\n");
			return -EINVAL;
		}

		/* Update WIN position */
		window.x = fimd_rect.left;
		window.y = fimd_rect.top;
		ret = s3cfb_direct_ioctl(id, S3CFB_WIN_POSITION,
				(unsigned long)&window);
		if (ret < 0) {
			fimc_err("direct_ioctl(S3CFB_WIN_POSITION) fail\n");
			return -EINVAL;
		}

		/* fall through */

	case FIMC_STREAMON_IDLE:
		ret =  fimc_detach_in_queue(ctrl, &index);
		if (ret < 0) {
			fimc_err("Fail: fimc_detach_in_queue\n");
			return -EINVAL;
		}

		fimc_outdev_set_src_addr(ctrl, ctrl->out->src[index].base);

		memset(&buf_set, 0x00, sizeof(buf_set));
		buf_set.base[FIMC_ADDR_Y] = ctrl->out->dst[index].base[FIMC_ADDR_Y];

		for (i = 0; i < FIMC_PHYBUFS; i++)
			fimc_hwset_output_address(ctrl, &buf_set, i);

		ret = fimc_outdev_start_camif(ctrl);
		if (ret < 0) {
			fimc_err("Fail: fimc_start_camif\n");
			return -EINVAL;
		}

		ctrl->out->idx.active = index;
		ctrl->status = FIMC_STREAMON;

		break;

	default:
		break;
	}

	return 0;
}

static int fimc_qbuf_output_dma_manual(struct fimc_control *ctrl)
{
	struct fimc_buf_set buf_set;
	u32 index = 0, i = 0;
	int ret = -1;

	switch (ctrl->status) {
	case FIMC_READY_ON:		/* fall through */
	case FIMC_STREAMON_IDLE:
		ret =  fimc_detach_in_queue(ctrl, &index);
		if (ret < 0) {
			fimc_err("Fail: fimc_detach_in_queue\n");
			return -EINVAL;
		}

		fimc_outdev_set_src_addr(ctrl, ctrl->out->src[index].base);

		memset(&buf_set, 0x00, sizeof(buf_set));
		buf_set.base[FIMC_ADDR_Y] = ctrl->out->dst[index].base[FIMC_ADDR_Y];

		for (i = 0; i < FIMC_PHYBUFS; i++)
			fimc_hwset_output_address(ctrl, &buf_set, i);

		ret = fimc_outdev_start_camif(ctrl);
		if (ret < 0) {
			fimc_err("Fail: fimc_start_camif\n");
			return -EINVAL;
		}

		ctrl->out->idx.active = index;
		ctrl->status = FIMC_STREAMON;

		break;

	default:
		break;
	}

	return 0;
}

static int fimc_qbuf_output_fifo(struct fimc_control *ctrl)
{
	u32 index = 0;
	int ret = -1;

	if (ctrl->status == FIMC_READY_ON) {
		ret =  fimc_detach_in_queue(ctrl, &index);
		if (ret < 0) {
			fimc_err("Fail: fimc_detach_in_queue\n");
			return -EINVAL;
		}

#if defined(CONFIG_VIDEO_IPC)
		if (ctrl->out->pix.field == V4L2_FIELD_INTERLACED_TB)
			ipc_start();
#endif

		fimc_outdev_set_src_addr(ctrl, ctrl->out->src[index].base);

		ret = fimc_start_fifo(ctrl);
		if (ret < 0) {
			fimc_err("Fail: fimc_start_fifo\n");
			return -EINVAL;
		}

		ctrl->out->idx.active = index;
		ctrl->status = FIMC_STREAMON;
	}

	return 0;
}

static int fimc_update_in_queue_addr(struct fimc_control *ctrl, u32 index, dma_addr_t *addr)
{
	if (index >= FIMC_OUTBUFS) {
		fimc_err("%s: Failed \n", __func__);
		return -EINVAL;
	}
	
	ctrl->out->src[index].base[FIMC_ADDR_Y] = addr[FIMC_ADDR_Y];
	ctrl->out->src[index].base[FIMC_ADDR_CB] = addr[FIMC_ADDR_CB];
	ctrl->out->src[index].base[FIMC_ADDR_CR] = addr[FIMC_ADDR_CR];

	return 0;
}

int fimc_qbuf_output(void *fh, struct v4l2_buffer *b)
{
	struct fimc_control *ctrl = (struct fimc_control *) fh;
	struct fimc_buf *buf = (struct fimc_buf *)b->m.userptr;
	int ret = -1;

	fimc_info2("%s: queued idx = %d\n", __func__, b->index);

	if (b->index > ctrl->out->buf_num) {
		fimc_err("The index is out of bounds" 
			"You requested %d buffers. "
			"But you set the index as %d\n",
			ctrl->out->buf_num, b->index);
		return -EINVAL;
	}

	/* Check the buffer state if the state is VIDEOBUF_IDLE. */
	if (ctrl->out->src[b->index].state != VIDEOBUF_IDLE) {
		fimc_err("The index(%d) buffer must be dequeued state(%d)\n", 
				 b->index, ctrl->out->src[b->index].state);
		return -EINVAL;
	}

	if (b->memory == V4L2_MEMORY_USERPTR) {
		ret = fimc_update_in_queue_addr(ctrl, b->index, buf->base);
		if (ret < 0)
			return ret;
	}

	/* Attach the buffer to the incoming queue. */
	ret =  fimc_attach_in_queue(ctrl, b->index);
	if (ret < 0) {
		fimc_err("Fail: fimc_attach_in_queue\n");
		return -EINVAL;
	}

	switch (ctrl->out->overlay.mode) {
	case FIMC_OVERLAY_NONE:
		ret = fimc_qbuf_output_none(ctrl);

		break;
	case FIMC_OVERLAY_DMA_MANUAL:
		ret = fimc_qbuf_output_dma_manual(ctrl);

		break;

	case FIMC_OVERLAY_DMA_AUTO:
		ret = fimc_qbuf_output_dma_auto(ctrl);

		break;
	case FIMC_OVERLAY_FIFO:
		ret = fimc_qbuf_output_fifo(ctrl);
		break;
	}

	return ret;
}

int fimc_break(void)
{
	int i = 0;
	i++;

	return 0;
}
int fimc_dqbuf_output(void *fh, struct v4l2_buffer *b)
{
	struct fimc_control *ctrl = (struct fimc_control *) fh;
	int index = -1, ret = -1;

	ret = fimc_detach_out_queue(ctrl, &index);
	if (ret < 0) {
		ret = wait_event_timeout(ctrl->wq, \
			(ctrl->out->out_queue[0] != -1), FIMC_DQUEUE_TIMEOUT);
		if (ret == 0) {
			fimc_break();
			fimc_dump_context(ctrl);
			fimc_err("[0] out_queue is empty\n");
			return -EINVAL;
		} else if (ret == -ERESTARTSYS) {
			fimc_print_signal(ctrl);
		} else {
			/* Normal case */
			ret = fimc_detach_out_queue(ctrl, &index);
			if (ret < 0) {
				fimc_err("[1] out_queue is empty\n");
				fimc_dump_context(ctrl);
				return -EINVAL;
			}
		}
	}

	b->index = index;

	fimc_info2("%s: dqueued idx = %d\n", __func__, b->index);

	return ret;
}

int fimc_g_fmt_vid_out(struct file *filp, void *fh, struct v4l2_format *f)
{
	struct fimc_control *ctrl = (struct fimc_control *) fh;
	struct fimc_outinfo *out = ctrl->out;

	fimc_info1("%s: called\n", __func__);

	if (!out) {
		out = kzalloc(sizeof(*out), GFP_KERNEL);
		if (!out) {
			fimc_err("%s: no memory for output device info\n", 
								__func__);
			return -ENOMEM;
		}

		ctrl->out = out;

		ctrl->out->is_requested = 0;
		ctrl->out->rotate = 0;
		ctrl->out->flip	= 0;
		ctrl->out->overlay.mode = FIMC_OVERLAY_MODE;

		ctrl->out->idx.prev = -1;
		ctrl->out->idx.active = -1;
		ctrl->out->idx.next = -1;
	}

	f->fmt.pix = ctrl->out->pix;

	return 0;
}

int fimc_try_fmt_vid_out(struct file *filp, void *fh, struct v4l2_format *f)
{
	struct fimc_control *ctrl = (struct fimc_control *) fh;
	u32 format = f->fmt.pix.pixelformat;

	fimc_info1("%s: called. width(%d), height(%d)\n", \
			__func__, f->fmt.pix.width, f->fmt.pix.height);

	if (ctrl->status != FIMC_STREAMOFF) {
		fimc_err("FIMC is running\n");
		return -EBUSY;
	}

	/*
	printk("fimc_try_fmt_vid_out : format             = 0x%x\n", format);
	printk("fimc_try_fmt_vid_out : V4L2_PIX_FMT_NV12  = 0x%x\n", V4L2_PIX_FMT_NV12);
	printk("fimc_try_fmt_vid_out : V4L2_PIX_FMT_NV12T = 0x%x\n", V4L2_PIX_FMT_NV12T);
	printk("fimc_try_fmt_vid_out : V4L2_PIX_FMT_YUYV  = 0x%x\n", V4L2_PIX_FMT_YUYV);
	printk("fimc_try_fmt_vid_out : V4L2_PIX_FMT_RGB32 = 0x%x\n", V4L2_PIX_FMT_RGB32);
	printk("fimc_try_fmt_vid_out : V4L2_PIX_FMT_RGB565= 0x%x\n", V4L2_PIX_FMT_RGB565);
	*/

	/* Check pixel format */
	switch (format) {
	case V4L2_PIX_FMT_NV12:		/* fall through */
	case V4L2_PIX_FMT_NV12T:	/* fall through */
	case V4L2_PIX_FMT_YUYV:		/* fall through */
	case V4L2_PIX_FMT_RGB32:	/* fall through */
	case V4L2_PIX_FMT_RGB565:	/* fall through */
		break;
	default: 
		fimc_warn("Supported format : V4L2_PIX_FMT_YUYV, V4L2_PIX_FMT_NV12, V4L2_PIX_FMT_NV12T, V4L2_PIX_FMT_RGB32, V4L2_PIX_FMT_RGB565\n");
		fimc_warn("Changed format : V4L2_PIX_FMT_RGB32\n");
		f->fmt.pix.pixelformat = V4L2_PIX_FMT_RGB32;
		return -EINVAL;
	}

	/* Fill the return value. */
	switch (format) {
	case V4L2_PIX_FMT_RGB32:
		f->fmt.pix.bytesperline	= f->fmt.pix.width<<2;
		break;
	case V4L2_PIX_FMT_YUYV:		/* fall through */
	case V4L2_PIX_FMT_RGB565:	/* fall through */
		f->fmt.pix.bytesperline	= f->fmt.pix.width<<1;		
		break;
	case V4L2_PIX_FMT_NV12:		/* fall through */
	case V4L2_PIX_FMT_NV12T:
		f->fmt.pix.bytesperline	= (f->fmt.pix.width * 3)>>1;
		break;

	default: 
		/* dummy value*/
		f->fmt.pix.bytesperline	= f->fmt.pix.width;
	}
	
	f->fmt.pix.sizeimage = f->fmt.pix.bytesperline * f->fmt.pix.height;

	return 0;
}

int fimc_s_fmt_vid_out(struct file *filp, void *fh, struct v4l2_format *f)
{
	struct fimc_control *ctrl = (struct fimc_control *) fh;
	int ret = -1;

	fimc_info1("%s: called\n", __func__);

	/* Check stream status */
	if (ctrl->status != FIMC_STREAMOFF) {
		fimc_err("FIMC is running\n");
		return -EBUSY;
	}

	ret = fimc_try_fmt_vid_out(filp, fh, f);
	if (ret < 0)
		return ret;

	ctrl->out->pix = f->fmt.pix;

	return ret;
}

int fimc_init_in_queue(struct fimc_control *ctrl)
{
	unsigned long	spin_flags;
	unsigned int	i;

	spin_lock_irqsave(&ctrl->lock_in, spin_flags);

	/* Init incoming queue */
	for (i = 0; i < FIMC_OUTBUFS; i++)
		ctrl->out->in_queue[i] = -1;

	spin_unlock_irqrestore(&ctrl->lock_in, spin_flags);

	return 0;
}

int fimc_init_out_queue(struct fimc_control *ctrl)
{
	unsigned long	spin_flags;
	unsigned int	i;

	spin_lock_irqsave(&ctrl->lock_out, spin_flags);

	/* Init incoming queue */
	for (i = 0; i < FIMC_OUTBUFS; i++)
		ctrl->out->out_queue[i] = -1;

	spin_unlock_irqrestore(&ctrl->lock_out, spin_flags);

	return 0;
}

int fimc_attach_in_queue(struct fimc_control *ctrl, u32 index)
{
	unsigned long		spin_flags;
	int			swap_queue[FIMC_OUTBUFS];
	int			i;

	fimc_dbg("%s: index = %d\n", __func__, index);

	spin_lock_irqsave(&ctrl->lock_in, spin_flags);

	/* Backup original queue */
	for (i = 0; i < FIMC_OUTBUFS; i++)
		swap_queue[i] = ctrl->out->in_queue[i];

	/* Attach new index */
	ctrl->out->in_queue[0] = index;
	ctrl->out->src[index].state = VIDEOBUF_QUEUED;
	ctrl->out->src[index].flags = V4L2_BUF_FLAG_MAPPED |
		V4L2_BUF_FLAG_QUEUED;

	/* Shift the origonal queue */
	for (i = 1; i < FIMC_OUTBUFS; i++)
		ctrl->out->in_queue[i] = swap_queue[i-1];

	spin_unlock_irqrestore(&ctrl->lock_in, spin_flags);

	return 0;
}

int fimc_detach_in_queue(struct fimc_control *ctrl, int *index)
{
	unsigned long		spin_flags;
	int			i, ret = 0;

	spin_lock_irqsave(&ctrl->lock_in, spin_flags);

	/* Find last valid index in incoming queue. */
	for (i = (FIMC_OUTBUFS-1); i >= 0; i--) {
		if (ctrl->out->in_queue[i] != -1) {
			*index = ctrl->out->in_queue[i];
			ctrl->out->in_queue[i] = -1;
			ctrl->out->src[*index].state = VIDEOBUF_ACTIVE;
			ctrl->out->src[*index].flags = V4L2_BUF_FLAG_MAPPED;
			break;
		}
	}

	/* incoming queue is empty. */
	if (i < 0)
		ret = -EINVAL;
	else
		fimc_dbg("%s: index = %d\n", __func__, *index);

	spin_unlock_irqrestore(&ctrl->lock_in, spin_flags);

	return ret;
}

int fimc_attach_out_queue(struct fimc_control *ctrl, u32 index)
{
	unsigned long		spin_flags;
	int			swap_queue[FIMC_OUTBUFS];
	int			i;

	fimc_dbg("%s: index = %d\n", __func__, index);

	spin_lock_irqsave(&ctrl->lock_out, spin_flags);

	/* Backup original queue */
	for (i = 0; i < FIMC_OUTBUFS; i++)
		swap_queue[i] = ctrl->out->out_queue[i];

	/* Attach new index */
	ctrl->out->out_queue[0]	= index;
	ctrl->out->src[index].state = VIDEOBUF_DONE;
	ctrl->out->src[index].flags = V4L2_BUF_FLAG_MAPPED | V4L2_BUF_FLAG_DONE;

	/* Shift the origonal queue */
	for (i = 1; i < FIMC_OUTBUFS; i++)
		ctrl->out->out_queue[i] = swap_queue[i-1];

	spin_unlock_irqrestore(&ctrl->lock_out, spin_flags);

	return 0;
}

int fimc_detach_out_queue(struct fimc_control *ctrl, int *index)
{
	unsigned long		spin_flags;
	int			i, ret = 0;

	spin_lock_irqsave(&ctrl->lock_out, spin_flags);

	/* Find last valid index in outgoing queue. */
	for (i = (FIMC_OUTBUFS-1); i >= 0; i--) {
		if (ctrl->out->out_queue[i] != -1) {
			*index = ctrl->out->out_queue[i];
			ctrl->out->out_queue[i] = -1;
			ctrl->out->src[*index].state = VIDEOBUF_IDLE;
			ctrl->out->src[*index].flags = V4L2_BUF_FLAG_MAPPED;
			break;
		}
	}

	/* outgoing queue is empty. */
	if (i < 0) {
		ret = -EINVAL;
		fimc_dbg("%s: outgoing queue : %d, %d, %d\n",
			__func__, ctrl->out->out_queue[0],
			ctrl->out->out_queue[1], ctrl->out->out_queue[2]);
	} else
		fimc_dbg("%s: index = %d\n", __func__, *index);


	spin_unlock_irqrestore(&ctrl->lock_out, spin_flags);

	return ret;
}

void fimc_dump_context(struct fimc_control *ctrl)
{
	u32 i = 0;

	for (i = 0; i < FIMC_OUTBUFS; i++) {
		fimc_err("in_queue[%d] : %d\n", i, \
				ctrl->out->in_queue[i]);
	}

	for (i = 0; i < FIMC_OUTBUFS; i++) {
		fimc_err("out_queue[%d] : %d\n", i, \
				ctrl->out->out_queue[i]);
	}

	fimc_err("state : prev = %d, active = %d, next = %d\n", \
		ctrl->out->idx.prev, ctrl->out->idx.active,
		ctrl->out->idx.next);
}

void fimc_print_signal(struct fimc_control *ctrl)
{
	if (signal_pending(current)) {
		fimc_dbg(".pend=%.8lx shpend=%.8lx\n",
			current->pending.signal.sig[0],
			current->signal->shared_pending.signal.sig[0]);
	} else {
		fimc_dbg(":pend=%.8lx shpend=%.8lx\n",
			current->pending.signal.sig[0],
			current->signal->shared_pending.signal.sig[0]);
	}
}

