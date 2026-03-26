package smart_home.smarthome.service

import android.app.Activity
import org.apache.commons.compress.compressors.bzip2.BZip2CompressorInputStream
import smart_home.smarthome.Chacha
import smart_home.smarthome.Graph
import smart_home.smarthome.entities.LastSensorDataResponse
import smart_home.smarthome.entities.Locations
import smart_home.smarthome.entities.SensorDataResponse
import java.io.ByteArrayInputStream
import java.io.IOException
import java.io.InputStream
import java.net.*

class SmartHomeServiceV3: SmartHomeService {
    private var mLocations: Locations? = null

    constructor(serverAddress: String, port: Int, keyStream: InputStream) {
        Chacha.setKey(keyStream.readBytes())
        setupServer(serverAddress, port)
    }

    override fun setupServer(serverAddress: String, port: Int) {
        mLocations = null
        super.setupServer(serverAddress, port)
        Thread {
            run {
                try {
                    getLocationsFromServer()
                } catch (_: Exception) {

                }
            }
        }.start()
    }

    override fun decompress(data: ByteArray): ByteArray {
        val inStream = BZip2CompressorInputStream(ByteArrayInputStream(data))
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
        val decrypted = Chacha.decodeNoTransformIv(response, response.size)
        val decompressed = decompress(decrypted)
        return decompressed
    }

    override fun getLocations(): Locations {
        if (mLocations == null) {
            throw IllegalStateException("Locations not loaded")
        }
        return mLocations!!
    }

    private fun createLastSensorDataResponse(response: ByteArray): LastSensorDataResponse {
        return when (response[0]) {
            //no error
            0.toByte() -> LastSensorDataResponse.parseResponse(response.drop(1))
            // error
            else -> throw ResponseErrorV3(response)
        }
    }

    private fun createSensorDataResponse(response: ByteArray): SensorDataResponse {
        return when (response[0]) {
            //no error
            0.toByte() -> SensorDataResponse.parseResponse(response.drop(1))
            // error
            else -> throw ResponseErrorV3(response)
        }
    }

    override fun getLastSensorData(onResponse: (response: LastSensorDataResponse) -> Unit,
                                   onFailure: (t: Throwable) -> Unit, context: Activity) {
        doInBackground(byteArrayOf(1), object: Callback<LastSensorDataResponse> {
            override fun deserialize(
                request: ByteArray,
                response: ByteArray
            ): LastSensorDataResponse {
                return createLastSensorDataResponse(response)
            }

            override fun onResponse(response: LastSensorDataResponse) {
                onResponse(response)
            }

            override fun onFailure(t: Throwable) {
                onFailure(t)
            }
        }, context)
    }

    override fun getSensorData(query: SensorDataQuery, onResponse: (response: SensorDataResponse) -> Unit,
                               onFailure: (t: Throwable) -> Unit, context: Activity) {
        doInBackground(byteArrayOf(2).plus(query.toBinary()), object: Callback<SensorDataResponse> {
            override fun deserialize(
                request: ByteArray,
                response: ByteArray
            ): SensorDataResponse {
                return createSensorDataResponse(response)
            }

            override fun onResponse(response: SensorDataResponse) {
                onResponse(response)
            }

            override fun onFailure(t: Throwable) {
                onFailure(t)
            }
        }, context)
    }

    private fun getLocationsFromServer() {
        val request = byteArrayOf(0)
        val decompressed = send(request)
        mLocations = when (decompressed[0]) {
            //no error
            0.toByte() -> Locations.parseResponse(decompressed.drop(1))
            // error
            else -> throw ResponseErrorV3(decompressed)
        }
        Graph.timeZone = mLocations!!.timeZone
    }

    override fun isReady(): Boolean {
        return mLocations != null
    }

    override fun encrypt(request: ByteArray): ByteArray {
        return Chacha.encode(request)
    }

    override fun decrypt(data: ByteArray, length: Int): ByteArray {
        return Chacha.decodeNoTransformIv(data, length)
    }
}

class ResponseErrorV3(decompressed: ByteArray) : IOException(buildMessage(decompressed)) {
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
