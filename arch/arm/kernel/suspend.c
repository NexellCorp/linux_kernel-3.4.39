#include <linux/init.h>

#include <asm/idmap.h>
#include <asm/pgalloc.h>
#include <asm/pgtable.h>
#include <asm/memory.h>
#include <asm/suspend.h>
#include <asm/tlbflush.h>

extern int __cpu_suspend(unsigned long, int (*)(unsigned long));
extern void cpu_resume_mmu(void);

#ifdef CONFIG_FALINUX_ZEROBOOT_NAL
static u32 zb_ptr;
#endif
/*
 * This is called by __cpu_suspend() to save the state, and do whatever
 * flushing is required to ensure that when the CPU goes to sleep we have
 * the necessary data available when the caches are not searched.
 */
void __cpu_suspend_save(u32 *ptr, u32 ptrsz, u32 sp, u32 *save_ptr)
{
	*save_ptr = virt_to_phys(ptr);

	/* This must correspond to the LDM in cpu_resume() assembly */
	*ptr++ = virt_to_phys(idmap_pgd);
	*ptr++ = sp;
	*ptr++ = virt_to_phys(cpu_do_resume);

#ifdef CONFIG_FALINUX_ZEROBOOT_NAL
	zb_ptr = *save_ptr;
#endif
	cpu_do_suspend(ptr);

	flush_cache_all();
	outer_clean_range(*save_ptr, *save_ptr + ptrsz);
	outer_clean_range(virt_to_phys(save_ptr),
			  virt_to_phys(save_ptr) + sizeof(*save_ptr));
}

#ifdef CONFIG_FALINUX_ZEROBOOT_NAL
#include <linux/export.h>
extern char  __idmap_text_start[], __idmap_text_end[];
unsigned long zb_get_idmap_pgd_phys(void)
{
	return virt_to_phys(idmap_pgd);
}
EXPORT_SYMBOL(zb_get_idmap_pgd_phys);

unsigned long zb_get_idmap_text_start(void)
{
	return virt_to_phys((void *)__idmap_text_start);
}
EXPORT_SYMBOL(zb_get_idmap_text_start);

unsigned long zb_get_idmap_text_end(void)
{
	return virt_to_phys((void *)__idmap_text_end);
}
EXPORT_SYMBOL(zb_get_idmap_text_end);

unsigned long zb_get_resume_ptr(void)
{
	return (unsigned long)zb_ptr;
}
EXPORT_SYMBOL(zb_get_resume_ptr);

unsigned long zb_get_cpu_resume_phys(void)
{
	return virt_to_phys((void *)cpu_resume);
}
EXPORT_SYMBOL(zb_get_cpu_resume_phys);

void lldebugout(const char *fmt, ...);
static int touch_all_memory(unsigned int size)
{
    volatile unsigned char *src, *dst;
    unsigned char dummy;

    src = (unsigned char *)PAGE_OFFSET;
    dst = (unsigned char *)(size);
	lldebugout("start touch all memory, start 0x%p end 0x%p\n", src, dst);

#if 0 // increment
    while (src != dst) {
        dummy = *src;
        src += 4096;
    }
#else // decrement
    dst -= 4096;
    while (src != dst) {
        dummy = *dst;
        dst -= 4096;
		if (!((unsigned int)dst & 0x000fffff))
			lldebugout("d %p\n", dst);
    }
#endif

    printk("    done.....\n");

//  fault_test();
    return 0;
}
#endif
/*
 * Hide the first two arguments to __cpu_suspend - these are an implementation
 * detail which platform code shouldn't have to know about.
 */
int cpu_suspend(unsigned long arg, int (*fn)(unsigned long))
{
	struct mm_struct *mm = current->active_mm;
	int ret;

	if (!idmap_pgd)
		return -EINVAL;

	/*
	 * Provide a temporary page table with an identity mapping for
	 * the MMU-enable code, required for resuming.  On successful
	 * resume (indicated by a zero return code), we need to switch
	 * back to the correct page tables.
	 */
	ret = __cpu_suspend(arg, fn);
	if (ret == 0) {
		cpu_switch_mm(mm->pgd, mm);
		local_flush_bp_all();
		local_flush_tlb_all();

#ifdef CONFIG_FALINUX_ZEROBOOT_NAL
		// now we touch all memory place force loading by touch
		//touch_all_memory((unsigned int)high_memory);


/*
	u32 debugMMCRegs;

   	debugMMCRegs = ioremap(0xC0069000,0x400);
	printk("regs:%8x\n",__raw_readl(debugMMCRegs));

	iounmap( debugMMCRegs );
*/

#endif
	}

	return ret;
}
