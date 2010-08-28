#################################################
# Makefile for Image Rotator 
# 2009 (C) Samsung Electronics 
# Author : Jonghun Han <jonghun.han@samsung.com>
#################################################

obj-$(CONFIG_CPU_S5PC100)	+= s3c_rotator.o
obj-$(CONFIG_CPU_S3C6410)	+= s3c_rotator.o
obj-$(CONFIG_CPU_S5PC110_EVT0_ERRATA)	+= s3c_rotator.o
obj-$(CONFIG_CPU_S5PC110_EVT1)	+= s3c_rotator2.o
obj-$(CONFIG_CPU_S5P6442)	+= s3c_rotator2.o

EXTRA_CFLAGS += -Idrivers/media/video

