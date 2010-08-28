/* linux/drivers/video/samsung/gvg/s3c_fimgvg.c
 *
 * Driver file for Samsung OpenVG Accelerator(FIMG-VG)
 *
 * Jegeon Jung, Copyright (c) 2009 Samsung Electronics
 * 	http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/poll.h>
#include <linux/fs.h>
#include <linux/irq.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/mman.h>
#include <linux/vmalloc.h>
#include <asm/io.h>
#include <mach/map.h>
#include <plat/media.h>

#define DEBUG_S3C_GVG

#ifdef DEBUG_S3C_GVG
#define DEBUG(fmt,args...) printk(fmt, ##args)
#else
#define DEBUG(fmt,args...) do {} while(0)
#endif

#define GVG_VERSION		0x10
#define GVG_INTERRUPT_ENABLE	0x04
#define GVG_INTPENDING		0x08
#define GVG_FIFO_STAT		0x0C
#define GVG_SCCF_STAT		0xE0C
#define GVG_FT_END		0xD84


#define GVG_IOCTL_MAGIC		'S'

#define S3C_VG_WAIT_FOR_CF_FLUSH			_IO(GVG_IOCTL_MAGIC, 100)
#define S3C_VG_WAIT_FOR_FILTER_FLUSH			_IO(GVG_IOCTL_MAGIC, 101)
#define S3C_VG_WAIT_FOR_SCCF_FLUSH			_IO(GVG_IOCTL_MAGIC, 102)


#define S3C_VG_MEM_ALLOC		_IOWR(GVG_IOCTL_MAGIC, 310, struct s3c_vg_mem_alloc)
#define S3C_VG_MEM_FREE		_IOWR(GVG_IOCTL_MAGIC, 311, struct s3c_vg_mem_alloc)
#define S3C_VG_BOOTMEM_GET		_IOWR(GVG_IOCTL_MAGIC, 350, struct s3c_vg_mem_alloc)

#define GVG_RESERVED_MEM_ADDR_PHY        (unsigned int)s3c_get_media_memory(S3C_MDEV_GVG)
#define GVG_RESERVED_MEM_SIZE            (unsigned int)s3c_get_media_memsize(S3C_MDEV_GVG)


#define MEM_ALLOC		1
#define MEM_ALLOC_SHARE		2
#define MEM_BOOTMEM		3

#define PFX 			"s3c_gvg"
#define GVG_MINOR  		201

enum eVGInterruptMask
{
	VG_INT_MASK_SCCF = 1 << 5,
	VG_INT_MASK_FILTER = 1 << 4,   
	VG_INT_MASK_EDGE_OVERFLOW = 1 << 3,
	VG_INT_MASK_VERTEX_OVERFLOW = 1 << 2,
	VG_INT_MASK_CURRENT_COMMAND =  1 << 0,
	VG_INT_MASK_ALL_COMMAND_END = VG_INT_MASK_SCCF | VG_INT_MASK_FILTER | VG_INT_MASK_CURRENT_COMMAND,
	VG_INT_MASK_ALL_OVERFLOW = VG_INT_MASK_EDGE_OVERFLOW | VG_INT_MASK_VERTEX_OVERFLOW,
	VG_INT_MASK_ALL = VG_INT_MASK_ALL_COMMAND_END | VG_INT_MASK_ALL_OVERFLOW,
};

static struct resource *s3c_gvg_mem;
static void __iomem *s3c_gvg_base;
static void __iomem *s3c_gvg_clk_base;
static int s3c_gvg_irq;
//static struct clk *gvg_clock;
//static struct clk *h_clk;

static DEFINE_MUTEX(mem_alloc_lock);
static DEFINE_MUTEX(mem_free_lock);

struct s3c_vg_mem_alloc {
	int		size;
	unsigned int 	vir_addr;
	unsigned int 	phy_addr;
};

static int flag = 0;
static unsigned int physical_address;

unsigned int s3c_gvg_base_physical;

static int interrupt_already_recevied = 0; 
static wait_queue_head_t waitq;

irqreturn_t s3c_gvg_isr(int irq, void *dev_id)
{
	u32 cfg = 0;
	u32 uiEnableVal;
	
	uiEnableVal = __raw_readl(s3c_gvg_base + GVG_INTERRUPT_ENABLE);	
	uiEnableVal &= ~(VG_INT_MASK_ALL);
	__raw_writel(uiEnableVal, s3c_gvg_base+GVG_INTERRUPT_ENABLE);

	switch(cfg & VG_INT_MASK_ALL)
	{
	case VG_INT_MASK_CURRENT_COMMAND:
//		printk("Current Command End Interupt\n");
	
		break;
		
	case VG_INT_MASK_FILTER:
//		printk("Filter Comman End Bit\n");
		break;
		
	case VG_INT_MASK_SCCF:
//		printk("SCCF Command End\n");
		break;
		
	case VG_INT_MASK_EDGE_OVERFLOW:
		printk("Temporary EDGE Storage is not enough to draw path\n");
		break;
		
	case VG_INT_MASK_VERTEX_OVERFLOW:
		printk("Temporary VERTEX Storage is not enough to draw path\n");
		break;		
	}

	cfg = __raw_readl(s3c_gvg_base + GVG_INTPENDING);
	cfg &= ~(VG_INT_MASK_ALL);
	__raw_writel(cfg, s3c_gvg_base + GVG_INTPENDING);

	interrupt_already_recevied = 1;
	wake_up_interruptible(&waitq);

	uiEnableVal |= VG_INT_MASK_ALL;
	__raw_writel(uiEnableVal, s3c_gvg_base+GVG_INTERRUPT_ENABLE);

	return IRQ_HANDLED;
}

int s3c_gvg_open(struct inode *inode, struct file *file)
{
	printk("open gvg node\n");
	return 0;
}

int s3c_gvg_release(struct inode *inode, struct file *file)
{
	printk("closed gvg node\n");
	return 0;
}

bool 
s3c_gvg_wait_for_flush(unsigned int uiInterruptEnableMask, unsigned int uiWaitReg, unsigned int uiWaitRegMask)
{
	unsigned int uiWaitRegVal;
	
	// enable interrupt
	interrupt_already_recevied = 0;

	while(1) {
		uiWaitRegVal = __raw_readl(s3c_gvg_base + uiWaitReg);

		if((uiWaitRegVal & uiWaitRegMask) != 0){
			break;
		}

		wait_event_interruptible(waitq, (interrupt_already_recevied>0));

		interrupt_already_recevied = 0;

		//printk("unadequate interrupt %p %p %p %p x\n", uiWaitRegVal, uiInterruptEnableMask, uiWaitReg, uiWaitRegMask);

	}

//	Interrupt Enable
//	uiEnableVal |= (VG_INT_MASK_ALL);			
//	__raw_writel(uiEnableVal, s3c_gvg_base + GVG_INTERRUPT_ENABLE);

	return true;
}

static int s3c_gvg_ioctl(struct inode *inode, struct file *file,
		unsigned int cmd, unsigned long arg)
{

	DECLARE_COMPLETION_ONSTACK(complete);

	unsigned long *virt_addr;
	struct mm_struct *mm = current->mm;
	struct s3c_vg_mem_alloc param;
	
	switch (cmd) {
	case S3C_VG_WAIT_FOR_CF_FLUSH:
		s3c_gvg_wait_for_flush(VG_INT_MASK_CURRENT_COMMAND, GVG_FIFO_STAT, 0x1);
		break;

	case S3C_VG_WAIT_FOR_FILTER_FLUSH:
		s3c_gvg_wait_for_flush(VG_INT_MASK_FILTER, GVG_FT_END, 0x1);
		break;

	case S3C_VG_WAIT_FOR_SCCF_FLUSH:
		s3c_gvg_wait_for_flush(VG_INT_MASK_SCCF, GVG_SCCF_STAT, 0x1);
		break;
		
	case S3C_VG_MEM_ALLOC:		
		mutex_lock(&mem_alloc_lock);
		if(copy_from_user(&param, (struct s3c_vg_mem_alloc *)arg, sizeof(struct s3c_vg_mem_alloc))) {
			mutex_unlock(&mem_alloc_lock);			
			return -EFAULT;
		}
		flag = MEM_ALLOC;

		param.vir_addr = do_mmap(file, 0, param.size, PROT_READ|PROT_WRITE, MAP_SHARED, 0);
		DEBUG("param.vir_addr = %08x\n", param.vir_addr);

		if(param.vir_addr == -EINVAL) {
			printk("S3C_VG_MEM_ALLOC FAILED\n");
			flag = 0;
			mutex_unlock(&mem_alloc_lock);			
			return -EFAULT;
		}
		param.phy_addr = physical_address;

		DEBUG("KERNEL MALLOC : param.phy_addr = 0x%X \t size = %d \t param.vir_addr = 0x%X\n", 
			param.phy_addr, param.size, param.vir_addr);

		if(copy_to_user((struct s3c_vg_mem_alloc *)arg, &param, sizeof(struct s3c_vg_mem_alloc))) {
			flag = 0;
			mutex_unlock(&mem_alloc_lock);
			return -EFAULT;		
		}

		flag = 0;
		
		mutex_unlock(&mem_alloc_lock);
		
		break;

	case S3C_VG_MEM_FREE:	
		mutex_lock(&mem_free_lock);
		if(copy_from_user(&param, (struct s3c_vg_mem_alloc *)arg, sizeof(struct s3c_vg_mem_alloc))) {
			mutex_unlock(&mem_free_lock);
			return -EFAULT;
		}

		DEBUG("KERNEL FREE : param.phy_addr = 0x%X \t size = %d \t param.vir_addr = 0x%X\n", 
			param.phy_addr, param.size, param.vir_addr);

		if (do_munmap(mm, param.vir_addr, param.size) < 0) {
			printk("do_munmap() failed !!\n");
			mutex_unlock(&mem_free_lock);
			return -EINVAL;
		}
		virt_addr = (unsigned long *)phys_to_virt(param.phy_addr);

		kfree(virt_addr);
		param.size = 0;
		DEBUG("do_munmap() succeed !!\n");

		if(copy_to_user((struct s3c_vg_mem_alloc *)arg, &param, sizeof(struct s3c_vg_mem_alloc))) {
			mutex_unlock(&mem_free_lock);
			return -EFAULT;
		}
		
		mutex_unlock(&mem_free_lock);
		
		break;

	case S3C_VG_BOOTMEM_GET:
		printk("Bootmem ENTER\n");
		flag = MEM_BOOTMEM;

		param.phy_addr = GVG_RESERVED_MEM_ADDR_PHY;        
		param.size = GVG_RESERVED_MEM_SIZE;
		
             		/*param.vir_addr = do_mmap(file, 0, GVG_RESERVED_MEM_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, 0);
		DEBUG("Bootmem param.vir_addr = %08x\n", param.vir_addr);
		printk("Bootmem param.vir_addr = %08x\n", param.vir_addr);

		if(param.vir_addr == -EINVAL) {
			printk("S3C_VG_BOOTMEM_GET FAILED\n");
			flag = 0;
			return -EFAULT;
		}
		param.phy_addr = physical_address;        
		param.size = GVG_RESERVED_MEM_SIZE;
		*/

		// printk("alloc %d\n", param.size);
		DEBUG("KERNEL Bootmem param.vir_addr : param.phy_addr = 0x%X \t size = %d \t param.vir_addr = 0x%X\n", param.phy_addr, param.size, param.vir_addr);

		if(copy_to_user((struct s3c_vg_mem_alloc *)arg, &param, sizeof(struct s3c_vg_mem_alloc))){
			flag = 0;
			return -EFAULT;		
		}

		flag = 0;
		
		break;
		
	default:
		DEBUG("s3c_gvg_ioctl() : default !!\n");
		return -EINVAL;
	}
	
	return 0;
}

int s3c_gvg_mmap(struct file* filp, struct vm_area_struct *vma)
{
	unsigned long pageFrameNo, size, phys_addr;
	unsigned long *virt_addr;

	size = vma->vm_end - vma->vm_start;

	switch (flag) { 
	case MEM_ALLOC :
		virt_addr = (unsigned long *)kmalloc(size, GFP_KERNEL);

		if (virt_addr == NULL) {
			printk("kmalloc() failed !\n");
			return -EINVAL;
		}
		DEBUG("MMAP_KMALLOC : virt addr = 0x%p, size = %d\n", virt_addr, (int)size);
		phys_addr = virt_to_phys(virt_addr);
		physical_address = (unsigned int)phys_addr;

		//DEBUG("MMAP_KMALLOC : phys addr = 0x%p\n", phys_addr);
		pageFrameNo = __phys_to_pfn(phys_addr);
		//DEBUG("MMAP_KMALLOC : PFN = 0x%x\n", pageFrameNo);
		break;
	case MEM_BOOTMEM:
		phys_addr = GVG_RESERVED_MEM_ADDR_PHY;

		if (((unsigned int)phys_addr) == NULL) {
			printk("There is no reserved memory for G3D!\n");
			return -EINVAL;
		}

//		printk("BOOTMEM : virt addr = 0x%p, phys addr = 0x%p, size = %d\n", virt_addr, phys_addr, size);
		physical_address = (unsigned int)phys_addr;

		pageFrameNo = __phys_to_pfn(phys_addr);
		break;
	default :
		// page frame number of the address for a source G2D_SFR_SIZE to be stored at.
		pageFrameNo = __phys_to_pfn(s3c_gvg_base_physical);
		DEBUG("MMAP : vma->end = 0x%p, vma->start = 0x%p, size = %d\n", 
			(void *)(vma->vm_end), (void *)(vma->vm_start), (int)size);

		if(size > (s3c_gvg_mem->end-s3c_gvg_mem->start+1)) {
			printk("The size of GVG_SFR_SIZE mapping is too big!\n");
			return -EINVAL;
		}
		break;
	}
	
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	if ((vma->vm_flags & VM_WRITE) && !(vma->vm_flags & VM_SHARED)) {
		printk("s3c_gvg_mmap() : Writable GVG_SFR_SIZE mapping must be shared !\n");
		return -EINVAL;
	}

	if (remap_pfn_range(vma, vma->vm_start, pageFrameNo, size, vma->vm_page_prot)) {
		printk("s3c_gvg_mmap() : remap_pfn_range() failed !\n");
		return -EINVAL;
	}

	return 0;
}

static struct file_operations s3c_gvg_fops = {
	.owner 	= THIS_MODULE,
	.ioctl 	= s3c_gvg_ioctl,
	.open 	= s3c_gvg_open,
	.release = s3c_gvg_release,
	.mmap	= s3c_gvg_mmap,
};


static struct miscdevice s3c_gvg_dev = {
	.minor		= GVG_MINOR,
	.name		= "s3c-gvg",
	.fops		= &s3c_gvg_fops,
};

static int s3c_gvg_remove(struct platform_device *dev)
{
	//clk_disable(gvg_clock);
	printk(KERN_INFO "s3c_gvg_remove called !\n");

	free_irq(s3c_gvg_irq, NULL);

	if (s3c_gvg_mem != NULL) {
		pr_debug("s3c_gvg: releasing s3c_post_mem\n");
		iounmap(s3c_gvg_base);
		release_resource(s3c_gvg_mem);
		kfree(s3c_gvg_mem);
	}

	misc_deregister(&s3c_gvg_dev);
	printk(KERN_INFO "s3c_gvg_remove Success !\n");
	return 0;
}

int s3c_gvg_probe(struct platform_device *pdev)
{
	struct resource *res;
	unsigned int cfg;

	int ret;
	unsigned int uiEnableVal;	

	DEBUG("s3c_gvg probe() called\n");

	s3c_gvg_irq = platform_get_irq(pdev, 0);
	if(s3c_gvg_irq <= 0) {
		printk(KERN_ERR PFX "failed to get irq resouce\n");
		return -ENOENT;
	}

	ret = request_irq(s3c_gvg_irq, s3c_gvg_isr, IRQF_DISABLED, pdev->name, NULL);
	if (ret) {
		printk("request_irq(GVG) failed.\n");
		return ret;
	}



	/* get the memory region for the post processor driver */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if(res == NULL) {
		printk(KERN_ERR PFX "failed to get memory region resouce\n");
		return -ENOENT;
	}

	s3c_gvg_base_physical = (unsigned int)res->start;

	s3c_gvg_mem = request_mem_region(res->start, res->end-res->start+1, pdev->name);
	if(s3c_gvg_mem == NULL) {
		printk(KERN_ERR PFX "failed to reserve memory region\n");
		return -ENOENT;
	}


	s3c_gvg_base = ioremap(s3c_gvg_mem->start, s3c_gvg_mem->end - res->start + 1);
	if(s3c_gvg_base == NULL) {
		printk(KERN_ERR PFX "failed ioremap\n");
		return -ENOENT;
	}

	// enable interrupt
	interrupt_already_recevied = 0;
	uiEnableVal = __raw_readl(s3c_gvg_base + GVG_INTERRUPT_ENABLE);
	uiEnableVal |= VG_INT_MASK_ALL;
	__raw_writel(uiEnableVal, s3c_gvg_base+GVG_INTERRUPT_ENABLE);

#if 0 /* To Do : Clock Setting */
	gvg_clock = clk_get(&pdev->dev, "post");
	if(gvg_clock == NULL) {
		printk(KERN_ERR PFX "failed to find post clock source\n");
		return -ENOENT;
	}

	clk_enable(gvg_clock);

	h_clk = clk_get(&pdev->dev, "hclk");
	if(h_clk == NULL) {
		printk(KERN_ERR PFX "failed to find h_clk clock source\n");
		return -ENOENT;
	}
#endif
	s3c_gvg_clk_base = ioremap(S5P64XX_PA_SYSCON, SZ_4K);
	if(s3c_gvg_clk_base == NULL) {
		printk(KERN_ERR PFX "failed ioremap for clk\n");
		return -ENOENT;
	}

#define VG_CLK_SRC_MPLL
#if defined(VG_CLK_SRC_MPLL)
             /* source clock = MPLL */
             cfg = __raw_readl(s3c_gvg_clk_base + 0x10c);
             cfg &= ~(0x2<<8);
             cfg |= (0x2<<8);
 
             __raw_writel(cfg, s3c_gvg_clk_base + 0x10c);
 
             /* DIV3 : MPLL /2 */
             cfg = __raw_readl(s3c_gvg_clk_base + 0x40);
             cfg &= ~(0xF<<4);
             cfg |= (0x1<<4);

             __raw_writel(cfg, s3c_gvg_clk_base + 0x40);

#else
	cfg = __raw_readl(s3c_gvg_clk_base + 0x40);
	cfg &= ~(0xF<<4);
	cfg |= (0x1<<4);

	__raw_writel(cfg, s3c_gvg_clk_base + 0x40);
#endif

	init_waitqueue_head(&waitq);

	ret = misc_register(&s3c_gvg_dev);
	if (ret) {
		printk (KERN_ERR "cannot register miscdev on minor=%d (%d)\n",
				GVG_MINOR, ret);
		return ret;
	}

	printk("s3c_gvg version : 0x%x\n",__raw_readl(s3c_gvg_base + GVG_VERSION));

	/* check to see if everything is setup correctly */
	return 0;
}

static int s3c_gvg_suspend(struct platform_device *dev, pm_message_t state)
{
	//clk_disable(gvg_clock);
	return 0;
}
static int s3c_gvg_resume(struct platform_device *pdev)
{
	//clk_enable(gvg_clock);
	return 0;
}
static struct platform_driver s3c_gvg_driver = {
	.probe          = s3c_gvg_probe,
	.remove         = s3c_gvg_remove,
	.suspend        = s3c_gvg_suspend,
	.resume         = s3c_gvg_resume,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "s3c-gvg",
	},
};

static char banner[] __initdata = KERN_INFO "S3C GVG Driver, (c) 2009 Samsung Electronics\n";

static char init_error[] __initdata = KERN_ERR "Intialization of S3C GVG driver is failed\n";

int __init  s3c_gvg_init(void)
{
	printk(banner);

	if(platform_driver_register(&s3c_gvg_driver)!=0) {
		printk(init_error);
		return -1;
	}

	printk("S3C GVG Init : Done\n");
	return 0;
}

void  s3c_gvg_exit(void)
{
	platform_driver_unregister(&s3c_gvg_driver);

	printk("S3C GVG module exit\n");
}

module_init(s3c_gvg_init);
module_exit(s3c_gvg_exit);

MODULE_AUTHOR("gwan.kim@samsung.com");
MODULE_DESCRIPTION("S3C GVG Device Driver");
MODULE_LICENSE("GPL");

