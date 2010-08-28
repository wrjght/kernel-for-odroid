/* linux/drivers/media/video/samsung/rp/s3c_rp_cfg.c
 *
 * Configuration support file for Samsung Renderer pipeline driver
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
#include <linux/vmalloc.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <plat/media.h>
#include <plat/regs-lcd.h>

#include "s3c_rp.h"

int s3c_rp_check_buf(struct s3c_rp_control *ctrl, unsigned int num)
{
	int 		ret = 0;
	unsigned int	y_buf_size, cbcr_buf_size, rgb_buf_size, total_size = 0;

	if (ctrl->v4l2.video_out_fmt.pixelformat == V4L2_PIX_FMT_YUV420) {
		y_buf_size	= PAGE_ALIGN(S3C_RP_YUV_SRC_MAX_WIDTH * S3C_RP_YUV_SRC_MAX_HEIGHT);
		cbcr_buf_size	= PAGE_ALIGN((S3C_RP_YUV_SRC_MAX_WIDTH * S3C_RP_YUV_SRC_MAX_HEIGHT)>>2);
		total_size	= (y_buf_size + (cbcr_buf_size<<1)) * num;
	} else if (ctrl->v4l2.video_out_fmt.pixelformat == V4L2_PIX_FMT_RGB32) {
		rgb_buf_size	= PAGE_ALIGN(ctrl->fimd.h_res * ctrl->fimd.v_res * 4);
		total_size	= rgb_buf_size * num;
	} else {
		rp_err(ctrl->log_level, "[%s : %d]Invalid input\n", __FUNCTION__, __LINE__);
		ret = -1;
	}

	if (total_size > RP_RESERVED_MEM_SIZE)
		ret = -1;

	return ret;
}

int s3c_rp_init_buf(struct s3c_rp_control *ctrl)
{
	unsigned int ycbcr_buf_size, y_buf_size, cb_buf_size, rgb_buf_size;
	unsigned int i;

	ctrl->buf_info.reserved_mem_start = RP_RESERVED_MEM_ADDR_PHY;

	if (ctrl->v4l2.video_out_fmt.pixelformat == V4L2_PIX_FMT_YUV420) {	/* 720 x 576 YUV420 */
		y_buf_size	= PAGE_ALIGN(S3C_RP_YUV_SRC_MAX_WIDTH * S3C_RP_YUV_SRC_MAX_HEIGHT);
		cb_buf_size	= PAGE_ALIGN((S3C_RP_YUV_SRC_MAX_WIDTH * S3C_RP_YUV_SRC_MAX_HEIGHT)>>2);
		ycbcr_buf_size	= y_buf_size + cb_buf_size *2;		

		for (i = 0; i < S3C_RP_BUFF_NUM; i++) {
			/* Initialize User Buffer */		
			ctrl->user_buf[i].buf_addr.phys_y	= ctrl->buf_info.reserved_mem_start	+ ycbcr_buf_size * i;
			ctrl->user_buf[i].buf_addr.phys_cb	= ctrl->user_buf[i].buf_addr.phys_y	+ y_buf_size;
			ctrl->user_buf[i].buf_addr.phys_cr	= ctrl->user_buf[i].buf_addr.phys_cb	+ cb_buf_size;
			ctrl->user_buf[i].buf_state		= BUF_DONE;
			ctrl->user_buf[i].buf_flag		= 0x0;
			ctrl->user_buf[i].buf_length		= ycbcr_buf_size;
			atomic_set(&ctrl->user_buf[i].buf_maped, 0);

			/* Initialize Driver Buffer which means the rotator output buffer */
			ctrl->driver_buf[i].buf_addr.phys_y	= ctrl->buf_info.reserved_mem_start 	+ ycbcr_buf_size * S3C_RP_BUFF_NUM + ycbcr_buf_size * i;
			ctrl->driver_buf[i].buf_addr.phys_cb	= ctrl->driver_buf[i].buf_addr.phys_y	+ y_buf_size;
			ctrl->driver_buf[i].buf_addr.phys_cr	= ctrl->driver_buf[i].buf_addr.phys_cb	+ cb_buf_size;
			ctrl->driver_buf[i].buf_state		= BUF_DONE;
			ctrl->driver_buf[i].buf_flag		= 0x0;
			ctrl->driver_buf[i].buf_length		= ycbcr_buf_size;
			atomic_set(&ctrl->driver_buf[i].buf_maped, 0);

			ctrl->incoming_queue[i]			= -1;
			ctrl->inside_queue[i]			= -1;
			ctrl->outgoing_queue[i]			= -1;
		}
	} else {
		rgb_buf_size = PAGE_ALIGN((ctrl->fimd.h_res * ctrl->fimd.v_res)<<2);

		for (i = 0; i < S3C_RP_BUFF_NUM; i++) {
			/* Initialize User Buffer */
			ctrl->user_buf[i].buf_addr.phys_rgb	= ctrl->buf_info.reserved_mem_start	+ rgb_buf_size * i;
			ctrl->user_buf[i].buf_state		= BUF_DONE;
			ctrl->user_buf[i].buf_flag		= 0x0;
			ctrl->user_buf[i].buf_length		= rgb_buf_size;
			atomic_set(&ctrl->user_buf[i].buf_maped, 0);

			/* Initialize Driver Buffer which means the rotator output buffer */
			ctrl->driver_buf[i].buf_addr.phys_rgb	= ctrl->buf_info.reserved_mem_start	+ rgb_buf_size * S3C_RP_BUFF_NUM + rgb_buf_size * i;
			ctrl->driver_buf[i].buf_state		= BUF_DONE;
			ctrl->driver_buf[i].buf_flag		= 0x0;
			ctrl->driver_buf[i].buf_length		= rgb_buf_size;
			atomic_set(&ctrl->driver_buf[i].buf_maped, 0);

			ctrl->incoming_queue[i]			= -1;
			ctrl->inside_queue[i]			= -1;
			ctrl->outgoing_queue[i]			= -1;
		}
	}

	ctrl->buf_info.requested	= FALSE;

	return 0;
}

int s3c_rp_pp_query_scaler(struct s3c_rp_control *ctrl, const struct v4l2_rect *src_rect, const struct v4l2_rect *dst_rect, struct s3c_rp_scaler_cfg *scaler_cfg)
{
	struct s3c_rp_scaler_cfg cfg;
	unsigned int i;
	int size = sizeof(struct s3c_rp_scaler_cfg);	

	/* Check width */
	if (src_rect->width >= (dst_rect->width<<6)) {
		rp_err(ctrl->log_level, "Out of horizontal scale range.\n");
		return -1;
	}

	for (i = 5; i > 0; i--) {
		if (src_rect->width >= (dst_rect->width<<i)) {
			if (src_rect->width % (4*(1<<i)) == 0 ) {
				/* src_width = 4 * n * PreScale_H_Ratio */
				cfg.prescale_h_ratio	= 1<<i;
				cfg.h_shift		= i;
				break;
			} else {
				rp_err(ctrl->log_level, "src_width should be the multiple of \
					4 * PreScale_H_Ratio(%d).\n", 1<<i);
				return -1;
			}
		}
	}

	if (i == 0) {
		cfg.prescale_h_ratio	= 1;
		cfg.h_shift		= 0;
	}

 	cfg.prescale_dst_width	= src_rect->width / cfg.prescale_h_ratio;
	cfg.dx			= (src_rect->width<<8) / (dst_rect->width<<cfg.h_shift);

	/* Check height */	
	if (src_rect->height >= (dst_rect->height<<6)) {
		rp_err(ctrl->log_level, "Out of vertical scale range.\n");
		return -1;
	}

	for (i = 5; i > 0; i--) {
		if (src_rect->height >= (dst_rect->height<<i)) {
			if (src_rect->height % (2*(1<<i)) == 0 ) {
				/* src_height = 2 * n * PreScale_V_Ratio */
				cfg.prescale_v_ratio	= 1<<i;
				cfg.v_shift		= i;
				break;				
			} else {
				rp_err(ctrl->log_level, "src_height should be the multiple of \
					2 * PreScale_V_Ratio(%d).\n", 1<<i);
				return -1;
			}
		}
	}

	if (i == 0) {
		cfg.prescale_v_ratio	= 1;
		cfg.v_shift		= 0;
	}

 	cfg.prescale_dst_height	= src_rect->height	/ cfg.prescale_v_ratio;
	cfg.dy			= (src_rect->height<<8)	/ (dst_rect->height<<cfg.v_shift);

	cfg.sh_factor		= 10 - (cfg.h_shift + cfg.v_shift);

	memset(scaler_cfg, 0, size);
	memcpy(scaler_cfg, &cfg, size);

	return 0;
}

int s3c_rp_check_param(struct s3c_rp_control *ctrl)
{
	struct v4l2_rect src_rect, dst_rect, fimd_rect;
	int	ret = 0;

	if((ctrl->rot.degree != ROT_90) && (ctrl->rot.degree != ROT_270)) {	/* Portrait mode */
		src_rect.width		= ctrl->v4l2.video_out_fmt.width;
		src_rect.height		= ctrl->v4l2.video_out_fmt.height; 
		fimd_rect.width		= ctrl->fimd.h_res;
		fimd_rect.height	= ctrl->fimd.v_res;
	} else {								/* Landscape mode */
		src_rect.width		= ctrl->v4l2.video_out_fmt.height;
		src_rect.height		= ctrl->v4l2.video_out_fmt.width;
		fimd_rect.width		= ctrl->fimd.v_res;
		fimd_rect.height	= ctrl->fimd.h_res;
	}
	dst_rect.width	= ctrl->v4l2.video_overlay_fmt.w.width;
	dst_rect.height	= ctrl->v4l2.video_overlay_fmt.w.height;
	dst_rect.top	= ctrl->v4l2.video_overlay_fmt.w.top;
	dst_rect.left	= ctrl->v4l2.video_overlay_fmt.w.left;

	if ((dst_rect.left + dst_rect.width) > fimd_rect.width) {
		rp_err(ctrl->log_level, "Horizontal position setting is failed.\n");
		rp_err(ctrl->log_level, "\tleft = %d, width = %d, lcd width = %d, \n", 
			dst_rect.left, dst_rect.width, fimd_rect.width);
		ret = -1;
	} else if ((dst_rect.top + dst_rect.height) > fimd_rect.height) {
		rp_err(ctrl->log_level, "Vertical position setting is failed.\n");
		rp_err(ctrl->log_level, "\ttop = %d, height = %d, lcd height = %d, \n", 
			dst_rect.top, dst_rect.height, fimd_rect.height);		
		ret = -1;
	}

	return ret;
}

int s3c_rp_pp_set_param(struct s3c_rp_control *ctrl)
{
	int	ret = -1;

	s3c_rp_pp_set_envid(ctrl, FALSE);

	ret = s3c_rp_pp_set_pixelformat(ctrl);
	if (ret < 0) {
		rp_err(ctrl->log_level, "Cannot set the post processor pixelformat.\n");
		return -1;
	}

	ret = s3c_rp_pp_set_scaler(ctrl);
	if (ret < 0) {
		rp_err(ctrl->log_level, "Cannot set the post processor scaler.\n");
		return -1;
	}

	s3c_rp_pp_set_int_enable(ctrl, TRUE);

	return 0;
}

int s3c_rp_tv_set_param(struct s3c_rp_control *ctrl)
{
	int	ret = -1;

	s3c_rp_tv_set_envid(ctrl, FALSE);

	ret = s3c_rp_tv_set_pixelformat(ctrl);
	if (ret < 0) {
		rp_err(ctrl->log_level, "Cannot set the tv scaler pixelformat.\n");
		return -1;
	}

	ret = s3c_rp_tv_set_scaler(ctrl);
	if (ret < 0) {
		rp_err(ctrl->log_level, "Cannot set the tv scaler \n");
		return -1;
	}

	s3c_rp_tv_set_addr(ctrl, ctrl->pp.buf_idx.run, 0);

	s3c_rp_tv_set_int_enable(ctrl, TRUE);

	return 0;
}

void s3c_rp_mapping_rot(struct s3c_rp_control *ctrl, int degree)
{
	switch (degree) {
	case 0:
		ctrl->rot.degree = ROT_0;
		break;

	case 90:
		ctrl->rot.degree = ROT_90;
		break;

	case 180:
		ctrl->rot.degree = ROT_180;
		break;

	case 270:
		ctrl->rot.degree = ROT_270;
		break;

	default:
		rp_warn(ctrl->log_level, "ROT_0 is set.");
		ctrl->rot.degree = ROT_0;
		break;
	}
}

int s3c_rp_init_in_queue(struct s3c_rp_control *ctrl)
{
	unsigned long	spin_flags;
	unsigned int	i;

	spin_lock_irqsave(&ctrl->spin.lock_in, spin_flags);

	/* Init incoming queue */
	for (i = 0; i < S3C_RP_BUFF_NUM; i++) {
		ctrl->incoming_queue[i] = -1;
	}

	spin_unlock_irqrestore(&ctrl->spin.lock_in, spin_flags);
	
	return 0;
}

int s3c_rp_init_out_queue(struct s3c_rp_control *ctrl)
{
	unsigned long	spin_flags;
	unsigned int	i;

	spin_lock_irqsave(&ctrl->spin.lock_in, spin_flags);

	/* Init outgoing queue */
	for (i = 0; i < S3C_RP_BUFF_NUM; i++) {
		ctrl->outgoing_queue[i] = -1;
	}

	spin_unlock_irqrestore(&ctrl->spin.lock_in, spin_flags);
	
	return 0;
}

int s3c_rp_init_inside_queue(struct s3c_rp_control *ctrl)
{
	unsigned long	spin_flags;
	unsigned int	i;

	spin_lock_irqsave(&ctrl->spin.lock_in, spin_flags);

	/* Init inside queue */
	for (i = 0; i < S3C_RP_BUFF_NUM; i++) {
		ctrl->inside_queue[i] = -1;
	}

	spin_unlock_irqrestore(&ctrl->spin.lock_in, spin_flags);
	
	return 0;
}

int s3c_rp_attach_in_queue(struct s3c_rp_control *ctrl, unsigned int index)
{
	unsigned long		spin_flags;
	int			swap_queue[S3C_RP_BUFF_NUM];
	int			i;

	rp_dbg(ctrl->log_level, "[%s] index = %d\n", __FUNCTION__, index);

	spin_lock_irqsave(&ctrl->spin.lock_in, spin_flags);

	/* Backup original queue */
	for (i = 0; i < S3C_RP_BUFF_NUM; i++) {
		swap_queue[i] = ctrl->incoming_queue[i];
	}

	/* Attach new index */
	ctrl->incoming_queue[0]		= index;
	ctrl->user_buf[index].buf_state	= BUF_QUEUED;
	ctrl->user_buf[index].buf_flag	= V4L2_BUF_FLAG_MAPPED | V4L2_BUF_FLAG_QUEUED;

	/* Shift the origonal queue */
	for (i = 1; i < S3C_RP_BUFF_NUM; i++) {
		ctrl->incoming_queue[i] = swap_queue[i-1];
	}

	spin_unlock_irqrestore(&ctrl->spin.lock_in, spin_flags);
	
	return 0;
}

int s3c_rp_detach_in_queue(struct s3c_rp_control *ctrl, int *index)
{
	unsigned long		spin_flags;
	int			i, ret = 0;

	spin_lock_irqsave(&ctrl->spin.lock_in, spin_flags);

	/* Find last valid index in incoming queue. */
	for (i = (S3C_RP_BUFF_NUM-1); i >= 0; i--) {
		if (ctrl->incoming_queue[i] != -1) {
			*index					= ctrl->incoming_queue[i];
			ctrl->incoming_queue[i]			= -1;
			ctrl->user_buf[*index].buf_state	= BUF_RUNNING;
			ctrl->user_buf[*index].buf_flag		= V4L2_BUF_FLAG_MAPPED;
			break;
		}
	}

	/* incoming queue is empty. */
	if (i < 0)
		ret = -1;
	else
		rp_dbg(ctrl->log_level, "[%s] index = %d\n", __FUNCTION__, *index);

	spin_unlock_irqrestore(&ctrl->spin.lock_in, spin_flags);
	
	return ret;
}

int s3c_rp_attach_out_queue(struct s3c_rp_control *ctrl, unsigned int index)
{
	unsigned long		spin_flags;
	int			swap_queue[S3C_RP_BUFF_NUM];
	int			i;

	rp_dbg(ctrl->log_level, "[%s] index = %d\n", __FUNCTION__, index);

	spin_lock_irqsave(&ctrl->spin.lock_out, spin_flags);

	/* Backup original queue */
	for (i = 0; i < S3C_RP_BUFF_NUM; i++) {
		swap_queue[i] = ctrl->outgoing_queue[i];
	}

	/* Attach new index */
	ctrl->outgoing_queue[0]		= index;
	ctrl->user_buf[index].buf_state	= BUF_DONE;
	ctrl->user_buf[index].buf_flag	= V4L2_BUF_FLAG_MAPPED | V4L2_BUF_FLAG_DONE;

	/* Shift the origonal queue */
	for (i = 1; i < S3C_RP_BUFF_NUM; i++) {
		ctrl->outgoing_queue[i] = swap_queue[i-1];
	}

	spin_unlock_irqrestore(&ctrl->spin.lock_out, spin_flags);

	return 0;
}

int s3c_rp_detach_out_queue(struct s3c_rp_control *ctrl, int *index)
{
	unsigned long		spin_flags;
	int			i, ret = 0;

	spin_lock_irqsave(&ctrl->spin.lock_out, spin_flags);

	/* Find last valid index in outgoing queue. */
	for (i = (S3C_RP_BUFF_NUM-1); i >= 0; i--) {
		if (ctrl->outgoing_queue[i] != -1) {
			*index					= ctrl->outgoing_queue[i];
			ctrl->outgoing_queue[i]			= -1;
			ctrl->user_buf[*index].buf_state	= BUF_DQUEUED;
			ctrl->user_buf[*index].buf_flag		= V4L2_BUF_FLAG_MAPPED;
			break;
		}
	}

	/* outgoing queue is empty. */
	if (i < 0) {
		ret = -1;
		rp_dbg(ctrl->log_level, "[%s] outgoing queue : %d, %d, %d\n", 
			__FUNCTION__, ctrl->outgoing_queue[0], 
			ctrl->outgoing_queue[1], ctrl->outgoing_queue[2]);
	} else
		rp_dbg(ctrl->log_level, "[%s] index = %d\n", __FUNCTION__, *index);
		

	spin_unlock_irqrestore(&ctrl->spin.lock_out, spin_flags);
	
	return ret;
}

int s3c_rp_attach_inside_queue(struct s3c_rp_control *ctrl, unsigned int index)
{
	unsigned long		spin_flags;
	int			swap_queue[S3C_RP_BUFF_NUM];
	int			i;

	rp_dbg(ctrl->log_level, "[%s] index = %d\n", __FUNCTION__, index);

	spin_lock_irqsave(&ctrl->spin.lock_inside, spin_flags);

	/* Backup original queue */
	for (i = 0; i < S3C_RP_BUFF_NUM; i++) {
		swap_queue[i] = ctrl->inside_queue[i];
	}

	/* Attach new index */
	ctrl->inside_queue[0]		= index;
	ctrl->user_buf[index].buf_state	= BUF_QUEUED;
	ctrl->user_buf[index].buf_flag	= V4L2_BUF_FLAG_MAPPED | V4L2_BUF_FLAG_QUEUED;

	/* Shift the origonal queue */
	for (i = 1; i < S3C_RP_BUFF_NUM; i++) {
		ctrl->inside_queue[i] = swap_queue[i-1];
	}

	spin_unlock_irqrestore(&ctrl->spin.lock_inside, spin_flags);

	return 0;
}

int s3c_rp_detach_inside_queue(struct s3c_rp_control *ctrl, int *index)
{
	unsigned long		spin_flags;
	int			i, ret = 0;

	spin_lock_irqsave(&ctrl->spin.lock_inside, spin_flags);

	/* Find last valid index in inside queue. */
	for (i = (S3C_RP_BUFF_NUM-1); i >= 0; i--) {
		if (ctrl->inside_queue[i] != -1) {
			*index					= ctrl->inside_queue[i];
			ctrl->inside_queue[i]			= -1;
			ctrl->user_buf[*index].buf_state	= BUF_RUNNING;
			ctrl->user_buf[*index].buf_flag		= V4L2_BUF_FLAG_MAPPED;
			break;
		}
	}

	/* inside queue is empty. */
	if (i < 0)
		ret = -1;
	else
		rp_dbg(ctrl->log_level, "[%s] index = %d\n", __FUNCTION__, *index);

	spin_unlock_irqrestore(&ctrl->spin.lock_inside, spin_flags);
	
	return ret;
}

int s3c_rp_pp_start(struct s3c_rp_control *ctrl, unsigned int index)
{
	ctrl->pp.buf_idx.run = index;

	s3c_rp_pp_set_addr(ctrl, index, PP_CURRENT);
	s3c_rp_pp_fifo_start(ctrl);

	return 0;
}

int s3c_rp_rot_run(struct s3c_rp_control *ctrl, unsigned int index)
{
	int		ret = 0;

	ctrl->rot.run_idx	= index;
	ctrl->rot.status	= ROT_RUNNING;
	
	s3c_rp_rot_set_addr(ctrl, index);
	s3c_rp_rot_start(ctrl);

	if (ctrl->stream_status == RP_READY_ON){
		while (1) {
			ret = wait_event_interruptible_timeout(ctrl->waitq.rot, (ctrl->inside_queue[0] != -1), S3C_RP_ROT_TIMEOUT);
			if (ret == 0) {
				rp_err(ctrl->log_level, "Waiting for rotator interrupt is timeout\n");
				ctrl->rot.status = ROT_ABORT;
				ret = -1;
				break;
			} else if (ret == -ERESTARTSYS) {
				if (signal_pending(current)) {
					rp_dbg(ctrl->log_level, ". pend=%.8lx shpend=%.8lx\n",
						current->pending.signal.sig[0], 
						current->signal->shared_pending.signal.sig[0]);
				} else {
					rp_dbg(ctrl->log_level, ": pend=%.8lx shpend=%.8lx\n",
						current->pending.signal.sig[0], 
						current->signal->shared_pending.signal.sig[0]);
				}

				/* SIGKILL */
				if ((current->pending.signal.sig[0] & (0x1<<SIGKILL)) || (current->signal->shared_pending.signal.sig[0] & (0x1<<SIGKILL))) {
					rp_err(ctrl->log_level, "[%s : %d] SIGKILL is pending.\n", __FUNCTION__, __LINE__);
					return -ERESTARTSYS;					
				}
			} else {
				/* Normal case */
				ctrl->rot.status = ROT_IDLE;
				ctrl->rot.run_idx = -1;
				ret = 0;
				break;
			}
		}
	}

	return ret;
}


int s3c_rp_init_capture_buf(struct s3c_rp_control *ctrl)
{	
	unsigned int ycbcr_buf_size, y_buf_size, cb_buf_size, rgb_buf_size;
	unsigned int catpure_start_buf, capture_buf_size;
	unsigned int i;

	ctrl->buf_info.reserved_mem_start = RP_RESERVED_MEM_ADDR_PHY;

	capture_buf_size = PAGE_ALIGN((ctrl->fimd.h_res * ctrl->fimd.v_res)<<2);

	if (ctrl->v4l2.video_out_fmt.pixelformat == V4L2_PIX_FMT_YUV420) {	/* 720 x 576 YUV420 */
		y_buf_size	= PAGE_ALIGN(S3C_RP_YUV_SRC_MAX_WIDTH * S3C_RP_YUV_SRC_MAX_HEIGHT);
		cb_buf_size	= PAGE_ALIGN((S3C_RP_YUV_SRC_MAX_WIDTH * S3C_RP_YUV_SRC_MAX_HEIGHT)>>2);
		ycbcr_buf_size = y_buf_size + cb_buf_size *2;	
		catpure_start_buf = ycbcr_buf_size * S3C_RP_BUFF_NUM * 2;
	} else {
		rgb_buf_size = PAGE_ALIGN((ctrl->fimd.h_res * ctrl->fimd.v_res)<<2);
		catpure_start_buf = rgb_buf_size * S3C_RP_BUFF_NUM * 2;
	}
	
	for (i = 0; i < S3C_RP_CAPTURE_BUF_NUM; i++) {
		/* Initialize capture Buffer */
		ctrl->cap_buf[i].buf_addr.phys_rgb	= ctrl->buf_info.reserved_mem_start + catpure_start_buf;
		ctrl->cap_buf[i].buf_state		= BUF_DONE;
		ctrl->cap_buf[i].buf_flag		= 0x0;
		ctrl->cap_buf[i].buf_length		= capture_buf_size;
		atomic_set(&ctrl->cap_buf[i].buf_maped, 0);
	}

	ctrl->c_buf_info.requested	= FALSE;
	return 0;
}

int s3c_rp_dump_context(struct s3c_rp_control *ctrl)
{
	rp_err(ctrl->log_level, "incoming queue : %d, %d, %d\n", 
		ctrl->incoming_queue[0], ctrl->incoming_queue[1], ctrl->incoming_queue[2]);
	rp_err(ctrl->log_level, "inside queue : %d, %d, %d\n", 
		ctrl->inside_queue[0], ctrl->inside_queue[1], ctrl->inside_queue[2]);
	rp_err(ctrl->log_level, "outgoing queue : %d, %d, %d\n", 
		ctrl->outgoing_queue[0], ctrl->outgoing_queue[1], ctrl->outgoing_queue[2]);
	rp_err(ctrl->log_level, "rot state : %d id = %d\n", 
		ctrl->rot.status, ctrl->rot.run_idx);
	rp_err(ctrl->log_level, "pp state : prev = %d, run = %d, next = %d\n", 
		ctrl->pp.buf_idx.prev, ctrl->pp.buf_idx.run, ctrl->pp.buf_idx.next);

	return 0;
}

