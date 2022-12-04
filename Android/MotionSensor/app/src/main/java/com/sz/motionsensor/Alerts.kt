package com.sz.motionsensor

import android.app.Notification
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import androidx.core.app.NotificationManagerCompat
import com.sz.motionsensor.entities.FLAG_ALARM

class Alerts(private val alertManager: AlertManager) {
    interface AlertManager {
        fun createAlert(text: String)
    }

    private class NotificationAlertManager(private val context: Context): AlertManager {
        private val pendingIntent: PendingIntent =
            Intent(context, MainActivity::class.java).let { notificationIntent ->
                PendingIntent.getActivity(context, 0, notificationIntent, PendingIntent.FLAG_IMMUTABLE)
            }
        private var notificationId = 10

        override fun createAlert(text: String) {
            val notification = createNotification(text)
            with(NotificationManagerCompat.from(context)) {
                // notificationId is a unique int for each notification that you must define
                notify(notificationId++, notification)
            }
        }

        private fun createNotification(text: String): Notification {
            val builder = Notification.Builder(
                context,
                MotionService.notificationChannelId
            )

            return builder
                .setContentTitle("Motion sensor")
                .setContentText(text)
                .setContentIntent(pendingIntent)
                .setSmallIcon(R.mipmap.ic_launcher_round)
                .setTicker("Motion sensor")
                .setAutoCancel(true)
                .build()
        }
    }

    companion object {
        const val MOTION = "Motion detected"
    }

    constructor(context: Context): this(NotificationAlertManager(context))

    private var alertsEnabled = true

    fun enableAlerts(enable: Boolean) {
        this.alertsEnabled = enable
    }

    fun create(flag: Int) {
        if (alertsEnabled && flag == FLAG_ALARM) {
            alertManager.createAlert(MOTION)
        }
    }
}