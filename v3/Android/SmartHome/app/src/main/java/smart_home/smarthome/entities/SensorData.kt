package smart_home.smarthome.entities

import com.sz.charts.IGraphData
import com.sz.charts.GraphDataItem
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.time.Instant
import java.time.LocalDateTime
import kotlin.math.max
import java.time.ZoneId

data class SensorDataItem(val timestamp: Long, val value: Double) {
    companion object {
        fun parseItem(buffer: ByteBuffer): SensorDataItem {
            val timestamp = buffer.getInt().toLong() * 1000
            val value = buffer.getInt().toDouble() / 100
            return SensorDataItem(timestamp, value)
        }

        fun parseItems(buffer: ByteBuffer): List<SensorDataItem> {
            var length = buffer.getShort()
            val list = mutableListOf<SensorDataItem>()
            while (length-- > 0) {
                list.add(parseItem(buffer))
            }
            return list
        }
    }
}

data class AggregatedSensorData(
    val min: List<SensorDataItem>,
    val avg: List<SensorDataItem>,
    val max: List<SensorDataItem>)

data class SensorData(
    val locationId: Int,
    val raw: List<SensorDataItem>?,
    val aggregated: AggregatedSensorData?) {
    companion object {
        fun parseResponse(buffer: ByteBuffer, aggregated: Boolean): SensorData {
            val locationId = buffer.get().toInt()
            if (aggregated) {
                val min = SensorDataItem.parseItems(buffer)
                val avg = SensorDataItem.parseItems(buffer)
                val max = SensorDataItem.parseItems(buffer)
                return SensorData(locationId, null, AggregatedSensorData(min, avg, max))
            }
            return SensorData(locationId, SensorDataItem.parseItems(buffer), null)
        }
    }

    fun getLength(): Int {
        if (raw != null) {
            return raw.size
        }
        return max(aggregated!!.min.size, max(aggregated.avg.size, aggregated.max.size))
    }

    fun minDate(): LocalDateTime {
        if (raw != null) {
            return LocalDateTime.ofInstant(Instant.ofEpochMilli(raw.minOf { it.timestamp }), ZoneId.systemDefault())
        }
        var min = LocalDateTime.ofInstant(Instant.ofEpochMilli(aggregated!!.min.minOf { it.timestamp }), ZoneId.systemDefault())
        val avg = LocalDateTime.ofInstant(Instant.ofEpochMilli(aggregated.avg.minOf { it.timestamp }), ZoneId.systemDefault())
        if (avg < min)
            min = avg
        val max = LocalDateTime.ofInstant(Instant.ofEpochMilli(aggregated.max.minOf { it.timestamp }), ZoneId.systemDefault())
        return if (max < min) max else min
    }

    fun maxDate(): LocalDateTime {
        if (raw != null) {
            return LocalDateTime.ofInstant(Instant.ofEpochMilli(raw.maxOf { it.timestamp }), ZoneId.systemDefault())
        }
        var max = LocalDateTime.ofInstant(Instant.ofEpochMilli(aggregated!!.max.maxOf { it.timestamp }), ZoneId.systemDefault())
        val avg = LocalDateTime.ofInstant(Instant.ofEpochMilli(aggregated.avg.maxOf { it.timestamp }), ZoneId.systemDefault())
        if (avg > max)
            max = avg
        val min = LocalDateTime.ofInstant(Instant.ofEpochMilli(aggregated.min.maxOf { it.timestamp }), ZoneId.systemDefault())
        return if (max < min) min else max
    }

    fun buildGraphData(onlyAvg: Boolean): List<IGraphData> {
        if (raw != null) {
            return listOf(GraphData(raw, ""))
        } else {
            if (onlyAvg) {
                return listOf(GraphData(aggregated!!.avg, ""))
            }
            return listOf(
                GraphData(aggregated!!.min, "min"),
                GraphData(aggregated.avg, "avg"),
                GraphData(aggregated.max, "max"))
        }
    }

    fun calculateTotal(): Double {
        if (raw != null) {
            return calculateTotal(raw)
        }
        return calculateTotal(aggregated!!.avg)
    }

    private fun calculateTotal(items: List<SensorDataItem>): Double {
        return items.sumOf { it.value } / items.size
    }

    private class GraphData(sensorData: List<SensorDataItem>, override val title: String): IGraphData {
        override val data: List<GraphDataItem> = sensorData
            .map { GraphDataItem((it.timestamp / 1000).toInt(), it.value) }
    }
}

data class SensorDataResponse(val aggregated: Boolean, val response: Map<String, List<SensorData>>) {
    companion object {
        fun parseResponse(response: List<Byte>): SensorDataResponse {
            val buffer = ByteBuffer.wrap(response.toByteArray()).order(ByteOrder.LITTLE_ENDIAN)
            val result = mutableMapOf<String, List<SensorData>>()
            val aggregated = buffer.get() != 0.toByte()
            val valueTypeArray = ByteArray(4)
            while (buffer.hasRemaining()) {
                buffer.get(valueTypeArray)
                val valueType = String(valueTypeArray)
                var length = buffer.get()
                val list = mutableListOf<SensorData>()
                while (length-- > 0) {
                    list.add(SensorData.parseResponse(buffer, aggregated))
                }
                result[valueType] = list
            }
            return SensorDataResponse(aggregated, result)
        }
    }
}

data class LastSensorDataResponse(val response: Map<Int, Map<String, SensorDataItem>>) {
    companion object {
        fun parseResponse(response: List<Byte>): LastSensorDataResponse {
            val buffer = ByteBuffer.wrap(response.toByteArray()).order(ByteOrder.LITTLE_ENDIAN)
            val result = mutableMapOf<Int, Map<String, SensorDataItem>>()
            val valueTypeArray = ByteArray(4)
            var count = buffer.get()
            while (count-- > 0)
            {
                val locationId = buffer.get()
                var length = buffer.get()
                val map = mutableMapOf<String, SensorDataItem>()
                result[locationId.toInt()] = map
                while (length-- > 0) {
                    buffer.get(valueTypeArray)
                    val valueType = String(valueTypeArray)
                    val item = SensorDataItem.parseItem(buffer)
                    map[valueType] = item
                }
            }
            return LastSensorDataResponse(result)
        }
    }
}
