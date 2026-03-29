package com.sz.motionsensor

import android.graphics.Color
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import com.sz.motionsensor.entities.FLAG_NONE
import com.sz.motionsensor.entities.FLAG_PREALARM
import com.sz.motionsensor.entities.MotionEvent

class EventsViewAdapter : RecyclerView.Adapter<EventsViewAdapter.ViewHolder>() {
    private val mData = mutableListOf<MotionEvent>()
    var selectedPosition = 0
        private set

    fun setData(data: List<MotionEvent>) {
        mData.clear()
        //reverse list
        for (item in data) {
            mData.add(0, item)
        }
        selectedPosition = 0
    }

    fun addData(e: MotionEvent) {
        mData.add(0, e)
        selectedPosition = 0
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
        val v = LayoutInflater.from(parent.context)
                .inflate(R.layout.event_data_row, parent, false) as View
        return ViewHolder(v)
    }

    override fun onBindViewHolder(holder: ViewHolder, position: Int) {
        val d = mData[position]
        holder.mTimeView.text = d.time.toString()
        holder.mHCDistanceView.text = d.hcDistance.toString()
        holder.mVLDistanceView.text = d.vlDistance.toString()
        holder.itemView.setBackgroundColor(buildColor(position, d.flag))
    }

    private fun buildColor(position: Int, flag: Int): Int {
        return if (selectedPosition == position) {
            when (flag) {
                FLAG_NONE -> Color.GREEN
                FLAG_PREALARM -> Color.CYAN
                else -> Color.MAGENTA
            }
        } else {
            when (flag) {
                FLAG_NONE -> Color.TRANSPARENT
                FLAG_PREALARM -> Color.YELLOW
                else -> Color.RED
            }
        }
    }

    override fun getItemCount(): Int {
        return mData.size
    }

    inner class ViewHolder(v: View) : RecyclerView.ViewHolder(v), View.OnClickListener {
        var mTimeView: TextView = v.findViewById(R.id.time)
        var mHCDistanceView: TextView = v.findViewById(R.id.hcDistance)
        var mVLDistanceView: TextView = v.findViewById(R.id.vlDistance)

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
