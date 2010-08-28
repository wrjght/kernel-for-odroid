/*
 * lpam-dma.c  --  LP-Audio Mode Dma driver
 *
 * Copyright (C) 2009, Samsung Elect. Ltd.
 * 	Jaswinder Singh <jassisinghbrar@gmail.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include <asm/dma.h>
#include <asm/io.h>
#include <mach/hardware.h>
#include <mach/dma.h>
#include <mach/audio.h>

#include "s5p-i2s-lp.h"

#define ST_RUNNING		(1<<0)
#define ST_OPENED		(1<<1)

static const struct snd_pcm_hardware lpam_dma_hardware = {
	.info = SNDRV_PCM_INFO_INTERLEAVED |
		    SNDRV_PCM_INFO_BLOCK_TRANSFER |
		    SNDRV_PCM_INFO_MMAP |
		    SNDRV_PCM_INFO_MMAP_VALID |
		    SNDRV_PCM_INFO_PAUSE |
		    SNDRV_PCM_INFO_RESUME,
	.formats = SNDRV_PCM_FMTBIT_S16_LE |
		    SNDRV_PCM_FMTBIT_U16_LE |
		    SNDRV_PCM_FMTBIT_S24_LE |
		    SNDRV_PCM_FMTBIT_U24_LE |
		    SNDRV_PCM_FMTBIT_U8 |
		    SNDRV_PCM_FMTBIT_S8,
	.channels_min = 2,
	.channels_max = 2,
	.buffer_bytes_max = MAX_LP_BUFF,
	.period_bytes_min = LP_DMA_PERIOD,
	.period_bytes_max = LP_DMA_PERIOD,
	.periods_min = 1,
	.periods_max = 2,
	.fifo_size = 64,
};

struct lpam_runtime_data {
	spinlock_t lock;
	int state;
};

static void lpam_dma_done(void *id, int bytes_xfer)
{
	struct snd_pcm_substream *substream = id;
	struct lpam_runtime_data *prtd = substream->runtime->private_data;

	s3cdbg("%s:%d\n", __func__, __LINE__);

	if (prtd && (prtd->state & ST_RUNNING))
		snd_pcm_period_elapsed(substream);
}

static int lpam_dma_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct s5p_i2s_lp_info *lpdma = rtd->dai->cpu_dai->dma_data;

	s3cdbg("Entered %s\n", __FUNCTION__);

	if (params_buffer_bytes(params) !=
				lpam_dma_hardware.buffer_bytes_max) {
		s3cdbg("Use full buffer in lowpower playback mode!");
		return -EINVAL;
	}

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
	runtime->dma_bytes = params_buffer_bytes(params);

	lpdma->setcallbk(lpam_dma_done, params_period_bytes(params));
	lpdma->enqueue((void *)substream);

	s3cdbg("DmaAddr=@%x Total=%lubytes PrdSz=%u #Prds=%u\n",
			runtime->dma_addr, runtime->dma_bytes, 
			params_period_bytes(params), runtime->hw.periods_min);

	return 0;
}

static int lpam_dma_hw_free(struct snd_pcm_substream *substream)
{
	s3cdbg("Entered %s\n", __FUNCTION__);

	snd_pcm_set_runtime_buffer(substream, NULL);

	return 0;
}

static int lpam_dma_prepare(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct s5p_i2s_lp_info *lpdma = rtd->dai->cpu_dai->dma_data;
	s3cdbg("Entered %s\n", __FUNCTION__);

	/* flush the DMA channel */
	lpdma->ctrl(LPAM_DMA_STOP);

	return 0;
}

static int lpam_dma_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct lpam_runtime_data *prtd = substream->runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct s5p_i2s_lp_info *lpdma = rtd->dai->cpu_dai->dma_data;
	int ret = 0;

	s3cdbg("Entered %s\n", __FUNCTION__);

	spin_lock(&prtd->lock);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_RESUME:
		prtd->state |= ST_RUNNING;
		break;

	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		prtd->state |= ST_RUNNING;
		lpdma->ctrl(LPAM_DMA_START);
		break;

	case SNDRV_PCM_TRIGGER_SUSPEND:
		prtd->state &= ~ST_RUNNING;
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		prtd->state &= ~ST_RUNNING;
		lpdma->ctrl(LPAM_DMA_STOP);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	spin_unlock(&prtd->lock);

	return ret;
}

static snd_pcm_uframes_t 
	lpam_dma_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct lpam_runtime_data *prtd = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct s5p_i2s_lp_info *lpdma = rtd->dai->cpu_dai->dma_data;
	dma_addr_t src, dst;
	unsigned long res;

	spin_lock(&prtd->lock);

	lpdma->getpos(&src, &dst);

	res = src - runtime->dma_addr;

	spin_unlock(&prtd->lock);

	//s3cdbg("Pointer %x %x\n", src, dst);

	return bytes_to_frames(substream->runtime, res);
}

static int lpam_dma_mmap(struct snd_pcm_substream *substream,
	struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	unsigned long size, offset;
	int ret;

	s3cdbg("Entered %s\n", __FUNCTION__);

	/* From snd_pcm_lib_mmap_iomem */
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	vma->vm_flags |= VM_IO;
	size = vma->vm_end - vma->vm_start;
	offset = vma->vm_pgoff << PAGE_SHIFT;
	ret = io_remap_pfn_range(vma, vma->vm_start,
			(runtime->dma_addr + offset) >> PAGE_SHIFT,
			size, vma->vm_page_prot);

	return ret;
}

static int lpam_dma_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct lpam_runtime_data *prtd;

	s3cdbg("Entered %s\n", __FUNCTION__);

	snd_soc_set_runtime_hwparams(substream, &lpam_dma_hardware);

	prtd = kzalloc(sizeof(struct lpam_runtime_data), GFP_KERNEL);
	if (prtd == NULL)
		return -ENOMEM;

	spin_lock_init(&prtd->lock);

	runtime->private_data = prtd;

	return 0;
}

static int lpam_dma_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct lpam_runtime_data *prtd = runtime->private_data;

	s3cdbg("Entered %s, prtd = %p\n", __FUNCTION__, prtd);

	if (prtd)
		kfree(prtd);

	return 0;
}

static struct snd_pcm_ops lpam_dma_ops = {
	.open		= lpam_dma_open,
	.close		= lpam_dma_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.trigger	= lpam_dma_trigger,
	.pointer	= lpam_dma_pointer,
	.mmap		= lpam_dma_mmap,
	.hw_params	= lpam_dma_hw_params,
	.hw_free	= lpam_dma_hw_free,
	.prepare	= lpam_dma_prepare,
};

static int lpam_dma_preallocate_buffer(struct snd_pcm *pcm, int stream)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;

	s3cdbg("Entered %s\n", __FUNCTION__);

	buf->dev.dev = pcm->card->dev;
	buf->private_data = NULL;

	/* Assign PCM buffer pointers */
	buf->dev.type = SNDRV_DMA_TYPE_CONTINUOUS;
	buf->addr = LP_TXBUFF_ADDR;
	buf->bytes = lpam_dma_hardware.buffer_bytes_max;
	buf->area = (unsigned char *)ioremap(buf->addr, buf->bytes);

	s3cdbg("Preallocate buffer(%s):  VA-%p  PA-%X  %ubytes\n", 
			stream ? "Capture": "Playback", 
			buf->area, buf->addr, buf->bytes);
	return 0;
}

static void lpam_dma_free_buffers(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;

	s3cdbg("Entered %s\n", __FUNCTION__);

	substream = pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream;
	if (!substream)
		return;

	buf = &substream->dma_buffer;
	if (!buf->area)
		return;;

	iounmap(buf->area);

	buf->area = NULL;
	buf->addr = 0;
}

static u64 lpam_dma_mask = DMA_32BIT_MASK;

static int lpam_dma_new(struct snd_card *card, 
	struct snd_soc_dai *dai, struct snd_pcm *pcm)
{
	int ret = 0;

	s3cdbg("Entered %s\n", __FUNCTION__);

	if (!card->dev->dma_mask)
		card->dev->dma_mask = &lpam_dma_mask;
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = 0xffffffff;

	if (dai->playback.channels_min) {
		ret = lpam_dma_preallocate_buffer(pcm,
			SNDRV_PCM_STREAM_PLAYBACK);
		if (ret)
			goto out;
	}

 out:
	return ret;
}

struct snd_soc_platform lpam_soc_platform = {
	.name = "s5p-lp-audio",
	.pcm_ops = &lpam_dma_ops,
	.pcm_new = lpam_dma_new,
	.pcm_free = lpam_dma_free_buffers,
};
EXPORT_SYMBOL_GPL(lpam_soc_platform);

static int __init s5p_soc_platform_init(void)
{
	return snd_soc_register_platform(&lpam_soc_platform);
}
module_init(s5p_soc_platform_init);

static void __exit s5p_soc_platform_exit(void)
{
	snd_soc_unregister_platform(&lpam_soc_platform);
}
module_exit(s5p_soc_platform_exit);

MODULE_AUTHOR("Jaswinder Singh, jassi.brar@samsung.com");
MODULE_DESCRIPTION("Samsung S5P LP-Audio DMA module");
MODULE_LICENSE("GPL");
