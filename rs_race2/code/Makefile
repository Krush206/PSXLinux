# rs_race2 port to PSXLinux
#
# Requires mipsel-linux-gcc (in PATH)

CC       = mipsel-linux-gcc
CFLAGS   = -nostdlib -static -G0 -O2 -Wall -fno-pic -mno-abicalls \
	   -mcpu=r3000 -mips1 -msoft-float -fomit-frame-pointer -DNO_MM
INC      = -I. -Ilibs -I../../psxdev
LDFLAGS  = -Wl,-T,../../psxdev/psxdev.ld -e _start

E2E      = /opt/PSXLinux/arch/mipsnommu/boot/elf2ecoff

LIBOBJS  = ../../psxdev/libps_compat.o ../../psxdev/psxlib.o \
	   ../../psxdev/minilibc.o ../../psxdev/libsoftfp.a

OBJS     = rsrace_entry.o main.o 1_track.o 1_roadob.o 1_map.o counter.o \
	   libs/screen.o libs/pad.o libs/sprite.o libs/box.o \
	   libs/3d.o libs/tmd.o

all: rsrace.bflt

rsrace.bflt: $(OBJS) $(LIBOBJS)
	$(CC) $(CFLAGS) $(INC) $(LDFLAGS) -o rsrace.elf $(OBJS) $(LIBOBJS)
	$(E2E) rsrace.elf rsrace.bflt

../../psxdev/libps_compat.o: ../../psxdev/libps_compat.c ../../psxdev/libps_compat.h
	$(CC) $(CFLAGS) -I../../psxdev -c -o $@ $<

../../psxdev/psxlib.o: ../../psxdev/psxlib.c ../../psxdev/psxlib.h
	$(CC) $(CFLAGS) -I../../psxdev -c -o $@ $<

../../psxdev/minilibc.o: ../../psxdev/minilibc.c ../../psxdev/minilibc.h
	$(CC) $(CFLAGS) -I../../psxdev -c -o $@ $<

main.o: main.c
	$(CC) $(CFLAGS) -O0 $(INC) -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) $(INC) -c -o $@ $<

libs/%.o: libs/%.c
	$(CC) $(CFLAGS) $(INC) -c -o $@ $<

clean:
	rm -f *.o *.elf *.bflt libs/*.o
