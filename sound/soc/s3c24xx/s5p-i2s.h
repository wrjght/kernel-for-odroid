/* sound/soc/s3c24xx/s5p-i2s.h
 *
 * ALSA SoC Audio Layer - S3C64XX I2S driver
 *
 * Copyright 2008 Openmoko, Inc.
 * Copyright 2008 Simtec Electronics
 *      Ben Dooks <ben@simtec.co.uk>
 *      http://armlinux.simtec.co.uk/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_S5P_I2S_H
#define __SND_SOC_S5P_I2S_H __FILE__

struct clk;

#include "odroid-i2s-bus1.h"

#define S5P_DIV_BCLK	S3C_I2SV2_DIV_BCLK
#define S5P_DIV_RCLK	S3C_I2SV2_DIV_RCLK
#define S5P_DIV_PRESCALER	S3C_I2SV2_DIV_PRESCALER

#define S5P_CLKSRC_PCLK	(0)
#define S5P_CLKSRC_MUX	(1)
#define S5P_CLKSRC_CDCLK    (2)

extern struct snd_soc_dai s5p_i2s_dai;
extern bool g_spdif_out;
extern bool g_probe_done;

extern struct clk *s5p_i2s_get_clock(struct snd_soc_dai *dai);

#endif /* __SND_SOC_S5P_I2S_H */
