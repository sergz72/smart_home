package com.sz.motionsensor

import android.app.*
import android.content.Context
import android.content.Intent
import android.graphics.Color
import android.os.*
import com.sz.motionsensor.entities.MotionEvent
import kotlinx.coroutines.*
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.nio.ByteBuffer
import java.nio.ByteOrder

class MotionService: Service() {
    companion object {
        const val notificationChannelId = "Motion service channel"
    }

    interface MotionEventCallback {
        fun onMotionEvent(e: MotionEvent)
    }

    inner class LocalBinder : Binder() {
        fun getService(): MotionService = this@MotionService
    }

    private val historySize = 200

    private lateinit var wakeLock: PowerManager.WakeLock
    private lateinit var mAlerts: Alerts
    private val mBinder = LocalBinder()
    private val mMotionHistory = MotionHistory(historySize)
    private var mCallback: MotionEventCallback? = null
    @Volatile private var isServiceStarted = false

    fun setCallback(c: MotionEventCallback?) {
        mCallback = c
    }

    suspend fun getHistory(): List<MotionEvent> {
        val l = mutableListOf<MotionEvent>()
        mMotionHistory.getItems(l)
        return l
    }

    override fun onCreate() {
        super.onCreate()
        val notification = createNotification()
        startForeground(1, notification)
    }

    @OptIn(DelicateCoroutinesApi::class)
    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (isServiceStarted) return START_STICKY
        isServiceStarted = true
        wakeLock =
            (getSystemService(Context.POWER_SERVICE) as PowerManager).run {
                newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "EndlessService::lock").apply {
                    acquire()
                }
            }
        GlobalScope.launch(Dispatchers.IO) {
            val buffer = ByteArray(65527) // maximum UDP packet size
            val packet = DatagramPacket(buffer, buffer.size)
            val socket = DatagramSocket(XXXX)
            while (isServiceStarted) {
                try {
                    socket.receive(packet)
                    if (packet.length == 4) {
                        ByteBuffer.wrap(packet.data).also {
                            it.order(ByteOrder.LITTLE_ENDIAN)
                            val hcDistance = it.short
                            val vlDistance = it.short
                            processEvent(hcDistance, vlDistance)
                        }
                    }
                } catch (e: Exception) {

                }
            }
        }

        return START_STICKY
    }

    private suspend fun processEvent(hcDistance: Short, vlDistance: Short) {
        val e = mMotionHistory.addItem(hcDistance.toInt(), vlDistance.toInt())
        mCallback?.onMotionEvent(e)
        mAlerts.create(e.flag)
    }

    override fun onTaskRemoved(rootIntent: Intent) {
        val restartServiceIntent = Intent(applicationContext, MotionService::class.java).also {
            it.setPackage(packageName)
        }
        val restartServicePendingIntent: PendingIntent = PendingIntent.getService(this, 1, restartServiceIntent,
            PendingIntent.FLAG_ONE_SHOT or PendingIntent.FLAG_IMMUTABLE)
        val alarmService: AlarmManager = applicationContext.getSystemService(Context.ALARM_SERVICE) as AlarmManager
        alarmService.set(AlarmManager.ELAPSED_REALTIME, SystemClock.elapsedRealtime() + 1000, restartServicePendingIntent)
    }

    override fun onDestroy() {
        isServiceStarted = false
        wakeLock.let {
            if (it.isHeld) {
                it.release()
            }
        }
        stopForeground(STOP_FOREGROUND_REMOVE)
        stopSelf()
    }

    fun onSettingsUpdated() {
        val settings = getSharedPreferences(MainActivity.PREFS_NAME, 0)
        var hcDistance = 0
        var vlDistance = 0
        val useHCSensor = settings.getBoolean("useHCSensor", true)
        if (useHCSensor) {
            hcDistance = settings.getString("hcAlertDistance", "0")!!.toInt()
        }
        val useVLSensor = settings.getBoolean("useVLSensor", true)
        if (useVLSensor) {
            vlDistance = settings.getString("vlAlertDistance", "0")!!.toInt()
        }
        mMotionHistory.setParameters(hcDistance, vlDistance)
        mAlerts.enableAlerts(!settings.getBoolean("disableAlarm", false))
    }

    override fun onBind(intent: Intent?): IBinder {
        mAlerts = Alerts(this)
        onSettingsUpdated()
        return mBinder
    }

    private fun createNotification(): Notification {
        val notificationManager = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        val channel = NotificationChannel(
            notificationChannelId,
            "MotionSensor Service notifications channel",
            NotificationManager.IMPORTANCE_HIGH
        ).let {
            it.description = notificationChannelId
            it.lightColor = Color.RED
            it
        }
        notificationManager.createNotificationChannel(channel)

        val pendingIntent: PendingIntent = Intent(this, MainActivity::class.java).let { notificationIntent ->
            PendingIntent.getActivity(this, 0, notificationIntent, PendingIntent.FLAG_IMMUTABLE)
        }

        val builder = Notification.Builder(
            this,
            notificationChannelId
        )

        return builder
            .setContentTitle("MotionSensor service")
            .setContentText("MotionSensor service working")
            .setContentIntent(pendingIntent)
            .setSmallIcon(R.mipmap.ic_launcher)
            .setTicker("MotionSensor")
            .setAutoCancel(true)
            .build()
    }
}