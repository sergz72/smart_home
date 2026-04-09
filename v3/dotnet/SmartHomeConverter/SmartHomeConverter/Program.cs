using System.Runtime.InteropServices;
using Npgsql;
using NRedisStack;
using NRedisStack.DataTypes;
using NRedisStack.RedisStackCommands;
using SmartHomeService;
using StackExchange.Redis;

if (args.Length is < 3 or > 6)
{
    Console.WriteLine("Usage: SmartHomeConverter [connection_string start_date [raw|aggregated] value_types_file_name output_folder_name [time zone]][compare file1 file2]");
    return;
}

var connectionString = args[0];

if (connectionString == "compare")
{
    Compare(args[1], args[2]);
    return;
}

var startDate = int.Parse(args[1]);
var aggregated = args[2] == "aggregated";
ValueTypes.Init(args[3]);
var outputFolderName = args[4];

if (connectionString.StartsWith("postgres://"))
{
    var parts = connectionString.Split('/');
    if (parts.Length != 4 || parts[1].Length != 0 || parts[2].Length == 0 || parts[3].Length == 0)
    {
        Console.WriteLine("Invalid connection string");
        return;
    }
    ConvertFromPostgres(parts[2], parts[3]);
    return;
}

if (connectionString.StartsWith("redis://"))
{
    ConvertFromRedis(args[5]); // time zone
    return;
}

Console.WriteLine("Unsupported database type");

return;

void Compare(string file1, string file2)
{
    if (file1.EndsWith(".aggregated"))
    {
        var data1 = new AggregatedSensorEvents(File.ReadAllBytes(file1));
        var data2 = new AggregatedSensorEvents(File.ReadAllBytes(file2));
        CompareEvents(data1, data2);
    }
    else
    {
        var data1 = new RawSensorEvents(File.ReadAllBytes(file1));
        var data2 = new RawSensorEvents(File.ReadAllBytes(file2));
        CompareEvents(data1, data2);
    }
}

void CompareEvents<T>(SensorEvents<T> e1, SensorEvents<T> e2)
{
    if (e1.Items.Count != e2.Items.Count)
    {
        Console.WriteLine($"Raw data count mismatch: {e1.Items.Count} vs {e2.Items.Count}");
        return;
    }

    for (var i = 0; i < e1.Items.Count; i++)
    {
        if (!e1.Items[i].Equals(e2.Items[i]))
        {
            Console.WriteLine($"Items are different: {e1} {e2}");
            return;
        }
    }
}

void ConvertFromRedis(string timeZoneName)
{
    var timeZone = TimeZoneInfo.FindSystemTimeZoneById(timeZoneName);
    
    var redisDbAddress = connectionString.Split("//")[1];
    
    Console.WriteLine($"Converting database '{redisDbAddress}' to '{outputFolderName}'");
    try
    {
        using var redisConnection = ConnectionMultiplexer.Connect(redisDbAddress);
        var db = redisConnection.GetDatabase();
        var ts = db.TS();

        IProcessor processor = aggregated
            ? new RedisAggregatedDataProcessor(ts, startDate, timeZone)
            : new RedisRawDataProcessor(ts, startDate, timeZone);
        
        foreach (var seriesName in ts.QueryIndex(["type=sensor"]))
        {
            var parts = seriesName.Split(':');
            var sensorId = uint.Parse(parts[0]);
            var valueType = parts[1];
            var valueTypeId = ValueTypes.Map[valueType];
            processor.Process(seriesName, sensorId, (byte)valueTypeId);
        }
        processor.WriteData(outputFolderName);
    }
    catch (Exception e)
    {
        Console.WriteLine(e);
    }
}

void ConvertFromPostgres(string hostName, string databaseName)
{
    Console.WriteLine($"Converting database '{databaseName}' to '{outputFolderName}'");

    var connection = new NpgsqlConnection($"Host={hostName};Username=postgres;Database={databaseName}");
    try
    {
        connection.Open();
        if (aggregated)
            ProcessPostgresAggregatedData(connection);
        else
            ProcessPostgresRawData(connection);
    }
    catch (Exception e)
    {
        Console.WriteLine(e);
    }
    finally
    {
        connection.Close();
    }
}

void ProcessPostgresRawData(NpgsqlConnection connection)
{
    ProcessPostgresData(connection,
        "select sensor_id, event_date, value_type, event_time, value from sensor_events",
        (eventTime, sensorId) => (eventTime << 8) | sensorId,
        (v4, v5, f) => v5,"");
}

void ProcessPostgresAggregatedData(NpgsqlConnection connection)
{
    ProcessPostgresData(connection,
        "select sensor_id, event_date, value_type, min_value, max_value, avg_value from sensor_events_aggregated",
        (_, sensorId) => sensorId,
        (v4, v5, f) => new AggregatedValues(v4, f(5), v5), ".aggregated");
}

void ProcessPostgresData<T>(NpgsqlConnection connection, string sql, Func<uint, uint, uint> keyBuilder,
    Func<int, int, Func<int, int>, T> objectBuilder, string fileSuffix) where T : struct
{
    sql += $" where event_date >= {startDate}";
    var data = new SortedDictionary<int, SortedDictionary<uint, EventHolder<T>>>();
    using (var reader = new NpgsqlCommand(sql, connection).ExecuteReader())
    {
        while (reader.Read())
        {
            var sensorId = (uint)reader.GetInt32(0);
            var eventDate = reader.GetInt32(1);
            var valueType = reader.GetString(2);
            var v4 = reader.GetInt32(3);
            var v5 = reader.GetInt32(4);
            var key = keyBuilder((uint)v4, sensorId);
            if (data.TryGetValue(eventDate, out var dateEvents))
            {
                if (!dateEvents.TryGetValue(key, out var events))
                {
                    events = new EventHolder<T>();
                    dateEvents[key] = events;
                }

                events.AddEvent(valueType, objectBuilder(v4, v5, reader.GetInt32));
            }
            else
            {
                var events = new EventHolder<T>();
                events.AddEvent(valueType, objectBuilder(v4, v5, reader.GetInt32));
                data[eventDate] = new SortedDictionary<uint, EventHolder<T>> { { key, events } };
            }
        }
    }
    
    IProcessor.WriteData(data, fileSuffix, outputFolderName);
}

internal sealed class EventHolder<T>
{
    public readonly Dictionary<byte, T> ValueMap;

    public EventHolder()
    {
        ValueMap = [];
    }
    
    public EventHolder(Dictionary<byte, T> valueMap)
    {
        ValueMap = valueMap;
    }
    
    internal void AddEvent(string valueType, T value)
    {
        if (ValueMap.Count >= 8)
            throw new ArgumentOutOfRangeException(nameof(ValueMap), "Event number exceeds allowed maximum");
        var valueTypeId = (byte)ValueTypes.Map[valueType];
        ValueMap.Add(valueTypeId, value);
    }
    
    internal SensorDataFileItem<T> BuildItem(uint timeAndSensorId)
    {
        return new SensorDataFileItem<T>(timeAndSensorId, ValueMap.Select(kv => new ValueTypeValue<T>(kv.Key, kv.Value)).ToList());
    }

    public void AddEvent(byte valueTypeId, T value)
    {
        if (ValueMap.Count >= 8)
            throw new ArgumentOutOfRangeException(nameof(ValueMap), "Event number exceeds allowed maximum");
        ValueMap.Add(valueTypeId, value);
    }
}

internal interface IProcessor
{
    void Process(string seriesName, uint sensorId, byte valueTypeId);
    void WriteData(string outputFolderName);
    
    internal static void WriteData<T>(SortedDictionary<int, SortedDictionary<uint, EventHolder<T>>> data,
        string fileSuffix, string outputFolderName) where T: struct
    {
        foreach (var entry in data)
        {
            var year = entry.Key / 10000;
            var folderName = Path.Combine(outputFolderName, year.ToString());
            var fileName = Path.Combine(folderName, entry.Key.ToString()) + fileSuffix;
            if (!Directory.Exists(folderName))
                Directory.CreateDirectory(folderName);

            var arr = entry.Value
                .Select(v => v.Value.BuildItem(v.Key))
                .ToArray();
            var bytes = MemoryMarshal.AsBytes(arr);
            File.WriteAllBytes(fileName, bytes);
        }
    }
}

internal abstract class RedisProcessor(TimeSeriesCommands ts, int from, TimeZoneInfo timeZone): IProcessor
{
    protected readonly SortedDictionary<int, SortedDictionary<uint, EventHolder<int>>> Data = new();
    private readonly TimeStamp _from = BuildTimestamp(from, timeZone);
    private readonly TimeStamp _to = new("+");

    private static TimeStamp BuildTimestamp(int from, TimeZoneInfo timeZone)
    {
        var dt = new DateTime(from / 10000, (from / 100) % 100, from % 100, 0, 0, 0, DateTimeKind.Unspecified);
        return new DateTimeOffset(TimeZoneInfo.ConvertTime(dt, timeZone).ToUniversalTime()).ToUnixTimeMilliseconds();
    }
    
    private DateTime GetDateTime(long timestamp)
    {
        return TimeZoneInfo
            .ConvertTimeFromUtc(DateTimeOffset.FromUnixTimeMilliseconds(timestamp).DateTime, timeZone);
    }

    public void Process(string seriesName, uint sensorId, byte valueTypeId)
    {
        var range = ts.Range(seriesName, _from, _to);

        foreach (var entry in range)
        {
            var t = GetDateTime((long)entry.Time.Value);
            var eventDate = t.Year * 10000 + t.Month * 100 + t.Day;
            var eventTime = (uint)(t.TimeOfDay.TotalMilliseconds / 10);
            var v = (int)Math.Round(entry.Val * 100);
            
            var key = (eventTime << 8) | sensorId;
            
            if (Data.TryGetValue(eventDate, out var dateEvents))
            {
                if (!dateEvents.TryGetValue(key, out var eh))
                {
                    eh = new EventHolder<int>();
                    dateEvents[key] = eh;
                }

                eh.AddEvent(valueTypeId, v);
            }
            else
            {
                var eh = new EventHolder<int>();
                eh.AddEvent(valueTypeId, v);
                Data[eventDate] = new SortedDictionary<uint, EventHolder<int>> { { key, eh } };
            }
        }
    }

    public abstract void WriteData(string outputFolderName);
}

internal sealed class RedisRawDataProcessor(TimeSeriesCommands ts, int from, TimeZoneInfo timeZone) :
    RedisProcessor(ts, from, timeZone)
{
    public override void WriteData(string outputFolderName)
    {
        IProcessor.WriteData(Data, "", outputFolderName);
    }
}

internal record struct AggregatedValuesL(int Min, long Sum, int Max, long Cnt)
{
    public AggregatedValuesL() : this(0, 0L, 0, 0L)
    {
    }
    
    internal AggregatedValues ToAggregatedValues()
    {
        return new AggregatedValues(Min, (int)(Sum / Cnt), Max);
    }
}

internal sealed class RedisAggregatedDataProcessor(TimeSeriesCommands ts, int from, TimeZoneInfo timeZone) :
    RedisProcessor(ts, from, timeZone)
{
    public override void WriteData(string outputFolderName)
    {
        var transformed = new SortedDictionary<int, SortedDictionary<uint, EventHolder<AggregatedValues>>>(Data
            .Select(d => (d.Key, Aggregate(d.Value)))
            .ToDictionary());
        IProcessor.WriteData(transformed, ".aggregated", outputFolderName);
    }
    
    private SortedDictionary<uint, EventHolder<AggregatedValues>> Aggregate(SortedDictionary<uint, EventHolder<int>> rawData)
    {
        return new SortedDictionary<uint, EventHolder<AggregatedValues>>(rawData
            .GroupBy(kv => kv.Key & 0xFF)
            .Select(grp => (grp.Key, Aggregate(grp.AsEnumerable())))
            .ToDictionary(kv => kv.Key, kv => kv.Item2)
        );
    }
    
    private static EventHolder<AggregatedValues> Aggregate(IEnumerable<KeyValuePair<uint, EventHolder<int>>> grp)
    {
        var map = new Dictionary<byte, AggregatedValuesL>();
        foreach (var i in grp)
        {
            foreach (var (vt, v) in i.Value.ValueMap)
            {
                if (map.TryGetValue(vt, out var values))
                {
                    if (values.Min > v)
                        values.Min = v;
                    else if (values.Max < v)
                        values.Max = v;
                    else
                        values.Sum += v;
                    values.Cnt++;
                }
                else
                    map[vt] = new AggregatedValuesL(v, v, v, 1);
            }
        }

        return new EventHolder<AggregatedValues>(map.Select(kv => (kv.Key, kv.Value.ToAggregatedValues())).ToDictionary());
    }
}
