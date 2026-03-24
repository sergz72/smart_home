package smart_home.smarthome.entities

import android.content.res.Resources
import smart_home.smarthome.R
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.time.ZoneId
import java.util.HashSet
import kotlin.collections.component1
import kotlin.collections.component2
import kotlin.collections.iterator

data class Location(val name: String, val locationType: String) {
    companion object {
        fun parseResponse(buffer: ByteBuffer): Map<Int, Location> {
            val locations = mutableMapOf<Int, Location>()
            while (buffer.hasRemaining()) {
                val locationId = buffer.get()
                val location = parseLocation(buffer)
                locations[locationId.toInt()] = location
            }
            return locations
        }

        private fun parseLocation(buffer: ByteBuffer): Location {
            val length = buffer.get()
            val nameArray = ByteArray(length.toInt())
            buffer.get(nameArray)
            val name = String(nameArray)
            val typeArray = ByteArray(3)
            buffer.get(typeArray)
            val locationType = String(typeArray)
            return Location(name, locationType)
        }
    }
}

data class Locations(val timeZone: ZoneId, val locations: Map<Int, Location>) {
    companion object {
        fun parseResponse(data: List<Byte>): Locations {
            val buffer = ByteBuffer.wrap(data.toByteArray()).order(ByteOrder.LITTLE_ENDIAN)
            val length = buffer.get()
            val tzArray = ByteArray(length.toInt())
            buffer.get(tzArray)
            val timeZone = ZoneId.of(String(tzArray))
            return Locations(timeZone, Location.parseResponse(buffer))
        }

        fun getDataPresentation(resources: Resources, valueType: String): String {
            return when (valueType) {
                "temp" -> resources.getString(R.string.temp)
                "humi" -> resources.getString(R.string.humi)
                "pres" -> resources.getString(R.string.pres)
                "vbat" -> resources.getString(R.string.vbat)
                "vcc " -> resources.getString(R.string.vcc)
                "icc " -> resources.getString(R.string.icc)
                "co2 " -> resources.getString(R.string.co2)
                "lux " -> resources.getString(R.string.lux)
                else -> valueType
            }
        }
    }

    fun isExtLocation(locationId: Int): Boolean {
        return locations[locationId]!!.locationType == "ext"
    }
}