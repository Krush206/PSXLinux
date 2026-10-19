#include "gltron.h"

callbacks *last_callback = 0;
callbacks *current_callback = 0;

void switchCallbacks(callbacks *new) {
  last_callback = current_callback;
  current_callback = new;

  glutIdleFunc(new->idle);
  glutDisplayFunc(new->display);
  glutKeyboardFunc(new->keyboard);
  glutSpecialFunc(new->special);

  lasttime = getElapsedTime();

 /* printf("callbacks registred\n"); */
  (new->init)();
  (new->initGL)();
  /* printf("callback init's completed\n"); */
}
  
void updateCallbacks() {
  /* called when the window is recreated */
  glutIdleFunc(current_callback->idle);
  glutDisplayFunc(current_callback->display);
  glutKeyboardFunc(current_callback->keyboard);
  glutSpecialFunc(current_callback->special);
  (current_callback->initGL)();
}

void restoreCallbacks() {
  if(last_callback == 0) {
    printf("no last callback present, exiting\n");
    exit(1);
  }
  current_callback = last_callback;

  glutIdleFunc(current_callback->idle);
  glutDisplayFunc(current_callback->display);
  glutKeyboardFunc(current_callback->keyboard);
  glutSpecialFunc(current_callback->special);
  
  lasttime = getElapsedTime();

  printf("restoring callbacks\n");
}

void chooseCallback(char *name) {
  /* maintain a table of names of callbacks */
  /* lets hardcode the names for all known modes in here */

/* TODO(3): incorporate model stuff */
  /*
  if(strcmp(name, "chooseModel") ==  0) {
    printf("change callbacks to chooseModel\n");
    switchCallbacks(&chooseModelCallbacks);
  }
  */
  if(strcmp(name, "gui") == 0) {
    /* printf("change callbacks to gui\n"); */
    switchCallbacks(&gameCallbacks);
  }
}

