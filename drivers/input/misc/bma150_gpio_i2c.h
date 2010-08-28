//[*]----------------------------------------------------------------------------------------------[*]
//
//
// 
//  HardKernel-C100 gpio i2c driver (charles.park)
//  2009.07.22
// 
//
//[*]----------------------------------------------------------------------------------------------[*]
#ifndef	_BMA150_GPIO_I2C_H_
#define	_BMA150_GPIO_I2C_H_

//[*]----------------------------------------------------------------------------------------------[*]
#define	BMA150_SENSOR_ADDR				0x70
#define	BMA150_SENSOR_WR_ADDR			BMA150_SENSOR_ADDR
#define	BMA150_SENSOR_RD_ADDR			(BMA150_SENSOR_ADDR + 1)

//[*]--------------------------------------------------------------------------------------------------[*]
extern	int 	bma150_sensor_read		(unsigned char st_reg, unsigned char *rdata, unsigned char rsize);
extern	int		bma150_sensor_write		(unsigned char reg, unsigned char data);
extern	void 	bma150_sensor_port_init	(void);
//[*]----------------------------------------------------------------------------------------------[*]
#endif	//_BMA150_GPIO_I2C_H_
//[*]----------------------------------------------------------------------------------------------[*]
