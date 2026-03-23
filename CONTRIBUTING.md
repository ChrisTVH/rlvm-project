# Contributing to RLVM

Thank you for your interest in contributing to RLVM! This document outlines how you can help improve the project.

## How to Contribute

1. **Fork the repository**
2. **Create a feature branch**: `git checkout -b feature/your-feature`
3. **Make your changes**
4. **Test your changes** on the target platforms
5. **Submit a pull request**

## Code Style

- **C++**: Follow existing code style in `rlvm/src/`
- **Kotlin**: Follow Android/Kotlin conventions in `rlvm-r/app/`
- Use the existing `.clang-format` for C++ formatting

## Build Environment

See [README.md](README.md) for build instructions.

## TO-DO

### High Priority
- [ ] **Complete UTF-8 support**: Fix protagonist dialogs (currently empty with UTF-8 encoding)
  - Issue: Character name markers (e.g., 0x81 0x96) are not processed correctly
  - Issue: Protagonist text arrives via `strout` opcode, not `TextoutElement`
  - Issue: Mixed CP932/UTF-8 encoding in bytecode causes text corruption
  - Issue: `DisplayName()` is never called for protagonist dialogs

### Medium Priority
- [ ] **Font settings**: Allow users to configure font type and size
  - Add font selection in Settings activity
  - Allow adjusting text size for better readability
  - Support custom font files

### Low Priority
- [ ] **Fix transitions**: Some transitions cut the screen in half
  - Investigate screen transition rendering
  - Fix incorrect screen capture during transitions

- [ ] **Fix filters/shaders**: Visual filters not rendering correctly
  - Investigate shader compatibility with OpenGL ES
  - Fix tone curve and color filter rendering

### Completed

**Android Application (rlvm-r vs xyzz/rlvm-android)**:
- [x] Migrated from Java to Kotlin (MainActivity, GameActivity, SettingsActivity)
- [x] Updated from support library to AndroidX
- [x] Material Design 3 UI with dark theme
- [x] Build system: Gradle 9.1.0 with Kotlin DSL (build.gradle.kts)
- [x] compileSdkVersion/targetSdkVersion: 36 (was 27)
- [x] Package name: io.github.rlvm (was is.xyz.rlvm)
- [x] Modern Gradle plugins (com.android.application, org.jetbrains.kotlin.android)

**Settings & Configuration**:
- [x] SettingsActivity for encoding configuration
- [x] Encoding selector (Auto, CP932, CP936, CP1252, CP949, UTF-8 BETA)
- [x] Per-game encoding config saved in .rlvm/encoding.cfg
- [x] Multi-language support (English, Spanish, Japanese)
- [x] Localized strings in res-lang/values-*/lang_*.xml

**Native Engine (rlvm C++ vs rlvm-o/)**:
- [x] UTF-8 encoding support (encoding=4)
- [x] UTF-8 codec implementation (utf8.cc, utf8.h)
- [x] JisEncoded CP1252 character handling (á, é, í, ó, ú, ñ, ¿, ¡)
- [x] UTF-8 accumulator in RLMachine for fragmented bytes
- [x] Encoding override from user config (LoadEncodingConfig)
- [x] Parser bytecode updated for UTF-8 detection
- [x] parseNames() updated for UTF-8 multi-byte characters
- [x] string_utilities: CopyOneUtf8Character, utf8_char_length helpers
- [x] rlBabel compatibility with UTF-8 encoding

**UI Improvements**:
- [x] Folder picker with space support in paths
- [x] Version display in launcher
- [x] Settings FAB button for quick access
- [x] Activity settings with radio buttons for encoding
- [x] Material Card views for settings sections

**Build System**:
- [x] build.sh script for native library compilation
- [x] CMakeLists.txt for dependency management
- [x] Support for arm and arm64 architectures
- [x] 16KB page alignment for Android 15+ compatibility

## Reporting Issues

Please use GitHub Issues to report bugs. Include:
- Device model and Android version
- Steps to reproduce
- Expected vs actual behavior
- Logcat output if possible

## License

By contributing, you agree that your contributions will be licensed under GPLv3.
