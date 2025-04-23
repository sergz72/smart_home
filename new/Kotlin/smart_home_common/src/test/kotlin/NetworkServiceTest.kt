import com.sz.smart_home.common.NetworkService
import com.sz.smart_home.common.NetworkServiceConfig
import org.junit.jupiter.api.Test
import org.junit.jupiter.api.Assertions.*

class SmartHomeServiceTest {
    private val config = NetworkServiceConfig(
        prefix = byteArrayOf(),
        key = byteArrayOf(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
                            25, 26, 27, 28, 29, 30, 31),
        hostName = "localhost",
        port = 60000)

    @Test
    fun ivTest() {
        val service = NetworkService(config)
        val iv = byteArrayOf(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12)
        val transformed = service.transformIV(iv)
        assertArrayEquals(transformed, byteArrayOf(1, 2, 3, 4, 87, 191u.toByte(), 4, 40, 131u.toByte(), 151u.toByte(), 75, 156u.toByte()))
        val transformed2 = service.transformIV(transformed)
        assertArrayEquals(iv, transformed2)
    }

    @Test
    fun buildIvTest() {
        val service = NetworkService(config)
        val iv = service.buildIV()
        val transformed = service.transformIV(iv)
        assertEquals(transformed[0], iv[0])
        assertEquals(transformed[1], iv[1])
        assertEquals(transformed[2], iv[2])
        assertEquals(transformed[3], iv[3])
        assertEquals(iv[8], 0)
        assertEquals(iv[9], 0)
        assertEquals(iv[10], 0)
        assertEquals(iv[11], 0)
    }

    @Test
    fun encryptTest() {
        val service = NetworkService(config)
        val data = byteArrayOf(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12)
        val encrypted = service.encrypt(data)
        val decrypted = service.decrypt(encrypted)
        assertArrayEquals(data, decrypted)
    }
}