package smart_home.smarthome.entities

import android.content.res.Resources

import com.google.gson.annotations.SerializedName

import java.util.Collections
import java.util.Date

import smart_home.smarthome.IGraphData
import smart_home.smarthome.R

data class SensorData(@SerializedName("locationName") override val seriesColumnData: String,
                      @SerializedName("locationType") val locationType: String,
                      @SerializedName("dataType") val dataType: String,
                      @SerializedName("date") override val date: Date,
                      @SerializedName("data") override val data: Map<String, Any>) : IGraphData {
    companion object {

        fun build(locationName: String, date: Date, dataName: String, value: Any): SensorData {
            return SensorData(locationName, "", "", date, Collections.singletonMap(dataName, value))
        }
    }

    override fun getData(dataName: String): Double? {
        return data[dataName] as Double?
    }

    fun getDataPresentation(resources: Resources): String {
        val result = StringBuilder()
        for ((key, value) in data) {
            val dataName: String
            when (key) {
                "temp" -> dataName = resources.getString(R.string.temp)
                "humi" -> dataName = resources.getString(R.string.humi)
                "pres" -> dataName = resources.getString(R.string.pres)
                "vbat" -> dataName = resources.getString(R.string.vbat)
                "vcc" -> dataName = resources.getString(R.string.vcc)
                else -> dataName = key
            }
            result.append("  ").append(dataName).append(": ").append(value).append('\n')
        }
        return result.toString()
    }
}
