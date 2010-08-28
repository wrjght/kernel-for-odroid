/*
 * s5p-i2s-lp.c  --  ALSA Soc Audio Layer
 *
 * (c) 2009 Samsung Electronics Co. Ltd
 *   - Jaswinder Singh Brar <jassi.brar@samsung.com>
 *     Derived from Ben Dooks' driver for s3c24xx
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include <asm/dma.h>
#include <mach/map.h>
#include <mach/s3c-dma.h>
#include <plat/regs-clock.h>

#include "s5p-i2s-lp.h"

static struct lpam_i2s_info {
	void __iomem  *regs;
	struct clk    *iis_clk;
	struct clk    *audio_bus;
	u32           iiscon;
	u32           iismod;
	u32           iisfic;
	u32           iispsr;
	u32           slave;
	u32           clk_rate;
} lpam_i2s;

static struct lpam_i2s_pdata {
	u32 *p_rate;
	void *token;
	spinlock_t lock;
	unsigned  dma_prd;
	void (*cb)(void *dt, int bytes_xfer);
} lpam_pdata;

u32 *lpam_clk_rate;
EXPORT_SYMBOL_GPL(lpam_clk_rate);

#define dump_i2s()	do{	\
				printk("%s:%s:%d\t", __FILE__, __func__, __LINE__);	\
				printk("\tS3C_IISCON : %x", readl(lpam_i2s.regs + S3C_IISCON));		\
				printk("\tS3C_IISMOD : %x\n", readl(lpam_i2s.regs + S3C_IISMOD));	\
				printk("\tS3C_IISFIC : %x", readl(lpam_i2s.regs + S3C_IISFICS));	\
				printk("\tS3C_IISPSR : %x\n", readl(lpam_i2s.regs + S3C_IISPSR));	\
				printk("\tS3C_IISAHB : %x\n", readl(lpam_i2s.regs + S3C_IISAHB));	\
				printk("\tS3C_IISSTR : %x\n", readl(lpam_i2s.regs + S3C_IISSTR));	\
				printk("\tS3C_IISSIZE : %x\n", readl(lpam_i2s.regs + S3C_IISSIZE));	\
				printk("\tS3C_IISADDR0 : %x\n", readl(lpam_i2s.regs + S3C_IISADDR0));	\
			}while(0)

	/********************
	 * Internal DMA i/f *
	 ********************/
static void lpam_dma_getpos(dma_addr_t *src, dma_addr_t *dst)
{
	*dst = S3C_IIS_PABASE + S3C_IISTXDS;
	*src = LP_TXBUFF_ADDR + (readl(lpam_i2s.regs + S3C_IISTRNCNT) & 0xffffff) * 4;
}

static int lpam_dma_enqueue(void *id)
{
	u32 val;

	spin_lock(&lpam_pdata.lock);
	lpam_pdata.token = id;
	spin_unlock(&lpam_pdata.lock);

	s3cdbg("%s: %d@%x\n", __func__, MAX_LP_BUFF, LP_TXBUFF_ADDR);

	val = LP_TXBUFF_ADDR + lpam_pdata.dma_prd;
	//val |= S3C_IISADDR_ENSTOP;
	writel(val, lpam_i2s.regs + S3C_IISADDR0);

	val = readl(lpam_i2s.regs + S3C_IISSTR);
	val = LP_TXBUFF_ADDR;
	writel(val, lpam_i2s.regs + S3C_IISSTR);

	val = readl(lpam_i2s.regs + S3C_IISSIZE);
	val &= ~(S3C_IISSIZE_TRNMSK << S3C_IISSIZE_SHIFT);
	val |= (((MAX_LP_BUFF >> 2) & S3C_IISSIZE_TRNMSK) << S3C_IISSIZE_SHIFT);
	writel(val, lpam_i2s.regs + S3C_IISSIZE);

	val = readl(lpam_i2s.regs + S3C_IISAHB);
	val |= S3C_IISAHB_INTENLVL0;
	writel(val, lpam_i2s.regs + S3C_IISAHB);

	return 0;
}

static void lpam_dma_setcallbk(void (*cb)(void *id, int result), unsigned prd)
{
	spin_lock(&lpam_pdata.lock);
	lpam_pdata.cb = cb;
	lpam_pdata.dma_prd = prd;
	spin_unlock(&lpam_pdata.lock);

	s3cdbg("%s:%d dma_period=%d\n", __func__, __LINE__, lpam_pdata.dma_prd);
}

static void lpam_dma_ctrl(int op)
{
	u32 val;

	spin_lock(&lpam_pdata.lock);

	val = readl(lpam_i2s.regs + S3C_IISAHB);
	val &= ~(S3C_IISAHB_INTENLVL0 | S3C_IISAHB_DMAEN);

	if (op == LPAM_DMA_START)
		val |= S3C_IISAHB_INTENLVL0 | S3C_IISAHB_DMAEN;

	writel(val, lpam_i2s.regs + S3C_IISAHB);

	spin_unlock(&lpam_pdata.lock);
}

static struct s5p_i2s_lp_info lpam_dma_data = {
	.getpos = lpam_dma_getpos,
	.enqueue = lpam_dma_enqueue,
	.setcallbk = lpam_dma_setcallbk,
	.ctrl = lpam_dma_ctrl,
};

static void s3c_snd_txctrl(int on)
{
	u32 iiscon;

	iiscon  = readl(lpam_i2s.regs + S3C_IISCON);

	if(on){
		iiscon |= S3C_IISCON_I2SACTIVE;
		iiscon  &= ~S3C_IISCON_TXCHPAUSE;
		iiscon  &= ~S3C_IISCON_TXSDMAPAUSE;
		iiscon  |= S3C_IISCON_TXSDMACTIVE;
		writel(iiscon,  lpam_i2s.regs + S3C_IISCON);
	}else{
		if(!(iiscon & S3C_IISCON_RXDMACTIVE)) /* Stop only if RX not active */
			iiscon &= ~S3C_IISCON_I2SACTIVE;
		iiscon  |= S3C_IISCON_TXCHPAUSE;
		iiscon  |= S3C_IISCON_TXSDMAPAUSE;
		iiscon  &= ~S3C_IISCON_TXSDMACTIVE;
		writel(iiscon,  lpam_i2s.regs + S3C_IISCON);
	}
}

#define msecs_to_loops(t) (loops_per_jiffy / 1000 * HZ * t)

/*
 * Wait for the LR signal to allow synchronisation to the L/R clock
 * from the codec. May only be needed for slave mode.
 */
static int s3c_snd_lrsync(void)
{
	u32 iiscon;
	unsigned long loops = msecs_to_loops(5);

	pr_debug("Entered %s\n", __func__);

	while (--loops) {
		iiscon = readl(lpam_i2s.regs + S3C_IISCON);
		if (iiscon & S3C_IISCON_LRI)
			break;

		cpu_relax();
	}

	if (!loops) {
		printk(KERN_ERR "%s: timeout\n", __func__);
		return -ETIMEDOUT;
	}

	return 0;
}

/*
 * Set s3c_ I2S DAI format
 */
static int s3c_i2s_set_fmt(struct snd_soc_dai *cpu_dai,
		unsigned int fmt)
{
	u32 iismod;

	iismod = readl(lpam_i2s.regs + S3C_IISMOD);
	iismod &= ~S3C_IISMOD_SDFMASK;

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		lpam_i2s.slave = 1;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		lpam_i2s.slave = 0;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		iismod &= ~S3C_IISMOD_MSB;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		iismod |= S3C_IISMOD_MSB;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		iismod |= S3C_IISMOD_LSB;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		iismod &= ~S3C_IISMOD_LRP;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		iismod |= S3C_IISMOD_LRP;
		break;
	case SND_SOC_DAIFMT_IB_IF:
	case SND_SOC_DAIFMT_IB_NF:
	default:
		printk("Inv-combo(%d) not supported!\n", fmt & SND_SOC_DAIFMT_FORMAT_MASK);
		return -EINVAL;
	}

	writel(iismod, lpam_i2s.regs + S3C_IISMOD);
	return 0;
}

static int s3c_i2s_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
	u32 iismod;

	cpu_dai->dma_data = &lpam_dma_data;

	/* Working copies of register */
	iismod = readl(lpam_i2s.regs + S3C_IISMOD);
	iismod &= ~(S3C_IISMOD_BLCMASK | S3C_IISMOD_BLCSMASK);

	/* RFS & BFS are set by dai_link(machine specific) code via set_clkdiv */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S8:
		iismod |= S3C_IISMOD_8BIT | S3C_IISMOD_S8BIT;
 		break;
 	case SNDRV_PCM_FORMAT_S16_LE:
 		iismod |= S3C_IISMOD_16BIT | S3C_IISMOD_S16BIT;
 		break;
 	case SNDRV_PCM_FORMAT_S24_LE:
 		iismod |= S3C_IISMOD_24BIT | S3C_IISMOD_S24BIT;
 		break;
	default:
		return -EINVAL;
 	}
 
	writel(iismod, lpam_i2s.regs + S3C_IISMOD);

	return 0;
}

static int s3c_i2s_startup(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	u32 iiscon, iisfic;

	iiscon = readl(lpam_i2s.regs + S3C_IISCON);

	/* FIFOs must be flushed before enabling PSR and other MOD bits, so we do it here. */
	if(iiscon & S3C_IISCON_TXSDMACTIVE)
		return 0;

	iisfic = readl(lpam_i2s.regs + S3C_IISFICS);
	iisfic |= S3C_IISFIC_TFLUSH;
	writel(iisfic, lpam_i2s.regs + S3C_IISFICS);

	do{
   	   cpu_relax();
	   //iiscon = __raw_readl(lpam_i2s.regs + S3C_IISCON);
	}while((__raw_readl(lpam_i2s.regs + S3C_IISFIC) >> 8) & 0x7f);

	iisfic = readl(lpam_i2s.regs + S3C_IISFICS);
	iisfic &= ~S3C_IISFIC_TFLUSH;
	writel(iisfic, lpam_i2s.regs + S3C_IISFICS);

	return 0;
}

static int s3c_i2s_trigger(struct snd_pcm_substream *substream, int cmd, struct snd_soc_dai *dai)
{
	int ret = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_RESUME:
			break;

	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (lpam_i2s.slave) {
			ret = s3c_snd_lrsync();
			if (ret)
				goto exit_err;
		}

		s3c_snd_txctrl(1);
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		s3c_snd_txctrl(0);
		break;

	default:
		ret = -EINVAL;
		break;
	}

exit_err:
	return ret;
}

/*
 * Set s3c_ Clock source
 * Since, we set frequencies using PreScaler and BFS, RFS, we select input clock source to the IIS here.
 */
static int s3c_i2s_set_sysclk(struct snd_soc_dai *cpu_dai,
	int clk_id, unsigned int freq, int dir)
{
	struct clk *clk;
	u32 iismod = readl(lpam_i2s.regs + S3C_IISMOD);

	switch (clk_id) {
	case S3C_CLKSRC_PCLK: /* IIS-IP is Master and derives its clocks from PCLK */
		if(lpam_i2s.slave)
			return -EINVAL;
		iismod &= ~S3C_IISMOD_IMSMASK;
		iismod |= clk_id;
		lpam_i2s.clk_rate = clk_get_rate(lpam_i2s.iis_clk);
		break;

	case S3C_CLKSRC_CLKAUDIO: /* IIS-IP is Master and derives its clocks from I2SCLKD2 */
		if(lpam_i2s.slave)
			return -EINVAL;
		iismod &= ~S3C_IISMOD_IMSMASK;
		iismod |= clk_id;
		clk = clk_get(NULL, RATESRCCLK);
		if (IS_ERR(clk)) {
			printk("failed to get %s\n", RATESRCCLK);
			return -EBUSY;
		}
		clk_disable(clk);
		switch (freq) {
		case 8000:
		case 16000:
		case 32000:
		case 48000:
		case 64000:
		case 96000:
			clk_set_rate(clk, 49152000);
			break;
		case 11025:
		case 22050:
		case 44100:
		case 88200:
		default:
			clk_set_rate(clk, 67738000);
			break;
		}
		clk_enable(clk);
		lpam_i2s.clk_rate = clk_get_rate(lpam_i2s.audio_bus);
		s3cdbg("Setting FOUTepll to %dHz", lpam_i2s.clk_rate);
		clk_put(clk);
		break;

	case S3C_CLKSRC_SLVPCLK: /* IIS-IP is Slave, but derives its clocks from PCLK */
	case S3C_CLKSRC_I2SEXT:  /* IIS-IP is Slave and derives its clocks from the WM8580 Codec Chip via I2SCLKD2 */
		iismod &= ~S3C_IISMOD_IMSMASK;
		iismod |= clk_id;
		break;

	case S3C_CDCLKSRC_INT:
		iismod &= ~S3C_IISMOD_CDCLKCON;
		break;

	case S3C_CDCLKSRC_EXT:
		iismod |= S3C_IISMOD_CDCLKCON;
		break;

	default:
		return -EINVAL;
	}

	writel(iismod, lpam_i2s.regs + S3C_IISMOD);
	return 0;
}

/*
 * Set s3c_ Clock dividers
 * NOTE: NOT all combinations of RFS, BFS and BCL are supported! XXX
 * Machine specific(dai-link) code must consider that while setting MCLK and BCLK in this function. XXX
 */
/* XXX BLC(bits-per-channel) --> BFS(bit clock shud be >= FS*(Bit-per-channel)*2) XXX */
/* XXX BFS --> RFS_VAL(must be a multiple of BFS)                                 XXX */
/* XXX RFS_VAL & SRC_CLK --> Prescalar Value(SRC_CLK / RFS_VAL / fs - 1)          XXX */
static int s3c_i2s_set_clkdiv(struct snd_soc_dai *cpu_dai,
	int div_id, int div)
{
	u32 reg;

	switch (div_id) {
	case S3C_DIV_MCLK:
		reg = readl(lpam_i2s.regs + S3C_IISMOD) & ~S3C_IISMOD_RFSMASK;
		switch(div) {
		case 256: div = S3C_IISMOD_256FS; break;
		case 512: div = S3C_IISMOD_512FS; break;
		case 384: div = S3C_IISMOD_384FS; break;
		case 768: div = S3C_IISMOD_768FS; break;
		default: return -EINVAL;
		}
		writel(reg | div, lpam_i2s.regs + S3C_IISMOD);
		break;
	case S3C_DIV_BCLK:
		reg = readl(lpam_i2s.regs + S3C_IISMOD) & ~S3C_IISMOD_BFSMASK;
		switch(div) {
		case 16: div = S3C_IISMOD_16FS; break;
		case 24: div = S3C_IISMOD_24FS; break;
		case 32: div = S3C_IISMOD_32FS; break;
		case 48: div = S3C_IISMOD_48FS; break;
		default: return -EINVAL;
		}
		writel(reg | div, lpam_i2s.regs + S3C_IISMOD);
		break;
	case S3C_DIV_PRESCALER:
		reg = readl(lpam_i2s.regs + S3C_IISPSR) & ~S3C_IISPSR_PSRAEN;
		writel(reg, lpam_i2s.regs + S3C_IISPSR);
		reg = readl(lpam_i2s.regs + S3C_IISPSR) & ~S3C_IISPSR_PSVALA;
		div &= 0x3f;
		writel(reg | (div<<8) | S3C_IISPSR_PSRAEN, lpam_i2s.regs + S3C_IISPSR);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static irqreturn_t s3c_iis_irq(int irqno, void *dev_id)
{
	u32 iiscon, iisahb, val;

	//dump_i2s();

	iisahb  = readl(lpam_i2s.regs + S3C_IISAHB);
	iiscon  = readl(lpam_i2s.regs + S3C_IISCON);
	if(iiscon & S3C_IISCON_FTXSURSTAT) {
		iiscon |= S3C_IISCON_FTXURSTATUS;
		writel(iiscon, lpam_i2s.regs + S3C_IISCON);
		s3cdbg("TX_S underrun interrupt IISCON = 0x%08x\n", readl(lpam_i2s.regs + S3C_IISCON));
	}
	if(iiscon & S3C_IISCON_FTXURSTATUS) {
		iiscon &= ~S3C_IISCON_FTXURINTEN;
		iiscon |= S3C_IISCON_FTXURSTATUS;
		writel(iiscon, lpam_i2s.regs + S3C_IISCON);
		s3cdbg("TX_P underrun interrupt IISCON = 0x%08x\n", readl(lpam_i2s.regs + S3C_IISCON));
	}

	val = 0;
	val |= ((iisahb & S3C_IISAHB_LVL0INT) ? S3C_IISAHB_CLRLVL0 : 0);
	//val |= ((iisahb & S3C_IISAHB_DMAEND) ? S3C_IISAHB_DMACLR : 0);
	if(val){
		iisahb |= val;
		iisahb |= S3C_IISAHB_INTENLVL0;
		writel(iisahb, lpam_i2s.regs + S3C_IISAHB);

		if(iisahb & S3C_IISAHB_LVL0INT){
			val = readl(lpam_i2s.regs + S3C_IISADDR0) - LP_TXBUFF_ADDR; /* current offset */
			val += lpam_pdata.dma_prd; /* Length before next Lvl0 Intr */
			val %= MAX_LP_BUFF; /* Round off at boundary */
			writel(LP_TXBUFF_ADDR + val, lpam_i2s.regs + S3C_IISADDR0); /* Update start address */
		}

		if(iisahb & S3C_IISAHB_DMAEND)
			printk("Didnt expect S3C_IISAHB_DMAEND\n");

		iisahb  = readl(lpam_i2s.regs + S3C_IISAHB);
		iisahb |= S3C_IISAHB_DMAEN;
		writel(iisahb, lpam_i2s.regs + S3C_IISAHB);

		/* Keep callback in the end */
		if(lpam_pdata.cb){
		   val = (iisahb & S3C_IISAHB_LVL0INT) ? 
				lpam_pdata.dma_prd:
				MAX_LP_BUFF;
		   lpam_pdata.cb(lpam_pdata.token, val);
		}
	}

	return IRQ_HANDLED;
}

static void init_i2s(void)
{
	u32 iiscon, iismod, iisahb;

	writel(S3C_IISCON_I2SACTIVE | S3C_IISCON_SWRESET, lpam_i2s.regs + S3C_IISCON);

	iiscon  = readl(lpam_i2s.regs + S3C_IISCON);
	iismod  = readl(lpam_i2s.regs + S3C_IISMOD);
	iisahb  = S3C_IISAHB_DMARLD | S3C_IISAHB_DISRLDINT;

	/* Enable all interrupts to find bugs */
	iiscon |= S3C_IISCON_FRXOFINTEN;
	iismod &= ~S3C_IISMOD_OPMSK;
	iismod |= S3C_IISMOD_OPCCO;

	iiscon &= ~S3C_IISCON_FTXURINTEN;
	iiscon |= S3C_IISCON_FTXSURINTEN;
	iismod |= S3C_IISMOD_TXSLP;
	//iisahb |= S3C_IISAHB_DMARLD | S3C_IISAHB_DISRLDINT;

	writel(iisahb, lpam_i2s.regs + S3C_IISAHB);
	writel(iismod, lpam_i2s.regs + S3C_IISMOD);
	writel(iiscon, lpam_i2s.regs + S3C_IISCON);
}

static void __lpinit(void)
{
#ifdef CONFIG_ARCH_S5PC1XX /* S5PC100 */
	u32 mdsel;

	mdsel = readl(S5P_LPMP_MODE_SEL) & ~0x3;
	mdsel |= (1<<0);
	writel(mdsel, S5P_LPMP_MODE_SEL);

	writel(readl(S5P_CLKGATE_D20) | S5P_CLKGATE_D20_HCLKD2, S5P_CLKGATE_D20);
#else
	//#error PUT SOME CODE HERE !!!
#endif
}

static int s3c_i2s_probe(struct platform_device *pdev,
			     struct snd_soc_dai *dai)
{
	int ret = 0;
	struct clk *cf, *cm;

	/* Already called for one DAI */
	if(lpam_i2s.regs != NULL)
		return 0;

	lpam_clk_rate = &lpam_i2s.clk_rate;
	spin_lock_init(&lpam_pdata.lock);

	__lpinit();

	lpam_i2s.regs = ioremap(S3C_IIS_PABASE, 0x100);
	if (lpam_i2s.regs == NULL)
		return -ENXIO;

	ret = request_irq(S3C_IISIRQ, s3c_iis_irq, 0, "s3c-i2s", pdev);
	if (ret < 0) {
		printk("fail to claim i2s irq , ret = %d\n", ret);
		iounmap(lpam_i2s.regs);
		return -ENODEV;
	}

	lpam_i2s.iis_clk = clk_get(&pdev->dev, "iis");
	if (IS_ERR(lpam_i2s.iis_clk)) {
		printk("failed to get clk(iis)\n");
		goto lb4;
	}
	s3cdbg("Got Clock -> iis\n");
	clk_enable(lpam_i2s.iis_clk);
	lpam_i2s.clk_rate = clk_get_rate(lpam_i2s.iis_clk);

	/* To avoid switching between sources(LP vs NM mode),
	 * we use EXTPRNT as parent clock of audio-bus.
	 */
	lpam_i2s.audio_bus = clk_get(&pdev->dev, "audio-bus");
	if (IS_ERR(lpam_i2s.audio_bus)) {
		printk("failed to get clk(audio-bus)\n");
		goto lb3;
	}
	s3cdbg("Got Audio Bus Clock -> audio-bus\n");

	cm = clk_get(NULL, EXTPRNT);
	if (IS_ERR(cm)) {
		printk("failed to get %s\n", EXTPRNT);
		goto lb2;
	}
	s3cdbg("Got Audio Bus Source Clock -> %s\n", EXTPRNT);

	if(clk_set_parent(lpam_i2s.audio_bus, cm)){
		printk("failed to set %s as parent of audio-bus\n", EXTPRNT);
		goto lb1;
	}
	s3cdbg("Set %s as parent of audio-bus\n", EXTPRNT);

#if defined(CONFIG_MACH_SMDKC110)
	cf = clk_get(NULL, "fout_epll");
	if (IS_ERR(cf)) {
		printk("failed to get fout_epll\n");
		goto lb1;
	}
	s3cdbg("Got Clock -> fout_epll\n");

	if(clk_set_parent(cm, cf)){
		printk("failed to set fout_epll as parent of %s\n", EXTPRNT);
		clk_put(cf);
		goto lb1;
	}
	s3cdbg("Set fout_epll as parent of %s\n", EXTPRNT);
	clk_put(cf);
#endif
	clk_put(cm);

	clk_enable(lpam_i2s.audio_bus);
	lpam_i2s.clk_rate = clk_get_rate(lpam_i2s.audio_bus);

	init_i2s();

	s3c_snd_txctrl(0);

	return 0;

lb1:
	clk_put(cm);
lb2:
	clk_put(lpam_i2s.audio_bus);
lb3:
	clk_disable(lpam_i2s.iis_clk);
	clk_put(lpam_i2s.iis_clk);
lb4:
	free_irq(S3C_IISIRQ, pdev);
	iounmap(lpam_i2s.regs);

	return -ENODEV;
}

static void s3c_i2s_remove(struct platform_device *pdev,
		       struct snd_soc_dai *dai)
{
	/* Already called for one DAI */
	if(lpam_i2s.regs == NULL)
		return;

	writel(0, lpam_i2s.regs + S3C_IISCON);

	clk_disable(lpam_i2s.audio_bus);
	clk_put(lpam_i2s.audio_bus);
	clk_disable(lpam_i2s.iis_clk);
	clk_put(lpam_i2s.iis_clk);
	free_irq(S3C_IISIRQ, pdev);
	iounmap(lpam_i2s.regs);
	lpam_i2s.regs = NULL;
}

#ifdef CONFIG_PM
static int s3c_i2s_suspend(struct snd_soc_dai *cpu_dai)
{
	lpam_i2s.iiscon = readl(lpam_i2s.regs + S3C_IISCON);
	lpam_i2s.iismod = readl(lpam_i2s.regs + S3C_IISMOD);
	lpam_i2s.iisfic = readl(lpam_i2s.regs + S3C_IISFICS);
	lpam_i2s.iispsr = readl(lpam_i2s.regs + S3C_IISPSR);

	clk_disable(lpam_i2s.iis_clk);

	return 0;
}

static int s3c_i2s_resume(struct snd_soc_dai *cpu_dai)
{
	clk_enable(lpam_i2s.iis_clk);

	writel(lpam_i2s.iiscon, lpam_i2s.regs + S3C_IISCON);
	writel(lpam_i2s.iismod, lpam_i2s.regs + S3C_IISMOD);
	writel(lpam_i2s.iisfic, lpam_i2s.regs + S3C_IISFICS);
	writel(lpam_i2s.iispsr, lpam_i2s.regs + S3C_IISPSR);

	return 0;
}
#else
#define s3c_i2s_suspend NULL
#define s3c_i2s_resume NULL
#endif

struct snd_soc_dai lpam_i2s_dai = {
	.name = "lpam-i2s",
	.id = 0,
	.probe = s3c_i2s_probe,
	.remove = s3c_i2s_remove,
	.suspend = s3c_i2s_suspend,
	.resume = s3c_i2s_resume,
	.playback = {
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_96000,
		.formats = SNDRV_PCM_FMTBIT_S8
				| SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE,
	},
	.ops = {
		.hw_params = s3c_i2s_hw_params,
		.startup   = s3c_i2s_startup,
		.trigger   = s3c_i2s_trigger,
		.set_fmt = s3c_i2s_set_fmt,
		.set_clkdiv = s3c_i2s_set_clkdiv,
		.set_sysclk = s3c_i2s_set_sysclk,
	},
};
EXPORT_SYMBOL_GPL(lpam_i2s_dai);

static int __init s5p_i2s_init(void)
{
	return snd_soc_register_dai(&lpam_i2s_dai);
}
module_init(s5p_i2s_init);

static void __exit s5p_i2s_exit(void)
{
	snd_soc_unregister_dai(&lpam_i2s_dai);
}
module_exit(s5p_i2s_exit);

/* Module information */
MODULE_AUTHOR("Jaswinder Singh <jassi.brar@samsung.com>");
MODULE_DESCRIPTION("S5P LPAM I2S SoC Interface");
MODULE_LICENSE("GPL");
