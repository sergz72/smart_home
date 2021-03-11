package smart_home.smarthome

import android.app.Activity
import android.app.DatePickerDialog
import android.content.Intent
import android.os.Bundle
import android.os.Parcel
import android.os.Parcelable
import android.view.MenuItem
import android.view.View
import android.widget.AdapterView
import android.widget.Button
import android.widget.Spinner
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import java.time.LocalDate
import java.time.format.DateTimeFormatter

class FiltersActivity() : AppCompatActivity(), View.OnClickListener, AdapterView.OnItemSelectedListener {
    companion object {
        val UI_DATE_FORMAT: DateTimeFormatter = DateTimeFormatter.ofPattern("dd MMM yyyy")
    }

    class Data() : Parcelable {
        var mResult: Int = Activity.RESULT_CANCELED
        var mDateStart: LocalDate = LocalDate.now().minusMonths(5).withDayOfMonth(1)
        var mDateEnd: LocalDate = LocalDate.now()
        var mPeriod: Int = 24
        var mUsePeriod: Boolean = true

        constructor(parcel: Parcel) : this() {
            mResult = parcel.readInt()
            mDateStart = LocalDate.ofEpochDay(parcel.readLong())
            mDateEnd = LocalDate.ofEpochDay(parcel.readLong())
            mPeriod = parcel.readInt()
            mUsePeriod = parcel.readBoolean()
        }

        override fun writeToParcel(parcel: Parcel, flags: Int) {
            parcel.writeInt(mResult)
            parcel.writeLong(mDateStart.toEpochDay())
            parcel.writeLong(mDateEnd.toEpochDay())
            parcel.writeInt(mPeriod)
            parcel.writeBoolean(mUsePeriod)
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
        }

        fun getFromDate(): Int {
            return mDateStart.year * 10000 + mDateStart.month.value * 100 + mDateStart.dayOfMonth
        }

        fun getToDate(): Int {
            return mDateEnd.year * 10000 + mDateEnd.month.value * 100 + mDateEnd.dayOfMonth
        }
    }

    private var mData: Data = Data()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.filters_activity)
        supportActionBar?.setDisplayHomeAsUpEnabled(true)

        val select_date_start = findViewById<Button>(R.id.select_date_start)
        val select_date_end = findViewById<Button>(R.id.select_date_end)
        val set = findViewById<Button>(R.id.set)
        val filter_type = findViewById<Spinner>(R.id.filter_type)
        val period = findViewById<Spinner>(R.id.period)

        select_date_start.setOnClickListener(this)
        select_date_end.setOnClickListener(this)
        set.setOnClickListener(this)
        filter_type.onItemSelectedListener = this

        mData = intent.getParcelableExtra("data")!!

        filter_type.setSelection(if (mData.mUsePeriod) {0} else {1})

        val periods = resources.getIntArray(R.array.periodValues)
        var idx = 0
        for (p in periods) {
            if (p == mData.mPeriod) {
                period.setSelection(idx)
                break
            }
            idx++
        }

        updateDate()
    }

    private fun updateDate() {
        val date_start = findViewById<TextView>(R.id.date_start)
        val date_end = findViewById<TextView>(R.id.date_end)
        date_start.text = mData.mDateStart.format(UI_DATE_FORMAT)
        date_end.text = mData.mDateEnd.format(UI_DATE_FORMAT)
    }

    public override fun onResume() {
        super.onResume()
        mData.mResult = Activity.RESULT_CANCELED
    }

    override fun onBackPressed() {
        setResult(mData.mResult)
        finish()
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        when (item.itemId) {
            android.R.id.home -> {
                onBackPressed()
                return true
            }
        }
        return super.onOptionsItemSelected(item)
    }

    override fun onClick(v: View?) {
        val select_date_start = findViewById<Button>(R.id.select_date_start)
        val select_date_end = findViewById<Button>(R.id.select_date_end)

        if (v == select_date_start) {
            val dialog = DatePickerDialog(this, { _, year, monthOfYear, dayOfMonth ->
                mData.mDateStart = LocalDate.of(year, monthOfYear + 1, dayOfMonth)
                if (mData.mDateEnd.isBefore(mData.mDateStart)) {
                    mData.mDateEnd = mData.mDateStart
                }
                updateDate()
            }, mData.mDateStart.year, mData.mDateStart.monthValue - 1, mData.mDateStart.dayOfMonth)
            dialog.show()
        } else if (v == select_date_end) {
            val dialog = DatePickerDialog(this, { _, year, monthOfYear, dayOfMonth ->
                mData.mDateEnd = LocalDate.of(year, monthOfYear + 1, dayOfMonth)
                if (mData.mDateStart.isAfter(mData.mDateEnd)) {
                    mData.mDateStart = mData.mDateEnd
                }
                updateDate()
            }, mData.mDateStart.year, mData.mDateStart.monthValue - 1, mData.mDateStart.dayOfMonth)
            dialog.show()
        } else { // Generate button
            val filter_type = findViewById<Spinner>(R.id.filter_type)
            val filterType =  resources.getStringArray(R.array.filterTypeValues)[filter_type.selectedItemPosition]
            mData.mUsePeriod = filterType == "Period"
            val period = findViewById<Spinner>(R.id.period)
            mData.mPeriod = resources.getIntArray(R.array.periodValues)[period.selectedItemPosition]
            val data: Intent = Intent()
            data.putExtra("data", mData)
            setResult(Activity.RESULT_OK, data)
            finish()
        }
    }

    override fun onItemSelected(parent: AdapterView<*>?, view: View?, position: Int, id: Long) {
        val filter_type = findViewById<Spinner>(R.id.filter_type)
        val periodLabel = findViewById<TextView>(R.id.period_label)
        val period = findViewById<Spinner>(R.id.period)
        val select_date_start = findViewById<Button>(R.id.select_date_start)
        val select_date_end = findViewById<Button>(R.id.select_date_end)
        val date_start = findViewById<TextView>(R.id.date_start)
        val date_end = findViewById<TextView>(R.id.date_end)
        val filterType =  resources.getStringArray(R.array.filterTypeValues)[filter_type.selectedItemPosition]
        if (filterType == "Period") {
            periodLabel.isEnabled = true
            period.isEnabled = true
            date_start.isEnabled = false
            select_date_start.isEnabled = false
            date_end.isEnabled = false
            select_date_end.isEnabled = false
        } else {
            periodLabel.isEnabled = false
            period.isEnabled = false
            date_start.isEnabled = true
            select_date_start.isEnabled = true
            date_end.isEnabled = true
            select_date_end.isEnabled = true
        }
    }

    override fun onNothingSelected(parent: AdapterView<*>?) {
        val filter_type = findViewById<Spinner>(R.id.filter_type)
        filter_type.setSelection(0)
    }
}