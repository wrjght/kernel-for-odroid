/*
 * Video for Linux Two header file for samsung
 * 
 * Copyright (C) 2009, Dongsoo Nathaniel Kim<dongsoo45.kim@samsung.com>
 *
 * This header file contains several v4l2 APIs to be proposed to v4l2
 * community and until bein accepted, will be used restrictly in Samsung's
 * camera interface driver FIMC.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __LINUX_VIDEODEV2_SAMSUNG_H
#define __LINUX_VIDEODEV2_SAMSUNG_H

/* Values for 'capabilities' field */
#define V4L2_CAP_OBJ_RECOGNITION	0x10000000	/* Object detection device */
#define V4L2_CAP_STROBE			0x20000000	/* strobe control */


#define V4L2_CID_FOCUS_MODE			(V4L2_CID_CAMERA_CLASS_BASE+17)
/* Focus Methods */
enum v4l2_focus_mode {
	V4L2_FOCUS_MODE_AUTO		= 0,
	V4L2_FOCUS_MODE_MACRO		= 1,
	V4L2_FOCUS_MODE_MANUAL		= 2,
	V4L2_FOCUS_MODE_LASTP		= 2,
};

#define V4L2_CID_ZOOM_MODE				(V4L2_CID_CAMERA_CLASS_BASE+18)
/* Zoom Methods */
enum v4l2_zoom_mode {
	V4L2_ZOOM_MODE_CONTINUOUS	= 0,
	V4L2_ZOOM_MODE_OPTICAL		= 1,
	V4L2_ZOOM_MODE_DIGITAL		= 2,
	V4L2_ZOOM_MODE_LASTP		= 2,
};

/* Exposure Methods */
#define V4L2_CID_PHOTOMETRY		(V4L2_CID_CAMERA_CLASS_BASE+19)
enum v4l2_photometry_mode {
	V4L2_PHOTOMETRY_MULTISEG	= 0, /* Multi Segment */
	V4L2_PHOTOMETRY_CWA		= 1, /* Centre Weighted Average */
	V4L2_PHOTOMETRY_SPOT		= 2,
	V4L2_PHOTOMETRY_AFSPOT		= 3, /* Spot metering on focused point */
	V4L2_PHOTOMETRY_LASTP		= V4L2_PHOTOMETRY_AFSPOT,
};

/* Manual exposure control items menu type: iris, shutter, iso */
#define V4L2_CID_CAM_APERTURE	(V4L2_CID_CAMERA_CLASS_BASE+20)
#define V4L2_CID_CAM_SHUTTER	(V4L2_CID_CAMERA_CLASS_BASE+21)
#define V4L2_CID_CAM_ISO	(V4L2_CID_CAMERA_CLASS_BASE+22)

/* Following CIDs are menu type */
#define V4L2_CID_SCENEMODE	(V4L2_CID_CAMERA_CLASS_BASE+23)
#define V4L2_CID_CAM_STABILIZE	(V4L2_CID_CAMERA_CLASS_BASE+24)
#define V4L2_CID_CAM_MULTISHOT	(V4L2_CID_CAMERA_CLASS_BASE+25)

/* Control dynamic range */
#define V4L2_CID_CAM_DR		(V4L2_CID_CAMERA_CLASS_BASE+26)

/* White balance preset control */
#define V4L2_CID_WHITE_BALANCE_PRESET	(V4L2_CID_CAMERA_CLASS_BASE+27)

/* CID extensions */
#define V4L2_CID_ROTATION		(V4L2_CID_PRIVATE_BASE + 0)
#define V4L2_CID_PADDR_Y		(V4L2_CID_PRIVATE_BASE + 1)
#define V4L2_CID_PADDR_CB		(V4L2_CID_PRIVATE_BASE + 2)
#define V4L2_CID_PADDR_CR		(V4L2_CID_PRIVATE_BASE + 3)
#define V4L2_CID_PADDR_CBCR		(V4L2_CID_PRIVATE_BASE + 4)

#define V4L2_CID_JPEG_INPUT		(V4L2_CID_PRIVATE_BASE + 5)
#define V4L2_CID_OVERLAY_AUTO		(V4L2_CID_PRIVATE_BASE + 5)
#define V4L2_CID_OVERLAY_VADDR0		(V4L2_CID_PRIVATE_BASE + 6)
#define V4L2_CID_OVERLAY_VADDR1		(V4L2_CID_PRIVATE_BASE + 7)
#define V4L2_CID_OVERLAY_VADDR2		(V4L2_CID_PRIVATE_BASE + 8)
#define V4L2_CID_OUTPUT_ADDR		(V4L2_CID_PRIVATE_BASE + 10)
#define V4L2_CID_INPUT_ADDR		(V4L2_CID_PRIVATE_BASE + 20)
#define V4L2_CID_INPUT_ADDR_RGB		(V4L2_CID_PRIVATE_BASE + 21)
#define V4L2_CID_INPUT_ADDR_Y		(V4L2_CID_PRIVATE_BASE + 22)
#define V4L2_CID_INPUT_ADDR_CB		(V4L2_CID_PRIVATE_BASE + 123)
#define V4L2_CID_INPUT_ADDR_CBCR	(V4L2_CID_PRIVATE_BASE + 124)
#define V4L2_CID_INPUT_ADDR_CR		(V4L2_CID_PRIVATE_BASE + 125)
#define V4L2_CID_EFFECT_ORIGINAL	(V4L2_CID_PRIVATE_BASE + 30)
#define V4L2_CID_EFFECT_ARBITRARY	(V4L2_CID_PRIVATE_BASE + 31)
#define V4L2_CID_EFFECT_NEGATIVE 	(V4L2_CID_PRIVATE_BASE + 33)
#define V4L2_CID_EFFECT_ARTFREEZE	(V4L2_CID_PRIVATE_BASE + 34)
#define V4L2_CID_EFFECT_EMBOSSING	(V4L2_CID_PRIVATE_BASE + 35)
#define V4L2_CID_EFFECT_SILHOUETTE	(V4L2_CID_PRIVATE_BASE + 36)
#define V4L2_CID_ROTATE_ORIGINAL	(V4L2_CID_PRIVATE_BASE + 40)
#define V4L2_CID_ROTATE_90		(V4L2_CID_PRIVATE_BASE + 41)
#define V4L2_CID_ROTATE_180		(V4L2_CID_PRIVATE_BASE + 42)
#define V4L2_CID_ROTATE_270		(V4L2_CID_PRIVATE_BASE + 43)
#define V4L2_CID_ROTATE_90_HFLIP	(V4L2_CID_PRIVATE_BASE + 44)
#define V4L2_CID_ROTATE_90_VFLIP	(V4L2_CID_PRIVATE_BASE + 45)
#define	V4L2_CID_ZOOM_IN		(V4L2_CID_PRIVATE_BASE + 51)
#define V4L2_CID_ZOOM_OUT		(V4L2_CID_PRIVATE_BASE + 52)

#define V4L2_CID_STREAM_PAUSE			(V4L2_CID_PRIVATE_BASE + 53)

#define V4L2_CID_JPEG_SIZE		(V4L2_CID_PRIVATE_BASE + 23)
#define V4L2_CID_THUMBNAIL_SIZE		(V4L2_CID_PRIVATE_BASE + 24)
#define V4L2_CID_JPEG_QUALITY		(V4L2_CID_PRIVATE_BASE + 25)

#define V4L2_CID_ACTIVE_CAMERA		(V4L2_CID_PRIVATE_BASE + 11)
#define V4L2_CID_NR_FRAMES		(V4L2_CID_PRIVATE_BASE + 12)
#define V4L2_CID_RESET			(V4L2_CID_PRIVATE_BASE + 13)
#define V4L2_CID_TEST_PATTERN		(V4L2_CID_PRIVATE_BASE + 14)
#define V4L2_CID_SCALER_BYPASS		(V4L2_CID_PRIVATE_BASE + 15)
//added by jamie (2009.08.25)
#define V4L2_CID_RESERVED_MEM_BASE_ADDR	(V4L2_CID_PRIVATE_BASE + 16)
#define V4L2_CID_FIMC_VERSION		(V4L2_CID_PRIVATE_BASE + 17)

/*      Pixel format         FOURCC                        	depth  Description  */
#define V4L2_PIX_FMT_NV12T    v4l2_fourcc('T', 'V', '1', '2') /* 12  Y/CbCr 4:2:0 64x32 macroblocks */


/*
 *  * V4L2 extention for digital camera
 *   */
/* Strobe flash light */
enum v4l2_strobe_control {
	/* turn off the flash light */
	V4L2_STROBE_CONTROL_OFF		= 0,
	/* turn on the flash light */
	V4L2_STROBE_CONTROL_ON		= 1,
	/* act guide light before splash */
	V4L2_STROBE_CONTROL_AFGUIDE	= 2,
	/* charge the flash light */
	V4L2_STROBE_CONTROL_CHARGE	= 3,
};

enum v4l2_strobe_conf {
	V4L2_STROBE_OFF			= 0,	/* Always off */
	V4L2_STROBE_ON			= 1,	/* Always splashes */
	/* Auto control presets */
	V4L2_STROBE_AUTO		= 2,
	V4L2_STROBE_REDEYE_REDUCTION	= 3,
	V4L2_STROBE_SLOW_SYNC		= 4,
	V4L2_STROBE_FRONT_CURTAIN	= 5,
	V4L2_STROBE_REAR_CURTAIN	= 6,
	/* Extra manual control presets */
	V4L2_STROBE_PERMANENT		= 7,	/* keep turned on until turning off */
	V4L2_STROBE_EXTERNAL		= 8,
};

enum v4l2_strobe_status {
	V4L2_STROBE_STATUS_OFF		= 0,
	V4L2_STROBE_STATUS_BUSY		= 1, /* while processing configurations */
	V4L2_STROBE_STATUS_ERR		= 2,
	V4L2_STROBE_STATUS_CHARGING	= 3,
	V4L2_STROBE_STATUS_CHARGED	= 4,
};

/* capabilities field */
#define V4L2_STROBE_CAP_NONE		0x0000	/* No strobe supported */
#define V4L2_STROBE_CAP_OFF		0x0001	/* Always flash off mode */
#define V4L2_STROBE_CAP_ON		0x0002	/* Always use flash light mode */
#define V4L2_STROBE_CAP_AUTO		0x0004	/* Flashlight works automatic */
#define V4L2_STROBE_CAP_REDEYE		0x0008	/* Red-eye reduction */
#define V4L2_STROBE_CAP_SLOWSYNC	0x0010	/* Slow sync */
#define V4L2_STROBE_CAP_FRONT_CURTAIN	0x0020	/* Front curtain */
#define V4L2_STROBE_CAP_REAR_CURTAIN	0x0040	/* Rear curtain */
#define V4L2_STROBE_CAP_PERMANENT	0x0080	/* keep turned on until turning off */
#define V4L2_STROBE_CAP_EXTERNAL	0x0100	/* use external strobe */

/* Set mode and Get status */
struct v4l2_strobe {
	/* off/on/charge:0/1/2 */
	enum	v4l2_strobe_control control;
	/* supported strobe capabilities */
	__u32	capabilities;
	enum	v4l2_strobe_conf mode;
	enum 	v4l2_strobe_status status;	/* read only */
	/* default is 0 and range of value varies from each models */
	__u32	flash_ev;
	__u32	reserved[4];
};

#define VIDIOC_S_STROBE     _IOWR ('V', 83, struct v4l2_strobe)
#define VIDIOC_G_STROBE     _IOR ('V', 84, struct v4l2_strobe)

/* Object recognition and collateral actions */
enum v4l2_recog_mode {
	V4L2_RECOGNITION_MODE_OFF	= 0,
	V4L2_RECOGNITION_MODE_ON	= 1,
	V4L2_RECOGNITION_MODE_LOCK	= 2,
};

enum v4l2_recog_action {
	V4L2_RECOGNITION_ACTION_NONE	= 0,	/* only recognition */
	V4L2_RECOGNITION_ACTION_BLINK	= 1,	/* Capture on blinking */
	V4L2_RECOGNITION_ACTION_SMILE	= 2,	/* Capture on smiling */
};

enum v4l2_recog_pattern {
	V4L2_RECOG_PATTERN_FACE		= 0, /* Face */
	V4L2_RECOG_PATTERN_HUMAN	= 1, /* Human */
	V4L2_RECOG_PATTERN_CHAR		= 2, /* Character */
};

struct v4l2_recog_rect {
	enum	v4l2_recog_pattern  p;	/* detected pattern */
	struct	v4l2_rect  o;	/* detected area */
	__u32	reserved[4];
};

struct v4l2_recog_data {
	__u8	detect_cnt;		/* detected object counter */
	struct	v4l2_rect	o;	/* detected area */
	__u32	reserved[4];
};

struct v4l2_recognition {
	enum v4l2_recog_mode	mode;

	/* Which pattern to detect */
	enum v4l2_recog_pattern  pattern;

	/* How many object to detect */
	__u8 	obj_num;

	/* select detected object */
	__u32	detect_idx;

	/* read only :Get object coordination */
	struct v4l2_recog_data	data;

	enum v4l2_recog_action	action;
	__u32	reserved[4];
};

#define VIDIOC_S_RECOGNITION	_IOWR ('V', 85, struct v4l2_recognition)
#define VIDIOC_G_RECOGNITION	_IOR ('V', 86, struct v4l2_recognition)

#endif /* __LINUX_VIDEODEV2_SAMSUNG_H */
