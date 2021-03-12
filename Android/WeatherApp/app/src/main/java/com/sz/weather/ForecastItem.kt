package com.sz.weather

import android.content.Context
import android.util.AttributeSet
import android.view.LayoutInflater
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.TextView
import com.sz.weather.entities.ClimacellInterval
import com.sz.weather.entities.HourlyData
import java.text.SimpleDateFormat
import java.time.ZoneId
import java.util.*
import kotlin.math.pow

class ForecastItem(ctx: Context, attrs: AttributeSet) : LinearLayout(ctx, attrs) {
    companion object {
        val FORMAT: SimpleDateFormat = SimpleDateFormat("HH:mm")
    }
    init {
        val inflater = LayoutInflater.from(ctx)
        inflater.inflate(R.layout.forecast_item, this)
    }

    private fun pressureConvert(p: Double, t: Double): Double {
        val h = 0.0065 * 135
        return p / ((1 - h / (t + h + 273.15)).pow(-5.257))
    }

    fun setData(data: HourlyData) {
        val millis = (data.dt.toLong() + 60 * 60 * 2) * 1000
        val c: Calendar = Calendar.getInstance()
        c.timeInMillis = millis
        val dt = findViewById<TextView>(R.id.date)
        dt.text = FORMAT.format(c.time)
        val t = findViewById<TextView>(R.id.temperature)
        t.text = Math.round(data.temp).toString()
        val w = findViewById<TextView>(R.id.wind)
        w.text = "Wind: ${data.windSpeed}"
        val d = findViewById<TextView>(R.id.pres)
        val p = pressureConvert(data.pressure, data.temp).toInt()
        d.text = "P: ${p} hPa"
        val i = findViewById<ImageView>(R.id.icon)
        DownloadImageTask(i).loadImage(data.weather[0].icon)
    }

    fun setClimacellData(interval: ClimacellInterval, offset: Int) {
        val c: Calendar = Calendar.getInstance()
        c.add(Calendar.HOUR_OF_DAY, offset)
        val dt = findViewById<TextView>(R.id.date)
        dt.text = FORMAT.format(c.time)
        val t = findViewById<TextView>(R.id.temperature)
        t.text = Math.round(interval.values.temperature).toString()
        val w = findViewById<TextView>(R.id.wind)
        w.text = "Wind: ${interval.values.windSpeed}"
        val d = findViewById<TextView>(R.id.pres)
        val p = interval.values.pressure.toInt()
        d.text = "P: ${p} hPa"
        val i = findViewById<ImageView>(R.id.icon)
        i.setImageResource(getDrawableFromWeatherCode(interval.values.weatherCode, c.get(Calendar.HOUR_OF_DAY)))
    }

    private fun getDrawableFromWeatherCode(weatherCode: Int, hour: Int): Int {
        return when (weatherCode) {
            1001 -> R.drawable.cloudy
            1100 -> if (hour in 7..20) R.drawable.mostly_clear_day else R.drawable.mostly_clear_night
            1101 -> if (hour in 7..20) R.drawable.partly_cloudy_day else R.drawable.partly_cloudy_night
            1102 -> R.drawable.mostly_cloudy
            2000 -> R.drawable.fog
            2100 -> R.drawable.fog_light
            4000 -> R.drawable.drizzle
            4001 -> R.drawable.rain
            4200 -> R.drawable.rain_light
            4201 -> R.drawable.rain_heavy
            5000 -> R.drawable.snow
            5001 -> R.drawable.flurries
            5100 -> R.drawable.snow_light
            5101 -> R.drawable.snow_heavy
            6001 -> R.drawable.freezing_rain
            6200 -> R.drawable.freezing_rain_light
            6201 -> R.drawable.freezing_rain_heavy
            7000 -> R.drawable.ice_pellets
            7101 -> R.drawable.ice_pellets_heavy
            7102 -> R.drawable.ice_pellets_light
            8000 -> R.drawable.tstorm
            else -> if (hour in 7..20) R.drawable.clear_day else R.drawable.clear_night
        }
    }
}