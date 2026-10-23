#include "gltron.h"

/*
 * Faces per GL_TRIANGLES batch.  The LOD cycle model is all
 * triangles; running them through one glBegin/glEnd pair per mesh
 * part (instead of per face) removes most of the per-face
 * bookkeeping.  30 faces = 90 vertices, inside minigl's 96-vertex
 * buffer.
 */
#define MESH_BATCH_FACES 30

void drawMeshPart(MeshPart* meshpart, int flag) {
  int i, j;
  int type = 0, c;
  int run = 0;
  float *normal;
  float *vertex;

  for(i = 0; i < meshpart->nFaces; i++) {
    c = *(meshpart->facesizes + i);
    if(flag & 1) {
      /* wireframe overlay: per-face line loops */
      glBegin(GL_LINE_LOOP);
      for(j = 0; j < c; j++) {
	normal = meshpart->normals + 3 * (i * MODEL_FACESIZE + j);
	glNormal3fv(normal);
	vertex = meshpart->vertices + 3 * (i * MODEL_FACESIZE + j);
	glVertex3fv(vertex);
      }
      glEnd();
      continue;
    }
    if(c == 3) {
      /* batch consecutive triangles */
      if(type != GL_TRIANGLES) {
	if(type != 0) glEnd();
	glBegin(GL_TRIANGLES);
	type = GL_TRIANGLES;
	run = 0;
      } else if(run == MESH_BATCH_FACES) {
	glEnd();
	glBegin(GL_TRIANGLES);
	run = 0;
      }
    } else {
      if(type != 0) { glEnd(); type = 0; }
      type = (c == 4) ? GL_QUADS : GL_POLYGON;
      glBegin(type);
    }
    for(j = 0; j < c; j++) {
      normal = meshpart->normals + 3 * (i * MODEL_FACESIZE + j);
      glNormal3fv(normal);
      vertex = meshpart->vertices + 3 * (i * MODEL_FACESIZE + j);
      glVertex3fv(vertex);
    }
    if(c == 3) {
      run++;
    } else {
      glEnd();
      type = 0;
    }
  }
  if(type != 0) glEnd();
}

void drawExplosionPart(MeshPart* meshpart, float radius, int flag) {
  int i, j;
  int type = 0, c;
  int run = 0;
  int rq, ox, oy, oz;
  float *normal;
  float *vertex;
  const int *vq = meshpart->vertices_q;
  const int *foq = meshpart->faceoff_q;

  rq = glQ16(radius);

  for(i = 0; i < meshpart->nFaces; i++) {
    c = *(meshpart->facesizes + i);
    if(c == 3 && !(flag & 1)) {
      /*
       * Batched path (the LOD model is all triangles): the per-face
       * explosion offset is folded into the vertices instead of a
       * glPushMatrix/glTranslate/glPopMatrix per face, and the faces
       * share one GL_TRIANGLES batch per MESH_BATCH_FACES faces.
       *
       * Fixed point: vertices_q and the per-face offset direction
       * (face normal + jitter, folded at model build — see model.c)
       * are Q16.16, so scaling by the radius is one 64-bit
       * multiply per axis per face and the per-vertex offset is an
       * integer add.  The float version did a soft-float chain per
       * component per face and per vertex - the single most
       * expensive stretch of guest code during the crash cascade.
       */
      if(run == MESH_BATCH_FACES) {
	glEnd();
	run = 0;
      }
      if(run == 0)
	glBegin(GL_TRIANGLES);
      ox = (int)(((long long)rq * foq[3 * i + 0]) >> 16);
      oy = (int)(((long long)rq * foq[3 * i + 1]) >> 16);
      oz = (int)(((long long)rq * foq[3 * i + 2]) >> 16);
      for(j = 0; j < c; j++) {
	int o = 3 * (i * MODEL_FACESIZE + j);
	normal = meshpart->normals + o;
	glNormal3fv(normal);
	glVertex3q(vq[o + 0] + ox, vq[o + 1] + oy, vq[o + 2] + oz);
      }
      run++;
      continue;
    }
    if(run) { glEnd(); run = 0; }
    c = *(meshpart->facesizes + i);
    type = 0;
    if(c > 4) type = GL_POLYGON;
    if(c == 4) type = GL_QUADS;
    if(c == 3) type = GL_TRIANGLES;
    if(flag & 1) type = GL_LINE_LOOP;
    if(type != 0) {
      glPushMatrix();
      normal = meshpart->normals + 3 * (i * MODEL_FACESIZE);
      glTranslatef(radius * (*(normal + 0) + exp_vectors[i % EXP_VECTORS][0]),
		   radius * (*(normal + 1) + exp_vectors[i % EXP_VECTORS][1]),
		   radius * (*(normal + 2) + exp_vectors[i % EXP_VECTORS][2]) );

      glBegin(type);
      for(j = 0; j < c; j++) {
	normal = meshpart->normals + 3 * (i * MODEL_FACESIZE + j);
	glNormal3fv(normal);
	vertex = meshpart->vertices + 3 * (i * MODEL_FACESIZE + j);
	glVertex3fv(vertex);
      }
      glEnd();
      glPopMatrix();
    }
  }
  if(run) glEnd();
}

void printColor(float *values, int count) {
  int i;
  printf("color: ");
  for(i = 0; i < count; i++)
    printf("%.2f", *(values + i));
  printf("\n");
}


void drawModel(Mesh *mesh, int mode, int flag) {
  int i;

  for(i = 0; i < mesh->nMaterials; i++) {
    /* set materials */
    if(mode & MODEL_USE_MATERIAL) {

      glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT,
		   (mesh->materials + i)->ambient);

      glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE,
		   (mesh->materials + i)->diffuse);

      glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR,
		   (mesh->materials + i)->specular);
    }
    drawMeshPart(mesh->meshparts + i, flag);
  }
}

void drawExplosion(Mesh *mesh, float radius, int mode, int flag) {
  int i;
  /* printf("radius: %.2f\n", radius); */
  for(i = 0; i < mesh->nMaterials; i++) {
      glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT,
		   (mesh->materials + i)->ambient);

      glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE,
		   (mesh->materials + i)->diffuse);

      glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR,
		   (mesh->materials + i)->specular);
      drawExplosionPart(mesh->meshparts + i, radius, flag);
  }
}
