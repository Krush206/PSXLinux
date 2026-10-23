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
#include <linux/string.h>

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
    cd_command(CD_CMD_INIT, NULL, 0);
    if (cd_wait_int() == 0)
        return 0;
    return 1;
}

int psxcd_drive_reset(void) {
    unsigned int i;

    cd_command(CD_CMD_RESET, NULL, 0);
    if (cd_wait_int() == 0)
        return 0;

    for (i = 0; i < 0x400000; i++)
        ;

    return 1;
}

/* Synchronously read a single 2048 byte sector from the CD-ROM. */
static int psxcd_read_sector(unsigned long lba, char *buf) {
    uint8_t msf[3];
    uint8_t s1, s2, s3;
    int i;

    lba_to_msf(lba, &msf[0], &msf[1], &msf[2]);

    printk(KERN_ERR "psxcd: read lba=%ld msf=%02x:%02x:%02x\n",
           lba, msf[0], msf[1], msf[2]);
    cd_command(CD_CMD_SETLOC, msf, 3);
    printk(KERN_ERR "psxcd: SETLOC sent, waiting...\n");
    s1 = cd_wait_int();
    printk(KERN_ERR "psxcd: SETLOC int=%d\n", s1);
    if (s1 == 0) {
        return 1;
    }

    /* Start streaming */
    cd_command(CD_CMD_READN, NULL, 0);
    printk(KERN_ERR "psxcd: READN sent, waiting ack...\n");
    s2 = cd_wait_int();
    printk(KERN_ERR "psxcd: READN ack=%d\n", s2);
    if (s2 == 0) {
        return 1;
    }

    /* Second interrupt: DataReady — data available in transfer buffer */
    printk(KERN_ERR "psxcd: waiting data...\n");
    s3 = cd_wait_int();
    printk(KERN_ERR "psxcd: READN data=%d\n", s3);
    if (s3 == 0) {
        return 1;
    }

    /* Switch to page 0 so that reg 3 write is treated as "transfer
     * start" (case 0), not "ack Stat" (case 1) */
    cd_set_page(0);
    CD_REGS[3] = 0x80;
    /* Sector data starts at Transfer+0: with Setmode size 2048 the
     * drive's data port yields pure user data (psx-spx RDDATA) */
    for (i = 0; i < PSXCD_HARDSECSIZE; i++)
        buf[i] = CD_REGS[2];

    /* Debug: dump first 16 bytes of first few sectors */
    if (lba <= 18) {
        printk(KERN_ERR "psxcd: lba=%ld data:", lba);
        for (i = 0; i < 16; i++)
            printk("%02x ", (unsigned char)buf[i]);
        printk("\n");
    }

    /* Stop the drive */
    cd_command(CD_CMD_GETSTAT, NULL, 0);
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

        /* Convert 512-byte block layer sectors to 2048-byte CD sectors */
        for (i = 0; i < nsect; i += 4) {
            unsigned long cd_lba = (start + i) >> 2; /* 512→2048 sector */
            char *dst = (char *) CURRENT->buffer + (i << 9);
            unsigned long tocopy = (nsect - i) << 9;
            static char tmp[2048];

            if (tocopy > 2048)
                tocopy = 2048;

            if (psxcd_read_sector(cd_lba, tmp)) {
                end_request (FALSE);
                goto next;
            }
            memcpy(dst, tmp, tocopy);
        }
        end_request (TRUE);
    next: ;
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
    /* Device size in 1K blocks; 0 = unknown size = read may be rejected */
    psxcd_sizes[0] = 350000;  /* ~350MB, typical CD capacity */
    blk_size[MAJOR_NR] = psxcd_sizes;

    printk (KERN_INFO DEVICE_NAME ": PSX CD-ROM block driver, major %d\n",
            MAJOR_NR);
    return 0;
}

void __exit psxcd_exit (void) {
    unregister_blkdev (MAJOR_NR, DEVICE_NAME);
}
