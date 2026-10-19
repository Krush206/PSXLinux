/*
 * texture.c — PSXLinux port of GLTron's texture setup.
 *
 * The SGI .rgb files ship preprocessed as PSX 15bpp halfwords
 * (tools/gltron_data.py): floor / wall / crash at 64x64, with the
 * crash texture carrying STP bits from the original alpha channel
 * so the explosion explosion blends per texel.  The GUI icon is not
 * used on the PSX (menus were dropped for the first playable port).
 */

#include "gltron.h"
#include "psxdata.h"

void deleteTextures(gDisplay *d) {
  glDeleteTextures(1, &(d->texFloor));
  glDeleteTextures(1, &(d->texWall));
  glDeleteTextures(1, &(d->texGui));
  glDeleteTextures(1, &(d->texCrash));
}

static void psx_texture(unsigned int *id, const unsigned short *hw,
			 int w, int h, int mode)
{
  glGenTextures(1, id);
  glBindTexture(GL_TEXTURE_2D, *id);
  glTexImage2DPSX(*id, w, h, hw);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, mode);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void initTexture(gDisplay *d) {
  /* floor (opaque, replaces the fragment colour) */
  psx_texture(&d->texFloor, tex_floor, 64, 64, GL_REPLACE);

  /* wall */
  psx_texture(&d->texWall, tex_wall, 64, 64, GL_REPLACE);

  /* crash explosion — STP bits make visible texels semi-transparent */
  psx_texture(&d->texCrash, tex_crash, 64, 64, GL_REPLACE);

  /* GUI icon unused (menus dropped in this port) */
  d->texGui = d->texFloor;
}
