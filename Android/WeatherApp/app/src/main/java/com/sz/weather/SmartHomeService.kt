package com.sz.weather

import android.app.Activity
import android.content.Context
import java.io.InputStream
import java.net.*
import java.util.concurrent.Executor
import java.util.concurrent.Executors

class SmartHomeService<T> {
    companion object {
        private val executor: Executor = Executors.newSingleThreadExecutor()
        private lateinit var mAddress: InetAddress
        private var mPort: Int = 0
        private lateinit var context: Context

        fun setupKeyAndContext(keyStream: InputStream, ctx: Context) {
            Aes.setKey(keyStream.readBytes())
            context = ctx
        }

        fun setupServer(serverAddress: String, port: Int) {
            getInetAddressByName(serverAddress)
            mPort = port
        }

        private fun getInetAddressByName(name: String) {
            executor.execute{
                mAddress = InetAddress.getByName(name)
            }
        }
    }

    interface Callback<T> {
        fun deserialize(response: String): T
        fun isString(): Boolean
        fun onResponse(response: T)
        fun onFailure(activity: Activity, t: Throwable?, response: String?)
    }

    private var mRequest: String? = null

    fun setRequest(request: String): SmartHomeService<T> {
        mRequest = request
        return this
    }

    fun doInBackground(activity: Activity, callback: Callback<T>) {
        if (mRequest != null) {
            executor.execute {
                val socket = DatagramSocket()
                val bytes = Aes.encode(mRequest!!)
                val packet = DatagramPacket(bytes, bytes.size, mAddress, mPort)
                val receiveData = ByteArray(65507)
                try {
                    val inPacket = DatagramPacket(receiveData, receiveData.size)
                    var exc: SocketTimeoutException? = null
                    socket.soTimeout = 2000 // 2 seconds
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
                                callback.onFailure(activity,null, body)
                            } else {
                                callback.onResponse(callback.deserialize(body))
                            }
                        }
                    } else {
                        callback.onFailure(activity, exc, null)
                    }
                } catch (e: Exception) {
                    callback.onFailure(activity, e, null)
                } finally {
                    socket.close()
                }
            }
        }
    }
}
