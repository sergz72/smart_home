package smart_home.smarthome

import android.view.View
import com.androidplot.xy.XYPlot
import smart_home.smarthome.entities.SensorData

class EleSensorsFragment : SensorsFragment(R.layout.fragment_ele_sensors, "data_type=ele&period=24") {
    override fun showResults(results: List<SensorData>) {
        val plot = view!!.findViewById<View>(R.id.plot_voltage) as XYPlot
        Graph.buildGraph(plot, results, "v", true)
    }
}
