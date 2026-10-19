/*
 * libps_compat.c — Sony libps SDK compatibility layer for PSXLinux.
 *
 * Implements the Gs* rendering pipeline, matrix math, VRAM access,
 * and controller input using direct PSX hardware register access.
 *
 * Rendering strategy: deferred, via an ordering table, exactly like
 * the real library (only without DMA).  Each GsSort* call transforms
 * its geometry and appends one self-contained GPU packet to the
 * bucket of the ordering table selected by the object's priority (for
 * 2D primitives) or by its projected depth (for polygons).  GsDrawOt
 * then walks the table back-to-front and feeds the packets to the GPU
 * through GP0.  This is what keeps far geometry behind near geometry
 * regardless of the order the game happens to submit it in.
 *
 * The game's coordinates are screen-centred for 2D and use the PSX
 * world convention (+X right, +Y down, +Z into the screen) for 3D.
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

/*
 * Deferred-rendering arena.  GsSetWorkBase() rewinds it at the start of
 * every frame and GsDrawOt() empties it into the GPU.  A single arena is
 * enough: a frame is completely rendered before the next one begins.
 *
 * Each packet node is
 *	[0] next node in its ordering-table bucket
 *	[1] number of GPU words
 *	[2..] the GPU words themselves
 * so callers only ever see a pointer to the words (node + 2).
 */
#define PKT_ARENA_WORDS	16384
static u_long pkt_arena[PKT_ARENA_WORDS];
static u_long *pkt_top;
static long pkt_dropped;

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
 * Point the GPU at one of the two framebuffers: the drawing area is the
 * buffer's rectangle in VRAM and the drawing offset relocates the
 * screen-relative coordinates produced by the Gs layer into it.  The
 * offset is applied by the GPU before clipping, so primitives keep using
 * plain 0..319 / 0..239 coordinates.
 */
static void set_draw_target(long idx)
{
	long x = display_area_x[idx];
	long y = display_area_y[idx];

	gp0_send(0xE3000000 | ((unsigned int)(y & 0x1FF) << 10) |
		 (unsigned int)(x & 0x3FF));
	gp0_send(0xE4000000 | ((unsigned int)((y + gpu_height - 1) & 0x1FF) << 10) |
		 (unsigned int)((x + gpu_width - 1) & 0x3FF));
	gp0_send(0xE5000000 | ((unsigned int)(y & 0x7FF) << 11) |
		 (unsigned int)(x & 0x7FF));
}

/*
 * ---- Low-level GPU access (exposed for psxlib.c) ----
 */

void GsSendGP0(u_long cmd)
{
	gp0_send((unsigned int)cmd);
}

void GsSendGP1(u_long cmd)
{
	gp1_send((unsigned int)cmd);
}

void GsSetDrawArea(long x, long y, long w, long h)
{
	gp0_send(0xE3000000 | ((unsigned int)(y & 0x1FF) << 10) |
		 (unsigned int)(x & 0x3FF));
	gp0_send(0xE4000000 | ((unsigned int)((y + h - 1) & 0x1FF) << 10) |
		 (unsigned int)((x + w - 1) & 0x3FF));
}

/*
 * Hardware drawing offset.  The Gs layer applies the double-buffer
 * offset in software (see GsSwapDispBuff), so callers normally leave
 * this at (0,0).
 */
void GsSetDrawOffset(long x, long y)
{
	gp0_send(0xE5000000 | ((unsigned int)(y & 0x7FF) << 11) |
		 (unsigned int)(x & 0x7FF));
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

	/*
	 * 320x240 15bpp non-interlaced.  Bits 0-1 select the horizontal
	 * resolution (0=256, 1=320) and bits 2-3 the vertical one
	 * (0=240).  The double buffer below is 240 lines tall, so we
	 * always use 240 lines regardless of the TV standard.
	 */
	gp1_send(0x08000001);

	/* Display area at (0,0) — GsDefDispBuff() refines this. */
	gp1_send(0x05000000);

	/* Horizontal display range: 2560 units = the full 320 pixels. */
	gp1_send(0x06C00200);

	/* Vertical display range: lines 0x10..0x100 = 240 lines. */
	gp1_send(0x07000410);

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

	/*
	 * A frame is rendered into the buffer that is *not* on screen, so
	 * the first frame goes into buffer active_buff ^ 1 while buffer
	 * active_buff stays displayed.
	 */
	set_draw_target(active_buff ^ 1);
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
	long drawn = active_buff ^ 1;	/* buffer the frame went into */

	/* Show what was just rendered... */
	gp1_send(0x05000000 |
		 ((unsigned int)display_area_y[drawn] << 10) |
		 (unsigned int)display_area_x[drawn]);

	active_buff ^= 1;

	/* ...and aim the next frame at the other buffer. */
	set_draw_target(active_buff ^ 1);
}

void GsClearOt(long offset, long point, GsOT *ot)
{
	long n, i;

	(void)offset; (void)point;

	if (!ot || !ot->org)
		return;

	n = 1L << ot->length;
	for (i = 0; i < n; i++)
		ot->org[i] = 0;
}

void GsDrawOt(GsOT *ot)
{
	long n, i, j;

	if (!ot || !ot->org)
		return;

	/*
	 * Walk the table back to front.  Bucket 0 is the front-most one,
	 * so it is emitted last and therefore ends up on top.
	 */
	n = 1L << ot->length;
	for (i = n - 1; i >= 0; i--) {
		u_long *p = (u_long *)ot->org[i];

		while (p) {
			long words = (long)p[1];

			for (j = 0; j < words; j++)
				gp0_send((unsigned int)p[2 + j]);
			p = (u_long *)p[0];
		}
	}
}

void GsSetWorkBase(PACKET *base)
{
	/*
	 * The game hands us one of its (small) packet arrays, but the
	 * arena actually lives here so it can be sized for the whole
	 * scene rather than for a single primitive.
	 */
	(void)base;
	pkt_top = pkt_arena;
	pkt_dropped = 0;
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

/*
 * Allocate a packet node holding room for `words` GPU words.  Returns a
 * pointer to the word area, or NULL if the arena is exhausted (in which
 * case the primitive is simply dropped and counted).
 */
static u_long *pkt_new(long words)
{
	u_long *p;

	if (!pkt_top)
		return 0;
	if (pkt_top + words + 2 > pkt_arena + PKT_ARENA_WORDS) {
		pkt_dropped++;
		return 0;
	}

	p = pkt_top;
	pkt_top += words + 2;
	p[0] = 0;
	p[1] = (u_long)words;
	return p + 2;
}

/*
 * Chain a finished packet into the bucket `priority` of the ordering
 * table.  Lower numbers are nearer the viewer and are drawn last.
 */
static void pkt_link(GsOT *ot, long priority, u_long *words)
{
	u_long *node = words - 2;
	long n;

	if (!ot || !ot->org)
		return;

	n = 1L << ot->length;
	if (priority < 0)
		priority = 0;
	else if (priority >= n)
		priority = n - 1;

	node[0] = (u_long)ot->org[priority];
	ot->org[priority] = (u_long)node;
}

void GsSortBoxFill(GsBOXF *box, GsOT *ot, long priority)
{
	long x = box->x + gpu_width / 2;
	long y = box->y + gpu_height / 2;
	long w = box->w;
	long h = box->h;
	u_long *p;

	if (w <= 0 || h <= 0)
		return;
	if (x < -w || x >= gpu_width || y < -h || y >= gpu_height)
		return;

	p = pkt_new(3);
	if (!p)
		return;

	p[0] = 0x60000000 |
	       ((u_long)(box->r & 0xFF) << 0) |
	       ((u_long)(box->g & 0xFF) << 8) |
	       ((u_long)(box->b & 0xFF) << 16);
	p[1] = (((u_long)y & 0xFFFF) << 16) | ((u_long)x & 0xFFFF);
	p[2] = (((u_long)h & 0xFFFF) << 16) | ((u_long)w & 0xFFFF);

	pkt_link(ot, priority, p);
}

void GsSortGLine(GsGLINE *line, GsOT *ot, long priority)
{
	long x0 = line->x0 + gpu_width / 2;
	long y0 = line->y0 + gpu_height / 2;
	long x1 = line->x1 + gpu_width / 2;
	long y1 = line->y1 + gpu_height / 2;
	u_long *p;

	p = pkt_new(4);
	if (!p)
		return;

	/*
	 * GP0 40h: two-vertex gouraud line.  The game builds filled
	 * boxes out of one such line per row, interpolating the colour
	 * across the row, so both endpoint colours matter.
	 */
	p[0] = 0x40000000 |
	       ((u_long)(line->r0 & 0xFF) << 0) |
	       ((u_long)(line->g0 & 0xFF) << 8) |
	       ((u_long)(line->b0 & 0xFF) << 16);
	p[1] = (((u_long)y0 & 0xFFFF) << 16) | ((u_long)x0 & 0xFFFF);
	p[2] = ((u_long)(line->r1 & 0xFF) << 0) |
	       ((u_long)(line->g1 & 0xFF) << 8) |
	       ((u_long)(line->b1 & 0xFF) << 16);
	p[3] = (((u_long)y1 & 0xFFFF) << 16) | ((u_long)x1 & 0xFFFF);

	pkt_link(ot, priority, p);
}

void GsSortSprite(GsSPRITE *spr, GsOT *ot, long priority)
{
	long x = spr->x + gpu_width / 2;
	long y = spr->y + gpu_height / 2;
	long w = spr->w;
	long h = spr->h;
	u_long clut, col;
	u_long *p;

	/*
	 * scalex/scaley are 12.4 fixed point (ONE == 1.0).  Rotation is
	 * not supported by the untextured rectangle path.
	 */
	if (spr->scalex != ONE && spr->scalex > 0)
		w = (w * spr->scalex) / ONE;
	if (spr->scaley != ONE && spr->scaley > 0)
		h = (h * spr->scaley) / ONE;

	if (w <= 0 || h <= 0)
		return;

	p = pkt_new(5);
	if (!p)
		return;

	/* CLUT word: X in bits 0-5 (in 16-pixel units), Y in bits 6-14. */
	clut = ((((u_long)spr->cx & 0x3FF) >> 4) & 0x3F) |
	       ((((u_long)spr->cy) & 0x1FF) << 6);
	col = ((u_long)(spr->b & 0xFF) << 16) |
	      ((u_long)(spr->g & 0xFF) << 8) |
	      ((u_long)(spr->r & 0xFF) << 0);

	/* GP0 E1h selects the texture page for the sprite that follows. */
	p[0] = 0xE1000000 | ((u_long)spr->tpage & 0x3FF);
	p[1] = 0x64000000 | col;
	p[2] = (((u_long)y & 0xFFFF) << 16) | ((u_long)x & 0xFFFF);
	p[3] = (clut << 16) | ((u_long)(spr->v & 0xFF) << 8) |
	       (u_long)(spr->u & 0xFF);
	p[4] = (((u_long)h & 0xFFFF) << 16) | ((u_long)w & 0xFFFF);

	pkt_link(ot, priority, p);
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

/* Integer square root (Newton's method). */
static long isqrt(long n)
{
	long x, y;

	if (n <= 0)
		return 0;
	x = n;
	y = (x + 1) / 2;
	while (y < x) {
		x = y;
		y = (x + n / x) / 2;
	}
	return x;
}

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
	 * Compose local-to-world with the current view matrix, giving
	 * local-to-camera (view) space in ls.  ls->m is the combined
	 * rotation and ls->t the camera-space translation; the perspective
	 * divide happens later in transform_point().
	 */
	{
		MATRIX view;
		VECTOR fwd, right, up;
		long fx, fy, fz, len;
		long eye[3];
		int k;

		eye[0] = current_view.vpx;
		eye[1] = current_view.vpy;
		eye[2] = current_view.vpz;

		/* forward = normalise(target - eye) */
		fx = current_view.vrx - eye[0];
		fy = current_view.vry - eye[1];
		fz = current_view.vrz - eye[2];
		len = isqrt(fx * fx + fy * fy + fz * fz);
		if (len < 1)
			len = 1;
		fwd.vx = (fx << 12) / len;
		fwd.vy = (fy << 12) / len;
		fwd.vz = (fz << 12) / len;

		/* right = normalise(worldUp x forward), with up=(0,1,0) */
		right.vx = fwd.vz;
		right.vy = 0;
		right.vz = -fwd.vx;
		len = isqrt(right.vx * right.vx + right.vz * right.vz);
		if (len < 1)
			len = 1;
		right.vx = (right.vx << 12) / len;
		right.vz = (right.vz << 12) / len;

		/* up = forward x right */
		up.vx = (fwd.vy * right.vz - fwd.vz * right.vy) >> 12;
		up.vy = (fwd.vz * right.vx - fwd.vx * right.vz) >> 12;
		up.vz = (fwd.vx * right.vy - fwd.vy * right.vx) >> 12;

		view.m[0][0] = right.vx; view.m[0][1] = right.vy; view.m[0][2] = right.vz;
		view.m[1][0] = up.vx;    view.m[1][1] = up.vy;    view.m[1][2] = up.vz;
		view.m[2][0] = fwd.vx;   view.m[2][1] = fwd.vy;   view.m[2][2] = fwd.vz;
		view.t[0] = view.t[1] = view.t[2] = 0;

		/* ls = view * world */
		for (i = 0; i < 3; i++) {
			long sum = 0;

			for (j = 0; j < 3; j++) {
				for (k = 0; k < 3; k++)
					sum += view.m[i][k] * world.m[k][j];
				ls->m[i][j] = sum >> 12;
			}

			sum = 0;
			for (k = 0; k < 3; k++)
				sum += view.m[i][k] * (world.t[k] - eye[k]);
			ls->t[i] = sum >> 12;
		}
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

/*
 * ---- Trigonometry ----
 *
 * The GTE's angle unit is 1/4096 of a revolution.  We keep a 4096-entry
 * Q12 sine table (and its cosine) built once, lazily, from a Taylor
 * series evaluated in soft-float.
 */

#define TRIG_SIZE	4096
#define TRIG_PI		3.14159265358979f
#define TRIG_2PI	6.28318530717959f

static short trig_sin[TRIG_SIZE];
static short trig_cos[TRIG_SIZE];
static int trig_ready;

static float taylor_sin(float x)
{
	float x2;
	int neg = 0;

	/* Reduce to [-pi, pi]. */
	while (x > TRIG_PI)
		x -= TRIG_2PI;
	while (x < -TRIG_PI)
		x += TRIG_2PI;

	/* Reduce to [-pi/2, pi/2]; sin(pi - x) = sin(x). */
	if (x > TRIG_PI / 2.0f) {
		x = TRIG_PI - x;
	} else if (x < -TRIG_PI / 2.0f) {
		x = -TRIG_PI - x;
		neg = 1;
	}

	x2 = x * x;
	x = x * (1.0f - x2 * (1.0f / 6.0f -
		x2 * (1.0f / 120.0f -
		x2 * (1.0f / 5040.0f - x2 / 362880.0f))));

	(void)neg;
	return x;
}

static void build_trig(void)
{
	int i;

	if (trig_ready)
		return;

	for (i = 0; i < TRIG_SIZE; i++) {
		float a = (float)i * TRIG_2PI / (float)TRIG_SIZE;
		trig_sin[i] = (short)(taylor_sin(a) * 4096.0f + 0.5f);
		trig_cos[i] = (short)(taylor_sin(a + TRIG_PI / 2.0f) * 4096.0f + 0.5f);
	}
	trig_ready = 1;
}

static long isin(long a)
{
	build_trig();
	return trig_sin[a & (TRIG_SIZE - 1)];
}

static long icos(long a)
{
	build_trig();
	return trig_cos[a & (TRIG_SIZE - 1)];
}

void RotMatrix(SVECTOR *vec, MATRIX *m)
{
	/*
	 * Build a rotation matrix from Euler angles (GTE convention,
	 * rotation order X then Y then Z).  Angles are 1/4096 rev.
	 */
	long sx, cx, sy, cy, sz, cz;

	sx = isin(vec->vx); cx = icos(vec->vx);
	sy = isin(vec->vy); cy = icos(vec->vy);
	sz = isin(vec->vz); cz = icos(vec->vz);

	m->m[0][0] = (cy * cz) >> 12;
	m->m[1][0] = (cy * sz) >> 12;
	m->m[2][0] = -sy;

	m->m[0][1] = ((((sx * sy) >> 12) * cz) >> 12) - ((cx * sz) >> 12);
	m->m[1][1] = ((((sx * sy) >> 12) * sz) >> 12) + ((cx * cz) >> 12);
	m->m[2][1] = (sx * cy) >> 12;

	m->m[0][2] = ((((cx * sy) >> 12) * cz) >> 12) + ((sx * sz) >> 12);
	m->m[1][2] = ((((cx * sy) >> 12) * sz) >> 12) - ((sx * cz) >> 12);
	m->m[2][2] = (cx * cy) >> 12;

	m->t[0] = m->t[1] = m->t[2] = 0;
}

void MulMatrix0(MATRIX *m1, MATRIX *m2, MATRIX *out)
{
	int i, j;

	/*
	 * RotateModel() passes the same matrix as m1 and out, so each row
	 * of m1 is read into locals before out is written.
	 */
	for (i = 0; i < 3; i++) {
		long a0 = m1->m[i][0], a1 = m1->m[i][1], a2 = m1->m[i][2];
		long t = m1->t[i];

		for (j = 0; j < 3; j++)
			out->m[i][j] = (a0 * m2->m[0][j] +
					a1 * m2->m[1][j] +
					a2 * m2->m[2][j]) >> 12;
		out->t[i] = t;
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
	 * The real GsMapModelingData patches a TMD's internal byte
	 * offsets into pointers, in place.  We leave the data alone:
	 * TMDs read from a file keep their offsets (resolved against
	 * the object header in GsSortObject4) while the dynamic builder
	 * stores absolute pointers, and GsSortObject4 tells the two
	 * apart by magnitude.  Kept so SetModelInfo() reads naturally.
	 */
	(void)top;
}

void GsLinkObject4(long obj_addr, GsDOBJ2 *handler, long scale)
{
	TMD_OBJECT *obj = (TMD_OBJECT *)obj_addr;

	(void)scale;

	handler->primtop = obj_addr;		/* TMD object header */
	handler->nprim = obj->n_primitive;
	handler->attribute = 0;
	handler->tpage = 0;
}

void GsLinkObject(long obj_addr, GsDOBJ2 *handler, long scale)
{
	GsLinkObject4(obj_addr, handler, scale);
}

/*
 * Transform a model-space vertex through the current local-to-view
 * matrix (ls_matrix) and project it to the screen.  The view-space
 * depth is returned so callers can reject geometry behind the camera.
 *
 * The world uses the PSX convention (+X right, +Y down, +Z away from the
 * viewer), so the vertical projection is added rather than subtracted,
 * and coordinates are relative to the centre of the screen: the GPU's
 * drawing offset places them in the current framebuffer.
 */
static void transform_point(long x, long y, long z,
			    long *sx, long *sy, long *depth)
{
	long vx, vy, vz;
	long px, py;

	vx = (ls_matrix.m[0][0] * x + ls_matrix.m[0][1] * y +
	      ls_matrix.m[0][2] * z) >> 12;
	vy = (ls_matrix.m[1][0] * x + ls_matrix.m[1][1] * y +
	      ls_matrix.m[1][2] * z) >> 12;
	vz = (ls_matrix.m[2][0] * x + ls_matrix.m[2][1] * y +
	      ls_matrix.m[2][2] * z) >> 12;
	vx += ls_matrix.t[0];
	vy += ls_matrix.t[1];
	vz += ls_matrix.t[2];

	*depth = vz;
	if (vz < 16) {
		*sx = 0;
		*sy = 0;
		return;
	}

	px = (vx * projection_distance) / vz;
	py = (vy * projection_distance) / vz;

	*sx = px + gpu_width / 2;
	*sy = py + gpu_height / 2;
}

/*
 * ---- TMD primitive packet decoder ----
 *
 * A TMD packet starts with a 4-byte header whose bytes, low to high,
 * are: olen (total packet length in 32-bit words), ilen (payload
 * words), flag and mode (the GPU polygon command).  The payload order
 * differs from the GPU wire format - the TMD groups texture
 * coordinates, then colours, then vertex *indices* - so every packet
 * is re-packed here, the indices being replaced with transformed
 * screen coordinates.
 */

#define TMD_XY(sx, sy)	((((u_long)(sy) & 0xFFFF) << 16) | ((u_long)(sx) & 0xFFFF))
#define TMD_UV(u, v)	((u_long)((u) & 0xFF) | (((u_long)(v) & 0xFF) << 8))

static void tmd_tri(VERTEX *verts, u_short v0, u_short v1, u_short v2,
		    long *sx0, long *sy0, long *sx1, long *sy1,
		    long *sx2, long *sy2, long *d0, long *d1, long *d2)
{
	transform_point(verts[v0].vx, verts[v0].vy, verts[v0].vz, sx0, sy0, d0);
	transform_point(verts[v1].vx, verts[v1].vy, verts[v1].vz, sx1, sy1, d1);
	transform_point(verts[v2].vx, verts[v2].vy, verts[v2].vz, sx2, sy2, d2);
}

static void tmd_quad(VERTEX *verts, u_short v0, u_short v1, u_short v2, u_short v3,
		     long *sx0, long *sy0, long *sx1, long *sy1, long *sx2, long *sy2,
		     long *sx3, long *sy3, long *d0, long *d1, long *d2, long *d3)
{
	tmd_tri(verts, v0, v1, v2, sx0, sy0, sx1, sy1, sx2, sy2, d0, d1, d2);
	transform_point(verts[v3].vx, verts[v3].vy, verts[v3].vz, sx3, sy3, d3);
}

/*
 * Emit one finished polygon into the ordering table.  The bucket comes
 * from the polygon's view-space depth and the GsSortObject4() shift
 * argument, so GsDrawOt() can paint the scene back to front regardless
 * of the order in which the game submitted its models.
 */
static void emit_poly(GsOT *ot, long shift, long depth, u_long *words, long n)
{
	u_long *p;
	long i;

	if (!ot || n <= 0)
		return;

	p = pkt_new(n);
	if (!p)
		return;

	for (i = 0; i < n; i++)
		p[i] = words[i];

	if (shift < 0)
		shift = 0;
	else if (shift > 20)
		shift = 20;

	pkt_link(ot, depth >> shift, p);
}

void GsSortObject4(GsDOBJ2 *obj, GsOT *ot, long shift, u_long *scratch)
{
	TMD_OBJECT *o;
	u_long base;
	u_long *prim;
	VERTEX *verts;
	long nprim, i;

	(void)scratch;

	if (!obj || !obj->primtop)
		return;

	base = (u_long)obj->primtop;
	o = (TMD_OBJECT *)base;

	/* File TMDs store offsets relative to the object header; the
	 * dynamic builder stores absolute pointers. */
	if (o->vert_top < 0x100000)
		verts = (VERTEX *)(base + o->vert_top);
	else
		verts = (VERTEX *)o->vert_top;

	if (o->primitive_top < 0x100000)
		prim = (u_long *)(base + o->primitive_top);
	else
		prim = (u_long *)o->primitive_top;

	nprim = o->n_primitive;
	if (!verts || !prim || nprim <= 0)
		return;

	for (i = 0; i < nprim; i++) {
		u_long hdr = prim[0];
		long olen = (long)(hdr & 0xFF);
		long mode = (long)((hdr >> 24) & 0xFF);
		u_long cmd = ((u_long)mode) << 24;
		long sx0, sy0, sx1, sy1, sx2, sy2, sx3, sy3;
		long d0, d1, d2, d3;
		long dep = 0;
		u_long pk[12];
		long n = 0;

		if (olen < 1)
			break;

		switch (mode & 0xFC) {
		case 0x20: {	/* F3 - flat triangle */
			u_short v0 = prim[2] & 0xFFFF;
			u_short v1 = prim[2] >> 16;
			u_short v2 = prim[3] & 0xFFFF;

			tmd_tri(verts, v0, v1, v2, &sx0, &sy0, &sx1, &sy1,
				&sx2, &sy2, &d0, &d1, &d2);
			if (d0 > 0 && d1 > 0 && d2 > 0) {
				pk[n++] = cmd | (prim[1] & 0xFFFFFF);
				pk[n++] = TMD_XY(sx0, sy0);
				pk[n++] = TMD_XY(sx1, sy1);
				pk[n++] = TMD_XY(sx2, sy2);
				dep = (d0 + d1 + d2) / 3;
			}
			break;
		}
		case 0x24: {	/* FT3 - flat textured triangle */
			u_short v0 = prim[5] & 0xFFFF;
			u_short v1 = prim[5] >> 16;
			u_short v2 = prim[6] & 0xFFFF;
			u_long cba = (prim[1] >> 16) & 0xFFFF;
			u_long tsb = (prim[2] >> 16) & 0xFFFF;
			u_long uv0 = TMD_UV(prim[1] & 0xFF, (prim[1] >> 8) & 0xFF);
			u_long uv1 = TMD_UV(prim[2] & 0xFF, (prim[2] >> 8) & 0xFF);
			u_long uv2 = TMD_UV(prim[3] & 0xFF, (prim[3] >> 8) & 0xFF);

			tmd_tri(verts, v0, v1, v2, &sx0, &sy0, &sx1, &sy1,
				&sx2, &sy2, &d0, &d1, &d2);
			if (d0 > 0 && d1 > 0 && d2 > 0) {
				pk[n++] = cmd | (prim[4] & 0xFFFFFF);
				pk[n++] = TMD_XY(sx0, sy0);
				pk[n++] = uv0 | (cba << 16);
				pk[n++] = TMD_XY(sx1, sy1);
				pk[n++] = uv1 | (tsb << 16);
				pk[n++] = TMD_XY(sx2, sy2);
				pk[n++] = uv2;
				dep = (d0 + d1 + d2) / 3;
			}
			break;
		}
		case 0x28: {	/* F4 - flat quad */
			u_short v0 = prim[2] & 0xFFFF;
			u_short v1 = prim[2] >> 16;
			u_short v2 = prim[3] & 0xFFFF;
			u_short v3 = prim[3] >> 16;

			tmd_quad(verts, v0, v1, v2, v3, &sx0, &sy0, &sx1, &sy1,
				 &sx2, &sy2, &sx3, &sy3, &d0, &d1, &d2, &d3);
			if (d0 > 0 && d1 > 0 && d2 > 0 && d3 > 0) {
				pk[n++] = cmd | (prim[1] & 0xFFFFFF);
				pk[n++] = TMD_XY(sx0, sy0);
				pk[n++] = TMD_XY(sx1, sy1);
				pk[n++] = TMD_XY(sx2, sy2);
				pk[n++] = TMD_XY(sx3, sy3);
				dep = (d0 + d1 + d2 + d3) / 4;
			}
			break;
		}
		case 0x2C: {	/* FT4 - flat textured quad */
			u_short v0 = prim[6] & 0xFFFF;
			u_short v1 = prim[6] >> 16;
			u_short v2 = prim[7] & 0xFFFF;
			u_short v3 = prim[7] >> 16;
			u_long cba = (prim[1] >> 16) & 0xFFFF;
			u_long tsb = (prim[2] >> 16) & 0xFFFF;
			u_long uv0 = TMD_UV(prim[1] & 0xFF, (prim[1] >> 8) & 0xFF);
			u_long uv1 = TMD_UV(prim[2] & 0xFF, (prim[2] >> 8) & 0xFF);
			u_long uv2 = TMD_UV(prim[3] & 0xFF, (prim[3] >> 8) & 0xFF);
			u_long uv3 = TMD_UV(prim[4] & 0xFF, (prim[4] >> 8) & 0xFF);

			tmd_quad(verts, v0, v1, v2, v3, &sx0, &sy0, &sx1, &sy1,
				 &sx2, &sy2, &sx3, &sy3, &d0, &d1, &d2, &d3);
			if (d0 > 0 && d1 > 0 && d2 > 0 && d3 > 0) {
				pk[n++] = cmd | (prim[5] & 0xFFFFFF);
				pk[n++] = TMD_XY(sx0, sy0);
				pk[n++] = uv0 | (cba << 16);
				pk[n++] = TMD_XY(sx1, sy1);
				pk[n++] = uv1 | (tsb << 16);
				pk[n++] = TMD_XY(sx2, sy2);
				pk[n++] = uv2;
				pk[n++] = TMD_XY(sx3, sy3);
				pk[n++] = uv3;
				dep = (d0 + d1 + d2 + d3) / 4;
			}
			break;
		}
		case 0x30: {	/* G3 - gouraud triangle */
			u_short v0 = prim[4] & 0xFFFF;
			u_short v1 = prim[4] >> 16;
			u_short v2 = prim[5] & 0xFFFF;

			tmd_tri(verts, v0, v1, v2, &sx0, &sy0, &sx1, &sy1,
				&sx2, &sy2, &d0, &d1, &d2);
			if (d0 > 0 && d1 > 0 && d2 > 0) {
				pk[n++] = cmd | (prim[1] & 0xFFFFFF);
				pk[n++] = TMD_XY(sx0, sy0);
				pk[n++] = prim[2] & 0xFFFFFF;
				pk[n++] = TMD_XY(sx1, sy1);
				pk[n++] = prim[3] & 0xFFFFFF;
				pk[n++] = TMD_XY(sx2, sy2);
				dep = (d0 + d1 + d2) / 3;
			}
			break;
		}
		case 0x34: {	/* GT3 - gouraud textured triangle */
			u_short v0 = prim[7] & 0xFFFF;
			u_short v1 = prim[7] >> 16;
			u_short v2 = prim[8] & 0xFFFF;
			u_long cba = (prim[1] >> 16) & 0xFFFF;
			u_long tsb = (prim[2] >> 16) & 0xFFFF;
			u_long uv0 = TMD_UV(prim[1] & 0xFF, (prim[1] >> 8) & 0xFF);
			u_long uv1 = TMD_UV(prim[2] & 0xFF, (prim[2] >> 8) & 0xFF);
			u_long uv2 = TMD_UV(prim[3] & 0xFF, (prim[3] >> 8) & 0xFF);

			tmd_tri(verts, v0, v1, v2, &sx0, &sy0, &sx1, &sy1,
				&sx2, &sy2, &d0, &d1, &d2);
			if (d0 > 0 && d1 > 0 && d2 > 0) {
				pk[n++] = cmd | (prim[4] & 0xFFFFFF);
				pk[n++] = TMD_XY(sx0, sy0);
				pk[n++] = uv0 | (cba << 16);
				pk[n++] = prim[5] & 0xFFFFFF;
				pk[n++] = TMD_XY(sx1, sy1);
				pk[n++] = uv1 | (tsb << 16);
				pk[n++] = prim[6] & 0xFFFFFF;
				pk[n++] = TMD_XY(sx2, sy2);
				pk[n++] = uv2;
				dep = (d0 + d1 + d2) / 3;
			}
			break;
		}
		case 0x38: {	/* G4 - gouraud quad */
			u_short v0 = prim[5] & 0xFFFF;
			u_short v1 = prim[5] >> 16;
			u_short v2 = prim[6] & 0xFFFF;
			u_short v3 = prim[6] >> 16;

			tmd_quad(verts, v0, v1, v2, v3, &sx0, &sy0, &sx1, &sy1,
				 &sx2, &sy2, &sx3, &sy3, &d0, &d1, &d2, &d3);
			if (d0 > 0 && d1 > 0 && d2 > 0 && d3 > 0) {
				pk[n++] = cmd | (prim[1] & 0xFFFFFF);
				pk[n++] = TMD_XY(sx0, sy0);
				pk[n++] = prim[2] & 0xFFFFFF;
				pk[n++] = TMD_XY(sx1, sy1);
				pk[n++] = prim[3] & 0xFFFFFF;
				pk[n++] = TMD_XY(sx2, sy2);
				pk[n++] = prim[4] & 0xFFFFFF;
				pk[n++] = TMD_XY(sx3, sy3);
				dep = (d0 + d1 + d2 + d3) / 4;
			}
			break;
		}
		case 0x3C: {	/* GT4 - gouraud textured quad */
			u_short v0 = prim[9] & 0xFFFF;
			u_short v1 = prim[9] >> 16;
			u_short v2 = prim[10] & 0xFFFF;
			u_short v3 = prim[10] >> 16;
			u_long cba = (prim[1] >> 16) & 0xFFFF;
			u_long tsb = (prim[2] >> 16) & 0xFFFF;
			u_long uv0 = TMD_UV(prim[1] & 0xFF, (prim[1] >> 8) & 0xFF);
			u_long uv1 = TMD_UV(prim[2] & 0xFF, (prim[2] >> 8) & 0xFF);
			u_long uv2 = TMD_UV(prim[3] & 0xFF, (prim[3] >> 8) & 0xFF);
			u_long uv3 = TMD_UV(prim[4] & 0xFF, (prim[4] >> 8) & 0xFF);

			tmd_quad(verts, v0, v1, v2, v3, &sx0, &sy0, &sx1, &sy1,
				 &sx2, &sy2, &sx3, &sy3, &d0, &d1, &d2, &d3);
			if (d0 > 0 && d1 > 0 && d2 > 0 && d3 > 0) {
				pk[n++] = cmd | (prim[5] & 0xFFFFFF);
				pk[n++] = TMD_XY(sx0, sy0);
				pk[n++] = uv0 | (cba << 16);
				pk[n++] = prim[6] & 0xFFFFFF;
				pk[n++] = TMD_XY(sx1, sy1);
				pk[n++] = uv1 | (tsb << 16);
				pk[n++] = prim[7] & 0xFFFFFF;
				pk[n++] = TMD_XY(sx2, sy2);
				pk[n++] = uv2;
				pk[n++] = prim[8] & 0xFFFFFF;
				pk[n++] = TMD_XY(sx3, sy3);
				pk[n++] = uv3;
				dep = (d0 + d1 + d2 + d3) / 4;
			}
			break;
		}
		default:
			break;
		}

		if (n)
			emit_poly(ot, shift, dep, pk, n);

		prim += olen;
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
	gp0_send(((unsigned int)src->y << 16) | (unsigned int)(u_short)src->x);
	gp0_send(((unsigned int)src->h << 16) | (unsigned int)(u_short)src->w);
	gp0_send(((unsigned int)dest_y << 16) | (unsigned int)dest_x);
}

void LoadImage(RECT *rect, u_long *data)
{
	long x = rect->x;
	long y = rect->y;
	long w = rect->w;
	long h = rect->h;
	long i;

	if (w <= 0 || h <= 0)
		return;

	/* GP0 A0h: CPU->VRAM transfer */
	gp0_send(0xA0000000);
	gp0_send(((unsigned int)y << 16) | (unsigned int)x);
	gp0_send(((unsigned int)h << 16) | (unsigned int)w);

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
	 * tim points at the TIM ID word.  Layout:
	 *
	 *   +00  u_long  id          0x00000010
	 *   +04  u_long  flags       bits 0-1: pixel mode
	 *                            (0=4bpp, 1=8bpp, 2=16bpp, 3=24bpp)
	 *   +08  u_long  clut_len    CLUT block size in bytes, header
	 *                            included (only for 4/8bpp)
	 *   +0c  RECT    clut_rect   x, y, w, h
	 *   +14  ...     clut data
	 *   ...  u_long  pix_len     pixel block size in bytes, header
	 *                            included
	 *   ...  RECT    pix_rect
	 *   ...  ...     pixel data
	 *
	 * Note the rect widths and heights are in 16-bit VRAM units, so
	 * the pixel data of a block is always w*h*2 bytes and LoadImage()
	 * consumes w*h/2 words.
	 */
	extern int printf(const char *fmt, ...);
	u_long flags;
	u_long *p;
	long pmode;
	u_long len;
	RECT *cr;
	RECT *pr;

	flags = tim[1];
	p = &tim[2];
	pmode = (long)(flags & 3);

	info->pmode = pmode;

	/* Indexed modes (4bpp and 8bpp) carry a colour lookup table. */
	if (pmode < 2) {
		len = p[0];
		cr = (RECT *)&p[1];

		info->crect = cr;
		info->cx = cr->x;
		info->cy = cr->y;
		info->cw = cr->w;
		info->ch = cr->h;
		info->clut = (u_long *)((char *)p + 12);
		info->clutlen = len - 12;

		p = (u_long *)((char *)p + len);
	} else {
		info->crect = 0;
		info->clut = 0;
		info->cx = info->cy = info->cw = info->ch = 0;
		info->clutlen = 0;
	}

	len = p[0];
	pr = (RECT *)&p[1];

	info->rect = pr;
	info->px = pr->x;
	info->py = pr->y;
	info->pw = pr->w;
	info->ph = pr->h;
	info->pixel = (u_long *)((char *)p + 12);
	info->pixlen = len - 12;
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
