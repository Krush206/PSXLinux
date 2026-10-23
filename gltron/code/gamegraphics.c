#include "gltron.h"
#include "geom.h"

void drawDebugTex(gDisplay *d) {
  int x = 100;
  int y = 100;

  rasonly(d);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glColor4f(.0, 1.0, .0, 1.0);
  glRasterPos2i(x, y);
  glBitmap(colwidth * 8 , GSIZE, 0, 0, 0, 0, colmap);
  glBegin(GL_LINE_LOOP);
  glVertex2i(x - 1, y - 1);
  glVertex2i(x + colwidth * 8, y - 1);
  glVertex2i(x + colwidth * 8, y + GSIZE);
  glVertex2i(x - 1, y + GSIZE);
  glEnd();
  polycount++;
}

void drawScore(Player *p, gDisplay *d) {
  char tmp[10]; /* hey, they won't reach such a score */

  sprintf(tmp, "%d", p->data->score);
  rasonly(d);
  glColor4f(1.0, 1.0, 0.2, 1.0);
  drawText(5, 5, 32, tmp);
}
  
static int floor_splits(int cam, int *out)
{
  int pts[40];
  int n = 0, m = 0, i, k;

  pts[n++] = 0;
  for(k = 0; k <= 6; k++) {
    int s = 1 << k;
    int a = cam - s, b = cam + s;
    if(a > 0 && a < GSIZE) pts[n++] = a;
    if(b > 0 && b < GSIZE) pts[n++] = b;
  }
  if(cam > 0 && cam < GSIZE)
    pts[n++] = cam;
  pts[n++] = GSIZE;
  /* insertion sort */
  for(i = 1; i < n; i++) {
    int v = pts[i];
    int j = i - 1;
    while(j >= 0 && pts[j] > v) { pts[j + 1] = pts[j]; j--; }
    pts[j + 1] = v;
  }
  /* dedupe */
  for(i = 0; i < n; i++)
    if(m == 0 || pts[i] - out[m - 1] >= 1)
      out[m++] = pts[i];
  /* cap cell size at 64 units (far cells cover few pixels each;
   * the affine stretch only matters for near cells, which stay
   * split small - and far cells are the expensive ones: each
   * projects far off screen and pays a full clip pass) */
  i = 0;
  while(i < m - 1) {
    if(out[i + 1] - out[i] > 64 && m < 38) {
      int j;
      for(j = m; j > i + 1; j--)
	out[j] = out[j - 1];
      out[i + 1] = (out[i] + out[i + 2]) / 2;
      m++;
    } else
      i++;
  }
  return m;
}

void drawFloor(gDisplay *d, float *cam) {
  int xs[40], ys[40];
  float u[40], v[40];
  int nx, ny, i, j;

  if(game->settings->show_floor_texture) {
    glDepthMask(GL_TRUE);
    /* floor and walls are opaque geometry; the global GL_BLEND state
     * (used by the trails/glow) must not dim them */
    glDisable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, game->screen->texFloor);
    /* The PSX GPU maps textures affinely per primitive: a 50-unit
     * tile carries only a few dozen texels and stretches them
     * across the whole screen whenever the camera sits low.
     * Emit the floor as a grid whose cells shrink with distance
     * from the camera (floor_splits on each axis).  Texture
     * phase comes straight from world position - one 64-texel
     * repeat per 10 units, the original game's t=5 repeats per
     * 50-unit tile - and minigl remaps each quad's UV bytes to
     * that quad's own phase, so the graded split points do not
     * need to align with repeat edges. */
    glColor4f(1.0, 1.0, 1.0, 1.0);
    nx = floor_splits(cam ? (int)(cam[0] + 0.5f) : 100, xs);
    ny = floor_splits(cam ? (int)(cam[1] + 0.5f) : 100, ys);
    /* x * 0.1f is one soft-float chain; compute the texcoords
     * once per split point instead of once per quad corner
     * (~300 quads x 4 corners used to be ~4800 soft-float
     * calls per frame, the single largest floor cost). */
    for(i = 0; i < nx; i++) u[i] = xs[i] * 0.1f;
    for(j = 0; j < ny; j++) v[j] = ys[j] * 0.1f;
    {
      /* batch the cells 24 quads per glBegin/glEnd (minigl's
       * 96-vertex buffer): the per-pair stage processing - packet
       * setup, near/screen clip, emit - used to be paid once per
       * quad and dwarfed the per-vertex transform cost.  All
       * cells share colour, texture and blend state, so the
       * batching is invisible. */
      int nb = 0;
      for(i = 0; i < nx - 1; i++)
	for(j = 0; j < ny - 1; j++) {
	  if(xs[i] == xs[i + 1] || ys[j] == ys[j + 1])
	    continue;
	  if(nb == 0)
	    glBegin(GL_QUADS);
	  glTexCoord2f(u[i], v[j]); glVertex2i(xs[i], ys[j]);
	  glTexCoord2f(u[i + 1], v[j]); glVertex2i(xs[i + 1], ys[j]);
	  glTexCoord2f(u[i + 1], v[j + 1]); glVertex2i(xs[i + 1], ys[j + 1]);
	  glTexCoord2f(u[i], v[j + 1]); glVertex2i(xs[i], ys[j + 1]);
	  polycount++;
	  nb++;
	  if(nb == 24) {
	    glEnd();
	    nb = 0;
	  }
	}
      if(nb)
	glEnd();
    }
    glDisable(GL_TEXTURE_2D);
    glDepthMask(GL_FALSE);
  } else {
    /* lines as floor... */
    glColor3f(0.0, 0.0, 1.0);
    glBegin(GL_LINES);
    for(j = 0; j <= GSIZE; j += game->settings->line_spacing) {
      glVertex3i(0, j, 0);
      glVertex3i(GSIZE, j, 0);
      glVertex3i(j, 0, 0);
      glVertex3i(j, GSIZE, 0);
      polycount += 2;
    }
    glEnd();
  }
  glEnable(GL_BLEND);
}

void drawTraces(Player *p, gDisplay *d, int instance) {
  line *line;
  float height;

  Data *data;
  data = p->data;
  height = data->trail_height;
  if(height > 0) {
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glColor4fv(p->model->color_alpha);
    /* glColor4f(0.5, 0.5, 0.5, 0.8); */
    line = &(data->trails[0]);
    /* Each trail point becomes 2 strip windows; unbounded, that
     * eats the whole prim budget as trails grow (a 300-point trail
     * = 600 triangles).  Decimate to ~40 chords - the ribbon is
     * 5 units tall, so 2-unit chords still read as smooth curves. */
    {
      int step = (int)(data->trail - line) / 40 + 1;

      glBegin(GL_TRIANGLE_STRIP);
      glVertex3f(line->sx, line->sy, 0.0);
      glVertex3f(line->sx, line->sy, height);
      while(line != data->trail) {
	line += step;
	if(line > data->trail)
	  line = data->trail;
	glVertex3f(line->ex, line->ey, 0.0);
	glVertex3f(line->ex, line->ey, height);    
	polycount++;
      }
      glVertex3f(line->ex, line->ey, 0.0);
      glVertex3f(line->ex, line->ey, height);
      polycount += 2;
      glEnd();
    }

    if(game->settings->camType == 1) {
      //       glLineWidth(3);
      // glBegin(GL_LINES);
      glBegin(GL_QUADS);
#define LINE_D 0.05
      glVertex2f(data->trail->sx - LINE_D, data->trail->sy - LINE_D);
      glVertex2f(data->trail->sx + LINE_D, data->trail->sy + LINE_D);
      glVertex2f(data->trail->ex + LINE_D, data->trail->ey + LINE_D);
      glVertex2f(data->trail->ex - LINE_D, data->trail->ey - LINE_D);

      glEnd();
      // glLineWidth(1);
      polycount++;
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  }
}

void drawCrash(float radius) {
#define CRASH_W 20
  glColor4f(1.0, 1.0, 1.0, (EXP_RADIUS_MAX - radius) / EXP_RADIUS_MAX);
  /* printf("exp_r: %.2f\n", (EXP_RADIUS_MAX - radius) / EXP_RADIUS_MAX); */
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, game->screen->texCrash);
  glEnable(GL_BLEND);
  glBegin(GL_QUADS);
  glTexCoord2f(0.0, 0.0);
  glVertex3f(- CRASH_W, 0.0, 0.0);
  glTexCoord2f(1.0, 0.0);
  glVertex3f(CRASH_W, 0.0, 0.0);
  glTexCoord2f(1.0, .5);
  glVertex3f(CRASH_W, 0.0, CRASH_W);
  glTexCoord2f(0.0, .5);
  glVertex3f(- CRASH_W, 0.0, CRASH_W);
  glEnd();
  glDisable(GL_TEXTURE_2D);
  if(game->settings->show_alpha == 0) glDisable(GL_BLEND);
}

void drawCycle(Player *p, float *cam) {
  float dirangles[] = { 180, 90, 0, 270 , 360, -90 };
  int time;
  int last_dir;
  float dirangle;
  Mesh *cycle;

#define turn_length 500

  cycle = p->model->mesh;

  glPushMatrix();
  glTranslatef(p->data->posx, p->data->posy, .0);

  if(game->settings->turn_cycle) {
    time = abs(p->data->turn_time - getElapsedTime());
    if(time < turn_length) {
      last_dir = p->data->last_dir;
      if(p->data->dir == 3 && last_dir == 2)
	last_dir = 4;
      if(p->data->dir == 2 && last_dir == 3)
	last_dir = 5;
      dirangle = ((turn_length - time) * dirangles[last_dir] +
		  time * dirangles[p->data->dir]) / turn_length;
    } else
      dirangle = dirangles[p->data->dir];
  } else  dirangle = dirangles[p->data->dir];

  glRotatef(dirangle, 0, 0.0, 1.0);

  if(game->settings->show_crash_texture)
    if(p->data->exp_radius > 0 && p->data->exp_radius < EXP_RADIUS_MAX) {
      /* The crash quad is 40x20 units centred on the bike.  When the
       * camera sits inside that footprint the quad reaches the near
       * plane and clips into a fan that spreads the whole 64x32-texel
       * crash texture across the screen - a full-view artifact.
       * Skip it there; distant explosions are unaffected. */
      float dx = cam[0] - p->data->posx;
      float dy = cam[1] - p->data->posy;
      float dz = cam[2];
      if(dx * dx + dy * dy + dz * dz > 20.0f * 20.0f)
	drawCrash(p->data->exp_radius);
    }

#define neigung 25
  if(game->settings->turn_cycle) {
    if(time < turn_length) {
      float axis = 1.0;
      if(p->data->dir < p->data->last_dir && p->data->last_dir != 3)
	axis = -1.0;
      else if((p->data->last_dir == 3 && p->data->dir == 2) ||
	      (p->data->last_dir == 0 && p->data->dir == 3))
	axis = -1.0;
      glRotatef(neigung * sin((float)time * (3.14159265f / turn_length)),
		0.0, axis, 0.0);
    }
  }

  glTranslatef(-cycle->bbox[0] / 2, -cycle->bbox[1] / 2, .0);
  /* glTranslatef(-cycle->bbox[0] / 2, 0, .0); */
  /* glTranslatef(-cycle->bbox[0] / 2, -cycle->bbox[1], .0); */

  glEnable(GL_LIGHTING);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);

  if(p->data->exp_radius == 0) {
    /* the cycle is a closed hull: cull back faces to halve its
     * primitive count (minigl's GL_CULL_FACE is a screen-space
     * signed-area test in the emit path) */
    glEnable(GL_CULL_FACE);
    drawModel(cycle, MODEL_USE_MATERIAL, 0);
    glDisable(GL_CULL_FACE);
  }
  else if(p->data->exp_radius < EXP_RADIUS_MAX) {
    float alpha;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    alpha = (float) (EXP_RADIUS_MAX - p->data->exp_radius) /
      (float) EXP_RADIUS_MAX;
    setMaterialAlphas(cycle, alpha);
    drawExplosion(cycle, p->data->exp_radius, MODEL_USE_MATERIAL, 0);
  }

  if(game->settings->show_alpha == 0) glDisable(GL_BLEND);

  glDisable(GL_LIGHTING);
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);

  glPopMatrix();
}

int playerVisible(Player *eye, Player *target) {
  float v1[3];
  float v2[3];
  float tmp[3];
  float s;
  float d;

  vsub(eye->camera->target, eye->camera->cam, v1);
  normalize(v1);
  tmp[0] = target->data->posx;
  tmp[1] = target->data->posy;
  tmp[2] = 0;
  vsub(tmp, eye->camera->cam, v2);
  normalize(v2);
  s = scalarprod(v1, v2);
  /* maybe that's not exactly correct, but I didn't notice anything */
  d = cos((float)(game->settings->fov / 2) * 0.00872664626f);
  /*
  printf("v1: %.2f %.2f %.2f\nv2: %.2f %.2f %.2f\ns: %.2f d: %.2f\n\n",
	 v1[0], v1[1], v1[2], v2[0], v2[1], v2[2],
	 s, d);
  */
  if(s < d)
    return 0;
  else return 1;
}
	    
/*
 * PSX-style distance LOD: past this many units the cycle model
 * covers a few pixels, so only the coloured base quad (already
 * drawn for every player) marks the bike.  Squared distance, so
 * the test is two multiplies.
 */
#define CYCLE_DRAW_DIST2 14400.0f      /* 120 units */

void drawPlayers(Player *p) {
  int i;
  int dir;
  float l = 5.0;
  float height;

  glShadeModel(GL_SMOOTH);
  glEnable(GL_BLEND);
  for(i = 0; i < game->players; i++) {
    height = game->player[i].data->trail_height;
    if(height > 0) {
      glPushMatrix();
      glTranslatef(game->player[i].data->posx,
		   game->player[i].data->posy,
		   0);
      /* draw Quad */
      dir = game->player[i].data->dir;
      /* PSX adaptation: on the PC this wall fades out through
       * vertex alpha (color at the bike, (0,0,0,0) further back)
       * under standard alpha blending.  The PSX GPU has no
       * per-pixel alpha, so reproduce the fade with additive
       * blending instead: colour + background at the bike,
       * background alone at the tail.  All vertex alphas stay at
       * 1 so the emitted quads are not dropped as "faded". */
      glBlendFunc(GL_ONE, GL_ONE);
      glColor3fv(game->player[i].model->color_model);
      glBegin(GL_QUADS);
      glVertex3f(0, 0, 0);
      glColor3f(0.0, 0.0, 0.0);
      glVertex3f(-dirsX[ dir ] * l, -dirsY[ dir ] * l, 0);
      glVertex3f(-dirsX[ dir ] * l, -dirsY[ dir ] * l, height);
      glColor3fv(game->player[i].model->color_model);
      glVertex3f(0, 0, height);
      glEnd();
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      polycount++;
      glPopMatrix();
    }
    if(playerVisible(p, &(game->player[i]))) {
      if(game->settings->show_model)
	drawCycle(&(game->player[i]), p->camera->cam);
    }
  }
  if(game->settings->show_alpha != 1) glDisable(GL_BLEND);
  glShadeModel(GL_FLAT);
}

void drawGlow(Player *p, gDisplay *d, float dim) {
  float mat[4*4];
  
  glPushMatrix();
  glTranslatef(p->data->posx,
               p->data->posy,
               0);
  /* draw Model */

  glShadeModel(GL_SMOOTH);
  glBlendFunc(GL_ONE, GL_ONE);
  glEnable(GL_BLEND);
  glGetFloatv(GL_MODELVIEW_MATRIX, mat);
  mat[0] = mat[5] = mat[10] = 1.0;
  mat[1] = mat[2] = 0.0;
  mat[4] = mat[6] = 0.0;
  mat[8] = mat[9] = 0.0;
  glLoadMatrixf(mat);
  glBegin(GL_TRIANGLE_FAN);
  glColor3fv(p->model->color_model);

  glVertex3f(0,TRAIL_HEIGHT/2, 0);
  glColor4f(0,0,0,0.0);
  glVertex3f(dim*cos(-0.2*3.1415f/5.0f),
	     TRAIL_HEIGHT/2+dim*sin(-0.2*3.1415f/5.0f), 0);
  glVertex3f(dim*cos(1.0*3.1415f/5.0f),
	     TRAIL_HEIGHT/2+dim*sin(1.0*3.1415f/5.0f), 0);
  glVertex3f(dim*cos(2.0*3.1415f/5.0f),
	     TRAIL_HEIGHT/2+dim*sin(2.0*3.1415f/5.0f), 0);
  glVertex3f(dim*cos(3.0*3.1415f/5.0f),
	     TRAIL_HEIGHT/2+dim*sin(3.0*3.1415f/5.0f), 0);
  glVertex3f(dim*cos(4.0*3.1415f/5.0f),
	     TRAIL_HEIGHT/2+dim*sin(4.0*3.1415f/5.0f), 0);
  glVertex3f(dim*cos(5.2*3.1415f/5.0f),
	     TRAIL_HEIGHT/2+dim*sin(5.2*3.1415f/5.0f), 0);
  glEnd();
  polycount += 5;


  glBegin(GL_TRIANGLES);
  glColor3fv(p->model->color_model);
  glVertex3f(0,TRAIL_HEIGHT/2, 0);
  glColor4f(0,0,0,0.0);
  glVertex3f(0,-TRAIL_HEIGHT/4,0);
  glVertex3f(dim*cos(-0.2*3.1415f/5.0f),
	     TRAIL_HEIGHT/2+dim*sin(-0.2*3.1415f/5.0f), 0);

  glColor3fv(p->model->color_model);
  glVertex3f(0,TRAIL_HEIGHT/2, 0);
  glColor4f(0,0,0,0.0);
  glVertex3f(dim*cos(5.2*3.1415f/5.0f),
	     TRAIL_HEIGHT/2+dim*sin(5.2*3.1415f/5.0f), 0);
  glVertex3f(0,-TRAIL_HEIGHT/4,0);
  glEnd();
  polycount += 3;


  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  if(game->settings->show_alpha != 1) glDisable(GL_BLEND);
  glShadeModel(GL_FLAT);
  glPopMatrix();  
}

void drawWalls(gDisplay *d) {
  float t = 4;                  /* 4 repeats over 200 units, as upstream */
  int seg;
#define WALL_SEGS 8
  glColor4f(1.0, 1.0, 1.0, 1.0);
  glDisable(GL_BLEND);

  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  glEnable(GL_CULL_FACE);

  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, game->screen->texWall);
  glBegin(GL_QUADS);
  /* Each 200-unit wall is emitted in 25-unit segments: the PSX GPU
   * maps textures affinely per quad, so one quad spanning the whole
   * arena shears its texture along the view direction. */
  for(seg = 0; seg < WALL_SEGS; seg++) {
    float f0 = (float)seg / WALL_SEGS, f1 = (float)(seg + 1) / WALL_SEGS;
    /* along +x at y=0: u = t at x=0 .. 0 at x=GSIZE */
    {
      float x0 = GSIZE * f0, x1 = GSIZE * f1;
      float u0 = t * (1.0f - f0), u1 = t * (1.0f - f1);
      glTexCoord2f(u0, 0.0); glVertex3f(x0, 0.0, 0.0);
      glTexCoord2f(u0, 1.0); glVertex3f(x0, 0.0, WALL_H);
      glTexCoord2f(u1, 1.0); glVertex3f(x1, 0.0, WALL_H);
      glTexCoord2f(u1, 0.0); glVertex3f(x1, 0.0, 0.0);
    }
    /* along +y at x=GSIZE: u = t at y=0 .. 0 at y=GSIZE */
    {
      float y0 = GSIZE * f0, y1 = GSIZE * f1;
      float u0 = t * (1.0f - f0), u1 = t * (1.0f - f1);
      glTexCoord2f(u0, 1.0); glVertex3f(GSIZE, y0, 0.0);
      glTexCoord2f(u0, 0.0); glVertex3f(GSIZE, y0, WALL_H);
      glTexCoord2f(u1, 0.0); glVertex3f(GSIZE, y1, WALL_H);
      glTexCoord2f(u1, 1.0); glVertex3f(GSIZE, y1, 0.0);
    }
    /* along -x at y=GSIZE: u = t at x=GSIZE .. 0 at x=0 */
    {
      float x0 = GSIZE * f1, x1 = GSIZE * f0;
      float u0 = t * f1, u1 = t * f0;
      glTexCoord2f(u0, 1.0); glVertex3f(x0, GSIZE, 0.0);
      glTexCoord2f(u0, 0.0); glVertex3f(x0, GSIZE, WALL_H);
      glTexCoord2f(u1, 0.0); glVertex3f(x1, GSIZE, WALL_H);
      glTexCoord2f(u1, 1.0); glVertex3f(x1, GSIZE, 0.0);
    }
    /* along -y at x=0: u = t at y=GSIZE .. 0 at y=0 */
    {
      float y0 = GSIZE * f1, y1 = GSIZE * f0;
      float u0 = t * f1, u1 = t * f0;
      glTexCoord2f(u0, 1.0); glVertex3f(0.0, y0, 0.0);
      glTexCoord2f(u0, 0.0); glVertex3f(0.0, y0, WALL_H);
      glTexCoord2f(u1, 0.0); glVertex3f(0.0, y1, WALL_H);
      glTexCoord2f(u1, 1.0); glVertex3f(0.0, y1, 0.0);
    }
  }
  glEnd();
  polycount += 4 * WALL_SEGS;

  glDisable(GL_TEXTURE_2D);

  glDisable(GL_CULL_FACE);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_BLEND);
}

/*
void drawHelp(gDisplay *d) {
  rasonly(d);
  glColor4f(0.2, 0.2, 0.2, 0.8);
  glEnable(GL_BLEND);
  glBegin(GL_QUADS);
  glVertex2i(0,0);
  glVertex2i(d->vp_w - 1, 0);
  glVertex2i(d->vp_w - 1, d->vp_h - 1);
  glVertex2i(0, d->vp_h - 1);
  glEnd();
  if(game->settings->show_alpha != 1) glDisable(GL_BLEND);
  glColor3f(1.0, 1.0, 0.0);
  drawLines(d->vp_w, d->vp_h,
	    help, HELP_LINES, 0);
}
*/

void drawCam(Player *p, gDisplay *d) {
  int i;

  if (d->fog == 1) glEnable(GL_FOG);

  glColor3f(0.0, 1.0, 0.0);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(game->settings->fov, d->vp_w / d->vp_h, 3.0, GSIZE);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  {
    /* directional light (w=0): a 4-component position; passing
     * camera->cam (float[3]) directly reads one float past the
     * array, which turned the light positional and forced a
     * per-vertex sqrt in the lighting path */
    static GLfloat lpos[4];

    lpos[0] = p->camera->cam[0];
    lpos[1] = p->camera->cam[1];
    lpos[2] = p->camera->cam[2];
    lpos[3] = 0.0f;
    glLightfv(GL_LIGHT0, GL_POSITION, lpos);
  }

  gluLookAt(p->camera->cam[0], p->camera->cam[1], p->camera->cam[2],
	    p->camera->target[0], p->camera->target[1], p->camera->target[2],
	    0, 0, 1);
  glProfPhase(0);        /* setup/clear/perspective/lookat */

  drawFloor(d, p->camera->cam);
  glProfPhase(1);        /* floor */
  if(game->settings->show_wall == 1) {
    drawWalls(d);
    glProfPhase(2);      /* walls */
  }
  for(i = 0; i < game->players; i++)
    drawTraces(&(game->player[i]), d, i);
  glProfPhase(3);        /* trails */

  drawPlayers(p);
  glProfPhase(4);        /* bikes */

  /* draw the glow around the other players: */
  if(game->settings->show_glow == 1)
    for(i = 0; i < game->players; i++)
      if ((p != &(game->player[i])) && (game->player[i].data->speed > 0))
	drawGlow(&(game->player[i]), d, TRAIL_HEIGHT * 4);
  glProfPhase(5);        /* glow */


  /* highLight crashed player */

  /*
  if(p->data->speed < 0 && game->settings->erase_crashed == 0) {
    glPushMatrix();
    glColor4f(1.0, 1.0, 0.1, 0.1);
    glTranslatef(p->data->posx, p->data->posy, TRAIL_HEIGHT / 2);
    if(game->settings->show_alpha == 1) 
      glutSolidSphere(8.0, 10, 10);
    glColor4f(1.0, 1.0, 0.3, 1.0);
    glutWireSphere(8.0, 10, 10);
    glPopMatrix();
  }
  */

  glDisable(GL_FOG);
}

void drawAI(gDisplay *d) {
  char ai[] = "computer player";

  rasonly(d);
  glColor3f(1.0, 1.0, 1.0);
  drawText(d->vp_w / 4, 10, d->vp_w / (2 * strlen(ai)), ai);
  /* glRasterPos2i(100, 0); */
}

void drawPause(gDisplay *display) {
  char pause[] = "Game is paused";
  char winner[] = "Player %d wins";
  char buf[100];
  char *message;
  static float d = 0;
  static float lt = 0;
  float delta;
  long now;

  now = getElapsedTime();
  delta = now - lt;
  lt = now;
  delta /= 500.0;
  d += delta;
  /* printf("%.5f\n", delta); */
  
  if(d > 2 * M_PI) { 
    d -= 2 * M_PI;
  }

  if(game->pauseflag & PAUSE_GAME_FINISHED &&
     game->winner != -1) {
    message = buf;
    sprintf(message, winner, game->winner + 1);
  } else {
    message = pause;
  }

  rasonly(game->screen);
  glColor3f(1.0, (sin(d) + 1) / 2, (sin(d) + 1) / 2);
  drawText(display->vp_w / 6, 20, 
	   display->vp_w / (6.0 / 4.0 * strlen(message)), message);
}



