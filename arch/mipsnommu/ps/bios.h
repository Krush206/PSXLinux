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

#define _REG32(x) (*((volatile uint32_t *) x))
#define _REG16(x) (*((volatile uint16_t *) x))

#define I_STAT _REG32(0x1F801070)
#define I_MASK _REG32(0x1F801074)

#define BIOS_DEFAULT_EVCB 0x10
#define BIOS_DEFAULT_TCB 0x4
#define BIOS_DEFAULT_STACKTOP ((void *) 0x801FFF00)

/**
 * Fake version of the EnqueueCdIntr call that we'll use to skip the CD reinitialization in the
 * SetConf call during the BIOS reinitialization.
 */
void FakeEnqueueCdIntr(void);

/**
 * Configured the RAM size.
 *
 * @param size memory size in megabytes, either 2 or 8.
 *
 * Table A, call 0x9F.
 */
void SetMemSize(uint32_t);

/**
 * (Re-)initializes kernel resources.
 *
 * @param evcb max number of events
 * @param tcb max number of threads
 * @param stacktop stack top
 *
 * Table A, call 0x9C.
 */
void SetConf(uint32_t, uint32_t, void *);

/**
 * Initializes the default device drivers for the TTY, CDROM and memory cards.
 *
 * The flags controls whether the TTY device should be a dummy (retail console) or an actual UART (dev console).
 * Note this will call will freeze if the UART is enabled but there is no such device.
 *
 * @param enable_tty 0 to use a dummy TTY, 1 to use a real UART.
 *
 * Table C, call 0x12.
 */
void InstallDevices(uint32_t);

/**
 * Exit critical section.
 *
 * Re-enables interrupts.
 */
void ExitCriticalSection(void);

/**
 * Enter critical section.
 *
 * Disables interrupts so code executes atomically.
 */
void EnterCriticalSection(void);

/**
 * Copies the three default four-opcode handlers for the A(NNh),B(NNh),C(NNh) functions to A00000A0h..A00000CFh.
 *
 * Table A, call 0x45.
 */
void init_a0_b0_c0_vectors(void);

/**
 * Restores the default exception exit handler.
 *
 * Table B, call 0x18.
 */
void SetDefaultExitFromException(void);

/**
 * Copies the default four-opcode exception handler to the exception vector at 0x80000080h~0x8000008F.
 *
 * Table C, call 0x07.
 */
void InstallExceptionHandlers(void);

/**
 * Fixes the A call table, copying missing entries from the C table.
 *
 * Table C, call 0x1C.
 */
void AdjustA0Table(void);
