package smart_home.smarthome

import android.annotation.SuppressLint
import android.content.Context
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import smart_home.smarthome.entities.LastSensorDataResponse
import smart_home.smarthome.service.SmartHomeService

class HomePageFragment(service: ServiceHolder) :
    SensorsFragment(service, R.layout.fragment_home_page, null, "all") {
    private var mSensorsView: RecyclerView? = null
    private var mSensorsViewAdapter: SensorsViewAdapter? = null

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?,
                              savedInstanceState: Bundle?): View {
        val rootView = super.onCreateView(inflater, container, savedInstanceState)
        mSensorsView = rootView!!.findViewById(R.id.sensors_view)
        mSensorsView!!.hasFixedSize()
        val lm = LinearLayoutManager(this.context)
        lm.orientation = LinearLayoutManager.VERTICAL
        mSensorsView!!.layoutManager = lm
        mSensorsView!!.adapter = mSensorsViewAdapter
        return rootView
    }

    override fun onAttach(context: Context) {
        mSensorsViewAdapter = SensorsViewAdapter(this.resources, this.service)
        super.onAttach(context)
    }

    override fun refresh() {
        service.service!!.getLastSensorData(
            {response -> mHandler.post { refresh(response) } },
            { t -> onFailure(t) },
            requireActivity())
    }

    @SuppressLint("NotifyDataSetChanged")
    private fun refresh(results: LastSensorDataResponse) {
        mSensorsViewAdapter!!.setData(results)
        mSensorsViewAdapter!!.notifyDataSetChanged()
    }
}
