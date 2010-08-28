#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/platform_device.h>

#include <asm/mach/arch.h>
#include <asm/mach/irq.h>
#include <mach/hardware.h>
#include <mach/map.h>

#include <plat/devs.h>

/* Serial port registrations */


/* NAND Controller */

static struct resource s3c_nand_resource[] = {
         [0] = {
                 .start = S3C64XX_PA_NAND,
                 .end   = S3C64XX_PA_NAND + S3C64XX_SZ_NAND - 1,
                 .flags = IORESOURCE_MEM,
         }
};

struct platform_device s3c_device_nand = {
         .name             = "s3c-nand",
         .id               = -1,
         .num_resources    = ARRAY_SIZE(s3c_nand_resource),
         .resource         = s3c_nand_resource,
};

EXPORT_SYMBOL(s3c_device_nand);


