// ***********************************************************************************
// Programs written by R.Swan - rs108@mdx.ac.uk - www.netyaroze-europe.com/~middex2
// General Screen functions
// ***********************************************************************************

#include "screen.h"

// ***********************************************************************************
// Function definitions
// ***********************************************************************************

// **** Setup up screen
void InitialiseScreen()
  {
  SetVideoMode(MODE_PAL);                             // define screen as PAL
  GsInitGraph(320, 240, GsOFSGPU, 0, 0);              // define screen resolution, whether
                                                      // interlaced, dithered & colour depth
  GsDefDispBuff(0, 0, 0, 240);                        // define top left corners of
                                                      // both screen buffers in memory
  OTable_Header[0].length = OTABLE_LENGTH;            // notify OTable headers of OTable
  OTable_Header[1].length = OTABLE_LENGTH;            // array lengths
  OTable_Header[0].org = OTable_Array[0];             // notify OTable headers of OTable
  OTable_Header[1].org = OTable_Array[1];             // array memory locations
  GsClearOt(0, 0, &OTable_Header[0]);                 // clear out OTable sorting info
  GsClearOt(0, 0, &OTable_Header[1]);                 // ready for use (not strictly necessary)

  FntLoad(960, 256);                                  // load the standard font into memory
  FntOne = FntOpen(0, 0, 320, 100, 0, 256);           // give display window info for font
  }

// **** Setup up screen for 3d graphics
void InitialiseScreen3D()
  {
  InitialiseScreen();
  GsInit3D();                                         // make sure 3d graphics will work
  FntOne = FntOpen(-160, -120, 320, 100, 0, 256);
  }
