/*
 * minigl.h — A minimal OpenGL subset for PSXLinux.
 *
 * Implements exactly the fixed-function, immediate-mode API that
 * GLTron 0.53 uses: a modelview/projection matrix stack, immediate
 * mode primitive accumulation (glBegin/glEnd), one directional light,
 * linear fog, 2D texturing against the PSX VRAM, blending, and a
 * per-primitive ordering-table depth sort.  Vertices are transformed
 * in software (float via libsoftfp) and emitted as PSX GPU packets.
 *
 * Coordinate conventions follow OpenGL: right-handed, +Y up in NDC,
 * viewport origin bottom-left.  The emulation maps NDC onto the PSX
 * screen (origin top-left) internally.
 */

#ifndef _MINIGL_H
#define _MINIGL_H

typedef float  GLfloat;
typedef double GLdouble;
typedef int    GLint;
typedef unsigned int   GLuint;
typedef unsigned int   GLsizei;
typedef unsigned int   GLenum;
typedef unsigned int   GLbitfield;
typedef void   GLvoid;
typedef unsigned char   GLboolean;
typedef unsigned char   GLubyte;
typedef char   GLchar;

#define GL_FALSE 0
#define GL_TRUE  1

/* primitive types */
#define GL_POINTS         0x0000
#define GL_LINES          0x0001
#define GL_LINE_LOOP      0x0002
#define GL_LINE_STRIP     0x0003
#define GL_TRIANGLES      0x0004
#define GL_TRIANGLE_STRIP 0x0005
#define GL_TRIANGLE_FAN   0x0006
#define GL_QUADS          0x0007
#define GL_QUAD_STRIP     0x0008
#define GL_POLYGON        0x0009

/* matrix modes */
#define GL_MODELVIEW  0x1700
#define GL_PROJECTION 0x1701

/* enables */
#define GL_BLEND      0x0BE2
#define GL_CULL_FACE  0x0B44
#define GL_DEPTH_TEST 0x0B71
#define GL_FOG        0x0B60
#define GL_LIGHTING   0x0B50
#define GL_TEXTURE_2D 0x0DE1

/* clear bits */
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_DEPTH_BUFFER_BIT 0x00000100

/* blend factors */
#define GL_ZERO                0
#define GL_ONE                 1
#define GL_SRC_ALPHA           0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303

/* lighting */
#define GL_LIGHT0        0x4000
#define GL_AMBIENT       0x1200
#define GL_DIFFUSE       0x1201
#define GL_SPECULAR      0x1202
#define GL_POSITION      0x1203
#define GL_FRONT_AND_BACK 0x0408

/* fog */
#define GL_FOG_MODE    0x0B65
#define GL_FOG_DENSITY 0x0B66
#define GL_FOG_START   0x0B61
#define GL_FOG_END     0x0B63
#define GL_LINEAR       0x2601

/* shade model */
#define GL_FLAT   0x1D00
#define GL_SMOOTH 0x1D01

/* texture */
#define GL_TEXTURE_ENV       0x2300
#define GL_TEXTURE_ENV_MODE  0x2200
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S     0x2802
#define GL_TEXTURE_WRAP_T     0x2803
#define GL_MODULATE 0x2100
#define GL_REPLACE  0x1E01
#define GL_NEAREST  0x2600
#define GL_REPEAT   0x2901
#define GL_CLAMP    0x2900

/* pixel transfer */
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_RGB16            0x805B   /* internal: PSX 15bpp halfwords */
#define GL_RGBA             0x1908
#define GL_UNSIGNED_BYTE    0x1401

/* queries */
#define GL_NO_ERROR          0
#define GL_VENDOR            0x1F00
#define GL_RENDERER          0x1F01
#define GL_VERSION           0x1F02
#define GL_MODELVIEW_MATRIX  0x0BA6
#define GL_SHADE_MODEL       0x0B54

/* ------------------------------------------------------------------ GL */

void glBegin(GLenum mode);
void glEnd(void);

void glVertex2f(GLfloat x, GLfloat y);
void glVertex2i(GLint x, GLint y);
void glVertex3f(GLfloat x, GLfloat y, GLfloat z);
void glVertex3i(GLint x, GLint y, GLint z);
/* raw Q16.16 vertex: coordinates already in fixed point (used by
 * the pre-converted mesh paths; NOT world units like glVertex3i) */
void glVertex3q(GLint x, GLint y, GLint z);
void glVertex3fv(const GLfloat *v);
void glVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w);

void glColor3f(GLfloat r, GLfloat g, GLfloat b);
void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
void glColor3fv(const GLfloat *v);
void glColor4fv(const GLfloat *v);
void glColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a);

void glTexCoord2f(GLfloat u, GLfloat v);
/* float -> Q16.16, exposed for code that pre-converts static
 * geometry at build time (see model.c); same rounding as the
 * glVertex3f path so both paths agree bit for bit */
int glQ16(GLfloat f);

void glTexCoord2i(GLint u, GLint v);

void glNormal3f(GLfloat x, GLfloat y, GLfloat z);
void glNormal3fv(const GLfloat *v);

void glMatrixMode(GLenum mode);
void glPushMatrix(void);
void glPopMatrix(void);
void glLoadIdentity(void);
void glLoadMatrixf(const GLfloat *m);
void glTranslatef(GLfloat x, GLfloat y, GLfloat z);
void glRotatef(GLfloat deg, GLfloat x, GLfloat y, GLfloat z);
void glScalef(GLfloat x, GLfloat y, GLfloat z);
void glOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t,
	     GLdouble n, GLdouble f);

void glViewport(GLint x, GLint y, GLsizei w, GLsizei h);

void glEnable(GLenum cap);
void glDisable(GLenum cap);
void glBlendFunc(GLenum sfactor, GLenum dfactor);
void glShadeModel(GLenum mode);
void glLineWidth(GLfloat width);
void glDepthMask(GLboolean flag);
void glClear(GLbitfield mask);
void glClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a);

void glLightfv(GLenum light, GLenum pname, const GLfloat *params);
void glMaterialfv(GLenum face, GLenum pname, const GLfloat *params);
void glFogf(GLenum pname, GLfloat param);
void glFogfv(GLenum pname, const GLfloat *params);

void glGenTextures(GLsizei n, GLuint *textures);
void glDeleteTextures(GLsizei n, const GLuint *textures);
void glBindTexture(GLenum target, GLuint texture);
void glTexImage2D(GLenum target, GLint level, GLint internalformat,
		  GLsizei width, GLsizei height, GLint border,
		  GLenum format, GLenum type, const GLvoid *data);
/*
 * PSX-specific: upload a texture that is already PSX 15bpp halfwords
 * (as produced by tools/gltron_data.py).  The STP bit (bit 15) is
 * preserved so per-texel semi-transparency keeps working.
 */
void glTexImage2DPSX(GLuint texture, GLsizei width, GLsizei height,
		     const unsigned short *halfwords);
void glTexParameteri(GLenum target, GLenum pname, GLint param);
void glTexParameterf(GLenum target, GLenum pname, GLfloat param);
void glTexEnvi(GLenum target, GLenum pname, GLint param);
void glPixelStorei(GLenum pname, GLint param);

GLenum glGetError(void);
const GLubyte *glGetString(GLenum name);
void glGetFloatv(GLenum pname, GLfloat *params);
void glGetIntegerv(GLenum pname, GLint *params);
void glBitmap(GLsizei w, GLsizei h, GLfloat x0, GLfloat y0,
	      GLfloat xi, GLfloat yi, const GLubyte *bitmap);
void glRasterPos2f(GLfloat x, GLfloat y);
void glRasterPos2i(GLint x, GLint y);

/* ----------------------------------------------------------------- GLU */

void gluPerspective(GLdouble fovy, GLdouble aspect,
		    GLdouble zNear, GLdouble zFar);
void gluLookAt(GLdouble ex, GLdouble ey, GLdouble ez,
	       GLdouble cx, GLdouble cy, GLdouble cz,
	       GLdouble ux, GLdouble uy, GLdouble uz);

/* ------------------------------------------------- PSX-specific extras */

/* Force-finish all pending packets and wait for the GPU (VSync).  */
void glFinish(void);

#endif /* _MINIGL_H */
