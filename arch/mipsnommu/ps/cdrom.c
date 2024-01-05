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

#include "cdrom.h"
#include <asm/ps/hwregs.h>
#include <linux/types.h>

volatile uint8_t *const CD_REGS = (volatile uint8_t *) (0x1F800000 + CDREG0_PORT);

inline void cd_set_page(uint8_t page) {
	CD_REGS[0] = page;
}

void cd_command(uint8_t cmd, const uint8_t * params) {

	// Wait for previous command to finish, if any
	while (CD_REGS[0] & 0x80)
		;

	// Switch to page 0
	cd_set_page(0);

	// Clear read and write FIFOs
	CD_REGS[3] = 0xC0;

	// Copy request
	while (params != NULL) {
		CD_REGS[2] = *params;
		params++;
	}

	// Switch to page 1
	cd_set_page(1);

	// Disable interrupts as we'll poll
	CD_REGS[2] = 0x00;

	// Acknowledge interrupts, if there were any
	CD_REGS[3] = 0x07;

	// Switch to page 0
	cd_set_page(0);

	// Finally write command to start
	CD_REGS[1] = cmd;
}

uint8_t cd_wait_int(void) {
	uint8_t interrupt;

	// Wait for command to finish, if any
	while (CD_REGS[0] & 0x80)
		;

	// Switch to page 1
	cd_set_page(1);

	// Wait until an interrupt happens (int != 0)
	do
		interrupt = CD_REGS[3] & 0x07;
	while (interrupt == 0);

	// Acknowledge it
	CD_REGS[3] = 0x07;

	// Return it
	return interrupt;
}

uint8_t cd_read_reply(uint8_t * reply_buffer) {
	uint8_t len = 0;

	// Switch to page 1
	cd_set_page(1);

	// Read reply
	while (CD_REGS[0] & 0x20) {
		*reply_buffer = CD_REGS[1];
		reply_buffer++;
		len++;
	}

	// Return length
	return len;
}

int cd_drive_init() {
	cd_command(CD_CMD_INIT, NULL);

	// Should succeed with 3
	if (cd_wait_int() != 3)
		return 0;

	// Should then return a 2
	if (cd_wait_int() != 2)
		return 0;

	return 1;
}

int cd_drive_reset() {
	unsigned int i;

	// Issue a reset
	cd_command(CD_CMD_RESET, NULL);

	// Should succeed with 3
	if (cd_wait_int() != 3)
		return 0;

	// Need to wait for some cycles before it springs back to life
	for (i = 0; i < 0x400000; i++)
		;

	return 1;
}
