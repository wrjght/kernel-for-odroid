/* linux/arch/arm/mach-s3c6410/mach-smdk6410.c
 *
 * Copyright 2008 Openmoko, Inc.
 * Copyright 2008 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *	http://armlinux.simtec.co.uk/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/serial_core.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/i2c.h>
#include <linux/fb.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <video/platform_lcd.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/videodev2.h>
#include <media/s5k3ba_platform.h>
#include <media/s5k4ba_platform.h>
#include <media/s5k4ea_platform.h>
#include <media/s5k6aa_platform.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <mach/hardware.h>
#include <mach/regs-fb.h>
#include <mach/map.h>
#include <mach/regs-mem.h>

#include <asm/irq.h>
#include <asm/mach-types.h>

#include <plat/regs-serial.h>
#include <plat/iic.h>
#include <plat/fimc.h>
#include <plat/fb.h>
#include <plat/regs-rtc.h>

#include <plat/nand.h>
#include <plat/partition.h>
#include <plat/s3c6410.h>
#include <plat/clock.h>
#include <plat/regs-clock.h>
#include <plat/devs.h>
#include <plat/cpu.h>
#if defined CONFIG_TOUCHSCREEN_S3C
#include <plat/ts.h>
#include <plat/adc.h>
#endif
#include <plat/reserved_mem.h>
#include <plat/pm.h>
#include <plat/regs-fimc.h>
#include <linux/android_pmem.h>

#include <plat/gpio-cfg.h>
#ifdef CONFIG_USB_SUPPORT
#include <plat/regs-otg.h>
#include <linux/usb/ch9.h>

/* S3C_USB_CLKSRC 0: EPLL 1: CLK_48M */
#define S3C_USB_CLKSRC	1

#ifdef USB_HOST_PORT2_EN
#define OTGH_PHY_CLK_VALUE      (0x60)  /* Serial Interface, otg_phy input clk 48Mhz Oscillator */
#else
#define OTGH_PHY_CLK_VALUE      (0x20)  /* UTMI Interface, otg_phy input clk 48Mhz Oscillator */
#endif
#endif

#define UCON S3C2410_UCON_DEFAULT | S3C2410_UCON_UCLK
#define ULCON S3C2410_LCON_CS8 | S3C2410_LCON_PNONE | S3C2410_LCON_STOPB
#define UFCON S3C2410_UFCON_RXTRIG8 | S3C2410_UFCON_FIFOMODE

extern struct sys_timer s3c_timer;
extern void s3c64xx_reserve_bootmem(void);
static struct s3c2410_uartcfg smdk6410_uartcfgs[] __initdata = {
	[0] = {
		.hwport	     = 0,
		.flags	     = 0,
		.ucon	     = 0x3c5,
		.ulcon	     = 0x03,
		.ufcon	     = 0x51,
	},
	[1] = {
		.hwport	     = 1,
		.flags	     = 0,
		.ucon	     = 0x3c5,
		.ulcon	     = 0x03,
		.ufcon	     = 0x51,
	},
};

#if 0
/* framebuffer and LCD setup. */

/* GPF15 = LCD backlight control
 * GPF13 => Panel power
 * GPN5 = LCD nRESET signal
 * PWM_TOUT1 => backlight brightness
 */

static void smdk6410_lcd_power_set(struct plat_lcd_data *pd,
				   unsigned int power)
{
	if (power) {
		gpio_direction_output(S3C64XX_GPF(13), 1);
		gpio_direction_output(S3C64XX_GPF(15), 1);

		/* fire nRESET on power up */
		gpio_direction_output(S3C64XX_GPN(5), 0);
		msleep(10);
		gpio_direction_output(S3C64XX_GPN(5), 1);
		msleep(1);
	} else {
		gpio_direction_output(S3C64XX_GPF(15), 0);
		gpio_direction_output(S3C64XX_GPF(13), 0);
	}
}

static struct plat_lcd_data smdk6410_lcd_power_data = {
	.set_power	= smdk6410_lcd_power_set,
};

static struct platform_device smdk6410_lcd_powerdev = {
	.name			= "platform-lcd",
	.dev.parent		= &s3c_device_fb.dev,
	.dev.platform_data	= &smdk6410_lcd_power_data,
};
#endif

static struct map_desc smdk6410_iodesc[] = {};

struct platform_device sec_device_backlight = {
	.name	= "smdk-backlight",
	.id		= -1,
};

struct platform_device sec_device_battery = {
	.name	= "smdk6410-battery",
	.id		= -1,
};

static struct s3c6410_pmem_setting pmem_setting = {
 	.pmem_start = RESERVED_PMEM_START,
	.pmem_size = RESERVED_PMEM,
	.pmem_gpu1_start = GPU1_RESERVED_PMEM_START,
	.pmem_gpu1_size = RESERVED_PMEM_GPU1,
	.pmem_render_start = RENDER_RESERVED_PMEM_START,
	.pmem_render_size = RESERVED_PMEM_RENDER,
	.pmem_stream_start = STREAM_RESERVED_PMEM_START,
	.pmem_stream_size = RESERVED_PMEM_STREAM,
	.pmem_preview_start = PREVIEW_RESERVED_PMEM_START,
	.pmem_preview_size = RESERVED_PMEM_PREVIEW,
	.pmem_picture_start = PICTURE_RESERVED_PMEM_START,
	.pmem_picture_size = RESERVED_PMEM_PICTURE,
	.pmem_jpeg_start = JPEG_RESERVED_PMEM_START,
	.pmem_jpeg_size = RESERVED_PMEM_JPEG,
	.pmem_skia_start = SKIA_RESERVED_PMEM_START,
	.pmem_skia_size = RESERVED_PMEM_SKIA,
};

static struct platform_device *smdk6410_devices[] __initdata = {
	&s3c_device_fb,
#ifdef CONFIG_SMDK6410_SD_CH0
	&s3c_device_hsmmc0,
#endif
#ifdef CONFIG_SMDK6410_SD_CH1
	&s3c_device_hsmmc1,
#endif
#ifdef CONFIG_SMDK6410_SD_CH2
	&s3c_device_hsmmc2,
#endif
	&s3c_device_usb,
	&s3c_device_usbgadget,
	&s3c_device_usb_otghcd,
	&s3c_device_wdt,
	&s3c_device_rtc,
	&s3c_device_i2c0,
	&s3c_device_i2c1,
#if defined CONFIG_TOUCHSCREEN_S3C
	&s3c_device_ts,
#endif
	&s3c_device_lcd,
#if defined CONFIG_SMC911X
	&s3c_device_smc911x,
#endif
	&s3c_device_nand,
#ifdef CONFIG_S3C_ADC
	&s3c_device_adc,
#endif
	&s3c_device_g2d,
	&s3c_device_fimc0,
	&s3c_device_keypad,
	&sec_device_backlight,
	&sec_device_battery,
	&s3c_device_mfc,
	&s3c_device_jpeg,
	&s3c_device_vpp,
};

static struct i2c_board_info i2c_devs0[] __initdata = {
	{ I2C_BOARD_INFO("24c08", 0x50), },
/*	{ I2C_BOARD_INFO("WM8580", 0x1b), },	*/
};

static struct i2c_board_info i2c_devs1[] __initdata = {
	{ I2C_BOARD_INFO("24c128", 0x57), },	/* Samsung S524AD0XD1 */
	{ I2C_BOARD_INFO("WM8580", 0x1b), },
};

#if defined CONFIG_TOUCHSCREEN_S3C
static struct s3c_ts_mach_info s3c_ts_platform __initdata = {
	.delay 			= 10000,
	.presc 			= 49,
	.oversampling_shift	= 2,
	.resol_bit 		= 12,
	.s3c_adc_con		= ADC_TYPE_2,
};
#endif
/*
 * Guide for Camera Configuration for SMDKC110
 * ITU channel must be set as A or B
 * ITU CAM CH A: S5K3BA only
 * ITU CAM CH B: one of S5K3BA and S5K4BA
 * MIPI: one of S5K4EA and S5K6AA
 *
 * NOTE1: if the S5K4EA is enabled, all other cameras must be disabled
 * NOTE2: currently, only 1 MIPI camera must be enabled
 * NOTE3: it is possible to use both one ITU cam and one MIPI cam except for S5K4EA case
 * 
*/
//#undef CAM_ITU_CH_A
#define CAM_ITU_CH_A

//#undef S5K3BA_ENABLED
//#undef S5K4BA_ENABLED
#define S5K4BA_ENABLED
//#define S5K4EA_ENABLED
//#undef S5K4EA_ENABLED
//#undef S5K6AA_ENABLED
//#define WRITEBACK_ENABLED

/*
 * External camera reset
 * Because the most of cameras take i2c bus signal, so that
 * you have to reset at the boot time for other i2c slave devices.
 * This function also called at fimc_init_camera()
 * Do optimization for cameras on your platform.
*/
/*
 * External camera reset
 * Because the most of cameras take i2c bus signal, so that
 * you have to reset at the boot time for other i2c slave devices.
 * Do optimization for cameras on your platform.
*/
static void smdk6410_reset_camera(void)
{
	void __iomem *regs = ioremap(S3C64XX_PA_FIMC, SZ_4K);
	u32 cfg;
	struct platform_device *pdev_dummy = NULL;

	s3c_fimc0_cfg_gpio(pdev_dummy);

	/* based on s5k4ba at the channel A */
	/* low reset */
	cfg = readl(regs + S3C_CIGCTRL);
	cfg &= ~S3C_CIGCTRL_CAMRST_A;
	writel(cfg, regs + S3C_CIGCTRL);
	udelay(200);

	cfg = readl(regs + S3C_CIGCTRL);
	cfg |= S3C_CIGCTRL_CAMRST_A;
	writel(cfg, regs + S3C_CIGCTRL);
	udelay(2000);

	iounmap(regs);
}

#ifdef CAM_ITU_CH_A
static int smdk6410_cam0_power(int onoff)
{
	/* Camera A */
	return 0;
}
#else
static int smdk6410_cam1_power(int onoff)
{
	/* Camera B */
	return 0;
}
#endif /* CAM_ITU_CH_A */

#if defined (S5K6AA_ENABLED) || defined (S5K4EA_ENABLED)
static int smdk6410_mipi_cam_power(int onoff)
{
	return 0;
}
#endif
/* External camera module setting */
/* 2 ITU Cameras */
#ifdef S5K3BA_ENABLED
static struct s5k3ba_platform_data s5k3ba_plat = {
	.default_width = 640,
	.default_height = 480,
	.pixelformat = V4L2_PIX_FMT_VYUY,
	.freq = 24000000,
	.is_mipi = 0,
};

static struct i2c_board_info __initdata s5k3ba_i2c_info = {
	I2C_BOARD_INFO("S5K3BA", 0x2d),
	.platform_data = &s5k3ba_plat,
};

static struct s3c_platform_camera __initdata s5k3ba = {
#ifdef CAM_ITU_CH_A
	.id		= CAMERA_PAR_A,
#else
	.id		= CAMERA_PAR_B,
#endif
	.type		= CAM_TYPE_ITU,
	.fmt		= ITU_601_YCBCR422_8BIT,
	.order422	= CAM_ORDER422_8BIT_CRYCBY,
	.i2c_busnum	= 0,
	.info		= &s5k3ba_i2c_info,
	.pixelformat	= V4L2_PIX_FMT_VYUY,
	.srclk_name	= "mout_epll",
#ifdef CAM_ITU_CH_A
	.clk_name	= "sclk_cam",
#else
	.clk_name	= "sclk_cam",
#endif
	.clk_rate	= 24000000,
	.line_length	= 1920,
	.width		= 640,
	.height		= 480,
	.window		= {
		.left	= 0,
		.top	= 0,
		.width	= 640,
		.height	= 480,
	},

	/* Polarity */
	.inv_pclk	= 0,
	.inv_vsync 	= 1,
	.inv_href	= 0,
	.inv_hsync	= 0,

	.initialized 	= 0,
#ifdef CAM_ITU_CH_A
	.cam_power	= smdk6410_cam0_power,
#else
	.cam_power	= smdk6410_cam1_power,
#endif
};
#endif

#ifdef S5K4BA_ENABLED
static struct s5k4ba_platform_data s5k4ba_plat = {
	.default_width = 800,
	.default_height = 600,
	.pixelformat = V4L2_PIX_FMT_YUYV,
	.freq = 44000000,
	.is_mipi = 0,
};

static struct i2c_board_info s5k4ba_i2c_info = {
	I2C_BOARD_INFO("S5K4BA", 0x2d),
	.platform_data = &s5k4ba_plat,
};

static struct s3c_platform_camera s5k4ba = {
	.id		= CAMERA_PAR_A,
	.type		= CAM_TYPE_ITU,
	.fmt		= ITU_601_YCBCR422_8BIT,
	.order422	= CAM_ORDER422_8BIT_YCBYCR,
	.i2c_busnum	= 0,
	.info		= &s5k4ba_i2c_info,
	.pixelformat	= V4L2_PIX_FMT_YUYV,
	.srclk_name	= "mout_mpll",
	.clk_name	= "sclk_cam",

	.clk_rate	= 44000000,
	.line_length	= 1920,
	.width		= 800,
	.height		= 600,
	.window		= {
		.left	= 0,
		.top	= 0,
		.width	= 800,
		.height	= 600,
	},

	/* Polarity */
	.inv_pclk	= 0,
	.inv_vsync 	= 1,
	.inv_href	= 0,
	.inv_hsync	= 0,

	.initialized 	= 0,
#ifdef CAM_ITU_CH_A
	.cam_power	= smdk6410_cam0_power,
#else
	.cam_power	= smdk6410_cam1_power,
#endif
};
#endif

/* 2 MIPI Cameras */
#ifdef S5K4EA_ENABLED
static struct s5k4ea_platform_data s5k4ea_plat = {
	.default_width = 1920,
	.default_height = 1080,
	.pixelformat = V4L2_PIX_FMT_UYVY,
	.freq = 24000000,
	.is_mipi = 1,
};

static struct i2c_board_info  __initdata s5k4ea_i2c_info = {
	I2C_BOARD_INFO("S5K4EA", 0x2d),
	.platform_data = &s5k4ea_plat,
};

static struct s3c_platform_camera __initdata s5k4ea = {
	.id		= CAMERA_CSI_C,
	.type		= CAM_TYPE_MIPI,
	.fmt		= MIPI_CSI_YCBCR422_8BIT,
	.order422	= CAM_ORDER422_8BIT_CBYCRY,
	.i2c_busnum	= 0,
	.info		= &s5k4ea_i2c_info,
	.pixelformat	= V4L2_PIX_FMT_UYVY,
	.srclk_name	= "mout_epll",
	.clk_name	= "sclk_cam",
	.clk_rate	= 24000000,
	.line_length	= 1920,
	.width		= 1920,
	.height		= 1080,
	.window		= {
		.left	= 0,
		.top	= 0,
		.width	= 1920,
		.height	= 1080,
	},

	.mipi_lanes	= 2,
	.mipi_settle	= 12,
	.mipi_align	= 32,

	/* Polarity */
	.inv_pclk	= 0,
	.inv_vsync 	= 1,
	.inv_href	= 0,
	.inv_hsync	= 0,

	.initialized 	= 0,
	.cam_power	= smdk6410_mipi_cam_power,
};
#endif

#ifdef S5K6AA_ENABLED
static struct s5k6aa_platform_data s5k6aa_plat = {
	.default_width = 640,
	.default_height = 480,
	.pixelformat = V4L2_PIX_FMT_UYVY,
	.freq = 24000000,
	.is_mipi = 1,
};

static struct i2c_board_info  __initdata s5k6aa_i2c_info = {
	I2C_BOARD_INFO("S5K6AA", 0x3c),
	.platform_data = &s5k6aa_plat,
};

static struct s3c_platform_camera __initdata s5k6aa = {
	.id		= CAMERA_CSI_C,
	.type		= CAM_TYPE_MIPI,
	.fmt		= MIPI_CSI_YCBCR422_8BIT,
	.order422	= CAM_ORDER422_8BIT_CBYCRY,
	.i2c_busnum	= 0,
	.info		= &s5k6aa_i2c_info,
	.pixelformat	= V4L2_PIX_FMT_UYVY,
	.srclk_name	= "mout_epll",
	.clk_name	= "sclk_cam",
	.clk_rate	= 24000000,
	.line_length	= 1920,
	/* default resol for preview kind of thing */
	.width		= 640,
	.height		= 480,
	.window		= {
		.left	= 0,
		.top	= 0,
		.width	= 640,
		.height	= 480,
	},

	.mipi_lanes	= 1,
	.mipi_settle	= 6,
	.mipi_align	= 32,

	/* Polarity */
	.inv_pclk	= 0,
	.inv_vsync 	= 1,
	.inv_href	= 0,
	.inv_hsync	= 0,

	.initialized 	= 0,
	.cam_power	= smdk6410_mipi_cam_power,
};
#endif


#ifdef CONFIG_S3C_ADC
static struct s3c_adc_mach_info s3c_adc_platform __initdata= {
	/* Support 12-bit resolution */
	.delay	= 	0xff,
	.presc 	= 	49,
	.resolution = 	12,
};
#endif

#ifdef WRITEBACK_ENABLED
static struct i2c_board_info  __initdata writeback_i2c_info = {
	I2C_BOARD_INFO("WriteBack", 0x0),
};

static struct s3c_platform_camera __initdata writeback = {
	.id		= CAMERA_WB,
	.fmt		= ITU_601_YCBCR422_8BIT,
	.order422	= CAM_ORDER422_8BIT_CBYCRY,
	.i2c_busnum	= 0,
	.info		= &writeback_i2c_info,
//	.pixelformat	= V4L2_PIX_FMT_UYVY,	/* not sure */
	.pixelformat	= V4L2_PIX_FMT_YUV444,
	.line_length	= 800,
	.width		= 480,
	.height		= 800,
	.window		= {
		.left	= 0,
		.top	= 0,
		.width	= 480,
		.height	= 800,
	},

	.initialized 	= 0,
};

#endif

/* Interface setting */
static struct s3c_platform_fimc fimc_plat = {
#if defined(S5K4EA_ENABLED) || defined(S5K6AA_ENABLED)
	.default_cam	= CAMERA_CSI_C,
#else

#ifdef WRITEBACK_ENABLED
	.default_cam 	= CAMERA_WB,
#elif defined (CAM_ITU_CH_A)
	.default_cam	= CAMERA_PAR_A,
#else
	.default_cam	= CAMERA_PAR_B,
#endif

#endif
	.camera		= {
#ifdef S5K3BA_ENABLED
		&s5k3ba,
#endif
#ifdef S5K4BA_ENABLED
		&s5k4ba,
#endif
#ifdef S5K4EA_ENABLED
		&s5k4ea,
#endif
#ifdef S5K6AA_ENABLED
		&s5k6aa,
#endif
#ifdef WRITEBACK_ENABLED
		&writeback,
#endif
	},
	.hw_ver		= 0x43,
};

static void __init smdk6410_map_io(void)
{
	s3c_device_nand.name = "s3c6410-nand";

	s3c64xx_init_io(smdk6410_iodesc, ARRAY_SIZE(smdk6410_iodesc));
	s3c24xx_init_clocks(12000000);
	s3c24xx_init_uarts(smdk6410_uartcfgs, ARRAY_SIZE(smdk6410_uartcfgs));
	s3c64xx_reserve_bootmem();
}

static void __init smdk6410_smc911x_set(void)
{
	unsigned int tmp;

	tmp = __raw_readl(S3C64XX_SROM_BW);
	tmp &= ~(S3C64XX_SROM_BW_WAIT_ENABLE1_MASK | S3C64XX_SROM_BW_WAIT_ENABLE1_MASK |
		S3C64XX_SROM_BW_DATA_WIDTH1_MASK);
	tmp |= S3C64XX_SROM_BW_BYTE_ENABLE1_ENABLE | S3C64XX_SROM_BW_WAIT_ENABLE1_ENABLE |
		S3C64XX_SROM_BW_DATA_WIDTH1_16BIT;

	__raw_writel(tmp, S3C64XX_SROM_BW);

	__raw_writel(S3C64XX_SROM_BCn_TACS(0) | S3C64XX_SROM_BCn_TCOS(4) |
			S3C64XX_SROM_BCn_TACC(13) | S3C64XX_SROM_BCn_TCOH(1) |
			S3C64XX_SROM_BCn_TCAH(4) | S3C64XX_SROM_BCn_TACP(6) |
			S3C64XX_SROM_BCn_PMC_NORMAL, S3C64XX_SROM_BC1);
}

static void __init smdk6410_machine_init(void)
{
	s3c_device_nand.dev.platform_data = &s3c_nand_mtd_part_info;
	smdk6410_smc911x_set();

	s3cfb_set_platdata(NULL);

	s3c_i2c0_set_platdata(NULL);
	s3c_i2c1_set_platdata(NULL);

#if defined CONFIG_TOUCHSCREEN_S3C
	s3c_ts_set_platdata(&s3c_ts_platform);
#endif
#ifdef CONFIG_S3C_ADC
	s3c_adc_set_platdata(&s3c_adc_platform);
#endif
	i2c_register_board_info(0, i2c_devs0, ARRAY_SIZE(i2c_devs0));
	i2c_register_board_info(1, i2c_devs1, ARRAY_SIZE(i2c_devs1));

	/* fimc */
	s3c_fimc0_set_platdata(&fimc_plat);
	smdk6410_reset_camera();

	platform_add_devices(smdk6410_devices, ARRAY_SIZE(smdk6410_devices));
	s3c6410_add_mem_devices (&pmem_setting);
	s3c6410_pm_init();
}

MACHINE_START(SMDK6410, "SMDK6410")
	/* Maintainer: Ben Dooks <ben@fluff.org> */
	.phys_io	= S3C_PA_UART & 0xfff00000,
	.io_pg_offst	= (((u32)S3C_VA_UART) >> 18) & 0xfffc,
	.boot_params	= S3C64XX_PA_SDRAM + 0x100,

	.init_irq	= s3c6410_init_irq,
	.map_io		= smdk6410_map_io,
	.init_machine	= smdk6410_machine_init,
	.timer		= &s3c64xx_timer,
MACHINE_END

#ifdef CONFIG_USB_SUPPORT
/* Initializes OTG Phy. */
void otg_phy_init(void) {

	writel(readl(S3C_OTHERS)|S3C_OTHERS_USB_SIG_MASK, S3C_OTHERS);
	writel(0x0, S3C_USBOTG_PHYPWR);		/* Power up */
        writel(OTGH_PHY_CLK_VALUE, S3C_USBOTG_PHYCLK);
	writel(0x1, S3C_USBOTG_RSTCON);

	udelay(50);
	writel(0x0, S3C_USBOTG_RSTCON);
	udelay(50);
}
EXPORT_SYMBOL(otg_phy_init);

/* USB Control request data struct must be located here for DMA transfer */
struct usb_ctrlrequest usb_ctrl __attribute__((aligned(8)));
EXPORT_SYMBOL(usb_ctrl);

/* OTG PHY Power Off */
void otg_phy_off(void) {
	writel(readl(S3C_USBOTG_PHYPWR)|(0x1F<<1), S3C_USBOTG_PHYPWR);
	writel(readl(S3C_OTHERS)&~S3C_OTHERS_USB_SIG_MASK, S3C_OTHERS);
}
EXPORT_SYMBOL(otg_phy_off);

void usb_host_clk_en(void) {
	struct clk *otg_clk;

        switch (S3C_USB_CLKSRC) {
	case 0: /* epll clk */
		writel((readl(S3C_CLK_SRC)& ~S3C6400_CLKSRC_UHOST_MASK)
			|S3C_CLKSRC_EPLL_CLKSEL|S3C_CLKSRC_UHOST_EPLL,
			S3C_CLK_SRC);

		/* USB host colock divider ratio is 2 */
		writel((readl(S3C_CLK_DIV1)& ~S3C6400_CLKDIV1_UHOST_MASK)
			|(1<<20), S3C_CLK_DIV1);
		break;
	case 1: /* oscillator 48M clk */
		otg_clk = clk_get(NULL, "otg");
		clk_enable(otg_clk);
		writel(readl(S3C_CLK_SRC)& ~S3C6400_CLKSRC_UHOST_MASK, S3C_CLK_SRC);
		otg_phy_init();

		/* USB host colock divider ratio is 1 */
		writel(readl(S3C_CLK_DIV1)& ~S3C6400_CLKDIV1_UHOST_MASK, S3C_CLK_DIV1);
		break;
	default:
		printk(KERN_INFO "Unknown USB Host Clock Source\n");
		BUG();
		break;
	}

	writel(readl(S3C_HCLK_GATE)|S3C_CLKCON_HCLK_UHOST|S3C_CLKCON_HCLK_SECUR,
		S3C_HCLK_GATE);
	writel(readl(S3C_SCLK_GATE)|S3C_CLKCON_SCLK_UHOST, S3C_SCLK_GATE);

}

EXPORT_SYMBOL(usb_host_clk_en);
#endif

#if defined(CONFIG_RTC_DRV_S3C)
/* RTC common Function for samsung APs*/
unsigned int s3c_rtc_set_bit_byte(void __iomem *base, uint offset, uint val)
{
	writeb(val, base + offset);

	return 0;
}

unsigned int s3c_rtc_read_alarm_status(void __iomem *base)
{
	return 1;
}

void s3c_rtc_set_pie(void __iomem *base, uint to)
{
	unsigned int tmp;

	tmp = readw(base + S3C2410_RTCCON) & ~S3C_RTCCON_TICEN;

        if (to)
                tmp |= S3C_RTCCON_TICEN;

        writew(tmp, base + S3C2410_RTCCON);
}

void s3c_rtc_set_freq_regs(void __iomem *base, uint freq, uint s3c_freq)
{
	unsigned int tmp;

        tmp = readw(base + S3C2410_RTCCON) & (S3C_RTCCON_TICEN | S3C2410_RTCCON_RTCEN );
        writew(tmp, base + S3C2410_RTCCON);
        s3c_freq = freq;
        tmp = (32768 / freq)-1;
        writel(tmp, base + S3C2410_TICNT);
}

void s3c_rtc_enable_set(struct platform_device *pdev,void __iomem *base, int en)
{
	unsigned int tmp;

	if (!en) {
		tmp = readw(base + S3C2410_RTCCON);
		writew(tmp & ~ (S3C2410_RTCCON_RTCEN | S3C_RTCCON_TICEN), base + S3C2410_RTCCON);
	} else {
		/* re-enable the device, and check it is ok */
		if ((readw(base+S3C2410_RTCCON) & S3C2410_RTCCON_RTCEN) == 0){
			dev_info(&pdev->dev, "rtc disabled, re-enabling\n");

			tmp = readw(base + S3C2410_RTCCON);
			writew(tmp|S3C2410_RTCCON_RTCEN, base+S3C2410_RTCCON);
		}

		if ((readw(base + S3C2410_RTCCON) & S3C2410_RTCCON_CNTSEL)){
			dev_info(&pdev->dev, "removing RTCCON_CNTSEL\n");

			tmp = readw(base + S3C2410_RTCCON);
			writew(tmp& ~S3C2410_RTCCON_CNTSEL, base+S3C2410_RTCCON);
		}

		if ((readw(base + S3C2410_RTCCON) & S3C2410_RTCCON_CLKRST)){
			dev_info(&pdev->dev, "removing RTCCON_CLKRST\n");

			tmp = readw(base + S3C2410_RTCCON);
			writew(tmp & ~S3C2410_RTCCON_CLKRST, base+S3C2410_RTCCON);
		}
	}
}
#endif

#if defined (CONFIG_KEYPAD_S3C) || defined (CONFIG_KEYPAD_S3C_MODULE)
void s3c_setup_keypad_cfg_gpio(void)
{
	int row_count, col_count;

#if defined(CONFIG_KEYPAD_PORT1)
	/* Set all the necessary GPK1 pins to special function (KP_ROWs) */
	for (row_count = 8; row_count < 16; row_count++) {
		s3c_gpio_cfgpin(S3C64XX_GPK(row_count), S3C_GPIO_SFN(3));
		s3c_gpio_setpull(S3C64XX_GPK(row_count), S3C_GPIO_PULL_UP);
	}
	
	/* Set all the necessary GPL pins to special function (KP_COLs) */
	for (col_count = 0; col_count < 8; col_count++) {
		s3c_gpio_cfgpin(S3C64XX_GPL(col_count), S3C_GPIO_SFN(3));
		s3c_gpio_setpull(S3C64XX_GPL(col_count), S3C_GPIO_PULL_NONE);
	}
#else
	/* Set all the necessary GPN pins to special function (KP_ROWs) */
	for (row_count = 0; row_count < 8; row_count++) {
		s3c_gpio_cfgpin(S3C64XX_GPN(row_count), S3C_GPIO_SFN(3));
		s3c_gpio_setpull(S3C64XX_GPN(row_count), S3C_GPIO_PULL_UP);
	}
	
	/* Set all the necessary GPH pins to special function (KP_COLs) */
	for (col_count = 0; col_count < 8; col_count++) {
		s3c_gpio_cfgpin(S3C64XX_GPH(col_count), S3C_GPIO_SFN(4));
		s3c_gpio_setpull(S3C64XX_GPH(col_count), S3C_GPIO_PULL_NONE);
	}
#endif
}

EXPORT_SYMBOL(s3c_setup_keypad_cfg_gpio);
#endif

#ifdef CONFIG_MMC_SDHCI_S3C
void s3c_setup_hsmmc_clock(void)
{
	struct clk *clk;

	clk = clk_get(NULL, "mmc_bus");
}
EXPORT_SYMBOL(s3c_setup_hsmmc_clock);
#endif

