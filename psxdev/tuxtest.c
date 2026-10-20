/*
 * tuxtest.c — Draw Tux (the Linux mascot) with minigl.
 *
 * A standalone library smoke test: if Tux shows up on screen,
 * the /dev/psxgpu pipeline (write batching, GP1 ioctls), the
 * ordering-table depth sort, flat/gouraud emission and the
 * double-buffer swap all work.  No textures, no game logic.
 *
 * Geometry: 2D shapes on the z=0..6 planes (z = paint layer,
 * the OT sorts back-to-front), seen through a perspective camera
 * placed so world (0..320, 0..240) maps 1:1 onto the screen.
 */

#include "minigl.h"
#include "psxsys.h"

typedef unsigned int   u32;
typedef unsigned short u16;
typedef unsigned char  u8;

extern int printf(const char *fmt, ...);
extern double cos(double);
extern double sin(double);
extern int glMiniInit(void);
extern void glSwapBuffersPSX(void);
extern void glGenTextures(GLsizei n, GLuint *textures);
extern void glBindTexture(GLenum target, GLuint id);
extern void glTexImage2DPSX(GLuint t, GLsizei w, GLsizei h,
			     const unsigned short *hw);
static GLuint tux_tex;

/* ------------------------------------------------------------- helpers */

#define PI 3.14159265f

static void fan(float cx, float cy, float rx, float ry, float z,
		int segments, float r, float g, float b)
{
	float ang;
	int i;

	glBegin(GL_TRIANGLE_FAN);
	glColor3f(r, g, b);
	glVertex3f(cx, cy, z);
	for (i = 0; i <= segments; i++) {
		ang = 2.0f * PI * (float)i / (float)segments;
		glVertex3f(cx + rx * cos(ang), cy + ry * sin(ang), z);
	}
	glEnd();
}

/* One Tux, drawn bottom-to-top via the z layering. */
static void drawTux(void)
{
	/* Full-screen textured checkerboard quad over a gray clear:
	 * regression test for the glClear fill packet, the quad
	 * zigzag vertex order and the texture pipeline in one frame.
	 * Tux goes on top, centered. */
	glClearColor(0.5, 0.5, 0.5, 0.0);

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, tux_tex);  /* re-bind: the first
		* bind (before glTexImage2DPSX) cannot stick, the
		* texture is only 'used' once registered */
	glBegin(GL_QUADS);
	glColor3f(1.0f, 1.0f, 1.0f);
	glTexCoord2f(0.0f, 0.0f);  glVertex3f(  0.0f,   0.0f, -1.0f);
	glTexCoord2f(5.0f, 0.0f);  glVertex3f(320.0f,   0.0f, -1.0f);
	glTexCoord2f(5.0f, 4.0f);  glVertex3f(320.0f, 240.0f, -1.0f);
	glTexCoord2f(0.0f, 4.0f);  glVertex3f(  0.0f, 240.0f, -1.0f);
	glEnd();
	glDisable(GL_TEXTURE_2D);

	/* small untextured white patch, bottom-right corner
	 * (does not cover the checkerboard) */
	glBegin(GL_QUADS);
	glColor3f(1.0f, 1.0f, 1.0f);
	glVertex3f(280.0f, 200.0f, -0.5f);
	glVertex3f(320.0f, 200.0f, -0.5f);
	glVertex3f(320.0f, 240.0f, -0.5f);
	glVertex3f(280.0f, 240.0f, -0.5f);
	glEnd();

	/* feet (orange) */
	fan(128.0f,  42.0f, 24.0f, 11.0f, 0.0f, 12, 1.0f, 0.55f, 0.0f);
	fan(192.0f,  42.0f, 24.0f, 11.0f, 0.0f, 12, 1.0f, 0.55f, 0.0f);

	/* wings / flippers (black) */
	fan(100.0f, 118.0f, 11.0f, 36.0f, 1.0f, 12, 0.0f, 0.0f, 0.0f);
	fan(220.0f, 118.0f, 11.0f, 36.0f, 1.0f, 12, 0.0f, 0.0f, 0.0f);

	/* body: black egg */
	fan(160.0f, 118.0f, 56.0f, 80.0f, 2.0f, 20, 0.0f, 0.0f, 0.0f);

	/* belly (white) */
	fan(160.0f, 103.0f, 38.0f, 55.0f, 3.0f, 16, 1.0f, 1.0f, 1.0f);

	/* beak (orange) */
	fan(160.0f, 152.0f, 15.0f, 9.0f, 4.0f, 10, 1.0f, 0.55f, 0.0f);

	/* eyes (white) + pupils (black) */
	fan(141.0f, 170.0f, 10.0f, 12.0f, 5.0f, 10, 1.0f, 1.0f, 1.0f);
	fan(179.0f, 170.0f, 10.0f, 12.0f, 5.0f, 10, 1.0f, 1.0f, 1.0f);
	fan(143.0f, 167.0f,  4.0f,  4.0f, 6.0f,  8, 0.0f, 0.0f, 0.0f);
	fan(177.0f, 167.0f,  4.0f,  4.0f, 6.0f,  8, 0.0f, 0.0f, 0.0f);
}

/* --------------------------------------------------------------- entry */

int main(int argc, char **argv)
{
	int frame;

	(void)argc; (void)argv;

	if (glMiniInit() < 0) {
		printf("TUXTEST: cannot open /dev/psxgpu\n");
		return 1;
	}
	printf("TUXTEST: /dev/psxgpu ok, drawing Tux\n");

	/* camera: frustum exactly covers (0..320, 0..240) at z=0 */
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(90.0, 320.0 / 240.0, 1.0, 1000.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(160.0, 120.0, 120.0,   /* eye */
		  160.0, 120.0,   0.0,   /* target */
		    0.0,   1.0,   0.0);  /* up */
	glViewport(0, 0, 320, 240);

	/* build a 64x64 checkerboard texture (8x8 blocks, red/yellow) */
	{
		static unsigned char px[64 * 64 * 4];
		static unsigned short hw[64 * 64];
		int i;

		for (i = 0; i < 64 * 64; i++) {
			int cx = (i % 64) / 8;
			int cy = (i / 64) / 8;
			int odd = (cx + cy) & 1;

			px[i * 4 + 0] = odd ? 255 : 0;    /* r */
			px[i * 4 + 1] = odd ? 200 : 0;    /* g */
			px[i * 4 + 2] = odd ? 0 : 255;   /* b */
			px[i * 4 + 3] = 255;              /* a */
			hw[i] = (unsigned short)
				((px[i * 4 + 0] >> 3) |
				 ((px[i * 4 + 1] >> 3) << 5) |
				 ((px[i * 4 + 2] >> 3) << 10));
		}
		glGenTextures(1, &tux_tex);
		glBindTexture(GL_TEXTURE_2D, tux_tex);
		glTexImage2DPSX(tux_tex, 64, 64, hw);
	}

	for (frame = 0; ; frame++) {
		glClearColor(0.5, 0.5, 0.5, 0.0);
		glClear(GL_COLOR_BUFFER_BIT);

		drawTux();

		glSwapBuffersPSX();

		if ((frame & 0x3F) == 0)
			printf("TUXTEST: frame %d\n", frame);
	}
	return 0;
}

void __main(void) { }

__attribute__((section(".text.start")))
void _start(void)
{
	int ret = main(0, 0);
	sys_exit(ret);
	for (;;) ;
}
