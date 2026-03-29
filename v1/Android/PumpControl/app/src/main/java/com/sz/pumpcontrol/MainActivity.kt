package com.sz.pumpcontrol

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
import androidx.activity.result.ActivityResultCallback

import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.contract.ActivityResultContracts.StartActivityForResult
import com.google.gson.Gson
import com.google.gson.GsonBuilder
import com.sz.pumpcontrol.entities.PumpStatus
import java.time.Instant
import java.time.LocalDateTime
import java.time.ZoneId
import java.time.format.DateTimeFormatter
import java.util.*

class MainActivity : AppCompatActivity(), View.OnClickListener, ActivityResultCallback<ActivityResult> {
    companion object {
        const val PREFS_NAME = "PumpControl"

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
    private var mForceOffButton: Button? = null
    private var mActivateButton: Button? = null
    private var mStatus: TextView? = null

    init {
        val gsonBuilder = GsonBuilder()
        mGson = gsonBuilder.create()
    }

    private fun updateServer() {
        val settings = getSharedPreferences(PREFS_NAME, 0)
        val name = settings.getString("server_name", "localhost")!!

        PumpService.setupServer(name, 48090)

        refresh()
    }

    private fun refresh() {
        PumpService<PumpStatus>()
            .doInBackground(this, "GET /status", object : PumpService.Callback<PumpStatus> {
                override fun deserialize(response: String): PumpStatus {
                    return mGson.fromJson(response, PumpStatus::class.java)
                }

                override fun isString(): Boolean {
                    return false
                }

                override fun onResponse(response: PumpStatus) {
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

    private fun showResults(s: PumpStatus) {
        mStatus!!.text = "minPressure %d.%d maxPressure %d.%d lastPressure %d.%d\npump is %s forcedOff %s\nlastPumpOn %s\nlastPumpOff %s"
            .format(s.minPressure / 10, s.minPressure % 10, s.maxPressure / 10, s.maxPressure % 10,
                s.lastPressure / 10, s.lastPressure % 10, if (s.pumpIsOn) "On" else "Off", s.forcedOff.toString(),
                LocalDateTime.ofInstant(Instant.ofEpochSecond(s.lastPumpOn), ZoneId.systemDefault()).format(
                    DateTimeFormatter.ISO_LOCAL_DATE_TIME),
                LocalDateTime.ofInstant(Instant.ofEpochSecond(s.lastPumpOff), ZoneId.systemDefault()).format(
                    DateTimeFormatter.ISO_LOCAL_DATE_TIME)
                )
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        mStatus = findViewById(R.id.statusView)
        mForceOffButton = findViewById(R.id.forceOffButton)
        mForceOffButton!!.setOnClickListener(this)
        mActivateButton = findViewById(R.id.activateButton)
        mActivateButton!!.setOnClickListener(this)
        val exitButton = findViewById<Button>(R.id.exitButton)
        exitButton.setOnClickListener(this)

        mActivityResultLauncher = registerForActivityResult(StartActivityForResult(), this)

        PumpService.setupKey(resources.openRawResource(R.raw.key))

        try {
            updateServer()
        } catch (e: Exception) {
            alert(this, true, e.message!!)
        }
    }

    override fun onClick(v: View?) {
        when (v!!) {
            mForceOffButton -> forcePumpOff()
            mActivateButton -> activatePump()
            else -> finishAndRemoveTask()
        }
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        menuInflater.inflate(R.menu.menu_main, menu)
        return true
    }

    private fun command(command: String) {
        PumpService<String>().doInBackground(this, command, object : PumpService.Callback<String> {
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

    private fun forcePumpOff() {
        command("POST /forceOff")
    }

    private fun activatePump() {
        command("POST /activate")
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        when (item.itemId) {
            R.id.action_settings -> {
                val intent = Intent(this, SettingsActivity::class.java)
                mActivityResultLauncher!!.launch(intent)
            }
            R.id.action_refresh -> refresh()
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