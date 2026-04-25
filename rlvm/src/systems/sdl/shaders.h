// -*- Mode: C++; tab-width:2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi:tw=80:et:ts=2:sts=2
// -----------------------------------------------------------------------
// This file is part of RLVM, a RealLive virtual machine clone.
// -----------------------------------------------------------------------
// Copyright (C) 2013 Elliot Glaysher
// Licensed under GPLv3+. See COPYING for details.
// -----------------------------------------------------------------------
// Rewritten for OpenGL ES 3.2 / core profile (no ARB extensions, no
// fixed-function pipeline).
// -----------------------------------------------------------------------

#ifndef SRC_SYSTEMS_SDL_SHADERS_H_
#define SRC_SYSTEMS_SDL_SHADERS_H_

#ifdef __ANDROID__
#include <GLES3/gl32.h>
#else
#include <SDL2/SDL_opengl.h>
#endif

class GraphicsObject;

// Static state about shaders. We just leak them.
class Shaders {
 public:
  // Immediately frees all OpenGL resources associated with shaders.
  static void Reset();

  // -----------------------------------------------------------------------
  // Color mask shader (text-box semi-transparent overlay)
  // -----------------------------------------------------------------------
  static GLuint getColorMaskProgram();
  static GLint  getColorMaskUniformCurrentValues();
  static GLint  getColorMaskUniformMask();
  // Extra uniforms used by the ES 3.2 color mask shader
  static GLint  getColorMaskUniformColor();
  static GLint  getColorMaskUniformProjection();

  // -----------------------------------------------------------------------
  // Object shader (tint / light / colour / mono / invert on sprites)
  // -----------------------------------------------------------------------
  static GLuint GetObjectProgram();
  static GLint  GetObjectUniformImage();
  static GLint  GetObjectUniformAlpha();
  static void   loadObjectUniformFromGraphicsObject(const GraphicsObject& go);
  static GLint  GetObjectUniformColour();
  static GLint  GetObjectUniformTint();
  static GLint  GetObjectUniformLight();
  static GLint  GetObjectUniformMono();
  static GLint  GetObjectUniformInvert();
  // ES 3.2 object shader also needs projection
  static GLint  GetObjectUniformProjection();

  // -----------------------------------------------------------------------
  // Sprite shader (plain textured quad – used for all normal draws)
  // -----------------------------------------------------------------------
  static GLuint GetSpriteProgram();
  static GLint  GetSpriteUniformTexture();
  static GLint  GetSpriteUniformProjection();
  static GLint  GetSpriteUniformColor();

 private:
  static GLuint buildProgram(const char* vert_src, const char* frag_src);

  // Color mask program
  static GLuint color_mask_program_object_id_;
  static GLint  color_mask_current_values_;
  static GLint  color_mask_mask_;
  static GLint  color_mask_color_;
  static GLint  color_mask_projection_;

  // Object program
  static GLuint object_program_object_id_;
  static GLint  object_image_;
  static GLint  object_colour_;
  static GLint  object_tint_;
  static GLint  object_light_;
  static GLint  object_alpha_;
  static GLint  object_mono_;
  static GLint  object_invert_;
  static GLint  object_projection_;

  // Sprite program
  static GLuint sprite_program_object_id_;
  static GLint  sprite_texture_;
  static GLint  sprite_projection_;
  static GLint  sprite_color_;
};

#endif  // SRC_SYSTEMS_SDL_SHADERS_H_
