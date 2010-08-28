//[*]----------------------------------------------------------------------------------------------[*]
//
//
// 
//  HardKernel-C100 gpio i2c driver (charles.park)
//  2009.07.22
// 
//
//[*]----------------------------------------------------------------------------------------------[*]
#ifndef	_HKC100_TOUCH_GPIO_I2C_H_
#define	_HKC100_TOUCH_GPIO_I2C_H_

//[*]----------------------------------------------------------------------------------------------[*]
#define	TOUCH_WR_ADDR		0xA0
#define	TOUCH_RD_ADDR		0xA1

//[*]----------------------------------------------------------------------------------------------[*]
extern	int 			hkc100_touch_write			(unsigned char mode);
extern	int 			hkc100_touch_read			(unsigned char *rdata, unsigned char rsize);
extern	int 			hkc100_touch_sleep			(unsigned char mode);
extern	int 			hkc100_touch_sensitivity	(unsigned char sensitivity);
extern	int				hkc100_touch_reset			(void);
extern	void			hkc100_touch_port_init		(void);

//[*]----------------------------------------------------------------------------------------------[*]
#endif	//_HKC100_TOUCH_GPIO_I2C_H_
//[*]----------------------------------------------------------------------------------------------[*]
