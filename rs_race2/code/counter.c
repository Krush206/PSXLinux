// ***********************************************************************************
// Programs written by R.Swan - rs108@mdx.ac.uk - www.netyaroze-europe.com/~middex2
// STANDARD CODE for handling sprites and 3d
// ***********************************************************************************

// ***********************************************************************************
// Preprocessor functions
// ***********************************************************************************

// **** include files
#include "header.h"                                      // include joypad handling include

// ***********************************************************************************
// Main function
// ***********************************************************************************

void InitialiseCounter(u_char tStyle)
  {
  long tTemp1, tTemp2, tTemp3, tTemp4;

  for (tTemp1=0; tTemp1<10; tTemp1++)
    {
    if (tTemp1==0)
      {
      tTemp2 = 16; tTemp3 = 14; tTemp4 = 90;
      }
    else
      {
      tTemp2 = 9;
      tTemp3 = (tTemp1-1)*12+14;
      tTemp4 = 310-8*8-16+100;
      };
    if (tStyle == 0)
    	{
	    SetSpriteInfo(&Numbers[tTemp1][0], TIM_NUMBERS, tTemp4-155, tTemp3-115);
	    SetSpriteInfo(&Numbers[tTemp1][1], TIM_NUMBERS, tTemp2*2 +tTemp4-155, tTemp3-115);
	    SetSpriteInfo(&Numbers[tTemp1][2], TIM_NUMBERS, tTemp2*3 +tTemp4-155, tTemp3-115);
	    SetSpriteInfo(&Numbers[tTemp1][3], TIM_NUMBERS, tTemp2*5 +tTemp4-155, tTemp3-115);
	    SetSpriteInfo(&Numbers[tTemp1][4], TIM_NUMBERS, tTemp2*6 +tTemp4-155, tTemp3-115);
	    SetSpriteInfo(&Numbers[tTemp1][5], TIM_NUMBERS, tTemp2*1 +tTemp4-155, tTemp3-115);
	    SetSpriteInfo(&Numbers[tTemp1][6], TIM_NUMBERS, tTemp2*4 +tTemp4-155, tTemp3-115);
	    };
    };

  for (tTemp1=0; tTemp1<10; tTemp1++)
    {
    switch(tTemp1)
      {
      case 0:   {tTemp3 = 18; tTemp4 = 0; break;}
      case 1:   {tTemp3 = 10; tTemp4 = 18; break;}
      }
    for (tTemp2=0; tTemp2<7; tTemp2++)
      {
      Numbers[tTemp1][tTemp2].attribute = (1<<30)+(1<<28);
      if (tTemp1 != 0) Numbers[tTemp1][tTemp2].attribute += (1<<31);
      Numbers[tTemp1][tTemp2].u = 0;
      Numbers[tTemp1][tTemp2].w = 16;
      Numbers[tTemp1][tTemp2].h = tTemp3;
      Numbers[tTemp1][tTemp2].v = tTemp4;
      if (tTemp1>0) Numbers[tTemp1][tTemp2].r = Numbers[tTemp1][tTemp2].g = (Numbers[tTemp1][tTemp2].b = 63);
      if (tTemp1==10) Numbers[tTemp1][tTemp2].g = 128;
      if (tTemp1==11) Numbers[tTemp1][tTemp2].r = 128;
      }
    Numbers[tTemp1][5].u = Numbers[tTemp1][6].u = 160;
    }
	CounterDigits[0][0] = 
	CounterDigits[0][1] = 
	CounterDigits[0][2] = 
	CounterDigits[0][3] = 
	CounterDigits[0][4] = 0;
  };
