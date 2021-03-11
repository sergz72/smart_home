package smart_home.smarthome

import android.graphics.Color
import com.androidplot.ui.DynamicTableModel
import com.androidplot.ui.TableOrder
import com.androidplot.xy.*
import smart_home.smarthome.entities.SensorData
import java.text.FieldPosition
import java.text.Format
import java.text.ParsePosition
import java.text.SimpleDateFormat
import java.time.Duration
import java.util.*


object Graph {

    private val FORMATTERS = arrayOf(LineAndPointFormatter(Color.RED, Color.GREEN, Color.TRANSPARENT, null),
            LineAndPointFormatter(Color.YELLOW, Color.BLUE, Color.TRANSPARENT, null),
            LineAndPointFormatter(Color.CYAN, Color.MAGENTA, Color.TRANSPARENT, null),
            LineAndPointFormatter(Color.BLACK, Color.valueOf(128f, 0f, 128f).toArgb(), Color.TRANSPARENT, null),
            LineAndPointFormatter(Color.GREEN, Color.CYAN, Color.TRANSPARENT, null),
            LineAndPointFormatter(Color.BLUE, Color.YELLOW, Color.TRANSPARENT, null))

    private val FORMATTER = SimpleDateFormat("dd.MM HH:mm", Locale.US)
    private val DAY_FORMATTER = SimpleDateFormat("dd.MM", Locale.US)
    private val HOUR_FORMATTER = SimpleDateFormat("HH:mm", Locale.US)

    private val LABEL_FORMATTER = object : Format() {
        override fun format(obj: Any, toAppendTo: StringBuffer, pos: FieldPosition): StringBuffer {
            val time = (obj as Number).toLong()
            val date = Date(time)
            if (date.hours != 0 || date.minutes != 0) {
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
            val date = Date(time)
            return toAppendTo.append(HOUR_FORMATTER.format(date))
        }

        override fun parseObject(source: String, pos: ParsePosition): Any? {
            return null
        }
    }

    private class DateComparator : Comparator<IGraphData> {
        override fun compare(o1: IGraphData, o2: IGraphData): Int {
            return o1.date.compareTo(o2.date)
        }
    }

    private class GraphDataSeries constructor(private val mData: List<IGraphData>, private val mDataName: String) : OrderedXYSeries {

        override fun size(): Int {
            return mData.size
        }

        override fun getX(index: Int): Number {
            return mData[index].date.time
        }

        override fun getY(index: Int): Number? {
            return mData[index].getData(mDataName)
        }

        override fun getTitle(): String {
            return mData[0].seriesColumnData
        }

        override fun getXOrder(): OrderedXYSeries.XOrder {
            return OrderedXYSeries.XOrder.ASCENDING
        }
    }

    private data class SeriesInfo(val mSeries: List<XYSeries>, val mLowerBoundary: Double, val mUpperBoundary: Double)

    private fun convertPrefixData(data: List<IGraphData>, dataName: String): List<IGraphData> {
        val result = ArrayList<IGraphData>()

        for (item in data) {
            for ((key, value) in item.data) {
                if (key.startsWith(dataName)) {
                    val location = item.seriesColumnData + ":" + key
                    result.add(SensorData.build(location, item.date, dataName, value))
                }
            }
        }

        return result
    }

    private fun convertSuffixData(data: List<IGraphData>, dataName: String): List<IGraphData> {
        val result = ArrayList<IGraphData>()

        for (item in data) {
            for ((key, _) in item.data) {
                if (key.endsWith(dataName)) {
                    var location = item.seriesColumnData + ":Max"
                    var v = item.getData("Max$key")
                    if (v != null) {
                        result.add(SensorData.build(location, item.date, dataName, v))
                    }
                    location = item.seriesColumnData + ":Min"
                    v = item.getData("Min$key")
                    if (v != null) {
                        result.add(SensorData.build(location, item.date, dataName, v))
                    }
                    location = item.seriesColumnData + ":Avg"
                    v = item.getData("Avg$key")
                    if (v != null) {
                        result.add(SensorData.build(location, item.date, dataName, v))
                    }
                }
            }
        }

        return result
    }

    private fun buildSeries(d: List<IGraphData>, dataName: String, isPrefix: Boolean, isSuffix: Boolean): SeriesInfo {
        var data = d
        if (isPrefix) {
            data = convertPrefixData(data, dataName)
        }
        if (isSuffix) {
            data = convertSuffixData(data, dataName)
        }
        val series = HashMap<String, MutableList<IGraphData>>()
        var lowerBoundary = java.lang.Double.MAX_VALUE
        var upperBoundary = java.lang.Double.MIN_VALUE

        for (dataItem in data) {
            var values: MutableList<IGraphData>? = series[dataItem.seriesColumnData]
            if (values == null) {
                values = ArrayList()
                series[dataItem.seriesColumnData] = values
            }

            val dataValue = dataItem.getData(dataName)
            if (dataValue != null) {
                values.add(dataItem)
                if (dataValue < lowerBoundary) {
                    lowerBoundary = dataValue
                }
                if (dataValue > upperBoundary) {
                    upperBoundary = dataValue
                }
            }
        }

        val result = ArrayList<XYSeries>()

        for ((_, value) in series) {
            if (value.size > 1) {
                result.add(GraphDataSeries(value, dataName))
            }
        }

        return SeriesInfo(result, lowerBoundary, upperBoundary)
    }

    fun buildGraph(plot: XYPlot, data: List<IGraphData>, dataName: String, isPrefix: Boolean, isSuffix: Boolean,
                   columns: Int, rows: Int) {
        plot.clear()
        if (data.size > 1) {
            val comparator = DateComparator()
            Collections.sort(data, comparator)
            val start = data[0].date.toInstant()
            val end = data[data.size - 1].date.toInstant()
            val days = Duration.between(start, end).toDays()
            var multiplier = 1
            if (days == 2L) {
                multiplier = 2
            } else if (days in 3..30L) {
                multiplier = 12
            } else if (days > 30L) {
                multiplier = 24 * days.toInt() / 30
            }
            val seriesInfo = buildSeries(data, dataName, isPrefix, isSuffix)
            if (seriesInfo.mSeries.isNotEmpty()) {
                var i = 0
                for (seriesItem in seriesInfo.mSeries) {
                    plot.addSeries(seriesItem, FORMATTERS[i++])
                }
                plot.setRangeBoundaries(seriesInfo.mLowerBoundary, seriesInfo.mUpperBoundary, BoundaryMode.FIXED)
                plot.domainStepMode = StepMode.INCREMENT_BY_VAL
                plot.domainStepValue = (60 * 60 * 2000 * multiplier).toDouble()
                plot.graph.getLineLabelStyle(XYGraphWidget.Edge.BOTTOM).format = if (days < 3L) LABEL_HOUR_FORMATTER else LABEL_FORMATTER
                plot.legend.setTableModel(DynamicTableModel(columns, rows, TableOrder.ROW_MAJOR))
            }
        }
        plot.redraw()
    }
}
