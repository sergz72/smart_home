package smart_home.smarthome

import android.content.res.Configuration
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import com.sz.charts.LineChart

import smart_home.smarthome.entities.SensorData
import smart_home.smarthome.entities.SensorDataResponse

class EnvSensorsFragment(params: IGraphParameters, service: ServiceHolder) :
    SensorsFragment(service, R.layout.fragment_env_sensors, params, "env"), View.OnClickListener {
    private var mNextTempIntGraph: Button? = null
    private var mNextTempExtGraph: Button? = null
    private var mNextHumiGraph: Button? = null
    private var mNextCO2Graph: Button? = null
    private var mNextLuxGraph: Button? = null
    private var mIntTempSensors: List<SensorData>? = null
    private var mExtTempSensors: List<SensorData>? = null
    private var mHumiSensors: List<SensorData>? = null
    private var mPresSensor: SensorData? = null
    private var mCO2Sensors: List<SensorData>? = null
    private var mLuxSensors: List<SensorData>? = null
    private var mIntTempSensorIdx: Int = 0
    private var mExtTempSensorIdx: Int = 0
    private var mHumiSensorIdx: Int = 0
    private var mCO2SensorIdx: Int = 0
    private var mLuxSensorIdx: Int = 0

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?,
                              savedInstanceState: Bundle?): View {
        val rootView = super.onCreateView(inflater, container, savedInstanceState)
        mNextTempIntGraph = rootView!!.findViewById(R.id.next_temp_int_graph)
        mNextTempIntGraph!!.setOnClickListener(this)
        mNextTempExtGraph = rootView.findViewById(R.id.next_temp_ext_graph)
        mNextTempExtGraph!!.setOnClickListener(this)
        mNextHumiGraph = rootView.findViewById(R.id.next_humi_graph)
        mNextHumiGraph!!.setOnClickListener(this)
        mNextCO2Graph = rootView.findViewById(R.id.next_co2_graph)
        mNextCO2Graph!!.setOnClickListener(this)
        mNextLuxGraph = rootView.findViewById(R.id.next_lux_graph)
        mNextLuxGraph!!.setOnClickListener(this)
        return rootView
    }

    private fun buildSensorLists(results: Map<String, List<SensorData>>) {
        val tempSensors = results["temp"]
        mIntTempSensors = tempSensors?.filter { !service.service!!.getLocations().isExtLocation(it.locationId) }
        mExtTempSensors = tempSensors?.filter { service.service!!.getLocations().isExtLocation(it.locationId) }
        mHumiSensors = results["humi"]
        mPresSensor = results["pres"]?.firstOrNull()
        mCO2Sensors = results["co2 "]
        mLuxSensors = results["lux "]
    }

    private fun refresh(results: SensorDataResponse) {
        buildSensorLists(results.response)
        mIntTempSensorIdx = 0
        mExtTempSensorIdx = 0
        mHumiSensorIdx = 0
        mCO2SensorIdx = 0
        mLuxSensorIdx = 0
        showResults(requireView())
    }

    private fun showResults(v: View) {
        showIntTempSensors(v)
        showExtTempSensors(v)
        showHumiSensors(v)
        showCO2Sensors(v)
        showLuxSensors(v)
        val plot = v.findViewById<LineChart>(R.id.plot_pres)
        Graph.buildGraph(plot, mPresSensor, getLocationName(mPresSensor), onlyAvg = false, useFloat0 = false)
    }

    private fun showIntTempSensors(v: View) {
        if (mIntTempSensors != null && mIntTempSensorIdx < mIntTempSensors!!.size) {
            val plot = v.findViewById<LineChart>(R.id.plot_temp_int)
            val sensor = mIntTempSensors!![mIntTempSensorIdx]
            Graph.buildGraph(plot, sensor, getLocationName(sensor), onlyAvg = false, useFloat0 = false)
        }
    }

    private fun showExtTempSensors(v: View) {
        val plot = v.findViewById<LineChart>(R.id.plot_temp_ext)
        if (mExtTempSensors != null && mExtTempSensorIdx < mExtTempSensors!!.size) {
            val sensor = mExtTempSensors!![mExtTempSensorIdx]
            Graph.buildGraph(plot, sensor, getLocationName(sensor), onlyAvg = false, useFloat0 = false)
        } else {
            plot.clearSeries()
        }
    }

    private fun showHumiSensors(v: View) {
        if (mHumiSensors != null && mHumiSensorIdx < mHumiSensors!!.size) {
            val plot = v.findViewById<LineChart>(R.id.plot_humi)
            val sensor = mHumiSensors!![mHumiSensorIdx]
            Graph.buildGraph(plot, sensor, getLocationName(sensor), onlyAvg = false, useFloat0 = false)
        }
    }

    private fun showCO2Sensors(v: View) {
        val plot = v.findViewById<LineChart>(R.id.plot_co2)
        if (mCO2Sensors != null && mCO2SensorIdx < mCO2Sensors!!.size) {
            val sensor = mCO2Sensors!![mCO2SensorIdx]
            Graph.buildGraph(plot, sensor, getLocationName(sensor), onlyAvg = false, useFloat0 = false)
        } else {
            plot.clearSeries()
        }
    }

    private fun showLuxSensors(v: View) {
        val plot = v.findViewById<LineChart>(R.id.plot_lux)
        if (mLuxSensors != null && mLuxSensorIdx < mLuxSensors!!.size) {
            val sensor = mLuxSensors!![mLuxSensorIdx]
            Graph.buildGraph(plot, sensor, getLocationName(sensor), onlyAvg = false, useFloat0 = false)
        } else {
            plot.clearSeries()
        }
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        if (mIntTempSensors != null) {
            showResults(view)
        }
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)
        parentFragmentManager
                .beginTransaction()
                .detach(this)
                .attach(this)
                .commit()
    }

    override fun onClick(v: View?) {
        when (v!!) {
            mNextTempIntGraph -> showNextTempIntGraph()
            mNextTempExtGraph -> showNextTempExtGraph()
            mNextHumiGraph -> showNextHumiGraph()
            mNextCO2Graph -> showNextCO2Graph()
            mNextLuxGraph -> showNextLuxGraph()
        }
    }

    private fun showNextTempIntGraph() {
        if (mIntTempSensors != null && mIntTempSensors!!.isNotEmpty()) {
            mIntTempSensorIdx++
            if (mIntTempSensorIdx >= mIntTempSensors!!.size) {
                mIntTempSensorIdx = 0
            }
            showIntTempSensors(requireView())
        }
    }

    private fun showNextTempExtGraph() {
        if (mExtTempSensors != null && mExtTempSensors!!.isNotEmpty()) {
            mExtTempSensorIdx++
            if (mExtTempSensorIdx >= mExtTempSensors!!.size) {
                mExtTempSensorIdx = 0
            }
            showExtTempSensors(requireView())
        }
    }

    private fun showNextHumiGraph() {
        if (mHumiSensors != null && mHumiSensors!!.isNotEmpty()) {
            mHumiSensorIdx++
            if (mHumiSensorIdx >= mHumiSensors!!.size) {
                mHumiSensorIdx = 0
            }
            showHumiSensors(requireView())
        }
    }

    private fun showNextCO2Graph() {
        if (mCO2Sensors != null && mCO2Sensors!!.isNotEmpty()) {
            mCO2SensorIdx++
            if (mCO2SensorIdx >= mCO2Sensors!!.size) {
                mCO2SensorIdx = 0
            }
            showCO2Sensors(requireView())
        }
    }

    private fun showNextLuxGraph() {
        if (mLuxSensors != null && mLuxSensors!!.isNotEmpty()) {
            mLuxSensorIdx++
            if (mLuxSensorIdx >= mLuxSensors!!.size) {
                mLuxSensorIdx = 0
            }
            showLuxSensors(requireView())
        }
    }

    override fun refresh() {
        service.service!!.getSensorData(
            buildQuery(),
            {response -> mHandler.post { refresh(response) }},
            { t -> onFailure(t) },
            requireActivity())
    }
}
