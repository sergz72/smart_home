package com.sz.weather.entities

import com.google.gson.annotations.SerializedName

data class ClimacellData(@SerializedName("data") val data: ClimacellDataData) {
}

data class ClimacellDataData(@SerializedName("timelines") val timelines: List<ClimacellTimeline>) {
}

data class ClimacellTimeline(@SerializedName("intervals") val intervals: List<ClimacellInterval>) {
}

data class ClimacellInterval(@SerializedName("values") val values: ClimacellValues) {
}

data class ClimacellValues(@SerializedName("temperature") val temperature: Double,
                           @SerializedName("windSpeed") val windSpeed: Double,
                           @SerializedName("pressureSurfaceLevel") val pressure: Double,
                           @SerializedName("weatherCode") val weatherCode: Int) {
}
