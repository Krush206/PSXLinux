/*
 * cargame.c - Racing car game for PSXLinux.
 *
 * A top-down / pseudo-3D racing game rendered entirely with the PSX GPU
 * hardware.  The player steers a car left/right along a scrolling road,
 * avoiding oncoming traffic and collecting checkpoints.
 *
 * All rendering is done via direct GP0 command-port writes (memory-mapped
 * hardware at 0x1F801810/0x1F801814).  No libc required.
 *
 * GPU reference: https://psx-spx.github.io/#gpu-gp0-commands
 *
 * Controls: uses the PSX controller via the SIO pad registers.
 *           Left/Right = steer, Cross = accelerate, Square = brake.
 *           Falls back to auto-accelerate if no controller detected.
 */

#include "psxsys.h"
#include "psxdev.h"

/* ---- Entry point ---- */

sint main(void);

/* GCC 2.91 MIPS calls __main() before main() */
void __main(void) { }

__attribute__((section(".text.start")))
void _start(void)
{
	sint ret;
	ret = main();
	sys_exit(ret);
	for (;;) ;
}

/* ---- GPU registers (memory-mapped) ---- */

#define GP0_REG		(*(volatile unsigned int  *)0x1F801810)
#define GP1_REG		(*(volatile unsigned int  *)0x1F801814)

#define GPUSTAT_FIFO_READY	(1u << 28)
#define GPUSTAT_VBLANK		(1u << 31)

/* ---- Screen ---- */

#define SCREEN_W	320
#define SCREEN_H	240
#define HORIZON		80		/* y coordinate of the horizon */

/* ---- PSX Controller (SIO pad) ---- */
/* Pad registers at 0x1F801040 (SIO0) */

#define PAD_DATA	(*(volatile unsigned short *)0x1F801044)
#define PAD_MODE	(*(volatile unsigned short *)0x1F801048)
#define PAD_CTRL	(*(volatile unsigned short *)0x1F80104A)
#define PAD_BAUD	(*(volatile unsigned short *)0x1F80104E)

/* Pad button bits (active low) */
#define PAD_LEFT	(1u << 15)
#define PAD_RIGHT	(1u << 13)
#define PAD_UP		(1u << 12)
#define PAD_DOWN	(1u << 14)
#define PAD_CROSS	(1u <<  7)
#define PAD_SQUARE	(1u <<  5)
#define PAD_START	(1u <<  3)

/* ---- Game constants ---- */

#define ROAD_HALF_W		80	/* half-width of road at bottom */
#define ROAD_HALF_HORIZON	8	/* half-width at horizon (perspective) */
#define CAR_W			24
#define CAR_H			36
#define NPC_COUNT		4
#define OBSTACLE_COUNT		3

#define MAX_SPEED		20
#define ACCEL			1
#define BRAKE			2
#define STEER_SPEED		4

#define ROAD_COLOR_R		64
#define ROAD_COLOR_G		64
#define ROAD_COLOR_B		72

#define GRASS_COLOR_R		16
#define GRASS_COLOR_G		80
#define GRASS_COLOR_B		24

#define CENTER_LINE_R		255
#define CENTER_LINE_G		255
#define CENTER_LINE_B		255

/* ---- Game state ---- */

static int player_x;		/* player car x position (center of road = 0) */
static int player_speed;		/* forward speed */
static int distance;		/* total distance travelled */
static int frame_count;
static int player_alive;
static int score;

static int npc_x[NPC_COUNT];	/* NPC car lateral positions */
static int npc_z[NPC_COUNT];	/* NPC car depth (0 = at player, 1000 = far) */
static int npc_speed[NPC_COUNT];

static int obs_x[OBSTACLE_COUNT];
static int obs_z[OBSTACLE_COUNT];
static int obs_type[OBSTACLE_COUNT];	/* 0 = cone, 1 = oil, 2 = barrier */

static unsigned int rng_seed;

/* ---- GPU helpers ---- */

static void gpu_wait_fifo(void)
{
	while (!(GP1_REG & GPUSTAT_FIFO_READY))
		;
}

static void gpu_send(unsigned int word)
{
	gpu_wait_fifo();
	GP0_REG = word;
}

static void gpu_init(void)
{
	GP1_REG = 0x00000000;		/* soft reset */
	GP1_REG = 0x03000000;		/* display enable */
	GP1_REG = 0x08000000;		/* NTSC 15bpp */
	GP1_REG = 0x05000000;		/* display at (0,0) */
	GP1_REG = 0x0602C0C60;		/* horizontal range */
	GP1_REG = 0x07000010;		/* vertical range */

	gpu_send(0xE3000000);		/* draw area top-left (0,0) */
	gpu_send(0xE4000000 | (239u << 10) | 319u);  /* bottom-right */
	gpu_send(0xE5000000);		/* drawing offset (0,0) */
	gpu_send(0xE6000000);		/* mask/dither off */
}

static void gpu_clear(int r, int g, int b)
{
	gpu_send(0x02000000);
	gpu_send(((unsigned int)r << 0) |
		 ((unsigned int)g << 8) |
		 ((unsigned int)b << 16));
	gpu_send(0);
	gpu_send((SCREEN_H << 16) | SCREEN_W);
}

static void gpu_rect(int x, int y, int w, int h, int r, int g, int b)
{
	if (x < 0) { w += x; x = 0; }
	if (y < 0) { h += y; y = 0; }
	if (x + w > SCREEN_W) w = SCREEN_W - x;
	if (y + h > SCREEN_H) h = SCREEN_H - y;
	if (w <= 0 || h <= 0) return;

	gpu_send(0x60000000 |
		 ((unsigned int)r << 0) |
		 ((unsigned int)g << 8) |
		 ((unsigned int)b << 16));
	gpu_send(((unsigned int)y << 16) | (unsigned int)x);
	gpu_send(((unsigned int)h << 16) | (unsigned int)w);
}

static void gpu_tri_flat(int x0, int y0, int x1, int y1,
			 int x2, int y2, int r, int g, int b)
{
	gpu_send(0x20000000 |
		 ((unsigned int)r << 0) |
		 ((unsigned int)g << 8) |
		 ((unsigned int)b << 16));
	gpu_send(((unsigned int)y0 << 16) | (unsigned int)x0);
	gpu_send(((unsigned int)y1 << 16) | (unsigned int)x1);
	gpu_send(((unsigned int)y2 << 16) | (unsigned int)x2);
}

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

static void gpu_wait_vblank(void)
{
	while (!(GP1_REG & GPUSTAT_VBLANK))
		;
	while (GP1_REG & GPUSTAT_VBLANK)
		;
}

/* ---- Simple PRNG ---- */

static unsigned int rand_next(void)
{
	rng_seed = rng_seed * 1103515245 + 12345;
	return (rng_seed >> 16) & 0x7FFF;
}

/* ---- Controller input ---- */

static unsigned short pad_buttons;

static void pad_read(void)
{
	int i;
	unsigned short buttons = 0xFFFF;

	/* Select pad 1, enable ACK */
	PAD_CTRL = 0x1003;

	/* Send read command */
	PAD_DATA = 0x01;

	/* Small delay for SIO */
	for (i = 0; i < 100; i++)
		;

	/* Read button state (16-bit, active low) */
	buttons = PAD_DATA;

	/* Deselect */
	PAD_CTRL = 0x0000;

	pad_buttons = buttons;
}

static int pad_held(unsigned short mask)
{
	/* Active low: bit cleared = button pressed */
	return !(pad_buttons & mask);
}

/* ---- Perspective road rendering ---- */

/*
 * Returns the half-width of the road at a given screen depth (0=near,
 * HORIZON=at the horizon line).  Linear interpolation creates a
 * pseudo-3D perspective effect.
 */
static int road_half_width(int y)
{
	if (y <= HORIZON)
		return ROAD_HALF_HORIZON;

	{
		int t = (y - HORIZON) * 100 / (SCREEN_H - HORIZON);
		return ROAD_HALF_HORIZON +
		       (ROAD_HALF_W - ROAD_HALF_HORIZON) * t / 100;
	}
}

/*
 * Convert a lateral road position (in units of road half-width,
 * -100 to +100) to a screen x coordinate at a given depth y.
 */
static int road_to_screen_x(int lateral, int y)
{
	int hw = road_half_width(y);
	return SCREEN_W / 2 + (lateral * hw) / 100;
}

/* ---- Game rendering ---- */

static void render_sky(void)
{
	/* Sky gradient from dark blue at top to lighter near horizon */
	gpu_rect(0, 0, SCREEN_W, HORIZON, 32, 32, 128);
	gpu_rect(0, HORIZON - 20, SCREEN_W, 10, 48, 48, 160);
	gpu_rect(0, HORIZON - 10, SCREEN_W, 5, 64, 64, 192);
	gpu_rect(0, HORIZON - 5, SCREEN_W, 5, 96, 96, 208);
}

static void render_road(void)
{
	int y;
	int scroll_off = (distance / 4) % 40; /* center line scroll */

	/* Grass on both sides */
	gpu_rect(0, HORIZON, SCREEN_W, SCREEN_H - HORIZON,
		 GRASS_COLOR_R, GRASS_COLOR_G, GRASS_COLOR_B);

	/* Road surface — drawn as horizontal strips for perspective */
	for (y = HORIZON; y < SCREEN_H; y += 4) {
		int hw = road_half_width(y);
		int cx = SCREEN_W / 2;

		/* Road strip */
		gpu_rect(cx - hw, y, hw * 2, 4,
			 ROAD_COLOR_R, ROAD_COLOR_G, ROAD_COLOR_B);

		/* Center line (dashed, scrolls with distance) */
		if (((y + scroll_off) % 40) < 20 && hw > 4) {
			gpu_rect(cx - 2, y, 4, 4,
				 CENTER_LINE_R, CENTER_LINE_G, CENTER_LINE_B);
		}

		/* Road edge lines (white) */
		if (hw > 6) {
			gpu_rect(cx - hw, y, 3, 4, 255, 255, 255);
			gpu_rect(cx + hw - 3, y, 3, 4, 255, 255, 255);
		}
	}
}

/*
 * Draw a car sprite at a given road position and depth.
 * lateral: -100 (far left) to +100 (far right)
 * depth:   0 (at player, bottom of screen) to 1000 (at horizon)
 */
static void render_car(int lateral, int depth, int is_player)
{
	int y;
	int x;
	int w;
	int h;

	if (depth > 1000 || depth < 0)
		return;

	/* Map depth to screen y (perspective) */
	if (depth >= 1000) {
		y = HORIZON;
		w = 2;
		h = 3;
	} else {
		int t = 1000 - depth;
		y = HORIZON + t * (SCREEN_H - HORIZON) / 1000;
		w = (CAR_W * t) / 1000;
		h = (CAR_H * t) / 1000;
	}

	if (w < 2) w = 2;
	if (h < 2) h = 2;

	x = road_to_screen_x(lateral, y);

	/* Car body */
	if (is_player) {
		gpu_rect(x - w / 2, y, w, h, 255, 0, 0);
		/* Windshield */
		gpu_rect(x - w / 3, y + h / 4, (w * 2) / 3, h / 3,
			 100, 200, 255);
		/* Wheels */
		gpu_rect(x - w / 2 - 1, y, 2, h / 4, 32, 32, 32);
		gpu_rect(x + w / 2 - 1, y, 2, h / 4, 32, 32, 32);
	} else {
		/* NPC cars: alternate colors */
		int r = (depth % 3 == 0) ? 255 : 0;
		int g = (depth % 3 == 1) ? 255 : 0;
		int b = (depth % 3 == 2) ? 255 : 128;
		gpu_rect(x - w / 2, y, w, h, r, g, b);
		gpu_rect(x - w / 3, y + h / 4, (w * 2) / 3, h / 3,
			 200, 200, 200);
	}
}

/*
 * Draw an obstacle at a given road position and depth.
 */
static void render_obstacle(int lateral, int depth, int type)
{
	int y;
	int x;
	int w;
	int h;

	if (depth > 1000 || depth < 0)
		return;

	if (depth >= 1000) {
		y = HORIZON;
		w = 2;
		h = 2;
	} else {
		int t = 1000 - depth;
		y = HORIZON + t * (SCREEN_H - HORIZON) / 1000;
		w = (16 * t) / 1000;
		h = (16 * t) / 1000;
	}

	if (w < 2) w = 2;
	if (h < 2) h = 2;

	x = road_to_screen_x(lateral, y);

	switch (type) {
	case 0:	/* Traffic cone: orange triangle */
		gpu_tri_flat(x, y + h, x - w / 2, y + h, x + w / 2, y + h,
			     255, 128, 0);
		gpu_tri_flat(x, y, x - w / 4, y + h / 2, x + w / 4, y + h / 2,
			     255, 96, 0);
		break;
	case 1:	/* Oil slick: dark ellipse approximation */
		gpu_rect(x - w / 2, y, w, h / 2, 20, 20, 20);
		break;
	case 2:	/* Barrier: yellow/black stripes */
		gpu_rect(x - w / 2, y, w, h, 255, 200, 0);
		gpu_rect(x - w / 4, y, w / 2, h, 32, 32, 32);
		break;
	}
}

static void render_hud(void)
{
	/* Score bar at top */
	gpu_rect(0, 0, SCREEN_W, 12, 0, 0, 0);

	/* Speed indicator (gouraud bar) */
	{
		int bar_w = player_speed * 8;
		int i;
		for (i = 0; i < bar_w; i += 4) {
			int r = 255 * i / (MAX_SPEED * 8);
			int b = 255 - r;
			gpu_rect(4 + i, 2, 4, 8, r, 128, b);
		}
	}

	/* Score ticks (one per 100 distance) */
	{
		int i;
		int ticks = score / 100;
		if (ticks > 40) ticks = 40;
		for (i = 0; i < ticks; i++)
			gpu_rect(SCREEN_W - 10 - i * 6, 4, 4, 4, 255, 255, 0);
	}
}

/* ---- Game logic ---- */

static void spawn_npc(int i)
{
	if (npc_z[i] <= 0) {
		npc_z[i] = 800 + (rand_next() % 200);
		npc_x[i] = (rand_next() % 140) - 70;
		npc_speed[i] = 4 + (rand_next() % 6);
	}
}

static void spawn_obstacle(int i)
{
	if (obs_z[i] <= 0) {
		obs_z[i] = 600 + (rand_next() % 400);
		obs_x[i] = (rand_next() % 160) - 80;
		obs_type[i] = rand_next() % 3;
	}
}

static void check_collisions(void)
{
	int i;

	/* Check NPC collisions */
	for (i = 0; i < NPC_COUNT; i++) {
		if (npc_z[i] < 50 && npc_z[i] > 0) {
			int dx = npc_x[i] - player_x;
			if (dx < 0) dx = -dx;
			if (dx < 25) {
				player_alive = 0;
				return;
			}
		}
	}

	/* Check obstacle collisions */
	for (i = 0; i < OBSTACLE_COUNT; i++) {
		if (obs_z[i] < 50 && obs_z[i] > 0) {
			int dx = obs_x[i] - player_x;
			if (dx < 0) dx = -dx;
			if (dx < 18) {
				player_alive = 0;
				return;
			}
		}
	}
}

static void update_game(void)
{
	int i;

	/* Read controller */
	pad_read();

	/* Steering */
	if (pad_held(PAD_LEFT))
		player_x -= STEER_SPEED;
	if (pad_held(PAD_RIGHT))
		player_x += STEER_SPEED;

	/* Acceleration */
	if (pad_held(PAD_CROSS)) {
		player_speed += ACCEL;
	} else if (pad_held(PAD_SQUARE)) {
		player_speed -= BRAKE;
	} else {
		/* Friction */
		if (player_speed > 0)
			player_speed--;
	}

	/* Clamp speed */
	if (player_speed < 0) player_speed = 0;
	if (player_speed > MAX_SPEED) player_speed = MAX_SPEED;

	/* Clamp position to road */
	if (player_x < -90) player_x = -90;
	if (player_x > 90) player_x = 90;

	/* Move forward */
	distance += player_speed;
	score += player_speed;

	/* Update NPCs (they move toward the player) */
	for (i = 0; i < NPC_COUNT; i++) {
		npc_z[i] -= player_speed - npc_speed[i] / 2;
		if (npc_z[i] < -50)
			npc_z[i] = 0;
		spawn_npc(i);
	}

	/* Update obstacles */
	for (i = 0; i < OBSTACLE_COUNT; i++) {
		obs_z[i] -= player_speed;
		if (obs_z[i] < -50)
			obs_z[i] = 0;
		spawn_obstacle(i);
	}

	/* Check for collision */
	check_collisions();
}

static void render_frame(void)
{
	int i;

	/* Draw the sky */
	render_sky();

	/* Draw the road */
	render_road();

	/* Draw obstacles (far to near for correct layering) */
	for (i = OBSTACLE_COUNT - 1; i >= 0; i--)
		render_obstacle(obs_x[i], obs_z[i], obs_type[i]);

	/* Draw NPC cars (far to near) */
	for (i = NPC_COUNT - 1; i >= 0; i--)
		render_car(npc_x[i], npc_z[i], 0);

	/* Draw player car (always at fixed depth) */
	render_car(player_x, 100, 1);

	/* Draw HUD */
	render_hud();

	/* If dead, draw explosion */
	if (!player_alive) {
		int cx = road_to_screen_x(player_x, SCREEN_H - 50);
		int cy = SCREEN_H - 50;
		int j;
		for (j = 0; j < 12; j++) {
			int a = j * 30;
			int r = 255 - j * 10;
			int g = j * 20;
			gpu_tri_flat(cx, cy,
				     cx + (j * 8) - 48, cy - 20 + a / 4,
				     cx + (j * 8) - 32, cy + 20,
				     r > 0 ? r : 0, g > 255 ? 255 : g, 0);
		}
		/* Game over text: big red rectangles */
		gpu_rect(SCREEN_W/2 - 60, SCREEN_H/2 - 20, 120, 40, 0, 0, 0);
		gpu_rect(SCREEN_W/2 - 56, SCREEN_H/2 - 16, 112, 32, 255, 0, 0);
		gpu_rect(SCREEN_W/2 - 52, SCREEN_H/2 - 12, 104, 24, 0, 0, 0);
	}
}

/* ---- Main game loop ---- */

sint main(void)
{
	int i;

	/* Initialize */
	gpu_init();
	rng_seed = 12345;
	player_x = 0;
	player_speed = 0;
	distance = 0;
	score = 0;
	player_alive = 1;
	frame_count = 0;

	/* Initialize NPCs and obstacles far away */
	for (i = 0; i < NPC_COUNT; i++) {
		npc_x[i] = 0;
		npc_z[i] = 0;
		npc_speed[i] = 5;
		spawn_npc(i);
	}
	for (i = 0; i < OBSTACLE_COUNT; i++) {
		obs_x[i] = 0;
		obs_z[i] = 0;
		spawn_obstacle(i);
	}

	/* Title screen */
	gpu_clear(0, 0, 32);
	gpu_rect(SCREEN_W/2 - 80, 60, 160, 30, 255, 200, 0);
	gpu_rect(SCREEN_W/2 - 76, 64, 152, 22, 0, 0, 64);
	gpu_rect(SCREEN_W/2 - 60, 100, 120, 20, 0, 200, 0);
	gpu_rect(SCREEN_W/2 - 56, 104, 112, 12, 0, 60, 0);

	/* Flash "PRESS START" */
	for (i = 0; i < 120; i++) {
		if (i % 30 < 20)
			gpu_rect(SCREEN_W/2 - 50, 140, 100, 14, 255, 255, 255);
		else
			gpu_rect(SCREEN_W/2 - 50, 140, 100, 14, 32, 32, 64);
		gpu_wait_vblank();
	}

	/* Main game loop */
	while (player_alive && frame_count < 3600) {
		update_game();
		render_frame();
		gpu_wait_vblank();
		frame_count++;
	}

	/* Death/timeout: show score for 2 seconds */
	for (i = 0; i < 120; i++) {
		if (!player_alive)
			render_frame();  /* keep showing explosion */
		else
			gpu_clear(0, 0, 0);
		gpu_wait_vblank();
	}

	/* Fade out */
	for (i = 255; i >= 0; i -= 5) {
		gpu_clear(i, i, i);
		gpu_wait_vblank();
	}

	return score;
}
