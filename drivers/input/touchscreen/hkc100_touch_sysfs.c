//[*]--------------------------------------------------------------------------------------------------[*]
//
//
// 
//  HKC100 Board : Touch Sensor Interface driver (charles.park)
//  2009.10.09
// 
//
//[*]--------------------------------------------------------------------------------------------------[*]
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/sysfs.h>

//[*]--------------------------------------------------------------------------------------------------[*]
#include "hkc100_touch_gpio_i2c.h"
#include "hkc100_touch.h"

//[*]--------------------------------------------------------------------------------------------------[*]
struct  timer_list		sysfs_timer;

//[*]--------------------------------------------------------------------------------------------------[*]
//
//   function prototype define
//
//[*]--------------------------------------------------------------------------------------------------[*]
static void 	hkc100_touch_sysfs_timer		(void);
static void 	hkc100_touch_sysfs_check		(unsigned long arg);

		int		hkc100_touch_sysfs_create		(struct platform_device *pdev);
		void	hkc100_touch_sysfs_remove		(struct platform_device *pdev);	
		
//[*]--------------------------------------------------------------------------------------------------[*]
//
//   sysfs function prototype define
//
//[*]--------------------------------------------------------------------------------------------------[*]
//   touch sensitivity control (default : 0x14, min : 0, max : 0xff)
//[*]--------------------------------------------------------------------------------------------------[*]
#define	SENSITIVITY_MAX		0xFF
static	ssize_t show_sensitivity		(struct device *dev, struct device_attribute *attr, char *buf);
static	ssize_t set_sensitivity			(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static	DEVICE_ATTR(sensitivity, S_IRWXUGO, show_sensitivity, set_sensitivity);

//[*]--------------------------------------------------------------------------------------------------[*]
//   screen hold control (on -> hold, off -> normal mode)
//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_hold_state			(struct device *dev, struct device_attribute *attr, char *buf);
static	ssize_t set_hold_state			(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static	DEVICE_ATTR(hold_state, S_IRWXUGO, show_hold_state, set_hold_state);

//[*]--------------------------------------------------------------------------------------------------[*]
//   touch sampling rate control (5, 10, 20 : unit msec)
//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_sampling_rate		(struct device *dev, struct device_attribute *attr, char *buf);
static	ssize_t set_sampling_rate		(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static	DEVICE_ATTR(sampling_rate, S_IRWXUGO, show_sampling_rate, set_sampling_rate);

//[*]--------------------------------------------------------------------------------------------------[*]
//   touch sleep state (sleep -> touch sleep, normal -> normal mode)
//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_sleep_state		(struct device *dev, struct device_attribute *attr, char *buf);
static	ssize_t set_sleep_state			(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static	DEVICE_ATTR(sleep_state, S_IRWXUGO, show_sleep_state, set_sleep_state);
	
//[*]--------------------------------------------------------------------------------------------------[*]
//   touch reset control (set to 1 than touch controller reset)
//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_reset_control		(struct device *dev, struct device_attribute *attr, char *buf);
static 	ssize_t set_reset_control		(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static	DEVICE_ATTR(reset_control, S_IRWXUGO, show_reset_control, set_reset_control);

//[*]--------------------------------------------------------------------------------------------------[*]
// 	touch threshold control (range 0 - 10) : default 3
//[*]--------------------------------------------------------------------------------------------------[*]
#define	THRESHOLD_MAX	10

//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t show_threshold_x		(struct device *dev, struct device_attribute *attr, char *buf);
static 	ssize_t set_threshold_x			(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static	DEVICE_ATTR(threshold_x, S_IRWXUGO, show_threshold_x, set_threshold_x);

static 	ssize_t show_threshold_y		(struct device *dev, struct device_attribute *attr, char *buf);
static 	ssize_t set_threshold_y			(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static	DEVICE_ATTR(threshold_y, S_IRWXUGO, show_threshold_y, set_threshold_y);

//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
static struct attribute *hkc100_touch_sysfs_entries[] = {
	&dev_attr_sensitivity.attr,
	&dev_attr_hold_state.attr,
	&dev_attr_sampling_rate.attr,
	&dev_attr_sleep_state.attr,
	&dev_attr_reset_control.attr,
	&dev_attr_threshold_x.attr,
	&dev_attr_threshold_y.attr,
	NULL
};

static struct attribute_group hkc100_touch_attr_group = {
	.name   = NULL,
	.attrs  = hkc100_touch_sysfs_entries,
};

//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_sensitivity		(struct device *dev, struct device_attribute *attr, char *buf)
{
	if(hkc100_touch.sensitivity > SENSITIVITY_MAX)	hkc100_touch.sensitivity = SENSITIVITY_MAX;
		
	return	sprintf(buf, "%d\n", hkc100_touch.sensitivity);
}

//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t set_sensitivity			(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    unsigned long 	flags;
    unsigned int	val;

    if(!(sscanf(buf, "%u\n", &val)))	return	-EINVAL;

    local_irq_save(flags);

	if(val < 0)					val *= (-1);
	if(val > SENSITIVITY_MAX)	val = SENSITIVITY_MAX;
		
	hkc100_touch.sensitivity = val;
    
    hkc100_touch.set_sysfs_state |= SYSFS_SENSITIVITY_SET;
    
	hkc100_touch_sysfs_timer();

    local_irq_restore(flags);

    return count;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t show_hold_state			(struct device *dev, struct device_attribute *attr, char *buf)
{
	if(hkc100_touch.cur_sysfs_state & SYSFS_HOLD_STATE)		return	sprintf(buf, "on\n");
	else													return	sprintf(buf, "off\n");
}

//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t set_hold_state			(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    unsigned long flags;

    local_irq_save(flags);

	if(!strcmp(buf, "on\n"))	hkc100_touch.set_sysfs_state |= ( SYSFS_HOLD_STATE);
	else						hkc100_touch.set_sysfs_state &= (~SYSFS_HOLD_STATE);

	hkc100_touch_sysfs_timer();

    local_irq_restore(flags);

    return count;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t show_sampling_rate		(struct device *dev, struct device_attribute *attr, char *buf)
{
	switch(hkc100_touch.sampling_rate)	{
		default	:
			hkc100_touch.sampling_rate = 0;
		case	0:
			return	sprintf(buf, "10 msec\n");
		case	1:
			return	sprintf(buf, "20 msec\n");
		case	2:
			return	sprintf(buf, "50 msec\n");
	}
}

//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t set_sampling_rate		(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    unsigned long 	flags;
    unsigned int	val;

    if(!(sscanf(buf, "%u\n", &val)))	return	-EINVAL;

    local_irq_save(flags);

    if		(val > 20)		hkc100_touch.sampling_rate = 2;
    else if	(val > 10)		hkc100_touch.sampling_rate = 1;
    else					hkc100_touch.sampling_rate = 0;

    local_irq_restore(flags);

    return count;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t show_sleep_state		(struct device *dev, struct device_attribute *attr, char *buf)
{
	if(hkc100_touch.cur_sysfs_state & SYSFS_SLEEP_STATE)	return	sprintf(buf, "on\n");
	else													return	sprintf(buf, "off\n");
}

//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t set_sleep_state			(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    unsigned long flags;

    local_irq_save(flags);

	if(!strcmp(buf, "on\n"))	hkc100_touch.set_sysfs_state |= ( SYSFS_SLEEP_STATE);
	else						hkc100_touch.set_sysfs_state &= (~SYSFS_SLEEP_STATE);

	hkc100_touch_sysfs_timer();

    local_irq_restore(flags);

    return count;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t show_reset_control		(struct device *dev, struct device_attribute *attr, char *buf)
{
	if(hkc100_touch.cur_sysfs_state & SYSFS_RESET_FLAG)	return	sprintf(buf, "wait. controller reset\n");
	else												return	sprintf(buf, "normal operating\n");
}

//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t set_reset_control		(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    unsigned long flags;

    local_irq_save(flags);

	if(!strcmp(buf, "on\n"))	hkc100_touch.set_sysfs_state |= ( SYSFS_RESET_FLAG);
	else						hkc100_touch.set_sysfs_state &= (~SYSFS_RESET_FLAG);

	hkc100_touch_sysfs_timer();

    local_irq_restore(flags);

    return count;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t show_threshold_x		(struct device *dev, struct device_attribute *attr, char *buf)
{
	if(hkc100_touch.threshold_x > THRESHOLD_MAX)	hkc100_touch.threshold_x = THRESHOLD_MAX;
		
	return	sprintf(buf, "%d\n", hkc100_touch.threshold_x);
}

//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t set_threshold_x			(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    unsigned long 	flags;
    unsigned int	val;

    if(!(sscanf(buf, "%u\n", &val)))	return	-EINVAL;

    local_irq_save(flags);

	if(val < 0)				val *= (-1);
	if(val > THRESHOLD_MAX)	val = THRESHOLD_MAX;
		
	hkc100_touch.threshold_x = val;
    
    local_irq_restore(flags);

    return count;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t show_threshold_y		(struct device *dev, struct device_attribute *attr, char *buf)
{
	if(hkc100_touch.threshold_y > THRESHOLD_MAX)	hkc100_touch.threshold_y = THRESHOLD_MAX;
		
	return	sprintf(buf, "%d\n", hkc100_touch.threshold_y);
}

//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t set_threshold_y			(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    unsigned long 	flags;
    unsigned int	val;

    if(!(sscanf(buf, "%u\n", &val)))	return	-EINVAL;

    local_irq_save(flags);
    
	if(val < 0)				val *= (-1);
	if(val > THRESHOLD_MAX)	val = THRESHOLD_MAX;
		
	hkc100_touch.threshold_y = val;
    
    local_irq_restore(flags);

    return count;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static void hkc100_touch_sysfs_check(unsigned long arg)
{
	unsigned long	flags;
	unsigned char	change_state;

	if(hkc100_touch.status)		{
		hkc100_touch_sysfs_timer();		return;
	}

	local_irq_save(flags);	local_irq_disable();
	
	change_state = hkc100_touch.set_sysfs_state ^ hkc100_touch.cur_sysfs_state;

	// changed hold state 
	if(change_state & SYSFS_HOLD_STATE)	{
		if(hkc100_touch.set_sysfs_state & SYSFS_HOLD_STATE)	hkc100_touch.cur_sysfs_state |= ( SYSFS_HOLD_STATE);
		else												hkc100_touch.cur_sysfs_state &= (~SYSFS_HOLD_STATE);
	}

	// changed sleep state 
	if(change_state & SYSFS_SLEEP_STATE)	{
		if(hkc100_touch.set_sysfs_state & SYSFS_SLEEP_STATE)	{
			// sleep cmd send
			hkc100_touch_sleep(0x01);
	
			hkc100_touch.cur_sysfs_state |= ( SYSFS_SLEEP_STATE);
		}
		else	{
			// wakeup cmd send
			hkc100_touch_sleep(0x00);	hkc100_touch_sleep(0x00);
	
			hkc100_touch.cur_sysfs_state &= (~SYSFS_SLEEP_STATE);
		}
	}

	// touch reset cmd
	if(hkc100_touch.set_sysfs_state & SYSFS_RESET_FLAG)	{
		hkc100_touch.cur_sysfs_state |= ( SYSFS_RESET_FLAG);

		// reset cmd send
		hkc100_touch_reset();	
		
		hkc100_touch.set_sysfs_state &= (~SYSFS_RESET_FLAG);
		hkc100_touch.cur_sysfs_state &= (~SYSFS_RESET_FLAG);
	}

	// changed touch sensitivity
	if(hkc100_touch.set_sysfs_state & SYSFS_SENSITIVITY_SET)	{

		// touch sensitivity setting
		hkc100_touch_sensitivity(hkc100_touch.sensitivity);
		
		hkc100_touch.set_sysfs_state &= (~SYSFS_SENSITIVITY_SET);
		hkc100_touch.cur_sysfs_state &= (~SYSFS_SENSITIVITY_SET);
	}

	// Single touch mode setup
	hkc100_touch_write(MODE_SINGLE_TOUCH_READ);	// set mode
	
	local_irq_restore(flags);
}
//[*]--------------------------------------------------------------------------------------------------[*]
static void hkc100_touch_sysfs_timer(void)
{
	del_timer_sync(&sysfs_timer);
	
	init_timer(&sysfs_timer);

	sysfs_timer.data 		= (unsigned long)&sysfs_timer;
	sysfs_timer.function 	= hkc100_touch_sysfs_check;
	sysfs_timer.expires 	= jiffies + PERIOD_10MS;
	
	add_timer(&sysfs_timer);
}

//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
int		hkc100_touch_sysfs_create		(struct platform_device *pdev)	
{
	// variable init
	hkc100_touch.cur_sysfs_state = 0;
	hkc100_touch.set_sysfs_state = 0;
	
	hkc100_touch.sampling_rate	= 0;	// 5 msec sampling
	hkc100_touch.threshold_x	= 5;	// x data threshold (0~10)
	hkc100_touch.threshold_y	= 5;	// y data threshold (0~10)

	return	sysfs_create_group(&pdev->dev.kobj, &hkc100_touch_attr_group);
}

//[*]--------------------------------------------------------------------------------------------------[*]
void	hkc100_touch_sysfs_remove		(struct platform_device *pdev)	
{
    sysfs_remove_group(&pdev->dev.kobj, &hkc100_touch_attr_group);
}
//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]

