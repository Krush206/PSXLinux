/*
 * gltron_entry.c — Entry point for GLTron on PSXLinux.
 *
 * main() is the game's own (gltron.c); this file only exists if the
 * game's main needs wrapping.  It doesn't, so it is empty apart from
 * the -nostdlib startup that the PSXLinux ECOFF loader needs.
 */

#include "../../psxdev/psxsys.h"

extern int main(int argc, char **argv);

/* GCC 2.91 MIPS calls __main() before main */
void __main(void) { }

__attribute__((section(".text.start")))
void _start(void)
{
	sint ret;

	ret = main(0, 0);
	sys_exit(ret);
	for (;;) ;
}
