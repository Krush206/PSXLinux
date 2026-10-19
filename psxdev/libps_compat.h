/*
 * libps_compat.h — Sony libps SDK compatibility layer for PSXLinux.
 *
 * Reimplements the subset of the Net Yaroze libps API used by
 * rs_race2 (and similar PSX games) on top of direct GPU hardware
 * register access, suitable for execution under the PSXLinux kernel.
 *
 * Reference: https://psx-spx.github.io/
 */

#ifndef _LIBPS_COMPAT_H
#define _LIBPS_COMPAT_H

/*
 * ---- Basic types ----
 */
typedef unsigned long  u_long;
typedef unsigned short u_short;
typedef unsigned char  u_char;
typedef short          short_;
typedef long           long_;

#ifndef NULL
#define NULL ((void*)0)
#endif

#define ONE		4096		/* 1.0 in fixed-point (12.4) */

#ifndef true
#define true	1
#define false	0
#endif

#define MODE_PAL	0
#define MODE_NTSC	1

/*
 * ---- Math structures ----
 */

/* Short 3D vector (16-bit fixed point) */
typedef struct {
	short vx, vy, vz;
	long pad;
} SVECTOR;

/* Long 3D vector (32-bit) */
typedef struct {
	long vx, vy, vz;
	long pad;
} VECTOR;

/* Color vector */
typedef struct {
	unsigned char r, g, b, mode;
	unsigned char cd, ce, isb, pad2;
} CVECTOR;

/* 3x3 rotation matrix + translation (fixed point) */
typedef struct {
	long m[3][3];
	long t[3];
	long pad;
} MATRIX;

/*
 * ---- VRAM rectangle ----
 */
typedef struct {
	short x, y;
	short w, h;
} RECT;

/*
 * ---- GPU sprite ----
 */
typedef struct {
	long x, y;			/* screen position */
	long w, h;			/* width/height in pixels */
	u_short tpage;			/* texture page attribute */
	u_short attribute;		/* semi-transparency, CLUT mode */
	u_char u, v;			/* texture offset within page */
	long cx, cy;			/* CLUT position in VRAM */
	long r, g, b;			/* color modulation */
	long mx, my;			/* rotation/scale center */
	long scalex, scaley;		/* fixed-point scale */
	long rotate;			/* rotation angle */
} GsSPRITE;

/*
 * ---- GPU filled box ----
 */
typedef struct {
	long x, y;			/* position (screen coords) */
	long w, h;			/* size */
	u_long attribute;		/* flags */
	long r, g, b;			/* color */
} GsBOXF;

/*
 * ---- GPU gradient line ----
 */
typedef struct {
	long x0, y0;			/* start point */
	long x1, y1;			/* end point */
	u_long r0, g0, b0;		/* start color */
	u_long r1, g1, b1;		/* end color */
	u_long attribute;		/* flags */
} GsGLINE;

/*
 * ---- Ordering table ----
 *
 * In the real Gs library, the OT is a linked list of GPU packets
 * sorted by depth.  Our implementation uses immediate-mode rendering
 * (each GsSort* call draws immediately), so the OT is simplified.
 */
typedef u_long GsOT_TAG;

typedef struct {
	long length;			/* OT depth */
	GsOT_TAG *org;			/* OT array pointer */
} GsOT;

/* Packet area (unused in immediate mode, kept for compatibility) */
typedef struct {
	u_long data[16];		/* enough for a 4-point gouraud poly */
} PACKET;

/*
 * ---- 3D object handling ----
 */

/* Coordinate system — maintains a transform hierarchy */
typedef struct GsCOORDINATE2 {
	MATRIX coord;			/* local transform matrix */
	short flg;			/* dirty flag */
	struct GsCOORDINATE2 *super;	/* parent coordinate */
	void (*worker)(void);		/* update callback */
} GsCOORDINATE2;

/* TMD primitive packet header */
typedef struct {
	u_long id;
	u_long flag;
	u_long nobj;
} TMD_HEADER;

typedef struct {
	u_long vert_top;
	u_long n_vert;
	u_long normal_top;
	u_long n_normal;
	u_long primitive_top;
	u_long n_primitive;
	long scale;
} TMD_OBJECT;

/* 3D object handler */
typedef struct {
	GsCOORDINATE2 *coord2;		/* coordinate system pointer */
	u_long attribute;		/* rendering attributes */
	long tpage;			/* texture page */
	long primtop;			/* pointer to primitives */
	long nprim;			/* number of primitives */
} GsDOBJ2;

/*
 * ---- Lights ----
 */
typedef struct {
	long vx, vy, vz;		/* light direction (flat, not SVECTOR) */
	long r, g, b;			/* light color */
} GsF_LIGHT;

/*
 * ---- Viewpoint ----
 */
typedef struct {
	GsCOORDINATE2 *super;		/* reference coordinate */
	long vpx, vpy, vpz;		/* view-from position */
	long vrx, vry, vrz;		/* view-to (reference) point */
	long rz;			/* roll */
	long proj_dist;			/* projection plane distance */
} GsRVIEW2;

/*
 * ---- TIM image info ----
 */
typedef struct {
	long pmode;			/* pixel mode (bpp + CLUT flag) */
	RECT *rect;			/* image rectangle in VRAM */
	u_long *pixel;			/* pointer to pixel data */
	long pw, ph;			/* pixel dimensions */
	long px, py;			/* VRAM position */
	RECT *crect;			/* CLUT rectangle */
	u_long *clut;			/* pointer to CLUT data */
	long cw, ch;			/* CLUT dimensions */
	long cx, cy;			/* CLUT VRAM position */
} GsIMAGE;

/*
 * ---- TMD structures ----
 */
#ifndef _TMD_TYPES_DEFINED
#define _TMD_TYPES_DEFINED

typedef struct { long vx, vy, vz; } VERTEX;
typedef struct { long nx, ny, nz; } NORMAL;
typedef struct { long r, g, b; } COLOUR;
typedef struct { long x0,y0,x1,y1,x2,y2,x3,y3; long cq,cx,cy; } TEXTURE;
typedef struct { u_long id; u_long flag; u_long nobj; } HEADER;
typedef struct {
	u_long vert_top; u_long n_vert;
	u_long normal_top; u_long n_normal;
	u_long primitive_top; u_long n_primitive;
	long scale;
} OBJECT;

typedef struct {
	u_long mode; u_long flag; u_long ilen; u_long olen;
	u_long mode2;
	u_short vert0, vert1, vert2;
	u_char r0,g0,b0,code0;
} FACE_3_NS_FP;

typedef struct {
	u_long mode; u_long flag; u_long ilen; u_long olen;
	u_long mode2;
	u_short vert0, vert1, vert2;
	u_char r0,g0,b0,code0;
	u_char r1,g1,b1,code1;
	u_char r2,g2,b2,code2;
} FACE_3_NS_GP;

typedef struct {
	u_long mode; u_long flag; u_long ilen; u_long olen;
	u_long mode2;
	u_short vert0, vert1, vert2, vert3;
	u_char r0,g0,b0,code0;
} FACE_4_NS_FP;

typedef struct {
	u_long mode; u_long flag; u_long ilen; u_long olen;
	u_long mode2;
	u_short vert0, vert1, vert2, vert3;
	u_char r0,g0,b0,code0;
	u_char r1,g1,b1,code1;
	u_char r2,g2,b2,code2;
	u_char r3,g3,b3,code3;
} FACE_4_NS_GP;

typedef struct {
	u_long mode; u_long flag; u_long ilen; u_long olen;
	u_long mode2;
	u_short vert0, vert1, vert2, vert3;
	u_char r0,g0,b0,code0;
	u_char u0, v0;
	u_short tsb;
	u_short cba;
	u_char u1, v1;
	u_char u2, v2;
	u_char u3, v3;
	u_long pad;
} FACE_4_TX_NS_FP;

typedef struct {
	u_long mode; u_long flag; u_long ilen; u_long olen;
	u_long mode2;
	u_short vert0, vert1, vert2, vert3;
	u_char r0,g0,b0,code0;
	u_char u0, v0;
	u_short tsb;
	u_short cba;
	u_char u1, v1;
	u_char r1,g1,b1,code1;
	u_char u2, v2;
	u_char r2,g2,b2,code2;
	u_char u3, v3;
	u_char r3,g3,b3,code3;
} FACE_4_TX_NS_GP;

#endif /* _TMD_TYPES_DEFINED */

/*
 * ---- Function prototypes ----
 */

/* Initialization */
void GsInit(void);
void GsInitGraph(long width, long height, long gpu, long interlace, long dither);
void GsDefDispBuff(long x0, long y0, long x1, long y1);
void GsInit3D(void);
void ResetGraph(long mode);
void SetVideoMode(long mode);

/* Buffer management */
long GsGetActiveBuff(void);
void GsSwapDispBuff(void);
void GsClearOt(long offset, long point, GsOT *ot);
void GsDrawOt(GsOT *ot);
void GsSetWorkBase(PACKET *base);

/* Sync */
long VSync(long mode);
void DrawSync(long mode);

/* 2D drawing */
void GsSortBoxFill(GsBOXF *box, GsOT *ot, long priority);
void GsSortGLine(GsGLINE *line, GsOT *ot, long priority);
void GsSortSprite(GsSPRITE *sprite, GsOT *ot, long priority);

/* 3D coordinate system */
void GsInitCoordinate2(GsCOORDINATE2 *parent, GsCOORDINATE2 *coord);
void GsSetProjection(long distance);
void GsSetRefView2(GsRVIEW2 *view);

/* Matrix operations */
void GsGetLws(GsCOORDINATE2 *coord, MATRIX *lw, MATRIX *ls);
void GsSetLsMatrix(MATRIX *ls);
void GsSetLightMatrix(MATRIX *lw);
void GsSetFlatLight(long num, GsF_LIGHT *light);
void GsSetAmbient(long r, long g, long b);
void GsSetLightMode(long mode);

/* 3D object rendering */
void GsMapModelingData(u_long *top);
void GsLinkObject4(long obj_addr, GsDOBJ2 *handler, long scale);
void GsLinkObject(long obj_addr, GsDOBJ2 *handler, long scale);
void GsSortObject4(GsDOBJ2 *obj, GsOT *ot, long shift, u_long *scratch);
void GsSortObject(GsDOBJ2 *obj, GsOT *ot, long shift, u_long *scratch);

/* Matrix math */
void RotMatrix(SVECTOR *vec, MATRIX *m);
void MulMatrix0(MATRIX *m1, MATRIX *m2, MATRIX *out);
void ApplyMatrixSV(MATRIX *m, SVECTOR *in, SVECTOR *out);
void ApplyMatrixLV(MATRIX *m, VECTOR *in, VECTOR *out);

/* VRAM */
void LoadImage(RECT *rect, u_long *data);
long GetTPage(long mode, long semi, long x, long y);
void GsGetTimInfo(u_long *tim, GsIMAGE *info);

/* Font (stubbed — we don't use the Sony font) */
long FntLoad(long x, long y);
long FntOpen(long x, long y, long w, long h, long clear, long bg);
void FntFlush(long id);

/* Pad */
void GetPadBuf(u_char **buf0, u_char **buf1);

/* VRAM-to-VRAM blit */
void MoveImage(RECT *src, long dest_x, long dest_y);

/* Utility */
u_long *getScratchAddr(long offset);

/* GPU constants */
#define GsOFSGPU	0
#define GsOFSPACKET	1
#define GsOFSTMD	2

/* PAD button masks (active-high after our ReadJoyPad inversion) */
#define PADselect	(1<<8)
#define PADstart	(1<<11)
#define PADup		(1<<12)
#define PADdown		(1<<14)
#define PADleft		(1<<15)
#define PADright		(1<<13)
#define PADtriangle	(1<<4)
#define PADcross	(1<<6)
#define PADsquare	(1<<7)
#define PADcircle	(1<<5)
#define PADL1		(1<<2)
#define PADR1		(1<<3)
#define PADL2		(1<<0)
#define PADR2		(1<<1)

/* Gs attribute bits */
#define GsOFSPOLY	0
#define GsTRANS	(1<<30)

#endif /* _LIBPS_COMPAT_H */
