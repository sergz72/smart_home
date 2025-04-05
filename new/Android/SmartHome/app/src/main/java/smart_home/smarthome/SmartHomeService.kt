package smart_home.smarthome

import android.app.Activity
import android.content.Context
import android.net.ConnectivityManager
import android.net.NetworkCapabilities
import android.os.NetworkOnMainThreadException
import java.io.InputStream
import java.net.*
import java.util.concurrent.Executor
import java.util.concurrent.Executors

class SmartHomeService<T> {
    companion object {
        private val executor: Executor = Executors.newSingleThreadExecutor()
        private var mAddress: InetAddress? = null
        private var mPort: Int = 0

        fun setupKey(keyStream: InputStream) {
            Aes.setKey(keyStream.readBytes())
        }

        fun setupServer(serverAddress: String, port: Int) {
            mAddress = try {
                InetAddress.getByName(serverAddress)
            } catch (e: NetworkOnMainThreadException) {
                null
            }
            mPort = port
        }
    }

    interface Callback<T> {
        fun deserialize(response: String): T
        fun isString(): Boolean
        fun onResponse(response: T)
        fun onFailure(t: Throwable?, response: String?)
    }

    private var mRequest: String? = null

    fun setRequest(request: String): SmartHomeService<T> {
        mRequest = request
        return this
    }

    fun doInBackground(callback: Callback<T>, context: Activity) {
        if (mRequest != null && mAddress != null) {
            executor.execute {
                val socket = DatagramSocket()
                val bytes = Aes.encode(mRequest!!)
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
                        val body = Aes.decode(inPacket.data, inPacket.length)
                        if (callback.isString()) {
                            @Suppress("UNCHECKED_CAST")
                            callback.onResponse(body as T)
                        } else {
                            if (body.isEmpty() || (body[0] != '{' && body[0] != '[')) {
                                callback.onFailure(null, body)
                            } else {
                                callback.onResponse(callback.deserialize(body))
                            }
                        }
                    } else {
                        callback.onFailure(exc, null)
                    }
                } catch (e: Exception) {
                    callback.onFailure(e, null)
                } finally {
                    socket.close()
                }
            }
        }
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
}
