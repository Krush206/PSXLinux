/*
 * minilibc.c - Minimal C runtime for PSXLinux userspace programs.
 *
 * Memory: a bump allocator over a static pool.  free() reclaims the
 * most recent block (LIFO), which is all the games need to reuse the
 * scratch buffer they stage CD files through.
 *
 * Output: printf/sprintf write through psx_putchar() which talks to the
 * PSX serial port registers directly (the kernel already set the port
 * up), so game messages show up on the console.
 *
 * Math: sin/cos use a 1024-entry table built once with a Taylor series
 * (float arithmetic -> soft-float), so per-frame trig costs a couple of
 * soft-float ops instead of a full series evaluation.
 */

#include "minilibc.h"

/* ---------------- memory ---------------- */

/*
 * Bump allocator with LIFO reclamation.  Every block is preceded by an
 * 8-byte header holding its size, which lets free() rewind the pool when
 * (and only when) the top-most block is released - enough to reuse the
 * scratch buffer used to stage TIM files from the CD.
 */
#define POOL_SIZE (384 * 1024)
#define BLOCK_HEADER 8

static char   mem_pool[POOL_SIZE];
static unsigned int mem_used;

void *malloc(unsigned int size)
{
	unsigned char *base;

	size = (size + 7) & ~7u;
	if (mem_used + size + BLOCK_HEADER > POOL_SIZE)
		return NULL;
	base = (unsigned char *)mem_pool + mem_used;
	*(unsigned int *)base = size;
	mem_used += size + BLOCK_HEADER;
	return base + BLOCK_HEADER;
}

void free(void *p)
{
	unsigned char *base;

	if (p == NULL)
		return;
	base = (unsigned char *)p - BLOCK_HEADER;
	/* Only the most recent allocation can be given back. */
	if (base + BLOCK_HEADER + *(unsigned int *)base ==
	    (unsigned char *)mem_pool + mem_used)
		mem_used = (unsigned int)(base - (unsigned char *)mem_pool);
}

void *calloc(unsigned int n, unsigned int size)
{
	unsigned int total = n * size;
	void *p = malloc(total);
	if (p != NULL)
		memset(p, 0, total);
	return p;
}

void *memset(void *s, int c, unsigned int n)
{
	unsigned char *p = (unsigned char *)s;
	while (n--)
		*p++ = (unsigned char)c;
	return s;
}

void *memcpy(void *d, const void *s, unsigned int n)
{
	unsigned char *dp = (unsigned char *)d;
	const unsigned char *sp = (const unsigned char *)s;
	while (n--)
		*dp++ = *sp++;
	return d;
}

void *memmove(void *d, const void *s, unsigned int n)
{
	unsigned char *dp = (unsigned char *)d;
	const unsigned char *sp = (const unsigned char *)s;

	if (dp == sp || n == 0)
		return d;
	if (dp < sp) {
		while (n--)
			*dp++ = *sp++;
	} else {
		dp += n;
		sp += n;
		while (n--)
			*--dp = *--sp;
	}
	return d;
}

int memcmp(const void *a, const void *b, unsigned int n)
{
	const unsigned char *x = (const unsigned char *)a;
	const unsigned char *y = (const unsigned char *)b;
	while (n--) {
		if (*x != *y)
			return (int)*x - (int)*y;
		x++;
		y++;
	}
	return 0;
}

/* ---------------- strings ---------------- */

int strlen(const char *s)
{
	const char *p = s;
	while (*p)
		p++;
	return (int)(p - s);
}

int strcmp(const char *a, const char *b)
{
	while (*a && *a == *b) {
		a++;
		b++;
	}
	return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

int strncmp(const char *a, const char *b, unsigned int n)
{
	while (n && *a && *a == *b) {
		a++;
		b++;
		n--;
	}
	if (n == 0)
		return 0;
	return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

char *strcpy(char *d, const char *s)
{
	char *r = d;
	while ((*d++ = *s++) != 0)
		;
	return r;
}

char *strncpy(char *d, const char *s, unsigned int n)
{
	char *r = d;
	while (n && *s) {
		*d++ = *s++;
		n--;
	}
	while (n--)
		*d++ = 0;
	return r;
}

char *strcat(char *d, const char *s)
{
	char *r = d;
	while (*d)
		d++;
	while ((*d++ = *s++) != 0)
		;
	return r;
}

char *strstr(const char *hay, const char *needle)
{
	unsigned int n = (unsigned int)strlen(needle);
	if (n == 0)
		return (char *)hay;
	while (*hay) {
		if (*hay == *needle && strncmp(hay, needle, n) == 0)
			return (char *)hay;
		hay++;
	}
	return NULL;
}

char *strchr(const char *s, int c)
{
	while (*s) {
		if (*s == (char)c)
			return (char *)s;
		s++;
	}
	return (c == 0) ? (char *)s : NULL;
}

int atoi(const char *s)
{
	int v = 0;
	int neg = 0;

	while (*s == ' ' || *s == '\t')
		s++;
	if (*s == '-') {
		neg = 1;
		s++;
	} else if (*s == '+') {
		s++;
	}
	while (*s >= '0' && *s <= '9')
		v = v * 10 + (*s++ - '0');
	return neg ? -v : v;
}

/* ---------------- serial output ---------------- */

#define SIO_DATA (*(volatile unsigned char *)0x1F801050)
#define SIO_STAT (*(volatile unsigned short *)0x1F801054)

void psx_putchar(int c)
{
	int i;

	if (c == '\n')
		psx_putchar('\r');
	for (i = 0; i < 100000; i++)
		if (SIO_STAT & 0x0001)
			break;
	SIO_DATA = (unsigned char)c;
}

int puts(const char *s)
{
	while (*s)
		psx_putchar(*s++);
	psx_putchar('\n');
	return 0;
}

/* ---------------- formatted output ---------------- */

static void out_str(char **out, const char *s, int n)
{
	while (n-- > 0 && *s)
		*(*out)++ = *s++;
}

static void out_num(char **out, unsigned int v, int base, int width,
		    int zeropad, int neg, int upper)
{
	char tmp[36];
	int n = 0;
	int digits = 0;

	do {
		int d = (int)(v % (unsigned int)base);
		v /= (unsigned int)base;
		tmp[n++] = (char)(d < 10 ? '0' + d
				  : (upper ? 'A' : 'a') + (d - 10));
	} while (v);

	digits = n + (neg ? 1 : 0);
	if (zeropad) {
		if (neg)
			*(*out)++ = '-';
		while (width-- > digits)
			*(*out)++ = '0';
	} else {
		while (width-- > digits)
			*(*out)++ = ' ';
		if (neg)
			*(*out)++ = '-';
	}
	while (n > 0)
		*(*out)++ = tmp[--n];
}

static void out_double(char **out, double d, int width, int prec)
{
	unsigned int ip;
	double frac;
	int p;
	char tmp[24];
	int n = 0;

	if (prec < 0)
		prec = 6;
	if (d < 0) {
		*(*out)++ = '-';
		d = -d;
	}
	ip = (unsigned int)d;
	frac = d - (double)ip;
	while (width-- > 0)
		;
	out_num(out, ip, 10, 0, 0, 0, 0);
	*(*out)++ = '.';
	for (p = 0; p < prec; p++) {
		frac *= 10.0;
		{
			int digit = (int)frac;
			if (digit < 0)
				digit = 0;
			if (digit > 9)
				digit = 9;
			tmp[n++] = (char)('0' + digit);
			frac -= (double)digit;
		}
	}
	for (p = 0; p < n; p++)
		*(*out)++ = tmp[p];
}

int vsprintf(char *buf, const char *fmt, void *ap)
{
	char *out = buf;
	char *args = (char *)ap;

#define ARG_INT()   (args += 4, *(int *)(args - 4))
#define ARG_UINT()  (args += 4, *(unsigned int *)(args - 4))
#define ARG_PTR()   (args += 4, *(void **)(args - 4))
#define ARG_DBL()   (args += 8, *(double *)(args - 8))

	while (*fmt) {
		int width, zeropad, prec;

		if (*fmt != '%') {
			*out++ = *fmt++;
			continue;
		}
		fmt++;

		width = 0;
		zeropad = 0;
		prec = -1;
		if (*fmt == '0') {
			zeropad = 1;
			fmt++;
		}
		while (*fmt >= '0' && *fmt <= '9')
			width = width * 10 + (*fmt++ - '0');
		if (*fmt == '.') {
			fmt++;
			prec = 0;
			while (*fmt >= '0' && *fmt <= '9')
				prec = prec * 10 + (*fmt++ - '0');
		}
		while (*fmt == 'l' || *fmt == 'h')
			fmt++;

		switch (*fmt) {
		case 'd':
		case 'i': {
			int v = ARG_INT();
			out_num(&out, (unsigned int)(v < 0 ? -v : v), 10,
				width, zeropad, v < 0, 0);
			break;
		}
		case 'u':
			out_num(&out, ARG_UINT(), 10, width, zeropad, 0, 0);
			break;
		case 'x':
			out_num(&out, ARG_UINT(), 16, width, zeropad, 0, 0);
			break;
		case 'X':
			out_num(&out, ARG_UINT(), 16, width, zeropad, 0, 1);
			break;
		case 'p':
			out_str(&out, "0x", 2);
			out_num(&out, (unsigned int)ARG_PTR(), 16, 8, 1, 0, 0);
			break;
		case 'c':
			*out++ = (char)ARG_INT();
			break;
		case 's': {
			char *s = (char *)ARG_PTR();
			int n;
			if (s == NULL)
				s = "(null)";
			n = strlen(s);
			if (prec >= 0 && n > prec)
				n = prec;
			while (width-- > n)
				*out++ = ' ';
			out_str(&out, s, n);
			break;
		}
		case 'f':
			out_double(&out, ARG_DBL(), width, prec);
			break;
		case '%':
			*out++ = '%';
			break;
		case 0:
			fmt--;
			break;
		default:
			*out++ = '%';
			*out++ = *fmt;
			break;
		}
		fmt++;
	}
	*out = 0;
	return (int)(out - buf);

#undef ARG_INT
#undef ARG_UINT
#undef ARG_PTR
#undef ARG_DBL
}

/* Serial output buffer: printf assembles the line, then pushes it out. */
static char printf_buf[512];

int printf(const char *fmt, ...)
{
	int n;

	n = vsprintf(printf_buf, fmt, (void *)&fmt + 4);
	{
		char *p = printf_buf;
		while (*p)
			psx_putchar(*p++);
	}
	return n;
}

int sprintf(char *buf, const char *fmt, ...)
{
	return vsprintf(buf, fmt, (void *)&fmt + 4);
}

/* ---------------- math ---------------- */

#define SIN_TABLE_SIZE 1024
#define SIN_TABLE_QUARTER (SIN_TABLE_SIZE / 4)

static float sin_table[SIN_TABLE_SIZE];
static int sin_table_ready;

static float taylor_sin(float x)
{
	float x2 = x * x;
	float t = x;
	float sum = x;

	/* x - x^3/6 + x^5/120 - x^7/5040 + x^9/362880 - x^11/39916800 */
	t *= x2;
	sum -= t / 6.0f;
	t *= x2;
	sum += t / 120.0f;
	t *= x2;
	sum -= t / 5040.0f;
	t *= x2;
	sum += t / 362880.0f;
	t *= x2;
	sum -= t / 39916800.0f;
	return sum;
}

static void build_sin_table(void)
{
	int i;
	const float two_pi = 6.28318530717958647692f;
	const float quarter = 1.57079632679489661923f;	/* pi/2 */

	if (sin_table_ready)
		return;

	for (i = 0; i < SIN_TABLE_QUARTER; i++) {
		float x = (float)i * (two_pi / (float)SIN_TABLE_SIZE);
		sin_table[i] = taylor_sin(x);
	}
	/* The mirror loop at i == SIN_TABLE_QUARTER reads
	 * sin_table[SIZE/2 - SIZE/4] == sin_table[i] — itself — which
	 * leaves the BSS zero in place: sin(pi/2) returns 0.0 and, via
	 * the half-table mirror, sin(3pi/2) too.  cos(x) = sin(x+pi/2)
	 * then returns 0.0 for x == 0 and pi.  Set the quarter-point
	 * entry explicitly and mirror the rest. */
	sin_table[SIN_TABLE_QUARTER] = 1.0f;
	for (i = SIN_TABLE_QUARTER + 1; i < SIN_TABLE_SIZE / 2; i++)
		sin_table[i] = sin_table[SIN_TABLE_SIZE / 2 - i];
	for (i = SIN_TABLE_SIZE / 2; i < SIN_TABLE_SIZE; i++)
		sin_table[i] = -sin_table[i - SIN_TABLE_SIZE / 2];
	sin_table_ready = 1;
	(void)quarter;
}

float sin(float x)
{
	int idx;
	const float scale = (float)SIN_TABLE_SIZE / 6.28318530717958647692f;

	build_sin_table();
	idx = (int)(x * scale);
	idx &= (SIN_TABLE_SIZE - 1);
	return sin_table[idx];
}

float cos(float x)
{
	return sin(x + 1.5707963f);
}

float tan(float x)
{
	return sin(x) / cos(x);
}

float floor(float x)
{
	int i = (int)x;
	if ((float)i > x)
		i--;
	return (float)i;
}

float fabs(float x)
{
	return x < 0 ? -x : x;
}

/*
 * Newton with a bit-trick seed: the classic exponent-halving guess
 * starts within ~7%, so three iterations land well below 16-bit
 * float precision.  The old double loop burned ~120k emulated
 * cycles per call (24 soft-float double divisions); this one is
 * ~1k.  Called ~10x per frame (gluLookAt, normalises).
 */
float sqrt(float x)
{
	union { float f; unsigned int i; } u;
	float r;
	int i;

	if (x <= 0.0f)
		return 0.0f;
	u.f = x;
	u.i = (u.i >> 1) + 0x1fc00000u;
	r = u.f;
	for (i = 0; i < 3; i++)
		r = 0.5f * (r + x / r);
	return r;
}

float atan2(float y, float x)
{
	/* Not used by the ported games; provided for completeness. */
	float ax = fabs(x);
	float ay = fabs(y);
	float a;

	if (ax + ay == 0.0f)
		return 0.0f;
	a = ay / (ax + ay);
	a = a * (0.7853981633974483f - 0.1963495408493621f * (a * a - 1.0f)
		 - 0.0981747704246810f * (a * a - 1.0f) * (a * a - 1.0f));
	if (x < 0.0f)
		a = 1.5707963267948966f - a;
	if (y < 0.0f)
		a = -a;
	return a;
}

float pow(float x, float y)
{
	int n;
	float r = 1.0f;
	int neg = 0;
	int iy = (int)y;

	if (iy < 0) {
		neg = 1;
		iy = -iy;
	}
	n = iy;
	while (n > 0) {
		if (n & 1)
			r *= x;
		x *= x;
		n >>= 1;
	}
	return neg ? 1.0f / r : r;
}

int abs(int x)
{
	return x < 0 ? -x : x;
}

static unsigned int rand_state = 12345;

void srand(unsigned int seed)
{
	rand_state = seed;
}

int rand(void)
{
	rand_state = rand_state * 1103515245u + 12345u;
	return (int)((rand_state >> 16) & 0x7FFF);
}

void exit(int code)
{
	psx_putchar('\n');
	sys_exit(code);
}

/* ---------------- fixed-point conversion ---------------- */

int psx_f2fix(float f, int frac)
{
	union {
		float f;
		unsigned int u;
	} c;
	int exp, mant, shift, res;

	c.f = f;
	exp = (int)((c.u >> 23) & 0xFF);
	mant = (int)(c.u & 0x7FFFFF) | 0x800000;	/* implicit 1 */
	if (exp == 0)
		return 0;			/* zero or denormal */
	exp -= 127;
	/* value = mant * 2^(exp-23); want value * 2^frac */
	shift = exp - 23 + frac;
	if (shift >= 0) {
		if (shift > 7)			/* would overflow int */
			return (c.u >> 31) ? -0x7FFFFFFF : 0x7FFFFFFF;
		res = mant << shift;
	} else {
		int s = -shift;
		if (s > 30)
			return 0;
		res = (mant + (1 << (s - 1))) >> s;
	}
	return (c.u >> 31) ? -res : res;
}

int psx_ftoi(float f)
{
	return psx_f2fix(f, 0);
}

/* ---------------- timing (root counter 2, system clock / 8) ---------------- */

#define RCNT2_COUNT (*(volatile unsigned short *)0x1F801120)
#define RCNT2_MODE  (*(volatile unsigned short *)0x1F801124)

/* 33.8688 MHz / 8 */
#define TICKS_PER_SEC 4233600u

static unsigned int time_ticks;
static unsigned short time_last;
static int time_ready;

void psx_time_init(void)
{
	RCNT2_MODE = 0x0000;
	RCNT2_COUNT = 0;
	RCNT2_MODE = 0x0200;		/* free run, sysclock / 8 */
	time_last = RCNT2_COUNT;
	time_ticks = 0;
	time_ready = 1;
}

void psx_time_poll(void)
{
	unsigned short now;
	unsigned int delta;

	if (!time_ready)
		psx_time_init();
	now = RCNT2_COUNT;
	delta = (unsigned int)(unsigned short)(now - time_last);
	time_last = now;
	time_ticks += delta;
}

unsigned int psx_time_ticks(void)
{
	psx_time_poll();
	return time_ticks;
}

unsigned int psx_time_ms(void)
{
	unsigned int t = psx_time_ticks();
	/* ticks * 1000 / 4233600, avoiding 32-bit overflow */
	return (t / 42336u) * 10u + ((t % 42336u) * 10u) / 42336u;
}
