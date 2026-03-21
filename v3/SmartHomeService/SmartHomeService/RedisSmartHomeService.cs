using System.Diagnostics;
using System.Text.Json;
using NRedisStack;
using NRedisStack.Literals.Enums;
using NRedisStack.RedisStackCommands;
using StackExchange.Redis;

namespace SmartHomeService;

internal record Configuration(string RedisConnectionString, string SensorsFile, string LocationsFile, string TimeZone);

public sealed class RedisSmartHomeService: ISmartHomeService
{
    private record DateRange(bool Aggregated, long From, long To, long BucketDuration);
    
    private const long MillisecondsInHour = 60 * 60 * 1000;
    private const long MinimumRawBucketDuration = 5 * 60 * 1000; // 5 minutes
    private const long MinimumAggregatedBucketDuration = 24 * 60 * 60 * 1000; // 1 Day
    
    private static readonly Dictionary<string, string> ValueTypeMap = new Dictionary<string, string>
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
    private readonly string _timeZoneName;
    
    public double ResponseTimeMs { get; private set; }
    
    public RedisSmartHomeService(string configFileName)
    {
        using var configurationStream = File.OpenRead(configFileName);
        var configuration = JsonSerializer.Deserialize<Configuration>(configurationStream)
                            ?? throw new Exception("Invalid configuration file");
        _timeZone = TimeZoneInfo.FindSystemTimeZoneById(configuration.TimeZone);
        _timeZoneName = configuration.TimeZone;
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

    public string GetValueType(string valueType)
    {
        return ValueTypeMap[valueType];
    }

    public string GetLocationNameBySensorId(int sensorId)
    {
        var locationId = _sensors[sensorId].LocationId;
        return _locations[locationId].Name;
    }

    public string GetLocationName(int locationId)
    {
        return _locations[locationId].Name;
    }
    
    public SensorDataItemWithDate BuildSensorDataItemWithDate(SensorDataItem data)
    {
        return new SensorDataItemWithDate(GetDateTime(data.Timestamp), data.Value);
    }
    
    public DateTime GetDateTime(long timestamp)
    {
        return TimeZoneInfo
            .ConvertTimeFromUtc(DateTimeOffset.FromUnixTimeMilliseconds(timestamp).DateTime, _timeZone);
    }
    
    public LastSensorData GetLastSensorData()
    {
        var db = _redisConnection.GetDatabase();
        var ts = db.TS();
        var result = new Dictionary<int, Dictionary<string, SensorDataItem>>();
        var minimumTimestamp = new DateTimeOffset(DateTime.Now.AddDays(-1).ToUniversalTime())
            .ToUnixTimeMilliseconds();
        var stopwatch = new Stopwatch();
        stopwatch.Start();
        foreach (var v in ts.MGet(["type=sensor"], true))
        {
            var timestamp = (long)v.value.Time.Value;
            if (timestamp < minimumTimestamp)
                continue;
            var parts = v.key.Split(':');
            var sensorId = int.Parse(parts[0]);
            var locationId = _sensors[sensorId].LocationId;
            var valueType = parts[1];
            var value = v.value.Val;
            if (result.TryGetValue(locationId, out var lastData))
                lastData[valueType] = new SensorDataItem(timestamp, value);
            else
                result[locationId] = new Dictionary<string, SensorDataItem> {{valueType, new SensorDataItem(timestamp, value)}};
        }
        stopwatch.Stop();
        ResponseTimeMs = stopwatch.Elapsed.TotalMilliseconds;
        return new LastSensorData(result);
    }

    public SensorDataResult GetSensorData(SmartHomeQuery sensorDataQuery)
    {
        var sensors = GetSensors(sensorDataQuery.DataType);
        if (sensors.Count == 0)
            return new SensorDataResult([]);
        var dateRange = BuildDateRange(sensorDataQuery);
        return dateRange.Aggregated ? GetAggregatedSensorData(sensors, dateRange) : GetRawSensorData(sensors, dateRange); 
    }

    private SensorDataResult GetAggregatedSensorData(HashSet<int> sensors, DateRange dateRange)
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
        return new SensorDataResult(result);
    }

    private static List<SensorDataItem> BuildSensorDataMap(TimeSeriesCommands ts, string seriesName, DateRange dateRange,
        TsAggregation? aggregation, long? bucketDuration)
    {
        var range = ts.Range(seriesName, dateRange.From, dateRange.To, false,
            null, null, null, null, aggregation, bucketDuration);
        return range
            .Select(item => new SensorDataItem((long)item.Time.Value, item.Val))
            .ToList();
    }

    private SensorDataResult GetRawSensorData(HashSet<int> sensors, DateRange dateRange)
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
                    .Select(item => new SensorDataItem((long)item.Time.Value, item.Val))
                    .ToList();
                sensorDataList.Add(new SensorData(sensorId, sensorData, null));
            }
            if (sensorDataList.Count != 0)
                result[kv.Key] = sensorDataList;
        }
        stopwatch.Stop();
        ResponseTimeMs = stopwatch.Elapsed.TotalMilliseconds;
        return new SensorDataResult(result);
    }

    private DateRange BuildDateRange(SmartHomeQuery query)
    {
        var maxPoints = query.MaxPoints <= 0 ? 2000 : query.MaxPoints;
        var now = TimeZoneInfo.ConvertTime(DateTime.Now, _timeZone);
        var startDateTime = query.StartDate ?? query.StartDateOffset!.CalculateDateSubtract(now);
        var endDateTime = query.Period?.CalculateDateAdd(startDateTime) ?? now;
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

    public Locations GetLocations()
    {
        var map = _sensors
            .GroupBy(s => s.Value.LocationId)
            .Select(s => (s.Key,
                new LocationAndSensors(
                    new Location(_locations[s.Key].Name, _locations[s.Key].LocationType), 
                    s.Select(ss => ss.Key).ToArray())))
            .ToDictionary(s => s.Key, s => s.Item2);
        return new Locations(map, _timeZoneName);
    }
}