# psxdev/Makefile - build the PSXLinux userspace game demos.
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

TARGETS  = demo.bflt gfxdemo.bflt cargame.bflt

all: $(TARGETS)

demo.bflt: demo.c
	$(CC) $(CFLAGS) $(INC) $(LDFLAGS) -o demo.elf demo.c
	$(E2E) demo.elf demo.bflt

gfxdemo.bflt: gfxdemo.c psxsys.h psxdev.h psxdev.ld
	$(CC) $(CFLAGS) $(INC) $(LDFLAGS) -o gfxdemo.elf gfxdemo.c
	$(E2E) gfxdemo.elf gfxdemo.bflt

cargame.bflt: cargame.c psxsys.h psxdev.h psxdev.ld
	$(CC) $(CFLAGS) $(INC) $(LDFLAGS) -o cargame.elf cargame.c
	$(E2E) cargame.elf cargame.bflt

clean:
	rm -f *.elf *.bflt *.o *.a demo gfxdemo cargame
