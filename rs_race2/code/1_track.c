#include "header.h"

// ***********************************************************************************
// MAIN FUNCTION
// ***********************************************************************************

void create_Track()
	{
	u_long tTemp1;
	u_long tTemp2;

  long modelX = 10000, modelZ = 10000;
	u_char	tAttribute;

	SVECTOR tInputMatrix;
  MATRIX tMatrix;
  VECTOR tPosition, tDirectionResult;

	tCurrentOrientation = 0;

	create_TRK_4_generic(TMD_DYNAMIC_START, TRK_4_S_S, 0, 0);
	create_TRK_4_generic(0, TRK_4_S_L8, 0, -8);
	create_TRK_4_generic(0, TRK_4_S_L32, 0, -32);
	create_TRK_4_generic(0, TRK_4_L8_S, -8, 0);
	create_TRK_4_generic(0, TRK_4_L8_L8, -8, -8);
	create_TRK_4_generic(0, TRK_4_L8_L16, -8, -16);
	create_TRK_4_generic(0, TRK_4_L16_S, -16, 0);
	create_TRK_4_generic(0, TRK_4_L16_L8, -16, -8);
	create_TRK_4_generic(0, TRK_4_L16_L16, -16, -16);
	create_TRK_4_generic(0, TRK_4_L32_S, -32, 0);
	create_TRK_4_generic(0, TRK_4_L32_L8, -32, -8);
	create_TRK_4_generic(0, TRK_4_L32_L16, -32, -16);
	create_TRK_4_generic(0, TRK_4_L32_L32, -32, -32);
	create_TRK_4_generic(0, TRK_4_L32_L64, -32, -64);
	create_TRK_4_generic(0, TRK_4_L64_L32, -64, -32);
	create_TRK_4_generic(0, TRK_4_L64_L64, -64, -64);
	create_TRK_4_generic(0, TRK_4_S_R8, 0, 8);
	create_TRK_4_generic(0, TRK_4_S_R64, 0, 64);
	create_TRK_4_generic(0, TRK_4_R8_S, 8, 0);
	create_TRK_4_generic(0, TRK_4_R8_R8, 8, 8);
	create_TRK_4_generic(0, TRK_4_R8_R20, 8, 20);
	create_TRK_4_generic(0, TRK_4_R20_R8, 20, 8);
	create_TRK_4_generic(0, TRK_4_R20_R20, 20, 20);
	create_TRK_4_generic(0, TRK_4_R20_R48, 20, 48);
	create_TRK_4_generic(0, TRK_4_R48_R8, 48, 8);
	create_TRK_4_generic(0, TRK_4_R48_R48, 48, 48);
	create_TRK_4_generic(0, TRK_4_R64_S, 64, 0);
	create_TRK_4_generic(0, TRK_4_R64_R64, 64, 64);
	create_TRK_2L_START_generic(0, TRK_2L_S_R32, 0, 32);
	create_TRK_2L_generic(0, TRK_2L_R32_R32, 32, 32);
	create_TRK_2L_END_generic(0, TRK_2L_R32_S, 32, 0);
	create_TRK_2L_generic(0, TRK_2L_S_S, 0, 0);
	create_TRK_2L_generic(0, TRK_2L_S_L16, 0, -16);
	create_TRK_2L_generic(0, TRK_2L_L16_L16, -16, -16);
	create_TRK_2L_END_generic(0, TRK_2L_L16_L64, -16, -64);
	create_TRK_2R_START_generic(0, TRK_2R_S_L32, 0, -32);
	create_TRK_2R_generic(0, TRK_2R_L32_L32, -32, -32);
	create_TRK_2R_generic(0, TRK_2R_L32_S, -32, 0);
	create_TRK_2R_generic(0, TRK_2R_S_S, 0, 0);
	create_TRK_2R_generic(0, TRK_2R_S_R32, 0, 32);
	create_TRK_2R_generic(0, TRK_2R_R32_R32, 32, 32);
	create_TRK_2R_END_generic(0, TRK_2R_R32_S, 32, 0);
	create_TRK_2L_END_generic(0, TRK_2L_S_S_END, 0, 0);

	create_TRK_4_BRIDGE();
	create_TRK_4_BRIDGE2();
	create_TRK_4_TUNNEL();
	create_TRK_4_TUNNELIN();
	create_TRK_4_TUNNELIN2();
	create_TRK_4_TUNNEL2();
	create_TRK_4_TUNNELOUT();
	create_TRK_4_TUNNELOUT2();

	create_TRK_2R_START_generic(0, TRK_2R_S_S_START, 0, 0);	
	create_TRK_2R_END_generic(0, TRK_2R_S_S_END, 0, 0);	
	create_TRK_2L_START_generic(0, TRK_2L_S_S_START, 0, 0);	

  for (tTemp1=0; tTemp1<DRAWUPTO; tTemp1++)
    {
    tTemp2 = ReturnTrack(tTemp1);
    tAttribute = NORMAL;
    switch (tTemp2)
      {
			case TRK_4_S_S:		tCurrentOrientation += 0; break;
			case TRK_4_S_L8:		tCurrentOrientation += 0; break;
			case TRK_4_S_L32:		tCurrentOrientation += 0; break;
			case TRK_4_L8_S:		tCurrentOrientation -= 16; break;
			case TRK_4_L8_L8:		tCurrentOrientation -= 16; break;
			case TRK_4_L8_L16:		tCurrentOrientation -= 16; break;
			case TRK_4_L16_S:		tCurrentOrientation -= 32; break;
			case TRK_4_L16_L8:		tCurrentOrientation -= 32; break;
			case TRK_4_L16_L16:		tCurrentOrientation -= 32; break;
			case TRK_4_L32_S:		tCurrentOrientation -= 64; break;
			case TRK_4_L32_L8:		tCurrentOrientation -= 64; break;
			case TRK_4_L32_L16:		tCurrentOrientation -= 64; break;
			case TRK_4_L32_L32:		tCurrentOrientation -= 64; break;
			case TRK_4_L32_L64:		tCurrentOrientation -= 64; break;
			case TRK_4_L64_L32:		tCurrentOrientation -= 128; break;
			case TRK_4_L64_L64:		tCurrentOrientation -= 128; break;
			case TRK_4_S_R8:		tCurrentOrientation += 0; break;
			case TRK_4_S_R64:		tCurrentOrientation += 0; break;
			case TRK_4_R8_S:		tCurrentOrientation += 16; break;
			case TRK_4_R8_R8:		tCurrentOrientation += 16; break;
			case TRK_4_R8_R20:		tCurrentOrientation += 16; break;
			case TRK_4_R20_R8:		tCurrentOrientation += 40; break;
			case TRK_4_R20_R20:		tCurrentOrientation += 40; break;
			case TRK_4_R20_R48:		tCurrentOrientation += 40; break;
			case TRK_4_R48_R8:		tCurrentOrientation += 96; break;
			case TRK_4_R48_R48:		tCurrentOrientation += 96; break;
			case TRK_4_R64_S:		tCurrentOrientation += 128; break;
			case TRK_4_R64_R64:		tCurrentOrientation += 128; break;
			case TRK_4_BRIDGE:		tCurrentOrientation += 0; break;
			case TRK_4_BRIDGE2:		tCurrentOrientation += 0; break;
			case TRK_4_TUNNEL:		tCurrentOrientation += 0; break;
			case TRK_4_TUNNELIN:		tCurrentOrientation += 0; break;
			case TRK_4_TUNNELIN2:		tCurrentOrientation += 0; break;
			case TRK_4_TUNNEL2:		tCurrentOrientation += 0; break;
			case TRK_4_TUNNELOUT:		tCurrentOrientation += 0; break;
			case TRK_4_TUNNELOUT2:		tCurrentOrientation += 0; break;
			case TRK_2L_S_R32:		tCurrentOrientation += 0; tAttribute = LEFT; break;
			case TRK_2L_R32_R32:		tCurrentOrientation += 64; tAttribute = LEFT; break;
			case TRK_2L_R32_S:		tCurrentOrientation += 64; tAttribute = LEFT; break;
			case TRK_2L_S_S:		tCurrentOrientation += 0; tAttribute = LEFT; break;
			case TRK_2L_S_L16:		tCurrentOrientation += 0; tAttribute = LEFT; break;
			case TRK_2L_L16_L16:		tCurrentOrientation -= 32; tAttribute = LEFT; break;
			case TRK_2L_L16_L64:		tCurrentOrientation -= 32; tAttribute = LEFT; break;
			case TRK_2R_S_L32:		tCurrentOrientation += 0; tAttribute = RIGHT; break;
			case TRK_2R_L32_L32:		tCurrentOrientation -= 64; tAttribute = RIGHT; break;
			case TRK_2R_L32_S:		tCurrentOrientation -= 64; tAttribute = RIGHT; break;
			case TRK_2R_S_S:		tCurrentOrientation += 0; tAttribute = RIGHT; break;
			case TRK_2R_S_R32:		tCurrentOrientation += 0; tAttribute = RIGHT; break;
			case TRK_2R_R32_R32:		tCurrentOrientation += 64; tAttribute = RIGHT; break;
			case TRK_2R_R32_S:		tCurrentOrientation += 64; tAttribute = RIGHT; break;
			case TRK_2R_S_S_START:		tCurrentOrientation += 0; tAttribute = RIGHT; break;
			case TRK_2R_S_S_END:		tCurrentOrientation += 0; tAttribute = RIGHT; break;
			case TRK_2L_S_S_START:		tCurrentOrientation += 0; tAttribute = LEFT; break;
      }

		tInputMatrix.vx = tInputMatrix.vz = 0;
    tInputMatrix.vy = tCurrentOrientation;
    RotMatrix(&tInputMatrix, &tMatrix);
    tPosition.vx = 0; tPosition.vz = 100*10;
    ApplyMatrixLV(&tMatrix, &tPosition, &tDirectionResult);
    modelX += tDirectionResult.vx;
		modelZ += tDirectionResult.vz;

		SetModelInfo(&TRACK_ARRAY[tTemp1].model, modelX/10, 0, modelZ/10, TMD_START_TRK[tTemp2]);
		TRACK_ARRAY[tTemp1].model.Orientation.vx = 0;
		TRACK_ARRAY[tTemp1].model.Orientation.vy = 0;
		TRACK_ARRAY[tTemp1].model.Orientation.vz = 0;
		RotateModel(&TRACK_ARRAY[tTemp1].model, 0, tCurrentOrientation, 0);
		TRACK_ARRAY[tTemp1].attribute = tAttribute;

		tInputMatrix.vx = tInputMatrix.vz = 0;
		tInputMatrix.vy = tCurrentOrientation;
		RotMatrix(&tInputMatrix, &tMatrix);
    tPosition.vx = 0; tPosition.vz = 100*10;
    ApplyMatrixLV(&tMatrix, &tPosition, &tDirectionResult);
    modelX += tDirectionResult.vx;
    modelZ += tDirectionResult.vz;
    }
  }

// ***********************************************************************************
// GENERIC vertex creation for various width roads
// ***********************************************************************************

void setpoint(VERTEX *tVertex, long tAngle, long tX, long tY, long tZ)
	{
  SVECTOR tInputMatrix;
  MATRIX tMatrix;
  VECTOR tPosition, tDirectionResult;

  tInputMatrix.vy = tAngle;
  tInputMatrix.vx = tInputMatrix.vz = 0;
  RotMatrix(&tInputMatrix, &tMatrix);
  tPosition.vx = tX;
  tPosition.vy = 0;
  tPosition.vz = tZ;
  ApplyMatrixLV(&tMatrix, &tPosition, &tDirectionResult);
  TMD_setVERTEX(tVertex, tDirectionResult.vx, tY, tDirectionResult.vz);
	}

void track_2L_front(VERTEX *tVertex, long tAngle)
  {
  setpoint(tVertex+7, tAngle, -500, -100, 105);
  setpoint(tVertex+8, tAngle, -400, 0, 105);
  setpoint(tVertex+9, tAngle, -200, 0, 105);
  setpoint(tVertex+10, tAngle, 0, 0, 105);
  setpoint(tVertex+11, tAngle, 0, -100, 105);
  setpoint(tVertex+12, tAngle, 0, -500, 105);
  setpoint(tVertex+13, tAngle, 0, -600, 105);
  setpoint(tVertex+18, tAngle, 600, 0, 105);
  setpoint(tVertex+19, tAngle, 600, -100, 105);
  setpoint(tVertex+20, tAngle, 600, -500, 105);
  setpoint(tVertex+21, tAngle, 600, -600, 105);
  }

void track_2L_rear(VERTEX *tVertex, long tAngle)
  {
  setpoint(tVertex+0, tAngle, -500, -100, -105);
  setpoint(tVertex+1, tAngle, -400, 0, -105);
  setpoint(tVertex+2, tAngle, -200, 0, -105);
  setpoint(tVertex+3, tAngle, 0, 0, -105);
  setpoint(tVertex+4, tAngle, 0, -100, -105);
  setpoint(tVertex+5, tAngle, 0, -500, -105);
  setpoint(tVertex+6, tAngle, 0, -600, -105);
  setpoint(tVertex+14, tAngle, 600, 0, -105);
  setpoint(tVertex+15, tAngle, 600, -100, -105);
  setpoint(tVertex+16, tAngle, 600, -500, -105);
  setpoint(tVertex+17, tAngle, 600, -600, -105);
  }

void track_2R_front(VERTEX *tVertex, long tAngle)
  {
  setpoint(tVertex+7, tAngle, 0, -600, 105);
  setpoint(tVertex+8, tAngle, 0, -500, 105);
  setpoint(tVertex+9, tAngle, 0, -100, 105);
  setpoint(tVertex+10, tAngle, 0, 0, 105);
  setpoint(tVertex+11, tAngle, 200, 0, 105);
  setpoint(tVertex+12, tAngle, 400, 0, 105);
  setpoint(tVertex+13, tAngle, 500, -100, 105);
  setpoint(tVertex+18, tAngle, -600, -600, 105);
  setpoint(tVertex+19, tAngle, -600, -500, 105);
  setpoint(tVertex+20, tAngle, -600, -100, 105);
  setpoint(tVertex+21, tAngle, -600, 0, 105);
  }

void track_2R_rear(VERTEX *tVertex, long tAngle)
  {
  setpoint(tVertex+0, tAngle, 0, -600, -105);
  setpoint(tVertex+1, tAngle, 0, -500, -105);
  setpoint(tVertex+2, tAngle, 0, -100, -105);
  setpoint(tVertex+3, tAngle, 0, 0, -105);
  setpoint(tVertex+4, tAngle, 200, 0, -105);
  setpoint(tVertex+5, tAngle, 400, 0, -105);
  setpoint(tVertex+6, tAngle, 500, -100, -105);
  setpoint(tVertex+14, tAngle, -600, -600, -105);
  setpoint(tVertex+15, tAngle, -600, -500, -105);
  setpoint(tVertex+16, tAngle, -600, -100, -105);
  setpoint(tVertex+17, tAngle, -600, 0, -105);
  }

void track_4_front(VERTEX *tVertex, long tAngle)
  {
  setpoint(tVertex+7, tAngle, -500, -100, 105);
  setpoint(tVertex+8, tAngle, -400, 0, 105);
  setpoint(tVertex+9, tAngle, -200, 0, 105);
  setpoint(tVertex+10, tAngle, 0, 0, 105);
  setpoint(tVertex+11, tAngle, 200, 0, 105);
  setpoint(tVertex+12, tAngle, 400, 0, 105);
  setpoint(tVertex+13, tAngle, 500, -100, 105);
  }

void track_4_rear(VERTEX *tVertex, long tAngle)
  {
  setpoint(tVertex+0, tAngle, -500, -100, -105);
  setpoint(tVertex+1, tAngle, -400, 0, -105);
  setpoint(tVertex+2, tAngle, -200, 0, -105);
  setpoint(tVertex+3, tAngle, 0, 0, -105);
  setpoint(tVertex+4, tAngle, 200, 0, -105);
  setpoint(tVertex+5, tAngle, 400, 0, -105);
  setpoint(tVertex+6, tAngle, 500, -100, -105);
  }

// ***********************************************************************************
// TRACK DEFINITION STARTS HERE
// ***********************************************************************************

void create_TRK_4_generic(u_long tMemType, u_long tType, long tRearAng, long tFrontAng)
	{
  VERTEX  v[14];
  COLOUR  rgb[4];

  track_4_rear(&v[0], -tRearAng);
  track_4_front(&v[0], tFrontAng);

  TMD_setCOLOUR(&rgb[0], 255, 255, 255);
  TMD_setCOLOUR(&rgb[1], 32, 34, 36);
  TMD_setCOLOUR(&rgb[2], 96, 80, 64);
  TMD_setCOLOUR(&rgb[3], 48, 40, 32);

  TMD_prepare(tMemType, &v[0], &rgb[0], 0);
  TMD_set4_NS_GP( 7, 0, 8, 1, 1, 1, 0, 0, true);
  TMD_set4_NS_GP( 0, 7, 1, 8, 1, 1, 0, 0, true);
  TMD_set4_NS_GP( 1, 8, 2, 9, 2, 2, 3, 3, false);
  TMD_set4_NS_GP( 2, 9, 3, 10, 3, 3, 2, 2, false);
  TMD_set4_NS_GP( 3, 10, 4, 11, 2, 2, 3, 3, false);
  TMD_set4_NS_GP( 4, 11, 5, 12, 3, 3, 2, 2, false);
  TMD_set4_NS_GP( 5, 12, 6, 13, 0, 0, 1, 1, true);
  TMD_set4_NS_GP( 12, 5, 13, 6, 0, 0, 1, 1, true);
  TMD_START_TRK[tType] = TMD_finish();
	}

void create_TRK_2L_generic(u_long tMemType, u_long tType, long tRearAng, long tFrontAng)
	{
  VERTEX  v[22];
  COLOUR  rgb[8];

  track_2L_rear(&v[0], -tRearAng);
  track_2L_front(&v[0], tFrontAng);

  TMD_setCOLOUR(&rgb[0], 255, 255, 255);
  TMD_setCOLOUR(&rgb[1], 32, 34, 36);
  TMD_setCOLOUR(&rgb[2], 96, 80, 64);
  TMD_setCOLOUR(&rgb[3], 48, 40, 32);
  TMD_setCOLOUR(&rgb[4], 0, 128, 144);
  TMD_setCOLOUR(&rgb[5], 48, 96, 128);
  TMD_setCOLOUR(&rgb[6], 32, 64, 112);
  TMD_setCOLOUR(&rgb[7], 212, 212, 236);

  TMD_prepare(tMemType, &v[0], &rgb[0], 0);

  TMD_set4_NS_GP( 0, 7, 1, 8, 1, 1, 0, 0, true);
  TMD_set4_NS_GP( 7, 0, 8, 1, 1, 1, 0, 0, true);
  TMD_set4_NS_GP( 1, 8, 2, 9, 2, 2, 3, 3, false);
  TMD_set4_NS_GP( 2, 9, 3, 10, 3, 3, 2, 2, false);

	TMD_set4_NS_FP( 3, 10, 4, 11, 7, false);
	TMD_set4_NS_GP( 4, 11, 5, 12, 5, 6, 4, 5, false);
	TMD_set4_NS_FP( 5, 12, 6, 13, 7, false);

  TMD_set4_NS_FP( 18, 14, 19, 15, 7, false);
  TMD_set4_NS_GP( 19, 15, 20, 16, 5, 6, 4, 5, false);
  TMD_set4_NS_FP( 20, 16, 21, 17, 7, false);

  TMD_START_TRK[tType] = TMD_finish();
	}

void create_TRK_2L_START_generic(u_long tMemType, u_long tType, long tRearAng, long tFrontAng)
	{
  VERTEX  v[30];
  COLOUR  rgb[9];

  track_2L_rear(&v[0], -tRearAng);
  track_2L_front(&v[0], tFrontAng);
	
	setpoint(&v[14], -tRearAng, 200, -600, -105);
	setpoint(&v[15], -tRearAng, 200, -500, -105);
	setpoint(&v[16], -tRearAng, 200, -100, -105);
	setpoint(&v[17], -tRearAng, 200, 0, -105);
	setpoint(&v[18], -tRearAng, 400, -600, -105);
	setpoint(&v[19], -tRearAng, 400, -500, -105);
	setpoint(&v[20], -tRearAng, 400, -100, -105);
	setpoint(&v[21], -tRearAng, 400, 0, -105);
	setpoint(&v[22], -tRearAng, 600, -600, -105);
	setpoint(&v[23], -tRearAng, 600, -500, -105);
	setpoint(&v[24], -tRearAng, 600, -100, -105);
	setpoint(&v[25], -tRearAng, 600, 0, -105);
	setpoint(&v[26], tFrontAng, 600, -600, 105);
	setpoint(&v[27], tFrontAng, 600, -500, 105);
	setpoint(&v[28], tFrontAng, 600, -100, 105);
	setpoint(&v[29], tFrontAng, 600, 0, 105);

  TMD_setCOLOUR(&rgb[0], 255, 255, 255);
  TMD_setCOLOUR(&rgb[1], 32, 34, 36);
  TMD_setCOLOUR(&rgb[2], 96, 80, 64);
  TMD_setCOLOUR(&rgb[3], 48, 40, 32);
  TMD_setCOLOUR(&rgb[4], 0, 128, 144);
  TMD_setCOLOUR(&rgb[5], 48, 96, 128);
  TMD_setCOLOUR(&rgb[6], 32, 64, 112);
  TMD_setCOLOUR(&rgb[7], 212, 212, 236);
  TMD_setCOLOUR(&rgb[8], 196, 196, 212);

  TMD_prepare(tMemType, &v[0], &rgb[0], 0);

  TMD_set4_NS_GP( 0, 7, 1, 8, 1, 1, 0, 0, true);
  TMD_set4_NS_GP( 7, 0, 8, 1, 1, 1, 0, 0, true);
  TMD_set4_NS_GP( 1, 8, 2, 9, 2, 2, 3, 3, false);
  TMD_set4_NS_GP( 2, 9, 3, 10, 3, 3, 2, 2, false);

	TMD_set4_NS_FP( 3, 10, 4, 11, 7, false);
	TMD_set4_NS_FP( 4, 11, 5, 12, 7, false);
	TMD_set4_NS_FP( 5, 12, 6, 13, 7, false);

	TMD_set4_NS_FP( 6, 14, 5, 15, 8, false);
	TMD_set4_NS_FP( 5, 15, 4, 16, 8, false);
	TMD_set4_NS_FP( 4, 16, 3, 17, 8, false);
	TMD_set4_NS_FP( 14, 18, 15, 19, 8, false);
	TMD_set4_NS_GP( 15, 19, 16, 20, 5, 4, 6, 5, false);
	TMD_set4_NS_FP( 16, 20, 17, 21, 8, false);
	TMD_set4_NS_FP( 18, 22, 19, 23, 8, false);
	TMD_set4_NS_FP( 19, 23, 20, 24, 8, false);
	TMD_set4_NS_FP( 20, 24, 21, 25, 8, false);

	TMD_set4_NS_FP( 22, 26, 23, 27, 7, false);
	TMD_set4_NS_FP( 23, 27, 24, 28, 7, false);
	TMD_set4_NS_FP( 24, 28, 25, 29, 7, false);

  TMD_START_TRK[tType] = TMD_finish();
	}

void create_TRK_2L_END_generic(u_long tMemType, u_long tType, long tRearAng, long tFrontAng)
	{
  VERTEX  v[30];
  COLOUR  rgb[9];

  track_2L_rear(&v[0], -tRearAng);
  track_2L_front(&v[0], tFrontAng);
	
	setpoint(&v[14], tFrontAng, 200, -600, 105);
	setpoint(&v[15], tFrontAng, 200, -500, 105);
	setpoint(&v[16], tFrontAng, 200, -100, 105);
	setpoint(&v[17], tFrontAng, 200, 0, 105);
	setpoint(&v[18], tFrontAng, 400, -600, 105);
	setpoint(&v[19], tFrontAng, 400, -500, 105);
	setpoint(&v[20], tFrontAng, 400, -100, 105);
	setpoint(&v[21], tFrontAng, 400, 0, 105);
	setpoint(&v[22], tFrontAng, 600, -600, 105);
	setpoint(&v[23], tFrontAng, 600, -500, 105);
	setpoint(&v[24], tFrontAng, 600, -100, 105);
	setpoint(&v[25], tFrontAng, 600, 0, 105);
	setpoint(&v[26], -tRearAng, 600, -600, -105);
	setpoint(&v[27], -tRearAng, 600, -500, -105);
	setpoint(&v[28], -tRearAng, 600, -100, -105);
	setpoint(&v[29], -tRearAng, 600, 0, -105);

  TMD_setCOLOUR(&rgb[0], 255, 255, 255);
  TMD_setCOLOUR(&rgb[1], 32, 34, 36);
  TMD_setCOLOUR(&rgb[2], 96, 80, 64);
  TMD_setCOLOUR(&rgb[3], 48, 40, 32);
  TMD_setCOLOUR(&rgb[4], 0, 128, 144);
  TMD_setCOLOUR(&rgb[5], 48, 96, 128);
  TMD_setCOLOUR(&rgb[6], 32, 64, 112);
  TMD_setCOLOUR(&rgb[7], 212, 212, 236);
  TMD_setCOLOUR(&rgb[8], 196, 196, 212);

  TMD_prepare(tMemType, &v[0], &rgb[0], 0);

  TMD_set4_NS_GP( 0, 7, 1, 8, 1, 1, 0, 0, true);
  TMD_set4_NS_GP( 7, 0, 8, 1, 1, 1, 0, 0, true);
  TMD_set4_NS_GP( 1, 8, 2, 9, 2, 2, 3, 3, false);
  TMD_set4_NS_GP( 2, 9, 3, 10, 3, 3, 2, 2, false);

	TMD_set4_NS_FP( 3, 10, 4, 11, 7, false);
	TMD_set4_NS_FP( 4, 11, 5, 12, 7, false);
	TMD_set4_NS_FP( 5, 12, 6, 13, 7, false);

	TMD_set4_NS_FP( 14, 13, 15, 12, 8, false);
	TMD_set4_NS_FP( 15, 12, 16, 11, 8, false);
	TMD_set4_NS_FP( 16, 11, 17, 10, 8, false);
	TMD_set4_NS_FP( 18, 14, 19, 15, 8, false);
	TMD_set4_NS_GP( 19, 15, 20, 16, 5, 4, 6, 5, false);
	TMD_set4_NS_FP( 20, 16, 21, 17, 8, false);
	TMD_set4_NS_FP( 22, 18, 23, 19, 8, false);
	TMD_set4_NS_FP( 23, 19, 24, 20, 8, false);
	TMD_set4_NS_FP( 24, 20, 25, 21, 8, false);

	TMD_set4_NS_FP( 26, 22, 27, 23, 7, false);
	TMD_set4_NS_FP( 27, 23, 28, 24, 7, false);
	TMD_set4_NS_FP( 28, 24, 29, 25, 7, false);

  TMD_START_TRK[tType] = TMD_finish();
	}

void create_TRK_2R_generic(u_long tMemType, u_long tType, long tRearAng, long tFrontAng)
	{
  VERTEX  v[22];
  COLOUR  rgb[8];

  track_2R_rear(&v[0], -tRearAng);
  track_2R_front(&v[0], tFrontAng);

  TMD_setCOLOUR(&rgb[0], 255, 255, 255);
  TMD_setCOLOUR(&rgb[1], 32, 34, 36);
  TMD_setCOLOUR(&rgb[2], 96, 80, 64);
  TMD_setCOLOUR(&rgb[3], 48, 40, 32);
  TMD_setCOLOUR(&rgb[4], 0, 128, 144);
  TMD_setCOLOUR(&rgb[5], 48, 96, 128);
  TMD_setCOLOUR(&rgb[6], 32, 64, 112);
  TMD_setCOLOUR(&rgb[7], 212, 212, 236);

  TMD_prepare(tMemType, &v[0], &rgb[0], 0);
  TMD_set4_NS_FP( 0, 7, 1, 8, 7, false);
  TMD_set4_NS_GP( 1, 8, 2, 9, 5, 4, 6, 5, false);
  TMD_set4_NS_FP( 2, 9, 3, 10, 7, false);

  TMD_set4_NS_FP( 18, 14, 19, 15, 7, false);
  TMD_set4_NS_GP( 19, 15, 20, 16, 5, 4, 6, 5, false);
  TMD_set4_NS_FP( 20, 16, 21, 17, 7, false);

  TMD_set4_NS_GP( 3, 10, 4, 11, 2, 2, 3, 3, false);
  TMD_set4_NS_GP( 4, 11, 5, 12, 3, 3, 2, 2, false);
  TMD_set4_NS_GP( 5, 12, 6, 13, 0, 0, 1, 1, true);
  TMD_set4_NS_GP( 12, 5, 13, 6, 0, 0, 1, 1, true);
  TMD_START_TRK[tType] = TMD_finish();
	}

void create_TRK_2R_START_generic(u_long tMemType, u_long tType, long tRearAng, long tFrontAng)
	{
  VERTEX  v[30];
  COLOUR  rgb[9];

  track_2R_rear(&v[0], -tRearAng);
  track_2R_front(&v[0], tFrontAng);

  setpoint(&v[14], -tRearAng, -600, -600, -105);
  setpoint(&v[15], -tRearAng, -600, -500, -105);
  setpoint(&v[16], -tRearAng, -600, -100, -105);
  setpoint(&v[17], -tRearAng, -600, 0, -105);
  setpoint(&v[18], -tRearAng, -400, -600, -105);
  setpoint(&v[19], -tRearAng, -400, -500, -105);
  setpoint(&v[20], -tRearAng, -400, -100, -105);
  setpoint(&v[21], -tRearAng, -400, 0, -105);
  setpoint(&v[22], -tRearAng, -200, -600, -105);
  setpoint(&v[23], -tRearAng, -200, -500, -105);
  setpoint(&v[24], -tRearAng, -200, -100, -105);
  setpoint(&v[25], -tRearAng, -200, 0, -105);
  setpoint(&v[26], tFrontAng, -600, -600, 105);
  setpoint(&v[27], tFrontAng, -600, -500, 105);
  setpoint(&v[28], tFrontAng, -600, -100, 105);
  setpoint(&v[29], tFrontAng, -600, 0, 105);

  TMD_setCOLOUR(&rgb[0], 255, 255, 255);
  TMD_setCOLOUR(&rgb[1], 32, 34, 36);
  TMD_setCOLOUR(&rgb[2], 96, 80, 64);
  TMD_setCOLOUR(&rgb[3], 48, 40, 32);
  TMD_setCOLOUR(&rgb[4], 0, 128, 144);
  TMD_setCOLOUR(&rgb[5], 48, 96, 128);
  TMD_setCOLOUR(&rgb[6], 32, 64, 112);
  TMD_setCOLOUR(&rgb[7], 196, 196, 212);
  TMD_setCOLOUR(&rgb[8], 212, 212, 236);

  TMD_prepare(tMemType, &v[0], &rgb[0], 0);
  TMD_set4_NS_FP( 0, 7, 1, 8, 8, false);
  TMD_set4_NS_FP( 1, 8, 2, 9, 8, false);
  TMD_set4_NS_FP( 2, 9, 3, 10, 8, false);

  TMD_set4_NS_FP( 26, 14, 27, 15, 8, false);
  TMD_set4_NS_FP( 27, 15, 28, 16, 8, false);
  TMD_set4_NS_FP( 28, 16, 29, 17, 8, false);

	TMD_set4_NS_FP( 14, 18, 15, 19, 7, false);
	TMD_set4_NS_FP( 15, 19, 16, 20, 7, false);
	TMD_set4_NS_FP( 16, 20, 17, 21, 7, false);
	TMD_set4_NS_FP( 18, 22, 19, 23, 7, false);
	TMD_set4_NS_GP( 19, 23, 20, 24, 5, 4, 6, 5, false);
	TMD_set4_NS_FP( 20, 24, 21, 25, 7, false);
	TMD_set4_NS_FP( 22, 0, 23, 1, 7, false);
	TMD_set4_NS_FP( 23, 1, 24, 2, 7, false);
	TMD_set4_NS_FP( 24, 2, 25, 3, 7, false);

  TMD_set4_NS_GP( 3, 10, 4, 11, 2, 2, 3, 3, false);
  TMD_set4_NS_GP( 4, 11, 5, 12, 3, 3, 2, 2, false);
  TMD_set4_NS_GP( 5, 12, 6, 13, 0, 0, 1, 1, true);
  TMD_set4_NS_GP( 12, 5, 13, 6, 0, 0, 1, 1, true);
  TMD_START_TRK[tType] = TMD_finish();
	}

void create_TRK_2R_END_generic(u_long tMemType, u_long tType, long tRearAng, long tFrontAng)
	{
  VERTEX  v[30];
  COLOUR  rgb[9];

  track_2R_rear(&v[0], -tRearAng);
  track_2R_front(&v[0], tFrontAng);

  setpoint(&v[14], tFrontAng, -600, -600, 105);
  setpoint(&v[15], tFrontAng, -600, -500, 105);
  setpoint(&v[16], tFrontAng, -600, -100, 105);
  setpoint(&v[17], tFrontAng, -600, 0, 105);
  setpoint(&v[18], tFrontAng, -400, -600, 105);
  setpoint(&v[19], tFrontAng, -400, -500, 105);
  setpoint(&v[20], tFrontAng, -400, -100, 105);
  setpoint(&v[21], tFrontAng, -400, 0, 105);
  setpoint(&v[22], tFrontAng, -200, -600, 105);
  setpoint(&v[23], tFrontAng, -200, -500, 105);
  setpoint(&v[24], tFrontAng, -200, -100, 105);
  setpoint(&v[25], tFrontAng, -200, 0, 105);
  setpoint(&v[26], -tRearAng, -600, -600, -105);
  setpoint(&v[27], -tRearAng, -600, -500, -105);
  setpoint(&v[28], -tRearAng, -600, -100, -105);
  setpoint(&v[29], -tRearAng, -600, 0, -105);

  TMD_setCOLOUR(&rgb[0], 255, 255, 255);
  TMD_setCOLOUR(&rgb[1], 32, 34, 36);
  TMD_setCOLOUR(&rgb[2], 96, 80, 64);
  TMD_setCOLOUR(&rgb[3], 48, 40, 32);
  TMD_setCOLOUR(&rgb[4], 0, 128, 144);
  TMD_setCOLOUR(&rgb[5], 48, 96, 128);
  TMD_setCOLOUR(&rgb[6], 32, 64, 112);
  TMD_setCOLOUR(&rgb[7], 196, 196, 212);
  TMD_setCOLOUR(&rgb[8], 212, 212, 236);

  TMD_prepare(tMemType, &v[0], &rgb[0], 0);
  TMD_set4_NS_FP( 0, 7, 1, 8, 8, false);
  TMD_set4_NS_FP( 1, 8, 2, 9, 8, false);
  TMD_set4_NS_FP( 2, 9, 3, 10, 8, false);

  TMD_set4_NS_FP( 14, 26, 15, 27, 8, false);
  TMD_set4_NS_FP( 15, 27, 16, 28, 8, false);
  TMD_set4_NS_FP( 16, 28, 17, 29, 8, false);

	TMD_set4_NS_FP( 18, 14, 19, 15, 7, false);
	TMD_set4_NS_FP( 19, 15, 20, 16, 7, false);
	TMD_set4_NS_FP( 20, 16, 21, 17, 7, false);
	TMD_set4_NS_FP( 22, 18, 23, 19, 7, false);
	TMD_set4_NS_GP( 23, 19, 24, 20, 5, 4, 6, 5, false);
	TMD_set4_NS_FP( 24, 20, 25, 21, 7, false);
	TMD_set4_NS_FP( 7, 22, 8, 23, 7, false);
	TMD_set4_NS_FP( 8, 23, 9, 24, 7, false);
	TMD_set4_NS_FP( 9, 24, 10, 25, 7, false);

  TMD_set4_NS_GP( 3, 10, 4, 11, 2, 2, 3, 3, false);
  TMD_set4_NS_GP( 4, 11, 5, 12, 3, 3, 2, 2, false);
  TMD_set4_NS_GP( 5, 12, 6, 13, 0, 0, 1, 1, true);
  TMD_set4_NS_GP( 12, 5, 13, 6, 0, 0, 1, 1, true);
  TMD_START_TRK[tType] = TMD_finish();
	}

// ***********************************************************************************
// INDIVIDUAL TRACK DATA HERE
// ***********************************************************************************

/*
void create_TRK_4_BRIDGE()

void create_TRK_4_BRIDGE2()

void create_TRK_4_TUNNEL()

void create_TRK_4_TUNNELIN()

void create_TRK_4_TUNNELIN2()

void create_TRK_4_TUNNEL2()

void create_TRK_4_TUNNELOUT()

void create_TRK_4_TUNNELOUT2()
*/

void create_TRK_4_BRIDGE()
  {
  VERTEX  v[14];
  COLOUR  rgb[5];

  track_4_rear(&v[0], 0);
  track_4_front(&v[0], 0);

  TMD_setCOLOUR(&rgb[0], 192, 192, 255);
  TMD_setCOLOUR(&rgb[1], 48, 64, 128);

  TMD_prepare(0, &v[0], &rgb[0], 0);
  TMD_set4_NS_GP( 1, 8, 2, 9, 0, 1, 1, 0, true);
  TMD_set4_NS_GP( 2, 9, 3, 10, 1, 0, 0, 1, true);
  TMD_set4_NS_GP( 3, 10, 4, 11, 0, 1, 1, 0, true);
  TMD_set4_NS_GP( 4, 11, 5, 12, 1, 0, 0, 1, true);
  TMD_START_TRK[TRK_4_BRIDGE] = TMD_finish();
  }

void create_TRK_4_BRIDGE2()
  {
  VERTEX  v[14];
  COLOUR  rgb[5];

  track_4_rear(&v[0], 0);
  track_4_front(&v[0], 0);

  TMD_setCOLOUR(&rgb[0], 192, 192, 255);
  TMD_setCOLOUR(&rgb[1], 48, 64, 128);

  TMD_prepare(0, &v[0], &rgb[0], 0);
  TMD_set4_NS_GP( 1, 8, 2, 9, 1, 0, 0, 1, true);
  TMD_set4_NS_GP( 2, 9, 3, 10, 0, 1, 1, 0, true);
  TMD_set4_NS_GP( 3, 10, 4, 11, 1, 0, 0, 1, true);
  TMD_set4_NS_GP( 4, 11, 5, 12, 0, 1, 1, 0, true);
  TMD_START_TRK[TRK_4_BRIDGE2] = TMD_finish();
  }

void create_TRK_4_TUNNEL()
  {
  VERTEX  v[28];
  COLOUR  rgb[8];

  track_4_rear(&v[0], 0);
  track_4_front(&v[0], 0);

  TMD_setVERTEX(&v[14], -450, -300, -105);
  TMD_setVERTEX(&v[15], -400, -350, -105);
  TMD_setVERTEX(&v[16], -200, -400, -105);
  TMD_setVERTEX(&v[17], 0, -400, -105);
  TMD_setVERTEX(&v[18], 200, -400, -105);
  TMD_setVERTEX(&v[19], 400, -350, -105);
  TMD_setVERTEX(&v[20], 450, -300, -105);

  TMD_setVERTEX(&v[21], -450, -300, 105);
  TMD_setVERTEX(&v[22], -400, -350, 105);
  TMD_setVERTEX(&v[23], -200, -400, 105);
  TMD_setVERTEX(&v[24], 0, -400, 105);
  TMD_setVERTEX(&v[25], 200, -400, 105);
  TMD_setVERTEX(&v[26], 400, -350, 105);
  TMD_setVERTEX(&v[27], 450, -300, 105);

  TMD_setCOLOUR(&rgb[0], 160, 144, 64);
  TMD_setCOLOUR(&rgb[1], 48, 32, 16);
  TMD_setCOLOUR(&rgb[2], 64, 52, 24);
  TMD_setCOLOUR(&rgb[3], 32, 24, 12);
  TMD_setCOLOUR(&rgb[5], 255, 192, 32);
  TMD_setCOLOUR(&rgb[6], 206, 168, 80);
  TMD_setCOLOUR(&rgb[7], 255, 255, 32);

  TMD_prepare(0, &v[0], &rgb[0], 0);
  TMD_set4_NS_GP( 0, 7, 1, 8, 1, 1, 0, 0, false);
  TMD_set4_NS_GP( 1, 8, 2, 9, 2, 2, 3, 3, false);
  TMD_set4_NS_GP( 2, 9, 3, 10, 3, 3, 2, 2, false);
  TMD_set4_NS_GP( 3, 10, 4, 11, 2, 2, 3, 3, false);
  TMD_set4_NS_GP( 4, 11, 5, 12, 3, 3, 2, 2, false);
  TMD_set4_NS_GP( 5, 12, 6, 13, 0, 0, 1, 1, false);

  TMD_set4_NS_GP( 0, 14, 7, 21, 1, 6, 1, 6, false);
  TMD_set4_NS_FP( 14, 15, 21, 22, 7, false);
  TMD_set4_NS_GP( 15, 16, 22, 23, 6, 1, 6, 1, false);
  TMD_set4_NS_FP( 16, 17, 23, 24, 1, false);
  TMD_set4_NS_FP( 17, 18, 24, 25, 1, false);
  TMD_set4_NS_GP( 18, 19, 25, 26, 1, 6, 1, 6, false);
  TMD_set4_NS_FP( 19, 20, 26, 27, 7, false);
  TMD_set4_NS_GP( 20, 6, 27, 13, 6, 1, 6, 1, false);
  TMD_START_TRK[TRK_4_TUNNEL] = TMD_finish();
  }

void create_TRK_4_TUNNEL2()
  {
  VERTEX  v[28];
  COLOUR  rgb[8];

  track_4_rear(&v[0], 0);
  track_4_front(&v[0], 0);

  TMD_setVERTEX(&v[14], -450, -300, -105);
  TMD_setVERTEX(&v[15], -400, -350, -105);
  TMD_setVERTEX(&v[16], -200, -400, -105);
  TMD_setVERTEX(&v[17], 0, -400, -105);
  TMD_setVERTEX(&v[18], 200, -400, -105);
  TMD_setVERTEX(&v[19], 400, -350, -105);
  TMD_setVERTEX(&v[20], 450, -300, -105);

  TMD_setVERTEX(&v[21], -450, -300, 105);
  TMD_setVERTEX(&v[22], -400, -350, 105);
  TMD_setVERTEX(&v[23], -200, -400, 105);
  TMD_setVERTEX(&v[24], 0, -400, 105);
  TMD_setVERTEX(&v[25], 200, -400, 105);
  TMD_setVERTEX(&v[26], 400, -350, 105);
  TMD_setVERTEX(&v[27], 450, -300, 105);

  TMD_setCOLOUR(&rgb[0], 160, 144, 64);
  TMD_setCOLOUR(&rgb[1], 48, 32, 16);
  TMD_setCOLOUR(&rgb[2], 64, 52, 24);
  TMD_setCOLOUR(&rgb[3], 32, 24, 12);
  TMD_setCOLOUR(&rgb[5], 255, 192, 32);
  TMD_setCOLOUR(&rgb[6], 206, 168, 80);
  TMD_setCOLOUR(&rgb[7], 255, 255, 32);

  TMD_prepare(0, &v[0], &rgb[0], 0);
  TMD_set4_NS_GP( 0, 7, 1, 8, 1, 1, 0, 0, false);
  TMD_set4_NS_GP( 1, 8, 2, 9, 2, 2, 3, 3, false);
  TMD_set4_NS_GP( 2, 9, 3, 10, 3, 3, 2, 2, false);
  TMD_set4_NS_GP( 3, 10, 4, 11, 2, 2, 3, 3, false);
  TMD_set4_NS_GP( 4, 11, 5, 12, 3, 3, 2, 2, false);
  TMD_set4_NS_GP( 5, 12, 6, 13, 0, 0, 1, 1, false);

  TMD_set4_NS_GP( 0, 14, 7, 21, 1, 6, 1, 6, false);
  TMD_set4_NS_FP( 14, 15, 21, 22, 6, false);
  TMD_set4_NS_GP( 15, 16, 22, 23, 6, 1, 6, 1, false);
  TMD_set4_NS_FP( 16, 17, 23, 24, 1, false);
  TMD_set4_NS_FP( 17, 18, 24, 25, 1, false);
  TMD_set4_NS_GP( 18, 19, 25, 26, 1, 6, 1, 6, false);
  TMD_set4_NS_FP( 19, 20, 26, 27, 6, false);
  TMD_set4_NS_GP( 20, 6, 27, 13, 6, 1, 6, 1, false);
  TMD_START_TRK[TRK_4_TUNNEL2] = TMD_finish();
  }

void create_TRK_4_TUNNELIN()
  {
  VERTEX  v[28];
  COLOUR  rgb[11];

  track_4_rear(&v[0], 0);
  track_4_front(&v[0], 0);

  TMD_setVERTEX(&v[14], -450, -300, -105);
  TMD_setVERTEX(&v[15], -400, -350, -105);
  TMD_setVERTEX(&v[16], -200, -400, -105);
  TMD_setVERTEX(&v[17], 0, -400, -105);
  TMD_setVERTEX(&v[18], 200, -400, -105);
  TMD_setVERTEX(&v[19], 400, -350, -105);
  TMD_setVERTEX(&v[20], 450, -300, -105);

  TMD_setVERTEX(&v[21], -450, -300, 105);
  TMD_setVERTEX(&v[22], -400, -350, 105);
  TMD_setVERTEX(&v[23], -200, -400, 105);
  TMD_setVERTEX(&v[24], 0, -400, 105);
  TMD_setVERTEX(&v[25], 200, -400, 105);
  TMD_setVERTEX(&v[26], 400, -350, 105);
  TMD_setVERTEX(&v[27], 450, -300, 105);

  TMD_setCOLOUR(&rgb[0], 160, 144, 64);
  TMD_setCOLOUR(&rgb[1], 48, 32, 16);
  TMD_setCOLOUR(&rgb[2], 64, 52, 24);
  TMD_setCOLOUR(&rgb[3], 32, 24, 12);
  TMD_setCOLOUR(&rgb[5], 255, 192, 32);
  TMD_setCOLOUR(&rgb[6], 206, 168, 80);

  TMD_setCOLOUR(&rgb[7], 96, 80, 64);
  TMD_setCOLOUR(&rgb[8], 48, 40, 32);
  TMD_setCOLOUR(&rgb[9], 128, 128, 128);
  TMD_setCOLOUR(&rgb[10], 32, 32, 32);

  TMD_prepare(0, &v[0], &rgb[0], 0);
  TMD_set4_NS_GP( 1, 0, 8, 7, 9, 10, 9, 1, false);

  TMD_set4_NS_GP( 1, 8, 2, 9, 7, 7, 8, 8, false);
  TMD_set4_NS_GP( 2, 9, 3, 10, 8, 8, 7, 7, false);
  TMD_set4_NS_GP( 3, 10, 4, 11, 7, 7, 8, 8, false);
  TMD_set4_NS_GP( 4, 11, 5, 12, 8, 8, 7, 7, false);

  TMD_set4_NS_GP( 5, 12, 6, 13, 9, 9, 10, 1, false);

  TMD_set4_NS_GP( 0, 14, 7, 21, 10, 6, 1, 6, false);
  TMD_set4_NS_FP( 14, 15, 21, 22, 6, false);
  TMD_set4_NS_GP( 15, 16, 22, 23, 6, 1, 6, 1, false);
  TMD_set4_NS_FP( 16, 17, 23, 24, 1, false);
  TMD_set4_NS_FP( 17, 18, 24, 25, 1, false);
  TMD_set4_NS_GP( 18, 19, 25, 26, 1, 6, 1, 6, false);
  TMD_set4_NS_FP( 19, 20, 26, 27, 6, false);
  TMD_set4_NS_GP( 20, 6, 27, 13, 6, 10, 6, 1, false);

  TMD_START_TRK[TRK_4_TUNNELIN] = TMD_finish();
  }

void create_TRK_4_TUNNELIN2()
  {
  VERTEX  v[28];
  COLOUR  rgb[11];

  track_4_rear(&v[0], 0);
  track_4_front(&v[0], 0);

  TMD_setVERTEX(&v[14], -450, -300, -105);
  TMD_setVERTEX(&v[15], -400, -350, -105);
  TMD_setVERTEX(&v[16], -200, -400, -105);
  TMD_setVERTEX(&v[17], 0, -400, -105);
  TMD_setVERTEX(&v[18], 200, -400, -105);
  TMD_setVERTEX(&v[19], 400, -350, -105);
  TMD_setVERTEX(&v[20], 450, -300, -105);

  TMD_setVERTEX(&v[21], -450, -300, 105);
  TMD_setVERTEX(&v[22], -400, -350, 105);
  TMD_setVERTEX(&v[23], -200, -400, 105);
  TMD_setVERTEX(&v[24], 0, -400, 105);
  TMD_setVERTEX(&v[25], 200, -400, 105);
  TMD_setVERTEX(&v[26], 400, -350, 105);
  TMD_setVERTEX(&v[27], 450, -300, 105);

  TMD_setCOLOUR(&rgb[0], 160, 144, 64);
  TMD_setCOLOUR(&rgb[1], 48, 32, 16);
  TMD_setCOLOUR(&rgb[2], 64, 52, 24);
  TMD_setCOLOUR(&rgb[3], 32, 24, 12);
  TMD_setCOLOUR(&rgb[5], 255, 192, 32);
  TMD_setCOLOUR(&rgb[6], 206, 168, 80);

  TMD_setCOLOUR(&rgb[7], 96, 80, 64);
  TMD_setCOLOUR(&rgb[8], 48, 40, 32);
  TMD_setCOLOUR(&rgb[9], 128, 128, 128);
  TMD_setCOLOUR(&rgb[10], 32, 32, 32);

  TMD_prepare(0, &v[0], &rgb[0], 0);
  TMD_set4_NS_GP( 1, 0, 8, 7, 9, 1, 0, 1, false);

  TMD_set4_NS_GP( 1, 8, 2, 9, 7, 2, 8, 3, false);
  TMD_set4_NS_GP( 2, 9, 3, 10, 8, 3, 7, 2, false);
  TMD_set4_NS_GP( 3, 10, 4, 11, 7, 2, 8, 3, false);
  TMD_set4_NS_GP( 4, 11, 5, 12, 8, 3, 7, 2, false);

  TMD_set4_NS_GP( 5, 12, 6, 13, 9, 0, 1, 1, false);

  TMD_set4_NS_GP( 0, 14, 7, 21, 1, 6, 1, 6, false);
  TMD_set4_NS_FP( 14, 15, 21, 22, 6, false);
  TMD_set4_NS_GP( 15, 16, 22, 23, 6, 1, 6, 1, false);
  TMD_set4_NS_FP( 16, 17, 23, 24, 1, false);
  TMD_set4_NS_FP( 17, 18, 24, 25, 1, false);
  TMD_set4_NS_GP( 18, 19, 25, 26, 1, 6, 1, 6, false);
  TMD_set4_NS_FP( 19, 20, 26, 27, 6, false);
  TMD_set4_NS_GP( 20, 6, 27, 13, 6, 1, 6, 1, false);

  TMD_START_TRK[TRK_4_TUNNELIN2] = TMD_finish();
  }

void create_TRK_4_TUNNELOUT()
  {
  VERTEX  v[28];
  COLOUR  rgb[11];

  track_4_rear(&v[0], 0);
  track_4_front(&v[0], 0);

  TMD_setVERTEX(&v[14], -450, -300, -105);
  TMD_setVERTEX(&v[15], -400, -350, -105);
  TMD_setVERTEX(&v[16], -200, -400, -105);
  TMD_setVERTEX(&v[17], 0, -400, -105);
  TMD_setVERTEX(&v[18], 200, -400, -105);
  TMD_setVERTEX(&v[19], 400, -350, -105);
  TMD_setVERTEX(&v[20], 450, -300, -105);

  TMD_setVERTEX(&v[21], -450, -300, 105);
  TMD_setVERTEX(&v[22], -400, -350, 105);
  TMD_setVERTEX(&v[23], -200, -400, 105);
  TMD_setVERTEX(&v[24], 0, -400, 105);
  TMD_setVERTEX(&v[25], 200, -400, 105);
  TMD_setVERTEX(&v[26], 400, -350, 105);
  TMD_setVERTEX(&v[27], 450, -300, 105);

  TMD_setCOLOUR(&rgb[0], 160, 144, 64);
  TMD_setCOLOUR(&rgb[1], 48, 32, 16);
  TMD_setCOLOUR(&rgb[2], 64, 52, 24);
  TMD_setCOLOUR(&rgb[3], 32, 24, 12);
  TMD_setCOLOUR(&rgb[5], 255, 192, 32);
  TMD_setCOLOUR(&rgb[6], 206, 168, 80);

  TMD_setCOLOUR(&rgb[7], 96, 80, 64);
  TMD_setCOLOUR(&rgb[8], 48, 40, 32);
  TMD_setCOLOUR(&rgb[9], 128, 128, 128);
  TMD_setCOLOUR(&rgb[10], 32, 32, 32);

  TMD_prepare(0, &v[0], &rgb[0], 0);
  TMD_set4_NS_GP( 0, 7, 1, 8, 1, 1, 0, 9, false);

  TMD_set4_NS_GP( 8, 9, 1, 2, 7, 8, 2, 3, false);
  TMD_set4_NS_GP( 2, 9, 3, 10, 3, 8, 2, 7, false);
  TMD_set4_NS_GP( 10, 11, 3, 4, 7, 8, 2, 3, false);
  TMD_set4_NS_GP( 4, 11, 5, 12, 3, 8, 2, 7, false);

  TMD_set4_NS_GP( 6, 5, 13, 12, 1, 0, 1, 9, false);

  TMD_set4_NS_GP( 0, 14, 7, 21, 1, 6, 1, 6, false);
  TMD_set4_NS_FP( 14, 15, 21, 22, 6, false);
  TMD_set4_NS_GP( 15, 16, 22, 23, 6, 1, 6, 1, false);
  TMD_set4_NS_FP( 16, 17, 23, 24, 1, false);
  TMD_set4_NS_FP( 17, 18, 24, 25, 1, false);
  TMD_set4_NS_GP( 18, 19, 25, 26, 1, 6, 1, 6, false);
  TMD_set4_NS_FP( 19, 20, 26, 27, 6, false);
  TMD_set4_NS_GP( 20, 6, 27, 13, 6, 1, 6, 1, false);

  TMD_START_TRK[TRK_4_TUNNELOUT] = TMD_finish();
  }

void create_TRK_4_TUNNELOUT2()
  {
  VERTEX  v[28];
  COLOUR  rgb[11];

  track_4_rear(&v[0], 0);
  track_4_front(&v[0], 0);

  TMD_setVERTEX(&v[14], -450, -300, -105);
  TMD_setVERTEX(&v[15], -400, -350, -105);
  TMD_setVERTEX(&v[16], -200, -400, -105);
  TMD_setVERTEX(&v[17], 0, -400, -105);
  TMD_setVERTEX(&v[18], 200, -400, -105);
  TMD_setVERTEX(&v[19], 400, -350, -105);
  TMD_setVERTEX(&v[20], 450, -300, -105);

  TMD_setVERTEX(&v[21], -450, -300, 105);
  TMD_setVERTEX(&v[22], -400, -350, 105);
  TMD_setVERTEX(&v[23], -200, -400, 105);
  TMD_setVERTEX(&v[24], 0, -400, 105);
  TMD_setVERTEX(&v[25], 200, -400, 105);
  TMD_setVERTEX(&v[26], 400, -350, 105);
  TMD_setVERTEX(&v[27], 450, -300, 105);

  TMD_setCOLOUR(&rgb[0], 160, 144, 64);
  TMD_setCOLOUR(&rgb[1], 48, 32, 16);
  TMD_setCOLOUR(&rgb[2], 64, 52, 24);
  TMD_setCOLOUR(&rgb[3], 32, 24, 12);
  TMD_setCOLOUR(&rgb[5], 255, 192, 32);
  TMD_setCOLOUR(&rgb[6], 206, 168, 80);

  TMD_setCOLOUR(&rgb[7], 96, 80, 64);
  TMD_setCOLOUR(&rgb[8], 48, 40, 32);
  TMD_setCOLOUR(&rgb[9], 128, 128, 128);
  TMD_setCOLOUR(&rgb[10], 32, 32, 32);

  TMD_prepare(0, &v[0], &rgb[0], 0);
  TMD_set4_NS_GP( 0, 7, 1, 8, 1, 10, 9, 9, false);

  TMD_set4_NS_GP( 1, 8, 2, 9, 7, 7, 8, 8, false);
  TMD_set4_NS_GP( 2, 9, 3, 10, 8, 8, 7, 7, false);
  TMD_set4_NS_GP( 3, 10, 4, 11, 7, 7, 8, 8, false);
  TMD_set4_NS_GP( 4, 11, 5, 12, 8, 8, 7, 7, false);

  TMD_set4_NS_GP( 12, 13, 5, 6, 9, 10, 9, 1, false);

  TMD_set4_NS_GP( 14, 21, 0, 7, 6, 6, 1, 10, false);
  TMD_set4_NS_FP( 14, 15, 21, 22, 6, false);
  TMD_set4_NS_GP( 15, 16, 22, 23, 6, 1, 6, 1, false);
  TMD_set4_NS_FP( 16, 17, 23, 24, 1, false);
  TMD_set4_NS_FP( 17, 18, 24, 25, 1, false);
  TMD_set4_NS_GP( 18, 19, 25, 26, 1, 6, 1, 6, false);
  TMD_set4_NS_FP( 19, 20, 26, 27, 6, false);
  TMD_set4_NS_GP( 20, 6, 27, 13, 6, 1, 6, 10, false);

  TMD_START_TRK[TRK_4_TUNNELOUT2] = TMD_finish();
  }
