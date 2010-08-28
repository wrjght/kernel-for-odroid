/* linux/arch/arm/plat-s3c/include/plat/regs-iis.h
 *
 * Copyright (c) 2003 Simtec Electronics <linux@simtec.co.uk>
 *		      http://www.simtec.co.uk/products/SWLINUX/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * S3C64XX IIS register definition
*/

#ifndef __ASM_ARCH_REGS_IIS_H
#define __ASM_ARCH_REGS_IIS_H

#define S5PC1XX_IIS0REG(x)	((x) + S5PC1XX_PA_IIS_V40)

//#define S3C_IIS0CON		S5PC1XX_IIS0REG(0x00)
//#define S3C_IIS0MOD		S5PC1XX_IIS0REG(0x04)
//#define S3C_IIS0FIC		S5PC1XX_IIS0REG(0x08)
//#define S3C_IIS0PSR		S5PC1XX_IIS0REG(0x0C)
//#define S3C_IIS0TXD		S5PC1XX_IIS0REG(0x10)
//#define S3C_IIS0RXD		S5PC1XX_IIS0REG(0x14)

#define S5PC1XX_IISCON		(0x00)
#define S5PC1XX_IISMOD		(0x04)
#define S5PC1XX_IISFIC		(0x08)
#define S5PC1XX_IISPSR		(0x0C)
#define S5PC1XX_IISTXD		(0x10)
#define S5PC1XX_IISRXD		(0x14)

#define S5PC1XX_ISCON_LRINDEX		(1<<8)
#define S5PC1XX_ISCON_TXFIFORDY	(1<<7)
#define S5PC1XX_ISCON_RXFIFORDY	(1<<6)

#define S5PC1XX_IISCON_FRXORSTATUS	(1<<19)
#define S5PC1XX_IISCON_FRXORINTEN 	(1<<18)
#define S5PC1XX_IISCON_FTXURSTATUS	(1<<17)
#define S5PC1XX_IISCON_FTXURINTEN 	(1<<16)

#define S5PC1XX_IISCON_LRI			(1<<11)
#define S5PC1XX_IISCON_FTXEMPT		(1<<10)
#define S5PC1XX_IISCON_FRXEMPT		(1<<9)

#define S5PC1XX_IISCON_TXDMAPAUSE	(1<<6)
#define S5PC1XX_IISCON_RXDMAPAUSE	(1<<5)
#define S5PC1XX_IISCON_TXCHPAUSE	(1<<4)
#define S5PC1XX_IISCON_RXCHPAUSE	(1<<3)
#define S5PC1XX_IISCON_TXDMACTIVE	(1<<2)
#define S5PC1XX_IISCON_RXDMACTIVE	(1<<1)
#define S5PC1XX_IISCON_I2SACTIVE	(1<<0)

#define S5PC1XX_IISMOD_BLCMASK				(0x3<<13)
#define S5PC1XX_IISMOD_BLC16BIT				(0x0<<13)
#define S5PC1XX_IISMOD_BLC8BIT				(0x1<<13)
#define S5PC1XX_IISMOD_BLC24BIT				(0x2<<13)
#define S5PC1XX_IISMOD_CDCLKCON				(0x1<<12)

#define S5PC1XX_IISMOD_INTERNAL_CLK			(0x0<<12)
#define S5PC1XX_IISMOD_EXTERNAL_CLK			(0x1<<12)

#define S5PC1XX_IISMOD_IMS_INTERNAL_MASTER	(0x0<<10)
#define S5PC1XX_IISMOD_MPLL					(0x01<<10)
#define S5PC1XX_IISMOD_IMS_EXTERNAL_MASTER	(0x1<<10)
#define S5PC1XX_IISMOD_IMS_SLAVE			(0x2<<10)

#define S5PC1XX_IISMOD_TXRMASK				(0x3<<8)
#define S5PC1XX_IISMOD_TX					(0x0<<8)
#define S5PC1XX_IISMOD_RX					(0x1<<8)
#define S5PC1XX_IISMOD_TXRX					(0x2<<8)

#define S5PC1XX_IISMOD_SLAVE		(1<<8)

#define S5PC1XX_IISMOD_LRP					(0x1<<7)
#define S5PC1XX_IISMOD_SDFMASK				(0x3<<5)
#define S5PC1XX_IISMOD_IIS					(0x0<<5)
#define S5PC1XX_IISMOD_MSB					(0x1<<5)
#define S5PC1XX_IISMOD_LSB					(0x2<<5)

#define S5PC1XX_IISMOD_RFSMASK				(0x3<<3)
#define S5PC1XX_IISMOD_768FS				(0x3<<3)
#define S5PC1XX_IISMOD_384FS				(0x2<<3)
#define S5PC1XX_IISMOD_512FS				(0x1<<3)
#define S5PC1XX_IISMOD_256FS				(0x0<<3)

#define S5PC1XX_IISMOD_BFSMASK				(0x3<<1)
#define S5PC1XX_IISMOD_24FS					(0x3<<1)
#define S5PC1XX_IISMOD_16FS					(0x2<<1)
#define S5PC1XX_IISMOD_48FS					(0x1<<1)
#define S5PC1XX_IISMOD_32FS					(0x0<<1)

#define S5PC1XX_IISPSR_PSRAEN		(0x01<<15)
#define S5PC1XX_IISPSR_PSVALA		(0x3f<<8)

#define S5PC1XX_IISFIFO		(0x10)
#define S5PC1XX_IISFIFORX	(0x14)

#define S5PC1XX_IISFIC_TFLUSH	(0x1<<15)
#define S5PC1XX_IISFIC_RFLUSH	(0x1<<7)

#endif /* __ASM_ARCH_REGS_IIS_H */
