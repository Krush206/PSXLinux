/*
 * fonts.c — PSXLinux port: the xenotron font is preprocessed into
 * tex_font (see fonttex.c), so loading needs no filesystem.
 */

#include "gltron.h"

void initFonts(void)
{
  if (ftx != NULL)
    ftxUnloadFont(ftx);
  ftx = ftxLoadFont("xenotron.ftx");     /* name kept; data embedded */
  if (ftx == NULL)
    exit(1);
  ftxEstablishTexture(ftx, GL_TRUE);
}

void deleteFonts(void)
{
  if (ftx != NULL)
    ftxUnloadFont(ftx);
  ftx = NULL;
}
