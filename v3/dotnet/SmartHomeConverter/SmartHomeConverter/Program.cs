using Npgsql;
using NRedisStack;
using NRedisStack.DataTypes;
using NRedisStack.RedisStackCommands;
using SmartHomeService;
using StackExchange.Redis;

if (args.Length is < 3 or > 6)
{
    Console.WriteLine("Usage: SmartHomeConverter\n" +
    "  [raw connection_string start_date value_types_file_name output_folder_name [time zone]]\n" +
    "  [aggregate start_date folder_name]\n" +
    "  [aggregate_yearly start_year folder_name]\n" +
    "  [compare file1 file2]\n" +
    "  [show value_types_file_name file_name]");
    return;
}

switch (args[0])
{
    case "raw":
        var connectionString = args[1];
        var startDate = int.Parse(args[2]);
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
            ConvertFromPostgres(startDate, outputFolderName, parts[2], parts[3]);
            return;
        }

        if (connectionString.StartsWith("redis://"))
        {
            ConvertFromRedis(connectionString, startDate, outputFolderName, args[5]); // time zone
            return;
        }

        Console.WriteLine("Unsupported database type");
        break;
    case "aggregate":
        var aStartDate = int.Parse(args[1]);
        var aFolderName = args[2];
        FileSmartHomeService.AggregateRawData(aStartDate, aFolderName, 10000);
        break;
    case "aggregate_yearly":
        var year = int.Parse(args[1]);
        var yFolderName = args[2];
        FileSmartHomeService.AggregateYearly(year, yFolderName, 10000);
        break;
    case "compare":
        Compare(args[1], args[2]);
        break;
    case "show":
        ValueTypes.Init(args[1]);
        Show(args[2]);
        break;
    default:
        Console.WriteLine("Unknown command");
        break;
}

return;

void Show(string fileName)
{
    if (fileName.EndsWith(FileSmartHomeService.AggregatedFileExtension))
    {
        var aData = new AggregatedSensorEvents(File.ReadAllBytes(fileName));
        ShowData(aData);
        return;
    }
    var data = new RawSensorEvents(File.ReadAllBytes(fileName));
    ShowData(data);
}

void ShowData<T>(SensorEvents<T> data)
{
    foreach (var item in data.Items)
        Console.WriteLine(item);
}

void Compare(string file1, string file2)
{
    if (file1.EndsWith(FileSmartHomeService.AggregatedFileExtension))
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

void ConvertFromRedis(string connectionString, int startDate, string outputFolderName, string timeZoneName)
{
    var timeZone = TimeZoneInfo.FindSystemTimeZoneById(timeZoneName);
    
    var redisDbAddress = connectionString.Split("//")[1];
    
    Console.WriteLine($"Converting database '{redisDbAddress}' to '{outputFolderName}'");
    try
    {
        using var redisConnection = ConnectionMultiplexer.Connect(redisDbAddress);
        var db = redisConnection.GetDatabase();
        var ts = db.TS();

        var processor = new RedisProcessor(ts, startDate, timeZone);
        
        foreach (var seriesName in ts.QueryIndex(["type=sensor"]))
        {
            var parts = seriesName.Split(':');
            var sensorId = uint.Parse(parts[0]);
            var valueType = parts[1];
            var valueTypeId = ValueTypes.Map[valueType];
            processor.Process(seriesName, sensorId, (byte)valueTypeId);
        }
        processor.WriteData(10000, outputFolderName);
    }
    catch (Exception e)
    {
        Console.WriteLine(e);
    }
}

void ConvertFromPostgres(int startDate, string outputFolderName, string hostName, string databaseName)
{
    Console.WriteLine($"Converting database '{databaseName}' to '{outputFolderName}'");

    using var connection = new NpgsqlConnection($"Host={hostName};Username=postgres;Database={databaseName}");
    try
    {
        connection.Open();
        ProcessPostgresRawData(connection, startDate, outputFolderName);
    }
    catch (Exception e)
    {
        Console.WriteLine(e);
    }
}

void ProcessPostgresRawData(NpgsqlConnection connection, int startDate, string outputFolderName)
{
    ProcessPostgresData(connection, startDate, outputFolderName,
        "select sensor_id, event_date, value_type, event_time, value from sensor_events",
        (eventTime, sensorId) => (To10Ms(eventTime) << 8) | sensorId,
        (v4, v5, f) => v5);
}

uint To10Ms(uint eventTime)
{
    var hour = eventTime / 10000;
    var minute = (eventTime / 100) % 100;
    var second = eventTime % 100;
    return (hour * 3600 + minute * 60 + second) * 100;
}

void ProcessPostgresData(NpgsqlConnection connection, int startDate, string outputFolderName, string sql,
    Func<uint, uint, uint> keyBuilder, Func<int, int, Func<int, int>, int> objectBuilder)
{
    sql += $" where event_date >= {startDate}";
    var data = new SortedDictionary<int, SortedDictionary<uint, EventHolder<int>>>();
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
                    events = new EventHolder<int>();
                    dateEvents[key] = events;
                }

                events.AddEvent(valueType, objectBuilder(v4, v5, reader.GetInt32));
            }
            else
            {
                var events = new EventHolder<int>();
                events.AddEvent(valueType, objectBuilder(v4, v5, reader.GetInt32));
                data[eventDate] = new SortedDictionary<uint, EventHolder<int>> { { key, events } };
            }
        }
    }
    
    RedisProcessor.WriteData(data, 10000, outputFolderName);
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

internal class RedisProcessor(TimeSeriesCommands ts, int from, TimeZoneInfo timeZone)
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

    internal void WriteData(int keyDivider, string outputFolderName) => WriteData(Data, keyDivider, outputFolderName);
    
    internal static void WriteData(SortedDictionary<int, SortedDictionary<uint, EventHolder<int>>> data, int keyDivider,
        string outputFolderName)
    {
        foreach (var entry in data)
        {
            var year = entry.Key / keyDivider;
            var folderName = Path.Combine(outputFolderName, year.ToString());
            if (!Directory.Exists(folderName))
                Directory.CreateDirectory(folderName);
            var fileName = Path.Combine(folderName, entry.Key.ToString()) + FileSmartHomeService.RawFileExtension;

            var list = entry.Value
                .Select(v => v.Value.BuildItem(v.Key))
                .ToList();
            var events = new RawSensorEvents(list);
            var bytes = events.ToBinary();
            File.WriteAllBytes(fileName, bytes);
        }
    }
}
