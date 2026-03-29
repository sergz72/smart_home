package com.sz.motionsensor.entities

import java.time.LocalTime

const val FLAG_NONE = 0
const val FLAG_PREALARM = 1
const val FLAG_ALARM = 2

data class MotionEvent(val time: LocalTime, val hcDistance: Int, val vlDistance: Int, val flag: Int)
