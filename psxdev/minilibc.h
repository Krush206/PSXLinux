/*
 * minilibc.h - Minimal C runtime for PSXLinux userspace programs.
 *
 * The mipsel-linux cross compiler has no target libc, so this provides
 * the handful of functions the ported games need: memory allocation
 * from a static pool, string ops, a small printf that writes to the
 * PSX serial port, basic math, and timing helpers.
 *
 * Floating point: everything is compiled with -msoft-float and linked
 * against libsoftfp.a (built from GCC's fp-bit.c/dp-bit.c) because the
 * PSX R3000A has no FPU.
 */

#ifndef _PSXMINILIBC_H
#define _PSXMINILIBC_H

#include "psxsys.h"

#ifndef NULL
#define NULL ((void *)0)
#endif

/* ---- memory ---- */

void  *malloc(unsigned int size);
void   free(void *p);
void  *calloc(unsigned int n, unsigned int size);
void  *memset(void *s, int c, unsigned int n);
void  *memcpy(void *d, const void *s, unsigned int n);
void  *memmove(void *d, const void *s, unsigned int n);
int    memcmp(const void *a, const void *b, unsigned int n);

/* ---- strings ---- */

int    strlen(const char *s);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, unsigned int n);
char  *strcpy(char *d, const char *s);
char  *strncpy(char *d, const char *s, unsigned int n);
char  *strcat(char *d, const char *s);
char  *strstr(const char *hay, const char *needle);
char  *strchr(const char *s, int c);
int    atoi(const char *s);

/* ---- formatted output (writes to the PSX serial port) ---- */

int    printf(const char *fmt, ...);
int    sprintf(char *buf, const char *fmt, ...);
int    vsprintf(char *buf, const char *fmt, void *ap);
void   psx_putchar(int c);
int    puts(const char *s);

/* ---- math (all single precision: the R3000A soft-floats doubles
 *      at ~10x the cost of floats, and nothing here needs more) ---- */

float sin(float x);
float cos(float x);
float tan(float x);
float sqrt(float x);
float fabs(float x);
float floor(float x);
float atan2(float y, float x);
float pow(float x, float y);
int    abs(int x);
int    rand(void);
void   srand(unsigned int seed);
void   exit(int code);

/* ---- fixed-point helpers (no FPU, no soft-float) ---- */

/* Convert a float to 16.16-ish fixed point scaled by 2^frac (frac 0..20).
 * Pure integer manipulation of the IEEE-754 bit pattern: about 15
 * instructions, which matters because it runs per vertex. */
int    psx_f2fix(float f, int frac);
int    psx_ftoi(float f);

/* ---- timing (PSX root counter 2, system clock / 8) ---- */

void          psx_time_init(void);
unsigned int  psx_time_ticks(void);   /* raw accumulated ticks (4.2336 MHz) */
unsigned int  psx_time_ms(void);      /* milliseconds since psx_time_init() */
void          psx_time_poll(void);    /* fold counter into the accumulator */

#endif /* _PSXMINILIBC_H */
