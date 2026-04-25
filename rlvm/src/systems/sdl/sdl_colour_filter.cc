// -*- Mode: C++; tab-width:2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi:tw=80:et:ts=2:sts=2
// -----------------------------------------------------------------------
// This file is part of RLVM, a RealLive virtual machine clone.
// -----------------------------------------------------------------------
// Copyright (C) 2013 Elliot Glaysher
// Licensed under GPLv3+. See COPYING for details.
// -----------------------------------------------------------------------
// Rewritten for OpenGL ES 3.2 / core profile.
// -----------------------------------------------------------------------

#ifdef __ANDROID__
#include <GLES3/gl32.h>
#else
#include "GL/glew.h"
#endif

#include "systems/sdl/sdl_colour_filter.h"

#include "systems/base/colour.h"
#include "systems/base/graphics_object.h"
#include "systems/sdl/quad_batch.h"
#include "systems/sdl/sdl_utils.h"
#include "systems/sdl/shaders.h"
#include "systems/sdl/texture.h"

SDLColourFilter::SDLColourFilter()
    : texture_width_(0), texture_height_(0), back_texture_id_(0) {}

SDLColourFilter::~SDLColourFilter() {
  if (back_texture_id_)
    glDeleteTextures(1, &back_texture_id_);
}

void SDLColourFilter::Fill(const GraphicsObject& go,
                           const Rect& screen_rect,
                           const RGBAColour& colour) {
  if (back_texture_id_ == 0) {
    glGenTextures(1, &back_texture_id_);
    glBindTexture(GL_TEXTURE_2D, back_texture_id_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    texture_width_  = SafeSize(screen_rect.width());
    texture_height_ = SafeSize(screen_rect.height());
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 texture_width_, texture_height_, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    DebugShowGLErrors();
  }

  // Capture the framebuffer region behind the object.
  // Convert game coords → window/framebuffer coords.
  {
    float scale_x = (Texture::ViewportWidth()  > 0)
        ? float(Texture::ViewportWidth())  / float(Texture::ScreenWidth())  : 1.0f;
    float scale_y = (Texture::ViewportHeight() > 0)
        ? float(Texture::ViewportHeight()) / float(Texture::ScreenHeight()) : 1.0f;

    int fb_x = Texture::ViewportX() + int(screen_rect.x() * scale_x);
    int fb_y = Texture::ViewportY()
               + int((Texture::ScreenHeight() - screen_rect.y()
                      - screen_rect.height()) * scale_y);
    int cap_w = int(screen_rect.width()  * scale_x);
    int cap_h = int(screen_rect.height() * scale_y);

    glBindTexture(GL_TEXTURE_2D, back_texture_id_);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, fb_x, fb_y, cap_w, cap_h);
    DebugShowGLErrors();
  }

  // Set up object shader (handles tint/light/colour/mono/invert).
  GLuint prog = Shaders::GetObjectProgram();
  glUseProgram(prog);

  GLint projLoc = Shaders::GetObjectUniformProjection();
  if (projLoc >= 0)
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, Texture::CurrentProjection());

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, back_texture_id_);
  GLint imgLoc = Shaders::GetObjectUniformImage();
  if (imgLoc >= 0) glUniform1i(imgLoc, 0);

  glUniform1f(Shaders::GetObjectUniformAlpha(),
              go.GetComputedAlpha() / 255.0f);
  Shaders::loadObjectUniformFromGraphicsObject(go);

  float tx1 = 0.0f;
  float ty1 = 0.0f;
  float tx2 = float(screen_rect.width())  / float(texture_width_);
  float ty2 = float(screen_rect.height()) / float(texture_height_);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  QuadBatch::draw(prog, Texture::CurrentProjection(),
                  back_texture_id_, 0,
                  float(screen_rect.x()),
                  float(screen_rect.y()),
                  float(screen_rect.x() + screen_rect.width()),
                  float(screen_rect.y() + screen_rect.height()),
                  // UV: y-flipped (capture is GL bottom-up)
                  tx1, ty2, tx2, ty1,
                  0.0f, 0.0f, 0.0f, 0.0f,
                  1.0f, 1.0f, 1.0f, 1.0f);

  glBlendFunc(GL_ONE, GL_ZERO);
  glUseProgram(0);
}
