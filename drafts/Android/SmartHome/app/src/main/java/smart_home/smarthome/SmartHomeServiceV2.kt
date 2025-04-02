package smart_home.smarthome

import android.app.Activity
import android.content.Context
import android.hardware.SensorPrivacyManager.Sensors
import android.net.ConnectivityManager
import android.net.NetworkCapabilities
import android.os.NetworkOnMainThreadException
import org.apache.commons.compress.compressors.bzip2.BZip2CompressorInputStream
import smart_home.smarthome.entities.Sensor
import smart_home.smarthome.entities.SensorsResponse
import java.io.ByteArrayInputStream
import java.io.IOException
import java.io.InputStream
import java.net.*
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.concurrent.Executor
import java.util.concurrent.Executors

enum class TimeUnit {
    Day,
    Month,
    Year
}

data class DateTime (
    val date: Int,
    val time: Int
)

data class DateOffset (
    val offset: Int,
    val unit: TimeUnit
)

data class SmartHomeQuery(
    val maxPoints: Short,
    val dataType: String,
    val startDate: DateTime?,
    val startDateOffset: DateOffset?,
    val period: DateOffset?
) {
    internal fun toBinary(): ByteArray {
        if (dataType.length < 3)
            throw IllegalArgumentException("dataType must be more than 2 characters")
        val buffer = ByteBuffer.allocate(17).order(ByteOrder.LITTLE_ENDIAN)
        buffer.put(1)
        buffer.putShort(maxPoints)
        val bytes = dataType.toByteArray()
        buffer.put(bytes[0])
        buffer.put(bytes[1])
        buffer.put(bytes[2])
        buffer.put(if (bytes.size > 3) { bytes[3] } else { 0x20 })
        if (startDate != null) {
            buffer.putInt(startDate.date)
            buffer.putInt(startDate.time)
        } else {
            buffer.putInt(-startDateOffset!!.offset)
            buffer.putInt(startDateOffset.unit.ordinal)
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

class SmartHomeServiceV2<T> {
    companion object {
        private val executor: Executor = Executors.newSingleThreadExecutor()
        private var mAddress: InetAddress? = null
        private var mPort: Int = 0
        internal var mSensors: Map<Int, Sensor>? = null

        fun setupKey(keyStream: InputStream) {
            Chacha.setKey(keyStream.readBytes())
        }

        fun setupServer(serverAddress: String, port: Int) {
            mAddress = try {
                InetAddress.getByName(serverAddress)
            } catch (e: NetworkOnMainThreadException) {
                null
            }
            mPort = port
            Thread {
                run {
                    try {
                        getSensors()
                    } catch (e: Exception) {

                    }
                }
            }.start()
        }

        private fun isWifiConnected(context: Activity): Boolean {
            try {
                val cm = context.getSystemService(Context.CONNECTIVITY_SERVICE)
                if (cm is ConnectivityManager) {
                    val n = cm.activeNetwork ?: return false
                    val cp = cm.getNetworkCapabilities(n)
                    return cp != null && cp.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)
                }
            } catch (e: Exception) {
                return true
            }

            return true
        }

        private fun decompress(decrypted: ByteArray): ByteArray {
            val inStream = BZip2CompressorInputStream(ByteArrayInputStream(decrypted))
            inStream.use {
                return inStream.readBytes()
            }
        }

        private fun sendUDP(data: ByteArray): ByteArray {
            val socket = DatagramSocket()
            socket.soTimeout = 7000
            val sendPacket = DatagramPacket(data, data.size, mAddress, mPort)
            socket.send(sendPacket)
            val receiveData = ByteArray(100000)
            val receivePacket = DatagramPacket(receiveData, receiveData.size)
            socket.receive(receivePacket)
            return receiveData.copyOfRange(0, receivePacket.length)
        }

        private fun send(request: ByteArray): ByteArray {
            val sendData = Chacha.encode(request)
            val response = sendUDP(sendData)
            val decrypted = Chacha.decode(response, response.size)
            val decompressed = decompress(decrypted)
            return decompressed
        }

        private fun getSensors() {
            val request = byteArrayOf(0, 0)
            val decompressed = send(request)
            mSensors = when (decompressed[0]) {
                //no error
                0.toByte() -> SensorsResponse.parseResponse(decompressed.drop(1))
                // error
                else -> throw ResponseError(decompressed)
            }
        }
    }

    interface Callback<T> {
        fun deserialize(request: ByteArray, response: ByteArray): T
        fun onResponse(response: T)
        fun onFailure(t: Throwable)
    }

    private var mRequest: ByteArray? = null

    fun setRequest(request: ByteArray): SmartHomeServiceV2<T> {
        mRequest = request
        return this
    }

    fun doInBackground(callback: Callback<T>, context: Activity) {
        if (mRequest != null && mAddress != null) {
            executor.execute {
                var timeout = 7
                while (timeout != 0 && mSensors == null) {
                    Thread.sleep(1000)
                    timeout--
                }
                if (mSensors == null)
                    return@execute

                val socket = DatagramSocket()
                val bytes = Chacha.encode(mRequest!!)
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
                        val body = Chacha.decode(inPacket.data, inPacket.length)
                        callback.onResponse(callback.deserialize(mRequest!!, body))
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
