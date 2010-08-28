/*
 *  linux/arch/arm/plat-s3c64xx/s3c64xx-cpufreq.c
 *
 *  CPU frequency scaling for S3C64XX
 *
 *  Copyright (C) 2008 Samsung Electronics
 *
 *  Based on cpu-sa1110.c, Copyright (C) 2001 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#ifndef CONFIG_HAS_WAKELOCK
#ifdef CONFIG_ANDROID_POWER
#include <linux/android_power.h>
#endif
#elif CONFIG_HAS_WAKELOCK
#include <linux/wakelock.h>
#include <linux/earlysuspend.h>
#endif

#include <asm/system.h>
#include <plat/s5pc1xx-dvfs.h>
#include <plat/regs-clock.h>

#include <mach/hardware.h>
#include <mach/map.h>

/* definition for power setting function */
//extern int set_power(unsigned int freq);
extern int set_pmic(unsigned int pwr, unsigned int voltage);
extern int s5pc1xx_clk_D1sync_change(int index);

#if 1 // add by sryoo
unsigned int S5PC1XX_MAXFREQLEVEL;
#endif

#define CLK_OUT_PROBING	//TP80 on SMDKC100 board

#define CLIP_LEVEL(a, b) (a > b ? b : a)
#define DVFS_SAMPLING_RATE 40000 //40mSec (in usec)
#define ENABLE_DVFS_LOCK_HIGH 1
unsigned int S5PC1XX_MAXFREQLEVEL = 3;
static unsigned int s5pc1xx_cpufreq_level = 3;
unsigned int S5PC1XX_HCLKD1_HIGH_LOCK = 0;

static char cpufreq_governor_name[CPUFREQ_NAME_LEN] = "userspace";
static char userspace_governor[CPUFREQ_NAME_LEN] = "userspace";

unsigned int s5pc1xx_cpufreq_index = 0;
static spinlock_t g_cpufreq_lock = SPIN_LOCK_UNLOCKED;
unsigned int dvfs_change_direction;
static DEFINE_MUTEX(dvfs_high_hclk_lock);
#if ENABLE_DVFS_LOCK_HIGH
unsigned int g_dvfs_high_lock_token = 0;
static DEFINE_MUTEX(dvfs_high_lock);
#endif //ENABLE_DVFS_LOCK_HIGH
//EVT1
static struct cpufreq_frequency_table freq_table_evt1b[] = {
	{0, 833*KHZ_T},
	{1, 666*KHZ_T},
	{2, 533*KHZ_T},
	{3, 417*KHZ_T},
	{4, 333*KHZ_T},
	{5, 278*KHZ_T},
	{6, 266*KHZ_T},
	{7, 222*KHZ_T},
	{8, 177*KHZ_T},
	{9, 139*KHZ_T},
	{10, 133*KHZ_T},
	{11, 66*KHZ_T},
	{12, 60*KHZ_T},
	{13, CPUFREQ_TABLE_END},
};

static unsigned char transition_state_evt1b[][2] = {
	{1, 1},
	{2, 1},
	{3, 2},
	{4, 2},
	{5, 2},
	{6, 2},
	{7, 6},
	{8, 6},
	{9, 6},
	{10, 6},
	{11, 10},
	{12, 10},
	{12, 10},
};

/* frequency voltage matching table */
static const unsigned int frequency_match_evt1b[][4] = {
/* frequency, Mathced VDD ARM voltage , Matched VDD INT*/
	{833*KHZ_T, VOUT_1_35, VOUT_1_20, 0},
	{666*KHZ_T, VOUT_1_20, VOUT_1_20, 1},
	{533*KHZ_T, VOUT_1_20, VOUT_1_20, 2},
	{417*KHZ_T, VOUT_1_20, VOUT_1_20, 3},
	{333*KHZ_T, VOUT_1_10, VOUT_1_10, 4},
	{278*KHZ_T, VOUT_1_10, VOUT_1_20, 5},
	{266*KHZ_T, VOUT_1_10, VOUT_1_20, 6},
	{222*KHZ_T, VOUT_1_10, VOUT_1_20, 7},
	{177*KHZ_T, VOUT_1_05, VOUT_1_20, 8},
	{139*KHZ_T, VOUT_1_05, VOUT_1_20, 9},
	{133*KHZ_T, VOUT_1_05, VOUT_1_20, 10},
	{66*KHZ_T, VOUT_1_00, VOUT_1_00, 11},
	{60*KHZ_T, VOUT_1_00, VOUT_1_00, 12},
};


static struct cpufreq_frequency_table freq_table_evt1[] = {
	{0, 666*KHZ_T},
	{1, 333*KHZ_T},
	{2, 222*KHZ_T},
	{3, 133*KHZ_T},
	{4, 133*KHZ_T},
	{5, 66*KHZ_T},
	{6, CPUFREQ_TABLE_END},
};

static unsigned char transition_state_evt1[][2] = {
	{1, 0},
	{2, 0},
	{3, 1},
	{4, 1},
	{5, 2},
	{5, 3},
};

/* frequency voltage matching table */
static const unsigned int frequency_match_evt1[][4] = {
/* frequency, Mathced VDD ARM voltage , Matched VDD INT*/
	{666*KHZ_T, VOUT_1_20, VOUT_1_20, 0},
	{333*KHZ_T, VOUT_1_15, VOUT_1_20, 1}, 
	{222*KHZ_T, VOUT_1_15, VOUT_1_20, 2},
	{133*KHZ_T, VOUT_1_10, VOUT_1_20, 3},
	{133*KHZ_T, VOUT_1_10, VOUT_1_15, 4},
	{66*KHZ_T, VOUT_1_05, VOUT_1_15, 5},
};


static const unsigned int (*frequency_match[2])[4] = {
	frequency_match_evt1b,
	frequency_match_evt1,
};

static unsigned char (*transition_state[2])[2] = {
	transition_state_evt1b,
	transition_state_evt1,
};

static struct cpufreq_frequency_table *s5pc100_freq_table[] = {
	freq_table_evt1b,
	freq_table_evt1,
};

/* TODO: Add support for SDRAM timing changes */

#ifdef USE_DVS
static unsigned int s_arm_voltage, s_int_voltage;
int set_voltage(unsigned int freq_index)
{
	static int index = 0;
	unsigned int arm_voltage, int_voltage;
	
	if(index == freq_index)
		return 0;
		
	index = freq_index;
	
	arm_voltage = frequency_match[S5PC1XX_FREQ_TAB][index][1];
	int_voltage = frequency_match[S5PC1XX_FREQ_TAB][index][2];
	
	if(arm_voltage != s_arm_voltage) { 	
		set_pmic(VCC_ARM, arm_voltage);
		s_arm_voltage = arm_voltage;
	}
	if(int_voltage != s_int_voltage) {
		set_pmic(VCC_INT, int_voltage);
		s_int_voltage = int_voltage;
	}
	udelay(30);
	return 0;
}
#endif


// for active high with event from TS and key
static int dvfs_perf_lock = 0;
int dvfs_change_quick = 0;

void static sdvfs_lock(unsigned int *lock)
{
	while(*lock) {
		msleep(1);
	}
	*lock = 1;
}

void static sdvfs_unlock(unsigned int *lock)
{
	*lock = 0;
}

void set_dvfs_perf_level(void) 
{
	unsigned long irqflags;

	sdvfs_lock(&dvfs_perf_lock);
	spin_lock_irqsave(&g_cpufreq_lock, irqflags);

	if(s5pc1xx_cpufreq_index > S5PC1XX_MAXFREQLEVEL) {
		s5pc1xx_cpufreq_index = 1;
		dvfs_change_quick = 1;
	}

	spin_unlock_irqrestore(&g_cpufreq_lock, irqflags);
	sdvfs_unlock(&dvfs_perf_lock);

}
EXPORT_SYMBOL(set_dvfs_perf_level);

#if ENABLE_DVFS_LOCK_HIGH
void s5pc100_lock_dvfs_high_level(unsigned int nToken) 
{
	#if 1

	mutex_lock(&dvfs_high_lock);
	g_dvfs_high_lock_token |= nToken;
	set_dvfs_perf_level();
	mutex_unlock(&dvfs_high_lock);
	
	#endif	
}
EXPORT_SYMBOL(s5pc100_lock_dvfs_high_level);

void s5pc100_unlock_dvfs_high_level(unsigned int nToken) 
{
	#if 1

   mutex_lock(&dvfs_high_lock);
	g_dvfs_high_lock_token &= ~nToken;
	mutex_unlock(&dvfs_high_lock);

	#endif
}
EXPORT_SYMBOL(s5pc100_unlock_dvfs_high_level);
#endif //ENABLE_DVFS_LOCK_HIGH

unsigned int s5pc1xx_target_frq(unsigned int pred_freq, 
				int flag)
{
	int index;
	unsigned long irqflags;
	unsigned int freq;
//	unsigned int original_freq = 666*KHZ_T;

	struct cpufreq_frequency_table *freq_tab = s5pc100_freq_table[S5PC1XX_FREQ_TAB];


#if ENABLE_DVFS_LOCK_HIGH
	if(freq_tab[0].frequency < pred_freq){
	   index = 1;	
		//printk("WC DVFS2 s5pc1xx_target_frq- freq_index = %d, index = 0 \n", pred_freq);
	   goto s5pc1xx_target_frq_end;
	}
	else if(g_dvfs_high_lock_token){
	   index = 1;	
		//printk("WC DVFS2 s5pc1xx_target_frq- freq_index = %d, index = 1 \n", pred_freq);
	   goto s5pc1xx_target_frq_end;	
	}
#else
	if(freq_tab[0].frequency < pred_freq) {
	   index = 0;	
	   goto s5pc1xx_target_frq_end;
	}
#endif //ENABLE_DVFS_LOCK_HIGH


	if((flag != 1)&&(flag != -1)) {
		printk("s5pc1xx_target_frq: flag error!!!!!!!!!!!!!");
	}

	sdvfs_lock(&dvfs_perf_lock);
	index = s5pc1xx_cpufreq_index;

	if(freq_tab[index].frequency == pred_freq) {	
		if(flag == 1)
			index = transition_state[S5PC1XX_FREQ_TAB][index][1];
		else
			index = transition_state[S5PC1XX_FREQ_TAB][index][0];
	}
	else if(flag == -1) {
		index = 1;
	}
	else {
		index = 0; 
	}

s5pc1xx_target_frq_end:
	spin_lock_irqsave(&g_cpufreq_lock, irqflags);
	index = CLIP_LEVEL(index, s5pc1xx_cpufreq_level);
	s5pc1xx_cpufreq_index = index;
	spin_unlock_irqrestore(&g_cpufreq_lock, irqflags);
	
	freq = freq_tab[index].frequency;
	sdvfs_unlock(&dvfs_perf_lock);

	return freq;
}


int s5pc1xx_target_freq_index(unsigned int freq)
{
	int index = 0;
	unsigned long irqflags;
	
	struct cpufreq_frequency_table *freq_tab = s5pc100_freq_table[S5PC1XX_FREQ_TAB];

	if(freq >= freq_tab[index].frequency) {
		goto s5pc1xx_target_freq_index_end;
	}

	/*Index might have been calculated before calling this function.
	check and early return if it is already calculated*/
	if(freq_tab[s5pc1xx_cpufreq_index].frequency == freq) {		
		return s5pc1xx_cpufreq_index;
	}

	while((freq < freq_tab[index].frequency) &&
			(freq_tab[index].frequency != CPUFREQ_TABLE_END)) {
		index++;
	}

	if(index > 0) {
		if(freq != freq_tab[index].frequency) {
			index--;
		}
	}

	if(freq_tab[index].frequency == CPUFREQ_TABLE_END) {
		index--;
	}

s5pc1xx_target_freq_index_end:
	spin_lock_irqsave(&g_cpufreq_lock, irqflags);
	index = CLIP_LEVEL(index, s5pc1xx_cpufreq_level);
	s5pc1xx_cpufreq_index = index;
	spin_unlock_irqrestore(&g_cpufreq_lock, irqflags);
	
	return index; 
} 

int is_userspace_gov(void)
{
	int ret = 0;
	unsigned long irqflags;

	spin_lock_irqsave(&g_cpufreq_lock, irqflags);

	if(!strnicmp(cpufreq_governor_name, userspace_governor, CPUFREQ_NAME_LEN)) {
		ret = 1;
	}

	spin_unlock_irqrestore(&g_cpufreq_lock, irqflags);
	return ret;
}

int s5pc100_verify_speed(struct cpufreq_policy *policy)
{
#ifndef USE_FREQ_TABLE
	struct clk *mpu_clk;
#endif
	if (policy->cpu)
		return -EINVAL;
#ifdef USE_FREQ_TABLE
	return cpufreq_frequency_table_verify(policy, s5pc100_freq_table[S5PC1XX_FREQ_TAB]);
#else
	cpufreq_verify_within_limits(policy, policy->cpuinfo.min_freq,
				     policy->cpuinfo.max_freq);
	mpu_clk = clk_get(NULL, MPU_CLK);

	if (IS_ERR(mpu_clk))
		return PTR_ERR(mpu_clk);

	policy->min = clk_round_rate(mpu_clk, policy->min * KHZ_T) / KHZ_T;
	policy->max = clk_round_rate(mpu_clk, policy->max * KHZ_T) / KHZ_T;

	cpufreq_verify_within_limits(policy, policy->cpuinfo.min_freq,
				     policy->cpuinfo.max_freq);

	clk_put(mpu_clk);

	return 0;
#endif
}

unsigned int s5pc100_getspeed(unsigned int cpu)
{
	struct clk * mpu_clk;
	unsigned long rate;

	if (cpu)
		return 0;

	mpu_clk = clk_get(NULL, MPU_CLK);
	if (IS_ERR(mpu_clk))
		return 0;

	rate = clk_get_rate(mpu_clk) / KHZ_T;
	clk_put(mpu_clk);

	return rate;
}

int s5pc100_dvfs_lock_high_hclk(unsigned int dToken) {
	int temp;

	mutex_lock(&dvfs_high_hclk_lock);
	if ((dToken & 0xFF000000) != 0xE2000000) return -2; // invalid token
	temp = S5PC1XX_HCLKD1_HIGH_LOCK & (dToken & 0x00FFFFFF);
	if (temp) return -1; // already set

	S5PC1XX_HCLKD1_HIGH_LOCK |= temp;
	mutex_unlock(&dvfs_high_hclk_lock);
	return 0;
}

int s5pc100_dvfs_unlock_high_hclk(unsigned int dToken) {
	int temp;

	mutex_lock(&dvfs_high_hclk_lock);
	if ((dToken & 0xFF000000) != 0xE2000000) return -2; // invalid token
	temp = S5PC1XX_HCLKD1_HIGH_LOCK & (dToken & 0x00FFFFFF);
	if (!temp) return -1; // already unlock

	S5PC1XX_HCLKD1_HIGH_LOCK &= ~temp;
	mutex_unlock(&dvfs_high_hclk_lock);
	return 0;
}

static int s5pc100_target(struct cpufreq_policy *policy,
		       unsigned int target_freq,
		       unsigned int relation)
{
	struct clk * mpu_clk;
	struct cpufreq_freqs freqs;
	static int prevIndex = 0;
	int ret = 0;
	unsigned long arm_clk;
	unsigned int index;
	unsigned long irqflags;
	int d1Index;

	mpu_clk = clk_get(NULL, MPU_CLK);
	if (IS_ERR(mpu_clk))
		return PTR_ERR(mpu_clk);

	if(policy != NULL) {
		if(policy -> governor) {
			spin_lock_irqsave(&g_cpufreq_lock, irqflags);
			if (strnicmp(cpufreq_governor_name, policy->governor->name, CPUFREQ_NAME_LEN)) {
				strcpy(cpufreq_governor_name, policy->governor->name);
			}
			spin_unlock_irqrestore(&g_cpufreq_lock, irqflags);
		}
	}

	freqs.old = s5pc100_getspeed(0);
	if(freqs.old == s5pc100_freq_table[S5PC1XX_FREQ_TAB][0].frequency) {
		prevIndex = 0;
	}

	index = s5pc1xx_target_freq_index(target_freq);
	if(index == INDX_ERROR) {
		printk("s5pc100_target: INDX_ERROR \n");
		return -EINVAL;
	}

	if(prevIndex == index)
		return ret;		// no feq. change

	arm_clk = s5pc100_freq_table[S5PC1XX_FREQ_TAB][index].frequency;
	//printk("index=%d->%d, armclk=%d\n",prevIndex, index, (int)arm_clk/1000);
	freqs.new = arm_clk;
	freqs.cpu = 0;
	d1Index = index; // for hclkD1

	if(S5PC1XX_HCLKD1_HIGH_LOCK && index > S5PC1XX_MAXFREQLEVEL) {
		printk("HCLKLocked. Lockval=0x%08x\n",S5PC1XX_HCLKD1_HIGH_LOCK);
		d1Index = S5PC1XX_MAXFREQLEVEL;
	}

	target_freq = arm_clk;
	cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);

	spin_lock_irqsave(&g_cpufreq_lock, irqflags);
#ifdef USE_DVS
	if(prevIndex < index) { // clock down
		dvfs_change_direction = 0;
		/* frequency scaling */
		ret = s5pc1xx_clk_D1sync_change(d1Index);

		ret = clk_set_rate(mpu_clk, target_freq * KHZ_T);
		if(ret != 0) {
			printk("frequency scaling error\n");
			ret = -EINVAL;
			goto s5pc100_target_end;
		}

		/* voltage scaling */
		set_voltage(index);
		dvfs_change_direction = -1;
	}else{						// clock up
		dvfs_change_direction = 1;
		/* voltage scaling */
		set_voltage(index);

		/* frequency scaling */
		ret = clk_set_rate(mpu_clk, target_freq * KHZ_T);
		if(ret != 0) {
			printk("frequency scaling error\n");
			ret = -EINVAL;
			goto s5pc100_target_end;
		}
		ret = s5pc1xx_clk_D1sync_change(d1Index);
		dvfs_change_direction = -1;
	}

#else
	ret = clk_set_rate(mpu_clk, target_freq * KHZ_T);
	if(ret != 0) {
		printk("frequency scaling error\n");
		ret = -EINVAL;
		goto s5pc100_target_end;
	}

#endif
	spin_unlock_irqrestore(&g_cpufreq_lock, irqflags);

	cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);
	prevIndex = index; // save to preIndex
	clk_put(mpu_clk);
	return ret;
s5pc100_target_end:
	spin_unlock_irqrestore(&g_cpufreq_lock, irqflags);
	return ret;
}

int s5pc100_pm_target(unsigned int target_freq)
{
	struct clk * mpu_clk;
	int ret = 0;
	unsigned long arm_clk;
	unsigned int index;

	mpu_clk = clk_get(NULL, MPU_CLK);
	if(IS_ERR(mpu_clk))
		return PTR_ERR(mpu_clk);

	index = s5pc1xx_target_freq_index(target_freq);
	if(index == INDX_ERROR) {
	   printk("s5pc100_target: INDX_ERROR \n");
	   return -EINVAL;
	}
	
	arm_clk = s5pc100_freq_table[S5PC1XX_FREQ_TAB][index].frequency;
	target_freq = arm_clk;

#ifdef USE_DVS
	set_voltage(index);
#endif
	/* frequency scaling */
	ret = clk_set_rate(mpu_clk, target_freq * KHZ_T);
	if(ret != 0) {
		printk("frequency scaling error\n");
		return -EINVAL;
	}
	
	clk_put(mpu_clk);
	return ret;
}

#ifndef CONFIG_HAS_WAKELOCK
#ifdef CONFIG_ANDROID_POWER
// 2.6.25 code
void s5pc1xx_cpufreq_powersave(android_early_suspend_t *h)
{
	unsigned long irqflags;
	spin_lock_irqsave(&g_cpufreq_lock, irqflags);
	s5pc1xx_cpufreq_level = S5PC1XX_MAXFREQLEVEL + 2;
	spin_unlock_irqrestore(&g_cpufreq_lock, irqflags);
	return;
}

void s5pc1xx_cpufreq_performance(android_early_suspend_t *h)
{
	unsigned long irqflags;
	if(!is_userspace_gov()) {
		spin_lock_irqsave(&g_cpufreq_lock, irqflags);
		s5pc1xx_cpufreq_level = S5PC1XX_MAXFREQLEVEL;
		s5pc1xx_cpufreq_index = CLIP_LEVEL(s5pc1xx_cpufreq_index, S5PC1XX_MAXFREQLEVEL);
		spin_unlock_irqrestore(&g_cpufreq_lock, irqflags);
		s5pc100_target(NULL, s5pc100_freq_table[S5PC1XX_FREQ_TAB][s5pc1xx_cpufreq_index].frequency, 1);
	}
	else {
		spin_lock_irqsave(&g_cpufreq_lock, irqflags);
		s5pc1xx_cpufreq_level = S5PC1XX_MAXFREQLEVEL;
		spin_unlock_irqrestore(&g_cpufreq_lock, irqflags);
#ifdef USE_DVS
		set_voltage(s5pc1xx_cpufreq_index);
#endif
	}
	return;
}

static struct android_early_suspend s5pc1xx_freq_suspend = {
	.suspend = s5pc1xx_cpufreq_powersave,
	.resume = s5pc1xx_cpufreq_performance,
	.level = ANDROID_EARLY_SUSPEND_LEVEL_DISABLE_FB + 1,
};

#endif  //CONFIG_ANDROID_POWER
#else  //CONFIG_HAS_WAKELOCK
 // 2.6.27 code 
void s5pc1xx_cpufreq_powersave(struct early_suspend *h)
{
	unsigned long irqflags;
	spin_lock_irqsave(&g_cpufreq_lock, irqflags);
	s5pc1xx_cpufreq_level = S5PC1XX_MAXFREQLEVEL + 2;
	spin_unlock_irqrestore(&g_cpufreq_lock, irqflags);
	return;
}

void s5pc1xx_cpufreq_performance(struct early_suspend *h)
{
	unsigned long irqflags;
	if(!is_userspace_gov()) {		//	if(is_conservative_gov()) {
		spin_lock_irqsave(&g_cpufreq_lock, irqflags);
		s5pc1xx_cpufreq_level = S5PC1XX_MAXFREQLEVEL;
		s5pc1xx_cpufreq_index = CLIP_LEVEL(s5pc1xx_cpufreq_index, S5PC1XX_MAXFREQLEVEL);
		spin_unlock_irqrestore(&g_cpufreq_lock, irqflags);
		s5pc100_target(NULL, s5pc100_freq_table[S5PC1XX_FREQ_TAB][s5pc1xx_cpufreq_index].frequency, 1);
	}
	else {
		spin_lock_irqsave(&g_cpufreq_lock, irqflags);
		s5pc1xx_cpufreq_level = S5PC1XX_MAXFREQLEVEL;
		spin_unlock_irqrestore(&g_cpufreq_lock, irqflags);
#ifdef USE_DVS
		set_voltage(s5pc1xx_cpufreq_index);
#endif
	}
	return;
}

static struct early_suspend s5pc1xx_freq_suspend = {
	.suspend = s5pc1xx_cpufreq_powersave,
	.resume = s5pc1xx_cpufreq_performance,
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 1,
};
#endif //CONFIG_HAS_WAKELOCK

#if 1  // add by sryoo 2009.05.27
unsigned int get_min_cpufreq(void)
{
	unsigned int frequency;
	
	frequency = frequency_match_evt1[S5PC1XX_MAXFREQLEVEL][0];

	return frequency;
}
#endif

static int __init s5pc100_cpu_init(struct cpufreq_policy *policy)
{
	struct clk * mpu_clk;
	u32 reg;
	unsigned long irqflags;

#ifdef CLK_OUT_PROBING
	
	reg = __raw_readl(S5P_CLK_OUT);
	reg &=~(0x1f << 12 | 0xf << 20);	// Mask Out CLKSEL bit field and DIVVAL
	reg |= (0x9 << 12 | 0x1 << 20);	// CLKSEL = ARMCLK/4, DIVVAL = 1 
	__raw_writel(reg, S5P_CLK_OUT);
#endif
	mpu_clk = clk_get(NULL, MPU_CLK);
	if (IS_ERR(mpu_clk))
		return PTR_ERR(mpu_clk);

	if (policy->cpu != 0)
		return -EINVAL;
	policy->cur = policy->min = policy->max = s5pc100_getspeed(0);

	spin_lock_irqsave(&g_cpufreq_lock, irqflags);
		S5PC1XX_FREQ_TAB = 1;			//todo: set and fix
//		S5PC1XX_MAXFREQLEVEL = 3;		// same as above

#if 1 //add by spring.ryoo
	if(policy->max == MAXIMUM_FREQ) {
		S5PC1XX_FREQ_TAB = 1;			//todo: set and fix
		S5PC1XX_MAXFREQLEVEL = 3;		// same as above
	}
	else {
		S5PC1XX_FREQ_TAB = 1;
		S5PC1XX_MAXFREQLEVEL = 2;	
	}
#else
/*
	if(policy->max == MAXIMUM_FREQ) {
		S5PC1XX_FREQ_TAB = 1;			//todo: set and fix
		S5PC1XX_MAXFREQLEVEL = 3;		// same as above
	}
	else {
		S5PC1XX_FREQ_TAB = 1;
		S5PC1XX_MAXFREQLEVEL = 2;	
	}
*/
#endif

	s5pc1xx_cpufreq_level = S5PC1XX_MAXFREQLEVEL;
	spin_unlock_irqrestore(&g_cpufreq_lock, irqflags);

#ifdef USE_FREQ_TABLE
	cpufreq_frequency_table_get_attr(s5pc100_freq_table[S5PC1XX_FREQ_TAB], policy->cpu);
#else
	policy->cpuinfo.min_freq = clk_round_rate(mpu_clk, 0) / KHZ_T;
	policy->cpuinfo.max_freq = clk_round_rate(mpu_clk, VERY_HI_RATE) / KHZ_T;
#endif
	policy->cpuinfo.transition_latency = DVFS_SAMPLING_RATE;	//usec


	clk_put(mpu_clk);

#ifdef USE_DVS
	s_arm_voltage = frequency_match[S5PC1XX_FREQ_TAB][0][1];
	s_int_voltage = frequency_match[S5PC1XX_FREQ_TAB][0][2];
#if defined(CONFIG_MACH_SMDKC100)
	ltc3714_init(s_arm_voltage, s_int_voltage);
#endif
#endif

#ifndef CONFIG_HAS_WAKELOCK
#ifdef CONFIG_ANDROID_POWER
	android_register_early_suspend(&s5pc1xx_freq_suspend);
#endif
#else
	register_early_suspend(&s5pc1xx_freq_suspend);
#endif

#ifdef USE_FREQ_TABLE
	return cpufreq_frequency_table_cpuinfo(policy, s5pc100_freq_table[S5PC1XX_FREQ_TAB]);
#else
	return 0;
#endif
}

static struct cpufreq_driver s5pc100_driver = {
	.flags		= CPUFREQ_STICKY,
	.verify		= s5pc100_verify_speed,
	.target		= s5pc100_target,
	.get		= s5pc100_getspeed,
	.init		= s5pc100_cpu_init,
	.name		= "s5pc100",
};

static int __init s5pc100_cpufreq_init(void)
{
	return cpufreq_register_driver(&s5pc100_driver);
}

arch_initcall(s5pc100_cpufreq_init);
