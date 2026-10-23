/*
 * PSX Memory Card (psxmemcard) ioctl interface
 */

#ifndef _LINUX_PSXMEMCARD_H
#define _LINUX_PSXMEMCARD_H

#include <linux/ioctl.h>

/* Major number */
#define PSXMEMCARD_MAJOR    211

#define PSXMEMCARD_MAX_PORTS  2

/* Memory Card constants */
#define MC_BLOCK_SIZE       8192
#define MC_SECTOR_SIZE      64
#define MC_BLOCKS_PER_CARD  16
#define MC_CARD_SIZE        (MC_BLOCK_SIZE * MC_BLOCKS_PER_CARD)  /* 128KB */

/* Memory Card commands */
#define MC_CMD_READ         0x52
#define MC_CMD_WRITE        0x57
#define MC_CMD_GET_ID       0x53
#define MC_CMD_FORMAT       0x46
#define MC_CMD_CHECK        0x43

/* Memory Card ID structure */
struct psxmemcard_id {
    __u8 type;         /* 0x00 = standard */
    __u8 size;         /* 0x00 = 128KB */
    __u8 reserved[2];
    __u8 manufacturer[8];
    __u8 product[8];
    __u8 version[8];
};

/* Ioctl commands */
#define PSXMEMCARD_IOC_MAGIC    'M'

#define PSXMEMCARD_IOC_GET_ID   _IOR(PSXMEMCARD_IOC_MAGIC, 0x01, struct psxmemcard_id)
#define PSXMEMCARD_IOC_GET_SIZE _IOR(PSXMEMCARD_IOC_MAGIC, 0x02, int)
#define PSXMEMCARD_IOC_FORMAT   _IO (PSXMEMCARD_IOC_MAGIC, 0x10)

#define PSXMEMCARD_IOC_MAXNR    0x10

#endif /* _LINUX_PSXMEMCARD_H */