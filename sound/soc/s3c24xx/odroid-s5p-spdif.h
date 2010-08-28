/*
 * s5pc1xx-spdif.h  --  ALSA Soc Audio Layer
 *
 * Copyright 2005 Wolfson Microelectronics PLC.
 * Author: Graeme Gregory
 *         graeme.gregory@wolfsonmicro.com or linux@wolfsonmicro.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  Revision history
 *    1th April 2009   Initial version.
 */

#ifndef __SND_SOC_ODROID_S5P_SPDIF_H
#define __SND_SOC_ODROID_S5P_SPDIF_H __FILE__

int 	s5pc1xx_spdif_probe(struct platform_device *pdev, struct snd_soc_dai *dai);
u32 	s5pc1xx_spdif_get_clockrate(void);
void 	s5pc1xx_spdif_txctrl(int on);
u32		s5pc1xx_spdif_get_clockrate(void);
void 	s5pc1xx_spdif_shutdown(struct snd_pcm_substream *substream, struct snd_soc_dai *socdai);
int 	s5pc1xx_spdif_prepare(struct snd_pcm_substream *substream, struct snd_soc_dai *socdai);
int 	s5pc1xx_spdif_hw_params(struct snd_pcm_substream *substream, 
						struct snd_pcm_hw_params *params,
						struct snd_soc_dai *socdai);

int 	s5pc1xx_spdif_power_off(void);


irqreturn_t s5pc1xx_spdif_irq(int irqno, void *dev_id);

#endif /*__SND_SOC_ODROID_S5P_SPDIF_H*/

