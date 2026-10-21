/*
 * minigl.c — Software implementation of the GLTron 0.53 OpenGL subset
 * on the PlayStation GPU.
 *
 * Pipeline: vertices accumulated by glBegin/glEnd are transformed by
 * the modelview matrix (soft-float), lit by one GL light (ambient +
 * diffuse), fogged (GL_LINEAR), projected, near-plane clipped, then
 * emitted as PSX GPU packets into an ordering table so the hardware
 * draws them back-to-front.  Textures live in VRAM as 15bpp halfword
 * blocks; binding selects the texture page and, for GL_REPEAT, a GPU
 * texture window that makes the mapping wrap per fragment.
 *
 * Packet word orders (verified against this emulator's GPU plugin,
 * see libps_compat.c GsSortObject4 for the same encoding):
 *
 *   flat tri   0x20 [cmd|rgb] [xy] [xy] [xy]
 *   flat quad  0x28 [cmd|rgb] [xy] [xy] [xy] [xy]
 *   gouraud    0x30/0x38 [cmd|c0] [xy0] [c1] [xy1] ...
 *   tex tri    0x24 [cmd|rgb] [xy0] [uv0|cba] [xy1] [uv1|tpb] [xy2] [uv2]
 *   tex quad   0x2C [cmd|rgb] [xy0] [uv0|cba] [xy1] [uv1|tpb] [xy2] [uv2]
 *                   [xy3] [uv3]
 *   gouraud tex 0x34/0x3C: as above with a colour word before each xy
 *   line       0x40 [cmd|rgb] [xy0] [xy1]
 *
 * Semi-transparency = command bit 0 (0x21/0x29/0x31/0x25/0x2D/0x41).
 *
 * Depth note: the PSX has no depth buffer; ordering comes from an OT
 * bucketed by the clip-space w (which equals the eye distance for
 * the perspective projection, and is constant for 2D ortho overlays
 * so they sort on top of the 3D scene).
 */

#include "minigl.h"

typedef unsigned int   u32;
typedef unsigned short u16;
typedef unsigned char  u8;
typedef unsigned long  u_long;

/* soft-float math from minilibc (no FPU on the R3000A); all
 * single precision — double soft-float costs ~10x per operation */
extern float sin(float);
extern float cos(float);
extern float tan(float);
extern float sqrt(float);
extern int printf(const char *fmt, ...);

/* ------------------------------------------------------- /dev/psxgpu
 *
 * All GPU access goes through the kernel's /dev/psxgpu character
 * device (major 210, see include/linux/psxgpu.h and
 * drivers/char/psxgpu.c).  GP0 command words are batched and pushed
 * with one write() per batch; GP1 control words and GPUSTAT reads
 * are ioctls.
 */

#include "psxsys.h"

#ifndef O_RDWR
#define O_RDWR 2
#endif

/* ioctl numbers — MIPS o32 _IOC encoding (dir<<29 | size<<16 |
 * type<<8 | nr), must match include/linux/psxgpu.h */
#define PSXGPU_IOC_GP1          0x80044701u  /* _IOW('G',1,uint) */
#define PSXGPU_IOC_STAT         0x40044702u  /* _IOR('G',2,uint) */
#define PSXGPU_IOC_VSYNC        0x20004703u  /* _IO ('G',3)     */
#define PSXGPU_IOC_UPLOAD_VRAM  0x80144704u  /* _IOW('G',4,xfer)*/
#define PSXGPU_IOC_RESET        0x20004705u  /* _IO ('G',5)     */

/* VRAM transfer descriptor (must match struct psxgpu_vram_xfer) */
struct psxgpu_vram_xfer {
	u32 x;			/* VRAM x (pixels) */
	u32 y;			/* VRAM y (pixels) */
	u32 w;			/* width (halfwords) */
	u32 h;			/* height */
	const void *data;	/* 16bpp pixel data */
};

#define GPUSTAT_FIFO   (1u << 28)   /* ready to receive a command word */
#define GPUSTAT_VBLANK (1u << 31)

static sint gpu_fd = -1;

/* GP0 batching: one write() syscall amortizes the FIFO waits for a
 * whole batch of command words. */
/* 512-word write batches: the kernel driver re-batches internally,
 * so this only sets the syscall granularity.  Kept small because
 * the buffer is BSS and the image-size budget is tight (see
 * ARENA_WORDS above). */
#define GP0_BATCH 512
static u32 gp0_batch[GP0_BATCH];
static int gp0_batch_n;

/* Near-plane clip threshold, Q16.16: the eye-space distance (clip w)
 * below which geometry is clipped away.  Kept in sync with the
 * projection's zNear by gluPerspective().  A near-clip at ~0 let
 * polygons reaching to a few thousandths of a unit in front of the
 * eye through: their projected coordinates explode (w divides to a
 * tiny denominator), xy_word clamps them to 0..1023, and whatever
 * texture crossed the camera - a floor tile at the camera's feet,
 * the crash quad at the lens - smeared across the whole screen.
 * Zero-initialized (BSS) on purpose: the exec loader maps .data as
 * empty, so a non-zero initializer would load as garbage;
 * glMiniInit() sets the 1.0 default. */
static int W_EPS_Q;

/* Last texture-window (GP0(E2h)) word emitted to the GPU; ot_emit()
 * dedupes window changes while draining the ordering table.  Also
 * BSS-zeroed - glMiniInit() resets it. */
static u32 last_twin;
static u32 last_e1;             /* last GP0(E1h) draw-mode echo */

static int gpu_open(void)
{
	if (gpu_fd < 0)
		gpu_fd = sys_open("/dev/psxgpu", O_RDWR);
	return gpu_fd;
}

static void gp0_flush(void)
{
	if (gp0_batch_n > 0) {
		sys_write(gpu_fd, gp0_batch, gp0_batch_n * 4);
		gp0_batch_n = 0;
	}
}

static void gp0_send(u32 w)
{
	gp0_batch[gp0_batch_n++] = w;
	if (gp0_batch_n == GP0_BATCH)
		gp0_flush();
}

static void gp1_send(u32 w)
{
	gp0_flush();
	sys_ioctl(gpu_fd, PSXGPU_IOC_GP1, (sint)w);
}

static u32 gpu_stat(void)
{
	gp0_flush();
	return (u32)sys_ioctl(gpu_fd, PSXGPU_IOC_STAT, 0);
}

/* ------------------------------------------------------------ VRAM layout
 *   x   0..255 : texture pages 0..3 (64x256 each, 15bpp halfwords)
 *   x 320..639 : framebuffers — buf0 (320x240) at y 0, buf1 at y 240
 */
#define SCREEN_W 320
#define SCREEN_H 240

#define FB0_X 320
#define FB0_Y 0
#define FB1_X 320
#define FB1_Y 240

static int active_buff;          /* buffer currently displayed */

static void set_draw_target(int idx)
{
	int x = (idx == 0) ? FB0_X : FB1_X;
	int y = (idx == 0) ? FB0_Y : FB1_Y;

	gp0_send(0xE3000000u | ((u32)(y & 0x1FF) << 10) | (u32)(x & 0x3FF));
	gp0_send(0xE4000000u |
		  ((u32)((y + SCREEN_H - 1) & 0x1FF) << 10) |
		  (u32)((x + SCREEN_W - 1) & 0x3FF));
	gp0_send(0xE5000000u |
		  ((u32)(y & 0x7FF) << 11) | (u32)(x & 0x7FF));
}

static void show_buffer(int idx)
{
	int x = (idx == 0) ? FB0_X : FB1_X;
	int y = (idx == 0) ? FB0_Y : FB1_Y;

	gp1_send(0x05000000u | ((u32)y << 10) | (u32)x);
}

/* ------------------------------------------------------------ OT + arena */

#define OT_BITS 10
#define OT_SIZE (1 << OT_BITS)
/*
 * Packet arena.  A frame builds ~800 packets of 4..12 words
 * (floor grid ~440 quads, wall segments, trails, bikes, HUD);
 * 12k words is a generous ceiling.  The arena is BSS and the
 * user-program image must stay below ~0x1AA0A0 so the 24K stack
 * top (image end + 0x6000) fits inside the kernel's 0x1B0000
 * reservation — the old 24k-word arena alone blew that budget by
 * pushing the stack into memory the boot allocator had given away
 * (the symptom was a dead /dev/psxgpu: all-black frames).
 */
#define ARENA_WORDS 12288

static u_long  ot[2][OT_SIZE];   /* linked-list heads, one per buffer */
static u_long  pkt_arena[ARENA_WORDS];
static u_long *pkt_top;

static void ot_reset(int buff)
{
	int i;

	for (i = 0; i < OT_SIZE; i++)
		ot[buff][i] = 0;
	pkt_top = pkt_arena;
}

/* Packet layout: p[0] = OT link, p[1] = word count, p[2] = texture
 * window word (GP0(E2h), 0 = leave alone), p[3] = draw-mode echo
 * word (GP0(E1h), 0 = leave alone; carries the semi-transparency
 * rate for untextured prims), p[4..] = the GP0 primitive words. */
static u_long *pkt_new(int words)
{
	u_long *p;

	if (pkt_top + words + 4 >= pkt_arena + ARENA_WORDS)
		return 0;
	p = pkt_top;
	p[1] = words;
	pkt_top += words + 4;
	return p;
}

static void pkt_link(int buff, int bucket, u_long *node)
{
	if (bucket < 0)
		bucket = 0;
	if (bucket >= OT_SIZE)
		bucket = OT_SIZE - 1;
	node[0] = ot[buff][bucket];
	ot[buff][bucket] = (u_long)node;
}

/* per-frame packet counter for the profiler in miniglut.c */
static u32 pkts_now, pkts_last;

u32 glStatPackets(void)
{
	return pkts_last;
}

static void ot_emit(int buff)
{
	/* The GPU's texture window and draw mode are global state;
	 * each packet carries the words it needs and we emit them
	 * only on change while draining the table.  last_twin and
	 * last_e1 live at file scope (BSS-zeroed; glMiniInit resets
	 * them) because the exec loader maps .data empty - an
	 * initializer would load as garbage. */
	int i;

	for (i = OT_SIZE - 1; i >= 0; i--) {
		u_long *p;
		for (p = (u_long *)ot[buff][i]; p; p = (u_long *)p[0]) {
			int n = (int)p[1], j;
			u32 twin = (u32)p[2];
			u32 e1 = (u32)p[3];

			if (twin != 0 && twin != last_twin) {
				gp0_send(twin);
				last_twin = twin;
			}
			if (e1 != 0 && e1 != last_e1) {
				gp0_send(e1);
				last_e1 = e1;
			}
			for (j = 0; j < n; j++)
				gp0_send((u32)p[4 + j]);
		}
	}
}

/* ------------------------------------------------------------- matrices */
/*
 * Matrices are kept in Q16.16 fixed point natively.  The R3000A has
 * no FPU and a float 4x4 multiply costs ~112 soft-float library
 * calls (~35k emulated cycles); the integer version is ~64 64-bit
 * multiplies (~700 cycles).  Entries are converted once per GL
 * matrix call (f2q), which is where the per-frame cost moved:
 * glRotatef/glTranslatef now cost a handful of conversions instead
 * of two float matrix multiplies.
 */
static int f2q(float f, int bits);      /* defined with the pipeline */

typedef struct {
	int m[16];        /* column-major, GL style, Q16.16 */
} Mat4;

static const Mat4 ident_mat = {
	{ 65536, 0, 0, 0,
	  0, 65536, 0, 0,
	  0, 0, 65536, 0,
	  0, 0, 0, 65536 }
};

static Mat4 mv_stack[8];
static int  mv_sp;
static Mat4 mv;
static Mat4 proj;
static GLenum cur_matrix = GL_MODELVIEW;

static void mat_apply(Mat4 *a, const Mat4 *b)
{
	int r[16];
	int c, i, k;

	for (c = 0; c < 4; c++) {
		for (i = 0; i < 4; i++) {
			int s = 0;
			for (k = 0; k < 4; k++)
				s += (int)(((long long)a->m[k * 4 + i] *
					    b->m[c * 4 + k]) >> 16);
			r[c * 4 + i] = s;
		}
	}
	for (i = 0; i < 16; i++)
		a->m[i] = r[i];
}

void glMatrixMode(GLenum mode)
{
	cur_matrix = mode;
}

void glPushMatrix(void)
{
	if (cur_matrix == GL_MODELVIEW) {
		if (mv_sp < 8)
			mv_stack[mv_sp++] = mv;
	}
}

void glPopMatrix(void)
{
	if (cur_matrix == GL_MODELVIEW) {
		if (mv_sp > 0)
			mv = mv_stack[--mv_sp];
	}
}

void glLoadIdentity(void)
{
	if (cur_matrix == GL_MODELVIEW)
		mv = ident_mat;
	else
		proj = ident_mat;
}

void glLoadMatrixf(const GLfloat *m)
{
	Mat4 *t = (cur_matrix == GL_MODELVIEW) ? &mv : &proj;
	int i;

	for (i = 0; i < 16; i++)
		t->m[i] = f2q(m[i], 16);
}

void glTranslatef(GLfloat x, GLfloat y, GLfloat z)
{
	Mat4 t = ident_mat;

	t.m[12] = f2q(x, 16);
	t.m[13] = f2q(y, 16);
	t.m[14] = f2q(z, 16);
	if (cur_matrix == GL_MODELVIEW)
		mat_apply(&mv, &t);
	else
		mat_apply(&proj, &t);
}

void glRotatef(GLfloat deg, GLfloat ax, GLfloat ay, GLfloat az)
{
	float len, inv, s, c, t, x, y, z;
	Mat4 r = ident_mat;

	len = (float)sqrt((double)(ax * ax + ay * ay + az * az));
	if (len <= 0.0f)
		return;
	inv = 1.0f / len;
	x = ax * inv; y = ay * inv; z = az * inv;

	s = sin(deg * 0.017453292519943295f);
	c = cos(deg * 0.017453292519943295f);
	t = 1.0f - c;

	r.m[0]  = f2q(x * x * t + c, 16);
	r.m[4]  = f2q(y * x * t + z * s, 16);
	r.m[8]  = f2q(z * x * t - y * s, 16);

	r.m[1]  = f2q(x * y * t - z * s, 16);
	r.m[5]  = f2q(y * y * t + c, 16);
	r.m[9]  = f2q(z * y * t + x * s, 16);

	r.m[2]  = f2q(x * z * t + y * s, 16);
	r.m[6]  = f2q(y * z * t - x * s, 16);
	r.m[10] = f2q(z * z * t + c, 16);

	if (cur_matrix == GL_MODELVIEW)
		mat_apply(&mv, &r);
	else
		mat_apply(&proj, &r);
}

void glScalef(GLfloat x, GLfloat y, GLfloat z)
{
	Mat4 s = ident_mat;

	s.m[0] = f2q(x, 16);
	s.m[5] = f2q(y, 16);
	s.m[10] = f2q(z, 16);
	if (cur_matrix == GL_MODELVIEW)
		mat_apply(&mv, &s);
	else
		mat_apply(&proj, &s);
}

void glOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t,
	     GLdouble n, GLdouble f)
{
	Mat4 o = ident_mat;

	o.m[0]  = f2q(2.0f / (float)(r - l), 16);
	o.m[5]  = f2q(2.0f / (float)(t - b), 16);
	o.m[10] = f2q(-2.0f / (float)(f - n), 16);
	o.m[12] = f2q(-((float)(r + l)) / (float)(r - l), 16);
	o.m[13] = f2q(-((float)(t + b)) / (float)(t - b), 16);
	o.m[14] = f2q(-((float)(f + n)) / (float)(f - n), 16);
	if (cur_matrix == GL_MODELVIEW)
		mat_apply(&mv, &o);
	else
		mat_apply(&proj, &o);
}

void gluPerspective(GLdouble fovy, GLdouble aspect,
		    GLdouble zNear, GLdouble zFar)
{
	Mat4 p;
	float f;
	int i;

	f = 1.0f / tan((float)(fovy * 3.14159265358979323846 / 360.0));

	/* the clipper cuts at the frustum's near plane, matching what
	 * this projection assumes (see W_EPS_Q above) */
	W_EPS_Q = f2q((float)zNear, 16);
	if (W_EPS_Q < 2)
		W_EPS_Q = 2;

	for (i = 0; i < 16; i++)
		p.m[i] = 0;
	p.m[0]  = f2q(f / (float)aspect, 16);
	p.m[5]  = f2q(f, 16);
	p.m[10] = f2q(((float)zFar + (float)zNear) /
		      ((float)zNear - (float)zFar), 16);
	p.m[11] = -65536;                       /* -1.0 */
	p.m[14] = f2q((2.0f * (float)zFar * (float)zNear) /
		      ((float)zNear - (float)zFar), 16);

	if (cur_matrix == GL_MODELVIEW)
		mat_apply(&mv, &p);
	else
		mat_apply(&proj, &p);
}

void gluLookAt(GLdouble ex, GLdouble ey, GLdouble ez,
	       GLdouble cx, GLdouble cy, GLdouble cz,
	       GLdouble ux, GLdouble uy, GLdouble uz)
{
	float fx, fy, fz, sx, sy, sz, rx, ry, rz;
	Mat4 m = ident_mat;

	fx = (float)cx - (float)ex;
	fy = (float)cy - (float)ey;
	fz = (float)cz - (float)ez;

	sx = fy * (float)uz - fz * (float)uy;
	sy = fz * (float)ux - fx * (float)uz;
	sz = fx * (float)uy - fy * (float)ux;

	rx = sy * fz - sz * fy;
	ry = sz * fx - sx * fz;
	rz = sx * fy - sy * fx;

	{
		float n;

		n = sqrt(fx * fx + fy * fy + fz * fz);
		if (n > 0.0f) { fx /= n; fy /= n; fz /= n; }
		n = sqrt(sx * sx + sy * sy + sz * sz);
		if (n > 0.0f) { sx /= n; sy /= n; sz /= n; }
		n = sqrt(rx * rx + ry * ry + rz * rz);
		if (n > 0.0f) { rx /= n; ry /= n; rz /= n; }
	}

	m.m[0] = f2q(sx, 16);   m.m[4] = f2q(sy, 16);   m.m[8]  = f2q(sz, 16);
	m.m[1] = f2q(rx, 16);   m.m[5] = f2q(ry, 16);   m.m[9]  = f2q(rz, 16);
	m.m[2] = f2q(-fx, 16);  m.m[6] = f2q(-fy, 16);  m.m[10] = f2q(-fz, 16);

	m.m[12] = f2q(-(sx * (float)ex + sy * (float)ey + sz * (float)ez), 16);
	m.m[13] = f2q(-(rx * (float)ex + ry * (float)ey + rz * (float)ez), 16);
	/* NB: row 3 is -f, so the translation is +f*eye (not -f*eye):
	 * eye-space z = -f.p + f.eye = -f.(p - eye);  with the wrong
	 * sign every point in front of the camera lands behind the
	 * near plane and clip_near() culls the whole scene. */
	m.m[14] = f2q(fx * (float)ex + fy * (float)ey + fz * (float)ez, 16);

	if (cur_matrix == GL_MODELVIEW)
		mat_apply(&mv, &m);
	else
		mat_apply(&proj, &m);
}

/* ----------------------------------------------------------------- state */

static int st_blend;
static int st_lighting;
static int st_fog;
static int st_texture_2d;
static int st_shade_smooth = 1;
static int st_depth_test;
static int st_depth_mask = 1;

static GLfloat clear_color[4];

static int    cur_color[4] = { 255, 255, 255, 255 };   /* 0..255 */
static int    cur_uv[2];                               /* Q8.8 */
static int    cur_normal[3] = { 0, 0, 16384 };         /* Q2.14 */

static GLfloat mat_ambient[4]  = { 0.2f, 0.2f, 0.2f, 1.0f };
static GLfloat mat_diffuse[4]  = { 0.8f, 0.8f, 0.8f, 1.0f };
static GLfloat mat_specular[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

static GLfloat light_ambient[4];
static GLfloat light_diffuse[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
static GLfloat light_pos[4];           /* eye-space, as set */
static int      light0_on;

static GLfloat fog_start = 0.0f, fog_end = 1.0f;
static GLfloat fog_color[4];

/* set by the light/material/fog setters; the fixed-point lighting
 * and fog coefficients are refreshed lazily at the next glEnd */
static int fx_dirty = 1;

static GLint vp_x, vp_y;
static GLint vp_w = SCREEN_W, vp_h = SCREEN_H;

/* --------------------------------------------------------------- textures */

#define MAX_TEX 16

typedef struct {
	int used;
	int w, h;
	int vram_x, vram_y;         /* top-left in VRAM halfwords */
	u16 tpage_bits;              /* tpage field for the TSB word */
	u32 twin_word;               /* GP0(E2h) texture-window word for this tex */
	int wrap_s, wrap_t;          /* GL_REPEAT / GL_CLAMP */
} Tex;

static Tex texs[MAX_TEX];
static Tex *bound_tex;
static int  tex_env_modulate = 1;   /* GL_MODULATE default */
static int  st_abr;                /* 1: additive (GL_ONE,GL_ONE) */
static GLuint next_tex = 1;

static int vram_next_x;              /* allocator marches across VRAM */
static int vram_next_y;

static u32 encode_tpage(int x, int y)
{
	/* 15bpp direct: mode bits = 2<<7; page = x/64, y/256 */
	return ((u32)((x / 64) & 0xF)) | (((u32)((y / 256) & 1)) << 4) |
	       (2u << 7);
}

static void tex_upload(const Tex *t, const unsigned short *hw)
{
	struct psxgpu_vram_xfer xf;

	/* one ioctl: the kernel drives the GP0(A0h) CPU->VRAM blit
	 * and copies the pixel data from this buffer */
	gp0_flush();
	xf.x = t->vram_x;
	xf.y = t->vram_y;
	xf.w = t->w;
	xf.h = t->h;
	xf.data = hw;
	sys_ioctl(gpu_fd, PSXGPU_IOC_UPLOAD_VRAM, (sint)&xf);
}

void glGenTextures(GLsizei n, GLuint *textures)
{
	int i;

	for (i = 0; i < n; i++)
		textures[i] = next_tex++;
}

void glDeleteTextures(GLsizei n, const GLuint *textures)
{
	int i;

	for (i = 0; i < n; i++) {
		if (textures[i] < MAX_TEX)
			texs[textures[i]].used = 0;
	}
}

void glBindTexture(GLenum target, GLuint id)
{
	u32 mx, my;
	Tex *t;

	(void)target;
	if (id < MAX_TEX && texs[id].used) {
		bound_tex = &texs[id];
		t = bound_tex;
		/* Texture window (GP0(E2h)): the word is NOT sent here.
		 * Primitives go into the ordering table and are drained
		 * by ot_emit() at swap time, so an E2 written at bind
		 * time would take effect before this frame's packets -
		 * i.e. every quad would render under the window of
		 * whichever texture was bound LAST.  Instead every
		 * textured packet carries its texture's window word in
		 * its header and ot_emit() emits it on change.
		 *
		 * GL_REPEAT maps the power-of-two size; the E2h mask
		 * fields are in 8-pixel steps (mask value v wraps UVs
		 * at v*8 texels).  GL_CLAMP leaves the fields zero:
		 * mask 0 = window off, UVs pass through unchanged.
		 * NB: this is E2h, NOT E6h - E6h is the mask-bit
		 * setting ("reject pixels with bit15 set"), which
		 * breaks drawing. */
		if (t->wrap_s == GL_REPEAT)
			mx = (t->w <= 8) ? 31 : (t->w <= 16) ? 30 :
			      (t->w <= 32) ? 28 : (t->w <= 64) ? 24 :
			      (t->w <= 128) ? 16 : 0;
		else
			mx = 0;
		if (t->wrap_t == GL_REPEAT)
			my = (t->h <= 8) ? 31 : (t->h <= 16) ? 30 :
			      (t->h <= 32) ? 28 : (t->h <= 64) ? 24 :
			      (t->h <= 128) ? 16 : 0;
		else
			my = 0;
		t->twin_word = 0xE2000000u | mx | (my << 5);
	} else {
		bound_tex = 0;
	}
}

void glTexImage2DPSX(GLuint texture, GLsizei width, GLsizei height,
		     const unsigned short *halfwords)
{
	Tex *t;

	if (texture >= MAX_TEX || width < 1 || height < 1)
		return;
	t = &texs[texture];
	t->used = 1;
	t->w = width;
	t->h = height;
	t->wrap_s = GL_REPEAT;
	t->wrap_t = GL_REPEAT;

	/* allocate VRAM in the texture area (x 0..319); the
	 * framebuffers own x 320..639 */
	if (vram_next_x + width > 320) {
		vram_next_x = 0;
		vram_next_y += 256;
		if (vram_next_y + height > 512)
			vram_next_y = 0;
	}
	t->vram_x = vram_next_x;
	t->vram_y = vram_next_y;
	vram_next_x += width;

	t->tpage_bits = (u16)encode_tpage(t->vram_x, t->vram_y);
	tex_upload(t, halfwords);
}

void glTexImage2D(GLenum target, GLint level, GLint internalformat,
		  GLsizei width, GLsizei height, GLint border,
		  GLenum format, GLenum type, const GLvoid *data)
{
	const GLubyte *px = (const GLubyte *)data;
	static unsigned short hw[64 * 256];
	int i, n;

	(void)target; (void)level; (void)internalformat;
	(void)border; (void)format; (void)type;
	if (!bound_tex || !data)
		return;
	n = width * height;
	if (n > 64 * 256)
		n = 64 * 256;
	for (i = 0; i < n; i++) {
		unsigned r = px[i * 4 + 0];
		unsigned g = px[i * 4 + 1];
		unsigned b = px[i * 4 + 2];
		unsigned a = px[i * 4 + 3];
		unsigned short v;

		v = (unsigned short)((r >> 3) | ((g >> 3) << 5) |
				     ((b >> 3) << 10));
		if (a < 128)
			v |= 0x8000;    /* PSX STP: per-texel semi-transparency */
		hw[i] = v;
	}
	glTexImage2DPSX((GLuint)(bound_tex - texs), width, height, hw);
}

void glTexParameteri(GLenum target, GLenum pname, GLint param)
{
	Tex *t = bound_tex;

	(void)target;
	if (!t)
		return;
	if (pname == GL_TEXTURE_WRAP_S)
		t->wrap_s = param;
	else if (pname == GL_TEXTURE_WRAP_T)
		t->wrap_t = param;
	/* MIN/MAG filters: the PSX is nearest-only. */
}

void glTexParameterf(GLenum target, GLenum pname, GLfloat param)
{
	glTexParameteri(target, pname, (GLint)param);
}

void glTexEnvi(GLenum target, GLenum pname, GLint param)
{
	(void)target;
	if (pname == GL_TEXTURE_ENV_MODE)
		tex_env_modulate = (param == GL_MODULATE);
}

void glPixelStorei(GLenum pname, GLint param)
{
	(void)pname; (void)param;
}

/* --------------------------------------------------------- state setters */

void glEnable(GLenum cap)
{
	switch (cap) {
	case GL_BLEND:      st_blend = 1; break;
	case GL_LIGHTING:   st_lighting = 1; break;
	case GL_LIGHT0:     light0_on = 1; break;
	case GL_FOG:        st_fog = 1; break;
	case GL_TEXTURE_2D: st_texture_2d = 1; break;
	case GL_DEPTH_TEST: st_depth_test = 1; break;
	default: break;          /* GL_CULL_FACE: always off */
	}
}

void glDisable(GLenum cap)
{
	switch (cap) {
	case GL_BLEND:      st_blend = 0; break;
	case GL_LIGHTING:   st_lighting = 0; break;
	case GL_LIGHT0:     light0_on = 0; break;
	case GL_FOG:        st_fog = 0; break;
	case GL_TEXTURE_2D: st_texture_2d = 0; break;
	case GL_DEPTH_TEST: st_depth_test = 0; break;
	default: break;
	}
}

void glBlendFunc(GLenum sfactor, GLenum dfactor)
{
	/* The PSX cannot weight by alpha, but it does have two useful
	 * fixed rates: B/2+F/2 (ABR=0) and B+F additive (ABR=1).
	 * GLTron's glow pass asks for (GL_ONE, GL_ONE) - additive -
	 * which makes the radial fans add light instead of averaging
	 * into solid slabs.  Everything else averages. */
	st_abr = (sfactor == GL_ONE && dfactor == GL_ONE) ? 1 : 0;
}

void glShadeModel(GLenum mode)
{
	st_shade_smooth = (mode == GL_SMOOTH);
}

void glLineWidth(GLfloat width)
{
	(void)width;                            /* 1-pixel lines only */
}

void glDepthMask(GLboolean flag)
{
	st_depth_mask = flag;
	/* The ordering table always sorts; the mask is advisory. */
	(void)st_depth_mask;
}

void glClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
	clear_color[0] = r; clear_color[1] = g;
	clear_color[2] = b; clear_color[3] = a;
}

void glClear(GLbitfield mask)
{
	int x = (active_buff ^ 1) ? FB1_X : FB0_X;
	int y = (active_buff ^ 1) ? FB1_Y : FB0_Y;

	if (mask & GL_COLOR_BUFFER_BIT) {
		unsigned r = (unsigned)(clear_color[0] * 31.0f) & 31;
		unsigned g = (unsigned)(clear_color[1] * 31.0f) & 31;
		unsigned b = (unsigned)(clear_color[2] * 31.0f) & 31;

		/* 0x02: raw-VRAM fill rectangle — the 15-bit color
		 * goes IN the command word; the packet is 3 words */
		gp0_send(0x02000000u | (u32)(r | (g << 5) | (b << 10)));
		gp0_send(((u32)y << 16) | (u32)x);
		gp0_send(((u32)SCREEN_H << 16) | (u32)SCREEN_W);
	}
	/* GL_DEPTH_BUFFER_BIT: the PSX has no depth buffer. */
}

void glViewport(GLint x, GLint y, GLsizei w, GLsizei h)
{
	vp_x = x; vp_y = y; vp_w = w; vp_h = h;
}

static void copy4(GLfloat *dst, const GLfloat *src)
{
	dst[0] = src[0]; dst[1] = src[1];
	dst[2] = src[2]; dst[3] = src[3];
}

static GLfloat light_pos[4];
static GLfloat light_dir[3] = { 0.0f, 0.0f, -1.0f };  /* normalized, for w==0 */

static void light_update_dir(void)
{
	float len;

	if (light_pos[3] != 0.0f)
		return;                     /* positional: per-vertex path */
	len = (float)sqrt((double)(light_pos[0] * light_pos[0] +
				   light_pos[1] * light_pos[1] +
				   light_pos[2] * light_pos[2]));
	if (len > 0.0f) {
		light_dir[0] = -light_pos[0] / len;
		light_dir[1] = -light_pos[1] / len;
		light_dir[2] = -light_pos[2] / len;
	} else {
		light_dir[0] = 0.0f;
		light_dir[1] = 0.0f;
		light_dir[2] = -1.0f;
	}
}

void glLightfv(GLenum light, GLenum pname, const GLfloat *params)
{
	(void)light;                             /* only GL_LIGHT0 */
	switch (pname) {
	case GL_AMBIENT:  copy4(light_ambient, params); fx_dirty = 1; break;
	case GL_DIFFUSE:  copy4(light_diffuse, params); fx_dirty = 1; break;
	case GL_SPECULAR: /* folded into ambient (cheap but stable) */
		light_ambient[0] += params[0] * 0.5f;
		light_ambient[1] += params[1] * 0.5f;
		light_ambient[2] += params[2] * 0.5f;
		break;
	case GL_POSITION:
		copy4(light_pos, params);
		light_update_dir();
		fx_dirty = 1;
		break;
	default: break;
	}
}

void glMaterialfv(GLenum face, GLenum pname, const GLfloat *params)
{
	(void)face;                              /* FRONT_AND_BACK only */
	switch (pname) {
	case GL_AMBIENT:  copy4(mat_ambient, params); fx_dirty = 1; break;
	case GL_DIFFUSE:  copy4(mat_diffuse, params); fx_dirty = 1; break;
	case GL_SPECULAR: copy4(mat_specular, params); break;
	default: break;
	}
}

void glFogf(GLenum pname, GLfloat param)
{
	switch (pname) {
	case GL_FOG_START:  fog_start = param; fx_dirty = 1; break;
	case GL_FOG_END:    fog_end = param; fx_dirty = 1; break;
	default: break;        /* MODE/DENSITY: linear only */
	}
}

void glFogfv(GLenum pname, const GLfloat *params)
{
	if (pname == GL_AMBIENT) {              /* abused as FOG_COLOR */
		copy4(fog_color, params);
		fx_dirty = 1;
	}
}

/* ---------------------------------------------------------------- queries */

GLenum glGetError(void)
{
	return GL_NO_ERROR;
}

const GLubyte *glGetString(GLenum name)
{
	switch (name) {
	case GL_VENDOR:   return (const GLubyte *)"PSXLinux";
	case GL_RENDERER: return (const GLubyte *)"minigl/psx-gpu";
	case GL_VERSION:  return (const GLubyte *)"1.0 minigl";
	}
	return (const GLubyte *)"";
}

void glGetFloatv(GLenum pname, GLfloat *params)
{
	int i;

	if (pname == GL_MODELVIEW_MATRIX)
		for (i = 0; i < 16; i++)
			params[i] = (float)mv.m[i] * (1.0f / 65536.0f);
}

void glGetIntegerv(GLenum pname, GLint *params)
{
	if (pname == GL_SHADE_MODEL)
		*params = st_shade_smooth ? GL_SMOOTH : GL_FLAT;
}

/* -------------------------------------------------------------- raster */

void glBitmap(GLsizei w, GLsizei h, GLfloat x0, GLfloat y0,
	      GLfloat xi, GLfloat yi, const GLubyte *bitmap)
{
	/* GLTron renders text as textured quads; bitmap fonts unused. */
	(void)w; (void)h; (void)x0; (void)y0; (void)xi; (void)yi;
	(void)bitmap;
}

void glRasterPos2f(GLfloat x, GLfloat y) { (void)x; (void)y; }
void glRasterPos2i(GLint x, GLint y)     { (void)x; (void)y; }

/* ------------------------------------------------------- immediate mode */

#define MAX_VERTS 96

/*
 * All per-vertex data is kept in fixed point.  The R3000A has no FPU
 * and every soft-float library call (fpmul/fpadd/...) costs hundreds
 * of cycles, which made the float pipeline spend ~33k cycles per
 * vertex.  Formats:
 *   positions / clip coords / screen coords : Q16.16
 *   normals                                  : Q2.14 (1.0 == 16384)
 *   colours                                  : 0..255 bytes
 * Texture coords stay float: they are only touched at packet-emit
 * time, once per primitive, not per transformed vertex.
 */
typedef struct {
	/* captured at glVertex/glColor/glNormal/glTexCoord time */
	int   x, y, z;          /* Q16.16 */
	int   r8, g8, b8, a8;   /* 0..255 */
	int   u, v;              /* texcoords, Q8.8 */
	int   nx, ny, nz;       /* Q2.14 */
	/* computed at glEnd: */
	int   c[4];             /* clip coordinates (x, y, z, w), Q16.16 */
	int   sx, sy;          /* screen coords, integer pixels */
	int   dist;            /* clip w == eye distance, Q16.16 */
} Vtx;

static GLenum prim_mode;
static int    prim_n;
static Vtx    verts[MAX_VERTS];
static int    in_begin;

static int c2i(float f);

void glBegin(GLenum mode)
{
	prim_mode = mode;
	prim_n = 0;
	in_begin = 1;
}

void glVertex2f(GLfloat x, GLfloat y)
{
	glVertex3f(x, y, 0.0f);
}

void glVertex3f(GLfloat x, GLfloat y, GLfloat z)
{
	Vtx *v;

	if (!in_begin || prim_n >= MAX_VERTS)
		return;
	v = &verts[prim_n++];
	v->x = f2q(x, 16); v->y = f2q(y, 16); v->z = f2q(z, 16);
	v->r8 = cur_color[0]; v->g8 = cur_color[1];
	v->b8 = cur_color[2]; v->a8 = cur_color[3];
	v->u = cur_uv[0];    v->v = cur_uv[1];
	v->nx = cur_normal[0]; v->ny = cur_normal[1]; v->nz = cur_normal[2];
}

/* Integer vertex paths: the arena grid, floor tiles, walls and text
 * quads are laid out in whole units, so the coordinates convert to
 * Q16.16 with a plain shift — no float conversion needed. */
void glVertex2i(GLint x, GLint y)
{
	Vtx *v;

	if (!in_begin || prim_n >= MAX_VERTS)
		return;
	v = &verts[prim_n++];
	v->x = x << 16; v->y = y << 16; v->z = 0;
	v->r8 = cur_color[0]; v->g8 = cur_color[1];
	v->b8 = cur_color[2]; v->a8 = cur_color[3];
	v->u = cur_uv[0];    v->v = cur_uv[1];
	v->nx = cur_normal[0]; v->ny = cur_normal[1]; v->nz = cur_normal[2];
}

void glVertex3i(GLint x, GLint y, GLint z)
{
	Vtx *v;

	if (!in_begin || prim_n >= MAX_VERTS)
		return;
	v = &verts[prim_n++];
	v->x = x << 16; v->y = y << 16; v->z = z << 16;
	v->r8 = cur_color[0]; v->g8 = cur_color[1];
	v->b8 = cur_color[2]; v->a8 = cur_color[3];
	v->u = cur_uv[0];    v->v = cur_uv[1];
	v->nx = cur_normal[0]; v->ny = cur_normal[1]; v->nz = cur_normal[2];
}

void glVertex3fv(const GLfloat *p)
{
	glVertex3f(p[0], p[1], p[2]);
}

void glVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
	/* w is accepted but ignored: the transform assumes w == 1
	 * (nothing in the port uses glVertex4f) */
	(void)w;
	glVertex3f(x, y, z);
}

void glColor3f(GLfloat r, GLfloat g, GLfloat b)
{
	glColor4f(r, g, b, 1.0f);
}

void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
	cur_color[0] = c2i(r); cur_color[1] = c2i(g);
	cur_color[2] = c2i(b); cur_color[3] = c2i(a);
}

void glColor3fv(const GLfloat *p)  { glColor3f(p[0], p[1], p[2]); }
void glColor4fv(const GLfloat *p) { glColor4f(p[0], p[1], p[2], p[3]); }

void glColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a)
{
	cur_color[0] = r; cur_color[1] = g;
	cur_color[2] = b; cur_color[3] = a;
}

void glTexCoord2f(GLfloat u, GLfloat v)
{
	cur_uv[0] = f2q(u, 8); cur_uv[1] = f2q(v, 8);
}

void glTexCoord2i(GLint u, GLint v)
{
	cur_uv[0] = u << 8; cur_uv[1] = v << 8;
}

void glNormal3f(GLfloat x, GLfloat y, GLfloat z)
{
	int t;

	t = f2q(x, 14); cur_normal[0] = (t > 16384) ? 16384 : (t < -16384 ? -16384 : t);
	t = f2q(y, 14); cur_normal[1] = (t > 16384) ? 16384 : (t < -16384 ? -16384 : t);
	t = f2q(z, 14); cur_normal[2] = (t > 16384) ? 16384 : (t < -16384 ? -16384 : t);
}

void glNormal3fv(const GLfloat *p)
{
	glNormal3f(p[0], p[1], p[2]);
}

/* ------------------------------------------- per-vertex pipeline stages */

static int c2i(float f)          /* float colour -> byte */
{
	int v = (int)(f * 255.0f);

	if (v < 0) return 0;
	if (v > 255) return 255;
	return v;
}

/*
 * float -> Q<bits> fixed point straight from the IEEE bits — no
 * soft-float library calls.  Exact for the whole useful range of
 * vertex coordinates; magnitudes above ~2^14 (positions) or 1
 * (normals) saturate, and the emit stage clamps to the screen
 * anyway.
 */
static int f2q(float f, int bits)
{
	union { float f; unsigned int i; } u;
	int e, m, r;

	u.f = f;
	if ((u.i & 0x7fffffffu) == 0)
		return 0;
	e = (int)((u.i >> 23) & 0xff) - 127;         /* unbiased exponent */
	m = (int)((u.i & 0x7fffffu) | 0x800000u);   /* 1.23 mantissa */
	/* value = m * 2^(e-23); want m * 2^(e + bits - 23) */
	e += bits - 23;
	if (e >= 0) {
		if (e > 7)
			e = 7;             /* saturate: m<<7 is the last
					     shift that stays in int32 */
		r = m << e;
	} else {
		if (e < -31)
			e = -31;
		r = m >> -e;               /* tiny |f|: rounds toward 0 */
	}
	return (u.i & 0x80000000u) ? -r : r;
}

/* --------------------------------------------------- fixed point state */
/*
 * Light/material/fog coefficients are converted to fixed point
 * lazily (at glEnd, when the dirty flag is set by the setters) so
 * the per-vertex inner loops never touch the soft-float library.
 * The matrices themselves are fixed point natively — see Mat4.
 */
static int  fx_light_dir[3];    /* Q2.14, directional */
static int  fx_lp[3];           /* Q16.16, positional light pos */
static int  fx_amb8[3];         /* ambient * light, 0..255 */
static int  fx_diff8[3];        /* diffuse * light, 0..255 */
static int  fx_light_positional;
static int  fx_fog_start, fx_fog_end, fx_fog_range;  /* Q16.16 */
static long long fx_fog_inv;    /* 2^32 / range, for Q16 fog factor */
static int  fx_fog_c8[3];       /* fog colour, 0..255 */

static void fx_rebuild(void)
{
	int i;

	for (i = 0; i < 3; i++) {
		fx_light_dir[i] = f2q(light_dir[i], 14);
		fx_lp[i]        = f2q(light_pos[i], 16);
		fx_amb8[i]      = c2i(mat_ambient[i] * light_ambient[i]);
		fx_diff8[i]     = c2i(mat_diffuse[i] * light_diffuse[i]);
		fx_fog_c8[i]    = c2i(fog_color[i]);
	}
	fx_fog_start = f2q(fog_start, 16);
	fx_fog_end   = f2q(fog_end, 16);
	fx_fog_range = fx_fog_end - fx_fog_start;
	fx_fog_inv   = (fx_fog_range > 0)
			? (((long long)1 << 32) / fx_fog_range) : 0;
	fx_light_positional = (light_pos[3] != 0.0f);
	fx_dirty = 0;
}

/*
 * Integer reciprocal: returns floor(2^30 / d) for d > 0.
 *
 * d is normalized into (2^30, 2^31], a linear seed is refined with
 * three Newton steps for  1/x  (x in (0.5, 1], fixed point 2^30),
 * then the normalization shift is folded back in.  All products fit
 * in 64 bits; the intermediate y stays below 2^31.
 */
static unsigned fx_inv30(unsigned d)
{
	unsigned dn, y, t;
	int k = 0;

	dn = d;
	while (dn <= (1u << 30)) {
		dn <<= 1;
		k++;
	}
	/* x = dn/2^31 in (0.5, 1];  y ~ (1/x) * 2^30 */
	y = 3u * (1u << 30) - dn;                      /* (3 - 2x)*2^30 */
	t = (unsigned)(((unsigned long long)dn * y) >> 31);
	y = (unsigned)(((unsigned long long)y * ((2u << 30) - t)) >> 30);
	t = (unsigned)(((unsigned long long)dn * y) >> 31);
	y = (unsigned)(((unsigned long long)y * ((2u << 30) - t)) >> 30);
	t = (unsigned)(((unsigned long long)dn * y) >> 31);
	y = (unsigned)(((unsigned long long)y * ((2u << 30) - t)) >> 30);
	/* y = 2^61/dn  =>  2^30/d = y * 2^k / 2^31 */
	return (unsigned)((((unsigned long long)y << k) >> 31));
}

/* integer sqrt (positional-light path only; rare and not hot) */
static int fx_isqrt(long long v)
{
	long long x;
	int i;

	if (v <= 0)
		return 0;
	x = 1;
	for (i = 0; i < 32; i++) {
		if ((x << (2 * i + 2)) > v)
			break;
		x <<= 1;
	}
	for (i = 0; i < 6; i++)
		x = (x + v / x) / 2;          /* Newton, converges from above */
	return (int)x;
}

/* Map clip coordinates to screen; called once the clip w is known
 * positive.  Fills sx, sy, dist. */
static void project_vtx(Vtx *v)
{
	unsigned inv = fx_inv30((unsigned)v->c[3]);
	int ndc_x = (int)(((long long)v->c[0] * (int)inv) >> 14);
	int ndc_y = (int)(((long long)v->c[1] * (int)inv) >> 14);
	int half_w = vp_w >> 1;
	int half_h = vp_h >> 1;

	/* clamp degenerate ratios before the screen map (the emit
	 * stage saturates anyway; this keeps the int32 math sane) */
	if (ndc_x > 4 * 65536) ndc_x = 4 * 65536;
	if (ndc_x < -4 * 65536) ndc_x = -4 * 65536;
	if (ndc_y > 4 * 65536) ndc_y = 4 * 65536;
	if (ndc_y < -4 * 65536) ndc_y = -4 * 65536;

	v->sx = vp_x + (int)(((long long)(ndc_x + 65536) * half_w
			     + 32768) >> 16);
	v->sy = vp_y + vp_h - (int)(((long long)(ndc_y + 65536) * half_h
				     + 32768) >> 16);
	v->dist = v->c[3];
}

/* Transform one accumulated vertex: modelview, lighting, fog,
 * projection — all in Q16.16 fixed point. */
static void transform_vertex(Vtx *v)
{
	const int *m = mv.m;            /* Q16.16, no conversion needed */
	const int *pm = proj.m;
	long long t;
	int ex, ey, ez;

	t  = (long long)m[0] * v->x + (long long)m[4] * v->y +
	     (long long)m[8] * v->z;
	ex = (int)(t >> 16) + m[12];
	t  = (long long)m[1] * v->x + (long long)m[5] * v->y +
	     (long long)m[9] * v->z;
	ey = (int)(t >> 16) + m[13];
	t  = (long long)m[2] * v->x + (long long)m[6] * v->y +
	     (long long)m[10] * v->z;
	ez = (int)(t >> 16) + m[14];

	/* lighting: one light, ambient + diffuse, in eye space.
	 * Normals must be unit vectors (as OBJ exports them); the
	 * Q2.14 x Q16.16 products then cannot overflow int32. */
	if (st_lighting && light0_on) {
		int nx = (m[0] * v->nx + m[4] * v->ny + m[8] * v->nz) >> 16;
		int ny = (m[1] * v->nx + m[5] * v->ny + m[9] * v->nz) >> 16;
		int nz = (m[2] * v->nx + m[6] * v->ny + m[10] * v->nz) >> 16;
		int lx, ly, lz, nd;

		if (!fx_light_positional) {
			lx = fx_light_dir[0];
			ly = fx_light_dir[1];
			lz = fx_light_dir[2];
		} else {
			long long dx = fx_lp[0] - ex;   /* Q16.16 */
			long long dy = fx_lp[1] - ey;
			long long dz = fx_lp[2] - ez;
			long long len = fx_isqrt(dx * dx + dy * dy + dz * dz);
			unsigned inv;

			if (len < 65536)
				len = 65536;
			inv = fx_inv30((unsigned)(len >> 16));
			lx = (int)((dx * inv) >> 16);
			ly = (int)((dy * inv) >> 16);
			lz = (int)((dz * inv) >> 16);
		}

		nd = (nx * lx + ny * ly + nz * lz) >> 14;
		if (nd < 0)
			nd = 0;
		v->r8 = fx_amb8[0] + ((fx_diff8[0] * nd) >> 14);
		if (v->r8 > 255) v->r8 = 255;
		v->g8 = fx_amb8[1] + ((fx_diff8[1] * nd) >> 14);
		if (v->g8 > 255) v->g8 = 255;
		v->b8 = fx_amb8[2] + ((fx_diff8[2] * nd) >> 14);
		if (v->b8 > 255) v->b8 = 255;
	}

	/* linear fog toward fog_color (eye distance = -ez) */
	if (st_fog && fx_fog_range > 0) {
		int d = -ez;
		int f = (int)(((long long)(fx_fog_end - d) * fx_fog_inv)
				>> 16);

		if (f < 0) f = 0;
		if (f > 65536) f = 65536;
		v->r8 = v->r8 + (((fx_fog_c8[0] - v->r8) * f) >> 16);
		v->g8 = v->g8 + (((fx_fog_c8[1] - v->g8) * f) >> 16);
		v->b8 = v->b8 + (((fx_fog_c8[2] - v->b8) * f) >> 16);
	}

	/* projection -> clip coords (Q16.16) */
	t  = (long long)pm[0] * ex + (long long)pm[4] * ey +
	     (long long)pm[8] * ez + pm[12];
	v->c[0] = (int)(t >> 16);
	t  = (long long)pm[1] * ex + (long long)pm[5] * ey +
	     (long long)pm[9] * ez + pm[13];
	v->c[1] = (int)(t >> 16);
	t  = (long long)pm[2] * ex + (long long)pm[6] * ey +
	     (long long)pm[10] * ez + pm[14];
	v->c[2] = (int)(t >> 16);
	t  = (long long)pm[3] * ex + (long long)pm[7] * ey +
	     (long long)pm[11] * ez + pm[15];
	v->c[3] = (int)(t >> 16);

	if (v->c[3] > W_EPS_Q)
		project_vtx(v);
	else
		v->dist = 0;             /* behind the eye; clipper decides */
}

/* ------------------------------------------- primitive packet emission */

static u32 xy_word(const Vtx *v)
{
	int x = v->sx, y = v->sy;

	if (x < 0) x = 0;
	if (x > 1023) x = 1023;
	if (y < 0) y = 0;
	if (y > 1023) y = 1023;
	return ((u32)y << 16) | (u32)x;
}

static u32 col_word(const Vtx *v)
{
	return (u32)(v->r8 | (v->g8 << 8) | (v->b8 << 16));
}

/* Per-primitive blend class, folding GL's blend/alpha state onto
 * the PSX's one-bit ABE plus the 2-bit ABR rate:
 *   - blending disabled, or every vertex alpha >= 128: opaque.
 *     (The GPU's B/2+F/2 average would halve the bikes and trails
 *     into dim "gray blobs", so mostly-opaque geometry must not
 *     carry the blend bit at all.)
 *   - additive intent (glBlendFunc GL_ONE,GL_ONE): ABE+ABR=1.
 *     Black adds nothing, so the radial glow fans and engine
 *     halos render as real light instead of averaged slabs.
 *   - a vertex with alpha < 16 marks faded-out geometry (trail-end
 *     quads, the crash flash tail): skipped entirely.
 *   - anything else: ABE+ABR=0 average. */
#define BL_OPAQUE 0
#define BL_SEMI   1     /* ABE on, ABR=0 (B/2+F/2) */
#define BL_ADD    2     /* ABE on, ABR=1 (B+F)     */
#define BL_SKIP   3     /* fully faded out: drop   */

static int blend_class(const Vtx *const *vp, int nv)
{
	int i, amin = 255;

	if (!st_blend)
		return BL_OPAQUE;
	if (st_abr)
		return BL_ADD;
	for (i = 0; i < nv; i++) {
		if (vp[i]->a8 < 16)
			return BL_SKIP;
		if (vp[i]->a8 < amin)
			amin = vp[i]->a8;
	}
	return (amin >= 128) ? BL_OPAQUE : BL_SEMI;
}

/* Phase-correct UV bytes for one textured primitive.
 *
 * The PSX UV field is 8 bits, but the texture window (GP0(E2h))
 * wraps sampling at the texture size, so only u mod w survives
 * rasterization.  A primitive whose coordinates cross the 256-byte
 * wrap would fold over: vertices at 192 and 256(=0) interpolate
 * backwards through the texture.  Instead remap each primitive's
 * bytes to start at the phase of its minimum vertex and grow by
 * the true span:  byte_i = (umin mod w) + (u_i - umin).  With
 * w <= 64 and spans of up to ~2.9 repeats every byte stays in
 * 0..255 and the per-pixel window wrap reproduces the exact
 * texels - which frees the game to subdivide tiling geometry at
 * any boundary it likes.  GL_CLAMP textures (the font sheet) keep
 * the raw bytes: their window is off, so the byte IS the texel
 * offset within the sheet. */
static void prim_uv_bytes(const Vtx *const *vp, int nv, u32 *out)
{
	int uf[4], vf[4];
	int um, vm, bu, bv, i;

	for (i = 0; i < nv; i++) {
		uf[i] = (vp[i]->u * bound_tex->w) >> 8;
		vf[i] = (vp[i]->v * bound_tex->h) >> 8;
	}
	um = uf[0];
	vm = vf[0];
	for (i = 1; i < nv; i++) {
		if (uf[i] < um) um = uf[i];
		if (vf[i] < vm) vm = vf[i];
	}
	bu = (bound_tex->wrap_s == GL_REPEAT) ? um % bound_tex->w : 0;
	bv = (bound_tex->wrap_t == GL_REPEAT) ? vm % bound_tex->h : 0;
	for (i = 0; i < nv; i++) {
		int b_u, b_v;

		if (bound_tex->wrap_s == GL_REPEAT)
			b_u = bu + uf[i] - um;
		else
			b_u = uf[i] & 0xFF;
		if (bound_tex->wrap_t == GL_REPEAT)
			b_v = bv + vf[i] - vm;
		else
			b_v = vf[i] & 0xFF;
		out[i] = (u32)b_u | ((u32)b_v << 8);
	}
}

static int depth_bucket(const Vtx *a, const Vtx *b,
			const Vtx *c, const Vtx *d)
{
	int dist, bucket;

	if (d)
		dist = (a->dist + b->dist + c->dist + d->dist) >> 2;
	else
		dist = (a->dist + b->dist + c->dist) / 3;
	bucket = (dist >> 16) << 2;      /* eye-units * OT_SIZE/256 */
	if (bucket < 0) bucket = 0;
	if (bucket >= OT_SIZE) bucket = OT_SIZE - 1;
	return bucket;
}

static void emit_packet(u_long *pk, u32 twin, u32 e1,
			int n, const Vtx *const *vp, int nv)
{
	int bucket = depth_bucket(vp[0], vp[1],
				  nv > 2 ? vp[2] : vp[1], vp[3]);

	pk[1] = n;
	pk[2] = twin;
	pk[3] = e1;
	pkts_now++;
	pkt_link(active_buff ^ 1, bucket, pk);
}

/* Emit one clipped screen-space triangle. */
static void emit_tri(const Vtx *a, const Vtx *b, const Vtx *c)
{
	u32 cmd;
	int bcls;
	int gouraud = st_shade_smooth;
	u_long *pk;
	int n = 0;
	{
		const Vtx *sv[3] = { a, b, c };

		bcls = blend_class(sv, 3);
	}
	if (bcls == BL_SKIP)
		return;

	if (st_texture_2d && bound_tex) {
		u32 tsb = (u32)bound_tex->tpage_bits;
		u32 uvb[3];
		const Vtx *vp[3];

		/* semi-transparency = ABE = command bit 25, i.e. +2 in
		 * the low byte (bit 24 is TGE: raw-texture mode) */
		cmd = ((gouraud ? 0x34u : 0x24u) |
		       ((u32)(bcls != BL_OPAQUE) << 1)) << 24;
		pk = pkt_new(gouraud ? 9 : 7);
		if (!pk)
			return;
		vp[0] = a; vp[1] = b; vp[2] = c;
		prim_uv_bytes(vp, 3, uvb);
		tsb = (tsb & ~0x60u) |
		      ((bcls == BL_ADD) ? 0x20u : 0u);   /* ABR rate */
		pk[4 + n++] = cmd | col_word(a);
		pk[4 + n++] = xy_word(a);
		pk[4 + n++] = uvb[0];                       /* cba = 0 */
		if (gouraud) pk[4 + n++] = col_word(b);
		pk[4 + n++] = xy_word(b);
		pk[4 + n++] = uvb[1] | (tsb << 16);
		if (gouraud) pk[4 + n++] = col_word(c);
		pk[4 + n++] = xy_word(c);
		pk[4 + n++] = uvb[2];
		emit_packet(pk, bound_tex->twin_word, 0, n, vp, 3);
	} else {
		const Vtx *vp[4];

		cmd = ((gouraud ? 0x30u : 0x20u) |
		       ((u32)(bcls != BL_OPAQUE) << 1)) << 24;
		pk = pkt_new(gouraud ? 6 : 4);
		if (!pk)
			return;
		pk[4 + n++] = cmd | col_word(a);
		pk[4 + n++] = xy_word(a);
		if (gouraud) pk[4 + n++] = col_word(b);
		pk[4 + n++] = xy_word(b);
		if (gouraud) pk[4 + n++] = col_word(c);
		pk[4 + n++] = xy_word(c);
		vp[0] = a; vp[1] = b; vp[2] = c; vp[3] = 0;
		emit_packet(pk, 0,
			    (bcls == BL_ADD) ? 0xE1000020u : 0, n, vp, 3);
	}
}

/* Emit one clipped screen-space quad.
 *
 * The GPU rasterizes the four vertices as a triangle STRIP:
 * (v1,v2,v3) + (v2,v3,v4), sharing the v2-v3 edge.  For that edge
 * to be the quad's diagonal the vertices must arrive in GTE
 * "zigzag" order (TL,TR,BL,BR), not cyclic order — cyclic input
 * makes v2-v3 a boundary edge and the quad renders as a bowtie
 * with a wedge missing.  minigl collects cyclic order, so swap
 * the last two vertices here. */
static void emit_quad(const Vtx *a, const Vtx *b,
		      const Vtx *c_in, const Vtx *d_in)
{
	/* cyclic (a,b,c,d) -> zigzag (a,b,d,c) */
	const Vtx *t = c_in, *c = d_in, *d = t;
	u32 cmd;
	int bcls;
	int gouraud = st_shade_smooth;
	u_long *pk;
	int n = 0;
	const Vtx *vp[4];
	u32 twin, e1;
	{
		const Vtx *sv[4] = { a, b, c, d };

		bcls = blend_class(sv, 4);
	}
	if (bcls == BL_SKIP)
		return;

	if (st_texture_2d && bound_tex) {
		u32 tsb = (u32)bound_tex->tpage_bits;
		u32 uvb[4];
		const Vtx *qvp[4];

		cmd = ((gouraud ? 0x3Cu : 0x2Cu) |
		       ((u32)(bcls != BL_OPAQUE) << 1)) << 24;
		pk = pkt_new(gouraud ? 12 : 9);
		if (!pk)
			return;
		qvp[0] = a; qvp[1] = b; qvp[2] = c; qvp[3] = d;
		prim_uv_bytes(qvp, 4, uvb);
		tsb = (tsb & ~0x60u) |
		      ((bcls == BL_ADD) ? 0x20u : 0u);   /* ABR rate */
		pk[4 + n++] = cmd | col_word(a);
		pk[4 + n++] = xy_word(a);
		pk[4 + n++] = uvb[0];
		if (gouraud) pk[4 + n++] = col_word(b);
		pk[4 + n++] = xy_word(b);
		pk[4 + n++] = uvb[1] | (tsb << 16);
		if (gouraud) pk[4 + n++] = col_word(c);
		pk[4 + n++] = xy_word(c);
		pk[4 + n++] = uvb[2];
		if (gouraud) pk[4 + n++] = col_word(d);
		pk[4 + n++] = xy_word(d);
		pk[4 + n++] = uvb[3];
		twin = bound_tex->twin_word;
		e1 = 0;
	} else {
		cmd = ((gouraud ? 0x38u : 0x28u) |
		       ((u32)(bcls != BL_OPAQUE) << 1)) << 24;
		pk = pkt_new(gouraud ? 8 : 5);
		if (!pk)
			return;
		pk[4 + n++] = cmd | col_word(a);
		pk[4 + n++] = xy_word(a);
		if (gouraud) pk[4 + n++] = col_word(b);
		pk[4 + n++] = xy_word(b);
		if (gouraud) pk[4 + n++] = col_word(c);
		pk[4 + n++] = xy_word(c);
		if (gouraud) pk[4 + n++] = col_word(d);
		pk[4 + n++] = xy_word(d);
		twin = 0;
		e1 = (bcls == BL_ADD) ? 0xE1000020u : 0;
	}
	vp[0] = a; vp[1] = b; vp[2] = c; vp[3] = d;
	emit_packet(pk, twin, e1, n, vp, 4);
}

/* Emit one clipped screen-space line. */
static void emit_line(const Vtx *a, const Vtx *b)
{
	const Vtx *sv[2] = { a, b };
	int bcls = blend_class(sv, 2);
	u32 cmd = (0x40u | ((u32)(bcls != BL_OPAQUE ? 1 : 0) << 1)) << 24;
	u_long *pk;
	const Vtx *vp[4];

	if (bcls == BL_SKIP)
		return;
	pk = pkt_new(3);
	if (!pk)
		return;
	pk[4] = cmd | col_word(a);
	pk[5] = xy_word(a);
	pk[6] = xy_word(b);
	vp[0] = a; vp[1] = b; vp[2] = a; vp[3] = 0;
	emit_packet(pk, 0, (bcls == BL_ADD) ? 0xE1000020u : 0, 3, vp, 2);
}

/* ------------------------------------------------ near-plane clipping */

static void clip_lerp(Vtx *out, const Vtx *a, const Vtx *b, int t)
{
	int i;

	for (i = 0; i < 4; i++)
		out->c[i] = a->c[i] +
			(int)(((long long)(b->c[i] - a->c[i]) * t) >> 16);
	out->r8 = a->r8 + (((b->r8 - a->r8) * t) >> 16);
	out->g8 = a->g8 + (((b->g8 - a->g8) * t) >> 16);
	out->b8 = a->b8 + (((b->b8 - a->b8) * t) >> 16);
	out->a8 = a->a8 + (((b->a8 - a->a8) * t) >> 16);
	out->u = a->u + (((b->u - a->u) * t) >> 16);
	out->v = a->v + (((b->v - a->v) * t) >> 16);

	/* screen position from the clipped clip-coords */
	if (out->c[3] > W_EPS_Q)
		project_vtx(out);
	else
		out->dist = 0;
}

/*
 * Clip a vertex fan against the near plane (clip w > W_EPS).
 * Input: v[0..n-1] with clip coords in c[0..3]; output: the clipped
 * polygon replaces v, new count returned (0 if fully culled).
 *
 * The scratch buffer is static, not on the stack: uClinux gives
 * user programs only a small stack, and a 4 KB local here used
 * to overflow it and corrupt the packet arena.
 */
static Vtx clip_out[MAX_VERTS];

static int clip_near(Vtx *v, int n)
{
	Vtx *out = clip_out;
	int m = 0, i;

	if (n < 2)
		return 0;

	/* fast path: no vertex crosses the near plane — pass the
	 * polygon through untouched.  Without this the 8-slot
	 * output buffer below silently truncated every larger
	 * fan/strip (Tux lost his belly, GLTron its trails). */
	for (i = 0; i < n; i++)
		if (v[i].c[3] <= W_EPS_Q)
			break;
	if (i == n)
		return n;

	for (i = 0; i < n; i++) {
		const Vtx *a = &v[i];
		const Vtx *b = &v[(i + 1) % n];
		int ain = a->c[3] > W_EPS_Q;
		int bin = b->c[3] > W_EPS_Q;

		if (ain) {
			out[m++] = *a;
			if (m >= MAX_VERTS) break;
		}
		if (ain != bin) {
			/* t = (a.w - eps) / (a.w - b.w), Q16.16;
			 * a is inside (w > eps) and b outside, so the
			 * denominator is positive */
			int t = (int)(((long long)(a->c[3] - W_EPS_Q) << 16)
				      / (a->c[3] - b->c[3]));

			clip_lerp(&out[m], a, b, t);
			m++;
			if (m >= MAX_VERTS) break;
		}
	}

	if (m < (n == 2 ? 2 : 3))
		return 0;
	for (i = 0; i < m; i++)
		v[i] = out[i];
	return m;
}

/* ---------------------------------------------------------------- glEnd */

void glEnd(void)
{
	int i;

	if (!in_begin)
		return;
	in_begin = 0;
	if (prim_n == 0)
		return;

	/* stage 0: refresh the fixed-point matrices/coefficients if
	 * any matrix, light, material or fog parameter changed */
	if (fx_dirty)
		fx_rebuild();

	/* stage 1: transform every accumulated vertex */
	for (i = 0; i < prim_n; i++)
		transform_vertex(&verts[i]);

	/* stage 2: assemble, clip, emit according to primitive type */
	switch (prim_mode) {
	case GL_TRIANGLES:
		for (i = 0; i + 2 < prim_n; i += 3) {
			Vtx tri[4];       /* a straddling tri clips to 4 */
			int j, m;

			for (j = 0; j < 3; j++)
				tri[j] = verts[i + j];
			m = clip_near(tri, 3);
			if (m == 3)
				emit_tri(&tri[0], &tri[1], &tri[2]);
			else if (m >= 3)
				for (j = 1; j + 1 < m; j++)
					emit_tri(&tri[0], &tri[j],
						 &tri[j + 1]);
		}
		break;

	case GL_QUADS:
		for (i = 0; i + 3 < prim_n; i += 4) {
			Vtx quad[8];
			int j, m;

			for (j = 0; j < 4; j++)
				quad[j] = verts[i + j];
			m = clip_near(quad, 4);
			if (m == 4)
				emit_quad(&quad[0], &quad[1],
					  &quad[2], &quad[3]);
			else if (m >= 3)
				for (j = 1; j + 1 < m; j++)
					emit_tri(&quad[0], &quad[j],
						 &quad[j + 1]);
		}
		break;

	case GL_POLYGON:
	case GL_TRIANGLE_FAN: {
		/* clip in place on the glBegin buffer — a local
		 * MAX_VERTS copy overflowed the 16K user stack */
		int m = clip_near(verts, prim_n);

		if (m >= 3)
			for (i = 1; i + 1 < m; i++)
				emit_tri(&verts[0], &verts[i],
					 &verts[i + 1]);
		break;
	}

	case GL_TRIANGLE_STRIP: {
		int m = clip_near(verts, prim_n);

		if (m >= 3)
			for (i = 0; i + 2 < m; i++)
				emit_tri(&verts[i], &verts[i + 1],
					 &verts[i + 2]);
		break;
	}

	case GL_LINES:
		for (i = 0; i + 1 < prim_n; i += 2) {
			Vtx seg[3];       /* a straddling line clips to 3 */
			int j, m;

			for (j = 0; j < 2; j++)
				seg[j] = verts[i + j];
			m = clip_near(seg, 2);
			if (m == 2)
				emit_line(&seg[0], &seg[1]);
		}
		break;

	case GL_LINE_LOOP: {
		int m = clip_near(verts, prim_n);

		if (m >= 2) {
			for (i = 0; i + 1 < m; i++)
				emit_line(&verts[i], &verts[i + 1]);
			emit_line(&verts[m - 1], &verts[0]);
		}
		break;
	}

	default:
		break;                    /* POINTS etc.: unused by GLTron */
	}

	prim_n = 0;
}

/* ------------------------------------------------------ frame lifecycle */

int glMiniInit(void)
{
	/* open /dev/psxgpu (the kernel GPU device, major 210) */
	if (gpu_open() < 0)
		return -1;

	/* NB: the ECOFF exec path loads .text and zero-fills .bss but
	 * maps .data with size 0 - initialized statics would load as
	 * garbage.  Keep non-zero defaults here (BSS starts zeroed):
	 * W_EPS_Q = 1.0 until the game's gluPerspective sets zNear. */
	W_EPS_Q = 65536;
	last_twin = 0;
	last_e1 = 0;

	/* display: 320x240 NTSC (register values from psx-spx:
	 * GP1(06h) X1=600/X2=3160, GP1(07h) Y1=0x10/Y2=0x100) */
	gp1_send(0x00000000);              /* soft reset */
	gp1_send(0x03000000);              /* display enable */
	gp1_send(0x08000001);              /* 320x240 mode */
	gp1_send(0x060C58258);             /* h range: 600..3160 */
	gp1_send(0x07040010);              /* v range: 16..256 */

	active_buff = 0;
	show_buffer(0);
	set_draw_target(1);
	ot_reset(0);
	ot_reset(1);
	return 0;
}

void glSwapBuffersPSX(void)
{
	int drawn = active_buff ^ 1;

	/* the frame's packets are all built: snapshot the count */
	pkts_last = pkts_now;
	pkts_now = 0;

	/* push all packets for the frame we just built */
	ot_emit(drawn);

	/* wait for the GPU to drain, then show the frame */
	while (!(gpu_stat() & GPUSTAT_FIFO))
		;

	show_buffer(drawn);
	active_buff ^= 1;

	/* aim the next frame at the other buffer and reset the OT */
	set_draw_target(active_buff ^ 1);
	ot_reset(active_buff ^ 1);
}

void glFinish(void)
{
	while (!(gpu_stat() & GPUSTAT_FIFO))
		;
}
