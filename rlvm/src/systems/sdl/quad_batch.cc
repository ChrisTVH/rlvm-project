// -*- Mode: C++; tab-width:2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi:tw=80:et:ts=2:sts=2
// -----------------------------------------------------------------------
// quad_batch.cc – implementation
// -----------------------------------------------------------------------

#ifdef __ANDROID__
#include <GLES3/gl32.h>
#else
#include "GL/glew.h"
#endif

#include <cstdint>
#include "systems/sdl/quad_batch.h"

// ---------------------------------------------------------------------------
// Vertex layout (stride = 12 floats = 48 bytes):
//   [0..1]  a_pos   (x, y)
//   [2..3]  a_uv0   (s, t)
//   [4..5]  a_uv1   (s, t)
//   [6..9]  a_color (r, g, b, a)
// ---------------------------------------------------------------------------

GLuint QuadBatch::vao_         = 0;
GLuint QuadBatch::vbo_         = 0;
GLuint QuadBatch::white_tex_   = 0;
bool   QuadBatch::initialized_ = false;

static const int kFloatsPerVertex = 10;   // pos(2)+uv0(2)+uv1(2)+color(4)
static const int kVerticesPerQuad = 4;
static const int kBufferFloats    = kFloatsPerVertex * kVerticesPerQuad;

void QuadBatch::ensureInit() {
  if (initialized_) return;

  glGenVertexArrays(1, &vao_);
  glBindVertexArray(vao_);

  glGenBuffers(1, &vbo_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  // Allocate for one quad; we'll update with glBufferSubData each draw call.
  glBufferData(GL_ARRAY_BUFFER,
               kBufferFloats * sizeof(float),
               nullptr, GL_DYNAMIC_DRAW);

  const GLsizei stride = kFloatsPerVertex * sizeof(float);
  // a_pos  (location 0)
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride,
                        (void*)(0 * sizeof(float)));
  // a_uv0  (location 1)
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride,
                        (void*)(2 * sizeof(float)));
  // a_uv1  (location 2)
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride,
                        (void*)(4 * sizeof(float)));
  // a_color (location 3)
  glEnableVertexAttribArray(3);
  glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride,
                        (void*)(6 * sizeof(float)));

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  // 1×1 opaque white texture used when no texture is needed (solid colour draws).
  glGenTextures(1, &white_tex_);
  glBindTexture(GL_TEXTURE_2D, white_tex_);
  const uint8_t white[4] = {255, 255, 255, 255};
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glBindTexture(GL_TEXTURE_2D, 0);

  initialized_ = true;
}

// static
void QuadBatch::reset() {
  if (vao_)       { glDeleteVertexArrays(1, &vao_);    vao_ = 0; }
  if (vbo_)       { glDeleteBuffers(1, &vbo_);         vbo_ = 0; }
  if (white_tex_) { glDeleteTextures(1, &white_tex_);  white_tex_ = 0; }
  initialized_ = false;
}

// static
void QuadBatch::draw(GLuint prog,
                     const float* projection,
                     GLuint tex0, GLuint tex1,
                     float dx1, float dy1, float dx2, float dy2,
                     float u0x1, float u0y1, float u0x2, float u0y2,
                     float u1x1, float u1y1, float u1x2, float u1y2,
                     float r, float g, float b, float a) {
  ensureInit();

  // Build 4 vertices: TL, TR, BR, BL (triangle-strip order)
  // Each vertex: pos(2) uv0(2) uv1(2) color(4)
  float verts[kBufferFloats] = {
    // TL
    dx1, dy1,  u0x1, u0y1,  u1x1, u1y1,  r, g, b, a,
    // TR
    dx2, dy1,  u0x2, u0y1,  u1x2, u1y1,  r, g, b, a,
    // BR
    dx2, dy2,  u0x2, u0y2,  u1x2, u1y2,  r, g, b, a,
    // BL
    dx1, dy2,  u0x1, u0y2,  u1x1, u1y2,  r, g, b, a,
  };

  glUseProgram(prog);

  // Upload projection matrix
  GLint projLoc = glGetUniformLocation(prog, "u_projection");
  if (projLoc >= 0)
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, projection);

  // Bind textures (use built-in 1×1 white when caller passes tex0==0)
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, tex0 ? tex0 : white_tex_);
  GLint tex0Loc = glGetUniformLocation(prog, "u_texture");
  if (tex0Loc < 0) tex0Loc = glGetUniformLocation(prog, "u_current");
  if (tex0Loc < 0) tex0Loc = glGetUniformLocation(prog, "u_image");
  if (tex0Loc >= 0) glUniform1i(tex0Loc, 0);

  if (tex1) {
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, tex1);
    GLint tex1Loc = glGetUniformLocation(prog, "u_mask");
    if (tex1Loc >= 0) glUniform1i(tex1Loc, 1);
  }

  // Upload vertex data
  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

  // Draw as triangle fan (4 vertices → 2 triangles covering the quad)
  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

  glBindVertexArray(0);
  glActiveTexture(GL_TEXTURE0);
  glUseProgram(0);
}
