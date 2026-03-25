package smart_home.smarthome.entities

import com.sz.charts.IGraphData
import com.sz.charts.GraphDataItem
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.time.Instant
import java.time.LocalDateTime
import kotlin.math.max
import java.time.ZoneId
import java.time.ZoneOffset

data class SensorDataItem(val timestamp: LocalDateTime, val value: Double) {
    companion object {
        fun parseItem(buffer: ByteBuffer, timeZone: ZoneId): SensorDataItem {
            val timestamp = buffer.getInt().toLong() * 1000
            val value = buffer.getInt().toDouble() / 100
            val dateTime = Instant.ofEpochMilli(timestamp).atZone(timeZone).toLocalDateTime()
            return SensorDataItem(dateTime, value)
        }

        fun parseItems(buffer: ByteBuffer, timeZone: ZoneId): List<SensorDataItem> {
            var length = buffer.getShort()
            val list = mutableListOf<SensorDataItem>()
            while (length-- > 0) {
                list.add(parseItem(buffer, timeZone))
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
        fun parseResponse(buffer: ByteBuffer, aggregated: Boolean, timeZone: ZoneId): SensorData {
            val locationId = buffer.get().toInt()
            if (aggregated) {
                val min = SensorDataItem.parseItems(buffer, timeZone)
                val avg = SensorDataItem.parseItems(buffer, timeZone)
                val max = SensorDataItem.parseItems(buffer, timeZone)
                return SensorData(locationId, null, AggregatedSensorData(min, avg, max))
            }
            return SensorData(locationId, SensorDataItem.parseItems(buffer, timeZone), null)
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
            return raw.minOf { it.timestamp }
        }
        var min = aggregated!!.min.minOf { it.timestamp }
        val avg = aggregated.avg.minOf { it.timestamp }
        if (avg < min)
            min = avg
        val max = aggregated.max.minOf { it.timestamp }
        return if (max < min) max else min
    }

    fun maxDate(): LocalDateTime {
        if (raw != null) {
            return raw.maxOf { it.timestamp }
        }
        var max = aggregated!!.max.maxOf { it.timestamp }
        val avg = aggregated.avg.maxOf { it.timestamp }
        if (avg > max)
            max = avg
        val min = aggregated.min.maxOf { it.timestamp }
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

    private class GraphData(val sensorData: List<SensorDataItem>, override val title: String): IGraphData {
        override val data: List<GraphDataItem> = sensorData
            .map { GraphDataItem(it.timestamp.toEpochSecond(ZoneOffset.UTC).toInt(), it.value) }
    }
}

data class SensorDataResponse(val aggregated: Boolean, val response: Map<String, List<SensorData>>) {
    companion object {
        fun parseResponse(response: List<Byte>, timeZone: ZoneId): SensorDataResponse {
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
                    list.add(SensorData.parseResponse(buffer, aggregated, timeZone))
                }
                result[valueType] = list
            }
            return SensorDataResponse(aggregated, result)
        }
    }
}

data class LastSensorDataResponse(val response: Map<Int, Map<String, SensorDataItem>>) {
    companion object {
        fun parseResponse(response: List<Byte>, timeZone: ZoneId): LastSensorDataResponse {
            val buffer = ByteBuffer.wrap(response.toByteArray()).order(ByteOrder.LITTLE_ENDIAN)
            val result = mutableMapOf<Int, Map<String, SensorDataItem>>()
            val valueTypeArray = ByteArray(4)
            while (buffer.hasRemaining())
            {
                val locationId = buffer.get()
                var length = buffer.get()
                val map = mutableMapOf<String, SensorDataItem>()
                result[locationId.toInt()] = map
                while (length-- > 0) {
                    buffer.get(valueTypeArray)
                    val valueType = String(valueTypeArray)
                    val item = SensorDataItem.parseItem(buffer, timeZone)
                    map[valueType] = item
                }
            }
            return LastSensorDataResponse(result)
        }
    }
}
