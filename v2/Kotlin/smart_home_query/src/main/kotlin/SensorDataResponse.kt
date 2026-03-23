package com.sz.smart_home.query

import java.nio.ByteBuffer
import java.nio.ByteOrder

data class SensorDataValues(
    val time: Int,
    val values: Map<String, Int>
) {
    companion object {
        fun fromBinary(buffer: ByteBuffer): SensorDataValues {
            val map = mutableMapOf<String, Int>()
            var valuesCount = buffer.get()
            val time = buffer.getInt()
            while (valuesCount-- > 0) {
                val keyArray = ByteArray(4)
                buffer.get(keyArray)
                val key = String(keyArray)
                val value = buffer.getInt()
                map[key] = value
            }
            return SensorDataValues(time, map)
        }
    }
}

data class Aggregated(
    val min: Int,
    val avg: Int,
    val max: Int
)

data class SensorData(
    val date: Int,
    val values: List<SensorDataValues>?,
    val aggregated: Map<String, Aggregated>?
) {
    companion object {
        fun buildFromAggregatedResponse(buffer: ByteBuffer): SensorData {
            val date = buffer.getInt()
            val map = mutableMapOf<String, Aggregated>()
            var valuesCount = buffer.get()
            while (valuesCount-- > 0) {
                val keyArray = ByteArray(4)
                buffer.get(keyArray)
                val key = String(keyArray)
                val min = buffer.getInt()
                val avg = buffer.getInt()
                val max = buffer.getInt()
                map[key] = Aggregated(min, avg, max)
            }
            return SensorData(date, null, map)
        }

        fun buildFromResponse(buffer: ByteBuffer): SensorData {
            val date = buffer.getInt()
            var dataCount = buffer.getInt()
            val list = mutableListOf<SensorDataValues>()
            while (dataCount-- > 0) {
                list.add(SensorDataValues.fromBinary(buffer))
            }
            return SensorData(date, list, null)
        }
    }
}

data class SensorDataResponse(
    val aggregated: Boolean,
    val data: Map<Int, List<SensorData>>
) {
    companion object {
        internal fun parseResponse(decompressed: List<Byte>, aggregated: Boolean,
                                   buildSensorData: (ByteBuffer) -> SensorData): SensorDataResponse {
            val buffer = ByteBuffer.wrap(decompressed.toByteArray()).order(ByteOrder.LITTLE_ENDIAN)
            val data = mutableMapOf<Int, List<SensorData>>()
            while (buffer.hasRemaining()) {
                val sensor_id = buffer.get()
                var length = buffer.getShort()
                val list = mutableListOf<SensorData>()
                while (length-- > 0) {
                    val sdata = buildSensorData(buffer)
                    list.add(sdata)
                }
                data[sensor_id.toInt()] = list
            }
            return SensorDataResponse(aggregated, data)
        }
    }
}

data class LastSensorData(
    val date: Int,
    val value: SensorDataValues
) {
    companion object {
        internal fun parseResponse(decompressed: List<Byte>): Map<Int, LastSensorData> {
            val buffer = ByteBuffer.wrap(decompressed.toByteArray()).order(ByteOrder.LITTLE_ENDIAN)
            val data = mutableMapOf<Int, LastSensorData>()
            var length = buffer.get()
            while (length-- > 0) {
                val sensorId = buffer.get()
                val date = buffer.getInt()
                val value = SensorDataValues.fromBinary(buffer)
                data[sensorId.toInt()] = LastSensorData(date, value)
            }
            return data
        }
    }
}
