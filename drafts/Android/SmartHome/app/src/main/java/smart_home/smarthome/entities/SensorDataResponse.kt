package smart_home.smarthome.entities

import smart_home.smarthome.ResponseError
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

data class SensorDataV2(
    val date: Int,
    val values: List<SensorDataValues>?,
    val aggregated: Map<String, Aggregated>?
) {
    companion object {
        fun buildFromAggregatedResponse(buffer: ByteBuffer): SensorDataV2 {
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
            return SensorDataV2(date, null, map)
        }

        fun buildFromResponse(buffer: ByteBuffer): SensorDataV2 {
            val date = buffer.getInt()
            var dataCount = buffer.getInt()
            val list = mutableListOf<SensorDataValues>()
            while (dataCount-- > 0) {
                list.add(SensorDataValues.fromBinary(buffer))
            }
            return SensorDataV2(date, list, null)
        }
    }
}

data class SensorDataResponse(
    val aggregated: Boolean,
    val data: Map<Int, List<SensorDataV2>>
) {
    companion object {
        private fun parseResponse(decompressed: List<Byte>, aggregated: Boolean,
                                   buildSensorData: (ByteBuffer) -> SensorDataV2): SensorDataResponse {
            val buffer = ByteBuffer.wrap(decompressed.toByteArray()).order(ByteOrder.LITTLE_ENDIAN)
            val data = mutableMapOf<Int, List<SensorDataV2>>()
            while (buffer.hasRemaining()) {
                val sensor_id = buffer.get()
                var length = buffer.getShort()
                val list = mutableListOf<SensorDataV2>()
                while (length-- > 0) {
                    val sdata = buildSensorData(buffer)
                    list.add(sdata)
                }
                data[sensor_id.toInt()] = list
            }
            return SensorDataResponse(aggregated, data)
        }

        internal fun parseResponse(decompressed: ByteArray): SensorDataResponse {
            return when (decompressed[0]) {
                //not aggregated
                0.toByte() -> parseResponse(decompressed.drop(1), false, SensorDataV2::buildFromResponse)
                // aggregated
                1.toByte() -> parseResponse(decompressed.drop(1), true, SensorDataV2::buildFromAggregatedResponse)
                // error
                else -> throw ResponseError(decompressed)
            }
        }
    }
}

data class LastSensorData(
    val date: Int,
    val value: SensorDataValues
) {
    companion object {
        private fun parseResponse(decompressed: List<Byte>): Map<Int, LastSensorData> {
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

        internal fun parseResponse(decompressed: ByteArray): Map<Int, LastSensorData> {
            return when (decompressed[0]) {
                //no error
                0.toByte() -> parseResponse(decompressed.drop(1))
                // error
                else -> throw ResponseError(decompressed)
            }
        }

        private fun toDouble(map: Map<String, Int>): Map<String, Double> {
            return map.map { (k, v) -> Pair(k, v.toDouble() / 100) }.toMap()
        }
    }

    internal fun toTimeData(): List<SensorTimeData> {
        return listOf(SensorTimeData(date, listOf(SensorDataArray(value.time, toDouble(value.values)))))
    }
}
