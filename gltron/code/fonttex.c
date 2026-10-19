/*
 * fonttex.c — PSXLinux port of GLTron's textured font renderer.
 *
 * The xenotron font ships preprocessed as one 64x256 15bpp sheet
 * (tools/gltron_data.py): 8 columns x 16 rows of 8x16 cells, cell
 * index = char - 31, glyph pixels white with STP set (semi-
 * transparent) so text blends smoothly over the scene.
 */

#include "gltron.h"
#include "psxdata.h"

#define FONT_SHEET_W 64
#define FONT_SHEET_H 256
#define CELL_W 8
#define CELL_H 16
#define CELL_COLS 8

static fonttex the_font;
static int font_built;

fonttex *ftxLoadFont(char *filename)
{
  (void)filename;                     /* preprocessed; no file I/O */
  the_font.texwidth = FONT_SHEET_W;
  the_font.width = CELL_W;
  the_font.height = CELL_H;
  the_font.lower = 32;
  the_font.upper = 126;
  return &the_font;
}

void ftxUnloadFont(fonttex *ftx)
{
  (void)ftx;
}

void ftxEstablishTexture(fonttex *ftx, unsigned char setupMipmaps)
{
  (void)setupMipmaps;                 /* the PSX is nearest-only */
  glGenTextures(1, &ftx->texID);
  glTexImage2DPSX(ftx->texID, FONT_SHEET_W, FONT_SHEET_H, tex_font);
  glBindTexture(GL_TEXTURE_2D, ftx->texID);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  font_built = 1;
}

void ftxRenderString(fonttex *ftx, char *string, int len)
{
  int i;
  float cu = (float)CELL_W / (float)FONT_SHEET_W;   /* 0.125 */
  float cv = (float)CELL_H / (float)FONT_SHEET_H;   /* 0.0625 */

  if (!font_built)
    ftxEstablishTexture(ftx, 0);
  glBindTexture(GL_TEXTURE_2D, ftx->texID);

  for (i = 0; i < len; i++) {
    int index = string[i] - ftx->lower + 1;   /* matches generator */
    float cx, cy;

    if (index < 1 || index > 95)
      continue;                          /* unmapped character */

    cx = (float)(index % CELL_COLS) / (float)CELL_COLS;
    cy = (float)(index / CELL_COLS) * cv;

    glBegin(GL_QUADS);
    glTexCoord2f(cx,       1.0f - cy - cv); glVertex2f((float)i,       0.0f);
    glTexCoord2f(cx + cu,  1.0f - cy - cv); glVertex2f((float)i + 1.0f, 0.0f);
    glTexCoord2f(cx + cu,  1.0f - cy);      glVertex2f((float)i + 1.0f, 1.0f);
    glTexCoord2f(cx,       1.0f - cy);      glVertex2f((float)i,       1.0f);
    glEnd();
  }
}
