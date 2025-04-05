package smart_home.smarthome

import java.io.IOException
import java.nio.ByteBuffer
import kotlin.random.Random
import java.nio.ByteOrder

object Chacha {
    private lateinit var mKey: ByteArray

    fun setKey(key: ByteArray) {
        mKey = key
    }

    fun encode(command: ByteArray): ByteArray {
        val iv = buildIV()
        val transformed = transformIV(iv)
        val cipher = ChaCha20(mKey, iv, 0u)
        val encrypted = cipher.encrypt(command)
        val data = ByteBuffer
            .allocate(encrypted.size + iv.size)
            .put(transformed)
            .put(encrypted)
            .array()
        return data
    }

    private fun transformIV(iv: ByteArray): ByteArray {
        val iv3 = ByteBuffer.allocate(12)
            .put(iv.sliceArray(0..3))
            .put(iv.sliceArray(0..3))
            .put(iv.sliceArray(0..3))
            .array()
        val cipher = ChaCha20(mKey, iv3, 0u)
        val transformed = cipher.encrypt(iv.sliceArray(4..11))
        transformed.copyInto(iv3, 4, 0)
        return iv3
    }

    private fun buildIV(): ByteArray {
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

    fun decode(body: ByteArray, length: Int): ByteArray {
        if (length < 13) {
            throw IOException("Invalid response")
        }
        val iv = transformIV(body.sliceArray(0..12))
        val cipher = ChaCha20(mKey, iv, 0u)
        return cipher.encrypt(body.sliceArray(12..<length))
    }
}