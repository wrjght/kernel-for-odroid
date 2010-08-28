/*
 *  s5p-spdif.c  
 *
 * (c) 2006 Wolfson Microelectronics PLC.
 * Graeme Gregory graeme.gregory@wolfsonmicro.com or linux@wolfsonmicro.com
 *
 * (c) 2004-2005 Simtec Electronics
 *	http://armlinux.simtec.co.uk/
 *	Ben Dooks <ben@simtec.co.uk>
 *	Ryu Euiyoul <ryu.real@gmail.com>
 *
 * Copyright (C) 2009, Kwak Hyun-Min<hyunmin.kwak@samsung.com>
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

#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
 
#include <mach/dma.h>
#include <mach/map.h>
#include <mach/gpio.h>

#include <plat/regs-iis1.h>
#include <plat/regs-clock.h>
#include <plat/gpio-cfg.h>
#include <plat/regs-spdif.h>
#include <plat/regs-clock.h>

#include "s3c-dma.h"

#include "s5p-i2s.h"
#include "odroid-s5p-spdif.h"
 
#if 0
#define s3cdbg(x...) printk(x)
#else
#define s3cdbg(x...)
#endif

struct s5pc1xx_spdif_info {
	void __iomem	*regs;
	struct clk	*spdif_clk;
	struct clk	*spdif_sclk;

	struct clk	*spdif_sclk_src;
	struct clk	*spdif_sclk_audio0;
	
	int 		master;
};

enum spdif_intr_src
{
	SPDIF_INT_FIFOLVL,
	SPDIF_INT_USERDATA,
	SPDIF_INT_BUFEMPTY,
	SPDIF_INT_STREAMEND,
	SPDIF_INT_UNKNOWN
};

	
struct s5pc1xx_spdif_info s5pc1xx_spdif;
	
static struct s3c2410_dma_client s5pc1xx_dma_spdif_out = {
	.name = "spdif out"
};

static struct s3c_dma_params s5pc1xx_spdif_pcm_out = {
	.client	 	= &s5pc1xx_dma_spdif_out,
	.channel	= DMACH_SPDIF_OUT,					 // to do: need to modify for spdif
	.dma_addr	= S5P_PA_SPDIF + S5PC1XX_SPDIF_SPDDAT, 			 // to do: need to modify for spdif
	.dma_size	= 2,							 // to do: need to modify for spdif 
};

int 	s5pc1xx_spdif_power_off(void)
{
	u32 clk_gate_D1_5, clk_gate_sclk1;

	/* spdif audio CLK gating start*/
	// SPDIF sclk gate Mask
	clk_gate_sclk1 = readl(S5P_SCLKGATE1);
	clk_gate_sclk1 &= ~S5P_CLKGATE_SCLK1_SPDIF;
	writel(clk_gate_sclk1,S5P_SCLKGATE1);
	
	// PCLK gating for spdif 
	clk_gate_D1_5 = readl(S5P_CLKGATE_D15);
	clk_gate_D1_5 &= ~S5P_CLKGATE_D15_SPDIF; // spdif gate Disable
	writel(clk_gate_D1_5,S5P_CLKGATE_D15);

	return 0;
}

int s5pc1xx_spdif_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params, 
				struct snd_soc_dai *socdai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai_link *dai = rtd->dai;
	int sampling_freq;

	u32 clk_gate_D1_5, clk_gate_sclk1, clk_src3, clk_div4;
	s3cdbg("Entered %s, rate = %d\n", __FUNCTION__, params_rate(params));

	/* spdif audio CLK gating start*/
	writel(readl(S5P_CLK_OUT)|(0x2<<12),S5P_CLK_OUT);
	s3cdbg("CLK OUT : 0x%08x\n",readl(S5P_CLK_OUT));
	
	clk_src3 = readl(S5P_CLK_SRC3);
	clk_src3 &= ~S5P_CLKSRC3_AUDIO1_MASK;
	clk_src3 |= (0x0 << S5P_CLKSRC3_AUDIO1_SHIFT);
	writel(clk_src3, S5P_CLK_SRC3); // MUX audio1
	s3cdbg("S5P_CLK_SRC3 : 0x%08x\n",readl(S5P_CLK_SRC3));
	
	clk_div4 = readl(S5P_CLK_DIV4);
	clk_div4 &= ~S5P_CLKDIV4_AUDIO1_MASK;
	clk_div4 |= (0x3 << S5P_CLKDIV4_AUDIO1_SHIFT);
	writel(clk_div4, S5P_CLK_DIV4); // MUX audio1
	s3cdbg("S5P_CLK_DIV4 : 0x%08x\n",readl(S5P_CLK_DIV4));
	
	// PCLK gating for IIS1 
	clk_gate_D1_5 = readl(S5P_CLKGATE_D15);
	clk_gate_D1_5 |= S5P_CLKGATE_D15_IIS1; // IIS1 gate Enable
	writel(clk_gate_D1_5,S5P_CLKGATE_D15);
	s3cdbg("GATE D1_5 : 0x%08x\n",readl(S5P_CLKGATE_D15));
	
	// SCLK Gateing Audio1
	clk_gate_sclk1 = readl(S5P_SCLKGATE1);
	clk_gate_sclk1 |= S5P_CLKGATE_SCLK1_AUDIO1; // IIS1 SCLK Gating enable
	writel(clk_gate_sclk1,S5P_SCLKGATE1);
	s3cdbg("S5P_SCLKGATE1 : 0x%08x\n",readl(S5P_SCLKGATE1));
	
	// SPDIF clk src Audio1
	clk_src3 = readl(S5P_CLK_SRC3);
	clk_src3 &= ~S5P_CLKSRC3_SPDIF_MASK;
	clk_src3 |= (0x1 << S5P_CLKSRC3_SPDIF_SHIFT);
	writel(clk_src3, S5P_CLK_SRC3); // MUX audio1
	s3cdbg("S5P_CLK_SRC3 : 0x%08x\n",readl(S5P_CLK_SRC3));
	
	// SPDIF sclk gate
	clk_gate_sclk1 = readl(S5P_SCLKGATE1);
	clk_gate_sclk1 |= S5P_CLKGATE_SCLK1_SPDIF;
	writel(clk_gate_sclk1,S5P_SCLKGATE1);
	s3cdbg("S5P_SCLKGATE1 : 0x%08x\n",readl(S5P_SCLKGATE1));
	
	// PCLK gating for spdif 
	clk_gate_D1_5 = readl(S5P_CLKGATE_D15);
	clk_gate_D1_5 |= S5P_CLKGATE_D15_SPDIF; // spdif gate Enable
	writel(clk_gate_D1_5,S5P_CLKGATE_D15);
	s3cdbg("GATE D1_5 : 0x%08x\n",readl(S5P_CLKGATE_D15));
		
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		dai->cpu_dai->dma_data = &s5pc1xx_spdif_pcm_out;
	} else {
	}

	switch (params_rate(params)) {
	case 32000:
			sampling_freq = S5PC1XX_SPDIF_SPDCSTAS_SAMPLING_FREQ_32;
		break;
	case 44100:
			sampling_freq = S5PC1XX_SPDIF_SPDCSTAS_SAMPLING_FREQ_44_1;
		break;
	case 48000:
			sampling_freq = S5PC1XX_SPDIF_SPDCSTAS_SAMPLING_FREQ_48;
		break;
	case 96000:
			sampling_freq = S5PC1XX_SPDIF_SPDCSTAS_SAMPLING_FREQ_96;
		break;
	default:
			sampling_freq = S5PC1XX_SPDIF_SPDCSTAS_SAMPLING_FREQ_44_1;
			break;
	}

	writel(S5PC1XX_SPDIF_SPDCON_SOFTWARE_RESET_EN, s5pc1xx_spdif.regs + S5PC1XX_SPDIF_SPDCON);	 // Sw Reset

	mdelay(100);
	// Clear Interrupt
	writel(S5PC1XX_SPDIF_SPDCON_FIFO_LEVEL_INT_STATUS_PENDING|
		S5PC1XX_SPDIF_SPDCON_USER_DATA_INT_STATUS_PENDING |
		S5PC1XX_SPDIF_SPDCON_BUFFER_EMPTY_INT_STATUS_PENDING |
		S5PC1XX_SPDIF_SPDCON_STREAM_END_INT_STATUS_PENDING, s5pc1xx_spdif.regs + S5PC1XX_SPDIF_SPDCON); 

	//set SPDIF Control Register
	writel(S5PC1XX_SPDIF_SPDCON_FIFO_LEVEL_THRESHOLD_4 |
		S5PC1XX_SPDIF_SPDCON_FIFO_LEVEL_INT_ENABLE |	 
		S5PC1XX_SPDIF_SPDCON_ENDIAN_FORMAT_BIG |
		S5PC1XX_SPDIF_SPDCON_USER_DATA_ATTACH_AUDIO_DATA |	 // user data attach on  
		S5PC1XX_SPDIF_SPDCON_USER_DATA_INT_ENABLE|	  
		S5PC1XX_SPDIF_SPDCON_BUFFER_EMPTY_INT_ENABLE|	 
		S5PC1XX_SPDIF_SPDCON_STREAM_END_INT_ENABLE |	 
		S5PC1XX_SPDIF_SPDCON_SOFTWARE_RESET_NO |
		S5PC1XX_SPDIF_SPDCON_MAIN_AUDIO_CLK_FREQ_384 |
		S5PC1XX_SPDIF_SPDCON_PCM_DATA_SIZE_16 |
		S5PC1XX_SPDIF_SPDCON_PCM, s5pc1xx_spdif.regs + S5PC1XX_SPDIF_SPDCON);	

	// Set SPDIFOUT Burst Status Register
	writel(S5PC1XX_SPDIF_SPDBSTAS_BURST_DATA_LENGTH(16)|		 	// bitper sample is 16
		S5PC1XX_SPDIF_SPDBSTAS_BITSTREAM_NUMBER(0)| 	 		// Bitstream number set to 0
		S5PC1XX_SPDIF_SPDBSTAS_DATA_TYPE_DEP_INFO |			// Data type dependent information
		S5PC1XX_SPDIF_SPDBSTAS_ERROR_FLAG_VALID |		 	// Error flag indicating a valid burst_payload
		S5PC1XX_SPDIF_SPDBSTAS_COMPRESSED_DATA_TYPE_NULL,		// Compressed data type is Null Data ?
		s5pc1xx_spdif.regs + S5PC1XX_SPDIF_SPDBSTAS);

	// Set SPDIFOUT Channel Status Register
	writel(S5PC1XX_SPDIF_SPDCSTAS_CLOCK_ACCURACY_50 |
		sampling_freq |
		S5PC1XX_SPDIF_SPDCSTAS_CHANNEL_NUMBER(0) |
		S5PC1XX_SPDIF_SPDCSTAS_SOURCE_NUMBER(0) |
		S5PC1XX_SPDIF_SPDCSTAS_CATEGORY_CODE_CD |
		S5PC1XX_SPDIF_SPDCSTAS_CHANNEL_SATAUS_MODE |
		S5PC1XX_SPDIF_SPDCSTAS_EMPHASIS_WITHOUT_PRE_EMP |
		S5PC1XX_SPDIF_SPDCSTAS_COPYRIGHT_ASSERTION_NO |
		S5PC1XX_SPDIF_SPDCSTAS_AUDIO_SAMPLE_WORD_LINEAR_PCM |
		S5PC1XX_SPDIF_SPDCSTAS_CHANNEL_STATUS_BLOCK_CON ,s5pc1xx_spdif.regs + S5PC1XX_SPDIF_SPDCSTAS);

	// SPDIFOUT Repitition Count register
	writel(S5PC1XX_SPDIF_SPDCNT_STREAM_REPETITION_COUNT(0), s5pc1xx_spdif.regs + S5PC1XX_SPDIF_SPDCNT);
	return 0;

}

int s5pc1xx_spdif_prepare(struct snd_pcm_substream *substream, struct snd_soc_dai *socdai)
{
//     struct snd_soc_pcm_runtime *rtd = substream->private_data;
     writel(S5PC1XX_SPDIF_SPDCON_SOFTWARE_RESET_EN, s5pc1xx_spdif.regs + S5PC1XX_SPDIF_SPDCON);      // Sw Reset
	
     mdelay(100);
	
     // Clear Interrupt
     writel(S5PC1XX_SPDIF_SPDCON_FIFO_LEVEL_INT_STATUS_PENDING|
            S5PC1XX_SPDIF_SPDCON_USER_DATA_INT_STATUS_PENDING |
            S5PC1XX_SPDIF_SPDCON_BUFFER_EMPTY_INT_STATUS_PENDING |
            S5PC1XX_SPDIF_SPDCON_STREAM_END_INT_STATUS_PENDING, s5pc1xx_spdif.regs + S5PC1XX_SPDIF_SPDCON);
	
     //set SPDIF Control Register
     writel(S5PC1XX_SPDIF_SPDCON_FIFO_LEVEL_THRESHOLD_4 |
            S5PC1XX_SPDIF_SPDCON_FIFO_LEVEL_INT_ENABLE |
            S5PC1XX_SPDIF_SPDCON_ENDIAN_FORMAT_BIG |
            S5PC1XX_SPDIF_SPDCON_USER_DATA_ATTACH_AUDIO_DATA |       // user data attach on  
            S5PC1XX_SPDIF_SPDCON_USER_DATA_INT_ENABLE|
            S5PC1XX_SPDIF_SPDCON_BUFFER_EMPTY_INT_ENABLE|
            S5PC1XX_SPDIF_SPDCON_STREAM_END_INT_ENABLE |
            S5PC1XX_SPDIF_SPDCON_SOFTWARE_RESET_NO |
            S5PC1XX_SPDIF_SPDCON_MAIN_AUDIO_CLK_FREQ_384 |
            S5PC1XX_SPDIF_SPDCON_PCM_DATA_SIZE_16 |
            S5PC1XX_SPDIF_SPDCON_PCM, s5pc1xx_spdif.regs + S5PC1XX_SPDIF_SPDCON);

     // Set SPDIFOUT Burst Status Register
     writel(S5PC1XX_SPDIF_SPDBSTAS_BURST_DATA_LENGTH(16)|                   // bitper sample is 16
             S5PC1XX_SPDIF_SPDBSTAS_BITSTREAM_NUMBER(0)|                    // Bitstream number set to 0
             S5PC1XX_SPDIF_SPDBSTAS_DATA_TYPE_DEP_INFO |                    // Data type dependent information
             S5PC1XX_SPDIF_SPDBSTAS_ERROR_FLAG_VALID |                      // Error flag indicating a valid burst_payload
             S5PC1XX_SPDIF_SPDBSTAS_COMPRESSED_DATA_TYPE_NULL,              // Compressed data type is Null Data ?
             s5pc1xx_spdif.regs + S5PC1XX_SPDIF_SPDBSTAS);
	
     // Set SPDIFOUT Channel Status Register
     writel(S5PC1XX_SPDIF_SPDCSTAS_CLOCK_ACCURACY_50 |
            //sampling_freq |
            S5PC1XX_SPDIF_SPDCSTAS_CHANNEL_NUMBER(0) |
            S5PC1XX_SPDIF_SPDCSTAS_SOURCE_NUMBER(0) |
            S5PC1XX_SPDIF_SPDCSTAS_CATEGORY_CODE_CD |
            S5PC1XX_SPDIF_SPDCSTAS_CHANNEL_SATAUS_MODE |
            S5PC1XX_SPDIF_SPDCSTAS_EMPHASIS_WITHOUT_PRE_EMP |
            S5PC1XX_SPDIF_SPDCSTAS_COPYRIGHT_ASSERTION_NO |
            S5PC1XX_SPDIF_SPDCSTAS_AUDIO_SAMPLE_WORD_LINEAR_PCM |
            S5PC1XX_SPDIF_SPDCSTAS_CHANNEL_STATUS_BLOCK_CON ,s5pc1xx_spdif.regs + S5PC1XX_SPDIF_SPDCSTAS);
						      
     // SPDIFOUT Repitition Count register
     writel(S5PC1XX_SPDIF_SPDCNT_STREAM_REPETITION_COUNT(0), s5pc1xx_spdif.regs + S5PC1XX_SPDIF_SPDCNT);
	
	return 0;
}


void s5pc1xx_spdif_shutdown(struct snd_pcm_substream *substream, struct snd_soc_dai *socdai)
{
	s3cdbg("Entered %s\n", __FUNCTION__); 
	writel((S5PC1XX_SPDIF_SPDCON_SOFTWARE_RESET_EN), s5pc1xx_spdif.regs + S5PC1XX_SPDIF_SPDCON);	 // Sw Reset
	mdelay(100);
	writel(S5PC1XX_SPDIF_SPDCLKCON_SPDIFOUT_POWER_OFF, s5pc1xx_spdif.regs + S5PC1XX_SPDIF_SPDCLKCON);	 // SPDIFOUT clock power off
}

#define SPDIF_SPDCON_INT_MASK	(S5PC1XX_SPDIF_SPDCON_FIFO_LEVEL_INT_STATUS_PENDING | \
 				 S5PC1XX_SPDIF_SPDCON_BUFFER_EMPTY_INT_STATUS_PENDING | \
 				 S5PC1XX_SPDIF_SPDCON_STREAM_END_INT_STATUS_PENDING | \
 				 S5PC1XX_SPDIF_SPDCON_USER_DATA_INT_STATUS_PENDING )

irqreturn_t s5pc1xx_spdif_irq(int irqno, void *dev_id)
{

	u32 spdifcon;
	s3cdbg("Entered %s\n", __FUNCTION__);
	spdifcon = readl(s5pc1xx_spdif.regs + S5PC1XX_SPDIF_SPDCON);

	if (spdifcon&(S5PC1XX_SPDIF_SPDCON_FIFO_LEVEL_INT_ENABLE | S5PC1XX_SPDIF_SPDCON_FIFO_LEVEL_INT_STATUS_PENDING)){
		s3cdbg("Entered %s interrupt src : SPDIF_INT_FIFOLVL \n", __FUNCTION__);
	}
	if (spdifcon&(S5PC1XX_SPDIF_SPDCON_BUFFER_EMPTY_INT_ENABLE | S5PC1XX_SPDIF_SPDCON_BUFFER_EMPTY_INT_STATUS_PENDING)){
		s3cdbg("Entered %s interrupt src : SPDIF_INT_BUFEMPTY \n", __FUNCTION__);
	}
	if (spdifcon&(S5PC1XX_SPDIF_SPDCON_STREAM_END_INT_ENABLE | S5PC1XX_SPDIF_SPDCON_STREAM_END_INT_STATUS_PENDING)){
		s3cdbg("Entered %s interrupt src : SPDIF_INT_STREAMEND \n", __FUNCTION__);
	}
	if (spdifcon&(S5PC1XX_SPDIF_SPDCON_USER_DATA_INT_ENABLE | S5PC1XX_SPDIF_SPDCON_USER_DATA_INT_STATUS_PENDING)){
		s3cdbg("Entered %s interrupt src : SPDIF_INT_USERDATA \n", __FUNCTION__);
	}

	writel(spdifcon, s5pc1xx_spdif.regs + S5PC1XX_SPDIF_SPDCON);	 // Sw Reset

	return IRQ_HANDLED;
}


u32 s5pc1xx_spdif_get_clockrate(void)
{
	s3cdbg("Entered %s\n", __FUNCTION__);
	return clk_get_rate(s5pc1xx_spdif.spdif_clk);
	}
EXPORT_SYMBOL_GPL(s5pc1xx_spdif_get_clockrate);


void spdif_clock_control(bool enable)
{
	s3cdbg("Entered %s\n", __FUNCTION__);
	if (!enable)
		s5pc1xx_spdif_power_off();
}
EXPORT_SYMBOL_GPL(spdif_clock_control);
	


static int s5pc1xx_spdif_clock_init(struct platform_device *pdev)
{
	struct clk *tmp_mout_epll;

	s3cdbg("Entered %s\n", __FUNCTION__);

	s5pc1xx_spdif.spdif_clk = clk_get(&pdev->dev, "spdif");
	if (IS_ERR(s5pc1xx_spdif.spdif_clk)) {
		printk("failed to get clock \"spdif\"\n");
		goto err_on_spdif_clk;
	}

	s5pc1xx_spdif.spdif_sclk_src = clk_get(NULL, "fout_epll");
	if (IS_ERR(s5pc1xx_spdif.spdif_sclk_src)) {
		printk("failed to get clock \"fout_epll\"\n");
		goto err_on_spdif_src_sclk;
	}

	tmp_mout_epll = clk_get(NULL, "mout_epll");
	if (IS_ERR(tmp_mout_epll)) {
		printk("failed to get clock \"mout_epll\"\n");
		goto err_on_mout_epll;
	}

	s5pc1xx_spdif.spdif_sclk_audio0 = clk_get(NULL, "sclk_audio");
	if (IS_ERR(s5pc1xx_spdif.spdif_sclk_audio0)) {
		printk("failed to get clock \"sclk_audio\"\n");
		goto err_on_spdif_sclk_audio0;
	}

#ifdef CONFIG_CPU_S5PC100
	s5pc1xx_spdif.spdif_sclk = clk_get(&pdev->dev, "sclk_spdif");
	if (IS_ERR(s5pc1xx_spdif.spdif_sclk)) {
		printk("failed to get spdif_clock\n");
		goto err_on_spdif_sclk;
	}
#endif

	if (clk_set_parent(tmp_mout_epll, s5pc1xx_spdif.spdif_sclk_src)) {
		printk("failed to set spdif_sclk_src"
			"as parent of \"mout_epll\"\n");
		goto err_on_set_parent;
	}

	if (clk_set_parent(s5pc1xx_spdif.spdif_sclk_audio0, tmp_mout_epll)) {
		printk("failed to set \"mout_epll\""
			"as parent of spdif_sclk_audio0\n");
		goto err_on_set_parent;
	}
	
	clk_put(tmp_mout_epll);

	clk_enable(s5pc1xx_spdif.spdif_sclk_audio0);
	clk_enable(s5pc1xx_spdif.spdif_sclk_src);
	clk_enable(s5pc1xx_spdif.spdif_clk);

#ifdef CONFIG_CPU_S5PC100
	clk_enable(s5pc1xx_spdif.spdif_sclk);
#endif
	
	return 0;

err_on_set_parent:
	
#ifdef CONFIG_CPU_S5PC100	
	clk_put(s5pc1xx_spdif.spdif_sclk);
	
err_on_spdif_sclk:
#endif
	clk_put(s5pc1xx_spdif.spdif_sclk_audio0);

err_on_spdif_sclk_audio0:
	clk_put(tmp_mout_epll);
err_on_mout_epll:	
	clk_put(s5pc1xx_spdif.spdif_sclk_src);
err_on_spdif_src_sclk:
	clk_put(s5pc1xx_spdif.spdif_clk);
err_on_spdif_clk:
	return -ENODEV;
}

int s5pc1xx_spdif_probe(struct platform_device *pdev, struct snd_soc_dai *dai)
{
	int ret;
	
	s3cdbg("Entered %s\n", __FUNCTION__);
	
	s5pc1xx_spdif.regs = ioremap(S5P_PA_SPDIF, 0x40);
	if (s5pc1xx_spdif.regs == NULL)
		return -ENXIO;
	
	s5pc1xx_spdif_clock_init(pdev);

	ret = request_irq(IRQ_SPDIF, s5pc1xx_spdif_irq, 0, "s5pc1xx_spdif", NULL);
	if (ret < 0) {
		s3cdbg("fail to claim i2s irq , ret = %d\n", ret);
		clk_disable(s5pc1xx_spdif.spdif_sclk);
		clk_put(s5pc1xx_spdif.spdif_sclk);
		s5pc1xx_spdif.spdif_sclk = NULL;
	return -ENODEV;
}

	return 0;
}
 
// to do: need to modify for spdif
void s5pc1xx_spdif_txctrl(int on)
{
	s3cdbg("Entered %s : on = %d \n", __FUNCTION__, on);
	if (on)
	{
		// SPDIFOUT clock power off 
		writel(S5PC1XX_SPDIF_SPDCLKCON_SPDIFOUT_POWER_OFF, s5pc1xx_spdif.regs + S5PC1XX_SPDIF_SPDCLKCON);
 
		writel(S5PC1XX_SPDIF_SPDCLKCON_MAIN_AUDIO_CLK_INT |
			S5PC1XX_SPDIF_SPDCLKCON_SPDIFOUT_POWER_ON, s5pc1xx_spdif.regs + S5PC1XX_SPDIF_SPDCLKCON);	 // SPDIFOUT clock power on
		// spdcon = readl(s5pc1xx_spdif.regs + S5PC1XX_SPDIF_SPDCON) & ~(0xf<<19|0x3<<17);
		// writel(spdcon | S5PC1XX_SPDIF_SPDCON_FIFO_LEVEL_THRESHOLD_7,  s5pc1xx_spdif.regs + S5PC1XX_SPDIF_SPDCON);		 // DMA
	}
	else
	{
		//writel(S5PC1XX_SPDIF_SPDCON_SOFTWARE_RESET_EN, s5pc1xx_spdif.regs + S5PC1XX_SPDIF_SPDCON);	 // Sw Reset
		//mdelay(50);
		writel(S5PC1XX_SPDIF_SPDCLKCON_SPDIFOUT_POWER_OFF, s5pc1xx_spdif.regs + S5PC1XX_SPDIF_SPDCLKCON);	 // SPDIFOUT clock power off
	}
}
	
	

//static int s5pc1xx_spdif_suspend(struct platform_device *dev,
//struct snd_soc_dai *dai)
//{
//	clk_disable(s5pc1xx_spdif.spdif_clk);
//	clk_disable(s5pc1xx_spdif.spdif_sclk);
//	s3cdbg("Entered %s\n", __FUNCTION__);
//	return 0;
//}
//
//static int s5pc1xx_spdif_resume(struct platform_device *pdev,
//struct snd_soc_dai *dai)
//{
//	clk_enable(s5pc1xx_spdif.spdif_clk);
//	clk_enable(s5pc1xx_spdif.spdif_sclk);	
//	s3cdbg("Entered %s\n", __FUNCTION__);
//	return 0;
//}
 
