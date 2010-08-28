/*
 * linux/arch/arm/mach-s5pc100/mach-universal.c
 *
 * Copyright (C) 2009 Samsung Electronics Co.Ltd
 * Author: InKi Dae <inki.dae@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/regulator/max8698.h>
#include <linux/gpio.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>
#include <linux/videodev2.h>
#include <media/s5k6aa_platform.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <plat/cpu.h>
#include <plat/devs.h>
#include <plat/fb.h>
#include <plat/iic.h>
#include <plat/regs-serial.h>
#include <plat/s5pc100.h>
#include <plat/gpio-cfg.h>
#include <plat/gpio-bank-e0.h>
#include <plat/gpio-bank-e1.h>
#include <plat/gpio-bank-h2.h>
#include <plat/gpio-bank-h3.h>
/*#include <mach/gpio.h>*/
#include <mach/map.h>

/* Camera interface */
#include <plat/fimc.h>
#include <plat/csis.h>

extern struct sys_timer s5pc1xx_timer;
extern void s5pc1xx_reserve_bootmem(void);

static struct s3c24xx_uart_clksrc universal_serial_clocks[] = {
#if defined(CONFIG_SERIAL_S5PC1XX_HSUART)
/* HS-UART Clock using SCLK */
        [0] = {
                .name           = "uclk1",
                .divisor        = 1,
                .min_baud       = 0,
                .max_baud       = 0,
        },
#else
        [0] = {
                .name           = "pclk",
                .divisor        = 1,
                .min_baud       = 0,
                .max_baud       = 0,
        },
#endif
};

static struct s3c2410_uartcfg universal_uartcfgs[] __initdata = {
	[0] = {
		.hwport	     = 0,
		.flags	     = 0,
		.ucon	     = S3C64XX_UCON_DEFAULT,
		.ulcon	     = S3C64XX_ULCON_DEFAULT,
		.ufcon	     = S3C64XX_UFCON_DEFAULT,
                .clocks      = universal_serial_clocks,
                .clocks_size = ARRAY_SIZE(universal_serial_clocks),
	},
	[1] = {
		.hwport	     = 1,
		.flags	     = 0,
		.ucon	     = S3C64XX_UCON_DEFAULT,
		.ulcon	     = S3C64XX_ULCON_DEFAULT,
		.ufcon	     = S3C64XX_UFCON_DEFAULT,
                .clocks      = universal_serial_clocks,
                .clocks_size = ARRAY_SIZE(universal_serial_clocks),
	},
        [2] = {
                .hwport      = 2,
                .flags       = 0,
		.ucon	     = S3C64XX_UCON_DEFAULT,
		.ulcon	     = S3C64XX_ULCON_DEFAULT,
		.ufcon	     = S3C64XX_UFCON_DEFAULT,
                .clocks      = universal_serial_clocks,
                .clocks_size = ARRAY_SIZE(universal_serial_clocks),
        },
        [3] = {
                .hwport      = 3,
                .flags       = 0,
		.ucon	     = S3C64XX_UCON_DEFAULT,
		.ulcon	     = S3C64XX_ULCON_DEFAULT,
		.ufcon	     = S3C64XX_UFCON_DEFAULT,
                .clocks      = universal_serial_clocks,
                .clocks_size = ARRAY_SIZE(universal_serial_clocks),
        },
};

/* PMIC */
static struct regulator_consumer_supply dcdc1_consumers[] = {
	{
		.supply		= "vddarm",
	},
};

static struct regulator_init_data max8698_dcdc1_data = {
	.constraints	= {
		.name		= "VCC_ARM",
		.min_uV		=  750000,
		.max_uV		= 1500000,
		.always_on	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE,
	},
	.num_consumer_supplies	= ARRAY_SIZE(dcdc1_consumers),
	.consumer_supplies	= dcdc1_consumers,
};

static struct regulator_init_data max8698_dcdc2_data = {
	.constraints	= {
		.name		= "VCC_INTERNAL",
		.min_uV		= 1200000,
		.max_uV		= 1200000,
		.apply_uV	= 1,
	},
};

static struct regulator_init_data max8698_dcdc3_data = {
	.constraints	= {
		.name		= "VCC_MEM",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.state_mem	= {
			.uV	= 1800000,
			.mode	= REGULATOR_MODE_NORMAL,
			.enabled = 1,
		},
		.initial_state	= PM_SUSPEND_MEM,
	},
};

static struct regulator_init_data max8698_ldo2_data = {
	.constraints	= {
		.name		= "VALIVE_1.2V",
		.min_uV		= 1200000,
		.max_uV		= 1200000,
		.apply_uV	= 1,
		.always_on	= 1,
	},
};

static struct regulator_init_data max8698_ldo3_data = {
	.constraints	= {
		.name		= "VUSB_1.2V/MIPI_1.2V",
		.min_uV		= 1200000,
		.max_uV		= 1200000,
		.apply_uV	= 1,
	},
};

static struct regulator_init_data max8698_ldo4_data = {
	.constraints	= {
		.name		= "VOPTIC_2.8V",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.boot_on	= 1,
	},
};

static struct regulator_consumer_supply universal_hsmmc1_supply = {
	.supply			= "hsmmc",	/* FIXME what's exact name? */
	.dev			= &s3c_device_hsmmc1.dev,
};

static struct regulator_init_data max8698_ldo5_data = {
	.constraints	= {
		.name		= "VTF_2.8V",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &universal_hsmmc1_supply,
};

static struct regulator_init_data max8698_ldo6_data = {
	.constraints	= {
		.name		= "VCC_2.6V",
		.min_uV		= 2600000,
		.max_uV		= 2600000,
		.apply_uV	= 1,
	},
};

static struct regulator_init_data max8698_ldo7_data = {
	.constraints	= {
		.name		= "VDAC_2.8V",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
	},
};

static struct regulator_init_data max8698_ldo8_data = {
	.constraints	= {
		.name		= "{VADC/VCAM/VUSB}_3.3V",
		.min_uV		= 3300000,
		.max_uV		= 3300000,
		.apply_uV	= 1,
	},
};

static struct regulator_init_data max8698_ldo9_data = {
	.constraints	= {
		.name		= "VCAM_2.8V",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
	},
};

static struct max8698_subdev_data universal_regulators[] = {
	{ MAX8698_LDO2, &max8698_ldo2_data },
	{ MAX8698_LDO3, &max8698_ldo3_data },
	{ MAX8698_LDO4, &max8698_ldo4_data },
	{ MAX8698_LDO5, &max8698_ldo5_data },
	{ MAX8698_LDO6, &max8698_ldo6_data },
	{ MAX8698_LDO7, &max8698_ldo7_data },
	{ MAX8698_LDO8, &max8698_ldo8_data },
	{ MAX8698_LDO9, &max8698_ldo9_data },
	{ MAX8698_DCDC1, &max8698_dcdc1_data },
	{ MAX8698_DCDC2, &max8698_dcdc2_data },
	{ MAX8698_DCDC3, &max8698_dcdc3_data },
};

static struct max8698_platform_data max8698_platform_data = {
	.num_regulators	= ARRAY_SIZE(universal_regulators),
	.regulators	= universal_regulators,
/*
	.set1		= S5PC1XX_GPJ0(6),
	.set2		= S5PC1XX_GPJ0(7),
	.set3		= S5PC1XX_GPJ1(0),
*/
};

/* I2C0 */
static struct i2c_board_info i2c_devs0[] __initdata = {
	{
		/* The address is 0xCC used since SRAD = 0 */
		I2C_BOARD_INFO("max8698", (0xCC >> 1)),
		.platform_data = &max8698_platform_data,
	},
};

/* I2C1 */
static struct i2c_board_info i2c_devs1[] __initdata = {
	{
		I2C_BOARD_INFO("max9877", 0x4d),
	},
	/* TODO
	 * KXSD9
	 */
};

/* GPIO I2C 2.6V */
#define I2C_GPIO_26V_BUS	2
static struct i2c_gpio_platform_data universal_i2c_gpio_26v_data = {
	.sda_pin	= S5PC1XX_GPJ3(6),
	.scl_pin	= S5PC1XX_GPJ3(7),
};

static struct platform_device universal_i2c_gpio_26v = {
	.name		= "i2c-gpio",
	.id		= I2C_GPIO_26V_BUS,
	.dev		= {
		.platform_data	= &universal_i2c_gpio_26v_data,
	},
};

static struct i2c_board_info i2c_gpio_26v_devs[] __initdata = {
	{
		I2C_BOARD_INFO("ak4671", 0x12),
	},
	/* TODO
	 * Proximity, Optical Sensor - GP2AP002 (0x44)
	 * Backlight Driver IC - BD6091GU (0x76)
	 */
};

/* GPIO I2C 2.6V - HDMI */
#define I2C_GPIO_HDMI_BUS	3
static struct i2c_gpio_platform_data universal_i2c_gpio_hdmi_data = {
	.sda_pin	= S5PC1XX_GPJ4(0),
	.scl_pin	= S5PC1XX_GPJ4(3),
};

static struct platform_device universal_i2c_gpio_hdmi = {
	.name		= "i2c-gpio",
	.id		= I2C_GPIO_HDMI_BUS,
	.dev		= {
		.platform_data	= &universal_i2c_gpio_hdmi_data,
	},
};

static struct i2c_board_info i2c_gpio_hdmi_devs[] __initdata = {
	/* TODO */
};

/* GPIO I2C 2.8V */
#define I2C_GPIO_28V_BUS	4
static struct i2c_gpio_platform_data universal_i2c_gpio_28v_data = {
	.sda_pin	= S5PC1XX_GPH2(4),
	.scl_pin	= S5PC1XX_GPH2(5),
};

static struct platform_device universal_i2c_gpio_28v = {
	.name		= "i2c-gpio",
	.id		= I2C_GPIO_28V_BUS,
	.dev		= {
		.platform_data	= &universal_i2c_gpio_28v_data,
	},
};

static struct i2c_board_info i2c_gpio_28v_devs[] __initdata = {
	/* TODO */
};

#define LCD_BUS_NUM 	3
#define DISPLAY_CS	S5PC1XX_GPK3(5)
static struct spi_board_info spi_board_info[] __initdata = {
    	{
	    	.modalias	= "tl2796",
		.platform_data	= NULL,
		.max_speed_hz	= 1200000,
		.bus_num	= LCD_BUS_NUM,
		.chip_select	= 0,
		.mode		= SPI_MODE_3,
		.controller_data = (void *)DISPLAY_CS,
	},
};

#define DISPLAY_CLK	S5PC1XX_GPK3(6)
#define DISPLAY_SI	S5PC1XX_GPK3(7)
static struct spi_gpio_platform_data tl2796_spi_gpio_data = {
	.sck	= DISPLAY_CLK,
	.mosi	= DISPLAY_SI,
	.miso	= -1,

	.num_chipselect	= 1,
};

static struct platform_device universal_spi_gpio = {
	.name	= "spi_gpio",
	.id	= LCD_BUS_NUM,
	.dev	= {
		.parent		= &s3c_device_fb.dev,
		.platform_data	= &tl2796_spi_gpio_data,
	},
};

static void tl2796_gpio_setup(void)
{
	gpio_request(S5PC1XX_GPH1(7), "MLCD_RST");
	gpio_request(S5PC1XX_GPJ1(3), "MLCD_ON");
}

/* Configure GPIO pins for RGB Interface */
static void rgb_cfg_gpio(struct platform_device *dev)
{
	int i;

	for (i = 0; i < 8; i++)
		s3c_gpio_cfgpin(S5PC1XX_GPF0(i), S3C_GPIO_SFN(2));

	for (i = 0; i < 8; i++)
		s3c_gpio_cfgpin(S5PC1XX_GPF1(i), S3C_GPIO_SFN(2));

	for (i = 0; i < 8; i++)
		s3c_gpio_cfgpin(S5PC1XX_GPF2(i), S3C_GPIO_SFN(2));

	for (i = 0; i < 4; i++)
		s3c_gpio_cfgpin(S5PC1XX_GPF3(i), S3C_GPIO_SFN(2));
}

static int tl2796_power_on(struct platform_device *dev)
{
	/* set gpio data for MLCD_RST to HIGH */
	gpio_direction_output(S5PC1XX_GPH1(7), 1);
	/* set gpio data for MLCD_ON to HIGH */
	gpio_direction_output(S5PC1XX_GPJ1(3), 1);
	mdelay(25);

	/* set gpio data for MLCD_RST to LOW */
	gpio_direction_output(S5PC1XX_GPH1(7), 0);
	udelay(20);
	/* set gpio data for MLCD_RST to HIGH */
	gpio_direction_output(S5PC1XX_GPH1(7), 1);
	mdelay(20);

	return 0;
}

static int tl2796_reset(struct platform_device *dev)
{
	/* set gpio pin for MLCD_RST to LOW */
	gpio_direction_output(S5PC1XX_GPH1(7), 0);
	udelay(1);	/* Shorter than 5 usec */
	/* set gpio pin for MLCD_RST to HIGH */
	gpio_direction_output(S5PC1XX_GPH1(7), 1);
	mdelay(10);

	return 0;
}

static struct s3c_platform_fb fb_data __initdata = {
	.hw_ver = 0x50,
	.clk_name = "lcd",
	.nr_wins = 5,
	.default_win = CONFIG_FB_S3C_DEFAULT_WINDOW,
	.swap = FB_SWAP_WORD | FB_SWAP_HWORD,

	.cfg_gpio =  rgb_cfg_gpio,
	.backlight_on = tl2796_power_on,
	.reset_lcd = tl2796_reset,
};

struct map_desc universal_iodesc[] = {};

static struct platform_device *universal_devices[] __initdata = {
	&s3c_device_fb,
	&s3c_device_mfc,
	&s3c_device_rotator,
	&universal_spi_gpio,
	&s3c_device_i2c0,
	&s3c_device_i2c1,
	&s3c_device_fimc0,
	&s3c_device_fimc1,
	&s3c_device_fimc2,
	&s3c_device_csis,
	&universal_i2c_gpio_26v,
	&universal_i2c_gpio_hdmi,
	&universal_i2c_gpio_28v,
};

/* External camera module setting */
static struct s5k6aa_platform_data s5k6aa = {
	.default_width = 640,
	.default_height = 480,
	.pixelformat = V4L2_PIX_FMT_YUYV,
	.freq = 24000000,
	.is_mipi = 1,
};

static struct i2c_board_info  __initdata camera_info[] = {
	{
		I2C_BOARD_INFO("S5K6AA", 0x3C),
		.platform_data = &s5k6aa,
	},
};

static void __init universal_i2c_gpio_init(void)
{
	s3c_gpio_setpull(S5PC1XX_GPH2(4), S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(S5PC1XX_GPH2(5), S3C_GPIO_PULL_NONE);

	s3c_gpio_setpull(S5PC1XX_GPJ3(6), S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(S5PC1XX_GPJ3(7), S3C_GPIO_PULL_NONE);

	s3c_gpio_setpull(S5PC1XX_GPJ4(0), S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(S5PC1XX_GPJ4(3), S3C_GPIO_PULL_NONE);
}

static void __init universal_map_io(void)
{
	s5pc1xx_init_io(universal_iodesc, ARRAY_SIZE(universal_iodesc));
	s3c24xx_init_clocks(0);
#if defined(CONFIG_SERIAL_S5PC1XX_HSUART)
        writel((readl(S5P_CLK_DIV2) & ~(0xf << 0)), S5P_CLK_DIV2);
#endif
	s3c24xx_init_uarts(universal_uartcfgs, ARRAY_SIZE(universal_uartcfgs));
	s5pc1xx_reserve_bootmem();
}

/* define gpio as a name used on target */
#define CAM_EN		S5PC1XX_GPH1(4)
#define CAM1_3M_EN	S5PC1XX_GPC(1)
#define VGA_CAM_nSTBY	S5PC1XX_GPB(1)
#define VGA_CAM_nRST	S5PC1XX_GPB(0)

static int camera_pin_request(void)
{
	int err;

	/*
	 * GPIO configuration
	 * CAM_EN: GPH0[6]
	 * 1.3M_EN: GPK2[4]
	 * VGA_CAM_nSTBY: GPB[1]
	 * VGA_CAM_nRST: GPB[0]
	 */

	/* CAM_EN: GPH0[6] */
	err = gpio_request(CAM_EN, "CAM_EN");
	if(err == 0) {
		gpio_export(CAM_EN, 1);
		gpio_direction_output(CAM_EN, 0);
	} else {
		printk(KERN_INFO "CAM_EN request err\n");
		return err;
	}

	/* 1.3M_EN: GPK2[4] */
	err = gpio_request(CAM1_3M_EN, "1.3M_EN");
	if(err == 0) {
		gpio_export(CAM1_3M_EN, 1);
		gpio_direction_output(CAM1_3M_EN, 0);
	} else {
		printk(KERN_INFO "1.3M_EN request err\n");
		return err;
	}

	/* VGA_CAM_nSTBY: GPB[1] */
	err = gpio_request(VGA_CAM_nSTBY, "VGA_CAM_nSTBY");
	if(err == 0) {
		gpio_export(VGA_CAM_nSTBY, 1);
		gpio_direction_output(VGA_CAM_nSTBY, 0);
	} else {
		printk(KERN_INFO "VGA_CAM_nSTBY request err\n");
		return err;
	}

	/* VGA_CAM_nRST	GPB[0] */
	err = gpio_request(VGA_CAM_nRST, "VGA_CAM_nRST");
	if(err == 0) {
		gpio_export(VGA_CAM_nRST, 1);
		gpio_direction_output(VGA_CAM_nRST, 0);
	} else {
		printk(KERN_INFO "VGA_CAM_nRST request err\n");
		return err;
	}

	return err;
}

static void camera_cfg_gpio(struct platform_device *pdev)
{
	int i;

	s3c_gpio_cfgpin(CAM_EN, S3C_GPIO_OUTPUT);
	s3c_gpio_cfgpin(CAM1_3M_EN, S3C_GPIO_OUTPUT);
	s3c_gpio_cfgpin(VGA_CAM_nSTBY, S3C_GPIO_OUTPUT);
	s3c_gpio_cfgpin(VGA_CAM_nRST, S3C_GPIO_OUTPUT);

	/* Following pins are C100 dedicated */
	s3c_gpio_cfgpin(S5PC1XX_GPE0(0), S5PC1XX_GPE0_0_CAM_A_PCLK);
	s3c_gpio_cfgpin(S5PC1XX_GPE0(1), S5PC1XX_GPE0_1_CAM_A_VSYNC);
	s3c_gpio_cfgpin(S5PC1XX_GPE0(2), S5PC1XX_GPE0_2_CAM_A_HREF);
	s3c_gpio_cfgpin(S5PC1XX_GPE0(3), S5PC1XX_GPE0_3_CAM_A_DATA_0);
	s3c_gpio_cfgpin(S5PC1XX_GPE0(4), S5PC1XX_GPE0_4_CAM_A_DATA_1);
	s3c_gpio_cfgpin(S5PC1XX_GPE0(5), S5PC1XX_GPE0_5_CAM_A_DATA_2);
	s3c_gpio_cfgpin(S5PC1XX_GPE0(6), S5PC1XX_GPE0_6_CAM_A_DATA_3);
	s3c_gpio_cfgpin(S5PC1XX_GPE0(7), S5PC1XX_GPE0_7_CAM_A_DATA_4);
	s3c_gpio_cfgpin(S5PC1XX_GPE1(0), S5PC1XX_GPE1_0_CAM_A_DATA_5);
	s3c_gpio_cfgpin(S5PC1XX_GPE1(1), S5PC1XX_GPE1_1_CAM_A_DATA_6);
	s3c_gpio_cfgpin(S5PC1XX_GPE1(2), S5PC1XX_GPE1_2_CAM_A_DATA_7);
	s3c_gpio_cfgpin(S5PC1XX_GPE1(3), S5PC1XX_GPE1_3_CAM_A_CLKOUT);
	s3c_gpio_cfgpin(S5PC1XX_GPE1(4), S5PC1XX_GPE1_4_CAM_A_RESET);
	s3c_gpio_cfgpin(S5PC1XX_GPE1(5), S5PC1XX_GPE1_5_CAM_A_FIELD);

	for (i = 0; i < 8; i++)
		s3c_gpio_setpull(S5PC1XX_GPE0(i), S3C_GPIO_PULL_UP);

	for (i = 0; i < 6; i++)
		s3c_gpio_setpull(S5PC1XX_GPE1(i), S3C_GPIO_PULL_UP);

	s3c_gpio_cfgpin(S5PC1XX_GPH2(0), S5PC1XX_GPH2_0_CAM_B_DATA_0);
	s3c_gpio_cfgpin(S5PC1XX_GPH2(1), S5PC1XX_GPH2_1_CAM_B_DATA_1);
	s3c_gpio_cfgpin(S5PC1XX_GPH2(2), S5PC1XX_GPH2_2_CAM_B_DATA_2);
	s3c_gpio_cfgpin(S5PC1XX_GPH2(3), S5PC1XX_GPH2_3_CAM_B_DATA_3);
	s3c_gpio_cfgpin(S5PC1XX_GPH2(4), S5PC1XX_GPH2_4_CAM_B_DATA_4);
	s3c_gpio_cfgpin(S5PC1XX_GPH2(5), S5PC1XX_GPH2_5_CAM_B_DATA_5);
	s3c_gpio_cfgpin(S5PC1XX_GPH2(6), S5PC1XX_GPH2_6_CAM_B_DATA_6);
	s3c_gpio_cfgpin(S5PC1XX_GPH2(7), S5PC1XX_GPH2_7_CAM_B_DATA_7);
	s3c_gpio_cfgpin(S5PC1XX_GPH3(0), S5PC1XX_GPH3_0_CAM_B_PCLK);
	s3c_gpio_cfgpin(S5PC1XX_GPH3(1), S5PC1XX_GPH3_1_CAM_B_VSYNC);
	s3c_gpio_cfgpin(S5PC1XX_GPH3(2), S5PC1XX_GPH3_2_CAM_B_HREF);
	s3c_gpio_cfgpin(S5PC1XX_GPH3(3), S5PC1XX_GPH3_3_CAM_B_FIELD);
	s3c_gpio_cfgpin(S5PC1XX_GPE1(3), S5PC1XX_GPE1_3_CAM_A_CLKOUT);

	for (i = 0; i < 8; i++)
		s3c_gpio_setpull(S5PC1XX_GPH2(i), S3C_GPIO_PULL_UP);

	for (i = 0; i < 4; i++)
		s3c_gpio_setpull(S5PC1XX_GPH3(i), S3C_GPIO_PULL_UP);
}

static int camera_power(int onoff)
{
	int err;

	if (onoff > 1)
		return -EINVAL;
	if (onoff < 0)
		return -EINVAL;

	if (onoff == 0) {
		printk(KERN_INFO "%s: off[%d]\n", __FUNCTION__, onoff);
		/* OFF */
		gpio_direction_output(CAM_EN, 0);
		gpio_direction_output(CAM1_3M_EN, 0);
		gpio_direction_output(VGA_CAM_nSTBY, 0);
		gpio_direction_output(VGA_CAM_nRST, 0);
	} else {
		printk(KERN_INFO "%s: on[%d]\n", __FUNCTION__, onoff);
		gpio_direction_output(VGA_CAM_nRST, 0);
		/* ON */
		gpio_direction_output(VGA_CAM_nSTBY, 1);
		gpio_direction_output(CAM_EN, 1);
		gpio_direction_output(CAM1_3M_EN, 1);
		msleep(50);
		gpio_direction_output(VGA_CAM_nRST, 1);
		err = 0;
	}
	
	return err;
}

/* Camera interface setting */
static struct s3c_fimc_camera camera_c = {
	.id		= CAMERA_CSI_C,		/* FIXME */
	.type		= CAM_TYPE_MIPI,	/* 1.3M MIPI */
	.i2c_busnum	= 1,
	.info		= &camera_info[0],
	.pixelformat	= V4L2_PIX_FMT_YUYV,
	.mode		= MIPI_CSI_YCBCR422_8BIT,
	.order422	= CAM_ORDER422_8BIT_CBYCRY,
	.clockrate	= 24000000,		/* 24MHz */
	.line_length	= 1280,			/* 1280*1024 */
	/* default resol for preview kind of thing */
	.width		= 640,
	.height		= 480,
	.offset		= {	/* FIXME: scaler */
		.h1	= 0,
		.h2	= 0,
		.v1	= 0,
		.v2	= 0,
	},

	/* Polarity */
	.polarity	= {
		.pclk	= 0,
		.vsync 	= 0,
		.href	= 0,
		.hsync	= 0,
	},

	.initialized = 0,
	.cam_power = camera_power,
};

/* Interface setting */
static struct s3c_platform_fimc fimc_setting = {
	.srclk_name	= "dout_mpll",
	.clk_name	= "sclk_fimc",
	.clockrate	= 133000000,
	.nr_frames	= 4,
	.shared_io	= 0,

	.default_cam	= CAMERA_CSI_C,
	.cfg_gpio	= camera_cfg_gpio,
};

static void __init universal_machine_init(void)
{
	int err;

	tl2796_gpio_setup();

	universal_i2c_gpio_init();

	s3c_i2c0_set_platdata(NULL);
	s3c_i2c1_set_platdata(NULL);
	i2c_register_board_info(0, i2c_devs0, ARRAY_SIZE(i2c_devs0));
	i2c_register_board_info(1, i2c_devs1, ARRAY_SIZE(i2c_devs1));
	i2c_register_board_info(I2C_GPIO_26V_BUS, i2c_gpio_26v_devs,
				ARRAY_SIZE(i2c_gpio_26v_devs));
	i2c_register_board_info(I2C_GPIO_HDMI_BUS, i2c_gpio_hdmi_devs,
				ARRAY_SIZE(i2c_gpio_hdmi_devs));
	i2c_register_board_info(I2C_GPIO_28V_BUS, i2c_gpio_28v_devs,
				ARRAY_SIZE(i2c_gpio_28v_devs));

	spi_register_board_info(spi_board_info, ARRAY_SIZE(spi_board_info));
	s3cfb_set_platdata(&fb_data);

	err = camera_pin_request();
	if (err)
		printk(KERN_INFO "CAMERA PIN REQUEST FAILED\n");

	/* Registering external camera to platform data */
	/*fimc_setting.camera[0] = &camera_a;*/
	fimc_setting.camera[0] = &camera_c;

	s3c_fimc0_set_platdata(&fimc_setting);
	s3c_fimc1_set_platdata(&fimc_setting);
	s3c_fimc2_set_platdata(&fimc_setting);	/* testing MIPI */
	
	/* CSI-2 */
	s3c_csis_set_platdata(NULL);

	s3c_fimc_reset_camera();
	platform_add_devices(universal_devices, ARRAY_SIZE(universal_devices));
}

MACHINE_START(UNIVERSAL, "UNIVERSAL")
	/* Maintainer: InKi Dae <inki.dae@samsung.com> */
	.phys_io	= S3C_PA_UART & 0xfff00000,
	.io_pg_offst	= (((u32)S3C_VA_UART) >> 18) & 0xfffc,
	.boot_params	= S5PC1XX_PA_SDRAM + 0x100,

	.init_irq	= s5pc100_init_irq,
	.map_io		= universal_map_io,
	.init_machine	= universal_machine_init,
	.timer		= &s5pc1xx_timer,
MACHINE_END
