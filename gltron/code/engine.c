#include "gltron.h"
extern int printf(const char *fmt, ...);
#include "psxdata.h"

void setCol(int x, int y) {
  int offset, mask;
  if(x < 0 || x > GSIZE - 1 || y < 0 || y > GSIZE - 1) {
    printf("setCol: %d %d is out of range!\n", x, y);
    return;
  }
  offset = x / 8 + y * colwidth;
  mask = 128 >> (x % 8);
  *(colmap + offset) |= mask;
}

void clearCol(int x, int y) {
  int offset, mask;
  if(x < 0 || x > GSIZE - 1 || y < 0 || y > GSIZE - 1) {
    printf("clearCol: %d %d is out of range!\n", x, y);
    return;
  }
  offset = x / 8 + y * colwidth;
  mask = 128 >> (x % 8);
  *(colmap + offset) &= !mask;
}

int getCol(int x, int y) {
  int offset, mask;
  if(x < 0 || x > GSIZE - 1 || y < 0 || y > GSIZE - 1)
    return -1;
  offset = x / 8 + y * colwidth;
  mask = 128 >> (x % 8);
  return *(colmap + offset) & mask;
}

void turn(Data* data, int direction) {
  line *new;

  if(data->speed > 0) { /* only allow turning when in-game */
    data->trail->ex = data->posx;
    data->trail->ey = data->posy;

    /* smooth turning */
    data->last_dir = data->dir;
    data->turn_time = getElapsedTime();

    data->dir = (data->dir + direction) % 4;

    new = (data->trail) + 1;
    new->ex = new->sx = data->trail->ex;
    new->ey = new->sy = data->trail->ey;

    data->trail = new;
  }
}


void initDisplay(gDisplay *d, int type, int p, int onScreen) {
  int field;
  field = game->screen->vp_w / 32;
  d->h = game->screen->h;
  d->w = game->screen->w;
  d->vp_x = vp_x[type][p] * field;
  d->vp_y = vp_y[type][p] * field;
  d->vp_w = vp_w[type][p] * field;
  d->vp_h = vp_h[type][p] * field;
  d->blending = 1;
  d->fog = 0;
  d->wall = 1;
  d->onScreen = onScreen;
}  

void changeDisplay() {
  int i;
  for(i = 0; i < game->players; i++)
    game->player[i].display->onScreen = 0;
  for(i = 0; i < vp_max[game->settings->display_type]; i++)
       initDisplay(game->player[ game->settings->content[i] ].display, 
		   game->settings->display_type, i, 1);
}

void initGame() { /* called when game mode is entered */
  /* The PSX port skips the GUI menus, so the viewports are set
   * up here instead: initGameScreen() sizes game->screen from
   * the settings, changeDisplay() marks the viewports that
   * drawGame() renders (display->onScreen = 1).  Without this
   * the per-player viewport loop in drawGame() draws nothing
   * and every frame is just the black clear. */
  initGameScreen();
  changeDisplay();
}

void initGameStructures() { /* called only once */
  /* init game screen */
  /* init players. for each player: */
  /*   init model */
  /*   init display */
  /*   init ai */
  /*   create data */
  /*   create camera */

  gDisplay *d;
  int i, j;
  /* int onScreen; */
  /* Data *data; */
  /* Camera *c; */
  /* Model *m; */
  AI *ai;
  Player *p;
  char *path;

  game->winner = -1;
  game->screen = (gDisplay*) malloc(sizeof(gDisplay));
  d = game->screen;
  d->h = game->settings->height; d->w = game->settings->width;
  d->vp_x = 0; d->vp_y = 0;
  d->vp_w = d->w; d->vp_h = d->h;
  d->blending = 1;
  d->fog = 0;
  d->wall = 1;
  d->onScreen = -1;

  game->players = PLAYERS;
  for(i = 0; i < game->players; i++) {
    p = &(game->player[i]);
    p->model = (Model*) malloc(sizeof(Model));
    p->display = (gDisplay*) malloc(sizeof(gDisplay));
    p->ai = (AI*) malloc(sizeof(AI));
    p->data = (Data*) malloc(sizeof(Data));
    p->camera = (Camera*) malloc(sizeof(Camera));

    // init model & display & ai

    // load player mesh — preprocessed (see psxdata.h), one shared Mesh
    p->model->mesh = psxBuildCycleModel();

    /* copy contents from colors_a[] to model struct */
    for(j = 0; j < 4; j++) {
      p->model->color_alpha[j] = colors_alpha[i][j];
      p->model->color_trail[j] = colors_trail[i][j];
      p->model->color_model[j] = colors_model[i][j];
    }
    // set material 0 to color_model
    setMaterialAmbient(p->model->mesh, 0, p->model->color_model);
    setMaterialDiffuse(p->model->mesh, 0, p->model->color_model);

    ai = p->ai;
    ai->active = (i == 0 && game->settings->screenSaver == 0) ? -1 : 1;
    ai->tdiff = 0;
    ai->moves = 0;
    ai->danger = 0;
  }

  changeDisplay();
  initData();
}

void initData() {
  /* for each player */
  /*   init camera (if any) */
  /*   init data */
  /*   reset ai (if any) */
  int i;
  Camera *cam;
  Data *data;
  AI *ai;
  Model *model;

  for(i = 0; i < game->players; i++) {
    data = game->player[i].data;
    cam = game->player[i].camera;
    ai = game->player[i].ai;
    model = game->player[i].model;

    setMaterialAlphas(model->mesh, 1.0);

    cam->camType = game->settings->camType;
    cam->target[0] = data->posx;
    cam->target[1] = data->posy;
    cam->target[2] = 0;

    cam->cam[0] = data->posx + CAM_CIRCLE_DIST;
    cam->cam[1] = data->posy;
    cam->cam[2] = CAM_CIRCLE_Z;

    data->posx = GSIZE / 2 + GSIZE / 4 *
      cos ( (float) (i * 2) * (6.28318530717958647692f / (float) game->players) );
    data->posy = GSIZE / 2 + GSIZE / 4 * 
      sin ( (float) (i * 2) * (6.28318530717958647692f / (float) game->players) );

    data->dir = rand() & 3;
    data->last_dir = data->dir;
    data->turn_time = 0;

    data->speed = game->settings->speed;
    data->trail_height = TRAIL_HEIGHT;
    data->trail = data->trails;
    data->exp_radius = 0;

    data->trail->sx = data->trail->ex = data->posx;
    data->trail->sy = data->trail->ey = data->posy;

    ai->tdiff = 0;
    ai->moves = 0;
    ai->danger = 0;
  }

  game->running = game->players; /* everyone is alive */
  game->winner = -1;
  /* colmap */
  colwidth = (GSIZE + 7) / 8;
  if(colmap == NULL) colmap = (unsigned char*) malloc(colwidth * GSIZE);
  for(i = 0; i < colwidth * GSIZE; i++)
    *(colmap + i) = 0;

  lasttime = getElapsedTime();
  game->pauseflag = 0;
}


int colldetect(float sx, float sy, float ex, float ey, int dir, int *x, int *y) {
  /* no getenv on PSXLinux; collision always on */
  *x = (int) sx;
  *y = (int) sy;

  while(*x != (int) ex || *y != (int) ey) {
    *x += dirsX[dir];
    *y += dirsY[dir];
    if(getCol(*x, *y)) {
      /* check if x/y are in bounds and correct it */
      if(*x < 0) *x = 0;
      if(*x >= GSIZE) *x = GSIZE -1; 
      if(*y < 0) *y = 0;
      if(*y >= GSIZE) *y = GSIZE -1; 
      return 1;
    }
  }
  return 0;
}

void doTrail(line *t, void(*mark)(int, int)) {	  
  int x, y, ex, ey, dx, dy;

  x = (t->sx < t->ex) ? t->sx : t->ex;
  y = (t->sy < t->ey) ? t->sy : t->ey;
  ex = (t->sx > t->ex) ? t->sx : t->ex;
  ey = (t->sy > t->ey) ? t->sy : t->ey;
  dx = (x == ex) ? 0 : 1;
  dy = (y == ey) ? 0 : 1;
  if(dx == 0 && dy == 0) {
    mark(x, y);
  } else 
    while(x <= ex && y <= ey) {
      mark(x, y);
      x += dx;
      y += dy;
    }
}

void fixTrails() {
  int i;
  Data *d;
  line *t;
  for(i = 0; i < game->players; i++) {
    d = game->player[i].data;
    if(d->speed > 0) {
      t = &(d->trails[0]);
      while(t != d->trail) {
	doTrail(t, setCol);
	t++;
      }
      doTrail(t, setCol);
    }
  }
}

void clearTrails(Data *data) {
  line *t = &(data->trails[0]);
  while(t != data->trail) {
    doTrail(t, clearCol);
    t++;
  }
  doTrail(t, clearCol);
}


void idleGame( void ) {
  int i, j;
  int loop; 


#ifdef SOUND
  soundIdle();
#endif

  if(game->settings->fast_finish == 1) {
    loop = FAST_FINISH;
    for(i = 0; i < game->players; i++)
      if(game->player[i].ai->active != 1 &&
	 game->player[i].data->exp_radius < EXP_RADIUS_MAX)
	 /* game->player[i].data->speed > 0) */
	loop = 1;
  } else loop = 1;

  if(getElapsedTime() - lasttime < 10 && loop == 1) return;
  timediff();
  for(j = 0; j < loop; j++) {
    if(loop == FAST_FINISH)
      dt = 20;
    movePlayers();

    /* do AI */
    for(i = 0; i < game->players; i++)
      if(game->player[i].ai != NULL)
	if(game->player[i].ai->active == 1)
	  doComputer(&(game->player[i]), game->player[i].data);
  }

  /* chase-cam movement here */
  camMove();
  chaseCamMove();

  glutPostRedisplay();
}

void defaultDisplay(int n) {
  game->settings->display_type = n;
  game->settings->content[0] = 0;
  game->settings->content[1] = 1;
  game->settings->content[2] = 2;
  game->settings->content[3] = 3;
  changeDisplay();
}

void initGameScreen() {
  gDisplay *d;
  d = game->screen;
  d->h = game->settings->height; d->w = game->settings->width;
  d->vp_x = 0; d->vp_y = 0;
  d->vp_w = d->w; d->vp_h = d->h;
}

void cycleDisplay(int p) {
  int q;
  q = (game->settings->content[p] + 1) % game->players;
  while(q != game->settings->content[p]) {
    if(game->player[q].display->onScreen == 0)
      game->settings->content[p] = q;
    else q = (q + 1) % game->players;
  }
  changeDisplay();
}

void resetScores() {
  int i;
  for(i = 0; i < game->players; i++)
    game->player[i].data->score = 0;
}

void movePlayers() {
  int i, j;
  float newx, newy;
  int x, y;
  int col;
  int winner;
  Data *data;

  /* do movement and collision */
  for(i = 0; i < game->players; i++) {
    data = game->player[i].data;
    if(data->speed > 0) { /* still alive */
      newx = data->posx + dt / 100 * data->speed * dirsX[data->dir];
      newy = data->posy + dt / 100 * data->speed * dirsY[data->dir];
      
      if((int)data->posx != newx || (int)data->posy != newy) {
	/* collision-test here */
	/* boundary-test here */
	col = colldetect(data->posx, data->posy, newx, newy,
			 data->dir, &x, &y);
	if (col) {
#ifdef SOUND
	  /* playCrash(); */
#endif
	  /* set endpoint to collision coordinates */
	  newx = x;
	  newy = y;
	  /* update scores; */
	  if(game->settings->screenSaver != 1)
	  for(j = 0; j < game->players; j++) {
	    if(j != i && game->player[j].data->speed > 0)
	      game->player[j].data->score++;
	  }
	  data->speed = SPEED_CRASHED;
	}

	/* now draw marks in the bitfield */
	x = (int) data->posx;
	y = (int) data->posy;
	while(x != (int)newx ||
	      y != (int)newy ) {
	  x += dirsX[data->dir];
	  y += dirsY[data->dir];
	  setCol(x, y);
	}
	data->trail->ex = data->posx = newx;
	data->trail->ey = data->posy = newy;

	if(col && game->settings->erase_crashed == 1) {
	  clearTrails(data);
	  fixTrails(); /* clearTrails does too much... */
	}
      }
    } else { /* do trail countdown && explosion */
      if(data->exp_radius < EXP_RADIUS_MAX)
	data->exp_radius += (float)dt * EXP_RADIUS_DELTA;
      else if (data->speed == SPEED_CRASHED) {
	data->speed = SPEED_GONE;
	game->running--;
	if(game->running <= 1) { /* all dead, find survivor */
	  for(winner = 0; winner < game->players; winner++)
	    if(game->player[winner].data->speed > 0) break;
	  game->winner = (winner == game->players) ? -1 : winner;
	  printf("winner: %d\n", winner);
	  switchCallbacks(&pauseCallbacks);
	  /* screenSaverCheck(0); */
	  game->pauseflag = PAUSE_GAME_FINISHED;
	}
      }
      if(game->settings->erase_crashed == 1 && data->trail_height > 0)
	data->trail_height -= (float)(dt * TRAIL_HEIGHT) / 1000;
    }
  }
}

void timediff() {
  int t;
  t = getElapsedTime();
  dt = t - lasttime;
  lasttime = t;
}


void chaseCamMove() {
  int i;
  Camera *cam;
  Data *data;
  float dest[3];
  float dcamx;
  float dcamy;
  float d;

  for(i = 0; i < game->players; i++) {
      
    cam = game->player[i].camera;
    data = game->player[i].data;

    switch(cam->camType) {
    case 0: /* Andi-cam */
      cam->cam[0] = data->posx + CAM_CIRCLE_DIST * COS(camAngle);
      cam->cam[1] = data->posy + CAM_CIRCLE_DIST * SIN(camAngle);
      cam->cam[2] = CAM_CIRCLE_Z;
      cam->target[0] = data->posx;
      cam->target[1] = data->posy;
      cam->target[2] = B_HEIGHT;
      break;
    case 1: // Mike-cam
      cam->target[0] = data->posx;
      cam->target[1] = data->posy;
      cam->target[2] = B_HEIGHT;
      
      dest[0] = cam->target[0] - CAM_FOLLOW_DIST * dirsX[ data->dir];
      dest[1] = cam->target[1] - CAM_FOLLOW_DIST * dirsY[ data->dir];

      d = sqrt((dest[0] - cam->cam[0]) * (dest[0] - cam->cam[0]) +
	       (dest[1] - cam->cam[1]) * (dest[1] - cam->cam[1]));
      if(d != 0) {
	dcamx = (float)dt * CAM_FOLLOW_SPEED * (dest[0] - cam->cam[0]) / d;
	dcamy = (float)dt * CAM_FOLLOW_SPEED * (dest[1] - cam->cam[1]) / d;

	if((dest[0] - cam->cam[0] > 0 && dest[0] - cam->cam[0] < dcamx) ||
	   (dest[0] - cam->cam[0] < 0 && dest[0] - cam->cam[0] > dcamx)) {
	  cam->cam[0] = dest[0];
	} else cam->cam[0] += dcamx;

	if((dest[1] - cam->cam[1] > 0 && dest[1] - cam->cam[1] < dcamy) ||
	   (dest[1] - cam->cam[1] < 0 && dest[1] - cam->cam[1] > dcamy)) {
	  cam->cam[1] = dest[1];
	} else cam->cam[1] += dcamy;
      }
      break;
    case 2: /* 1st person */
#define H 3
      cam->target[0] = data->posx + dirsX[data->dir];
      cam->target[1] = data->posy + dirsY[data->dir];
      cam->target[2] = H;

      cam->cam[0] = data->posx;
      cam->cam[1] = data->posy;
      cam->cam[2] = H + 0.5;
      break;
    }
  }
}

void camMove() {
  camAngle += CAM_SPEED * dt / 100;
  while(camAngle > 360) camAngle -= 360;
}
