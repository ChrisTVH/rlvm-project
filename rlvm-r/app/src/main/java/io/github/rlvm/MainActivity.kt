package io.github.rlvm

import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.provider.Settings
import android.view.View
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import io.github.rlvm.databinding.ActivityMainBinding
import java.io.File
import java.net.URLDecoder

/**
 * Launcher screen.
 *
 * This activity stays in Portrait mode and shows the standard system bars.
 * Broad filesystem access is requested only when the user attempts to launch a game.
 */
class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding

    private val manageStorageSettings =
        registerForActivityResult(ActivityResultContracts.StartActivityForResult()) {
            if (hasAllFilesPermission()) {
                pickDirectory.launch(null)
            } else {
                Toast.makeText(this, R.string.permission_required_toast, Toast.LENGTH_LONG).show()
            }
        }

    private val requestReadStorage =
        registerForActivityResult(ActivityResultContracts.RequestPermission()) { granted ->
            if (granted) {
                pickDirectory.launch(null)
            } else {
                Toast.makeText(this, R.string.permission_required_toast, Toast.LENGTH_LONG).show()
            }
        }

    private val pickDirectory =
        registerForActivityResult(ActivityResultContracts.OpenDocumentTree()) { uri: Uri? ->
            if (uri == null) return@registerForActivityResult
            val path = uriToRealPath(uri)
            if (path != null) {
                saveAndLaunch(path)
            } else {
                Toast.makeText(this, R.string.error_path_resolve, Toast.LENGTH_LONG).show()
            }
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        // Standard inset handling for a non-edge-to-edge Portrait activity.
        // This ensures the content is correctly placed below the status bar.
        ViewCompat.setOnApplyWindowInsetsListener(binding.root) { view, insets ->
            val bars = insets.getInsets(WindowInsetsCompat.Type.systemBars())
            view.setPadding(bars.left, bars.top, bars.right, bars.bottom)
            insets
        }

        binding.btnLaunch.setOnClickListener {
            if (!hasAllFilesPermission()) {
                requestStoragePermission()
                return@setOnClickListener
            }
            pickDirectory.launch(null)
        }

        updateLastPathUI()
        displayAppVersion()
    }

    // ------------------------------------------------------------------
    // Storage permission
    // ------------------------------------------------------------------

    private fun hasAllFilesPermission(): Boolean =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            Environment.isExternalStorageManager()
        } else {
            checkSelfPermission(android.Manifest.permission.READ_EXTERNAL_STORAGE) ==
                    android.content.pm.PackageManager.PERMISSION_GRANTED
        }

    private fun requestStoragePermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            val intent = Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION).apply {
                data = Uri.fromParts("package", packageName, null)
            }

            if (intent.resolveActivity(packageManager) != null) {
                manageStorageSettings.launch(intent)
            } else {
                Toast.makeText(
                    this,
                    R.string.permission_restricted_fallback,
                    Toast.LENGTH_LONG
                ).show()
                pickDirectory.launch(null)
            }
        } else {
            requestReadStorage.launch(android.Manifest.permission.READ_EXTERNAL_STORAGE)
        }
    }

    // ------------------------------------------------------------------
    // URI → real filesystem path
    // ------------------------------------------------------------------

    private fun uriToRealPath(uri: Uri): String? {
        val treeId = try {
            android.provider.DocumentsContract.getTreeDocumentId(uri)
        } catch (e: Exception) {
            null
        } ?: return null

        val colon = treeId.indexOf(':')
        if (colon == -1) return null

        val volume = treeId.substring(0, colon)
        val relative = treeId.substring(colon + 1)

        // Decode percent-encoded characters (e.g., %20 -> space)
        val decodedRelative = try {
            URLDecoder.decode(relative, "UTF-8")
        } catch (e: Exception) {
            relative // Fallback to original if decoding fails
        }

        val base = if (volume.equals("primary", ignoreCase = true)) {
            Environment.getExternalStorageDirectory().absolutePath
        } else {
            "/storage/$volume"
        }

        val path = if (decodedRelative.isEmpty()) base else "$base/$decodedRelative"
        return path.takeIf { File(it).exists() }
    }

    // ------------------------------------------------------------------
    // UI
    // ------------------------------------------------------------------

    private fun displayAppVersion() {
        binding.tvVersion.text = getString(R.string.version_format, BuildConfig.VERSION_NAME)
    }

    private fun updateLastPathUI() {
        val lastPath = getSharedPreferences(PREFS_NAME, MODE_PRIVATE)
            .getString(KEY_LAST_PATH, null)
        if (lastPath != null) {
            binding.tvLastPath.text = getString(R.string.last_path, lastPath)
            binding.tvLastPath.visibility = View.VISIBLE
            binding.btnResume.visibility = View.VISIBLE
            binding.btnResume.setOnClickListener { launchGame(lastPath) }
        } else {
            binding.tvLastPath.visibility = View.GONE
            binding.btnResume.visibility = View.GONE
        }
    }

    private fun saveAndLaunch(path: String) {
        getSharedPreferences(PREFS_NAME, MODE_PRIVATE).edit()
            .putString(KEY_LAST_PATH, path)
            .apply()
        updateLastPathUI()
        launchGame(path)
    }

    private fun launchGame(homePath: String) {
        if (!File(homePath).exists()) {
            Toast.makeText(this, R.string.error_path_not_found, Toast.LENGTH_LONG).show()
            return
        }
        startActivity(Intent(this, GameActivity::class.java).apply {
            putExtra(GameActivity.EXTRA_HOME_PATH, homePath)
        })
    }

    companion object {
        private const val PREFS_NAME = "rlvm_prefs"
        private const val KEY_LAST_PATH = "last_game_path"
    }
}
