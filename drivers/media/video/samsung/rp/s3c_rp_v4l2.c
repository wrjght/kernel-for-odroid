/* linux/drivers/media/video/samsung/rp/s3c_rp_v4l2.c
 *
 * V4L2 interface support file for Samsung Renderer pipeline driver
 *
 * Jonghun Han, Copyright (c) 2009 Samsung Electronics
 * 	http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/videodev2.h>
#include <media/v4l2-ioctl.h>

#include "s3c_rp.h"

static int s3c_rp_v4l2_querycap(struct file *filp, void *fh, struct v4l2_capability *cap)
{
	struct s3c_rp_control *ctrl = (struct s3c_rp_control *) fh;

	rp_info(ctrl->log_level, "[%s] called\n", __FUNCTION__);

	strcpy(cap->driver, "Samsung Renderer pipeline Driver");
	strlcpy(cap->card, ctrl->vd->name, sizeof(cap->card));
	sprintf(cap->bus_info, "Renderer pipeline AHB-bus");

	cap->version = 0;
	cap->capabilities = (V4L2_CAP_VIDEO_OVERLAY | V4L2_CAP_VIDEO_OUTPUT |	\
				V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_CAPTURE);

	return 0;
}

static int s3c_rp_v4l2_g_fmt_vid_out(struct file *filp, void *fh, struct v4l2_format *f)
{
	struct s3c_rp_control *ctrl = (struct s3c_rp_control *) fh;

	rp_info(ctrl->log_level, "[%s] called\n", __FUNCTION__);

	f->fmt.pix = ctrl->v4l2.video_out_fmt;

	return 0;
}

static int s3c_rp_v4l2_try_fmt_vid_out(struct file *filp, void *fh, struct v4l2_format *f)
{
	struct s3c_rp_control *ctrl = (struct s3c_rp_control *) fh;
	unsigned int	fixup_width;
	int		ret = 0;

	/* Check pixel format */
	if ((f->fmt.pix.pixelformat != V4L2_PIX_FMT_YUV420) && (f->fmt.pix.pixelformat != V4L2_PIX_FMT_RGB32)) {
		rp_warn(ctrl->log_level, "The pixelformat of V4L2_BUF_TYPE_VIDEO_OUTPUT must be V4L2_PIX_FMT_YUV420 or V4L2_PIX_FMT_RGB32.\n");
		rp_warn(ctrl->log_level, "The renderer pipeline driver will change the pixelformat to V4L2_PIX_FMT_YUV420.\n");
		f->fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;	/* Default value */
		ret = -1;
	}

	if (f->fmt.pix.pixelformat == V4L2_PIX_FMT_YUV420) {
		/* Check pixel width : pixel width must be 8's multiple. */
		if (f->fmt.pix.width > S3C_RP_YUV_SRC_MAX_WIDTH) {
			rp_warn(ctrl->log_level, "The width of V4L2_BUF_TYPE_VIDEO_OUTPUT must be up to %d pixels.\n",
				S3C_RP_YUV_SRC_MAX_WIDTH);
			rp_warn(ctrl->log_level, "The renderer pipeline driver will change the width from %d to %d.\n",
				f->fmt.pix.width, S3C_RP_YUV_SRC_MAX_WIDTH);
			f->fmt.pix.width = S3C_RP_YUV_SRC_MAX_WIDTH;
			ret = -1;		
		}	
	} else if (f->fmt.pix.pixelformat == V4L2_PIX_FMT_RGB32) {
		/* fall through : We cannot check max size. */
		/* Because rotation will be called after VIDIOC_S_FMT. */
	}

	/* Check pixel width : pixel width must be 8's multiple. */
	if ((f->fmt.pix.width)%8 != 0) {
		rp_warn(ctrl->log_level, "The width of V4L2_BUF_TYPE_VIDEO_OUTPUT must be 8's multiple.\n");
		fixup_width = (f->fmt.pix.width) & 0xFF1;
		rp_warn(ctrl->log_level, "The renderer pipeline driver will change the width from %d to %d.\n",
			f->fmt.pix.width, fixup_width);
		f->fmt.pix.width = fixup_width;
		ret = -1;		
	}

	/* Fill the return value. */
	if (f->fmt.pix.pixelformat == V4L2_PIX_FMT_YUV420) {
		/* YUV420 format uses 1.5 bytes per pixel. */
		f->fmt.pix.bytesperline	= (f->fmt.pix.width * 3)>>1;
	} else if (f->fmt.pix.pixelformat == V4L2_PIX_FMT_RGB32) {
		/* ARGBX888 format uses 4 bytes per pixel. */
		f->fmt.pix.bytesperline	= f->fmt.pix.width<<2;
	} else {
		/* dummy value*/
		f->fmt.pix.bytesperline	= f->fmt.pix.width;
	}

	f->fmt.pix.sizeimage	= f->fmt.pix.bytesperline * f->fmt.pix.height;
	f->fmt.pix.colorspace	= V4L2_COLORSPACE_SMPTE170M;

	return ret;
}

static int s3c_rp_v4l2_s_fmt_vid_out(struct file *filp, void *fh, struct v4l2_format *f)
{
	struct s3c_rp_control *ctrl = (struct s3c_rp_control *) fh;
	int		ret;

	rp_info(ctrl->log_level, "[%s] called. width(%d), height(%d)\n", 
		__FUNCTION__, f->fmt.pix.width, f->fmt.pix.height);

	/* Check stream status */
	if (ctrl->stream_status != RP_STREAMOFF) {
		rp_err(ctrl->log_level, "The data format cannot be changed at this time.\n	\
			Because the streaming is already in progress.\n");
		return -EBUSY;
	}

	ret = s3c_rp_v4l2_try_fmt_vid_out(filp, fh, f);
	if (ret == -1) {
		rp_err(ctrl->log_level, "Ther renderer pipeline cannot support the output argument which you set.\n");
		return -1;
	}

	ctrl->v4l2.video_out_fmt = f->fmt.pix;

	return 0;
}


static int s3c_rp_v4l2_g_fmt_vid_overlay(struct file *filp, void *fh, struct v4l2_format *f)
{
	struct s3c_rp_control *ctrl = (struct s3c_rp_control *) fh;

	rp_info(ctrl->log_level, "[%s] called\n", __FUNCTION__);

	f->fmt.win = ctrl->v4l2.video_overlay_fmt;

	return 0;
}

static int s3c_rp_v4l2_s_fmt_vid_overlay(struct file *filp, void *fh, struct v4l2_format *f)
{
	struct s3c_rp_control *ctrl = (struct s3c_rp_control *) fh;
	struct s3c_rp_scaler_cfg scaler_cfg;
	struct v4l2_rect src_rect;
	int ret;

	rp_info(ctrl->log_level, "[%s] called. top(%d), left(%d), width(%d), height(%d)\n", 
		__FUNCTION__, f->fmt.win.w.top, f->fmt.win.w.left, f->fmt.win.w.width, f->fmt.win.w.height);

	src_rect.left	= 0;
	src_rect.top	= 0;

	/* Check stream status */
	if (ctrl->stream_status != RP_STREAMOFF) {
		rp_err(ctrl->log_level, "The data format cannot be changed at this time.\n	\
			Because the streaming is already in progress.\n");
		return -EBUSY;
	}

	/* Check Overlay Size : Overlay size must be smaller than LCD size. */
	if ((ctrl->rot.degree != ROT_90) && (ctrl->rot.degree != ROT_270)) {	/* Portrait mode */
		if (f->fmt.win.w.width > ctrl->fimd.h_res) {
			rp_warn(ctrl->log_level, "The width of V4L2_BUF_TYPE_VIDEO_OVERLAY must be up to %d pixels.\n",
				ctrl->fimd.h_res);
			rp_warn(ctrl->log_level, "The renderer pipeline driver will change the width from %d to %d.\n",
				f->fmt.win.w.width, ctrl->fimd.h_res);
			f->fmt.win.w.width = ctrl->fimd.h_res;
		}
		
		if (f->fmt.win.w.height > ctrl->fimd.v_res) {
			rp_warn(ctrl->log_level, "The height of V4L2_BUF_TYPE_VIDEO_OVERLAY must be up to %d pixels.\n",
				ctrl->fimd.v_res);
			rp_warn(ctrl->log_level, "The renderer pipeline driver will change the height from %d to %d.\n",
				f->fmt.win.w.height, ctrl->fimd.v_res);
			f->fmt.win.w.height = ctrl->fimd.v_res;
		}

		src_rect.width	= ctrl->v4l2.video_out_fmt.width;
		src_rect.height	= ctrl->v4l2.video_out_fmt.height; 
	} else {				/* Landscape mode */
		if (f->fmt.win.w.width > ctrl->fimd.v_res) {
			rp_warn(ctrl->log_level, "The width of V4L2_BUF_TYPE_VIDEO_OVERLAY must be up to %d pixels.\n",
				ctrl->fimd.v_res);
			rp_warn(ctrl->log_level, "The renderer pipeline driver will change the width from %d to %d.\n",
				f->fmt.win.w.width, ctrl->fimd.v_res);
			f->fmt.win.w.width = ctrl->fimd.v_res;
		}
		
		if (f->fmt.win.w.height > ctrl->fimd.h_res) {
			rp_warn(ctrl->log_level, "The height of V4L2_BUF_TYPE_VIDEO_OVERLAY must be up to %d pixels.\n",
				ctrl->fimd.h_res);
			rp_warn(ctrl->log_level, "The renderer pipeline driver will change the height from %d to %d.\n",
				f->fmt.win.w.height, ctrl->fimd.h_res);
			f->fmt.win.w.height = ctrl->fimd.h_res;
		}
		src_rect.width	= ctrl->v4l2.video_out_fmt.height;
		src_rect.height	= ctrl->v4l2.video_out_fmt.width;
	}

 	ret = s3c_rp_pp_query_scaler(ctrl, &src_rect, &f->fmt.win.w, &scaler_cfg);
	if (ret == -1) {
		rp_err(ctrl->log_level, "The renderer pipeline cannot support the overlay argument which you set.\n");
		return -1;
	}

	ctrl->v4l2.video_overlay_fmt = f->fmt.win;

	return 0;
}


static int s3c_rp_v4l2_s_ctrl(struct file *filp, void *fh, struct v4l2_control *c)
{
	struct s3c_rp_control	*ctrl = (struct s3c_rp_control *) fh;

	rp_info(ctrl->log_level, "[%s] called. CID = %d\n", __FUNCTION__, c->id);

	switch (c->id) {
	case V4L2_CID_ROTATION:
		s3c_rp_mapping_rot(ctrl, c->value);
		break;

	default:
		rp_err(ctrl->log_level, "invalid control id: %d\n", c->id);
		return -EINVAL;
	}

	return 0;
}

static int s3c_rp_v4l2_streamon(struct file *filp, void *fh, enum v4l2_buf_type i)
{
	struct s3c_rp_control *ctrl = (struct s3c_rp_control *) fh;
	int		ret = 0;

	rp_info(ctrl->log_level, "[%s] called\n", __FUNCTION__);

	if ((i != V4L2_BUF_TYPE_VIDEO_OUTPUT) && (i != V4L2_BUF_TYPE_VIDEO_CAPTURE) ) {
		rp_err(ctrl->log_level, "V4L2_BUF_TYPE_VIDEO_OUTPUT and V4L2_BUF_TYPE_VIDEO_CAPTURE \
		are only supported.\n");
		return -EINVAL;
	}

	switch (i) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		ret = s3c_rp_check_param(ctrl);
		if (ret < 0) {
			rp_err(ctrl->log_level, "s3c_rp_check_param failed.\n");
			return -1;
		}

		ctrl->stream_status = RP_READY_ON;

		if ( ctrl->rot.degree != 0) {
			ret = s3c_rp_rot_set_param(ctrl);
			if (ret < 0) {
				rp_err(ctrl->log_level, "s3c_rp_rot_set_param failed.\n");
				return -1;
			}
		}

		ret = s3c_rp_pp_set_param(ctrl);
		if (ret < 0) {
			rp_err(ctrl->log_level, "s3c_rp_pp_set_param failed.\n");
			return -1;
		}
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		rp_info(ctrl->log_level, "[%s] called : CAPTURE MODE\n", __FUNCTION__);

		ret = s3c_rp_tv_set_param(ctrl);
		if (ret < 0) {          
			rp_err(ctrl->log_level, "s3c_rp_pp_set_param failed.\n");
			return -1;     
		}

		s3c_rp_tv_set_envid(ctrl, TRUE);	/* Start capture */

		if (interruptible_sleep_on_timeout(&ctrl->waitq.tv, S3C_RP_TV_TIMEOUT)== 0) {
			rp_err(ctrl->log_level, "\n%s: Waiting for interrupt is timeout\n", __FUNCTION__);
		}

	default :
		break;

	}

	return 0;
}

static int s3c_rp_v4l2_streamoff(struct file *filp, void *fh, enum v4l2_buf_type i)
{
	struct s3c_rp_control	*ctrl = (struct s3c_rp_control *) fh;
	unsigned int		j;
	int 			ret;	

	rp_info(ctrl->log_level, "[%s] called\n", __FUNCTION__);

	if (i != V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		rp_err(ctrl->log_level, "V4L2_BUF_TYPE_VIDEO_OUTPUT is only supported\n");
		return -EINVAL;
	}

	ctrl->stream_status = RP_READY_OFF;

	if ((ctrl->rot.degree != ROT_0) && (ctrl->rot.status != ROT_IDLE)) {
		while (1) {
			ret = wait_event_interruptible_timeout(ctrl->waitq.rot, (ctrl->rot.status == ROT_IDLE), S3C_RP_ROT_TIMEOUT);
			if (ret == 0) {
				rp_err(ctrl->log_level, "[%s : %d] Waiting for rotator interrupt is timeout\n", __FUNCTION__, __LINE__);
				ret = s3c_rp_dump_context(ctrl);
				break;
			} else if (ret == -ERESTARTSYS) {
				if (signal_pending(current)) {
					rp_dbg(ctrl->log_level, ".pend=%.8lx shpend=%.8lx\n",
						current->pending.signal.sig[0], 
						current->signal->shared_pending.signal.sig[0]);
				} else {
					rp_dbg(ctrl->log_level, ":pend=%.8lx shpend=%.8lx\n",
						current->pending.signal.sig[0], 
						current->signal->shared_pending.signal.sig[0]);
				}

				/* SIGKILL */
				if ((current->pending.signal.sig[0] & (0x1<<SIGKILL)) || (current->signal->shared_pending.signal.sig[0] & (0x1<<SIGKILL))) {
					rp_err(ctrl->log_level, "[%s : %d] SIGKILL is pending.\n", __FUNCTION__, __LINE__);
					break;
				}
			} else {
				break;
			}
		}
	} else {
		/* Fall through : There is nothing to do. */
	}

	/* Waiting for stop post processor */
	s3c_rp_pp_fifo_stop(ctrl, FIFO_CLOSE);

	/* Init all queues */
	ret = s3c_rp_init_in_queue(ctrl);
	if (ret < 0) {
		rp_err(ctrl->log_level, "Fail : s3c_rp_init_in_queue\n");
		return -EINVAL;
	}

	ret = s3c_rp_init_inside_queue(ctrl);
	if (ret < 0) {
		rp_err(ctrl->log_level, "Fail : s3c_rp_init_inside_queue\n");
		return -EINVAL;
	}

	ret = s3c_rp_init_out_queue(ctrl);
	if (ret < 0) {
		rp_err(ctrl->log_level, "Fail : s3c_rp_init_out_queue\n");
		return -EINVAL;
	}

	/* Make all buffers DQUEUED state. */
	for(j = 0; j < S3C_RP_BUFF_NUM; j++) {
		ctrl->user_buf[j].buf_state	= BUF_DQUEUED;
		ctrl->user_buf[j].buf_flag	= V4L2_BUF_FLAG_MAPPED | V4L2_BUF_FLAG_QUEUED;		
	}

	ctrl->pp.buf_idx.prev	= -1;
	ctrl->pp.buf_idx.run	= -1;
	ctrl->pp.buf_idx.next	= -1;

	ctrl->rot.run_idx	= -1;

	ctrl->stream_status = RP_STREAMOFF;

	return 0;
}

static int s3c_rp_v4l2_reqbufs(struct file *filp, void *fh, struct v4l2_requestbuffers *b)
{
	struct s3c_rp_control	*ctrl = (struct s3c_rp_control *) fh;
	unsigned int		i;
	int			ret = 0;

	rp_info(ctrl->log_level, "[%s] called\n", __FUNCTION__);

	if ((b->type != V4L2_BUF_TYPE_VIDEO_OUTPUT) && (b->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) ) {
		rp_err(ctrl->log_level, "V4L2_BUF_TYPE_VIDEO_OUTPUT and V4L2_BUF_TYPE_VIDEO_CAPTURE \
		are only supported.\n");
		return -EINVAL;
	}

	if (b->memory != V4L2_MEMORY_MMAP) {
		rp_err(ctrl->log_level, "V4L2_MEMORY_MMAP is only supported\n");
		return -EINVAL;
	}

	switch (b->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT :
		if (ctrl->stream_status != RP_STREAMOFF) {
			rp_err(ctrl->log_level, "Renderer pipeline is running.\n");
			return -EBUSY;
		}

		if (ctrl->buf_info.requested == TRUE && b->count != 0 ) {
			rp_err(ctrl->log_level, "Buffers were already requested.\n");
			return -EBUSY;
		}
		
		/* control user input */
		if (b->count > S3C_RP_BUFF_NUM) {
			rp_warn(ctrl->log_level, "The buffer count is modified by driver from %d to %d.\n", 
				b->count, S3C_RP_BUFF_NUM);
			b->count = S3C_RP_BUFF_NUM;
		} 

		if (b->count == 0) {
			for (i = 0; i < S3C_RP_BUFF_NUM; i++) {
				if (atomic_read(&ctrl->user_buf[i].buf_maped)) {
					rp_err(ctrl->log_level, "The buffers are mapped.\n");
					return -EBUSY;
				}
			}
		}

		/* Initialize all buffers */
		ret = s3c_rp_check_buf(ctrl, b->count);
		if (ret) {
			rp_err(ctrl->log_level, "Reserved memory is not enough to allocate buffers.\n");
			return -1;
		}
		
		ret = s3c_rp_init_buf(ctrl);
		if (ret) {
			rp_err(ctrl->log_level, "Cannot initialize the buffers\n");
			return -1;
		}

		if (b->count != 0) {	/* allocate buffers */
			ctrl->buf_info.requested = TRUE;
			
			for(i = 0; i < b->count; i++) {
				ctrl->user_buf[i].buf_state = BUF_DQUEUED;
			}
		} else {
			/* fall through */
			/* All buffers are initialized.  */
		}

		ctrl->buf_info.num = b->count;
		break;

	case V4L2_BUF_TYPE_VIDEO_CAPTURE :
		if (ctrl->c_buf_info.requested == TRUE && b->count != 0 ) {
			rp_err(ctrl->log_level, "Buffers were already requested.\n");
			return -EBUSY;
		}

		/* control user input */
		if (b->count > S3C_RP_CAPTURE_BUF_NUM) {
			rp_warn(ctrl->log_level, "The buffer count is modified by driver from %d to %d.\n", 
				b->count, 1);
			b->count = 1;
		} 

		if (b->count == 0) {
			for (i = 0; i < S3C_RP_CAPTURE_BUF_NUM; i++) {
				if (atomic_read(&ctrl->cap_buf[i].buf_maped)) {
					rp_err(ctrl->log_level, "The capture buffer is mapped.\n");
					return -EBUSY;
				}
			}
		}

		/* Initialize the buffers for capture*/
		ret = s3c_rp_init_capture_buf(ctrl);
		if (ret) {
			rp_err(ctrl->log_level, "Cannot initialize the buffers for capture\n");
			return -1;
		}

		if (b->count != 0) {	/* allocate buffers for capture*/
			ctrl->c_buf_info.requested = TRUE;
			
			for(i = 0; i < b->count; i++) {
				ctrl->cap_buf[i].buf_state = BUF_DQUEUED;
			}
		} else {
			/* fall through */
			/* All buffers are initialized.  */
		}

		ctrl->c_buf_info.num = b->count;
		break;
		
	default :
		
		break;
	}

	return 0;
}

static int s3c_rp_v4l2_querybuf(struct file *filp, void *fh, struct v4l2_buffer *b)
{
	struct s3c_rp_control *ctrl = (struct s3c_rp_control *) fh;

	rp_info(ctrl->log_level, "[%s] called\n", __FUNCTION__);

	if ((b->type != V4L2_BUF_TYPE_VIDEO_OUTPUT) && (b->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) ) {
		rp_err(ctrl->log_level, "V4L2_BUF_TYPE_VIDEO_OUTPUT and V4L2_BUF_TYPE_VIDEO_CAPTURE \
		are only supported.\n");
		return -EINVAL;
	}

	if (b->memory != V4L2_MEMORY_MMAP ) {
		rp_err(ctrl->log_level, "V4L2_MEMORY_MMAP is only supported.\n");
		return -EINVAL;
	}

	switch (b->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		if (b->index > ctrl->buf_info.num ) {
			rp_err(ctrl->log_level, "The index is out of bounds. You requested %d buffers. \
				But you set the index as %d.\n", ctrl->buf_info.num, b->index);
			return -EINVAL;
		}
		
		b->flags	= ctrl->user_buf[b->index].buf_flag;
		b->m.offset	= b->index * PAGE_SIZE;
	 	b->length	= ctrl->user_buf[b->index].buf_length;
	 	break;
	 	
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	 	if (b->index > ctrl->c_buf_info.num ) {
			rp_err(ctrl->log_level, "The index is out of bounds. You requested %d buffers. \
				But you set the index as %d.\n", ctrl->c_buf_info.num, b->index);
			return -EINVAL;
		}
		
		b->flags	= ctrl->cap_buf[b->index].buf_flag;
		b->m.offset	= (b->index + S3C_RP_BUFF_NUM) * PAGE_SIZE;
	 	b->length	= ctrl->cap_buf[b->index].buf_length;
	 	break;

	default :
		break;
	 }

	return 0;
}

static int s3c_rp_v4l2_qbuf(struct file *filp, void *fh, struct v4l2_buffer *b)
{
	struct s3c_rp_control *ctrl = (struct s3c_rp_control *) fh;
	unsigned int		index = 0;
	int			ret = 0;

	rp_info(ctrl->log_level, "[%s] enqueued index = %d\n", __FUNCTION__, b->index);

	if (b->type != V4L2_BUF_TYPE_VIDEO_OUTPUT ) {
		rp_err(ctrl->log_level, "V4L2_BUF_TYPE_VIDEO_OUTPUT is only supported.\n");
		return -EINVAL;
	}

	if (b->memory != V4L2_MEMORY_MMAP ) {
		rp_err(ctrl->log_level, "V4L2_MEMORY_MMAP is only supported.\n");
		return -EINVAL;
	}

	if (b->index > ctrl->buf_info.num ) {
		rp_err(ctrl->log_level, "The index is out of bounds. You requested %d buffers. \
			But you set the index as %d.\n", ctrl->buf_info.num, b->index);
		return -EINVAL;
	}

	/* Check the buffer state if the state is BUF_DQUEUED. */
	if (ctrl->user_buf[b->index].buf_state != BUF_DQUEUED) {
		rp_err(ctrl->log_level, "The index(%d) buffer must be dequeued state(%d).\n", b->index, ctrl->user_buf[b->index].buf_state);
		return -EINVAL;
	}

	/* Attach the buffer to the incoming queue. */
	ret =  s3c_rp_attach_in_queue(ctrl, b->index);
	if (ret < 0) {
		rp_err(ctrl->log_level, "Failed :  s3c_rp_attach_in_queue.\n");
		return -1;
	}

	if (ctrl->stream_status == RP_READY_ON) {
		ret =  s3c_rp_detach_in_queue(ctrl, &index);
		if (ret < 0) {
			rp_err(ctrl->log_level, "Failed :  s3c_rp_detach_in_queue.\n");
			return -1;
		}
		
		if (ctrl->rot.degree != ROT_0) {
			ret = s3c_rp_rot_run(ctrl, index);
			if (ret < 0) {
				rp_err(ctrl->log_level, "Failed : s3c_rp_run_rot().\n");
				return -1;
			}
			
			ret =  s3c_rp_detach_inside_queue(ctrl, &index);
			if (ret < 0) {
				rp_err(ctrl->log_level, "Failed : s3c_rp_detach_inside_queue().\n");
				ret = s3c_rp_dump_context(ctrl);

				return -1;
			}		
		} 

		ret = s3c_rp_pp_start(ctrl, index);
		if (ret < 0) {
			rp_err(ctrl->log_level, "Failed : s3c_rp_pp_start().\n");
			return -1;
		}

		ctrl->stream_status = RP_STREAMON;
	} else if ((ctrl->rot.degree != ROT_0) && (ctrl->rot.status == ROT_IDLE)) {
		ret =  s3c_rp_detach_in_queue(ctrl, &index);
		if (ret < 0) {
			rp_err(ctrl->log_level, "Failed :  s3c_rp_detach_in_queue.\n");
			return -1;
		}
		
		ret = s3c_rp_rot_run(ctrl, index);
		if (ret < 0) {
			rp_err(ctrl->log_level, "Failed : s3c_rp_run_rot().\n");
			return -1;
		}
	}

	return 0;
}

static int s3c_rp_v4l2_dqbuf(struct file *filp, void *fh, struct v4l2_buffer *b)
{
	struct s3c_rp_control	*ctrl = (struct s3c_rp_control *) fh;
	int			index = -1;
	int			ret = -1;

	ret = s3c_rp_detach_out_queue(ctrl, &index);
	if (ret < 0) {
		while (1) {
			ret = wait_event_interruptible_timeout(ctrl->waitq.pp, (ctrl->outgoing_queue[0] != -1), S3C_RP_DQUEUE_TIMEOUT);
			if (ret == 0) {
				rp_err(ctrl->log_level, "[0] There is no buffer in outgoing queue.\n");
				return -1;
			} else if (ret == -ERESTARTSYS) {
				if (signal_pending(current)) {
					rp_dbg(ctrl->log_level, ".pend=%.8lx shpend=%.8lx\n",
						current->pending.signal.sig[0], 
						current->signal->shared_pending.signal.sig[0]);
				} else {
					rp_dbg(ctrl->log_level, ":pend=%.8lx shpend=%.8lx\n",
						current->pending.signal.sig[0], 
						current->signal->shared_pending.signal.sig[0]);
				}

				/* SIGKILL */
				if ((current->pending.signal.sig[0] & (0x1<<SIGKILL)) || (current->signal->shared_pending.signal.sig[0] & (0x1<<SIGKILL))) {
					rp_err(ctrl->log_level, "[%s : %d] SIGKILL is pending.\n", __FUNCTION__, __LINE__);
					s3c_rp_pp_fifo_stop(ctrl, FIFO_CLOSE);
					return -ERESTARTSYS;					
				}
			} else {
				/* Normal case */
				ret = s3c_rp_detach_out_queue(ctrl, &index);
				if (ret < 0) {
					rp_err(ctrl->log_level, "[1] There is no buffer in outgoing queue.\n");
					ret = s3c_rp_dump_context(ctrl);

					return -1;
				} else
					break;
			}
		}
	}

	b->index = index;

	rp_info(ctrl->log_level, "[%s] dqueued index = %d\n", __FUNCTION__, b->index);

	return 0;
}

static int s3c_rp_v4l2_cropcap(struct file *filp, void *fh, struct v4l2_cropcap *a)
{
	struct s3c_rp_control *ctrl = (struct s3c_rp_control *) fh;
	unsigned int	max_width = 0, max_height = 0;

	rp_info(ctrl->log_level, "[%s] called\n", __FUNCTION__);

	if (a->type != V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		rp_err(ctrl->log_level, "The buffer type must be V4L2_BUF_TYPE_VIDEO_OUTPUT.\n");
		return -EINVAL;
	}

	if (ctrl->v4l2.video_out_fmt.pixelformat == V4L2_PIX_FMT_YUV420) {
		max_width	= S3C_RP_YUV_SRC_MAX_WIDTH;
		max_height	= S3C_RP_YUV_SRC_MAX_HEIGHT;
	} else if (ctrl->v4l2.video_out_fmt.pixelformat == V4L2_PIX_FMT_RGB32) {
		if ((ctrl->rot.degree == ROT_0) || (ctrl->rot.degree == ROT_180)) {
			max_width	= ctrl->fimd.h_res;
			max_height	= ctrl->fimd.v_res;
		} else {
			max_width	= ctrl->fimd.v_res;
			max_height	= ctrl->fimd.h_res;
		}
	}

	/* crop bounds */
	ctrl->v4l2.crop_bounds.left	= 0;
	ctrl->v4l2.crop_bounds.top	= 0;
	ctrl->v4l2.crop_bounds.width	= max_width;
	ctrl->v4l2.crop_bounds.height	= max_height;

	/* crop default values */
	ctrl->v4l2.crop_defrect.left	= 0;
	ctrl->v4l2.crop_defrect.top	= 0;
	ctrl->v4l2.crop_defrect.width	= max_width;
	ctrl->v4l2.crop_defrect.height	= max_height;

	/* crop pixel aspec values */
	/* To Do : Have to modify but I don't know the meaning. */
	ctrl->v4l2.pixelaspect.numerator	= 5;
	ctrl->v4l2.pixelaspect.denominator	= 3;

	a->bounds	= ctrl->v4l2.crop_bounds;
	a->defrect	= ctrl->v4l2.crop_defrect;
	a->pixelaspect	= ctrl->v4l2.pixelaspect;

	return 0;
}


static int s3c_rp_v4l2_s_crop(struct file *filp, void *fh, struct v4l2_crop *a)
{
	struct s3c_rp_control *ctrl = (struct s3c_rp_control *) fh;
	unsigned int	max_width = 0, max_height = 0;

	rp_info(ctrl->log_level, "[%s] called\n", __FUNCTION__);

	if (a->type != V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		rp_err(ctrl->log_level, "The buffer type must be V4L2_BUF_TYPE_VIDEO_OUTPUT.\n");
		return -1;
	}

	/* Check arguments : widht and height */
	if ((a->c.width < 0) || (a->c.height < 0)) {
		rp_err(ctrl->log_level, "The crop rect width and height must be bigger than zero.\n");
		return -1;
	}

	if (ctrl->v4l2.video_out_fmt.pixelformat == V4L2_PIX_FMT_YUV420) {
		max_width	= S3C_RP_YUV_SRC_MAX_WIDTH;
		max_height	= S3C_RP_YUV_SRC_MAX_HEIGHT;
	} else if (ctrl->v4l2.video_out_fmt.pixelformat == V4L2_PIX_FMT_RGB32) {
		if ((ctrl->rot.degree == ROT_0) || (ctrl->rot.degree == ROT_180)) {
			max_width	= ctrl->fimd.h_res;
			max_height	= ctrl->fimd.v_res;
		} else {
			max_width	= ctrl->fimd.v_res;
			max_height	= ctrl->fimd.h_res;
		}	
	}

	if ((a->c.width > max_width) || (a->c.height > max_height)) {
		rp_err(ctrl->log_level, "The crop rect width and height must be smaller than %d and %d.\n", 
			max_width, max_height);
		return -1;
	}

	/* Check arguments : left and top */
	if ((a->c.left < 0) || (a->c.top < 0)) {
		rp_err(ctrl->log_level, "The crop rect left and top must be bigger than zero.\n");
		return -1;
	}
	
	if ((a->c.left > max_width) || (a->c.top > max_height)) {
		rp_err(ctrl->log_level, "The crop rect left and top must be smaller than %d, %d.\n", 
			max_width, max_height);
		return -1;
	}

	if ((a->c.left + a->c.width) > max_width) {
		rp_err(ctrl->log_level, "The crop rect must be in bound rect.\n");
		return -1;
	}
	
	if ((a->c.top + a->c.height) > max_height) {
		rp_err(ctrl->log_level, "The crop rect must be in bound rect.\n");
		return -1;
	}

	ctrl->v4l2.crop_rect.left	= a->c.left;
	ctrl->v4l2.crop_rect.top	= a->c.top;
	ctrl->v4l2.crop_rect.width	= a->c.width;
	ctrl->v4l2.crop_rect.height	= a->c.height;

	return 0;
}

static int s3c_rp_v4l2_try_fmt_vid_cap(struct file *filp, void *fh,
					  struct v4l2_format *f)
{
	return 0;
}


const struct v4l2_ioctl_ops s3c_rp_v4l2_ops = {
	.vidioc_querycap		= s3c_rp_v4l2_querycap,

	.vidioc_reqbufs			= s3c_rp_v4l2_reqbufs,
	.vidioc_querybuf		= s3c_rp_v4l2_querybuf,

	.vidioc_qbuf			= s3c_rp_v4l2_qbuf,
	.vidioc_dqbuf			= s3c_rp_v4l2_dqbuf,

	.vidioc_streamon		= s3c_rp_v4l2_streamon,
	.vidioc_streamoff		= s3c_rp_v4l2_streamoff,

	.vidioc_s_ctrl			= s3c_rp_v4l2_s_ctrl,

	.vidioc_cropcap			= s3c_rp_v4l2_cropcap,
	.vidioc_s_crop			= s3c_rp_v4l2_s_crop,	

	.vidioc_g_fmt_vid_out		= s3c_rp_v4l2_g_fmt_vid_out,
	.vidioc_s_fmt_vid_out		= s3c_rp_v4l2_s_fmt_vid_out,
	.vidioc_try_fmt_vid_out		= s3c_rp_v4l2_try_fmt_vid_out,
	
	.vidioc_g_fmt_vid_overlay	= s3c_rp_v4l2_g_fmt_vid_overlay,
	.vidioc_s_fmt_vid_overlay	= s3c_rp_v4l2_s_fmt_vid_overlay,

	.vidioc_try_fmt_vid_cap		= s3c_rp_v4l2_try_fmt_vid_cap,
};

