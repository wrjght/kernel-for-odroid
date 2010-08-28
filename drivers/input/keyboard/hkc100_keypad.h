//[*]--------------------------------------------------------------------------------------------------[*]
/*
 *	
 * HKC100 Dev Board key-pad header file (charles.park) 
 *
 */
//[*]--------------------------------------------------------------------------------------------------[*]

#ifndef	_HKC100_KEYPAD_H_
#define	_HKC100_KEYPAD_H_

	#define TRUE 				1
	#define FALSE 				0
	
	#define	KEY_PRESS			1
	#define	KEY_RELEASE			0
	
	#define	KEYPAD_STATE_BOOT	0
	#define	KEYPAD_STATE_RESUME	1
	
	#define	SPDIF_ONOFF_CONTROL		// HDMI SPDIF control
	//#define	HDMI_POWER_CONTROL		// HDMI Power On/Off control flag
	#define		SLIDE_VOLUME_LOCK		// Slide Volume on/off control
	
	#if defined(CONFIG_HAS_WAKELOCK)
//		#define		COMBO_MODULE_WAKELOCK	// wake lock
	#endif

	// keypad sampling rate
	#define	PERIOD_10MS			(HZ/100)	// 10ms
	#define	PERIOD_20MS			(HZ/50)		// 20ms
	#define	PERIOD_50MS			(HZ/20)		// 50ms

	// power off timer	
	#define	PERIOD_1SEC			(1*HZ)		// 1sec
	#define	PERIOD_3SEC			(3*HZ)		// 3sec
	#define	PERIOD_5SEC			(5*HZ)		// 5sec

	// Keypad wake up delay
	#define	KEYPAD_WAKEUP_DELAY		100		// 1 sec delay
	
	//
	// GPH2, GPH3 Port define
	//
	#define	GPH2CON				(*(unsigned long *)S5PC1XX_GPH2CON)
	#define	GPH2DAT				(*(unsigned long *)S5PC1XX_GPH2DAT)
	#define	GPH2PUD				(*(unsigned long *)S5PC1XX_GPH2PUD)
	
	#define	GPH3CON				(*(unsigned long *)S5PC1XX_GPH3CON)
	#define	GPH3DAT				(*(unsigned long *)S5PC1XX_GPH3DAT)
	#define	GPH3PUD				(*(unsigned long *)S5PC1XX_GPH3PUD)

	// Key Port Init	
	#define	KEY_PORT_INIT()		{	GPH2CON = 0x00000000;	GPH2PUD = 0x0000AAAA;	GPH3CON = 0x00000000;	GPH3PUD = 0x00006AAA;	}

	// Keypad data read function
	#define	GET_KEYPAD_DATA()	((((~GPH3DAT) <<8) & 0x7F00) | ((~GPH2DAT) & 0x00FF))		// reverse control
	
	// HIGH = GAME MODE, LOW = NORMAL MODE
	#define	GET_KEYPAD_MODE()	(GPH3DAT & 0x80)
	
	//
	// GPA1 Port Define (Touch Sensor LED Control)
	//
	#define	GPA1CON				(*(unsigned long *)S5PC1XX_GPA1CON)
	#define	GPA1DAT				(*(unsigned long *)S5PC1XX_GPA1DAT)
	#define	GPA1PUD				(*(unsigned long *)S5PC1XX_GPA1PUD)
	
	#define	ON					1
	#define	OFF					0
	
	#define	LED_PORT_INIT()		{	GPA1CON &= (~0x00000FFF);	GPA1CON |= 0x00000111;	GPA1PUD &= (~0x0000003F);	GPA1PUD |= 0x0000002A;	} 
	#define	LED1_ONOFF(x)		{	x ? (GPA1DAT &= (~0x00000001)) : (GPA1DAT |= 0x00000001);	}
	#define	LED2_ONOFF(x)		{	x ? (GPA1DAT &= (~0x00000002)) : (GPA1DAT |= 0x00000002);	}
	#define	POWER_LED_OFF()		{	GPA1DAT |= 0x00000004;		}

	//
	// GPH.0 Port Define(System Power & HDMI Cable Detect)
	//	
	#define	GPH0CON				(*(unsigned long *)S5PC1XX_GPH0CON)
	#define	GPH0DAT				(*(unsigned long *)S5PC1XX_GPH0DAT)
	#define	GPH0PUD				(*(unsigned long *)S5PC1XX_GPH0PUD)
	
	//
	// Power Off Key(GPH0.6)
	//
	#define	POWER_OFF_PORT_INIT()	{	GPH0CON &= (~0x0F000000);	GPH0DAT |= 0x00000040;	GPH0CON |= 0x01000000;	GPH0PUD &= (~0x00003000);	GPH0PUD |= 0x00002000;	}		// pull-up, output
	#define	POWER_OFF()				{	GPH0DAT &= (~0x00000040);	}
	
	//
	// HDMI Detect Check Port(GPH0.2) : Port Initialize bootloader
	//	
	// #define	HDMI_STATUS_PORT_INIT()	{	GPH0CON &= (~0x00000F00);	GPH0PUD &= (~0x00000030);	GPH0PUD |= 0x00000020;	}	// Pull-up, Input
	#define	GET_HDMI_STATUS()		(	GPH0DAT & 0x00000004	)
	
	//
	// HDMI Detect Event (Switch event used)
	//
	#define	SW_HDMI				0x06
	
	//
	// Key Hold event(Switch event used)
	//
	#define	SW_KEY_HOLD			0x07
	#define	SW_SCREEN_ROTATE	0x08

	//
	// GPH.1 Port Define(HDMI Power Control Port)
	//	
	#define	GPH1CON				(*(unsigned long *)S5PC1XX_GPH1CON)
	#define	GPH1DAT				(*(unsigned long *)S5PC1XX_GPH1DAT)
	#define	GPH1PUD				(*(unsigned long *)S5PC1XX_GPH1PUD)
	
	//
	// HDMI Power Control(GPH1.5)
	//	
	#define	HDMI_POWER_PORT_INIT()	{	GPH1CON &= (~0x00F00000);	GPH1CON |= 0x00100000;	GPH1PUD &= (~0x00000C00);	GPH1PUD |= 0x00000800;	}	// pull-up, output
	#define	HDMI_POWER_ONOFF(x)		{	x ? (GPH1DAT |= 0x00000020) : (GPH1DAT &= (~0x00000020));	}
	
	typedef	struct	hkc100_keypad__t	{
		
		// keypad control
		struct input_dev	*driver;			// input driver
		struct timer_list 	rd_timer;			// keyscan timer

		// power off control
		struct timer_list	poweroff_timer;		// long power key process
		unsigned char		poweroff_flag;		// power key press flag

		// HDMI Detect
		unsigned char		hdmi_state;			// cur hdmi connect state

		// slide sw state
		unsigned char		slide_sw_state;		// hold on/off state
		
		// sysfs used		
		unsigned char		hold_state;			// key hold off
		unsigned char		sampling_rate;		// 10 msec sampling
		unsigned char		led_state;			// sleep off
		unsigned char		poweroff_time;		// reset off
		
		unsigned int		wakeup_delay;		// key wakeup delay
	}	hkc100_keypad_t;
	
	extern	hkc100_keypad_t	hkc100_keypad;
#endif		/* _HKC100_KEYPAD_H_*/
