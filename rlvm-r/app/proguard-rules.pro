# RLVM ProGuard rules

# Keep SDL2 JNI bridge intact — native code calls these directly
-keep class org.libsdl.app.** { *; }

# Keep our own Activity classes (referenced from Manifest and native)
-keep class io.github.rlvm.** { *; }

# Preserve JNI-called methods (SDL2 uses reflection to locate them)
-keepclassmembers class * {
    @android.webkit.JavascriptInterface <methods>;
}

# Remove verbose logging in release builds
-assumenosideeffects class android.util.Log {
    public static *** d(...);
    public static *** v(...);
    public static *** i(...);
}
