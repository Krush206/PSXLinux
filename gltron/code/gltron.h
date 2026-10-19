/*
  gltron 0.50 beta
  Copyright (C) 1999 by Andreas Umbach <marvin@dataway.ch>

  PSXLinux port: no libc headers — minilibc provides printf,
  malloc, the math wrappers and the PSX base types.
*/

#include "../../psxdev/minilibc.h"

#ifndef GLTRON_H
#define GLTRON_H 1

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SEPERATOR '/'
#define RC_NAME ".gltronrc"
#define CURRENT_DIR "."
#define HOMEVAR "HOME"

/* win32 additions by Jean-Bruno Richard <jean-bruno.richard@mg2.com> */

#ifdef WIN32
#include <windows.h>
#define SOUND
#define M_PI 3.141592654
#define SEPERATOR '\\'
#define RC_NAME "gltron.ini"
#define HOMEVAR "HOMEPATH"
#endif

/* FreeBSD additions by Andrey Zakhatov <andy@icc.surw.chel.su>  */

#ifdef __FreeBSD__
#include <floatingpoint.h>
#endif

/* MacOS additions by Stefan Buchholtz <sbuchholtz@online.de> */

#ifdef macintosh
#include <string.h>
#include <console.h>
#define M_PI 3.141592654
#define SEPERATOR ':'
#define RC_NAME "gltron.ini"
#endif

#define COS(X)	cos( (X) * M_PI/180.0 )
#define SIN(X)	sin( (X) * M_PI/180.0 )

/* glut includes all necessary GL - Headers */

#include "../../psxdev/minigl.h"
#include "../../psxdev/miniglut.h"

/* use texfont for rendering fonts as textured quads */
/* todo: get rid of that (it's not free) */

/* #include "TexFont.h" */
#include "fonttex.h"

/* menu stuff */


/* TODO(3): incorporate model stuff */
/* model stuff */
#include "model.h"
/* poly-soup stuff */
/* #include "polysoup.h" */

/* do Sound */


/* global constants */

#define PLAYERS 4
#define MAX_PLAYERS 4
#define MAX_TRAIL 1000

#define GSIZE 200

#define B_HEIGHT 0
#define TRAIL_HEIGHT 3.5
#define CYCLE_HEIGHT 8
#define WALL_H 12

#define CAM_COUNT 3
#define CAM_CIRCLE_DIST 15
#define CAM_CIRCLE_Z 8.0
#define CAM_FOLLOW_DIST 12
#define CAM_FOLLOW_Z 6.0
#define CAM_FOLLOW_SPEED 0.05
#define CAM_SPEED 2.0

#define EXP_RADIUS_MAX 30
#define EXP_RADIUS_DELTA 0.01

/* these must be < 0 */
#define SPEED_CRASHED -1
#define SPEED_GONE -2

#define FAST_FINISH 40

/* when running as screen saver, wait SCREENSAVER_WAIT ms after each round */

#define SCREENSAVER_WAIT 2000

/* data structures */
/* todo: move to seperate file */

typedef struct callbacks {
  void (*display)(void);
  void (*idle)(void);
  void (*keyboard)(unsigned char, int, int);
  void (*special)(int, int, int);
  void (*init)(void);
  void (*initGL)(void);
} callbacks;

typedef struct line {
  float sx, sy, ex, ey;
} line;

typedef struct Model {
  Mesh* mesh; /* model */
  float color_alpha[4]; /* alpha trail */
  float color_trail[4]; /* solid edges of trail */
  float color_model[4]; /* model color */
} Model;

typedef struct Data {
  float posx; float posy;

  int dir; int last_dir;
  int turn_time;
  
  int score;
  float speed; /* set to -1 when dead */
  float trail_height; /* countdown to zero when dead */
  float exp_radius; /* explosion of the cycle model */
  line trails[MAX_TRAIL];
  line *trail; /* current trail */
} Data;

typedef struct Camera {
  float cam[3];
  float target[3];
  float angle;
  int camType;
} Camera;

typedef struct AI {
  int active;
  int tdiff; /*  */
  int moves;
  int danger;
} AI;

typedef struct gDisplay {
  int win_id;     /* nur das globale Window hat eine */
  int h, w;       /* window */
  int vp_x, vp_y; /* viewport */
  int vp_h, vp_w;
  int blending;
  int fog;
  int wall;
  int onScreen;

  unsigned int texFloor; 
  unsigned int texWall;
  unsigned int texGui;
  unsigned int texCrash;
} gDisplay;

typedef struct Player {
  Model *model;
  Data *data;
  Camera *camera;
  gDisplay *display;
  AI *ai;
} Player;

/* if you want to add something and make it permanent (via
   .gltronrc) then
   1) add it to Settings in gltron.h
   2) add it to settings.txt
   3) add pointer to initSettingsData() in settings.c
   4) add a default to initMainGameSettings() in settings.c
   5) make a menu entry in menu.txt
*/
typedef struct Settings {
  int show_help;
  int show_fps;
  int show_wall;
  int show_2d;
  int show_alpha;
  int show_floor_texture;
  int show_glow;
  int show_ai_status;
  int show_model;
  int show_crash_texture;
  int turn_cycle;
  int erase_crashed;
  int fast_finish;
  int display_type; /* 0-2 -> 1, 2 or 4 displays on the screen */
  int content[4]; /* max. 4 individual viewports on the screen */
  int playSound;
  int screenSaver; /* 1: all for players are AIs when the game starts */
  int windowMode;
  int line_spacing;
  int camType;
  int mouse_warp;
  float speed;

  int ai_player1;
  int ai_player2;
  int ai_player3;
  int ai_player4;

  int fov;
  int width;
  int height;

  int sound_driver;

} Settings;

typedef struct Game {
  gDisplay *screen;
  Settings *settings;
  Player player[MAX_PLAYERS];
  int players;
  int winner;
  int pauseflag;
  int running;
} Game;

typedef struct settings_int {
  char name[32];
  int *value;
} settings_int;

typedef struct settings_float {
  char name[32];
  float *value;
} settings_float;

#define PAUSE_GAME_FINISHED 1

extern callbacks guiCallbacks;
extern callbacks pauseCallbacks;
extern callbacks gameCallbacks;

extern int gl_error;

extern settings_int *si;
extern int si_count;
extern settings_float *sf;
extern int sf_count;

extern Game main_game;
extern Game *game;
extern float camAngle;

/* extern TexFont *txf; */
extern fonttex *ftx;
extern int fontID;
#define MAX_FONTS 17

/* PSXLinux port: menus are not compiled — stub the type here so
 * the globals still declare. */
typedef struct { int dummy; } Menu;

extern Menu** pMenuList;
extern Menu* pRootMenu;
extern Menu* pCurrent;

/* game mode entry points (engine.c / input.c / gltron.c) */
extern void displayGame(void);
extern void idleGame(void);
extern void keyGame(unsigned char k, int x, int y);
extern void specialGame(int key, int x, int y);
extern void initGame(void);
extern void initGLGame(void);

extern unsigned char* colmap;
extern int colwidth;

extern int dirsX[];
extern int dirsY[];

extern int lasttime; 
extern double dt; /* milliseconds since last frame */

extern int polycount;

extern float colors_alpha[][4];
extern float colors_trail[][4];
extern float colors_model[][4];
extern int vp_max[];
extern float vp_x[3][4];
extern float vp_y[3][4];
extern float vp_w[3][4];
extern float vp_h[3][4];

#define HELP_LINES 18
#define HELP_FONT GLUT_BITMAP_9_BY_15
#define HELP_DY 20

extern char *help[];

#endif /* GLTRON_H */
