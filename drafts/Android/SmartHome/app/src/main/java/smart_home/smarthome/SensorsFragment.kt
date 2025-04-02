package smart_home.smarthome

import android.app.Activity
import android.content.Context
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment

import com.google.gson.Gson
import com.google.gson.GsonBuilder
import com.google.gson.reflect.TypeToken
import smart_home.smarthome.entities.LastSensorData

import smart_home.smarthome.entities.SensorData
import smart_home.smarthome.entities.SensorDataResponse

abstract class SensorsFragment(private val mId: Int, protected val params: IGraphParameters?,
                               private val  dataType: String, private val maxPoints: Int = 200):
    Fragment(), ISmartHomeData {

    companion object { internal var useV2 = false }

    protected val mGson: Gson
    private val mService = buildService()
    private val mServiceV2 = buildServiceV2()

    private val mHandler = Handler(Looper.getMainLooper())

    init {
        val gsonBuilder = GsonBuilder()
        mGson = gsonBuilder.create()
    }

    fun findSensors(results: List<SensorData>, locationTypeChecker: (String) -> Boolean, dataTypeChecker: (Set<String>) -> Boolean): List<SensorData> {
        return results.filter { sd -> locationTypeChecker(sd.locationType) && dataTypeChecker(sd.series.last().data.keys) }
    }

    private fun buildService(): SmartHomeService<List<SensorData>> {
        return SmartHomeService<List<SensorData>>()
                .setRequest(buildRequest())
    }

    private fun buildServiceV2(): SmartHomeServiceV2<List<SensorData>> {
        return SmartHomeServiceV2<List<SensorData>>()
    }

    private fun buildRequestV2(): ByteArray {
        /*var startDate: DateTime? = null
        var offset: DateOffset? = null
        var period: DateOffset? = null
        if (params != null) {
            //todo
        }
        val query = SmartHomeQuery(maxPoints.toShort(), dataType, startDate, offset, period)
        return query.toBinary()*/
        return byteArrayOf(1, 1)
    }

    private fun buildRequest(): String {
        var requestString = "GET /sensor_data?data_type=$dataType"
        if (params != null) {
            if (params.getData().mUsePeriod) {
                requestString += "&period=" + params.getData().mPeriod
            } else {
                requestString += "&start=" + params.getData().getFromDate() + "&end=" + params.getData().getToDate()
            }
            requestString += "&maxPoints=$maxPoints"
        }
        return requestString
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?,
                              savedInstanceState: Bundle?): View? {
        // Inflate the layout for this fragment
        return inflater.inflate(mId, container, false)
    }

    override fun onAttach(context: Context) {
        super.onAttach(context)
        refresh()
    }

    private fun refreshV1() {
        mService.setRequest(buildRequest())
        mService.doInBackground(object : SmartHomeService.Callback<List<SensorData>> {
            override fun deserialize(response: String): List<SensorData> {
                return mGson.fromJson(response, object : TypeToken<List<SensorData>>() {}.type)
            }

            override fun isString(): Boolean {
                return false
            }

            override fun onResponse(response: List<SensorData>) {
                mHandler.post { showResults(response) }
            }

            override fun onFailure(t: Throwable?, response: String?) {
                mHandler.post {
                    if (t != null) {
                        MainActivity.alert(activity!!, t.message)
                    } else {
                        MainActivity.alert(activity!!, response)
                    }
                }
            }
        }, Activity())
    }

    private fun refreshV2() {
        mServiceV2.setRequest(buildRequestV2())
        mServiceV2.doInBackground(object : SmartHomeServiceV2.Callback<List<SensorData>> {
            override fun deserialize(request: ByteArray, response: ByteArray): List<SensorData> {
                if (request[0] == 1.toByte()) // last sensor data
                    return SensorData.from(LastSensorData.parseResponse(response))
                return SensorData.from(SensorDataResponse.parseResponse(response))
            }

            override fun onResponse(response: List<SensorData>) {
                mHandler.post { showResults(response) }
            }

            override fun onFailure(t: Throwable) {
                mHandler.post { MainActivity.alert(activity!!, t.message) }
            }
        }, Activity())
    }

    override fun refresh() {
        if (useV2)
            refreshV2()
        else
            refreshV1()
    }

    protected abstract fun showResults(results: List<SensorData>)
}
