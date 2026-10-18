/*
 * demo.c - PSXLinux game development demo (bare metal, no libc).
 *
 * Uses raw Linux syscalls (psxsys.h) and the kernel GPU/SPU device nodes
 * defined in psxdev.h.
 */

#include "psxsys.h"
#include "psxdev.h"

sint main(void);

/* GCC 2.91 MIPS calls __main() before main() to run global ctors */
void __main(void) { }

__attribute__((section(".text.start")))
void _start(void)
{
	sint ret;

	/* Direct SIO write to prove we're running (bypasses syscalls) */
	{
		volatile unsigned short *stat = (volatile unsigned short *)0x1F801054;
		volatile unsigned char *data = (volatile unsigned char *)0x1F801050;
		int i;
		for (i = 0; i < 1000; i++) {
			if (*stat & 0x0001)
				break;
		}
		*data = 'D';
		for (i = 0; i < 1000; i++) {
			if (*stat & 0x0001)
				break;
		}
		*data = '!';
		for (i = 0; i < 1000; i++) {
			if (*stat & 0x0001)
				break;
		}
		*data = '\n';
	}

	ret = main();
	sys_exit(ret);
	for (;;) ;  /* unreachable */
}

/*
 * Tiny printf - supports %d, %x, %s
 */
sint psx_printf(const char *fmt, ...)
{
	char buf[512];
	int idx = 0;
	const char *p = fmt;
	const sint *args = (const sint *)&fmt + 1;

	while (*p && idx < 500) {
		if (*p == '%') {
			p++;
			switch (*p) {
			case 'd': {
				sint v = *args++;
				char t[12];
				int n = 0;
				if (v < 0) { buf[idx++] = '-'; v = -v; }
				if (v == 0) { buf[idx++] = '0'; }
				while (v > 0) { t[n++] = '0' + (v % 10); v /= 10; }
				while (n > 0) buf[idx++] = t[--n];
				break;
			}
			case 'x': {
				sint v = *args++;
				int i;
				for (i = 28; i >= 0; i -= 4) {
					sint d = (v >> i) & 0xF;
					buf[idx++] = d < 10 ? '0' + d : 'A' + d - 10;
				}
				break;
			}
			case 's': {
				const char *s = (const char *)*args++;
				while (*s && idx < 500) buf[idx++] = *s++;
				break;
			}
			}
			p++;
		} else {
			buf[idx++] = *p++;
		}
	}
	buf[idx] = 0;

	return sys_write(1, buf, idx);
}

/*
 * ---- File descriptors ----
 */
static int gpu_fd = -1;
static int spu_fd = -1;

/*
 * ---- Graphics ----
 */

int psxgfx_init(void)
{
	gpu_fd = sys_open("/dev/psxgpu", 2);
	if (gpu_fd < 0)
		return -1;

	sys_ioctl(gpu_fd, PSXGPU_IOC_RESET, 0);
	sys_ioctl(gpu_fd, PSXGPU_IOC_GP1, 0x03000000);  /* display on */
	sys_ioctl(gpu_fd, PSXGPU_IOC_GP1, 0x08000000);  /* NTSC 15bpp */
	sys_ioctl(gpu_fd, PSXGPU_IOC_GP1, 0x05000000);  /* display (0,0) */
	sys_ioctl(gpu_fd, PSXGPU_IOC_GP1, 0x0602C0C60); /* horiz range */
	sys_ioctl(gpu_fd, PSXGPU_IOC_GP1, 0x07000010); /* vert range */
	return 0;
}

void psxgfx_close(void)
{
	if (gpu_fd >= 0) {
		sys_close(gpu_fd);
		gpu_fd = -1;
	}
}

static void gpu_write(const uint32_t *words, int count)
{
	if (gpu_fd >= 0)
		sys_write(gpu_fd, words, count * 4);
}

void psxgfx_clear(int r, int g, int b)
{
	uint32_t cmd[4];
	cmd[0] = 0x02000000u;
	cmd[1] = ((uint32_t)r << 0) | ((uint32_t)g << 8) | ((uint32_t)b << 16);
	cmd[2] = 0;
	cmd[3] = (240u << 16) | 320u;
	gpu_write(cmd, 4);
}

void psxgfx_rect(int x, int y, int w, int h, int r, int g, int b)
{
	uint32_t cmd[3];
	cmd[0] = 0x60000000u |
		 ((uint32_t)r << 0) | ((uint32_t)g << 8) | ((uint32_t)b << 16);
	cmd[1] = ((uint32_t)y << 16) | (uint32_t)x;
	cmd[2] = ((uint32_t)h << 16) | (uint32_t)w;
	gpu_write(cmd, 3);
}

void psxgfx_triangle_gouraud(int x0, int y0, int r0, int g0, int b0,
			     int x1, int y1, int r1, int g1, int b1,
			     int x2, int y2, int r2, int g2, int b2)
{
	uint32_t cmd[6];
	cmd[0] = 0x30000000u |
		 ((uint32_t)r0 << 0) | ((uint32_t)g0 << 8) | ((uint32_t)b0 << 16);
	cmd[1] = ((uint32_t)y0 << 16) | (uint32_t)x0;
	cmd[2] = ((uint32_t)r1 << 0) | ((uint32_t)g1 << 8) | ((uint32_t)b1 << 16);
	cmd[3] = ((uint32_t)y1 << 16) | (uint32_t)x1;
	cmd[4] = ((uint32_t)r2 << 0) | ((uint32_t)g2 << 8) | ((uint32_t)b2 << 16);
	cmd[5] = ((uint32_t)y2 << 16) | (uint32_t)x2;
	gpu_write(cmd, 6);
}

void psxgfx_vsync(void)
{
	sys_ioctl(gpu_fd, PSXGPU_IOC_VSYNC, 0);
}

uint32_t psxgfx_status(void)
{
	uint32_t stat = 0;
	sys_ioctl(gpu_fd, PSXGPU_IOC_STAT, (sint)&stat);
	return stat;
}

/*
 * ---- Sound ----
 */

int psxsnd_init(void)
{
	spu_fd = sys_open("/dev/psxspu", 2);
	if (spu_fd < 0)
		return -1;
	sys_ioctl(spu_fd, PSXSPU_IOC_MASTER_VOL, 0x3FFF);
	return 0;
}

void psxsnd_close(void)
{
	if (spu_fd >= 0) {
		sys_close(spu_fd);
		spu_fd = -1;
	}
}

void psxsnd_play(int voice)
{
	sys_ioctl(spu_fd, PSXSPU_IOC_KEY_ON, 1u << voice);
}

void psxsnd_stop(int voice)
{
	sys_ioctl(spu_fd, PSXSPU_IOC_KEY_OFF, 1u << voice);
}

int psxsnd_beep(int voice, int freq_hz, int volume)
{
	short buf[441];
	int i;
	int half_period;
	int pitch;
	struct psxspu_voice vs;

	half_period = 44100 / (2 * freq_hz);
	if (half_period < 1) half_period = 1;
	pitch = (freq_hz * 0x1000) / 44100;
	if (pitch < 0) pitch = 0;
	if (pitch > 0x3FFF) pitch = 0x3FFF;

	for (i = 0; i < 441; i++)
		buf[i] = (i % (half_period * 2)) < half_period ?
			  (short)volume : (short)-volume;

	/* Upload sample data to sound RAM at halfword offset 0x100 */
	sys_ioctl(spu_fd, PSXSPU_IOC_SET_ADDR, 0x100);
	sys_write(spu_fd, buf, sizeof(buf));

	/* Configure voice */
	vs.voice = voice;
	vs.volume_l = 0x3FFF;
	vs.volume_r = 0x3FFF;
	vs.pitch = pitch;
	vs.start_addr = 0x100 / 4;  /* sound RAM address in 8-byte units */
	vs.adsr_lo = 0x000F;  /* attack level/mode: sustain mode, no decay */
	vs.adsr_hi = 0x0000;
	sys_ioctl(spu_fd, PSXSPU_IOC_VOICE, (sint)&vs);

	psxsnd_play(voice);
	return 0;
}

/*
 * ---- Main ----
 */

sint main(void)
{
	int frame;

	psx_printf("PSX Game Dev Demo\n");

	if (psxgfx_init() < 0) {
		psx_printf("GPU init FAILED\n");
		sys_exit(1);
	}
	psx_printf("GPU OK\n");

	if (psxsnd_init() < 0) {
		psx_printf("SPU FAILED\n");
	} else {
		psx_printf("SPU OK\n");
		psxsnd_beep(0, 440, 8000);
		psx_printf("Beep 440Hz\n");
	}

	for (frame = 0; frame < 600; frame++) {
		int x = (frame * 2) % 280;
		int y = frame % 200;

		psxgfx_clear(0, 0, 32);
		psxgfx_rect(x, y, 40, 40, 255, 0, 0);
		psxgfx_rect(320 - x - 30, 240 - y - 30, 30, 30, 0, 255, 0);
		psxgfx_triangle_gouraud(
			50, 50, (frame * 4) & 0xFF, 0, 0,
			250, 50, 0, (frame * 8) & 0xFF, 0,
			150, 200, 0, 0, 255 - ((frame * 2) & 0xFF));

		psxgfx_vsync();

		if (frame % 60 == 0) {
			psx_printf("frame %d stat=%x\n",
				   frame, psxgfx_status());
			if (frame % 120 == 0)
				psxsnd_beep(0, 440 + (frame / 60) * 55, 8000);
		}
	}

	psxsnd_stop(0);
	psxsnd_close();
	psxgfx_close();
	psx_printf("Done.\n");
	return 0;
}
