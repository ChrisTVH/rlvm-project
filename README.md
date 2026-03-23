# RLVM Project

**RLVM** is an open-source emulator for the RealLive virtual machine, used in Japanese visual novels. This repository contains the source code for the main emulator (`rlvm/`) and its Android port (`rlvm-r/`).

## Repository Structure

```
rlvm-project/
├── LICENSE.md          # GPLv3 License
├── README.md           # You are here
├── rlvm/               # Main emulator (C++)
└── rlvm-r/             # Android port (Kotlin/Gradle)
```

## rlvm/ - Main Emulator

### What is RLVM?

RLVM is a clone of the RealLive virtual machine, a proprietary engine used by Japanese visual novel developers. The emulator allows running games developed with this engine on non-Windows platforms, primarily Linux, macOS, and now Android.

### Features

- **Language**: C++11
- **Build system**: SCons
- **Supported platforms**:
  - Linux (GTK+ backend)
  - macOS (Cocoa backend)
  - Android (SDL2/OpenGL ES backend)
- **License**: GNU General Public License v3

### Main Dependencies

- **Boost** (>=1.40): Program Options, Serialization, Iostreams, Filesystem, Date/Time, Thread, System
- **SDL2**: Simple DirectMedia Layer for input/output and multimedia
- **SDL2_image**: Image loading
- **SDL2_ttf**: TrueType font rendering
- **SDL2_mixer**: Audio and music
- **OpenGL/GLEW**: 3D graphics
- **libogg, libvorbis**: Ogg Vorbis audio codec
- **libsndfile**: Audio file reading/writing
- **FreeType2**: Font rendering
- **Guichan** (0.8.2): GUI library for user interfaces
- **zita-resampler**: High-quality audio resampling

### Directory Structure

```
rlvm/
├── src/
│   ├── base/           # System base components
│   ├── effects/        # Visual effects
│   ├── encodings/      # Character encoding (CP932, CP936, CP949, UTF-8)
│   ├── libreallive/    # Library for reading RealLive engine files
│   ├── long_operations/ # Long duration operations
│   ├── machine/        # Virtual machine and bytecode execution
│   ├── modules/        # System modules (graphics, sound, events)
│   ├── platforms/      # Platform-specific implementations
│   │   ├── android/    # Android backend
│   │   ├── gtk/        # GTK+ backend for Linux
│   │   └── osx/        # Cocoa backend for macOS
│   ├── systems/        # Abstract systems (graphics, sound, events)
│   └── utilities/      # General utilities
├── vendor/             # Third-party libraries included
├── test/               # Unit tests
└── scripts/            # Utility scripts
```

### Building on Linux/macOS

```bash
cd rlvm
# Install system dependencies first
scons
```

For optimized build:
```bash
scons --release
```

For Android (cross-compilation):
```bash
scons --android
```

## rlvm-r/ - Android Port

### What is rlvm-r?

`rlvm-r` is the official RLVM port for Android. It provides a native application that allows running RealLive-based visual novels directly on Android devices.

### Features

- **Language**: Kotlin (Java 21)
- **Android SDK**: 36 (compile)
- **Minimum SDK**: 21 (Android 5.0 Lollipop)
- **NDK**: r27c (native code compilation)
- **Supported architectures**: ARM (armeabi-v7a, arm64-v8a)
- **Build system**: Gradle 9.1.0 + CMake/NDK

### Dependencies

- **AndroidX Core KTX**: Kotlin extensions for Android
- **AndroidX AppCompat**: Backward compatibility
- **Material Components**: Material Design components
- **SDL2**: Multimedia library (compiled for Android)
- **Boost**: C++ libraries (compiled for Android)
- **gl4es**: OpenGL → OpenGL ES translation layer
- **Freetype2**: Font rendering

### Directory Structure

```
rlvm-r/
├── app/
│   ├── src/main/
│   │   ├── java/io/github/rlvm/    # Kotlin code
│   │   │   ├── MainActivity.kt     # Launcher activity
│   │   │   └── GameActivity.kt     # Game activity (extends SDLActivity)
│   │   ├── jniLibs/                # Native libraries (.so) per architecture
│   │   ├── res/                    # Android resources
│   │   └── AndroidManifest.xml     # Application manifest
│   └── build.gradle.kts            # Gradle configuration
├── scripts/
│   ├── build.sh                    # Main build script
│   ├── CMakeLists.txt              # CMake configuration for dependencies
│   ├── toolchain/                  # Compilation tools (NDK)
│   └── downloads/                  # Dependency downloads
├── build.gradle.kts                # Root configuration
└── settings.gradle.kts             # Module configuration
```

### Building

The build process has two stages:

#### Stage 1: Build Native Libraries (C++ engine)

```bash
cd rlvm-r

# Build for ARM 64-bit (most modern devices)
./scripts/build.sh --arch arm64 2>&1 | tee build.log

# Build for ARM 32-bit (older devices)
./scripts/build.sh --arch arm 2>&1 | tee build.log

# Or build both architectures
./scripts/build.sh --arch arm64 2>&1 | tee build.log
./scripts/build.sh --arch arm 2>&1 | tee build.log
```

This compiles:
- SDL2 and dependencies (Boost, Ogg, Vorbis, etc.)
- The rlvm engine (libgame.so)
- Outputs to `app/src/main/jniLibs/{arch}/`

#### Stage 2: Build Android Application

```bash
cd rlvm-r

# Build debug APK (via command line)
./gradlew assembleDebug

# Build and install on connected device
./gradlew installDebug

# Or use Android Studio:
# - Open project in Android Studio
# - Run > Run 'app'
```

### Build Options

```bash
./scripts/build.sh --help
# Options:
#   --arch: Specify architecture (arm, arm64, x86, x86_64)
#   --debug: Debug build with symbols
#   --release: Optimized build (default)
#   --asan: Enable AddressSanitizer
```

### Cleaning

```bash
# Clean native libraries only (faster)
rm -rf scripts/build/arm64/rlvm-prefix
rm -rf scripts/build/arm/rlvm-prefix

# Clean everything
./scripts/clean.sh
```

## Relationship between rlvm/ and rlvm-r/

1. **rlvm** is the main emulator source code, written in C++ with cross-platform support.

2. **rlvm-r** is an Android container that:
   - Copies the source code from `rlvm/` during build
   - Compiles `rlvm` using SCons with `--android` option
   - Packages the result as `libgame.so` in the Android application
   - Provides an Android user interface for selecting and running games
   - Uses `SDLActivity` as the base for the game activity

3. **Build process**:
   ```
   rlvm-r/scripts/build.sh
   ├── Downloads NDK r27c if not exists
   ├── Downloads and verifies SDL2
   ├── Builds dependencies with CMake (Boost, Ogg, Vorbis, etc.)
   ├── Builds rlvm with SCons (using --android)
   └── Copies libgame.so to jniLibs/
   ```

## Connected Build Scripts

1. **`rlvm-r/scripts/build.sh`**: Main script that orchestrates the entire build:
   - Configures environment variables for cross-compilation
   - Downloads and sets up NDK
   - Downloads dependencies (SDL2, Boost, etc.)
   - Runs CMake to compile third-party libraries
   - Runs SCons to compile rlvm (with `--android`)
   - Copies compiled libraries to the Android application

2. **`rlvm-r/app/build.gradle.kts`**: Defines Gradle tasks:
   - `buildNativeLibs*`: Architecture-specific tasks
   - `buildNativeLibs`: Main task that depends on all architectures
   - Integrates with `assembleDebug`/`assembleRelease`

3. **`rlvm/SConstruct`**: Main build system:
   - Detects platform (Linux, macOS, Android)
   - Checks system dependencies
   - Compiles internal static libraries
   - Executes platform-specific scripts (`SConscript.android`, etc.)

## Supported Platforms

| Platform | Backend | Status |
|----------|---------|--------|
| Linux | GTK+ | Supported |
| macOS | Cocoa | Supported |
| Android | SDL2/OpenGL ES | Supported |
| Windows | (Not included) | Not supported |

## License

This project is under the **GNU General Public License v3** (GPLv3). See [LICENSE.md](LICENSE.md) for details.

## Contributions

Contributions are welcome. Please ensure to follow the existing code style and test your changes on the target platforms.

## Notes

- The project uses third-party libraries in `rlvm/vendor/` that may have their own licenses.
- For Android 15+ (API 35+), native libraries are aligned to 16 KB pages for Google Play compatibility.
- The Android app requires storage permissions to access game files.

---

## Credits

### Original Repository
- **RLVM**: https://github.com/eglaysher/rlvm

### Repositories that laid the foundations for the Android port
- **xyzz/rlvm**: https://github.com/xyzz/rlvm
- **xyzz/rlvm-android**: https://github.com/xyzz/rlvm-android

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for information on how to contribute, including the TO-DO list and completed tasks.
