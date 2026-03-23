using Npgsql;

if (args.Length != 6)
{
    Console.WriteLine("Usage: SmartHomeConverter host_name start_date [raw|aggregated] value_types_file_name database_name output_folder_name");
    return;
}

var hostName = args[0];
var startDate = int.Parse(args[1]);
var aggregated = args[2] == "aggregated";
ValueTypes.Map = File.ReadAllLines(args[3])
    .Select((line, idx) => (line, idx))
    .ToDictionary(lineAndIdx => lineAndIdx.line.Trim().PadRight(4), lineAndIdx => lineAndIdx.idx + 1);
var databaseName = args[4];
var outputFolderName = args[5];

Console.WriteLine($"Converting database '{databaseName}' to '{outputFolderName}'");

var connection = new NpgsqlConnection($"Host={hostName};Username=postgres;Database={databaseName}");
try
{
    connection.Open();
    if (aggregated)
        ProcessAggregatedData();
    else
        ProcessRawData();
}
catch (Exception e)
{
    Console.WriteLine(e);
}
finally
{
    connection.Close();
}

return;

void ProcessRawData()
{
    ProcessData<SensorEventsKey, RawSensorEvents>(
        "select sensor_id, event_date, value_type, event_time, value from sensor_events",
        (eventTime, sensorId) => new SensorEventsKey(eventTime, sensorId),
        (writer, key) =>
        {
            writer.Write(key.EventTime);
            writer.Write(key.SensorId);
        }, "");
}

void ProcessAggregatedData()
{
    ProcessData<int, AggregatedSensorEvents>(
        "select sensor_id, event_date, value_type, min_value, max_value, avg_value from sensor_events_aggregated",
        (_, sensorId) => sensorId,
        (writer, key) => writer.Write(key), ".aggregated");
}

void ProcessData<TK, TV>(string sql, Func<int, int, TK> keyBuilder, Action<BinaryWriter, TK> keyWriter, string fileSuffix)
    where TK : IComparable<TK> where TV: IEventHandler, new()
{
    sql += $" where event_date >= {startDate}";
    var data = new SortedDictionary<int, SortedDictionary<TK, TV>>();
    using (var reader = new NpgsqlCommand(sql, connection).ExecuteReader())
    {
        while (reader.Read())
        {
            var sensorId = reader.GetInt32(0);
            var eventDate = reader.GetInt32(1);
            var valueType = reader.GetString(2);
            var v4 = reader.GetInt32(3);
            var v5 = reader.GetInt32(4);
            var key = keyBuilder(v4, sensorId);
            if (data.TryGetValue(eventDate, out var dateEvents))
            {
                if (!dateEvents.TryGetValue(key, out var events))
                {
                    events = new TV();
                    dateEvents[key] = events;
                }
                events.AddEvent(valueType, v4, v5, reader);
            }
            else
            {
                var events = new TV();
                events.AddEvent(valueType, v4, v5, reader);
                data[eventDate] = new SortedDictionary<TK, TV> { { key, events } };
            }
        }
    }

    foreach (var entry in data)
    {
        var year = entry.Key / 10000;
        var folderName = Path.Combine(outputFolderName, year.ToString());
        var fileName = Path.Combine(folderName, entry.Key.ToString()) + fileSuffix;
        if (!Directory.Exists(folderName))
            Directory.CreateDirectory(folderName);

        var stream = new MemoryStream();
        var writer = new BinaryWriter(stream);
        foreach (var kv in entry.Value)
        {
            keyWriter(writer, kv.Key);
            kv.Value.Write(writer);
        }
        File.WriteAllBytes(fileName, stream.ToArray());
    }
}

internal record SensorEventsKey(int EventTime, int SensorId) : IComparable<SensorEventsKey>
{
    public int CompareTo(SensorEventsKey? other)
    {
        if (other is null) return 1;
        var v = EventTime.CompareTo(other.EventTime);
        return v != 0 ? v : SensorId.CompareTo(other.SensorId);
    }
}

internal static class ValueTypes
{
    internal static Dictionary<string, int> Map = null!;
}

internal interface IEventHandler
{
    void AddEvent(string valueType, int v4, int v5, NpgsqlDataReader reader);
    void Write(BinaryWriter writer);
}

internal abstract class SensorEvents<T>: IEventHandler where T: new()
{
    private int _eventNo;
    internal readonly SensorEvent<T> Event = new();

    public void AddEvent(string valueType, int v4, int v5, NpgsqlDataReader reader)
    {
        if (_eventNo >= Event.Values.Length)
            throw new ArgumentOutOfRangeException(nameof(_eventNo), "Event number exceeds allowed maximum");
        var valueTypeId = (byte)ValueTypes.Map[valueType];
        Event.ValueTypes[_eventNo] = valueTypeId;
        Event.Values[_eventNo] = BuildValue(v4, v5, reader);
        _eventNo++;
    }

    public abstract void Write(BinaryWriter writer);

    protected abstract T BuildValue(int v4, int v5, NpgsqlDataReader reader);
}

internal sealed class RawSensorEvents: SensorEvents<int>
{
    public override void Write(BinaryWriter writer)
    {
        writer.Write(Event.ValueTypes);
        for (var i = 0; i < 8; i++)
            writer.Write(Event.Values[i]);
    }

    protected override int BuildValue(int v4, int v5, NpgsqlDataReader reader)
    {
        return v5;
    }
}

internal sealed class AggregatedSensorEvents: SensorEvents<AggregatedValues>
{
    public override void Write(BinaryWriter writer)
    {
        writer.Write(Event.ValueTypes);
        for (var i = 0; i < 8; i++)
        {
            writer.Write(Event.Values[i].Min);
            writer.Write(Event.Values[i].Avg);
            writer.Write(Event.Values[i].Max);
        }
    }

    protected override AggregatedValues BuildValue(int v4, int v5, NpgsqlDataReader reader)
    {
        return new AggregatedValues(v4, reader.GetInt32(5), v5);
    }
}

internal class SensorEvent<T> where T: new()
{
    internal readonly byte[] ValueTypes;
    internal readonly T[] Values;
    
    public SensorEvent()
    {
        ValueTypes = new byte[8];
        Values = new T[8];
        for (var i = 0; i < 8; i++)
            Values[i] = new T();
    }
}

internal record AggregatedValues(int Min, int Avg, int Max)
{
    public AggregatedValues(): this(0, 0, 0) { }
}
