#ifndef _LINUX_PSXSPU_H
#define _LINUX_PSXSPU_H

#include <linux/ioctl.h>

#define PSXSPU_MAJOR	211	/* PSX SPU sound port */

/* ioctl commands */
#define PSXSPU_IOC_MAGIC	'S'

#define PSXSPU_IOC_SET_ADDR	_IOW(PSXSPU_IOC_MAGIC, 1, unsigned int)
#define PSXSPU_IOC_VOICE	_IOW(PSXSPU_IOC_MAGIC, 2, struct psxspu_voice)
#define PSXSPU_IOC_KEY_ON	_IOW(PSXSPU_IOC_MAGIC, 3, unsigned int)
#define PSXSPU_IOC_KEY_OFF	_IOW(PSXSPU_IOC_MAGIC, 4, unsigned int)
#define PSXSPU_IOC_MASTER_VOL	_IOW(PSXSPU_IOC_MAGIC, 5, unsigned int)
#define PSXSPU_IOC_CTRL		_IOW(PSXSPU_IOC_MAGIC, 6, unsigned int)
#define PSXSPU_IOC_STATUS	_IOR(PSXSPU_IOC_MAGIC, 7, unsigned int)

/* SPU voice configuration (one ioctl per voice) */
struct psxspu_voice {
	unsigned int voice;	/* 0..23 */
	unsigned short volume_l;	/* 0x0000..0x3FFF */
	unsigned short volume_r;	/* 0x0000..0x3FFF */
	unsigned short pitch;		/* 0x1000 = 44100 Hz (normal) */
	unsigned short start_addr;	/* sound RAM offset / 8 bytes */
	unsigned short adsr_lo;		/* ADSR level/mode low */
	unsigned short adsr_hi;		/* ADSR level/mode high */
};

#endif /* _LINUX_PSXSPU_H */
