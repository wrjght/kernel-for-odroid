/* linux/drivers/media/video/samsung/fimc_core.c
 *
 * Core file for Samsung Camera Interface (FIMC) driver
 *
 * Dongsoo Kim, Copyright (c) 2009 Samsung Electronics
 * 	http://www.samsung.com/sec/
 * Jinsung Yang, Copyright (c) 2009 Samsung Electronics
 * 	http://www.samsungsemi.com/
 * Jonghun Han, Copyright (c) 2009 Samsung Electronics
 * 	http://www.samsungsemi.com/
 *
 * Note: This driver supports common i2c client driver style
 * which uses i2c_board_info for backward compatibility and
 * new v4l2_subdev as well.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/irq.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <media/v4l2-device.h>
#include <linux/io.h>
#include <linux/memory.h>
#include <linux/ctype.h>
#include <plat/clock.h>
#include <plat/media.h>
#include <plat/fimc.h>

#include "fimc.h"

struct fimc_global *fimc_dev;

int fimc_dma_alloc(struct fimc_control *ctrl, struct fimc_buf_set *bs, int i, int align)
{
	dma_addr_t end, *curr;

	mutex_lock(&ctrl->lock);

	end = ctrl->mem.base + ctrl->mem.size;
	curr = &ctrl->mem.curr;

	if (!bs->length[i])
		return -EINVAL;

	if (!align) {
		if (*curr + bs->length[i] > end) {
			goto overflow;
		} else {
			bs->base[i] = *curr;
			bs->garbage[i] = 0;
			*curr += bs->length[i];
		}
	} else {
		if (ALIGN(*curr, align) + bs->length[i] > end)
			goto overflow;
		else {
			bs->base[i] = ALIGN(*curr, align);
			bs->garbage[i] = ALIGN(*curr, align) - *curr;
			*curr += (bs->length[i] + bs->garbage[i]);
		}
	}

	mutex_unlock(&ctrl->lock);

	return 0;

overflow:
	bs->base[i] = 0;
	bs->length[i] = 0;
	bs->garbage[i] = 0;

	mutex_unlock(&ctrl->lock);

	return -ENOMEM;
}

void fimc_dma_free(struct fimc_control *ctrl, struct fimc_buf_set *bs, int i)
{
	int total = bs->length[i] + bs->garbage[i];
	mutex_lock(&ctrl->lock);

	if (bs->base[i]) {
		if (ctrl->mem.curr - total >= ctrl->mem.base)
			ctrl->mem.curr -= total;

		bs->base[i] = 0;
		bs->length[i] = 0;
		bs->garbage[i] = 0;
	}

	mutex_unlock(&ctrl->lock);
}

static inline u32 fimc_irq_out_none(struct fimc_control *ctrl)
{
	u32 next = 0, wakeup = 1;
	int ret = -1;

	if (ctrl->status == FIMC_READY_OFF) {
		ctrl->out->idx.active = -1;
		ctrl->status = FIMC_STREAMOFF;
		return wakeup;
	}

	/* Attach done buffer to outgoing queue. */
	ret = fimc_attach_out_queue(ctrl, ctrl->out->idx.active);
	if (ret < 0)
		fimc_err("Failed: fimc_attach_out_queue\n");

	/* Detach buffer from incomming queue. */
	ret =  fimc_detach_in_queue(ctrl, &next);
	if (ret == 0) {	/* There is a buffer in incomming queue. */
		fimc_outdev_set_src_addr(ctrl, ctrl->out->src[next].base);
		ret = fimc_outdev_start_camif(ctrl);
		if (ret < 0)
			fimc_err("Fail: fimc_start_camif\n");

		ctrl->out->idx.active = next;
		ctrl->status = FIMC_STREAMON;
	} else {	/* There is no buffer in incomming queue. */
		ctrl->out->idx.active = -1;
		ctrl->status = FIMC_STREAMON_IDLE;
	}

	return wakeup;
}

static inline u32 fimc_irq_out_dma(struct fimc_control *ctrl)
{
	struct fimc_buf_set buf_set;
	u32 next = 0, wakeup = 1;
	int idx = ctrl->out->idx.active;
	int ret = -1, i;

	if (ctrl->status == FIMC_READY_OFF) {
		ctrl->out->idx.active = -1;
		ctrl->status = FIMC_STREAMOFF;
		return wakeup;
	}

	/* Attach done buffer to outgoing queue. */
	ret = fimc_attach_out_queue(ctrl, idx);
	if (ret < 0)
		fimc_err("Failed: fimc_attach_out_queue\n");

	if(ctrl->out->overlay.mode == FIMC_OVERLAY_DMA_AUTO) {
		ret = s3cfb_direct_ioctl(ctrl->id, S3CFB_SET_WIN_ADDR, \
				(unsigned long)ctrl->out->dst[idx].base[FIMC_ADDR_Y]);
		if (ret < 0) {
			fimc_err("direct_ioctl(S3CFB_SET_WIN_ADDR) fail\n");
			return -EINVAL;
		}

		if(ctrl->fb.is_enable == 0) {
			ret = s3cfb_direct_ioctl(ctrl->id, S3CFB_SET_WIN_ON, \
								(unsigned long)NULL);
			if (ret < 0) {
				fimc_err("direct_ioctl(S3CFB_SET_WIN_ON) fail\n");
				return -EINVAL;
			}

			ctrl->fb.is_enable = 1;
		}
	}

	/* Detach buffer from incomming queue. */
	ret =  fimc_detach_in_queue(ctrl, &next);
	if (ret == 0) {	/* There is a buffer in incomming queue. */
		fimc_outdev_set_src_addr(ctrl, ctrl->out->src[next].base);

		memset(&buf_set, 0x00, sizeof(buf_set));
		buf_set.base[FIMC_ADDR_Y] = ctrl->out->dst[next].base[FIMC_ADDR_Y];

		for (i = 0; i < FIMC_PHYBUFS; i++)
			fimc_hwset_output_address(ctrl, &buf_set, i);
		
		ret = fimc_outdev_start_camif(ctrl);
		if (ret < 0)
			fimc_err("Fail: fimc_start_camif\n");

		ctrl->out->idx.active = next;
		ctrl->status = FIMC_STREAMON;
	} else {	/* There is no buffer in incomming queue. */
		ctrl->out->idx.active = -1;
		ctrl->status = FIMC_STREAMON_IDLE;
	}

	return wakeup;
}

static inline u32 fimc_irq_out_fimd(struct fimc_control *ctrl)
{
	u32 prev, next, wakeup = 0;
	int ret = -1;

	/* Attach done buffer to outgoing queue. */
	if (ctrl->out->idx.prev != -1) {
		ret = fimc_attach_out_queue(ctrl, ctrl->out->idx.prev);
		if (ret < 0) {
			fimc_err("Failed: fimc_attach_out_queue\n");
		} else {
			ctrl->out->idx.prev = -1;
			wakeup = 1; /* To wake up fimc_v4l2_dqbuf(). */
		}
	}

	/* Update index structure. */
	if (ctrl->out->idx.next != -1) {
		ctrl->out->idx.active	= ctrl->out->idx.next;
		ctrl->out->idx.next	= -1;
	}

	/* Detach buffer from incomming queue. */
	ret =  fimc_detach_in_queue(ctrl, &next);
	if (ret == 0) {	/* There is a buffer in incomming queue. */
		prev = ctrl->out->idx.active;
		ctrl->out->idx.prev	= prev;
		ctrl->out->idx.next	= next;

		/* Set the address */
		fimc_outdev_set_src_addr(ctrl, ctrl->out->src[next].base);
	}

	return wakeup;
}

static inline void fimc_irq_out(struct fimc_control *ctrl)
{
	u32 wakeup = 1;

	/* Interrupt pendding clear */
	fimc_hwset_clear_irq(ctrl);

	switch (ctrl->out->overlay.mode) {
	case FIMC_OVERLAY_NONE:
		wakeup = fimc_irq_out_none(ctrl);
		break;
	case FIMC_OVERLAY_DMA_AUTO:
	case FIMC_OVERLAY_DMA_MANUAL:
		wakeup = fimc_irq_out_dma(ctrl);
		break;
	case FIMC_OVERLAY_FIFO:
		if (ctrl->status != FIMC_READY_OFF)
			wakeup = fimc_irq_out_fimd(ctrl);
		break;
	}

	if (wakeup == 1)
		wake_up(&ctrl->wq);
}

static inline void fimc_irq_cap(struct fimc_control *ctrl)
{
	struct fimc_capinfo *cap = ctrl->cap;
	int pp;

	fimc_hwset_clear_irq(ctrl);
	fimc_hwget_overflow_state(ctrl);
	pp = ((fimc_hwget_frame_count(ctrl) + 2) % 4);
	if (cap->fmt.field == V4L2_FIELD_INTERLACED_TB) {
		/* odd value of pp means one frame is made with top/bottom */
		if (pp & 0x1) {	
			cap->irq = 1;
			wake_up(&ctrl->wq);
		}
	}
	else {
		cap->irq = 1;
		wake_up(&ctrl->wq);
	}	
	
}

static irqreturn_t fimc_irq(int irq, void *dev_id)
{
	struct fimc_control *ctrl = (struct fimc_control *) dev_id;

	if (ctrl->cap)
		fimc_irq_cap(ctrl);
	else if (ctrl->out)
		fimc_irq_out(ctrl);

	return IRQ_HANDLED;
}

static
struct fimc_control *fimc_register_controller(struct platform_device *pdev)
{
	struct s3c_platform_fimc *pdata;
	struct fimc_control *ctrl;
	struct resource *res;
	int id, mdev_id, irq;

	id = pdev->id;
	mdev_id = S3C_MDEV_FIMC0 + id;
	pdata = to_fimc_plat(&pdev->dev);

	ctrl = get_fimc_ctrl(id);
	ctrl->id = id;
	ctrl->dev = &pdev->dev;
	ctrl->vd = &fimc_video_device[id];
	ctrl->vd->minor = id;

	/* alloc from bank1 as default */
	ctrl->mem.base = s3c_get_media_memory_node(mdev_id, 1);
	ctrl->mem.size = s3c_get_media_memsize_node(mdev_id, 1);
	ctrl->mem.curr = ctrl->mem.base;

	ctrl->status = FIMC_STREAMOFF;
	ctrl->limit = &fimc_limits[id];
	ctrl->log = FIMC_LOG_DEFAULT;

	sprintf(ctrl->name, "%s%d", FIMC_NAME, id);
	strcpy(ctrl->vd->name, ctrl->name);

	atomic_set(&ctrl->in_use, 0);
	mutex_init(&ctrl->lock);
	mutex_init(&ctrl->v4l2_lock);
	spin_lock_init(&ctrl->lock_in);
	spin_lock_init(&ctrl->lock_out);
	init_waitqueue_head(&ctrl->wq);

	/* get resource for io memory */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		fimc_err("%s: failed to get io memory region\n", __func__);
		return NULL;
	}

	/* request mem region */
	res = request_mem_region(res->start, res->end - res->start + 1,
			pdev->name);
	if (!res) {
		fimc_err("%s: failed to request io memory region\n", __func__);
		return NULL;
	}

	/* ioremap for register block */
	ctrl->regs = ioremap(res->start, res->end - res->start + 1);
	if (!ctrl->regs) {
		fimc_err("%s: failed to remap io region\n", __func__);
		return NULL;
	}

	/* irq */
	irq = platform_get_irq(pdev, 0);
	if (request_irq(irq, fimc_irq, IRQF_DISABLED, ctrl->name, ctrl))
		fimc_err("%s: request_irq failed\n", __func__);

	fimc_hwset_reset(ctrl);

	return ctrl;
}

static int fimc_unregister_controller(struct platform_device *pdev)
{
	struct s3c_platform_fimc *pdata;
	struct fimc_control *ctrl;
	int id = pdev->id;

	pdata = to_fimc_plat(&pdev->dev);
	ctrl = get_fimc_ctrl(id);
	
	if (pdata->clk_off)
		pdata->clk_off(pdev, ctrl->clk);

	iounmap(ctrl->regs);
	memset(ctrl, 0, sizeof(*ctrl));

	return 0;
}

static void fimc_mmap_open(struct vm_area_struct *vma)
{
	struct fimc_global *dev = fimc_dev;
	int pri_data	= (int)vma->vm_private_data;
	u32 id		= (pri_data / 0x10);
	u32 idx		= (pri_data % 0x10);

	atomic_inc(&dev->ctrl[id].out->src[idx].mapped_cnt);
}

static void fimc_mmap_close(struct vm_area_struct *vma)
{
	struct fimc_global *dev = fimc_dev;
	int pri_data	= (int)vma->vm_private_data;
	u32 id		= (pri_data / 0x10);
	u32 idx		= (pri_data % 0x10);

	atomic_dec(&dev->ctrl[id].out->src[idx].mapped_cnt);
}

static struct vm_operations_struct fimc_mmap_ops = {
	.open	= fimc_mmap_open,
	.close	= fimc_mmap_close,
};

static inline int fimc_mmap_out_src(struct file *filp, struct vm_area_struct *vma)
{
	struct fimc_control *ctrl = filp->private_data;
	u32 start_phy_addr = 0;
	u32 size = vma->vm_end - vma->vm_start;
	u32 pfn, idx = vma->vm_pgoff;
	u32 buf_length = 0;
	int pri_data = 0;

	buf_length = ctrl->out->src[idx].length[FIMC_ADDR_Y] + \
				ctrl->out->src[idx].length[FIMC_ADDR_CB] + \
				ctrl->out->src[idx].length[FIMC_ADDR_CR];
	if (size > buf_length) {
		fimc_err("Requested mmap size is too big\n");
		return -EINVAL;
	}

	pri_data = (ctrl->id * 0x10) + idx;
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	vma->vm_flags |= VM_RESERVED;
	vma->vm_ops = &fimc_mmap_ops;
	vma->vm_private_data = (void *)pri_data;

	if ((vma->vm_flags & VM_WRITE) && !(vma->vm_flags & VM_SHARED)) {
		fimc_err("writable mapping must be shared\n");
		return -EINVAL;
	}

	start_phy_addr = ctrl->out->src[idx].base[FIMC_ADDR_Y];
	pfn = __phys_to_pfn(start_phy_addr);

	if (remap_pfn_range(vma, vma->vm_start, pfn, size,
						vma->vm_page_prot)) {
		fimc_err("mmap fail\n");
		return -EINVAL;
	}

	vma->vm_ops->open(vma);

	ctrl->out->src[idx].flags |= V4L2_BUF_FLAG_MAPPED;

	return 0;
}

static inline int fimc_mmap_out_dst(struct file *filp, struct vm_area_struct *vma, u32 idx)
{
	struct fimc_control *ctrl = filp->private_data;
	unsigned long pfn = 0, size;
	int ret = 0;

	size = vma->vm_end - vma->vm_start;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	vma->vm_flags |= VM_RESERVED;

	pfn = __phys_to_pfn(ctrl->out->dst[idx].base[0]);
	ret = remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot);
	if (ret != 0)
		fimc_err("remap_pfn_range fail.\n");

	return ret;
}

static inline int fimc_mmap_out(struct file *filp, struct vm_area_struct *vma)
{
	struct fimc_control *ctrl = filp->private_data;
	int idx = ctrl->out->overlay.req_idx;
	int ret = 0;

	if (idx >= 0)
		ret = fimc_mmap_out_dst(filp, vma, idx);
	else
		ret = fimc_mmap_out_src(filp, vma);

	return ret;
}

static inline int fimc_mmap_cap(struct file *filp, struct vm_area_struct *vma)
{
	struct fimc_control *ctrl = filp->private_data;
	u32 size = vma->vm_end - vma->vm_start;
	u32 pfn, idx = vma->vm_pgoff;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	vma->vm_flags |= VM_RESERVED;

	/*
	 * page frame number of the address for a source frame
	 * to be stored at.
	 */
	pfn = __phys_to_pfn(ctrl->cap->bufs[idx].base[0]);

	if ((vma->vm_flags & VM_WRITE) && !(vma->vm_flags & VM_SHARED)) {
		fimc_err("%s: writable mapping must be shared\n", __func__);
		return -EINVAL;
	}

	if (remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot)) {
		fimc_err("%s: mmap fail\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static int fimc_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct fimc_control *ctrl = filp->private_data;
	int ret;

	if (ctrl->cap)
		ret = fimc_mmap_cap(filp, vma);
	else
		ret = fimc_mmap_out(filp, vma);

	return ret;
}

static u32 fimc_poll(struct file *filp, poll_table *wait)
{
	struct fimc_control *ctrl = filp->private_data;
	struct fimc_capinfo *cap = ctrl->cap;
	u32 mask = 0;

	if (cap) {
		if (cap->irq) {
			mask = POLLIN | POLLRDNORM;
			cap->irq = 0;
		} else {
			poll_wait(filp, &ctrl->wq, wait);
		}
	}

	return mask;
}

static
ssize_t fimc_read(struct file *filp, char *buf, size_t count, loff_t *pos)
{
	return 0;
}

static
ssize_t fimc_write(struct file *filp, const char *b, size_t c, loff_t *offset)
{
	return 0;
}

u32 fimc_mapping_rot_flip(u32 rot, u32 flip)
{
	u32 ret = 0;

	switch (rot) {
	case 0:
		if (flip & V4L2_CID_HFLIP)
			ret |= FIMC_XFLIP;

		if (flip & V4L2_CID_VFLIP)
			ret |= FIMC_YFLIP;
		break;

	case 90:
		ret = FIMC_ROT;
		if (flip & V4L2_CID_HFLIP)
			ret |= FIMC_XFLIP;

		if (flip & V4L2_CID_VFLIP)
			ret |= FIMC_YFLIP;
		break;

	case 180:
		ret = (FIMC_XFLIP | FIMC_YFLIP);
		if (flip & V4L2_CID_HFLIP)
			ret &= ~FIMC_XFLIP;

		if (flip & V4L2_CID_VFLIP)
			ret &= ~FIMC_YFLIP;
		break;

	case 270:
		ret = (FIMC_XFLIP | FIMC_YFLIP | FIMC_ROT);
		if (flip & V4L2_CID_HFLIP)
			ret &= ~FIMC_XFLIP;

		if (flip & V4L2_CID_VFLIP)
			ret &= ~FIMC_YFLIP;
		break;
	}

	return ret;
}

int fimc_get_scaler_factor(u32 src, u32 tar, u32 *ratio, u32 *shift)
{
	if (src >= tar * 64) {
		return -EINVAL;
	} else if (src >= tar * 32) {
		*ratio = 32;
		*shift = 5;
	} else if (src >= tar * 16) {
		*ratio = 16;
		*shift = 4;
	} else if (src >= tar * 8) {
		*ratio = 8;
		*shift = 3;
	} else if (src >= tar * 4) {
		*ratio = 4;
		*shift = 2;
	} else if (src >= tar * 2) {
		*ratio = 2;
		*shift = 1;
	} else {
		*ratio = 1;
		*shift = 0;
	}

	return 0;
}

static int fimc_open(struct file *filp)
{
	struct fimc_control *ctrl;
	struct s3c_platform_fimc *pdata;
	int ret;

	ctrl = video_get_drvdata(video_devdata(filp));
	pdata = to_fimc_plat(ctrl->dev);

	mutex_lock(&ctrl->lock);

	if (atomic_read(&ctrl->in_use)) {
		ret = -EBUSY;
		goto resource_busy;
	} else {
		atomic_inc(&ctrl->in_use);
	}

	if (pdata->clk_on)
		pdata->clk_on(to_platform_device(ctrl->dev), ctrl->clk);

	if (pdata->hw_ver == 0x40)
		fimc_hw_reset_camera(ctrl);

	/* Apply things to interface register */
	fimc_hwset_reset(ctrl);
	filp->private_data = ctrl;

	ctrl->fb.open_fifo = s3cfb_open_fifo;
	ctrl->fb.close_fifo = s3cfb_close_fifo;

	ret = s3cfb_direct_ioctl(ctrl->id, S3CFB_GET_LCD_WIDTH,
					(unsigned long)&ctrl->fb.lcd_hres);
	if (ret < 0)
		fimc_err("Fail: S3CFB_GET_LCD_WIDTH\n");

	ret = s3cfb_direct_ioctl(ctrl->id, S3CFB_GET_LCD_HEIGHT,
					(unsigned long)&ctrl->fb.lcd_vres);
	if (ret < 0)
		fimc_err("Fail: S3CFB_GET_LCD_HEIGHT\n");

	ctrl->mem.curr = ctrl->mem.base;
	ctrl->status = FIMC_STREAMOFF;

	mutex_unlock(&ctrl->lock);

	return 0;

resource_busy:
	mutex_unlock(&ctrl->lock);
	return ret;
}

static int fimc_release(struct file *filp)
{
	struct fimc_control *ctrl = filp->private_data;
	struct s3c_platform_fimc *pdata;
	struct fimc_overlay_buf *buf;
	struct mm_struct *mm = current->mm;

	int ret = 0, i;

	ctrl->mem.curr = ctrl->mem.base;

	atomic_dec(&ctrl->in_use);
	filp->private_data = NULL;

	pdata = to_fimc_plat(ctrl->dev);

	/* FIXME: turning off actual working camera */
	if (ctrl->cam) {
		/* shutdown the MCLK */
		clk_disable(ctrl->cam->clk);

		/* shutdown */
		if (ctrl->cam->cam_power)
			ctrl->cam->cam_power(0);

		/* should be initialized at the next open */
		ctrl->cam->initialized = 0;
	}

	if (ctrl->cap) {
		for (i = 0; i < FIMC_CAPBUFS; i++) {
			fimc_dma_free(ctrl, &ctrl->cap->bufs[i], 0);
			fimc_dma_free(ctrl, &ctrl->cap->bufs[i], 1);
		}

		kfree(ctrl->cap);
		ctrl->cap = NULL;
	}

	if (ctrl->out) {
		if (ctrl->status != FIMC_STREAMOFF) {
			ret = fimc_outdev_stop_streaming(ctrl);
			if (ret < 0)
				fimc_err("Fail: fimc_stop_streaming\n");
			ctrl->status = FIMC_STREAMOFF;
		}

		buf = &ctrl->out->overlay.buf;

		for (i = 0; i < FIMC_OUTBUFS; i++) {
			if (buf->vir_addr[i]) {
				ret = do_munmap(mm, buf->vir_addr[i], buf->size[i]);
				if (ret < 0)
					fimc_err("%s: do_munmap fail\n", __func__);
			}
		}

		kfree(ctrl->out);
		ctrl->out = NULL;
	}

	if (pdata->clk_off)
		pdata->clk_off(to_platform_device(ctrl->dev), ctrl->clk);

	fimc_info1("%s: successfully released\n", __func__);

	return 0;
}

static const struct v4l2_file_operations fimc_fops = {
	.owner = THIS_MODULE,
	.open = fimc_open,
	.release = fimc_release,
	.ioctl = video_ioctl2,
	.read = fimc_read,
	.write = fimc_write,
	.mmap = fimc_mmap,
	.poll = fimc_poll,
};

static void fimc_vdev_release(struct video_device *vdev)
{
	kfree(vdev);
}

struct video_device fimc_video_device[FIMC_DEVICES] = {
	[0] = {
		.fops = &fimc_fops,
		.ioctl_ops = &fimc_v4l2_ops,
		.release  = fimc_vdev_release,
	},
	[1] = {
		.fops = &fimc_fops,
		.ioctl_ops = &fimc_v4l2_ops,
		.release  = fimc_vdev_release,
	},
	[2] = {
		.fops = &fimc_fops,
		.ioctl_ops = &fimc_v4l2_ops,
		.release  = fimc_vdev_release,
	},
};

static int fimc_init_global(struct platform_device *pdev)
{
	struct fimc_control *ctrl;
	struct s3c_platform_fimc *pdata;
	struct s3c_platform_camera *cam;
	struct clk *srclk;
	int id, i;

	pdata = to_fimc_plat(&pdev->dev);
	id = pdev->id;
	ctrl = get_fimc_ctrl(id);

	/* Registering external camera modules. re-arrange order to be sure */
	for (i = 0; i < FIMC_MAXCAMS; i++) {
		cam = pdata->camera[i];
		if (!cam)
			break;

		/* WriteBack doesn't need clock setting */
		if(cam->id == CAMERA_WB) { 
			fimc_dev->camera[cam->id] = cam;
			break;
		}
		
		srclk = clk_get(&pdev->dev, cam->srclk_name);
		if (IS_ERR(srclk)) {
			fimc_err("%s: failed to get mclk source\n", __func__);
			return -EINVAL;
		}

		/* mclk */
		cam->clk = clk_get(&pdev->dev, cam->clk_name);
		if (IS_ERR(cam->clk)) {
			fimc_err("%s: failed to get mclk source\n", __func__);
			return -EINVAL;
		}

		if (cam->clk->set_parent) {
			cam->clk->parent = srclk;
			cam->clk->set_parent(cam->clk, srclk);
		}

		/* Assign camera device to fimc */
		fimc_dev->camera[cam->id] = cam;
	}

	fimc_dev->initialized = 1;

	return 0;
}

/*
 * Assign v4l2 device and subdev to fimc
 * it is called per every fimc ctrl registering
 */
static int fimc_configure_subdev(struct platform_device *pdev, int id)
{
	struct s3c_platform_fimc *pdata;
	struct s3c_platform_camera *cam;
	struct i2c_adapter *i2c_adap;
	struct i2c_board_info *i2c_info;
	struct v4l2_subdev *sd;
	struct fimc_control *ctrl;
	unsigned short addr;
	char *name;

	ctrl = get_fimc_ctrl(id);
	pdata = to_fimc_plat(&pdev->dev);
	cam = pdata->camera[id];

	/* Subdev registration */
	if (cam) {
		i2c_adap = i2c_get_adapter(cam->i2c_busnum);
		if (!i2c_adap) {
			fimc_info1("subdev i2c_adapter missing-skip "
							"registration\n");
		}

		i2c_info = cam->info;
		if (!i2c_info) {
			fimc_err("%s: subdev i2c board info missing\n",
								__func__);
			return -ENODEV;
		}

		name = i2c_info->type;
		if (!name) {
			fimc_info1("subdev i2c dirver name missing-skip "
				"registration\n");
			return -ENODEV;
		}

		addr = i2c_info->addr;
		if (!addr) {
			fimc_info1("subdev i2c address missing-skip "
							"registration\n");
			return -ENODEV;
		}

		/*
		 * NOTE: first time subdev being registered,
		 * s_config is called and try to initialize subdev device
		 * but in this point, we are not giving MCLK and power to subdev
		 * so nothing happens but pass platform data through
		 */
		sd = v4l2_i2c_new_subdev_board(&ctrl->v4l2_dev, i2c_adap,
				name, i2c_info, &addr);
		if (!sd) {
			fimc_err("%s: v4l2 subdev board registering failed\n",
				__func__);
		}

		/* Assign camera device to fimc */
		fimc_dev->camera[cam->id] = cam;

		/* Assign subdev to proper camera device pointer */
		fimc_dev->camera[cam->id]->sd = sd;
	}

	return 0;
}

static int fimc_show_log_level(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fimc_control *ctrl;
	struct platform_device *pdev;
	int id = -1;

	char temp[150];

	pdev = to_platform_device(dev);
	id = pdev->id;
	ctrl = get_fimc_ctrl(id);

	sprintf(temp, "\t");
	strcat(buf, temp);
	if (ctrl->log & FIMC_LOG_DEBUG) {
		sprintf(temp, "FIMC_LOG_DEBUG | ");
		strcat(buf, temp);
	}

	if (ctrl->log & FIMC_LOG_INFO_L2) {
		sprintf(temp, "FIMC_LOG_INFO_L2 | ");
		strcat(buf, temp);
	}

	if (ctrl->log & FIMC_LOG_INFO_L1) {
		sprintf(temp, "FIMC_LOG_INFO_L1 | ");
		strcat(buf, temp);
	}
	
	if (ctrl->log & FIMC_LOG_WARN) {
		sprintf(temp, "FIMC_LOG_WARN | ");
		strcat(buf, temp);
	}
	
	if (ctrl->log & FIMC_LOG_ERR) {
		sprintf(temp, "FIMC_LOG_ERR\n");
		strcat(buf, temp);
	}
	
	return strlen(buf);
}

static int fimc_store_log_level(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct fimc_control *ctrl;
	struct platform_device *pdev;

	const char *p = buf;
	char msg[150] = {0, };
	int id = -1;
	u32 match = 0;

	pdev = to_platform_device(dev);
	id = pdev->id;
	ctrl = get_fimc_ctrl(id);

	while (*p != '\0') {
		if (!isspace(*p))
			strncat(msg, p, 1);
		p++;
	}

	ctrl->log = 0;
	printk(KERN_INFO "FIMC.%d log level is set as below.\n", id);

	if (strstr(msg, "FIMC_LOG_ERR") != NULL) {
		ctrl->log |= FIMC_LOG_ERR;
		match = 1;
		printk(KERN_INFO "\tFIMC_LOG_ERR\n");
	}

	if (strstr(msg, "FIMC_LOG_WARN") != NULL) {
		ctrl->log |= FIMC_LOG_WARN;
		match = 1;
		printk(KERN_INFO "\tFIMC_LOG_WARN\n");
	}

	if (strstr(msg, "FIMC_LOG_INFO_L1") != NULL) {
		ctrl->log |= FIMC_LOG_INFO_L1;
		match = 1;
		printk(KERN_INFO "\tFIMC_LOG_INFO_L1\n");
	}

	if (strstr(msg, "FIMC_LOG_INFO_L2") != NULL) {
		ctrl->log |= FIMC_LOG_INFO_L2;
		match = 1;
		printk(KERN_INFO "\tFIMC_LOG_INFO_L2\n");
	}

	if (strstr(msg, "FIMC_LOG_DEBUG") != NULL) {
		ctrl->log |= FIMC_LOG_DEBUG;
		match = 1;
		printk(KERN_INFO "\tFIMC_LOG_DEBUG\n");
	}
	
	if (!match) {
		printk(KERN_INFO "FIMC_LOG_ERR		\t: Error condition.\n");
		printk(KERN_INFO "FIMC_LOG_WARN		\t: WARNING condition.\n");
		printk(KERN_INFO "FIMC_LOG_INFO_L1	\t: V4L2 API without QBUF, DQBUF.\n");
		printk(KERN_INFO "FIMC_LOG_INFO_L2	\t: V4L2 API QBUF, DQBUF.\n");
		printk(KERN_INFO "FIMC_LOG_DEBUG		\t: Queue status report.\n");
	}

	return len;
}

static DEVICE_ATTR(log_level, 0644, \
			fimc_show_log_level,
			fimc_store_log_level);

static int __devinit fimc_probe(struct platform_device *pdev)
{
	struct s3c_platform_fimc *pdata;
	struct fimc_control *ctrl;
	int ret;

	if (!fimc_dev) {
		fimc_dev = kzalloc(sizeof(*fimc_dev), GFP_KERNEL);
		if (!fimc_dev) {
			dev_err(&pdev->dev, "%s: not enough memory\n",
				__func__);
			goto err_fimc;
		}
	}

	ctrl = fimc_register_controller(pdev);
	if (!ctrl) {
		printk(KERN_ERR "%s: cannot register fimc\n", __func__);
		goto err_fimc;
	}

	pdata = to_fimc_plat(&pdev->dev);
	if (pdata->cfg_gpio)
		pdata->cfg_gpio(pdev);

	/* V4L2 device-subdev registration */
	ret = v4l2_device_register(&pdev->dev, &ctrl->v4l2_dev);
	if (ret) {
		fimc_err("%s: v4l2 device register failed\n", __func__);
		goto err_v4l2;
	}

	/* things to initialize once */
	if (!fimc_dev->initialized) {
		ret = fimc_init_global(pdev);
		if (ret)
			goto err_global;
	}

	/* v4l2 subdev configuration */
	ret = fimc_configure_subdev(pdev, ctrl->id);
	if (ret) {
		fimc_err("%s: subdev[%d] registering failed\n",
							__func__, ctrl->id);
	}

	/* video device register */
	ret = video_register_device(ctrl->vd, VFL_TYPE_GRABBER, ctrl->id);
	if (ret) {
		fimc_err("%s: cannot register video driver\n", __func__);
		goto err_global;
	}

	video_set_drvdata(ctrl->vd, ctrl);

	ret = device_create_file(&(pdev->dev), &dev_attr_log_level);
	if (ret < 0)
		fimc_err("failed to add sysfs entries\n");

	fimc_info1("controller %d registered successfully\n", ctrl->id);

	return 0;

err_global:
	clk_disable(ctrl->clk);
	clk_put(ctrl->clk);

err_v4l2:
	fimc_unregister_controller(pdev);

err_fimc:
	return -EINVAL;

}

static int fimc_remove(struct platform_device *pdev)
{
	fimc_unregister_controller(pdev);

	device_remove_file(&(pdev->dev), &dev_attr_log_level);

	if (fimc_dev) {
		kfree(fimc_dev);
		fimc_dev = NULL;
	}

	return 0;
}

#ifdef CONFIG_PM
static inline int fimc_suspend_out(struct fimc_control *ctrl)
{
	switch (ctrl->out->overlay.mode) {
	case FIMC_OVERLAY_NONE:	/* fall through */
	case FIMC_OVERLAY_DMA_AUTO:
	case FIMC_OVERLAY_DMA_MANUAL:
		if (ctrl->status == FIMC_STREAMON) {
			if (ctrl->out->in_queue[0] != -1)
				fimc_err("[%s : %d] in queue status isn't stable\n",
					__func__, __LINE__);

			if ((ctrl->out->idx.next != -1) || (ctrl->out->idx.prev != -1))
				fimc_err("[%s : %d] FIMC status isn't stable\n",
					__func__, __LINE__);

			fimc_outdev_stop_streaming(ctrl);

			if (ctrl->status == FIMC_STREAMON_IDLE)
				ctrl->status = FIMC_ON_IDLE_SLEEP;
			else
				ctrl->status = FIMC_ON_SLEEP;
		} else if (ctrl->status == FIMC_STREAMON_IDLE) {
			fimc_outdev_stop_streaming(ctrl);
			ctrl->status = FIMC_ON_IDLE_SLEEP;
		} else {
			ctrl->status = FIMC_OFF_SLEEP;
		}

		break;

	case FIMC_OVERLAY_FIFO:
		if (ctrl->status == FIMC_STREAMON) {
			if (ctrl->out->in_queue[0] != -1)
				fimc_err("[%s : %d] in queue status isn't stable\n",
					__func__, __LINE__);

			if ((ctrl->out->idx.next != -1) || (ctrl->out->idx.prev != -1))
				fimc_err("[%s : %d] FIMC status isn't stable\n",
					__func__, __LINE__);

			fimc_outdev_stop_streaming(ctrl);

			if (ctrl->status == FIMC_STREAMON_IDLE)
				ctrl->status = FIMC_ON_IDLE_SLEEP;
			else
				ctrl->status = FIMC_ON_SLEEP;
		} else {
			ctrl->status = FIMC_OFF_SLEEP;
		}

		break;
	}

	return 0;
}

static inline int fimc_suspend_cap(struct fimc_control *ctrl)
{
	if (ctrl->cam->id == CAMERA_WB && ctrl->status == FIMC_STREAMON)
		fimc_streamoff_capture((void*)ctrl);
	ctrl->status = FIMC_ON_SLEEP;

	return 0;
}

int fimc_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct fimc_control *ctrl;
	struct s3c_platform_fimc *pdata;
	int id;

	id = pdev->id;
	ctrl = get_fimc_ctrl(id);
	pdata = to_fimc_plat(ctrl->dev);

	if (ctrl->out)
		fimc_suspend_out(ctrl);

	else if (ctrl->cap)
		fimc_suspend_cap(ctrl);
	else
		ctrl->status = FIMC_OFF_SLEEP;

	if (pdata->clk_off)
		pdata->clk_off(pdev, ctrl->clk);

	return 0;
}

static inline int fimc_resume_out(struct fimc_control *ctrl)
{
        int index = -1, ret = -1;

	switch (ctrl->out->overlay.mode) {
	case FIMC_OVERLAY_NONE:
		if (ctrl->status == FIMC_ON_IDLE_SLEEP) {
		        ret = fimc_outdev_set_param(ctrl);
		        if (ret < 0)
		                fimc_err("Fail: fimc_outdev_set_param\n");

			ctrl->status = FIMC_STREAMON_IDLE;
		} else if (ctrl->status == FIMC_OFF_SLEEP) {
			ctrl->status = FIMC_STREAMOFF;
		} else {
			fimc_err("%s: Undefined status : %d\n", 
						__func__, ctrl->status);
		}
		break;

	case FIMC_OVERLAY_DMA_AUTO:
		if (ctrl->status == FIMC_ON_IDLE_SLEEP) {
			fimc_outdev_resume_dma(ctrl);
		        ret = fimc_outdev_set_param(ctrl);
		        if (ret < 0)
		                fimc_err("Fail: fimc_outdev_set_param\n");

			ctrl->status = FIMC_STREAMON_IDLE;

		} else if (ctrl->status == FIMC_OFF_SLEEP) {
			ctrl->status = FIMC_STREAMOFF;
		} else {
			fimc_err("%s: Undefined status : %d\n", 
						__func__, ctrl->status);
		}

		break;

	case FIMC_OVERLAY_DMA_MANUAL:
		if (ctrl->status == FIMC_ON_IDLE_SLEEP) {
		        ret = fimc_outdev_set_param(ctrl);
		        if (ret < 0)
		                fimc_err("Fail: fimc_outdev_set_param\n");

			ctrl->status = FIMC_STREAMON_IDLE;

		} else if (ctrl->status == FIMC_OFF_SLEEP) {
			ctrl->status = FIMC_STREAMOFF;
		} else {
			fimc_err("%s: Undefined status : %d\n", 
						__func__, ctrl->status);
		}

		break;

	case FIMC_OVERLAY_FIFO:
		if (ctrl->status == FIMC_ON_SLEEP) {
		        ctrl->status = FIMC_READY_ON;

		        ret = fimc_outdev_set_param(ctrl);
		        if (ret < 0)
		                fimc_err("Fail: fimc_outdev_set_param\n");

#if defined(CONFIG_VIDEO_IPC)
	                if (ctrl->out->pix.field == V4L2_FIELD_INTERLACED_TB)
	                        ipc_start();
#endif
			index = ctrl->out->idx.active;
	                fimc_outdev_set_src_addr(ctrl, ctrl->out->src[index].base);

	                ret = fimc_start_fifo(ctrl);
	                if (ret < 0)
	                        fimc_err("Fail: fimc_start_fifo\n");

	                ctrl->status = FIMC_STREAMON;
		} else if (ctrl->status == FIMC_OFF_SLEEP) {
			ctrl->status = FIMC_STREAMOFF;
		} else {
			fimc_err("%s: Undefined status : %d\n", __func__, ctrl->status);
		}

		break;
	}

	return 0;
}

static inline int fimc_resume_cap(struct fimc_control *ctrl)
{
	if (ctrl->cam->id == CAMERA_WB)
		fimc_streamon_capture((void*)ctrl);

	return 0;
}

int fimc_resume(struct platform_device *pdev)
{
	struct fimc_control *ctrl;
	struct s3c_platform_fimc *pdata;
  	int id = pdev->id;

	ctrl = get_fimc_ctrl(id);
	pdata = to_fimc_plat(ctrl->dev);

	if (pdata->clk_on)
		pdata->clk_on(pdev, ctrl->clk);

	if (ctrl->out)
		fimc_resume_out(ctrl);

	else if (ctrl->cap)
		fimc_resume_cap(ctrl);
	else
		ctrl->status = FIMC_STREAMOFF;

	return 0;
}
#else
#define fimc_suspend	NULL
#define fimc_resume	NULL
#endif

static struct platform_driver fimc_driver = {
	.probe		= fimc_probe,
	.remove		= fimc_remove,
	.suspend	= fimc_suspend,
	.resume		= fimc_resume,
	.driver		= {
		.name	= FIMC_NAME,
		.owner	= THIS_MODULE,
	},
};

static int fimc_register(void)
{
	platform_driver_register(&fimc_driver);

	return 0;
}

static void fimc_unregister(void)
{
	platform_driver_unregister(&fimc_driver);
}

late_initcall(fimc_register);
module_exit(fimc_unregister);

MODULE_AUTHOR("Dongsoo, Kim <dongsoo45.kim@samsung.com>");
MODULE_AUTHOR("Jinsung, Yang <jsgood.yang@samsung.com>");
MODULE_AUTHOR("Jonghun, Han <jonghun.han@samsung.com>");
MODULE_DESCRIPTION("Samsung Camera Interface (FIMC) driver");
MODULE_LICENSE("GPL");

