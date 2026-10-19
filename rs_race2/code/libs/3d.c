// ***********************************************************************************
// Programs written by R.Swan - rs108@mdx.ac.uk - www.netyaroze-europe.com/~middex2
// Three d model definitions file
// ***********************************************************************************

#include "libps_bridge.h"
#include "screen.h"
#include "3d.h"

// ***********************************************************************************
// Function definitions
// ***********************************************************************************

// **** Set up a model
void SetModelInfo(Model *tModel, long tX, long tY, long tZ, u_long *tMemAddress)
  {
  tMemAddress++;
  GsMapModelingData(tMemAddress);

  GsInitCoordinate2(WORLD, &tModel->Object_Coord);
  tMemAddress += 2;
  GsLinkObject4((u_long) tMemAddress, &tModel->Object_Handler, 0);

  tModel->Object_Handler.coord2 = &tModel->Object_Coord;
  tModel->Object_Coord.coord.t[0] = tX;
  tModel->Object_Coord.coord.t[1] = tY;
  tModel->Object_Coord.coord.t[2] = tZ;
  tModel->Object_Coord.flg = 0;

  tModel->Orientation.vx = tModel->Orientation.vy = tModel->Orientation.vz = 0;
  tModel->Subdivision = 0;
  }

// **** Set viewpoint for scene
void SetViewPoint(long tProjDist, long tRz,
      long tFromX, long tFromY, long tFromZ,
      long tToX, long tToY, long tToZ, GsCOORDINATE2 *tReference)
  {
  GsSetProjection(tProjDist);
  ViewPoint.vpx = tFromX;
  ViewPoint.vpy = tFromY;
  ViewPoint.vpz = tFromZ;
  ViewPoint.vrx = tToX;
  ViewPoint.vry = tToY;
  ViewPoint.vrz = tToZ;
  ViewPoint.rz = -tRz;

  if (tReference == 0) ViewPoint.super = WORLD;
  else ViewPoint.super = tReference;

	ProjectionDistance = tProjDist;

  GsSetRefView2(&ViewPoint);
  }

// **** Draw the model
void DrawModel(Model *tModel, long tBitShift)
  {
  MATRIX    tLocalWorld, tLocalScreen;
  GsGetLws(tModel->Object_Handler.coord2, &tLocalWorld, &tLocalScreen);
  GsSetLightMatrix(&tLocalWorld);
  GsSetLsMatrix(&tLocalScreen);
  GsSortObject4(&tModel->Object_Handler,
    &OTable_Header[CurrentBuffer], 16-OTABLE_LENGTH+tBitShift,
    (u_long*)getScratchAddr(0));
  }

// **** Rotate Model
void RotateModel(Model *tModel, long tXr, long tYr, long tZr)
  {
  MATRIX    tMatrix;

  tModel->Object_Coord.coord.m[0][0] =
    tModel->Object_Coord.coord.m[1][1] =
    tModel->Object_Coord.coord.m[2][2] = ONE;
  tModel->Object_Coord.coord.m[0][1] =
    tModel->Object_Coord.coord.m[0][2] =
    tModel->Object_Coord.coord.m[1][0] =
    tModel->Object_Coord.coord.m[1][2] =
    tModel->Object_Coord.coord.m[2][0] =
    tModel->Object_Coord.coord.m[2][1] = 0;

  tModel->Orientation.vx = (tModel->Orientation.vx + tXr) % ONE;
  tModel->Orientation.vy = (tModel->Orientation.vy + tYr) % ONE;
  tModel->Orientation.vz = (tModel->Orientation.vz + tZr) % ONE;

  RotMatrix(&tModel->Orientation, &tMatrix);
  MulMatrix0(&tModel->Object_Coord.coord, &tMatrix, &tModel->Object_Coord.coord);
  tModel->Object_Coord.flg = 0;
  }

// **** Translate an object according to the direction it is facing
void MoveModelForward(Model *tModel, long tX, long tY, long tZ)
  {
  MATRIX tMatrix;                                     // temporary rotation matrix
  SVECTOR tOneUnitForward, tDirectionResult;          // two vectors for rotation

  tOneUnitForward.vx = tX;                           // to move forward = no x translation
  tOneUnitForward.vy = tY;                           // to move forward = no y translation
  tOneUnitForward.vz = tZ;                         // to move forward = 4096 z translation
  RotMatrix(&tModel->Orientation, &tMatrix);           // turn vector into rotation matrix
  ApplyMatrixSV(&tMatrix, &tOneUnitForward, &tDirectionResult);
                                                      // multiply our direction vector with
                                                      // the rotation matrix and give the
                                                      // result as another direction vector
  tModel->Object_Coord.coord.t[0] += tDirectionResult.vx;
  tModel->Object_Coord.coord.t[1] += tDirectionResult.vy;
  tModel->Object_Coord.coord.t[2] += tDirectionResult.vz;
  tModel->Object_Coord.flg = 0;
  }

