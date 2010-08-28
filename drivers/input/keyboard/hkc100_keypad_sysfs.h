//[*]--------------------------------------------------------------------------------------------------[*]
/*
 *	
 * S5PC100 _HKC100_KEYPAD_SYSFS_H_ Header file(charles.park) 
 *
 */
//[*]--------------------------------------------------------------------------------------------------[*]

#ifndef	_HKC100_KEYPAD_SYSFS_H_
#define	_HKC100_KEYPAD_SYSFS_H_

extern	void 	combo_module_suspend			(void);

extern	void 	combo_module_control			(void);
extern	int		hkc100_keypad_sysfs_create		(struct platform_device *pdev);
extern	void	hkc100_keypad_sysfs_remove		(struct platform_device *pdev);

//[*]--------------------------------------------------------------------------------------------------[*]
#endif	// _HKC100_KEYPAD_SYSFS_H_
//[*]--------------------------------------------------------------------------------------------------[*]

