#include <linux/types.h>

#include "bios.h"

void * original_disc_error;

/**
 * A0-table location.
 */
static void ** const BIOS_A0_TABLE = (void **) 0x200;

static int console_has_tty(void);
static void *parse_warmboot_jal(uint32_t);
static void bios_copy_relocated_kernel(void);
static void bios_copy_a0_table(void);

static int console_has_tty(void)
{
	/*
	 * Check if the console has a SCN2681 TTY used for debug by writing data to the control
	 * registers and reading it back.
	 *
	 * The control is 16 bit wide, and is accessed by writing or reading twice the same register.
	 */
	volatile uint8_t * scn2681modereg = (uint8_t *) 0x1F802020;

	*scn2681modereg = 0x55;
	*scn2681modereg = 0xAA;
	return *scn2681modereg == 0x55 && *scn2681modereg == 0xAA;
}

void bios_reinitialize(void)
{
	// Disable interrupts
	EnterCriticalSection();

	// Clear kernel heap space. Not really needed but nice for debugging.
	memset((void *) 0xA000E000, 0, 0x2000);

	// The following is adapted from the WarmBoot call

	// Copy the relocatable kernel chunk
	bios_copy_relocated_kernel();

	// Reinitialize the A table
	bios_copy_a0_table();

	// Restore A, B and C tables
	init_a0_b0_c0_vectors();

	// Fix A table
	AdjustA0Table();

	// Install default exception handlers
	InstallExceptionHandlers();

	// Restore default exception return function
	SetDefaultExitFromException();

	// Clear interrupts and mask
	I_STAT = 0;
	I_MASK = 0;

	// Setup devices.
	InstallDevices(console_has_tty());

	/*
	 * Configure with default values
	 *
	 * SetConf call does:
	 *  - Configure the system memory (via SysInitMemory)
	 *  - Initialize the exception handler arrays
	 *  - Enqueues the syscall handler (via EnqueueSyscallHandler)
	 *  - Initializes the default interrupt (via InitDefInt)
	 *  - Allocates the event handler array
	 *  - Allocates the thread structure
	 *  - Enqueues the timer and VBlank handler (via EnqueueTimerAndVblankIrqs)
	 *  - Calls a function that re-configures the CD subsystem as follows:
	 *      - Enqueues the CD interrupt (via EnqueueCdIntr)
	 *      - Opens shit-ton of CD-related events (via OpenEvent)
	 *      - Enables the CD-related events (via EnableEvent)
	 *      - Re-enables interrupts (via ExitCriticalSection)
	 *
	 * This call is to be used after the CdInit has happened, once we've read the SYSTEM.CNF
	 * file and we want to reconfigure the kernel before launching the game's executable.
	 *
	 * However, for the purpose of reinitializing the BIOS, the CD reinitialization procedure is
	 * problematic, as CdInit (which we'll call later once the disc is swapped) calls this very
	 * same function, resulting in the CD interrupt being added twice to the array of handlers,
	 * as wells as events being opened twice.
	 *
	 * We can't patch this code because it's stored in ROM. Instead, before calling this function
	 * we'll replace the EnqueueCdIntr with a fake version that patches the system state to return
	 * earlier and avoid the CD reinitialization entirely.
	 */
	{
		void * realEnqueueCdIntr = BIOS_A0_TABLE[0xA2];
		BIOS_A0_TABLE[0xA2] = FakeEnqueueCdIntr;
		SetConf(BIOS_DEFAULT_EVCB, BIOS_DEFAULT_TCB, BIOS_DEFAULT_STACKTOP);
		BIOS_A0_TABLE[0xA2] = realEnqueueCdIntr;
	}

	// End of code adapted

	/*
	 * Set RAM size to 8MB, which is incorrect but it's what the BIOS sets.
	 *
	 * This is required because the entrypoint game might've set it to 2MB, and a bugged target
	 * game might accidentally access an address in the mirror region, causing a fault to be caused
	 * in real hardware.
	 */
	SetMemSize(8);

	// Re-enable interrupts
	ExitCriticalSection();

	// Save for later
	original_disc_error = BIOS_A0_TABLE[0xA1];
}

static void * parse_warmboot_jal(uint32_t opcode)
{
	const uint32_t * warmboot_start = (const uint32_t *) BIOS_A0_TABLE[0xA0];
	uint32_t prefix = (uint32_t) warmboot_start & 0xF0000000;

	uint32_t * jal = (uint32_t *) (warmboot_start + opcode);
	uint32_t suffix = (*jal & 0x3FFFFFF) << 2;

	return (void *) (prefix | suffix);
}

static void bios_copy_relocated_kernel(void)
{
	/*
	 * This function indirectly calls the BIOS function that copies the relocated kernel code to
	 * 0x500.
	 *
	 * The 12th opcode of WarmBoot is a "jal" to this function for all BIOS I've checked,
	 * including the PS2 consoles in PS1 mode.
	 */

	void * address = parse_warmboot_jal(12);
	((void (*)(void)) address)();
}

static void bios_copy_a0_table(void)
{
	/*
	 * This function indirectly calls the BIOS function that copies the A0 table to 0x200.
	 *
	 * As with the kernel relocation function, the 14th opcode of WarmBoot is a "jal" to this
	 * function.
	 */

	void * address = parse_warmboot_jal(14);
	((void (*)(void)) address)();
}
