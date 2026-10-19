// **** Global #defines
// The models and textures live on the boot CD as separate files (see
// LoadGameAssets() in main.c); these names point at them once loaded.
extern u_long  *TMD_SHIP_TOP;            // ship.tmd, at its ID word
extern u_long  *TMD_ARENA;               // scratch for the dynamic TMDs
extern u_long   TIM_NUMBERS_TOP;
extern u_long   TIM_BACK_TOP;
extern u_long   TIM_TITLE_TOP;

// The game reads TIMs through GsGetTimInfo() after stepping over the
// two-word block header, so the addresses it passes sit one word before
// the TIM ID word.
#define		TMD_SHIP					(TMD_SHIP_TOP)
#define		TIM_NUMBERS				(TIM_NUMBERS_TOP)
#define		TIM_BACK				(TIM_BACK_TOP)
#define 	TIM_TITLE       (TIM_TITLE_TOP)
#define   TMD_DYNAMIC_START       ((u_long) TMD_ARENA)
#define   DRAWUPTO          (484)

u_long    *TMD_START_TRK[100];                        // stores memery address of
									// all dynamically created TMDs
u_long    *TMD_START_OBJ[100];
u_long    *TMD_YOURCAR;

typedef struct
	{
	Model   model;
	u_long  attribute;
	} TRACK_ELEMENT;

TRACK_ELEMENT TRACK_ARRAY[DRAWUPTO];

// **** Global variables and structures

// **** Function prototypes
void UpdateWorld();
void RenderWorld();
void ProcessJoyPad();
void MoveOverStartLine();
u_long CheckForCollision(long	tDisplacement, long tPosition);

