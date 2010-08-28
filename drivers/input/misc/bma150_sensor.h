//[*]--------------------------------------------------------------------------------------------------[*]
/*
 *	
 * S5PC100 _BMA150_SENSOR_H_ Header file(charles.park) 
 *
 */
//[*]--------------------------------------------------------------------------------------------------[*]

#ifndef	_BMA150_SENSOR_H_
#define	_BMA150_SENSOR_H_
//[*]--------------------------------------------------------------------------------------------------[*]
#ifdef CONFIG_HAS_EARLYSUSPEND
	#include <linux/earlysuspend.h>
#endif

//[*]--------------------------------------------------------------------------------------------------[*]
#define BMA150_SENSOR_DEVICE_NAME 		"bma150-sensor"

//[*]--------------------------------------------------------------------------------------------------[*]
#define	BMA150_SENSOR_IRQ 				IRQ_EINT5	

//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
// BMA150 Triaxial acceleration sensor register define
//[*]--------------------------------------------------------------------------------------------------[*]
#define	BMA150_REGx15				0x15
#define	BMA150_REGx14				0x14
#define	BMA150_REGx13				0x13
#define	BMA150_REGx12				0x12
#define	BMA150_REGx11				0x11
#define	BMA150_REGx10				0x10
#define	BMA150_REGx0F				0x0F
#define	BMA150_REGx0E				0x0E
#define	BMA150_REGx0D				0x0D
#define	BMA150_REGx0C				0x0C
#define	BMA150_REGx0B				0x0B
#define	BMA150_REGx0A				0x0A
#define	BMA150_REGx09				0x09
#define	BMA150_REGx08				0x08
#define	BMA150_REGx07				0x07
#define	BMA150_REGx06				0x06
#define	BMA150_REGx05				0x05
#define	BMA150_REGx04				0x04
#define	BMA150_REGx03				0x03
#define	BMA150_REGx02				0x02
#define	BMA150_REGx01				0x01
#define	BMA150_REGx00				0x00

//[*]--------------------------------------------------------------------------------------------------[*]
#define	BMA150_BANDWIDTH_25HZ		0
#define	BMA150_BANDWIDTH_50HZ		1
#define	BMA150_BANDWIDTH_100HZ		2
#define	BMA150_BANDWIDTH_190HZ		3
#define	BMA150_BANDWIDTH_375HZ		4
#define	BMA150_BANDWIDTH_750HZ		5
#define	BMA150_BANDWIDTH_1500HZ		6

#define	BMA150_ACC_RANGE_2G			0x00
#define	BMA150_ACC_RANGE_4G			0x08
#define	BMA150_ACC_RANGE_8G			0x10
#define	BMA150_ACC_RANGE_ERROR		0x18
		
//[*]--------------------------------------------------------------------------------------------------[*]
// BMA150(Triaxial acceleration sensor) Command List
//[*]--------------------------------------------------------------------------------------------------[*]
#define	BMA150_MODE_FASTEST		0
#define	BMA150_MODE_GAME		1
#define	BMA150_MODE_UI			2
#define	BMA150_MODE_NORMAL		3

#define	PERIOD_FASTEST			(HZ/50)		// 20ms
#define	PERIOD_GAME				(HZ/20)		// 50ms
#define	PERIOD_UI				(HZ/10)		// 100ms
#define	PERIOD_NORMAL			(HZ/5)		// 200ms

//[*]--------------------------------------------------------------------------------------------------[*]
#define	AIR_TEMP		6
                        	
#define	ACC_X_MSB		1
#define	ACC_X_LSB		0
                        	
#define	ACC_Y_MSB		3
#define	ACC_Y_LSB		2
                        	
#define	ACC_Z_MSB		5
#define	ACC_Z_LSB		4
                        	
#define	ACC_RANGE_2G	2
#define	ACC_RANGE_4G	4
#define	ACC_RANGE_8G	8

//[*]--------------------------------------------------------------------------------------------------[*]
// input event (sw)
//[*]--------------------------------------------------------------------------------------------------[*]
#define	SCREEN_ROTATE_0		0x0A
#define	SCREEN_ROTATE_90	0x0B
#define	SCREEN_ROTATE_180	0x0C
#define	SCREEN_ROTATE_270	0x0D

//[*]--------------------------------------------------------------------------------------------------[*]
#define	SENSOR_STATE_BOOT	0
#define	SENSOR_STATE_RESUME	1

//[*]--------------------------------------------------------------------------------------------------[*]
typedef struct 	bma150_sensor_data__t	{
	int		air_temp;
	int		acc_x;
	int		acc_y;
	int		acc_z;
}	bma150_sensor_data_t;

//[*]--------------------------------------------------------------------------------------------------[*]
typedef	struct	bma150_sensor__t	{
	struct	input_dev		*driver;

	unsigned char			enable;
	
	// seqlock_t
	seqlock_t				lock;
	unsigned int			seq;

	// config update flag
	unsigned char			modify_range;
	// timer
	struct  timer_list		rd_timer;
	
	// data store
	unsigned char			mode;
	int						acc_range;

	// x,y,z threshold control
	unsigned char			threshold_x;	// range(0 - 255) : default 50
	unsigned char			threshold_y;
	unsigned char			threshold_z;

	bma150_sensor_data_t	data;
	unsigned char			bytes[8];
	
	#ifdef CONFIG_HAS_EARLYSUSPEND
		struct	early_suspend		power;
	#endif

}	bma150_sensor_t;

extern	bma150_sensor_t	bma150_sensor;

//[*]--------------------------------------------------------------------------------------------------[*]
#endif		/* _BMA150_SENSOR_H_ */
//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
