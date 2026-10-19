// ***********************************************************************************
// Header file to describe PAD buttons
// An amalgamation of <PAD.H> Copyright (c) (1996) Sony Computer Entertainment Inc.
// and additions by Sean Butler (14th July 1996)
// ***********************************************************************************

// **** function  prototypes for joypad reading\
void    InitialiseJoyPad(void);
u_long  ReadJoyPad(void);

// **** definitions for easy access to user input
#define PADselect   (1<< 8)                           // select pressed
#define PADstart    (1<<11)                           // start pressed

#define PADup       (1<<12)                           // joypad moved up
#define PADdown     (1<<14)                           // joypad moved down
#define PADleft     (1<<15)                           // joypad moved left
#define PADright    (1<<13)                           // joypad moved right

#define PADtriangle (1<< 4)                           // triangle pressed
#define PADcross    (1<< 6)                           // cross pressed
#define PADsquare   (1<< 7)                           // square pressed
#define PADcircle   (1<< 5)                           // circle pressed

#define PADL1       (1<< 2)                           // top left rear button pressed
#define PADR1       (1<< 3)                           // top right rear button pressed
#define PADL2       (1<< 0)                           // bottom left rear button pressed
#define PADR2       (1<< 1)                           // bottom right rear button pressed

#define PADi        (1<< 9)                           // not sure!
#define PADj        (1<<10)                           // not sure!
