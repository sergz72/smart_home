package com.sz.smart_home.query

import com.sz.smart_home.common.NetworkService
import java.nio.file.Files
import java.nio.file.Paths

fun usage() {
    println("Usage: java -jar smart_home_query.jar keyFileName hostName port query")
}

fun buildKeyValue(parameter: String): Pair<String, String> {
    val parts = parameter.split("=")
    if (parts.size != 2) {
        throw IllegalArgumentException("Invalid parameter $parameter")
    }
    return Pair(parts[0], parts[1])
}

fun parseDate(dateStr: String): Pair<Int?, DateOffset?> {
    if (dateStr.startsWith("-")) {
        val period = parsePeriod(dateStr.substring(1))
        return Pair(null, period)
    }
    val date = dateStr.toInt()
    return Pair(date, null)
}

fun parsePeriod(periodStr: String?): DateOffset? {
    if (periodStr == null) {
        return null
    }
    val unit: TimeUnit = when (periodStr.last()) {
        'd' -> TimeUnit.Day
        'm' -> TimeUnit.Month
        'y' -> TimeUnit.Year
        else -> throw IllegalArgumentException("Invalid period $periodStr")
    }
    val value = periodStr.substring(0, periodStr.length - 1).toInt()
    return DateOffset(value, unit)
}

fun buildRequest(query: String): SmartHomeQuery {
    val parameters = query
        .split('&')
        .associate { buildKeyValue(it) }
    val dataType = parameters.getValue("dataType")
    val maxPoints = parameters.getOrDefault("maxPoints", "0").toShort()
    println("maxPoints: $maxPoints")
    println("dataType: $dataType")
    val (startDate, startDateOffset) = parseDate(parameters.getValue("startDate"))
    if (startDate != null) {
        println("startDate: $startDate")
    } else {
        println("startDateOffset.offset: ${startDateOffset!!.offset}")
        println("startDateOffset.unit: ${startDateOffset.unit}")
    }
    val period = parsePeriod(parameters["period"])
    if (period != null) {
        println("period.offset: ${period.offset}")
        println("period.unit: ${period.unit}")
    }
    return SmartHomeQuery(maxPoints, dataType, startDate, startDateOffset, period)
}

fun printResponse(response: SensorDataResponse) {
    println("Response: aggregated: ${response.aggregated}, sensor count: ${response.data.size}")
    for ((k, v) in response.data) {
        println("Sensor $k: ${v.size} records.")
    }
}

fun printResponse(response: List<Sensor>) {
    println("Response: sensor count: ${response.size}")
    for (v in response) {
        println("Sensor ${v.id}: ${v.dataType} ${v.location} ${v.locationType}")
    }
}

fun printResponse(response: Map<Int, LastSensorData>) {
    println("Response:")
    for ((k, v) in response) {
        println("Sensor $k: ${v.date} ${v.value.time} ${v.value.values}")
    }
}

fun main(args: Array<String>) {
    if (args.size != 4) {
        usage()
        return
    }
    val keyBytes = Files.readAllBytes(Paths.get(args[0]))
    val hostName = args[1]
    val port = args[2].toInt()
    val query = args[3]

    val service = SmartHomeService(keyBytes, hostName, port)

    if (query == "sensors") {
        service.getSensors(object: NetworkService.Callback<List<Sensor>> {
            override fun onResponse(response: List<Sensor>) {
                printResponse(response)
            }

            override fun onFailure(t: Throwable) {
                println(t.message)
            }
        })
    } else if (query.startsWith("last_data_from=")) {
        service.getLastSensorData(query.split("=")[1].toByte(), object: NetworkService.Callback<Map<Int, LastSensorData>> {
            override fun onResponse(response: Map<Int, LastSensorData>) {
                printResponse(response)
            }

            override fun onFailure(t: Throwable) {
                println(t.message)
            }
        })
    } else {
        val request = buildRequest(query)
        service.send(request, object: NetworkService.Callback<SensorDataResponse> {
            override fun onResponse(response: SensorDataResponse) {
                printResponse(response)
            }

            override fun onFailure(t: Throwable) {
                println(t.message)
            }
        })
    }
}
