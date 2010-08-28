/*
 * Generic IXP4xx beeper driver
 *
 * Copyright (C) 2009 SAMSUNG Electronics
 *
 * Author: Hyoung Min Seo <hmseo@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/irq.h>
#include <linux/fs.h>
#include <linux/kdev_t.h>
#include <linux/suspend.h>
#include <linux/cdev.h>
#include <plat/map-base.h>
#include <plat/regs-serial.h>
#include <plat/regs-clock.h>
#include <plat/regs-gpio.h>
#include <plat/gpio-cfg.h>
#include <mach/hardware.h>
#include <mach/map.h>
#include <mach/regs-mem.h>
#include <mach/regs-irq.h>
#include <asm/gpio.h>

MODULE_AUTHOR("HyoungMin Seo <hmseo@samsung.com>");
MODULE_DESCRIPTION("SMDK Power Buttong driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:smdkbtn");

int pb_major = 111;
int pb_minor = 0;
int number_of_devices = 1;
static int dummy;       /* dev_id for request_irq() */

struct cdev cdev;
dev_t dev = 0;
bool PowerOffSignal;

static DEFINE_SPINLOCK(smdk_pwr_btn);

static irqreturn_t smdk_pwr_btn_interrupt(int irq, void *dev_id)
{
	printk(KERN_INFO "smdk_power_btn_interrupt called\n");
	PowerOffSignal = true;
	return IRQ_HANDLED;
}

static int __devinit smdk_power_btn_open(struct inode *inode, struct file *file)
{
	printk(KERN_INFO "smdk_power_btn_open called\n");
	PowerOffSignal = false;

	return 0;
}

static void smdk_power_btn_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	if (PowerOffSignal == true)
	{
		PowerOffSignal = false;
		pm_suspend(PM_SUSPEND_MEM);
	}
}

static int smdk_power_btn_release(struct inode *inode, struct file *file)
{
	printk (KERN_INFO "Device closed\n");
	return 0;
}

static unsigned int s3c_button_gpio_init(void)
{
	u32 err;

	err = gpio_request(S5P64XX_GPH1(2),"GPH1");
	if (err){
		printk("gpio request error : %d\n",err);
	}else{
		s3c_gpio_cfgpin(S5P64XX_GPH1(2),S5P64XX_GPH1_2_EXT_INT1_2);
		s3c_gpio_setpull(S5P64XX_GPH1(2), S3C_GPIO_PULL_NONE);
	}

	return err;
}

struct file_operations smdk_power_btn_fops = {
	.owner		= THIS_MODULE,
	.open 		= smdk_power_btn_open,
	.release	= smdk_power_btn_release,
	.write		= smdk_power_btn_write,
};

static int __init smdk_power_btn_init(void)
{
	int result;
	int err;

	dev = MKDEV(pb_major, pb_minor);
	result = register_chrdev_region(dev, number_of_devices, "smdk-btn");
	if (result < 0) {
		printk (KERN_WARNING "Power BTN: can't get major number %d\n", pb_major);
	}

	cdev_init (&cdev, &smdk_power_btn_fops);
	cdev.owner = THIS_MODULE;
	cdev.ops = &smdk_power_btn_fops;
	result = cdev_add(&cdev, dev, 1);
	if (result) printk (KERN_NOTICE "Error %d adding character device", result);

	PowerOffSignal = false;
	s3c_button_gpio_init();

	set_irq_type(IRQ_EINT10, IRQ_TYPE_EDGE_FALLING);

	err = request_irq(IRQ_EINT10, &smdk_pwr_btn_interrupt,
			  IRQF_DISABLED, "smdk-btn", &dummy);
	if (err)
	{
		printk(KERN_INFO "IRQ_EINT10 request_irq failed\n");
		goto err_free_irq;
	}

	printk(KERN_INFO "Write driver registered\n");

	return 0;

err_free_irq:
	free_irq(IRQ_EINT10, &dummy);

	return err;
}

static void __exit smdk_power_btn_exit(void)
{
	dev_t devno = MKDEV (pb_major, pb_minor);
	cdev_del(&cdev);
	unregister_chrdev_region (devno, number_of_devices);
	printk(KERN_INFO "Write driver released\n");
}

module_init(smdk_power_btn_init);
module_exit(smdk_power_btn_exit);
