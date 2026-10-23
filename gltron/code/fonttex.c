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
  int in_batch = 0;

  if (!font_built)
    ftxEstablishTexture(ftx, 0);
  glBindTexture(GL_TEXTURE_2D, ftx->texID);

  /* Batched and fully integer: one glBegin per 20 characters
   * (80 verts, inside minigl's 96-vertex buffer) instead of one
   * glBegin/glEnd pair per character, and the cell coordinates
   * come out of the glyph index with shifts - 8 columns of
   * 1/8 sheet width (32 units in minigl's Q8 texcoords) by 16
   * rows of 1/16 sheet height (16 units).  The float version
   * paid a soft-float divide per character and a GPU packet per
   * character. */
  for (i = 0; i < len; i++) {
    int index = string[i] - ftx->lower + 1;   /* matches generator */
    int gx, gy, u0, u1, v0, v1;

    if (index < 1 || index > 95)
      continue;                          /* unmapped character */

    if ((i % 20) == 0) {
      if (in_batch)
	glEnd();
      glBegin(GL_QUADS);
      in_batch = 1;
    }

    gx = index % CELL_COLS;
    gy = index / CELL_COLS;
    u0 = gx << 5; u1 = (gx + 1) << 5;    /* 32 Q8 units per cell */
    v0 = gy << 4; v1 = (gy + 1) << 4;    /* 16 Q8 units per cell */

    /* No V flip: glTexImage2DPSX uploads the sheet's first array
     * row to VRAM v=0, so texture v grows downward through the
     * sheet exactly as the generator laid the cells out.  (The
     * 1.0-cy flip from the GL bottom-up convention sampled
     * mirrored, mostly empty cells - garbled glyphs.)  The quad's
     * bottom (y=0) takes the cell's bottom row (v1), the top
     * (y=1) the cell's top row (v0). */
    glTexCoord2i(u0, v1); glVertex2i(i, 0);
    glTexCoord2i(u1, v1); glVertex2i(i + 1, 0);
    glTexCoord2i(u1, v0); glVertex2i(i + 1, 1);
    glTexCoord2i(u0, v0); glVertex2i(i, 1);
  }
  if (in_batch)
    glEnd();
}
