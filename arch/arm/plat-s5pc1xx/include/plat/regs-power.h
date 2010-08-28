/* arch/arm/plat-s5pc1xx/include/plat/regs-power.h
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * S5PC1XX power control register definitions
*/

#ifndef __ASM_ARM_REGS_PWR
#define __ASM_ARM_REGS_PWR __FILE__

/* register for EINT on PM Driver */
#define S5P_APM_GPIO		(S5PC1XX_PA_GPIO + 0xC00)
#define S5P_APM_REG(x)		((x) + S5P_APM_GPIO)

#define S5P_APM_BASE	 	S5P_APM_REG(0x000)
#define S5P_APM_GPH1CON 	(0x020)
#define S5P_APM_GPH1DAT 	(0x024)
#define S5P_APM_GPH1PUD 	(0x028)
#define S5P_APM_GPH1DRV 	(0x02C)
#define S5P_APM_GPH2CON 	(0x040)
#define S5P_APM_GPH2DAT 	(0x044)
#define S5P_APM_GPH2PUD 	(0x048)
#define S5P_APM_GPH2DRV 	(0x04C)
#define S5P_APM_GPH3CON 	(0x060)
#define S5P_APM_GPH3DAT 	(0x064)
#define S5P_APM_GPH3PUD 	(0x068)
#define S5P_APM_GPH3DRV 	(0x06C)
#define S5P_APM_WEINT0_CON 	(0x200)
#define S5P_APM_WEINT1_CON 	(0x204)
#define S5P_APM_WEINT2_CON 	(0x208)
#define S5P_APM_WEINT3_CON 	(0x20C)
#define S5P_APM_WEINT0_FLTCON0 	(0x280)
#define S5P_APM_WEINT0_FLTCON1 	(0x284)
#define S5P_APM_WEINT1_FLTCON0 	(0x288)
#define S5P_APM_WEINT1_FLTCON1 	(0x28C)
#define S5P_APM_WEINT2_FLTCON0 	(0x290)
#define S5P_APM_WEINT2_FLTCON1 	(0x294)
#define S5P_APM_WEINT3_FLTCON0 	(0x298)
#define S5P_APM_WEINT3_FLTCON1 	(0x29C)
#define S5P_APM_WEINT0_MASK 	(0x300)
#define S5P_APM_WEINT1_MASK 	(0x304)
#define S5P_APM_WEINT2_MASK 	(0x308)
#define S5P_APM_WEINT3_MASK 	(0x30C)
#define S5P_APM_WEINT0_PEND 	(0x340)
#define S5P_APM_WEINT1_PEND 	(0x344)
#define S5P_APM_WEINT2_PEND 	(0x348)
#define S5P_APM_WEINT3_PEND 	(0x34C)

#ifdef CONFIG_PM_PWR_GATING
//add by jay
/* defintion to support clock & power gating */
#define DOMAIN_ACTIVE_MODE 1
#define DOMAIN_LP_MODE     2

// Clock & Power Group Domain
#define D0_DOMAIN   0
#define D1_DOMAIN   1
#define D2_DOMAIN   2

// Power Domain of S5PC100
#define S5PC100_POWER_DOMAIN_NA      0
#define S5PC100_POWER_DOMAIN_MFC     (0x1 <<1)  //MFC
#define S5PC100_POWER_DOMAIN_G3D     (0x1 <<2)  //G3D
#define S5PC100_POWER_DOMAIN_LCD     (0x1 <<3)  //CAMIF, DISPCON, G2D, JPEG, MIPI_CSI, MIPI_DSI, ROTATOR 
#define S5PC100_POWER_DOMAIN_TV      (0x1 <<4)  //HDMI, MIXER, VP, TV encoder
#define S5PC100_POWER_DOMAIN_AUDIO   (0x1 <<5)  //Audio domain

// NORMAL_CFG definition
#define AUDIO_ACTIVE_MODE	0x20
#define TV_ACTIVE_MODE		0x10
#define LCD_ACTIVE_MODE		0x08
#define G3D_ACTIVE_MODE		0x04
#define MFC_ACTIVE_MODE		0x02

// BLK_PWR_STAT definition
#define BLK_PWR_AUDIO_READY		0x20
#define BLK_PWR_TV_READY		0x10
#define BLK_PWR_LCD_READY		0x08
#define BLK_PWR_G3D_READY		0x04
#define BLK_PWR_MFC_READY		0x02

#define S5P_CLKSRC0_AUDIO_DOMAIN_MASK	(0x1<<0)
#define S5P_CLKSRC0_TV_DOMAIN_MASK		(0x1<<0)
#define S5P_CLKSRC0_LCD_DOMAIN_MASK		(0x1<<0)
#define S5P_CLKSRC0_G3D_DOMAIN_MASK		(0x1<<0)
#define S5P_CLKSRC0_MFC_DOMAIN_MASK		(0x1<<0)

// Redefine warnning(reg_clock.h) removed
//#define S5P_CLKSRC0_APLL_SHIFT		(0)
//#define S5P_CLKSRC0_MPLL_MASK		(0x1<<4)
//#define S5P_CLKSRC0_MPLL_SHIFT		(4)
//#define S5P_CLKSRC0_EPLL_MASK		(0x1<<8)
//#define S5P_CLKSRC0_EPLL_SHIFT		(8)
//#define S5P_CLKSRC0_HPLL_MASK		(0x1<<12)
//#define S5P_CLKSRC0_HPLL_SHIFT		(12)
//#define S5P_CLKSRC0_AMMUX_MASK		(0x1<<16)
//#define S5P_CLKSRC0_AMMUX_SHIFT		(16)
//#define S5P_CLKSRC0_HREF_MASK		(0x1<<20)
//#define S5P_CLKSRC0_HREF_SHIFT		(20)
//#define S5P_CLKSRC0_ONENAND_MASK	(0x1<<24)
//#define S5P_CLKSRC0_ONENAND_SHIFT	(24

// API functions
extern int s5p_wait_blk_pwr_ready(unsigned int power_domain);
extern int s5p_power_gating(unsigned int power_domain, unsigned int on_off);
extern int s5p_domain_off_check(unsigned int power_domain);

#endif  // End of CONFIG_PM_PWR_GATING

#endif /* __ASM_ARM_REGS_PWR */

