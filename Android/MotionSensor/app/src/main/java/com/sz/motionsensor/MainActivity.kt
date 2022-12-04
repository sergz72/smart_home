package com.sz.motionsensor

import android.annotation.SuppressLint
import android.app.Activity
import android.content.*
import android.os.Bundle
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import androidx.appcompat.app.AppCompatActivity
import android.view.Menu
import android.view.MenuItem
import androidx.activity.result.ActivityResult
import androidx.activity.result.ActivityResultCallback
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.contract.ActivityResultContracts
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.sz.motionsensor.databinding.ActivityMainBinding
import com.sz.motionsensor.entities.MotionEvent
import kotlinx.coroutines.runBlocking

class MainActivity : AppCompatActivity(), ServiceConnection, MotionService.MotionEventCallback,
    ActivityResultCallback<ActivityResult> {
    companion object {
        const val PREFS_NAME = "MotionSensor"
        private const val SETTINGS = 1
    }

    private lateinit var binding: ActivityMainBinding
    private val mHandler = Handler(Looper.getMainLooper())
    private var mService: MotionService? = null
    private var mActivityResultLauncher: ActivityResultLauncher<Intent>? = null
    private lateinit var serviceIntent: Intent
    private lateinit var mEventsView: RecyclerView
    private lateinit var mEventsViewAdapter: EventsViewAdapter

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        setSupportActionBar(binding.toolbar)

        mEventsView = findViewById(R.id.events_view)
        mEventsViewAdapter = EventsViewAdapter()
        mEventsView.hasFixedSize()
        val lm = LinearLayoutManager(this)
        lm.orientation = LinearLayoutManager.VERTICAL
        mEventsView.layoutManager = lm
        mEventsView.adapter = mEventsViewAdapter

        mActivityResultLauncher = registerForActivityResult(ActivityResultContracts.StartActivityForResult(), this)

        serviceIntent = Intent(this, MotionService::class.java)
        bindService(serviceIntent, this, Context.BIND_AUTO_CREATE)
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        // Inflate the menu; this adds items to the action bar if it is present.
        menuInflater.inflate(R.menu.menu_main, menu)
        return true
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        // Handle action bar item clicks here. The action bar will
        // automatically handle clicks on the Home/Up button, so long
        // as you specify a parent activity in AndroidManifest.xml.
        return when (item.itemId) {
            R.id.action_settings -> {
                val intent = Intent(this, SettingsActivity::class.java)
                intent.putExtra("code", SETTINGS)
                mActivityResultLauncher!!.launch(intent)
                return true
            }
            else -> super.onOptionsItemSelected(item)
        }
    }

    override fun onServiceConnected(name: ComponentName?, service: IBinder?) {
        val binder = service as MotionService.LocalBinder
        mService = binder.getService()
        mService?.setCallback(this)
        startForegroundService(serviceIntent)
    }

    override fun onServiceDisconnected(name: ComponentName?) {
        mService = null
    }

    override fun onResume() {
        super.onResume()
        showHistory()
        mService?.setCallback(this)
    }

    override fun onPause() {
        super.onPause()
        mService?.setCallback(null)
    }

    override fun onMotionEvent(e: MotionEvent) {
        mHandler.post { addEventToHistory(e) }
    }

    @SuppressLint("NotifyDataSetChanged")
    private fun addEventToHistory(e: MotionEvent) {
        val s = mService
        if (s != null) {
            mEventsViewAdapter.addData(e)
            mEventsViewAdapter.notifyDataSetChanged()
        }
    }

    @SuppressLint("NotifyDataSetChanged")
    private fun showHistory() {
        val s = mService
        if (s != null) {
            runBlocking {  mEventsViewAdapter.setData(s.getHistory()) }
            mEventsViewAdapter.notifyDataSetChanged()
        }
    }

    override fun onActivityResult(result: ActivityResult) {
        val requestCode = result.data!!.getIntExtra("code", -2)
        if (result.resultCode == Activity.RESULT_OK) {
            when (requestCode) {
                SETTINGS -> mService?.onSettingsUpdated()
            }
        }
    }
}
