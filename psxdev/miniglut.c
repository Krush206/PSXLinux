/*
 * miniglut.c — GLUT shim for PSXLinux.
 *
 * Window creation becomes GPU display setup (minigl), the main loop
 * drives the registered display/idle callbacks with digital-pad
 * input, and "swap buffers" presents the finished ordering table.
 *
 * Pad protocol (SIO0 0x1F801040..4F), matching this emulator's sio.c
 * exactly:  CTRL=0x1003 (select pad 1, TX enable)  →  send 01h
 * (access, reply FFh)  →  42h (reply = pad ID, 41h digital / 73h
 * analog)  →  00h (reply 5Ah sync)  →  00h (reply buttons byte A)
 * →  00h (reply buttons byte B)  →  CTRL=0x1010 (deselect).  A
 * poll that does not see the 5Ah sync byte reports "no buttons",
 * never garbage — else a desynced poll reads zeroes and a resting
 * pad looks like "everything pressed" (phantom ESC/pause/steering).
 *
 * IMPORTANT: every transfer's ACK raises I_STAT bit 7 (controller
 * interrupt).  The kernel has no handler registered for it, so we
 * mask that bit in I_MASK and clear any pending instance before the
 * first poll — otherwise the kernel would panic with
 * "do_IRQ: unregistered IRQ".
 *
 * Button bits (response word, 0 = pressed):
 *   byte0 (low):  L2 R2 L1 R1 TRI CIR CRO SQU
 *   byte1 (high): SEL  i  j  STA  UP RIGHT DOWN LEFT
 * matching rs_race2's libs/pad.h masks.
 */

#include "miniglut.h"
#include "psxsys.h"     /* sys_call3, SYS_gettimeofday */

/* ---------------------------------------------------------------- clock */
/*
 * The game advances its world by a fixed 17 ms per loop, which ties
 * the world speed to the frame rate: at ~16 fps the arena runs at
 * ~27% speed.  Advance the GLUT clock with the real wall time
 * instead (kernel gettimeofday), clamped so a hitch cannot make the
 * simulation jump.
 */
static unsigned real_ms(void)
{
	struct { int sec, usec; } tv;          /* struct timeval */

	sys_call3(SYS_gettimeofday, (sint)&tv, 0, 0);
	return (unsigned)tv.sec * 1000u + (unsigned)tv.usec / 1000u;
}
#include "psxsys.h"     /* sys_call3, SYS_gettimeofday */

typedef unsigned int   u32;
typedef unsigned short u16;
typedef unsigned char  u8;

/* ---------------------------------------------------------------- timing */
/* Per-phase profiling: milliseconds from the kernel clock, summed
 * over PROF_EVERY frames and printed on the serial console.  (The
 * cp0 COUNT register reads as a constant from user mode in this
 * emulator, so the syscall clock it is.) */
static u32 rcount(void)
{
	return real_ms();
}

#define PROF_EVERY 32
static u32 acc_idle, acc_draw, acc_swap;   /* COUNT ticks per phase */
extern int printf(const char *, ...);
extern unsigned int glStatPackets(void);
extern void glTakeCounters(unsigned *, unsigned *, unsigned *);
extern void glProfTake(unsigned *);

/* SIO0 — the controller port.  The data register is byte-wide: a
 * 16-bit access is split by the hardware (and the emulator) into
 * two 8-bit transfers, so the trailing 0x00 high byte would abort
 * every pad transaction. */
#define JOY_DATA  (*(volatile u8 *)0x1F801040)
#define JOY_STAT  (*(volatile u16 *)0x1F801044)
#define JOY_MODE  (*(volatile u16 *)0x1F801048)
#define JOY_CTRL  (*(volatile u16 *)0x1F80104A)
#define JOY_BAUD  (*(volatile u16 *)0x1F80104E)

/* interrupt controller */
#define I_STAT    (*(volatile u32 *)0x1F801070)
#define I_MASK    (*(volatile u32 *)0x1F801074)

#define TX_RDY   0x0001
#define RX_RDY   0x0002
#define IRQ_CTRL 0x0080             /* I_STAT bit 7: controller (ACK) */

/* button masks (pad.h conventions; word bit set = pressed) */
#define BTN_L2      0x0001
#define BTN_R2      0x0002
#define BTN_L1      0x0004
#define BTN_R1      0x0008
#define BTN_TRI     0x0010
#define BTN_CIR     0x0020
#define BTN_CRO     0x0040
#define BTN_SQU     0x0080
#define BTN_SEL     0x0100
#define BTN_STA     0x0800
#define BTN_UP      0x1000
#define BTN_RIGHT   0x2000
#define BTN_DOWN    0x4000
#define BTN_LEFT    0x8000

/* from minigl.c */
extern int  glMiniInit(void);
extern void glSwapBuffersPSX(void);

/* ------------------------------------------------------------ callbacks */

static void (*cb_display)(void);
static void (*cb_idle)(void);
static void (*cb_keyboard)(unsigned char key, int x, int y);
static void (*cb_special)(int key, int x, int y);

static u32  elapsed_ms;
static int  pad_inited;
static u16  pad_buttons;             /* bit set = pressed */
static u16  pad_prev;
static u32  frame_count;

/* ---------------------------------------------------------------- init */

void glutInit(int *argcp, char **argv)     { (void)argcp; (void)argv; }
void glutInitDisplayMode(unsigned int mode) { (void)mode; }
void glutInitWindowPosition(int x, int y)   { (void)x; (void)y; }
void glutInitWindowSize(int width, int height) { (void)width; (void)height; }

int glutCreateWindow(const char *title)
{
	(void)title;
	glMiniInit();
	return 1;
}

void glutDestroyWindow(int win) { (void)win; }

void glutGameModeString(const char *string) { (void)string; }

int glutEnterGameMode(void)
{
	glMiniInit();
	return 1;
}

int glutGameModeGet(int mode)
{
	switch (mode) {
	case GLUT_GAME_MODE_ACTIVE:   return 1;
	case GLUT_GAME_MODE_POSSIBLE: return 1;
	case GLUT_GAME_MODE_WIDTH:    return 320;
	case GLUT_GAME_MODE_HEIGHT:   return 240;
	case GLUT_GAME_MODE_PIXEL_DEPTH:    return 15;
	case GLUT_GAME_MODE_REFRESH_RATE:  return 60;
	case GLUT_GAME_MODE_DISPLAY_CHANGED: return 0;
	}
	return 0;
}

/* ------------------------------------------------------------ callbacks */

void glutDisplayFunc(void (*func)(void))  { cb_display = func; }
void glutIdleFunc(void (*func)(void))    { cb_idle = func; }
void glutKeyboardFunc(void (*func)(unsigned char, int, int))
{
	cb_keyboard = func;
}

void glutSpecialFunc(void (*func)(int, int, int))
{
	cb_special = func;
}

/* ------------------------------------------------------------- pad poll */

static void pad_init_once(void)
{
	if (pad_inited)
		return;
	pad_inited = 1;

	/* The kernel has no handler for the controller (pad-ACK)
	 * interrupt; mask it and clear any pending instance before
	 * the first poll, or the kernel would panic. */
	I_MASK &= ~IRQ_CTRL;
	I_STAT = ~IRQ_CTRL;

	JOY_MODE = 0x000D;             /* 8N1, factor 1 */
	JOY_BAUD = 0x0088;             /* 250 kHz */
	JOY_CTRL = 0x0000;             /* reset */
}

static u8 joy_xmit(u8 val)
{
	int i;

	for (i = 0; i < 1000 && !(JOY_STAT & TX_RDY); i++)
		;
	JOY_DATA = val;
	for (i = 0; i < 10000 && !(JOY_STAT & RX_RDY); i++)
		;
	return (u8)JOY_DATA;
}

static void pad_poll(void)
{
	u8 id, sync, a, b;

	pad_init_once();

	JOY_CTRL = 0x1003;             /* select pad 1, TX enable */
	(void)joy_xmit(0x01);          /* access   -> 0xFF */
	id   = joy_xmit(0x42);          /* read     -> pad ID */
	sync = joy_xmit(0x00);          /*          -> 0x5A */
	a    = joy_xmit(0x00);          /*          -> SEL i j STA UP RIGHT DOWN LEFT */
	b    = joy_xmit(0x00);          /*          -> L2 R2 L1 R1 TRI CIR CRO SQU */
	JOY_CTRL = 0x1010;             /* deselect + clear error/ACK */
	I_STAT = ~IRQ_CTRL;            /* drop the pending ACK */

	pad_prev = pad_buttons;
	if (sync != 0x5A || id != 0x41)    /* no pad / desynced / not digital pad */
		pad_buttons = 0;               /* (memcard replies can interleave on SIO) */
	else
		pad_buttons = (u16)~(((u16)a << 8) | (u16)b);  /* 1 = pressed */
}

/* --------------------------------------------------------- key dispatch */

static void send_char(unsigned char key)
{
	if (cb_keyboard)
		cb_keyboard(key, 0, 0);
}

static void send_special(int key)
{
	if (cb_special)
		cb_special(key, 0, 0);
}

static void dispatch_key(u16 mask, unsigned char ch, int special)
{
	int now  = pad_buttons & mask;
	int prev = pad_prev & mask;

	/* fire on press; auto-repeat while held every ~8 frames */
	if (now && (!prev || (frame_count & 7) == 0)) {
		if (special)
			send_special((int)ch);
		else
			send_char(ch);
	}
}

static void dispatch_keys(void)
{
	dispatch_key(BTN_LEFT,   'a', 0);        /* steer player 0 left */
	dispatch_key(BTN_RIGHT,  's', 0);        /* steer player 0 right */
	dispatch_key(BTN_CRO,    ' ', 0);        /* pause */
	dispatch_key(BTN_STA,    27,  0);        /* back / quit to gui */
	dispatch_key(BTN_UP,     GLUT_KEY_UP,    1);
	dispatch_key(BTN_DOWN,   GLUT_KEY_DOWN,  1);
	/* NOTE: triangle/circle/square deliberately not bound — an
	 * idle physical joystick in the emulator can report them
	 * held, and F1/F2/F3 would switch the display to 2- or
	 * 4-view mode at startup, quadrupling the render load. */
}

/* ----------------------------------------------------------------- loop */

void glutMainLoop(void)
{
	static unsigned last_real;

	for (;;) {
		unsigned now = real_ms();
		unsigned d;

		if (last_real == 0 || now == last_real)
			d = 17;                        /* first loop / same ms */
		else {
			d = now - last_real;
			if (d > 100)
				d = 100;               /* clamp long hitches */
		}
		last_real = now;
		elapsed_ms += d;

		pad_poll();
		dispatch_keys();
		frame_count++;

		{
			u32 t0 = rcount();

			if (cb_idle)
				cb_idle();
			acc_idle += rcount() - t0;
			t0 = rcount();
			if (cb_display)
				cb_display();
			acc_draw += rcount() - t0;
			acc_draw -= 0;   /* swap time is booked by glutSwapBuffers */
		}

		if ((frame_count % PROF_EVERY) == 0) {
			u32 tot = acc_idle + acc_draw + acc_swap;
			u32 other = 0;

			/* subtract the swap share booked inside cb_display */
			if (acc_swap <= acc_draw)
				acc_draw -= acc_swap;
			else {
				other = acc_swap - acc_draw;
				acc_draw = 0;
			}
			(void)other;
			{
				unsigned v, pr, sc, ph[7];

				glTakeCounters(&v, &pr, &sc);
				glProfTake(ph);
				printf("prof: idle=%u draw=%u swap=%u pkts=%u fr=%u vt=%u pr=%u sc=%u\n",
				       acc_idle, acc_draw, acc_swap,
				       glStatPackets(), frame_count,
				       v, pr, sc);
				printf("ph: setup=%u floor=%u walls=%u trails=%u bikes=%u glow=%u hud=%u (us)\n",
				       ph[0], ph[1], ph[2], ph[3], ph[4],
				       ph[5], ph[6]);
			}
			acc_idle = acc_draw = acc_swap = 0;
		}
	}
}

void glutPostRedisplay(void)
{
	/* the main loop redraws every pass anyway */
}

void glutSwapBuffers(void)
{
	u32 t0 = rcount();

	glSwapBuffersPSX();
	acc_swap += rcount() - t0;
}

void glutWarpPointer(int x, int y)
{
	(void)x; (void)y;
}

int glutGet(int state)
{
	switch (state) {
	case GLUT_ELAPSED_TIME:  return (int)elapsed_ms;
	case GLUT_SCREEN_WIDTH:  return 320;
	case GLUT_SCREEN_HEIGHT: return 240;
	}
	return 0;
}

/* ------------------------------------------------------------ font stubs */

void glutBitmapCharacter(void *font, int character)
{
	(void)font; (void)character;   /* text is drawn as textured quads */
}

/* --------------------------------------- sphere generators (unused but
 * linked by the commented-out crash highlight) */

void glutSolidSphere(GLdouble radius, GLint slices, GLint stacks)
{
	(void)radius; (void)slices; (void)stacks;
}

void glutWireSphere(GLdouble radius, GLint slices, GLint stacks)
{
	(void)radius; (void)slices; (void)stacks;
}
