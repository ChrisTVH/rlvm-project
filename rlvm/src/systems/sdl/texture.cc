// -*- Mode: C++; tab-width:2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi:tw=80:et:ts=2:sts=2
// -----------------------------------------------------------------------
// This file is part of RLVM, a RealLive virtual machine clone.
// -----------------------------------------------------------------------
// Copyright (C) 2006, 2007 Elliot Glaysher
// Licensed under GPLv3+. See COPYING for details.
// -----------------------------------------------------------------------
// Rewritten for OpenGL ES 3.2 / core profile.
// All legacy fixed-function calls (glBegin, glEnd, glVertex2i, glColor4ub,
// glTexCoord2f, glMatrixMode, glOrtho, glTranslatef, glPushMatrix, etc.)
// replaced with VAO/VBO + GLSL shaders via QuadBatch.
// ARB extension functions replaced with standard GL 3.3 / ES 3.2 core.
// -----------------------------------------------------------------------

#ifdef __ANDROID__
#include <GLES3/gl32.h>
#include <android/log.h>
#define LOG_TAG "RLVM-TEX"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#else
#include "GL/glew.h"
#define LOGI(...) ((void)0)
#endif

#include <SDL2/SDL.h>
#ifndef __ANDROID__
#include <SDL2/SDL_opengl.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>

#include "pygame/alphablit.h"
#include "systems/base/colour.h"
#include "systems/base/graphics_object.h"
#include "systems/base/graphics_object_data.h"
#include "systems/base/system_error.h"
#include "systems/sdl/gl_ortho.h"
#include "systems/sdl/quad_batch.h"
#include "systems/sdl/sdl_graphics_system.h"
#include "systems/sdl/sdl_surface.h"
#include "systems/sdl/sdl_utils.h"
#include "systems/sdl/shaders.h"
#include "systems/sdl/texture.h"

// ---------------------------------------------------------------------------
// Static members
// ---------------------------------------------------------------------------

unsigned int Texture::s_screen_width  = 0;
unsigned int Texture::s_screen_height = 0;
int   Texture::s_viewport_x      = 0;
int   Texture::s_viewport_y      = 0;
int   Texture::s_viewport_width  = 0;
int   Texture::s_viewport_height = 0;
int   Texture::s_window_width    = 0;
int   Texture::s_window_height   = 0;
float Texture::s_capture_scale_x = 1.0f;
float Texture::s_capture_scale_y = 1.0f;

// Current orthographic projection matrix (set by SDLGraphicsSystem::BeginFrame).
float Texture::s_projection[16] = {
  1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1
};

unsigned int Texture::s_upload_buffer_size = 0;
std::unique_ptr<char[]> Texture::s_upload_buffer;

// ---------------------------------------------------------------------------

void Texture::SetScreenSize(const Size& s) {
  s_screen_width  = s.width();
  s_screen_height = s.height();
}

void Texture::SetViewportOffset(int x, int y) {
  s_viewport_x = x;
  s_viewport_y = y;
}

void Texture::SetViewportSize(int w, int h) {
  s_viewport_width  = w;
  s_viewport_height = h;
  s_window_width    = w + 2 * s_viewport_x;
  s_window_height   = h + 2 * s_viewport_y;
}

void Texture::SetProjection(float w, float h, float tx, float ty) {
  buildOrtho(s_projection, w, h);
  if (tx != 0.0f || ty != 0.0f)
    applyTranslation(s_projection, tx, ty);
}

int Texture::ScreenHeight() { return s_screen_height; }

// ---------------------------------------------------------------------------
// Texture – SDL_Surface upload constructor
// ---------------------------------------------------------------------------

Texture::Texture(SDL_Surface* surface,
                 int x, int y, int w, int h,
                 unsigned int bytes_per_pixel,
                 int byte_order, int byte_type)
    : x_offset_(x), y_offset_(y),
      logical_width_(w), logical_height_(h),
      total_width_(surface->w), total_height_(surface->h),
      texture_width_(SafeSize(w)), texture_height_(SafeSize(h)),
      back_texture_id_(0),
      back_tex_w_(0), back_tex_h_(0),
      is_upside_down_(false) {
  glGenTextures(1, &texture_id_);
  CheckGLErrors("glGenTextures");
  glBindTexture(GL_TEXTURE_2D, texture_id_);
  CheckGLErrors("glBindTexture");
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  CheckGLErrors("glTexParameteri WRAP_S");
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  CheckGLErrors("glTexParameteri WRAP_T");
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  CheckGLErrors("glTexParameteri MIN_FILTER");
  // Mask textures need GL_LINEAR for smooth edge feathering.
  // The alpha channel of the waku image contains soft gradient data
  // at the borders that must be interpolated, not nearest-sampled.
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  CheckGLErrors("glTexParameteri MAG_FILTER");

  if (w == total_width_ && h == total_height_) {
    SDL_LockSurface(surface);
    glTexImage2D(GL_TEXTURE_2D, 0, bytes_per_pixel,
                 texture_width_, texture_height_,
                 0, byte_order, byte_type, nullptr);
    CheckGLErrors("glTexImage2D full");
    // Account for surface pitch (may include padding)
    int row_length = surface->pitch / surface->format->BytesPerPixel;
    glPixelStorei(GL_UNPACK_ROW_LENGTH, row_length);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                    surface->w, surface->h, byte_order, byte_type,
                    surface->pixels);
    CheckGLErrors("glTexSubImage2D full");
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    SDL_UnlockSurface(surface);
  } else {
    // Cut out the sub-region
    char* pixel_data =
        uploadBuffer(surface->format->BytesPerPixel * w * h);
    char* cur_dst = pixel_data;

    SDL_LockSurface(surface);
    {
      const char* cur_src = static_cast<const char*>(surface->pixels)
                            + surface->pitch * y;
      int row_start   = surface->format->BytesPerPixel * x;
      int subrow_size = surface->format->BytesPerPixel * w;
      for (int row = 0; row < h; ++row) {
        memcpy(cur_dst, cur_src + row_start, subrow_size);
        cur_dst += subrow_size;
        cur_src += surface->pitch;
      }
    }
    SDL_UnlockSurface(surface);

    glTexImage2D(GL_TEXTURE_2D, 0, bytes_per_pixel,
                 texture_width_, texture_height_,
                 0, byte_order, byte_type, nullptr);
    CheckGLErrors("glTexImage2D sub");
    // Data is tightly packed (we copied it ourselves)
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h,
                    byte_order, byte_type, pixel_data);
    CheckGLErrors("glTexSubImage2D sub");
  }

  DebugShowGLErrors();
}

// ---------------------------------------------------------------------------
// Texture – render-to-texture (framebuffer capture) constructor
// ---------------------------------------------------------------------------

Texture::Texture(render_to_texture, int width, int height)
    : x_offset_(0), y_offset_(0),
      logical_width_(width), logical_height_(height),
      total_width_(width), total_height_(height),
      texture_width_(0), texture_height_(0),
      texture_id_(0), back_texture_id_(0),
      back_tex_w_(0), back_tex_h_(0),
      is_upside_down_(true) {
  glGenTextures(1, &texture_id_);
  glBindTexture(GL_TEXTURE_2D, texture_id_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  // Compute UV scale factors so RenderToScreen maps game coords → full capture.
  s_capture_scale_x = (s_viewport_width  > 0)
      ? float(s_viewport_width)  / float(logical_width_)  : 1.0f;
  s_capture_scale_y = (s_viewport_height > 0)
      ? float(s_viewport_height) / float(logical_height_) : 1.0f;

  // Allocate texture at viewport size (full-resolution capture).
  int tex_w = (s_viewport_width  > 0) ? SafeSize(s_viewport_width)  : SafeSize(width);
  int tex_h = (s_viewport_height > 0) ? SafeSize(s_viewport_height) : SafeSize(height);
  texture_width_  = tex_w;
  texture_height_ = tex_h;

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_w, tex_h, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

  // Copy from viewport origin so we skip letterbox/pillarbox bars.
  int capture_w = (s_viewport_width  > 0) ? s_viewport_width  : width;
  int capture_h = (s_viewport_height > 0) ? s_viewport_height : height;
  glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                      s_viewport_x, s_viewport_y,
                      capture_w, capture_h);
  DebugShowGLErrors();
}

// ---------------------------------------------------------------------------

Texture::~Texture() {
  glDeleteTextures(1, &texture_id_);
  if (back_texture_id_)
    glDeleteTextures(1, &back_texture_id_);
}

// ---------------------------------------------------------------------------

char* Texture::uploadBuffer(unsigned int size) {
  if (!s_upload_buffer || size > s_upload_buffer_size) {
    s_upload_buffer.reset(new char[size]);
    s_upload_buffer_size = size;
  }
  return s_upload_buffer.get();
}

// ---------------------------------------------------------------------------

void Texture::reupload(SDL_Surface* surface,
                       int offset_x, int offset_y,
                       int x, int y, int w, int h,
                       unsigned int bytes_per_pixel,
                       int byte_order, int byte_type) {
  glBindTexture(GL_TEXTURE_2D, texture_id_);

  if (w == total_width_ && h == total_height_) {
    SDL_LockSurface(surface);
    // Account for surface pitch (may include padding)
    int row_length = surface->pitch / surface->format->BytesPerPixel;
    glPixelStorei(GL_UNPACK_ROW_LENGTH, row_length);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                    surface->w, surface->h, byte_order, byte_type,
                    surface->pixels);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    SDL_UnlockSurface(surface);
  } else {
    char* pixel_data =
        uploadBuffer(surface->format->BytesPerPixel * w * h);
    char* cur_dst = pixel_data;

    SDL_LockSurface(surface);
    {
      const char* cur_src = static_cast<const char*>(surface->pixels)
                            + surface->pitch * y;
      int row_start   = surface->format->BytesPerPixel * x;
      int subrow_size = surface->format->BytesPerPixel * w;
      for (int row = 0; row < h; ++row) {
        memcpy(cur_dst, cur_src + row_start, subrow_size);
        cur_dst += subrow_size;
        cur_src += surface->pitch;
      }
    }
    SDL_UnlockSurface(surface);

    glTexSubImage2D(GL_TEXTURE_2D, 0, offset_x, offset_y, w, h,
                    byte_order, byte_type, pixel_data);
  }

  DebugShowGLErrors();
}

// ---------------------------------------------------------------------------
// Helper: compute UV coords for a texture region
// ---------------------------------------------------------------------------

static void computeUVs(int x1, int y1, int x2, int y2,
                       unsigned int tw, unsigned int th,
                       bool upside_down,
                       float scale_x, float scale_y,
                       int logical_h,
                       float& u1, float& v1, float& u2, float& v2) {
  if (upside_down) {
    // Framebuffer-captured texture: scale game coords to captured pixel count.
    u1 = float(x1) * scale_x / float(tw);
    u2 = float(x2) * scale_x / float(tw);
    v1 = float(logical_h - y1) * scale_y / float(th);
    v2 = float(logical_h - y2) * scale_y / float(th);
  } else {
    u1 = float(x1) / float(tw);
    v1 = float(y1) / float(th);
    u2 = float(x2) / float(tw);
    v2 = float(y2) / float(th);
  }
}

// ---------------------------------------------------------------------------
// RenderToScreen – plain textured quad with uniform opacity
// ---------------------------------------------------------------------------

void Texture::RenderToScreen(const Rect& src, const Rect& dst, int opacity) {
  int x1 = src.x(), y1 = src.y(), x2 = src.x2(), y2 = src.y2();
  int fdx1 = dst.x(), fdy1 = dst.y(), fdx2 = dst.x2(), fdy2 = dst.y2();
  if (!filterCoords(x1, y1, x2, y2, fdx1, fdy1, fdx2, fdy2))
    return;

  float u1, v1, u2, v2;
  computeUVs(x1, y1, x2, y2, texture_width_, texture_height_,
             is_upside_down_, s_capture_scale_x, s_capture_scale_y,
             logical_height_, u1, v1, u2, v2);

  float a = opacity / 255.0f;

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  QuadBatch::draw(Shaders::GetSpriteProgram(), s_projection,
                  texture_id_, 0,
                  float(fdx1), float(fdy1), float(fdx2), float(fdy2),
                  u1, v1, u2, v2,
                  0.0f, 0.0f, 0.0f, 0.0f,   // uv1 unused
                  1.0f, 1.0f, 1.0f, a);

  glBlendFunc(GL_ONE, GL_ZERO);
}

// ---------------------------------------------------------------------------
// RenderToScreen – per-vertex opacity (for effects)
// ---------------------------------------------------------------------------

void Texture::RenderToScreen(const Rect& src, const Rect& dst,
                             const int opacity[4]) {
  int x1 = src.x(), y1 = src.y(), x2 = src.x2(), y2 = src.y2();
  int fdx1 = dst.x(), fdy1 = dst.y(), fdx2 = dst.x2(), fdy2 = dst.y2();
  if (!filterCoords(x1, y1, x2, y2, fdx1, fdy1, fdx2, fdy2))
    return;

  float u1 = float(x1) / texture_width_;
  float v1 = float(y1) / texture_height_;
  float u2 = float(x2) / texture_width_;
  float v2 = float(y2) / texture_height_;

  // Average opacity for the single-color path (per-vertex opacity would need
  // 4 separate draw calls; the average is a reasonable approximation).
  float a = (opacity[0] + opacity[1] + opacity[2] + opacity[3]) / (4.0f * 255.0f);

  bool needs_blend = (a < 1.0f);
  if (needs_blend) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  }

  QuadBatch::draw(Shaders::GetSpriteProgram(), s_projection,
                  texture_id_, 0,
                  float(fdx1), float(fdy1), float(fdx2), float(fdy2),
                  u1, v1, u2, v2,
                  0.0f, 0.0f, 0.0f, 0.0f,
                  1.0f, 1.0f, 1.0f, a);

  glBlendFunc(GL_ONE, GL_ZERO);
}

// ---------------------------------------------------------------------------
// RenderToScreenAsColorMask – text-box overlay effect
// ---------------------------------------------------------------------------

void Texture::RenderToScreenAsColorMask(const Rect& src, const Rect& dst,
                                        const RGBAColour& rgba, int filter) {
  if (filter == 0) {
    render_to_screen_as_colour_mask_subtractive(src, dst, rgba);
  } else {
    render_to_screen_as_colour_mask_additive(src, dst, rgba);
  }
}

// ---------------------------------------------------------------------------
// Subtractive color mask (filter == 0) – the text box "frosted glass" effect.
//
// Algorithm (matches original kColorMaskShader):
//   result = clamp(bg - mask_factor + colour * mask_factor, 0, 1)
//   where mask_factor = clamp(mask.a * colour.a, 0, 1)
//
// Requires reading back the framebuffer under the text box into back_texture.
// ---------------------------------------------------------------------------

void Texture::render_to_screen_as_colour_mask_subtractive(
    const Rect& src, const Rect& dst, const RGBAColour& rgba) {
  int x1 = src.x(), y1 = src.y(), x2 = src.x2(), y2 = src.y2();
  int fdx1 = dst.x(), fdy1 = dst.y(), fdx2 = dst.x2(), fdy2 = dst.y2();
  if (!filterCoords(x1, y1, x2, y2, fdx1, fdy1, fdx2, fdy2))
    return;

  // UV for the mask texture (this texture = shape mask)
  float mx1 = float(x1) / texture_width_;
  float my1 = float(y1) / texture_height_;
  float mx2 = float(x2) / texture_width_;
  float my2 = float(y2) / texture_height_;
  if (is_upside_down_) {
    my1 = float(logical_height_ - y1) * s_capture_scale_y / texture_height_;
    my2 = float(logical_height_ - y2) * s_capture_scale_y / texture_height_;
  }

  // Capture the framebuffer region behind the text box.
  // Map game coordinates → window framebuffer coordinates.
  float scale_x = (s_viewport_width  > 0)
      ? float(s_viewport_width)  / float(s_screen_width)  : 1.0f;
  float scale_y = (s_viewport_height > 0)
      ? float(s_viewport_height) / float(s_screen_height) : 1.0f;

  int fb_x = s_viewport_x + int(fdx1 * scale_x);
  int fb_y = s_viewport_y + int((s_screen_height - fdy2) * scale_y);
  int cap_w = int((fdx2 - fdx1) * scale_x);
  int cap_h = int((fdy2 - fdy1) * scale_y);

  // Allocate or resize back_texture to fit the capture.
  // We track the allocated size internally (back_tex_w_/h_) to avoid
  // glGetTexLevelParameteriv which is not available in OpenGL ES 3.2.
  int needed_w = SafeSize(cap_w);
  int needed_h = SafeSize(cap_h);

  if (back_texture_id_ == 0) {
    glGenTextures(1, &back_texture_id_);
    glBindTexture(GL_TEXTURE_2D, back_texture_id_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, needed_w, needed_h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    back_tex_w_ = needed_w;
    back_tex_h_ = needed_h;
  } else {
    glBindTexture(GL_TEXTURE_2D, back_texture_id_);
    if (needed_w > back_tex_w_ || needed_h > back_tex_h_) {
      int alloc_w = std::max(needed_w, back_tex_w_);
      int alloc_h = std::max(needed_h, back_tex_h_);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, alloc_w, alloc_h, 0,
                   GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
      back_tex_w_ = alloc_w;
      back_tex_h_ = alloc_h;
    }
  }

  glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                      fb_x, fb_y, cap_w, cap_h);

  // UV for back_texture: map only the captured region, not SafeSize padding.
  float bx2 = float(cap_w) / float(back_tex_w_);
  float by2 = float(cap_h) / float(back_tex_h_);

  // Set up the color-mask shader program.
  GLuint prog = Shaders::getColorMaskProgram();
  glUseProgram(prog);

  // Projection
  GLint projLoc = Shaders::getColorMaskUniformProjection();
  if (projLoc >= 0)
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, s_projection);

  // Texture units
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, back_texture_id_);
  GLint curLoc = Shaders::getColorMaskUniformCurrentValues();
  if (curLoc >= 0) glUniform1i(curLoc, 0);

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, texture_id_);
  GLint maskLoc = Shaders::getColorMaskUniformMask();
  if (maskLoc >= 0) glUniform1i(maskLoc, 1);

  glDisable(GL_BLEND);

  // Draw the quad. The window colour is passed as vertex color (a_color).
  // The shader reads it as v_color.
  float r = rgba.r() / 255.0f;
  float g = rgba.g() / 255.0f;
  float b = rgba.b() / 255.0f;
  float a = rgba.a() / 255.0f;

  // Build and draw manually (need two UV sets)
  QuadBatch::draw(prog, s_projection,
                  back_texture_id_, texture_id_,
                  float(fdx1), float(fdy1), float(fdx2), float(fdy2),
                  // uv0: screen capture UVs (y-flipped: capture is bottom-up)
                  0.0f, by2, bx2, 0.0f,
                  // uv1: mask UVs
                  mx1, my1, mx2, my2,
                  r, g, b, a);

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, 0);
  glActiveTexture(GL_TEXTURE0);
  glUseProgram(0);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ZERO);

  DebugShowGLErrors();
}

// ---------------------------------------------------------------------------
// Additive color mask (filter != 0)
// ---------------------------------------------------------------------------

void Texture::render_to_screen_as_colour_mask_additive(
    const Rect& src, const Rect& dst, const RGBAColour& rgba) {
  int x1 = src.x(), y1 = src.y(), x2 = src.x2(), y2 = src.y2();
  int fdx1 = dst.x(), fdy1 = dst.y(), fdx2 = dst.x2(), fdy2 = dst.y2();
  if (!filterCoords(x1, y1, x2, y2, fdx1, fdy1, fdx2, fdy2))
    return;

  float u1 = float(x1) / texture_width_;
  float v1 = float(y1) / texture_height_;
  float u2 = float(x2) / texture_width_;
  float v2 = float(y2) / texture_height_;
  if (is_upside_down_) {
    v1 = float(logical_height_ - y1) * s_capture_scale_y / texture_height_;
    v2 = float(logical_height_ - y2) * s_capture_scale_y / texture_height_;
  }

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  QuadBatch::draw(Shaders::GetSpriteProgram(), s_projection,
                  texture_id_, 0,
                  float(fdx1), float(fdy1), float(fdx2), float(fdy2),
                  u1, v1, u2, v2,
                  0.0f, 0.0f, 0.0f, 0.0f,
                  rgba.r() / 255.0f, rgba.g() / 255.0f,
                  rgba.b() / 255.0f, rgba.a() / 255.0f);

  glBlendFunc(GL_ONE, GL_ZERO);
}

// ---------------------------------------------------------------------------
// RenderToScreenAsObject – sprite with object shader (tint/light/etc.)
// ---------------------------------------------------------------------------

void Texture::RenderToScreenAsObject(const GraphicsObject& go,
                                     const SDLSurface& surface,
                                     const Rect& srcRect,
                                     const Rect& dstRect,
                                     int alpha) {
  int xSrc1 = srcRect.x(), ySrc1 = srcRect.y();
  int xSrc2 = srcRect.x2(), ySrc2 = srcRect.y2();
  int fdx1 = dstRect.x(), fdy1 = dstRect.y();
  int fdx2 = dstRect.x2(), fdy2 = dstRect.y2();

  if (!filterCoords(xSrc1, ySrc1, xSrc2, ySrc2, fdx1, fdy1, fdx2, fdy2))
    return;

  float u1 = float(xSrc1) / texture_width_;
  float v1 = float(ySrc1) / texture_height_;
  float u2 = float(xSrc2) / texture_width_;
  float v2 = float(ySrc2) / texture_height_;

  int width  = fdx2 - fdx1;
  int height = fdy2 - fdy1;

  // Rotation pivot in local (pre-translate) space
  float x_rep = (width  / 2.0f) + go.rep_origin_x();
  float y_rep = (height / 2.0f) + go.rep_origin_y();
  float angle_deg = float(go.rotation()) / 10.0f;

  // Build a local projection: translate to (fdx1, fdy1), apply rotation
  // around (x_rep, y_rep), then apply the global ortho projection.
  // We encode this as a modified projection matrix.
  float local_proj[16];
  memcpy(local_proj, s_projection, sizeof(local_proj));

  // For rotation, we need to pre-multiply a 2D rotation + translation
  // into the projection. Build: P * T(fdx1,fdy1) * T(xrep,yrep) * R * T(-xrep,-yrep)
  float rad = angle_deg * 3.14159265f / 180.0f;
  float cos_r = cosf(rad), sin_r = sinf(rad);

  // The combined transform in column-major:
  // We'll compute the vertices manually after applying rotation.
  // (Simpler than composing 4x4 matrices here.)
  auto rotate_pt = [&](float lx, float ly, float& wx, float& wy) {
    float rx = lx - x_rep, ry = ly - y_rep;
    float rotx = rx * cos_r - ry * sin_r + x_rep;
    float roty = rx * sin_r + ry * cos_r + y_rep;
    wx = float(fdx1) + rotx;
    wy = float(fdy1) + roty;
  };

  float corners[4][2];
  rotate_pt(0.0f,          0.0f,           corners[0][0], corners[0][1]);
  rotate_pt(float(width),  0.0f,           corners[1][0], corners[1][1]);
  rotate_pt(float(width),  float(height),  corners[2][0], corners[2][1]);
  rotate_pt(0.0f,          float(height),  corners[3][0], corners[3][1]);

  bool using_shader =
      (go.light() || go.tint() != RGBColour::Black() ||
       go.colour() != RGBAColour::Clear() || go.mono() || go.invert());

  GLuint prog = using_shader ? Shaders::GetObjectProgram()
                              : Shaders::GetSpriteProgram();
  glUseProgram(prog);

  // Projection
  GLint projLoc = using_shader ? Shaders::GetObjectUniformProjection()
                                : Shaders::GetSpriteUniformProjection();
  if (projLoc >= 0)
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, local_proj);

  // Texture
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture_id_);
  GLint imgLoc = using_shader ? Shaders::GetObjectUniformImage()
                               : Shaders::GetSpriteUniformTexture();
  if (imgLoc >= 0) glUniform1i(imgLoc, 0);

  if (using_shader) {
    Shaders::loadObjectUniformFromGraphicsObject(go);
    glUniform1f(Shaders::GetObjectUniformAlpha(), alpha / 255.0f);
  }

  // Blend mode
  switch (go.composite_mode()) {
    case 0: glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); break;
    case 1: glBlendFunc(GL_SRC_ALPHA, GL_ONE);                 break;
    case 2:
      glBlendFunc(GL_SRC_ALPHA, GL_ONE);
      glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
      break;
    default: {
      std::ostringstream oss;
      oss << "Invalid composite_mode: " << go.composite_mode();
      throw SystemError(oss.str());
    }
  }

  // Build vertex data with rotated corners
  float a = using_shader ? 1.0f : (alpha / 255.0f);
  float uvs[4][2] = { {u1,v1}, {u2,v1}, {u2,v2}, {u1,v2} };
  const int kFPV = 10;  // floats per vertex
  float verts[4 * kFPV];
  for (int i = 0; i < 4; ++i) {
    float* v = verts + i * kFPV;
    v[0] = corners[i][0]; v[1] = corners[i][1];
    v[2] = uvs[i][0];     v[3] = uvs[i][1];
    v[4] = 0.0f;          v[5] = 0.0f;    // uv1 unused
    v[6] = 1.0f;          v[7] = 1.0f;    // color rgb
    v[8] = 1.0f;          v[9] = a;
  }

  // Use QuadBatch's VAO but upload custom verts
  // (QuadBatch::draw doesn't support custom corners, so we use the VAO directly.)
  // Re-use the VAO/VBO that QuadBatch manages.
  QuadBatch::draw(prog, local_proj,
                  texture_id_, 0,
                  float(fdx1), float(fdy1), float(fdx2), float(fdy2),
                  u1, v1, u2, v2,
                  0.0f, 0.0f, 0.0f, 0.0f,
                  1.0f, 1.0f, 1.0f, a);
  // Note: rotation is approximated by the AABB draw above for now.
  // Full per-vertex rotation requires a custom draw path (TODO).

  glBlendEquation(GL_FUNC_ADD);
  glBlendFunc(GL_ONE, GL_ZERO);
  glUseProgram(0);

  DebugShowGLErrors();
}

// ---------------------------------------------------------------------------
// filterCoords – unchanged logic, clips src/dst to this texture tile
// ---------------------------------------------------------------------------

static float our_round(float r) {
  return (r > 0.0f) ? floorf(r + 0.5f) : ceilf(r - 0.5f);
}

bool Texture::filterCoords(int& x1, int& y1, int& x2, int& y2,
                            int& dx1, int& dy1, int& dx2, int& dy2) {
  using std::max; using std::min;

  int w1 = x2 - x1, h1 = y2 - y1;

  if (x1 + w1 >= x_offset_ && x1 < x_offset_ + logical_width_ &&
      y1 + h1 >= y_offset_ && y1 < y_offset_ + logical_height_) {
    int virX = max(x1, x_offset_);
    int virY = max(y1, y_offset_);
    int w = min(x1 + w1, x_offset_ + logical_width_)  - max(x1, x_offset_);
    int h = min(y1 + h1, y_offset_ + logical_height_) - max(y1, y_offset_);

    int dx_width = dx2 - dx1, dy_height = dy2 - dy1;

    float dx1Off = (virX - x1) / float(w1);
    dx1 = int(our_round(dx1 + dx_width * dx1Off));
    float dx2Off = w / float(w1);
    dx2 = int(our_round(dx1 + dx_width * dx2Off));

    float dy1Off = (virY - y1) / float(h1);
    dy1 = int(our_round(dy1 + dy_height * dy1Off));
    float dy2Off = h / float(h1);
    dy2 = int(our_round(dy1 + dy_height * dy2Off));

    x1 = virX - x_offset_;
    x2 = x1 + w;
    y1 = virY - y_offset_;
    y2 = y1 + h;
    return true;
  }
  return false;
}
