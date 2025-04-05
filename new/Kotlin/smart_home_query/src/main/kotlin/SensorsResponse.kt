package com.sz.smart_home.query

import java.nio.ByteBuffer
import java.nio.ByteOrder

class SensorsResponse {
    companion object {
        fun parseResponse(decompressed: List<Byte>): List<Sensor> {
            val buffer = ByteBuffer.wrap(decompressed.toByteArray()).order(ByteOrder.LITTLE_ENDIAN)
            val sensors = mutableListOf<Sensor>()
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
                sensors.add(Sensor(id, dataType, location, locationType))
            }
            return sensors
        }
    }
}