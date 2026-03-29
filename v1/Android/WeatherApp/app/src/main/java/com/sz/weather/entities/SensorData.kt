package com.sz.weather.entities

import com.google.gson.annotations.SerializedName

data class SensorData(@SerializedName("locationName") val locationName: String,
                      @SerializedName("locationType") val locationType: String,
                      @SerializedName("dataType") val dataType: String,
                      @SerializedName("data") val data: Map<String, Any>) {

    fun getData(dataName: String): Double? {
        val v = data[dataName]
        return v as Double?
    }
}
