/*
 * libpsxdev.h - Userspace game development library for PSXLinux.
 *
 * Provides GPU graphics (via /dev/psxgpu) and SPU sound (via /dev/psxspu).
 * Register format reference: https://psx-spx.github.io/
 *
 * Copyright (C) 2026 PSXLinux contributors.
 * Licensed under the GPL.
 */

#ifndef _LIBPSXDEV_H
#define _LIBPSXDEV_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ---- Graphics (GPU) ----
 */

/* Initialize the graphics library (opens /dev/psxgpu, resets GPU) */
int psxgfx_init(void);
void psxgfx_close(void);

/* Set video mode (320x240, 15bpp, NTSC by default) */
int psxgfx_setmode(int width, int height);

/* Clear the drawing area to a solid color */
void psxgfx_clear(int r, int g, int b);

/* Draw a filled rectangle */
void psxgfx_rect(int x, int y, int w, int h, int r, int g, int b);

/* Draw a flat-shaded triangle */
void psxgfx_triangle_flat(int x0, int y0, int x1, int y1,
			   int x2, int y2, int r, int g, int b);

/* Draw a gouraud-shaded triangle (each vertex has its own color) */
void psxgfx_triangle_gouraud(int x0, int y0, int r0, int g0, int b0,
			     int x1, int y1, int r1, int g1, int b1,
			     int x2, int y2, int r2, int g2, int b2);

/* Draw a textured 4-point quad (sprite) */
void psxgfx_quad_textured(int x0, int y0, int u0, int v0,
			  int x1, int y1, int u1, int v1,
			  int x2, int y2, int u2, int v2,
			  int x3, int y3, int u3, int v3,
			  int tpage, int clut,
			  int r, int g, int b);

/* Upload a 16bpp image to VRAM */
int psxgfx_upload_vram(int x, int y, int w, int h, const void *data);

/* Set the display area in VRAM (for double buffering) */
void psxgfx_display_area(int x, int y);

/* Set the drawing area in VRAM */
void psxgfx_draw_area(int x0, int y0, int x1, int y1);

/* Wait for vertical blank (use once per frame) */
void psxgfx_vsync(void);

/* Swap draw/display buffers for double buffering */
void psxgfx_flip(void);

/* Read the GPU status register */
unsigned int psxgfx_status(void);

/*
 * ---- Sound (SPU) ----
 */

/* Initialize the sound library (opens /dev/psxspu) */
int psxsnd_init(void);
void psxsnd_close(void);

/* Set master volume (0..0x3FFF) */
void psxsnd_master_volume(int vol);

/* Upload a 16-bit signed sample to sound RAM at the given halfword offset */
int psxsnd_upload(int halfword_addr, const short *samples, int count);

/* Configure a voice (0-23) */
void psxsnd_voice(int voice, int vol_l, int vol_r,
		  int pitch, int start_halfwords,
		  int adsr_lo, int adsr_hi);

/* Start playing a voice (key on) */
void psxsnd_play(int voice);

/* Stop playing a voice (key off) */
void psxsnd_stop(int voice);

/* Convenience: generate and upload a square-wave beep */
int psxsnd_beep(int voice, int freq_hz, int volume);

/*
 * ---- Utility ----
 */

/* Current frame buffer coordinates (for double buffering) */
extern int psxgfx_display_x, psxgfx_display_y;
extern int psxgfx_draw_x, psxgfx_draw_y;
extern int psxgfx_screen_w, psxgfx_screen_h;

#ifdef __cplusplus
}
#endif

#endif /* _LIBPSXDEV_H */
