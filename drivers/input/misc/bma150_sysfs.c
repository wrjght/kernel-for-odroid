//[*]--------------------------------------------------------------------------------------------------[*]
//
//
// 
//  HKC100 Board : BMA150 Sensor Interface driver (charles.park)
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
#include "bma150_gpio_i2c.h"
#include "bma150_sensor.h"

//[*]--------------------------------------------------------------------------------------------------[*]
//
//   sysfs function prototype define
//
//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t show_temperature		(struct device *dev, struct device_attribute *attr, char *buf);
static 	DEVICE_ATTR(temperature, S_IRUGO, show_temperature, NULL);

//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_delay				(struct device *dev, struct device_attribute *attr, char *buf);
static	ssize_t set_delay				(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static	DEVICE_ATTR(delay, S_IRWXUGO, show_delay, set_delay);

//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_scale				(struct device *dev, struct device_attribute *attr, char *buf);
static	ssize_t set_scale				(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static	DEVICE_ATTR(full_scale, S_IRWXUGO, show_scale, set_scale);

//[*]--------------------------------------------------------------------------------------------------[*]
#define	THRESHOLD_MAX	50

//[*]--------------------------------------------------------------------------------------------------[*]
//	x,y,z data threshold control
//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t show_threshold_x		(struct device *dev, struct device_attribute *attr, char *buf);
static 	ssize_t set_threshold_x			(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static	DEVICE_ATTR(threshold_x, S_IRWXUGO, show_threshold_x, set_threshold_x);

static 	ssize_t show_threshold_y		(struct device *dev, struct device_attribute *attr, char *buf);
static 	ssize_t set_threshold_y			(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static	DEVICE_ATTR(threshold_y, S_IRWXUGO, show_threshold_y, set_threshold_y);

static 	ssize_t show_threshold_z		(struct device *dev, struct device_attribute *attr, char *buf);
static 	ssize_t set_threshold_z			(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static	DEVICE_ATTR(threshold_z, S_IRWXUGO, show_threshold_z, set_threshold_z);

//[*]--------------------------------------------------------------------------------------------------[*]
//   Sensor Data Enable/Disable Control
//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t show_enable				(struct device *dev, struct device_attribute *attr, char *buf);
static 	ssize_t set_enable				(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static	DEVICE_ATTR(enable, S_IRWXUGO, show_enable, set_enable);

//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
//
//   extern function
//
//[*]--------------------------------------------------------------------------------------------------[*]
int		bma150_sensor_sysfs_create		(struct platform_device *pdev);
void	bma150_sensor_sysfs_remove		(struct platform_device *pdev);

//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
static struct attribute *bma150_sensor_sysfs_entries[] = {
	&dev_attr_temperature,
	&dev_attr_delay.attr,
	&dev_attr_full_scale.attr,
	&dev_attr_threshold_x.attr,
	&dev_attr_threshold_y.attr,
	&dev_attr_threshold_z.attr,
	&dev_attr_enable.attr,
	NULL
};

static struct attribute_group bma150_sensor_attr_group = {
	.name   = NULL,
	.attrs  = bma150_sensor_sysfs_entries,
};

//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
static ssize_t show_temperature			(struct device *dev, struct device_attribute *attr, char *buf)
{
    unsigned long 	flags;
    int				temp;

    local_irq_save(flags);

	temp = bma150_sensor.data.air_temp;

    local_irq_restore(flags);
    
    return sprintf(buf, "%d.%d C.\n", temp/10, temp%10);
}

//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t show_delay				(struct device *dev, struct device_attribute *attr, char *buf)
{
    unsigned long 	flags;
    unsigned int	delay_val = 0;

    local_irq_save(flags);
    
    switch(bma150_sensor.mode)	{
    	default	:						bma150_sensor.mode = BMA150_MODE_NORMAL;
		case	BMA150_MODE_NORMAL	:	delay_val = PERIOD_NORMAL;					break;
		case	BMA150_MODE_UI		:	delay_val = PERIOD_UI;						break;
		case	BMA150_MODE_GAME	:	delay_val = PERIOD_GAME;					break;
		case	BMA150_MODE_FASTEST	:	delay_val = PERIOD_FASTEST;					break;
    }

    local_irq_restore(flags);

    return sprintf(buf, "%d ms\n", delay_val * 5);
}

//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t set_delay				(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    unsigned long 	flags;
	unsigned int 	val = 0;

	if (!sscanf(buf, "%u\n", &val))		return -EINVAL;

    local_irq_save(flags);

	if(val < 4)		bma150_sensor.mode = val;
	else			bma150_sensor.mode = 3;		// BMA150_MODE_NORMAL

    local_irq_restore(flags);

    return count;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t show_scale				(struct device *dev, struct device_attribute *attr, char *buf)
{
    unsigned long 	flags;

    local_irq_save(flags);
    
    switch(bma150_sensor.acc_range)	{
    	default	:				bma150_sensor.acc_range = ACC_RANGE_2G;				break;
    	case	ACC_RANGE_2G:	case	ACC_RANGE_4G:  	case	ACC_RANGE_8G:  		break;
    }
    
    local_irq_restore(flags);

	return sprintf(buf, "(+/-) %d G\n", bma150_sensor.acc_range);
}

//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t set_scale				(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    unsigned long 	flags;
	unsigned int 	val = 0, range = 0;

	if (!sscanf(buf, "%u\n", &val))		return -EINVAL;

    local_irq_save(flags);

    switch(val)	{
    	default	:				
    	case	ACC_RANGE_2G:	range = ACC_RANGE_2G;	break;
    	case	ACC_RANGE_4G:  	range = ACC_RANGE_4G;	break;
    	case	ACC_RANGE_8G:  	range = ACC_RANGE_8G;	break;
    }

	// work q run
	if(bma150_sensor.acc_range != range)	{
		bma150_sensor.acc_range = range;	bma150_sensor.modify_range = 1;
	}
	
    local_irq_restore(flags);

    return count;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t show_threshold_x		(struct device *dev, struct device_attribute *attr, char *buf)
{
	if(bma150_sensor.threshold_x > THRESHOLD_MAX)	bma150_sensor.threshold_x = THRESHOLD_MAX;
		
	return	sprintf(buf, "%d\n", bma150_sensor.threshold_x);
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
		
	bma150_sensor.threshold_x = val;
    
    local_irq_restore(flags);

    return count;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t show_threshold_y		(struct device *dev, struct device_attribute *attr, char *buf)
{
	if(bma150_sensor.threshold_y > THRESHOLD_MAX)	bma150_sensor.threshold_y = THRESHOLD_MAX;
		
	return	sprintf(buf, "%d\n", bma150_sensor.threshold_y);
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
		
	bma150_sensor.threshold_y = val;
    
    local_irq_restore(flags);

    return count;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t show_threshold_z		(struct device *dev, struct device_attribute *attr, char *buf)
{
	if(bma150_sensor.threshold_z > THRESHOLD_MAX)	bma150_sensor.threshold_z = THRESHOLD_MAX;
		
	return	sprintf(buf, "%d\n", bma150_sensor.threshold_z);
}

//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t set_threshold_z			(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    unsigned long 	flags;
    unsigned int	val;

    if(!(sscanf(buf, "%u\n", &val)))	return	-EINVAL;

    local_irq_save(flags);
    
	if(val < 0)				val *= (-1);
	if(val > THRESHOLD_MAX)	val = THRESHOLD_MAX;
		
	bma150_sensor.threshold_z = val;
    
    local_irq_restore(flags);

    return count;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t show_enable				(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", bma150_sensor.enable);
}

//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t set_enable				(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    unsigned long 	flags;
	unsigned int 	val = 0;

	if (!sscanf(buf, "%u\n", &val))		return -EINVAL;

    local_irq_save(flags);

	bma150_sensor.enable = (val > 0) ? 1 : 0;

    local_irq_restore(flags);

    return count;
}

//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
int		bma150_sensor_sysfs_create		(struct platform_device *pdev)	
{
	bma150_sensor.threshold_x = 10;
	bma150_sensor.threshold_y = 10;
	bma150_sensor.threshold_z = 10;

	return	sysfs_create_group(&pdev->dev.kobj, &bma150_sensor_attr_group);
}

//[*]--------------------------------------------------------------------------------------------------[*]
void	bma150_sensor_sysfs_remove		(struct platform_device *pdev)	
{
    sysfs_remove_group(&pdev->dev.kobj, &bma150_sensor_attr_group);
}
//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
