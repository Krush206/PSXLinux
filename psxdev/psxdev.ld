/*
 * psxdev.ld - Linker script for PSXLinux userspace programs.
 *
 * Places the program at 0x00100000 (1MB), which is above the kernel
 * (0x10000-0xDB440) and within the 2MB PSX RAM.
 */

OUTPUT_FORMAT("elf32-littlemips")
OUTPUT_ARCH(mips)
ENTRY(_start)

SECTIONS
{
    . = 0x00100000;

    .text : {
        *(.text.start)
        *(.text)
        *(.text.*)
    }

    .rodata : {
        *(.rodata)
        *(.rodata.*)
        *(.rdata)
    }

    .data : {
        *(.data)
        *(.data.*)
        *(.sdata)
    }

    _gp = . + 0x7FF0;

    .bss : {
        *(.bss)
        *(.bss.*)
        *(.sbss)
        *(COMMON)
    }

    /DISCARD/ : {
        *(.MIPS.abiflags)
        *(.mdebug*)
        *(.comment)
        *(.note*)
    }
}
