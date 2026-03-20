using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text.Json;
using NRedisStack;
using NRedisStack.DataTypes;
using NRedisStack.Literals.Enums;
using NRedisStack.RedisStackCommands;
using StackExchange.Redis;

namespace SmartHomeUI;

internal record SensorDataItem(DateTime DateTime, double Value);
internal record AggregatedSensorData(List<SensorDataItem> Min, List<SensorDataItem> Avg, List<SensorDataItem> Max);
internal record SensorData(int SensorId, List<SensorDataItem>? Raw, AggregatedSensorData? Aggregated);

internal record LastData(DateTime Dt, double Value)
{
    public override string ToString()
    {
        return $"{Value:0.00} ({Dt:HH:mm:ss})";
    }
}

public enum TimeUnit {
    Day,
    Month,
    Year
}

public record DateOffset(int Offset, TimeUnit Unit)
{
    internal DateTime CalculateDateSubtract(DateTime dateTime)
    {
        return Unit switch
        {
            TimeUnit.Day => dateTime.AddDays(-Offset),
            TimeUnit.Month => dateTime.AddMonths(-Offset),
            TimeUnit.Year => dateTime.AddYears(-Offset),
            _ => throw new ArgumentOutOfRangeException()
        };
    }
    
    internal DateTime CalculateDateAdd(DateTime dateTime)
    {
        return Unit switch
        {
            TimeUnit.Day => dateTime.AddDays(Offset),
            TimeUnit.Month => dateTime.AddMonths(Offset),
            TimeUnit.Year => dateTime.AddYears(Offset),
            _ => throw new ArgumentOutOfRangeException()
        };
    }
}

public record SmartHomeQuery(
    short MaxPoints,
    string DataType,
    DateTime? StartDate,
    DateOffset? StartDateOffset,
    DateOffset? Period);

internal record Location(int Id, string Name, string LocationType);
internal record Sensor(int Id, string Name, string DataType, int LocationId, int? DeviceId,
    Dictionary<int, string> DeviceSensors, Dictionary<string, double> Offsets, bool Enabled);
internal record Configuration(string RedisConnectionString, string SensorsFile, string LocationsFile, string TimeZone);

public sealed class SmartHomeService
{
    private record DateRange(bool Aggregated, long From, long To, long BucketDuration);
    
    private const long MillisecondsInHour = 60 * 60 * 1000;
    private const long MinimumRawBucketDuration = 5 * 60 * 1000; // 5 minutes
    private const long MinimumAggregatedBucketDuration = 24 * 60 * 60 * 1000; // 1 Day
    
    private static Dictionary<string, string> _valueTypeMap = new Dictionary<string, string>
    {
        {"humi", "Humidity"},
        {"temp", "Temperature"},
        {"pres", "Pressure"},
        {"co2 ", "CO2"},
        {"lux ", "Luminosity"},
        {"pwr ", "Power"},
        {"icc ", "Current"},
        {"vcc ", "Voltage"}
    };
    
    private readonly ConnectionMultiplexer _redisConnection;
    private readonly Dictionary<int, Sensor> _sensors;
    private readonly Dictionary<int, Location> _locations;
    private readonly Dictionary<string, HashSet<int>> _sensorValueTypeMap;
    private readonly TimeZoneInfo _timeZone;
    
    public double ResponseTimeMs { get; private set; }
    
    public SmartHomeService(string configFileName)
    {
        using var configurationStream = File.OpenRead(configFileName);
        var configuration = JsonSerializer.Deserialize<Configuration>(configurationStream)
                            ?? throw new Exception("Invalid configuration file");
        _timeZone = TimeZoneInfo.FindSystemTimeZoneById(configuration.TimeZone);
        using var sensorsStream = File.OpenRead(configuration.SensorsFile);
        _sensors = JsonSerializer.Deserialize<Dictionary<int, Sensor>>(sensorsStream)
                   ?? throw new Exception("Invalid sensors file");
        using var locationsStream = File.OpenRead(configuration.LocationsFile);
        _locations = JsonSerializer.Deserialize<Dictionary<int, Location>>(locationsStream)
                     ?? throw new Exception("Invalid locations file");
        _redisConnection = ConnectionMultiplexer.Connect(configuration.RedisConnectionString);
        
        var db = _redisConnection.GetDatabase();
        var ts = db.TS();
        _sensorValueTypeMap = new Dictionary<string, HashSet<int>>();
        foreach (var seriesName in ts.QueryIndex(["type=sensor"]))
        {
            var parts = seriesName.Split(':');
            var sensorId = int.Parse(parts[0]);
            var valueType = parts[1];
            if (_sensorValueTypeMap.TryGetValue(valueType, out var sensorIds))
                sensorIds.Add(sensorId);
            else
                _sensorValueTypeMap[valueType] = [sensorId];
        }
    }

    internal string GetLocationName(int sensorId)
    {
        var locationId = _sensors[sensorId].LocationId;
        return _locations[locationId].Name;
    }

    private DateTime GetDateTime(TimeStamp timestamp)
    {
        return TimeZoneInfo
            .ConvertTimeFromUtc(DateTimeOffset.FromUnixTimeMilliseconds((long)timestamp.Value).DateTime, _timeZone);
    }
    
    internal Dictionary<string, Dictionary<string, LastData>> GetLastSensorData()
    {
        var db = _redisConnection.GetDatabase();
        var ts = db.TS();
        var result = new Dictionary<string, Dictionary<string, LastData>>();
        var minimumDateTime = DateTime.Now.AddDays(-1);
        var stopwatch = new Stopwatch();
        stopwatch.Start();
        foreach (var v in ts.MGet(["type=sensor"], true))
        {
            var dateTime = GetDateTime(v.value.Time);
            if (dateTime < minimumDateTime)
                continue;
            var parts = v.key.Split(':');
            var sensorId = int.Parse(parts[0]);
            var locationName = GetLocationName(sensorId);
            var valueType = _valueTypeMap[parts[1]];
            var value = v.value.Val;
            if (result.TryGetValue(locationName, out var lastData))
                lastData[valueType] = new LastData(dateTime, value);
            else
                result[locationName] = new Dictionary<string, LastData> {{valueType, new LastData(dateTime, value)}};
        }
        stopwatch.Stop();
        ResponseTimeMs = stopwatch.Elapsed.TotalMilliseconds;
        return result;
    }

    // map valueType -> map sensorId to sensor data
    internal Dictionary<string, List<SensorData>> GetSensorData(SmartHomeQuery sensorDataQuery)
    {
        var sensors = GetSensors(sensorDataQuery.DataType);
        if (sensors.Count == 0)
            return [];
        var dateRange = BuildDateRange(sensorDataQuery);
        return dateRange.Aggregated ? GetAggregatedSensorData(sensors, dateRange) : GetRawSensorData(sensors, dateRange); 
    }

    private Dictionary<string, List<SensorData>> GetAggregatedSensorData(HashSet<int> sensors, DateRange dateRange)
    {
        var db = _redisConnection.GetDatabase();
        var ts = db.TS();
        var result = new Dictionary<string, List<SensorData>>();
        var skipAggregation = dateRange.BucketDuration <= MinimumAggregatedBucketDuration;
        TsAggregation? minAggregation = skipAggregation ? null : TsAggregation.Min;
        TsAggregation? avgAggregation = skipAggregation ? null : TsAggregation.Avg;
        TsAggregation? maxAggregation = skipAggregation ? null : TsAggregation.Max;
        long ?bucketDuration = skipAggregation ? null : dateRange.BucketDuration;
        var stopwatch = new Stopwatch();
        stopwatch.Start();
        foreach (var kv in _sensorValueTypeMap)
        {
            var sensorsToProcess = sensors.Intersect(kv.Value).ToList();
            if (sensorsToProcess.Count == 0)
                continue;
            var sensorDataList = new List<SensorData>();
            foreach (var sensorId in sensorsToProcess)
            {
                var minList = BuildSensorDataMap(ts, $"{sensorId}:{kv.Key}:min", dateRange, minAggregation,
                    bucketDuration);
                var avgList = BuildSensorDataMap(ts, $"{sensorId}:{kv.Key}:avg", dateRange, avgAggregation,
                    bucketDuration);
                var maxList = BuildSensorDataMap(ts, $"{sensorId}:{kv.Key}:max", dateRange, maxAggregation,
                    bucketDuration);
                if (minList.Count != 0 || avgList.Count != 0 || maxList.Count != 0)
                    sensorDataList.Add(
                        new SensorData(sensorId, null, new AggregatedSensorData(minList, avgList, maxList)));
            }
            if (sensorDataList.Count != 0)
                result[kv.Key] = sensorDataList;
        }
        stopwatch.Stop();
        ResponseTimeMs = stopwatch.Elapsed.TotalMilliseconds;
        return result;
    }

    private List<SensorDataItem> BuildSensorDataMap(TimeSeriesCommands ts, string seriesName, DateRange dateRange,
        TsAggregation? aggregation, long? bucketDuration)
    {
        var range = ts.Range(seriesName, dateRange.From, dateRange.To, false,
            null, null, null, null, aggregation, bucketDuration);
        return range
            .Select(item => new SensorDataItem(GetDateTime((long)item.Time.Value), item.Val))
            .ToList();
    }

    private Dictionary<string, List<SensorData>> GetRawSensorData(HashSet<int> sensors, DateRange dateRange)
    {
        var db = _redisConnection.GetDatabase();
        var ts = db.TS();
        var result = new Dictionary<string, List<SensorData>>();
        var skipAggregation = dateRange.BucketDuration <= MinimumRawBucketDuration;
        TsAggregation? aggregation = skipAggregation ? null : TsAggregation.Avg;
        long ?bucketDuration = skipAggregation ? null : dateRange.BucketDuration;
        var stopwatch = new Stopwatch();
        stopwatch.Start();
        foreach (var kv in _sensorValueTypeMap)
        {
            var sensorsToProcess = sensors.Intersect(kv.Value).ToList();
            if (sensorsToProcess.Count == 0)
                continue;
            var sensorDataList = new List<SensorData>();
            foreach (var sensorId in sensorsToProcess)
            {
                var seriesName = $"{sensorId}:{kv.Key}";
                var range = ts.Range(seriesName, dateRange.From, dateRange.To, false,
                    null, null, null, null, aggregation, bucketDuration);
                if (range.Count == 0)
                    continue;
                var sensorData = range
                    .Select(item => new SensorDataItem(GetDateTime(item.Time), item.Val))
                    .ToList();
                sensorDataList.Add(new SensorData(sensorId, sensorData, null));
            }
            if (sensorDataList.Count != 0)
                result[kv.Key] = sensorDataList;
        }
        stopwatch.Stop();
        ResponseTimeMs = stopwatch.Elapsed.TotalMilliseconds;
        return result;
    }

    private static DateRange BuildDateRange(SmartHomeQuery query)
    {
        var maxPoints = query.MaxPoints <= 0 ? 2000 : query.MaxPoints; 
        var startDateTime = query.StartDate ?? query.StartDateOffset!.CalculateDateSubtract(DateTime.Now);
        var endDateTime = query.Period?.CalculateDateAdd(startDateTime) ?? DateTime.Now;
        var startMillis = new DateTimeOffset(startDateTime.ToUniversalTime()).ToUnixTimeMilliseconds();
        var endMillis = new DateTimeOffset(endDateTime.ToUniversalTime()).ToUnixTimeMilliseconds();
        var maxUnaggregatedMilliseconds = MillisecondsInHour * maxPoints;
        var aggregated = endMillis - startMillis > maxUnaggregatedMilliseconds;
        return new DateRange(aggregated, startMillis, endMillis, 
                    (endMillis - startMillis) / maxPoints);
    }

    private HashSet<int> GetSensors(string dataType)
    {
        return _sensors
            .Where(s => s.Value.DataType == dataType)
            .Select(s => s.Key)
            .ToHashSet();
    }

    public bool IsExtSensor(int sensorId)
    {
        var locationId = _sensors[sensorId].LocationId;
        return _locations[locationId].LocationType == "ext";
    }
}