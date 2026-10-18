/*
 * psxdev.h - Userspace device ioctl definitions for PSXLinux.
 *
 * These mirror the kernel headers but are self-contained (no dependency
 * on kernel-internal headers like <asm/ioctl.h>).
 */

#ifndef _PSXDEV_H
#define _PSXDEV_H

/* MIPS IOC: _IOC(dir,type,nr,size) — same encoding as asm-mipsnommu */
#define _PSX_IOC(dir, type, nr, size) \
	(((dir)  << 30) | ((size) << 16) | ((type) << 8) | (nr))
#define _PSX_IOC_NONE	0U
#define _PSX_IOC_READ	1U
#define _PSX_IOC_WRITE	2U

/*
 * ---- GPU (major 210) ----
 */
#define PSXGPU_MAJOR		210

#define PSXGPU_IOC_MAGIC	'G'

#define PSXGPU_IOC_GP1		_PSX_IOC(_PSX_IOC_WRITE, PSXGPU_IOC_MAGIC, 1, 4)
#define PSXGPU_IOC_STAT		_PSX_IOC(_PSX_IOC_READ,  PSXGPU_IOC_MAGIC, 2, 4)
#define PSXGPU_IOC_VSYNC	_PSX_IOC(_PSX_IOC_NONE,  PSXGPU_IOC_MAGIC, 3, 0)
#define PSXGPU_IOC_UPLOAD_VRAM	_PSX_IOC(_PSX_IOC_WRITE, PSXGPU_IOC_MAGIC, 4, 20)
#define PSXGPU_IOC_RESET	_PSX_IOC(_PSX_IOC_NONE,  PSXGPU_IOC_MAGIC, 5, 0)

struct psxgpu_vram_xfer {
	unsigned int x;
	unsigned int y;
	unsigned int w;
	unsigned int h;
	const void *data;
};

/*
 * ---- SPU (major 211) ----
 */
#define PSXSPU_MAJOR		211

#define PSXSPU_IOC_MAGIC	'S'

#define PSXSPU_IOC_SET_ADDR	_PSX_IOC(_PSX_IOC_WRITE, PSXSPU_IOC_MAGIC, 1, 4)
#define PSXSPU_IOC_VOICE	_PSX_IOC(_PSX_IOC_WRITE, PSXSPU_IOC_MAGIC, 2, 16)
#define PSXSPU_IOC_KEY_ON	_PSX_IOC(_PSX_IOC_WRITE, PSXSPU_IOC_MAGIC, 3, 4)
#define PSXSPU_IOC_KEY_OFF	_PSX_IOC(_PSX_IOC_WRITE, PSXSPU_IOC_MAGIC, 4, 4)
#define PSXSPU_IOC_MASTER_VOL	_PSX_IOC(_PSX_IOC_WRITE, PSXSPU_IOC_MAGIC, 5, 4)
#define PSXSPU_IOC_CTRL		_PSX_IOC(_PSX_IOC_WRITE, PSXSPU_IOC_MAGIC, 6, 4)
#define PSXSPU_IOC_STATUS	_PSX_IOC(_PSX_IOC_READ,  PSXSPU_IOC_MAGIC, 7, 4)

struct psxspu_voice {
	unsigned int voice;
	unsigned short volume_l;
	unsigned short volume_r;
	unsigned short pitch;
	unsigned short start_addr;
	unsigned short adsr_lo;
	unsigned short adsr_hi;
};

#endif /* _PSXDEV_H */
