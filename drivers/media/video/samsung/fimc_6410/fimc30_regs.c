/* linux/drivers/media/video/samsung/s3c_fimc4x_regs.c
 *
 * Register interface file for Samsung Camera Interface (FIMC) driver
 *
 * Jinsung Yang, Copyright (c) 2009 Samsung Electronics
 * 	http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/videodev2.h>
#include <linux/videodev2_samsung.h>
#include <linux/io.h>
#include <mach/map.h>
#include <plat/regs-fimc.h>
#include <plat/fimc.h>

#include "fimc.h"

/* struct fimc_limit: Limits for FIMC */
struct fimc_limit fimc_limits[FIMC_DEVICES] = {
	{
		.pre_dst_w	= 3264,
		.bypass_w	= 8192,
		.trg_h_no_rot	= 3264,
		.trg_h_rot	= 1280,
		.real_w_no_rot	= 8192,
		.real_h_rot	= 1280,
	}, {
		.pre_dst_w	= 1280,
		.bypass_w	= 8192,
		.trg_h_no_rot	= 1280,
		.trg_h_rot	= 8192,
		.real_w_no_rot	= 8192,
		.real_h_rot	= 768,
	}, {
		.pre_dst_w	= 1440,
		.bypass_w	= 8192,
		.trg_h_no_rot	= 1440,
		.trg_h_rot	= 0,
		.real_w_no_rot	= 8192,
		.real_h_rot	= 0,
	},
};

int fimc_hwset_camera_source(struct fimc_control *ctrl)
{
	struct s3c_platform_camera *cam = ctrl->cam;
	u32 cfg = 0;

	/* for now, we support only ITU601 8 bit mode */
	cfg |= S3C_CISRCFMT_ITU601_8BIT;
	cfg |= cam->order422;

	if (cam->type == CAM_TYPE_ITU)
		cfg |= cam->fmt;

	cfg |= S3C_CISRCFMT_SOURCEHSIZE(cam->width);
	cfg |= S3C_CISRCFMT_SOURCEVSIZE(cam->height);

	writel(cfg, ctrl->regs + S3C_CISRCFMT);

	return 0;
}

int fimc_hwset_enable_irq(struct fimc_control *ctrl, int overflow, int level)
{
	u32 cfg = readl(ctrl->regs + S3C_CIGCTRL);

	cfg &= ~(S3C_CIGCTRL_IRQ_OVFEN | S3C_CIGCTRL_IRQ_LEVEL);
	//cfg |= S3C_CIGCTRL_IRQ_ENABLE;

	if (overflow)
		cfg |= S3C_CIGCTRL_IRQ_OVFEN;

	if (level)
		cfg |= S3C_CIGCTRL_IRQ_LEVEL;

	writel(cfg, ctrl->regs + S3C_CIGCTRL);

	return 0;
}

int fimc_hwset_disable_irq(struct fimc_control *ctrl)
{
	u32 cfg = readl(ctrl->regs + S3C_CIGCTRL);

	cfg &= ~S3C_CIGCTRL_IRQ_OVFEN;
	writel(cfg, ctrl->regs + S3C_CIGCTRL);

	return 0;
}

int fimc_hwset_clear_irq(struct fimc_control *ctrl)
{
	u32 cfg = readl(ctrl->regs + S3C_CIGCTRL);

	cfg |= S3C_CIGCTRL_IRQ_CLR;

	writel(cfg, ctrl->regs + S3C_CIGCTRL);

	return 0;
}

static void fimc_reset_cfg(struct fimc_control *ctrl)
{
	int i;
	u32 cfg[][2] = {
		{ 0x018, 0x00000000 }, { 0x01c, 0x00000000 },
		{ 0x020, 0x00000000 }, { 0x024, 0x00000000 },
		{ 0x028, 0x00000000 }, { 0x02c, 0x00000000 },
		{ 0x030, 0x00000000 }, { 0x034, 0x00000000 },
		{ 0x038, 0x00000000 }, { 0x03c, 0x00000000 },
		{ 0x040, 0x00000000 }, { 0x044, 0x00000000 },
		{ 0x048, 0x00000000 }, { 0x04c, 0x00000000 },
		{ 0x050, 0x00000000 }, { 0x054, 0x00000000 },
		{ 0x058, 0x18000000 }, { 0x05c, 0x00000000 },
		{ 0x068, 0x00000000 }, { 0x06c, 0x00000000 },
		{ 0x070, 0x00000000 }, { 0x074, 0x00000000 },
		{ 0x078, 0x18000000 }, { 0x07c, 0x00000000 },
		{ 0x080, 0x00000000 }, { 0x084, 0x00000000 },
		{ 0x088, 0x00000000 }, { 0x08c, 0x00000000 },
		{ 0x090, 0x00000000 }, { 0x094, 0x00000000 },
		{ 0x098, 0x18000000 }, { 0x0a0, 0x00000000 },
		{ 0x0a4, 0x00000000 }, { 0x0a8, 0x00000000 },
		{ 0x0ac, 0x18000000 }, { 0x0b0, 0x00000000 },
		{ 0x0c0, 0x00000000 }, { 0x0c4, 0xffffffff },
		{ 0x0d0, 0x00100080 }, { 0x0d4, 0x00000000 },
		{ 0x0d8, 0x00000000 }, { 0x0dc, 0x00000000 }, 
		{ 0x0e0, 0x00000000 }, { 0x0e4, 0x00000000 },
		{ 0x0e8, 0x00000000 }, { 0x0ec, 0x00000000 },
		{ 0x0f0, 0x00000000 }, { 0x0f4, 0x00000000 },
		{ 0x0f8, 0x00000000 }, { 0x0fc, 0x00000000 },
		{ 0x100, 0x00000000 }, { 0x104, 0x00000000 },
		{ 0x108, 0x00000000 }, { 0x10c, 0x00000000 },
		{ 0x110, 0x00000000 }, { 0x114, 0x00000000 },
		{ 0x118, 0x00000000 }, { 0x11c, 0x00000000 },
		{ 0x120, 0x00000000 }, { 0x124, 0x00000000 },
		{ 0x128, 0x00000000 }, { 0x12c, 0x00000000 },
		{ 0x130, 0x00000000 }, { 0x134, 0x00000000 },
		{ 0x138, 0x00000000 }, { 0x13c, 0x00000000 },
		{ 0x140, 0x00000000 },
	};

	for (i = 0; i < sizeof(cfg) / 8; i++)
		writel(cfg[i][1], ctrl->regs + cfg[i][0]);
}

int fimc_hwset_reset(struct fimc_control *ctrl)
{
	u32 cfg = 0;

	cfg = readl(ctrl->regs + S3C_CISRCFMT);
	cfg |= S3C_CISRCFMT_ITU601_8BIT;
	writel(cfg, ctrl->regs + S3C_CISRCFMT);

	/* s/w reset */
	cfg = readl(ctrl->regs + S3C_CIGCTRL);
	cfg |= (S3C_CIGCTRL_SWRST | S3C_CIGCTRL_IRQ_LEVEL);
	writel(cfg, ctrl->regs + S3C_CIGCTRL);
	mdelay(1);

	cfg = readl(ctrl->regs + S3C_CIGCTRL);
	cfg &= ~S3C_CIGCTRL_SWRST;
	writel(cfg, ctrl->regs + S3C_CIGCTRL);

	/* in case of ITU656, CISRCFMT[31] should be 0 */
	if ((ctrl->cap != NULL) && (ctrl->cam->fmt == ITU_656_YCBCR422_8BIT)) {
		cfg = readl(ctrl->regs + S3C_CISRCFMT);
		cfg &= ~S3C_CISRCFMT_ITU601_8BIT;
		writel(cfg, ctrl->regs + S3C_CISRCFMT);
	}

	fimc_reset_cfg(ctrl);

	return 0;
}

int fimc_hwget_overflow_state(struct fimc_control *ctrl)
{
	u32 cfg, status, flag;

	status = readl(ctrl->regs + S3C_CISTATUS);
	flag = S3C_CISTATUS_OVFIY | S3C_CISTATUS_OVFICB | S3C_CISTATUS_OVFICR;

	if (status & flag) {
		cfg = readl(ctrl->regs + S3C_CIWDOFST);
		cfg |= (S3C_CIWDOFST_CLROVFIY | S3C_CIWDOFST_CLROVFICB |
			S3C_CIWDOFST_CLROVFICR);
		writel(cfg, ctrl->regs + S3C_CIWDOFST);

		cfg = readl(ctrl->regs + S3C_CIWDOFST);
		cfg &= ~(S3C_CIWDOFST_CLROVFIY | S3C_CIWDOFST_CLROVFICB |
			S3C_CIWDOFST_CLROVFICR);
		writel(cfg, ctrl->regs + S3C_CIWDOFST);

		return 1;
	}

	return 0;
}

int fimc_hwset_camera_offset(struct fimc_control *ctrl)
{
	struct s3c_platform_camera *cam = ctrl->cam;
	struct v4l2_rect *rect = &cam->window;
	u32 cfg, h1, h2, v1, v2;

	if (!cam) {
		dev_err(ctrl->dev, "%s: no active camera\n",
			__func__);
		return -ENODEV;
	}

	h1 = rect->left;
	h2 = cam->width - rect->width - rect->left;
	v1 = rect->top;
	v2 = cam->height - rect->height - rect->top;

	cfg = readl(ctrl->regs + S3C_CIWDOFST);
	cfg &= ~(S3C_CIWDOFST_WINHOROFST_MASK | S3C_CIWDOFST_WINVEROFST_MASK);
	cfg |= S3C_CIWDOFST_WINHOROFST(h1);
	cfg |= S3C_CIWDOFST_WINVEROFST(v1);
	cfg |= S3C_CIWDOFST_WINOFSEN;
	writel(cfg, ctrl->regs + S3C_CIWDOFST);

	cfg = 0;
	cfg |= S3C_CIWDOFST2_WINHOROFST2(h2);
	cfg |= S3C_CIWDOFST2_WINVEROFST2(v2);
	writel(cfg, ctrl->regs + S3C_CIWDOFST2);

	return 0;
}

int fimc_hwset_camera_polarity(struct fimc_control *ctrl)
{
	struct s3c_platform_camera *cam = ctrl->cam;
	u32 cfg;

	if (!cam) {
		dev_err(ctrl->dev, "%s: no active camera\n",
			__func__);
		return -ENODEV;
	}

	cfg = readl(ctrl->regs + S3C_CIGCTRL);

	cfg &= ~(S3C_CIGCTRL_INVPOLPCLK | S3C_CIGCTRL_INVPOLVSYNC |
		 S3C_CIGCTRL_INVPOLHREF);

	if (cam->inv_pclk)
		cfg |= S3C_CIGCTRL_INVPOLPCLK;

	if (cam->inv_vsync)
		cfg |= S3C_CIGCTRL_INVPOLVSYNC;

	if (cam->inv_href)
		cfg |= S3C_CIGCTRL_INVPOLHREF;

	writel(cfg, ctrl->regs + S3C_CIGCTRL);

	return 0;
}

int fimc_hwset_camera_type(struct fimc_control *ctrl)
{
	struct s3c_platform_camera *cam = ctrl->cam;
	u32 cfg;

	if (!cam) {
		dev_err(ctrl->dev, "%s: no active camera\n",
			__func__);
		return -ENODEV;
	}

	cfg = readl(ctrl->regs + S3C_CIGCTRL);
	cfg &= ~S3C_CIGCTRL_TESTPATTERN_MASK;

	writel(cfg, ctrl->regs + S3C_CIGCTRL);

	return 0;
}

int fimc_hwset_test_pattern(struct fimc_control *ctrl, int type)
{
	struct s3c_platform_camera *cam = ctrl->cam;
	u32 cfg;

	if (!cam) {
		dev_err(ctrl->dev, "%s: no active camera\n",
			__func__);
		return -ENODEV;
	}

	cfg = readl(ctrl->regs + S3C_CIGCTRL);
	cfg &= ~S3C_CIGCTRL_TESTPATTERN_MASK;
	cfg |= type << S3C_CIGCTRL_TESTPATTERN_SHIFT;

	writel(cfg, ctrl->regs + S3C_CIGCTRL);

	return 0;
}

int fimc_hwset_output_size(struct fimc_control *ctrl, int width, int height)
{
	u32 cfg = readl(ctrl->regs + S3C_CITRGFMT);

	cfg &= ~(S3C_CITRGFMT_TARGETH_MASK | S3C_CITRGFMT_TARGETV_MASK);

	cfg |= S3C_CITRGFMT_TARGETHSIZE(width);
	cfg |= S3C_CITRGFMT_TARGETVSIZE(height);

	writel(cfg, ctrl->regs + S3C_CITRGFMT);

	return 0;
}

int fimc_hwset_output_colorspace(struct fimc_control *ctrl, u32 pixelformat)
{
	u32 cfg;

	cfg = readl(ctrl->regs + S3C_CITRGFMT);
	cfg &= ~S3C_CITRGFMT_OUTFORMAT_MASK;

	switch (pixelformat) {
	case V4L2_PIX_FMT_RGB565:	/* fall through */
	case V4L2_PIX_FMT_RGB32:
		cfg |= S3C_CITRGFMT_OUTFORMAT_RGB;
		break;

	case V4L2_PIX_FMT_YUYV:		/* fall through */
	case V4L2_PIX_FMT_UYVY:		/* fall through */
	case V4L2_PIX_FMT_VYUY:		/* fall through */
	case V4L2_PIX_FMT_YVYU:		/* fall through */
		cfg |= S3C_CITRGFMT_OUTFORMAT_YCBCR422_1PLANE;
		break;

	case V4L2_PIX_FMT_YUV422P:
		cfg |= S3C_CITRGFMT_OUTFORMAT_YCBCR422;
		break;

	case V4L2_PIX_FMT_YUV420:	/* fall through */
	case V4L2_PIX_FMT_NV12:		/* fall through */
	case V4L2_PIX_FMT_NV21:		/* fall through */
	case V4L2_PIX_FMT_NV16:		/* fall through */
	case V4L2_PIX_FMT_NV61:
		cfg |= S3C_CITRGFMT_OUTFORMAT_YCBCR420;
		break;

	default:
		dev_err(ctrl->dev, "%s: invalid pixel format\n", __func__);
		break;
	}

	writel(cfg, ctrl->regs + S3C_CITRGFMT);

	return 0;
}

/* FIXME */
int fimc_hwset_output_rot_flip(struct fimc_control *ctrl, u32 rot, u32 flip)
{
	u32 cfg, val;

	cfg = readl(ctrl->regs + S3C_CITRGFMT);
	cfg &= ~S3C_CITRGFMT_FLIP_MASK;
	//cfg &= ~S3C_CITRGFMT_OUTROT90_CLOCKWISE;

	val = fimc_mapping_rot_flip(rot, flip);

	//if (val & FIMC_ROT)
	//	cfg |= S3C_CITRGFMT_OUTROT90_CLOCKWISE;

	if (val & FIMC_XFLIP)
		cfg |= S3C_CITRGFMT_FLIP_X_MIRROR;

	if (val & FIMC_YFLIP)
		cfg |= S3C_CITRGFMT_FLIP_Y_MIRROR;

	writel(cfg, ctrl->regs + S3C_CITRGFMT);

	return 0;
}

int fimc_hwset_output_area(struct fimc_control *ctrl, u32 width, u32 height)
{
	u32 cfg = 0;

	cfg = S3C_CITAREA_TARGET_AREA(width * height);
	writel(cfg, ctrl->regs + S3C_CITAREA);

	return 0;
}

int fimc_hwset_enable_lastirq(struct fimc_control *ctrl)
{
	u32 cfg = readl(ctrl->regs + S3C_CIOCTRL);

	cfg |= S3C_CIOCTRL_LASTIRQ_ENABLE;
	writel(cfg, ctrl->regs + S3C_CIOCTRL);

	return 0;
}

int fimc_hwset_disable_lastirq(struct fimc_control *ctrl)
{
	u32 cfg = readl(ctrl->regs + S3C_CIOCTRL);

	cfg &= ~S3C_CIOCTRL_LASTIRQ_ENABLE;
	writel(cfg, ctrl->regs + S3C_CIOCTRL);

	return 0;
}

int fimc_hwset_prescaler(struct fimc_control *ctrl)
{
	struct fimc_scaler *sc = &ctrl->sc;
	u32 cfg = 0, shfactor;

	shfactor = 10 - (sc->hfactor + sc->vfactor);

	cfg |= S3C_CISCPRERATIO_SHFACTOR(shfactor);
	cfg |= S3C_CISCPRERATIO_PREHORRATIO(sc->pre_hratio);
	cfg |= S3C_CISCPRERATIO_PREVERRATIO(sc->pre_vratio);

	writel(cfg, ctrl->regs + S3C_CISCPRERATIO);

	cfg = 0;
	cfg |= S3C_CISCPREDST_PREDSTWIDTH(sc->pre_dst_width);
	cfg |= S3C_CISCPREDST_PREDSTHEIGHT(sc->pre_dst_height);

	writel(cfg, ctrl->regs + S3C_CISCPREDST);

	return 0;
}

int fimc_hwset_output_address(struct fimc_control *ctrl,
				struct fimc_buf_set *bs, int id)
{
	writel(bs->base[FIMC_ADDR_Y], ctrl->regs + S3C_CIOYSA(id));
	writel(bs->base[FIMC_ADDR_CB], ctrl->regs + S3C_CIOCBSA(id));
	writel(bs->base[FIMC_ADDR_CR], ctrl->regs + S3C_CIOCRSA(id));

	return 0;
}

static void s3c_fimc_get_burst_422i(u32 width, u32 *mburst, u32 *rburst)
{
	unsigned int tmp, wanted;

	tmp = (width / 2) & 0xf;

	switch (tmp) {
	case 0:
		wanted = 16;
		break;

	case 4:
		wanted = 4;
		break;

	case 8:
		wanted = 8;
		break;

	default:
		wanted = 4;
		break;
	}

	*mburst = wanted / 2;
	*rburst = wanted / 2;
}

static void s3c_fimc_get_burst(u32 width, u32 *mburst, u32 *rburst)
{
	unsigned int tmp;

	tmp = (width / 4) & 0xf;

	switch (tmp) {
	case 0:
		*mburst = 16;
		*rburst = 16;
		break;

	case 4:
		*mburst = 16;
		*rburst = 4;
		break;

	case 8:
		*mburst = 16;
		*rburst = 8;
		break;

	default:
		tmp = (width / 4) % 8;

		if (tmp == 0) {
			*mburst = 8;
			*rburst = 8;
		} else if (tmp == 4) {
			*mburst = 8;
			*rburst = 4;
		} else {
			tmp = (width / 4) % 4;
			*mburst = 4;
			*rburst = (tmp) ? tmp : 4;
		}

		break;
	}
}

int fimc_hwset_output_yuv(struct fimc_control *ctrl, struct v4l2_pix_format *fmt)
{
	u32 cfg;
	u32 yburst_m, yburst_r, cburst_m, cburst_r;
	
	cfg = readl(ctrl->regs + S3C_CIOCTRL);
	cfg &= ~(S3C_CICOCTRL_BURST_MASK | S3C_CIOCTRL_ORDER422_MASK);

	switch (fmt->pixelformat) {
	/* 1 plane formats */
	case V4L2_PIX_FMT_YUYV:
		cfg |= S3C_CIOCTRL_ORDER422_YCBYCR;
		break;

	case V4L2_PIX_FMT_UYVY:
		cfg |= S3C_CIOCTRL_ORDER422_CBYCRY;
		break;

	case V4L2_PIX_FMT_VYUY:
		cfg |= S3C_CIOCTRL_ORDER422_CRYCBY;
		break;

	case V4L2_PIX_FMT_YVYU:
		cfg |= S3C_CIOCTRL_ORDER422_YCRYCB;
		break;

	default:
		break;
	}
	
	switch (fmt->pixelformat) {
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_YVYU:
		s3c_fimc_get_burst_422i(fmt->width, &yburst_m, &yburst_r);
		cburst_m = yburst_m / 2;
		cburst_r = yburst_r / 2;
		break;
/* no consider case of YUV422P */
#if 0
	case V4L2_PIX_FMT_YUV422P:		
		s3c_fimc_get_burst_422i(fmt->width/2, &yburst_m, &yburst_r);
		cburst_m = yburst_m;
		cburst_r = yburst_r / 2;
		break;
#endif
	default:
		s3c_fimc_get_burst(fmt->width, &yburst_m, &yburst_r);
		s3c_fimc_get_burst(fmt->width / 2, &cburst_m, &cburst_r);
		break;

	}

	cfg |= (S3C_CICOCTRL_YBURST1(yburst_m) | S3C_CICOCTRL_YBURST2(yburst_r));
	cfg |= (S3C_CICOCTRL_CBURST1(cburst_m) | S3C_CICOCTRL_CBURST2(cburst_r));

	writel(cfg, ctrl->regs + S3C_CIOCTRL);

	return 0;
}

int fimc_hwset_output_scan(struct fimc_control *ctrl, struct v4l2_pix_format *fmt)
{
	/* nothing to do: not supported interlaced and weave output */

	return 0;
}

int fimc_hwset_scaler(struct fimc_control *ctrl)
{
	u32 cfg = readl(ctrl->regs + S3C_CISCCTRL);

	cfg &= ~(S3C_CISCCTRL_SCALERBYPASS |
		S3C_CISCCTRL_SCALEUP_H | S3C_CISCCTRL_SCALEUP_V |
		S3C_CISCCTRL_MAIN_V_RATIO_MASK |
		S3C_CISCCTRL_MAIN_H_RATIO_MASK);
	cfg |= (S3C_CISCCTRL_CSCR2Y_WIDE | S3C_CISCCTRL_CSCY2R_WIDE);

	if (ctrl->sc.bypass)
		cfg |= S3C_CISCCTRL_SCALERBYPASS;

	if (ctrl->sc.scaleup_h)
		cfg |= S3C_CISCCTRL_SCALEUP_H;

	if (ctrl->sc.scaleup_v)
		cfg |= S3C_CISCCTRL_SCALEUP_V;

	cfg |= S3C_CISCCTRL_MAINHORRATIO(ctrl->sc.main_hratio);
	cfg |= S3C_CISCCTRL_MAINVERRATIO(ctrl->sc.main_vratio);

	writel(cfg, ctrl->regs + S3C_CISCCTRL);

	return 0;
}

int fimc_hwset_enable_lcdfifo(struct fimc_control *ctrl)
{
	u32 cfg = readl(ctrl->regs + S3C_CISCCTRL);

	cfg |= S3C_CISCCTRL_LCDPATHEN_FIFO;
	writel(cfg, ctrl->regs + S3C_CISCCTRL);

	return 0;
}

int fimc_hwset_disable_lcdfifo(struct fimc_control *ctrl)
{
	u32 cfg = readl(ctrl->regs + S3C_CISCCTRL);

	cfg &= ~S3C_CISCCTRL_LCDPATHEN_FIFO;
	writel(cfg, ctrl->regs + S3C_CISCCTRL);

	return 0;
}

int fimc_hwget_frame_count(struct fimc_control *ctrl)
{
	return S3C_CISTATUS_GET_FRAME_COUNT(readl(ctrl->regs + S3C_CISTATUS));
}

int fimc_hwget_frame_end(struct fimc_control *ctrl)
{
	unsigned long timeo = jiffies;	
	unsigned long frame_cnt = 0;
	u32 cfg;

	timeo += 20;	/* waiting for 100ms */
	while (time_before(jiffies, timeo)) {
		cfg = readl(ctrl->regs + S3C_CISTATUS);

		if (S3C_CISTATUS_GET_FRAME_END(cfg)) {
			cfg &= ~S3C_CISTATUS_FRAMEEND;
			writel(cfg, ctrl->regs + S3C_CISTATUS);
			
			if (frame_cnt == 2)
				break;
			else
				frame_cnt++;
		}
		cond_resched();
	}

	return 0;
}

int fimc_hwset_start_scaler(struct fimc_control *ctrl)
{
	u32 cfg = readl(ctrl->regs + S3C_CISCCTRL);

	cfg |= S3C_CISCCTRL_SCALERSTART;
	writel(cfg, ctrl->regs + S3C_CISCCTRL);

	return 0;
}

int fimc_hwset_stop_scaler(struct fimc_control *ctrl)
{
	u32 cfg = readl(ctrl->regs + S3C_CISCCTRL);

	cfg &= ~S3C_CISCCTRL_SCALERSTART;
	writel(cfg, ctrl->regs + S3C_CISCCTRL);

	return 0;
}

int fimc_hwset_input_rgb(struct fimc_control *ctrl, u32 pixelformat)
{
	u32 cfg = readl(ctrl->regs + S3C_CISCCTRL);
	cfg &= ~S3C_CISCCTRL_INRGB_FMT_RGB_MASK;

	if (pixelformat == V4L2_PIX_FMT_RGB32)
		cfg |= S3C_CISCCTRL_INRGB_FMT_RGB888;
	else if (pixelformat == V4L2_PIX_FMT_RGB565)
		cfg |= S3C_CISCCTRL_INRGB_FMT_RGB565;

	writel(cfg, ctrl->regs + S3C_CISCCTRL);

	return 0;
}

int fimc_hwset_intput_field(struct fimc_control *ctrl, enum v4l2_field field)
{
	return 0;
}

int fimc_hwset_output_rgb(struct fimc_control *ctrl, u32 pixelformat)
{
	u32 cfg = readl(ctrl->regs + S3C_CISCCTRL);
	cfg &= ~S3C_CISCCTRL_OUTRGB_FMT_RGB_MASK;

	if (pixelformat == V4L2_PIX_FMT_RGB32)
		cfg |= S3C_CISCCTRL_OUTRGB_FMT_RGB888;
	else if (pixelformat == V4L2_PIX_FMT_RGB565)
		cfg |= S3C_CISCCTRL_OUTRGB_FMT_RGB565;

	writel(cfg, ctrl->regs + S3C_CISCCTRL);

	return 0;
}

int fimc_hwset_ext_rgb(struct fimc_control *ctrl, int enable)
{
	u32 cfg = readl(ctrl->regs + S3C_CISCCTRL);
	cfg &= ~S3C_CISCCTRL_EXTRGB_EXTENSION;

	if (enable)
		cfg |= S3C_CISCCTRL_EXTRGB_EXTENSION;

	writel(cfg, ctrl->regs + S3C_CISCCTRL);

	return 0;
}

int fimc_hwset_enable_capture(struct fimc_control *ctrl)
{
	u32 cfg = readl(ctrl->regs + S3C_CIIMGCPT);
	cfg &= ~S3C_CIIMGCPT_IMGCPTEN_SC;
	cfg |= S3C_CIIMGCPT_IMGCPTEN;

	if (!ctrl->sc.bypass)
		cfg |= S3C_CIIMGCPT_IMGCPTEN_SC;

	writel(cfg, ctrl->regs + S3C_CIIMGCPT);

	return 0;
}

int fimc_hwset_disable_capture(struct fimc_control *ctrl)
{
	u32 cfg = readl(ctrl->regs + S3C_CIIMGCPT);

	cfg &= ~(S3C_CIIMGCPT_IMGCPTEN_SC | S3C_CIIMGCPT_IMGCPTEN);

	writel(cfg, ctrl->regs + S3C_CIIMGCPT);

	return 0;
}

int fimc_hwset_input_address(struct fimc_control *ctrl, dma_addr_t *base)
{
	writel(base[FIMC_ADDR_Y], ctrl->regs + S3C_CIIYSA0);
	writel(base[FIMC_ADDR_CB], ctrl->regs + S3C_CIICBSA0);
	writel(base[FIMC_ADDR_CR], ctrl->regs + S3C_CIICRSA0);

	return 0;
}

int fimc_hwset_enable_autoload(struct fimc_control *ctrl)
{
	u32 cfg = readl(ctrl->regs + S3C_CIREAL_ISIZE);

	cfg |= S3C_CIREAL_ISIZE_AUTOLOAD_ENABLE;

	writel(cfg, ctrl->regs + S3C_CIREAL_ISIZE);

	return 0;
}

int fimc_hwset_disable_autoload(struct fimc_control *ctrl)
{
	u32 cfg = readl(ctrl->regs + S3C_CIREAL_ISIZE);

	cfg &= ~S3C_CIREAL_ISIZE_AUTOLOAD_ENABLE;

	writel(cfg, ctrl->regs + S3C_CIREAL_ISIZE);

	return 0;
}

int fimc_hwset_real_input_size(struct fimc_control *ctrl, u32 width, u32 height)
{
	u32 cfg = readl(ctrl->regs + S3C_CIREAL_ISIZE);
	cfg &= ~(S3C_CIREAL_ISIZE_HEIGHT_MASK | S3C_CIREAL_ISIZE_WIDTH_MASK);

	cfg |= S3C_CIREAL_ISIZE_WIDTH(width);
	cfg |= S3C_CIREAL_ISIZE_HEIGHT(height);

	writel(cfg, ctrl->regs + S3C_CIREAL_ISIZE);

	return 0;
}

int fimc_hwset_addr_change_enable(struct fimc_control *ctrl)
{
	u32 cfg = readl(ctrl->regs + S3C_CIREAL_ISIZE);

	cfg &= ~S3C_CIREAL_ISIZE_ADDR_CH_DISABLE;

	writel(cfg, ctrl->regs + S3C_CIREAL_ISIZE);

	return 0;
}

int fimc_hwset_addr_change_disable(struct fimc_control *ctrl)
{
	u32 cfg = readl(ctrl->regs + S3C_CIREAL_ISIZE);

	cfg |= S3C_CIREAL_ISIZE_ADDR_CH_DISABLE;

	writel(cfg, ctrl->regs + S3C_CIREAL_ISIZE);

	return 0;
}

int fimc_hwset_input_colorspace(struct fimc_control *ctrl, u32 pixelformat)
{
	u32 cfg = readl(ctrl->regs + S3C_MSCTRL);
	cfg &= ~S3C_MSCTRL_INFORMAT_RGB;

	/* Color format setting */
	switch (pixelformat) {
	case V4L2_PIX_FMT_NV12:		/* fall through */
	case V4L2_PIX_FMT_NV12T:
		cfg |= S3C_MSCTRL_INFORMAT_YCBCR420;
		break;
	case V4L2_PIX_FMT_YUYV:
		cfg |= S3C_MSCTRL_INFORMAT_YCBCR422_1PLANE;
		break;
	case V4L2_PIX_FMT_YUV422P:
		cfg |= S3C_MSCTRL_INFORMAT_YCBCR422;
		break;
	case V4L2_PIX_FMT_RGB565:	/* fall through */
	case V4L2_PIX_FMT_RGB32:
		cfg |= S3C_MSCTRL_INFORMAT_RGB;
		break;
	default: 
		dev_err(ctrl->dev, "%s: Invalid pixelformt : %d\n", 
				__FUNCTION__, pixelformat);
		return -EINVAL;
	}

	writel(cfg, ctrl->regs + S3C_MSCTRL);

	return 0;
}

int fimc_hwset_input_yuv(struct fimc_control *ctrl, u32 pixelformat)
{
	u32 cfg = readl(ctrl->regs + S3C_MSCTRL);
	cfg &= ~S3C_MSCTRL_ORDER422_YCBYCR;

	switch (pixelformat) {
	case V4L2_PIX_FMT_YUYV:		/* fall through */
	case V4L2_PIX_FMT_YUV422P:
		cfg |= S3C_MSCTRL_ORDER422_YCBYCR;
		break;
	case V4L2_PIX_FMT_NV12:		/* fall through */
	case V4L2_PIX_FMT_NV12T:
	case V4L2_PIX_FMT_RGB565:	/* fall through */
	case V4L2_PIX_FMT_RGB32:
		break;
	default: 
		dev_err(ctrl->dev, "%s: Invalid pixelformt : %d\n", 
				__FUNCTION__, pixelformat);
	}

	writel(cfg, ctrl->regs + S3C_MSCTRL);

	return 0;
}

int fimc_hwset_input_source(struct fimc_control *ctrl, enum fimc_input path)
{
	u32 cfg = readl(ctrl->regs + S3C_MSCTRL);
	cfg &= ~S3C_MSCTRL_INPUT_MASK;

	if (path == FIMC_SRC_MSDMA)
		cfg |= S3C_MSCTRL_INPUT_MEMORY;
	else if (path == FIMC_SRC_CAM)
		cfg |= S3C_MSCTRL_INPUT_EXTCAM;

	writel(cfg, ctrl->regs + S3C_MSCTRL);

	return 0;

}

int fimc_hwset_start_input_dma(struct fimc_control *ctrl)
{
	u32 cfg = readl(ctrl->regs + S3C_MSCTRL);
	cfg |= S3C_MSCTRL_ENVID;

	writel(cfg, ctrl->regs + S3C_MSCTRL);

	return 0;
}

int fimc_hwset_stop_input_dma(struct fimc_control *ctrl)
{
	u32 cfg = readl(ctrl->regs + S3C_MSCTRL);
	cfg &= ~S3C_MSCTRL_ENVID;

	writel(cfg, ctrl->regs + S3C_MSCTRL);

	return 0;
}

/* FIXME */
int fimc_hw_wait_winoff(struct fimc_control *ctrl)
{
	return 0;
}

int fimc_hw_wait_stop_input_dma(struct fimc_control *ctrl)
{
	return 0;
}

int fimc_hwset_change_effect(struct fimc_control *ctrl, 
			struct fimc_s3c_effect *effect)
{
	u32 cfg = readl(ctrl->regs + S3C_CIIMGEFF);

	cfg &= ~S3C_CIIMGEFF_FIN_MASK;
	cfg |= effect->type;
	cfg |= S3C_CIIMGEFF_IE_ENABLE;

	if (effect->type == EFFECT_ARBITRARY) {
		cfg &= ~S3C_CIIMGEFF_PAT_CBCR_MASK;
		cfg |= S3C_CIIMGEFF_PAT_CB(effect->pat_cb);
		cfg |= S3C_CIIMGEFF_PAT_CR(effect->pat_cr);
	}

	writel(cfg, ctrl->regs + S3C_CIIMGEFF);
	
	return 0;	
}

#ifdef CONFIG_PM
static unsigned int fimc_save_reg[6];

void fimc_save_regs(struct fimc_control *ctrl)
{
	fimc_save_reg[0] = readl(ctrl->regs + S3C_CIGCTRL);
	fimc_save_reg[1] = readl(ctrl->regs + S3C_CIOCTRL);
	fimc_save_reg[2] = readl(ctrl->regs + S3C_CISCPRERATIO);
	fimc_save_reg[3] = readl(ctrl->regs + S3C_CISCPREDST);
	fimc_save_reg[4] = readl(ctrl->regs + S3C_CISCCTRL);
	fimc_save_reg[5] = readl(ctrl->regs + S3C_MSCTRL);
}

void fimc_restore_regs(struct fimc_control *ctrl)
{	
	writel(fimc_save_reg[0], ctrl->regs + S3C_CIGCTRL);
	writel(fimc_save_reg[1], ctrl->regs + S3C_CIOCTRL);
	writel(fimc_save_reg[2], ctrl->regs + S3C_CISCPRERATIO);
	writel(fimc_save_reg[3], ctrl->regs + S3C_CISCPREDST);
	writel(fimc_save_reg[4], ctrl->regs + S3C_CISCCTRL);
	writel(fimc_save_reg[5], ctrl->regs + S3C_MSCTRL);
}
#endif

void fimc_hwset_hw_reset(void)
{
	void __iomem *regs = ioremap(S3C64XX_PA_FIMC, SZ_4K);
	u32 cfg;

	cfg = readl(regs + S3C_CIGCTRL);
	cfg &= ~S3C_CIGCTRL_CAMRST_A;
	writel(cfg, regs + S3C_CIGCTRL);
	udelay(2000);
	
	cfg = readl(regs + S3C_CIGCTRL);
	cfg |= S3C_CIGCTRL_CAMRST_A;
	writel(cfg, regs + S3C_CIGCTRL);
	udelay(2000);
	
	iounmap(regs);
}

