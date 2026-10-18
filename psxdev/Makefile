# psxdev/Makefile - build the PSXLinux userspace game demo.
#
# The demo uses raw syscalls (no libc); the kernel provides
# /dev/psxgpu (major 210) and /dev/psxspu (major 211).

CC       = mipsel-linux-gcc
CFLAGS   = -nostdlib -static -G0 -O2 -Wall -fno-pic -mno-abicalls \
	   -mcpu=r3000 -mips1 -fomit-frame-pointer -DNO_MM
INC      = -I../include -I.

all: demo

demo: demo.c psxsys.h libpsxdev.h ../include/linux/psxgpu.h ../include/linux/psxspu.h
	$(CC) $(CFLAGS) $(INC) -o $@ demo.c

clean:
	rm -f demo *.o *.a

# Convert to PS-X EXE for standalone testing:
#   mipsel-linux-objcopy -O binary demo demo.bin
#   (or use elf2ecoff + eco2exe)
