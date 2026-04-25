// -*- Mode: C++; tab-width:2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi:tw=80:et:ts=2:sts=2
// -----------------------------------------------------------------------
// This file is part of RLVM, a RealLive virtual machine clone.
// -----------------------------------------------------------------------
// Copyright (C) 2006 Elliot Glaysher
// Licensed under GPLv3+. See COPYING for details.
// -----------------------------------------------------------------------
// Rewritten for OpenGL ES 3.2 / core profile.
// -----------------------------------------------------------------------

#ifndef SRC_SYSTEMS_SDL_TEXTURE_H_
#define SRC_SYSTEMS_SDL_TEXTURE_H_

#ifdef __ANDROID__
#include <GLES3/gl32.h>
#else
#include <SDL2/SDL_opengl.h>
#endif

#include <memory>
#include <string>

struct SDL_Surface;
class SDLSurface;
class GraphicsObject;
class RGBAColour;
class Rect;
class Size;

struct render_to_texture {};

class Texture {
 public:
  // -----------------------------------------------------------------------
  // Global state (set by SDLGraphicsSystem each frame)
  // -----------------------------------------------------------------------
  static void SetScreenSize(const Size& s);

  // Viewport offset (in window pixels) for letterbox/pillarbox correction.
  static void SetViewportOffset(int x, int y);

  // Viewport pixel dimensions (window pixels).
  static void SetViewportSize(int w, int h);

  // Update the shared orthographic projection matrix.
  // Call from BeginFrame after computing viewport & screen-shake origin.
  // w, h = game coordinate space; tx, ty = screen-shake translation.
  static void SetProjection(float w, float h, float tx = 0.0f, float ty = 0.0f);

  // Returns pointer to the current column-major 4x4 projection matrix.
  static const float* CurrentProjection() { return s_projection; }

  // Viewport/screen accessors for colour filter and other subsystems.
  static int ViewportX()      { return s_viewport_x; }
  static int ViewportY()      { return s_viewport_y; }
  static int ViewportWidth()  { return s_viewport_width; }
  static int ViewportHeight() { return s_viewport_height; }
  static int ScreenWidth()    { return int(s_screen_width); }

  static int ScreenHeight();

 public:
  Texture(SDL_Surface* surface,
          int x, int y, int w, int h,
          unsigned int bytes_per_pixel,
          int byte_order, int byte_type);
  Texture(render_to_texture, int screen_width, int screen_height);
  ~Texture();

  void reupload(SDL_Surface* surface,
                int offset_x, int offset_y,
                int x, int y, int w, int h,
                unsigned int bytes_per_pixel,
                int byte_order, int byte_type);

  int   width()     { return logical_width_;  }
  int   height()    { return logical_height_; }
  GLuint textureId() { return texture_id_;    }

  void RenderToScreen(const Rect& src, const Rect& dst, int opacity);
  void RenderToScreen(const Rect& src, const Rect& dst, const int opacity[4]);
  void RenderToScreenAsColorMask(const Rect& src, const Rect& dst,
                                 const RGBAColour& rgba, int filter);
  void RenderToScreenAsObject(const GraphicsObject& go,
                              const SDLSurface& surface,
                              const Rect& srcRect,
                              const Rect& dstRect,
                              int alpha);

 private:
  static char* uploadBuffer(unsigned int size);

  void render_to_screen_as_colour_mask_subtractive(const Rect& src,
                                                   const Rect& dst,
                                                   const RGBAColour& rgba);
  void render_to_screen_as_colour_mask_additive(const Rect& src,
                                                const Rect& dst,
                                                const RGBAColour& rgba);

  bool filterCoords(int& x1, int& y1, int& x2, int& y2,
                    int& dx1, int& dy1, int& dx2, int& dy2);

  int x_offset_, y_offset_;
  int logical_width_, logical_height_;
  int total_width_, total_height_;
  unsigned int texture_width_, texture_height_;
  GLuint texture_id_;
  GLuint back_texture_id_;
  int    back_tex_w_;        // Tracked size of back_texture (avoids GLES-incompatible queries)
  int    back_tex_h_;
  bool is_upside_down_;

  // -----------------------------------------------------------------------
  // Shared static state
  // -----------------------------------------------------------------------
  static unsigned int s_screen_width, s_screen_height;
  static int   s_viewport_x, s_viewport_y;
  static int   s_viewport_width, s_viewport_height;
  static int   s_window_width, s_window_height;
  static float s_capture_scale_x, s_capture_scale_y;

  // Current orthographic projection matrix (column-major 4×4).
  // Shared by all Texture draw calls within a frame.
  static float s_projection[16];

  static unsigned int s_upload_buffer_size;
  static std::unique_ptr<char[]> s_upload_buffer;
};

#endif  // SRC_SYSTEMS_SDL_TEXTURE_H_
