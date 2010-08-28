#ifndef _S3C_ROTATOR_COMMON2_H_
#define _S3C_ROTATOR_COMMON2_H_

#define ROTATOR_IOCTL_MAGIC 'R'

#define ROTATOR_MINOR			230
#define ROTATOR_TIMEOUT			100	/* normally 800 * 480 * 2 rotation takes about 20ms */

#define S3C_ROT_CLK_NAME		"rotator"

#define ROTATOR_0			_IO(ROTATOR_IOCTL_MAGIC, 0)
#define ROTATOR_90			_IO(ROTATOR_IOCTL_MAGIC, 1)
#define ROTATOR_180			_IO(ROTATOR_IOCTL_MAGIC, 2)
#define ROTATOR_270			_IO(ROTATOR_IOCTL_MAGIC, 3)
#define HFLIP				_IO(ROTATOR_IOCTL_MAGIC, 4)
#define VFLIP				_IO(ROTATOR_IOCTL_MAGIC, 5)

enum s3c_rot_status {
	ROT_IDLE,
	ROT_RUN,
	ROT_READY_SLEEP,
	ROT_SLEEP,
};

struct s3c_rotator_ctrl {
	char			clk_name[16];
	struct clk		*clock;

	enum s3c_rot_status		status;
};

typedef enum {
	ROTATOR_YCbCr420_3P,
	ROTATOR_YCbCr420_2P,
	ROTATOR_YCbCr422_1P,
	ROTATOR_RGB565,
	ROTATOR_RGB888
} rot_inputfmt;

typedef enum {
	ROTATOR_DEGREE_0,
	ROTATOR_DEGREE_90,
	ROTATOR_DEGREE_180,
	ROTATOR_DEGREE_270,
	ROTATOR_FLIP_VERTICAL,
	ROTATOR_FLIP_HORIZONTAL
} rot_degree;

typedef enum {
	ROTATOR_64_BYTE,
	ROTATOR_128_BYTE,
	ROTATOR_256_BYTE,
	ROTATOR_512_BYTE,
	ROTATOR_1024_BYTE,
	ROTATOR_2048_BYTE,
	ROTATOR_4096_BYTE
} rot_h_tilesize;

typedef enum {
	ROTATOR_1_LINE,
	ROTATOR_2_LINE,
	ROTATOR_4_LINE,
	ROTATOR_8_LINE,
	ROTATOR_16_LINE,
	ROTATOR_32_LINE,
} rot_v_tilesize;

typedef enum {
	ROTATOR_CONFIGURABLE_TILE,
	ROTATOR_16_16_TILE,
	ROTATOR_64_32_TILE,
	ROTATOR_LINEAR_TILE,
} rot_tilemode;

typedef struct{
	unsigned int src_width;			/* Source Image Full Width */
	unsigned int src_height;		/* Source Image Full Height */
	unsigned int src_addr_rgb_y; 		/* Base Address of the Source Image (RGB or Y) */
	unsigned int src_addr_cb;		/* Base Address of the Source Image (CB Component) */
	unsigned int src_addr_cr;		/* Base Address of the Source Image (CR Component)  */
	rot_inputfmt src_format;		/* Color Space of the Source Image  */

	unsigned int src_window_offset_x;	/* The pixel coordinates on X-axis of a image to be rotated */
	unsigned int src_window_offset_y;	/* The pixel coordinates on Y-axis of a image to be rotated */

	unsigned int src_window_width;		/* Horizontal pixel size of a image to be rotated */
	unsigned int src_window_height;		/* Vertical pixel size of a image to be rotated */

	unsigned int dst_addr_rgb_y;		/* Base Address of the Destination Image (RGB or Y) */
	unsigned int dst_addr_cb;		/* Base Address of the Destination Image (CB Component) */
	unsigned int dst_addr_cr;		/* Base Address of the Destination Image (CR Component) */

	unsigned int dst_window_width;		/* Horizontal pixel size of a dst image */
	unsigned int dst_window_height;		/* Vertical pixel size of a dst image */

	unsigned int dst_window_offset_x;		/* The pixel coordinates on X-axis of a image to be rotated */
	unsigned int dst_window_offset_y;		/* The pixel coordinates on Y-axis of a image to be rotated */

} ro_params;
#endif /* _S3C_ROTATOR_COMMON2_H_  */

