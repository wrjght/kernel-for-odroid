//[*]--------------------------------------------------------------------------------------------------[*]
/*
 *	
 * HardKernel-C100 _HKC100_TOUCH_H_ Header file(charles.park) 
 *
 */
//[*]--------------------------------------------------------------------------------------------------[*]
#ifndef	_HKC100_TOUCH_H_
#define	_HKC100_TOUCH_H_

//[*]--------------------------------------------------------------------------------------------------[*]
#ifdef CONFIG_ANDROID_POWER
	#include <linux/android_power.h>
#endif

//[*]--------------------------------------------------------------------------------------------------[*]
#define HKC100_TOUCH_DEVICE_NAME 	"hkc100-touch"

//[*]--------------------------------------------------------------------------------------------------[*]
#define	TOUCH_PRESS				1
#define	TOUCH_RELEASE			0
		
//[*]--------------------------------------------------------------------------------------------------[*]
#define	HKC100_TOUCH_IRQ 		IRQ_EINT4

//[*]--------------------------------------------------------------------------------------------------[*]
#define	MODE_MODULE_ID_READ			0x01
#define	MODE_SINGLE_TOUCH_READ		0x02
#define	MODE_MULTI_TOUCH_READ		0x03
#define	MODE_SENSITIVITY_CTL		0x04
#define	MODE_SLEEP_CTL				0x05
#define	MODE_RESET_CTL				0x06

//[*]--------------------------------------------------------------------------------------------------[*]
#define	TS_ABS_MIN_X				0
#define	TS_ABS_MIN_Y				0
#define	TS_ABS_MAX_X				320
#define	TS_ABS_MAX_Y				480

#define	PERIOD_10MS					(HZ/100)	// 10ms
#define	PERIOD_20MS					(HZ/50)		// 20ms
#define	PERIOD_50MS					(HZ/20)		// 50ms

//[*]--------------------------------------------------------------------------------------------------[*]
#define	GET_INT_STATUS()	(((*(unsigned long *)S5PC1XX_GPH0DAT) & 0x10) ? 1 : 0)

//[*]--------------------------------------------------------------------------------------------------[*]
#define	SYSFS_HOLD_STATE					0x01
#define	SYSFS_SLEEP_STATE					0x02
#define	SYSFS_RESET_FLAG					0x04
#define	SYSFS_SENSITIVITY_SET				0x08
#define	SYSFS_STATE_MASK					0x0F

//[*]--------------------------------------------------------------------------------------------------[*]
#define	TOUCH_STATE_BOOT	0
#define	TOUCH_STATE_RESUME	1

//[*]--------------------------------------------------------------------------------------------------[*]
// Touch hold event
//[*]--------------------------------------------------------------------------------------------------[*]
#define	SW_TOUCH_HOLD						0x09

//[*]--------------------------------------------------------------------------------------------------[*]
typedef	struct	hkc100_touch__t	{
	struct	input_dev		*driver;

	// seqlock_t
	seqlock_t				lock;
	unsigned int			seq;

	// timer
	struct  timer_list		penup_timer;

	// data store
	unsigned int			status;
	unsigned int			x;
	unsigned int			y;
	
	unsigned char			rd[10];

	// sysfs used
	unsigned char			set_sysfs_state;	
	unsigned char			cur_sysfs_state;
	
	unsigned char			sampling_rate;

	unsigned char			threshold_x;	// x data threshold (0-10) : default 3
	unsigned char			threshold_y;	// y data threshold (0-10) : default 3
	
	unsigned char			sensitivity;	// touch sensitivity (0-255) : default 0x14
	
	#ifdef	CONFIG_ANDROID_POWER
		android_early_suspend_t		power;
	#endif

}	hkc100_touch_t;

extern	hkc100_touch_t	hkc100_touch;

//[*]--------------------------------------------------------------------------------------------------[*]
#endif		/* _HKC100_TOUCH_H_ */
//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
