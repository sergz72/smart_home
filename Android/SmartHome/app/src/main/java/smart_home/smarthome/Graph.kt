package smart_home.smarthome

import android.graphics.Color

import com.androidplot.ui.DynamicTableModel
import com.androidplot.ui.TableOrder
import com.androidplot.xy.BoundaryMode
import com.androidplot.xy.LineAndPointFormatter
import com.androidplot.xy.OrderedXYSeries
import com.androidplot.xy.StepMode
import com.androidplot.xy.XYGraphWidget
import com.androidplot.xy.XYPlot
import com.androidplot.xy.XYSeries

import java.text.FieldPosition
import java.text.Format
import java.text.ParsePosition
import java.text.SimpleDateFormat

import smart_home.smarthome.entities.SensorData
import java.util.*

object Graph {

    private val FORMATTERS = arrayOf(LineAndPointFormatter(Color.RED, Color.GREEN, Color.TRANSPARENT, null),
                                     LineAndPointFormatter(Color.YELLOW, Color.BLUE, Color.TRANSPARENT, null),
                                     LineAndPointFormatter(Color.CYAN, Color.MAGENTA, Color.TRANSPARENT, null),
                                     LineAndPointFormatter(Color.BLACK, Color.valueOf(128f, 0f, 128f).toArgb(), Color.TRANSPARENT, null),
                                     LineAndPointFormatter(Color.GREEN, Color.CYAN, Color.TRANSPARENT, null),
                                     LineAndPointFormatter(Color.BLUE, Color.YELLOW, Color.TRANSPARENT, null))

    private val FORMATTER = SimpleDateFormat("HH:mm", Locale.US)

    private val LABEL_FORMATTER = object : Format() {
        override fun format(obj: Any, toAppendTo: StringBuffer, pos: FieldPosition): StringBuffer {
            val time = (obj as Number).toLong()
            val date = Date(time)
            return toAppendTo.append(FORMATTER.format(date))
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

    private class GraphDataSeries internal constructor(private val mData: List<IGraphData>, private val mDataName: String) : OrderedXYSeries {

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

    private fun convertData(data: List<IGraphData>, dataName: String): List<IGraphData> {
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

    private fun buildSeries(d: List<IGraphData>, dataName: String, isPrefix: Boolean): SeriesInfo {
        var data = d
        if (isPrefix) {
            data = convertData(data, dataName)
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

    @JvmOverloads
    fun buildGraph(plot: XYPlot, data: List<IGraphData>, dataName: String, isPrefix: Boolean = false) {
        plot.clear()
        if (data.size > 1) {
            val comparator = DateComparator()
            Collections.sort(data, comparator)
            val seriesInfo = buildSeries(data, dataName, isPrefix)
            if (!seriesInfo.mSeries.isEmpty()) {
                var i = 0
                for (seriesItem in seriesInfo.mSeries) {
                    plot.addSeries(seriesItem, FORMATTERS[i++])
                }
                plot.setRangeBoundaries(seriesInfo.mLowerBoundary, seriesInfo.mUpperBoundary, BoundaryMode.FIXED)
                plot.domainStepMode = StepMode.INCREMENT_BY_VAL
                plot.domainStepValue = (60 * 60 * 2000).toDouble() // 2 hours
                plot.graph.getLineLabelStyle(XYGraphWidget.Edge.BOTTOM).format = LABEL_FORMATTER
                plot.legend.setTableModel(DynamicTableModel(2, 2, TableOrder.ROW_MAJOR))
            }
        }
        plot.redraw()
    }
}
