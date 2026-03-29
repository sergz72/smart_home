package com.sz.weather

import android.app.Activity
import android.app.AlertDialog
import android.content.Intent
import android.os.Bundle
import android.os.CountDownTimer
import android.os.Handler
import android.os.Looper
import android.view.Menu
import android.view.MenuItem
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import com.google.gson.Gson
import com.google.gson.GsonBuilder
import com.google.gson.reflect.TypeToken
import com.sz.weather.entities.ClimacellData
import com.sz.weather.entities.ForecastData
import com.sz.weather.entities.SensorData
import okhttp3.OkHttpClient
import retrofit2.Call
import retrofit2.Callback
import retrofit2.Response
import retrofit2.Retrofit
import retrofit2.converter.gson.GsonConverterFactory
import java.security.SecureRandom
import java.security.cert.CertificateException
import java.security.cert.X509Certificate
import java.util.*
import javax.net.ssl.SSLContext
import javax.net.ssl.SSLSocketFactory
import javax.net.ssl.TrustManager
import javax.net.ssl.X509TrustManager

class MainActivity : AppCompatActivity() {
    companion object {
        const val PREFS_NAME = "WeatherApp"
        private const val SETTINGS = 1

        fun alert(activity: Activity, message: String?) {
            val builder = AlertDialog.Builder(activity)
            builder.setMessage(message)
                .setTitle("Error")
            val dialog = builder.create()
            dialog.show()

            object : CountDownTimer(5000, 1000) {
                override fun onTick(millisUntilFinished: Long) {
                    // TODO Auto-generated method stub
                }

                override fun onFinish() {
                    // TODO Auto-generated method stub
                    dialog.dismiss()
                }
            }.start()
        }
    }

    private var mDate: Calendar = Calendar.getInstance()
    private var mSecondSymbolIsOn: Boolean = true
    private val mHandler: Handler = Handler(Looper.getMainLooper())
    private var mSecondCounter: Int = 0
    private val mGson: Gson = GsonBuilder().create()
    private val mService = buildService()
    private var useClimacellAPI: Boolean = true

    private fun buildService(): SmartHomeService<List<SensorData>> {
        return SmartHomeService<List<SensorData>>()
            .setRequest("GET /sensor_data?data_type=all")
    }

    fun timerTask() {
        mSecondCounter++
        if (mSecondCounter == 600) { // 10 min
            mSecondCounter = 0
            weatherUpdate()
        }

        val c = Calendar.getInstance()

        val minute = c.get(Calendar.MINUTE)
        if (minute != mDate.get(Calendar.MINUTE)) {
            val v = findViewById<TextView>(R.id.minutes)
            v.text = String.format("%02d", minute)
        }

        val hour = c.get(Calendar.HOUR_OF_DAY)
        if (hour != mDate.get(Calendar.HOUR)) {
            val v = findViewById<TextView>(R.id.hours)
            v.text = String.format("%2d", hour)
        }

        val sep = findViewById<TextView>(R.id.separator)
        mSecondSymbolIsOn = !mSecondSymbolIsOn
        sep.text = if (mSecondSymbolIsOn) ":" else " "

        val day = c.get(Calendar.DAY_OF_MONTH)
        val month = c.get(Calendar.MONTH)
        val year = c.get(Calendar.YEAR)
        if (day != mDate.get(Calendar.DAY_OF_MONTH) || month != mDate.get(Calendar.MONTH) ||
            year != mDate.get(Calendar.YEAR)) {
            val dow = c.get(Calendar.DAY_OF_WEEK)
            val dowString =  resources.getStringArray(R.array.weekdayNames)[dow - 1]
            val dowV = findViewById<TextView>(R.id.day_of_week)
            dowV.text = dowString
            val dateV = findViewById<TextView>(R.id.date)
            dateV.text = String.format("%2d.%02d.%d", day, month + 1, year)
        }
        mDate = c
    }

    private fun weatherUpdate() {
        localWeatherUpdate()
        weatherForecastUpdate()
    }

    private fun getUnsafeOkHttpClient(): OkHttpClient.Builder {
        try {
            // Create a trust manager that does not validate certificate chains
            val trustAllCerts: Array<TrustManager> = arrayOf(
                object : X509TrustManager {
                    @Throws(CertificateException::class)
                    override fun checkClientTrusted(
                        chain: Array<X509Certificate?>?,
                        authType: String?
                    ) = Unit

                    @Throws(CertificateException::class)
                    override fun checkServerTrusted(
                        chain: Array<X509Certificate?>?,
                        authType: String?
                    ) = Unit

                    override fun getAcceptedIssuers(): Array<X509Certificate> = arrayOf()
                }
            )
            // Install the all-trusting trust manager
            val sslContext: SSLContext = SSLContext.getInstance("SSL")
            sslContext.init(null, trustAllCerts, SecureRandom())
            // Create an ssl socket factory with our all-trusting manager
            val sslSocketFactory: SSLSocketFactory = sslContext.socketFactory
            val builder = OkHttpClient.Builder()
            builder.sslSocketFactory(
                sslSocketFactory,
                trustAllCerts[0] as X509TrustManager
            )
            builder.hostnameVerifier { _, _ -> true }
            return builder
        } catch (e: Exception) {
            throw RuntimeException(e)
        }
    }

    private fun weatherForecastUpdate() {
        if (useClimacellAPI) {
            climacellDataUpdate()
            return
        }
        val httpClient = getUnsafeOkHttpClient()
        val retrofit = Retrofit.Builder()
            .baseUrl("https://api.openweathermap.org")
            .client(httpClient.build())
            .addConverterFactory(GsonConverterFactory.create())
            .build()
        val service = retrofit.create(WeatherForecastService::class.java)
        val call = service.forecast()
        call.enqueue(object : Callback<ForecastData> {
            override fun onResponse(call: Call<ForecastData>, response: Response<ForecastData>) {
                mHandler.post { showWeatherForecast(response.body()!!) }
            }

            override fun onFailure(call: Call<ForecastData>, t: Throwable) {
                alert(this@MainActivity, t.toString())
            }
        })
    }

    private fun climacellDataUpdate() {
        val httpClient = getUnsafeOkHttpClient()
        val retrofit = Retrofit.Builder()
            .baseUrl("http://XXX.XXX.XXX.XXX:8080")
            .client(httpClient.build())
            .addConverterFactory(GsonConverterFactory.create())
            .build()
        val service = retrofit.create(ClimaCellService::class.java)
        val call = service.forecast()
        call.enqueue(object : Callback<ClimacellData> {
            override fun onResponse(call: Call<ClimacellData>, response: Response<ClimacellData>) {
                mHandler.post { showClimacellForecast(response.body()!!) }
            }

            override fun onFailure(call: Call<ClimacellData>, t: Throwable) {
                alert(this@MainActivity, t.toString())
            }
        })
    }

    private fun showClimacellForecast(body: ClimacellData) {
        val v1 = findViewById<ForecastItem>(R.id.forecastItem1)
        v1.setClimacellData(body.data.timelines[0].intervals[2], 2)
        val v2 = findViewById<ForecastItem>(R.id.forecastItem2)
        v2.setClimacellData(body.data.timelines[0].intervals[5], 5)
        val v3 = findViewById<ForecastItem>(R.id.forecastItem3)
        v3.setClimacellData(body.data.timelines[0].intervals[8], 8)
        val v4 = findViewById<ForecastItem>(R.id.forecastItem4)
        v4.setClimacellData(body.data.timelines[0].intervals[11], 11)
        val v5 = findViewById<ForecastItem>(R.id.forecastItem5)
        v5.setClimacellData(body.data.timelines[0].intervals[14], 14)
        val v6 = findViewById<ForecastItem>(R.id.forecastItem6)
        v6.setClimacellData(body.data.timelines[0].intervals[17], 17)
        val v7 = findViewById<ForecastItem>(R.id.forecastItem7)
        v7.setClimacellData(body.data.timelines[0].intervals[20], 20)
        val v8 = findViewById<ForecastItem>(R.id.forecastItem8)
        v8.setClimacellData(body.data.timelines[0].intervals[23], 23)
    }

    private fun showWeatherForecast(body: ForecastData) {
        val v1 = findViewById<ForecastItem>(R.id.forecastItem1)
        v1.setData(body.hourlyData[0])
        val v2 = findViewById<ForecastItem>(R.id.forecastItem2)
        v2.setData(body.hourlyData[3])
        val v3 = findViewById<ForecastItem>(R.id.forecastItem3)
        v3.setData(body.hourlyData[6])
        val v4 = findViewById<ForecastItem>(R.id.forecastItem4)
        v4.setData(body.hourlyData[9])
        val v5 = findViewById<ForecastItem>(R.id.forecastItem5)
        v5.setData(body.hourlyData[12])
        val v6 = findViewById<ForecastItem>(R.id.forecastItem6)
        v6.setData(body.hourlyData[15])
        val v7 = findViewById<ForecastItem>(R.id.forecastItem7)
        v7.setData(body.hourlyData[18])
        val v8 = findViewById<ForecastItem>(R.id.forecastItem8)
        v8.setData(body.hourlyData[21])
    }

    private fun localWeatherUpdate() {
        mService.doInBackground(this, object : SmartHomeService.Callback<List<SensorData>> {
            override fun deserialize(response: String): List<SensorData> {
                return mGson.fromJson(response, object : TypeToken<List<SensorData>>() {}.type)
            }

            override fun isString(): Boolean {
                return false
            }

            override fun onResponse(response: List<SensorData>) {
                mHandler.post { showLocalWeatherResults(response) }
            }

            override fun onFailure(activity: Activity, t: Throwable?, response: String?) {
                mHandler.post {
                    if (t != null) {
                        alert(activity, t.message)
                    } else {
                        alert(activity, response)
                    }
                }
            }
        })
    }

    private fun showLocalWeatherResults(response: List<SensorData>) {
        var N: Double? = null
        var W: Double? = null
        var P: Double? = null
        for (d in response) {
            if (d.locationType == "ext") {
                when (d.locationName) {
                    "Северная сторона дома" -> N = d.getData("temp")
                    "Западная сторона дома" -> W = d.getData("temp")
                }
            }
            if (P == null) {
                P = d.getData("pres")
            }
        }
        if (N != null) {
            val t = findViewById<TextView>(R.id.localTempN)
            t.text = String.format("N: %.1f", N)
        }
        if (W != null) {
            val t = findViewById<TextView>(R.id.localTempW)
            t.text = String.format("W: %.1f", W)
        }
        if (P != null) {
            val p = findViewById<TextView>(R.id.localPres)
            p.text = String.format("P: %d hPa", P.toInt())
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        mDate.set(2020, 1, 1, 0, 0)
        SmartHomeService.setupKeyAndContext(resources.openRawResource(R.raw.key), this)
        try {
            updateSmartHomeServer()
            weatherForecastUpdate()
        } catch (e: Exception) {
            alert(this, e.message)
        }
        val timer = Timer()
        timer.schedule(object : TimerTask() {
            override fun run() {
                mHandler.post { timerTask() }
            }
        }, 0, 1000)
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        // Inflate the menu; this adds items to the action bar if it is present.
        menuInflater.inflate(R.menu.main, menu)
        return true
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        // Handle action bar item clicks here. The action bar will
        // automatically handle clicks on the Home/Up button, so long
        // as you specify a parent activity in AndroidManifest.xml.
        val id = item.itemId

        when (id) {
            R.id.action_settings -> {
                val intent = Intent(this, SettingsActivity::class.java)
                startActivityForResult(intent, SETTINGS)
                return true
            }
        }

        return super.onOptionsItemSelected(item)
    }

    override fun onActivityResult(requestCodeIn: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCodeIn, resultCode, data)
        var requestCode = requestCodeIn
        requestCode = requestCode and 0xFFFF
        if (requestCode == SETTINGS && resultCode == Activity.RESULT_OK) {
            updateSmartHomeServer()
        }
    }

    private fun updateSmartHomeServer() {
        val settings = getSharedPreferences(PREFS_NAME, 0)
        val name = settings.getString("smart_home_server_name", "localhost")!!

        SmartHomeService.setupServer(name, 60001)
        localWeatherUpdate()
    }
}
