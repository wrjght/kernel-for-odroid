/* linux/drivers/input/keyboard/s3c-keypad.h 
 *
 * Driver header for Samsung SoC keypad.
 *
 * Kim Kyoungil, Copyright (c) 2006-2009 Samsung Electronics
 *      http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#ifndef _S3C_KEYPAD_H_
#define _S3C_KEYPAD_H_

static void __iomem *key_base;

#ifdef CONFIG_WAKELOCK
#define GET_KEYCODE(x)                  (x+1)
#endif /* CONFIG_WAKELOCK */

#define KEYPAD_COLUMNS	8
#define KEYPAD_ROWS	8
#define MAX_KEYPAD_NR	64	/* 8*8 */
#define MAX_KEYMASK_NR	32
#define KEYCODE_FOCUS	250
#define KEYCODE_ENDCALL	249
#define KEYCODE_HOME	251

#if defined(CONFIG_CPU_S3C6410)
int keypad_keycode[] = {
		1, 2, 3, 4, 5, 6, 7, 8,
		9, 10, 11, 12, 13, 14, 15, 16,
		17, 18, 19, 20, 21, 22, 23, 24,
		KEY_UP, KEY_MENU, 27, 28, 29, 30, 31, 32,
		KEY_ENTER, 231, 35, 36, 37, 38, 39, 40,
		KEY_DOWN, 249, 43, 44, 45, 46, 47, 48,
		KEY_RIGHT, KEY_HOME, 51, 52, 53, 54, 55, 56,
		KEY_LEFT, KEY_BACK, 59, 60, 61, 62, 63, 64
};
#elif defined(CONFIG_CPU_S5PC100)
#ifdef CONFIG_ANDROID
int keypad_keycode[] = {
		1,2,3,4,5,6,7,8,
		9,10,11,12,13,14,15,16,
		17,18,19,20,21,22,23,24,
		25,26,27,28,29,30,31,32,
		33,34,35,36,37,38,39,40,
		41,42,43,44,45,46,47,48,
		49,50,51,52,53,54,55,56,
		57,58,59,60,61,62,63,64
	};
#endif
#endif

#ifdef CONFIG_WAKELOCK
#define KEYPAD_ROW_GPIOCON      S5PC1XX_GPH3CON
#define KEYPAD_ROW_GPIOPUD      S5PC1XX_GPH3PUD
#define KEYPAD_COL_GPIOCON      S5PC1XX_GPH2CON
#define KEYPAD_COL_GPIOPUD      S5PC1XX_GPH2PUD
#else
#if CONFIG_ANDROID
#ifdef CONFIG_CPU_S3C6410
#define KEYPAD_ROW_GPIOCON      S3C64XX_GPKCON1
#define KEYPAD_ROW_GPIOPUD      S3C64XX_GPKPUD
#define KEYPAD_COL_GPIOCON      S3C64XX_GPLCON
#define KEYPAD_COL_GPIOPUD      S3C64XX_GPLPUD
#elif CONFIG_CPU_S5PC100
#define KEYPAD_ROW_GPIOCON      S5PC1XX_GPH3CON
#define KEYPAD_ROW_GPIOPUD      S5PC1XX_GPH3PUD
#define KEYPAD_COL_GPIOCON      S5PC1XX_GPH2CON
#define KEYPAD_COL_GPIOPUD      S5PC1XX_GPH2PUD
#endif
#endif /* CONFIG_ANDROID */
#endif /* CONFIG_WAKELOCK */ 

#ifdef CONFIG_CPU_S3C6410
#define KEYPAD_DELAY		(50)
#elif defined(CONFIG_CPU_S5PC100)
#define KEYPAD_DELAY		(600)
#elif defined(CONFIG_CPU_S5PC110)
#define KEYPAD_DELAY		(900)
#elif defined(CONFIG_CPU_S5P6442)
#define KEYPAD_DELAY		(50)
#endif

#define	KEYIFCOL_CLEAR		(readl(key_base+S3C_KEYIFCOL) & ~0xffff)
#define	KEYIFCON_CLEAR		(readl(key_base+S3C_KEYIFCON) & ~0x1f)
#define KEYIFFC_DIV		(readl(key_base+S3C_KEYIFFC) | 0x1)
#if defined(CONFIG_MACH_SMDK6410)
struct s3c_keypad_slide
{
	int     eint;
	int     gpio;
	int     gpio_af;
	int     state_upset;
};

struct s3c_keypad_gpio_key
{
	int     eint;
	int     gpio;
	int     gpio_af;
	int     keycode;
	int     state_upset;
};

struct s3c_keypad_extra 
{
	int 				board_num;
	struct s3c_keypad_slide		*slide;
	struct s3c_keypad_gpio_key	*gpio_key;
	int				gpio_key_num;
	int				wakeup_by_keypad;
};
#endif

struct s3c_keypad {
    struct input_dev *dev;
	int nr_rows;	
	int no_cols;
	int total_keys; 
	int keycodes[MAX_KEYPAD_NR];
#if defined(CONFIG_MACH_SMDK6410)	
	struct s3c_keypad_extra *extra;
#endif	
};

#if defined(CONFIG_MACH_SMDK6410)
struct s3c_keypad_slide slide_smdk6410 = {IRQ_EINT(11), S3C64XX_GPN(11), 2, 1};

struct s3c_keypad_gpio_key gpio_key_smdk6410[] = {
	{IRQ_EINT(10),  S3C64XX_GPN(10),   2,      KEY_POWER, 1},
	{IRQ_EINT(9),  S3C64XX_GPN(9),   2,      KEY_HOME, 1},
};

struct s3c_keypad_extra s3c_keypad_extra[] = {
	{0x0000, &slide_smdk6410, &gpio_key_smdk6410[0], 2, 0},
};
#endif

#if defined(CONFIG_MACH_SMDK6410)
extern void s3c_setup_keypad_cfg_gpio(void);
#elif defined(CONFIG_MACH_SMDKC100)
extern void s3c_setup_keypad_cfg_gpio(int rows, int columns);
#endif

#endif				/* _S3C_KEYIF_H_ */
