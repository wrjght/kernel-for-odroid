# Machine Support
snd-soc-smdk6410-s5m8751-objs := smdk6410_s5m8751.o
snd-soc-smdk6410-wm8580-objs  := smdk6410_wm8580.o
snd-soc-smdk6410-wm9713-objs  := smdk6410_wm9713.o

snd-soc-smdks5p-wm8580-objs  := smdks5p_wm8580.o
snd-soc-smdkc100-s5m8751-objs := smdkc100_s5m8751.o
snd-soc-smdkc100-wm9713-objs  := smdkc100_wm9713.o

snd-soc-universal-ak4671-objs := universal_ak4671.o

obj-$(CONFIG_SND_S3C64XX_SOC) += s3c-pcm_smdk6410.o
obj-$(CONFIG_SND_S3C_I2S_V40)  += s3c-i2s-v40.o

obj-$(CONFIG_SND_S3C_I2S_V50)  += s3c-pcm-lp.o
obj-$(CONFIG_SND_S3C_I2S_V50)  += s3c-i2s.o

obj-$(CONFIG_SND_S3C_AC97)     += s3c-ac97.o
obj-$(CONFIG_SND_S3C_AC97)     += s3c-pcm.o

obj-$(CONFIG_SND_S5P_SPDIF)   += s5p-spdif.o
obj-$(CONFIG_SND_S5P_SPDIF)   += s3c-pcm-lp.o

# S3C6410 has AC97 and I2S(v-3.2 and 4.0) controller.
# SMDK6410 has WM8580 and WM9713 onboard and S5M8751 as a module.
obj-$(CONFIG_SND_SMDK6410_S5M8751) += snd-soc-smdk6410-s5m8751.o
obj-$(CONFIG_SND_SMDK6410_WM8580)  += snd-soc-smdk6410-wm8580.o
obj-$(CONFIG_SND_SMDK6410_WM9713)  += snd-soc-smdk6410-wm9713.o

# S5PC100 has AC97 and I2S(v-3.2 and 5.0) controller.
# SMDKC100 has WM8580 and WM9713 onboard and S5M8751 as a module.
obj-$(CONFIG_SND_SMDKC100_S5M8751) += snd-soc-smdkc100-s5m8751.o
obj-$(CONFIG_SND_SMDKC100_WM8580)  += snd-soc-smdks5p-wm8580.o
obj-$(CONFIG_SND_SMDKC100_WM9713)  += snd-soc-smdkc100-wm9713.o

obj-$(CONFIG_SND_SMDKC110_WM8580)  += snd-soc-smdks5p-wm8580.o

obj-$(CONFIG_SND_SMDK6442_WM8580)  += snd-soc-smdks5p-wm8580.o

obj-$(CONFIG_SND_S5PC100_SOC_UNIVERSAL_AK4671) += snd-soc-universal-ak4671.o

# S5PC100 and S5PC110 has SPDIF transmitter for HDMI Audio
obj-$(CONFIG_SND_SMDKC100_HDMI_SPDIF)   += smdks5p_hdmi_spdif.o
obj-$(CONFIG_SND_SMDKC110_HDMI_SPDIF)   += smdks5p_hdmi_spdif.o

