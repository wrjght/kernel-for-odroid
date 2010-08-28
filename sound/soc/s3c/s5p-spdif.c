/*
 *  s5p-spdif.c  
 *
 *  Copyright (C) 2009 Samsung Electronics Co.Ltd
 *  Copyright (C) 2009 Kwak Hyun Min <hyunmin.kwak@samsung.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  Revision history
 *    1th April 2009   Initial version.
 */

#include <linux/delay.h>
#include <linux/interrupt.h>
 
#include <sound/soc.h>
 
#include <asm/io.h>

#include <plat/regs-spdif.h>
 
#include <mach/dma.h>

#include "s5p-spdif.h"
#include "s3c-pcm-lp.h"
 
#ifdef CONFIG_SND_DEBUG
#define s3cdbg(x...) printk(x)
#else
#define s3cdbg(x...)
#endif

 
static struct s3c2410_dma_client s5p_spdif_dma_client_out = {
	.name = "SPDIF out"
};


static struct s5p_pcm_dma_params s5p_spdif_pcm_stereo_out = {
	.client	 = &s5p_spdif_dma_client_out,
	.channel = DMACH_SPDIF_OUT,
	.dma_addr = S5P_PA_SPDIF + S5P_SPDIF_SPDDAT,
	.dma_size = 2,
};


static void s5p_spdif_snd_txctrl(int on)
{

	s3cdbg("Entered %s: on=%d \n", __FUNCTION__, on);
	
	if (on) {
		writel(S5P_SPDIF_SPDCLKCON_SPDIFOUT_POWER_OFF, 
			s5p_spdif.regs + S5P_SPDIF_SPDCLKCON);
		writel(S5P_SPDIF_SPDCLKCON_MAIN_AUDIO_CLK_INT | 
			S5P_SPDIF_SPDCLKCON_SPDIFOUT_POWER_ON, 
			s5p_spdif.regs + S5P_SPDIF_SPDCLKCON);	 // SPDIFOUT clock power on
	} else {
		writel(S5P_SPDIF_SPDCON_SOFTWARE_RESET_EN, 
			s5p_spdif.regs + S5P_SPDIF_SPDCON);	 // Sw Reset
		mdelay(100);
		writel(S5P_SPDIF_SPDCLKCON_SPDIFOUT_POWER_OFF, 
			s5p_spdif.regs + S5P_SPDIF_SPDCLKCON);	 // SPDIFOUT clock power off
	}
}


static int s5p_spdif_hw_params(struct snd_pcm_substream *substream,
			       struct snd_pcm_hw_params *params,
			       struct snd_soc_dai *dai )
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int sampling_freq;

	s3cdbg("Entered %s: rate=%d\n", __FUNCTION__, params_rate(params));
	
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		rtd->dai->cpu_dai->dma_data = &s5p_spdif_pcm_stereo_out;
	} else {
	}

	switch (params_rate(params)) {
	case 32000:
		sampling_freq = S5P_SPDIF_SPDCSTAS_SAMPLING_FREQ_32;
		break;
	case 44100:
		sampling_freq = S5P_SPDIF_SPDCSTAS_SAMPLING_FREQ_44_1;
		break;
	case 48000:
		sampling_freq = S5P_SPDIF_SPDCSTAS_SAMPLING_FREQ_48;
		break;
	case 96000:
		sampling_freq = S5P_SPDIF_SPDCSTAS_SAMPLING_FREQ_96;
		break;
	default:
		sampling_freq = S5P_SPDIF_SPDCSTAS_SAMPLING_FREQ_48;
		return -EINVAL;
	}

	writel(S5P_SPDIF_SPDCON_SOFTWARE_RESET_EN, s5p_spdif.regs + S5P_SPDIF_SPDCON); // Sw Reset

	mdelay(100);
	
	// Clear Interrupt
	writel(S5P_SPDIF_SPDCON_FIFO_LEVEL_INT_STATUS_PENDING|
		S5P_SPDIF_SPDCON_USER_DATA_INT_STATUS_PENDING |
		S5P_SPDIF_SPDCON_BUFFER_EMPTY_INT_STATUS_PENDING |
		S5P_SPDIF_SPDCON_STREAM_END_INT_STATUS_PENDING, 
		s5p_spdif.regs + S5P_SPDIF_SPDCON); 

	//set SPDIF Control Register
	writel(S5P_SPDIF_SPDCON_FIFO_LEVEL_THRESHOLD_7 |
		S5P_SPDIF_SPDCON_FIFO_LEVEL_INT_ENABLE |	 
		S5P_SPDIF_SPDCON_ENDIAN_FORMAT_BIG |
		S5P_SPDIF_SPDCON_USER_DATA_ATTACH_AUDIO_DATA |	// user data attach on  
		S5P_SPDIF_SPDCON_USER_DATA_INT_ENABLE|	  
		S5P_SPDIF_SPDCON_BUFFER_EMPTY_INT_ENABLE|	 
		S5P_SPDIF_SPDCON_STREAM_END_INT_ENABLE |	 
		S5P_SPDIF_SPDCON_SOFTWARE_RESET_NO |
		S5P_SPDIF_SPDCON_MAIN_AUDIO_CLK_FREQ_512 |
		S5P_SPDIF_SPDCON_PCM_DATA_SIZE_16 |
		S5P_SPDIF_SPDCON_PCM, 
		s5p_spdif.regs + S5P_SPDIF_SPDCON);	

	// Set SPDIFOUT Burst Status Register
	writel(S5P_SPDIF_SPDBSTAS_BURST_DATA_LENGTH(16)|	// bitper sample is 16
		S5P_SPDIF_SPDBSTAS_BITSTREAM_NUMBER(0)| 	// Bitstream number set to 0
		S5P_SPDIF_SPDBSTAS_DATA_TYPE_DEP_INFO |		// Data type dependent information
		S5P_SPDIF_SPDBSTAS_ERROR_FLAG_VALID |		// Error flag indicating a valid burst_payload
		S5P_SPDIF_SPDBSTAS_COMPRESSED_DATA_TYPE_NULL,	// Compressed data type is Null Data ?
		s5p_spdif.regs + S5P_SPDIF_SPDBSTAS);

	// Set SPDIFOUT Channel Status Register
	writel(S5P_SPDIF_SPDCSTAS_CLOCK_ACCURACY_50 |
		sampling_freq |
		S5P_SPDIF_SPDCSTAS_CHANNEL_NUMBER(0) |
		S5P_SPDIF_SPDCSTAS_SOURCE_NUMBER(0) |
		S5P_SPDIF_SPDCSTAS_CATEGORY_CODE_CD |
		S5P_SPDIF_SPDCSTAS_CHANNEL_SATAUS_MODE |
		S5P_SPDIF_SPDCSTAS_EMPHASIS_WITHOUT_PRE_EMP |
		S5P_SPDIF_SPDCSTAS_COPYRIGHT_ASSERTION_NO |
		S5P_SPDIF_SPDCSTAS_AUDIO_SAMPLE_WORD_LINEAR_PCM |
		S5P_SPDIF_SPDCSTAS_CHANNEL_STATUS_BLOCK_CON,
		s5p_spdif.regs + S5P_SPDIF_SPDCSTAS);

	// SPDIFOUT Repitition Count register
	writel(S5P_SPDIF_SPDCNT_STREAM_REPETITION_COUNT(0), 
		s5p_spdif.regs + S5P_SPDIF_SPDCNT);
		
	return 0;
}


static int s5p_spdif_trigger(struct snd_pcm_substream 	*substream, 
			     int 			cmd, 
			     struct snd_soc_dai 	*dai)
{
	int ret = 0;

	s3cdbg("Entered %s: cmd=%d\n", __FUNCTION__, cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:

		s5p_spdif_snd_txctrl(1);
		break;
		
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	
		s5p_spdif_snd_txctrl(0);
		break;
		
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}


static void s5p_spdif_shutdown(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	s3cdbg("Entered %s\n", __FUNCTION__);

	writel(S5P_SPDIF_SPDCON_SOFTWARE_RESET_EN, 
		s5p_spdif.regs + S5P_SPDIF_SPDCON);	 // Sw Reset
		
	mdelay(100);
	
	writel(S5P_SPDIF_SPDCLKCON_SPDIFOUT_POWER_OFF, 
		s5p_spdif.regs + S5P_SPDIF_SPDCLKCON);	 // SPDIFOUT clock power off
}


static int s5p_spdif_prepare(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	s3cdbg("Entered %s\n", __FUNCTION__);
 	return 0;
}

 
static int s5p_spdif_set_sysclk(struct snd_soc_dai *cpu_dai,
				    int clk_id, unsigned int freq, int dir)
{
	s3cdbg("Entered %s\n", __FUNCTION__);
	return 0;
}

 
static int s5p_spdif_set_clkdiv(struct snd_soc_dai *cpu_dai, int div_id, int div)
{
	s3cdbg("Entered %s: div_id=%d, div=%d\n", __FUNCTION__, div_id, div);
	return 0;
}

u32 s5p_spdif_get_clockrate(void)
{
	return clk_get_rate(s5p_spdif.spdif_clk);
}
EXPORT_SYMBOL_GPL(s5p_spdif_get_clockrate);

#define SPDIF_SPDCON_INT_MASK	(S5P_SPDIF_SPDCON_FIFO_LEVEL_INT_STATUS_PENDING | \
 				 S5P_SPDIF_SPDCON_BUFFER_EMPTY_INT_STATUS_PENDING | \
 				 S5P_SPDIF_SPDCON_STREAM_END_INT_STATUS_PENDING | \
 				 S5P_SPDIF_SPDCON_USER_DATA_INT_STATUS_PENDING )

static irqreturn_t s5p_spdif_irq(int irqno, void *dev_id)
{
	u32 spdifcon;

	spdifcon = readl(s5p_spdif.regs + S5P_SPDIF_SPDCON);

	if ((spdifcon & (S5P_SPDIF_SPDCON_FIFO_LEVEL_INT_ENABLE | 
			 S5P_SPDIF_SPDCON_FIFO_LEVEL_INT_STATUS_PENDING))
	    == (S5P_SPDIF_SPDCON_FIFO_LEVEL_INT_ENABLE | 
		S5P_SPDIF_SPDCON_FIFO_LEVEL_INT_STATUS_PENDING)) {
		// s3cdbg("Entered %s: interrupt src : SPDIF_INT_FIFOLVL \n", __FUNCTION__);
	}
	
	if ((spdifcon & (S5P_SPDIF_SPDCON_BUFFER_EMPTY_INT_ENABLE 
			 | S5P_SPDIF_SPDCON_BUFFER_EMPTY_INT_STATUS_PENDING))
	    == (S5P_SPDIF_SPDCON_BUFFER_EMPTY_INT_ENABLE | 
	    	S5P_SPDIF_SPDCON_BUFFER_EMPTY_INT_STATUS_PENDING)) {
		// s3cdbg("Entered %s: interrupt src : SPDIF_INT_BUFEMPTY \n", __FUNCTION__);
	}
	
	if ((spdifcon & (S5P_SPDIF_SPDCON_STREAM_END_INT_ENABLE | 
			 S5P_SPDIF_SPDCON_STREAM_END_INT_STATUS_PENDING))
	    == (S5P_SPDIF_SPDCON_STREAM_END_INT_ENABLE | 
	        S5P_SPDIF_SPDCON_STREAM_END_INT_STATUS_PENDING)) {
		// s3cdbg("Entered %s: interrupt src : SPDIF_INT_STREAMEND \n", __FUNCTION__);
	}
	
	if ((spdifcon & (S5P_SPDIF_SPDCON_USER_DATA_INT_ENABLE | 
			 S5P_SPDIF_SPDCON_USER_DATA_INT_STATUS_PENDING))
	    == (S5P_SPDIF_SPDCON_USER_DATA_INT_ENABLE | 
	        S5P_SPDIF_SPDCON_USER_DATA_INT_STATUS_PENDING)) {
		// s3cdbg("Entered %s: interrupt src : SPDIF_INT_USERDATA \n", __FUNCTION__);
	}

	writel(spdifcon, s5p_spdif.regs + S5P_SPDIF_SPDCON);

	return IRQ_HANDLED;
}

static int s5p_spdif_probe(struct platform_device *pdev,
			   struct snd_soc_dai *dai)
{
	int ret;
 
	s3cdbg("Entered %s\n", __FUNCTION__);
 
	s5p_spdif.regs = ioremap(S5P_PA_SPDIF, 0x40);
	if (s5p_spdif.regs == NULL)
		return -ENXIO;


	s5p_spdif.spdif_clk = clk_get(&pdev->dev, "spdif");
	
	if (s5p_spdif.spdif_clk == NULL) {
		s3cdbg("failed to get spdif_clock\n");
		iounmap(s5p_spdif.regs);
		return -ENODEV;
	}
	clk_enable(s5p_spdif.spdif_clk);
	
	clk_put(s5p_spdif.spdif_clk);
 
#ifdef CONFIG_SND_SMDKC100_HDMI_SPDIF

	s5p_spdif.spdif_sclk=clk_get(&pdev->dev, "sclk_spdif");
	if (s5p_spdif.spdif_clk == NULL) {
		s3cdbg("failed to get spdif_clock\n");
		iounmap(s5p_spdif.regs);
		return -ENODEV;
	}
	clk_enable(s5p_spdif.spdif_sclk);
	
#else 
#ifdef CONFIG_SND_SMDKC110_HDMI_SPDIF

	s5p_spdif.spdif_sclk = clk_get(NULL, "fout_epll");
	if (IS_ERR(s5p_spdif.spdif_sclk)) {
		printk("failed to get FOUTepll\n");
		return -EBUSY;
	}
	
	clk_disable(s5p_spdif.spdif_sclk);

	/* 
	 * To Do: various sampling rate support: currently support 48KHZ only 
	 *        this code is moved to s5p_spdif_hw_params for various sampling rate  
	 */	
	clk_set_rate(s5p_spdif.spdif_sclk, 49152000); 
						      
	clk_enable(s5p_spdif.spdif_sclk);

	clk_put(s5p_spdif.spdif_sclk);
#endif
#endif

	ret = request_irq(IRQ_SPDIF, s5p_spdif_irq, 0,
		  "s5p_spdif", pdev);
	if (ret < 0) {
		s3cdbg("fail to claim i2s irq , ret = %d\n", ret);
		return -ENODEV;
	}
	
	return 0;
}

static void s5p_spdif_remove(struct platform_device *pdev,
			     struct snd_soc_dai *dai)
{
	s3cdbg("Entered %s\n", __FUNCTION__);
	return;
}

static int s5p_spdif_startup(struct snd_pcm_substream *substream, 
			     struct snd_soc_dai *dai)
{
	s3cdbg("Entered %s\n", __FUNCTION__);
	return 0;
}

static int s5p_spdif_set_fmt(struct snd_soc_dai *cpu_dai,
			     unsigned int fmt)
{
	s3cdbg("Entered %s\n", __FUNCTION__);
	return 0;
}

#ifdef CONFIG_PM
static int s5p_spdif_suspend(struct platform_device *dev,
				 struct snd_soc_dai *dai)
{
	s3cdbg("Entered %s\n", __FUNCTION__);
	return 0;
}
 
static int s5p_spdif_resume(struct platform_device *pdev,
				struct snd_soc_dai *dai)
{
	s3cdbg("Entered %s\n", __FUNCTION__);
	return 0;
}
#else
#define s5p_spdif_suspend	NULL
#define s5p_spdif_resume	NULL
#endif

/*
 * SPDIF supports various sampling rate of 32KHz~192KHz.
 * But currently it supports 48KHz only. 
 */
#define S5P_SPDIF_RATES \
	(SNDRV_PCM_RATE_48000)

/*
 * SPDIF supports formats of 16, 20 24 bits.
 * But currently it supports 16 bits only. 
 */
#define S5P_SPDIF_FORMATS \
	(SNDRV_PCM_FMTBIT_S16_LE)


struct snd_soc_dai s5p_spdif_dai = {
	.name = "s5p-spdif",
	.id = 0,
	.probe = s5p_spdif_probe,	
	.remove = s5p_spdif_remove,
	.suspend = s5p_spdif_suspend,
	.resume = s5p_spdif_resume,
	.playback = {
		.channels_min = 1,
		.channels_max = 2,
		.rates = S5P_SPDIF_RATES, 
		.formats = S5P_SPDIF_FORMATS
	}, 
	.capture = {
		.channels_min = 2,
		.channels_max = 2,
		.rates = S5P_SPDIF_RATES,
		.formats = S5P_SPDIF_FORMATS,
	},
	.ops = {
	 	.hw_params  = s5p_spdif_hw_params,
	 	.prepare    = s5p_spdif_prepare,
	 	.startup    = s5p_spdif_startup,
	 	.trigger    = s5p_spdif_trigger,
	 	.set_fmt    = s5p_spdif_set_fmt, 		
		.set_clkdiv = s5p_spdif_set_clkdiv,	 
		.set_sysclk = s5p_spdif_set_sysclk,
		.shutdown   = s5p_spdif_shutdown,			
	},	
 };

 EXPORT_SYMBOL_GPL(s5p_spdif_dai);

 static int __init s5p_spdif_init(void)
 {
 	s3cdbg("Entered %s\n", __FUNCTION__);
	return snd_soc_register_dai(&s5p_spdif_dai);
 }
 module_init(s5p_spdif_init);

 
 static void __exit s5p_spdif_exit(void)
 {
 	s3cdbg("Entered %s\n", __FUNCTION__);
	snd_soc_unregister_dai(&s5p_spdif_dai);
 }
 module_exit(s5p_spdif_exit);
 

 /* Module information */
 MODULE_DESCRIPTION("s5p SPDIF SoC Interface");
 MODULE_LICENSE("GPL");
 
