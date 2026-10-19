// ***********************************************************************************
// C file to read PAD buttons
// Set of pad routines to present a similar interface to the controller
// pad as dtlh2500 (?)
// A smaller version of <PAD.C> Copyright (c) (1996) Sony Computer Entertainment Inc.
// ***********************************************************************************

// **** To use -
// 1)   call PadInit() during initialisation in MAIN function
// 2)   each frame, call PadRead() and assign return value to an u_long variable, ie
//      u_long PAD;                                   // as global variable
//      PAD = PadRead();                              // in main function
// 3)   Test which buttons are pressed by ANDing the returned variable with the appropriate
//      constant #defined in <PAD.H>, ie
//      if (PAD & PADtriangle)                        // if the triangle button is pressed
//        {
//        // do actions associated with pressing the triangle button here
//        };

#include "libps_bridge.h"                                    // include PSX library
#include "pad.h"                                      // include #defines and prototypes

volatile u_char *bb0, *bb1;                           // pad buffers- never need to touch

// **** Call once at the start of main function
void InitialiseJoyPad(void)
  {
  GetPadBuf(&bb0, &bb1);
  }

// **** Call once every frame to read state of controller
u_long ReadJoyPad(void)
  {
  return(~(*(bb0+3) | *(bb0+2) << 8 | *(bb1+3) << 16 | *(bb1+2) << 24));
  }
