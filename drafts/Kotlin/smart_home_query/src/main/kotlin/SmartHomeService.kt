package com.sz.smart_home.query

import com.sz.smart_home.query.SensorData.Companion.buildFromAggregatedResponse
import com.sz.smart_home.query.SensorData.Companion.buildFromResponse
import org.apache.commons.compress.compressors.bzip2.BZip2CompressorInputStream
import java.io.ByteArrayInputStream
import java.io.IOException
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.nio.ByteBuffer
import java.nio.ByteOrder
import kotlin.random.Random

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

class SmartHomeService(private val key: ByteArray, hostName: String, private val port: Int) {
    private val address: InetAddress = InetAddress.getByName(hostName)

    private fun sendUDP(data: ByteArray): ByteArray {
        val socket = DatagramSocket()
        socket.soTimeout = 1000
        val sendPacket = DatagramPacket(data, data.size, address, port)
        socket.send(sendPacket)
        val receiveData = ByteArray(100000)
        val receivePacket = DatagramPacket(receiveData, receiveData.size)
        socket.receive(receivePacket)
        return receiveData.copyOfRange(0, receivePacket.length)
    }

    internal fun encrypt(request: ByteArray): ByteArray {
        val iv = buildIV()
        val transformed = transformIV(iv)
        val cipher = ChaCha20(key, iv, 0u)
        val encrypted = cipher.encrypt(request)
        val data = ByteBuffer
            .allocate(encrypted.size + iv.size)
            .put(transformed)
            .put(encrypted)
            .array()
        return data
    }

    internal fun transformIV(iv: ByteArray): ByteArray {
        val iv3 = ByteBuffer.allocate(12)
            .put(iv.sliceArray(0..3))
            .put(iv.sliceArray(0..3))
            .put(iv.sliceArray(0..3))
            .array()
        val cipher = ChaCha20(key, iv3, 0u)
        val transformed = cipher.encrypt(iv.sliceArray(4..11))
        transformed.copyInto(iv3, 4, 0)
        return iv3
    }

    internal fun decrypt(response: ByteArray): ByteArray {
        if (response.size < 13) {
            throw IOException("Invalid response")
        }
        val iv = transformIV(response.sliceArray(0..12))
        val cipher = ChaCha20(key, iv, 0u)
        return cipher.encrypt(response.sliceArray(12..<response.size))
    }

    private fun decompress(decrypted: ByteArray): ByteArray {
        val inStream = BZip2CompressorInputStream(ByteArrayInputStream(decrypted))
        inStream.use {
            return inStream.readBytes()
        }
    }

    internal fun buildIV(): ByteArray {
        val random = Random(System.currentTimeMillis())
        val iv = random.nextBytes(12)
        val time = System.currentTimeMillis() / 1000L
        ByteBuffer.allocate(Long.SIZE_BYTES)
            .order(ByteOrder.LITTLE_ENDIAN)
            .putLong(time)
            .array()
            .copyInto(iv, 4, 0)
        return iv
    }

    fun send(request: ByteArray): ByteArray {
        val sendData = encrypt(request)
        val response = sendUDP(sendData)
        println("Response size: ${response.size}")
        val decrypted = decrypt(response)
        val decompressed = decompress(decrypted)
        println("Decompressed size: ${decompressed.size}")
        return decompressed
    }

    fun send(query: SmartHomeQuery): SensorDataResponse {
        val request = query.toBinary()
        val decompressed = send(request)
        return when (decompressed[0]) {
            //not aggregated
            0.toByte() -> SensorDataResponse.parseResponse(decompressed.drop(1), false, ::buildFromResponse)
            // aggregated
            1.toByte() -> SensorDataResponse.parseResponse(decompressed.drop(1), true, ::buildFromAggregatedResponse)
            // error
            else -> throw ResponseError(decompressed)
        }
    }

    fun getSensors(): List<Sensor> {
        val request = byteArrayOf(0, 0)
        val decompressed = send(request)
        return when (decompressed[0]) {
            //no error
            0.toByte() -> SensorsResponse.parseResponse(decompressed.drop(1))
            // error
            else -> throw ResponseError(decompressed)
        }
    }

    fun getLastSensorData(days: Byte): Map<Int, LastSensorData> {
        val request = byteArrayOf(1, days)
        val decompressed = send(request)
        return when (decompressed[0]) {
            //no error
            0.toByte() -> LastSensorData.parseResponse(decompressed.drop(1))
            // error
            else -> throw ResponseError(decompressed)
        }
    }
}

class ResponseError(decompressed: ByteArray) : IOException(buildMessage(decompressed)) {
    companion object {
        fun buildMessage(decompressed: ByteArray): String {
            if (decompressed[0] == 2.toByte()) {
                val message = decompressed.drop(1).toByteArray().toString(Charsets.UTF_8)
                return "Error: $message"
            }
            return "Wrong response type ${decompressed[0]}"
        }
    }
}