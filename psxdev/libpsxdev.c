/*
 * libpsxdev.c - Userspace game development library for PSXLinux.
 *
 * Graphics via /dev/psxgpu (kernel char device, major 210)
 * Sound via /dev/psxspu (kernel char device, major 211)
 *
 * GPU command format reference: https://psx-spx.github.io/#gpu-gp0-commands
 * SPU register reference: https://psx-spx.github.io/#spu-io-ports
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/psxgpu.h>
#include <linux/psxspu.h>

#include "libpsxdev.h"

/* File descriptors */
static int gpu_fd = -1;
static int spu_fd = -1;

/* Double-buffer coordinates */
int psxgfx_display_x = 0, psxgfx_display_y = 0;
int psxgfx_draw_x = 0, psxgfx_draw_y = 240;
int psxgfx_screen_w = 320;
int psxgfx_screen_h = 240;

/*
 * ---------------- Graphics ----------------
 */

int psxgfx_init(void)
{
	gpu_fd = open("/dev/psxgpu", O_RDWR);
	if (gpu_fd < 0) {
		perror("psxgfx: open /dev/psxgpu");
		return -1;
	}

	/* Soft reset */
	ioctl(gpu_fd, PSXGPU_IOC_RESET, 0);

	/* GP1 03h: enable display */
	ioctl(gpu_fd, PSXGPU_IOC_GP1, 0x03000000);

	/* GP1 08h: NTSC 320x240 15bpp non-interlaced */
	/*   bit 0-1: 0=NTSC  1=PAL
	 *   bit 5:   0=15bpp direct  1=8bpp CLUT
	 *   bit 6:   0=normal (not interlace) */
	ioctl(gpu_fd, PSXGPU_IOC_GP1, 0x08000000);

	/* GP1 05h: display start at (0,0) */
	ioctl(gpu_fd, PSXGPU_IOC_GP1, 0x05000000);

	/* GP1 06h: horizontal display range (standard NTSC) */
	/*   x1=0x260, x2=0xC60 (in GPU clock units) */
	ioctl(gpu_fd, PSXGPU_IOC_GP1, 0x0602C0C60);

	/* GP1 07h: vertical display range (standard NTSC) */
	/*   y1=0x10, y2=0x100 */
	ioctl(gpu_fd, PSXGPU_IOC_GP1, 0x07000010);

	/* Set drawing area: top-left = (0,0), bottom-right = (320,240) */
	psxgfx_draw_area(0, 0, 320, 240);

	/* Set display area to VRAM (0,0) */
	psxgfx_display_area(0, 0);

	/* Set drawing offset within the drawing area to (0,0) */
	{
		uint32_t cmd[2];
		cmd[0] = 0xE1000000u | ((psxgfx_draw_x & 0x7FF) > 0) |
			 ((psxgfx_draw_y & 0x3FF) << 11);
		/* GP0 E1h: drawing offset (tex page X | tex page Y<<4 | ...) */
		cmd[0] = 0xE1000000u | 0x0000;
		cmd[1] = 0;
		(void)cmd;
	}

	return 0;
}

void psxgfx_close(void)
{
	if (gpu_fd >= 0) {
		close(gpu_fd);
		gpu_fd = -1;
	}
}

int psxgfx_setmode(int width, int height)
{
	psxgfx_screen_w = width;
	psxgfx_screen_h = height;
	psxgfx_draw_area(0, 0, width, height);
	return 0;
}

static void gpu_write(const uint32_t *words, int count)
{
	if (gpu_fd < 0)
		return;
	write(gpu_fd, words, count * 4);
}

void psxgfx_clear(int r, int g, int b)
{
	/*
	 * GP0 02h: fill VRAM rectangle.
	 *   word 0: 02h (command)
	 *   word 1: color (R,G,B each 8 bits)
	 *   word 2: x,y (top-left, relative to drawing area)
	 *   word 3: w,h (width, height)
	 */
	uint32_t cmd[4];
	cmd[0] = 0x02000000u;
	cmd[1] = ((uint32_t)r << 0) | ((uint32_t)g << 8) | ((uint32_t)b << 16);
	cmd[2] = ((uint32_t)psxgfx_draw_y << 16) | (uint32_t)psxgfx_draw_x;
	cmd[3] = ((uint32_t)psxgfx_screen_h << 16) | (uint32_t)psxgfx_screen_w;
	gpu_write(cmd, 4);
}

void psxgfx_rect(int x, int y, int w, int h, int r, int g, int b)
{
	/* GP0 60h: fill rectangle (variable size) */
	uint32_t cmd[3];
	cmd[0] = 0x60000000u |
		 ((uint32_t)r << 0) | ((uint32_t)g << 8) | ((uint32_t)b << 16);
	cmd[1] = ((uint32_t)(y + psxgfx_draw_y) << 16) |
		 (uint32_t)(x + psxgfx_draw_x);
	cmd[2] = ((uint32_t)h << 16) | (uint32_t)w;
	gpu_write(cmd, 3);
}

void psxgfx_triangle_flat(int x0, int y0, int x1, int y1,
			  int x2, int y2, int r, int g, int b)
{
	/* GP0 20h: flat-shaded triangle */
	uint32_t cmd[4];
	cmd[0] = 0x20000000u |
		 ((uint32_t)r << 0) | ((uint32_t)g << 8) | ((uint32_t)b << 16);
	cmd[1] = ((uint32_t)(y0 + psxgfx_draw_y) << 16) |
		 (uint32_t)(x0 + psxgfx_draw_x);
	cmd[2] = ((uint32_t)(y1 + psxgfx_draw_y) << 16) |
		 (uint32_t)(x1 + psxgfx_draw_x);
	cmd[3] = ((uint32_t)(y2 + psxgfx_draw_y) << 16) |
		 (uint32_t)(x2 + psxgfx_draw_x);
	gpu_write(cmd, 4);
}

void psxgfx_triangle_gouraud(int x0, int y0, int r0, int g0, int b0,
			     int x1, int y1, int r1, int g1, int b1,
			     int x2, int y2, int r2, int g2, int b2)
{
	/* GP0 30h: gouraud-shaded triangle */
	uint32_t cmd[6];
	cmd[0] = 0x30000000u |
		 ((uint32_t)r0 << 0) | ((uint32_t)g0 << 8) | ((uint32_t)b0 << 16);
	cmd[1] = ((uint32_t)(y0 + psxgfx_draw_y) << 16) |
		 (uint32_t)(x0 + psxgfx_draw_x);
	cmd[2] = ((uint32_t)r1 << 0) | ((uint32_t)g1 << 8) | ((uint32_t)b1 << 16);
	cmd[3] = ((uint32_t)(y1 + psxgfx_draw_y) << 16) |
		 (uint32_t)(x1 + psxgfx_draw_x);
	cmd[4] = ((uint32_t)r2 << 0) | ((uint32_t)g2 << 8) | ((uint32_t)b2 << 16);
	cmd[5] = ((uint32_t)(y2 + psxgfx_draw_y) << 16) |
		 (uint32_t)(x2 + psxgfx_draw_x);
	gpu_write(cmd, 6);
}

void psxgfx_quad_textured(int x0, int y0, int u0, int v0,
			  int x1, int y1, int u1, int v1,
			  int x2, int y2, int u2, int v2,
			  int x3, int y3, int u3, int v3,
			  int tpage, int clut,
			  int r, int g, int b)
{
	/* GP0 2Ch + variation: textured 4-point quad, opaque, blending */
	uint32_t cmd[9];
	cmd[0] = 0x2C000000u | 0x01; /* semi-transparency: 1 = B-F/2 */
	/* Actually for opaque textured quad use 0x2C + modulate */
	cmd[0] = 0x2C000000u |
		 ((uint32_t)r << 0) | ((uint32_t)g << 8) | ((uint32_t)b << 16);
	cmd[1] = ((uint32_t)(y0 + psxgfx_draw_y) << 16) |
		 (uint32_t)(x0 + psxgfx_draw_x);
	cmd[2] = ((uint32_t)v0 << 8) | (uint32_t)u0;
	cmd[3] = ((uint32_t)(y1 + psxgfx_draw_y) << 16) |
		 (uint32_t)(x1 + psxgfx_draw_x);
	cmd[4] = ((uint32_t)v1 << 8) | (uint32_t)u1;
	cmd[5] = ((uint32_t)(y2 + psxgfx_draw_y) << 16) |
		 (uint32_t)(x2 + psxgfx_draw_x);
	cmd[6] = ((uint32_t)v2 << 8) | (uint32_t)u2;
	/* texture page + CLUT in the 7th word */
	cmd[6] |= ((uint32_t)(tpage & 0x1F) << 0);
	cmd[7] = ((uint32_t)(y3 + psxgfx_draw_y) << 16) |
		 (uint32_t)(x3 + psxgfx_draw_x);
	cmd[8] = ((uint32_t)v3 << 8) | (uint32_t)u3;
	cmd[8] |= ((uint32_t)(clut & 0x3F) << 12);

	/* texture page word is combined into UV coordinates in GP0 2Ch */
	/* For simplicity we set it via GP0 E2h (texture window) or */
	/* the texpage is set via the texpage attribute in the cmd[0] */
	(void)tpage; (void)clut;
	gpu_write(cmd, 9);
}

int psxgfx_upload_vram(int x, int y, int w, int h, const void *data)
{
	struct psxgpu_vram_xfer xf;
	xf.x = x;
	xf.y = y;
	xf.w = w;
	xf.h = h;
	xf.data = data;
	return ioctl(gpu_fd, PSXGPU_IOC_UPLOAD_VRAM, &xf);
}

void psxgfx_display_area(int x, int y)
{
	psxgfx_display_x = x;
	psxgfx_display_y = y;
	/* GP1 05h: display start */
	ioctl(gpu_fd, PSXGPU_IOC_GP1, 0x05000000u | ((uint32_t)y << 10) | x);
}

void psxgfx_draw_area(int x0, int y0, int x1, int y1)
{
	/* GP0 E3h: drawing area top-left */
	{
		uint32_t cmd[2];
		cmd[0] = 0xE3000000u | ((uint32_t)y0 << 10) | (uint32_t)x0;
		gpu_write(cmd, 1);
	}
	/* GP0 E4h: drawing area bottom-right */
	{
		uint32_t cmd[2];
		cmd[0] = 0xE4000000u | ((uint32_t)y1 << 10) | (uint32_t)x1;
		gpu_write(cmd, 1);
	}
	/* GP0 EFh: drawing offset (0,0 relative to draw area) */
	{
		uint32_t cmd[2];
		cmd[0] = 0xEF000000u | ((uint32_t)0 << 11) | 0;
		gpu_write(cmd, 1);
	}
}

void psxgfx_vsync(void)
{
	ioctl(gpu_fd, PSXGPU_IOC_VSYNC, 0);
}

void psxgfx_flip(void)
{
	static int which;
	int next_y;

	/* swap draw/display buffers */
	if (which == 0) {
		next_y = 240;
		which = 1;
	} else {
		next_y = 0;
		which = 0;
	}

	psxgfx_display_area(0, which ? 0 : 240);
	psxgfx_draw_y = which ? 240 : 0;
	psxgfx_draw_area(0, psxgfx_draw_y, 320, psxgfx_draw_y + 240);
	(void)next_y;
}

unsigned int psxgfx_status(void)
{
	uint32_t stat = 0;
	ioctl(gpu_fd, PSXGPU_IOC_STAT, &stat);
	return stat;
}

/*
 * ---------------- Sound ----------------
 */

int psxsnd_init(void)
{
	spu_fd = open("/dev/psxspu", O_RDWR);
	if (spu_fd < 0) {
		perror("psxsnd: open /dev/psxspu");
		return -1;
	}

	/* Set master volume */
	psxsnd_master_volume(0x3FFF);

	return 0;
}

void psxsnd_close(void)
{
	if (spu_fd >= 0) {
		close(spu_fd);
		spu_fd = -1;
	}
}

void psxsnd_master_volume(int vol)
{
	ioctl(spu_fd, PSXSPU_IOC_MASTER_VOL, vol);
}

int psxsnd_upload(int halfword_addr, const short *samples, int count)
{
	if (spu_fd < 0)
		return -1;

	/* Set the transfer address */
	ioctl(spu_fd, PSXSPU_IOC_SET_ADDR, halfword_addr);

	/* Write the sample data (16-bit signed PCM) */
	return write(spu_fd, samples, count * 2);
}

void psxsnd_voice(int voice, int vol_l, int vol_r,
		  int pitch, int start_halfwords,
		  int adsr_lo, int adsr_hi)
{
	struct psxspu_voice vs;
	memset(&vs, 0, sizeof(vs));
	vs.voice = voice;
	vs.volume_l = vol_l;
	vs.volume_r = vol_r;
	vs.pitch = pitch;
	vs.start_addr = start_halfwords;	/* in 8-byte units */
	vs.adsr_lo = adsr_lo;
	vs.adsr_hi = adsr_hi;
	ioctl(spu_fd, PSXSPU_IOC_VOICE, &vs);
}

void psxsnd_play(int voice)
{
	ioctl(spu_fd, PSXSPU_IOC_KEY_ON, 1u << voice);
}

void psxsnd_stop(int voice)
{
	ioctl(spu_fd, PSXSPU_IOC_KEY_OFF, 1u << voice);
}

int psxsnd_beep(int voice, int freq_hz, int volume)
{
	/*
	 * Generate a simple square-wave beep.
	 * SPU pitch register: 0x1000 = 44100 Hz (middle C).
	 * So pitch = freq_hz * 0x1000 / 44100.
	 */
	int i;
	short buf[441];		/* 10ms at 44.1kHz */
	int half_period = 44100 / (2 * freq_hz);
	int pitch = (freq_hz * 0x1000) / 44100;

	if (pitch < 0)
		pitch = 0;
	if (pitch > 0x3FFF)
		pitch = 0x3FFF;

	/* Generate square wave */
	for (i = 0; i < 441; i++) {
		buf[i] = (i % (half_period * 2)) < half_period ?
			  (short)(volume) : (short)(-volume);
	}

	/* Upload at halfword address 0x100 (well past the voice regs) */
	if (psxsnd_upload(0x100, buf, 441) < 0)
		return -1;

	/* Simple ADSR: instant attack, sustain, slow release */
	psxsnd_voice(voice, 0x3FFF, 0x3FFF, pitch, 0x100 / 4,
		     0x0000, 0x0000);

	/* Play */
	psxsnd_play(voice);

	return 0;
}
