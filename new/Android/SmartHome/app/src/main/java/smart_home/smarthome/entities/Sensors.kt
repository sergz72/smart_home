package smart_home.smarthome.entities

import java.nio.ByteBuffer
import java.nio.ByteOrder

data class Sensor(
    val id: Int,
    val dataType: String,
    val location: String,
    val locationType: String
)

class SensorsResponse {
    companion object {
        fun parseResponse(decompressed: List<Byte>): Map<Int, Sensor> {
            val buffer = ByteBuffer.wrap(decompressed.toByteArray()).order(ByteOrder.LITTLE_ENDIAN)
            val sensors = mutableMapOf<Int, Sensor>()
            var length = buffer.get()
            while (length-- > 0) {
                val id = buffer.get().toInt()
                val dataTypeBytes = ByteArray(3)
                buffer.get(dataTypeBytes)
                val dataType = String(dataTypeBytes, Charsets.UTF_8)
                val locationLength = buffer.get().toInt()
                val locationBytes = ByteArray(locationLength)
                buffer.get(locationBytes)
                val location = String(locationBytes, Charsets.UTF_8)
                val locationTypeBytes = ByteArray(3)
                buffer.get(locationTypeBytes)
                val locationType = String(locationTypeBytes, Charsets.UTF_8)
                sensors.put(id, Sensor(id, dataType, location, locationType))
            }
            return sensors
        }
    }
}