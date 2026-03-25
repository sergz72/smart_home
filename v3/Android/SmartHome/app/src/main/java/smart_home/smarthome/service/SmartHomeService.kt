package smart_home.smarthome.service

import android.app.Activity
import android.content.Context
import android.net.ConnectivityManager
import android.net.NetworkCapabilities
import android.os.NetworkOnMainThreadException
import smart_home.smarthome.MainActivity
import smart_home.smarthome.entities.LastSensorDataResponse
import smart_home.smarthome.entities.Locations
import smart_home.smarthome.entities.SensorDataResponse
import java.io.InputStream
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.net.SocketTimeoutException
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.concurrent.Executor
import java.util.concurrent.Executors

enum class TimeUnit {
    Day,
    Month,
    Year
}

data class DateOffset (
    val offset: Int,
    val unit: TimeUnit
) {
    fun toHours(): Int {
        return when (unit) {
            TimeUnit.Day -> offset * 24
            TimeUnit.Month -> 24
            TimeUnit.Year -> 24
        }
    }
}

data class SensorDataQuery(
    val maxPoints: Short,
    val dataType: String,
    val startDate: Int?,
    val startDateOffset: DateOffset?,
    val period: DateOffset?
) {
    internal fun toBinary(): ByteArray {
        if (dataType.length < 3)
            throw IllegalArgumentException("dataType must be more than 2 characters")
        val buffer = ByteBuffer.allocate(12).order(ByteOrder.LITTLE_ENDIAN)
        buffer.put(2)
        buffer.putShort(maxPoints)
        val bytes = dataType.toByteArray()
        buffer.put(bytes[0])
        buffer.put(bytes[1])
        buffer.put(bytes[2])
        if (startDate != null) {
            buffer.putInt(startDate)
        } else {
            val offset = (-startDateOffset!!.offset shl 8) or startDateOffset.unit.ordinal
            buffer.putInt(offset)
        }
        if (period != null) {
            buffer.put(period.offset.toByte())
            buffer.put(period.unit.ordinal.toByte())
        } else {
            buffer.putShort(0)
        }
        return buffer.array()
    }
}

abstract class SmartHomeService {
    companion object {
        fun build(serverAddress: String, port: Int, serverProtocol: String, keyStream: InputStream):
                SmartHomeService {
            when (serverProtocol) {
                "3" -> return SmartHomeServiceV3(serverAddress, port, keyStream)
                else -> throw IllegalArgumentException("unsupported service protocol version")
            }
        }

        fun isWifiConnected(context: Activity): Boolean {
            try {
                val cm = context.getSystemService(Context.CONNECTIVITY_SERVICE)
                if (cm is ConnectivityManager) {
                    val n = cm.activeNetwork ?: return false
                    val cp = cm.getNetworkCapabilities(n)
                    return cp != null && cp.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)
                }
            } catch (_: Exception) {
                return true
            }

            return true
        }
    }

    private val executor: Executor = Executors.newSingleThreadExecutor()
    protected var mAddress: InetAddress? = null
    protected var mPort: Int = 0

    protected open fun setupServer(serverAddress: String, port: Int) {
        mAddress = try {
            InetAddress.getByName(serverAddress)
        } catch (_: NetworkOnMainThreadException) {
            null
        }
        mPort = port
    }

    interface Callback<T> {
        fun deserialize(request: ByteArray, response: ByteArray): T
        fun onResponse(response: T)
        fun onFailure(t: Throwable)
    }

    abstract fun getLocations(): Locations
    abstract fun getLastSensorData(onResponse: (response: LastSensorDataResponse) -> Unit,
                                   onFailure: (t: Throwable) -> Unit, context: Activity)
    abstract fun getSensorData(query: SensorDataQuery, onResponse: (response: SensorDataResponse) -> Unit,
                               onFailure: (t: Throwable) -> Unit, context: Activity)

    protected abstract fun isReady(): Boolean
    protected abstract fun encrypt(request: ByteArray): ByteArray
    protected abstract fun decrypt(data: ByteArray, length: Int): ByteArray
    protected abstract fun decompress(data: ByteArray): ByteArray

    protected fun<T> doInBackground(request: ByteArray, callback: Callback<T>, context: Activity) {
        if (mAddress != null) {
            executor.execute {
                var timeout = 7
                while (timeout != 0 && !isReady()) {
                    Thread.sleep(1000)
                    timeout--
                }
                if (!isReady())
                    return@execute

                val socket = DatagramSocket()
                val bytes = encrypt(request)
                val packet = DatagramPacket(bytes, bytes.size, mAddress, mPort)
                val receiveData = ByteArray(65507)
                try {
                    val inPacket = DatagramPacket(receiveData, receiveData.size)
                    var exc: SocketTimeoutException? = null
                    socket.soTimeout = if (isWifiConnected(context)) 2000 else 7000 // 2 seconds
                    for (retry in 1..3) {
                        socket.send(packet)
                        try {
                            socket.receive(inPacket)
                            exc = null
                        } catch (e: SocketTimeoutException) {
                            exc = e
                            continue
                        }
                        break
                    }
                    if (exc == null) {
                        val decrypted = decrypt(inPacket.data, inPacket.length)
                        val decompressed = decompress(decrypted)
                        callback.onResponse(callback.deserialize(request, decompressed))
                    } else {
                        callback.onFailure(exc)
                    }
                } catch (e: Exception) {
                    callback.onFailure(e)
                } finally {
                    socket.close()
                }
            }
        }
    }
}
