/* linux/driver/media/video/mfc/s3c_mfc_databuf.c
 *
 * C file for Samsung MFC (Multi Function Codec - FIMV) driver 
 *
 * PyoungJae Jung, Jiun Yu, Copyright (c) 2009 Samsung Electronics 
 * http://www.samsungsemi.com/ 
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <asm/io.h>
#include <linux/kernel.h>

#include "s3c_mfc_base.h"
#include "s3c_mfc_types.h"
#include "s3c_mfc_databuf.h"
#include "s3c_mfc_config.h"
#include "s3c_mfc.h"

static volatile unsigned char     *s3c_mfc_virt_data_buf = NULL;
static unsigned int                s3c_mfc_phys_data_buf = 0;


BOOL s3c_mfc_memmap_databuf()
{
	BOOL	ret = FALSE;

	s3c_mfc_virt_data_buf = phys_to_virt(s3c_mfc_phys_buffer + S3C_MFC_BITPROC_BUF_SIZE);
	if (s3c_mfc_virt_data_buf == NULL) {
		mfc_err("fail to mapping data buffer\n");
		return ret;
	}

	mfc_debug("virtual address of data buffer = 0x%x\n", \
			(unsigned int)s3c_mfc_virt_data_buf);
	

	/* Physical register address mapping */
	s3c_mfc_phys_data_buf = S3C_MFC_BASEADDR_DATA_BUF;

	ret = TRUE;

	return ret;
}

volatile unsigned char *s3c_mfc_get_databuf_virt_addr()
{
	volatile unsigned char	*data_buf;

	data_buf	= s3c_mfc_virt_data_buf;

	return data_buf;	
}

volatile unsigned char *s3c_mfc_get_yuvbuff_virt_addr()
{
	volatile unsigned char	*yuv_buff;

	yuv_buff	= s3c_mfc_virt_data_buf + S3C_MFC_STREAM_BUF_SIZE;

	return yuv_buff;	
}

volatile unsigned char *s3c_mfc_get_segment_info_virt_addr()
{
	volatile unsigned char	*segment_info_buff;

	segment_info_buff	= s3c_mfc_virt_data_buf + S3C_MFC_STREAM_BUF_SIZE + S3C_MFC_YUV_BUF_SIZE;

	return segment_info_buff;	
}

volatile unsigned char *s3c_mfc_get_commit_info_virt_addr()
{
	volatile unsigned char	*commit_info_buff;

	commit_info_buff	= s3c_mfc_virt_data_buf + S3C_MFC_STREAM_BUF_SIZE + S3C_MFC_YUV_BUF_SIZE + S3C_MFC_SEGMENT_INFO_SIZE;

	return commit_info_buff;	
}



unsigned int s3c_mfc_get_databuf_phys_addr()
{
	unsigned int	phys_data_buf;

	phys_data_buf	= s3c_mfc_phys_data_buf;

	return phys_data_buf;
}

unsigned int s3c_mfc_get_yuvbuff_phys_addr()
{
	unsigned int	phys_yuv_buff;

	phys_yuv_buff	= s3c_mfc_phys_data_buf + S3C_MFC_STREAM_BUF_SIZE;

	return phys_yuv_buff;
}
unsigned int s3c_mfc_get_segment_info_phys_addr()
{
	unsigned int	phys_segment_info_buff;

	phys_segment_info_buff	= s3c_mfc_phys_data_buf + S3C_MFC_STREAM_BUF_SIZE + S3C_MFC_YUV_BUF_SIZE;

	return phys_segment_info_buff;
}
unsigned int s3c_mfc_get_commit_info_phys_addr()
{
	unsigned int	phys_commit_info_buff;

	phys_commit_info_buff	= s3c_mfc_phys_data_buf + S3C_MFC_STREAM_BUF_SIZE + S3C_MFC_YUV_BUF_SIZE + S3C_MFC_SEGMENT_INFO_SIZE ;

	return phys_commit_info_buff;
}
