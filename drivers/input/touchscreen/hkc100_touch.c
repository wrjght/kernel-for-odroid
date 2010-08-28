//[*]--------------------------------------------------------------------------------------------------[*]
//
//
// 
//  HardKernel(HKC100) Touch driver (charles.park)
//  2009.07.22
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

#include <mach/irqs.h>
#include <asm/system.h>

#include <plat/regs-gpio.h>
//[*]--------------------------------------------------------------------------------------------------[*]
#include "hkc100_touch.h"
#include "hkc100_touch_gpio_i2c.h"
#include "hkc100_touch_sysfs.h"

//[*]--------------------------------------------------------------------------------------------------[*]
//#define	DEBUG_HKC100_TOUCH_MSG
#define	DEBUG_HKC100_TOUCH_PM_MSG

//[*]--------------------------------------------------------------------------------------------------[*]
hkc100_touch_t	hkc100_touch;

//[*]--------------------------------------------------------------------------------------------------[*]
static void 			hkc100_touch_timer_start	(void);
static void				hkc100_touch_timer_irq		(unsigned long arg);

static void 			hkc100_touch_get_data		(void);

irqreturn_t				hkc100_touch_irq			(int irq, void *dev_id);

static int              hkc100_touch_open			(struct input_dev *dev);
static void             hkc100_touch_close			(struct input_dev *dev);

static void             hkc100_touch_release_device	(struct device *dev);

static int              hkc100_touch_resume			(struct platform_device *dev);
static int              hkc100_touch_suspend		(struct platform_device *dev, pm_message_t state);

static void 			hkc100_touch_config			(unsigned char state);

static int __devinit    hkc100_touch_probe			(struct platform_device *pdev);
static int __devexit    hkc100_touch_remove			(struct platform_device *pdev);

static int __init       hkc100_touch_init			(void);
static void __exit      hkc100_touch_exit			(void);

//[*]--------------------------------------------------------------------------------------------------[*]
static struct platform_driver hkc100_touch_platform_device_driver = {
		.probe          = hkc100_touch_probe,
		.remove         = hkc100_touch_remove,

		.suspend        = hkc100_touch_suspend,
		.resume         = hkc100_touch_resume,

		.driver		= {
			.owner	= THIS_MODULE,
			.name	= HKC100_TOUCH_DEVICE_NAME,
		},
};

//[*]--------------------------------------------------------------------------------------------------[*]
static struct platform_device hkc100_touch_platform_device = {
        .name           = HKC100_TOUCH_DEVICE_NAME,
        .id             = 0,
        .num_resources  = 0,
        .dev    = {
                .release	= hkc100_touch_release_device,
        },
};

//[*]--------------------------------------------------------------------------------------------------[*]
module_init(hkc100_touch_init);
module_exit(hkc100_touch_exit);

//[*]--------------------------------------------------------------------------------------------------[*]
MODULE_AUTHOR("HardKernel");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("HYUPJIN TOUCH interface for HKC100-Dev Board");

//[*]--------------------------------------------------------------------------------------------------[*]
static void	hkc100_touch_timer_irq(unsigned long arg)
{
	// Acc data read
	write_seqlock(&hkc100_touch.lock);

	if(!(hkc100_touch.cur_sysfs_state & SYSFS_STATE_MASK))	{
		if(GET_INT_STATUS())	{
			hkc100_touch_get_data();		hkc100_touch_timer_start();
			hkc100_touch.status = TOUCH_PRESS;
		}
		else	{
			if(hkc100_touch.status)	{
				input_report_key(hkc100_touch.driver, BTN_TOUCH, TOUCH_RELEASE);
				input_report_abs(hkc100_touch.driver, ABS_X, hkc100_touch.x);
				input_report_abs(hkc100_touch.driver, ABS_Y, hkc100_touch.y);
				input_sync(hkc100_touch.driver);
			
				#if	defined(DEBUG_HKC100_TOUCH_MSG)
					printk("%s : Penup event send[x = %d, y = %d]\n", __FUNCTION__, hkc100_touch.x, hkc100_touch.y);
				#endif
				
				hkc100_touch.x = -1;	hkc100_touch.y = -1;
				hkc100_touch.status = TOUCH_RELEASE;
			}
		}
	}
	else	{
		if(hkc100_touch.cur_sysfs_state & SYSFS_HOLD_STATE)	{
			if(hkc100_touch.status)	{
				input_report_key(hkc100_touch.driver, BTN_TOUCH, TOUCH_RELEASE);
				input_report_abs(hkc100_touch.driver, ABS_X, hkc100_touch.x);
				input_report_abs(hkc100_touch.driver, ABS_Y, hkc100_touch.y);
				input_sync(hkc100_touch.driver);
			
				hkc100_touch.status = TOUCH_RELEASE;
	
				#if	defined(DEBUG_HKC100_TOUCH_MSG)
					printk("%s : Penup event send[x = %d, y = %d]\n", __FUNCTION__, hkc100_touch.x, hkc100_touch.y);
				#endif
			}
			else	{
				if(GET_INT_STATUS())	input_report_switch(hkc100_touch.driver, SW_TOUCH_HOLD, 1);
				else					input_report_switch(hkc100_touch.driver, SW_TOUCH_HOLD, 0);
			}
		}
	}
	
	write_sequnlock(&hkc100_touch.lock);
}

//[*]--------------------------------------------------------------------------------------------------[*]
static void hkc100_touch_timer_start(void)
{
	init_timer(&hkc100_touch.penup_timer);
	hkc100_touch.penup_timer.data 		= (unsigned long)&hkc100_touch.penup_timer;
	hkc100_touch.penup_timer.function 	= hkc100_touch_timer_irq;
	
	switch(hkc100_touch.sampling_rate)	{
		default	:
			hkc100_touch.sampling_rate = 0;
			hkc100_touch.penup_timer.expires = jiffies + PERIOD_10MS;
			break;
		case	1:
			hkc100_touch.penup_timer.expires = jiffies + PERIOD_20MS;
			break;
		case	2:
			hkc100_touch.penup_timer.expires = jiffies + PERIOD_50MS;
			break;
	}

	add_timer(&hkc100_touch.penup_timer);
}

//[*]--------------------------------------------------------------------------------------------------[*]
static void hkc100_touch_get_data(void)
{
	unsigned short	temp_x, temp_y;

	// read hyupjin device register
	hkc100_touch_read(&hkc100_touch.rd[0], 5);
	
	if(hkc100_touch.rd[0] == MODE_SINGLE_TOUCH_READ)	{
		temp_x = (hkc100_touch.rd[1] << 8) | (hkc100_touch.rd[2]);
		temp_y = (hkc100_touch.rd[3] << 8) | (hkc100_touch.rd[4]);
		
		if((temp_x != 0) || (temp_y != 0))	{
			if((temp_x < TS_ABS_MAX_X) && (temp_y < TS_ABS_MAX_Y))	{

				temp_x = TS_ABS_MAX_X - temp_x;
				temp_y = TS_ABS_MAX_Y - temp_y;

				if((abs(hkc100_touch.x - temp_x) > hkc100_touch.threshold_x) ||
				   (abs(hkc100_touch.y - temp_y) > hkc100_touch.threshold_y))	{
					
					hkc100_touch.x 	= temp_x;	hkc100_touch.y	= temp_y;
					
					input_report_key(hkc100_touch.driver, BTN_TOUCH, TOUCH_PRESS);
					input_report_abs(hkc100_touch.driver, ABS_X, hkc100_touch.x);                  
					input_report_abs(hkc100_touch.driver, ABS_Y, hkc100_touch.y);
					input_sync(hkc100_touch.driver);
					
					#if	defined(DEBUG_HKC100_TOUCH_MSG)
						printk("%s : x = %d, y = %d \r\n", __FUNCTION__, hkc100_touch.x, hkc100_touch.y);
					#endif
				}
			}
		#if	defined(DEBUG_HKC100_TOUCH_MSG)
			else	{
				printk("%s : read value overflow x=%d, y=%d\n", __FUNCTION__, temp_x, temp_y);
			}
		#endif
		}
	#if	defined(DEBUG_HKC100_TOUCH_MSG)
		else	{
			printk("%s : read value error x=%d, y=%d\n", __FUNCTION__, temp_x, temp_y);
		}
	#endif
	}
	else	{
		#if	defined(DEBUG_HKC100_TOUCH_MSG)
			printk("%s [mode = 0x%02x]: touch re-initialize \r\n", __FUNCTION__, hkc100_touch.rd[0]);
		#endif
		// Single touch mode setup
		hkc100_touch_write(MODE_SINGLE_TOUCH_READ);	// set mode
	}
}

//------------------------------------------------------------------------------------------------------------------------
irqreturn_t	hkc100_touch_irq(int irq, void *dev_id)
{
	unsigned long	flags;
	
	local_irq_save(flags);	local_irq_disable();

	if(!hkc100_touch.status)	{
		del_timer_sync(&hkc100_touch.penup_timer);
		hkc100_touch_timer_start();
	}

	local_irq_restore(flags);
	
	return	IRQ_HANDLED;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static int	hkc100_touch_open	(struct input_dev *dev)
{
	#if	defined(DEBUG_HKC100_TOUCH_MSG)
		printk("%s\n", __FUNCTION__);
	#endif
	
	return	0;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static void	hkc100_touch_close	(struct input_dev *dev)
{
	#if	defined(DEBUG_HKC100_TOUCH_MSG)
		printk("%s\n", __FUNCTION__);
	#endif
}

//[*]--------------------------------------------------------------------------------------------------[*]
static void	hkc100_touch_release_device	(struct device *dev)
{
	#if	defined(DEBUG_HKC100_TOUCH_MSG)
		printk("%s\n", __FUNCTION__);
	#endif
}

//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
static 	int		hkc100_touch_resume			(struct platform_device *dev)
{
	#if	defined(DEBUG_HKC100_TOUCH_PM_MSG)
		printk("%s\n", __FUNCTION__);
	#endif

	hkc100_touch.cur_sysfs_state &= ~SYSFS_SLEEP_STATE;
	// wake-up cmd send
	hkc100_touch_sleep(0x00);
	hkc100_touch_sleep(0x00);
	
	hkc100_touch_config(TOUCH_STATE_RESUME);

	// interrupt enable
	enable_irq(HKC100_TOUCH_IRQ);

	return	0;
}
//[*]--------------------------------------------------------------------------------------------------[*]
static 	int		hkc100_touch_suspend		(struct platform_device *dev, pm_message_t state)
{
	#if	defined(DEBUG_HKC100_TOUCH_PM_MSG)
		printk("%s\n", __FUNCTION__);
	#endif

	// interrupt disable
	disable_irq(HKC100_TOUCH_IRQ);

	hkc100_touch.cur_sysfs_state |=  SYSFS_SLEEP_STATE;
	// sleep cmd send
	hkc100_touch_sleep(0x01);
	
	return	0;
}

//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
static void 	hkc100_touch_config(unsigned char state)
{
	//
	// HyupJin touch Device Init
	//
	hkc100_touch_port_init();
	
	#if	defined(DEBUG_HKC100_TOUCH_MSG)
		// module id read
		hkc100_touch_write(MODE_MODULE_ID_READ);	// set mode
		if(!hkc100_touch_read(&hkc100_touch.rd[0], 2))	printk("TOUCH ID         : 0x%02X\r\n", hkc100_touch.rd[1]);
			
		hkc100_touch_write(MODE_SLEEP_CTL);			// set mode
		if(!hkc100_touch_read(&hkc100_touch.rd[0], 2))	printk("SLEEP MODE       : 0x%02X\r\n", hkc100_touch.rd[1]);
	#endif

	hkc100_touch_write(MODE_SENSITIVITY_CTL);	// set mode
	if(!hkc100_touch_read(&hkc100_touch.rd[0], 2))	printk("TOUCH SENSITIVITY : 0x%02X\r\n", hkc100_touch.rd[1]);
	
	hkc100_touch.sensitivity = hkc100_touch.rd[1];

	hkc100_touch_write(MODE_MODULE_ID_READ);	// set mode
	if(!hkc100_touch_read(&hkc100_touch.rd[0], 2))	printk("TOUCH ID          : 0x%02X\r\n", hkc100_touch.rd[1]);

	// Single touch mode setup
	hkc100_touch_write(MODE_SINGLE_TOUCH_READ);	// set mode

	if(state == TOUCH_STATE_BOOT)	{
		if(!request_irq(HKC100_TOUCH_IRQ, hkc100_touch_irq, IRQF_DISABLED, "HKC100-Touch IRQ", (void *)&hkc100_touch))	{
			printk("HKC100 TOUCH request_irq = %d\r\n" , HKC100_TOUCH_IRQ);
		}
		else	{
			printk("HKC100 TOUCH request_irq = %d error!! \r\n", HKC100_TOUCH_IRQ);
		}
	
	//	set_irq_type(HKC100_TOUCH_IRQ, IRQ_TYPE_EDGE_RISING);
		set_irq_type(HKC100_TOUCH_IRQ, IRQ_TYPE_EDGE_BOTH);
	
		// seqlock init
		seqlock_init(&hkc100_touch.lock);		hkc100_touch.seq = 0;
	}

	hkc100_touch.status = TOUCH_RELEASE;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static int __devinit    hkc100_touch_probe	(struct platform_device *pdev)
{
	int		rc;

	// struct init
	memset(&hkc100_touch, 0x00, sizeof(hkc100_touch_t));
	
	// create sys_fs
	if((rc = hkc100_touch_sysfs_create(pdev)))	{
		printk("%s : sysfs_create_group fail!!\n", __FUNCTION__);
		return	rc;
	}
	
	hkc100_touch.driver = input_allocate_device();

    if(!(hkc100_touch.driver))	{
		printk("ERROR! : %s cdev_alloc() error!!! no memory!!\n", __FUNCTION__);
		hkc100_touch_sysfs_remove(pdev);
		return -ENOMEM;
    }

	hkc100_touch.driver->name 	= HKC100_TOUCH_DEVICE_NAME;
	hkc100_touch.driver->phys 	= "hkc100_touch/input1";
    hkc100_touch.driver->open 	= hkc100_touch_open;
    hkc100_touch.driver->close	= hkc100_touch_close;
	
	hkc100_touch.driver->id.bustype	= BUS_HOST;
	hkc100_touch.driver->id.vendor 	= 0x16B4;
	hkc100_touch.driver->id.product	= 0x0702;
	hkc100_touch.driver->id.version	= 0x0001;

	hkc100_touch.driver->evbit[0]  = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	hkc100_touch.driver->absbit[0] = BIT_MASK(ABS_X)  | BIT_MASK(ABS_Y);   
	hkc100_touch.driver->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);

	// Touch Hold Event
	set_bit(EV_SW, hkc100_touch.driver->evbit);
	set_bit(SW_TOUCH_HOLD & SW_MAX, hkc100_touch.driver->swbit);

	input_set_abs_params(hkc100_touch.driver, ABS_X, TS_ABS_MIN_X, TS_ABS_MAX_X, 0, 0);
	input_set_abs_params(hkc100_touch.driver, ABS_Y, TS_ABS_MIN_Y, TS_ABS_MAX_Y, 0, 0);
	
	if(input_register_device(hkc100_touch.driver))	{
		printk("HKC100 TOUCH input register device fail!!\n");

		hkc100_touch_sysfs_remove(pdev);
		input_free_device(hkc100_touch.driver);		return	-ENODEV;
	}

	hkc100_touch_config(TOUCH_STATE_BOOT);

	printk("HardKernel-C100 touch driver initialized!! Ver 1.0\n");

	return	0;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static int __devexit    hkc100_touch_remove	(struct platform_device *pdev)
{
	#if	defined(DEBUG_HKC100_TOUCH_MSG)
		printk("%s\n", __FUNCTION__);
	#endif
	
	free_irq(HKC100_TOUCH_IRQ, (void *)&hkc100_touch); 

	del_timer_sync(&hkc100_touch.penup_timer);

	hkc100_touch_sysfs_remove(pdev);

	input_unregister_device(hkc100_touch.driver);

	return  0;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static int __init	hkc100_touch_init	(void)
{
	int ret = platform_driver_register(&hkc100_touch_platform_device_driver);
	
	#if	defined(DEBUG_HKC100_TOUCH_MSG)
		printk("%s\n", __FUNCTION__);
	#endif
	
	if(!ret)        {
		ret = platform_device_register(&hkc100_touch_platform_device);
		
		#if	defined(DEBUG_HKC100_TOUCH_MSG)
			printk("platform_driver_register %d \n", ret);
		#endif
		
		if(ret)	platform_driver_unregister(&hkc100_touch_platform_device_driver);
	}
	return ret;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static void __exit	hkc100_touch_exit	(void)
{
	#if	defined(DEBUG_HKC100_TOUCH_MSG)
		printk("%s\n",__FUNCTION__);
	#endif
	
	platform_device_unregister(&hkc100_touch_platform_device);
	platform_driver_unregister(&hkc100_touch_platform_device_driver);
}

//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
