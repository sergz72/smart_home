package com.sz.smart_home.common

import kotlinx.coroutines.*
import org.apache.commons.compress.compressors.bzip2.BZip2CompressorInputStream
import java.io.ByteArrayInputStream
import java.io.IOException
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.nio.ByteBuffer
import java.nio.ByteOrder
import kotlin.random.Random

data class NetworkServiceConfig(val prefix: ByteArray, val key: ByteArray, val hostName: String, val port: Int,
                                val timeoutMs: Int = 1000, val useBzip2: Boolean = true)
open class NetworkService(private val config: NetworkServiceConfig) {
    interface Callback<T> {
        fun onResponse(response: T)
        fun onFailure(t: Throwable)
    }

    private var port: Int = config.port
    private var address: InetAddress = InetAddress.getByName(config.hostName)

    fun updateServer(hostname: String, port: Int) {
        this.port = port
        address = InetAddress.getByName(hostname)
    }

    private fun sendUDP(data: ByteArray): ByteArray {
        val socket = DatagramSocket()
        socket.soTimeout = config.timeoutMs
        val sendPacket = DatagramPacket(data, data.size, address, port)
        socket.send(sendPacket)
        val receiveData = ByteArray(65507)
        val receivePacket = DatagramPacket(receiveData, receiveData.size)
        socket.receive(receivePacket)
        return receiveData.copyOfRange(0, receivePacket.length)
    }

    internal fun encrypt(request: ByteArray): ByteArray {
        val iv = buildIV()
        val transformed = transformIV(iv)
        val cipher = ChaCha20(config.key, iv, 0u)
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
        val cipher = ChaCha20(config.key, iv3, 0u)
        val transformed = cipher.encrypt(iv.sliceArray(4..11))
        transformed.copyInto(iv3, 4, 0)
        return iv3
    }

    internal fun decrypt(response: ByteArray): ByteArray {
        if (response.size < 13) {
            throw IOException("Invalid response")
        }
        val iv = transformIV(response.sliceArray(0..12))
        val cipher = ChaCha20(config.key, iv, 0u)
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

    @OptIn(DelicateCoroutinesApi::class)
    protected fun send(request: ByteArray, callback: Callback<ByteArray>) {
        GlobalScope.launch(Dispatchers.IO) {
            try {
                val sendData = config.prefix + encrypt(request)
                val response = sendUDP(sendData)
                println("Response size: ${response.size}")
                val decrypted = decrypt(response)
                if (config.useBzip2) {
                    val decompressed = decompress(decrypted)
                    println("Decompressed size: ${decompressed.size}")
                    callback.onResponse(decompressed)
                } else {
                    callback.onResponse(decrypted)
                }
            } catch (e: Throwable) {
                callback.onFailure(e)
            }
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
