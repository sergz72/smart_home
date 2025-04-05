package smart_home.smarthome

import android.content.res.Configuration
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import com.sz.charts.LineChart

import smart_home.smarthome.entities.SensorData
import smart_home.smarthome.entities.SensorDataResponseV1

class EnvSensorsFragment(params: IGraphParameters) : SensorsFragment(R.layout.fragment_env_sensors, params, "env"), View.OnClickListener {
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
    private var mAggregated: Boolean = false

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

    override fun showResults(results: SensorDataResponseV1) {
        mAggregated = results.aggregated
        buildSensorLists(results.response)
        mIntTempSensorIdx = 0
        mExtTempSensorIdx = 0
        mHumiSensorIdx = 0
        mCO2SensorIdx = 0;
        mLuxSensorIdx = 0;
        showResults(requireView())
    }

    private fun buildSensorLists(results: List<SensorData>) {
        mIntTempSensors = findSensors(results, { it != "ext" }, { it.contains("temp") })
        mExtTempSensors = findSensors(results, { it == "ext" }, { it.contains("temp") })
        mHumiSensors = findSensors(results, { true }, { it.contains("humi") })
        mPresSensor = findSensors(results, { true }, { it.contains("pres") }).firstOrNull()
        mCO2Sensors = findSensors(results, { true }, { it.contains("co2") })
        mLuxSensors = findSensors(results, { true }, { it.contains("lux") })
    }

    private fun showResults(v: View) {
        showIntTempSensors(v)
        showExtTempSensors(v)
        showHumiSensors(v)
        showCO2Sensors(v)
        showLuxSensors(v)
        val plot = v.findViewById<LineChart>(R.id.plot_pres)
        Graph.buildGraph(plot, mPresSensor, "pres", false, mAggregated, false)
    }

    private fun showIntTempSensors(v: View) {
        if (mIntTempSensors != null && mIntTempSensorIdx < mIntTempSensors!!.size) {
            val plot = v.findViewById<LineChart>(R.id.plot_temp_int)
            Graph.buildGraph(plot, mIntTempSensors!![mIntTempSensorIdx], "temp", false, mAggregated, false)
        }
    }

    private fun showExtTempSensors(v: View) {
        val plot = v.findViewById<LineChart>(R.id.plot_temp_ext)
        if (mExtTempSensors != null && mExtTempSensorIdx < mExtTempSensors!!.size) {
            Graph.buildGraph(plot, mExtTempSensors!![mExtTempSensorIdx], "temp", false, mAggregated, false)
        } else {
            plot.clearSeries()
        }
    }

    private fun showHumiSensors(v: View) {
        if (mHumiSensors != null && mHumiSensorIdx < mHumiSensors!!.size) {
            val plot = v.findViewById<LineChart>(R.id.plot_humi)
            Graph.buildGraph(plot, mHumiSensors!![mHumiSensorIdx], "humi", false, mAggregated, false)
        }
    }

    private fun showCO2Sensors(v: View) {
        if (mCO2Sensors != null && mCO2SensorIdx < mCO2Sensors!!.size) {
            val plot = v.findViewById<LineChart>(R.id.plot_co2)
            Graph.buildGraph(plot, mCO2Sensors!![mCO2SensorIdx], "co2", false, mAggregated, false)
        }
    }

    private fun showLuxSensors(v: View) {
        if (mLuxSensors != null && mLuxSensorIdx < mLuxSensors!!.size) {
            val plot = v.findViewById<LineChart>(R.id.plot_lux)
            Graph.buildGraph(plot, mLuxSensors!![mLuxSensorIdx], "lux", false, mAggregated, false)
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
}
