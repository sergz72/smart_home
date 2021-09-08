package com.sz.heatercontrol

import java.io.ByteArrayInputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.security.SecureRandom
import java.util.zip.GZIPInputStream
import javax.crypto.Cipher
import javax.crypto.spec.GCMParameterSpec
import javax.crypto.spec.SecretKeySpec

object Aes {
    private lateinit var mKey: SecretKeySpec

    fun setKey(key: ByteArray) {
        mKey = SecretKeySpec(key, "AES")
    }

    fun encode(command: String): ByteArray {
        val secureRandom = SecureRandom()
        val randomPart = ByteArray(6)
        secureRandom.nextBytes(randomPart)
        val millis = ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN).putLong(System.currentTimeMillis()).array()
        val iv = randomPart.plus(byteArrayOf(millis[0], millis[1], millis[2], millis[3], millis[4], millis[5]))
        val cipher = Cipher.getInstance("AES/GCM/NoPadding")
        val parameterSpec = GCMParameterSpec(128, iv)
        cipher.init(Cipher.ENCRYPT_MODE, mKey, parameterSpec)
        return iv.plus(cipher.doFinal(command.toByteArray()))
    }
}