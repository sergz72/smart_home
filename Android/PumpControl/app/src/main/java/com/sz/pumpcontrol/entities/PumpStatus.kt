package com.sz.pumpcontrol.entities

import com.google.gson.annotations.SerializedName

data class PumpStatus(@SerializedName("MinPressure") val minPressure: Int,
                      @SerializedName("MaxPressure") val maxPressure: Int,
                      @SerializedName("LastPressure") val lastPressure: Int,
                      @SerializedName("PumpIsOn") val pumpIsOn: Boolean,
                      @SerializedName("ForcedOff") val forcedOff: Boolean,
                      @SerializedName("LastPumpOn") val lastPumpOn: Long,
                      @SerializedName("LastPumpOff") val lastPumpOff: Long) {
}