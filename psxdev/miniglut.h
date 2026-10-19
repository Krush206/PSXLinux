/*
 * miniglut.h — GLUT shim for PSXLinux.
 *
 * Maps the GLUT surface GLTron 0.53 uses onto the PSX: window
 * creation becomes GPU display setup, the main loop drives the
 * registered display/idle callbacks with pad input, and "swap
 * buffers" presents the finished ordering table.
 *
 * Keyboard: the PSX digital pad maps to the keys the game binds —
 *   d-pad left/right  -> 'a' / 's'      (steer left/right)
 *   cross / start     -> ' ' / ESC       (pause / back)
 *   triangle/square    -> F1..F3 camera  (special keys)
 */

#ifndef _MINIGLUT_H
#define _MINIGLUT_H

#include "minigl.h"

/* init */
void glutInit(int *argcp, char **argv);
void glutInitDisplayMode(unsigned int mode);
void glutInitWindowPosition(int x, int y);
void glutInitWindowSize(int width, int height);
int  glutCreateWindow(const char *title);
void glutDestroyWindow(int win);

/* display-mode (fullscreen) — the PSX is always "game mode" */
void glutGameModeString(const char *string);
int  glutEnterGameMode(void);
int  glutGameModeGet(int mode);

/* callbacks */
void glutDisplayFunc(void (*func)(void));
void glutIdleFunc(void (*func)(void));
void glutKeyboardFunc(void (*func)(unsigned char key, int x, int y));
void glutSpecialFunc(void (*func)(int key, int x, int y));

/* loop */
void glutMainLoop(void);
void glutPostRedisplay(void);

/* buffer + misc */
void glutSwapBuffers(void);
void glutWarpPointer(int x, int y);
int  glutGet(int state);

/* bitmap font stubs (GLTron renders text as textured quads) */
void glutBitmapCharacter(void *font, int character);
#define GLUT_BITMAP_9_BY_15      ((void *)0)
#define GLUT_BITMAP_HELVETICA_18 ((void *)1)

/* sphere generators used by the (commented-out) crash highlight */
void glutSolidSphere(GLdouble radius, GLint slices, GLint stacks);
void glutWireSphere(GLdouble radius, GLint slices, GLint stacks);

/* display-mode bits */
#define GLUT_DOUBLE 0x02
#define GLUT_RGBA   0x00
#define GLUT_DEPTH  0x10

/* glutGet */
#define GLUT_ELAPSED_TIME   700
#define GLUT_SCREEN_WIDTH   200
#define GLUT_SCREEN_HEIGHT  201

/* glutGameModeGet */
#define GLUT_GAME_MODE_ACTIVE         0
#define GLUT_GAME_MODE_POSSIBLE       1
#define GLUT_GAME_MODE_WIDTH          2
#define GLUT_GAME_MODE_HEIGHT         3
#define GLUT_GAME_MODE_PIXEL_DEPTH    4
#define GLUT_GAME_MODE_REFRESH_RATE  5
#define GLUT_GAME_MODE_DISPLAY_CHANGED 6

/* special keys */
#define GLUT_KEY_LEFT   100
#define GLUT_KEY_UP     101
#define GLUT_KEY_RIGHT  102
#define GLUT_KEY_DOWN   103
#define GLUT_KEY_F1      1
#define GLUT_KEY_F2      2
#define GLUT_KEY_F3      3
#define GLUT_KEY_F5      5
#define GLUT_KEY_F10    10

#endif /* _MINIGLUT_H */
