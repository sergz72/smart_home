package smart_home.smarthome

import java.nio.ByteBuffer
import java.nio.ByteOrder
import kotlin.experimental.xor

class ChaCha20(private val key: ByteArray, private val iv: ByteArray, private var counter: UInt = 0u) {
    companion object {
        private const val ROUNDS = 20
    }

    private val state = Array(16) { 0u }
    private val workingState = Array(16) { 0u }
    private var wordIndex = 16

    private fun initState() {
        state[ 0] = 0x61707865u
        state[ 1] = 0x3320646eu
        state[ 2] = 0x79622d32u
        state[ 3] = 0x6b206574u

        var buffer = ByteBuffer.wrap(key).order(ByteOrder.LITTLE_ENDIAN)
        state[ 4] = buffer.getInt().toUInt()
        state[ 5] = buffer.getInt().toUInt()
        state[ 6] = buffer.getInt().toUInt()
        state[ 7] = buffer.getInt().toUInt()
        state[ 8] = buffer.getInt().toUInt()
        state[ 9] = buffer.getInt().toUInt()
        state[10] = buffer.getInt().toUInt()
        state[11] = buffer.getInt().toUInt()

        buffer = ByteBuffer.wrap(iv).order(ByteOrder.LITTLE_ENDIAN)
        state[12] = counter
        state[13] = buffer.getInt().toUInt()
        state[14] = buffer.getInt().toUInt()
        state[15] = buffer.getInt().toUInt()
    }

    private fun rotatedLeft(value: UInt, count: Int): UInt {
        //return Integer.rotateLeft(value.toInt(), count).toUInt()
        return (value shl count) or (value shr (32 - count))
    }

    private fun quarterRound(a: Int, b: Int, c: Int, d: Int) {
        state[a] += state[b]; state[d] = rotatedLeft(state[d] xor state[a], 16)
        state[c] += state[d]; state[b] = rotatedLeft(state[b] xor state[c], 12)
        state[a] += state[b]; state[d] = rotatedLeft(state[d] xor state[a],  8)
        state[c] += state[d]; state[b] = rotatedLeft(state[b] xor state[c],  7)
    }

    private fun doubleRound() {
        quarterRound(0, 4,  8, 12)
        quarterRound(1, 5,  9, 13)
        quarterRound(2, 6, 10, 14)
        quarterRound(3, 7, 11, 15)

        quarterRound(0, 5, 10, 15)
        quarterRound(1, 6, 11, 12)
        quarterRound(2, 7,  8, 13)
        quarterRound(3, 4,  9, 14)
    }

    private fun chachaU32(): UInt {
        if (wordIndex == 16) {
            initState()
            counter++

            for (i in 0..<16) {
                workingState[i] = state[i]
            }

            for (i in 0..<ROUNDS step 2) {
                doubleRound()
            }

            for (i in 0..<16) {
                workingState[i] += state[i]
            }

            wordIndex = 0
        }

        val result = workingState[wordIndex]

        wordIndex++

        return result
    }

    fun fill(data: ByteArray, offset: Int = 0, length: Int = data.size) {
        val tailCount = length % 4
        val to = offset + length

        for (i in offset..<to - tailCount step 4) {
            val word = chachaU32()
            data[i + 0] = word.toByte()
            data[i + 1] = (word shr 8).toByte()
            data[i + 2] = (word shr 16).toByte()
            data[i + 3] = (word shr 24).toByte()
        }

        if (tailCount > 0) {
            var word = chachaU32()
            for (i in tailCount downTo 1) {
                data[to - i] = word.toByte()
                word = word shr 8
            }
        }
    }

    fun encrypt(data: ByteArray): ByteArray {
        val result = ByteArray(data.size)
        fill(result)
        for (i in 0..<data.size) {
            result[i] = result[i].xor(data[i])
        }
        return result
    }
}