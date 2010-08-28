/* /arch/arm/plat-s5pc1xx/include/plat/s5pc1xx-dvfs.h
 *
 * Copyright (c) 2009 Samsung Electronics
  *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __PLAT_S5PC1XX_DVFS_H
#define __PLAT_S5PC1XX_DVFS_H __FILE__

#define MAXIMUM_FREQ 666000
#define USE_FREQ_TABLE
//#undef USE_DVS
#define USE_DVS
#define VERY_HI_RATE  666*1000*1000
#define APLL_GEN_CLK  666*1000
#define KHZ_T		1000

#define MPU_CLK		"dout_arm"
#define INDX_ERROR  65535

#define ARM_LE		0
#define INT_LE		1

#define PMIC_ARM		0
#define PMIC_INT		1
#define PMIC_BOTH	2
#if defined(CONFIG_MACH_SMDKC100)
#define VCC_ARM		0
#define VCC_INT		1
#endif

enum PMIC_VOLTAGE {
	VOUT_1_00, 
	VOUT_1_05, 
	VOUT_1_10, 
	VOUT_1_15, 
	VOUT_1_20, 
	VOUT_1_25, 
	VOUT_1_30, 
	VOUT_1_35, 
	VOUT_1_40, 
	VOUT_1_45, 
	VOUT_1_50 	
};

// ltc3714 voltage table
static const unsigned int voltage_table[11] = {
	0xf, 0xe, 0xd, 0xc, 0xb, 0xa, 0x9,
	0x8, 0x7, 0x6, 0x5,
};

extern unsigned int s5pc1xx_cpufreq_index;
extern unsigned int S5PC1XX_FREQ_TAB;
extern unsigned int S5PC1XX_MAXFREQLEVEL;


extern int set_voltage(unsigned int);
#if defined(CONFIG_MACH_SMDKC100)
extern void ltc3714_init(unsigned int, unsigned int);
#endif
extern unsigned int s5pc1xx_target_frq(unsigned int pred_freq, int flag);
extern int s5pc100_pm_target(unsigned int target_freq);
extern int is_conservative_gov(void);
extern int is_userspace_gov(void);
extern void set_dvfs_perf_level(void);

#define S5PC100_LOCKHCLK_USBHOST		0xE2000001
#define S5PC100_LOCKHCLK_USBOTG		0xE2000002
#define S5PC100_LOCKHCLK_SDMMC0		0xE2000004
#define S5PC100_LOCKHCLK_SDMMC1		0xE2000008
#define S5PC100_LOCKHCLK_SDMMC2		0xE2000010
extern int s5pc100_dvfs_lock_high_hclk(unsigned int dToken);
extern int s5pc100_dvfs_unlock_high_hclk(unsigned int dToken);

#define DVFS_LOCK_TOKEN_1	 0x01 << 0
#define DVFS_LOCK_TOKEN_2	 0x01 << 1
#define DVFS_LOCK_TOKEN_3	 0x01 << 2
#define DVFS_LOCK_TOKEN_4	 0x01 << 3
#define DVFS_LOCK_TOKEN_5	 0x01 << 4
void s5pc100_lock_dvfs_high_level(unsigned int nToken);
void s5pc100_unlock_dvfs_high_level(unsigned int nToken);

#endif /* __PLAT_S5PC1XX_DVFS_H */
