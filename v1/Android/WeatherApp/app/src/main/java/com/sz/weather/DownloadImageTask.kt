package com.sz.weather

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.os.Handler
import android.os.Looper
import android.widget.ImageView
import java.io.InputStream
import java.net.URL
import java.util.concurrent.Executor
import java.util.concurrent.Executors

class DownloadImageTask(private val bmImage: ImageView) {
    companion object {
        private val executor: Executor = Executors.newSingleThreadExecutor()
        private val iconMap: MutableMap<String, Bitmap> = mutableMapOf()
    }

    private val mHandler = Handler(Looper.getMainLooper())

    fun loadImage(iconName: String) {
        var icon = iconMap[iconName]
        if (icon == null) {
            val url = "http://openweathermap.org/img/wn/$iconName@2x.png"
            executor.execute {
                try {
                    val st: InputStream = URL(url).openStream()
                    icon = BitmapFactory.decodeStream(st)
                } catch (e: Exception) {
                }
                if (icon != null) {
                    iconMap[iconName] = icon!!
                }
                mHandler.post { onPostExecute(icon) }
            }
            return
        }
        onPostExecute(icon)
    }

    private fun onPostExecute(result: Bitmap?) {
        bmImage.setImageBitmap(result)
    }
}