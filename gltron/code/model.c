/*
 * model.c — PSXLinux port of GLTron's model handling.
 *
 * loadModel() read the Wavefront .obj at runtime; the model ships
 * preprocessed instead (see psxdata.h / tools/gltron_data.py) and
 * this file assembles it into the game's Mesh structure.  The
 * setMaterial* helpers are kept verbatim from the original.
 */

#include "gltron.h"
#include "psxdata.h"

static Mesh *cycle_mesh;

/* per-face jitter directions for the explosion path, folded into
 * faceoff_q once at build time (kept here so the float fallback in
 * drawExplosionPart shares the same table) */
const float exp_vectors[EXP_VECTORS][3] = {
  { 0.03, -0.06, -0.07 },
  { 0.04, 0.08, -0.03 },
  { 0.10, -0.04, -0.07 },
  { 0.06, -0.09, -0.10 },
  { -0.03, -0.05, 0.02 },
  { 0.07, 0.08, -0.00 },
  { 0.01, -0.04, 0.10 },
  { -0.01, -0.07, 0.09 },
  { 0.01, -0.01, -0.09 },
  { -0.04, 0.04, 0.02 }
};

Mesh *psxBuildCycleModel(void)
{
  Mesh *mesh;
  MeshPart *mp;
  int i, j, k, total;
  int nf[4];
  const int *fs[4];
  const float (*vv)[4][3];
  const float (*nn)[4][3];
  const float *vs[4];
  const float *ns[4];
  float *dstv, *dstn;
  int *dstvq, *dstoq;

  if (cycle_mesh)
    return cycle_mesh;                       /* built once, shared */

  fs[0] = model_fs_0;  vs[0] = &model_v_0[0][0][0];
  ns[0] = &model_n_0[0][0][0];
  fs[1] = model_fs_1;  vs[1] = &model_v_1[0][0][0];
  ns[1] = &model_n_1[0][0][0];
  fs[2] = model_fs_2;  vs[2] = &model_v_2[0][0][0];
  ns[2] = &model_n_2[0][0][0];
  fs[3] = model_fs_3;  vs[3] = &model_v_3[0][0][0];
  ns[3] = &model_n_3[0][0][0];
  nf[0] = model_nf_0; nf[1] = model_nf_1;
  nf[2] = model_nf_2; nf[3] = model_nf_3;

  mesh = (Mesh *)malloc(sizeof(Mesh));
  mesh->nFaces = model_nfaces_total;
  mesh->nMaterials = model_nmaterials;
  mesh->materials = (Material *)model_materials;
  mesh->meshparts = (MeshPart *)malloc(model_nmaterials *
				       sizeof(MeshPart));
  mesh->bbox[0] = model_bbox[0];
  mesh->bbox[1] = model_bbox[1];
  mesh->bbox[2] = model_bbox[2];

  for (i = 0; i < model_nmaterials; i++) {
    mp = &mesh->meshparts[i];
    mp->nFaces = nf[i];
    mp->facesizes = (int *)fs[i];
    total = nf[i] * MODEL_FACESIZE * 3;
    dstv = (float *)malloc(total * sizeof(float));
    dstn = (float *)malloc(total * sizeof(float));
    /* fixed-point mirrors for drawExplosionPart's hot path (see
     * model.h) — one-time cost at boot, no soft-float per frame */
    dstvq = (int *)malloc(total * sizeof(int));
    dstoq = (int *)malloc(nf[i] * 3 * sizeof(int));
    vv = (const float (*)[4][3])vs[i];
    nn = (const float (*)[4][3])ns[i];
    for (j = 0; j < nf[i]; j++) {
      /* per-face explosion offset: the face normal plus this
       * face's jitter direction, pre-converted to Q16.16 */
      for (k = 0; k < 3; k++)
	dstoq[3 * j + k] =
	  glQ16(nn[j][0][k] + exp_vectors[j % EXP_VECTORS][k]);
      for (k = 0; k < MODEL_FACESIZE; k++) {
	int o = 3 * (j * MODEL_FACESIZE + k);

	dstv[o + 0] = vv[j][k][0];
	dstv[o + 1] = vv[j][k][1];
	dstv[o + 2] = vv[j][k][2];
	dstn[o + 0] = nn[j][k][0];
	dstn[o + 1] = nn[j][k][1];
	dstn[o + 2] = nn[j][k][2];
	dstvq[o + 0] = glQ16(vv[j][k][0]);
	dstvq[o + 1] = glQ16(vv[j][k][1]);
	dstvq[o + 2] = glQ16(vv[j][k][2]);
      }
    }
    mp->vertices = dstv;
    mp->normals = dstn;
    mp->vertices_q = dstvq;
    mp->faceoff_q = dstoq;
  }

  cycle_mesh = mesh;
  return mesh;
}

/* ---------------- material helpers (from the original model.c) */

void setMaterialAmbient(Mesh *mesh, int material, float color[4]) {
  int i;
  for(i = 0; i < 4; i++)
    (mesh->materials + material)->ambient[i] = color[i];
}

void setMaterialDiffuse(Mesh *mesh, int material, float color[4]) {
  int i;
  for(i = 0; i < 4; i++)
    (mesh->materials + material)->diffuse[i] = color[i];
}

void setMaterialSpecular(Mesh *mesh, int material, float color[4]) {
  int i;
  for(i = 0; i < 4; i++)
    (mesh->materials + material)->specular[i] = color[i];
}

void setMaterialAlphas(Mesh *mesh, float alpha) {
  int i;
  /* vertex alpha is the alpha of the diffuse material component */
  for(i = 0; i < mesh->nMaterials; i++)
    (mesh->materials + i)->diffuse[3] = alpha;
}

void unloadModel(Mesh *mesh) {
  int i;
  for(i = 0; i < mesh->nMaterials; i++) {
    free(mesh->meshparts[i].vertices);
    free(mesh->meshparts[i].normals);
  }
  free(mesh->meshparts);
  free(mesh);
  cycle_mesh = NULL;
}
