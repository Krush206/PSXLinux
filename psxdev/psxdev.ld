/*
 * psxdev.ld - Linker script for PSXLinux userspace programs.
 *
 * Places the program at 0x00100000 (1MB), which is above the kernel
 * (0x10000-0xDB440) and within the 2MB PSX RAM.
 */

OUTPUT_FORMAT("elf32-littlemips")
OUTPUT_ARCH(mips)
ENTRY(_start)

/* Two loadable segments, text (R E) and data (RW).  With the single
 * default RWE segment GNU ld emits, elf2ecoff folds everything into
 * the text chunk and zeroes dsize - the ECOFF kernel loader then
 * never copies .data, and every initialized static (minigl's
 * material and colour defaults, glGenTextures' id counter, gpu_fd)
 * ran as whatever zeros the boot memory held: lit geometry rendered
 * black, the first texture got the invalid id 0. */
PHDRS
{
    text PT_LOAD FLAGS(5);   /* R E */
    data PT_LOAD FLAGS(6);   /* RW  */
}

SECTIONS
{
    . = 0x00100000;

    .text : {
        *(.text.start)
        *(.text)
        *(.text.*)
    } :text

    .rodata : {
        *(.rodata)
        *(.rodata.*)
        *(.rdata)
    } :text

    .data : {
        *(.data)
        *(.data.*)
        *(.sdata)
    } :data

    _gp = . + 0x7FF0;

    .bss : {
        *(.bss)
        *(.bss.*)
        *(.sbss)
        *(COMMON)
    } :data

    /DISCARD/ : {
        *(.MIPS.abiflags)
        *(.mdebug*)
        *(.comment)
        *(.note*)
    }
}
