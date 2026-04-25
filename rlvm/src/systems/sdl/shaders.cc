// -*- Mode: C++; tab-width:2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi:tw=80:et:ts=2:sts=2
// -----------------------------------------------------------------------
// This file is part of RLVM, a RealLive virtual machine clone.
// -----------------------------------------------------------------------
// Copyright (C) 2013 Elliot Glaysher
// Licensed under GPLv3+. See COPYING for details.
// -----------------------------------------------------------------------
// Rewritten for OpenGL ES 3.2 / core profile.
// All ARB extension calls replaced with standard ES 3.2 / GL 3.3 core API.
// All shaders updated from GLSL 1.10 (gl_FragColor, varying, gl_TexCoord)
// to GLSL ES 3.20 (out vec4, in/out, texture()).
// -----------------------------------------------------------------------

#ifdef __ANDROID__
#include <GLES3/gl32.h>
#else
#include "GL/glew.h"
#endif

#ifndef NDEBUG
#include <iostream>
#endif

#include "systems/base/graphics_object.h"
#include "systems/base/system_error.h"
#include "systems/sdl/sdl_utils.h"
#include "systems/sdl/shaders.h"
#include "systems/sdl/quad_batch.h"

namespace {

// -----------------------------------------------------------------------
// GLSL ES 3.20 / GLSL 3.30 version string
// -----------------------------------------------------------------------
#ifdef __ANDROID__
#  define GLSL_VERSION "#version 320 es\n"
#  define PRECISION    "precision mediump float;\n"
#else
#  define GLSL_VERSION "#version 330 core\n"
#  define PRECISION    ""
#endif

// -----------------------------------------------------------------------
// Shared vertex shader – used by sprite, object, and color-mask programs.
// Receives per-quad vertices/texcoords set via our quad helper and
// transforms them with the projection matrix uniform.
// -----------------------------------------------------------------------
const char kCommonVertSrc[] =
  GLSL_VERSION
  "layout(location = 0) in vec2 a_pos;\n"
  "layout(location = 1) in vec2 a_uv0;\n"
  "layout(location = 2) in vec2 a_uv1;\n"   // second UV for color-mask
  "layout(location = 3) in vec4 a_color;\n"  // per-vertex color/opacity
  "uniform mat4 u_projection;\n"
  "out vec2 v_uv0;\n"
  "out vec2 v_uv1;\n"
  "out vec4 v_color;\n"
  "void main() {\n"
  "  gl_Position = u_projection * vec4(a_pos, 0.0, 1.0);\n"
  "  v_uv0 = a_uv0;\n"
  "  v_uv1 = a_uv1;\n"
  "  v_color = a_color;\n"
  "}\n";

// -----------------------------------------------------------------------
// Sprite fragment shader – plain textured quad with per-vertex opacity.
// -----------------------------------------------------------------------
const char kSpriteFrag[] =
  GLSL_VERSION
  PRECISION
  "in vec2 v_uv0;\n"
  "in vec4 v_color;\n"
  "uniform sampler2D u_texture;\n"
  "out vec4 frag_color;\n"
  "void main() {\n"
  "  vec4 tex = texture(u_texture, v_uv0);\n"
  "  frag_color = tex * v_color;\n"
  "}\n";

// -----------------------------------------------------------------------
// Color-mask fragment shader – replicates the original ARB shader logic.
//
// Inputs:
//   u_current  – screenshot of the framebuffer behind the text box
//   u_mask     – shape mask (alpha channel = mask intensity)
//   u_color    – window colour from #WINDOW_ATTR (r,g,b,a)
//
// Formula (matches original kColorMaskShader):
//   mask_factor = clamp(mask.a * colour.a, 0, 1)
//   result      = clamp(bg - mask_factor + colour * mask_factor, 0, 1)
// -----------------------------------------------------------------------
const char kColorMaskFrag[] =
  GLSL_VERSION
  PRECISION
  "in vec2 v_uv0;\n"   // UVs for u_current (screen capture)
  "in vec2 v_uv1;\n"   // UVs for u_mask
  "in vec4 v_color;\n" // window colour (passed as vertex color)
  "uniform sampler2D u_current;\n"
  "uniform sampler2D u_mask;\n"
  "out vec4 frag_color;\n"
  "void main() {\n"
  "  vec4 bg   = texture(u_current, v_uv0);\n"
  "  vec4 mask = texture(u_mask,    v_uv1);\n"
  "  float mf  = mask.a * v_color.a;\n"
  "  // Original RealLive subtractive formula:\n"
  "  // result = clamp(bg - mask_factor + colour * mask_factor, 0, 1)\n"
  "  vec3 result = clamp(bg.rgb - mf + v_color.rgb * mf, 0.0, 1.0);\n"
  "  frag_color = vec4(result, 1.0);\n"
  "}\n";

// -----------------------------------------------------------------------
// Object fragment shader – tint / light / mono / invert / colour / alpha.
// -----------------------------------------------------------------------
const char kObjectFrag[] =
  GLSL_VERSION
  PRECISION
  "in vec2 v_uv0;\n"
  "in vec4 v_color;\n"
  "uniform sampler2D u_image;\n"
  "uniform vec4  u_colour;\n"
  "uniform vec3  u_tint;\n"
  "uniform float u_light;\n"
  "uniform float u_alpha;\n"
  "uniform float u_mono;\n"
  "uniform float u_invert;\n"
  "out vec4 frag_color;\n"
  "\n"
  "float tinter(float pixel_val, float tint_val) {\n"
  "  if (tint_val > 0.0)\n"
  "    return pixel_val + tint_val - pixel_val * tint_val;\n"
  "  else if (tint_val < 0.0)\n"
  "    return pixel_val * abs(tint_val);\n"
  "  return pixel_val;\n"
  "}\n"
  "\n"
  "void main() {\n"
  "  vec4 pixel = texture(u_image, v_uv0);\n"
  "\n"
  "  // Colour overlay\n"
  "  vec3 coloured = mix(pixel.rgb, u_colour.rgb, u_colour.a);\n"
  "  pixel = vec4(coloured, pixel.a);\n"
  "\n"
  "  // Mono\n"
  "  if (u_mono > 0.0) {\n"
  "    float gray = dot(pixel.rgb, vec3(0.299, 0.587, 0.114));\n"
  "    pixel.rgb = mix(pixel.rgb, vec3(gray), u_mono);\n"
  "  }\n"
  "\n"
  "  // Invert\n"
  "  if (u_invert > 0.0)\n"
  "    pixel.rgb = mix(pixel.rgb, vec3(1.0) - pixel.rgb, u_invert);\n"
  "\n"
  "  // Light\n"
  "  pixel.r = tinter(pixel.r, u_light);\n"
  "  pixel.g = tinter(pixel.g, u_light);\n"
  "  pixel.b = tinter(pixel.b, u_light);\n"
  "\n"
  "  // Tint\n"
  "  pixel.r = tinter(pixel.r, u_tint.r);\n"
  "  pixel.g = tinter(pixel.g, u_tint.g);\n"
  "  pixel.b = tinter(pixel.b, u_tint.b);\n"
  "\n"
  "  pixel.a *= u_alpha;\n"
  "  frag_color = pixel;\n"
  "}\n";

}  // namespace

// ---------------------------------------------------------------------------
// Static member definitions
// ---------------------------------------------------------------------------

GLuint Shaders::color_mask_program_object_id_ = 0;
GLint  Shaders::color_mask_current_values_    = -1;
GLint  Shaders::color_mask_mask_              = -1;
GLint  Shaders::color_mask_color_             = -1;
GLint  Shaders::color_mask_projection_        = -1;

GLuint Shaders::object_program_object_id_ = 0;
GLint  Shaders::object_image_      = -1;
GLint  Shaders::object_colour_     = -1;
GLint  Shaders::object_tint_       = -1;
GLint  Shaders::object_light_      = -1;
GLint  Shaders::object_alpha_      = -1;
GLint  Shaders::object_mono_       = -1;
GLint  Shaders::object_invert_     = -1;
GLint  Shaders::object_projection_ = -1;

GLuint Shaders::sprite_program_object_id_ = 0;
GLint  Shaders::sprite_texture_    = -1;
GLint  Shaders::sprite_projection_ = -1;
GLint  Shaders::sprite_color_      = -1;

// ---------------------------------------------------------------------------

// static
void Shaders::Reset() {
  // Also reset the shared VAO/VBO so they are recreated with the new GL context.
  QuadBatch::reset();

  if (color_mask_program_object_id_) {
    glDeleteProgram(color_mask_program_object_id_);
    color_mask_program_object_id_ = 0;
    color_mask_current_values_ = color_mask_mask_ =
        color_mask_color_ = color_mask_projection_ = -1;
  }
  if (object_program_object_id_) {
    glDeleteProgram(object_program_object_id_);
    object_program_object_id_ = 0;
    object_image_ = object_colour_ = object_tint_ = object_light_ =
        object_alpha_ = object_mono_ = object_invert_ = object_projection_ = -1;
  }
  if (sprite_program_object_id_) {
    glDeleteProgram(sprite_program_object_id_);
    sprite_program_object_id_ = 0;
    sprite_texture_ = sprite_projection_ = sprite_color_ = -1;
  }
}

// ---------------------------------------------------------------------------
// Color mask program
// ---------------------------------------------------------------------------

GLuint Shaders::getColorMaskProgram() {
  if (!color_mask_program_object_id_)
    color_mask_program_object_id_ = buildProgram(kCommonVertSrc, kColorMaskFrag);
  return color_mask_program_object_id_;
}

GLint Shaders::getColorMaskUniformCurrentValues() {
  if (color_mask_current_values_ < 0)
    color_mask_current_values_ =
        glGetUniformLocation(getColorMaskProgram(), "u_current");
  return color_mask_current_values_;
}

GLint Shaders::getColorMaskUniformMask() {
  if (color_mask_mask_ < 0)
    color_mask_mask_ = glGetUniformLocation(getColorMaskProgram(), "u_mask");
  return color_mask_mask_;
}

GLint Shaders::getColorMaskUniformColor() {
  if (color_mask_color_ < 0)
    color_mask_color_ = glGetUniformLocation(getColorMaskProgram(), "u_color");  // unused – color goes via vertex attrib
  return color_mask_color_;
}

GLint Shaders::getColorMaskUniformProjection() {
  if (color_mask_projection_ < 0)
    color_mask_projection_ =
        glGetUniformLocation(getColorMaskProgram(), "u_projection");
  return color_mask_projection_;
}

// ---------------------------------------------------------------------------
// Object program
// ---------------------------------------------------------------------------

GLuint Shaders::GetObjectProgram() {
  if (!object_program_object_id_)
    object_program_object_id_ = buildProgram(kCommonVertSrc, kObjectFrag);
  return object_program_object_id_;
}

GLint Shaders::GetObjectUniformImage() {
  if (object_image_ < 0)
    object_image_ = glGetUniformLocation(GetObjectProgram(), "u_image");
  return object_image_;
}

GLint Shaders::GetObjectUniformAlpha() {
  if (object_alpha_ < 0)
    object_alpha_ = glGetUniformLocation(GetObjectProgram(), "u_alpha");
  return object_alpha_;
}

GLint Shaders::GetObjectUniformColour() {
  if (object_colour_ < 0)
    object_colour_ = glGetUniformLocation(GetObjectProgram(), "u_colour");
  return object_colour_;
}

GLint Shaders::GetObjectUniformTint() {
  if (object_tint_ < 0)
    object_tint_ = glGetUniformLocation(GetObjectProgram(), "u_tint");
  return object_tint_;
}

GLint Shaders::GetObjectUniformLight() {
  if (object_light_ < 0)
    object_light_ = glGetUniformLocation(GetObjectProgram(), "u_light");
  return object_light_;
}

GLint Shaders::GetObjectUniformMono() {
  if (object_mono_ < 0)
    object_mono_ = glGetUniformLocation(GetObjectProgram(), "u_mono");
  return object_mono_;
}

GLint Shaders::GetObjectUniformInvert() {
  if (object_invert_ < 0)
    object_invert_ = glGetUniformLocation(GetObjectProgram(), "u_invert");
  return object_invert_;
}

GLint Shaders::GetObjectUniformProjection() {
  if (object_projection_ < 0)
    object_projection_ =
        glGetUniformLocation(GetObjectProgram(), "u_projection");
  return object_projection_;
}

void Shaders::loadObjectUniformFromGraphicsObject(const GraphicsObject& go) {
  RGBAColour colour = go.colour();
  glUniform4f(GetObjectUniformColour(),
              colour.r_float(), colour.g_float(),
              colour.b_float(), colour.a_float());

  RGBColour tint = go.tint();
  glUniform3f(GetObjectUniformTint(),
              tint.r_float(), tint.g_float(), tint.b_float());

  glUniform1f(GetObjectUniformLight(),  go.light()  / 255.0f);
  glUniform1f(GetObjectUniformMono(),   go.mono()   / 255.0f);
  glUniform1f(GetObjectUniformInvert(), go.invert() / 255.0f);
}

// ---------------------------------------------------------------------------
// Sprite program
// ---------------------------------------------------------------------------

GLuint Shaders::GetSpriteProgram() {
  if (!sprite_program_object_id_)
    sprite_program_object_id_ = buildProgram(kCommonVertSrc, kSpriteFrag);
  return sprite_program_object_id_;
}

GLint Shaders::GetSpriteUniformTexture() {
  if (sprite_texture_ < 0)
    sprite_texture_ = glGetUniformLocation(GetSpriteProgram(), "u_texture");
  return sprite_texture_;
}

GLint Shaders::GetSpriteUniformProjection() {
  if (sprite_projection_ < 0)
    sprite_projection_ =
        glGetUniformLocation(GetSpriteProgram(), "u_projection");
  return sprite_projection_;
}

GLint Shaders::GetSpriteUniformColor() {
  if (sprite_color_ < 0)
    sprite_color_ = glGetUniformLocation(GetSpriteProgram(), "u_color");  // unused
  return sprite_color_;
}

// ---------------------------------------------------------------------------
// Internal: compile and link a vertex + fragment shader pair
// ---------------------------------------------------------------------------

// static
GLuint Shaders::buildProgram(const char* vert_src, const char* frag_src) {
  auto compileShader = [](GLenum type, const char* src) -> GLuint {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);

#ifndef NDEBUG
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
      GLint len = 0;
      glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
      std::string log(len, '\0');
      glGetShaderInfoLog(s, len, nullptr, &log[0]);
      std::cerr << "Shader compile error (" << (type == GL_VERTEX_SHADER ? "vert" : "frag")
                << "):\n" << log << "\n";
    }
#endif
    return s;
  };

  GLuint vert = compileShader(GL_VERTEX_SHADER,   vert_src);
  GLuint frag = compileShader(GL_FRAGMENT_SHADER, frag_src);

  GLuint prog = glCreateProgram();
  glAttachShader(prog, vert);
  glAttachShader(prog, frag);

  // Bind attribute locations to match the vertex layout used in QuadBatch.
  glBindAttribLocation(prog, 0, "a_pos");
  glBindAttribLocation(prog, 1, "a_uv0");
  glBindAttribLocation(prog, 2, "a_uv1");
  glBindAttribLocation(prog, 3, "a_color");

  glLinkProgram(prog);

#ifndef NDEBUG
  GLint ok = 0;
  glGetProgramiv(prog, GL_LINK_STATUS, &ok);
  if (!ok) {
    GLint len = 0;
    glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
    std::string log(len, '\0');
    glGetProgramInfoLog(prog, len, nullptr, &log[0]);
    std::cerr << "Program link error:\n" << log << "\n";
  }
#endif

  glDeleteShader(vert);
  glDeleteShader(frag);

  return prog;
}
