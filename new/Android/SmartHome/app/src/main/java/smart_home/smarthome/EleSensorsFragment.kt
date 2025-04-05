package smart_home.smarthome

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.TextView
import com.sz.charts.LineChart
import smart_home.smarthome.entities.SensorData
import smart_home.smarthome.entities.SensorDataResponseV1

class EleSensorsFragment(params: IGraphParameters) : SensorsFragment(R.layout.fragment_ele_sensors, params, "ele"), View.OnClickListener {
    private var mNextPowerGraph: Button? = null
    private var mTotalLabel: TextView? = null
    private var mPowerSensors: List<SensorData>? = null
    private var mVoltageSensors: List<SensorData>? = null
    private var mPowerSensorIdx: Int = 0
    private var mAggregated: Boolean = false

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?,
                              savedInstanceState: Bundle?): View {
        val rootView = super.onCreateView(inflater, container, savedInstanceState)
        mNextPowerGraph = rootView!!.findViewById(R.id.next_power_graph)
        mNextPowerGraph!!.setOnClickListener(this)
        mTotalLabel = rootView.findViewById(R.id.total_power)
        return rootView
    }

    override fun showResults(results: SensorDataResponseV1) {
        buildSensorLists(results.response)
        mPowerSensorIdx = 0
        showResults(requireView(), results.aggregated)
    }

    private fun buildSensorLists(results: List<SensorData>) {
        mPowerSensors = findSensors(results, { true }, { it.contains("pwr") })
        mVoltageSensors = findSensors(results, { true }, { it -> it.any { it.startsWith("v") } })
    }

    private fun showResults(v: View, aggregated: Boolean) {
        mAggregated = aggregated
        showPowerSensors(v)
        if (!aggregated) {
            val plot = requireView().findViewById<View>(R.id.plot_voltage) as LineChart
            Graph.buildGraph(plot, mVoltageSensors!!, "v", true, false, true)
        }
    }

    private fun showPowerSensors(v: View) {
        if (mPowerSensors != null && mPowerSensorIdx < mPowerSensors!!.size) {
            val plot = v.findViewById<LineChart>(R.id.plot_power)
            val data = mPowerSensors!![mPowerSensorIdx]
            Graph.buildGraph(plot, data, "pwr", false, mAggregated, false)
            mTotalLabel!!.text = getString(R.string.total, data.total / 100)
        }
    }

    private fun showNextPowerGraph() {
        if (mPowerSensors != null && mPowerSensors!!.isNotEmpty()) {
            mPowerSensorIdx++
            if (mPowerSensorIdx >= mPowerSensors!!.size) {
                mPowerSensorIdx = 0
            }
            showPowerSensors(requireView())
        }
    }

    override fun onClick(v: View?) {
        when (v!!) {
            mNextPowerGraph -> showNextPowerGraph()
        }
    }
}
