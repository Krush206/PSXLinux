// ***********************************************************************************
// Programs written by R.Swan - rs108@mdx.ac.uk - www.netyaroze-europe.com/~middex2
// Sprite header file
// ***********************************************************************************

// **** Global defines
#define   DrawSprite(p,_t) \
        GsSortSprite((p), &OTable_Header[CurrentBuffer], (_t))


// **** Global variables

// ***********************************************************************************
// Function prototypes
// ***********************************************************************************

// **** This function transfers a .TIM form main memory to video memory
void LoadTIMData(u_long tMemAddress);

// **** This sets up sprite information, such as x,y, what .TIM data to use etc. etc.
void SetSpriteInfo(GsSPRITE *tSprite, u_long tMemAddress, long tX, long tY);

// **** Draw a sprite on screen
//void DrawSprite(GsSPRITE *tSprite, u_long tPriority);
