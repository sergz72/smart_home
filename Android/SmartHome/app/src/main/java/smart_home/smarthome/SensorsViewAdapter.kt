package smart_home.smarthome

import android.content.res.Resources
import android.graphics.Color
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView

import smart_home.smarthome.entities.SensorData

class SensorsViewAdapter(private val mResources: Resources) : RecyclerView.Adapter<SensorsViewAdapter.ViewHolder>() {
    private val mData = mutableListOf<List<SensorData>>()
    var selectedPosition = 0
        private set

    fun setData(data: Map<String, List<SensorData>>) {
        mData.clear()
        val keys = data.keys.sorted()
        for (key in keys) {
            mData.add(data[key]!!)
        }
        selectedPosition = 0
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
        val v = LayoutInflater.from(parent.context)
                .inflate(R.layout.sensor_data_row, parent, false) as View
        return ViewHolder(v)
    }

    override fun onBindViewHolder(holder: ViewHolder, position: Int) {
        val d = mData[position]
        holder.mLocationNameView.text = d[0].seriesColumnData
        holder.mDataView.text = d.map { it.getDataPresentation(mResources) }.joinToString("")
        holder.itemView.setBackgroundColor(if (selectedPosition == position) Color.GREEN else Color.TRANSPARENT)
    }

    override fun getItemCount(): Int {
        return mData.size
    }

    inner class ViewHolder(v: View) : RecyclerView.ViewHolder(v), View.OnClickListener {
        var mLocationNameView: TextView
        var mDataView: TextView

        init {
            mLocationNameView = v.findViewById<View>(R.id.location_name) as TextView
            mDataView = v.findViewById<View>(R.id.data) as TextView
            v.setOnClickListener(this)
        }

        override fun onClick(v: View) {
            if (adapterPosition == RecyclerView.NO_POSITION)
                return

            // Updating old as well as new positions
            notifyItemChanged(selectedPosition)
            selectedPosition = adapterPosition
            notifyItemChanged(selectedPosition)
        }
    }
}
