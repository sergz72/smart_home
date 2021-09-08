package com.sz.heatercontrol.entities

import com.google.gson.annotations.SerializedName

data class HeaterStatus(@SerializedName("SetTemp") val setTemp: Int,
                        @SerializedName("LastTemp") val lastTemp: Int,
                        @SerializedName("HeaterOn") val heaterOn: Boolean,
                        @SerializedName("DayTemp") val dayTemp: Int,
                        @SerializedName("NightTemp") val nightTemp: Int) {
}