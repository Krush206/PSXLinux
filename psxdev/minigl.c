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

/* soft-float math from minilibc (no FPU on the R3000A) */
extern double sin(double);
extern double cos(double);
extern double tan(double);
extern double sqrt(double);
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
#define GP0_BATCH 256
static u32 gp0_batch[GP0_BATCH];
static int gp0_batch_n;

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
#define ARENA_WORDS 24576

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

static u_long *pkt_new(int words)
{
	u_long *p;

	if (pkt_top + words + 2 >= pkt_arena + ARENA_WORDS)
		return 0;
	p = pkt_top;
	p[1] = words;
	pkt_top += words + 2;
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

static void ot_emit(int buff)
{
	int i;

	for (i = OT_SIZE - 1; i >= 0; i--) {
		u_long *p;
		for (p = (u_long *)ot[buff][i]; p; p = (u_long *)p[0]) {
			int n = (int)p[1], j;
			for (j = 0; j < n; j++)
				gp0_send((u32)p[2 + j]);
		}
	}
}

/* ------------------------------------------------------------- matrices */

typedef struct {
	float m[16];      /* column-major, GL style */
} Mat4;

static const Mat4 ident_mat = {
	{ 1.0f, 0.0f, 0.0f, 0.0f,
	  0.0f, 1.0f, 0.0f, 0.0f,
	  0.0f, 0.0f, 1.0f, 0.0f,
	  0.0f, 0.0f, 0.0f, 1.0f }
};

static Mat4 mv_stack[8];
static int  mv_sp;
static Mat4 mv;
static Mat4 proj;
static GLenum cur_matrix = GL_MODELVIEW;

static void mat_apply(Mat4 *a, const Mat4 *b)
{
	float r[16];
	int c, i, k;

	for (c = 0; c < 4; c++) {
		for (i = 0; i < 4; i++) {
			float s = 0.0f;
			for (k = 0; k < 4; k++)
				s += a->m[k * 4 + i] * b->m[c * 4 + k];
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
		t->m[i] = m[i];
}

void glTranslatef(GLfloat x, GLfloat y, GLfloat z)
{
	Mat4 t = ident_mat;

	t.m[12] = x; t.m[13] = y; t.m[14] = z;
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

	s = (float)sin((double)deg * 3.14159265 / 180.0);
	c = (float)cos((double)deg * 3.14159265 / 180.0);
	t = 1.0f - c;

	r.m[0]  = x * x * t + c;
	r.m[4]  = y * x * t + z * s;
	r.m[8]  = z * x * t - y * s;

	r.m[1]  = x * y * t - z * s;
	r.m[5]  = y * y * t + c;
	r.m[9]  = z * y * t + x * s;

	r.m[2]  = x * z * t + y * s;
	r.m[6]  = y * z * t - x * s;
	r.m[10] = z * z * t + c;

	if (cur_matrix == GL_MODELVIEW)
		mat_apply(&mv, &r);
	else
		mat_apply(&proj, &r);
}

void glScalef(GLfloat x, GLfloat y, GLfloat z)
{
	Mat4 s = ident_mat;

	s.m[0] = x; s.m[5] = y; s.m[10] = z;
	if (cur_matrix == GL_MODELVIEW)
		mat_apply(&mv, &s);
	else
		mat_apply(&proj, &s);
}

void glOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t,
	     GLdouble n, GLdouble f)
{
	Mat4 o = ident_mat;

	o.m[0]  = 2.0f / (float)(r - l);
	o.m[5]  = 2.0f / (float)(t - b);
	o.m[10] = -2.0f / (float)(f - n);
	o.m[12] = -((float)(r + l)) / (float)(r - l);
	o.m[13] = -((float)(t + b)) / (float)(t - b);
	o.m[14] = -((float)(f + n)) / (float)(f - n);
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

	f = (float)(1.0 / tan((double)fovy * 3.14159265 / 360.0));

	for (i = 0; i < 16; i++)
		p.m[i] = 0.0f;
	p.m[0]  = f / (float)aspect;
	p.m[5]  = f;
	p.m[10] = ((float)zFar + (float)zNear) / ((float)zNear - (float)zFar);
	p.m[11] = -1.0f;
	p.m[14] = (2.0f * (float)zFar * (float)zNear) /
		  ((float)zNear - (float)zFar);

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

		n = (float)sqrt((double)(fx * fx + fy * fy + fz * fz));
		if (n > 0.0f) { fx /= n; fy /= n; fz /= n; }
		n = (float)sqrt((double)(sx * sx + sy * sy + sz * sz));
		if (n > 0.0f) { sx /= n; sy /= n; sz /= n; }
		n = (float)sqrt((double)(rx * rx + ry * ry + rz * rz));
		if (n > 0.0f) { rx /= n; ry /= n; rz /= n; }
	}

	m.m[0] = sx;   m.m[4] = sy;   m.m[8]  = sz;
	m.m[1] = rx;   m.m[5] = ry;   m.m[9]  = rz;
	m.m[2] = -fx;  m.m[6] = -fy;  m.m[10] = -fz;

	m.m[12] = -(sx * (float)ex + sy * (float)ey + sz * (float)ez);
	m.m[13] = -(rx * (float)ex + ry * (float)ey + rz * (float)ez);
	/* NB: row 3 is -f, so the translation is +f*eye (not -f*eye):
	 * eye-space z = -f.p + f.eye = -f.(p - eye);  with the wrong
	 * sign every point in front of the camera lands behind the
	 * near plane and clip_near() culls the whole scene. */
	m.m[14] = (fx * (float)ex + fy * (float)ey + fz * (float)ez);

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

static GLfloat cur_color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
static GLfloat cur_uv[2];
static GLfloat cur_normal[3] = { 0.0f, 0.0f, 1.0f };

static GLfloat mat_ambient[4]  = { 0.2f, 0.2f, 0.2f, 1.0f };
static GLfloat mat_diffuse[4]  = { 0.8f, 0.8f, 0.8f, 1.0f };
static GLfloat mat_specular[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

static GLfloat light_ambient[4];
static GLfloat light_diffuse[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
static GLfloat light_pos[4];           /* eye-space, as set */
static int      light0_on;

static GLfloat fog_start = 0.0f, fog_end = 1.0f;
static GLfloat fog_color[4];

static GLint vp_x, vp_y;
static GLint vp_w = SCREEN_W, vp_h = SCREEN_H;

/* --------------------------------------------------------------- textures */

#define MAX_TEX 16

typedef struct {
	int used;
	int w, h;
	int vram_x, vram_y;         /* top-left in VRAM halfwords */
	u16 tpage_bits;              /* tpage field for the TSB word */
	int wrap_s, wrap_t;          /* GL_REPEAT / GL_CLAMP */
} Tex;

static Tex texs[MAX_TEX];
static Tex *bound_tex;
static int  tex_env_modulate = 1;   /* GL_MODULATE default */
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
		/* Texture window (GP0(E2h)): GL_REPEAT wraps UVs within the
		 * texture.  The E2h mask fields are in 8-pixel steps, so the
		 * tiling code maps the power-of-two size; GL_CLAMP passes
		 * UVs through unchanged (mask = all bits).
		 * NB: this is E2h, NOT E6h - E6h is the mask-bit setting
		 * ("reject pixels with bit15 set"), which breaks drawing. */
		if (t->wrap_s == GL_REPEAT)
			mx = (t->w <= 8) ? 31 : (t->w <= 16) ? 30 :
			      (t->w <= 32) ? 28 : (t->w <= 64) ? 24 :
			      (t->w <= 128) ? 16 : 0;
		else
			mx = 31;
		if (t->wrap_t == GL_REPEAT)
			my = (t->h <= 8) ? 31 : (t->h <= 16) ? 30 :
			      (t->h <= 32) ? 28 : (t->h <= 64) ? 24 :
			      (t->h <= 128) ? 16 : 0;
		else
			my = 31;
		gp0_send(0xE2000000u | mx | (my << 5));
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
	/* The PSX has a fixed per-primitive blend rate; both GL modes
	 * GLTron uses map to the hardware average (B/2+F/2). */
	(void)sfactor; (void)dfactor;
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
	case GL_AMBIENT:  copy4(light_ambient, params); break;
	case GL_DIFFUSE:  copy4(light_diffuse, params); break;
	case GL_SPECULAR: /* folded into ambient (cheap but stable) */
		light_ambient[0] += params[0] * 0.5f;
		light_ambient[1] += params[1] * 0.5f;
		light_ambient[2] += params[2] * 0.5f;
		break;
	case GL_POSITION:
		copy4(light_pos, params);
		light_update_dir();
		break;
	default: break;
	}
}

void glMaterialfv(GLenum face, GLenum pname, const GLfloat *params)
{
	(void)face;                              /* FRONT_AND_BACK only */
	switch (pname) {
	case GL_AMBIENT:  copy4(mat_ambient, params); break;
	case GL_DIFFUSE:  copy4(mat_diffuse, params); break;
	case GL_SPECULAR: copy4(mat_specular, params); break;
	default: break;
	}
}

void glFogf(GLenum pname, GLfloat param)
{
	switch (pname) {
	case GL_FOG_START:  fog_start = param; break;
	case GL_FOG_END:    fog_end = param; break;
	default: break;        /* MODE/DENSITY: linear only */
	}
}

void glFogfv(GLenum pname, const GLfloat *params)
{
	if (pname == GL_AMBIENT)                /* abused as FOG_COLOR */
		copy4(fog_color, params);
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
			params[i] = mv.m[i];
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

#define MAX_VERTS 48
#define W_EPS     0.0001f

typedef struct {
	/* captured at glVertex/glColor/glNormal/glTexCoord time */
	float x, y, z, w;
	float r, g, b, a;
	float u, v;
	float nx, ny, nz;
	/* computed at glEnd: */
	float c[4];          /* clip coordinates (x, y, z, w) */
	float sx, sy;        /* screen coords, PSX top-left origin */
	float dist;          /* clip w == eye distance (perspective) */
} Vtx;

static GLenum prim_mode;
static int    prim_n;
static Vtx    verts[MAX_VERTS];
static int    in_begin;

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

void glVertex2i(GLint x, GLint y)
{
	glVertex3f((GLfloat)x, (GLfloat)y, 0.0f);
}

void glVertex3f(GLfloat x, GLfloat y, GLfloat z)
{
	Vtx *v;

	if (!in_begin || prim_n >= MAX_VERTS)
		return;
	v = &verts[prim_n++];
	v->x = x; v->y = y; v->z = z; v->w = 1.0f;
	v->r = cur_color[0]; v->g = cur_color[1];
	v->b = cur_color[2]; v->a = cur_color[3];
	v->u = cur_uv[0];    v->v = cur_uv[1];
	v->nx = cur_normal[0]; v->ny = cur_normal[1]; v->nz = cur_normal[2];
}

void glVertex3i(GLint x, GLint y, GLint z)
{
	glVertex3f((GLfloat)x, (GLfloat)y, (GLfloat)z);
}

void glVertex3fv(const GLfloat *p)
{
	glVertex3f(p[0], p[1], p[2]);
}

void glVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
	glVertex3f(x, y, z);
	if (prim_n > 0)
		verts[prim_n - 1].w = w;
}

void glColor3f(GLfloat r, GLfloat g, GLfloat b)
{
	cur_color[0] = r; cur_color[1] = g; cur_color[2] = b;
	cur_color[3] = 1.0f;
}

void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
	cur_color[0] = r; cur_color[1] = g;
	cur_color[2] = b; cur_color[3] = a;
}

void glColor3fv(const GLfloat *p)  { glColor3f(p[0], p[1], p[2]); }
void glColor4fv(const GLfloat *p) { glColor4f(p[0], p[1], p[2], p[3]); }

void glColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a)
{
	glColor4f((GLfloat)r / 255.0f, (GLfloat)g / 255.0f,
		  (GLfloat)b / 255.0f, (GLfloat)a / 255.0f);
}

void glTexCoord2f(GLfloat u, GLfloat v)
{
	cur_uv[0] = u; cur_uv[1] = v;
}

void glTexCoord2i(GLint u, GLint v)
{
	cur_uv[0] = (GLfloat)u; cur_uv[1] = (GLfloat)v;
}

void glNormal3f(GLfloat x, GLfloat y, GLfloat z)
{
	cur_normal[0] = x; cur_normal[1] = y; cur_normal[2] = z;
}

void glNormal3fv(const GLfloat *p)
{
	glNormal3f(p[0], p[1], p[2]);
}

/* ------------------------------------------- per-vertex pipeline stages */

static int f2i(float f)          /* saturating float -> int */
{
	if (f <= 0.0f) return 0;
	if (f >= 1023.0f) return 1023;
	return (int)f;
}

static int c2i(float f)          /* float colour -> byte */
{
	int v = (int)(f * 255.0f);

	if (v < 0) return 0;
	if (v > 255) return 255;
	return v;
}

/* Map clip coordinates to screen; called once the clip w is known
 * positive.  Fills sx, sy, dist. */
static void project_vtx(Vtx *v)
{
	float inv = 1.0f / v->c[3];
	float ndc_x = v->c[0] * inv;
	float ndc_y = v->c[1] * inv;

	v->sx = (float)vp_x + (ndc_x * 0.5f + 0.5f) * (float)vp_w;
	v->sy = (float)vp_y + (float)vp_h -
		(ndc_y * 0.5f + 0.5f) * (float)vp_h;
	v->dist = v->c[3];
}

/* Transform one accumulated vertex: modelview, lighting, fog,
 * projection (clip coords cached in v->c). */
static void transform_vertex(Vtx *v)
{
	float ex, ey, ez, ew;

	ex = mv.m[0] * v->x + mv.m[4] * v->y + mv.m[8]  * v->z + mv.m[12];
	ey = mv.m[1] * v->x + mv.m[5] * v->y + mv.m[9]  * v->z + mv.m[13];
	ez = mv.m[2] * v->x + mv.m[6] * v->y + mv.m[10] * v->z + mv.m[14];
	ew = mv.m[3] * v->x + mv.m[7] * v->y + mv.m[11] * v->z + mv.m[15];
	(void)ew;

	/* lighting: one light, ambient + diffuse, in eye space */
	if (st_lighting && light0_on) {
		float nx, ny, nz, lx, ly, lz, nd;

		nx = mv.m[0] * v->nx + mv.m[4] * v->ny + mv.m[8]  * v->nz;
		ny = mv.m[1] * v->nx + mv.m[5] * v->ny + mv.m[9]  * v->nz;
		nz = mv.m[2] * v->nx + mv.m[6] * v->ny + mv.m[10] * v->nz;

		if (light_pos[3] == 0.0f) {
			/* directional light: direction precomputed once in
			 * glLightfv (light_dir) — no per-vertex sqrt */
			lx = light_dir[0];
			ly = light_dir[1];
			lz = light_dir[2];
		} else {
			float px = light_pos[0] / light_pos[3];
			float py = light_pos[1] / light_pos[3];
			float pz = light_pos[2] / light_pos[3];
			float dx = ex - px, dy = ey - py, dz = ez - pz;
			float len = (float)sqrt((double)(
				dx * dx + dy * dy + dz * dz));

			if (len > 0.0f) {
				float inv = 1.0f / len;

				lx = dx * inv; ly = dy * inv; lz = dz * inv;
			} else {
				lx = 0.0f; ly = 0.0f; lz = -1.0f;
			}
		}

		nd = nx * lx + ny * ly + nz * lz;
		if (nd < 0.0f)
			nd = 0.0f;

		v->r = mat_ambient[0] * light_ambient[0] +
		       mat_diffuse[0] * light_diffuse[0] * nd;
		v->g = mat_ambient[1] * light_ambient[1] +
		       mat_diffuse[1] * light_diffuse[1] * nd;
		v->b = mat_ambient[2] * light_ambient[2] +
		       mat_diffuse[2] * light_diffuse[2] * nd;
		v->a = mat_diffuse[3];
	}

	/* linear fog toward fog_color (eye distance = -ez) */
	if (st_fog && fog_end > fog_start) {
		float d = -ez;
		float f = (fog_end - d) / (fog_end - fog_start);

		if (f < 0.0f) f = 0.0f;
		if (f > 1.0f) f = 1.0f;
		v->r = v->r * f + fog_color[0] * (1.0f - f);
		v->g = v->g * f + fog_color[1] * (1.0f - f);
		v->b = v->b * f + fog_color[2] * (1.0f - f);
	}

	/* projection -> clip coords */
	v->c[0] = proj.m[0] * ex + proj.m[4] * ey +
		  proj.m[8]  * ez + proj.m[12];
	v->c[1] = proj.m[1] * ex + proj.m[5] * ey +
		  proj.m[9]  * ez + proj.m[13];
	v->c[2] = proj.m[2] * ex + proj.m[6] * ey +
		  proj.m[10] * ez + proj.m[14];
	v->c[3] = proj.m[3] * ex + proj.m[7] * ey +
		  proj.m[11] * ez + proj.m[15];

	if (v->c[3] > W_EPS)
		project_vtx(v);
	else
		v->dist = 0.0f;    /* behind the eye; clipper decides */
}

/* ------------------------------------------- primitive packet emission */

static u32 xy_word(const Vtx *v)
{
	return ((u32)f2i(v->sy) << 16) | (u32)f2i(v->sx);
}

static u32 col_word(const Vtx *v)
{
	return (u32)(c2i(v->r) | (c2i(v->g) << 8) | (c2i(v->b) << 16));
}

static u32 uv_word(const Vtx *v)
{
	int u, vv;

	if (bound_tex) {
		u  = (int)(v->u * (float)bound_tex->w) & 0xFF;
		vv = (int)(v->v * (float)bound_tex->h) & 0xFF;
	} else {
		u = 0; vv = 0;
	}
	return (u32)u | ((u32)vv << 8);
}

static int depth_bucket(const Vtx *a, const Vtx *b,
			const Vtx *c, const Vtx *d)
{
	float dist;

	if (d)
		dist = (a->dist + b->dist + c->dist + d->dist) * 0.25f;
	else
		dist = (a->dist + b->dist + c->dist) / 3.0f;
	return f2i(dist * (float)OT_SIZE / 256.0f);
}

static void emit_packet(u_long *pk, int n, const Vtx *const *vp, int nv)
{
	int bucket = depth_bucket(vp[0], vp[1],
				  nv > 2 ? vp[2] : vp[1], vp[3]);

	pk[1] = n;
	pkt_link(active_buff ^ 1, bucket, pk);
}

/* Emit one clipped screen-space triangle. */
static void emit_tri(const Vtx *a, const Vtx *b, const Vtx *c)
{
	u32 cmd;
	int semi = st_blend ? 1 : 0;
	int gouraud = st_shade_smooth;
	u_long *pk;
	int n = 0;

	if (st_texture_2d && bound_tex) {
		u32 tsb = (u32)bound_tex->tpage_bits;
		const Vtx *vp[3];

		/* semi-transparency = ABE = command bit 25, i.e. +2 in
		 * the low byte (bit 24 is TGE: raw-texture mode) */
		cmd = ((gouraud ? 0x34u : 0x24u) | ((u32)semi << 1)) << 24;
		pk = pkt_new(gouraud ? 9 : 7);
		if (!pk)
			return;
		pk[2 + n++] = cmd | col_word(a);
		pk[2 + n++] = xy_word(a);
		pk[2 + n++] = uv_word(a);                   /* cba = 0 */
		if (gouraud) pk[2 + n++] = col_word(b);
		pk[2 + n++] = xy_word(b);
		pk[2 + n++] = uv_word(b) | (tsb << 16);
		if (gouraud) pk[2 + n++] = col_word(c);
		pk[2 + n++] = xy_word(c);
		pk[2 + n++] = uv_word(c);
		vp[0] = a; vp[1] = b; vp[2] = c;
		emit_packet(pk, n, vp, 3);
	} else {
		const Vtx *vp[4];

		cmd = ((gouraud ? 0x30u : 0x20u) | ((u32)semi << 1)) << 24;
		pk = pkt_new(gouraud ? 6 : 4);
		if (!pk)
			return;
		pk[2 + n++] = cmd | col_word(a);
		pk[2 + n++] = xy_word(a);
		if (gouraud) pk[2 + n++] = col_word(b);
		pk[2 + n++] = xy_word(b);
		if (gouraud) pk[2 + n++] = col_word(c);
		pk[2 + n++] = xy_word(c);
		vp[0] = a; vp[1] = b; vp[2] = c; vp[3] = 0;
		emit_packet(pk, n, vp, 3);
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
	int semi = st_blend ? 1 : 0;
	int gouraud = st_shade_smooth;
	u_long *pk;
	int n = 0;
	const Vtx *vp[4];

	if (st_texture_2d && bound_tex) {
		u32 tsb = (u32)bound_tex->tpage_bits;

		cmd = ((gouraud ? 0x3Cu : 0x2Cu) | ((u32)semi << 1)) << 24;
		pk = pkt_new(gouraud ? 12 : 9);
		if (!pk)
			return;
		pk[2 + n++] = cmd | col_word(a);
		pk[2 + n++] = xy_word(a);
		pk[2 + n++] = uv_word(a);
		if (gouraud) pk[2 + n++] = col_word(b);
		pk[2 + n++] = xy_word(b);
		pk[2 + n++] = uv_word(b) | (tsb << 16);
		if (gouraud) pk[2 + n++] = col_word(c);
		pk[2 + n++] = xy_word(c);
		pk[2 + n++] = uv_word(c);
		if (gouraud) pk[2 + n++] = col_word(d);
		pk[2 + n++] = xy_word(d);
		pk[2 + n++] = uv_word(d);
	} else {
		cmd = ((gouraud ? 0x38u : 0x28u) | ((u32)semi << 1)) << 24;
		pk = pkt_new(gouraud ? 8 : 5);
		if (!pk)
			return;
		pk[2 + n++] = cmd | col_word(a);
		pk[2 + n++] = xy_word(a);
		if (gouraud) pk[2 + n++] = col_word(b);
		pk[2 + n++] = xy_word(b);
		if (gouraud) pk[2 + n++] = col_word(c);
		pk[2 + n++] = xy_word(c);
		if (gouraud) pk[2 + n++] = col_word(d);
		pk[2 + n++] = xy_word(d);
	}
	vp[0] = a; vp[1] = b; vp[2] = c; vp[3] = d;
	emit_packet(pk, n, vp, 4);
}

/* Emit one clipped screen-space line. */
static void emit_line(const Vtx *a, const Vtx *b)
{
	u32 cmd = (0x40u | ((st_blend ? 1u : 0u) << 1)) << 24;
	u_long *pk;
	const Vtx *vp[4];

	pk = pkt_new(3);
	if (!pk)
		return;
	pk[2] = cmd | col_word(a);
	pk[3] = xy_word(a);
	pk[4] = xy_word(b);
	vp[0] = a; vp[1] = b; vp[2] = a; vp[3] = 0;
	emit_packet(pk, 3, vp, 2);
}

/* ------------------------------------------------ near-plane clipping */

static void clip_lerp(Vtx *out, const Vtx *a, const Vtx *b, float t)
{
	int i;

	for (i = 0; i < 4; i++)
		out->c[i] = a->c[i] + (b->c[i] - a->c[i]) * t;
	out->r = a->r + (b->r - a->r) * t;
	out->g = a->g + (b->g - a->g) * t;
	out->b = a->b + (b->b - a->b) * t;
	out->a = a->a + (b->a - a->a) * t;
	out->u = a->u + (b->u - a->u) * t;
	out->v = a->v + (b->v - a->v) * t;

	/* screen position from the clipped clip-coords */
	if (out->c[3] > W_EPS)
		project_vtx(out);
	else
		out->dist = 0.0f;
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
		if (v[i].c[3] <= W_EPS)
			break;
	if (i == n)
		return n;

	for (i = 0; i < n; i++) {
		const Vtx *a = &v[i];
		const Vtx *b = &v[(i + 1) % n];
		int ain = a->c[3] > W_EPS;
		int bin = b->c[3] > W_EPS;

		if (ain) {
			out[m++] = *a;
			if (m >= MAX_VERTS) break;
		}
		if (ain != bin) {
			float t = (a->c[3] - W_EPS) /
				  (a->c[3] - b->c[3]);

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
