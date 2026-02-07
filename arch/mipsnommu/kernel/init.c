#include <linux/linkage.h>
#include <linux/init.h>
#include <linux/stddef.h>
#include <linux/config.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/smp_lock.h>
#include <asm/ps/bios.h>

extern char _stext;
extern char _etext;
extern char *linux_banner;

extern struct PlayStation psbios;

static int init(void *data)
{
	printf("%d\n", psbios.putchar('z'));
}

/*
 *	Activate the first processor.
 */
 
asmlinkage void __init start_kernel(void)
{
	char * command_line;
	unsigned long mempages;
	extern char saved_command_line[];
/*
 * Interrupts are still disabled. Do necessary setups, then
 * enable them
 */
	lock_kernel();
	printk(linux_banner);
	setup_arch(&command_line);
	printk("Kernel command line: %s\n", saved_command_line);
	parse_options(command_line);
	trap_init();
	init_IRQ();
	sched_init();
	time_init();
	softirq_init();

	/*
	 * HACK ALERT! This is early. We're enabling the console before
	 * we've done PCI setups etc, and console_init() must be aware of
	 * this. But we do want output early, in case something goes wrong.
	 */
	console_init();
#ifdef CONFIG_MODULES
	init_modules();
#endif
	if (prof_shift) {
		unsigned int size;
		/* only text is profiled */
		prof_len = (unsigned long) &_etext - (unsigned long) &_stext;
		prof_len >>= prof_shift;
		
		size = prof_len * sizeof(unsigned int) + PAGE_SIZE-1;
		prof_buffer = (unsigned int *) alloc_bootmem(size);
	}

	kmem_cache_init();
	sti();
	calibrate_delay();
   
#ifdef CONFIG_BLK_DEV_INITRD
	if (initrd_start && !initrd_below_start_ok &&
			initrd_start < min_low_pfn << PAGE_SHIFT) {
		printk(KERN_CRIT "initrd overwritten (0x%08lx < 0x%08lx) - "
		    "disabling it.\n",initrd_start,min_low_pfn << PAGE_SHIFT);
		initrd_start = 0;
	}
#endif
	mem_init();
	kmem_cache_sizes_init();
#ifdef CONFIG_3215_CONSOLE
        con3215_activate();
#endif
#ifdef CONFIG_PROC_FS
	proc_root_init();
#endif
	mempages = num_physpages;

	fork_init(mempages);
	proc_caches_init();
	vfs_caches_init(mempages);
	buffer_init(mempages);
	page_cache_init(mempages);
	kiobuf_setup();
	signals_init();
	bdev_init();
	inode_init(mempages);
#if defined(CONFIG_SYSVIPC)
	ipc_init();
#endif
#if defined(CONFIG_QUOTA)
	dquot_init_hash();
#endif
	check_bugs();
	printk("POSIX conformance testing by UNIFIX\n");

	/* 
	 *	We count on the initial thread going ok 
	 *	Like idlers init is an unlocked kernel thread, which will
	 *	make syscalls (and thus be locked).
	 */
	smp_init();
	kernel_thread(init, NULL, 0);
	unlock_kernel();
	current->need_resched = 1;
 	cpu_idle();
}
