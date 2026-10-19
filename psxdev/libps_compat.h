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
 *
 * Layout matches the Sony libps definition (libps.h) so that the
 * attribute bits (and the fixed-point handles) keep their full width.
 * x,y are screen coordinates measured from the centre of the screen,
 * as is everything else in the 2D API.
 */
typedef struct {
	u_long attribute;		/* flags (bit28/30/31, ABE, ...) */
	short  x, y;			/* screen position (centre origin) */
	u_short w, h;			/* width/height in pixels */
	u_short tpage;			/* texture page attribute */
	u_char u, v;			/* texture offset within page */
	short  cx, cy;			/* CLUT position in VRAM */
	u_char r, g, b;			/* colour modulation (0x80 = 1.0) */
	short  mx, my;			/* rotation/scale centre */
	short  scalex, scaley;		/* fixed-point scale (ONE = 1.0) */
	long   rotate;			/* rotation angle */
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
 * The OT is an array of buckets, one per depth slice.  Each bucket holds
 * a singly-linked list of GPU packets; GsDrawOt() walks the buckets from
 * far to near, so a lower index is closer to the viewer.  The game sizes
 * the array as (1 << length) and passes a priority for 2D primitives or
 * a depth shift for polygons.
 */
typedef u_long GsOT_TAG;

typedef struct {
	long length;			/* log2 of the number of buckets */
	GsOT_TAG *org;			/* bucket array */
} GsOT;

/*
 * Packet descriptor.  GsSetWorkBase() is handed the game's array, but
 * the real packet arena is private to this library; the type only has to
 * be a word-aligned block for the cast in the game's RenderPrepare().
 */
typedef struct {
	u_long data[16];
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
	u_long pixlen;			/* pixel data length in bytes */
	u_long clutlen;			/* CLUT data length in bytes */
} GsIMAGE;

/*
 * ---- TMD structures ----
 *
 * These mirror the real TMD / GPU packet layout (see psx-spx
 * "TMD File Format").  The first byte of every packet is its total
 * length in 32-bit words (olen), the second is the number of data
 * words (ilen), and byte 3 is the GPU command that the packet maps
 * onto.  Packs are stored in VRAM-order so a parsed packet needs
 * only its vertex coordinates patched before being sent.
 *
 * Field names match the game's own tmd.h so the dynamic TMD builder
 * (libs/tmd.c) compiles unchanged against this header.
 */
#ifndef _TMD_TYPES_DEFINED
#define _TMD_TYPES_DEFINED

typedef struct { short vx, vy, vz, pad; } VERTEX;
typedef struct { short nx, ny, nz, pad; } NORMAL;
typedef struct { u_char r, g, b; } COLOUR;
typedef struct {
	u_short x0, y0, x1, y1, x2, y2, x3, y3;
	int     cq;
	u_short cx, cy;
} TEXTURE;
typedef struct { u_long id; u_long flag; u_long nobj; } HEADER;
typedef struct {
	u_long vert_top; u_long n_vert;
	u_long normal_top; u_long n_normal;
	u_long primitive_top; u_long n_primitive;
	long scale;
} OBJECT;

/* F3 flat / gouraud - 4 / 6 words */
typedef struct {
	u_char olen, ilen, flag, mode;
	u_char r0, g0, b0, mode2;
	u_short vert0, vert1;
	u_short vert2, pad;
} FACE_3_NS_FP;

typedef struct {
	u_char olen, ilen, flag, mode;
	u_char r0, g0, b0, mode2;
	u_char r1, g1, b1, pad1;
	u_char r2, g2, b2, pad2;
	u_short vert0, vert1;
	u_short vert2, pad3;
} FACE_3_NS_GP;

/* F4 flat / gouraud - 5 / 8 words (one trailing pad word) */
typedef struct {
	u_char olen, ilen, flag, mode;
	u_char r0, g0, b0, mode2;
	u_short vert0, vert1;
	u_short vert2, vert3;
	u_long pad;
} FACE_4_NS_FP;

typedef struct {
	u_char olen, ilen, flag, mode;
	u_char r0, g0, b0, mode2;
	u_char r1, g1, b1, pad0;
	u_char r2, g2, b2, pad1;
	u_char r3, g3, b3, pad2;
	u_short vert0, vert1;
	u_short vert2, vert3;
	u_long pad;
} FACE_4_NS_GP;

/* FT4 flat textured - 9 words */
typedef struct {
	u_char olen, ilen, flag, mode;
	u_char u0, v0;
	u_short cba;
	u_char u1, v1;
	u_short tsb;
	u_char u2, v2;
	u_short pad1;
	u_char u3, v3;
	u_short pad2;
	u_char r0, g0, b0, pad3;
	u_short vert0, vert1;
	u_short vert2, vert3;
	u_long pad;
} FACE_4_TX_NS_FP;

/* GT4 gouraud textured - 12 words */
typedef struct {
	u_char olen, ilen, flag, mode;
	u_char u0, v0;
	u_short cba;
	u_char u1, v1;
	u_short tsb;
	u_char u2, v2;
	u_short pad1;
	u_char u3, v3;
	u_short pad2;
	u_char r0, g0, b0, pad3;
	u_char r1, g1, b1, pad4;
	u_char r2, g2, b2, pad5;
	u_char r3, g3, b3, pad6;
	u_short vert0, vert1;
	u_short vert2, vert3;
	u_long pad;
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

/* Low-level GPU access (used by psxlib.c) */
void GsSendGP0(u_long cmd);
void GsSendGP1(u_long cmd);
void GsSetDrawArea(long x, long y, long w, long h);
void GsSetDrawOffset(long x, long y);

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
