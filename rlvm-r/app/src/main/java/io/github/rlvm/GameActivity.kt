package io.github.rlvm

import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.system.Os
import android.util.Log
import android.view.WindowManager
import android.widget.Toast
import android.window.OnBackInvokedCallback
import android.window.OnBackInvokedDispatcher
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import org.libsdl.app.SDLActivity
import java.io.File
import java.io.FileWriter
import java.io.PrintWriter
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * Host activity for the SDL2 native engine.
 *
 * This activity runs in its own process (:game) as defined in the manifest.
 * This isolation ensures that when the activity is finished, we can safely
 * kill the process to clean up native engine state without affecting the launcher.
 */
class GameActivity : SDLActivity() {

    private val handler = Handler(Looper.getMainLooper())
    private var backPressedOnce = false
    private val resetBackFlag = Runnable { backPressedOnce = false }

    private val backInvokedCallback: Any? by lazy {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            OnBackInvokedCallback { handleBack() }
        } else null
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        setupCrashHandler()

        val homePath = intent.getStringExtra(EXTRA_HOME_PATH)
            ?: getExternalFilesDir(null)?.absolutePath
            ?: filesDir.absolutePath

        Log.v(TAG, "Setting HOME to: $homePath")
        Os.setenv("LIBGL_ES", "2", true)
        Os.setenv("HOME", homePath, true)

        // Edge-to-edge MUST happen before super.onCreate()
        WindowCompat.setDecorFitsSystemWindows(window, false)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            window.attributes = window.attributes.also {
                it.layoutInDisplayCutoutMode =
                    WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
            }
        }

        super.onCreate(savedInstanceState)

        // Hide system bars for immersive fullscreen
        val controller = WindowInsetsControllerCompat(window, window.decorView)
        controller.hide(
            WindowInsetsCompat.Type.systemBars() or
                    WindowInsetsCompat.Type.displayCutout()
        )
        controller.systemBarsBehavior =
            WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE

        mLayout?.let { layout ->
            layout.fitsSystemWindows = false
            ViewCompat.setOnApplyWindowInsetsListener(layout) { v, _ ->
                v.setPadding(0, 0, 0, 0)
                WindowInsetsCompat.CONSUMED
            }
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            @Suppress("NewApi")
            onBackInvokedDispatcher.registerOnBackInvokedCallback(
                OnBackInvokedDispatcher.PRIORITY_DEFAULT,
                backInvokedCallback as OnBackInvokedCallback
            )
        }
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) {
            val controller = WindowInsetsControllerCompat(window, window.decorView)
            controller.hide(
                WindowInsetsCompat.Type.systemBars() or
                        WindowInsetsCompat.Type.displayCutout()
            )
        }
    }

    @Deprecated("Superseded by OnBackInvokedCallback on API 33+")
    override fun onBackPressed() {
        handleBack()
    }

    private fun handleBack() {
        if (backPressedOnce) {
            handler.removeCallbacks(resetBackFlag)
            exitClean()
            return
        }
        backPressedOnce = true
        Toast.makeText(this, R.string.back_again_to_exit, Toast.LENGTH_SHORT).show()
        handler.removeCallbacks(resetBackFlag)
        handler.postDelayed(resetBackFlag, BACK_TIMEOUT_MS)
    }

    /**
     * Finishes the activity and kills the (:game) process to leave native state clean.
     */
    private fun exitClean() {
        Log.v(TAG, "exitClean: finishing and killing process")
        finish()
        android.os.Process.killProcess(android.os.Process.myPid())
    }

    override fun onDestroy() {
        handler.removeCallbacks(resetBackFlag)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            @Suppress("NewApi")
            (backInvokedCallback as? OnBackInvokedCallback)?.let {
                onBackInvokedDispatcher.unregisterOnBackInvokedCallback(it)
            }
        }
        // Force process death on finish to ensure native engine resources are released.
        // This handles cases where the game closes from an internal menu.
        if (isFinishing) {
            android.os.Process.killProcess(android.os.Process.myPid())
        }
        super.onDestroy()
    }

    private fun setupCrashHandler() {
        val defaultHandler = Thread.getDefaultUncaughtExceptionHandler()
        Thread.setDefaultUncaughtExceptionHandler { thread, throwable ->
            try {
                getExternalFilesDir(null)?.let { dir ->
                    val logFile = File(dir, "rlvm_crash_log.txt")
                    FileWriter(logFile, true).use { writer ->
                        PrintWriter(writer).use { pw ->
                            val fmt = SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.getDefault())
                            pw.println("=== CRASH REPORT ${fmt.format(Date())} ===")
                            pw.println("Thread: ${thread.name}")
                            throwable.printStackTrace(pw)
                            pw.println("=== END REPORT ===")
                            pw.println()
                        }
                    }
                }
            } catch (e: Exception) {
                Log.e(TAG, "Failed to write crash log", e)
            }
            defaultHandler?.uncaughtException(thread, throwable)
        }
    }

    override fun getLibraries(): Array<String> = arrayOf("SDL2", "game")

    companion object {
        const val EXTRA_HOME_PATH = "io.github.rlvm.EXTRA_HOME_PATH"
        private const val TAG = "GameActivity"
        private const val BACK_TIMEOUT_MS = 2000L
    }
}
