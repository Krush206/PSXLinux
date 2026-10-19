/*
 * psxlib.h - Runtime support library for PSXLinux userspace games.
 *
 * A single, self-contained library covering the three subsystems every
 * PlayStation game needs:
 *
 *   1. Graphics  - the Gs-compatible GPU layer (see libps_compat.h),
 *                  plus helpers for screen setup, TIM upload and
 *                  back-buffer management.
 *   2. Sound     - the SPU (24 ADPCM/PCM voices, master volume, sample
 *                  upload).
 *   3. CD-ROM    - reading files from the boot CD through the Linux
 *                  VFS (the kernel mounts the ISO9660 volume as root).
 *
 * Everything talks to the hardware directly (or through raw MIPS
 * syscalls) because PSXLinux runs without an MMU and the games are
 * linked with -nostdlib against minilibc.
 *
 * Hardware reference: https://psx-spx.github.io/
 */

#ifndef _PSXLIB_H
#define _PSXLIB_H

#include "psxsys.h"
#include "libps_compat.h"

/*
 * =====================================================================
 *  CD-ROM
 * =====================================================================
 *
 * The kernel's psxcd block driver is mounted as the root filesystem,
 * so ordinary open()/read()/lseek()/close() syscalls reach files on
 * the disc.  These wrappers add the small amount of book-keeping the
 * games want (files are usually loaded whole into RAM).
 */

#define PSX_CD_PATH_MAX		64

/* Open a file on the CD. Returns a file descriptor, or -1. */
sint   psx_cd_open(const char *name);
/* Read up to n bytes. Returns bytes read, 0 at EOF, or -1 on error. */
sint   psx_cd_read(sint fd, void *buf, sint n);
/* Seek to an absolute byte offset. Returns the new position, or -1. */
sint   psx_cd_seek(sint fd, sint off);
/* Size of the file in bytes, or -1. Leaves the position unchanged. */
sint   psx_cd_size(sint fd);
/* Close a descriptor opened with psx_cd_open(). */
void   psx_cd_close(sint fd);
/* Non-zero if the file exists and can be opened for reading. */
sint   psx_cd_exists(const char *name);
/*
 * Load an entire file into a freshly malloc()'d buffer.  If size is
 * non-NULL it receives the file length.  Returns NULL on failure.
 * The caller owns the buffer (free() it when done).
 */
void  *psx_cd_load(const char *name, sint *size);
/*
 * Load an entire file into a caller-supplied buffer.  Returns the
 * number of bytes read, or -1 if the file does not fit.
 */
sint   psx_cd_load_into(const char *name, void *buf, sint bufsize);

/*
 * =====================================================================
 *  Sound (SPU)
 * =====================================================================
 *
 * Volume registers are 15-bit (0..0x3FFF).  Pitch is 12-bit
 * (0x1000 = 44100 Hz).  Sound RAM addresses are in 8-byte units.
 */

#define PSX_SND_VOICES		24
#define PSX_SND_RAM_SIZE	512	/* KB */
#define PSX_SND_MAX_VOL		0x3FFF
#define PSX_SND_PITCH_UNIT	0x1000	/* 44100 Hz */

/* Reset the SPU, silence all voices, set full master volume. */
void   psx_snd_init(void);
/* Master volume, 0..PSX_SND_MAX_VOL. */
void   psx_snd_master(sint left, sint right);
/* Key on/off by 24-bit voice mask. */
void   psx_snd_key_on(u_long mask);
void   psx_snd_key_off(u_long mask);
/* Stop every voice (key off + zero volume). */
void   psx_snd_stop_all(void);

/*
 * Upload raw 16-bit signed PCM to SPU RAM.  ram_addr is in 8-byte
 * units, data is nbytes long (rounded up to a multiple of 8).
 */
void   psx_snd_upload(sint ram_addr, const void *data, sint nbytes);

/* Per-voice parameters. */
void   psx_snd_volume(sint voice, sint left, sint right);
void   psx_snd_pitch(sint voice, sint pitch);
void   psx_snd_adsr(sint voice, sint adsr_lo, sint adsr_hi);
/* Play a sample already in SPU RAM at ram_addr (8-byte units). */
void   psx_snd_play(sint voice, sint ram_addr, sint pitch,
		    sint vol_left, sint vol_right);
void   psx_snd_stop(sint voice);

/*
 * Synthesise and play a simple looping square wave at freq Hz.  This
 * is the quickest way to produce a tone without shipping sample data.
 * Returns the SPU RAM address used (8-byte units).
 */
sint   psx_snd_play_tone(sint voice, sint freq_hz);
/* Release a tone started with psx_snd_play_tone(). */
void   psx_snd_stop_tone(sint voice);

/* Simple completion counter - incremented for convenience by games. */
extern volatile u_long psx_snd_tick;

/*
 * =====================================================================
 *  Graphics helpers
 * =====================================================================
 *
 * The core GPU API is in libps_compat.h (GsInitGraph, GsDefDispBuff,
 * GsSortSprite, GsSortObject4, ...).  These helpers cover the parts
 * that are tedious to repeat in every game.
 */

/* One-shot setup: soft reset, video mode, 320x240 15bpp, double buffer. */
void   psx_gfx_init(void);
/* Clear both VRAM buffers to a colour (also resets the draw area). */
void   psx_gfx_clear(u_char r, u_char g, u_char b);
/* Upload a TIM already resident in RAM to VRAM.  Returns 0 on success. */
sint   psx_gfx_upload_tim(u_long *tim);
/* Load a TIM from the CD and upload it; returns 0 on success. */
sint   psx_gfx_load_tim(const char *name);
/* Wait for the current frame and flip the display/draw buffers. */
void   psx_gfx_swap(void);

#endif /* _PSXLIB_H */
