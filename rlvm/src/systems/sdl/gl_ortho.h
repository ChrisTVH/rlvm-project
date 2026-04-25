// gl_ortho.h – orthographic projection matrix (column-major, OpenGL convention)
// Replaces glOrtho() for use with shader uniforms.
#ifndef SRC_SYSTEMS_SDL_GL_ORTHO_H_
#define SRC_SYSTEMS_SDL_GL_ORTHO_H_

// Fills |out| (16 floats, column-major) with an orthographic projection:
//   left=0, right=w, top=0 (y-down), bottom=h, near=-1, far=1
// This matches the original glOrtho(0, w, h, 0, 0, 1) call.
inline void buildOrtho(float* out, float w, float h) {
  // Column-major:
  out[ 0] = 2.0f / w;  out[ 4] = 0.0f;       out[ 8] = 0.0f; out[12] = -1.0f;
  out[ 1] = 0.0f;      out[ 5] = -2.0f / h;  out[ 9] = 0.0f; out[13] =  1.0f;
  out[ 2] = 0.0f;      out[ 6] = 0.0f;       out[10] =-1.0f; out[14] =  0.0f;
  out[ 3] = 0.0f;      out[ 7] = 0.0f;       out[11] = 0.0f; out[15] =  1.0f;
}

// Applies translation to the projection matrix (simulates glTranslatef for screen-shake).
inline void applyTranslation(float* proj, float tx, float ty) {
  // Modify the translation column (column 3 in column-major)
  proj[12] += proj[0] * tx + proj[4] * ty;
  proj[13] += proj[1] * tx + proj[5] * ty;
}

#endif  // SRC_SYSTEMS_SDL_GL_ORTHO_H_
