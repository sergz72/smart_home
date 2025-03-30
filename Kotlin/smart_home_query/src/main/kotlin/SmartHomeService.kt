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
import javax.crypto.Cipher
import javax.crypto.spec.ChaCha20ParameterSpec
import javax.crypto.spec.SecretKeySpec
import kotlin.random.Random

enum class TimeUnit {
    hour,
    day,
    month,
    year
}

data class DateTime (
    val date: Int,
    val time: Int
)

data class DateOffset (
    val offset: Int,
    val unit: TimeUnit
)

data class SmartHomeQuery(
    val maxPoints: Short,
    val dataType: String,
    val startDate: DateTime?,
    val startDateOffset: DateOffset?,
    val period: DateOffset?
)

class SmartHomeService(keyBytes: ByteArray, hostName: String, private val port: Int) {
    private val key = SecretKeySpec(keyBytes, 0, keyBytes.size, "ChaCha20")
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
        val cipher = Cipher.getInstance("ChaCha20")
        val iv = buildIV()
        val transformed = transformIV(iv)
        val param = ChaCha20ParameterSpec(iv, 0)
        cipher.init(Cipher.ENCRYPT_MODE, key, param)
        val encrypted = cipher.doFinal(request)
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
        val cipher = Cipher.getInstance("ChaCha20")
        val param = ChaCha20ParameterSpec(iv3, 0)
        cipher.init(Cipher.ENCRYPT_MODE, key, param)
        val transformed = cipher.doFinal(iv.sliceArray(4..11))
        transformed.copyInto(iv3, 4, 0)
        return iv3
    }

    internal fun decrypt(response: ByteArray): ByteArray {
        if (response.size < 13) {
            throw IOException("Invalid response")
        }
        val cipher = Cipher.getInstance("ChaCha20")
        val iv = transformIV(response.sliceArray(0..12))
        val param = ChaCha20ParameterSpec(iv, 0)
        cipher.init(Cipher.DECRYPT_MODE, key, param)
        return cipher.doFinal(response.sliceArray(12..<response.size))
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

    private fun buildRequest(query: SmartHomeQuery): ByteArray {
        if (query.dataType.length < 3)
            throw IllegalArgumentException("dataType must be more than 2 characters")
        val buffer = ByteBuffer.allocate(16).order(ByteOrder.LITTLE_ENDIAN)
        buffer.putShort(query.maxPoints)
        val bytes = query.dataType.toByteArray()
        buffer.put(bytes[0])
        buffer.put(bytes[1])
        buffer.put(bytes[2])
        buffer.put(if (bytes.size > 3) { bytes[3] } else { 0x20 })
        if (query.startDate != null) {
            buffer.putInt(query.startDate.date)
            buffer.putInt(query.startDate.time)
        } else {
            buffer.putInt(-query.startDateOffset!!.offset)
            buffer.putInt(query.startDateOffset.unit.ordinal)
        }
        if (query.period != null) {
            buffer.put(query.period.offset.toByte())
            buffer.put(query.period.unit.ordinal.toByte())
        } else {
            buffer.putShort(0)
        }
        return buffer.array()
    }

    fun send(query: SmartHomeQuery): SensorDataResponse {
        val request = buildRequest(query)
        val sendData = encrypt(request)
        val response = sendUDP(sendData)
        val decrypted = decrypt(response)
        val decompressed = decompress(decrypted)
        return when (decompressed[0]) {
            //not aggregated
            0.toByte() -> SensorDataResponse.parseResponse(decompressed.drop(1), false, ::buildFromResponse)
            // aggregated
            1.toByte() -> SensorDataResponse.parseResponse(decompressed.drop(1), true, ::buildFromAggregatedResponse)
            // error
            2.toByte() -> {
                val message = decompressed.drop(1).toByteArray().toString(Charsets.UTF_8)
                throw IOException("Error: $message")
            }
            else -> throw IOException("Wrong response type ${decrypted[0]}")
        }
    }
}