package smart_home.smarthome.entities

import android.content.res.Resources

import com.google.gson.annotations.SerializedName

import smart_home.smarthome.R
import com.sz.charts.IGraphData
import smart_home.smarthome.SmartHomeServiceV2
import java.time.LocalDateTime
import java.time.ZoneId

data class SensorDataEvent(val date: LocalDateTime, val data: Map<String, Any>) {
    fun getData(dataName: String): Double? {
        if (dataName.startsWith("Max") || dataName.startsWith("Min") ||
            dataName.startsWith("Avg") || dataName.startsWith("Sum") ||
            dataName.startsWith("Cnt")) {
            val v = data[dataName.substring(3)]
            if (v is Map<*,*>) {
                val vv = v[dataName.substring(0, 3)]
                if (vv is Double?) {
                    return vv
                }
            }
            return 0.0
        }
        val v = data[dataName]
        return v as Double?
    }

    fun getDataPresentation(resources: Resources): String {
        val result = StringBuilder()
        for ((key, value) in data) {
            val dataName = when (key) {
                "temp" -> resources.getString(R.string.temp)
                "humi" -> resources.getString(R.string.humi)
                "pres" -> resources.getString(R.string.pres)
                "vbat" -> resources.getString(R.string.vbat)
                "vcc" -> resources.getString(R.string.vcc)
                "icc" -> resources.getString(R.string.icc)
                "co2" -> resources.getString(R.string.co2)
                "lux" -> resources.getString(R.string.lux)
                else -> key
            }
            result.append("  ").append(dataName).append(": ").append(value).append('\n')
        }
        return result.toString()
    }
}

data class SensorDataArray(@SerializedName("EventTime") val eventTime: Int,
                           @SerializedName("Data") val data: Map<String, Any>)

data class SensorTimeData(@SerializedName("date") val date: Int,
                          @SerializedName("data") val data: List<SensorDataArray>) {
    fun buildTimeData(dataName: String): SensorTimeData {
        return SensorTimeData(date, data.filter { it.data.containsKey(dataName) }.toList())
    }
}

data class SensorData(@SerializedName("locationName") val locationName: String,
                      @SerializedName("locationType") val locationType: String,
                      @SerializedName("dataType") val dataType: String,
                      @SerializedName("total") val total: Int,
                      @SerializedName("timeData") val timeData: List<SensorTimeData>) {
    companion object {
        internal fun from(data: Map<Int, LastSensorData>): SensorDataResponseV1 {
            return SensorDataResponseV1(false, data.map { (k, v) -> from(k, v) })
        }

        internal fun from(data: SensorDataResponse): SensorDataResponseV1 {
            return SensorDataResponseV1(data.aggregated, data.data.map { (k, v) -> from(k, v) })
        }

        private fun from(sensorId: Int, data: List<SensorDataV2>): SensorData {
            val sensor = SmartHomeServiceV2.mSensors!!.get(sensorId)!!
            return SensorData(sensor.location, sensor.locationType, sensor.dataType, 0, data.map { it.toTimeData() })
        }

        private fun from(sensorId: Int, data: LastSensorData): SensorData {
            val sensor = SmartHomeServiceV2.mSensors!!.get(sensorId)!!
            return SensorData(sensor.location, sensor.locationType, sensor.dataType, 0, data.toTimeData())
        }
    }

    private var _series: List<SensorDataEvent>? = null

    val series get() = buildSeries()

    fun buildGraphData(title: String, dn: String, dataName: String): IGraphData {
        return GraphData(buildSensorData(dataName), title, dn)
    }

    private fun buildSensorData(dataName: String): SensorData {
        return SensorData(locationName, locationType, dataType, total, buildTimeData(dataName))
    }

    private fun buildTimeData(dataName: String): List<SensorTimeData> {
        return timeData.map { it.buildTimeData(dataName) }.toList()
    }

    fun minDate(dataName: String): LocalDateTime {
        return series.filter { it.data.containsKey(dataName) }.minOf { it.date }
    }

    fun maxDate(dataName: String): LocalDateTime {
        return series.filter { it.data.containsKey(dataName) }.maxOf { it.date }
    }

    private fun buildSeries(): List<SensorDataEvent> {
        if (_series == null)
          _series = timeData.flatMap { sd -> buildSensorDataSeries(sd) }.sortedBy { ev -> ev.date }.toList()
        return _series!!
    }

    private fun buildSensorDataSeries(sd: SensorTimeData): Iterable<SensorDataEvent> {
        return sd.data.map { d -> SensorDataEvent(buildDate(sd.date, d.eventTime), d.data) }
    }

    private fun buildDate(date: Int, eventTime: Int): LocalDateTime {
        return LocalDateTime.of(date / 10000, (date / 100) % 100, date % 100,
            eventTime / 10000, (eventTime / 100) % 100, eventTime % 100)
    }

    private class GraphData(val sensorData: SensorData, override val title: String, val dataName: String): IGraphData {
        override val size: Int
            get() = sensorData.series.size

        override fun getY(pos: Int): Double {
            return sensorData.series[pos].getData(dataName)!!
        }

        override fun getX(pos: Int): Int {
            return (sensorData.series[pos].date.atZone(ZoneId.systemDefault()).toEpochSecond()).toInt()
        }
    }
}

data class SensorDataResponseV1(val aggregated: Boolean, val response: List<SensorData>)