package smart_home.smarthome

import android.app.Activity
import android.app.DatePickerDialog
import android.os.Bundle
import android.os.Parcel
import android.os.Parcelable
import android.view.MenuItem
import android.view.View
import android.widget.AdapterView
import android.widget.Button
import android.widget.Spinner
import android.widget.TextView
import androidx.activity.OnBackPressedCallback
import androidx.appcompat.app.AppCompatActivity
import java.time.LocalDate
import java.time.format.DateTimeFormatter

class FiltersActivity : AppCompatActivity(), View.OnClickListener, AdapterView.OnItemSelectedListener {
    companion object {
        val UI_DATE_FORMAT: DateTimeFormatter = DateTimeFormatter.ofPattern("dd MMM yyyy")
    }

    class Data(var mResult: Int, var mDateStart: LocalDate?, var mDateOffset: Int,
        var mDateOffsetUnit: TimeUnit, var mPeriod: Int, var mPeriodUnit: TimeUnit) : Parcelable {

        constructor() : this(RESULT_CANCELED, null, 1, TimeUnit.Day, 0, TimeUnit.Day)

        constructor(parcel: Parcel) : this(
            parcel.readInt(), readDate(parcel), parcel.readInt(),
            TimeUnit.entries[parcel.readInt()], parcel.readInt(), TimeUnit.entries[parcel.readInt()]
        )

        override fun writeToParcel(parcel: Parcel, flags: Int) {
            parcel.writeInt(mResult)
            parcel.writeLong(mDateStart?.toEpochDay() ?: 0L)
            parcel.writeInt(mDateOffset)
            parcel.writeInt(mDateOffsetUnit.ordinal)
            parcel.writeInt(mPeriod)
            parcel.writeInt(mPeriodUnit.ordinal)
        }

        override fun describeContents(): Int {
            return 0
        }

        companion object CREATOR : Parcelable.Creator<Data> {
            override fun createFromParcel(parcel: Parcel): Data {
                return Data(parcel)
            }

            override fun newArray(size: Int): Array<Data?> {
                return arrayOfNulls(size)
            }

            fun readDate(parcel: Parcel): LocalDate? {
                val date = parcel.readLong()
                return if (date == 0L) {
                    null
                } else {
                    LocalDate.ofEpochDay(date)
                }
            }

            fun parsePeriod(date: LocalDate, offset: Int, unit: TimeUnit): LocalDate {
                return when (unit) {
                    TimeUnit.Day -> date.plusDays(offset.toLong())
                    TimeUnit.Month -> date.plusMonths(offset.toLong())
                    TimeUnit.Year -> date.plusYears(offset.toLong())
                }
            }
        }

        private fun getFromDateDate(): LocalDate {
            return mDateStart ?: parsePeriod(LocalDate.now(), -mDateOffset, mDateOffsetUnit)
        }

        fun getFromDate(): Int {
            val date = getFromDateDate()
            return date.year * 10000 + date.month.value * 100 + date.dayOfMonth
        }

        fun getToDate(): Int {
            val date = if (mPeriod == 0) {
                LocalDate.now()
            } else {
                parsePeriod(getFromDateDate(), mPeriod, mPeriodUnit)
            }
            return date.year * 10000 + date.month.value * 100 + date.dayOfMonth
        }

        fun getPeriod(): DateOffset? {
            return if (mPeriod == 0) {
                null
            } else {
                DateOffset(mPeriod, mPeriodUnit)
            }
        }

        fun getDateOffset(): DateOffset? {
            return if (mDateOffset == 0) { null } else { DateOffset(mDateOffset, mDateOffsetUnit) }
        }
    }

    private var mData: Data = Data()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.filters_activity)
        supportActionBar?.setDisplayHomeAsUpEnabled(true)

        val selectDateStart = findViewById<Button>(R.id.select_date_start)
        val set = findViewById<Button>(R.id.set)
        val filterType = findViewById<Spinner>(R.id.filter_type)
        val period = findViewById<Spinner>(R.id.period)

        selectDateStart.setOnClickListener(this)
        set.setOnClickListener(this)
        filterType.onItemSelectedListener = this

        mData = intent.getParcelableExtra("data", Data::class.java)!!

        filterType.setSelection(if (mData.mDateStart != null) {0} else {1})

        val periods = resources.getIntArray(R.array.periodValues)
        var idx = 0
        for (p in periods) {
            if (p == mData.mPeriod) {
                period.setSelection(idx)
                break
            }
            idx++
        }

        onBackPressedDispatcher.addCallback(this, object : OnBackPressedCallback(true) {
            override fun handleOnBackPressed() {
                setResult(mData.mResult, intent)
                finish()
            }
        })

        updateDate()
    }

    private fun updateDate() {
        val dateStart = findViewById<TextView>(R.id.date_start)
        dateStart.text = mData.mDateStart?.format(UI_DATE_FORMAT) ?: ""
    }

    public override fun onResume() {
        super.onResume()
        mData.mResult = RESULT_CANCELED
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        when (item.itemId) {
            android.R.id.home -> {
                setResult(mData.mResult, intent)
                finish()
                return true
            }
        }
        return super.onOptionsItemSelected(item)
    }

    override fun onClick(v: View?) {
        val selectDateStart = findViewById<Button>(R.id.select_date_start)

        when (v) {
            selectDateStart -> {
                val dialog = DatePickerDialog(this, { _, year, monthOfYear, dayOfMonth ->
                    mData.mDateStart = LocalDate.of(year, monthOfYear + 1, dayOfMonth)
                    updateDate()
                }, mData.mDateStart!!.year, mData.mDateStart!!.monthValue - 1, mData.mDateStart!!.dayOfMonth)
                dialog.show()
            }
            else -> { // Generate button
                val filterTypeView = findViewById<Spinner>(R.id.filter_type)
                val filterType =  resources.getStringArray(R.array.filterTypeValues)[filterTypeView.selectedItemPosition]
                //todo
                //mData.mUsePeriod = filterType == "Period"
                val period = findViewById<Spinner>(R.id.period)
                mData.mPeriod = resources.getIntArray(R.array.periodValues)[period.selectedItemPosition]
                intent.putExtra("data", mData)
                setResult(RESULT_OK, intent)
                finish()
            }
        }
    }

    override fun onItemSelected(parent: AdapterView<*>?, view: View?, position: Int, id: Long) {
        val filterTypeView = findViewById<Spinner>(R.id.filter_type)
        val periodLabel = findViewById<TextView>(R.id.period_label)
        val period = findViewById<Spinner>(R.id.period)
        val selectDateStart = findViewById<Button>(R.id.select_date_start)
        val dateStart = findViewById<TextView>(R.id.date_start)
        val filterType =  resources.getStringArray(R.array.filterTypeValues)[filterTypeView.selectedItemPosition]
        if (filterType == "Period") {
            periodLabel.isEnabled = true
            period.isEnabled = true
            dateStart.isEnabled = false
            selectDateStart.isEnabled = false
        } else {
            periodLabel.isEnabled = false
            period.isEnabled = false
            dateStart.isEnabled = true
            selectDateStart.isEnabled = true
        }
    }

    override fun onNothingSelected(parent: AdapterView<*>?) {
        val filterType = findViewById<Spinner>(R.id.filter_type)
        filterType.setSelection(0)
    }
}