// -*- Mode: C++; tab-width:2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi:tw=80:et:ts=2:sts=2
// -----------------------------------------------------------------------
// This file is part of RLVM, a RealLive virtual machine clone.
// -----------------------------------------------------------------------
// quad_batch.h – Lightweight immediate-mode quad renderer for OpenGL ES 3.2.
//
// Replaces glBegin/glEnd/glVertex2i/glTexCoord2f/glColor4ub with a VAO+VBO
// approach that is compatible with ES 3.2 core profile (no fixed-function).
//
// Usage:
//   QuadBatch::draw(prog, projection, tex0, tex1,
//                   x1,y1, x2,y2,          // destination rect (game coords)
//                   u0x1,u0y1, u0x2,u0y2,  // UV set 0 (primary texture)
//                   u1x1,u1y1, u1x2,u1y2,  // UV set 1 (secondary/mask)
//                   r,g,b,a);              // per-quad color (0..1)
// -----------------------------------------------------------------------

#ifndef SRC_SYSTEMS_SDL_QUAD_BATCH_H_
#define SRC_SYSTEMS_SDL_QUAD_BATCH_H_

#ifdef __ANDROID__
#include <GLES3/gl32.h>
#else
#include "GL/glew.h"
#endif

class QuadBatch {
 public:
  // Draws a single textured quad.
  //  prog        – shader program (already linked)
  //  projection  – pointer to column-major 4×4 projection matrix
  //  tex0        – primary texture (bound to unit 0)
  //  tex1        – secondary texture (bound to unit 1, 0 = none)
  //  dst*        – destination rectangle in game (pixel) coordinates
  //  uv0*        – UV coordinates for tex0
  //  uv1*        – UV coordinates for tex1 (unused if tex1==0)
  //  r,g,b,a     – vertex color (0.0–1.0)
  static void draw(GLuint prog,
                   const float* projection,
                   GLuint tex0, GLuint tex1,
                   float dx1, float dy1, float dx2, float dy2,
                   float u0x1, float u0y1, float u0x2, float u0y2,
                   float u1x1, float u1y1, float u1x2, float u1y2,
                   float r, float g, float b, float a);

  // Free the shared VAO/VBO.
  static void reset();

 private:
  static void ensureInit();

  static GLuint vao_;
  static GLuint vbo_;
  static GLuint white_tex_;   // 1×1 white RGBA texture for solid-colour draws
  static bool   initialized_;
};

#endif  // SRC_SYSTEMS_SDL_QUAD_BATCH_H_
