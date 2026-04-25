cmake_minimum_required(VERSION 3.10.0)

# Suppress CMP0135 warnings: use extraction time for downloaded archive timestamps
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

set(RLVM_TAG android)

# --- Dependency versions & hashes ---

set(BOOST_VERSION 1.87.0)
set(BOOST_HASH SHA256=af57be25cb4c4f4b413ed692fe378affb4352ea50fbe294a11ef548f4d527d89)

set(OGG_VERSION 1.3.5)
set(OGG_HASH SHA256=c4d91be36fc8e54deae7575241e03f4211eb102afb3fc0775fbbc1b740016705)

set(VORBIS_VERSION 1.3.7)
set(VORBIS_HASH SHA256=b33cc4934322bcbf6efcbacf49e3ca01aadbea4114ec9589d1b1e9d20f72954b)

set(SNDFILE_VERSION 1.2.2)
set(SNDFILE_HASH SHA256=3799ca9924d3125038880367bf1468e53a1b7e3686a934f098b7e1d286cdb80e)

# SDL2 2.30.x fixes ALooper_pollAll removal in NDK r27 (bug #9792, commit 558630d).
# Releases from 2.30.x are hosted on GitHub; libsdl.org just mirrors them.
# TODO: after the first download, run:
#   sha256sum scripts/downloads/SDL2-2.30.12.tar.gz
# and replace the placeholder below for fully reproducible builds.
set(SDL2_VERSION 2.30.12)
set(SDL2_HASH SHA256=ac356ea55e8b9dd0b2d1fa27da40ef7e238267ccf9324704850d5d47375b48ea)

set(FREETYPE2_VERSION 2.13.2)
set(FREETYPE2_HASH SHA256=1ac27e16c134a7f2ccea177faba19801131116fd682efc1f5737037c5db224b5)

set(SDL2_TTF_VERSION 2.22.0)
set(SDL2_TTF_HASH SHA256=d48cbd1ce475b9e178206bf3b72d56b66d84d44f64ac05803328396234d67723)

set(SDL2_MIXER_VERSION 2.8.0)
set(SDL2_MIXER_HASH SHA256=1cfb34c87b26dbdbc7afd68c4f545c0116ab5f90bbfecc5aebe2a9cb4bb31549)

set(SDL2_IMAGE_VERSION 2.8.2)
set(SDL2_IMAGE_HASH SHA256=8f486bbfbcf8464dd58c9e5d93394ab0255ce68b51c5a966a918244820a76ddc)

# gl4es removed: rlvm now uses OpenGL ES 3.2 natively (Android 7+ / API 24+).
# libGLESv3 and libEGL are provided by the Android NDK sysroot.

# --- End of configurable options ---

project(rlvm-android)

include(ExternalProject)

# Determine NDK directory (use existing NDK from toolchain/ndk/)
set(NDK_DIR ${CMAKE_SOURCE_DIR}/toolchain/ndk)
set(NDK_BIN ${NDK_DIR}/toolchains/llvm/prebuilt/linux-x86_64/bin)

# Determine the correct clang triplet based on architecture
if("${ARCH}" STREQUAL "arm")
    set(CLANG_TRIPLET "armv7a-linux-androideabi")
else()
    set(CLANG_TRIPLET ${NDK_TRIPLET})
endif()

# Set compiler paths
set(CC_PATH "${NDK_BIN}/${CLANG_TRIPLET}${ANDROID_API}-clang")
set(CXX_PATH "${NDK_BIN}/${CLANG_TRIPLET}${ANDROID_API}-clang++")
set(AR_PATH "${NDK_BIN}/llvm-ar")
set(RANLIB_PATH "${NDK_BIN}/llvm-ranlib")
set(STRIP_PATH "${NDK_BIN}/llvm-strip")

# Generate command_wrapper.sh
set(wrapper_command ${CMAKE_BINARY_DIR}/command_wrapper.sh)

# Set compiler flags
set(CFLAGS "-fPIC")
set(CXXFLAGS "-fPIC -frtti -fexceptions -I${CMAKE_SOURCE_DIR}/toolchain/${ARCH}/sysroot/usr/include/SDL2 -I${CMAKE_SOURCE_DIR}/toolchain/${ARCH}/sysroot/usr/include")
set(LDFLAGS "-Wl,--exclude-libs,libgcc.a -Wl,--exclude-libs,libatomic.a -Wl,--exclude-libs,libunwind.a -Wl,-z,max-page-size=16384 -L${CMAKE_SOURCE_DIR}/toolchain/${ARCH}/sysroot/usr/lib")

# Add architecture-specific flags
if("${ARCH}" STREQUAL "arm")
    set(CFLAGS "${CFLAGS} -mthumb")
    set(CXXFLAGS "${CXXFLAGS} -mthumb")
endif()

# Add build type flags
if("${BUILD_TYPE}" STREQUAL "release")
    set(CFLAGS "${CFLAGS} -O2")
    set(CXXFLAGS "${CXXFLAGS} -O2")
else()
    set(CFLAGS "${CFLAGS} -O0 -g")
    set(CXXFLAGS "${CXXFLAGS} -O0 -g")
endif()

file(WRITE ${wrapper_command}
"#!/bin/bash
export CC=\"${CC_PATH}\"
export CXX=\"${CXX_PATH}\"
export AR=\"${AR_PATH}\"
export RANLIB=\"${RANLIB_PATH}\"
export STRIP=\"${STRIP_PATH}\"
export PKG_CONFIG_LIBDIR=\"${CMAKE_SOURCE_DIR}/toolchain/${ARCH}/sysroot/usr/lib/pkgconfig\"
export PATH=\"${CMAKE_SOURCE_DIR}/toolchain/${ARCH}/bin:${NDK_BIN}:${NDK_DIR}:\$PATH\"
export CFLAGS=\"${CFLAGS}\"
export CXXFLAGS=\"${CXXFLAGS}\"
export CPPFLAGS=\"${CXXFLAGS}\"
export LDFLAGS=\"${LDFLAGS}\"
export LINKFLAGS=\"${LDFLAGS}\"

COMMAND_NAME=\"\$1\"
shift
\$COMMAND_NAME \"\$@\"
"
)
execute_process(COMMAND chmod +x ${wrapper_command})

set(ndk_triplet ${NDK_TRIPLET})
set(download_dir ${CMAKE_SOURCE_DIR}/downloads/)
set(prefix ${CMAKE_INSTALL_PREFIX})
set(host ${ndk_triplet})
set(android_platform android-${ANDROID_API})
set(app_abi ${ABI})

set(COMMON_CMAKE_ARGS
	-DCMAKE_TOOLCHAIN_FILE=${CMAKE_SOURCE_DIR}/toolchain/ndk/build/cmake/android.toolchain.cmake
	-DCMAKE_INSTALL_PREFIX=${prefix}
	-DCMAKE_FIND_ROOT_PATH=${prefix}
	-DANDROID_ABI=${app_abi}
	-DANDROID_PLATFORM=${android_platform}
	-DANDROID_STL=c++_shared
	-DANDROID_CPP_FEATURES=rtti\ exceptions
)

set(COMMON_NDK_BUILD_FLAGS
	NDK_PROJECT_PATH=.
	APP_BUILD_SCRIPT=<SOURCE_DIR>/Android.mk
	APP_PLATFORM=${android_platform}
	APP_ABI=${app_abi}
	APP_ALLOW_MISSING_DEPS=true
	-j4
)

set(COMMON_AUTOCONF_FLAGS
	--host=${host}
	--enable-static
	--disable-shared
	--prefix=${prefix}
)

# ffmpeg flags (kept for future use; not currently a direct dependency)
set(ffmpeg_configure_flags ${COMMON_AUTOCONF_FLAGS})
if (${BUILD_TYPE} STREQUAL "debug")
	set(ffmpeg_configure_flags ${ffmpeg_configure_flags} --disable-asm --disable-optimizations)
endif()
if ((${ARCH} STREQUAL "x86_64") OR (${ARCH} STREQUAL "x86"))
	set(ffmpeg_configure_flags ${ffmpeg_configure_flags} --disable-asm)
endif()
if (${ARCH} STREQUAL "arm")
	set(ffmpeg_configure_flags ${ffmpeg_configure_flags} --enable-thumb)
endif()

set(libjpeg_turbo_flags "")
if (${ARCH} STREQUAL "x86")
	set(libjpeg_turbo_flags ${libjpeg_turbo_flags} --without-simd)
endif()


## --- Dependencies ---

ExternalProject_Add(ogg
	URL https://downloads.xiph.org/releases/ogg/libogg-${OGG_VERSION}.tar.xz
	URL_HASH ${OGG_HASH}
	DOWNLOAD_DIR ${download_dir}

	# Touch Makefile.in files so automake doesn't try to regenerate them
	# (host automake version differs from what the tarball was built with)
	PATCH_COMMAND find <SOURCE_DIR> -name "Makefile.in" -exec touch {} +

	CONFIGURE_COMMAND ${wrapper_command} <SOURCE_DIR>/configure
	${COMMON_AUTOCONF_FLAGS}

	BUILD_COMMAND ${wrapper_command} $(MAKE)
	INSTALL_COMMAND ${wrapper_command} $(MAKE) install
)

ExternalProject_Add(vorbis
	DEPENDS ogg

	URL https://downloads.xiph.org/releases/vorbis/libvorbis-${VORBIS_VERSION}.tar.xz
	URL_HASH ${VORBIS_HASH}
	DOWNLOAD_DIR ${download_dir}

	CONFIGURE_COMMAND ${wrapper_command} <SOURCE_DIR>/configure
	${COMMON_AUTOCONF_FLAGS}

	BUILD_COMMAND ${wrapper_command} $(MAKE)
	INSTALL_COMMAND ${wrapper_command} $(MAKE) install
)

ExternalProject_Add(sndfile
	URL https://github.com/libsndfile/libsndfile/releases/download/${SNDFILE_VERSION}/libsndfile-${SNDFILE_VERSION}.tar.xz
	URL_HASH ${SNDFILE_HASH}
	DOWNLOAD_DIR ${download_dir}

	BUILD_IN_SOURCE 1

	# Touch generated files to avoid autoreconf/autogen regeneration
	PATCH_COMMAND find <SOURCE_DIR> -name "Makefile.in" -exec touch {} +
	COMMAND find <SOURCE_DIR> -name "*.c" -path "*/tests/*" -exec touch {} +

	CONFIGURE_COMMAND ${wrapper_command} <SOURCE_DIR>/configure
	${COMMON_AUTOCONF_FLAGS}
	--disable-external-libs
	--disable-full-suite

	# Touch test source files after configure to prevent autogen regeneration
	BUILD_COMMAND find <SOURCE_DIR> -name "test_*.c" -exec touch {} +
	COMMAND find <SOURCE_DIR> -name "test_*.am" -exec touch {} +
	COMMAND ${wrapper_command} $(MAKE)
	INSTALL_COMMAND ${wrapper_command} $(MAKE) install
	COMMAND ${wrapper_command} $(MAKE) install-pkgconfigDATA
)

string(REPLACE "." "_" BOOST_VERSION_UNDERSCORE ${BOOST_VERSION})
ExternalProject_Add(boost
	# archives.boost.io is the official stable mirror (bintray/jfrog decommissioned)
	URL https://archives.boost.io/release/${BOOST_VERSION}/source/boost_${BOOST_VERSION_UNDERSCORE}.tar.bz2
	URL_HASH ${BOOST_HASH}
	DOWNLOAD_DIR ${download_dir}

	BUILD_IN_SOURCE 1

	# bootstrap builds b2 for the HOST (not cross-compile).
	# Explicitly use system g++ and system ld (not the NDK ones from PATH).
	CONFIGURE_COMMAND env PATH=/usr/local/bin:/usr/bin:/bin
	<SOURCE_DIR>/bootstrap.sh
	--prefix=${prefix}
	--with-toolset=gcc

	BUILD_COMMAND ""

	# Write user-config.jam then invoke b2 for cross-compilation.
	# project-config.jam (written by bootstrap with a host 'gcc' entry) is removed first.
	# NDK_CC is passed in from build.sh via -DNDK_CC=...
	INSTALL_COMMAND ${CMAKE_COMMAND} -E remove -f <SOURCE_DIR>/project-config.jam
	COMMAND ${CMAKE_COMMAND} -E remove -f <SOURCE_DIR>/user-config.jam
	COMMAND ${CMAKE_COMMAND}
	-DFILE=<SOURCE_DIR>/user-config.jam
	-DNDK_CC=${NDK_CC}
	-P ${CMAKE_SOURCE_DIR}/write_boost_user_config.cmake
	COMMAND ./b2
	-j4
	--user-config=<SOURCE_DIR>/user-config.jam
	--with-program_options
	--with-serialization
	--with-iostreams
	--with-filesystem
	--with-date_time
	--with-thread
	--with-system
	--prefix=${prefix}
	toolset=clang-android
	architecture=${BOOST_ARCH}
	variant=release
	target-os=android
	threading=multi
	threadapi=pthread
	link=static
	runtime-link=static
	cxxflags=-fPIC
	cxxflags=-frtti
	cxxflags=-fexceptions
	cxxflags=-mthumb
	cxxflags=-O2
	cflags=-fPIC
	cflags=-mthumb
	cflags=-O2
	install
)

# Only pass URL_HASH to CMake once the placeholder has been replaced by build.sh
if(SDL2_HASH MATCHES "^SHA256=TODO")
    set(SDL2_URL_HASH_ARG "")
else()
    set(SDL2_URL_HASH_ARG URL_HASH ${SDL2_HASH})
endif()

ExternalProject_Add(sdl2
	URL https://github.com/libsdl-org/SDL/releases/download/release-${SDL2_VERSION}/SDL2-${SDL2_VERSION}.tar.gz
	${SDL2_URL_HASH_ARG}
	DOWNLOAD_DIR ${download_dir}

	CONFIGURE_COMMAND ""

	BUILD_COMMAND ${wrapper_command} ndk-build
	${COMMON_NDK_BUILD_FLAGS}
	APP_LDFLAGS=-Wl,-z,max-page-size=16384

	INSTALL_COMMAND mkdir -p ${prefix}/lib/ ${prefix}/include/
	COMMAND cp libs/${app_abi}/libSDL2.so ${prefix}/lib/
	COMMAND cp -r <SOURCE_DIR>/include/ ${prefix}/include/SDL2
)

ExternalProject_Add(zlib
	URL https://github.com/madler/zlib/archive/refs/tags/v1.3.1.tar.gz
	URL_HASH SHA256=17e88863f3600672ab49182f217281b6fc4d3c762bde361935e436a95214d05c
	DOWNLOAD_DIR ${download_dir}

	CONFIGURE_COMMAND ${CMAKE_COMMAND} -S <SOURCE_DIR> -B <BINARY_DIR>
	${COMMON_CMAKE_ARGS}
	-DINSTALL_PKGCONFIGDIR=OFF

	BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR>
	INSTALL_COMMAND ${CMAKE_COMMAND} --install <BINARY_DIR>
)

ExternalProject_Add(freetype2
	DEPENDS zlib

	URL https://download.savannah.gnu.org/releases/freetype/freetype-${FREETYPE2_VERSION}.tar.gz
	URL_HASH ${FREETYPE2_HASH}
	DOWNLOAD_DIR ${download_dir}

	# Use CMake build to avoid freetype's detect.mk native compiler test
	# which fails when NDK ld is in PATH (unknown --no-add-needed argument)
	CONFIGURE_COMMAND ${CMAKE_COMMAND} -S <SOURCE_DIR> -B <BINARY_DIR>
	${COMMON_CMAKE_ARGS}
	-DFT_DISABLE_PNG=ON
	-DFT_DISABLE_BZIP2=ON
	-DFT_DISABLE_BROTLI=ON
	-DFT_DISABLE_HARFBUZZ=ON
	-DFT_DISABLE_ZLIB=OFF
	-DBUILD_SHARED_LIBS=OFF

	BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR>
	INSTALL_COMMAND ${CMAKE_COMMAND} --install <BINARY_DIR>
)

ExternalProject_Add(sdl2_ttf
	DEPENDS sdl2 freetype2

	URL https://www.libsdl.org/projects/SDL_ttf/release/SDL2_ttf-${SDL2_TTF_VERSION}.tar.gz
	URL_HASH ${SDL2_TTF_HASH}
	DOWNLOAD_DIR ${download_dir}

	PATCH_COMMAND rm -rf <SOURCE_DIR>/external
	COMMAND sed -i "/LOCAL_C_INCLUDES/a LOCAL_C_INCLUDES += ${prefix}/include/freetype2/ ${prefix}/include/SDL2" <SOURCE_DIR>/Android.mk || true
	COMMAND sed -i "/LOCAL_SHARED_LIBRARIES := SDL2/a LOCAL_LDFLAGS := -L${prefix}/lib -lSDL2 -lfreetype -lz" <SOURCE_DIR>/Android.mk || true
	COMMAND sed -i "s/FREETYPE_LIBRARY_PATH := external\\/freetype/FREETYPE_LIBRARY_PATH :=/" <SOURCE_DIR>/Android.mk || true

	CONFIGURE_COMMAND ""

	BUILD_COMMAND ${wrapper_command} ndk-build
	${COMMON_NDK_BUILD_FLAGS}
	APP_LDFLAGS=-Wl,-z,max-page-size=16384
	LOCAL_SHARED_LIBRARIES=
	SUPPORT_HARFBUZZ=false

	INSTALL_COMMAND mkdir -p ${prefix}/lib/ ${prefix}/include/SDL2/
	COMMAND cp libs/${app_abi}/libSDL2_ttf.so ${prefix}/lib/
	COMMAND cp <SOURCE_DIR>/SDL_ttf.h ${prefix}/include/SDL2/
)

ExternalProject_Add(sdl2_mixer
	DEPENDS sdl2 ogg vorbis

	URL https://www.libsdl.org/projects/SDL_mixer/release/SDL2_mixer-${SDL2_MIXER_VERSION}.tar.gz
	URL_HASH ${SDL2_MIXER_HASH}
	DOWNLOAD_DIR ${download_dir}

	PATCH_COMMAND sed -i "s|LOCAL_C_INCLUDES :=|LOCAL_C_INCLUDES := ${prefix}/include/SDL2 |" <SOURCE_DIR>/Android.mk || true
		COMMAND sed -i "/LOCAL_SHARED_LIBRARIES := SDL2/a LOCAL_LDFLAGS := -L${prefix}/lib -lSDL2" <SOURCE_DIR>/Android.mk || true

	CONFIGURE_COMMAND ""

	BUILD_COMMAND ${wrapper_command} ndk-build
	${COMMON_NDK_BUILD_FLAGS}
	APP_LDFLAGS=-Wl,-z,max-page-size=16384
	LOCAL_SHARED_LIBRARIES=
	SUPPORT_FLAC=false
	SUPPORT_OGG_STB=true
	SUPPORT_WAVPACK=false
	SUPPORT_MP3_MPG123=false
	SUPPORT_MOD_MODPLUG=false
	SUPPORT_TIMIDITY=false

	INSTALL_COMMAND mkdir -p ${prefix}/lib/ ${prefix}/include/SDL2/
	COMMAND cp libs/${app_abi}/libSDL2_mixer.so ${prefix}/lib/
	COMMAND cp <SOURCE_DIR>/include/SDL_mixer.h ${prefix}/include/SDL2/
)

ExternalProject_Add(sdl2_image
	DEPENDS sdl2

	URL https://www.libsdl.org/projects/SDL_image/release/SDL2_image-${SDL2_IMAGE_VERSION}.tar.gz
	URL_HASH ${SDL2_IMAGE_HASH}
	DOWNLOAD_DIR ${download_dir}

	PATCH_COMMAND touch <SOURCE_DIR>/IMG_WIC.c
	COMMAND sed -i "/LOCAL_C_INCLUDES/a LOCAL_C_INCLUDES += ${prefix}/include/SDL2" <SOURCE_DIR>/Android.mk || true
	COMMAND sed -i "/LOCAL_SHARED_LIBRARIES := SDL2/a LOCAL_LDFLAGS := -L${prefix}/lib -lSDL2" <SOURCE_DIR>/Android.mk || true

	CONFIGURE_COMMAND ""

	BUILD_COMMAND ${wrapper_command} ndk-build
	${COMMON_NDK_BUILD_FLAGS}
	APP_LDFLAGS=-Wl,-z,max-page-size=16384
	USE_STBIMAGE=true
	SUPPORT_JPG=false
	SUPPORT_PNG=false
	SUPPORT_WEBP=false

	INSTALL_COMMAND mkdir -p ${prefix}/lib/ ${prefix}/include/SDL2/
	COMMAND cp libs/${app_abi}/libSDL2_image.so ${prefix}/lib/
	COMMAND cp <SOURCE_DIR>/include/SDL_image.h ${prefix}/include/SDL2/
)

# gl4es ExternalProject removed (native ES 3.2)

## --- Main project ---

ExternalProject_Add(rlvm
	DEPENDS boost ogg vorbis sndfile sdl2 freetype2 sdl2_ttf sdl2_mixer sdl2_image

	DOWNLOAD_COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/../../rlvm <SOURCE_DIR>

	BUILD_IN_SOURCE 1

	CONFIGURE_COMMAND ""

	UPDATE_COMMAND ""

	BUILD_COMMAND ${wrapper_command} scons VERBOSE=1 --release --android

	INSTALL_COMMAND cp build/release/libgame.so ${prefix}/lib
)

# Create dummy GL/glew.h in sysroot for rlvm build
file(WRITE ${CMAKE_SOURCE_DIR}/toolchain/arm/sysroot/usr/include/GL/glew.h "
#ifndef __GLEW_H__
#define __GLEW_H__

#include <GLES3/gl32.h>

#ifdef __cplusplus
extern \"C\" {
#endif

#define GLEW_OK 0
#define GLEW_NO_ERROR 0

typedef unsigned int GLenum;
typedef void* GLEWContext;

#define glewInit() GLEW_OK
#define glewGetErrorString(err) \"OK\"
#define glewGetString(ver) \"dummy\"

// Define some common extension macros as true
#define GLEW_VERSION_1_1 1
#define GLEW_VERSION_1_2 1
#define GLEW_VERSION_1_3 1
#define GLEW_VERSION_1_4 1
#define GLEW_VERSION_1_5 1
#define GLEW_VERSION_2_0 1
#define GLEW_VERSION_2_1 1
#define GLEW_VERSION_3_0 1
#define GLEW_VERSION_3_1 1
#define GLEW_VERSION_3_2 1
#define GLEW_VERSION_3_3 1
#define GLEW_VERSION_4_0 1
#define GLEW_VERSION_4_1 1
#define GLEW_VERSION_4_2 1
#define GLEW_VERSION_4_3 1
#define GLEW_VERSION_4_4 1
#define GLEW_VERSION_4_5 1
#define GLEW_VERSION_4_6 1

// Define some common extensions as true
#define GLEW_ARB_vertex_buffer_object 1
#define GLEW_ARB_shader_objects 1
#define GLEW_ARB_vertex_shader 1
#define GLEW_ARB_fragment_shader 1
#define GLEW_ARB_texture_non_power_of_two 1
#define GLEW_EXT_texture_filter_anisotropic 1
#define GLEW_NV_vertex_buffer_unified_memory 1

#ifdef __cplusplus
}
#endif

#endif /* __GLEW_H__ */
")
