#ifndef _LINUX_PSXGPU_H
#define _LINUX_PSXGPU_H

#include <linux/ioctl.h>

#define PSXGPU_MAJOR	210	/* PSX GPU command port */

/* ioctl commands */
#define PSXGPU_IOC_MAGIC	'G'

#define PSXGPU_IOC_GP1		_IOW(PSXGPU_IOC_MAGIC, 1, unsigned int)
#define PSXGPU_IOC_STAT		_IOR(PSXGPU_IOC_MAGIC, 2, unsigned int)
#define PSXGPU_IOC_VSYNC	_IO (PSXGPU_IOC_MAGIC, 3)
#define PSXGPU_IOC_UPLOAD_VRAM	_IOW(PSXGPU_IOC_MAGIC, 4, struct psxgpu_vram_xfer)
#define PSXGPU_IOC_RESET	_IO (PSXGPU_IOC_MAGIC, 5)

/* CPU->VRAM transfer descriptor */
struct psxgpu_vram_xfer {
	unsigned int x;		/* VRAM x coordinate (pixels) */
	unsigned int y;		/* VRAM y coordinate (pixels) */
	unsigned int w;		/* width in pixels */
	unsigned int h;		/* height in pixels */
	const void *data;	/* pointer to 16bpp pixel data */
};

#endif /* _LINUX_PSXGPU_H */
