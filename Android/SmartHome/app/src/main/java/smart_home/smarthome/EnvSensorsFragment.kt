package smart_home.smarthome

import android.content.res.Configuration
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button

import com.androidplot.xy.XYPlot
import smart_home.smarthome.entities.SensorData

class EnvSensorsFragment(params: IGraphParameters) : SensorsFragment(R.layout.fragment_env_sensors, params, "env"), View.OnClickListener {
    private var mNextTempIntGraph: Button? = null
    private var mNextTempExtGraph: Button? = null
    private var mNextHumiGraph: Button? = null
    private var mIntTempSensors: MutableList<List<IGraphData>>? = null
    private var mExtTempSensors: MutableList<List<IGraphData>>? = null
    private var mHumiSensors: MutableList<List<IGraphData>>? = null
    private var mPresSensors: MutableList<IGraphData>? = null
    private var mIntTempSensorIdx: Int = 0
    private var mExtTempSensorIdx: Int = 0
    private var mHumiSensorIdx: Int = 0
    
    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?,
                              savedInstanceState: Bundle?): View {
        val rootView = super.onCreateView(inflater, container, savedInstanceState)
        mNextTempIntGraph = rootView!!.findViewById<Button>(R.id.next_temp_int_graph)
        mNextTempIntGraph!!.setOnClickListener(this)
        mNextTempExtGraph = rootView.findViewById<Button>(R.id.next_temp_ext_graph)
        mNextTempExtGraph!!.setOnClickListener(this)
        mNextHumiGraph = rootView.findViewById<Button>(R.id.next_humi_graph)
        mNextHumiGraph!!.setOnClickListener(this)
        return rootView
    }

    override fun showResults(results: List<SensorData>) {
        buildSensorLists(results)
        mIntTempSensorIdx = 0
        mExtTempSensorIdx = 0
        mHumiSensorIdx = 0
        showResults(requireView())
    }

    private fun buildSensorLists(results: List<SensorData>) {
        val intTempSensorsMap = mutableMapOf<String, MutableList<IGraphData>>()
        val extTempSensorsMap = mutableMapOf<String, MutableList<IGraphData>>()
        val humiSensorsMap = mutableMapOf<String, MutableList<IGraphData>>()
        mPresSensors = mutableListOf()
        results.forEach { data ->
            if (data.data.containsKey("temp")) {
                if (data.locationType != "ext") {
                    val list = intTempSensorsMap[data.seriesColumnData]
                    if (list == null) {
                        intTempSensorsMap[data.seriesColumnData] = mutableListOf(data)
                    } else {
                        list.add(data)
                    }
                } else {
                    val list = extTempSensorsMap[data.seriesColumnData]
                    if (list == null) {
                        extTempSensorsMap[data.seriesColumnData] = mutableListOf(data)
                    } else {
                        list.add(data)
                    }
                }
            }
            if (data.data.containsKey("humi")) {
                val list = humiSensorsMap[data.seriesColumnData]
                if (list == null) {
                    humiSensorsMap[data.seriesColumnData] = mutableListOf(data)
                } else {
                    list.add(data)
                }
            }
            if (data.data.containsKey("pres")) {
                mPresSensors!!.add(data)
            }
        }

        mIntTempSensors = ArrayList()
        intTempSensorsMap.forEach { _, v ->
            mIntTempSensors!!.add(v)
        }
        mExtTempSensors = ArrayList()
        extTempSensorsMap.forEach { _, v ->
            mExtTempSensors!!.add(v)
        }
        mHumiSensors = ArrayList()
        humiSensorsMap.forEach { _, v ->
            mHumiSensors!!.add(v)
        }
    }

    private fun showResults(v: View) {
        showIntTempSensors(v)
        showExtTempSensors(v)
        showHumiSensors(v)
        val plot = v.findViewById<XYPlot>(R.id.plot_pres)
        Graph.buildGraph(plot, mPresSensors!!, "pres", false, !params!!.getData().mUsePeriod, 2, 2)
    }

    private fun showIntTempSensors(v: View) {
        if (mIntTempSensors != null && mIntTempSensorIdx < mIntTempSensors!!.size) {
            val plot = v.findViewById<XYPlot>(R.id.plot_temp_int)
            Graph.buildGraph(plot, mIntTempSensors!![mIntTempSensorIdx], "temp", false, !params!!.getData().mUsePeriod,2, 2)
        }
    }

    private fun showExtTempSensors(v: View) {
        if (mExtTempSensors != null && mExtTempSensorIdx < mExtTempSensors!!.size) {
            val plot = v.findViewById<XYPlot>(R.id.plot_temp_ext)
            Graph.buildGraph(plot, mExtTempSensors!![mExtTempSensorIdx], "temp", false, !params!!.getData().mUsePeriod, 2, 2)
        }
    }

    private fun showHumiSensors(v: View) {
        if (mHumiSensors != null && mHumiSensorIdx < mHumiSensors!!.size) {
            val plot = v.findViewById<XYPlot>(R.id.plot_humi)
            Graph.buildGraph(plot, mHumiSensors!![mHumiSensorIdx], "humi", false, !params!!.getData().mUsePeriod, 2, 2)
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
        }
    }

    private fun showNextTempIntGraph() {
        if (mIntTempSensors != null && mIntTempSensors!!.size > 0) {
            mIntTempSensorIdx++
            if (mIntTempSensorIdx >= mIntTempSensors!!.size) {
                mIntTempSensorIdx = 0
            }
            showIntTempSensors(requireView())
        }
    }

    private fun showNextTempExtGraph() {
        if (mExtTempSensors != null && mExtTempSensors!!.size > 0) {
            mExtTempSensorIdx++
            if (mExtTempSensorIdx >= mExtTempSensors!!.size) {
                mExtTempSensorIdx = 0
            }
            showExtTempSensors(requireView())
        }
    }

    private fun showNextHumiGraph() {
        if (mHumiSensors != null && mHumiSensors!!.size > 0) {
            mHumiSensorIdx++
            if (mHumiSensorIdx >= mHumiSensors!!.size) {
                mHumiSensorIdx = 0
            }
            showHumiSensors(requireView())
        }
    }
}
