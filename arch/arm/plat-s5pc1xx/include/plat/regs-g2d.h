/* linux/arch/arm/plat-s5pc1xx/include/plat/regs-g2d.h
 *
 * Driver file for Samsung 2D Accelerator(FIMG-2D)
 * Jinhee Hyeon, Copyright (c) 2009 Samsung Electronics
 * 	http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#ifndef __ASM_ARM_REGS_G2D_H
#define __ASM_ARM_REGS_G2D_H

/************************************************************************/
/*Graphics 2D Registers part						*/
/************************************************************************/
#define G2DREG(x)	((x))

/* Graphics 2D General Registers */
#define	G2D_CONTROL_REG			G2DREG(0x00)		/* Control register */
#define	G2D_INTEN_REG			G2DREG(0x04)		/* Interrupt enable register */
#define	G2D_FIFO_INTC_REG		G2DREG(0x08)		/* Interrupt control register */
#define	G2D_INTC_PEND_REG		G2DREG(0x0c)		/* Interrupt control pending register */
#define	G2D_FIFO_STAT_REG		G2DREG(0x10)		/* Command FIFO status register */

/* Graphics 2D Command Registers */
#define	G2D_CMD0_REG			G2DREG(0x100)		/* Command register for Line/Point drawing */
#define	G2D_CMD1_REG			G2DREG(0x104)		/* Command register for BitBLT */
#define	G2D_CMD2_REG			G2DREG(0x108)		/* Command register for Host to Screen Bitblt transfer start */
#define	G2D_CMD3_REG			G2DREG(0x10c)		/* Command register for Host to Screen Bitblt transfer continue */
#define	G2D_CMD4_REG			G2DREG(0x110)		/* Command register for Color expansion (Font start) */
#define	G2D_CMD5_REG			G2DREG(0x114)		/* Command register for Color expansion (Font continue) */
#define	G2D_CMD6_REG			G2DREG(0x118)		/* Reserved */
#define	G2D_CMD7_REG			G2DREG(0x11c)		/* Command register for Color expansion (memory to screen) */

/* Graphics 2D Parameter Setting Registers */
/* Resolution */
#define	G2D_SRC_RES_REG			G2DREG(0x200)		/* Source image resolution */
#define	G2D_HORI_RES_REG		G2DREG(0x204)		/* Source image horizontal resolution */
#define	G2D_VERT_RES_REG		G2DREG(0x208)		/* Source image vertical resolution */
#define	G2D_SC_RES_REG			G2DREG(0x210)		/* Screen resolution */
#define	G2D_SC_HORI_REG			G2DREG(0x214)		/* Screen horizontal resolutuon */
#define	G2D_SC_VERT_REG			G2DREG(0x218)		/* Screen vertical resolution */

/* Clipping window */
#define	G2D_CW_LT_REG			G2DREG(0x220)		/* LeftTop coordinates of Clip Window */
#define	G2D_CW_LT_X_REG			G2DREG(0x224)		/* Left X coordinate of Clip Window */
#define	G2D_CW_LT_Y_REG			G2DREG(0x228)		/* Top Y coordinate of Clip Window */
#define	G2D_CW_RB_REG			G2DREG(0x230)		/* RightBottom coordinate of Clip Window */
#define	G2D_CW_RB_X_REG			G2DREG(0x234)		/* Right X coordinate of Clip Window */
#define	G2D_CW_RB_Y_REG			G2DREG(0x238)		/* Bottom Y coordinate of Clip Window */

/* Coordinates */
#define	G2D_COORD0_REG			G2DREG(0x300)
#define	G2D_COORD0_X_REG		G2DREG(0x304)
#define	G2D_COORD0_Y_REG		G2DREG(0x308)
#define	G2D_COORD1_REG			G2DREG(0x310)
#define	G2D_COORD1_X_REG		G2DREG(0x314)
#define	G2D_COORD1_Y_REG		G2DREG(0x318)
#define	G2D_COORD2_REG			G2DREG(0x320)
#define	G2D_COORD2_X_REG		G2DREG(0x324)
#define	G2D_COORD2_Y_REG		G2DREG(0x328)
#define	G2D_COORD3_REG			G2DREG(0x330)
#define	G2D_COORD3_X_REG		G2DREG(0x334)
#define	G2D_COORD3_Y_REG		G2DREG(0x338)

/* Rotation */
#define	G2D_ROT_OC_REG			G2DREG(0x340)		/* Rotation Origin Coordinates */
#define	G2D_ROT_OC_X_REG		G2DREG(0x344)		/* X coordinate of Rotation Origin Coordinates */
#define	G2D_ROT_OC_Y_REG		G2DREG(0x348)		/* Y coordinate of Rotation Origin Coordinates */
#define	G2D_ROTATE_REG			G2DREG(0x34c)		/* Rotation Mode register */
#define	G2D_ENDIAN_REG			G2DREG(0x350)		/* Big & Little ENDIAN Select */

/* X,Y Increment setting */
#define	G2D_X_INCR_REG			G2DREG(0x400)
#define	G2D_Y_INCR_REG			G2DREG(0x404)
#define	G2D_ROP_REG			G2DREG(0x410)
#define	G2D_ALPHA_REG			G2DREG(0x420)

/* Color */
#define	G2D_FG_COLOR_REG		G2DREG(0x500)		/* Foreground Color Alpha register */
#define	G2D_BG_COLOR_REG		G2DREG(0x504)		/* Background Color register */
#define	G2D_BS_COLOR_REG		G2DREG(0x508)		/* Blue Screen Color register */
#define	G2D_SRC_COLOR_MODE		G2DREG(0x510)		/* Src Image Color Mode register */
#define	G2D_DST_COLOR_MODE		G2DREG(0x514)		/* Dest Image Color Mode register */

/* Pattern */
#define	G2D_PATTERN_REG			G2DREG(0x600)
#define	G2D_PATOFF_REG			G2DREG(0x700)
#define	G2D_PATOFF_X_REG		G2DREG(0x704)
#define	G2D_PATOFF_Y_REG		G2DREG(0x708)
#define	G2D_STENCIL_CNTL_REG 		G2DREG(0x720)
#define	G2D_STENCIL_DR_MIN_REG		G2DREG(0x724)
#define	G2D_STENCIL_DR_MAX_REG		G2DREG(0x728)

#define	G2D_SRC_BASE_ADDR		G2DREG(0x730)		/* Source image base address register */
#define	G2D_DST_BASE_ADDR		G2DREG(0x734)		/* Dest image base address register */


/************************************************************************/
/* Bit definition part							*/
/************************************************************************/
#define G2D_FIFO_USED(x)				(((x)&0x7f)>>1)

#define G2D_FULL_H(x)					((x)&0x7FF)
#define G2D_FULL_V(x)					(((x)&0x7FF)<<16)

#define G2D_ALPHA(x)					((x)&0xFF)

/* inter mode select */
#define	G2D_INTC_PEND_REG_CLRSEL_LEVEL			(1<<31)
#define	G2D_INTC_PEND_REG_CLRSEL_PULSE			(0<<31)

#define G2D_INTEN_REG_FIFO_INT_E			(1<<0)
#define G2D_INTEN_REG_ACF				(1<<9)
#define G2D_INTEN_REG_CCF				(1<<10)

#define G2D_PEND_REG_INTP_ALL_FIN			(1<<9)
#define G2D_PEND_REG_INTP_CMD_FIN			(1<<10)

/* Line/t drawing */
#define	G2D_CMD0_REG_D_LAST				(0<<9)
#define	G2D_CMD0_REG_D_NO_LAST				(1<<9)

#define	G2D_CMD0_REG_M_Y				(0<<8)
#define	G2D_CMD0_REG_M_X				(1<<8)

#define	G2D_CMD0_REG_L					(1<<1)
#define	G2D_CMD0_REG_P					(1<<0)

/* BitBLT */
#define	G2D_CMD1_REG_S					(1<<1)
#define	G2D_CMD1_REG_N					(1<<0)

/* resource color mode */
#define G2D_COLOR_MODE_REG_C3_32BPP			(1<<3)
#define G2D_COLOR_MODE_REG_C3_24BPP			(1<<3)
#define G2D_COLOR_MODE_REG_C2_18BPP			(1<<2)
#define G2D_COLOR_MODE_REG_C1_16BPP			(1<<1)
#define G2D_COLOR_MODE_REG_C0_15BPP			(1<<0)

#define G2D_COLOR_RGB_565				(0x0<<0)
#define G2D_COLOR_RGBA_5551				(0x1<<0)
#define G2D_COLOR_ARGB_1555				(0x2<<0)
#define G2D_COLOR_RGBA_8888				(0x3<<0)
#define G2D_COLOR_ARGB_8888				(0x4<<0)
#define G2D_COLOR_XRGB_8888				(0x5<<0)
#define G2D_COLOR_RGBX_8888				(0x6<<0)

/* rotation mode */
#define G2D_ROTATE_REG_FY				(1<<5)
#define G2D_ROTATE_REG_FX				(1<<4)
#define G2D_ROTATE_REG_R3_270				(1<<3)
#define G2D_ROTATE_REG_R2_180				(1<<2)
#define G2D_ROTATE_REG_R1_90				(1<<1)
#define G2D_ROTATE_REG_R0_0				(1<<0)

/* Endian Select */
#define G2D_ENDIAN_READSIZE_BIG_ENDIAN_BIG		(1<<4)
#define G2D_ENDIAN_READSIZE_BIG_ENDIAN_LITTLE		(0<<4)

#define G2D_ENDIAN_READSIZE_SIZE_HW_DISABLE		(0<<2)
#define G2D_ENDIAN_READSIZE_SIZE_HW_ENABLE		(1<<2)

/* read buffer size */
#define	G2D_ENDIAN_READSIZE_READ_SIZE_1			(0<<0)
#define	G2D_ENDIAN_READSIZE_READ_SIZE_4			(1<<0)
#define	G2D_ENDIAN_READSIZE_READ_SIZE_8			(2<<0)
#define	G2D_ENDIAN_READSIZE_READ_SIZE_16		(3<<0)

/* Third Operans Select */
#define G2D_ROP_REG_OS_PATTERN				(0<<13)
#define G2D_ROP_REG_OS_FG_COLOR				(1<<13)

/* Alpha Blending Mode */
#define G2D_ROP_REG_ABM_NO_BLENDING			(0<<10)
#define G2D_ROP_REG_ABM_SRC_BITMAP			(1<<10)
#define G2D_ROP_REG_ABM_REGISTER			(2<<10)
#define G2D_ROP_REG_ABM_FADING 				(4<<10)

/* Rasteeration mode */
#define G2D_ROP_REG_T_OPAQUE_MODE			(0<<9)
#define G2D_ROP_REG_T_TRANSP_MODE			(1<<9)

#define G2D_ROP_REG_B_BS_MODE_OFF			(0<<8)
#define G2D_ROP_REG_B_BS_MODE_ON			(1<<8)


/* stencontrol */
#define G2D_STENCIL_CNTL_REG_STENCIL_ON_ON		(1<<31)
#define G2D_STENCIL_CNTL_REG_STENCIL_ON_OFF		(0<<31)

#define G2D_STENCIL_CNTL_REG_STENCIL_INVERSE		(1<<23)
#define G2D_STENCIL_CNTL_REG_STENCIL_SWAP		(1<<0)

/*********************************************************************************/
#endif /* __ASM_ARM_REGS_G2D_H */
