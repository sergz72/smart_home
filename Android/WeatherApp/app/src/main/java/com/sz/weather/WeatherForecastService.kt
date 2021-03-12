package com.sz.weather

import retrofit2.Call
import com.sz.weather.entities.ForecastData
import retrofit2.http.GET

interface WeatherForecastService {
    @GET("/data/2.5/onecall?lat=50.549916&lon=30.240000&exclude=current,minutely,daily&units=metric&appid=cee8d1171f06c601ff868efbec254351")
    fun forecast(): Call<ForecastData>
}