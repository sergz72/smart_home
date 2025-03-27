package com.sz.smart_home.query

import org.apache.commons.compress.compressors.bzip2.BZip2CompressorInputStream
import java.io.ByteArrayInputStream
import java.io.IOException
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.file.Files
import java.nio.file.Paths
import javax.crypto.Cipher
import javax.crypto.spec.ChaCha20ParameterSpec
import javax.crypto.spec.SecretKeySpec
import kotlin.random.Random

fun usage() {
    println("Usage: java -jar smart_home_query.jar keyFileName hostName port query")
}

fun sendUDP(hostName: String, port: Int, data: ByteArray): ByteArray {
    val socket = DatagramSocket()
    socket.soTimeout = 1000
    val sendPacket = DatagramPacket(data, data.size, InetAddress.getByName(hostName), port)
    socket.send(sendPacket)
    val receiveData = ByteArray(100000)
    val receivePacket = DatagramPacket(receiveData, receiveData.size)
    socket.receive(receivePacket)
    return receiveData.copyOfRange(0, receivePacket.length)
}

fun encrypt(request: ByteArray, key: SecretKeySpec): ByteArray {
    val cipher = Cipher.getInstance("ChaCha20")
    val param = ChaCha20ParameterSpec(buildIV(key), 0)
    cipher.init(Cipher.ENCRYPT_MODE, key, param)
    return cipher.doFinal(request)
}

fun transformIV(key: SecretKeySpec, iv: ByteArray): ByteArray {
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

fun decrypt(response: ByteArray, key: SecretKeySpec): ByteArray {
    if (response.size < 13) {
        throw IOException("Invalid response")
    }
    val cipher = Cipher.getInstance("ChaCha20")
    val param = ChaCha20ParameterSpec(transformIV(key, response.sliceArray(0..12)), 0)
    cipher.init(Cipher.DECRYPT_MODE, key, param)
    return cipher.doFinal(response.sliceArray(12..<response.size))
}

fun decompress(decrypted: ByteArray): String {
    val inStream = BZip2CompressorInputStream(ByteArrayInputStream(decrypted))
    inStream.use {
        return inStream.readBytes().toString(Charsets.UTF_8)
    }
}

fun buildIV(key: SecretKeySpec): ByteArray {
    val random = Random(System.currentTimeMillis())
    val iv = random.nextBytes(12)
    val time = System.currentTimeMillis() / 1000L
    ByteBuffer.allocate(Long.SIZE_BYTES)
        .putLong(time)
        .order(ByteOrder.LITTLE_ENDIAN)
        .array()
        .copyInto(iv, 4, 0)
    return transformIV(key, iv)
}

fun buildKeyValue(parameter: String): Pair<String, String> {
    val parts = parameter.split("=")
    if (parts.size != 2) {
        throw IllegalArgumentException("Invalid parameter $parameter")
    }
    return Pair(parts[0], parts[1])
}

fun parseDate(dateStr: String): Pair<Int, Int> {
    if (dateStr.startsWith("-")) {
        val period = parsePeriod(dateStr.substring(1))
        return Pair(period.first.toInt(), period.first.toInt())
    }
    val datetime = dateStr.toLong()
    val date = datetime / 1000000
    val time = datetime % 1000000
    return Pair(date.toInt(), time.toInt())
}

fun parsePeriod(periodStr: String?): Pair<Byte, Byte> {
    if (periodStr == null) {
        return Pair(0, 0)
    }
    val unit: Byte = when (periodStr.last()) {
        'h' -> 0
        'd' -> 1
        'm' -> 2
        'y' -> 3
        else -> throw IllegalArgumentException("Invalid period $periodStr")
    }
    val value = periodStr.substring(0, periodStr.length - 1).toByte()
    return Pair(value, unit)
}

fun buildRequest(query: String): ByteArray {
    val parameters = query
        .split('&')
        .associate { buildKeyValue(it) }
    val dataType = parameters.getValue("dataType")
    if (dataType.length < 3)
        throw IllegalArgumentException("dataType must be more than 2 characters")
    val maxPoints = parameters.getOrDefault("maxPoints", "0").toShort()
    println("maxPoints: $maxPoints")
    println("dataType: $dataType")
    val startDate = parseDate(parameters.getValue("startDate"))
    println("startDate.first: ${startDate.first}")
    println("startDate.second: ${startDate.second}")
    val period = parsePeriod(parameters["period"])
    println("period.first: ${period.first}")
    println("period.second: ${period.second}")

    val buffer = ByteBuffer.allocate(16).order(ByteOrder.LITTLE_ENDIAN)
    buffer.putShort(maxPoints)
    val bytes = dataType.toByteArray()
    buffer.put(bytes[0])
    buffer.put(bytes[1])
    buffer.put(bytes[2])
    buffer.put(if (bytes.size > 3) { bytes[3] } else { 0 })
    buffer.putInt(startDate.first)
    buffer.putInt(startDate.second)
    buffer.put(period.first)
    buffer.put(period.second)
    return buffer.array()
}

fun main(args: Array<String>) {
    if (args.size != 4) {
        usage()
        return
    }
    val keyBytes = Files.readAllBytes(Paths.get(args[0]))
    val hostName = args[1]
    val port = args[2].toInt()
    val query = args[3]

    val key = SecretKeySpec(keyBytes, 0, keyBytes.size, "ChaCha20")

    val request = buildRequest(query)
    val sendData = encrypt(request, key)
    val response = sendUDP(hostName, port, sendData)
    val decrypted = decrypt(response, key)
    val decompressed = decompress(decrypted)

    println(decompressed)
}
