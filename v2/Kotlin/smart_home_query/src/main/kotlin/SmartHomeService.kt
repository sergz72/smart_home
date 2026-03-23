package com.sz.smart_home.query

import com.sz.smart_home.common.NetworkService
import com.sz.smart_home.common.NetworkServiceConfig
import com.sz.smart_home.common.ResponseError
import com.sz.smart_home.query.SensorData.Companion.buildFromAggregatedResponse
import com.sz.smart_home.query.SensorData.Companion.buildFromResponse
import java.nio.ByteBuffer
import java.nio.ByteOrder

enum class TimeUnit {
    Day,
    Month,
    Year
}

data class DateOffset (
    val offset: Int,
    val unit: TimeUnit
)

data class SmartHomeQuery(
    val maxPoints: Short,
    val dataType: String,
    val startDate: Int?,
    val startDateOffset: DateOffset?,
    val period: DateOffset?
) {
    internal fun toBinary(): ByteArray {
        if (dataType.length < 3)
            throw IllegalArgumentException("dataType must be more than 2 characters")
        val buffer = ByteBuffer.allocate(12).order(ByteOrder.LITTLE_ENDIAN)
        buffer.put(2)
        buffer.putShort(maxPoints)
        val bytes = dataType.toByteArray()
        buffer.put(bytes[0])
        buffer.put(bytes[1])
        buffer.put(bytes[2])
        if (startDate != null) {
            buffer.putInt(startDate)
        } else {
            val offset = (-startDateOffset!!.offset shl 8) or startDateOffset.unit.ordinal
            buffer.putInt(offset)
        }
        if (period != null) {
            buffer.put(period.offset.toByte())
            buffer.put(period.unit.ordinal.toByte())
        } else {
            buffer.putShort(0)
        }
        return buffer.array()
    }
}

data class Sensor(
    val id: Int,
    val dataType: String,
    val location: String,
    val locationType: String
)

class SmartHomeService(key: ByteArray, hostName: String, port: Int): NetworkService(NetworkServiceConfig(
    prefix = byteArrayOf(),
    key = key,
    hostName = hostName,
    port = port
)) {
    fun send(query: SmartHomeQuery, callback: Callback<SensorDataResponse>) {
        val request = query.toBinary()
        send(request, object: Callback<ByteArray> {
            override fun onResponse(response: ByteArray) {
                try {
                    when (response[0]) {
                        //not aggregated
                        0.toByte() -> callback.onResponse(SensorDataResponse.parseResponse(response.drop(1), false, ::buildFromResponse))
                        // aggregated
                        1.toByte() -> callback.onResponse(SensorDataResponse.parseResponse(
                            response.drop(1),
                            true,
                            ::buildFromAggregatedResponse
                        ))
                        // error
                        else -> throw ResponseError(response)
                    }
                } catch (t: Throwable) {
                    callback.onFailure(t)
                }
            }

            override fun onFailure(t: Throwable) {
                callback.onFailure(t)
            }
        })
    }

    fun getSensors(callback: Callback<List<Sensor>>) {
        val request = byteArrayOf(0, 0)
        send(request, object: Callback<ByteArray> {
            override fun onResponse(response: ByteArray) {
                try {
                    when (response[0]) {
                        //no error
                        0.toByte() -> callback.onResponse(SensorsResponse.parseResponse(response.drop(1)))
                        // error
                        else -> throw ResponseError(response)
                    }
                } catch (t: Throwable) {
                    callback.onFailure(t)
                }
            }

            override fun onFailure(t: Throwable) {
                callback.onFailure(t)
            }
        })
    }

    fun getLastSensorData(days: Byte, callback: Callback<Map<Int, LastSensorData>>) {
        val request = byteArrayOf(1, days)
        send(request, object: Callback<ByteArray> {
            override fun onResponse(response: ByteArray) {
                try {
                    when (response[0]) {
                        //no error
                        0.toByte() -> callback.onResponse(LastSensorData.parseResponse(response.drop(1)))
                        // error
                        else -> throw ResponseError(response)
                    }
                } catch (t: Throwable) {
                    callback.onFailure(t)
                }
            }

            override fun onFailure(t: Throwable) {
                callback.onFailure(t)
            }

        })
    }
}
