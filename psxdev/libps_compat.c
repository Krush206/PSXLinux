/*
 * libps_compat.c — Sony libps SDK compatibility layer for PSXLinux.
 *
 * Implements the Gs* rendering pipeline, matrix math, VRAM access,
 * and controller input using direct PSX hardware register access.
 *
 * Rendering strategy: immediate mode. Each GsSort* call sends GP0
 * command packets directly to the GPU rather than building an
 * ordering table for deferred DMA. This is simpler and sufficient
 * for the polygon counts this game produces.
 */

#include "libps_compat.h"

/*
 * ---- GPU registers (physical memory-mapped) ----
 */
#define GP0		(*(volatile unsigned int  *)0x1F801810)
#define GP1		(*(volatile unsigned int  *)0x1F801814)

#define GPUSTAT_FIFO	(1u << 28)
#define GPUSTAT_VBLANK	(1u << 31)

/* Pad registers (SIO0) */
#define PAD_DATA	(*(volatile unsigned short *)0x1F801044)
#define PAD_MODE	(*(volatile unsigned short *)0x1F801048)
#define PAD_CTRL	(*(volatile unsigned short *)0x1F80104A)
#define PAD_BAUD	(*(volatile unsigned short *)0x1F80104E)

/*
 * ---- Internal state ----
 */
static long gpu_width = 320;
static long gpu_height = 240;
static long active_buff = 0;
static long display_area_x[2] = { 0, 0 };
static long display_area_y[2] = { 0, 240 };
static long draw_offset_x = 0;
static long draw_offset_y = 0;

static MATRIX ls_matrix;		/* local-to-screen matrix */
static MATRIX light_matrix;		/* local-to-world (for lighting) */
static long projection_distance = 250;

static GsCOORDINATE2 *world_coord_ptr = 0;
static GsCOORDINATE2 world_coord_storage;

static GsRVIEW2 current_view;

/* Pad state */
static u_char pad_buf0[8];
static u_char pad_buf1[8];
static u_long pad_state;

/* Scratch pad (2KB at 0x1F800000 on R3000) */
#define SCRATCH_BASE	0x1F800000

/* Global WORLD coordinate — the game's header.h references this */
GsCOORDINATE2 *WORLD = 0;

/*
 * ---- GPU low-level helpers ----
 */

static void gp0_wait(void)
{
	while (!(GP1 & GPUSTAT_FIFO))
		;
}

static void gp0_send(unsigned int w)
{
	gp0_wait();
	GP0 = w;
}

static void gp1_send(unsigned int w)
{
	GP1 = w;
}

/*
 * ---- Initialization ----
 */

void GsInit(void)
{
	/* nothing extra needed beyond GsInitGraph */
}

void GsInitGraph(long width, long height, long gpu, long interlace, long dither)
{
	gpu_width = width;
	gpu_height = height;

	/* Soft reset */
	gp1_send(0x00000000);

	/* Display enable */
	gp1_send(0x03000000);

	/* NTSC 320x240 15bpp non-interlaced */
	gp1_send(0x08000000);

	/* Display area at (0,0) — will be updated by GsDefDispBuff */
	gp1_send(0x05000000);

	/* Horizontal display range (standard NTSC) */
	gp1_send(0x0602C0C60);

	/* Vertical display range (standard NTSC) */
	gp1_send(0x07000010);

	/* Drawing area covers full screen */
	gp0_send(0xE3000000);
	gp0_send(0xE4000000 | ((height - 1) << 10) | (width - 1));
	gp0_send(0xE5000000);
	gp0_send(0xE6000000);

	/* Initialize matrices to identity */
	{
		int i, j;
		for (i = 0; i < 3; i++)
			for (j = 0; j < 3; j++)
				ls_matrix.m[i][j] = (i == j) ? ONE : 0;
		for (i = 0; i < 3; i++)
			ls_matrix.t[i] = 0;
		light_matrix = ls_matrix;
	}
}

void GsDefDispBuff(long x0, long y0, long x1, long y1)
{
	display_area_x[0] = x0;
	display_area_y[0] = y0;
	display_area_x[1] = x1;
	display_area_y[1] = y1;
}

void GsInit3D(void)
{
	/* nothing extra needed */
}

void ResetGraph(long mode)
{
	gp1_send(0x02000000); /* clear VRAM */
	gp1_send(0x00000000); /* soft reset */
}

void SetVideoMode(long mode)
{
	/* PAL = 1, NTSC = 0 in GP1 08h bit 0 */
	if (mode == MODE_PAL)
		gp1_send(0x08000001);
	else
		gp1_send(0x08000000);
}

/*
 * ---- Buffer management ----
 */

long GsGetActiveBuff(void)
{
	return active_buff;
}

void GsSwapDispBuff(void)
{
	active_buff ^= 1;

	/* Update display area to show the just-drawn buffer */
	gp1_send(0x05000000 |
		 ((unsigned int)display_area_y[active_buff ^ 1] << 10) |
		 (unsigned int)display_area_x[active_buff ^ 1]);

	/* Update drawing offset for the new drawing buffer */
	draw_offset_x = display_area_x[active_buff];
	draw_offset_y = display_area_y[active_buff];
}

void GsClearOt(long offset, long point, GsOT *ot)
{
	/* Immediate mode — nothing to clear */
	(void)offset; (void)point; (void)ot;
}

void GsDrawOt(GsOT *ot)
{
	/* Immediate mode — everything already drawn */
	(void)ot;
}

void GsSetWorkBase(PACKET *base)
{
	/* Immediate mode — no packet buffer needed */
	(void)base;
}

/*
 * ---- Sync ----
 */

long VSync(long mode)
{
	/* Wait for VBlank start */
	while (!(GP1 & GPUSTAT_VBLANK))
		;
	/* Wait for VBlank end */
	while (GP1 & GPUSTAT_VBLANK)
		;
	return 0;
}

void DrawSync(long mode)
{
	/* Wait for GPU to finish (check FIFO empty + DMA done) */
	while (!(GP1 & GPUSTAT_FIFO))
		;
	(void)mode;
}

/*
 * ---- 2D drawing ----
 */

void GsSortBoxFill(GsBOXF *box, GsOT *ot, long priority)
{
	long x = box->x + draw_offset_x;
	long y = box->y + draw_offset_y;
	long w = box->w;
	long h = box->h;

	(void)ot; (void)priority;

	if (w <= 0 || h <= 0)
		return;
	if (x < -w || x >= gpu_width || y < -h || y >= gpu_height)
		return;

	gp0_send(0x60000000 |
		 ((unsigned int)(box->r & 0xFF) << 0) |
		 ((unsigned int)(box->g & 0xFF) << 8) |
		 ((unsigned int)(box->b & 0xFF) << 16));
	gp0_send(((unsigned int)y << 16) | (unsigned int)x);
	gp0_send(((unsigned int)h << 16) | (unsigned int)w);
}

void GsSortGLine(GsGLINE *line, GsOT *ot, long priority)
{
	long x0 = line->x0 + draw_offset_x;
	long y0 = line->y0 + draw_offset_y;
	long x1 = line->x1 + draw_offset_x;
	long y1 = line->y1 + draw_offset_y;

	(void)ot; (void)priority;

	if (y0 < 0 || y0 >= gpu_height)
		return;

	/* GP0 40h: gouraud line, 2 vertices */
	gp0_send(0x40000000 |
		 ((unsigned int)(line->r0 & 0xFF) << 0) |
		 ((unsigned int)(line->g0 & 0xFF) << 8) |
		 ((unsigned int)(line->b0 & 0xFF) << 16));
	gp0_send(((unsigned int)y0 << 16) | (unsigned int)x0);
	gp0_send(((unsigned int)(line->r1 & 0xFF) << 0) |
		 ((unsigned int)(line->g1 & 0xFF) << 8) |
		 ((unsigned int)(line->b1 & 0xFF) << 16));
	gp0_send(((unsigned int)y1 << 16) | (unsigned int)x1);
}

void GsSortSprite(GsSPRITE *spr, GsOT *ot, long priority)
{
	long x = spr->x + draw_offset_x;
	long y = spr->y + draw_offset_y;
	long w = spr->w;
	long h = spr->h;

	(void)ot; (void)priority;

	if (w <= 0 || h <= 0)
		return;

	/*
	 * For now, render sprites as flat colored rectangles.
	 * Textured sprite rendering requires VRAM CLUT upload which
	 * is handled by LoadTIMData in the game's sprite.c.
	 */
	gp0_send(0x60000000 |
		 ((unsigned int)(spr->r & 0xFF) << 0) |
		 ((unsigned int)(spr->g & 0xFF) << 8) |
		 ((unsigned int)(spr->b & 0xFF) << 16));
	gp0_send(((unsigned int)y << 16) | (unsigned int)x);
	gp0_send(((unsigned int)h << 16) | (unsigned int)w);
}

/*
 * ---- 3D coordinate system ----
 */

static GsCOORDINATE2 *world_ptr = 0;

/* Global WORLD coordinate */
static GsCOORDINATE2 _world;

void GsInitCoordinate2(GsCOORDINATE2 *parent, GsCOORDINATE2 *coord)
{
	int i, j;

	if (parent == 0) {
		if (world_ptr == 0) {
			/* First call — create WORLD root */
			for (i = 0; i < 3; i++)
				for (j = 0; j < 3; j++)
					_world.coord.m[i][j] =
						(i == j) ? ONE : 0;
			for (i = 0; i < 3; i++)
				_world.coord.t[i] = 0;
			_world.flg = 0;
			_world.super = 0;
			world_ptr = &_world;
		}
		coord->super = world_ptr;
	} else {
		coord->super = parent;
	}

	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++)
			coord->coord.m[i][j] = (i == j) ? ONE : 0;
	for (i = 0; i < 3; i++)
		coord->coord.t[i] = 0;

	coord->flg = 0;
	coord->worker = 0;
}

void GsSetProjection(long distance)
{
	projection_distance = distance;
}

void GsSetRefView2(GsRVIEW2 *view)
{
	current_view = *view;
}

/*
 * ---- Matrix operations ----
 */

void GsGetLws(GsCOORDINATE2 *coord, MATRIX *lw, MATRIX *ls)
{
	MATRIX world;
	GsCOORDINATE2 *c;
	int i, j;

	/*
	 * Compute local-to-world by walking up the coordinate hierarchy
	 * and accumulating transforms.
	 */
	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++)
			world.m[i][j] = (i == j) ? ONE : 0;
	for (i = 0; i < 3; i++)
		world.t[i] = 0;

	c = coord;
	while (c) {
		MATRIX tmp;

		/* world = world * c->coord */
		for (i = 0; i < 3; i++) {
			for (j = 0; j < 3; j++) {
				long sum = 0;
				int k;
				for (k = 0; k < 3; k++)
					sum += world.m[i][k] *
						c->coord.m[k][j];
				tmp.m[i][j] = sum >> 12;
			}
			tmp.t[i] = world.t[i];
		}
		for (i = 0; i < 3; i++) {
			long sum = 0;
			int k;
			for (k = 0; k < 3; k++)
				sum += world.m[i][k] * c->coord.t[k];
			tmp.t[i] += sum >> 12;
		}

		world = tmp;
		c = c->super;
	}

	*lw = world;

	/*
	 * Compute local-to-screen:
	 * 1. Subtract the view-from position
	 * 2. Rotate by inverse view orientation
	 * 3. Apply perspective projection
	 *
	 * Simplified: the game sets the view each frame via GsSetRefView2,
	 * so we compute the view matrix from the current_view.
	 */
	{
		long dx, dy, dz;
		long scale;

		dx = world.t[0] - current_view.vpx;
		dy = world.t[1] - current_view.vpy;
		dz = world.t[2] - current_view.vpz;

		/* Simple projection: divide x,y by z, scale by proj dist */
		scale = projection_distance;
		if (dz < 1) dz = 1;

		ls->t[0] = dx * scale / dz;
		ls->t[1] = dy * scale / dz;
		ls->t[2] = dz;

		/* Copy the rotation part from local-world */
		for (i = 0; i < 3; i++)
			for (j = 0; j < 3; j++)
				ls->m[i][j] = world.m[i][j];
	}
}

void GsSetLsMatrix(MATRIX *ls)
{
	ls_matrix = *ls;
}

void GsSetLightMatrix(MATRIX *lw)
{
	light_matrix = *lw;
}

void GsSetFlatLight(long num, GsF_LIGHT *light)
{
	(void)num; (void)light;
	/* Lighting not implemented — flat/gouraud colors used directly */
}

void GsSetAmbient(long r, long g, long b)
{
	(void)r; (void)g; (void)b;
}

void GsSetLightMode(long mode)
{
	(void)mode;
	/* Fogging not implemented */
}

/*
 * ---- Matrix math ----
 */

void RotMatrix(SVECTOR *vec, MATRIX *m)
{
	/*
	 * Build rotation matrix from Euler angles (fixed point 12.4).
	 * Standard rotation order: X then Y then Z.
	 */
	long sx, cx, sy, cy, sz, cz;
	long v;

	/* Simple sine/cosine using linear approximation */
	/* In real Gs this uses hardware lookup tables */

	v = vec->vx & 0xFFFF;
	sx = v >> 4;
	cx = (4096 - (v >> 4) * (v >> 4) / 4096);

	v = vec->vy & 0xFFFF;
	sy = v >> 4;
	cy = (4096 - (v >> 4) * (v >> 4) / 4096);

	v = vec->vz & 0xFFFF;
	sz = v >> 4;
	cz = (4096 - (v >> 4) * (v >> 4) / 4096);

	/* R = Rz * Ry * Rx */
	m->m[0][0] = (cy * cz) >> 12;
	m->m[0][1] = ((-sy * sx * cz) >> 24) - ((sz * cx * cz) >> 24);
	m->m[0][2] = ((-sy * cx * cz) >> 24) + ((sz * sx * cz) >> 24);
	m->m[1][0] = (cy * sz) >> 12;
	m->m[1][1] = ((-sy * sx * sz) >> 24) + ((cz * cx) >> 12);
	m->m[1][2] = ((-sy * cx * sz) >> 24) - ((cz * sx) >> 12);
	m->m[2][0] = sy;
	m->m[2][1] = (cy * sx) >> 12;
	m->m[2][2] = (cy * cx) >> 12;

	m->t[0] = m->t[1] = m->t[2] = 0;
}

void MulMatrix0(MATRIX *m1, MATRIX *m2, MATRIX *out)
{
	int i, j, k;
	long sum;

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			sum = 0;
			for (k = 0; k < 3; k++)
				sum += m1->m[i][k] * m2->m[k][j];
			out->m[i][j] = sum >> 12;
		}
		out->t[i] = m1->t[i];
	}
}

void ApplyMatrixSV(MATRIX *m, SVECTOR *in, SVECTOR *out)
{
	long x = in->vx, y = in->vy, z = in->vz;
	long sum;

	sum = (m->m[0][0] * x + m->m[0][1] * y + m->m[0][2] * z) >> 12;
	out->vx = (short)sum;
	sum = (m->m[1][0] * x + m->m[1][1] * y + m->m[1][2] * z) >> 12;
	out->vy = (short)sum;
	sum = (m->m[2][0] * x + m->m[2][1] * y + m->m[2][2] * z) >> 12;
	out->vz = (short)sum;
}

void ApplyMatrixLV(MATRIX *m, VECTOR *in, VECTOR *out)
{
	long x = in->vx, y = in->vy, z = in->vz;
	long sum;

	sum = (m->m[0][0] * x + m->m[0][1] * y + m->m[0][2] * z) >> 12;
	out->vx = sum;
	sum = (m->m[1][0] * x + m->m[1][1] * y + m->m[1][2] * z) >> 12;
	out->vy = sum;
	sum = (m->m[2][0] * x + m->m[2][1] * y + m->m[2][2] * z) >> 12;
	out->vz = sum;
}

/*
 * ---- 3D object rendering ----
 */

/*
 * GsMapModelingData parses the TMD header and stores pointers.
 * The TMD format has a header, then objects, then vertices/normals/primitives.
 */
void GsMapModelingData(u_long *top)
{
	/*
	 * In the real Gs library, this maps TMD data for the GPU.
	 * For our immediate-mode renderer, the caller (GsLinkObject4)
	 * stores the primitive pointer and we parse at draw time.
	 *
	 * The game's SetModelInfo does:
	 *   tMemAddress++;                    // skip TMD header
	 *   GsMapModelingData(tMemAddress);   // maps objects
	 *   tMemAddress += 2;                  // skip to object table
	 *   GsLinkObject4(tMemAddress, ...);  // link first object
	 *
	 * We just record the address; actual parsing happens in
	 * GsSortObject4 when drawing.
	 */
	(void)top;
}

static long obj_prim_top[16];		/* stored primitive pointers */
static long obj_nprim[16];		/* number of primitives */
static long obj_count = 0;

void GsLinkObject4(long obj_addr, GsDOBJ2 *handler, long scale)
{
	TMD_OBJECT *obj;

	(void)scale;

	handler->primtop = obj_addr;
	handler->coord2 = 0;
	handler->attribute = 0;
	handler->tpage = 0;

	obj = (TMD_OBJECT *)obj_addr;
	handler->nprim = obj->n_primitive;

	if (obj_count < 16) {
		obj_prim_top[obj_count] = obj_addr;
		obj_nprim[obj_count] = obj->n_primitive;
		obj_count++;
	}
}

void GsLinkObject(long obj_addr, GsDOBJ2 *handler, long scale)
{
	GsLinkObject4(obj_addr, handler, scale);
}

/*
 * Transform a 3D point to screen coordinates.
 * Uses the current ls_matrix (local-to-screen).
 */
static void transform_point(long x, long y, long z,
			    long *sx, long *sy, long *depth)
{
	long wx, wy, wz;
	long px, py;

	/* Apply local-world rotation + translation */
	wx = (ls_matrix.m[0][0] * x + ls_matrix.m[0][1] * y +
	      ls_matrix.m[0][2] * z) / ONE + ls_matrix.t[0];
	wy = (ls_matrix.m[1][0] * x + ls_matrix.m[1][1] * y +
	      ls_matrix.m[1][2] * z) / ONE + ls_matrix.t[1];
	wz = (ls_matrix.m[2][0] * x + ls_matrix.m[2][1] * y +
	      ls_matrix.m[2][2] * z) / ONE + ls_matrix.t[2];

	/* Perspective projection */
	if (wz < 1)
		wz = 1;

	px = wx * projection_distance / wz;
	py = wy * projection_distance / wz;

	*sx = px + gpu_width / 2 + draw_offset_x;
	*sy = py + gpu_height / 2 + draw_offset_y;
	*depth = wz;
}

void GsSortObject4(GsDOBJ2 *obj, GsOT *ot, long shift, u_long *scratch)
{
	TMD_OBJECT *tmd_obj;
	u_long *prim;
	VERTEX *verts;
	long nprim;
	long i;

	(void)ot; (void)shift; (void)scratch;

	if (!obj || !obj->primtop)
		return;

	/* obj->primtop points to the OBJECT structure in TMD data */
	tmd_obj = (TMD_OBJECT *)obj->primtop;

	/* Read pointers directly from the OBJECT fields */
	prim = (u_long *)tmd_obj->primitive_top;
	verts = (VERTEX *)tmd_obj->vert_top;
	nprim = tmd_obj->n_primitive;

	if (!prim || !verts || nprim <= 0)
		return;

	for (i = 0; i < nprim; i++) {
		/*
		 * Each TMD primitive starts with:
		 *   u_long mode  (packet type in low byte)
		 *   u_long flag  (ilen/olen packed)
		 * Then vertex indices and colors follow.
		 *
		 * The game's dynamic TMD format uses these modes:
		 *   0x21 = 3-vertex flat pigment
		 *   0x31 = 3-vertex gouraud pigment
		 *   0x29 = 4-vertex flat pigment
		 *   0x39 = 4-vertex gouraud pigment
		 */
		u_long mode = prim[0] & 0xFF;
		u_long *p = prim;

		switch (mode) {
		case 0x21: { /* 3-vertex flat */
			/*
			 * Layout: mode, flag, mode2, vert0/1, vert2/pad, color
			 * (from tmd.h: FACE_3_NS_FP)
			 */
			u_short v0, v1, v2;
			u_char r, g, b;
			long sx0, sy0, d0;
			long sx1, sy1, d1;
			long sx2, sy2, d2;
			VERTEX *vv;

			/* skip mode, flag, mode2 = 3 u_longs */
			p += 3;
			v0 = (u_short)(p[0] & 0xFFFF);
			v1 = (u_short)(p[0] >> 16);
			v2 = (u_short)(p[1] & 0xFFFF);
			r = (u_char)(p[1] >> 16);
			g = (u_char)(p[2] & 0xFF);
			b = (u_char)(p[2] >> 8) & 0xFF;

			vv = &verts[v0];
			transform_point(vv->vx, vv->vy, vv->vz,
				       &sx0, &sy0, &d0);
			vv = &verts[v1];
			transform_point(vv->vx, vv->vy, vv->vz,
				       &sx1, &sy1, &d1);
			vv = &verts[v2];
			transform_point(vv->vx, vv->vy, vv->vz,
				       &sx2, &sy2, &d2);

			if (d0 > 0 && d1 > 0 && d2 > 0) {
				gp0_send(0x20000000 |
					 (r) | (g << 8) | (b << 16));
				gp0_send((sy0 << 16) | sx0);
				gp0_send((sy1 << 16) | sx1);
				gp0_send((sy2 << 16) | sx2);
			}

			prim += 4; /* 4 u_longs per FACE_3_NS_FP */
			break;
		}
		case 0x31: { /* 3-vertex gouraud */
			u_short v0, v1, v2;
			u_char r0, g0, b0;
			u_char r1, g1, b1;
			u_char r2, g2, b2;
			long sx0, sy0, d0;
			long sx1, sy1, d1;
			long sx2, sy2, d2;
			VERTEX *vv;

			p += 3;
			v0 = (u_short)(p[0] & 0xFFFF);
			v1 = (u_short)(p[0] >> 16);
			v2 = (u_short)(p[1] & 0xFFFF);
			r0 = (u_char)(p[1] >> 16);
			g0 = (u_char)(p[2] & 0xFF);
			b0 = (u_char)((p[2] >> 8) & 0xFF);
			r1 = (u_char)((p[2] >> 16) & 0xFF);
			g1 = (u_char)(p[3] & 0xFF);
			b1 = (u_char)((p[3] >> 8) & 0xFF);
			r2 = (u_char)((p[3] >> 16) & 0xFF);
			g2 = (u_char)(p[4] & 0xFF);
			b2 = (u_char)((p[4] >> 8) & 0xFF);

			vv = &verts[v0];
			transform_point(vv->vx, vv->vy, vv->vz,
				       &sx0, &sy0, &d0);
			vv = &verts[v1];
			transform_point(vv->vx, vv->vy, vv->vz,
				       &sx1, &sy1, &d1);
			vv = &verts[v2];
			transform_point(vv->vx, vv->vy, vv->vz,
				       &sx2, &sy2, &d2);

			if (d0 > 0 && d1 > 0 && d2 > 0) {
				gp0_send(0x30000000 |
					 (r0) | (g0 << 8) | (b0 << 16));
				gp0_send((sy0 << 16) | sx0);
				gp0_send((r1) | (g1 << 8) | (b1 << 16));
				gp0_send((sy1 << 16) | sx1);
				gp0_send((r2) | (g2 << 8) | (b2 << 16));
				gp0_send((sy2 << 16) | sx2);
			}

			prim += 6; /* 6 u_longs per FACE_3_NS_GP */
			break;
		}
		case 0x29: { /* 4-vertex flat */
			u_short v0, v1, v2, v3;
			u_char r, g, b;
			long sx0, sy0, d0;
			long sx1, sy1, d1;
			long sx2, sy2, d2;
			long sx3, sy3, d3;
			VERTEX *vv;

			p += 3;
			v0 = (u_short)(p[0] & 0xFFFF);
			v1 = (u_short)(p[0] >> 16);
			v2 = (u_short)(p[1] & 0xFFFF);
			v3 = (u_short)(p[1] >> 16);
			r = (u_char)(p[2] & 0xFF);
			g = (u_char)((p[2] >> 8) & 0xFF);
			b = (u_char)((p[2] >> 16) & 0xFF);

			vv = &verts[v0];
			transform_point(vv->vx, vv->vy, vv->vz,
				       &sx0, &sy0, &d0);
			vv = &verts[v1];
			transform_point(vv->vx, vv->vy, vv->vz,
				       &sx1, &sy1, &d1);
			vv = &verts[v2];
			transform_point(vv->vx, vv->vy, vv->vz,
				       &sx2, &sy2, &d2);
			vv = &verts[v3];
			transform_point(vv->vx, vv->vy, vv->vz,
				       &sx3, &sy3, &d3);

			if (d0 > 0 && d1 > 0 && d2 > 0 && d3 > 0) {
				/* Two triangles for the quad */
				gp0_send(0x20000000 |
					 (r) | (g << 8) | (b << 16));
				gp0_send((sy0 << 16) | sx0);
				gp0_send((sy1 << 16) | sx1);
				gp0_send((sy2 << 16) | sx2);

				gp0_send(0x20000000 |
					 (r) | (g << 8) | (b << 16));
				gp0_send((sy0 << 16) | sx0);
				gp0_send((sy2 << 16) | sx2);
				gp0_send((sy3 << 16) | sx3);
			}

			prim += 5; /* 5 u_longs per FACE_4_NS_FP */
			break;
		}
		case 0x39: { /* 4-vertex gouraud */
			u_short v0, v1, v2, v3;
			u_char r0, g0, b0;
			u_char r1, g1, b1;
			u_char r2, g2, b2;
			u_char r3, g3, b3;
			long sx0, sy0, d0;
			long sx1, sy1, d1;
			long sx2, sy2, d2;
			long sx3, sy3, d3;
			VERTEX *vv;

			p += 3;
			v0 = (u_short)(p[0] & 0xFFFF);
			v1 = (u_short)(p[0] >> 16);
			v2 = (u_short)(p[1] & 0xFFFF);
			v3 = (u_short)(p[1] >> 16);
			r0 = (u_char)(p[2] & 0xFF);
			g0 = (u_char)((p[2] >> 8) & 0xFF);
			b0 = (u_char)((p[2] >> 16) & 0xFF);
			r1 = (u_char)(p[3] & 0xFF);
			g1 = (u_char)((p[3] >> 8) & 0xFF);
			b1 = (u_char)((p[3] >> 16) & 0xFF);
			r2 = (u_char)(p[4] & 0xFF);
			g2 = (u_char)((p[4] >> 8) & 0xFF);
			b2 = (u_char)((p[4] >> 16) & 0xFF);
			r3 = (u_char)(p[5] & 0xFF);
			g3 = (u_char)((p[5] >> 8) & 0xFF);
			b3 = (u_char)((p[5] >> 16) & 0xFF);

			vv = &verts[v0];
			transform_point(vv->vx, vv->vy, vv->vz,
				       &sx0, &sy0, &d0);
			vv = &verts[v1];
			transform_point(vv->vx, vv->vy, vv->vz,
				       &sx1, &sy1, &d1);
			vv = &verts[v2];
			transform_point(vv->vx, vv->vy, vv->vz,
				       &sx2, &sy2, &d2);
			vv = &verts[v3];
			transform_point(vv->vx, vv->vy, vv->vz,
				       &sx3, &sy3, &d3);

			if (d0 > 0 && d1 > 0 && d2 > 0 && d3 > 0) {
				gp0_send(0x30000000 |
					 (r0) | (g0 << 8) | (b0 << 16));
				gp0_send((sy0 << 16) | sx0);
				gp0_send((r1) | (g1 << 8) | (b1 << 16));
				gp0_send((sy1 << 16) | sx1);
				gp0_send((r2) | (g2 << 8) | (b2 << 16));
				gp0_send((sy2 << 16) | sx2);

				gp0_send(0x30000000 |
					 (r0) | (g0 << 8) | (b0 << 16));
				gp0_send((sy0 << 16) | sx0);
				gp0_send((r2) | (g2 << 8) | (b2 << 16));
				gp0_send((sy2 << 16) | sx2);
				gp0_send((r3) | (g3 << 8) | (b3 << 16));
				gp0_send((sy3 << 16) | sx3);
			}

			prim += 8; /* 8 u_longs per FACE_4_NS_GP */
			break;
		}
		default:
			/* Unknown mode — skip 4 u_longs as best guess */
			prim += 4;
			break;
		}
	}
}

void GsSortObject(GsDOBJ2 *obj, GsOT *ot, long shift, u_long *scratch)
{
	GsSortObject4(obj, ot, shift, scratch);
}

/*
 * ---- VRAM access ----
 */

void MoveImage(RECT *src, long dest_x, long dest_y)
{
	/* GP0 80h: VRAM-to-VRAM blit */
	gp0_send(0x80000000);
	gp0_send(((unsigned int)src->y << 16) | (unsigned int)src->x);
	gp0_send(((unsigned int)(src->h - 1) << 16) | (unsigned int)(src->w - 1));
	gp0_send(((unsigned int)dest_y << 16) | (unsigned int)dest_x);
}

void LoadImage(RECT *rect, u_long *data)
{
	long x = rect->x;
	long y = rect->y;
	long w = rect->w;
	long h = rect->h;
	long i, j;

	if (w <= 0 || h <= 0)
		return;

	/* GP0 A0h: CPU->VRAM transfer */
	gp0_send(0xA0000000);
	gp0_send(((unsigned int)y << 16) | (unsigned int)x);
	gp0_send(((unsigned int)(h - 1) << 16) | (unsigned int)(w - 1));

	/* Send pixel data (16bpp = 1 u_long per 2 pixels) */
	{
		long words = (w * h + 1) / 2;
		for (i = 0; i < words; i++)
			gp0_send(data[i]);
	}
}

long GetTPage(long mode, long semi, long x, long y)
{
	long tpage;
	long xpage = (x / 64) & 31;
	long ypage = (y / 256) & 1;
	long mode_bits = mode & 3; /* 0=4bpp, 1=8bpp, 2=15bpp, 3=mixed */
	long semi_bits = (semi & 3) << 5;

	tpage = (xpage << 0) |
		(mode_bits << 7) |
		(ypage << 4) |
		semi_bits;

	return tpage;
}

void GsGetTimInfo(u_long *tim, GsIMAGE *info)
{
	/*
	 * TIM file format:
	 *   u_long id      (0x10 = TIM file)
	 *   u_long flags   (bit 0: CLUT present, bits 1-2: pixel mode)
	 * If CLUT present:
	 *   u_long clut_len
	 *   u_short clut_x, clut_y, clut_w, clut_h
	 *   u_short[] clut_data
	 * Then pixel data:
	 *   u_long pixel_len
	 *   u_short pixel_x, pixel_y, pixel_w, pixel_h
	 *   u_short[] pixel_data
	 */
	u_long flags = tim[1];

	info->pmode = flags;

	if (flags & 1) {
		/* CLUT present */
		u_long *p = &tim[2];
		RECT *cr = (RECT *)&p[1];

		info->cx = cr->x;
		info->cy = cr->y;
		info->cw = cr->w;
		info->ch = cr->h;
		info->clut = (u_long *)&p[1 + 4];
		info->crect = cr;

		/* Advance to pixel data */
		p = (u_long *)((char *)p + p[0] + 4);
		{
			RECT *pr = (RECT *)&p[1];
			info->px = pr->x;
			info->py = pr->y;
			info->pw = pr->w;
			info->ph = pr->h;
			info->pixel = (u_long *)&p[1 + 4];
			info->rect = pr;
		}
	} else {
		/* No CLUT */
		u_long *p = &tim[2];
		RECT *pr = (RECT *)&p[1];

		info->px = pr->x;
		info->py = pr->y;
		info->pw = pr->w;
		info->ph = pr->h;
		info->pixel = (u_long *)&p[1 + 4];
		info->rect = pr;
		info->cx = info->cy = info->cw = info->ch = 0;
		info->clut = 0;
		info->crect = 0;
	}
}

/*
 * ---- Font (stubbed) ----
 */

long FntLoad(long x, long y)
{
	(void)x; (void)y;
	return 0;
}

long FntOpen(long x, long y, long w, long h, long clear, long bg)
{
	(void)x; (void)y; (void)w; (void)h; (void)clear; (void)bg;
	return 0;
}

void FntFlush(long id)
{
	(void)id;
}

/*
 * ---- Pad input ----
 */

void GetPadBuf(u_char **buf0, u_char **buf1)
{
	*buf0 = pad_buf0;
	*buf1 = pad_buf1;
}

/*
 * ---- Utility ----
 */

u_long *getScratchAddr(long offset)
{
	return (u_long *)(SCRATCH_BASE + offset * 4);
}
