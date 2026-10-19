/*
 * psxdata.h — Preprocessed GLTron assets for PSXLinux.
 *
 * The original game loads the lightcycle from t-u-low.obj at runtime
 * and the textures from SGI .rgb files.  On the PSX they are compiled
 * in instead: tools/gltron_data.py converted them once, offline:
 *
 *   psx_model_data.c — the model exactly as loadModel() produced it
 *                      (rescaled to CYCLE_HEIGHT, normals inverted,
 *                       split into MeshParts per material)
 *   psx_tex_data.c   — floor / wall / crash / font textures as
 *                      PSX 15bpp VRAM halfwords (STP from alpha)
 */

#ifndef PSXDATA_H
#define PSXDATA_H

#include "model.h"

/* textures (64x64, 64x64, 64x64, 64x256; 15bpp halfwords) */
extern const unsigned short tex_floor[64 * 64];
extern const unsigned short tex_wall[64 * 64];
extern const unsigned short tex_crash[64 * 64];
extern const unsigned short tex_font[64 * 256];

/* model materials */
extern const Material model_materials[];
extern const int model_nmaterials;
extern const float model_bbox[3];
extern const int model_nfaces_total;

/* per-material MeshParts: facesizes[], vertices[], normals[] */
extern const int model_fs_0[];  extern const float model_v_0[][4][3];
extern const float model_n_0[][4][3];
extern const int model_nf_0;
extern const int model_fs_1[];  extern const float model_v_1[][4][3];
extern const float model_n_1[][4][3];
extern const int model_nf_1;
extern const int model_fs_2[];  extern const float model_v_2[][4][3];
extern const float model_n_2[][4][3];
extern const int model_nf_2;
extern const int model_fs_3[];  extern const float model_v_3[][4][3];
extern const float model_n_3[][4][3];
extern const int model_nf_3;

/* build the game's Mesh from the preprocessed arrays (the
 * replacement for loadModel("t-u-low.obj", CYCLE_HEIGHT, 1)) */
Mesh *psxBuildCycleModel(void);

#endif
