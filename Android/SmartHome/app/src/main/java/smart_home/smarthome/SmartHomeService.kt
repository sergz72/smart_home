package smart_home.smarthome

import android.os.AsyncTask
import java.io.InputStream
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.net.UnknownHostException

class SmartHomeService<T>: AsyncTask<SmartHomeService.Callback<T>, Void, Unit>() {
    companion object {
        private lateinit var mAddress: InetAddress
        private var mPort: Int = 0

        fun setupKey(keyStream: InputStream) {
            Aes.setKey(keyStream.readBytes())
        }

        fun setupServer(serverAddress: String, port: Int) {
            mAddress = getInetAddressByName(serverAddress)
            mPort = port
        }

        private fun getInetAddressByName(name: String): InetAddress {
            val task = object : AsyncTask<String, Void, InetAddress?>() {

                override fun doInBackground(vararg params: String): InetAddress? {
                    try {
                        return InetAddress.getByName(params[0])
                    } catch (e: UnknownHostException) {
                        return null
                    }
                }
            }
            return task.execute(name).get() ?: throw UnknownHostException()
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

    override fun doInBackground(vararg params: Callback<T>) {
        if (params.size == 1 && mRequest != null) {
            val callback = params[0]
            val socket = DatagramSocket()
            val bytes = Aes.encode(mRequest!!)
            val packet = DatagramPacket(bytes, bytes.size, mAddress, mPort)
            val receiveData = ByteArray(65507)
            try {
                socket.send(packet)
                val inPacket = DatagramPacket(receiveData, receiveData.size)
                socket.soTimeout = 10000 // 10 seconds
                socket.receive(inPacket)
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
            } catch (e: Exception) {
                callback.onFailure(e, null)
            } finally {
                socket.close()
            }
        }
    }
}