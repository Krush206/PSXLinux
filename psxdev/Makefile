# psxdev/Makefile - build the PSXLinux userspace game library and demos.
#
# The demos use raw syscalls (no libc); the kernel provides
# /dev/psxgpu (major 210) and /dev/psxspu (major 211).
#
# The binaries are ECOFF executables loaded by the kernel's
# CONFIG_BINFMT_ECOFF handler.  Link them at 0x00100000 (above
# the kernel, within the 2MB PSX RAM) via psxdev.ld.

CC       = mipsel-linux-gcc
CFLAGS   = -nostdlib -static -G0 -O2 -Wall -fno-pic -mno-abicalls \
	   -mcpu=r3000 -mips1 -fomit-frame-pointer -DNO_MM
INC      = -I../include -I.
LDFLAGS  = -Wl,-T,psxdev.ld -e _start

E2E      = /opt/PSXLinux/arch/mipsnommu/boot/elf2ecoff

all: demo.bflt gfxdemo.bflt

demo.bflt: demo.c psxsys.h psxdev.h psxdev.ld
	$(CC) $(CFLAGS) $(INC) $(LDFLAGS) -o demo.elf demo.c
	$(E2E) demo.elf $@

gfxdemo.bflt: gfxdemo.c psxsys.h psxdev.h psxdev.ld
	$(CC) $(CFLAGS) $(INC) $(LDFLAGS) -o gfxdemo.elf gfxdemo.c
	$(E2E) gfxdemo.elf $@

clean:
	rm -f *.elf *.bflt *.o *.a demo gfxdemo
