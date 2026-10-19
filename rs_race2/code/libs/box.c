// ***********************************************************************************
// Programs written by R.Swan - rs108@mdx.ac.uk - www.netyaroze-europe.com/~middex2
// Sprite functions
// ***********************************************************************************

#include "libps_bridge.h"
#include "screen.h"
#include "box.h"

// ***********************************************************************************
// Function definitions
// ***********************************************************************************

// **** Draw a sprite on screen
void DrawColouredBox( long tX1, long tY1, long tX2, long tY2,
                      long tAttribute, u_long tPriority,
                      long tR1, long tG1, long tB1,
                      long tR2, long tG2, long tB2,
                      long tR3, long tG3, long tB3,
                      long tR4, long tG4, long tB4)
  {
  long tTemp1;
  long tLeftColorR = tR1<<20, tLeftColorG = tG1<<20, tLeftColorB = tB1<<20;
  long tRightColorR = tR2<<20, tRightColorG= tG2<<20, tRightColorB = tB2<<20;
  long tLeftAddR, tLeftAddG, tLeftAddB;
  long tRightAddR, tRightAddG, tRightAddB;
  float tTempWidth;

  if ((tAttribute>-1) && (tAttribute<4))
    {
    tempLine.attribute = (1<<30) + (tAttribute<<28);
    }
  else tempLine.attribute = 0;

  tempLine.x0 = tX1;
  tempLine.x1 = tX2;
  tempLine.y0 = tY1;
  tempLine.y1 = tY1;

  tTempWidth = (tY2 - tY1);
  tLeftAddR = (((tR1 - tR3) << 20) / tTempWidth) -1;
  tLeftAddG = (((tG1 - tG3) << 20) / tTempWidth) -1;
  tLeftAddB = (((tB1 - tB3) << 20) / tTempWidth) -1;
  tRightAddR = (((tR2- tR4) << 20) / tTempWidth) -1;
  tRightAddG = (((tG2 - tG4) << 20) / tTempWidth) -1;
  tRightAddB = (((tB2 - tB4) << 20) / tTempWidth) -1;

  for (tTemp1 = tY1; tTemp1<tY2; tTemp1++)
    {
    tLeftColorR -= tLeftAddR;
    tLeftColorG -= tLeftAddG;
    tLeftColorB -= tLeftAddB;

    tRightColorR -= tRightAddR;
    tRightColorG -= tRightAddG;
    tRightColorB -= tRightAddB;

    tempLine.r0 = (short) (tLeftColorR>>20);
    tempLine.g0 = (short) (tLeftColorG>>20);
    tempLine.b0 = (short) (tLeftColorB>>20);
    tempLine.r1 = (short) (tRightColorR>>20);
    tempLine.g1 = (short) (tRightColorG>>20);
    tempLine.b1 = (short) (tRightColorB>>20);

    GsSortGLine(&tempLine, &OTable_Header[CurrentBuffer], tPriority);
    tempLine.y0++;
    tempLine.y1++;
    }
  }
