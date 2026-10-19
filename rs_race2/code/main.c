// ***********************************************************************************
// Programs written by R.Swan - rs108@mdx.ac.uk - www.netyaroze-europe.com/~middex2
// MAIN routine
// ***********************************************************************************

#include "header.h"
#include "../../psxdev/psxlib.h"
extern int printf(const char *fmt, ...);
//#include "scrngrab.h"

Model myOBJ;
Model enemyOBJ[1];
Model TunnelThing;

u_char		IsTitle = true;
u_char 		crossPressed = false;

u_char    GameRunning = true;
//u_long    Tmd_Selected = 0;
//u_long    JustMoved = 0;
u_char    COLLIDE = false;

u_short   DRAWFROM = 10;
u_char    OVERHEAD;

u_char 		PlayerStartedRacing = false;

long                    health = 1000<<7;
u_long				          boost = 1000;
long                    velocity = 0;
long                    momentumX = 0, momentumZ = 0;
long                    playerX, playerZ;
long                    viewFromOrientation = 0;
u_char		LapsDone = 0;

GsBOXF		Box1;
GsBOXF		Wipe;
RECT			Background;
RECT			Title;

// ***********************************************************************************
// MAIN FUNCTION
// ***********************************************************************************

// ***********************************************************************************
// ASSET LOADING
// ***********************************************************************************

// **** Assets read off the boot CD (see LoadGameAssets below)
u_long  *TMD_SHIP_TOP;                        // ship.tmd, at its ID word
u_long  *TMD_ARENA;                           // scratch for the dynamic TMDs
u_long   TIM_NUMBERS_TOP;
u_long   TIM_BACK_TOP;
u_long   TIM_TITLE_TOP;

extern void *malloc(unsigned int tSize);
extern void  free(void *tPtr);

#define TMD_ARENA_BYTES (128 * 1024)

// **** Read a whole file off the CD, returning the malloc()'d base of the
//      buffer.  `tPad` bytes of padding are left in front of the data so
//      the game's "+4 to step over the TIM magic" convention still leaves
//      the base free()able.
static void *LoadFile(const char *tName, long tPad)
  {
  sint tFd, tSize, tTot, tGot;
  u_char *tBuf;

  // Hardcoded sizes since lseek/fstat on ISO9660 are unreliable
  if (strcmp(tName, "/numbers.tim") == 0) tSize = 2528;
  else if (strcmp(tName, "/title.tim") == 0) tSize = 153620;
  else if (strcmp(tName, "/back.tim") == 0) tSize = 65556;
  else if (strcmp(tName, "/ship.tmd") == 0) tSize = 5224;
  else tSize = 0;

  tFd = psx_cd_open(tName);
  if (tFd < 0)
    {
    printf("rsrace: cannot open %s\n", tName);
    return 0;
    }

  tBuf = (u_char *) malloc((unsigned int) tSize + (unsigned int) tPad);
  if (tBuf == 0)
    {
    printf("rsrace: out of memory for %s (%d bytes)\n", tName, tSize);
    return 0;
    }

  tTot = 0;
  while (tTot < tSize)
    {
    tGot = psx_cd_read(tFd, (char *) tBuf + tPad + tTot, tSize - tTot);
    if (tGot <= 0) break;
    tTot += tGot;
    }

  return tBuf;
  }

// **** Load everything the game needs off the boot CD
static void LoadGameAssets()
  {
  u_char *tBuf;

  // numbers.tim stays resident: SetSpriteInfo() re-reads it every time
  // the counter is initialised.
  tBuf = (u_char *) LoadFile("/numbers.tim", 4);
  if (tBuf != 0)
    {
    TIM_NUMBERS_TOP = (u_long) (tBuf + 4);
    LoadTIMData(TIM_NUMBERS_TOP);
    }

  // The title and background images are only needed to fill VRAM with,
  // so their buffers go straight back to the allocator.
  tBuf = (u_char *) LoadFile("/title.tim", 4);
  TIM_TITLE_TOP = (u_long) (tBuf + 4);
  if (tBuf != 0)
    {
    LoadTIMData(TIM_TITLE_TOP);
    free(tBuf);
    }

  tBuf = (u_char *) LoadFile("/back.tim", 4);
  TIM_BACK_TOP = (u_long) (tBuf + 4);
  if (tBuf != 0)
    {
    LoadTIMData(TIM_BACK_TOP);
    free(tBuf);
    }

  TMD_SHIP_TOP = (u_long *) LoadFile("/ship.tmd", 0);
  // Skip close

  // Scratch memory for the track models that create_Track() builds.
  TMD_ARENA = (u_long *) malloc(TMD_ARENA_BYTES);
  if (TMD_ARENA == 0)
    printf("rsrace: no room for the track models\n");
  }

// ***********************************************************************************
// MAIN routine
// ***********************************************************************************

sint GameMain(void)
	{
	SCREENR = 123;
	SCREENG = 115;
	SCREENB = 74;

	OVERHEAD = 0;

	Box1.x = -152;
	Box1.y = -93;
	Box1.w = 125;
	Box1.h = 13;
	Box1.attribute += (1<<30);
	Box1.g = Box1.b = (Box1.r = 64) >>1 ;

	Background.x = 320;
	Background.y = 256;
	Background.w = 320;
	Background.h = 64;

	Title.x = 704;
	Title.y = 0;
	Title.w = 320;
	Title.h = 240;

	Wipe.x = -160;
	Wipe.y = -120+64;
	Wipe.w = 320;
	Wipe.h = 240-64;
	Wipe.r = SCREENR;
	Wipe.g = SCREENG;
	Wipe.b = SCREENB;

	InitialiseScreen3D();	InitialiseJoyPad();
	LoadGameAssets();
	InitialiseCounter(0);

	create_Track();
	create_YourCar();
	create_RoadsideObjects();

	SetModelInfo(&myOBJ,  TRACK_ARRAY[10].model.Object_Coord.coord.t[0],
											0,
											TRACK_ARRAY[10].model.Object_Coord.coord.t[2],
											TMD_SHIP);
	playerX = TRACK_ARRAY[10].model.Object_Coord.coord.t[0]<<8;
	playerZ = TRACK_ARRAY[10].model.Object_Coord.coord.t[2]<<8;

	SetModelInfo(&TunnelThing,  TRACK_ARRAY[450].model.Object_Coord.coord.t[0],
				TRACK_ARRAY[450].model.Object_Coord.coord.t[1],
				TRACK_ARRAY[450].model.Object_Coord.coord.t[2],
				TMD_START_OBJ[0]);
	RotateModel(&TunnelThing,       TRACK_ARRAY[450].model.Orientation.vx,
				TRACK_ARRAY[450].model.Orientation.vy,
				TRACK_ARRAY[450].model.Orientation.vz);

	SetLightDirectional(0, 1, 1, 0, 255, 255, 255);
	SetLightAmbient(512,512,512);
	TurnFoggingOff();
	SetViewPoint(250, 0, 0, -300, -1000, 0, -135, 0, &myOBJ.Object_Coord);

	while (GameRunning == true)
		{
		ProcessJoyPad();

		UpdateWorld();
		RenderWorld();
		}

  ResetGraph(3);
  return 0;
  }

// ***********************************************************************************
// Function Definitions
// ***********************************************************************************

// **** Update everything in the world before drawing
void UpdateWorld()
	{
  u_long tTemp1, tTemp2 = false;
  long tDistance1, tDistance2;
  long tTempX, tTempZ, tTemp3;
  SVECTOR tInputMatrix;
	MATRIX tMatrix;
	VECTOR tPosition, tDirectionResult;
  u_char tCollision;

// **** Update the counter
	if (velocity > 0) PlayerStartedRacing = true;

	if (PlayerStartedRacing==true)
		{
		CounterDigits[0][4] += 2;
//    TimeCurrent++;
		}
	for (tTemp1 = 4; tTemp1 != 0; tTemp1--)
    {
		if (CounterDigits[0][tTemp1] > 9)
      {
      CounterDigits[0][tTemp1] = CounterDigits[0][tTemp1] % 10;
      CounterDigits[0][tTemp1-1]++;
    }
   if (CounterDigits[0][1] > 5)
    {
    CounterDigits[0][1] = CounterDigits[0][1] % 6;
    CounterDigits[0][0]++;
    }
   }
	if (CounterDigits[0][0] == 10) CounterDigits[0][0] = 0;

  for (tTemp1 = 0; tTemp1 < 5; tTemp1++)
   {
   Numbers[0][tTemp1].u = CounterDigits[0][tTemp1]*16;
	 }

// **** move the camera
	tInputMatrix.vx = tInputMatrix.vz = 0;
	tInputMatrix.vy = viewFromOrientation;
	RotMatrix(&tInputMatrix, &tMatrix);
	tPosition.vx = tPosition.vy = 0;
	tPosition.vz = -750;
	ApplyMatrixLV(&tMatrix, &tPosition, &tDirectionResult);

	if (OVERHEAD == false)
		{
//		ViewPoint.rz = -viewFromOrientation * 100;
	  ViewPoint.vpx = tDirectionResult.vx;
	  ViewPoint.vpy = -300;
	  ViewPoint.vpz = tDirectionResult.vz;
	  }
	else
		{
	  ViewPoint.vpx = 0;
	  ViewPoint.vpy = -50000;
	  ViewPoint.vpz = 0;
		}

	GsSetRefView2(&ViewPoint);

	viewFromOrientation *= 0.92;

// **** momentum calculations + movement
	velocity *= 0.962;
	momentumX *= 0.97; momentumZ *= 0.97;	
	tInputMatrix.vy = myOBJ.Orientation.vy;
	RotMatrix(&tInputMatrix, &tMatrix);
	tPosition.vx = 0;
	tPosition.vz = velocity;
	ApplyMatrixLV(&tMatrix, &tPosition, &tDirectionResult);
	momentumX += tDirectionResult.vx;
	momentumZ += tDirectionResult.vz;
	tTempX = (playerX + momentumX)>>8;
	tTempZ = (playerZ + momentumZ)>>8;

// **** collision detection with the walls
  tInputMatrix.vy = -TRACK_ARRAY[DRAWFROM].model.Orientation.vy;
  RotMatrix(&tInputMatrix, &tMatrix);
	tPosition.vx =  tTempX - 
									TRACK_ARRAY[DRAWFROM].model.Object_Coord.coord.t[0];
	tPosition.vz =  tTempZ - 
									TRACK_ARRAY[DRAWFROM].model.Object_Coord.coord.t[2];
  ApplyMatrixLV(&tMatrix, &tPosition, &tDirectionResult);
	tTemp3 = tDirectionResult.vx;

// **** collision detection with the walls on section of track currently on
  tInputMatrix.vy = -TRACK_ARRAY[DRAWFROM].model.Orientation.vy;
  RotMatrix(&tInputMatrix, &tMatrix);
	tPosition.vx = 	tTempX - 
									TRACK_ARRAY[DRAWFROM].model.Object_Coord.coord.t[0];
	tPosition.vz = 	tTempZ -
									TRACK_ARRAY[DRAWFROM].model.Object_Coord.coord.t[2];
	ApplyMatrixLV(&tMatrix, &tPosition, &tDirectionResult);
	tTemp3 = tDirectionResult.vx;

	tCollision = CheckForCollision(0, tTemp3);
	if (tCollision != false)
		{
		tInputMatrix.vy = -TRACK_ARRAY[DRAWFROM].model.Orientation.vy;
		RotMatrix(&tInputMatrix, &tMatrix);
		tPosition.vx = momentumX;
		tPosition.vz = momentumZ;
		ApplyMatrixLV(&tMatrix, &tPosition, &tDirectionResult);
		if (tCollision == RIGHT)
			{
			velocity = 0;
			tInputMatrix.vy = TRACK_ARRAY[DRAWFROM].model.Orientation.vy;
			RotMatrix(&tInputMatrix, &tMatrix);
			tPosition.vx = -tDirectionResult.vx;
			if (tPosition.vx > -512) tPosition.vx = -512;
			health += tPosition.vx;
			tPosition.vz = tDirectionResult.vz;
			ApplyMatrixLV(&tMatrix, &tPosition, &tDirectionResult);
			momentumX = tDirectionResult.vx;
			momentumZ = tDirectionResult.vz;
			}
		else
			{
			velocity = 0;
			tInputMatrix.vy = TRACK_ARRAY[DRAWFROM].model.Orientation.vy;
			RotMatrix(&tInputMatrix, &tMatrix);
			tPosition.vx = -tDirectionResult.vx;
			if (tPosition.vx < 512) tPosition.vx = 512;
			health -= tPosition.vx;
			tPosition.vz = tDirectionResult.vz;
			ApplyMatrixLV(&tMatrix, &tPosition, &tDirectionResult);
			momentumX = tDirectionResult.vx;
			momentumZ = tDirectionResult.vz;
			}
		}

// **** check for movement from one piece of track to the next
// **** going forward
	tInputMatrix.vy = -TRACK_ARRAY[DRAWFROM].model.Orientation.vy;
	RotMatrix(&tInputMatrix, &tMatrix);
	tPosition.vx = 	tTempX -
									TRACK_ARRAY[DRAWFROM].model.Object_Coord.coord.t[0];
	tPosition.vz = 	tTempZ -
									TRACK_ARRAY[DRAWFROM].model.Object_Coord.coord.t[2];
	ApplyMatrixLV(&tMatrix, &tPosition, &tDirectionResult);
	tTemp3 = tDirectionResult.vx;

	tTemp1 = (DRAWFROM + 1) %DRAWUPTO;
	tDistance1 = tDirectionResult.vz;
	tInputMatrix.vy = -TRACK_ARRAY[tTemp1].model.Orientation.vy;
	RotMatrix(&tInputMatrix, &tMatrix);
	tPosition.vx = 	myOBJ.Object_Coord.coord.t[0] -
									TRACK_ARRAY[tTemp1].model.Object_Coord.coord.t[0];
	tPosition.vz = 	myOBJ.Object_Coord.coord.t[2] -
									TRACK_ARRAY[tTemp1].model.Object_Coord.coord.t[2];
	ApplyMatrixLV(&tMatrix, &tPosition, &tDirectionResult);
	tDistance2 = -tDirectionResult.vz;
	if (tDistance2 < tDistance1)
		{
		tCollision = CheckForCollision(1, tDirectionResult.vx);
		if (tCollision != false)
			{
			tInputMatrix.vy = -TRACK_ARRAY[tTemp1].model.Orientation.vy;
			RotMatrix(&tInputMatrix, &tMatrix);
			tPosition.vx = momentumX;
			tPosition.vz = momentumZ;
			ApplyMatrixLV(&tMatrix, &tPosition, &tDirectionResult);
			velocity = 0;
			tInputMatrix.vy = TRACK_ARRAY[tTemp1].model.Orientation.vy;
			RotMatrix(&tInputMatrix, &tMatrix);
			tPosition.vx = tDirectionResult.vx;
			tPosition.vz = -tDirectionResult.vz;
			if (tPosition.vz > -512) tPosition.vz = -512;
			health += tPosition.vz;
			ApplyMatrixLV(&tMatrix, &tPosition, &tDirectionResult);
			momentumX = tDirectionResult.vx;
			momentumZ = tDirectionResult.vz;
			}
		else
			{
			DRAWFROM = tTemp1;
			if (DRAWFROM == 0) MoveOverStartLine();
			}
		}

// **** going backward
	tTemp1 = (DRAWFROM - 1 + DRAWUPTO) %DRAWUPTO;
	tInputMatrix.vy = -TRACK_ARRAY[tTemp1].model.Orientation.vy;
	RotMatrix(&tInputMatrix, &tMatrix);
	tPosition.vx = 	myOBJ.Object_Coord.coord.t[0] -
									TRACK_ARRAY[tTemp1].model.Object_Coord.coord.t[0];
	tPosition.vz = 	myOBJ.Object_Coord.coord.t[2] -
									TRACK_ARRAY[tTemp1].model.Object_Coord.coord.t[2];
	ApplyMatrixLV(&tMatrix, &tPosition, &tDirectionResult);
	tDistance2 = -tDirectionResult.vz;
	if (tDistance2 > tDistance1)
		{
		tCollision = CheckForCollision(-1, tDirectionResult.vx);
		if (tCollision != false)
			{
			tInputMatrix.vy = -TRACK_ARRAY[tTemp1].model.Orientation.vy;
			RotMatrix(&tInputMatrix, &tMatrix);
			tPosition.vx = momentumX;
			tPosition.vz = momentumZ;
			ApplyMatrixLV(&tMatrix, &tPosition, &tDirectionResult);
			velocity = 0;
			tInputMatrix.vy = TRACK_ARRAY[tTemp1].model.Orientation.vy;
			RotMatrix(&tInputMatrix, &tMatrix);
			tPosition.vx = tDirectionResult.vx;
			tPosition.vz = -tDirectionResult.vz;
			if (tPosition.vz < 512) tPosition.vz = 512;
			health -= tPosition.vz;
			ApplyMatrixLV(&tMatrix, &tPosition, &tDirectionResult);
			momentumX = tDirectionResult.vx;
			momentumZ = tDirectionResult.vz;
			}
		else	DRAWFROM = tTemp1;
		}

	playerX += momentumX;
	playerZ += momentumZ;
	MoveModelTo(&myOBJ, playerX>>8, 0, playerZ>>8);

	if (boost<1000) boost++;

//        FntPrint(FntOne, "Refresh           = %d / 280\n\n", VerticalSync);
//	FntPrint(FntOne, "Selected track = %d\n", DRAWFROM);
//  FntPrint(FntOne, "Health         = %d / 1000\n", health>>7);
//  FntPrint(FntOne, "Velocity       = %d / 350\n", velocity);
//  FntPrint(FntOne, "Boost          = %d / 500\n", boost);
//  FntPrint(FntOne, "Mom            = %d %d \n", momentumX/256, momentumZ/256);
//  FntPrint(FntOne, "fs1-%d, fe1-%d, ", TRACK_ARRAY[DRAWFROM].fs1, TRACK_ARRAY[DRAWFROM].fe1);
//  FntPrint(FntOne, "fs2-%d, fe2-%d, ", TRACK_ARRAY[DRAWFROM].fs2, TRACK_ARRAY[DRAWFROM].fe2);

  }

// ***********************************************************************************
// RENDERWORLD
// ***********************************************************************************

// **** Register everything that needs to be drawn and then draw it
void RenderWorld()
	{
	u_long tTemp1, tTempY;
	long tTemp4 = (((myOBJ.Orientation.vy + viewFromOrientation)>>1) + (512<<4)) % 512;
  long tTemp2 = DRAWFROM - 10, tTemp3 = DRAWFROM + 90;

  RenderPrepare();

	if (CurrentBuffer == 0)	tTempY = 240;
	else tTempY = 0;

	if (IsTitle == true)
		{
		MoveImage(&Title, 0, tTempY);
		RenderFinish();
		}
	else
		{
	 	for (tTemp1=0; tTemp1<10; tTemp1++)
			{
			DrawSprite(&Numbers[tTemp1][0], 0);
			DrawSprite(&Numbers[tTemp1][1], 0);
			DrawSprite(&Numbers[tTemp1][2], 0);
			DrawSprite(&Numbers[tTemp1][3], 0);
			DrawSprite(&Numbers[tTemp1][4], 0);
			DrawSprite(&Numbers[tTemp1][5], 0);
			DrawSprite(&Numbers[tTemp1][6], 0);
			}

		GsSortBoxFill(&Box1, &OTable_Header[CurrentBuffer], 1);

	  DrawColouredBox(-153, -93, -153+(boost>>3), -80, 1, 0,           // draw a nice blended background
										191,  63,   63,                     // with top left at (0,0), width of 320,
										63,   191,  63,                     // height of 240, to be drawn solidly,
	                  63,   63,   191,                    // to be drawn behind everything else,
	                  191,  191,  191);                   // with four sets of red/green/blue

		DrawModel(&myOBJ,1);
		if (OVERHEAD != true)
			{
			if (tTemp2 < 0)
				{
				for (tTemp1 = tTemp2 + DRAWUPTO; tTemp1<DRAWUPTO; tTemp1++)
					DrawModel(&TRACK_ARRAY[tTemp1].model, 0);
				for (tTemp1 = 0; tTemp1<tTemp3; tTemp1++)
					DrawModel(&TRACK_ARRAY[tTemp1].model, 0);
				}
			else
				{
				if (tTemp3 >= DRAWUPTO)
					{
					for (tTemp1 = tTemp2; tTemp1<DRAWUPTO; tTemp1++)
						DrawModel(&TRACK_ARRAY[tTemp1].model, 0);
					for (tTemp1 = 0; tTemp1<tTemp3-DRAWUPTO; tTemp1++)
						DrawModel(&TRACK_ARRAY[tTemp1].model, 0);
					}
				else
					{
					for (tTemp1 = tTemp2; tTemp1 < tTemp3; tTemp1++)
						DrawModel(&TRACK_ARRAY[tTemp1].model, 0);
					}
				}
			}
		else
			{
			for (tTemp1=0; tTemp1<DRAWUPTO; tTemp1++)
				DrawModel(&TRACK_ARRAY[tTemp1].model, 0);
			}

  	DrawModel(&TunnelThing, 0);

// do backround image
		if (tTemp4 > 192)
			{
			Background.x = tTemp4+320;
			Background.w = 512 - tTemp4;
			MoveImage(&Background, 0, tTempY);
			Background.x = 320;
			Background.w = -192 + tTemp4;
			MoveImage(&Background, 512- tTemp4, tTempY);
			}
		else
			{
			Background.x = tTemp4+320;
			Background.w = 320;
			MoveImage(&Background, 0, tTempY);
			}
	
		GsSortBoxFill(&Wipe, &OTable_Header[CurrentBuffer], BACK);
		RenderFinish();
		}
  }

// **** Respond to user input from joypad 1
void ProcessJoyPad()
	{
	u_long PAD = ReadJoyPad();
	u_long tTemp1;
	u_long tTemp2 = 0;

	if ((PAD& PADselect)) GameRunning = false;
//  if (PAD& PADR2) dumpScreen(0,240,319,481);

	if (IsTitle == true)
		{
		if (PAD& PADcross) 
			{
			IsTitle = false;
			crossPressed = true;
			}
		}
	else
		{
			{
			if ((PAD& PADstart) || (LapsDone == 8))
				{
				IsTitle = true;
				DRAWFROM = 10;
				PlayerStartedRacing = false;
				health = 1000<<7;
				boost = 1000;
				velocity = 0;
				momentumX = 0, momentumZ = 0;
				viewFromOrientation = 0;
				SetModelInfo(&myOBJ,  TRACK_ARRAY[10].model.Object_Coord.coord.t[0],
											0,
											TRACK_ARRAY[10].model.Object_Coord.coord.t[2],
											TMD_SHIP);
				playerX = TRACK_ARRAY[10].model.Object_Coord.coord.t[0]<<8;
				playerZ = TRACK_ARRAY[10].model.Object_Coord.coord.t[2]<<8;
				RotateModel(&TunnelThing,       TRACK_ARRAY[450].model.Orientation.vx,
						TRACK_ARRAY[450].model.Orientation.vy,
						TRACK_ARRAY[450].model.Orientation.vz);
		
				InitialiseCounter(1);
/*				for (tTemp1=0; tTemp1<12; tTemp1++)
					{
					if (tTemp1 != 0) tTemp2 = (1<<31);
					else tTemp2 = (1<<30);
					Numbers[tTemp1][0].attribute = 
					Numbers[tTemp1][1].attribute = 
					Numbers[tTemp1][2].attribute = 
					Numbers[tTemp1][3].attribute = 
					Numbers[tTemp1][4].attribute = (1<<30) + (1<<28) + tTemp2;
					}
*/				LapsDone = 0;
				}
			if (PAD& PADleft)
			{
			RotateModel(&myOBJ, 0, -24, 0);
			viewFromOrientation += 24;
			}
			if (PAD& PADL1)
			{
			RotateModel(&myOBJ, 0, -24, 0);
			viewFromOrientation += 24;
			velocity *= 0.952;
			}
			if (PAD& PADright)
			{
			RotateModel(&myOBJ, 0, 24, 0);
			viewFromOrientation -= 24;
			}
			if (PAD& PADR1)
			{
			RotateModel(&myOBJ, 0, 24, 0);
			viewFromOrientation -= 24;
			velocity *= 0.952;
			}
			if ((PAD& PADcross) && (crossPressed == false))	velocity += 18;
			if (!(PAD& PADcross) && (crossPressed == true)) crossPressed = false;
			if ((PAD& PADcircle) && (boost>5))
			{
			velocity += 18;
			boost -= 5;
			}
			if (PAD& PADsquare)
			{
			momentumX *= 0.97;
			momentumZ *= 0.97;
			}
			if (PAD& PADtriangle) OVERHEAD = true;
			else OVERHEAD = false;
			}
		}
	}

void MoveOverStartLine()
	{
	u_long tTemp1, tTemp2;
	for (tTemp1=9; tTemp1>1; tTemp1--)
		{
		for (tTemp2 = 0; tTemp2 < 7; tTemp2++)
			{
			Numbers[tTemp1][tTemp2].attribute = Numbers[tTemp1-1][tTemp2].attribute;
			Numbers[tTemp1][tTemp2].u = Numbers[tTemp1-1][tTemp2].u;
			}
		}
	for (tTemp1= 0; tTemp1<7; tTemp1++)
		{
		Numbers[1][tTemp1].attribute = (1<<30)+(1<<28);
		Numbers[1][tTemp1].u = Numbers[0][tTemp1].u;
		CounterDigits[0][tTemp1%5] = 0;
		Numbers[0][tTemp1%5].u = 0;
		}
	LapsDone ++;
	}

u_long CheckForCollision(long	tDisplacement, long tPosition)
	{
	u_long tReturn = false;
	u_long tTemp1 = TRACK_ARRAY[(DRAWFROM+DRAWUPTO+tDisplacement) % DRAWUPTO].attribute;
	long tTemp2, tTemp3;

	switch (tTemp1)
		{
		case 	NORMAL:	tTemp2 = -350; tTemp3 = 350; break;
		case 	LEFT:		tTemp2 = -350; tTemp3 = -50; break;
		case 	RIGHT:	tTemp2 = 50; tTemp3 =350; break;
		}
	if (tPosition < tTemp2) tReturn = LEFT;
	if (tPosition > tTemp3) tReturn = RIGHT;
	return tReturn;
	}
