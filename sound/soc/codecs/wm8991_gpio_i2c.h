/*
 * wm8753.h  --  audio driver for WM8753
 *
 * Copyright 2007 Wolfson Microelectronics PLC.
 * Author: Graeme Gregory
 *         graeme.gregory@wolfsonmicro.com or linux@wolfsonmicro.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef __WM8991_GPIO_I2C_H__
#define __WM8991_GPIO_I2C_H__

int 	wm8991_hw_write( void *i2c_dev, const char *buff, int count);
int 	wm8991_hw_read( void *i2c_dev, char *buff, int count);
int 	wm8991_i2c_gpio_init(void);


#endif	/* __WM8991_GPIO_I2C_H__ */
/*------------------------------ END OF FILE ---------------------------------*/
