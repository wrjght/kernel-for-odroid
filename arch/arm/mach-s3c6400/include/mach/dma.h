/* linux/arch/arm/mach-s3c6400/include/mach/dma.h
 *
 * Copyright 2008 Openmoko, Inc.
 * Copyright 2008 Simtec Electronics
 *      Ben Dooks <ben@simtec.co.uk>
 *      http://armlinux.simtec.co.uk/
 *
 * S3C6400 - DMA support
 */

#ifndef __ASM_ARCH_DMA_H
#define __ASM_ARCH_DMA_H __FILE__

#include <mach/s3c-dma.h>

#ifdef CONFIG_ARCH_S3C64XX
#define S3C_DMA_CONTROLLERS        	(4)
#define S3C_CHANNELS_PER_DMA       	(8)
#define S3C_CANDIDATE_CHANNELS_PER_DMA  (16)
#define S3C_DMA_CHANNELS		(S3C_DMA_CONTROLLERS*S3C_CHANNELS_PER_DMA)
#else
#define S3C_DMA_CONTROLLERS        	(3)
#define S3C_CHANNELS_PER_DMA       	(8)
#define S3C_CANDIDATE_CHANNELS_PER_DMA  (32)
#define S3C_DMA_CHANNELS		(S3C_DMA_CONTROLLERS*S3C_CHANNELS_PER_DMA)
#endif
#endif /* __ASM_ARCH_DMA_H */
