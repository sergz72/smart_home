package smart_home.smarthome

import com.sz.charts.GraphSeries
import com.sz.charts.LineChart
import smart_home.smarthome.entities.SensorData
import java.text.FieldPosition
import java.text.Format
import java.text.ParsePosition
import java.time.Duration
import java.time.Instant
import java.time.LocalDateTime
import java.time.ZoneId
import java.time.format.DateTimeFormatter
import java.util.*


object Graph {

    private val FORMATTER = DateTimeFormatter.ofPattern("dd.MM HH:mm", Locale.US)
    private val DAY_FORMATTER = DateTimeFormatter.ofPattern("dd.MM", Locale.US)
    private val HOUR_FORMATTER = DateTimeFormatter.ofPattern("HH:mm", Locale.US)

    private val LABEL_FORMATTER = object : Format() {
        override fun format(obj: Any, toAppendTo: StringBuffer, pos: FieldPosition): StringBuffer {
            val time = (obj as Number).toLong()
            val date = LocalDateTime.ofInstant(Instant.ofEpochSecond(time), ZoneId.systemDefault())
            if (date.hour != 0 || date.minute != 0) {
                return toAppendTo.append(FORMATTER.format(date))
            }
            return toAppendTo.append(DAY_FORMATTER.format(date))
        }

        override fun parseObject(source: String, pos: ParsePosition): Any? {
            return null
        }
    }

    private val LABEL_HOUR_FORMATTER = object : Format() {
        override fun format(obj: Any, toAppendTo: StringBuffer, pos: FieldPosition): StringBuffer {
            val time = (obj as Number).toLong()
            val date = LocalDateTime.ofInstant(Instant.ofEpochSecond(time), ZoneId.systemDefault())
            return toAppendTo.append(HOUR_FORMATTER.format(date))
        }

        override fun parseObject(source: String, pos: ParsePosition): Any? {
            return null
        }
    }

    private val LABEL_FLOAT1_FORMATTER = object : Format() {
        override fun format(obj: Any, toAppendTo: StringBuffer, pos: FieldPosition): StringBuffer {
            val v = obj as Double
            return toAppendTo.append("%.1f".format(v))
        }

        override fun parseObject(source: String, pos: ParsePosition): Any? {
            return null
        }
    }

    private val LABEL_FLOAT0_FORMATTER = object : Format() {
        override fun format(obj: Any, toAppendTo: StringBuffer, pos: FieldPosition): StringBuffer {
            val v = obj as Double
            return toAppendTo.append("%.0f".format(v))
        }

        override fun parseObject(source: String, pos: ParsePosition): Any? {
            return null
        }
    }

    private fun buildGraphParameters(d: SensorData, dataName: String, isPrefix: Boolean, isSuffix: Boolean): Map<String, String> {
        if (isPrefix) {
            return d.series.last().data.keys.filter { it.startsWith(dataName) }.associateBy { d.locationName + ":" + it }
        }
        if (isSuffix) {
            if (dataName == "pwr") {
                return mapOf(d.locationName to "Sum$dataName")
            }
            return mapOf(d.locationName + ":Min" to "Min$dataName",
                         d.locationName + ":Avg" to "Avg$dataName",
                         d.locationName + ":Max" to "Max$dataName")
        }
        return mapOf(d.locationName to dataName)
    }

    private fun buildSeries(d: List<SensorData>, dataName: String, isPrefix: Boolean, isSuffix: Boolean): GraphSeries {
        val result = GraphSeries()
        d.filter { it.series.size > 1 && it.series.last().data.containsKey(dataName) }
         .forEach { data -> buildGraphParameters(data, dataName, isPrefix, isSuffix).forEach { (title, dn) ->
                    result.addGraph(data.buildGraphData(title, dn, dataName))
            }
        }

        return result
    }

    fun buildGraph(plot: LineChart, data: SensorData?, dataName: String, isPrefix: Boolean,
                   isSuffix: Boolean, useFloat0: Boolean) {
        if (data == null) {
            plot.clearSeries()
        } else {
            buildGraph(plot, listOf(data), dataName, isPrefix, isSuffix, useFloat0)
        }
    }

    fun buildGraph(plot: LineChart, data: List<SensorData>, dataName: String, isPrefix: Boolean,
                   isSuffix: Boolean, useFloat0: Boolean) {
        if (data.isNotEmpty() && data.maxOf { sd -> sd.series.size } > 1) {
            val start = data.minOf { d -> d.minDate(dataName) }
            val end = data.maxOf { d -> d.maxDate(dataName) }
            val days = Duration.between(start, end).toDays()
            var multiplier = 1
            when {
                days == 2L -> {
                    multiplier = 2
                }
                days in 3..30L -> {
                    multiplier = 12
                }
                days > 30L -> {
                    multiplier = 24 * days.toInt() / 30
                }
            }
            val seriesInfo = buildSeries(data, dataName, isPrefix, isSuffix)
            val dy = seriesInfo.mUpperYBoundary - seriesInfo.mLowerYBoundary
            var rangeStepValue = 0.1F
            when {
                dy > 1.0F && dy <= 2.0F -> {
                    rangeStepValue = 0.2F
                }
                dy > 2.0F && dy <= 5.0F -> {
                    rangeStepValue = 0.5F
                }
                dy > 5.0F && dy <= 10.0F -> {
                    rangeStepValue = 1.0F
                }
                dy > 10.0F && dy <= 20.0F -> {
                    rangeStepValue = 2.0F
                }
                dy > 20.0F && dy <= 50.0F -> {
                    rangeStepValue = 5.0F
                }
                dy > 50.0F && dy <= 100.0F -> {
                    rangeStepValue = 10.0F
                }
                dy > 100.0F && dy <= 200.0F -> {
                    rangeStepValue = 20.0F
                }
                dy > 200.0F && dy <= 500.0F -> {
                    rangeStepValue = 50.0F
                }
                dy > 500.0F && dy <= 1000.0F -> {
                    rangeStepValue = 100.0F
                }
                dy > 1000.0F && dy <= 2000.0F -> {
                    rangeStepValue = 200.0F
                }
                dy > 2000.0F && dy <= 5000.0F -> {
                    rangeStepValue = 500.0F
                }
                dy > 5000.0F -> {
                    rangeStepValue = 1000.0F
                }
            }
            plot.setSeries(seriesInfo,
                60 * 60 * 2 * multiplier,
                if (days < 3L) LABEL_HOUR_FORMATTER else LABEL_FORMATTER,
                rangeStepValue,
                if (useFloat0) LABEL_FLOAT0_FORMATTER else LABEL_FLOAT1_FORMATTER
            )
        } else {
            plot.clearSeries()
        }
    }
}
