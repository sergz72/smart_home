package com.sz.smart_home.query

import java.nio.file.Files
import java.nio.file.Paths
import javax.crypto.spec.SecretKeySpec

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

fun parseDate(dateStr: String): Pair<DateTime?, DateOffset?> {
    if (dateStr.startsWith("-")) {
        val period = parsePeriod(dateStr.substring(1))
        return Pair(null, period)
    }
    val datetime = dateStr.toLong()
    val date = datetime / 1000000
    val time = datetime % 1000000
    return Pair(DateTime(date.toInt(), time.toInt()), null)
}

fun parsePeriod(periodStr: String?): DateOffset? {
    if (periodStr == null) {
        return null
    }
    val unit: TimeUnit = when (periodStr.last()) {
        'h' -> TimeUnit.hour
        'd' -> TimeUnit.day
        'm' -> TimeUnit.month
        'y' -> TimeUnit.year
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
        println("startDate.date: ${startDate.date}")
        println("startDate.time: ${startDate.time}")
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

fun main(args: Array<String>) {
    if (args.size != 4) {
        usage()
        return
    }
    val keyBytes = Files.readAllBytes(Paths.get(args[0]))
    val hostName = args[1]
    val port = args[2].toInt()
    val query = args[3]

    val request = buildRequest(query)
    val service = SmartHomeService(keyBytes, hostName, port)
    val response = service.send(request)
    printResponse(response)
}
