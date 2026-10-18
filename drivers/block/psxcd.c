/*
 * psxcd - PlayStation CD-ROM block driver.
 *
 * Sits on top of the low-level register driver in
 * arch/mipsnommu/ps/cdrom.c and exposes the drive as a Linux block
 * device so it can be mounted (ISO9660).
 */

#include <linux/config.h>

#define MAJOR_NR   PSXCD_MAJOR

#include <linux/major.h>
#include <linux/malloc.h>
#include <linux/vmalloc.h>
#include <linux/blk.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/spinlock.h>
#include <linux/fs.h>
#include <linux/hdreg.h>
#include <linux/blkpg.h>

#include <asm/io.h>
#include <asm/delay.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/ps/interrupts.h>

#include "psxcd.h"

#define FALSE  (0)
#define TRUE   (1)

#define SECTOR_SZ_SHIFT    (9)
#define PSXCD_MINORS       (1)
#define MINORS             PSXCD_MINORS
#define PSXCD_BLOCK_SIZE   (1024)
#define PSXCD_BLOCK_SHIFT  (10)
#define PSXCD_HARDSECSIZE  (2048)
#define PSXCD_RAHEAD       (2)

static int psxcd_hardsects[MINORS];
static int psxcd_blocksizes[MINORS];
static int psxcd_sizes[MINORS];

static volatile int psxcd_open = FALSE;
static DECLARE_WAIT_QUEUE_HEAD (psxcd_wait);

/* Convert an LBA (2048-byte data sector) into the BCD minute/second/frame
 * triple SETLOC expects.  The data track starts at 2 seconds lead-in. */
static void lba_to_msf(unsigned long lba, uint8_t *m, uint8_t *s, uint8_t *f)
{
    unsigned long min, sec, frame;
    unsigned long tmp = lba + 150;

    min   = tmp / 75 / 60;
    sec   = (tmp / 75) % 60;
    frame = tmp % 75;
    *m = (uint8_t) (((min / 10) << 4) | (min % 10));
    *s = (uint8_t) (((sec / 10) << 4) | (sec % 10));
    *f = (uint8_t) (((frame / 10) << 4) | (frame % 10));
}

/* ---- Public interface ------------------------------------------------- */

int psxcd_drive_init(void) {
    cd_command(CD_CMD_INIT, NULL);
    if (cd_wait_int() != 3)
        return 0;
    if (cd_wait_int() != 2)
        return 0;
    return 1;
}

int psxcd_drive_reset(void) {
    unsigned int i;

    cd_command(CD_CMD_RESET, NULL);
    if (cd_wait_int() != 3)
        return 0;

    for (i = 0; i < 0x400000; i++)
        ;

    return 1;
}

/* Synchronously read a single 2048 byte sector from the CD-ROM. */
static int psxcd_read_sector(unsigned long lba, char *buf) {
    uint8_t msf[3];
    int i;

    lba_to_msf(lba, &msf[0], &msf[1], &msf[2]);

    cd_command(CD_CMD_SETLOC, msf);
    if (cd_wait_int() != 3)
        return 1;

    cd_command(CD_CMD_SEEKL, NULL);
    if (cd_wait_int() != 2)
        return 1;

    /* Start streaming */
    cd_command(CD_CMD_READN, NULL);

    /* Wait for the data-ready interrupt */
    if (cd_wait_int() != 1)
        return 1;

    /* Pull 2048 bytes out of the data FIFO */
    for (i = 0; i < PSXCD_HARDSECSIZE; i++)
        buf[i] = CD_REGS[1];

    /* Stop the drive */
    cd_command(CD_CMD_GETSTAT, NULL);
    (void) cd_read_reply(NULL);

    return 0;
}

/* ---- Block device request handling ------------------------------------ */

static void do_psxcd_request (request_queue_t * q) {
    unsigned long start, nsect, i;

    while (TRUE) {
        INIT_REQUEST;

        start = CURRENT->sector;
        nsect = CURRENT->current_nr_sectors;

        if (CURRENT->cmd != READ) {
            printk (KERN_ERR DEVICE_NAME ": CD-ROM is read-only\n");
            end_request (FALSE);
            continue;
        }

        for (i = 0; i < nsect; i++) {
            if (psxcd_read_sector(start + i,
                    (char *) CURRENT->buffer + (i << SECTOR_SZ_SHIFT)))
                break;
        }
        end_request(i >= nsect);
    }
}

/* ---- Module init/exit ------------------------------------------------- */

static int psxcd_open_drive (struct inode *inode, struct file *filp) {
    if (psxcd_open)
        return -EBUSY;
    psxcd_open = TRUE;
    return 0;
}

static int psxcd_close_drive (struct inode *inode, struct file *filp) {
    psxcd_open = FALSE;
    return 0;
}

static int psxcd_ioctl (struct inode *inode, struct file *filp,
			unsigned int cmd, unsigned long arg)
{
	return blk_ioctl (inode->i_rdev, cmd, arg);
}

static struct block_device_operations psxcd_fops = {
	open:     psxcd_open_drive,
	release:  psxcd_close_drive,
	ioctl:    psxcd_ioctl,
};

int __init psxcd_init (void) {
    int i;

    if (register_blkdev (MAJOR_NR, DEVICE_NAME, &psxcd_fops)) {
        printk (KERN_ERR DEVICE_NAME ": Unable to get major %d\n", MAJOR_NR);
        return -EBUSY;
    }

    for (i = 0; i < MINORS; i++) {
        psxcd_hardsects[i]  = PSXCD_HARDSECSIZE;
        psxcd_blocksizes[i] = PSXCD_BLOCK_SIZE;
        psxcd_sizes[i]      = 0;
    }

    blk_init_queue (BLK_DEFAULT_QUEUE (MAJOR_NR), DEVICE_REQUEST);
    blk_size[MAJOR_NR]      = psxcd_sizes;
    blksize_size[MAJOR_NR]  = psxcd_blocksizes;
    hardsect_size[MAJOR_NR] = psxcd_hardsects;
    read_ahead[MAJOR_NR]    = PSXCD_RAHEAD;

    if (!psxcd_drive_reset()) {
        printk (KERN_WARNING DEVICE_NAME ": drive reset failed\n");
        unregister_blkdev (MAJOR_NR, DEVICE_NAME);
        return -EIO;
    }
    if (!psxcd_drive_init()) {
        printk (KERN_WARNING DEVICE_NAME ": drive init failed\n");
        unregister_blkdev (MAJOR_NR, DEVICE_NAME);
        return -EIO;
    }

    printk (KERN_INFO DEVICE_NAME ": PSX CD-ROM block driver, major %d\n",
            MAJOR_NR);
    return 0;
}

void __exit psxcd_exit (void) {
    unregister_blkdev (MAJOR_NR, DEVICE_NAME);
}
