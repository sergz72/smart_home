package smart_home.smarthome

import android.annotation.SuppressLint
import android.content.res.Resources
import android.graphics.Color
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import smart_home.smarthome.entities.LastSensorDataResponse
import smart_home.smarthome.entities.Locations

import smart_home.smarthome.entities.SensorDataItem
import java.time.Instant
import java.time.LocalDateTime
import java.time.ZoneId
import java.time.format.DateTimeFormatter

data class SensorViewItem(val valueType: String, val data: SensorDataItem)
data class SensorViewLocation(val locationName: String, val data: List<SensorViewItem>) {
    companion object {
        var formatter: DateTimeFormatter = DateTimeFormatter.ofPattern("HH:mm")

        fun build(
            kv: Map.Entry<Int, Map<String, SensorDataItem>>,
            service: ServiceHolder,
            resources: Resources
        ): SensorViewLocation {
            val locationName = service.service!!.getLocations().locations[kv.key]!!.name
            val data = kv.value.map { SensorViewItem(Locations.getDataPresentation(resources, it.key), it.value) }
            return SensorViewLocation(locationName, data)
        }
    }

    @SuppressLint("DefaultLocale")
    fun buildText(zoneId: ZoneId): String {
        val sb = StringBuilder()
        for (d in data) {
            sb.append("  ")
                .append(d.valueType)
                .append(": ")
                .append(String.format("%.2f", d.data.value))
                .append('(')
                .append(LocalDateTime.ofInstant(Instant.ofEpochMilli(d.data.timestamp), zoneId).format(formatter))
                .append(')')
                .append('\n')
        }
        return sb.toString()
    }
}


class SensorsViewAdapter(private val mResources: Resources, private val mService: ServiceHolder) :
    RecyclerView.Adapter<SensorsViewAdapter.ViewHolder>() {
    private val mData = mutableListOf<SensorViewLocation>()
    var selectedPosition = 0
        private set

    fun setData(data: LastSensorDataResponse) {
        mData.clear()
        for (kv in data.response) {
            mData.add(SensorViewLocation.build(kv, mService, mResources))
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
        holder.mLocationNameView.text = d.locationName
        holder.mDataView.text = d.buildText(mService.service!!.getLocations().timeZone)
        holder.itemView.setBackgroundColor(if (selectedPosition == position) Color.GREEN else Color.TRANSPARENT)
    }

    override fun getItemCount(): Int {
        return mData.size
    }

    inner class ViewHolder(v: View) : RecyclerView.ViewHolder(v), View.OnClickListener {
        var mLocationNameView: TextView = v.findViewById(R.id.location_name)
        var mDataView: TextView = v.findViewById(R.id.data)

        init {
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
