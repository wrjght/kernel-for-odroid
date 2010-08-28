/*
 *  s5p-spdif.h
 *
 *  Copyright (C) 2009 Samsung Electronics Co.Ltd
 *  Copyright (C) 2009 Kwak Hyun Min <hyunmin.kwak@samsung.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *
 *  Revision history
 *    1th April 2009   Initial version.
 */

#ifndef __S5P_SPDIF_H
#define __S5P_SPDIF_H __FILE__

#include <linux/clk.h>

extern struct snd_soc_dai s5p_spdif_dai;

struct s5p_spdif_info {
	void __iomem	*regs;
	struct clk	*spdif_clk;
	struct clk	*spdif_sclk_src;
	struct clk	*spdif_sclk_audio0;
	
#ifdef CONFIG_CPU_S5PC100	
	struct clk	*spdif_sclk;
#endif	
	
	int 		master;
};
extern struct s5p_spdif_info s5p_spdif;

enum spdif_intr_src
{
	SPDIF_INT_FIFOLVL,
	SPDIF_INT_USERDATA,
	SPDIF_INT_BUFEMPTY,
	SPDIF_INT_STREAMEND,
	SPDIF_INT_UNKNOWN
};

#endif /* __S5P_SPDIF_H */

