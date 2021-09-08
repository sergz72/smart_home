package com.sz.heatercontrol

import android.app.Activity
import android.app.AlertDialog
import android.content.Intent
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.os.CountDownTimer
import android.os.Handler
import android.os.Looper
import android.view.Menu
import android.view.MenuItem
import android.view.View
import android.widget.Button
import android.widget.TextView
import androidx.activity.result.ActivityResult
import com.google.gson.Gson
import com.google.gson.GsonBuilder
import androidx.activity.result.ActivityResultCallback

import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.contract.ActivityResultContracts.StartActivityForResult
import com.sz.heatercontrol.entities.HeaterStatus


class MainActivity : AppCompatActivity(), View.OnClickListener, ActivityResultCallback<ActivityResult> {
    companion object {
        const val PREFS_NAME = "HeaterControl"

        fun alert(activity: Activity, isError: Boolean, message: String) {
            val builder = AlertDialog.Builder(activity)
            builder.setMessage(message)
                .setTitle(if (isError) "Error" else "Response")
            val dialog = builder.create()
            dialog.show()

            if (!isError) {
                object : CountDownTimer(1000, 100) {
                    override fun onTick(millisUntilFinished: Long) {
                    }

                    override fun onFinish() {
                        dialog.dismiss()
                    }
                }.start()
            }
        }
    }

    private val mGson: Gson
    private val mHandler = Handler(Looper.getMainLooper())
    private var mActivityResultLauncher: ActivityResultLauncher<Intent>? = null
    private var mSetDayTemp: Button? = null
    private var mSetNightTemp: Button? = null
    private var mStatus: TextView? = null

    init {
        val gsonBuilder = GsonBuilder()
        mGson = gsonBuilder.create()
    }

    private fun updateServer() {
        val settings = getSharedPreferences(PREFS_NAME, 0)
        val name = settings.getString("server_name", "localhost")!!

        HeaterService.setupServer(name, 49199)

        refresh()
    }

    private fun refresh() {
        HeaterService<HeaterStatus>()
            .doInBackground(this, "GET /status", object : HeaterService.Callback<HeaterStatus> {
                override fun deserialize(response: String): HeaterStatus {
                    return mGson.fromJson(response, HeaterStatus::class.java)
                }

                override fun isString(): Boolean {
                    return false
                }

                override fun onResponse(response: HeaterStatus) {
                    mHandler.post { showResults(response) }
                }

                override fun onFailure(activity: Activity, t: Throwable?, response: String?) {
                    mHandler.post {
                        if (t != null) {
                            alert(activity, true, t.message!!)
                        } else {
                            alert(activity, true, response!!)
                        }
                    }
                }
            })
    }

    private fun showResults(s: HeaterStatus) {
        mStatus!!.text = "setTemp %d.%d lastTemp %d.%d heater is %s dayTemp %d.%d nightTemp %d.%d"
            .format(s.setTemp / 10, s.setTemp % 10, s.lastTemp / 10, s.lastTemp % 10,
                    if (s.heaterOn) "On" else "Off", s.dayTemp / 10, s.dayTemp % 10,
                    s.nightTemp / 10, s.nightTemp % 10)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        mStatus = findViewById(R.id.statusView)
        mSetDayTemp = findViewById(R.id.setDayTempButton)
        mSetDayTemp!!.setOnClickListener(this)
        mSetNightTemp = findViewById(R.id.setNightTempButton)
        mSetNightTemp!!.setOnClickListener(this)
        val exitButton = findViewById<Button>(R.id.exitButton)
        exitButton.setOnClickListener(this)

        mActivityResultLauncher = registerForActivityResult(StartActivityForResult(), this)

        HeaterService.setupKey(resources.openRawResource(R.raw.key))

        try {
            updateServer()
        } catch (e: Exception) {
            alert(this, true, e.message!!)
        }
    }

    override fun onClick(v: View?) {
        when (v!!) {
            mSetDayTemp -> setDayTemp()
            mSetNightTemp -> setNightTemp()
            else -> finishAndRemoveTask()
        }
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        menuInflater.inflate(R.menu.menu_main, menu)
        return true
    }

    private fun setTemp(command: String) {
        HeaterService<String>().doInBackground(this, command, object : HeaterService.Callback<String> {
            override fun deserialize(response: String): String {
                return response
            }

            override fun isString(): Boolean {
                return true
            }

            override fun onResponse(response: String) {
                mHandler.post { showResponse(response) }
            }

            override fun onFailure(activity: Activity, t: Throwable?, response: String?) {
                mHandler.post {
                    if (t != null) {
                        alert(activity, true, t.message!!)
                    } else {
                        alert(activity, true, response!!)
                    }
                }
            }
        })
    }

    private fun showResponse(response: String) {
        alert(this, false, response)
        refresh()
    }

    private fun setNightTemp() {
        setTemp("POST /setNightTemp")
    }

    private fun setDayTemp() {
        setTemp("POST /setDayTemp")
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        when (item.itemId) {
            R.id.action_settings -> {
                val intent = Intent(this, SettingsActivity::class.java)
                mActivityResultLauncher!!.launch(intent)
            }
            else -> return super.onOptionsItemSelected(item)
        }
        return true
    }

    override fun onActivityResult(result: ActivityResult) {
        if (result.resultCode == Activity.RESULT_OK) {
            try {
                updateServer()
            } catch (e: Exception) {
                alert(this, true, e.message!!)
            }
        }
    }
}