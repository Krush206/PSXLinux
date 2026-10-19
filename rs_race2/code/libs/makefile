CC = psx-gcc
CFLAGS	= -Wall
LINKER  = -Xlinker -Ttext -Xlinker 80140000

PROG	=	main
OBJS    =    sprite.o screen.o pad.o box.o 3d.o tmd.o

all: $(PROG)

main    : $(OBJS)
	$(CC) $(LINKER) $(OBJS) -o $@
	psx-strip $@

.c.o:
	$(CC) $(CFLAGS) -funsigned-char -c $<

clean:
	$(RM) $(PROG)
	$(RM) *.o

