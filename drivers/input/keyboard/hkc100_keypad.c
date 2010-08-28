//[*]--------------------------------------------------------------------------------------------------[*]
/*
 *
 * HKC100 Dev Board keypad driver (charles.park)
 *
 */
//[*]--------------------------------------------------------------------------------------------------[*]
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/clk.h>

#include <linux/delay.h>
#include <linux/io.h>
#include <mach/hardware.h>
#include <asm/delay.h>
#include <asm/irq.h>

#include <plat/regs-gpio.h>
#include <plat/regs-keypad.h>

#ifdef CONFIG_CPU_FREQ
#include <plat/s5pc1xx-dvfs.h>
#endif

//[*]--------------------------------------------------------------------------------------------------[*]
#include "hkc100_keypad.h"
#include "hkc100_keycode.h"
#include "hkc100_keypad_sysfs.h"

//[*]--------------------------------------------------------------------------------------------------[*]
#define DEVICE_NAME "hkc100-keypad"

// Debug message enable flag
//#define	DEBUG_MSG			
#define	DEBUG_PM_MSG

//[*]--------------------------------------------------------------------------------------------------[*]
hkc100_keypad_t	hkc100_keypad;

//[*]--------------------------------------------------------------------------------------------------[*]
static void				generate_keycode				(unsigned short prev_key, unsigned short cur_key, const int *key_table);
static void 			hkc100_keyled_control			(int keycode, unsigned char status);
static void 			hkc100_poweroff_timer_handler	(unsigned long data);

static void 			hdmi_connect_control			(void);
static void 			hkc100_keypad_control			(void);

static void				hkc100_rd_timer_handler			(unsigned long data);

static int              hkc100_keypad_open              (struct input_dev *dev);
static void             hkc100_keypad_close             (struct input_dev *dev);

static void             hkc100_keypad_release_device    (struct device *dev);
static int              hkc100_keypad_resume            (struct platform_device *dev);
static int              hkc100_keypad_suspend           (struct platform_device *dev, pm_message_t state);

static void				hkc100_keypad_config			(unsigned char state);

static int __devinit    hkc100_keypad_probe				(struct platform_device *pdev);
static int __devexit    hkc100_keypad_remove			(struct platform_device *pdev);

static int __init       hkc100_keypad_init				(void);
static void __exit      hkc100_keypad_exit				(void);

//[*]--------------------------------------------------------------------------------------------------[*]
static struct platform_driver hkc100_platform_device_driver = {
		.probe          = hkc100_keypad_probe,
		.remove         = hkc100_keypad_remove,
		.suspend        = hkc100_keypad_suspend,
		.resume         = hkc100_keypad_resume,
		.driver		= {
			.owner	= THIS_MODULE,
			.name	= DEVICE_NAME,
		},
};

//[*]--------------------------------------------------------------------------------------------------[*]
static struct platform_device hkc100_platform_device = {
        .name           = DEVICE_NAME,
        .id             = 0,
        .num_resources  = 0,
        .dev    = {
                .release	= hkc100_keypad_release_device,
        },
};

static 	unsigned int	wait_count = 0;
//[*]--------------------------------------------------------------------------------------------------[*]
module_init(hkc100_keypad_init);
module_exit(hkc100_keypad_exit);

//[*]--------------------------------------------------------------------------------------------------[*]
MODULE_AUTHOR("Hard-Kernel");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Keypad interface for HKC100-Dev board");

//[*]--------------------------------------------------------------------------------------------------[*]
static void hkc100_keyled_control(int keycode, unsigned char status)
{
	if(!hkc100_keypad.led_state)	return;
		
	switch(keycode)	{
		case	KEY_VOLUMEDOWN:	LED1_ONOFF(status);		break;
		case	KEY_VOLUMEUP:	LED2_ONOFF(status);		break;
		case	KEY_L:		
			if(status)	{	LED1_ONOFF(OFF);	}
			else		{	LED1_ONOFF(ON);		}
			break;
		case	KEY_R:			
			if(status)	{	LED2_ONOFF(OFF);	}
			else		{	LED2_ONOFF(ON);		}
			break;
		default:										break;
	}
}

//[*]--------------------------------------------------------------------------------------------------[*]
static void hkc100_poweroff_timer_handler(unsigned long data)
{
	// POWER OFF 
	hkc100_keypad.poweroff_flag	= TRUE;
	POWER_LED_OFF();
}

//[*]--------------------------------------------------------------------------------------------------[*]
static void	generate_keycode(unsigned short prev_key, unsigned short cur_key, const int *key_table)
{
	unsigned short 	press_key, release_key, i;
	
	press_key	= (cur_key ^ prev_key) & cur_key;
	release_key	= (cur_key ^ prev_key) & prev_key;
	
	i = 0;
	while(press_key)	{
		if(press_key & 0x01)	{
			
			if(hkc100_keypad.hold_state)	{
				if(key_table[i] == KEY_POWER)	input_report_key(hkc100_keypad.driver, key_table[i], KEY_PRESS);					
				else							input_report_switch(hkc100_keypad.driver, SW_KEY_HOLD, ON);
			}
			else	{
				input_report_key(hkc100_keypad.driver, key_table[i], KEY_PRESS);
				hkc100_keyled_control(key_table[i], KEY_PRESS);
			}
			
			// POWER OFF PRESS
			if(key_table[i] == KEY_POWER)	{
				init_timer(&hkc100_keypad.poweroff_timer);
				hkc100_keypad.poweroff_timer.function = hkc100_poweroff_timer_handler;
			
				switch(hkc100_keypad.poweroff_time)	{
					default	:
						hkc100_keypad.poweroff_time = 0;
					case	0:
						hkc100_keypad.poweroff_timer.expires = jiffies + PERIOD_1SEC;
						break;
					case	1:
						hkc100_keypad.poweroff_timer.expires = jiffies + PERIOD_3SEC;
						break;
					case	2:
						hkc100_keypad.poweroff_timer.expires = jiffies + PERIOD_5SEC;
						break;
				}
				add_timer(&hkc100_keypad.poweroff_timer);
			}
		}
		i++;	press_key >>= 1;
	}
	
	i = 0;
	while(release_key)	{
		if(release_key & 0x01)	{

			if(hkc100_keypad.hold_state)	{
				if(key_table[i] == KEY_POWER)	input_report_key(hkc100_keypad.driver, key_table[i], KEY_RELEASE);
				else							input_report_switch(hkc100_keypad.driver, SW_KEY_HOLD, OFF);
			}
			else	{
				input_report_key(hkc100_keypad.driver, key_table[i], KEY_RELEASE);
				hkc100_keyled_control(key_table[i], KEY_RELEASE);
			}

			// POWER OFF (LONG PRESS)		
			if(key_table[i] == KEY_POWER)	{
				del_timer_sync(&hkc100_keypad.poweroff_timer);
				if(hkc100_keypad.poweroff_flag)	POWER_OFF();
				hkc100_keypad.poweroff_flag = FALSE;	
			}
		}
		i++;	release_key >>= 1;
	}
}
//[*]--------------------------------------------------------------------------------------------------[*]
#if defined(DEBUG_MSG)
	static void debug_keycode_printf(unsigned short prev_key, unsigned short cur_key, const char *key_table)
	{
		unsigned short 	press_key, release_key, i;
		
		press_key	= (cur_key ^ prev_key) & cur_key;
		release_key	= (cur_key ^ prev_key) & prev_key;
		
		i = 0;
		while(press_key)	{
			if(press_key & 0x01)	printk("PRESS KEY : %s", (char *)&key_table[(i + (hkc100_keypad.mode * MAX_KEYPAD_CNT)) * 20]);
			i++;					press_key >>= 1;
		}
		
		i = 0;
		while(release_key)	{
			if(release_key & 0x01)	printk("RELEASE KEY : %s", (char *)&key_table[(i + (hkc100_keypad.mode * MAX_KEYPAD_CNT)) * 20]);
			i++;					release_key >>= 1;
		}
	}
#endif

//[*]--------------------------------------------------------------------------------------------------[*]
static void hkc100_keypad_control(void)
{
	static	unsigned short	prev_keypad_data = 0, cur_keypad_data = 0;
	static	unsigned char	prev_keypad_mode = -1, cur_keypad_mode = 0;

	// use for lcd rotate
	cur_keypad_mode = GET_KEYPAD_MODE();

	if(prev_keypad_mode != cur_keypad_mode)	{

		prev_keypad_mode = cur_keypad_mode;
		
		hkc100_keypad.slide_sw_state = cur_keypad_mode;
		
		// send rotate event
		if(cur_keypad_mode)		input_report_switch(hkc100_keypad.driver, SW_SCREEN_ROTATE, ON);
		else					input_report_switch(hkc100_keypad.driver, SW_SCREEN_ROTATE, OFF);

		input_sync(hkc100_keypad.driver);
	}

	// key data process
	cur_keypad_data = GET_KEYPAD_DATA();

	#if defined(SLIDE_VOLUME_LOCK)
		// Volume key mask
		if(!hkc100_keypad.slide_sw_state)	cur_keypad_data &= (~0x3000);
	#endif

	if(prev_keypad_data != cur_keypad_data)	{
		
		generate_keycode(prev_keypad_data, cur_keypad_data, &HKC100_KeyMap[0]);

		#if defined(DEBUG_MSG)
			debug_keycode_printf(prev_keypad_data, cur_keypad_data, &HKC100_KeyMapStr[0][0]);
		#endif

		prev_keypad_data = cur_keypad_data;

		input_sync(hkc100_keypad.driver);
	}
}

//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
#if defined(SPDIF_ONOFF_CONTROL)
	extern void spdif_out_onoff(bool onoff);
#endif
//[*]--------------------------------------------------------------------------------------------------[*]
static void hdmi_connect_control(void)
{
	static	unsigned char	prev_hdmi_status = -1, cur_hdmi_status = 0;

	cur_hdmi_status = GET_HDMI_STATUS();

	if(prev_hdmi_status != cur_hdmi_status)	{
		prev_hdmi_status = cur_hdmi_status;
		hkc100_keypad.hdmi_state = cur_hdmi_status;

		if(cur_hdmi_status)	{
			
			#if defined(HDMI_POWER_CONTROL)
				HDMI_POWER_ONOFF(OFF);
			#else
				HDMI_POWER_ONOFF(ON);	// HDMI Always on : i2c error fix
			#endif
			
			#if defined(SPDIF_ONOFF_CONTROL)
				spdif_out_onoff(0);
			#endif
			
			input_report_switch(hkc100_keypad.driver, SW_HDMI, OFF);
			
			#if defined(DEBUG_MSG)
				printk("HDMI Cable Disconnected!!\n");
			#endif
		}
		else	{

			#if defined(HDMI_POWER_CONTROL)
				HDMI_POWER_ONOFF(ON);
			#else
				HDMI_POWER_ONOFF(ON);	// HDMI Always on : i2c error fix
			#endif

			#if defined(SPDIF_ONOFF_CONTROL)
				spdif_out_onoff(1);
			#endif
			
			input_report_switch(hkc100_keypad.driver, SW_HDMI, ON);

			#if defined(DEBUG_MSG)
				printk("HDMI Cable Connected!!\n");
			#endif
		}
		input_sync(hkc100_keypad.driver);
	}
}

//[*]--------------------------------------------------------------------------------------------------[*]
static void	hkc100_rd_timer_handler(unsigned long data)
{
    unsigned long flags;

    local_irq_save(flags);

	if(hkc100_keypad.wakeup_delay > KEYPAD_WAKEUP_DELAY)	hkc100_keypad_control();	
	else													hkc100_keypad.wakeup_delay++;
		
	hdmi_connect_control();
	
	// Kernel Timer restart
	switch(hkc100_keypad.sampling_rate)	{
		default	:
			hkc100_keypad.sampling_rate = 0;
		case	0:
			mod_timer(&hkc100_keypad.rd_timer,jiffies + PERIOD_10MS);
			break;
		case	1:
			mod_timer(&hkc100_keypad.rd_timer,jiffies + PERIOD_20MS);
			break;
		case	2:
			mod_timer(&hkc100_keypad.rd_timer,jiffies + PERIOD_50MS);
			break;
	}

    local_irq_restore(flags);
}

//[*]--------------------------------------------------------------------------------------------------[*]
static int	hkc100_keypad_open(struct input_dev *dev)
{
	#if	defined(DEBUG_MSG)
		printk("%s\n", __FUNCTION__);
	#endif
	
	return	0;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static void	hkc100_keypad_close(struct input_dev *dev)
{
	#if	defined(DEBUG_MSG)
		printk("%s\n", __FUNCTION__);
	#endif
}

//[*]--------------------------------------------------------------------------------------------------[*]
static void	hkc100_keypad_release_device(struct device *dev)
{
	#if	defined(DEBUG_MSG)
		printk("%s\n", __FUNCTION__);
	#endif
}

//[*]--------------------------------------------------------------------------------------------------[*]
static int	hkc100_keypad_resume(struct platform_device *dev)
{
	#if	defined(DEBUG_PM_MSG)
		printk("%s\n", __FUNCTION__);
	#endif
	
	hkc100_keypad_config(KEYPAD_STATE_RESUME);
	
    combo_module_control();

	// Wakeup dumy key send
	input_report_key(hkc100_keypad.driver, KEY_HOME, KEY_PRESS);					
	input_report_key(hkc100_keypad.driver, KEY_HOME, KEY_RELEASE);					
	input_sync(hkc100_keypad.driver);
	
	hkc100_keypad.wakeup_delay = 0;

    return  0;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static int	hkc100_keypad_suspend(struct platform_device *dev, pm_message_t state)
{
	#if	defined(DEBUG_PM_MSG)
		printk("%s\n", __FUNCTION__);
	#endif

	del_timer_sync(&hkc100_keypad.rd_timer);
	
	LED1_ONOFF(OFF);	LED2_ONOFF(OFF);

	HDMI_POWER_ONOFF(OFF);
	
	combo_module_suspend();
	
    return  0;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static void	hkc100_keypad_config(unsigned char state)
{
	KEY_PORT_INIT();	// GPH2, GPH3 All Input, All Pull up
	LED_PORT_INIT();	// GPA0.0, GPA0.1 Output, Pull Up

	POWER_OFF_PORT_INIT();		HDMI_POWER_PORT_INIT();	

	HDMI_POWER_ONOFF(ON);

	// GPA0.0, GPA0.1 LED OFF
	LED1_ONOFF(OFF);	LED2_ONOFF(OFF);	mdelay(20);	
	LED1_ONOFF(ON);		LED2_ONOFF(ON);		mdelay(20);	
	LED1_ONOFF(OFF);	LED2_ONOFF(OFF);
		
	/* Scan timer init */
	init_timer(&hkc100_keypad.rd_timer);

	hkc100_keypad.rd_timer.function = hkc100_rd_timer_handler;
	hkc100_keypad.rd_timer.expires = jiffies + (HZ/100);

	add_timer(&hkc100_keypad.rd_timer);
}

//[*]--------------------------------------------------------------------------------------------------[*]
static int __devinit    hkc100_keypad_probe(struct platform_device *pdev)
{
    int 	key, code, rc;

	// struct init
	memset(&hkc100_keypad, 0x00, sizeof(hkc100_keypad_t));
	
	// create sys_fs
	if((rc = hkc100_keypad_sysfs_create(pdev)))	{
		printk("%s : sysfs_create_group fail!!\n", __FUNCTION__);
		return	rc;
	}

	hkc100_keypad.driver = input_allocate_device();
	
    if(!(hkc100_keypad.driver))	{
		printk("ERROR! : %s input_allocate_device() error!!! no memory!!\n", __FUNCTION__);
		hkc100_keypad_sysfs_remove(pdev);
		return -ENOMEM;
    }

	set_bit(EV_KEY, hkc100_keypad.driver->evbit);
//	set_bit(EV_REP, hkc100_keypad.driver->evbit);	// Repeat Key

	// HDMI Detect Event / key hold event / screen rotate event
	set_bit(EV_SW, hkc100_keypad.driver->evbit);
	set_bit(SW_HDMI 			& SW_MAX, hkc100_keypad.driver->swbit);
	set_bit(SW_KEY_HOLD			& SW_MAX, hkc100_keypad.driver->swbit);
	set_bit(SW_SCREEN_ROTATE 	& SW_MAX, hkc100_keypad.driver->swbit);

	for(key = 0; key < MAX_KEYCODE_CNT; key++){
		code = HKC100_Keycode[key];
		if(code<=0)
			continue;
		set_bit(code & KEY_MAX, hkc100_keypad.driver->keybit);
	}

	hkc100_keypad.driver->name 	= DEVICE_NAME;
	hkc100_keypad.driver->phys 	= "hkc100-keypad/input0";
    hkc100_keypad.driver->open 	= hkc100_keypad_open;
    hkc100_keypad.driver->close	= hkc100_keypad_close;
	
	hkc100_keypad.driver->id.bustype	= BUS_HOST;
	hkc100_keypad.driver->id.vendor 	= 0x16B4;
	hkc100_keypad.driver->id.product 	= 0x0701;
	hkc100_keypad.driver->id.version 	= 0x0001;

	hkc100_keypad.driver->keycode = HKC100_Keycode;

	if(input_register_device(hkc100_keypad.driver))	{
		printk("HardKernel-C100 keypad input register device fail!!\n");

		hkc100_keypad_sysfs_remove(pdev);
		input_free_device(hkc100_keypad.driver);	return	-ENODEV;
	}

	hkc100_keypad_config(KEYPAD_STATE_BOOT);

	printk("HardKernel-C100(HKC100) dev board keypad driver initialized!!\n");

    return 0;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static int __devexit    hkc100_keypad_remove(struct platform_device *pdev)
{
	hkc100_keypad_sysfs_remove(pdev);

	input_unregister_device(hkc100_keypad.driver);
	
	del_timer_sync(&hkc100_keypad.rd_timer);
	
	#if	defined(DEBUG_MSG)
		printk("%s\n", __FUNCTION__);
	#endif
	
	return  0;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static int __init	hkc100_keypad_init(void)
{
	int ret = platform_driver_register(&hkc100_platform_device_driver);
	
	#if	defined(DEBUG_MSG)
		printk("%s\n", __FUNCTION__);
	#endif
	
	if(!ret)        {
		ret = platform_device_register(&hkc100_platform_device);
		
		#if	defined(DEBUG_MSG)
			printk("platform_driver_register %d \n", ret);
		#endif
		
		if(ret)	platform_driver_unregister(&hkc100_platform_device_driver);
	}
	return ret;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static void __exit	hkc100_keypad_exit(void)
{
	#if	defined(DEBUG_MSG)
		printk("%s\n",__FUNCTION__);
	#endif
	
	platform_device_unregister(&hkc100_platform_device);
	platform_driver_unregister(&hkc100_platform_device_driver);
}

//[*]--------------------------------------------------------------------------------------------------[*]
