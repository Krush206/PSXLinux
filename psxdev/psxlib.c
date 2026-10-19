/*
 * psxlib.c - Runtime support library for PSXLinux userspace games.
 *
 * Implements the CD-ROM, SPU sound and graphics helper modules
 * declared in psxlib.h.  The core GPU routines live in libps_compat.c.
 *
 * Hardware reference: https://psx-spx.github.io/
 */

#include "psxlib.h"

/* Provided by minilibc (psxdev/minilibc.c). */
extern void *malloc(unsigned int size);
extern void  free(void *p);

/*
 * =====================================================================
 *  CD-ROM
 * =====================================================================
 */

#define SEEK_SET_	0
#define SEEK_END_	2

static sint cd_lseek(sint fd, sint off, sint whence)
{
	return sys_call3(SYS_lseek, fd, off, whence);
}

sint psx_cd_open(const char *name)
{
	return sys_open(name, 0 /* O_RDONLY */);
}

sint psx_cd_read(sint fd, void *buf, sint n)
{
	return sys_read(fd, buf, n);
}

sint psx_cd_seek(sint fd, sint off)
{
	return cd_lseek(fd, off, SEEK_SET_);
}

sint psx_cd_size(sint fd)
{
	/* Use fstat to get file size - avoids lseek(SEEK_END) bug on ISO9660 */
	struct {
		unsigned int	st_dev;
		unsigned int	st_ino;
		unsigned int	st_mode;
		unsigned int	st_nlink;
		unsigned int	st_uid;
		unsigned int	st_gid;
		unsigned int	st_rdev;
		long		st_size;
		unsigned int	st_atime, st_res1;
		unsigned int	st_mtime, st_res2;
		unsigned int	st_ctime, st_res3;
		unsigned int	st_blksize;
		int		st_blocks;
		unsigned int	st_unused0[2];
	} st;
	
	if (sys_fstat(fd, &st) < 0)
		return -1;
	return (sint)st.st_size;
}

void psx_cd_close(sint fd)
{
	if (fd >= 0)
		sys_close(fd);
}

sint psx_cd_exists(const char *name)
{
	sint fd = psx_cd_open(name);
	if (fd < 0)
		return 0;
	sys_close(fd);
	return 1;
}

sint psx_cd_load_into(const char *name, void *buf, sint bufsize)
{
	sint fd, size, total, got;

	fd = psx_cd_open(name);
	if (fd < 0)
		return -1;

	size = psx_cd_size(fd);
	if (size > bufsize) {
		psx_cd_close(fd);
		return -1;
	}
	if (size < 0)
		size = bufsize;

	psx_cd_seek(fd, 0);

	total = 0;
	while (total < size) {
		got = sys_read(fd, (char *)buf + total, size - total);
		if (got <= 0)
			break;
		total += got;
	}
	psx_cd_close(fd);
	return total;
}

void *psx_cd_load(const char *name, sint *size)
{
	sint fd, len, total, got;
	void *buf;

	fd = psx_cd_open(name);
	if (fd < 0) {
		if (size)
			*size = 0;
		return 0;
	}

	len = psx_cd_size(fd);
	if (len <= 0) {
		psx_cd_close(fd);
		if (size)
			*size = 0;
		return 0;
	}

	buf = malloc((unsigned int)len);
	if (!buf) {
		psx_cd_close(fd);
		if (size)
			*size = 0;
		return 0;
	}

	psx_cd_seek(fd, 0);
	total = 0;
	while (total < len) {
		got = sys_read(fd, (char *)buf + total, len - total);
		if (got <= 0)
			break;
		total += got;
	}
	psx_cd_close(fd);

	if (size)
		*size = total;
	return buf;
}

/*
 * =====================================================================
 *  Sound (SPU)
 * =====================================================================
 *
 * SPU register block: 0x1F801C00..0x1F801DFF (uncached KSEG1).
 */

#define SPU_BASE	0x1F801C00u
#define SPU_VOICE(n)	(SPU_BASE + (n) * 16u)

#define SPU_MASTER_L	0x1F801D80u
#define SPU_MASTER_R	0x1F801D82u
#define SPU_KEYON_L	0x1F801D88u
#define SPU_KEYON_H	0x1F801D8Au
#define SPU_KEYOFF_L	0x1F801D8Cu
#define SPU_KEYOFF_H	0x1F801D8Eu
#define SPU_RAM_ADDR	0x1F801DA6u
#define SPU_RAM_DATA	0x1F801DA8u
#define SPU_CTRL	0x1F801DAAu
#define SPU_STAT	0x1F801DAEu

/* SPUCNT bits */
#define SPUCNT_ENABLE	0xC000u		/* bit15 enable + bit14 unmute */
#define SPUCNT_WRITE	0x0010u		/* transfer mode 1 = manual write */

static void spu_w(sint addr, u_short v)
{
	*(volatile u_short *)addr = v;
}

static u_short spu_r(sint addr)
{
	return *(volatile u_short *)addr;
}

/* Next free halfword address for tone samples (8-byte units). */
static sint snd_next_addr = 0x100;
/* Per-voice SPU RAM address of a tone started by psx_snd_play_tone(). */
static sint snd_tone_addr[PSX_SND_VOICES];

volatile u_long psx_snd_tick;

void psx_snd_init(void)
{
	sint i;

	/* Disable everything while we reconfigure. */
	spu_w(SPU_CTRL, 0x0000);
	for (i = 0; i < PSX_SND_VOICES; i++) {
		spu_w(SPU_VOICE(i) + 0x00, 0);
		spu_w(SPU_VOICE(i) + 0x02, 0);
		spu_w(SPU_VOICE(i) + 0x04, 0x1000);
		spu_w(SPU_VOICE(i) + 0x06, 0);
		spu_w(SPU_VOICE(i) + 0x08, 0);
		spu_w(SPU_VOICE(i) + 0x0A, 0);
		snd_tone_addr[i] = -1;
	}
	spu_w(SPU_KEYOFF_L, 0xFFFF);
	spu_w(SPU_KEYOFF_H, 0x00FF);

	/* Enable + unmute, full master volume. */
	spu_w(SPU_MASTER_L, PSX_SND_MAX_VOL);
	spu_w(SPU_MASTER_R, PSX_SND_MAX_VOL);
	spu_w(SPU_CTRL, SPUCNT_ENABLE);

	snd_next_addr = 0x100;
	psx_snd_tick = 0;
}

void psx_snd_master(sint left, sint right)
{
	spu_w(SPU_MASTER_L, (u_short)left);
	spu_w(SPU_MASTER_R, (u_short)right);
}

void psx_snd_key_on(u_long mask)
{
	spu_w(SPU_KEYON_L, (u_short)(mask & 0xFFFF));
	spu_w(SPU_KEYON_H, (u_short)((mask >> 16) & 0xFF));
}

void psx_snd_key_off(u_long mask)
{
	spu_w(SPU_KEYOFF_L, (u_short)(mask & 0xFFFF));
	spu_w(SPU_KEYOFF_H, (u_short)((mask >> 16) & 0xFF));
}

void psx_snd_stop_all(void)
{
	sint i;

	spu_w(SPU_KEYOFF_L, 0xFFFF);
	spu_w(SPU_KEYOFF_H, 0x00FF);
	for (i = 0; i < PSX_SND_VOICES; i++) {
		spu_w(SPU_VOICE(i) + 0x00, 0);
		spu_w(SPU_VOICE(i) + 0x02, 0);
		snd_tone_addr[i] = -1;
	}
}

void psx_snd_upload(sint ram_addr, const void *data, sint nbytes)
{
	const u_short *p = (const u_short *)data;
	sint halfwords = (nbytes + 1) / 2;
	sint i;

	/* Address register is in 8-byte (4 halfword) units. */
	spu_w(SPU_RAM_ADDR, (u_short)ram_addr);

	/* Manual write mode (bit4 set) while keeping enable + unmute. */
	spu_w(SPU_CTRL, SPUCNT_ENABLE | SPUCNT_WRITE);

	for (i = 0; i < halfwords; i++)
		spu_w(SPU_RAM_DATA, p[i]);

	spu_w(SPU_CTRL, SPUCNT_ENABLE);
}

void psx_snd_volume(sint voice, sint left, sint right)
{
	spu_w(SPU_VOICE(voice) + 0x00, (u_short)left);
	spu_w(SPU_VOICE(voice) + 0x02, (u_short)right);
}

void psx_snd_pitch(sint voice, sint pitch)
{
	spu_w(SPU_VOICE(voice) + 0x04, (u_short)pitch);
}

void psx_snd_adsr(sint voice, sint adsr_lo, sint adsr_hi)
{
	spu_w(SPU_VOICE(voice) + 0x08, (u_short)adsr_lo);
	spu_w(SPU_VOICE(voice) + 0x0A, (u_short)adsr_hi);
}

void psx_snd_play(sint voice, sint ram_addr, sint pitch,
		  sint vol_left, sint vol_right)
{
	if (voice < 0 || voice >= PSX_SND_VOICES)
		return;

	spu_w(SPU_VOICE(voice) + 0x00, (u_short)vol_left);
	spu_w(SPU_VOICE(voice) + 0x02, (u_short)vol_right);
	spu_w(SPU_VOICE(voice) + 0x04, (u_short)pitch);
	spu_w(SPU_VOICE(voice) + 0x06, (u_short)ram_addr);

	/* Sustain forever: attack rate, then sustain level. */
	spu_w(SPU_VOICE(voice) + 0x08, 0x00FF);
	spu_w(SPU_VOICE(voice) + 0x0A, 0x0000);

	psx_snd_key_on(1u << voice);
}

void psx_snd_stop(sint voice)
{
	if (voice < 0 || voice >= PSX_SND_VOICES)
		return;
	psx_snd_key_off(1u << voice);
	spu_w(SPU_VOICE(voice) + 0x00, 0);
	spu_w(SPU_VOICE(voice) + 0x02, 0);
}

/*
 * A looping sample of TONE_SAMPLES samples describes one full cycle of
 * a square wave when played at pitch TONE_PITCH.  For any other pitch
 * the output frequency is  freq = 44100 * pitch / (4096 * TONE_SAMPLES).
 * Solving for pitch:  pitch = freq * 4096 * TONE_SAMPLES / 44100.
 */
#define TONE_SAMPLES	64
#define TONE_AMPLITUDE	6000

sint psx_snd_play_tone(sint voice, sint freq_hz)
{
	short buf[TONE_SAMPLES];
	sint half = TONE_SAMPLES / 2;
	sint i;
	sint addr, pitch;

	if (voice < 0 || voice >= PSX_SND_VOICES)
		return -1;

	for (i = 0; i < TONE_SAMPLES; i++)
		buf[i] = (i < half) ? TONE_AMPLITUDE : -TONE_AMPLITUDE;

	addr = snd_next_addr;
	psx_snd_upload(addr, buf, sizeof(buf));
	snd_next_addr += (sizeof(buf) + 7) / 8;
	if (snd_next_addr >= PSX_SND_RAM_SIZE * 128)
		snd_next_addr = 0x100;

	pitch = (freq_hz * TONE_SAMPLES * PSX_SND_PITCH_UNIT) / 44100;
	if (pitch > 0x3FFF)
		pitch = 0x3FFF;
	if (pitch < 1)
		pitch = 1;

	snd_tone_addr[voice] = addr;
	psx_snd_play(voice, addr, pitch, 0x2000, 0x2000);
	return addr;
}

void psx_snd_stop_tone(sint voice)
{
	psx_snd_stop(voice);
}

/*
 * =====================================================================
 *  Graphics helpers
 * =====================================================================
 */

/* Provided by libps_compat.c. */
extern void GsSendGP0(u_long cmd);
extern void GsSendGP1(u_long cmd);
extern void GsSetDrawArea(long x, long y, long w, long h);
extern void GsSetDrawOffset(long x, long y);

void psx_gfx_init(void)
{
	ResetGraph(0);
	SetVideoMode(MODE_NTSC);
	GsInitGraph(320, 240, GsOFSGPU, 0, 0);
	GsDefDispBuff(0, 0, 0, 240);
	GsSetDrawArea(0, 0, 320, 240);
	GsSetDrawOffset(0, 0);
}

void psx_gfx_clear(u_char r, u_char g, u_char b)
{
	/* Widen the drawing area so both framebuffers are covered, fill
	 * the whole 320x480 region, then restore the game's draw area. */
	GsSendGP0(0xE3000000);			/* drawing area top-left = 0,0 */
	GsSendGP0(0xE4000000 | (479 << 10) | 319);
	GsSendGP0(0xE5000000);			/* drawing offset = 0,0 */

	GsSendGP0(0x60000000 | ((u_long)r << 0) |
		  ((u_long)g << 8) | ((u_long)b << 16));
	GsSendGP0(0x00000000);			/* y0=0, x0=0 */
	GsSendGP0((479 << 16) | 319);		/* y1=479, x1=319 */

	GsSetDrawArea(0, 0, 320, 240);
	GsSetDrawOffset(0, 0);
}

sint psx_gfx_upload_tim(u_long *tim)
{
	GsIMAGE img;

	if (!tim || (tim[0] & 0xFF) != 0x10)
		return -1;

	GsGetTimInfo(tim, &img);

	/* Upload the pixel data. */
	LoadImage(img.rect, img.pixel);

	/* Upload the CLUT, if the image is colour-indexed. */
	if (img.crect)
		LoadImage(img.crect, img.clut);

	return 0;
}

sint psx_gfx_load_tim(const char *name)
{
	void *data;
	sint size;

	data = psx_cd_load(name, &size);
	if (!data)
		return -1;

	(void)size;
	if ((*(u_long *)data & 0xFF) != 0x10) {
		free(data);
		return -1;
	}

	psx_gfx_upload_tim((u_long *)data);
	free(data);
	return 0;
}

void psx_gfx_swap(void)
{
	DrawSync(0);
	VSync(0);
	GsSwapDispBuff();
}
