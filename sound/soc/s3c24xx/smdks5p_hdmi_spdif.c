/*
 *  smdks5p_hdmi_spdif.c
 *
 *  Copyright (C) 2009 Samsung Electronics Co.Ltd
 *  Copyright (C) 2009 Kwak Hyun Min <hyunmin.kwak@samsung.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */
 
#include <sound/soc-dapm.h>
	 
#include <plat/gpio-cfg.h> 
#include <mach/gpio.h>

#include "s5p-spdif.h"
#include "s3c-dma.h"

#ifdef CONFIG_SND_DEBUG
#define s3cdbg(x...) printk(x)
#else
#define s3cdbg(x...)
#endif

struct s5p_spdif_info s5p_spdif;


/*
 * SPDIF supports various sampling rates of 32KHz~192KHz.
 * But currently it supports 48KHz only. 
 */
#define S5P_HDMI_SPDIF_RATES \
	(SNDRV_PCM_RATE_48000)

/*
 * SPDIF supports formats of 16, 20 24 bits.
 * But currently it supports 16 bits only. 
 */
#define S5P_HDMI_SPDIF_FORMATS \
	(SNDRV_PCM_FMTBIT_S16_LE)
	
struct snd_soc_dai s5p_hdmi_spdif_dai[] = {
	{
		.name = "HDMI-SPDIF Codec",
		.id = 0,
		.playback = {
			.stream_name  = "Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates        = S5P_HDMI_SPDIF_RATES,
			.formats      = S5P_HDMI_SPDIF_FORMATS,
		},
		.capture = {
			.stream_name  = "Capture",
			.channels_min = 2,
			.channels_max = 2,
			.rates        = S5P_HDMI_SPDIF_RATES,
			.formats      = S5P_HDMI_SPDIF_FORMATS,
		},
		.ops = {
			 .hw_params  = NULL,
			 .set_fmt    = NULL,
			 .set_clkdiv = NULL,
			 .set_pll    = NULL,
		 },
	},
};

static const struct snd_soc_dapm_widget spdif_dapm_widgets[] = {
	SND_SOC_DAPM_MIXER("VOUT1L",SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("VOUT1R",SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("VOUT2L",SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("VOUT2R",SND_SOC_NOPM, 0, 0, NULL, 0),
	
	SND_SOC_DAPM_LINE("SPDIF Front Jack", NULL),
	SND_SOC_DAPM_LINE("SPDIF Rear Jack", NULL),
};

static const struct snd_soc_dapm_route audio_map[] = {
	{ "SPDIF Front Jack", NULL, "VOUT1L" },
	{ "SPDIF Front Jack", NULL, "VOUT1R" },

	{ "SPDIF Rear Jack", NULL, "VOUT2L" },
	{ "SPDIF Rear Jack", NULL, "VOUT2R" },
};

static int smdks5p_spdif_init(struct snd_soc_codec *codec)
{
	int i;

	s3cdbg("Entered %s\n", __FUNCTION__);
	
	/* Add smdks5p specific widgets */
	snd_soc_dapm_new_controls(codec, spdif_dapm_widgets,ARRAY_SIZE(spdif_dapm_widgets));

	/* set up smdks5p specific audio paths */
	snd_soc_dapm_add_routes(codec, audio_map,ARRAY_SIZE(audio_map));

	snd_soc_dapm_new_widgets(codec);
	
	/* No jack detect - mark all jacks as enabled */
	for (i = 0; i < ARRAY_SIZE(spdif_dapm_widgets); i++) {
		snd_soc_dapm_enable_pin(codec, spdif_dapm_widgets[i].name); 
	}

	snd_soc_dapm_sync(codec);

	return 0;
}

static int smdks5p_spdif_hw_params(struct snd_pcm_substream *substream,
				    struct snd_pcm_hw_params *params)
{
	// configure gpio for spdif
	s3cdbg("Entered %s: rate=%d\n", __FUNCTION__, params_rate(params));

#ifdef CONFIG_CPU_S5PC100
	s3c_gpio_cfgpin(S5PC1XX_GPG3(5), S3C_GPIO_SFN(5));	//GPG3CON[5] spdif_0_out
	s3c_gpio_setpull(S5PC1XX_GPG3(5), S3C_GPIO_PULL_UP);
	s3c_gpio_cfgpin(S5PC1XX_GPG3(6), S3C_GPIO_SFN(5));	//GPG3CON[5] spdif_extcal
	s3c_gpio_setpull(S5PC1XX_GPG3(6), S3C_GPIO_PULL_UP);	
#else 
#ifdef CONFIG_CPU_S5PC110
	s3c_gpio_cfgpin(S5PC11X_GPC1(0), S3C_GPIO_SFN(3));	//GPG3CON[5] spdif_0_out
	s3c_gpio_setpull(S5PC11X_GPC1(0), S3C_GPIO_PULL_UP);
	s3c_gpio_cfgpin(S5PC11X_GPC1(1), S3C_GPIO_SFN(3));	//GPG3CON[5] spdif_extcal
	s3c_gpio_setpull(S5PC11X_GPC1(1), S3C_GPIO_PULL_UP);	
#endif
#endif
	return 0;
}

static struct snd_soc_ops smdks5p_spdif_ops = {
	.hw_params = smdks5p_spdif_hw_params,
};

static int spdif_probe(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec;
	
	int ret = 0;
	
	codec = kzalloc(sizeof(struct snd_soc_codec), GFP_KERNEL);
	if (codec == NULL)
		return -ENOMEM;
		
	socdev->codec = codec;	

	mutex_init(&codec->mutex);
	INIT_LIST_HEAD(&codec->dapm_widgets);
	INIT_LIST_HEAD(&codec->dapm_paths);

	codec->name    = "HDMI-SPDIF";
	codec->owner   = THIS_MODULE;
	codec->dai     = s5p_hdmi_spdif_dai;
	codec->num_dai = ARRAY_SIZE(s5p_hdmi_spdif_dai);
	
	/* register pcms */
	ret = snd_soc_new_pcms(socdev, -1, NULL);

	if (ret < 0) {
		printk(KERN_ERR "spdif: failed to create pcms\n");
		return -1;
	}

	ret = snd_soc_init_card(socdev);
	if (ret < 0) {
		printk(KERN_ERR "spdif: failed to register card\n");
		return -1;
	}
	return 0;
}


static int spdif_remove(struct platform_device *pdev)
{
	
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->codec;

	s3cdbg("Entered %s\n", __FUNCTION__);
	
	snd_soc_free_pcms(socdev);
	kfree(codec->private_data);
	kfree(codec);
	return 0;
}


struct snd_soc_codec_device soc_codec_dev_spdif = {
	.probe  = spdif_probe,
	.remove = spdif_remove,
};

static struct snd_soc_dai_link smdks5p_dai[] = {
	{
		.name = "HDMI-SPDIF",
		.stream_name = "HDMI-SPDIF Playback",
		.cpu_dai = &s5p_spdif_dai,
		.codec_dai = &s5p_hdmi_spdif_dai[0],
		.init = smdks5p_spdif_init,
		.ops = &smdks5p_spdif_ops,
	},
};

static struct snd_soc_card smdks5p = {
	.name = "smdks5p",	
	.platform = &s3c24xx_soc_platform,
	.dai_link = smdks5p_dai,
	.num_links = ARRAY_SIZE(smdks5p_dai),
};

static struct snd_soc_device smdks5p_snd_devdata = {
	.card = &smdks5p,
	.codec_dev = &soc_codec_dev_spdif,
};

static struct platform_device *smdks5p_snd_device;


static int __init smdks5p_audio_init(void)
{
	int ret;

	snd_soc_register_dais(s5p_hdmi_spdif_dai, ARRAY_SIZE(s5p_hdmi_spdif_dai));

	s3cdbg("Entered %s\n", __FUNCTION__);
	smdks5p_snd_device = platform_device_alloc("soc-audio", 0);
	if (!smdks5p_snd_device)
		return -ENOMEM;

	platform_set_drvdata(smdks5p_snd_device, &smdks5p_snd_devdata);
	smdks5p_snd_devdata.dev = &smdks5p_snd_device->dev;
	ret = platform_device_add(smdks5p_snd_device);
	if (ret)
		platform_device_put(smdks5p_snd_device);
	
	return ret;
}


static void __exit smdks5p_audio_exit(void)
{
	snd_soc_unregister_dais(s5p_hdmi_spdif_dai, ARRAY_SIZE(s5p_hdmi_spdif_dai));
	platform_device_unregister(smdks5p_snd_device);
}


module_init(smdks5p_audio_init);
module_exit(smdks5p_audio_exit);

/* Module information */
MODULE_DESCRIPTION("ALSA SoC SMDKS5P HDMI-SPDIF");
MODULE_LICENSE("GPL");
