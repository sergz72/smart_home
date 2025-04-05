package smart_home.smarthome

import android.app.DatePickerDialog
import android.os.Bundle
import android.os.Parcel
import android.os.Parcelable
import android.view.MenuItem
import android.view.View
import android.widget.AdapterView
import android.widget.Button
import android.widget.CheckBox
import android.widget.NumberPicker
import android.widget.Spinner
import android.widget.TextView
import androidx.activity.OnBackPressedCallback
import androidx.appcompat.app.AppCompatActivity
import java.time.LocalDate
import java.time.format.DateTimeFormatter

class FiltersActivity : AppCompatActivity(), View.OnClickListener, AdapterView.OnItemSelectedListener {
    companion object {
        val UI_DATE_FORMAT: DateTimeFormatter = DateTimeFormatter.ofPattern("dd MMM yyyy")

        fun findUnitValue(unit: Int): TimeUnit {
            return TimeUnit.entries.first { it.ordinal == unit }
        }
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

        val offset = findViewById<NumberPicker>(R.id.offset)
        offset.minValue = 1
        offset.maxValue = 30
        val offsetUnit = findViewById<Spinner>(R.id.offset_unit)

        val periodBox = findViewById<CheckBox>(R.id.period_box)
        val period = findViewById<NumberPicker>(R.id.period)
        period.minValue = 1
        period.maxValue = 30
        val periodUnit = findViewById<Spinner>(R.id.period_unit)

        selectDateStart.setOnClickListener(this)
        set.setOnClickListener(this)
        filterType.onItemSelectedListener = this
        periodBox.setOnClickListener(this)

        mData = intent.getParcelableExtra("data", Data::class.java)!!

        filterType.setSelection(if (mData.mDateStart != null) {0} else {1})

        offset.value = mData.mDateOffset
        setUnit(offsetUnit, mData.mDateOffsetUnit)

        if (mData.mPeriod != 0) {
            periodBox.isChecked = true
            period.value = mData.mPeriod
            setUnit(periodUnit, mData.mPeriodUnit)
        } else {
            period.value = 1
            periodUnit.setSelection(0)
        }

        onBackPressedDispatcher.addCallback(this, object : OnBackPressedCallback(true) {
            override fun handleOnBackPressed() {
                setResult(mData.mResult, intent)
                finish()
            }
        })

        updateDate()
    }

    private fun setUnit(unit: Spinner, value: TimeUnit) {
        val units = resources.getIntArray(R.array.periodUnitValues)
        var idx = 0
        for (u in units) {
            if (u == value.ordinal) {
                unit.setSelection(idx)
                break
            }
            idx++
        }
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
        val periodBox = findViewById<CheckBox>(R.id.period_box)

        when (v) {
            selectDateStart -> {
                val now = LocalDate.now()
                val dialog = DatePickerDialog(this, { _, year, monthOfYear, dayOfMonth ->
                    mData.mDateStart = LocalDate.of(year, monthOfYear + 1, dayOfMonth)
                    updateDate()
                }, mData.mDateStart?.year ?: now.year,
                    (mData.mDateStart?.monthValue ?: now.monthValue) - 1,
                    mData.mDateStart?.dayOfMonth ?: now.dayOfMonth)
                dialog.show()
            }
            periodBox -> {
                val checked = periodBox.isChecked
                val period = findViewById<NumberPicker>(R.id.period)
                val periodUnit = findViewById<Spinner>(R.id.period_unit)
                val periodLabel = findViewById<TextView>(R.id.period_label)
                period.isEnabled = checked
                periodUnit.isEnabled = checked
                periodLabel.isEnabled = checked
            }
            else -> { // Set button
                updateData()
                intent.putExtra("data", mData)
                setResult(RESULT_OK, intent)
                finish()
            }
        }
    }

    private fun updateData() {
        val periodBoxView = findViewById<CheckBox>(R.id.period_box)
        val checked = periodBoxView.isChecked

        val offsetUnit = findViewById<Spinner>(R.id.offset_unit)
        val offsetUnitValue = findUnitValue(resources.getIntArray(R.array.periodUnitValues)[offsetUnit.selectedItemPosition])
        mData.mDateOffsetUnit = offsetUnitValue
        val offset = findViewById<NumberPicker>(R.id.offset)
        mData.mDateOffset = offset.value

        if (checked) {
            val periodUnit = findViewById<Spinner>(R.id.period_unit)
            val periodUnitValue =
                findUnitValue(resources.getIntArray(R.array.periodUnitValues)[periodUnit.selectedItemPosition])
            mData.mPeriodUnit = periodUnitValue
            val period = findViewById<NumberPicker>(R.id.period)
            mData.mPeriod = period.value
        } else {
            mData.mPeriod = 0
        }
    }

    override fun onItemSelected(parent: AdapterView<*>?, view: View?, position: Int, id: Long) {
        val filterTypeView = findViewById<Spinner>(R.id.filter_type)
        val offsetLabel = findViewById<TextView>(R.id.offset_label)
        val offset = findViewById<NumberPicker>(R.id.offset)
        val offsetUnit = findViewById<Spinner>(R.id.offset_unit)
        val selectDateStart = findViewById<Button>(R.id.select_date_start)
        val dateStart = findViewById<TextView>(R.id.date_start)
        val filterType =  resources.getStringArray(R.array.filterTypeValues)[filterTypeView.selectedItemPosition]
        val offsetType = filterType == "Offset"
        offsetLabel.isEnabled = offsetType
        offset.isEnabled = offsetType
        offsetUnit.isEnabled = offsetType
        dateStart.isEnabled = !offsetType
        selectDateStart.isEnabled = !offsetType
        if (offsetType)
            mData.mDateStart = null
        else
            mData.mDateStart = LocalDate.now().minusDays(1)
        updateDate()
    }

    override fun onNothingSelected(parent: AdapterView<*>?) {
        val filterType = findViewById<Spinner>(R.id.filter_type)
        filterType.setSelection(0)
    }
}