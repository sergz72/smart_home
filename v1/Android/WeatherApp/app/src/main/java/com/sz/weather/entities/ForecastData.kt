package com.sz.weather.entities

import com.google.gson.annotations.SerializedName

data class ForecastData(@SerializedName("hourly") val hourlyData: List<HourlyData>) {

}

data class HourlyData(@SerializedName("dt") val dt: Int,
                      @SerializedName("temp") val temp: Double,
                      @SerializedName("pressure") val pressure: Double,
                      @SerializedName("humidity") val humidity: Double,
                      @SerializedName("wind_speed") val windSpeed: Double,
                      @SerializedName("weather") val weather: List<WeatherData>) {
}

data class WeatherData(@SerializedName("description") val description: String,
                       @SerializedName("icon") val icon: String) {
}
