//[*]--------------------------------------------------------------------------------------------------[*]
//
//
// 
//  HKC100 Board : HKC100 Keypad Interface driver (charles.park)
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
#include <linux/input.h>

#include <plat/regs-gpio.h>
#include <plat/regs-keypad.h>

//[*]--------------------------------------------------------------------------------------------------[*]
#include "hkc100_keypad.h"

//[*]--------------------------------------------------------------------------------------------------[*]
//   function prototype define
//[*]--------------------------------------------------------------------------------------------------[*]
int		hkc100_keypad_sysfs_create		(struct platform_device *pdev);
void	hkc100_keypad_sysfs_remove		(struct platform_device *pdev);	
		
//[*]--------------------------------------------------------------------------------------------------[*]
//
//   sysfs function prototype define
//
//[*]--------------------------------------------------------------------------------------------------[*]
//   keypad hold control (on -> hold, off -> normal mode)
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
//   led on/off state (on -> normal led control, off -> led always off)
//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_led_state			(struct device *dev, struct device_attribute *attr, char *buf);
static	ssize_t set_led_state			(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static	DEVICE_ATTR(led_state, S_IRWXUGO, show_led_state, set_led_state);
	
//[*]--------------------------------------------------------------------------------------------------[*]
//   long power off control (set to 1 than touch controller reset)
//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_poweroff_control	(struct device *dev, struct device_attribute *attr, char *buf);
static 	ssize_t set_poweroff_control	(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static	DEVICE_ATTR(poweroff_control, S_IRWXUGO, show_poweroff_control, set_poweroff_control);

//[*]--------------------------------------------------------------------------------------------------[*]
//   HDMI Connect status
//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t show_hdmi_connect_state	(struct device *dev, struct device_attribute *attr, char *buf);
static 	DEVICE_ATTR(hdmi_connect_state, S_IRUGO, show_hdmi_connect_state, NULL);

//[*]--------------------------------------------------------------------------------------------------[*]
//   slide Switch status
//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t show_slide_sw_state		(struct device *dev, struct device_attribute *attr, char *buf);
static 	DEVICE_ATTR(slide_sw_state, S_IRUGO, show_slide_sw_state, NULL);

//[*]--------------------------------------------------------------------------------------------------[*]
// 	 Wifi & Bluetooth module power control struct (if wifi disable && bt disable than module power down)
//[*]--------------------------------------------------------------------------------------------------[*]
#define	GPJ0CON				(*(unsigned long *)S5PC1XX_GPJ0CON)
#define	GPJ0DAT				(*(unsigned long *)S5PC1XX_GPJ0DAT)
#define	GPJ0PUD				(*(unsigned long *)S5PC1XX_GPJ0PUD)

#if defined(CONFIG_HAS_WAKELOCK)
	#include <linux/wakelock.h>
#endif

typedef	struct	combo_module__t	{
	unsigned char		status_wifi;
	unsigned char		status_wifi_wakeup;
	unsigned char		status_bt;
	unsigned char		status_bt_wakeup;

#if defined(CONFIG_HAS_WAKELOCK)
	#if defined(COMBO_MODULE_WAKELOCK)
		struct wake_lock	wakelock;
	#endif
#endif
}	combo_module_t	;

static	combo_module_t	combo_module;

void combo_module_control(void);
//[*]--------------------------------------------------------------------------------------------------[*]
//   WIFI Reset Control (Set 1 to Enable : Reset High)
//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_wifi_onoff	(struct device *dev, struct device_attribute *attr, char *buf);
static 	ssize_t set_wifi_onoff	(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static	DEVICE_ATTR(wifi_onoff, S_IRWXUGO, show_wifi_onoff, set_wifi_onoff);

//[*]--------------------------------------------------------------------------------------------------[*]
//   BT Reset Controll (Set 1 to Enable : Reset High)
//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_bt_onoff	(struct device *dev, struct device_attribute *attr, char *buf);
static 	ssize_t set_bt_onoff	(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static	DEVICE_ATTR(bt_onoff, S_IRWXUGO, show_bt_onoff, set_bt_onoff);

//[*]--------------------------------------------------------------------------------------------------[*]
//   Module wakeup Controll (Set 1 to Wakeup : Reset suspend)
//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_wifi_wakeup	(struct device *dev, struct device_attribute *attr, char *buf);
static 	ssize_t set_wifi_wakeup	(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static	DEVICE_ATTR(wifi_wakeup, S_IRWXUGO, show_wifi_wakeup, set_wifi_wakeup);

//[*]--------------------------------------------------------------------------------------------------[*]
//   BT Module wakeup Controll (Set 1 to Wakeup : Reset suspend)
//[*]--------------------------------------------------------------------------------------------------[*]
static	ssize_t show_bt_wakeup	(struct device *dev, struct device_attribute *attr, char *buf);
static 	ssize_t set_bt_wakeup	(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static	DEVICE_ATTR(bt_wakeup, S_IRWXUGO, show_bt_wakeup, set_bt_wakeup);

//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
static struct attribute *hkc100_keypad_sysfs_entries[] = {
	&dev_attr_hold_state.attr,
	&dev_attr_sampling_rate.attr,
	&dev_attr_led_state.attr,
	&dev_attr_poweroff_control.attr,
	&dev_attr_hdmi_connect_state.attr,
	&dev_attr_slide_sw_state.attr,
	&dev_attr_wifi_onoff.attr,
	&dev_attr_bt_onoff.attr,
	&dev_attr_wifi_wakeup.attr,
	&dev_attr_bt_wakeup.attr,
	NULL
};

static struct attribute_group hkc100_keypad_attr_group = {
	.name   = NULL,
	.attrs  = hkc100_keypad_sysfs_entries,
};

//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t show_hold_state			(struct device *dev, struct device_attribute *attr, char *buf)
{
	if(hkc100_keypad.hold_state)	return	sprintf(buf, "on\n");
	else							return	sprintf(buf, "off\n");
}

//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t set_hold_state			(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    unsigned long flags;

    local_irq_save(flags);

	if(!strcmp(buf, "on\n"))	hkc100_keypad.hold_state = 1;
	else						hkc100_keypad.hold_state = 0;

    local_irq_restore(flags);

    return count;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t show_sampling_rate		(struct device *dev, struct device_attribute *attr, char *buf)
{
	switch(hkc100_keypad.sampling_rate)	{
		default	:
			hkc100_keypad.sampling_rate = 0;
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

    if(!(sscanf(buf, "%u\n", &val))) 	return	-EINVAL;
    
    local_irq_save(flags);
    
    if		(val > 20)		hkc100_keypad.sampling_rate = 2;
    else if	(val > 10)		hkc100_keypad.sampling_rate = 1;
    else					hkc100_keypad.sampling_rate = 0;

    local_irq_restore(flags);

    return count;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t show_led_state		(struct device *dev, struct device_attribute *attr, char *buf)
{
	if(hkc100_keypad.led_state)		return	sprintf(buf, "on\n");
	else							return	sprintf(buf, "off\n");
}

//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t set_led_state		(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    unsigned long flags;

    local_irq_save(flags);

	if(!strcmp(buf, "on\n"))	hkc100_keypad.led_state = 1;
	else						hkc100_keypad.led_state = 0;

	LED1_ONOFF(OFF);	LED2_ONOFF(OFF);
    
    local_irq_restore(flags);

    return count;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t show_poweroff_control	(struct device *dev, struct device_attribute *attr, char *buf)
{
	switch(hkc100_keypad.poweroff_time)	{
		default	:
			hkc100_keypad.poweroff_time = 0;
		case	0:
			return	sprintf(buf, "1 sec\n");
		case	1:
			return	sprintf(buf, "3 sec\n");
		case	2:
			return	sprintf(buf, "5 sec\n");
	}
}

//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t set_poweroff_control	(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    unsigned long 	flags;
    unsigned int	val;

    if(!(sscanf(buf, "%u\n", &val)))	return	-EINVAL;

    local_irq_save(flags);
    
    if		(val > 3)		hkc100_keypad.sampling_rate = 2;
    else if	(val > 1)		hkc100_keypad.sampling_rate = 1;
    else					hkc100_keypad.sampling_rate = 0;

    local_irq_restore(flags);

    return count;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t show_hdmi_connect_state	(struct device *dev, struct device_attribute *attr, char *buf)
{
	if(hkc100_keypad.hdmi_state)		return	sprintf(buf, "off\n");
	else								return	sprintf(buf, "on\n");
}

//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t show_slide_sw_state		(struct device *dev, struct device_attribute *attr, char *buf)
{
	if(hkc100_keypad.slide_sw_state)	return	sprintf(buf, "on\n");
	else								return	sprintf(buf, "off\n");
}

//[*]--------------------------------------------------------------------------------------------------[*]
void combo_module_suspend(void)
{
	combo_module.status_wifi		= 0;
	combo_module.status_wifi_wakeup	= 0;
	combo_module.status_bt			= 0;
	combo_module.status_bt_wakeup	= 0;
}

//[*]--------------------------------------------------------------------------------------------------[*]
void combo_module_control(void)
{
	if(combo_module.status_wifi)	{
		GPJ0DAT |= ( 0x00000001);	mdelay(1);		GPJ0DAT |= ( 0x00000002);
		printk("%s : wifi reset high!!\n", __FUNCTION__);
	}
	else	{
		combo_module.status_wifi_wakeup = 0;
		GPJ0DAT &= (~0x00000003);
		printk("%s : wifi reset low!!\n", __FUNCTION__);
	}

	#if defined(CONFIG_HAS_WAKELOCK)
		#if defined(COMBO_MODULE_WAKELOCK)
			if(combo_module.status_wifi || combo_module.status_bt)	{
				wake_lock(&combo_module.wakelock);
				printk("%s : wake lock!!\n", __FUNCTION__);
			}		
			else	{
				wake_unlock(&combo_module.wakelock);
				printk("%s : wake lock!!\n", __FUNCTION__);
			}
		#endif
	#endif
}

//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t show_bt_wakeup		(struct device *dev, struct device_attribute *attr, char *buf)
{
	return	sprintf(buf, "%d\n", combo_module.status_bt_wakeup);
}

//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t set_bt_wakeup		(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    unsigned int	val;

    if(!(sscanf(buf, "%u\n", &val))) 	return	-EINVAL;

	if(val != 0)	combo_module.status_bt_wakeup = 1;
	else			combo_module.status_bt_wakeup = 0;

    return count;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t show_wifi_wakeup		(struct device *dev, struct device_attribute *attr, char *buf)
{
	return	sprintf(buf, "%d\n", combo_module.status_wifi_wakeup);
}

//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t set_wifi_wakeup		(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    unsigned int	val;

    if(!(sscanf(buf, "%u\n", &val))) 	return	-EINVAL;

	if(val != 0)	combo_module.status_wifi_wakeup = 1;
	else			combo_module.status_wifi_wakeup = 0;

    return count;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t show_wifi_onoff		(struct device *dev, struct device_attribute *attr, char *buf)
{
	return	sprintf(buf, "%d\n", combo_module.status_wifi);
}

//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t set_wifi_onoff		(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    unsigned int	val;

    if(!(sscanf(buf, "%u\n", &val))) 	return	-EINVAL;

	if(val != 0)	combo_module.status_wifi = 1;
	else			combo_module.status_wifi = 0;

	combo_module_control();
	
    return count;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t show_bt_onoff		(struct device *dev, struct device_attribute *attr, char *buf)
{
	return	sprintf(buf, "%d\n", combo_module.status_bt);
}

//[*]--------------------------------------------------------------------------------------------------[*]
static 	ssize_t set_bt_onoff		(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    unsigned int	val;

    if(!(sscanf(buf, "%u\n", &val))) 	return	-EINVAL;
    
	if(val != 0)	combo_module.status_bt = 1;
	else			combo_module.status_bt = 0;

	if(combo_module.status_bt)	{
		GPJ0DAT |= ( 0x00000004);	printk("%s : bt reset high!!\n", __FUNCTION__);
	}
	else	{
		GPJ0DAT &= (~0x00000004);	printk("%s : bt reset low!!\n", __FUNCTION__);
	}

    return count;
}

//[*]--------------------------------------------------------------------------------------------------[*]
int		hkc100_keypad_sysfs_create		(struct platform_device *pdev)	
{
	// variable init
	hkc100_keypad.hold_state		= 0;	// key hold off
	hkc100_keypad.sampling_rate		= 0;	// 10 msec sampling
	hkc100_keypad.led_state			= 1;	// sleep off
	hkc100_keypad.poweroff_time		= 1;	// 3 sec

	// combo module power & reset control
	combo_module.status_wifi 		= 0;
	combo_module.status_wifi_wakeup	= 0;
	combo_module.status_bt 			= 0;
	combo_module.status_bt_wakeup 	= 0;

#if defined(CONFIG_HAS_WAKELOCK)
	#if defined(COMBO_MODULE_WAKELOCK)
		wake_lock_init(&combo_module.wakelock, WAKE_LOCK_SUSPEND, "combo_module_wakelock");
	#endif
#endif

	combo_module_control();

	return	sysfs_create_group(&pdev->dev.kobj, &hkc100_keypad_attr_group);
}

//[*]--------------------------------------------------------------------------------------------------[*]
void	hkc100_keypad_sysfs_remove		(struct platform_device *pdev)	
{
#if defined(CONFIG_HAS_WAKELOCK)
	#if defined(COMBO_MODULE_WAKELOCK)
		wake_lock_destroy(&combo_module.wakelock);
	#endif
#endif

    sysfs_remove_group(&pdev->dev.kobj, &hkc100_keypad_attr_group);
}
//[*]--------------------------------------------------------------------------------------------------[*]
//[*]--------------------------------------------------------------------------------------------------[*]

