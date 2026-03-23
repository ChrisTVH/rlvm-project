package io.github.rlvm

import android.os.Bundle
import android.view.MenuItem
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import io.github.rlvm.databinding.ActivitySettingsBinding
import java.io.File

class SettingsActivity : AppCompatActivity() {

    private lateinit var binding: ActivitySettingsBinding
    private var gamePath: String? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        binding = ActivitySettingsBinding.inflate(layoutInflater)
        setContentView(binding.root)

        // Enable back button
        supportActionBar?.setDisplayHomeAsUpEnabled(true)

        // Standard inset handling
        ViewCompat.setOnApplyWindowInsetsListener(binding.root) { view, insets ->
            val bars = insets.getInsets(WindowInsetsCompat.Type.systemBars())
            view.setPadding(bars.left, bars.top, bars.right, bars.bottom)
            insets
        }

        // Get game path from intent
        gamePath = intent.getStringExtra(EXTRA_GAME_PATH)

        // Update subtitle to show game info
        if (gamePath != null) {
            val gameDir = File(gamePath!!)
            supportActionBar?.subtitle = getString(R.string.game_settings_for, gameDir.name)
        }

        // Load saved encoding for this game
        loadSavedEncoding()

        // Save button
        binding.btnSave.setOnClickListener {
            saveEncoding()
            finish()
        }
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return when (item.itemId) {
            android.R.id.home -> {
                finish()
                true
            }

            else -> super.onOptionsItemSelected(item)
        }
    }

    private fun loadSavedEncoding() {
        // Try to load from game's .rlvm/encoding.cfg first
        val encodingIndex = if (gamePath != null) {
            val configFile = File(gamePath, ".rlvm/encoding.cfg")
            if (configFile.exists()) {
                try {
                    val encoding = configFile.readText().trim().toInt()
                    // Map encoding value to index: -1->0, 0->1, 1->2, 2->3, 3->4, 4->5
                    when (encoding) {
                        -1 -> 0
                        in 0..4 -> encoding + 1
                        else -> 0
                    }
                } catch (e: Exception) {
                    0
                }
            } else {
                0
            }
        } else {
            0
        }

        when (encodingIndex) {
            0 -> binding.rbEncodingAuto.isChecked = true
            1 -> binding.rbEncodingCp932.isChecked = true
            2 -> binding.rbEncodingCp936.isChecked = true
            3 -> binding.rbEncodingCp1252.isChecked = true
            4 -> binding.rbEncodingCp949.isChecked = true
            5 -> binding.rbEncodingUtf8.isChecked = true
        }
    }

    private fun saveEncoding() {
        val index = when (binding.rgEncoding.checkedRadioButtonId) {
            R.id.rb_encoding_auto -> 0
            R.id.rb_encoding_cp932 -> 1
            R.id.rb_encoding_cp936 -> 2
            R.id.rb_encoding_cp1252 -> 3
            R.id.rb_encoding_cp949 -> 4
            R.id.rb_encoding_utf8 -> 5
            else -> 0
        }

        // Map index to encoding value: 0->-1, 1->0, 2->1, 3->2, 4->3, 5->4
        val encoding = when (index) {
            0 -> -1
            else -> index - 1
        }

        // Save to game's .rlvm/encoding.cfg
        if (gamePath != null) {
            val rlvmDir = File(gamePath, ".rlvm")
            if (!rlvmDir.exists()) {
                rlvmDir.mkdirs()
            }
            val configFile = File(rlvmDir, "encoding.cfg")
            try {
                if (encoding == -1) {
                    // Auto: remove config file
                    if (configFile.exists()) {
                        configFile.delete()
                    }
                } else {
                    configFile.writeText(encoding.toString())
                }
            } catch (e: Exception) {
                // Silently fail
            }
        }
    }

    companion object {
        const val EXTRA_GAME_PATH = "game_path"
    }
}