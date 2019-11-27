package smart_home.smarthome

import android.content.res.Configuration
import android.os.Bundle
import android.view.View

import com.androidplot.xy.XYPlot
import smart_home.smarthome.entities.SensorData

class EnvSensorsFragment : SensorsFragment(R.layout.fragment_env_sensors, "data_type=env&period=24") {
    internal var mData: List<IGraphData>? = null

    override fun showResults(results: List<SensorData>) {
        mData = results
        showResults(view!!)
    }

    private fun showResults(v: View) {
        var plot = v.findViewById<View>(R.id.plot_temp) as XYPlot
        Graph.buildGraph(plot, mData!!, "temp")
        plot = v.findViewById<View>(R.id.plot_humi) as XYPlot
        Graph.buildGraph(plot, mData!!, "humi")
        plot = v.findViewById<View>(R.id.plot_pres) as XYPlot
        Graph.buildGraph(plot, mData!!, "pres")
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        if (mData != null) {
            showResults(view)
        }
    }

    override fun onConfigurationChanged(newConfig: Configuration?) {
        super.onConfigurationChanged(newConfig)
        fragmentManager!!
                .beginTransaction()
                .detach(this)
                .attach(this)
                .commit()
    }
}
