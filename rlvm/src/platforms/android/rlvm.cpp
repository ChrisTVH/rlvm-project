// -----------------------------------------------------------------------
//
// This file is part of RLVM, a RealLive virtual machine clone.
//
// -----------------------------------------------------------------------
//
// Copyright (C) 2014 Ilya Zhuravlev
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
//
// -----------------------------------------------------------------------

#include <SDL2/SDL.h>
#include <jni.h>

#include <atomic>
#include <boost/program_options.hpp>
#include <iostream>
#include <string>

#include <unistd.h>

#include <SDL2/SDL.h>
#include <android/log.h>

#ifdef __ANDROID__
#include <GLES3/gl32.h>
#else
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#endif

#include "AndroidRLVMInstance.hpp"

#include "libreallive/gameexe.h"
#include "systems/base/rect.h"
#include "utilities/file.h"
#include "utilities/graphics.h"

#define LOG_TAG "RLVM"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using namespace std;

#define SDL_JAVA_PREFIX org_libsdl_app
#define CONCAT1(prefix, class, function) CONCAT2(prefix, class, function)
#define CONCAT2(prefix, class, function) Java_##prefix##_##class##_##function
#define SDL_JAVA_INTERFACE(function) \
  CONCAT1(SDL_JAVA_PREFIX, SDLActivity, function)

namespace po = boost::program_options;
namespace fs = boost::filesystem;

AndroidRLVMInstance instance;
std::string g_root_path;

bool g_background = false;

static void appPutToBackground() {
  // TODO(xyz) wtf, this repeatedly gets called
  g_background = true;
  // SDL_ANDROID_PauseAudioPlayback();
}

static void appPutToForeground() {
  g_background = false;
  instance.ReloadAllTextures();
  // SDL_ANDROID_ResumeAudioPlayback();

  // TODO(xyz): remove the copypasta here
  // NOTE: GL_TEXTURE_2D removed - not available in ES 3.2 core profile
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  // ES 3.2 core: no fixed-function state (glShadeModel, glColor4f, etc.)
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void ScreenSizeCallback(const Size& size) {
  JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();

  // retrieve the Java instance of the SDLActivity
  jobject activity = (jobject)SDL_AndroidGetActivity();

  // find the Java class of the activity. It should be SDLActivity or a subclass
  // of it.
  jclass clazz(env->GetObjectClass(activity));

  // find the identifier of the method to call
  jmethodID method_id = env->GetMethodID(clazz, "setScreenSize", "(II)V");

  // effectively call the Java method
  env->CallVoidMethod(activity, method_id, size.width(), size.height());

  // clean up the local references.
  env->DeleteLocalRef(activity);
  env->DeleteLocalRef(clazz);
}

// has to be duplicated here because we need to set ScreenSize ASAP
boost::filesystem::path FindGameFile(
    const boost::filesystem::path& gamerootPath,
    const std::string& filename) {
  fs::path search_for = gamerootPath / filename;
  fs::path corrected_path = CorrectPathCase(search_for);
  if (corrected_path.empty()) {
    throw std::runtime_error("Corrected path is empty");
  }

  return corrected_path;
}

static std::atomic<bool> ready;

extern "C" JNIEXPORT void JNICALL SDL_JAVA_INTERFACE(nativeReady)(JNIEnv* env,
                                                                  jclass cls) {
  LOGI("nativeReady called from thread %p, setting ready to true",
       pthread_self());
  ready.store(true, std::memory_order_seq_cst);
  LOGI("nativeReady store complete");
}

__attribute__((visibility("default"))) int main(int argc, char* argv[]) {
  // Get game path from environment variable set by GameActivity
  const char* home_path = std::getenv("HOME");
  LOGI("Native main: HOME env var = %s", home_path ? home_path : "NULL");

  if (home_path != nullptr) {
    g_root_path = home_path;
  } else {
    // Fallback for testing (should not happen in production)
    g_root_path = "/storage/emulated/0/";
  }

  LOGI("Native main: g_root_path = %s", g_root_path.c_str());

  fs::path gameexePath = FindGameFile(g_root_path, "Gameexe.ini");
  LOGI("Native main: gameexePath = %s", gameexePath.c_str());

  // ready is already initialized to false by static storage duration.
  // We do NOT reset it here to avoid race conditions with nativeReady()
  // which might have been called before main() reached this point.

  try {
    Gameexe gameexe(gameexePath);
    SDL_Log("Native main: Gameexe loaded successfully");
    // ScreenSizeCallback(GetScreenSize(gameexe)); // Removed to avoid calling
    // Java before activity is ready
  } catch (const std::exception& e) {
    LOGE("Failed to load Gameexe.ini: %s", e.what());
    return 1;
  }

  // we want the surface to get the changes and resize itself before we create
  // SDL2 context and everything
  while (!ready.load(std::memory_order_seq_cst)) {
    LOGI("Native main: Waiting for ready signal...");
    sleep(1);
  }

  fs::path gamerootPath(g_root_path);
  LOGI("Native main: Starting game loop with root path: %s",
       gamerootPath.c_str());

  instance.Run(gamerootPath);

  return 0;
}
