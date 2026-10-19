/*
 * psxgpu - PlayStation GPU character device.
 *
 * Exposes the GPU command/status interface to userspace so that a
 * rendering library (minigl) never touches hardware registers
 * directly.
 *
 * Interface (see include/linux/psxgpu.h):
 *   write()  : stream of GP0 command words (rendering, VRAM blits).
 *              The driver waits for the GP0 FIFO ready bit before
 *              every word, so arbitrary-length command streams can
 *              be pushed with a single syscall.
 *   read()   : GP0 data port reads (VRAM -> CPU, after the client
 *              has issued a GP0(C0h) command via write()).
 *   ioctl()  :
 *              PSXGPU_IOC_GP1          write a GP1 control word
 *              PSXGPU_IOC_STAT         read GPUSTAT
 *              PSXGPU_IOC_VSYNC        wait for the interlace-field
 *                                      bit to toggle (frame boundary)
 *              PSXGPU_IOC_UPLOAD_VRAM  bulk CPU -> VRAM transfer
 *              PSXGPU_IOC_RESET        GP1(00h) soft reset
 *
 * Register reference: https://psx-spx.github.io/#gpu-io-ports
 *   1F801810h GP0  write: commands   read: VRAM read data
 *   1F801814h GP1  write: control   read: GPUSTAT
 * GPUSTAT:
 *   bit 27  data available for reading (VRAM -> CPU ready)
 *   bit 28  ready to receive command word (FIFO not full)
 *   bit 31  interlace field (toggles every frame)
 */

#include <linux/config.h>

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/major.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#include <linux/psxgpu.h>

#define GP0_PORT	0x1F801810u
#define GP1_PORT	0x1F801814u

#define GPUSTAT_READY_DATA	(1u << 27)	/* VRAM read data ready */
#define GPUSTAT_READY_CMD	(1u << 28)	/* FIFO ready for a word */
#define GPUSTAT_FIELD		(1u << 31)	/* interlace field */

static inline void gpu_gp0(u32 w)
{
	while (!(readl(GP1_PORT) & GPUSTAT_READY_CMD))
		;
	writel(w, GP0_PORT);
}

static inline void gpu_gp1(u32 w)
{
	writel(w, GP1_PORT);
}

static inline u32 gpu_stat(void)
{
	return readl(GP1_PORT);
}

/* ---- File operations ---- */

static int psxgpu_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int psxgpu_release(struct inode *inode, struct file *filp)
{
	/* drain the FIFO on close so the GPU is idle */
	while (!(gpu_stat() & GPUSTAT_READY_CMD))
		;
	return 0;
}

static ssize_t psxgpu_write(struct file *filp, const char *buf,
			    size_t count, loff_t *ppos)
{
	/*
	 * Stream of 32-bit GP0 command words.  Copy in chunks and
	 * push each word with the FIFO wait in the kernel, so the
	 * client can batch a whole frame (or a whole VRAM upload)
	 * into one syscall.
	 */
	static u32 tmp[256];		/* 1 KB bounce buffer */
	size_t done = 0;
	size_t chunk;
	size_t i;

	if (count & 3)
		count &= ~3;

	while (done < count) {
		chunk = count - done;
		if (chunk > sizeof(tmp))
			chunk = sizeof(tmp);

		if (copy_from_user(tmp, buf + done, chunk))
			return -EFAULT;

		for (i = 0; i < chunk / 4; i++)
			gpu_gp0(tmp[i]);

		done += chunk;
	}

	return done;
}

static ssize_t psxgpu_read(struct file *filp, char *buf,
			   size_t count, loff_t *ppos)
{
	/*
	 * GP0 data port reads: after the client sends a GP0(C0h)
	 * VRAM->CPU blit command (via write()), it reads the pixel
	 * words back here.  Wait for the "data available" bit before
	 * every word.
	 */
	static u32 tmp[256];
	size_t done = 0;
	size_t chunk;
	size_t i;

	if (count & 3)
		count &= ~3;

	while (done < count) {
		chunk = count - done;
		if (chunk > sizeof(tmp))
			chunk = sizeof(tmp);

		for (i = 0; i < chunk / 4; i++) {
			while (!(gpu_stat() & GPUSTAT_READY_DATA))
				;
			tmp[i] = readl(GP0_PORT);
		}

		if (copy_to_user(buf + done, tmp, chunk))
			return -EFAULT;

		done += chunk;
	}

	return done;
}

static int psxgpu_ioctl(struct inode *inode, struct file *filp,
			unsigned int cmd, unsigned long arg)
{
	u32 v = (u32)arg;

	switch (cmd) {
	case PSXGPU_IOC_GP1:
		/* write a GP1 display-control word */
		gpu_gp1(v);
		break;

	case PSXGPU_IOC_STAT:
		/* read GPUSTAT */
		return gpu_stat();

	case PSXGPU_IOC_VSYNC:
		/*
		 * Wait for a frame boundary: GPUSTAT bit 31 (the
		 * interlace-field flag) toggles once per frame.
		 * Bound the loop so a stuck GPU cannot hang the
		 * kernel forever.
		 */
		{
			u32 field = gpu_stat() & GPUSTAT_FIELD;
			long timeout = 20000000;

			while ((gpu_stat() & GPUSTAT_FIELD) == field) {
				if (--timeout <= 0)
					return -ETIME;
			}
		}
		break;

	case PSXGPU_IOC_UPLOAD_VRAM:
		/*
		 * Bulk CPU -> VRAM transfer.  One syscall for a whole
		 * texture; the kernel drives the GP0(A0h) command and
		 * copies the pixels from user memory.
		 */
		{
			struct psxgpu_vram_xfer xf;
			static u16 pixels[512];	/* 1 KB bounce buffer */
			const u16 *src;
			size_t total, done, chunk, i;

			if (copy_from_user(&xf, (void *)arg, sizeof(xf)))
				return -EFAULT;
			if (!xf.w || !xf.h || !xf.data)
				return -EINVAL;

			/* GP0(A0h): command, dest coord, width/height.
			 * The GPU masks (size-1)+1 internally, so raw
			 * w/h are passed (same as the plugin expects). */
			gpu_gp0(0xA0000000u);
			gpu_gp0(((u32)(xf.y & 0x1FF) << 16) |
				(u32)(xf.x & 0x3FF));
			gpu_gp0(((u32)(xf.h & 0x1FF) << 16) |
				(u32)(xf.w & 0x3FF));

			/* pixel data, two halfwords per GP0 word */
			total = (size_t)xf.w * xf.h;
			src = (const u16 *)xf.data;
			done = 0;
			while (done < total) {
				chunk = total - done;
				if (chunk > 512)
					chunk = 512;
				if (chunk & 1)
					chunk--;	/* keep words even */

				if (copy_from_user(pixels, src + done,
						   chunk * 2))
					return -EFAULT;

				for (i = 0; i < chunk; i += 2)
					gpu_gp0(((u32)pixels[i + 1] << 16) |
						pixels[i]);

				done += chunk;
			}

			/* odd halfword count: one final padded word */
			if (total & 1) {
				u16 last;

				if (copy_from_user(&last,
						   src + total - 1, 2))
					return -EFAULT;
				gpu_gp0((u32)last);	/* padded low half */
			}
		}
		break;

	case PSXGPU_IOC_RESET:
		/* GP1(00h): soft reset */
		gpu_gp1(0x00000000u);
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static struct file_operations psxgpu_fops = {
	open:		psxgpu_open,
	release:	psxgpu_release,
	read:		psxgpu_read,
	write:		psxgpu_write,
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

	printk(KERN_INFO "psxgpu: PSX GPU device, major %d\n", PSXGPU_MAJOR);
	return 0;
}

void __exit psxgpu_exit(void)
{
	unregister_chrdev(PSXGPU_MAJOR, "psxgpu");
}

module_init(psxgpu_init);
module_exit(psxgpu_exit);
