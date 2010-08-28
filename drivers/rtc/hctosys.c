/*
 * RTC subsystem, initialize system time on startup
 *
 * Copyright (C) 2005 Tower Technologies
 * Author: Alessandro Zummo <a.zummo@towertech.it>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/rtc.h>

/* IMPORTANT: the RTC only stores whole seconds. It is arbitrary
 * whether it stores the most close value or the value with partial
 * seconds truncated. However, it is important that we use it to store
 * the truncated value. This is because otherwise it is necessary,
 * in an rtc sync function, to read both xtime.tv_sec and
 * xtime.tv_nsec. On some processors (i.e. ARM), an atomic read
 * of >32bits is not possible. So storing the most close value would
 * slow down the sync API. So here we have the truncated value and
 * the best guess is to add 0.5s.
 */

static int __init rtc_hctosys(void)
{
	int err, rtc_init = 0;
	struct rtc_time tm;
	struct rtc_device *rtc = rtc_class_open(CONFIG_RTC_HCTOSYS_DEVICE);

	if (rtc == NULL) {
		printk("%s: unable to open rtc device (%s)\n",
			__FILE__, CONFIG_RTC_HCTOSYS_DEVICE);
		return -ENODEV;
	}

	err = rtc_read_time(rtc, &tm);
	
	#if 1	// charles debug
		printk("============== RTC Debug Param =================\n");
		printk("tm_year = %d\n", tm.tm_year);	printk("tm_mon  = %d\n", tm.tm_mon);
		printk("tm_mday = %d\n", tm.tm_mday);	printk("tm_hour = %d\n", tm.tm_hour);
		printk("tm_min  = %d\n", tm.tm_min);	printk("tm_sec  = %d\n", tm.tm_sec);
		printk("================================================\n");
	#endif

	// android system time valid check, 
	// rtc read time is invalid, set to default system time 2010, jan, 1, 00:00:00
	if((tm.tm_year+1900)<=1970 || (tm.tm_year+1900)>2037) {
		printk("%s[%d]: invalid system time -> set to default system time.. \n", __FILE__,__LINE__);
		rtc_init = 1;
	}
	
	if (err == 0) {

		err = rtc_valid_tm(&tm);

		if(err || rtc_init)	{
			// RTC default value setting (2010-01-01, 0:0:0)
			tm.tm_year = 110;	tm.tm_mon = 0;		tm.tm_mday = 1;
			tm.tm_hour = 0;		tm.tm_min = 0;		tm.tm_sec = 0;

			err = rtc_valid_tm(&tm);

			if(err == 0)	{
				printk("============== RTC set to default system time =================\n");
				printk("tm_year = %d\n", tm.tm_year);		printk("tm_mon  = %d\n", tm.tm_mon);
				printk("tm_mday = %d\n", tm.tm_mday);		printk("tm_hour = %d\n", tm.tm_hour);
				printk("tm_min  = %d\n", tm.tm_min);		printk("tm_sec  = %d\n", tm.tm_sec);
				printk("===============================================================\n");
				rtc_set_time(rtc, &tm);
			}
		}

		if (err == 0) {
			struct timespec tv;

			tv.tv_nsec = NSEC_PER_SEC >> 1;

			rtc_tm_to_time(&tm, &tv.tv_sec);

			do_settimeofday(&tv);

			dev_info(rtc->dev.parent,
				"setting system clock to "
				"%d-%02d-%02d %02d:%02d:%02d UTC (%u)\n",
				tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
				tm.tm_hour, tm.tm_min, tm.tm_sec,
				(unsigned int) tv.tv_sec);
		}
		else
			dev_err(rtc->dev.parent,
				"hctosys: invalid date/time\n");
	}
	else
		dev_err(rtc->dev.parent,
			"hctosys: unable to read the hardware clock\n");

	rtc_class_close(rtc);

	return 0;
}

late_initcall(rtc_hctosys);
