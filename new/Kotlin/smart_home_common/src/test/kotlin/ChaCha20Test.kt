import com.sz.smart_home.common.ChaCha20
import org.junit.jupiter.api.Assertions.*
import org.junit.jupiter.api.Test
import javax.crypto.Cipher
import javax.crypto.spec.ChaCha20ParameterSpec
import javax.crypto.spec.SecretKeySpec
import kotlin.random.Random
import kotlin.time.TimeSource

class ChaCha20Test {
    private val key = byteArrayOf(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
        25, 26, 27, 28, 29, 30, 31)
    private val iv = byteArrayOf(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12)

    @Test
    fun chaCha20Test() {
        val timeSource = TimeSource.Monotonic
        val data = Random.nextBytes(1000000)

        val mark1 = timeSource.markNow()
        val cipher = ChaCha20(key, iv, 0u)
        val encrypted = cipher.encrypt(data)
        val mark2 = timeSource.markNow()
        println("ChaCha20(custom) encrypt 1000000 elapsed ${mark2 - mark1}")

        val k = SecretKeySpec(key, 0, key.size, "ChaCha20")
        val mark3 = timeSource.markNow()
        val icipher = Cipher.getInstance("ChaCha20")
        val param = ChaCha20ParameterSpec(iv, 0)
        icipher.init(Cipher.ENCRYPT_MODE, k, param)
        val iencrypted = icipher.doFinal(data)
        val mark4 = timeSource.markNow()
        println("ChaCha20(standard) encrypt 1000000 elapsed ${mark4 - mark3}")

        assertArrayEquals(iencrypted, encrypted)

        val cipher2 = ChaCha20(key, iv, 0u)
        val data2 = cipher2.encrypt(encrypted)
        assertArrayEquals(data, data2)
    }
}