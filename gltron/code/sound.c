/*
 * sound.c — PSXLinux port: mikmod is stubbed out for the first
 * playable build (the SPU port of the GLTron soundtrack is future
 * work — see psxdev/psxlib.c's SPU layer).
 */

#include "gltron.h"

int initSound(void)   { return 1; }     /* "failed" → game runs silent */
void deleteSound(void) { }
void soundIdle(void) { }
int loadSound(char *name) { (void)name; return 1; }
int playSound(void)  { return 1; }
int stopSound(void)  { return 0; }
