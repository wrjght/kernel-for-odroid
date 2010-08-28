//[*]--------------------------------------------------------------------------------------------------[*]
/*
 *	
 * HKC100 Dev Board key-pad header file (charles.park) 
 *
 */
//[*]--------------------------------------------------------------------------------------------------[*]

#ifndef	_HKC100_KEYCODE_H_
#define	_HKC100_KEYCODE_H_

	//
	// Aesop S5PC1XX Key PAD Layout(top view)
	//
	//	+-----------------------------------------------------------+
	//	|				KEY_E(VOL_DN)		KEY_F(VOL_UP)			|
	//	+-----------------------------------------------------------+
	//	|															|
	//	|			KEY_4(LEFT)										|
	//	|	KEY1(DOWN)		KEY_3(UP)								|
	//	|			KEY_2(RIGHT)									|
	//	|										KEY_C(X)			|
	//	|								KEY_D(Y)		KEY_B(A)	|
	//	|										KEY_A(B)			|
	//	|			KEY_7(HOME)										|
	//	|	KEY_8(ENTER)	KEY_6(MENU)								|
	//	|			KEY_5(BACK)										|
	//	|															|
	//	+-----------------------------------------------------------+
	//	|								KEY_G(PWR_OFF)	KEY_H(MODE)	|
	//	+-----------------------------------------------------------+
	//
	//	Define key-matrix
	//	int HKC100_Keycode[MAX_KEYPAD_CNT] = {
	//		KEY_1,	KEY_2,	KEY_3,	KEY_4,
	//		KEY_5,	KEY_6,	KEY_7,	KEY_8,
	//		KEY_A,	KEY_B,	KEY_C,	KEY_D,
	//		KEY_E,	KEY_F,	KEY_G,	KEY_H
	//	};
	
	#define	MAX_KEYCODE_CNT		21
	
	int HKC100_Keycode[MAX_KEYCODE_CNT] = {
		KEY_DOWN,		KEY_RIGHT,		KEY_LEFT,		KEY_UP,
		KEY_BACK,		KEY_MENU,		KEY_HOME,		KEY_ENTER,
		KEY_B,			KEY_A,			KEY_X,			KEY_Y,
		KEY_K,			KEY_M,			KEY_H,			KEY_O,
		KEY_VOLUMEDOWN,	KEY_VOLUMEUP,	KEY_POWER,		KEY_L,			KEY_R
	};

	#define	MAX_KEYPAD_CNT		15
	
	const	int	HKC100_KeyMap[MAX_KEYPAD_CNT] = {
		KEY_DOWN,		KEY_RIGHT,		KEY_LEFT,		KEY_UP,
		KEY_BACK,		KEY_MENU,		KEY_HOME,		KEY_ENTER,
		KEY_B,			KEY_A,			KEY_X,			KEY_Y,
		KEY_VOLUMEDOWN,	KEY_VOLUMEUP,	KEY_POWER
	};

	#if	defined(DEBUG_MSG)
		const char HKC100_KeyMapStr[MAX_KEYPAD_CNT][20] = {
			"KEY_DOWN\n",		"KEY_RIGHT\n",		"KEY_LEFT\n",	"KEY_UP\n",
			"KEY_BACK\n",		"KEY_MENU\n",		"KEY_HOME\n",	"KEY_ENTER\n",
			"KEY_B\n",			"KEY_A\n",			"KEY_X\n",		"KEY_Y\n",
			"KEY_VOLUMEDOWN\n",	"KEY_VOLUMEUP\n",	"KEY_POWER\n",	
		};
	#endif	// DEBUG_MSG

#endif		/* _HKC100_KEYPAD_H_*/
