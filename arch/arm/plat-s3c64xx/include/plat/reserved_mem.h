#ifndef _ASM_ARM_ARCH_RESERVED_MEM_H
#define _ASM_ARM_ARCH_RESERVED_MEM_H

#include <linux/types.h>
#include <linux/list.h>
#include <asm/setup.h>

#define DRAM_END_ADDR (PHYS_OFFSET+PHYS_SIZE)

#define RESERVED_PMEM_STREAM	(4 * 1024 * 1024)
#define RESERVED_PMEM_JPEG	(RESERVED_PMEM_STREAM)
#define RESERVED_MEM_CMM	(2 * 1024 * 1024 + RESERVED_PMEM_STREAM)
#define RESERVED_PMEM_PREVIEW	(4 * 1024 * 1024)
#define RESERVED_PMEM_SKIA	(0)
#define RESERVED_MEM_MFC	(4 * 1024 * 1024)
#define RESERVED_PMEM_PICTURE	(2 * 1024 * 1024)
#define RESERVED_PMEM_RENDER	(4 * 1024 * 1024)
#define RESERVED_PMEM_GPU1	(3 * 1024 * 1024)
#define RESERVED_PMEM		(0 * 1024 * 1024)

#if defined(CONFIG_RESERVED_MEM_CMM_JPEG_MFC_POST_CAMERA)
#define STREAM_RESERVED_PMEM_START	(DRAM_END_ADDR - RESERVED_PMEM_STREAM)
#define JPEG_RESERVED_PMEM_START	(STREAM_RESERVED_PMEM_START)
#define CMM_RESERVED_MEM_START		(STREAM_RESERVED_PMEM_START - 2 * 1024 * 1024)
#define PREVIEW_RESERVED_PMEM_START	(CMM_RESERVED_MEM_START - RESERVED_PMEM_PREVIEW)
#define SKIA_RESERVED_PMEM_START	(PREVIEW_RESERVED_PMEM_START)
#define MFC_RESERVED_MEM_START		(PREVIEW_RESERVED_PMEM_START - RESERVED_MEM_MFC)
#define PICTURE_RESERVED_PMEM_START	(MFC_RESERVED_MEM_START)
#define RENDER_RESERVED_PMEM_START	(MFC_RESERVED_MEM_START - RESERVED_PMEM_RENDER)
#define GPU1_RESERVED_PMEM_START	(RENDER_RESERVED_PMEM_START - RESERVED_PMEM_GPU1)
#define RESERVED_PMEM_START		(GPU1_RESERVED_PMEM_START - RESERVED_PMEM)
#define PHYS_UNRESERVED_SIZE		(RESERVED_PMEM_START - PHYS_OFFSET)
#else
#define PHYS_UNRESERVED_SIZE		(DRAM_END_ADDR - PHYS_OFFSET)

#endif 

struct s3c6410_pmem_setting{
        resource_size_t pmem_start;
        resource_size_t pmem_size;
        resource_size_t pmem_gpu1_start;
        resource_size_t pmem_gpu1_size;
        resource_size_t pmem_render_start;
        resource_size_t pmem_render_size;
        resource_size_t pmem_stream_start;
        resource_size_t pmem_stream_size;
        resource_size_t pmem_preview_start;
        resource_size_t pmem_preview_size;
        resource_size_t pmem_picture_start;
        resource_size_t pmem_picture_size;
        resource_size_t pmem_jpeg_start;
        resource_size_t pmem_jpeg_size;
        resource_size_t pmem_skia_start;
        resource_size_t pmem_skia_size;
};
 
void s3c6410_add_mem_devices (struct s3c6410_pmem_setting *setting);

#endif /* _ASM_ARM_ARCH_RESERVED_MEM_H */

