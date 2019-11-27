package smart_home.smarthome

import android.app.Activity
import android.content.SharedPreferences
import android.os.Bundle
import android.preference.PreferenceActivity
import android.preference.PreferenceFragment

class SettingsActivity : PreferenceActivity(), SharedPreferences.OnSharedPreferenceChangeListener {
    private var mResult: Int = 0

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val fragment = PreferencesFragment()
        fragment.setListener(this)
        fragmentManager.beginTransaction().replace(android.R.id.content, fragment).commit()
    }

    class PreferencesFragment : PreferenceFragment() {
        private var mListener: SharedPreferences.OnSharedPreferenceChangeListener? = null

        fun setListener(listener: SharedPreferences.OnSharedPreferenceChangeListener) {
            mListener = listener
        }

        override fun onCreate(savedInstanceState: Bundle?) {
            super.onCreate(savedInstanceState)
            preferenceManager.sharedPreferencesName = MainActivity.PREFS_NAME
            addPreferencesFromResource(R.xml.pref_general)
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
}