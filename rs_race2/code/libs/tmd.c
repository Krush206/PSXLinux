// ***********************************************************************************
// Programs written by R.Swan - rs108@mdx.ac.uk - www.netyaroze-europe.com/~middex2
// Dynamic TMD creation main file
//   Edited from code that is Copyright (C) Sony Computer Entertainment.
//   All rights reserved. Original author S. Ashley 7th Aug 97
// ***********************************************************************************

#include "tmd.h"

// **** Global #defines and variables
static u_long *TMD_MEM_START;
static u_long *TMD_MEM_CURRENT;
OBJECT        *TMD_MEM_OBJECT;

static u_long *TMD_VERTEX_LIST_START;
static u_long *TMD_NORMAL_LIST_START;
static u_long *TMD_PRIMITIVE_LIST_START;
static u_long TMD_VERTEX_QUANTITY;
static u_long TMD_NORMAL_QUANTITY;
static u_long TMD_PRIMITIVE_QUANTITY;

VERTEX        *VERTEX_LUT;
COLOUR        *COLOUR_LUT;
TEXTURE       *TEXTURE_LUT;

// ***********************************************************************************
// Function definitions
// ***********************************************************************************

// **** set up required variables ready for tmd creation
void TMD_prepare(u_long tMemAddress, VERTEX *tVertex, COLOUR *tColour, TEXTURE *tTexture)
  {
  VERTEX_LUT = tVertex;
  COLOUR_LUT = tColour;
  TEXTURE_LUT = tTexture;

  if (tMemAddress != 0) TMD_MEM_START = TMD_MEM_CURRENT = (u_long *) tMemAddress;
  else TMD_MEM_START = TMD_MEM_CURRENT;

  TMD_VERTEX_QUANTITY = TMD_NORMAL_QUANTITY = TMD_PRIMITIVE_QUANTITY = 0;
  sws_setHEADER((HEADER *) TMD_MEM_CURRENT);
  TMD_MEM_CURRENT = (u_long *) ((HEADER *) TMD_MEM_CURRENT + 1);
  TMD_MEM_OBJECT = (OBJECT *) TMD_MEM_CURRENT;
  TMD_MEM_CURRENT = (u_long *) ((OBJECT *) TMD_MEM_CURRENT + 1);
  TMD_PRIMITIVE_LIST_START = TMD_MEM_CURRENT;
  }

// **** end tmd file format by setting up remaining structures and variables
u_long *TMD_finish()
  {
  u_long tTemp;
  VERTEX *tMemAddress;

  TMD_VERTEX_QUANTITY++;
  TMD_VERTEX_LIST_START = TMD_MEM_CURRENT;
  tMemAddress = (VERTEX *) TMD_VERTEX_LIST_START;
  TMD_NORMAL_LIST_START = (u_long *) ((VERTEX *) TMD_VERTEX_LIST_START + TMD_VERTEX_QUANTITY);

  sws_setOBJECT(TMD_MEM_OBJECT);

  for (tTemp=0; tTemp<TMD_VERTEX_QUANTITY; tTemp++)
    {
    tMemAddress->vx = (VERTEX_LUT + tTemp)->vx;
    tMemAddress->vy = (VERTEX_LUT + tTemp)->vy;
    tMemAddress->vz = (VERTEX_LUT + tTemp)->vz;
    tMemAddress++;
    }

  TMD_MEM_CURRENT = (u_long *) ((NORMAL *) TMD_NORMAL_LIST_START + TMD_NORMAL_QUANTITY);

//  printf("\nNumber of VERTICES - %d, ", (int) TMD_VERTEX_QUANTITY);
//  printf("Number of NORMALS - %d, ", (int) TMD_NORMAL_QUANTITY);
//  printf("Number of PRIMITIVES - %d", (int) TMD_PRIMITIVE_QUANTITY);

  return TMD_MEM_START;
  }

// **** Create 3 sided, no shading, flat pigment polygon
void TMD_set3_NS_FP(u_short v0, u_short v1, u_short v2, 
                    u_long c0,
                    u_long tTransparency)
  {
  FACE_3_NS_FP *tPoly = (FACE_3_NS_FP *) TMD_MEM_CURRENT;

  sws_setPRIMITIVE_HEADER(tPoly, TYPE_3_NS_FP + (tTransparency<<1), 1, 3, 4);
  tPoly->mode2 = TYPE_3_NS_FP + (tTransparency<<1);
  tPoly->vert0 = v0;
  tPoly->vert1 = v1;
  tPoly->vert2 = v2;
  sws_setCOLOUR0();
  if (v0>TMD_VERTEX_QUANTITY) TMD_VERTEX_QUANTITY = v0;
  if (v1>TMD_VERTEX_QUANTITY) TMD_VERTEX_QUANTITY = v1;
  if (v2>TMD_VERTEX_QUANTITY) TMD_VERTEX_QUANTITY = v2;
  TMD_PRIMITIVE_QUANTITY++;

  TMD_MEM_CURRENT = (u_long *) (tPoly + 1);
  }

// **** Create 3 sided, no shading, gradient pigment polygon
void TMD_set3_NS_GP(u_short v0, u_short v1, u_short v2, 
                    u_long c0, u_long c1, u_long c2,
                    u_long tTransparency)
  {
  FACE_3_NS_GP *tPoly = (FACE_3_NS_GP *) TMD_MEM_CURRENT;

  sws_setPRIMITIVE_HEADER(tPoly, TYPE_3_NS_GP + (tTransparency<<1), 1, 5, 6);
  tPoly->mode2 = TYPE_3_NS_GP + (tTransparency<<1);
  tPoly->vert0 = v0;
  tPoly->vert1 = v1;
  tPoly->vert2 = v2;
  sws_setCOLOUR0();
  sws_setCOLOUR1();
  sws_setCOLOUR2();
  if (v0>TMD_VERTEX_QUANTITY) TMD_VERTEX_QUANTITY = v0;
  if (v1>TMD_VERTEX_QUANTITY) TMD_VERTEX_QUANTITY = v1;
  if (v2>TMD_VERTEX_QUANTITY) TMD_VERTEX_QUANTITY = v2;
  TMD_PRIMITIVE_QUANTITY++;

  TMD_MEM_CURRENT = (u_long *) (tPoly + 1);
  }

// **** Create 4 sided, no shading, flat pigment polygon
void TMD_set4_NS_FP(u_short v0, u_short v1, u_short v2, u_short v3,
                    u_long c0,
                    u_long tTransparency)
  {
  FACE_4_NS_FP *tPoly = (FACE_4_NS_FP *) TMD_MEM_CURRENT;

  sws_setPRIMITIVE_HEADER(tPoly, TYPE_4_NS_FP + (tTransparency<<1), 1, 3, 5);
  tPoly->mode2 = TYPE_4_NS_FP + (tTransparency<<1);
  tPoly->vert0 = v0;
  tPoly->vert1 = v1;
  tPoly->vert2 = v2;
  tPoly->vert3 = v3;
  sws_setCOLOUR0();
  if (v0>TMD_VERTEX_QUANTITY) TMD_VERTEX_QUANTITY = v0;
  if (v1>TMD_VERTEX_QUANTITY) TMD_VERTEX_QUANTITY = v1;
  if (v2>TMD_VERTEX_QUANTITY) TMD_VERTEX_QUANTITY = v2;
  if (v3>TMD_VERTEX_QUANTITY) TMD_VERTEX_QUANTITY = v3;
  TMD_PRIMITIVE_QUANTITY++;

  TMD_MEM_CURRENT = (u_long *) (tPoly + 1);
  }

// **** Create 4 sided, no shading, gradient pigment polygon
void TMD_set4_NS_GP(u_short v0, u_short v1, u_short v2, u_short v3,
                    u_long c0, u_long c1, u_long c2, u_long c3,
                    u_long tTransparency)
  {
  FACE_4_NS_GP *tPoly = (FACE_4_NS_GP *) TMD_MEM_CURRENT;

  sws_setPRIMITIVE_HEADER(tPoly, TYPE_4_NS_GP + (tTransparency<<1), 1, 6, 8);
  tPoly->mode2 = TYPE_4_NS_GP + (tTransparency<<1);
  tPoly->vert0 = v0;
  tPoly->vert1 = v1;
  tPoly->vert2 = v2;
  tPoly->vert3 = v3;
  sws_setCOLOUR0();
  sws_setCOLOUR1();
  sws_setCOLOUR2();
  sws_setCOLOUR3();
  if (v0>TMD_VERTEX_QUANTITY) TMD_VERTEX_QUANTITY = v0;
  if (v1>TMD_VERTEX_QUANTITY) TMD_VERTEX_QUANTITY = v1;
  if (v2>TMD_VERTEX_QUANTITY) TMD_VERTEX_QUANTITY = v2;
  if (v3>TMD_VERTEX_QUANTITY) TMD_VERTEX_QUANTITY = v3;
  TMD_PRIMITIVE_QUANTITY++;

  TMD_MEM_CURRENT = (u_long *) (tPoly + 1);
  }

// **** Create 4 sided, textured, no shading, flat pigment polygon
void TMD_set4_TX_NS_FP(u_short v0, u_short v1, u_short v2, u_short v3,
                       u_long c0,
                       u_long t0)
  {
  FACE_4_TX_NS_FP *tPoly = (FACE_4_TX_NS_FP *) TMD_MEM_CURRENT;

  sws_setPRIMITIVE_HEADER(tPoly, TYPE_4_TX_NS_FP, 1, 7, 9);
  tPoly->vert0 = v0;
  tPoly->vert1 = v1;
  tPoly->vert2 = v2;
  tPoly->vert3 = v3;
  sws_setCOLOUR0();
  sws_setTEXTURE4((FACE_4_TX_NS_FP *) tPoly, t0);

  if (v0>TMD_VERTEX_QUANTITY) TMD_VERTEX_QUANTITY = v0;
  if (v1>TMD_VERTEX_QUANTITY) TMD_VERTEX_QUANTITY = v1;
  if (v2>TMD_VERTEX_QUANTITY) TMD_VERTEX_QUANTITY = v2;
  if (v3>TMD_VERTEX_QUANTITY) TMD_VERTEX_QUANTITY = v3;
  TMD_PRIMITIVE_QUANTITY++;

  TMD_MEM_CURRENT = (u_long *) (tPoly + 1);
  }

// **** Create 4 sided, textured, no shading, gradient pigment polygon
void TMD_set4_TX_NS_GP(u_short v0, u_short v1, u_short v2, u_short v3,
                       u_long c0, u_long c1, u_long c2, u_long c3,
                       u_long t0, u_long tTransparency)
  {
  FACE_4_TX_NS_GP *tPoly = (FACE_4_TX_NS_GP *) TMD_MEM_CURRENT;

  sws_setPRIMITIVE_HEADER(tPoly, TYPE_4_TX_NS_GP + (tTransparency<<1), 1, 10, 12);
  tPoly->vert0 = v0;
  tPoly->vert1 = v1;
  tPoly->vert2 = v2;
  tPoly->vert3 = v3;
  sws_setCOLOUR0();
  sws_setCOLOUR1();
  sws_setCOLOUR2();
  sws_setCOLOUR3();
  sws_setTEXTURE4((FACE_4_TX_NS_FP *) tPoly, t0);

  if (v0>TMD_VERTEX_QUANTITY) TMD_VERTEX_QUANTITY = v0;
  if (v1>TMD_VERTEX_QUANTITY) TMD_VERTEX_QUANTITY = v1;
  if (v2>TMD_VERTEX_QUANTITY) TMD_VERTEX_QUANTITY = v2;
  if (v3>TMD_VERTEX_QUANTITY) TMD_VERTEX_QUANTITY = v3;
  TMD_PRIMITIVE_QUANTITY++;

  TMD_MEM_CURRENT = (u_long *) (tPoly + 1);
  }

// **** Set TSB (information about a texture on a polygon
void sws_setTEXTURE4(FACE_4_TX_NS_FP *tPoly, u_long tTextureNum)
  {
  TEXTURE *tTexture = TEXTURE_LUT + tTextureNum;
  u_short tTexturePage;
  int tXOffset = 1028, tYOffset = 512;

  if (tXOffset > (tTexture->x0)) tXOffset = tTexture->x0;
  if (tXOffset > (tTexture->x1)) tXOffset = tTexture->x1;
  if (tXOffset > (tTexture->x2)) tXOffset = tTexture->x2;
  if (tXOffset > (tTexture->x3)) tXOffset = tTexture->x3;
  if (tYOffset > (tTexture->y0)) tYOffset = tTexture->y0;
  if (tYOffset > (tTexture->y1)) tYOffset = tTexture->y1;
  if (tYOffset > (tTexture->y2)) tYOffset = tTexture->y2;
  if (tYOffset > (tTexture->y3)) tYOffset = tTexture->y3;

  tXOffset = tXOffset/64*64;
  tYOffset = tYOffset/256*256;
  tTexturePage = GetTPage((int) tTexture->cq, 0, tXOffset, tYOffset);

  tPoly->u0 = (u_char) (tTexture->x0 - tXOffset);
  tPoly->v0 = (u_char) (tTexture->y0 - tYOffset);
  tPoly->u1 = (u_char) (tTexture->x1 - tXOffset);
  tPoly->v1 = (u_char) (tTexture->y1 - tYOffset);
  tPoly->u2 = (u_char) (tTexture->x2 - tXOffset);
  tPoly->v2 = (u_char) (tTexture->y2 - tYOffset);
  tPoly->u3 = (u_char) (tTexture->x3 - tXOffset);
  tPoly->v3 = (u_char) (tTexture->y3 - tYOffset);

  tPoly->tsb = (u_short) ((tTexturePage));// + (0<<5) + ((u_short) (tTexture->cq)<<6));
  tPoly->cba = ((tTexture->cx)>>4) + ((tTexture->cy)<<6);
  }
