package smart_home.smarthome

import android.view.View
import com.androidplot.xy.XYPlot
import smart_home.smarthome.entities.SensorData

class EleSensorsFragment(params: IGraphParameters) : SensorsFragment(R.layout.fragment_ele_sensors, params, "ele") {
    override fun showResults(results: List<SensorData>) {
        if (params!!.getData().mUsePeriod) {
            val plot = requireView().findViewById<View>(R.id.plot_voltage) as XYPlot
            Graph.buildGraph(plot, results, "v", true, false, 2, 2)
        }
    }
}
