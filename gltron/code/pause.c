#include "gltron.h"

/* very brief - just the pause mode */

void idlePause() {
#ifdef SOUND
  soundIdle();
#endif
  if(getElapsedTime() - lasttime < 10) return;
  timediff();

  glutPostRedisplay();
}

void displayPause() {
  drawGame();
  drawPause(game->screen);

  if(game->settings->mouse_warp)
    mouseWarp();
  glutSwapBuffers();
}

void keyboardPause(unsigned char key, int x, int y) {
  switch(key) {
  case 27:
    switchCallbacks(&gameCallbacks);
    break;
  case ' ':
    if(game->pauseflag & PAUSE_GAME_FINISHED)
      initData();
    lasttime = getElapsedTime();
    switchCallbacks(&gameCallbacks);
    break;
  case 'q':
    exit(1);
    break;
  }
}

void specialPause(int key, int x, int y) {
  int i;

  switch(key) {
  case GLUT_KEY_F1: defaultDisplay(0); break;
  case GLUT_KEY_F2: defaultDisplay(1); break;
  case GLUT_KEY_F3: defaultDisplay(2); break;

  case GLUT_KEY_F10:
    game->settings->camType = (game->settings->camType + 1) % CAM_COUNT;
    for(i = 0; i < game->players; i++)
      game->player[i].camera->camType = game->settings->camType;
    break;

  case GLUT_KEY_F5: saveSettings(); break;
  }
}

void initPause() {
}

void initPauseGL() {
  initGLGame();
}

callbacks pauseCallbacks = {
  displayPause, idlePause, keyboardPause, specialPause, initPause, initPauseGL
};


