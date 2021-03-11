package smart_home.smarthome

import java.util.Date

interface IGraphData {
    val seriesColumnData: String
    val date: Date
    val data: Map<String, Any>
    fun getData(dataName: String): Double?
}
