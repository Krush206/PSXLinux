// ***********************************************************************************
// Programs written by R.Swan - rs108@mdx.ac.uk - www.netyaroze-europe.com/~middex2
// General Screen header file
// ***********************************************************************************

#include "libps_bridge.h"

// **** Global defines
#define     OTABLE_LENGTH       (12)
#define     BACK                ((1<<OTABLE_LENGTH)-1)

#define     RenderPrepare() \
	    CurrentBuffer = GsGetActiveBuff(), \
	    GsClearOt(0, 0, &OTable_Header[CurrentBuffer]), \
	    GsSetWorkBase((PACKET*)Packet_Memory[CurrentBuffer])

#define     RenderFinish() \
	    DrawSync(0), \
		VerticalSync = VSync(0), \
	    GsSwapDispBuff(), \
	    GsDrawOt(&OTable_Header[CurrentBuffer]), \
	    FntFlush(FntOne)

// **** Global variables
u_long      CurrentBuffer;
u_long      VerticalSync;

u_char      SCREENR, SCREENG, SCREENB;

GsOT        OTable_Header[2];
GsOT_TAG    OTable_Array[2][1<<OTABLE_LENGTH];

/*
 * Packet area token.  The real packet arena lives in libps_compat
 * (GsSetWorkBase resets it each frame), so the game only needs a
 * word-aligned object to pass; nothing is ever stored here.
 */
PACKET      Packet_Memory[2][1];

long        FntOne;

//static GsCOORDINATE2 trans;
short                   ProjectionDistance;

// ***********************************************************************************
// Function prototypes
// ***********************************************************************************

// **** Setup up screen
void InitialiseScreen();

// **** Setup up screen for 3d graphics
void InitialiseScreen3D();
