# GLTron 0.53 port to PSXLinux
#
# Requires mipsel-linux-gcc (in PATH).  Everything is compiled with
# -msoft-float and linked against libsoftfp.a (the R3000A has no
# FPU); minigl/miniglut are the GL/GLUT implementation.

CC       = mipsel-linux-gcc
CFLAGS   = -nostdlib -static -G0 -O2 -Wall -fno-pic -mno-abicalls \
	   -mcpu=r3000 -mips1 -msoft-float -fomit-frame-pointer -DNO_MM
INC      = -I. -I../../psxdev
LDFLAGS  = -Wl,-T,../../psxdev/psxdev.ld -e _start

E2E      = /opt/PSXLinux/arch/mipsnommu/boot/elf2ecoff

LIBOBJS  = ../../psxdev/minigl.o ../../psxdev/miniglut.o \
	   ../../psxdev/minilibc.o ../../psxdev/libsoftfp.a

OBJS     = gltron_entry.o gltron.o engine.o computer.o gamegraphics.o \
	   graphics.o modelgraphics.o model.o fonttex.o fonts.o \
	   texture.o input.o pause.o switchCallbacks.o geom.o \
	   settings.o file.o sound.o \
	   ../psx_model_data.o ../psx_tex_data.o

all: gltron.bflt

gltron.bflt: $(OBJS) $(LIBOBJS)
	$(CC) $(CFLAGS) $(INC) $(LDFLAGS) -o gltron.elf $(OBJS) $(LIBOBJS)
	$(E2E) gltron.elf gltron.bflt

../../psxdev/minigl.o: ../../psxdev/minigl.c ../../psxdev/minigl.h
	$(CC) $(CFLAGS) -I../../psxdev -c -o $@ ../../psxdev/minigl.c

../../psxdev/miniglut.o: ../../psxdev/miniglut.c ../../psxdev/miniglut.h \
			   ../../psxdev/minigl.h
	$(CC) $(CFLAGS) -I../../psxdev -c -o $@ ../../psxdev/miniglut.c

../../psxdev/minilibc.o: ../../psxdev/minilibc.c ../../psxdev/minilibc.h
	$(CC) $(CFLAGS) -I../../psxdev -c -o $@ ../../psxdev/minilibc.c

../psx_model_data.o: ../psx_model_data.c psxdata.h model.h
	$(CC) $(CFLAGS) $(INC) -c -o $@ ../psx_model_data.c

../psx_tex_data.o: ../psx_tex_data.c psxdata.h
	$(CC) $(CFLAGS) $(INC) -c -o $@ ../psx_tex_data.c

%.o: %.c
	$(CC) $(CFLAGS) $(INC) -c -o $@ $<

clean:
	rm -f *.o gltron.elf gltron.bflt ../psx_model_data.o ../psx_tex_data.o

.PHONY: all clean
