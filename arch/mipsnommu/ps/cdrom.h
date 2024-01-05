/*
 * DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
 *                    Version 2, December 2004
 * 
 * Copyright (C) 2004 Sam Hocevar <sam@hocevar.net>
 * 
 * Everyone is permitted to copy and distribute verbatim or modified
 * copies of this license document, and changing it is allowed as long
 * as the name is changed.
 * 
 *            DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
 *   TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION
 * 
 *  0. You just DO WHAT THE FUCK YOU WANT TO.
 */

#ifndef _CDROMPSX_H
#define _CDROMPSX_H
#include <linux/types.h>

#define CD_CMD_GETSTAT 0x01
#define CD_CMD_SETLOC 0x02
#define CD_CMD_READN 0x06
#define CD_CMD_INIT 0x0A
#define CD_CMD_SEEKL 0x15
#define CD_CMD_TEST 0x19
#define CD_CMD_RESET 0x1C
#define CD_TEST_REGION 0x22

extern volatile uint8_t *const CD_REGS;

/*
 * Starts executing a CD command.
 *
 * @param cmd the command
 * @param params the param buffer, or NULL if length is zero
 * @param params_len parameter length in bytes
 */
extern inline void cd_command(uint8_t cmd, const uint8_t * params);

/*
 * Waits for an interrupt to happen, and returns its code.
 *
 * @returns interrupt code
 */
extern uint8_t cd_wait_int(void);

/*
 * Reads a reply from the controller after an interrupt has happened.
 *
 * @param reply reply buffer
 * @returns reply length
 */
extern uint8_t cd_read_reply(uint8_t * reply_buffer);

/*
 * Reinitializes the CD drive.
 *
 * @returns true if succeded, or false otherwise.
 */
extern int cd_drive_init(void);

/*
 * Resets the drive.
 *
 * @returns true if succeded, or false otherwise.
 */
extern int cd_drive_reset(void);
#endif
