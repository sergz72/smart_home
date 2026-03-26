package smart_home.smarthome

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.TextView
import com.sz.charts.LineChart
import smart_home.smarthome.entities.SensorData
import smart_home.smarthome.entities.SensorDataResponse

class EleSensorsFragment(params: IGraphParameters, service: ServiceHolder) :
    SensorsFragment(service, R.layout.fragment_ele_sensors, params, "ele"), View.OnClickListener {
    private var mNextPowerGraph: Button? = null
    private var mTotalLabel: TextView? = null
    private var mPowerSensors: List<SensorData>? = null
    // map locationId -> map valueType -> data
    private var mVoltageSensors: Map<Int, Map<String, SensorData>>? = null
    private var mPowerSensorIdx: Int = 0

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?,
                              savedInstanceState: Bundle?): View {
        val rootView = super.onCreateView(inflater, container, savedInstanceState)
        mNextPowerGraph = rootView!!.findViewById(R.id.next_power_graph)
        mNextPowerGraph!!.setOnClickListener(this)
        mTotalLabel = rootView.findViewById(R.id.total_power)
        return rootView
    }

    private fun buildSensorLists(results: SensorDataResponse) {
        mPowerSensors = results.response["pwr "]
        mVoltageSensors = results.response
            .filter { it.key.startsWith("v") }
            .flatMap { s -> s.value.map { ss -> Pair(ss.locationId, Pair(s.key, ss)) } }
            .groupBy { it.first }
            .map { (key, value) -> key to value.associate { it.second.first to it.second.second } }
            .toMap()
    }

    fun refresh(results: SensorDataResponse) {
        buildSensorLists(results)
        mPowerSensorIdx = 0
        showResults(requireView())
    }

    private fun showResults(v: View) {
        showPowerSensors(v)
        val plot = requireView().findViewById<View>(R.id.plot_voltage) as LineChart
        //todo
        //Graph.buildGraph(plot, mVoltageSensors!!, getLocationName(mVoltageSensors!!), onlyAvg = true, useFloat0 = false)
    }

    private fun showPowerSensors(v: View) {
        if (mPowerSensors != null && mPowerSensorIdx < mPowerSensors!!.size) {
            val plot = v.findViewById<LineChart>(R.id.plot_power)
            val data = mPowerSensors!![mPowerSensorIdx]
            Graph.buildGraph(plot, data, getLocationName(data), onlyAvg = true, useFloat0 = false)
            mTotalLabel!!.text = getString(R.string.total, data.calculateTotal())
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

    override fun refresh() {
        service.service!!.getSensorData(
            buildQuery(),
            {response -> mHandler.post { refresh(response) }},
            { t -> onFailure(t) },
            requireActivity())
    }
}
