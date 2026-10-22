/*
 * gltron.c — PSXLinux port of GLTron 0.53 (Andreas Umbach, 1999).
 *
 * The original main() parsed settings files, loaded menu.txt and
 * dropped into the GUI menus; the PSX port keeps the game flow and
 * drives straight into a match (menus are future work).  ESC pauses;
 * ESC from pause or the cross button restarts the round.
 */

#include "gltron.h"

#include "globals.h"

extern void glProfPhase(int ph);   /* minigl.c draw-phase timer */

int getElapsedTime(void) {
  return glutGet(GLUT_ELAPSED_TIME);
}

void mouseWarp() {
  glutWarpPointer(game->screen->w / 2, game->screen->h / 2);
}

void drawGame() {
  GLint i;
  gDisplay *d;
  Player *p;

  glProfPhase(-1);   /* open the phase timer (excludes idle time) */
  polycount = 0;
  glClearColor(.0, .0, .0, .0);
  glDepthMask(GL_TRUE);
  glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDepthMask(GL_FALSE);

  for(i = 0; i < vp_max[ game->settings->display_type]; i++) {
    p = &(game->player[ game->settings->content[i] ]);
    if(p->display->onScreen == 1) {
      d = p->display;
      glViewport(d->vp_x, d->vp_y, d->vp_w, d->vp_h);
      drawCam(p, d);
      drawScore(p, d);
      if(game->settings->show_ai_status)
	if(p->ai->active == 1)
	  drawAI(d);
    }
  }

  if(game->settings->show_2d > 0)
    drawDebugTex(game->screen);
  if(game->settings->show_fps)
    drawFPS(game->screen);
  glProfPhase(6);        /* score/AI/map/fps overlays */
}

void displayGame() {
  drawGame();
  if(game->settings->mouse_warp)
    mouseWarp();
  glutSwapBuffers();
}

void initCustomLights() {
  float col[] = { .77f, .77f, .77f, 1.0f };
  float dif[] =  { 0.4f, 0.4f, 0.4f, 1.0f };
  float amb[] = { 0.25f, 0.25f, 0.25f, 1.0f };

  glEnable(GL_LIGHT0);
  glLightfv(GL_LIGHT0, GL_AMBIENT, amb);
  glLightfv(GL_LIGHT0, GL_SPECULAR, col);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, dif);
}

void initGLGame() {
  printf("minigl %s (%s / %s)\n", glGetString(GL_VERSION),
	 glGetString(GL_VENDOR), glGetString(GL_RENDERER));

  glShadeModel( GL_SMOOTH );

  if(game->settings->show_alpha)
    glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glFogf(GL_FOG_START, 50.0f);
  glFogf(GL_FOG_END, 100.0f);
  glFogf(GL_FOG_MODE, GL_LINEAR);
  glFogf(GL_FOG_DENSITY, 0.1f);
  glDisable(GL_FOG);

  initCustomLights();

  glDepthMask(GL_FALSE);
  glDisable(GL_DEPTH_TEST);
}

int initWindow() {
  int win_id;

  glutInitWindowSize(game->settings->width, game->settings->height);
  glutInitWindowPosition(0, 0);
  glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);

  win_id = glutCreateWindow("gltron");
  if (win_id < 0) {
    printf("could not create window...exiting\n");
    exit(1);
  }
  return win_id;
}

void shutdownDisplay(gDisplay *d) {
  deleteTextures();
  deleteFonts();
  glutDestroyWindow(d->win_id);
}

void setupDisplay(gDisplay *d) {
  d->win_id = initWindow();
  initFonts();
  initTexture(game->screen);
}

int main(int argc, char *argv[])
{
  glutInit(&argc, argv);

  /* settings: original defaults, no .gltronrc */
  initMainGameSettings("settings.txt");

  /* sound: stubbed (SPU port is future work) */

  initGameStructures();
  resetScores();
  initData();

  setupDisplay(game->screen);
  switchCallbacks(&gameCallbacks);

  glutMainLoop();

  return 0;
}

callbacks gameCallbacks = {
  displayGame,
  idleGame,
  keyGame,
  specialGame,
  initGame,
  initGLGame
};

/* ESC lands here when there is no menu system: restart the round. */
callbacks guiCallbacks = {
  displayGame,
  idleGame,
  keyGame,
  specialGame,
  initGame,
  initGLGame
};
