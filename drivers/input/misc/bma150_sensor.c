//[*]--------------------------------------------------------------------------------------------------[*]
//
//
// 
//  HKC100 Board : BMA150 Sensor Interface driver (charles.park)
//  2009.08.13
// 
//
//[*]--------------------------------------------------------------------------------------------------[*]
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/input.h>
#include <linux/fs.h>
#include <linux/sysfs.h>
#include <linux/delay.h>

#include <mach/irqs.h>
#include <asm/system.h>

//[*]--------------------------------------------------------------------------------------------------[*]
#ifdef CONFIG_HAS_EARLYSUSPEND
	#include <linux/wakelock.h>
	#include <linux/earlysuspend.h>
	#include <linux/suspend.h>
#endif

//[*]--------------------------------------------------------------------------------------------------[*]
#include <plat/regs-gpio.h>
#include <linux/delay.h>

//[*]--------------------------------------------------------------------------------------------------[*]
#include "bma150_gpio_i2c.h"
#include "bma150_sensor.h"
#include "bma150_sysfs.h"

//[*]--------------------------------------------------------------------------------------------------[*]
//#define	DEBUG_BMA150_SENSOR_MSG
#define	DEBUG_BMA150_SENSOR_PM_MSG

#define	DATA_AVERAGE_COUNT	5	// Sensor data average flage & count

//[*]--------------------------------------------------------------------------------------------------[*]
bma150_sensor_t	bma150_sensor;

//[*]--------------------------------------------------------------------------------------------------[*]
static void				bma150_sensor_timer_irq		(unsigned long arg);
static void 			bma150_sensor_timer_start	(void);
static void 			bma150_sensor_read_data		(void);
static	int				bma150_sensor_temp_calc		(unsigned char tmp);
static	int				bma150_sensor_data_calc		(unsigned char msb, unsigned char lsb);

static int              bma150_sensor_open			(struct input_dev *dev);
static void				bma150_sensor_close			(struct input_dev *dev);
static void             bma150_sensor_release_device(struct device *dev);

#ifndef CONFIG_HAS_EARLYSUSPEND
	static int              bma150_sensor_resume		(struct platform_device *dev);
	static int              bma150_sensor_suspend		(struct platform_device *dev, pm_message_t state);
#else
	static void				bma150_sensor_early_suspend	(struct early_suspend *h);
	static void				bma150_sensor_late_resume	(struct early_suspend *h);
#endif

static void 			bma150_sensor_modify_range	(void);
static void 			bma150_sensor_config		(unsigned char state);

static int __devinit    bma150_sensor_probe			(struct platform_device *pdev);
static int __devexit    bma150_sensor_remove		(struct platform_device *pdev);

static int __init       bma150_sensor_init			(void);
static void __exit      bma150_sensor_exit			(void);

//[*]--------------------------------------------------------------------------------------------------[*]
static struct platform_driver bma150_sensor_platform_device_driver = {
		.probe          = bma150_sensor_probe,
		.remove         = bma150_sensor_remove,

#ifndef CONFIG_HAS_EARLYSUSPEND
		.suspend        = bma150_sensor_suspend,
		.resume         = bma150_sensor_resume,
#endif
		.driver		= {
			.owner	= THIS_MODULE,
			.name	= BMA150_SENSOR_DEVICE_NAME,
		},
};

//[*]--------------------------------------------------------------------------------------------------[*]
struct platform_device bma150_sensor_platform_device = {
        .name           = BMA150_SENSOR_DEVICE_NAME,
        .id             = 0,
        .num_resources  = 0,
        .dev    = {
                .release	= bma150_sensor_release_device,
        },
};

//[*]--------------------------------------------------------------------------------------------------[*]
module_init(bma150_sensor_init);
module_exit(bma150_sensor_exit);

//[*]--------------------------------------------------------------------------------------------------[*]
MODULE_AUTHOR("Hard Kernel");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("BMA150(Triaxial acceleration Sensor) interface for HKC100 Board");

//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
#if defined(DATA_AVERAGE_COUNT)
	#define	DATA_LOWERBIT_MASK	50
	static	void bma150_sensor_data_average(int x, int y, int z, bma150_sensor_data_t *result);

	static	void bma150_sensor_data_average(int x, int y, int z, bma150_sensor_data_t *result)
	{
		static	bma150_sensor_data_t	save_buf[DATA_AVERAGE_COUNT];
	
		static	int		pos = 0;
		static	long	ave_x = 0, ave_y = 0, ave_z = 0;
				int		i;
	
		// Lower bit mask
		x = (x / DATA_LOWERBIT_MASK) * DATA_LOWERBIT_MASK;
		y = (y / DATA_LOWERBIT_MASK) * DATA_LOWERBIT_MASK;
		z = (z / DATA_LOWERBIT_MASK) * DATA_LOWERBIT_MASK;
	
		// average real data
		ave_x += x;		ave_x /= 2;		save_buf[pos].acc_x = ave_x;	ave_x = 0;
		ave_y += y;		ave_y /= 2;		save_buf[pos].acc_y = ave_y;	ave_y = 0;
		ave_z += z;		ave_z /= 2;		save_buf[pos].acc_z = ave_z;	ave_z = 0;
	
		pos++;	
		if(pos >= DATA_AVERAGE_COUNT)	pos = 0;
	
		for(i = 0; i < DATA_AVERAGE_COUNT; i++)	{
			ave_x += (save_buf[i].acc_x / DATA_AVERAGE_COUNT);
			ave_y += (save_buf[i].acc_y / DATA_AVERAGE_COUNT);
			ave_z += (save_buf[i].acc_z / DATA_AVERAGE_COUNT);
		}
		
		// threshold check
		if(abs(ave_x - result->acc_x) > bma150_sensor.threshold_x)	result->acc_x = ave_x;
		if(abs(ave_y - result->acc_y) > bma150_sensor.threshold_y)	result->acc_y = ave_y;
		if(abs(ave_z - result->acc_z) > bma150_sensor.threshold_z)	result->acc_z = ave_z;
	}
#endif

//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
int bma150_sensor_get_air_temp(void)
{
    unsigned long 	flags;
    int				temp;

    local_irq_save(flags);

	temp = bma150_sensor.data.air_temp;

    local_irq_restore(flags);
    
	return	temp;
}

EXPORT_SYMBOL(bma150_sensor_get_air_temp);
//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
static void bma150_sensor_timer_start(void)
{
	switch(bma150_sensor.mode)	{
		default	:
		case	BMA150_MODE_NORMAL:		bma150_sensor.rd_timer.expires = jiffies + PERIOD_NORMAL;	break;
		case	BMA150_MODE_UI:			bma150_sensor.rd_timer.expires = jiffies + PERIOD_UI;		break;
		case	BMA150_MODE_GAME:		bma150_sensor.rd_timer.expires = jiffies + PERIOD_GAME;		break;
		case	BMA150_MODE_FASTEST:	bma150_sensor.rd_timer.expires = jiffies + PERIOD_FASTEST;	break;
	}

	add_timer(&bma150_sensor.rd_timer);
}

//[*]--------------------------------------------------------------------------------------------------[*]
static void	bma150_sensor_timer_irq(unsigned long arg)
{
	// Acc data read
	write_seqlock(&bma150_sensor.lock);

	bma150_sensor_read_data();

	// new config detect
	if(bma150_sensor.modify_range)	{
		bma150_sensor.modify_range = 0;		bma150_sensor_modify_range();	
	}

	bma150_sensor_timer_start();

	write_sequnlock(&bma150_sensor.lock);
}

//[*]--------------------------------------------------------------------------------------------------[*]
static	int	bma150_sensor_temp_calc(unsigned char tmp)
{
	return	((int)tmp * 5 - 300);	// temp * 10
}

//[*]--------------------------------------------------------------------------------------------------[*]
//   return to mg
//[*]--------------------------------------------------------------------------------------------------[*]
static	int	bma150_sensor_data_calc(unsigned char msb, unsigned char lsb)
{
	int	data = 0;

	// new data flag
	data  = ((msb << 2) & 0x3FC);
	data |= ((lsb >> 6) & 0x3);
	
	if(data & 0x200)	{
		data = (~data) + 1;	data &= 0x1FF;
		
		if(data)	data = data * bma150_sensor.acc_range * (-2);
		else		data = 1000 * bma150_sensor.acc_range * (-1);
	}
	else	{
		data = data * bma150_sensor.acc_range * 2;
	}

	return	data;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static	void bma150_sensor_read_data(void)
{
	int	temp_x, temp_y, temp_z;
	
	bma150_sensor_read(BMA150_REGx02, &bma150_sensor.bytes[0], 7);

	// Air temp calc
	bma150_sensor.data.air_temp = bma150_sensor_temp_calc(bma150_sensor.bytes[AIR_TEMP]);

	// changed X, Y 
	temp_x = bma150_sensor_data_calc(bma150_sensor.bytes[ACC_Y_MSB], bma150_sensor.bytes[ACC_Y_LSB]);
	temp_y = bma150_sensor_data_calc(bma150_sensor.bytes[ACC_X_MSB], bma150_sensor.bytes[ACC_X_LSB]);
	temp_y *= (-1);
		
	// Acc z cal
	temp_z = bma150_sensor_data_calc(bma150_sensor.bytes[ACC_Z_MSB], bma150_sensor.bytes[ACC_Z_LSB]);

	#if defined(DATA_AVERAGE_COUNT)	
		bma150_sensor_data_average(temp_x, temp_y, temp_z, &bma150_sensor.data);
	#endif

	// sensor data report
	if(bma150_sensor.enable)	{
		input_report_rel(bma150_sensor.driver, REL_X, bma150_sensor.data.acc_x);
		input_report_rel(bma150_sensor.driver, REL_Y, bma150_sensor.data.acc_y);
		input_report_rel(bma150_sensor.driver, REL_Z, bma150_sensor.data.acc_z);
		input_sync(bma150_sensor.driver);
	}

	#if	defined(DEBUG_BMA150_SENSOR_MSG)
		printk("%s : TEMP[%04d], X[%04d], Y[%04d], Z[%04d]\r\n"	, __FUNCTION__,
																 bma150_sensor.data.air_temp,
																 bma150_sensor.data.acc_x,
																 bma150_sensor.data.acc_y,
																 bma150_sensor.data.acc_z);
	#endif
}

//[*]--------------------------------------------------------------------------------------------------[*]
static int	bma150_sensor_open	(struct input_dev *dev)
{
	#if	defined(DEBUG_BMA150_SENSOR_MSG)
		printk("%s\n", __FUNCTION__);
	#endif
	
	return	0;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static void	bma150_sensor_close	(struct input_dev *dev)
{
	#if	defined(DEBUG_BMA150_SENSOR_MSG)
		printk("%s\n", __FUNCTION__);
	#endif
}

//[*]--------------------------------------------------------------------------------------------------[*]
static void	bma150_sensor_release_device	(struct device *dev)
{
	#if	defined(DEBUG_BMA150_SENSOR_MSG)
		printk("%s\n", __FUNCTION__);
	#endif
}

//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
#ifdef CONFIG_HAS_EARLYSUSPEND
static	void	bma150_sensor_late_resume	(struct early_suspend *h)
#else
static	int		bma150_sensor_resume		(struct platform_device *dev)
#endif
{
	#if	defined(DEBUG_BMA150_SENSOR_PM_MSG)
		printk("%s\n", __FUNCTION__);
	#endif

	bma150_sensor_config(SENSOR_STATE_RESUME);
	
#ifdef CONFIG_HAS_EARLYSUSPEND
	return;
#else
	return	0;
#endif
}

//[*]--------------------------------------------------------------------------------------------------[*]
#ifdef CONFIG_HAS_EARLYSUSPEND
static	void	bma150_sensor_early_suspend	(struct early_suspend *h)
#else
static 	int		bma150_sensor_suspend		(struct platform_device *dev, pm_message_t state)
#endif
{
	#if	defined(DEBUG_BMA150_SENSOR_PM_MSG)
		printk("%s\n", __FUNCTION__);
	#endif

	del_timer_sync(&bma150_sensor.rd_timer);

#ifdef CONFIG_HAS_EARLYSUSPEND
	return;
#else
	return	0;
#endif
}

//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
static	void bma150_sensor_modify_range(void)
{
	unsigned char	rd = 0;
	
	bma150_sensor_read(BMA150_REGx14, &rd, sizeof(rd));
	
	rd &= (~BMA150_ACC_RANGE_ERROR);	// acc range clear
	
	switch(bma150_sensor.acc_range)	{
		default	:				bma150_sensor.acc_range = ACC_RANGE_2G;
		case	ACC_RANGE_2G:	rd |= BMA150_ACC_RANGE_2G;				break;
		case	ACC_RANGE_4G:	rd |= BMA150_ACC_RANGE_4G;				break;
		case	ACC_RANGE_8G:	rd |= BMA150_ACC_RANGE_8G;				break;
	}
	
	bma150_sensor_write(BMA150_REGx14, rd);
}

//[*]--------------------------------------------------------------------------------------------------[*]
static	void bma150_sensor_config(unsigned char state)
{
	unsigned char	rd = 0;

	// GPIO Init 
	bma150_sensor_port_init();
	
	// Setting I2C Protocol
	bma150_sensor_write(BMA150_REGx15, 0);	// 3 wire

	// EEPROM Unlock
	bma150_sensor_read(BMA150_REGx0A, &rd, sizeof(rd));
	bma150_sensor_write(BMA150_REGx0A, (rd | 0x10));
	bma150_sensor_write(0x35, 0x00);
	// EEPROM Lock
	bma150_sensor_write(BMA150_REGx0A, (rd & (~0x10)));
	
	mdelay(10);	// wait

	// Must set to 0, BIT7 of REGx15 (SPI4 = 0 -> 3 wire, default 4 wire) : MODE I2C
	bma150_sensor_read(BMA150_REGx15, &rd, sizeof(rd));
	
	if(rd & 0x80)	printk("BMA150 Sensor SPI 4 Wire Mode [rd = 0x%02X] \r\n", rd);
	else			printk("BMA150 Sensor I2C 3 Wire Mode [rd = 0x%02X] \r\n", rd);

	bma150_sensor_read(BMA150_REGx01, &rd, sizeof(rd));
	printk("BMA150 Sensor Chip version [%d.%d]\r\n", (rd >> 4) & 0x0F, rd & 0x0F);
	
	bma150_sensor_read(BMA150_REGx00, &rd, sizeof(rd));
	printk("BMA150 Sensor Chip ID [%d] \r\n", rd);

	// set all interrupt disable (hw bug) : wrong interrupt connected (bma interrupt -> pin 4)
	bma150_sensor_write(BMA150_REGx0B, 0x00);

	if(state == SENSOR_STATE_BOOT)	{
		// set default config
		bma150_sensor.acc_range = ACC_RANGE_2G;	// +- 2g
		bma150_sensor.mode 		= BMA150_MODE_NORMAL;
		
		bma150_sensor.modify_range = 0;	// acc range modify flag clear
	}
	else	{	// SENSOR_STATE RESUME
		if(bma150_sensor.acc_range != ACC_RANGE_2G)
			bma150_sensor.modify_range = 1;	// acc range modify flag set
	}

	init_timer(&bma150_sensor.rd_timer);
	bma150_sensor.rd_timer.data 		= (unsigned long)&bma150_sensor.rd_timer;
	bma150_sensor.rd_timer.function 	= bma150_sensor_timer_irq;

	seqlock_init(&bma150_sensor.lock);	
	
	bma150_sensor_timer_start();
}

//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
static int __devinit    bma150_sensor_probe	(struct platform_device *pdev)
{
	int		rc;
	
	// struct init
	memset(&bma150_sensor, 0x00, sizeof(bma150_sensor_t));

	// create sys_fs
	if((rc = bma150_sensor_sysfs_create(pdev)))	{
		printk("%s : sysfs_create_group fail!!\n", __FUNCTION__);
		return	rc;
	}
	
	bma150_sensor.driver = input_allocate_device();

    if(!(bma150_sensor.driver))	{
		printk("ERROR! : %s input_allocate_device() error!!! no memory!!\n", __FUNCTION__);
		bma150_sensor_sysfs_remove(pdev);
		return -ENOMEM;
    }

	bma150_sensor.driver->name 	= BMA150_SENSOR_DEVICE_NAME;
	bma150_sensor.driver->phys 	= "bma150_sensor/input2";
    bma150_sensor.driver->open 	= bma150_sensor_open;
    bma150_sensor.driver->close	= bma150_sensor_close;
	
	bma150_sensor.driver->id.bustype	= BUS_I2C;
	bma150_sensor.driver->id.vendor 	= 0x16B4;
	bma150_sensor.driver->id.product	= 0x0703;
	bma150_sensor.driver->id.version	= 0x0001;

	// sensor event
	set_bit(EV_REL, bma150_sensor.driver->evbit); 
	set_bit(REL_X, bma150_sensor.driver->relbit); 
	set_bit(REL_Y, bma150_sensor.driver->relbit); 
	set_bit(REL_Z, bma150_sensor.driver->relbit); 

	if(input_register_device(bma150_sensor.driver))	{
		printk("BMA150 Accelometor Sensor input register device fail!!\n");
		bma150_sensor_sysfs_remove(pdev);
		input_free_device(bma150_sensor.driver);	return	-ENODEV;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	bma150_sensor.power.suspend = bma150_sensor_early_suspend;
	bma150_sensor.power.resume = bma150_sensor_late_resume;
	bma150_sensor.power.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	//if, is in USER_SLEEP status and no active auto expiring wake lock
	//if (has_wake_lock(WAKE_LOCK_SUSPEND) == 0 && get_suspend_state() == PM_SUSPEND_ON)
	register_early_suspend(&bma150_sensor.power);
#endif

	bma150_sensor_config(SENSOR_STATE_BOOT);

	printk("HardKernel-C100 BMA150 Sensor driver initialized!! Ver 1.0\n");

	return	0;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static int __devexit    bma150_sensor_remove	(struct platform_device *pdev)
{
	#if	defined(DEBUG_BMA150_SENSOR_MSG)
		printk("%s\n", __FUNCTION__);
	#endif

	del_timer_sync(&bma150_sensor.rd_timer);

	bma150_sensor_sysfs_remove(pdev);
	
	input_unregister_device(bma150_sensor.driver);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&bma150_sensor.power);
#endif

	return  0;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static int __init	bma150_sensor_init	(void)
{
	int ret = platform_driver_register(&bma150_sensor_platform_device_driver);

	#if	defined(DEBUG_BMA150_SENSOR_MSG)
		printk("%s\n", __FUNCTION__);
	#endif
	
	if(!ret)	{
		ret = platform_device_register(&bma150_sensor_platform_device);

		#if	defined(DEBUG_BMA150_SENSOR_MSG)
			printk("platform_driver_register %d \n", ret);
		#endif

		if(ret)	platform_driver_unregister(&bma150_sensor_platform_device_driver);
	}

	return ret;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static void __exit	bma150_sensor_exit	(void)
{
	#if	defined(DEBUG_BMA150_SENSOR_MSG)
		printk("%s\n",__FUNCTION__);
	#endif
	
	platform_device_unregister(&bma150_sensor_platform_device);
	platform_driver_unregister(&bma150_sensor_platform_device_driver);
}

//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
