package com.sz.smart_home.common

import org.apache.commons.compress.compressors.bzip2.BZip2CompressorInputStream
import java.io.ByteArrayInputStream
import java.io.IOException
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.nio.ByteBuffer
import java.nio.ByteOrder
import kotlin.random.Random

open class NetworkService(private val key: ByteArray, hostName: String, private val port: Int) {
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

    protected fun send(request: ByteArray): ByteArray {
        val sendData = encrypt(request)
        val response = sendUDP(sendData)
        println("Response size: ${response.size}")
        val decrypted = decrypt(response)
        val decompressed = decompress(decrypted)
        println("Decompressed size: ${decompressed.size}")
        return decompressed
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
