package smart_home.smarthome

import android.content.Context
import android.os.Bundle
import android.support.v7.widget.LinearLayoutManager
import android.support.v7.widget.RecyclerView
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup

import smart_home.smarthome.entities.SensorData

class HomePageFragment : SensorsFragment(R.layout.fragment_home_page, "data_type=all") {
    private var mSensorsView: RecyclerView? = null
    private var mSensorsViewAdapter: SensorsViewAdapter? = null

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?,
                              savedInstanceState: Bundle?): View? {
        val rootView = super.onCreateView(inflater, container, savedInstanceState)
        mSensorsView = rootView!!.findViewById(R.id.sensors_view)
        mSensorsView!!.hasFixedSize()
        val lm = LinearLayoutManager(this.context)
        lm.orientation = LinearLayoutManager.VERTICAL
        mSensorsView!!.layoutManager = lm
        mSensorsView!!.adapter = mSensorsViewAdapter
        return rootView
    }

    override fun onAttach(context: Context?) {
        mSensorsViewAdapter = SensorsViewAdapter(this.resources)
        super.onAttach(context)
    }

    override fun showResults(results: List<SensorData>) {
        val dataMap = results.groupBy {  it.seriesColumnData }
        mSensorsViewAdapter!!.setData(dataMap)
        mSensorsViewAdapter!!.notifyDataSetChanged()
    }
}
