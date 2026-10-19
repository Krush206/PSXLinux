#ifndef FONTTEX_H
#define FONTTEX_H

#include "../../psxdev/minilibc.h"
#include "../../psxdev/minilibc.h"

#include "minigl.h"

/*
 * PSXLinux port: the xenotron font ships preprocessed as a single
 * 64x256 15bpp texture (tools/gltron_data.py), holding 128 cells of
 * 8x16 texels.  ASCII 32..126 maps to cells 0..94.
 */

typedef struct fonttex {
  int texwidth;   /* texture width  (64)  */
  int width;      /* cell width     (8)   */
  int height;     /* cell height    (16)  */
  int lower;      /* lowest ascii character (32) */
  int upper;      /* highest ascii character (126) */
  unsigned int texID;
} fonttex;

extern fonttex *ftxLoadFont(char *filename);
extern void ftxUnloadFont(fonttex *ftx);
extern void ftxEstablishTexture(fonttex *ftx, unsigned char setupMipmaps);
extern void ftxRenderString(fonttex *ftx, char *string, int len);

#endif
