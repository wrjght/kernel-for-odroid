/* linux/drivers/media/video/samsung/rp/s3c_rp_tv_regs.c
 *
 * Register interface file for Samsung S3C6410 Renderer pipeline driver 
 *
 * HyunMin Kwak, Copyright (c) 2009 Samsung Electronics
 * 	http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/fb.h>
#include <asm/io.h>
#include <mach/map.h>
#include <plat/gpio-cfg.h>
#include <plat/regs-lcd.h>
#include <plat/regs-tvscaler.h>

#include "s3c_rp.h"

void s3c_rp_tv_set_envid(struct s3c_rp_control *ctrl, unsigned int onoff)
{
	unsigned int regs = 0;

	regs = __raw_readl(ctrl->tv.regs + S3C_POSTENVID);

	regs &= ~S3C_POSTENVID_ENABLE;
	regs |= S3C_TV_POSTENVID(onoff);

	__raw_writel(regs, ctrl->tv.regs + S3C_POSTENVID);
}

int s3c_rp_tv_set_pixelformat(struct s3c_rp_control *ctrl)
{
	unsigned int regs = 0;
	unsigned int src_pixelformat = 0, dst_pixelformat = 0;

	regs = __raw_readl(ctrl->tv.regs + S3C_MODE);

	regs &= ~S3C_TV_MODE_NARROW;
	regs |= S3C_TV_MODE_WIDE;
	regs |= S3C_TV_MODE_R2YSEL_WIDE;

	regs &= ~S3C_TV_MODE_FORMAT_MASK;

	/* Set source pixel format */
	switch (ctrl->v4l2.video_out_fmt.pixelformat) {
	case V4L2_PIX_FMT_YUV420:
		src_pixelformat	|= (0x1<<8)|(0x1<<1);
		break;
	case V4L2_PIX_FMT_RGB32:
		src_pixelformat	|= (0x1<<3)|(0x1<<2)|(0x1<<1);
		break;
	default:
		rp_err(ctrl->log_level, "Invalid pixelformat !\n");
		return -EINVAL;
	}

	/* Set destination pixel format */
	dst_pixelformat = (0x1<<18)|(0x0<<13)|(0x1<<4);/* RGB24 via DMA */

	regs |= src_pixelformat;
	regs |= dst_pixelformat;	
	__raw_writel(regs, ctrl->tv.regs + S3C_MODE);	

	return 0;
}


int s3c_rp_tv_set_scaler(struct s3c_rp_control *ctrl)
{
	struct s3c_rp_scaler_cfg scaler_cfg;
	struct v4l2_rect src_rect, dst_rect;
	unsigned int	pre_ratio, pre_dst, src_size, dst_size;
	int 		ret = 0;

	src_rect.left	= 0;
	src_rect.top	= 0;
	dst_rect.left	= 0;
	dst_rect.top	= 0;

	src_rect.width	= ctrl->v4l2.video_out_fmt.width;
	src_rect.height	= ctrl->v4l2.video_out_fmt.height; 
	dst_rect.width	= ctrl->v4l2.video_overlay_fmt.w.width;
	dst_rect.height	= ctrl->v4l2.video_overlay_fmt.w.height; 

 	ret = s3c_rp_pp_query_scaler(ctrl, &src_rect, &dst_rect, &scaler_cfg);
	if (ret < 0) {
		rp_err(ctrl->log_level, "The renderer pipeline cannot support the video output argument which you set.\n");
		rp_err(ctrl->log_level, "Or the renderer pipeline cannot support the overlay argument which you set.\n");
		return -1;
	}

	pre_ratio	= S3C_TV_PRE_RATIO_V(scaler_cfg.prescale_v_ratio)	
				| S3C_TV_PRE_RATIO_H(scaler_cfg.prescale_h_ratio);
	pre_dst		= S3C_TV_PRE_DST_H(scaler_cfg.prescale_dst_height)	
				| S3C_TV_PRE_DST_W(scaler_cfg.prescale_dst_width);
	src_size	= S3C_TV_SRC_H(src_rect.height) 			
				| S3C_TV_SRC_W(src_rect.width);
	dst_size	= S3C_TV_DST_H(dst_rect.height)				
				| S3C_TV_DST_W(dst_rect.width);

	__raw_writel(pre_ratio,			ctrl->tv.regs + S3C_PRESCALE_RATIO);
	__raw_writel(pre_dst,			ctrl->tv.regs + S3C_PRESCALEIMGSIZE);
	__raw_writel(scaler_cfg.sh_factor,	ctrl->tv.regs + S3C_PRESCALE_SHFACTOR);
	__raw_writel(scaler_cfg.dx,		ctrl->tv.regs + S3C_MAINSCALE_H_RATIO);
	__raw_writel(scaler_cfg.dy,		ctrl->tv.regs + S3C_MAINSCALE_V_RATIO);
	__raw_writel(src_size,			ctrl->tv.regs + S3C_SRCIMGSIZE);
	__raw_writel(dst_size,			ctrl->tv.regs + S3C_DSTIMGSIZE);

	return 0;
}

void s3c_rp_tv_set_int_enable(struct s3c_rp_control *ctrl, unsigned int onoff)
{
	unsigned int regs = 0;

	regs = __raw_readl(ctrl->tv.regs + S3C_MODE);
	
	regs |= S3C_MODE_IRQ_LEVEL;
	regs &= ~S3C_MODE_TV_INT_ENABLE;
	regs |= S3C_TV_INT_ENABLE(onoff);

	__raw_writel(regs, ctrl->tv.regs + S3C_MODE);
}

void s3c_rp_tv_set_clock(struct s3c_rp_control *ctrl)
{
	unsigned int regs = 0;

	regs = __raw_readl(ctrl->tv.regs + S3C_MODE);

	regs &= ~(S3C_TV_MODE_CLKSEL_F_MASK | S3C_TV_MODE_CLKDIR_MASK | S3C_TV_MODE_CLKVAL_F_MASK);
	regs |= S3C_TV_CLKSEL_F(0x0);		/* set clock source : hclk */
	regs |= S3C_TV_CLKDIR(0x1);		/* set direction : CLKVAL_F */
	regs |= S3C_TV_CLKVAL_F(0x1);		/* set clock divide value : CLKVAL_F = 1 */

	__raw_writel(regs, ctrl->tv.regs + S3C_MODE);
}

static void s3c_rp_tv_make_addr(struct s3c_rp_control *ctrl, unsigned int index, struct s3c_rp_pp_addr *tv_addr)
{
	unsigned int	width, height, dst_width, dst_height;

	width		= ctrl->v4l2.video_out_fmt.width;
	height		= ctrl->v4l2.video_out_fmt.height;
	dst_width	= ctrl->v4l2.video_overlay_fmt.w.width;
	dst_height	= ctrl->v4l2.video_overlay_fmt.w.height;

	rp_info(ctrl->log_level, "s3c_rp_tv_make_addr : src w: %d, src h: %d, dst w: %d, dst h: %d\n", 
			width, height, dst_width, dst_height);
			
	tv_addr->phys_y_start	= ctrl->user_buf[index].buf_addr.phys_y;

	tv_addr->phys_cb_start	= tv_addr->phys_y_start		+ (width * height);
	tv_addr->phys_cr_start	= tv_addr->phys_cb_start	+ ((width * height)>>2);

	tv_addr->phys_y_offset	= 0;			/* Number of bytes */

	tv_addr->phys_cb_offset	= tv_addr->phys_y_offset / 2;
	tv_addr->phys_cr_offset	= tv_addr->phys_y_offset / 2;

	tv_addr->phys_y_end	= tv_addr->phys_y_start		
					+ ((width * height)	
						+ tv_addr->phys_y_offset * (height - 1));
	tv_addr->phys_cb_end	= tv_addr->phys_cb_start	
					+ (((width * height)>>2)	
						+ tv_addr->phys_cb_offset * (height/2 - 1));
	tv_addr->phys_cr_end	= tv_addr->phys_cr_start	
					+ (((width * height)>>2)	
						+ tv_addr->phys_cr_offset * (height/2 - 1));
}

void s3c_rp_tv_set_addr(struct s3c_rp_control *ctrl, unsigned int src_index, unsigned int dst_index)
{
	struct s3c_rp_pp_addr	tv_addr;
	unsigned int		width, height;
	unsigned int		phys_rgb_start, phys_rgb_end;

	/* Source */
	width = ctrl->v4l2.video_out_fmt.width;
	height = ctrl->v4l2.video_out_fmt.height;

	if (ctrl->v4l2.video_out_fmt.pixelformat == V4L2_PIX_FMT_YUV420) {
		s3c_rp_tv_make_addr(ctrl, src_index, &tv_addr);
		
		__raw_writel(tv_addr.phys_y_start,	ctrl->tv.regs + S3C_ADDRSTART_Y);
		__raw_writel(tv_addr.phys_cr_start,	ctrl->tv.regs + S3C_ADDRSTART_CR);
		__raw_writel(tv_addr.phys_cb_start,	ctrl->tv.regs + S3C_ADDRSTART_CB);

		__raw_writel(tv_addr.phys_y_end,	ctrl->tv.regs + S3C_ADDREND_Y);
		__raw_writel(tv_addr.phys_cb_end,	ctrl->tv.regs + S3C_ADDREND_CB);
		__raw_writel(tv_addr.phys_cr_end,	ctrl->tv.regs + S3C_ADDREND_CR);
		
		__raw_writel(tv_addr.phys_y_offset,	ctrl->tv.regs + S3C_OFFSET_Y);
		__raw_writel(tv_addr.phys_cb_offset,	ctrl->tv.regs + S3C_OFFSET_CB);
		__raw_writel(tv_addr.phys_cr_offset,	ctrl->tv.regs + S3C_OFFSET_CR);

	} else {
		phys_rgb_start	= ctrl->user_buf[src_index].buf_addr.phys_rgb;
	
		phys_rgb_end	= phys_rgb_start + ((width * height)<<2);

		__raw_writel(phys_rgb_start,	ctrl->tv.regs + S3C_ADDRSTART_Y);
		__raw_writel(phys_rgb_end,	ctrl->tv.regs + S3C_ADDREND_Y);
	}

	/* destination */
	width = ctrl->v4l2.video_overlay_fmt.w.width; 
	height = ctrl->v4l2.video_overlay_fmt.w.height;

	phys_rgb_start	= ctrl->cap_buf[dst_index].buf_addr.phys_rgb;
	phys_rgb_end	= phys_rgb_start + ((width * height)<<2);

	__raw_writel(phys_rgb_start,	ctrl->tv.regs + S3C_ADDRSTART_RGB);
	__raw_writel(phys_rgb_end,	ctrl->tv.regs + S3C_ADDREND_RGB);
}     

