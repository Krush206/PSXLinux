/*
 * binfmt_ecoff.c - ECOFF binary format handler for no-MMU Linux.
 *
 * Loads ECOFF executables (produced by elf2ecoff from the mipsel-linux
 * toolchain) at their text_start address.  On a no-MMU system the text
 * and data are loaded contiguously into a pre-allocated memory block
 * whose address was fixed at link time (e.g. -Ttext=0x100000), so no
 * runtime relocations are needed.
 *
 * ECOFF layout (little-endian MIPS, magic 0x0162):
 *   struct filehdr  (20 bytes): magic, nscns, timdat, symptr, nsyms, opthdr, flags
 *   struct aouthdr  (56 bytes): magic, vstamp, tsize, dsize, bsize, entry,
 *                               text_start, data_start, bss_start, ...
 *   struct scnhdr[]          : section headers with paddr/vaddr/size/scnptr
 *   raw section data
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/file.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/binfmts.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/mipsregs.h>
#include <asm/isadep.h>

/*
 * ECOFF structures (same as arch/mipsnommu/boot/ecoff.h but using
 * fixed-width types since this is a kernel-side header).
 */
struct ecoff_filehdr {
	unsigned short f_magic;
	unsigned short f_nscns;
	unsigned int   f_timdat;
	unsigned int   f_symptr;
	unsigned int   f_nsyms;
	unsigned short f_opthdr;
	unsigned short f_flags;
};

struct ecoff_aouthdr {
	unsigned short magic;
	unsigned short vstamp;
	unsigned int   tsize;
	unsigned int   dsize;
	unsigned int   bsize;
	unsigned int   entry;
	unsigned int   text_start;
	unsigned int   data_start;
	unsigned int   bss_start;
	unsigned int   gprmask;
	unsigned int   cprmask[4];
	unsigned int   gp_value;
};

struct ecoff_scnhdr {
	char           s_name[8];
	unsigned int   s_paddr;
	unsigned int   s_vaddr;
	unsigned int   s_size;
	unsigned int   s_scnptr;
	unsigned int   s_relptr;
	unsigned int   s_lnnoptr;
	unsigned short s_nreloc;
	unsigned short s_nlnno;
	unsigned int   s_flags;
};

#define MIPSELMAGIC	0x0162
#define ECOFF_HDR_SIZE	(sizeof(struct ecoff_filehdr) + \
			 sizeof(struct ecoff_aouthdr))

static int load_ecoff_binary(struct linux_binprm *bprm, struct pt_regs *regs)
{
	struct ecoff_filehdr fh;
	struct ecoff_aouthdr ah;
	unsigned long text_addr;
	unsigned long total_size;
	int retval;
	int i;
	loff_t fpos = 0;

	/* Read the file header */
	if (bprm->file->f_dentry->d_inode->i_size < ECOFF_HDR_SIZE)
		return -ENOEXEC;

	fpos = 0;
	retval = bprm->file->f_op->read(bprm->file, (char *)&fh,
					 sizeof(fh), &fpos);
	if (retval != sizeof(fh))
		return -ENOEXEC;

	/* Check ECOFF little-endian MIPS magic */
	if (fh.f_magic != MIPSELMAGIC)
		return -ENOEXEC;

	printk(KERN_ERR "ECOFF: magic ok, nscns=%d opthdr=%d\n",
	       fh.f_nscns, fh.f_opthdr);

	/* Read the a.out header */
	fpos = sizeof(fh);
	retval = bprm->file->f_op->read(bprm->file, (char *)&ah,
					 sizeof(ah), &fpos);
	if (retval != sizeof(ah))
		return -ENOEXEC;

	printk(KERN_ERR "ECOFF: tsize=%d dsize=%d bsize=%d "
	       "entry=%08x text_start=%08x\n",
	       ah.tsize, ah.dsize, ah.bsize, ah.entry, ah.text_start);

	text_addr = ah.text_start;
	total_size = ah.tsize + ah.dsize + ah.bsize;

	/* Sanity: entry must be within the text segment */
	if (ah.entry < ah.text_start ||
	    ah.entry >= ah.text_start + ah.tsize)
		return -ENOEXEC;

	/* On no-MMU, text must be in RAM above the kernel but not
	 * in KSEG0/KSEG1 (kernel virtual space) */
	if (text_addr == 0 || text_addr >= 0x80000000u)
		return -ENOEXEC;
	if (text_addr < 0x00100000u)  /* below kernel + safety gap */
		return -ENOEXEC;

	printk(KERN_ERR "ECOFF: sanity ok, flushing old exec\n");

	/* Flush old state */
	retval = flush_old_exec(bprm);
	if (retval)
		return retval;

	printk(KERN_ERR "ECOFF: flush ok, loading sections\n");

	/* Read section data directly to text_addr */
	{
		struct ecoff_scnhdr sh;
		unsigned long dst;

		for (i = 0; i < fh.f_nscns; i++) {
			fpos = sizeof(fh) + fh.f_opthdr +
			       i * sizeof(sh);
			retval = bprm->file->f_op->read(bprm->file,
						       (char *)&sh,
						       sizeof(sh), &fpos);
			if (retval != sizeof(sh))
				return -ENOEXEC;

			printk(KERN_ERR "ECOFF: sec %d name=%c%c%c%c "
			       "paddr=%08x size=%d scnptr=%d flags=%x\n",
			       i, sh.s_name[0], sh.s_name[1],
			       sh.s_name[2], sh.s_name[3],
			       sh.s_paddr, sh.s_size, sh.s_scnptr,
			       sh.s_flags);

			/* Skip empty and non-loaded sections */
			if (sh.s_size == 0 || sh.s_scnptr == 0)
				continue;
			if (sh.s_flags == 0x82)	/* S_BSS */
				continue;

			dst = sh.s_paddr;
			if (dst == 0)
				continue;

			/* Copy section from file to memory */
			fpos = sh.s_scnptr;
			{
				char *kbuf = (char *)dst;
				unsigned int remaining = sh.s_size;
				unsigned int chunk;

				while (remaining > 0) {
					chunk = remaining;
					if (chunk > 4096)
						chunk = 4096;
					retval = bprm->file->f_op->read(
							bprm->file, kbuf,
							chunk, &fpos);
					if (retval < 0)
						return retval;
					kbuf += chunk;
					remaining -= chunk;
				}
			}
		}

		/* Verify first bytes loaded */
		{
			unsigned int *p = (unsigned int *)text_addr;
			printk(KERN_ERR "ECOFF: first words at %08x: "
			       "%08x %08x\n", text_addr, p[0], p[1]);
		}

		/* Zero the BSS: the kernel guarantees cleared BSS for
		 * freshly exec'd programs.  On this no-MMU target the
		 * RAM under the image still holds kernel slab/buffer
		 * data, so skipping this leaves every global variable
		 * as garbage (matrices, OTs, tables...). */
		if (ah.bsize) {
			unsigned int baddr = ah.text_start + ah.tsize +
					     ah.dsize;

			printk(KERN_DEBUG "ECOFF: zeroing bss %08x..%08x\n",
			       baddr, baddr + ah.bsize);
			memset((void *)baddr, 0, ah.bsize);
		}
	}

	/* Set up the process memory map (no-MMU style) */
	current->mm->start_code = text_addr;
	current->mm->end_code = text_addr + ah.tsize;
	current->mm->start_data = text_addr + ah.tsize;
	current->mm->end_data = current->mm->start_data + ah.dsize;
	current->mm->brk = current->mm->start_brk = text_addr + total_size;
	current->mm->rss = 0;

	/* Set up stack */
	{
		unsigned long sp;
		unsigned long stack_top = text_addr + total_size +
					  0x4000; /* 16K above image */
		sp = stack_top & ~7;

		/* Push argv[0] filename onto stack */
		sp -= strlen(bprm->filename) + 1;
		copy_to_user((void *)sp, bprm->filename,
			     strlen(bprm->filename) + 1);
		bprm->argv[0] = (char *)sp;

		/* Push null argv terminator */
		sp -= sizeof(char *);
		clear_user((void *)sp, sizeof(char *));

		current->mm->start_stack = sp;
	}

	/*
	 * Start the program in USER mode, exactly like start_thread():
	 * the exception entry code derives `current` from the stack
	 * pointer when entering from kernel mode (CU0 set), which would
	 * compute a garbage task_struct from the program's own stack.
	 * In user mode the entry code switches to the task's kernel
	 * stack (kernelsp) instead, keeping `current` valid.
	 *
	 * On the PSX no TLB setup is needed: the 2 MB RAM and the
	 * hardware registers are mirrored throughout KUSEG, so user
	 * mode can access text/data/stack without wired TLB entries.
	 */
	regs->cp0_status = (regs->cp0_status & ~ST0_CU0) | KU_USER;
	regs->cp0_epc = ah.entry;
	regs->regs[29] = current->mm->start_stack;

	printk(KERN_ERR "ECOFF: starting at pc=%08x sp=%08x\n",
	       ah.entry, current->mm->start_stack);

	printk(KERN_DEBUG "ECOFF: loaded at %08lx, entry %08x, "
	       "text %u data %u bss %u\n",
	       text_addr, ah.entry, ah.tsize, ah.dsize, ah.bsize);

	return 0;
}

static struct linux_binfmt ecoff_format = {
	NULL, THIS_MODULE, load_ecoff_binary, NULL, NULL
};

static int __init init_ecoff_binfmt(void)
{
	return register_binfmt(&ecoff_format);
}

static void __exit exit_ecoff_binfmt(void)
{
	unregister_binfmt(&ecoff_format);
}

module_init(init_ecoff_binfmt);
module_exit(exit_ecoff_binfmt);
