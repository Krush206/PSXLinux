// ***********************************************************************************
// Programs written by R.Swan - rs108@mdx.ac.uk - www.netyaroze-europe.com/~middex2
// Dynamic TMD creation header
//   Edited from code that is Copyright (C) Sony Computer Entertainment.
//   All rights reserved. Original author S. Ashley 7th Aug 97
// ***********************************************************************************

#include "libps_bridge.h"

// **** #defines
#define TYPE_3_NS_FP				(0x21)
#define TYPE_3_NS_GP				(0x31)

#define TYPE_4_NS_FP        (0x29)
#define TYPE_4_NS_GP        (0x39)
#define TYPE_4_TX_NS_FP     (0x2d)
#define TYPE_4_TX_NS_GP     (0x3d)

#define sws_setHEADER(h) \
  (h)->id = 0x00000041, \
  (h)->flag = 1, \
  (h)->nobj = 1

#define sws_setOBJECT(o) \
  (o)->vert_top = TMD_VERTEX_LIST_START, \
  (o)->n_vert = TMD_VERTEX_QUANTITY, \
  (o)->normal_top = TMD_NORMAL_LIST_START, \
  (o)->n_normal = TMD_NORMAL_QUANTITY, \
  (o)->primitive_top = TMD_PRIMITIVE_LIST_START, \
  (o)->n_primitive = TMD_PRIMITIVE_QUANTITY, \
  (o)->scale = 0

#define sws_setPRIMITIVE_HEADER(p,m,f,i,o) \
  (p)->mode = (m), \
  (p)->flag = (f), \
  (p)->ilen = (i), \
  (p)->olen = (o)

#define sws_setCOLOUR0() \
  tPoly->r0 = (COLOUR_LUT+c0)->r, \
  tPoly->g0 = (COLOUR_LUT+c0)->g, \
  tPoly->b0 = (COLOUR_LUT+c0)->b
#define sws_setCOLOUR1() \
  tPoly->r1 = (COLOUR_LUT+c1)->r, \
  tPoly->g1 = (COLOUR_LUT+c1)->g, \
  tPoly->b1 = (COLOUR_LUT+c1)->b
#define sws_setCOLOUR2() \
  tPoly->r2 = (COLOUR_LUT+c2)->r, \
  tPoly->g2 = (COLOUR_LUT+c2)->g, \
  tPoly->b2 = (COLOUR_LUT+c2)->b
#define sws_setCOLOUR3() \
  tPoly->r3 = (COLOUR_LUT+c3)->r, \
  tPoly->g3 = (COLOUR_LUT+c3)->g, \
  tPoly->b3 = (COLOUR_LUT+c3)->b

#define TMD_setVERTEX(v,x,y,z) \
  (v)->vx = (x), \
  (v)->vy = (y), \
  (v)->vz = (z)

#define TMD_setNORMAL(v,x,y,z) \
  (v)->nx = (x), \
  (v)->ny = (y), \
  (v)->nz = (z)

#define TMD_setCOLOUR(c,rc,gc,bc) \
  (c)->r = (rc), \
  (c)->g = (gc), \
  (c)->b = (bc)

#define TMD_setTEXTURE(t,X0,Y0,X1,Y1,X2,Y2,X3,Y3,Cq,Cx,Cy) \
  (t)->x0 = (X0), \
  (t)->y0 = (Y0), \
  (t)->x1 = (X1), \
  (t)->y1 = (Y1), \
  (t)->x2 = (X2), \
  (t)->y2 = (Y2), \
  (t)->x3 = (X3), \
  (t)->y3 = (Y3), \
  (t)->cq = (Cq), \
  (t)->cx = (Cx), \
  (t)->cy = (Cy)

// **** global structures that define the tmd file format
#ifndef _TMD_TYPES_DEFINED
typedef struct
  {
  u_long  id;
  u_long  flag;
  u_long  nobj;
  } HEADER;

typedef struct
  {
  u_long  *vert_top;
  u_long  n_vert;
  u_long  *normal_top;
  u_long  n_normal;
  u_long  *primitive_top;
  u_long  n_primitive;
  long    scale;
  } OBJECT;

typedef struct
  {
  short   vx;
  short   vy;
  short   vz;
  short   pad;
  } VERTEX;

typedef struct
  {
  short   nx;
  short   ny;
  short   nz;
  short   pad;
  } NORMAL;

typedef struct
  {
  u_char  r;
  u_char  g;
  u_char  b;
  } COLOUR;

typedef struct
  {
  u_short x0, y0;
  u_short x1, y1;
  u_short x2, y2;
  u_short x3, y3;
  int     cq;
  u_short cx, cy;
  } TEXTURE;

// **** structures for different primitive types
typedef struct
  {
  u_char  olen, ilen, flag, mode;
  u_char  r0, g0, b0, mode2;
  u_short vert0, vert1;
  u_short vert2, pad;
  } FACE_3_NS_FP;

typedef struct
  {
  u_char  olen, ilen, flag, mode;
  u_char  r0, g0, b0, mode2;
  u_char  r1, g1, b1, pad1;
  u_char  r2, g2, b2, pad2;
  u_short vert0, vert1;
  u_short vert2, pad3;
  } FACE_3_NS_GP;

typedef struct
  {
  u_char  olen, ilen, flag, mode;
  u_char  r0, g0, b0, mode2;
  u_short vert0, vert1;
  u_short vert2, vert3;
  } FACE_4_NS_FP;

typedef struct
  {
  u_char  olen, ilen, flag, mode;
  u_char  r0, g0, b0, mode2;
  u_char  r1, g1, b1, pad0;
  u_char  r2, g2, b2, pad1;
  u_char  r3, g3, b3, pad2;
  u_short vert0, vert1;
  u_short vert2, vert3;
  } FACE_4_NS_GP;

typedef struct
  {
  u_char  olen, ilen, flag, mode;
  u_char  u0, v0;
  u_short cba;
  u_char  u1, v1;
  u_short tsb;
  u_char  u2, v2;
  u_short pad1;
  u_char  u3, v3;
  u_short pad2;
  u_char  r0, g0, b0, pad3;
  u_short vert0, vert1;
  u_short vert2, vert3;
  } FACE_4_TX_NS_FP;

typedef struct
  {
  u_char  olen, ilen, flag, mode;
  u_char  u0, v0;
  u_short cba;
  u_char  u1, v1;
  u_short tsb;
  u_char  u2, v2;
  u_short pad1;
  u_char  u3, v3;
  u_short pad2;
  u_char  r0, g0, b0, pad3;
  u_char  r1, g1, b1, pad4;
  u_char  r2, g2, b2, pad5;
  u_char  r3, g3, b3, pad6;
  u_short vert0, vert1;
  u_short vert2, vert3;
  } FACE_4_TX_NS_GP;

#endif /* _TMD_TYPES_DEFINED */

// **** function prototypes
void      TMD_prepare(u_long tMemAddress, VERTEX *tVertex, COLOUR *tColour, TEXTURE *tTexture);
u_long    *TMD_finish();

void      TMD_set3_NS_FP(u_short v0, u_short v1, u_short v2, 
                         u_long c0,
                         u_long tTransparency);
void      TMD_set3_NS_GP(u_short v0, u_short v1, u_short v2, 
                         u_long c0, u_long c1, u_long c2,
                         u_long tTransparency);
void      TMD_set4_NS_FP(u_short v0, u_short v1, u_short v2, u_short v3,
                         u_long c0,
                         u_long tTransparency);
void      TMD_set4_NS_GP(u_short v0, u_short v1, u_short v2, u_short v3,
                         u_long c0, u_long c1, u_long c2, u_long c3,
                         u_long tTransparency);
void      TMD_set4_TX_NS_FP(u_short v0, u_short v1, u_short v2, u_short v3,
                            u_long c0,
                            u_long t0);
void      TMD_set4_TX_NS_GP(u_short v0, u_short v1, u_short v2, u_short v3,
                            u_long c0, u_long c1, u_long c2, u_long c3,
                            u_long t0, u_long tTransparency);

void      sws_setTEXTURE4(FACE_4_TX_NS_FP *tPoly, u_long tTextureNum);
