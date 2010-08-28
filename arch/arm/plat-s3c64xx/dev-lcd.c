#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/platform_device.h>

#include <asm/mach/arch.h>
#include <asm/mach/irq.h>
#include <mach/hardware.h>
#include <mach/map.h>

#include <plat/irqs.h>
#include <plat/devs.h>
#include <plat/adc.h>

/* LCD Controller */

static struct resource s3c_lcd_resource[] = {
         [0] = {
                 .start = S3C64XX_PA_LCD,
                 .end   = S3C64XX_PA_LCD + SZ_1M - 1,
                 .flags = IORESOURCE_MEM,
         },
         [1] = {
                 .start = IRQ_LCD_VSYNC,
                 .end   = IRQ_LCD_SYSTEM,
                 .flags = IORESOURCE_IRQ,
         }
};

static u64 s3c_device_lcd_dmamask = 0xffffffffUL;

struct platform_device s3c_device_lcd = {
         .name             = "s3c-lcd",
         .id               = -1,
         .num_resources    = ARRAY_SIZE(s3c_lcd_resource),
         .resource         = s3c_lcd_resource,
         .dev              = {
         .dma_mask               = &s3c_device_lcd_dmamask,
                .coherent_dma_mask      = 0xffffffffUL
         }
};
