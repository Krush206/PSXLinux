/*
 * psxgame.c - In-kernel GPU/SPU hardware test.
 *
 * Runs after all initcalls as a late_initcall. Directly exercises the
 * GPU (drawing rectangles, triangles) and SPU (playing a tone) to prove
 * the device drivers are functional. This is the kernel-space analog
 * of the userspace demo in psxdev/demo.c.
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <asm/io.h>

#include <linux/psxgpu.h>
#include <linux/psxspu.h>

/* GPU registers */
#define GP0	0x1F801810u
#define GP1	0x1F801814u

/* SPU registers */
#define SPU_CTRL	0x1F801DAAu
#define SPU_MASTER_L	0x1F801D80u
#define SPU_MASTER_R	0x1F801D82u
#define SPU_KEYON_L	0x1F801D88u
#define SPU_KEYOFF_L	0x1F801D8Cu
#define SPU_RAM_ADDR	0x1F801DA6u
#define SPU_RAM_DATA	0x1F801DA8u
#define SPU_VOICE(n)	(0x1F801C00u + (n) * 16u)

static void gp0_wait(void)
{
	int i;
	for (i = 0; i < 100000; i++)
		if (readl(GP1) & (1u << 28))
			return;
}

static void gpu_cmd(uint32_t v)
{
	gp0_wait();
	writel(v, GP0);
}

static void gpu_init_display(void)
{
	printk(KERN_INFO "psxgame: initializing GPU display\n");

	/* soft reset */
	writel(0x00000000u, GP1);

	/* display enable */
	writel(0x03000000u, GP1);

	/* NTSC 15bpp */
	writel(0x08000000u, GP1);

	/* display area at (0,0) */
	writel(0x05000000u, GP1);

	/* horizontal range: standard NTSC */
	writel(0x0602C0C60u, GP1);

	/* vertical range: standard NTSC */
	writel(0x07000010u, GP1);

	/* drawing area: (0,0) to (320,240) */
	gpu_cmd(0xE3000000u);          /* top-left (0,0) */
	gpu_cmd(0xE4000000u | (240u << 10) | 320u);  /* bottom-right */
	gpu_cmd(0xE5000000u);          /* drawing offset (0,0) */
}

static void gpu_clear(int r, int g, int b)
{
	gpu_cmd(0x02000000u);
	gpu_cmd(((uint32_t)r << 0) | ((uint32_t)g << 8) | ((uint32_t)b << 16));
	gpu_cmd(0);  /* x=0, y=0 */
	gpu_cmd((240u << 16) | 320u);  /* w=320, h=240 */
}

static void gpu_rect(int x, int y, int w, int h, int r, int g, int b)
{
	gpu_cmd(0x60000000u |
		 ((uint32_t)r << 0) | ((uint32_t)g << 8) | ((uint32_t)b << 16));
	gpu_cmd(((uint32_t)y << 16) | (uint32_t)x);
	gpu_cmd(((uint32_t)h << 16) | (uint32_t)w);
}

static void gpu_triangle(int x0, int y0, int r0, int g0, int b0,
			 int x1, int y1, int r1, int g1, int b1,
			 int x2, int y2, int r2, int g2, int b2)
{
	gpu_cmd(0x30000000u |
		 ((uint32_t)r0 << 0) | ((uint32_t)g0 << 8) | ((uint32_t)b0 << 16));
	gpu_cmd(((uint32_t)y0 << 16) | (uint32_t)x0);
	gpu_cmd(((uint32_t)r1 << 0) | ((uint32_t)g1 << 8) | ((uint32_t)b1 << 16));
	gpu_cmd(((uint32_t)y1 << 16) | (uint32_t)x1);
	gpu_cmd(((uint32_t)r2 << 0) | ((uint32_t)g2 << 8) | ((uint32_t)b2 << 16));
	gpu_cmd(((uint32_t)y2 << 16) | (uint32_t)x2);
}

/*
 * Play a simple 440 Hz square-wave beep on SPU voice 0.
 */
static void spu_test(void)
{
	short buf[441];  /* 10ms at 44.1kHz */
	int i;
	int freq = 440;
	int half_period = 44100 / (2 * freq);
	int pitch = (freq * 0x1000) / 44100;

	printk(KERN_INFO "psxgame: SPU test: 440 Hz beep\n");

	/* generate square wave */
	for (i = 0; i < 441; i++)
		buf[i] = (i % (half_period * 2)) < half_period ? 8000 : -8000;

	/* silence all voices */
	for (i = 0; i < 24; i++) {
		writew(0, SPU_VOICE(i) + 0x00);
		writew(0, SPU_VOICE(i) + 0x02);
	}
	writew(0xFFFF, SPU_KEYOFF_L);
	writel(0, 0x1F801D8Eu);

	/* set master volume */
	writew(0x3FFF, SPU_MASTER_L);
	writew(0x3FFF, SPU_MASTER_R);

	/* enable SPU, unmute */
	writew(0xC000, SPU_CTRL);  /* bit 15 = enable, bit 14 = unmute */

	/* upload sample to sound RAM at halfword offset 0x100 */
	writew(0x0100, SPU_RAM_ADDR);
	writew(0x0005, SPU_CTRL);  /* direction = write to sound RAM */
	for (i = 0; i < 441; i++)
		writew((unsigned short)buf[i], SPU_RAM_DATA);

	/* re-enable with transfer disabled */
	writew(0xC000, SPU_CTRL);

	/* configure voice 0 */
	writew(0x3FFF, SPU_VOICE(0) + 0x00);  /* volume left */
	writew(0x3FFF, SPU_VOICE(0) + 0x02);  /* volume right */
	writew((unsigned short)pitch, SPU_VOICE(0) + 0x04); /* pitch */
	writew(0x100 / 4, SPU_VOICE(0) + 0x06); /* start addr (8-byte units) */
	writew(0x000F, SPU_VOICE(0) + 0x08); /* ADSR lo: sustain */
	writew(0x0000, SPU_VOICE(0) + 0x0A); /* ADSR hi: sustain */

	/* key on voice 0 */
	writew(0x0001, SPU_KEYON_L);

	printk(KERN_INFO "psxgame: SPU voice 0 started\n");
}

/*
 * Main test: draw a colorful scene on the GPU.
 */
static int __init psxgame_run(void)
{
	int frame;

	printk(KERN_INFO "psxgame: starting GPU/SPU demo\n");

	gpu_init_display();

	/* Play the beep */
	spu_test();

	/* Draw a few frames */
	for (frame = 0; frame < 30; frame++) {
		gpu_clear(0, 0, 32);  /* dark blue background */

		/* red rectangle */
		gpu_rect(50 + frame, 50, 60, 60, 255, 0, 0);

		/* green rectangle */
		gpu_rect(250 - frame, 150, 40, 40, 0, 255, 0);

		/* blue rectangle */
		gpu_rect(150, 100 + frame, 30, 30, 0, 0, 255);

		/* gouraud triangle */
		gpu_triangle(160, 30, 255, 0, 0,
			     60, 200, 0, 255, 0,
			     260, 200, 0, 0, 255);
	}

	printk(KERN_INFO "psxgame: drew 30 frames\n");
	printk(KERN_INFO "psxgame: GPUSTAT = %08x\n", readl(GP1));
	printk(KERN_INFO "psxgame: done\n");
	return 0;
}

__initcall(psxgame_run);
