/*
 * psxspu - PlayStation SPU (sound) character device.
 *
 * Exposes the SPU voice/sample interface to userspace.
 *   write()   : copy 16-bit sample data into sound RAM via DMA
 *   ioctl()   : voice control (key on/off, volume, pitch, ADSR), master vol
 *
 * Register reference: https://psx-spx.github.io/#spu-io-ports
 *   1F801C00h + i*16 : voice i registers
 *     +00h  volume left
 *     +02h  volume right
 *     +04h  pitch (sample rate, 0x1000 = 44100Hz)
 *     +06h  start address (in 8-byte units into 512K sound RAM)
 *     +08h  ADSR level/mode low
 *     +0Ah  ADSR level/mode high
 *     +0Ch  ADSR current volume (read-only)
 *     +0Eh  repeat address (read-only)
 *   1F801D80h  main volume left
 *   1F801D82h  main volume right
 *   1F801D88h  key on  (voices 0-15)
 *   1F801D8Ah  key on  (voices 16-23)
 *   1F801D8Ch  key off (voices 0-15)
 *   1F801D8Eh  key off (voices 16-23)
 *   1F801DAAh  SPUCNT
 *   1F801DA6h  sound RAM transfer address (in 16-bit halfwords)
 *   1F801DA8h  sound RAM transfer data (16-bit FIFO port)
 *   1F801DACh  SPU status
 */

#include <linux/config.h>

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/major.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/stat.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/ps/hwregs.h>

#ifdef CONFIG_DEVFS_FS
#include <linux/devfs_fs_kernel.h>
#endif

#include <linux/psxspu.h>

#define SPU_BASE	  0x1F801C00u
#define SPU_VOICE(i)	  (SPU_BASE + (i) * 16u)
#define SPU_VOL_L(i)	  (SPU_VOICE(i) + 0x00)
#define SPU_VOL_R(i)	  (SPU_VOICE(i) + 0x02)
#define SPU_PITCH(i)	  (SPU_VOICE(i) + 0x04)
#define SPU_START(i)	  (SPU_VOICE(i) + 0x06)
#define SPU_ADSR_LO(i)	  (SPU_VOICE(i) + 0x08)
#define SPU_ADSR_HI(i)	  (SPU_VOICE(i) + 0x0A)
#define SPU_ADDR(i)	  (SPU_VOICE(i) + 0x0C)

#define SPU_MASTER_VOL_L  0x1F801D80u
#define SPU_MASTER_VOL_R  0x1F801D82u
#define SPU_KEY_ON_LO	  0x1F801D88u
#define SPU_KEY_ON_HI	  0x1F801D8Au
#define SPU_KEY_OFF_LO	  0x1F801D8Cu
#define SPU_KEY_OFF_HI	  0x1F801D8Eu
#define SPU_CTRL	  0x1F801DAAu
#define SPU_RAM_ADDR	  0x1F801DA6u
#define SPU_RAM_DATA	  0x1F801DA8u
#define SPU_STATUS	  0x1F801DACu

/* SPUCNT bits (psx-spx: 1F801DAAh) */
#define SPUCNT_ENABLE	  (1u << 15)	/* SPU on */
#define SPUCNT_UNMUTE	  (1u << 14)	/* 0 = muted */
#define SPUCNT_REVERB	  (1u << 7)
#define SPUCNT_DMA_WRITE  (5u << 4)	/* 5 = write to sound RAM */

static void psxspu_writew(uint32_t addr, uint16_t v)
{
	writew(v, addr);
}

static uint16_t psxspu_readw(uint32_t addr)
{
	return readw(addr);
}

/* ---- File operations ---- */

static int psxspu_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int psxspu_release(struct inode *inode, struct file *filp)
{
	/* key off all voices on last close */
	psxspu_writew(SPU_KEY_OFF_LO, 0xFFFF);
	psxspu_writew(SPU_KEY_OFF_HI, 0x00FF);
	return 0;
}

static ssize_t psxspu_write(struct file *filp, const char *buf,
			    size_t count, loff_t *ppos)
{
	/*
	 * Upload 16-bit sample data to sound RAM at the current transfer
	 * address.  The userspace library sets the transfer address with
	 * PSXSPU_IOC_SET_ADDR first, then write()s the sample bytes here.
	 *
	 * Transfer address is in 16-bit halfword units (0x1F801DA6).
	 * Data port is 16-bit wide (0x1F801DA8).
	 */
	static uint32_t xfer_addr;	/* in halfwords */
	uint16_t tmp[256];
	size_t done = 0;
	size_t chunk;

	if (count & 1)
		count--;

	while (done < count) {
		chunk = count - done;
		if (chunk > sizeof(tmp))
			chunk = sizeof(tmp);

		if (copy_from_user(tmp, buf + done, chunk))
			return -EFAULT;

		{
			size_t i;
			for (i = 0; i < chunk / 2; i++)
				psxspu_writew(SPU_RAM_DATA, tmp[i]);
			xfer_addr += chunk / 2;
		}
		done += chunk;
	}

	return done;
}

static int psxspu_ioctl(struct inode *inode, struct file *filp,
			unsigned int cmd, unsigned long arg)
{
	uint32_t v = (uint32_t)arg;

	switch (cmd) {
	case PSXSPU_IOC_SET_ADDR:
		/* set sound RAM transfer address (in halfwords) */
		psxspu_writew(SPU_RAM_ADDR, (uint16_t)(v & 0xFFFF));
		/* select "transfer to sound RAM" direction */
		psxspu_writew(SPU_CTRL, SPUCNT_ENABLE | SPUCNT_UNMUTE |
			      SPUCNT_DMA_WRITE);
		break;

	case PSXSPU_IOC_VOICE:
		/* configure a voice */
		{
			struct psxspu_voice vs;
			unsigned int i;

			if (copy_from_user(&vs, (void *)arg, sizeof(vs)))
				return -EFAULT;
			if (vs.voice >= 24)
				return -EINVAL;

			i = vs.voice;
			psxspu_writew(SPU_VOL_L(i), vs.volume_l);
			psxspu_writew(SPU_VOL_R(i), vs.volume_r);
			psxspu_writew(SPU_PITCH(i), vs.pitch);
			psxspu_writew(SPU_START(i), vs.start_addr);
			psxspu_writew(SPU_ADSR_LO(i), vs.adsr_lo);
			psxspu_writew(SPU_ADSR_HI(i), vs.adsr_hi);
		}
		break;

	case PSXSPU_IOC_KEY_ON:
		/* key on voice(s); arg = bitmask */
		{
			uint16_t lo = (uint16_t)(v & 0xFFFF);
			uint16_t hi = (uint16_t)((v >> 16) & 0xFF);
			uint16_t prev_lo, prev_hi;

			/* key off first to restart the sample */
			prev_lo = psxspu_readw(SPU_KEY_ON_LO);
			prev_hi = psxspu_readw(SPU_KEY_ON_HI);
			(void)prev_lo; (void)prev_hi;

			psxspu_writew(SPU_KEY_OFF_LO, lo);
			psxspu_writew(SPU_KEY_OFF_HI, hi);
			psxspu_writew(SPU_KEY_ON_LO, lo);
			psxspu_writew(SPU_KEY_ON_HI, hi);
		}
		break;

	case PSXSPU_IOC_KEY_OFF:
		psxspu_writew(SPU_KEY_OFF_LO, (uint16_t)(v & 0xFFFF));
		psxspu_writew(SPU_KEY_OFF_HI, (uint16_t)((v >> 16) & 0xFF));
		break;

	case PSXSPU_IOC_MASTER_VOL:
		psxspu_writew(SPU_MASTER_VOL_L, (uint16_t)(v & 0xFFFF));
		psxspu_writew(SPU_MASTER_VOL_R, (uint16_t)(v & 0xFFFF));
		break;

	case PSXSPU_IOC_CTRL:
		psxspu_writew(SPU_CTRL, (uint16_t)(v & 0xFFFF));
		break;

	case PSXSPU_IOC_STATUS:
		return psxspu_readw(SPU_STATUS);

	default:
		return -EINVAL;
	}

	return 0;
}

static struct file_operations psxspu_fops = {
	open:		psxspu_open,
	release:	psxspu_release,
	write:		psxspu_write,
	ioctl:		psxspu_ioctl,
};

int __init psxspu_init(void)
{
	int ret;
	int i;

	ret = register_chrdev(PSXSPU_MAJOR, "psxspu", &psxspu_fops);
	if (ret) {
		printk(KERN_ERR "psxspu: can't get major %d\n", PSXSPU_MAJOR);
		return ret;
	}

#ifdef CONFIG_DEVFS_FS
	devfs_register(NULL, "psxspu", DEVFS_FL_DEFAULT,
		       PSXSPU_MAJOR, 0,
		       S_IFCHR | S_IRUSR | S_IWUSR,
		       &psxspu_fops, NULL);
#endif

	/* silence all voices at boot */
	for (i = 0; i < 24; i++) {
		psxspu_writew(SPU_VOL_L(i), 0);
		psxspu_writew(SPU_VOL_R(i), 0);
	}
	psxspu_writew(SPU_KEY_OFF_LO, 0xFFFF);
	psxspu_writew(SPU_KEY_OFF_HI, 0x00FF);
	psxspu_writew(SPU_MASTER_VOL_L, 0x3FFF);
	psxspu_writew(SPU_MASTER_VOL_R, 0x3FFF);
	/* enable SPU, unmute, no reverb, transfer=0 */
	psxspu_writew(SPU_CTRL, SPUCNT_ENABLE | SPUCNT_UNMUTE);

	printk(KERN_INFO "psxspu: PSX SPU device, major %d\n", PSXSPU_MAJOR);
	return 0;
}

void __exit psxspu_exit(void)
{
	psxspu_writew(SPU_KEY_OFF_LO, 0xFFFF);
	psxspu_writew(SPU_KEY_OFF_HI, 0x00FF);
	psxspu_writew(SPU_CTRL, 0);
	unregister_chrdev(PSXSPU_MAJOR, "psxspu");
}

module_init(psxspu_init);
module_exit(psxspu_exit);
