package smart_home.smarthome

import android.view.View
import com.sz.charts.LineChart
import smart_home.smarthome.entities.SensorData
import smart_home.smarthome.entities.SensorDataResponseV1

class WatSensorsFragment(params: IGraphParameters) : SensorsFragment(R.layout.fragment_wat_sensors, params, "wat", 10000) {
    override fun showResults(results: SensorDataResponseV1) {
        if (!results.aggregated) {
            var plot = requireView().findViewById<View>(R.id.plot_pressure) as LineChart
            Graph.buildGraph(plot, results.response, "pres", false, false, false)
            plot = requireView().findViewById<View>(R.id.plot_current) as LineChart
            Graph.buildGraph(plot, results.response, "icc", false, false, false)
        }
    }
}
