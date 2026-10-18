#ifndef _PSXCD_H
#define _PSXCD_H

#include "asm/types.h"

/* low-level CD register driver */
#include <asm/ps/cdrom.h>

#define PSXCD_MAJOR		209

#define SECTOR_SZ_SHIFT		(9)

#define PSXCD_SECTORS		(2048)
#define PSXCD_BLOCK_SHIFT	(10)

typedef unsigned long psxcd_sector_t;

typedef struct {
   uint8_t cmd;
   uint8_t params[4];
   uint8_t status;
   uint8_t reply[2048];
} psxcd_cmdbuf_t;

typedef struct {
   int drive;
   psxcd_sector_t start_sector;
   psxcd_sector_t nb_sectors;
   char *buffer;
   int error;
   int complete;
} psxcd_request_t;

typedef struct {
   psxcd_request_t curr_request;
} psxcd_curr_t;

extern volatile uint8_t *const CD_REGS;

extern int psxcd_drive_init(void);
extern int psxcd_drive_reset(void);
extern psxcd_sector_t psxcd_capacity(void);

#endif
