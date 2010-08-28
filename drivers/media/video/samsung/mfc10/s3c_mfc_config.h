/* linux/driver/media/video/mfc/s3c_mfc_config.h
 *
 * Header file for Samsung MFC (Multi Function Codec - FIMV) driver 
 *
 * PyoungJae Jung, Jiun Yu, Copyright (c) 2009 Samsung Electronics 
 * http://www.samsungsemi.com/ 
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _S3C_MFC_CONFIG_H
#define _S3C_MFC_CONFIG_H

#include <asm/types.h>

/* Physical Base Address for the MFC Host I/F Registers */
/* #define S3C6400_BASEADDR_MFC_SFR			0x7e002000 */
/* #define MFC_RESERVED_MEM_START			0x57800000 */

#ifndef S3C_MFC_PHYS_BUFFER_SET
extern dma_addr_t s3c_mfc_phys_buffer;
#endif

#define MFC_RESERVED_MEM_START			s3c_mfc_phys_buffer

/* 
 * Physical Base Address for the MFC Shared Buffer 
 * Shared Buffer = {CODE_BUF, WORK_BUF, PARA_BUF}
 */
#define S3C_MFC_BASEADDR_BITPROC_BUF	MFC_RESERVED_MEM_START

/* Physical Base Address for the MFC Data Buffer 
 * Data Buffer = {STRM_BUF, FRME_BUF}
 */
#define S3C_MFC_BASEADDR_DATA_BUF		(MFC_RESERVED_MEM_START + 0x116000)

/* Physical Base Address for the MFC Host I/F Registers */
#define S3C_MFC_BASEADDR_POST_SFR			0x77000000


/* 
 * MFC BITPROC_BUF
 *
 * the following three buffers have fixed size
 * firware buffer is to download boot code and firmware code
 */
#define S3C_MFC_CODE_BUF_SIZE	81920	/* It is fixed depending on the MFC'S FIRMWARE CODE (refer to 'Prism_S_V133.h' file) */

/* working buffer is uded for video codec operations by MFC */
#define S3C_MFC_WORK_BUF_SIZE	1048576 /* 1024KB = 1024 * 1024 */


/* Parameter buffer is allocated to store yuv frame address of output frame buffer. */
#define S3C_MFC_PARA_BUF_SIZE	8192  /* Stores the base address of Y , Cb , Cr for each decoded frame */

#define S3C_MFC_BITPROC_BUF_SIZE			 \
				(S3C_MFC_CODE_BUF_SIZE + \
				 S3C_MFC_PARA_BUF_SIZE + \
				 S3C_MFC_WORK_BUF_SIZE)


/* 
 * MFC DATA_BUF
 */
#define S3C_MFC_NUM_INSTANCES_MAX	2	/* MFC Driver supports 4 instances MAX. */

/* 
 * Determine if 'Post Rotate Mode' is enabled.
 * If it is enabled, the memory size of SD YUV420(720x576x1.5 bytes) is required more.
 * In case of linux driver, reserved buffer size will be changed.
 */
#define S3C_MFC_ROTATE_ENABLE		0


/*
 * stream buffer size must be a multiple of 512bytes 
 * becasue minimun data transfer unit between stream buffer and internal bitstream handling block 
 * in MFC core is 512bytes
 */
#define S3C_MFC_LINE_BUF_SIZE_PER_INSTANCE		(512000)


#define S3C_MFC_LINE_BUF_SIZE		(S3C_MFC_NUM_INSTANCES_MAX * S3C_MFC_LINE_BUF_SIZE_PER_INSTANCE)

#define S3C_MFC_YUV_BUF_SIZE		((CONFIG_VIDEO_SAMSUNG_MEMSIZE_MFC-((S3C_MFC_BITPROC_BUF_SIZE+S3C_MFC_STREAM_BUF_SIZE+S3C_MFC_YUV_BUF_CON_SIZE)/1024+1))*1024)

#define S3C_MFC_SEGMENT_INFO_SIZE	(40000)
#define S3C_MFC_COMMIT_INFO_SIZE	(40000)
#define S3C_MFC_YUV_BUF_CON_SIZE	(S3C_MFC_SEGMENT_INFO_SIZE+S3C_MFC_COMMIT_INFO_SIZE)

#define S3C_MFC_STREAM_BUF_SIZE		(S3C_MFC_LINE_BUF_SIZE)
#define S3C_MFC_DATA_BUF_SIZE		(S3C_MFC_STREAM_BUF_SIZE + S3C_MFC_YUV_BUF_SIZE+S3C_MFC_YUV_BUF_CON_SIZE)



/* Frame Buffer Size */
#define S3C_MFC_QCIF_FRAME_SIZE		(176*72*3)	/* 176*144*1.5 */
#define S3C_MFC_CIF_FRAME_SIZE		(352*144*3)	/* 352*288*1.5 */
#define S3C_MFC_QVGA_FRAME_SIZE		(320*120*3)	/* 320*240*1.5 */
#define S3C_MFC_VGA_FRAME_SIZE		(640*240*3)	/* 640*480*1.5 */
#define S3C_MFC_D1_FRAME_SIZE		(720*240*3)	/* 720*480*1.5 */
#define S3C_MFC_PAL_FRAME_SIZE		(720*288*3)	/* 720*576*1.5 */
#define S3C_MFC_DIVX_FRAME_SIZE		((720+32*2)*(288+32)*3)	/* 752*608*1.5 */

/* Minimum size of YUV buffer */
#define S3C_MFC_MIN_YUV_BUF_SIZE	(S3C_MFC_DIVX_FRAME_SIZE*3 + S3C_MFC_CIF_FRAME_SIZE*2 )


#if (S3C_MFC_YUV_BUF_SIZE < S3C_MFC_MIN_YUV_BUF_SIZE)
#error "You need more MFC YUV Buffer"
#endif

#define S3C_MFC_USE_USER_GAMMA_VALUE		0
#define S3C_MFC_USE_USER_QPMAX_VALUE		0

#endif /* _S3C_MFC_CONFIG_H */
