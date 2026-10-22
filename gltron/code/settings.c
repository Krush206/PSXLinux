/*
 * settings.c — PSXLinux port of GLTron's settings.
 *
 * The original parsed settings.txt + ~/.gltronrc; the port keeps the
 * original defaults verbatim and drops the file I/O (and the
 * settings_int/settings_float registry those files fed).
 */

#include "gltron.h"

void initSettingData(char *filename)
{
  (void)filename;                     /* no file-backed registry */
}

void initMainGameSettings(char *filename)
{
  int i;
  (void)filename;

  game = &main_game;
  game->settings = (Settings *)malloc(sizeof(Settings));

  game->pauseflag = 0;

  game->settings->show_help = 0;
  game->settings->show_fps = 1;
  game->settings->show_wall = 1;
  /* The PSX has no per-pixel alpha, so the additive glow fans can
   * only render at full rate - at 4x TRAIL_HEIGHT radius they
   * swallow the 8-unit bike into a saturated blob (fine on the PC,
   * where a perspective-correct soft halo stays subtle).  The
   * Oct-20-3am build the user references has only a faint halo;
   * without usable alpha the closest look is no glow disc at all.
   * 'g' still toggles it at runtime. */
  game->settings->show_glow = 0;
  game->settings->show_2d = 0;
  game->settings->show_alpha = 1;
  game->settings->show_floor_texture = 1;
  game->settings->show_crash_texture = 1;
  game->settings->show_model = 1;
  game->settings->turn_cycle = 1;
  game->settings->line_spacing = 20;
  game->settings->erase_crashed = 0;
  game->settings->fast_finish = 0;
  game->settings->fov = 105;
  game->settings->speed = 4.2f;
  game->settings->width = 320;        /* PSX screen */
  game->settings->height = 240;
  game->settings->show_ai_status = 1;
  game->settings->camType = 0;
  game->settings->mouse_warp = 0;

  game->settings->display_type = 0;
  game->settings->playSound = 0;      /* SPU stub */

  game->settings->ai_player1 = 0;
  game->settings->ai_player2 = 1;
  game->settings->ai_player3 = 1;
  game->settings->ai_player4 = 1;

  game->settings->sound_driver = 0;

  game->settings->screenSaver = 0;
  game->settings->windowMode = 0;
  for (i = 0; i < 4; i++)
    game->settings->content[i] = i;
}

void saveSettings(void)
{
  /* no persistent settings on the PSX */
}

int *getVi(char *name)
{
  (void)name;
  return 0;
}
