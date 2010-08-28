/* linux/arch/arm/plat-s5pc1xx/include/plat/regs-gpio.h
 *
 * Copyright 2008 Openmoko, Inc.
 * Copyright 2008 Simtec Electronics
 *      Ben Dooks <ben@simtec.co.uk>
 *      http://armlinux.simtec.co.uk/
 *
 * S5PC1XX - GPIO register definitions
 */

#ifndef __ASM_PLAT_S5PC1XX_REGS_GPIO_H
#define __ASM_PLAT_S5PC1XX_REGS_GPIO_H __FILE__

/* Base addresses for each of the banks */
#include <plat/gpio-bank-a0.h>
#include <plat/gpio-bank-a1.h>
#include <plat/gpio-bank-b.h>
#include <plat/gpio-bank-c.h>
#include <plat/gpio-bank-d.h>
#include <plat/gpio-bank-e0.h>
#include <plat/gpio-bank-e1.h>
#include <plat/gpio-bank-f0.h>
#include <plat/gpio-bank-f1.h>
#include <plat/gpio-bank-f2.h>
#include <plat/gpio-bank-f3.h>
#include <plat/gpio-bank-g0.h>
#include <plat/gpio-bank-g1.h>
#include <plat/gpio-bank-g2.h>
#include <plat/gpio-bank-g3.h>
#include <plat/gpio-bank-h0.h>
#include <plat/gpio-bank-h1.h>
#include <plat/gpio-bank-h2.h>
#include <plat/gpio-bank-h3.h>
#include <plat/gpio-bank-i.h>
#include <plat/gpio-bank-j0.h>
#include <plat/gpio-bank-j1.h>
#include <plat/gpio-bank-j2.h>
#include <plat/gpio-bank-j3.h>
#include <plat/gpio-bank-j4.h>
#include <plat/gpio-bank-k0.h>
#include <plat/gpio-bank-k1.h>
#include <plat/gpio-bank-k2.h>
#include <plat/gpio-bank-k3.h>
#include <plat/gpio-bank-l0.h>
#include <plat/gpio-bank-l1.h>
#include <plat/gpio-bank-l2.h>
#include <plat/gpio-bank-l3.h>
#include <plat/gpio-bank-l4.h>

#include <mach/map.h>

#define S5PC1XX_GPA0_BASE	(S5PC1XX_VA_GPIO + 0x0000)
#define S5PC1XX_GPA1_BASE	(S5PC1XX_VA_GPIO + 0x0020)
#define S5PC1XX_GPB_BASE	(S5PC1XX_VA_GPIO + 0x0040)
#define S5PC1XX_GPC_BASE	(S5PC1XX_VA_GPIO + 0x0060)
#define S5PC1XX_GPD_BASE	(S5PC1XX_VA_GPIO + 0x0080)
#define S5PC1XX_GPE0_BASE	(S5PC1XX_VA_GPIO + 0x00A0)
#define S5PC1XX_GPE1_BASE	(S5PC1XX_VA_GPIO + 0x00C0)
#define S5PC1XX_GPF0_BASE	(S5PC1XX_VA_GPIO + 0x00E0)
#define S5PC1XX_GPF1_BASE	(S5PC1XX_VA_GPIO + 0x0100)
#define S5PC1XX_GPF2_BASE	(S5PC1XX_VA_GPIO + 0x0120)
#define S5PC1XX_GPF3_BASE	(S5PC1XX_VA_GPIO + 0x0140)
#define S5PC1XX_GPG0_BASE	(S5PC1XX_VA_GPIO + 0x0160)
#define S5PC1XX_GPG1_BASE	(S5PC1XX_VA_GPIO + 0x0180)
#define S5PC1XX_GPG2_BASE	(S5PC1XX_VA_GPIO + 0x01A0)
#define S5PC1XX_GPG3_BASE	(S5PC1XX_VA_GPIO + 0x01C0)
#define S5PC1XX_GPH0_BASE	(S5PC1XX_VA_GPIO + 0x0C00)
#define S5PC1XX_GPH1_BASE	(S5PC1XX_VA_GPIO + 0x0C20)
#define S5PC1XX_GPH2_BASE	(S5PC1XX_VA_GPIO + 0x0C40)
#define S5PC1XX_GPH3_BASE	(S5PC1XX_VA_GPIO + 0x0C60)
#define S5PC1XX_GPI_BASE	(S5PC1XX_VA_GPIO + 0x01E0)
#define S5PC1XX_GPJ0_BASE	(S5PC1XX_VA_GPIO + 0x0200)
#define S5PC1XX_GPJ1_BASE	(S5PC1XX_VA_GPIO + 0x0220)
#define S5PC1XX_GPJ2_BASE	(S5PC1XX_VA_GPIO + 0x0240)
#define S5PC1XX_GPJ3_BASE	(S5PC1XX_VA_GPIO + 0x0260)
#define S5PC1XX_GPJ4_BASE	(S5PC1XX_VA_GPIO + 0x0280)
#define S5PC1XX_GPK0_BASE	(S5PC1XX_VA_GPIO + 0x02A0)
#define S5PC1XX_GPK1_BASE	(S5PC1XX_VA_GPIO + 0x02C0)
#define S5PC1XX_GPK2_BASE	(S5PC1XX_VA_GPIO + 0x02E0)
#define S5PC1XX_GPK3_BASE	(S5PC1XX_VA_GPIO + 0x0300)
#define S5PC1XX_GPL0_BASE	(S5PC1XX_VA_GPIO + 0x0320)
#define S5PC1XX_GPL1_BASE	(S5PC1XX_VA_GPIO + 0x0340)
#define S5PC1XX_GPL2_BASE	(S5PC1XX_VA_GPIO + 0x0360)
#define S5PC1XX_GPL3_BASE	(S5PC1XX_VA_GPIO + 0x0380)
#define S5PC1XX_GPL4_BASE	(S5PC1XX_VA_GPIO + 0x03A0)
#define S5PC1XX_EINT_BASE	(S5PC1XX_VA_GPIO + 0x0E00)

#if 1 //add by Jay 7/18
#define S5PC1XX_GPIOINT_BASE	(S5PC1XX_VA_GPIO + 0x0700)
#define S5PC1XX_WKUPINT_BASE	(S5PC1XX_VA_GPIO + 0x0E00)
#define S5PC1XX_WKUPMASK_BASE	(S5PC1XX_VA_GPIO + 0x0F00)

/* WKUP interrupt */
#define S5PC1XX_WKUPINT0CON		(S5PC1XX_WKUPINT_BASE + 0x00)
#define S5PC1XX_WKUPINT1CON		(S5PC1XX_WKUPINT_BASE + 0x04)
#define S5PC1XX_WKUPINT2CON		(S5PC1XX_WKUPINT_BASE + 0x08)
#define S5PC1XX_WKUPINT3CON		(S5PC1XX_WKUPINT_BASE + 0x0c)
#define S5PC1XX_WKUPINT0FLTCON0 (S5PC1XX_WKUPINT_BASE + 0x80)
#define S5PC1XX_WKUPINT0FLTCON1	(S5PC1XX_WKUPINT_BASE + 0x84)
#define S5PC1XX_WKUPINT1FLTCON0	(S5PC1XX_WKUPINT_BASE + 0x88)
#define S5PC1XX_WKUPINT1FLTCON1	(S5PC1XX_WKUPINT_BASE + 0x8C)
#define S5PC1XX_WKUPINT2FLTCON0	(S5PC1XX_WKUPINT_BASE + 0x90)
#define S5PC1XX_WKUPINT2FLTCON1	(S5PC1XX_WKUPINT_BASE + 0x94)
#define S5PC1XX_WKUPINT3FLTCON0	(S5PC1XX_WKUPINT_BASE + 0x98)
#define S5PC1XX_WKUPINT3FLTCON1	(S5PC1XX_WKUPINT_BASE + 0x9C)
#define S5PC1XX_WKUPINT0MASK	(S5PC1XX_WKUPMASK_BASE + 0x00)
#define S5PC1XX_WKUPINT1MASK	(S5PC1XX_WKUPMASK_BASE + 0x04)
#define S5PC1XX_WKUPINT2MASK	(S5PC1XX_WKUPMASK_BASE + 0x08)
#define S5PC1XX_WKUPINT3MASK	(S5PC1XX_WKUPMASK_BASE + 0x0C)
#define S5PC1XX_WKUPINT0PEND	(S5PC1XX_WKUPMASK_BASE + 0x40)
#define S5PC1XX_WKUPINT1PEND	(S5PC1XX_WKUPMASK_BASE + 0x44)
#define S5PC1XX_WKUPINT2PEND	(S5PC1XX_WKUPMASK_BASE + 0x48)
#define S5PC1XX_WKUPINT3PEND	(S5PC1XX_WKUPMASK_BASE + 0x4C)
#endif

#define S5PC1XX_UHOST	(S5PC1XX_VA_GPIO + 0x0B68)
#define S5PC1XX_PDNEN	(S5PC1XX_VA_GPIO + 0x0F80)

#define S5PC1XX_GPIOREG(x)		(S5PC1XX_VA_GPIO + (x))

#define S5PC1XX_EINT0CON		S5PC1XX_GPIOREG(0xE00)			/* EINT0  ~ EINT7  */
#define S5PC1XX_EINT1CON		S5PC1XX_GPIOREG(0xE04)			/* EINT8  ~ EINT15 */
#define S5PC1XX_EINT2CON		S5PC1XX_GPIOREG(0xE08)			/* EINT16 ~ EINT23 */
#define S5PC1XX_EINT3CON		S5PC1XX_GPIOREG(0xE0C)			/* EINT24 ~ EINT31 */
#define S5PC1XX_EINTCON(x)		(S5PC1XX_EINT0CON + ((x) * 0x4))	/* EINT0  ~ EINT31 */

#define S5PC1XX_EINT0FLTCON0		S5PC1XX_GPIOREG(0xE80)			/* EINT0  ~ EINT3  */
#define S5PC1XX_EINT0FLTCON1		S5PC1XX_GPIOREG(0xE84)			
#define S5PC1XX_EINT1FLTCON0		S5PC1XX_GPIOREG(0xE88)			/* EINT8 ~  EINT11 */
#define S5PC1XX_EINT1FLTCON1		S5PC1XX_GPIOREG(0xE8C)
#define S5PC1XX_EINT2FLTCON0		S5PC1XX_GPIOREG(0xE90)
#define S5PC1XX_EINT2FLTCON1		S5PC1XX_GPIOREG(0xE94)
#define S5PC1XX_EINT3FLTCON0		S5PC1XX_GPIOREG(0xE98)
#define S5PC1XX_EINT3FLTCON1		S5PC1XX_GPIOREG(0xE9C)
#define S5PC1XX_EINTFLTCON(x)		(S5PC1XX_EINT0FLTCON0 + ((x) * 0x4))	/* EINT0  ~ EINT31 */

#define S5PC1XX_EINT0MASK		S5PC1XX_GPIOREG(0xF00)			/* EINT0 ~  EINT7  */
#define S5PC1XX_EINT1MASK		S5PC1XX_GPIOREG(0xF04)			/* EINT8 ~  EINT15 */
#define S5PC1XX_EINT2MASK		S5PC1XX_GPIOREG(0xF08)			/* EINT16 ~ EINT23 */
#define S5PC1XX_EINT3MASK		S5PC1XX_GPIOREG(0xF0C)			/* EINT24 ~ EINT31 */
#define S5PC1XX_EINTMASK(x)		(S5PC1XX_EINT0MASK + ((x) * 0x4))	/* EINT0 ~  EINT31 */

#define S5PC1XX_EINT0PEND		S5PC1XX_GPIOREG(0xF40)			/* EINT0 ~  EINT7  */
#define S5PC1XX_EINT1PEND		S5PC1XX_GPIOREG(0xF44)			/* EINT8 ~  EINT15 */
#define S5PC1XX_EINT2PEND		S5PC1XX_GPIOREG(0xF48)			/* EINT16 ~ EINT23 */
#define S5PC1XX_EINT3PEND		S5PC1XX_GPIOREG(0xF4C)			/* EINT24 ~ EINT31 */
#define S5PC1XX_EINTPEND(x)		(S5PC1XX_EINT0PEND + ((x) * 0x4))	/* EINT0 ~  EINT31 */

#define eint_offset(irq)		((irq) < IRQ_EINT16_31 ? ((irq) - IRQ_EINT0) :  \
					((irq - S3C_IRQ_EINT_BASE) + IRQ_EINT16_31 - IRQ_EINT0))
					
#define eint_irq_to_bit(irq)		(1 << (eint_offset(irq) & 0x7))

#define eint_conf_reg(irq)		((eint_offset(irq)) >> 3)
#define eint_filt_reg(irq)		((eint_offset(irq)) >> 2)
#define eint_mask_reg(irq)		((eint_offset(irq)) >> 3)
#define eint_pend_reg(irq)		((eint_offset(irq)) >> 3)


#endif /* __ASM_PLAT_S5PC1XX_REGS_GPIO_H */
