package com.sz.weather

import com.sz.weather.entities.ClimacellData
import retrofit2.Call
import com.sz.weather.entities.ForecastData
import retrofit2.http.GET
import retrofit2.http.Headers

interface ClimaCellService {
    @GET("/")
    fun forecast(): Call<ClimacellData>
}