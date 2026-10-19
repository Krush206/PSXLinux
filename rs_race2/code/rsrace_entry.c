/*
 * rsrace_entry.c — Entry point for rs_race2 running on PSXLinux.
 *
 * The original game uses `void main()` which doesn't work with our
 * -nostdlib build. This file provides the `_start` entry point and
 * calls the game's `GameMain()` function (renamed from `main`).
 */

#include "../../psxdev/psxsys.h"

sint GameMain(void);
extern int printf(const char *fmt, ...);

/* GCC 2.91 MIPS calls __main() before main */
void __main(void) { }

__attribute__((section(".text.start")))
void _start(void)
{
	sint ret;
	printf("rsrace: entry\n");
	ret = GameMain();
	sys_exit(ret);
	for (;;) ;
}
