plugins {
    alias(libs.plugins.android.application)
}

android {
    namespace = "io.github.rlvm"
    compileSdk = 36

    defaultConfig {
        applicationId = "io.github.rlvm"
        minSdk = 24  // Android 7.0+ required for OpenGL ES 3.2
        targetSdk = 36
        versionCode = 2
        versionName = "26.2"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"

        ndk {
            // Supported ABIs for SDL2 native libraries
            abiFilters += listOf("armeabi-v7a", "arm64-v8a")
        }

        // NDK r27c: first LTS release with 16 KB page-size aligned libc++_shared.so,
        // required for Google Play compatibility with Android 15 devices.
        ndkVersion = "27.2.12479018"
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
        debug {
            isMinifyEnabled = false
            applicationIdSuffix = ".debug"
        }
    }

    buildFeatures {
        viewBinding = true
        buildConfig = true
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_21
        targetCompatibility = JavaVersion.VERSION_21
    }

    packaging {
        resources {
            excludes += "/META-INF/{AL2.0,LGPL2.1}"
        }
    }

    sourceSets {
        getByName("main") {
            res.srcDirs("src/main/res", "src/main/res-lang")
        }
    }
}

dependencies {
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.appcompat)
    implementation(libs.androidx.constraintlayout)
    implementation("com.google.android.material:material:1.12.0")

    testImplementation(libs.junit)
    androidTestImplementation(libs.androidx.test.runner)
    androidTestImplementation(libs.espresso.core)
}

// Helper function to map architecture to ABI
fun getAbiForArch(arch: String): String {
    return when (arch) {
        "arm" -> "armeabi-v7a"
        "arm64" -> "arm64-v8a"
        "x86" -> "x86"
        "x86_64" -> "x86_64"
        else -> arch
    }
}

// Task to build native libraries using scripts/build.sh
val architectures = listOf("arm", "arm64")

// Create individual tasks for each architecture
architectures.forEach { arch ->
    val taskName = "buildNativeLibs${arch.replaceFirstChar { it.uppercase() }}"
    val outputDir = file("src/main/jniLibs/${getAbiForArch(arch)}")
    val existingLibs = outputDir.listFiles { f -> f.extension == "so" }?.isNotEmpty() ?: false

    if (existingLibs) {
        println("[$taskName] Native libs already present — skipping build for $arch.")
        // Capture only serializable primitives — no script-scope references
        // inside the task block, required for Gradle Configuration Cache.
        val outputDirPath: String = outputDir.absolutePath
        val taskLabel: String = taskName
        tasks.register(taskName) {
            description = "Builds native libraries for $arch (skipped — libs already present)"
            group = "build"
            outputs.dir(outputDirPath)

            doFirst {
                // Resolve readelf at execution time using only primitive values
                val ndkBase = System.getenv("ANDROID_NDK_HOME")
                    ?: System.getenv("NDK_HOME")
                    ?: "${System.getenv("HOME")}/Android/Sdk/ndk"
                val ndkDir = File(ndkBase)
                val llvmReadelf = ndkDir.walkTopDown()
                    .firstOrNull { it.name == "llvm-readelf" && it.canExecute() }
                    ?.absolutePath ?: "readelf"

                val soDir = File(outputDirPath)
                val misaligned = soDir.listFiles { f -> f.extension == "so" }
                    ?.filter { so ->
                        try {
                            val output =
                                ProcessBuilder(llvmReadelf, "--program-headers", so.absolutePath)
                                    .redirectErrorStream(true).start()
                                    .inputStream.bufferedReader().readText()
                            output.lines()
                                .filter { it.trimStart().startsWith("LOAD") }
                                .any { line ->
                                    val align = line.trimEnd().substringAfterLast(" ")
                                        .removePrefix("0x").toLongOrNull(16) ?: 0L
                                    align < 16384L
                                }
                        } catch (e: Exception) {
                            false
                        }
                    } ?: emptyList()

                if (misaligned.isNotEmpty()) {
                    logger.warn("[$taskLabel] WARNING: libs NOT 16 KB page-aligned (rejected by Google Play on Android 15+):")
                    misaligned.forEach { logger.warn("  - ${it.name}") }
                    logger.warn("  Run ./scripts/clean.sh and rebuild to fix.")
                }
            }
        }
    } else {
        tasks.register(taskName, Exec::class) {
            description = "Builds native libraries for $arch"
            group = "build"

            environment(mapOf("PATH" to "/usr/bin:/bin:/usr/local/bin:" + System.getenv("PATH")))
            commandLine(
                "bash",
                "-c",
                "export PATH=/usr/bin:/bin:/usr/local/bin:\$PATH && ${rootProject.projectDir}/scripts/build.sh --arch $arch"
            )

            outputs.dir(outputDir)
            dependsOn("preBuild")
        }
    }
}

// Create a main task that depends on all architecture-specific tasks
tasks.register("buildNativeLibs") {
    description = "Builds native libraries for all architectures"
    group = "build"

    architectures.forEach { arch ->
        dependsOn("buildNativeLibs${arch.replaceFirstChar { it.uppercase() }}")
    }
}

// Make assemble task depend on buildNativeLibs and ensure all mergeJniLibFolders tasks depend on buildNativeLibs
afterEvaluate {
    tasks.findByName("assembleDebug")?.dependsOn("buildNativeLibs")
    tasks.findByName("assembleRelease")?.dependsOn("buildNativeLibs")
    tasks.matching { it.name.contains("merge") && it.name.endsWith("JniLibFolders") }
        .configureEach {
            dependsOn("buildNativeLibs")
        }
}
