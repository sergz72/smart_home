import com.sz.smart_home.query.buildIV
import com.sz.smart_home.query.transformIV
import org.junit.jupiter.api.Test
import org.junit.jupiter.api.Assertions.*
import javax.crypto.spec.SecretKeySpec

class MainKtTest {
    private val keyBytes = byteArrayOf(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
                                        25, 26, 27, 28, 29, 30, 31)

    @Test
    fun ivTest() {
        val key = SecretKeySpec(keyBytes, 0, keyBytes.size, "ChaCha20")
        val iv = byteArrayOf(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12)
        val transformed = transformIV(key, iv)
        assertArrayEquals(transformed, byteArrayOf(1, 2, 3, 4, 87, 191u.toByte(), 4, 40, 131u.toByte(), 151u.toByte(), 75, 156u.toByte()))
        val transformed2 = transformIV(key, transformed)
        assertArrayEquals(iv, transformed2)
    }

    @Test
    fun buildIvTest() {
        val key = SecretKeySpec(keyBytes, 0, keyBytes.size, "ChaCha20")
        val iv = buildIV(key)
        val transformed = transformIV(key, iv)
        assertEquals(transformed[0], iv[0])
        assertEquals(transformed[1], iv[1])
        assertEquals(transformed[2], iv[2])
        assertEquals(transformed[3], iv[3])
        assertEquals(transformed[8], 0)
        assertEquals(transformed[9], 0)
        assertEquals(transformed[10], 0)
        assertEquals(transformed[11], 0)
    }
}