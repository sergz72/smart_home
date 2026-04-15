using System.Collections.Immutable;
using System.Diagnostics;
using System.Text.Json;
using NRedisStack;
using NRedisStack.Literals.Enums;
using NRedisStack.RedisStackCommands;
using StackExchange.Redis;

namespace SmartHomeService;

public record RedisSmartHomeServiceConfiguration(string RedisConnectionString, string SensorsFile, string LocationsFile, string TimeZone);

public sealed class RedisSmartHomeService: BaseSmartHomeService
{
    private record DateRange(bool Aggregated, long From, long To, long BucketDuration);
    
    private const long MillisecondsInHour = 60 * 60 * 1000;
    private const long MinimumRawBucketDuration = 5 * 60 * 1000; // 5 minutes
    private const long MinimumAggregatedBucketDuration = 24 * 60 * 60 * 1000; // 1 Day
    
    private readonly Dictionary<string, HashSet<uint>> _sensorValueTypeMap;
    private readonly ConnectionMultiplexer _redisConnection;

    private static RedisSmartHomeServiceConfiguration ReadConfiguration(string configFileName)
    {
        using var configurationStream = File.OpenRead(configFileName);
        return JsonSerializer.Deserialize<RedisSmartHomeServiceConfiguration>(configurationStream)
                            ?? throw new Exception("Invalid configuration file");
    }
    
    public RedisSmartHomeService(string configFileName): this(ReadConfiguration(configFileName))
    {
    }

    public RedisSmartHomeService(RedisSmartHomeServiceConfiguration redisSmartHomeServiceConfiguration): 
        base(redisSmartHomeServiceConfiguration.SensorsFile, redisSmartHomeServiceConfiguration.LocationsFile,
             redisSmartHomeServiceConfiguration.TimeZone)
    {
        _redisConnection = ConnectionMultiplexer.Connect(redisSmartHomeServiceConfiguration.RedisConnectionString);
        
        var db = _redisConnection.GetDatabase();
        var ts = db.TS();
        _sensorValueTypeMap = new Dictionary<string, HashSet<uint>>();
        foreach (var seriesName in ts.QueryIndex(["type=sensor"]))
        {
            var parts = seriesName.Split(':');
            var sensorId = uint.Parse(parts[0]);
            var valueType = parts[1];
            if (_sensorValueTypeMap.TryGetValue(valueType, out var sensorIds))
                sensorIds.Add(sensorId);
            else
                _sensorValueTypeMap[valueType] = [sensorId];
        }
    }

    public override LastSensorData GetLastSensorData()
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
            var sensorId = uint.Parse(parts[0]);
            var locationId = Sensors[sensorId].LocationId;
            var valueType = parts[1];
            var value = v.value.Val;
            if (result.TryGetValue(locationId, out var lastData))
            {
                if (!lastData.TryGetValue(valueType, out var lastValue) || lastValue.Timestamp < timestamp)
                    lastData[valueType] = new SensorDataItem(timestamp, value);
            }
            else
                result[locationId] = new Dictionary<string, SensorDataItem> {{valueType, new SensorDataItem(timestamp, value)}};
        }
        stopwatch.Stop();
        ResponseTimeMs = stopwatch.Elapsed.TotalMilliseconds;
        return new LastSensorData(result);
    }

    public override SensorDataResult GetSensorData(SmartHomeQuery sensorDataQuery, out bool aggregated)
    {
        var sensors = GetSensors(sensorDataQuery.DataType);
        if (sensors.Count == 0)
        {
            aggregated = false;
            return new SensorDataResult([]);
        }
        var dateRange = BuildDateRange(sensorDataQuery);
        aggregated = dateRange.Aggregated;
        return dateRange.Aggregated ? GetAggregatedSensorData(sensors, dateRange) : GetRawSensorData(sensors, dateRange); 
    }

    public override YearlySensorDataResult GetYearlySensorData()
    {
        var db = _redisConnection.GetDatabase();
        var ts = db.TS();
        // year to location to valueType
        var result = new Dictionary<int, Dictionary<int, Dictionary<string, AggregatedSensorDataItem>>>();
        var stopwatch = new Stopwatch();
        stopwatch.Start();
        foreach (var kv in _sensorValueTypeMap)
        {
            foreach (var sensorId in kv.Value)
            {
                var minList = BuildSensorDataMap(ts, $"{sensorId}:{kv.Key}:min_y");
                ProcessList(result, minList, sensorId, kv.Key, (aggregated, item) => aggregated with {Min = item});
                var avgList = BuildSensorDataMap(ts, $"{sensorId}:{kv.Key}:avg_y");
                ProcessList(result, avgList, sensorId, kv.Key, (aggregated, item) => aggregated with {Avg = item.Value});
                var maxList = BuildSensorDataMap(ts, $"{sensorId}:{kv.Key}:max_y");
                ProcessList(result, maxList, sensorId, kv.Key, (aggregated, item) => aggregated with {Max = item});
            }
        }
        stopwatch.Stop();
        ResponseTimeMs = stopwatch.Elapsed.TotalMilliseconds;
        var dict = result.Select(kv => (kv.Key, new LastAggregatedSensorData(kv.Value))).ToDictionary();
        return new YearlySensorDataResult(new SortedDictionary<int, LastAggregatedSensorData>(dict));
    }

    private void ProcessList(Dictionary<int, Dictionary<int, Dictionary<string, AggregatedSensorDataItem>>> result, 
        List<SensorDataItem> list, uint sensorId, string valueType,
        Func<AggregatedSensorDataItem, SensorDataItem, AggregatedSensorDataItem> transformer)
    {
        var locationId = Sensors[sensorId].LocationId;
        foreach (var value in list)
        {
            var year = GetDateTime(value.Timestamp).Year;
            if (!result.TryGetValue(year, out var yearData))
            {
                yearData = new Dictionary<int, Dictionary<string, AggregatedSensorDataItem>>();
                result[year] = yearData;
            }

            if (!yearData.TryGetValue(locationId, out var locationData))
            {
                locationData = new Dictionary<string, AggregatedSensorDataItem>();
                yearData[locationId] = locationData;
            }

            if (!locationData.TryGetValue(valueType, out var valueTypeData))
                valueTypeData = new AggregatedSensorDataItem(value, value.Value, value);
            else
                valueTypeData = transformer(valueTypeData, value);
            locationData[valueType] = valueTypeData;
        }
    }

    private SensorDataResult GetAggregatedSensorData(HashSet<uint> sensors, DateRange dateRange)
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
                        new SensorData(Sensors[sensorId].LocationId, null, new AggregatedSensorData(minList, avgList, maxList)));
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

    private static List<SensorDataItem> BuildSensorDataMap(TimeSeriesCommands ts, string seriesName)
    {
        var range = ts.Range(seriesName, "-", "+");
        return range
            .Select(item => new SensorDataItem((long)item.Time.Value, item.Val))
            .ToList();
    }
    
    private SensorDataResult GetRawSensorData(HashSet<uint> sensors, DateRange dateRange)
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
                sensorDataList.Add(new SensorData(Sensors[sensorId].LocationId, sensorData, null));
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
        var range = BuildBaseDateRange(query);
        var startMillis = new DateTimeOffset(range.StartDateTime).ToUnixTimeMilliseconds();
        var endMillis = new DateTimeOffset(range.EndDateTime).ToUnixTimeMilliseconds();
        var maxUnaggregatedMilliseconds = MillisecondsInHour * range.MaxPoints;
        var aggregated = endMillis - startMillis > maxUnaggregatedMilliseconds;
        return new DateRange(aggregated, startMillis, endMillis, 
                    (endMillis - startMillis) / range.MaxPoints);
    }
}