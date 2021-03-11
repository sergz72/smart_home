package smart_home.smarthome

import android.app.Activity
import android.content.SharedPreferences
import android.os.Bundle
import android.view.MenuItem
import androidx.appcompat.app.AppCompatActivity
import androidx.preference.PreferenceFragmentCompat

class SettingsActivity : AppCompatActivity(), SharedPreferences.OnSharedPreferenceChangeListener {
    private var mResult: Int = 0

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.settings_activity)
        if (savedInstanceState == null) {
            val fragment = SettingsFragment()
            fragment.setListener(this)
            supportFragmentManager
                .beginTransaction()
                .replace(R.id.settings, fragment)
                .commit()
        }
        supportActionBar?.setDisplayHomeAsUpEnabled(true)
    }

    class SettingsFragment : PreferenceFragmentCompat() {
        private var mListener: SharedPreferences.OnSharedPreferenceChangeListener? = null

        fun setListener(listener: SharedPreferences.OnSharedPreferenceChangeListener) {
            mListener = listener
        }

        override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
            preferenceManager.sharedPreferencesName = MainActivity.PREFS_NAME
            setPreferencesFromResource(R.xml.pref_general, rootKey)
        }

        override fun onResume() {
            super.onResume()
            // Set up a listener whenever a key changes
            preferenceScreen.sharedPreferences.registerOnSharedPreferenceChangeListener(mListener)
        }

        override fun onPause() {
            super.onPause()
            // Set up a listener whenever a key changes
            preferenceScreen.sharedPreferences.unregisterOnSharedPreferenceChangeListener(mListener)
        }
    }

    public override fun onResume() {
        super.onResume()
        mResult = Activity.RESULT_CANCELED
    }

    override fun onSharedPreferenceChanged(sharedPreferences: SharedPreferences, key: String) {
        mResult = Activity.RESULT_OK
    }

    override fun onBackPressed() {
        setResult(mResult)
        finish()
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        when (item.itemId) {
            android.R.id.home -> {
                onBackPressed()
                return true
            }
        }
        return super.onOptionsItemSelected(item)
    }
}