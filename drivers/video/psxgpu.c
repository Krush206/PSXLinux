/*
 * psxgpu - PlayStation GPU character device.
 *
 * Exposes the GPU GP0/GP1 command interface to userspace.
 *   write()  : queue 32-bit GP0 data words (primitive packets)
 *   ioctl() : GP1 control commands, status read, VRAM upload, vsync wait
 *
 * Register reference: https://psx-spx.github.io/#gpu-interface
 *   1F801810h  GP0  (write: command/data, read: GPUREAD)
 *   1F801814h  GP1  (write: control, read: GPUSTAT)
 *
 * VRAM is 1MB (1024x512 16-bit pixels); display is 320x240 by default.
 */

#include <linux/config.h>

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/major.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <asm/io.h>
#include <asm/ps/hwregs.h>
#include <asm/ps/interrupts.h>

#include <linux/psxgpu.h>

#define PSXGPU_BASE    0x1F801810u
#define PSXGPU_GP0    (PSXGPU_BASE + 0)
#define PSXGPU_GP1    (PSXGPU_BASE + 4)

#define GPUSTAT_READY_FIFO  (1u << 28)  /* GP0 command FIFO not full */
#define GPUSTAT_READY_DMA   (1u << 29)  /* DMA ready */
#define GPUSTAT_VBLANK     (1u << 31)  /* currently in VBlank */

static void psxgpu_write_gp0(uint32_t v)
{
	writel(v, PSXGPU_GP0);
}

static void psxgpu_write_gp1(uint32_t v)
{
	writel(v, PSXGPU_GP1);
}

static uint32_t psxgpu_read_gp1(void)
{
	return readl(PSXGPU_GP1);
}

static uint32_t psxgpu_read_gp0(void)
{
	return readl(PSXGPU_GP0);
}

static inline void psxgpu_gp0_wait_fifo(void)
{
	while (!(psxgpu_read_gp1() & GPUSTAT_READY_FIFO))
		;
}

/* ---- VBlank ---- */

static DECLARE_WAIT_QUEUE_HEAD(psxgpu_vblank_wait);
static volatile int psxgpu_vblank_count;

void psxgpu_vblank_interrupt(int irq, void *dev, struct pt_regs *regs)
{
	psxgpu_vblank_count++;
	wake_up_interruptible(&psxgpu_vblank_wait);
}

/* ---- File operations ---- */

static int psxgpu_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int psxgpu_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t psxgpu_write(struct file *filp, const char *buf,
			    size_t count, loff_t *ppos)
{
	uint32_t words[64];
	size_t written = 0;
	size_t chunk;

	if (count & 3)
		return -EINVAL;

	while (written < count) {
		chunk = count - written;
		if (chunk > sizeof(words))
			chunk = sizeof(words);

		if (copy_from_user(words, buf + written, chunk))
			return -EFAULT;

		{
			size_t n = chunk / 4;
			size_t i;
			for (i = 0; i < n; i++) {
				psxgpu_gp0_wait_fifo();
				psxgpu_write_gp0(words[i]);
			}
		}
		written += chunk;
	}

	return written;
}

static ssize_t psxgpu_read(struct file *filp, char *buf,
			   size_t count, loff_t *ppos)
{
	/* reading returns GPUREAD (VRAM read data) 32 bits at a time */
	uint32_t v;
	size_t n = 0;

	while (n + 4 <= count) {
		v = psxgpu_read_gp0();
		if (copy_to_user(buf + n, &v, 4))
			return -EFAULT;
		n += 4;
	}
	return n;
}

static unsigned int psxgpu_poll(struct file *filp, poll_table *table)
{
	unsigned int mask = POLLOUT | POLLWRNORM;

	poll_wait(filp, &psxgpu_vblank_wait, table);
	if (psxgpu_vblank_count)
		mask |= POLLIN;
	return mask;
}

static int psxgpu_ioctl(struct inode *inode, struct file *filp,
			unsigned int cmd, unsigned long arg)
{
	uint32_t v = (uint32_t)arg;

	switch (cmd) {
	case PSXGPU_IOC_GP1:
		/* send a raw GP1 command */
		psxgpu_write_gp1(v);
		break;

	case PSXGPU_IOC_STAT:
		/* return GPUSTAT */
		return put_user(psxgpu_read_gp1(), (uint32_t *)arg);

	case PSXGPU_IOC_VSYNC:
		/* wait for next VBlank interrupt */
		{
			int target = psxgpu_vblank_count + 1;
			int ret = wait_event_interruptible(
				psxgpu_vblank_wait,
				psxgpu_vblank_count >= target);
			if (ret)
				return ret;
		}
		break;

	case PSXGPU_IOC_UPLOAD_VRAM:
		/* copy a buffer to VRAM via GP0 A0h (CPU->VRAM) */
		{
			struct psxgpu_vram_xfer xf;
			uint32_t tmp[512];
			size_t remaining;

			if (copy_from_user(&xf, (void *)arg, sizeof(xf)))
				return -EFAULT;

			/* GP0 A0h: VRAM write at (x,y) size (w,h) */
			psxgpu_gp0_wait_fifo();
			psxgpu_write_gp0(0xA0000000u);
			psxgpu_write_gp0((xf.y & 0x3FF) << 16 | (xf.x & 0x3FF));
			psxgpu_write_gp0(((xf.h - 1) & 0x1FF) << 16 |
					  ((xf.w - 1) & 0x3FF));

			remaining = (size_t)xf.w * xf.h * 2; /* 16bpp */
			while (remaining > 0) {
				size_t n = remaining;
				size_t i;
				if (n > sizeof(tmp))
					n = sizeof(tmp);
				if (copy_from_user(tmp,
						   (void *)(xf.data + ((size_t)xf.w * xf.h * 2) - remaining),
						   n))
					return -EFAULT;
				for (i = 0; i < n / 4; i++) {
					psxgpu_gp0_wait_fifo();
					psxgpu_write_gp0(tmp[i]);
				}
				remaining -= n;
			}
		}
		break;

	case PSXGPU_IOC_RESET:
		psxgpu_write_gp1(0x00000000); /* soft reset */
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static struct file_operations psxgpu_fops = {
	open:		psxgpu_open,
	release:	psxgpu_release,
	write:		psxgpu_write,
	read:		psxgpu_read,
	poll:		psxgpu_poll,
	ioctl:		psxgpu_ioctl,
};

int __init psxgpu_init(void)
{
	int ret;

	ret = register_chrdev(PSXGPU_MAJOR, "psxgpu", &psxgpu_fops);
	if (ret) {
		printk(KERN_ERR "psxgpu: can't get major %d\n", PSXGPU_MAJOR);
		return ret;
	}

	ret = request_irq(VBL, psxgpu_vblank_interrupt, SA_INTERRUPT,
			  "psxgpu", NULL);
	if (ret) {
		printk(KERN_WARNING "psxgpu: VBlank IRQ %d unavailable (%d), "
		       "polling only\n", VBL, ret);
	}

	printk(KERN_INFO "psxgpu: PSX GPU device, major %d\n", PSXGPU_MAJOR);
	return 0;
}

void __exit psxgpu_exit(void)
{
	free_irq(VBL, NULL);
	unregister_chrdev(PSXGPU_MAJOR, "psxgpu");
}

module_init(psxgpu_init);
module_exit(psxgpu_exit);
