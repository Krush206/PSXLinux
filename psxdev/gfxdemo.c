/*
 * gfxdemo.c - Graphics rendering demo for PSXLinux.
 *
 * Draws an animated 3D-style scene directly on the PSX GPU hardware.
 * Uses the kernel's /dev/psxgpu device node (via syscalls) when
 * available, or falls back to direct GP0 register access (which
 * always works on the PSX since hardware is memory-mapped).
 *
 * Builds on the libpsxdev environment. See psxdev/Makefile.
 *
 * GPU reference: https://psx-spx.github.io/#gpu-gp0-commands
 */

#include "psxsys.h"
#include "psxdev.h"

/*
 * Entry point
 */
sint main(void);

/* GCC 2.91 MIPS calls __main() before main() to run global ctors */
void __main(void) { }

__attribute__((section(".text.start")))
void _start(void)
{
	sint ret;
	ret = main();
	sys_exit(ret);
	for (;;) ;
}

/*
 * GPU register addresses (physical, directly accessible on PSX)
 */
#define GP0_REG		(*(volatile unsigned int  *)0x1F801810)
#define GP1_REG		(*(volatile unsigned int  *)0x1F801814)

#define GPUSTAT_FIFO_READY	(1u << 28)
#define GPUSTAT_VBLANK		(1u << 31)

/*
 * Screen dimensions (320x240 NTSC)
 */
#define SCREEN_W	320
#define SCREEN_H	240

/*
 * Wait until the GPU can accept a GP0 command
 */
static void gpu_wait_fifo(void)
{
	while (!(GP1_REG & GPUSTAT_FIFO_READY))
		;
}

/*
 * Send a 32-bit word to the GP0 command port
 */
static void gpu_send(unsigned int word)
{
	gpu_wait_fifo();
	GP0_REG = word;
}

/*
 * Initialize the GPU: set video mode, display area, drawing area
 */
static void gpu_init(void)
{
	/* Soft reset */
	GP1_REG = 0x00000000;

	/* Display enable */
	GP1_REG = 0x03000000;

	/* NTSC 320x240 15bpp non-interlaced (GP1 08h) */
	GP1_REG = 0x08000000;

	/* Display area starts at VRAM (0,0) (GP1 05h) */
	GP1_REG = 0x05000000;

	/* Horizontal display range: 0x260 to 0xC60 (GP1 06h) */
	GP1_REG = 0x0602C0C60;

	/* Vertical display range: 0x10 to 0x100 (GP1 07h) */
	GP1_REG = 0x07000010;

	/* Drawing area top-left: (0,0) — GP0 E3h */
	gpu_send(0xE3000000);

	/* Drawing Area bottom-right: (319,239) — GP0 E4h */
	gpu_send(0xE4000000 | (239u << 10) | 319u);

	/* Drawing offset: (0,0) — GP0 E5h */
	gpu_send(0xE5000000);

	/* Disable mask bit, no dithering — GP0 E6h */
	gpu_send(0xE6000000);
}

/*
 * Clear the drawing area to a solid color.
 * GP0 02h: fill rectangle in VRAM
 */
static void gpu_clear(int r, int g, int b)
{
	gpu_send(0x02000000);					/* command */
	gpu_send(((unsigned int)r << 0) |
		 ((unsigned int)g << 8) |
		 ((unsigned int)b << 16));			/* color */
	gpu_send(0x00000000);					/* x,y = (0,0) */
	gpu_send((SCREEN_H << 16) | SCREEN_W);			/* w,h */
}

/*
 * Draw a filled rectangle.
 * GP0 60h: variable-size fill rectangle (1-word size)
 */
static void gpu_rect(int x, int y, int w, int h, int r, int g, int b)
{
	gpu_send(0x60000000 |
		 ((unsigned int)r << 0) |
		 ((unsigned int)g << 8) |
		 ((unsigned int)b << 16));
	gpu_send(((unsigned int)y << 16) | (unsigned int)x);
	gpu_send(((unsigned int)h << 16) | (unsigned int)w);
}

/*
 * Draw a flat-shaded triangle.
 * GP0 20h: monochrome triangle, 3 vertices
 */
static void gpu_tri_flat(int x0, int y0, int x1, int y1,
			 int x2, int y2,
			 int r, int g, int b)
{
	gpu_send(0x20000000 |
		 ((unsigned int)r << 0) |
		 ((unsigned int)g << 8) |
		 ((unsigned int)b << 16));
	gpu_send(((unsigned int)y0 << 16) | (unsigned int)x0);
	gpu_send(((unsigned int)y1 << 16) | (unsigned int)x1);
	gpu_send(((unsigned int)y2 << 16) | (unsigned int)x2);
}

/*
 * Draw a gouraud-shaded triangle.
 * GP0 30h: gouraud triangle, 3 vertices each with RGB
 */
static void gpu_tri_gouraud(int x0, int y0, int r0, int g0, int b0,
			    int x1, int y1, int r1, int g1, int b1,
			    int x2, int y2, int r2, int g2, int b2)
{
	gpu_send(0x30000000 |
		 ((unsigned int)r0 << 0) |
		 ((unsigned int)g0 << 8) |
		 ((unsigned int)b0 << 16));
	gpu_send(((unsigned int)y0 << 16) | (unsigned int)x0);
	gpu_send(((unsigned int)r1 << 0) |
		 ((unsigned int)g1 << 8) |
		 ((unsigned int)b1 << 16));
	gpu_send(((unsigned int)y1 << 16) | (unsigned int)x1);
	gpu_send(((unsigned int)r2 << 0) |
		 ((unsigned int)g2 << 8) |
		 ((unsigned int)b2 << 16));
	gpu_send(((unsigned int)y2 << 16) | (unsigned int)x2);
}

/*
 * Draw a line (GP0 40h: monochrome line)
 */
static void gpu_line(int x0, int y0, int x1, int y1,
		     int r, int g, int b)
{
	gpu_send(0x40000000 |
		 ((unsigned int)r << 0) |
		 ((unsigned int)g << 8) |
		 ((unsigned int)b << 16));
	gpu_send(((unsigned int)y0 << 16) | (unsigned int)x0);
	gpu_send(((unsigned int)y1 << 16) | (unsigned int)x1);
}

/*
 * Wait for vertical blank by polling GPUSTAT bit 31.
 * On real hardware this fires at ~60Hz (NTSC).
 */
static void gpu_wait_vblank(void)
{
	/* Wait for VBlank to start */
	while (!(GP1_REG & GPUSTAT_VBLANK))
		;
	/* Wait for VBlank to end (frame is done) */
	while (GP1_REG & GPUSTAT_VBLANK)
		;
}

/*
 * Simple software 3D projection: project a 3D point onto the 2D screen.
 * Returns screen coordinates in the given pointers.
 */
static void project3d(sint x, sint y, sint z,
		     int *sx, int *sy, int scale)
{
	int denom = z + 256;	/* camera at z=-256 */
	if (denom < 1)
		denom = 1;
	*sx = SCREEN_W / 2 + (x * scale) / denom;
	*sy = SCREEN_H / 2 - (y * scale) / denom;
}

/*
 * Draw a rotating 3D cube using gouraud-shaded triangles.
 * The cube has 6 faces, each drawn as 2 triangles.
 */
static void draw_cube(int angle, int scale)
{
	/* Precomputed sin/cos table for 64 angles */
	static const short sintab[64] = {
		0,   6,  12,  18,  24,  30,  36,  42,
	       48,  53,  58,  63,  67,  71,  75,  78,
	       80,  82,  83,  84,  84,  83,  82,  80,
	       78,  75,  71,  67,  63,  58,  53,  48,
	       42,  36,  30,  24,  18,  12,   6,   0,
	       -6, -12, -18, -24, -30, -36, -42, -48,
	      -53, -58, -63, -67, -71, -75, -78, -80,
	      -82, -83, -84, -84, -83, -82, -80, -78
	};
	int s = angle & 63;
	int s2 = (angle + 16) & 63;

	/* Cube vertices (before rotation): ±50 in each axis */
	/* We rotate around Y axis using sin/cos */
	int cos_a = sintab[s2];
	int sin_a = sintab[s];

	/* 8 cube vertices, rotated around Y */
	int vx[8], vy[8], vz[8];
	int i;
	int px[8], py[8];

	for (i = 0; i < 8; i++) {
		int x = (i & 1) ? 50 : -50;
		int y = (i & 2) ? 50 : -50;
		int z = (i & 4) ? 50 : -50;

		/* Rotate around Y: x' = x*cos + z*sin, z' = -x*sin + z*cos */
		vx[i] = (x * cos_a + z * sin_a) / 80;
		vy[i] = y;
		vz[i] = (-x * sin_a + z * cos_a) / 80;

		/* Project onto 2D screen */
		project3d(vx[i], vy[i], vz[i], &px[i], &py[i], scale);
	}

	/* Draw the 6 faces as gouraud triangles */
	/* Front face (z>0): vertices 1,3,7,5 */
	gpu_tri_gouraud(px[1], py[1], 255, 0, 0,
			px[3], py[3], 200, 0, 0,
			px[7], py[7], 150, 0, 0);
	gpu_tri_gouraud(px[1], py[1], 255, 0, 0,
			px[7], py[7], 150, 0, 0,
			px[5], py[5], 200, 0, 0);

	/* Top face (y>0): vertices 2,3,7,6 */
	gpu_tri_gouraud(px[2], py[2], 0, 255, 0,
			px[3], py[3], 0, 200, 0,
			px[7], py[7], 0, 150, 0);
	gpu_tri_gouraud(px[2], py[2], 0, 255, 0,
			px[7], py[7], 0, 150, 0,
			px[6], py[6], 0, 200, 0);

	/* Right face (x>0): vertices 1,5,7,3 */
	gpu_tri_gouraud(px[1], py[1], 0, 0, 255,
			px[5], py[5], 0, 0, 200,
			px[7], py[7], 0, 0, 150);
	gpu_tri_gouraud(px[1], py[1], 0, 0, 255,
			px[7], py[7], 0, 0, 150,
			px[3], py[3], 0, 0, 200);
}

/*
 * Main: render an animated scene
 */
sint main(void)
{
	int frame;

	/* Initialize the GPU */
	gpu_init();

	/* Animation loop */
	for (frame = 0; frame < 1800; frame++) {
		int x, y;

		/* Clear to dark blue */
		gpu_clear(0, 0, 32);

		/* Draw a "ground" grid */
		for (y = 160; y < SCREEN_H; y += 20) {
			gpu_line(0, y, SCREEN_W, y, 32, 32, 64);
		}
		for (x = 0; x < SCREEN_W; x += 40) {
			gpu_line(x, 160, x, SCREEN_H, 32, 32, 64);
		}

		/* Draw a rotating 3D cube in the upper area */
		draw_cube(frame, 120);

		/* Draw a moving sun (orange circle approximated as triangles) */
		{
			int sun_x = 160 + 100 * frame / 1800;
			int sun_y = 50;
			int j;
			for (j = 0; j < 8; j++) {
				int a = j * 8;
				int b = (j + 1) * 8;
				gpu_tri_flat(sun_x, sun_y,
					     sun_x + (a - 32), sun_y - 32 + a / 2,
					     sun_x + (b - 32), sun_y - 32 + b / 2,
					     255, 200, 0);
			}
		}

		/* Draw animated rectangles */
		x = (frame * 3) % (SCREEN_W - 40);
		gpu_rect(x, 180, 40, 40, 255, 0, 0);
		gpu_rect(SCREEN_W - 40 - x, 200, 30, 30, 0, 255, 0);

		/* Draw a flat green triangle */
		gpu_tri_flat(50 + (frame % 100), 100,
			     150, 100,
			     100, 150,
			     0, 255, 0);

		/* Wait for next frame (~60Hz) */
		gpu_wait_vblank();
	}

	/* Fade to black */
	for (frame = 255; frame >= 0; frame -= 5) {
		gpu_clear(frame, frame, frame);
		gpu_wait_vblank();
	}

	return 0;
}
