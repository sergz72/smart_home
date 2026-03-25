package smart_home.smarthome

import android.view.View
import com.sz.charts.LineChart
import smart_home.smarthome.entities.SensorDataResponse

class WatSensorsFragment(params: IGraphParameters, service: ServiceHolder) :
    SensorsFragment(service, R.layout.fragment_wat_sensors, params, "wat", 10000) {
    override fun refresh() {
        service.service!!.getSensorData(
            buildQuery(),
            {response -> mHandler.post { refresh(response) }},
            { t -> onFailure(t) },
            requireActivity())
    }

    private fun refresh(response: SensorDataResponse) {
        var plot = requireView().findViewById<View>(R.id.plot_pressure) as LineChart
        val presSensors = response.response["pres"]?.firstOrNull()
        if (presSensors != null) {
            Graph.buildGraph(plot, presSensors, getLocationName(presSensors),
                onlyAvg = false,
                useFloat0 = false
            )
        } else {
            plot.clearSeries()
        }
        plot = requireView().findViewById<View>(R.id.plot_current) as LineChart
        val iccSensors = response.response["icc "]?.firstOrNull()
        if (iccSensors != null) {
            Graph.buildGraph(plot, iccSensors, getLocationName(iccSensors),
                onlyAvg = false,
                useFloat0 = false
            )
        } else {
            plot.clearSeries()
        }
    }
}
