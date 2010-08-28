/* linux/arch/arm/mach-s5pc100/mach-hkdkc100.c
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
#include <linux/delay.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/clk.h>
#include <linux/mm.h>
#include <linux/pwm_backlight.h>
#include <linux/spi/spi.h>
#include <linux/videodev2.h>
#include <media/s5k3ba_platform.h>
#include <media/s5k6aa_platform.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <mach/hardware.h>
#include <mach/map.h>
#include <mach/regs-mem.h>
#include <mach/gpio.h>

#include <asm/irq.h>
#include <asm/mach-types.h>

#include <plat/regs-serial.h>
#include <plat/regs-rtc.h>
#include <plat/iic.h>
#include <plat/fimc.h>
#include <plat/fb.h>
#include <plat/csis.h>

#include <plat/nand.h>
#include <plat/partition.h>
#include <plat/s5pc100.h>
#include <plat/clock.h>
#include <plat/devs.h>
#include <plat/cpu.h>
#include <plat/ts.h>
#include <plat/adc.h>
#include <plat/gpio-cfg.h>
#include <plat/regs-fimc.h>
#include <plat/regs-gpio.h>
#include <plat/gpio-bank-k0.h>
#include <plat/gpio-bank-a1.h>
#include <plat/gpio-bank-b.h>
#include <plat/gpio-bank-d.h>
#include <plat/gpio-bank-h3.h>
#include <plat/regs-clock.h>
#include <plat/s5p-spi.h>
#include <plat/spi.h>
#include <linux/bootmem.h>
#include <linux/android_pmem.h>
#include <plat/media.h>

#ifdef CONFIG_USB_SUPPORT
#include <plat/regs-otg.h>
#include <plat/pll.h>
#include <linux/usb/ch9.h>

extern struct platform_device s5p_device_tvout;

/* S3C_USB_CLKSRC 0: EPLL 1: CLK_48M */
#define S3C_USB_CLKSRC	1
#define OTGH_PHY_CLK_VALUE      (0x22)  /* UTMI Interface, otg_phy input clk 12Mhz Oscillator */
#endif

#if defined(CONFIG_PM)
#include <plat/pm.h>
#endif

#define UCON S3C2410_UCON_DEFAULT | S3C2410_UCON_UCLK
#define ULCON S3C2410_LCON_CS8 | S3C2410_LCON_PNONE | S3C2410_LCON_STOPB
#define UFCON S3C2410_UFCON_RXTRIG8 | S3C2410_UFCON_FIFOMODE

extern struct sys_timer s5pc1xx_timer;
extern void s5pc1xx_reserve_bootmem(void);

#if defined(CONFIG_MMC_SDHCI_S3C)
extern void s3c_sdhci_set_platdata(void);
#endif

static struct s3c24xx_uart_clksrc hkdkc100_serial_clocks[] = {
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

static struct s3c2410_uartcfg hkdkc100_uartcfgs[] __initdata = {
	[0] = {
		.hwport	     = 0,
		.flags	     = 0,
		.ucon	     = S3C64XX_UCON_DEFAULT,
		.ulcon	     = S3C64XX_ULCON_DEFAULT,
		.ufcon	     = S3C64XX_UFCON_DEFAULT,
                .clocks      = hkdkc100_serial_clocks,
                .clocks_size = ARRAY_SIZE(hkdkc100_serial_clocks),
	},
	[1] = {
		.hwport	     = 1,
		.flags	     = 0,
		.ucon	     = S3C64XX_UCON_DEFAULT,
		.ulcon	     = S3C64XX_ULCON_DEFAULT,
		.ufcon	     = S3C64XX_UFCON_DEFAULT,
                .clocks      = hkdkc100_serial_clocks,
                .clocks_size = ARRAY_SIZE(hkdkc100_serial_clocks),
	},

#if !defined(CONFIG_MACH_HKDKC100)
    [2] = {
            .hwport      = 2,
            .flags       = 0,
	.ucon	     = S3C64XX_UCON_DEFAULT,
	.ulcon	     = S3C64XX_ULCON_DEFAULT,
	.ufcon	     = S3C64XX_UFCON_DEFAULT,
            .clocks      = hkdkc100_serial_clocks,
            .clocks_size = ARRAY_SIZE(hkdkc100_serial_clocks),
    },
    [3] = {
            .hwport      = 3,
            .flags       = 0,
	.ucon	     = S3C64XX_UCON_DEFAULT,
	.ulcon	     = S3C64XX_ULCON_DEFAULT,
	.ufcon	     = S3C64XX_UFCON_DEFAULT,
            .clocks      = hkdkc100_serial_clocks,
            .clocks_size = ARRAY_SIZE(hkdkc100_serial_clocks),
    },
#endif
    
};

#ifdef CONFIG_ANDROID_PMEM
// sy82.yoon - pmem initialize (2009.05.18)
static struct android_pmem_platform_data pmem_pdata = {
   .name = "pmem",
	 .no_allocator = 1,
   .cached = 1,
   .start = 0, // will be set during proving pmem driver.
   .size = 0 // will be set during proving pmem driver.
};

static struct android_pmem_platform_data pmem_gpu1_pdata = {
   .name = "pmem_gpu1",
   .no_allocator = 1,
   .cached = 1,
   .buffered = 1,
   .start = 0,
   .size = 0,
};

static struct android_pmem_platform_data pmem_adsp_pdata = {
   .name = "pmem_adsp",
   .no_allocator = 1,
   .cached = 1,
   .buffered = 1,
   .start = 0,
   .size = 0,
};

static struct platform_device pmem_device = {
   .name = "android_pmem",
   .id = 0,
   .dev = { .platform_data = &pmem_pdata },
};
static struct platform_device pmem_gpu1_device = {
	.name = "android_pmem",
	.id = 1,
	.dev = { .platform_data = &pmem_gpu1_pdata },
};

static struct platform_device pmem_adsp_device = {
	.name = "android_pmem",
	.id = 2,
	.dev = { .platform_data = &pmem_adsp_pdata },
};
#endif

#if defined(CONFIG_RFKILL)
static struct platform_device hkdkc100_rfkill = {
    .name = "hkdkc100_rfkill",
    .id = -1,
};
#endif


struct map_desc hkdkc100_iodesc[] = {};

static struct platform_device *hkdkc100_devices[] __initdata = {
#ifdef CONFIG_FB_S3C
	&s3c_device_fb,
#endif
    &s3c_device_rtc,
	&s3c_device_i2c0,

	&s3c_device_usb,
	&s3c_device_usbgadget,

#ifdef CONFIG_S3C_DEV_HSMMC
	&s3c_device_hsmmc0,
#endif
#ifdef CONFIG_S3C_DEV_HSMMC1
	&s3c_device_hsmmc1,
#endif        
#ifdef CONFIG_S3C_DEV_HSMMC2     
	&s3c_device_hsmmc2,
#endif
	&s3c_device_mfc,
	&s3c_device_jpeg,
	&s3c_device_fimc0,
	&s3c_device_fimc1,
//	&s3c_device_fimc2,
	&s3c_device_wdt,
	&s3c_device_g3d,
	&s3c_device_g2d,

//	&s3c_device_ac97,
//#ifdef SND_S5P_HKDK_WM8991_I2S
#ifdef	CONFIG_SND_S5P_SOC_I2S
	&s5pc100_device_iis1,
#endif

#ifdef CONFIG_ANDROID_PMEM
    &pmem_device,
    &pmem_gpu1_device,
	&pmem_adsp_device,
#endif

	&s3c_device_rotator,
//	&s3c_device_csis,
#if defined(CONFIG_HAVE_PWM)
    &s3c_device_timer[0],
    &s3c_device_timer[1],
#endif
	&s5p_device_tvout,
#if defined(CONFIG_RFKILL)
	&hkdkc100_rfkill,
#endif	
};

static struct i2c_board_info i2c_devs0[] __initdata = {
	{ I2C_BOARD_INFO("max17040", 0x36), },
	{ I2C_BOARD_INFO("max8698c", 0x66), },
};

#ifdef CONFIG_SPI_S3C64XX
static void hkdk_mmcspi_cs_set_level(int high)
{
	u32 val;

	val = readl(S5PC1XX_GPBDAT);

	if (high)
		val |= (1<<3);
	else
		val &= ~(1<<3);

	writel(val, S5PC1XX_GPBDAT);
}

#define SMDK_MMCSPI_CS 0

static struct s3c64xx_spi_csinfo hkdk_spi0_csi[] = {
	[SMDK_MMCSPI_CS] = {
		.set_level = hkdk_mmcspi_cs_set_level,
		.fb_delay = 0x3,
	},
};

static struct spi_board_info s3c_spi_devs[] __initdata = {
	{
		.modalias	 = "mmc_spi", /* MMC SPI */
		.mode		 = SPI_MODE_0,	/* CPOL=0, CPHA=0 */
		.max_speed_hz    = 10000000,
		/* Connected to SPI-0 as 1st Slave */
		.bus_num	 = 0,
		.chip_select	 = 0,
		.controller_data = &hkdk_spi0_csi[SMDK_MMCSPI_CS],
	},
};
#endif

#if !defined(CONFIG_MACH_HKDKC100)
/* External camera module setting */
static struct s5k3ba_platform_data s5k3ba = {
	.default_width = 640,
	.default_height = 480,
	.pixelformat = V4L2_PIX_FMT_VYUY,
	.freq = 24000000,
	.is_mipi = 0,
};

static struct s5k6aa_platform_data s5k6aa = {
	.default_width = 640,
	.default_height = 480,
	.pixelformat = V4L2_PIX_FMT_YUYV,
	.freq = 24000000,
	.is_mipi = 1,
};

static struct i2c_board_info  __initdata camera_info[] = {
	{
		I2C_BOARD_INFO("S5K3BA", 0x2d),
		.platform_data = &s5k3ba,
	},
	{
		I2C_BOARD_INFO("S5K6AA", 0x3c),
		.platform_data = &s5k6aa,
	},
};

/* Camera interface setting */
static struct s3c_platform_camera __initdata camera_a = {
	.id		= CAMERA_PAR_A,		/* FIXME */
	.type		= CAM_TYPE_ITU,		/* 2.0M ITU */
	.fmt		= ITU_601_YCBCR422_8BIT,
	.order422	= CAM_ORDER422_8BIT_CRYCBY,
	.i2c_busnum	= 0,
	.info		= &camera_info[0],
	.pixelformat	= V4L2_PIX_FMT_VYUY,
	.srclk_name	= "dout_mpll",
	.clk_name	= "sclk_cam",
	.clk_rate	= 24000000,		/* 44MHz */
	.line_length	= 1280,			/* 1280*1024 */
	/* default resol for preview kind of thing */
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

	.initialized = 0,
};

static struct s3c_platform_camera __initdata camera_c = {
	.id		= CAMERA_CSI_C,		/* FIXME */
	.type		= CAM_TYPE_MIPI,	/* 1.3M MIPI */
	.fmt		= MIPI_CSI_YCBCR422_8BIT,
	.order422	= CAM_ORDER422_8BIT_CBYCRY,
	.i2c_busnum	= 0,
	.info		= &camera_info[1],
	.pixelformat	= V4L2_PIX_FMT_YUYV,
	.srclk_name	= "dout_mpll",
	.clk_name	= "sclk_cam",
	.clk_rate	= 24000000,		/* 24MHz */
	.line_length	= 1280,			/* 1280*1024 */
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

	.initialized = 0,
};

/* Interface setting */
static struct s3c_platform_fimc __initdata fimc_plat = {
	.default_cam	= CAMERA_PAR_A,
	.camera		= { 
		&camera_a, 
		&camera_c,
	},
	.hw_ver		= 0x40,
};

/*
 * External camera reset
 * Because the most of cameras take i2c bus signal, so that
 * you have to reset at the boot time for other i2c slave devices.
 * Do optimization for cameras on your platform.
*/
static void hkdkc100_reset_camera(void)
{
	void __iomem *regs = ioremap(S5PC1XX_PA_FIMC0, SZ_4K);
	u32 cfg;
	struct clk *clk = NULL;
	struct platform_device *pdev = &s3c_device_fimc0;

	clk = clk_get(&pdev->dev, "fimc");
	if (IS_ERR(clk)) {
		dev_err(&pdev->dev, "failed to get fimc clock on reset camera\n");
		goto err_clk;
	}

	clk_enable(clk);
	/* based on s5k4ba at the channel A */
#if 0
	/* high reset */
	cfg = readl(regs + S3C_CIGCTRL);
	cfg |= S3C_CIGCTRL_CAMRST_A;
	writel(cfg, regs + S3C_CIGCTRL);
	udelay(200);

	cfg = readl(regs + S3C_CIGCTRL);
	cfg &= ~S3C_CIGCTRL_CAMRST_A;
	writel(cfg, regs + S3C_CIGCTRL);
	udelay(2000);
#else
	/* low reset */
	cfg = readl(regs + S3C_CIGCTRL);
	cfg &= ~S3C_CIGCTRL_CAMRST_A;
	writel(cfg, regs + S3C_CIGCTRL);
	udelay(200);

	cfg = readl(regs + S3C_CIGCTRL);
	cfg |= S3C_CIGCTRL_CAMRST_A;
	writel(cfg, regs + S3C_CIGCTRL);
	udelay(2000);
#endif

#if 0
	/* channel B reset: should be done by following after ch A reset */
	cfg = readl(S5PC1XX_GPH3CON);
	cfg &= ~S5PC1XX_GPH3_CONMASK(6);
	cfg |= S5PC1XX_GPH3_OUTPUT(6);
	writel(cfg, S5PC1XX_GPH3CON);

	cfg = readl(S5PC1XX_GPH3DAT);
	cfg &= ~(0x1 << 6);
	writel(cfg, S5PC1XX_GPH3DAT);
	udelay(200);

	cfg |= (0x1 << 6);
	writel(cfg, S5PC1XX_GPH3DAT);
	udelay(2000);
#endif

	clk_disable(clk);
	clk_put(clk);

err_clk:
	iounmap(regs);

}
#endif

#if defined(CONFIG_HAVE_PWM)
static struct platform_pwm_backlight_data hkdk_backlight_data = {
        .pwm_id         = 0,
        .max_brightness = 255,
        .dft_brightness = 255,
        .pwm_period_ns  = 78770,
};

static struct platform_device hkdk_backlight_device = {
        .name           = "pwm-backlight",
        .dev            = {
                .parent = &s3c_device_timer[0].dev,
                .platform_data = &hkdk_backlight_data,
        },
};

static void __init hkdk_backlight_register(void)
{
        int ret = platform_device_register(&hkdk_backlight_device);
        if (ret)
                printk(KERN_ERR "hkdk: failed to register backlight device: %d\n", ret);
}
#else
#define hkdk_backlight_register()       do { } while (0)
#endif

static void __init hkdkc100_map_io(void)
{
	s3c_device_nand.name = "s5pc100-nand";
	s5pc1xx_init_io(hkdkc100_iodesc, ARRAY_SIZE(hkdkc100_iodesc));
	s3c24xx_init_clocks(0);
#if defined(CONFIG_SERIAL_S5PC1XX_HSUART)
        writel((readl(S5P_CLK_DIV2) & ~(0xf << 0)), S5P_CLK_DIV2);
#endif
	s3c24xx_init_uarts(hkdkc100_uartcfgs, ARRAY_SIZE(hkdkc100_uartcfgs));
	s5pc1xx_reserve_bootmem();
}

// added by sy82.yoon(2009.05.18) to support android pmem
static void __init android_pmem_set_platdata(void)
{
        #ifdef CONFIG_ANDROID_PMEM
        pmem_pdata.start = (u32)s3c_get_media_memory(S3C_MDEV_PMEM);
        pmem_pdata.size = (u32)s3c_get_media_memsize(S3C_MDEV_PMEM);

        pmem_gpu1_pdata.start = (u32)s3c_get_media_memory(S3C_MDEV_PMEM_GPU1);
        pmem_gpu1_pdata.size = (u32)s3c_get_media_memsize(S3C_MDEV_PMEM_GPU1);

        pmem_adsp_pdata.start = (u32)s3c_get_media_memory(S3C_MDEV_PMEM_ADSP);
        pmem_adsp_pdata.size = (u32)s3c_get_media_memsize(S3C_MDEV_PMEM_ADSP);
        #endif
}

#define	GPJ0CON				(*(unsigned long *)S5PC1XX_GPJ0CON)
#define	GPJ0DAT				(*(unsigned long *)S5PC1XX_GPJ0DAT)
#define	GPJ0PUD				(*(unsigned long *)S5PC1XX_GPJ0PUD)

static void __init hkdkc100_machine_init(void)
{
	printk("%s : %s (%d) : Wifi&BT Power OFF!!\n", __FILE__, __FUNCTION__, __LINE__);

	// Wifi module Power off
	GPJ0CON  = ((GPJ0CON & ~0x00000FFF) | 0x00000111);	// GPH0.0 - GPH0.2 Output
	GPJ0PUD  = ((GPJ0PUD & ~0x0000003F) | 0x0000002A);	// GPH0.0 - GPH0.2 Pull Up
	GPJ0DAT &= (~0x00000007);							
	/* i2c */
	s3c_i2c0_set_platdata(NULL);
	i2c_register_board_info(0, i2c_devs0, ARRAY_SIZE(i2c_devs0));

	/* fimc */
	s3c_fimc0_set_platdata(NULL);
	s3c_fimc1_set_platdata(NULL);
	s3c_fimc2_set_platdata(NULL);
//	s3c_csis_set_platdata(NULL);

        /* pmem */
#ifdef CONFIG_ANDROID_PMEM
        android_pmem_set_platdata();    // added by sy82.yoon(2009.05.18)
#endif

	/* fb */
#ifdef CONFIG_FB_S3C
	s3cfb_set_platdata(NULL);
#endif
	platform_add_devices(hkdkc100_devices, ARRAY_SIZE(hkdkc100_devices));

#if defined(CONFIG_PM)
	s5pc1xx_pm_init();
#endif
    hkdk_backlight_register();

#if defined(CONFIG_MMC_SDHCI_S3C)
	s3c_sdhci_set_platdata();
#endif

#if !defined(CONFIG_MACH_HKDKC100)
	hkdkc100_reset_camera();
#endif
}

MACHINE_START(SMDKC100, "HKDKC100")
	/* Maintainer: Ben Dooks <ben@fluff.org> */
	.phys_io	= S3C_PA_UART & 0xfff00000,
	.io_pg_offst	= (((u32)S3C_VA_UART) >> 18) & 0xfffc,
	.boot_params	= S5PC1XX_PA_SDRAM + 0x100,

	.init_irq	= s5pc100_init_irq,
	.map_io		= hkdkc100_map_io,
	.init_machine	= hkdkc100_machine_init,
	.timer		= &s5pc1xx_timer,
MACHINE_END


#ifdef CONFIG_USB_SUPPORT
/* Initializes OTG Phy. */
void otg_phy_init(void) {
        writel(readl(S5P_OTHERS)|S5P_OTHERS_USB_SIG_MASK, S5P_OTHERS);
        writel(0x0, S3C_USBOTG_PHYPWR);         /* Power up */
        writel(OTGH_PHY_CLK_VALUE, S3C_USBOTG_PHYCLK);
        writel(0x7, S3C_USBOTG_RSTCON);

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
        writel(readl(S3C_USBOTG_PHYCLK) | (0X1 << 4), S3C_USBOTG_PHYCLK);
        writel(readl(S5P_OTHERS)&~S5P_OTHERS_USB_SIG_MASK, S5P_OTHERS);
}
EXPORT_SYMBOL(otg_phy_off);

void usb_host_clk_en(void) {
	struct clk *otg_clk;

        switch (S3C_USB_CLKSRC) {
        case 0: /* epll clk */
                /* Setting the epll clk to 48 MHz, P=3, M=96, S=3 */
                writel((readl(S5P_EPLL_CON) & ~(S5P_EPLL_MASK)) | (S5P_EPLL_EN \
						| S5P_EPLLVAL(96,3,3)), S5P_EPLL_CON);
                writel((readl(S5P_CLK_SRC0) | S5P_CLKSRC0_EPLL_MASK), S5P_CLK_SRC0);
                writel((readl(S5P_CLK_SRC1)& ~S5P_CLKSRC1_UHOST_MASK), S5P_CLK_SRC1);

                /* USB host clock divider ratio is 1 */
                writel((readl(S5P_CLK_DIV2)& ~S5P_CLKDIV2_UHOST_MASK), S5P_CLK_DIV2);
                break;

	case 1: /* oscillator 12M clk */
		otg_clk = clk_get(NULL, "otg");
		clk_enable(otg_clk);
		otg_phy_init();
		writel((readl(S5P_CLK_SRC1) | S5P_CLKSRC1_CLK48M_MASK) \
						| S5P_CLKSRC1_UHOST_MASK, S5P_CLK_SRC1);

		//USB host colock divider ratio is 1
		writel(readl(S5P_CLK_DIV2)& ~S5P_CLKDIV2_UHOST_MASK, S5P_CLK_DIV2);
		break;
	/* Add other clock sources here */

        default:
                printk(KERN_INFO "Unknown USB Host Clock Source\n");
                BUG();
                break;
        }

        writel(readl(S5P_CLKGATE_D10)|S5P_CLKGATE_D10_USBHOST, S5P_CLKGATE_D10);
        writel(readl(S5P_SCLKGATE0)|S5P_CLKGATE_SCLK0_USBHOST, S5P_SCLKGATE0);

}

EXPORT_SYMBOL(usb_host_clk_en);
#endif

