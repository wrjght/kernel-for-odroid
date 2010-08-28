#ifndef _FIMG2D_DRIVER_H_
#define _FIMG2D_DRIVER_H_


#define TRUE		1
#define FALSE		0

#define G2D_SFR_SIZE	0x1000
#define G2D_MINOR 	240
#define G2D_TIMEOUT	100

#define ALPHA_VALUE_MAX	255

#define FIFO_NUM	32


#define G2D_IOCTL_MAGIC 'G'

#define G2D_ROTATOR_0		_IO(G2D_IOCTL_MAGIC,0)
#define G2D_ROTATOR_90		_IO(G2D_IOCTL_MAGIC,1)
#define G2D_ROTATOR_180		_IO(G2D_IOCTL_MAGIC,2)
#define G2D_ROTATOR_270		_IO(G2D_IOCTL_MAGIC,3)
#define G2D_ROTATOR_X_FLIP	_IO(G2D_IOCTL_MAGIC,4)
#define G2D_ROTATOR_Y_FLIP	_IO(G2D_IOCTL_MAGIC,5)

#define G2D_ROP_SRC_ONLY			(0xf0)
#define G2D_ROP_3RD_OPRND_ONLY			(0xaa)
#define G2D_ROP_DST_ONLY			(0xcc)
#define G2D_ROP_SRC_OR_DST			(0xfc)
#define G2D_ROP_SRC_OR_3RD_OPRND		(0xfa)
#define G2D_ROP_SRC_AND_DST			(0xc0) //(pat==1)? src:dst
#define G2D_ROP_SRC_AND_3RD_OPRND		(0xa0)
#define G2D_ROP_SRC_XOR_3RD_OPRND		(0x5a)
#define G2D_ROP_DST_OR_3RD_OPRND		(0xee)

typedef enum
{
	ROT_0,
	ROT_90,
	ROT_180,
	ROT_270,
	ROT_X_FLIP,
	ROT_Y_FLIP
} G2D_ROT;

typedef enum
{
	ROP_DST_ONLY,
	ROP_SRC_ONLY,
	ROP_3RD_OPRND_ONLY,
	ROP_SRC_AND_DST,
	ROP_SRC_AND_3RD_OPRND,
	ROP_SRC_OR_DST,
	ROP_SRC_OR_3RD_OPRND,
	ROP_SRC_OR_3RD,
	ROP_SRC_XOR_3RD_OPRND
} G2D_ROP_TYPE;

typedef enum
{
	G2D_NO_ALPHA_MODE,
	G2D_PP_ALPHA_SOURCE_MODE,
	G2D_ALPHA_MODE,
	G2D_FADING_MODE
} G2D_ALPHA_BLENDING_MODE;

typedef enum
{
	G2D_BLACK = 0, G2D_RED = 1, G2D_GREEN = 2, G2D_BLUE = 3, G2D_WHITE = 4,
	G2D_YELLOW = 5, G2D_CYAN = 6, G2D_MGENTA = 7
} G2D_COLOR;

typedef enum
{
	PAL1, PAL2, PAL4, PAL8,
	RGB8, ARGB8, RGB16, ARGB16, RGB18, RGB24, RGB31, ARGB24, RGBA16, RGBX24, RGBA24,
	YC420, YC422, 					/*Non-Interleave */
	CRYCBY, CBYCRY, YCRYCB, YCBYCR, YUV444		/* Interleave */
} G2D_COLOR_SPACE;

typedef struct
{
	u32	src_base_addr;
	u32	src_full_width;
	u32	src_full_height;
	u32	src_start_x;
	u32	src_start_y;
	u32	src_work_width;
	u32	src_work_height;

	u32	dst_base_addr;
	u32	dst_full_width;
	u32	dst_full_height;
	u32	dst_start_x;
	u32	dst_start_y;
	u32	dst_work_width;
	u32	dst_work_height;

	/* Coordinate (X, Y) of clipping window */
	u32	cw_x1, cw_y1;
	u32	cw_x2, cw_y2;

	u32 color_val[8];
	G2D_COLOR_SPACE bpp_dst;

	u32	alpha_mode;		/* true : enable, false : disable */
	u32	alpha_val;
	
	u32	color_key_mode;		/* true : enable, false : disable */
	u32	color_key_val;	
	G2D_COLOR_SPACE bpp_src;

	u32	stretch_mode;
	u32	transparent_mode;
}G2D_PARAMS;


#endif
