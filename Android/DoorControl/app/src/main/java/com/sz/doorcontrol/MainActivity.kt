package com.sz.doorcontrol

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

class MainActivity : AppCompatActivity(), View.OnClickListener, ActivityResultCallback<ActivityResult> {
    companion object {
        const val PREFS_NAME = "DoorControl"

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

    private val mHandler = Handler(Looper.getMainLooper())
    private var mActivityResultLauncher: ActivityResultLauncher<Intent>? = null
    private var mDoorButton: Button? = null
    private var mLightButton: Button? = null

    private fun updateServer() {
        val settings = getSharedPreferences(PREFS_NAME, 0)
        val name = settings.getString("server_name", "localhost")!!

        DoorService.setupServer(name, 49099)

        turnLightOn()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        mDoorButton = findViewById(R.id.doorButton)
        mDoorButton!!.setOnClickListener(this)
        mLightButton = findViewById(R.id.lightButton)
        mLightButton!!.setOnClickListener(this)
        val exitButton = findViewById<Button>(R.id.exitButton)
        exitButton.setOnClickListener(this)

        mActivityResultLauncher = registerForActivityResult(StartActivityForResult(), this)

        DoorService.setupKey(resources.openRawResource(R.raw.key))

        try {
            updateServer()
        } catch (e: Exception) {
            alert(this, true, e.message!!)
        }
    }

    override fun onClick(v: View?) {
        when (v!!) {
            mDoorButton -> openTheDoor()
            mLightButton -> turnLightOff()
            else -> finishAndRemoveTask()
        }
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        menuInflater.inflate(R.menu.menu_main, menu)
        return true
    }

    private fun command(command: String) {
        DoorService<String>().doInBackground(this, command, object : DoorService.Callback<String> {
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
    }

    private fun openTheDoor() {
        command("POST /openTheDoor")
    }

    private fun turnLightOn() {
        command("POST /turnLightOn?time=300")
    }

    private fun turnLightOff() {
        command("POST /turnLightOff")
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