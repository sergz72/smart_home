package com.sz.motionsensor

import com.sz.motionsensor.entities.FLAG_ALARM
import com.sz.motionsensor.entities.FLAG_NONE
import com.sz.motionsensor.entities.MotionEvent
import java.time.LocalTime

class MotionHistory(capacity: Int): FixedSizeHistory<MotionEvent>(capacity) {
    private var minHCDistance: Int = 0
    private var minVLDistance: Int = 0

    fun setParameters(minHCDistance: Int, minVLDistance: Int) {
        this.minVLDistance = minVLDistance
        this.minHCDistance = minHCDistance
    }

    suspend fun addItem(hcDistance: Int, vlDistance: Int): MotionEvent {
        lock.acquire()
        val e = MotionEvent(LocalTime.now(), hcDistance, vlDistance, buildFlag(hcDistance, vlDistance))
        internalAddItem(e)
        lock.release()
        return e
    }

    private fun buildFlag(hcDistance: Int, vlDistance: Int): Int {
        if ((hcDistance in 1 until minHCDistance) || (vlDistance in 1 until minVLDistance)) {
            return FLAG_ALARM
        }
        return FLAG_NONE
    }
}