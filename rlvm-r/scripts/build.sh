#!/bin/bash

set -e
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $DIR

export PATH=/usr/bin:$PATH
export ARCH="arm"
ASAN="false"
BUILD_TYPE="release"
CFLAGS="-fPIC"
CXXFLAGS="-fPIC -frtti -fexceptions"
# Added -Wl,-z,max-page-size=16384 for Android 15 (16 KB page alignment) compatibility.
LDFLAGS="-Wl,--exclude-libs,libgcc.a -Wl,--exclude-libs,libatomic.a -Wl,--exclude-libs,libunwind.a -Wl,-z,max-page-size=16384"

usage() {
	echo "Usage: ./build.sh [--help] [--asan] [--arch arch] [--debug|--release]"
	echo "	--help: print this message"
	echo "	--arch: build for specified architecture [arm, arm64, x86_64, x86] (default: arm)"
	echo "	--asan: build with AddressSanitizer enabled"
	echo "	--debug: produce a debug build without optimizations"
	echo "	--release: produce a release build with optimizations (default)"
	exit 0
}

# Parse command-line arguments
while [[ $# -gt 0 ]]; do
	key="$1"

	case $key in
		--help)
			usage
			shift
			;;
		--arch)
			export ARCH="$2"
			shift 2
			;;
		--asan)
			ASAN=true
			shift
			;;
		--debug)
			BUILD_TYPE="debug"
			shift
			;;
		--release)
			BUILD_TYPE="release"
			shift
			;;
		*)
			echo "Invalid argument: $key"
			exit 1
			;;
	esac
done

if [[ $ASAN = true && $ARCH != "arm" ]]; then
	echo "AddressSanitizer is only supported on arm and aarch64 architectures"
	exit 1
fi

source ./include/version.sh

if [ $ASAN = true ]; then
	CFLAGS="$CFLAGS -fsanitize=address"
	CXXFLAGS="$CXXFLAGS -fsanitize=address"
	LDFLAGS="$LDFLAGS -fsanitize=address"
fi

if [ $BUILD_TYPE = "release" ]; then
	CFLAGS="$CFLAGS -O2"
	CXXFLAGS="$CXXFLAGS -O2"
else
	CFLAGS="$CFLAGS -O0 -g"
	CXXFLAGS="$CXXFLAGS -O0 -g"
fi

if [ $ARCH = "arm" ]; then
	CFLAGS="$CFLAGS -mthumb"
	CXXFLAGS="$CXXFLAGS -mthumb"
fi

# Check if NDK exists, if not download it
NDK_DIR="$DIR/toolchain/ndk"
if [ ! -d "$NDK_DIR" ]; then
    echo "==> NDK not found, downloading..."
    mkdir -p downloads
    mkdir -p toolchain
    
    # Download NDK r27c
    NDK_URL="https://dl.google.com/android/repository/android-ndk-r27c-linux.zip"
    NDK_ZIP="$DIR/downloads/android-ndk-r27c-linux.zip"
    
    if [ ! -f "$NDK_ZIP" ]; then
        echo "Downloading NDK from $NDK_URL..."
        wget --no-check-certificate -q -O "$NDK_ZIP" "$NDK_URL"
        if [ $? -ne 0 ]; then
            echo "ERROR: Failed to download NDK"
            exit 1
        fi
    else
        echo "Using cached NDK from $NDK_ZIP"
    fi
    
    echo "Extracting NDK..."
    unzip -q "$NDK_ZIP" -d "$DIR/toolchain/"
    mv "$DIR/toolchain/android-ndk-r27c" "$NDK_DIR"
    echo "NDK extracted to $NDK_DIR"
fi

# https://github.com/android-ndk/ndk/issues/105 to be fixed in r17

# --- SDL2 hash bootstrap ---
# SDL2 2.30.x is hosted on GitHub. On the first run we download the tarball,
# compute its SHA256, and cache it in downloads/SDL2.sha256 so CMake can verify
# it on every subsequent run without needing network access.
SDL2_VERSION="2.30.12"
SDL2_TARBALL="$DIR/downloads/SDL2-${SDL2_VERSION}.tar.gz"
SDL2_HASH_FILE="$DIR/downloads/SDL2-${SDL2_VERSION}.sha256"
SDL2_URL="https://github.com/libsdl-org/SDL/releases/download/release-${SDL2_VERSION}/SDL2-${SDL2_VERSION}.tar.gz"

mkdir -p "$DIR/downloads"

if [ ! -f "$SDL2_HASH_FILE" ]; then
    if [ ! -f "$SDL2_TARBALL" ]; then
        echo "==> Downloading SDL2 ${SDL2_VERSION}..."
        wget --no-check-certificate -q -O "$SDL2_TARBALL" "$SDL2_URL"
        if [ $? -ne 0 ]; then
            echo "ERROR: Failed to download SDL2 tarball"
            exit 1
        fi
    fi
    echo "==> Computing SDL2 SHA256..."
    SDL2_COMPUTED=$(sha256sum "$SDL2_TARBALL" | awk '{print $1}')
    echo "$SDL2_COMPUTED" > "$SDL2_HASH_FILE"
    echo "==> SDL2 SHA256: $SDL2_COMPUTED (saved to downloads/SDL2-${SDL2_VERSION}.sha256)"
    # Patch the placeholder in CMakeLists.txt with the real hash
    sed -i "s|SHA256=TODO_verify_and_replace_after_first_download|SHA256=${SDL2_COMPUTED}|" "$DIR/CMakeLists.txt"
    echo "==> CMakeLists.txt updated with verified SDL2 hash"
else
    echo "==> SDL2 hash already verified: $(cat $SDL2_HASH_FILE)"
fi
# --- end SDL2 hash bootstrap ---

echo ""
echo "================================================================================"
echo ""
echo "Build configuration:"
echo " - Architecture: $ARCH"
echo " - Build type: $BUILD_TYPE"
echo " - AddressSanitizer: $ASAN"
echo ""
echo " ------------------------------------------------------------------------------ "
echo ""
echo "Computed flags:"
echo " - CFLAGS: $CFLAGS"
echo " - CXXFLAGS: $CXXFLAGS"
echo " - LDFLAGS: $LDFLAGS"
echo ""
echo "================================================================================"
echo "(Please run ./clean.sh manually if you modify any of the options)"
echo ""

NCPU=$(grep -c ^processor /proc/cpuinfo)
echo "==> Build using $NCPU CPUs"
mkdir -p build/$ARCH/

# symlink lib64 -> lib so we don't get half the libs in one directory half in another
mkdir -p toolchain/$ARCH/sysroot/usr/lib
ln -sf lib toolchain/$ARCH/sysroot/usr/lib64

# Use the existing NDK from toolchain/ndk/
NDK_DIR="$DIR/toolchain/ndk"
NDK_BIN="$NDK_DIR/toolchains/llvm/prebuilt/linux-x86_64/bin"

# Determine the correct clang triplet
if [[ "$ARCH" == "arm" ]]; then
	CLANG_TRIPLET="armv7a-linux-androideabi"
else
	CLANG_TRIPLET="$NDK_TRIPLET"
fi

# Full path to clang
RESOLVED_CC="$NDK_BIN/${CLANG_TRIPLET}${ANDROID_API}-clang"
RESOLVED_CXX="$NDK_BIN/${CLANG_TRIPLET}${ANDROID_API}-clang++"

echo "==> CC:  $RESOLVED_CC"
echo "==> CXX: $RESOLVED_CXX"

# Change to build directory
pushd build/$ARCH/

# Determine the library directory based on architecture
if [[ "$ARCH" == "arm" ]]; then
	LIB_ARCH="arm"
	SYSROOT_LIB_DIR="arm-linux-androideabi"
elif [[ "$ARCH" == "arm64" ]]; then
	LIB_ARCH="aarch64"
	SYSROOT_LIB_DIR="aarch64-linux-android"
elif [[ "$ARCH" == "x86" ]]; then
	LIB_ARCH="i386"
	SYSROOT_LIB_DIR="i686-linux-android"
elif [[ "$ARCH" == "x86_64" ]]; then
	LIB_ARCH="x86_64"
	SYSROOT_LIB_DIR="x86_64-linux-android"
fi

# Build using the NDK's CMake toolchain file
# Detect the actual LLVM/clang version inside the NDK dynamically
LLVM_CLANG_VERSION=$(ls "$NDK_DIR/toolchains/llvm/prebuilt/linux-x86_64/lib64/clang/" 2>/dev/null | head -1)
if [ -z "$LLVM_CLANG_VERSION" ]; then
	# Fallback path used by newer NDKs (r26+) that moved lib64/clang → lib/clang
	LLVM_CLANG_LIB_DIR="$NDK_DIR/toolchains/llvm/prebuilt/linux-x86_64/lib/clang"
	LLVM_CLANG_VERSION=$(ls "$LLVM_CLANG_LIB_DIR" 2>/dev/null | head -1)
	CLANG_RUNTIME_DIR="$LLVM_CLANG_LIB_DIR/$LLVM_CLANG_VERSION/lib/linux/$LIB_ARCH"
else
	CLANG_RUNTIME_DIR="$NDK_DIR/toolchains/llvm/prebuilt/linux-x86_64/lib64/clang/$LLVM_CLANG_VERSION/lib/linux/$LIB_ARCH"
fi
echo "==> Detected LLVM Clang version: $LLVM_CLANG_VERSION"
echo "==> Clang runtime dir: $CLANG_RUNTIME_DIR"

cmake ../.. \
	-G "Unix Makefiles" \
	-DCMAKE_MAKE_PROGRAM="$(which make)" \
	-DCMAKE_TOOLCHAIN_FILE="$NDK_DIR/build/cmake/android.toolchain.cmake" \
	-DANDROID_ABI="$ABI" \
	-DANDROID_PLATFORM="android-$ANDROID_API" \
	-DANDROID_NDK="$NDK_DIR" \
	-DANDROID_STL=c++_shared \
	-DCMAKE_INSTALL_PREFIX=$DIR/toolchain/$ARCH/sysroot/usr/ \
	-DARCH=$ARCH \
	-DBUILD_TYPE=$BUILD_TYPE \
	-DNDK_TRIPLET=$NDK_TRIPLET \
	-DANDROID_API=$ANDROID_API \
	-DABI=$ABI \
	-DBOOST_ARCH=$BOOST_ARCH \
	-DBOOST_ADDRESS_MODEL=$BOOST_ADDRESS_MODEL \
	-DFFMPEG_CPU=$FFMPEG_CPU \
	-DNDK_CC="$RESOLVED_CC" \
	-DCMAKE_EXE_LINKER_FLAGS="$LDFLAGS -L$CLANG_RUNTIME_DIR -L$NDK_DIR/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/$SYSROOT_LIB_DIR/$ANDROID_API" \
	-DCMAKE_SHARED_LINKER_FLAGS="$LDFLAGS -L$CLANG_RUNTIME_DIR -L$NDK_DIR/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/$SYSROOT_LIB_DIR/$ANDROID_API"

make -j$NCPU

popd

echo "==> Installing shared libraries"

rm -rf ../app/src/main/jniLibs/$ABI/
mkdir -p ../app/src/main/jniLibs/$ABI/

# copy over libs we compiled (paths are relative to scripts/ directory)
cp $DIR/toolchain/$ARCH/sysroot/usr/lib/lib{SDL2,SDL2_mixer,SDL2_ttf,SDL2_image,game}.so ../app/src/main/jniLibs/$ABI/

# copy over libc++_shared from NDK sysroot
cp $NDK_DIR/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/$NDK_TRIPLET/libc++_shared.so ../app/src/main/jniLibs/$ABI/

# Strip symbols using the NDK's strip tool
"$NDK_BIN/llvm-strip" ../app/src/main/jniLibs/$ABI/*.so

# echo "==> Making your debugging life easier"

# # copy unstripped libs to aid debugging
# rm -rf "./build/$ARCH/symbols" && mkdir -p "./build/$ARCH/symbols"
# cp "./build/$ARCH/openal-prefix/src/openal-build/libopenal.so" "./build/$ARCH/symbols/"
# cp "./build/$ARCH/sdl2-prefix/src/sdl2-build/obj/local/$ABI/libSDL2.so" "./build/$ARCH/symbols/"
# cp "./build/$ARCH/openmw-prefix/src/openmw-build/libopenmw.so" "./build/$ARCH/symbols/"
# cp "./build/$ARCH/gl4es-prefix/src/gl4es-build/obj/local/$ABI/libGL.so" "./build/$ARCH/symbols/"

# if [ $ASAN = true ]; then
# 	cp "./toolchain/arm/lib64/clang/5.0/lib/linux/libclang_rt.asan-arm-android.so" "./build/$ARCH/symbols/"
# fi

# if [[ $ARCH = "arm" ]]; then
# 	for file in ./build/$ARCH/symbols/*.so; do
# 		PATH="$DIR/toolchain/ndk/prebuilt/linux-x86_64/bin/:$DIR/toolchain/$ARCH/$NDK_TRIPLET/bin/:$PATH" ./include/gdb-add-index $file
# 	done
# fi

echo "==> Success"
