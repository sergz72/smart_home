package smart_home.smarthome

import android.content.Context
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment

import smart_home.smarthome.entities.SensorData
import smart_home.smarthome.service.SensorDataQuery
import smart_home.smarthome.service.SmartHomeService

abstract class SensorsFragment(protected val service: SmartHomeService,
                               private val mId: Int, protected val params: IGraphParameters?,
                                  private val  dataType: String, private val maxPoints: Int = 200):
    Fragment(), ISmartHomeData {

    fun findSensors(results: List<SensorData>, locationTypeChecker: (String) -> Boolean, dataTypeChecker: (Set<String>) -> Boolean): List<SensorData> {
        return results.filter { sd -> locationTypeChecker(sd.locationType) && dataTypeChecker(sd.series.last().data.keys) }
    }

    private fun buildQuery(): SensorDataQuery {
        val period = params!!.getData().getPeriod()
        if (params.getData().mDateStart == null) {
            return SensorDataQuery(
                maxPoints.toShort(), dataType, null,
                params.getData().getDateOffset(), period
            )
        }
        return SensorDataQuery(
            maxPoints.toShort(), dataType, params.getData().getFromDate(),
            null, period
        )
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?,
                              savedInstanceState: Bundle?): View? {
        // Inflate the layout for this fragment
        return inflater.inflate(mId, container, false)
    }

    override fun onAttach(context: Context) {
        super.onAttach(context)
        refresh()
    }
}
