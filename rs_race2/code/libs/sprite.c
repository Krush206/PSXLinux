// ***********************************************************************************
// Programs written by R.Swan - rs108@mdx.ac.uk - www.netyaroze-europe.com/~middex2
// Sprite functions
// ***********************************************************************************

#include "libps_bridge.h"
#include "screen.h"
#include "sprite.h"

// ***********************************************************************************
// Function definitions
// ***********************************************************************************

// **** This function transfers a .TIM form main memory to video memory
void LoadTIMData(u_long tMemAddress)
  {
  RECT tRect;                                         // rectangular area to be used for TIM
  GsIMAGE tTim;                                       // TIM image information
  tMemAddress += 4;                                   // advance memory pointer to data
  GsGetTimInfo((u_long *) tMemAddress, &tTim);        // fill tTim with info from TIM
  tRect.x = tTim.px;                                  // tell GPU where graphics data is
  tRect.y = tTim.py;
  tRect.w = tTim.pw;
  tRect.h = tTim.ph;
  LoadImage(&tRect, tTim.pixel);                      // load video memory with TIM data
    tRect.x = tTim.cx;                                // tell GPU where colour table is
    tRect.y = tTim.cy;
    tRect.w = tTim.cw;
    tRect.h = tTim.ch;
    LoadImage(&tRect, tTim.clut);                     // load video memory with CLUT data
  DrawSync(0);                                        // force LoadImages to complete
  }

// **** This sets up sprite information, such as x,y, what .TIM data to use etc. etc.
void SetSpriteInfo(GsSPRITE *tSprite, u_long tMemAddress, long tX, long tY)
  {
  u_long tTexturePageX, tTexturePageY;                // four variables to clear up
  u_long tTexturePageXOffset, tTexturePageYOffset;    // assignment of texture pages
  GsIMAGE tTim;                                       // TIM image information
  u_long tCLUT;

  tMemAddress += 4;                                   // advance memory pointer to data
  GsGetTimInfo((u_long *) tMemAddress, &tTim);        // fill tTim with info from TIM

  tSprite->x = tX;                                    // set sprites screen coordinates
  tSprite->y = tY;                                    // based on values passed
  if (tTim.pmode& 0x01)
    {
    tSprite->attribute = (1<<24);                     // set up for 256 colour sprite
    tSprite->w = tTim.pw*2;
    tCLUT = 1;
    }
  else
    {
    tSprite->attribute = 0;                           // set up for 16 colour sprite
    tSprite->w = tTim.pw*4;
    tCLUT = 0;
    };
  tSprite->h = tTim.ph;
    tTexturePageX = (tTim.px/64) *64;                 // calculate texture page
    tTexturePageY = (tTim.py/256) *256;
    tTexturePageXOffset = tTim.px % 64;
    tTexturePageYOffset = tTim.py % 256;
  tSprite->tpage = GetTPage(tCLUT, 0, tTim.px, tTim.py);
                                                      // calculate correct texture page
  tSprite->u = tTexturePageXOffset*4;                 // offset for sprite from left of tpage
  tSprite->v = tTexturePageYOffset;                   // offset for sprite from top of tpage
  tSprite->cx = tTim.cx;                              // set sprite to use correct CLUT
  tSprite->cy = tTim.cy;

  tSprite->r = tSprite->g = tSprite->b = 128;         // make sure the sprite is set to
                                                      // normal colour intensity
  tSprite->mx = tSprite->w/2;                         // set handle of sprite to the center
  tSprite->my = tSprite->h/2;                         // (this does not need to be done if
                                                      // you would rather move the objects
                                                      // by their top left corner)
  tSprite->scalex = tSprite->scaley = ONE;            // make sure the sprite isn't scaled
  tSprite->rotate = 0;                                // make sure its pointing up
  }

