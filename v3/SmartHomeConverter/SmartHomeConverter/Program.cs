using Npgsql;

if (args.Length != 4)
{
    Console.WriteLine("Usage: SmartHomeConverter [raw|aggregated] value_types_file_name database_name output_folder_name");
    return;
}

var aggregated = args[0] == "aggregated";
ValueTypes.Map = File.ReadAllLines(args[1])
    .Select((line, idx) => (line, idx))
    .ToDictionary(lineAndIdx => lineAndIdx.line.Trim().PadRight(4), lineAndIdx => lineAndIdx.idx + 1);
var databaseName = args[2];
var outputFolderName = args[3];

Console.WriteLine($"Converting database '{databaseName}' to '{outputFolderName}'");

var connection = new NpgsqlConnection($"Host=127.0.0.1;Username=postgres;Database={databaseName}");
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
    var data = new SortedDictionary<int, SortedDictionary<SensorEventsKey, SensorEvents<int>>>();
    using (var reader =
           new NpgsqlCommand("select sensor_id, event_date, event_time, value_type, value from sensor_events",
               connection).ExecuteReader())
    {
        while (reader.Read())
        {
            var sensorId = reader.GetInt32(0);
            var eventDate = reader.GetInt32(1);
            var eventTime = reader.GetInt32(2);
            var valueType = reader.GetString(3);
            var value = reader.GetInt32(4);
            var key = new SensorEventsKey(eventTime, sensorId);
            if (data.TryGetValue(eventDate, out var dateEvents))
            {
                if (dateEvents.TryGetValue(key, out var events))
                    events.AddEvent(valueType, value);
                else
                    dateEvents[key] = new SensorEvents<int>(valueType, value, new int[8]);
            }
            else
                data[eventDate] = new SortedDictionary<SensorEventsKey, SensorEvents<int>> { {key, new SensorEvents<int>(valueType, value, new int[8])} };
        }
    }

    foreach (var entry in data)
    {
        var year = entry.Key / 10000;
        var folderName = Path.Combine(outputFolderName, year.ToString());
        var fileName = Path.Combine(folderName, entry.Key.ToString());
        if (!Directory.Exists(folderName))
            Directory.CreateDirectory(folderName);
        File.WriteAllBytes(fileName, BuildEventsFile(entry.Value));
    }
}

void ProcessAggregatedData()
{
    var data = new SortedDictionary<int, SortedDictionary<int, SensorEvents<AggregatedValues>>>();
    using (var reader =
           new NpgsqlCommand("select sensor_id, event_date, value_type, min_value, max_value, avg_value from sensor_events_aggregated",
               connection).ExecuteReader())
    {
        while (reader.Read())
        {
            var sensorId = reader.GetInt32(0);
            var eventDate = reader.GetInt32(1);
            var valueType = reader.GetString(2);
            var minValue = reader.GetInt32(3);
            var maxValue = reader.GetInt32(4);
            var avgValue = reader.GetInt32(5);
            var values = new AggregatedValues(minValue, avgValue, maxValue);
            var initialValues = new AggregatedValues[8];
            for (var i = 0; i < 8; i++)
                initialValues[i] = new AggregatedValues(0, 0, 0);
            if (data.TryGetValue(eventDate, out var dateEvents))
            {
                if (dateEvents.TryGetValue(sensorId, out var events))
                    events.AddEvent(valueType, values);
                else
                    dateEvents[sensorId] = new SensorEvents<AggregatedValues>(valueType, values, initialValues);
            }
            else
                data[eventDate] = new SortedDictionary<int, SensorEvents<AggregatedValues>>
                    { { sensorId, new SensorEvents<AggregatedValues>(valueType, values, initialValues) } };
        }
    }
    var fileName = Path.Combine(outputFolderName, "aggregated");
    File.WriteAllBytes(fileName, BuildAggregatedEventsFile(data));
}

byte[] BuildEventsFile(SortedDictionary<SensorEventsKey, SensorEvents<int>> dateEvents)
{
    var stream = new MemoryStream();
    var writer = new BinaryWriter(stream);
    foreach (var entry in dateEvents)
    {
        writer.Write(entry.Key.EventTime);
        writer.Write(entry.Key.SensorId);
        writer.Write(entry.Value.Event.ValueTypes);
        for (var i = 0; i < 8; i++)
            writer.Write(entry.Value.Event.Values[i]);
    }
    return stream.ToArray();
}

byte[] BuildAggregatedEventsFile(SortedDictionary<int, SortedDictionary<int, SensorEvents<AggregatedValues>>> events)
{
    var stream = new MemoryStream();
    var writer = new BinaryWriter(stream);
    foreach (var entry in events)
    {
        foreach (var sensorEvents in entry.Value)
        {
            writer.Write(entry.Key); // date
            writer.Write(sensorEvents.Key); // sensor id
            writer.Write(sensorEvents.Value.Event.ValueTypes);
            for (var i = 0; i < 8; i++)
            {
                writer.Write(sensorEvents.Value.Event.Values[i].Min);
                writer.Write(sensorEvents.Value.Event.Values[i].Avg);
                writer.Write(sensorEvents.Value.Event.Values[i].Max);
            }
        }
    }
    return stream.ToArray();
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

internal class SensorEvents<T>
{
    private int EventNo;
    internal readonly SensorEvent<T> Event;

    internal SensorEvents(string valueType, T value, T[] initialValues)
    {
        EventNo = 0;
        Event = new SensorEvent<T>(new byte[8], initialValues);
        AddEvent(valueType, value);
    }
    
    internal void AddEvent(string valueType, T value)
    {
        if (EventNo >= Event.Values.Length)
            throw new ArgumentOutOfRangeException(nameof(EventNo), "Event number exceeds allowed maximum");
        var valueTypeId = (byte)ValueTypes.Map[valueType];
        Event.ValueTypes[EventNo] = valueTypeId;
        Event.Values[EventNo] = value;
        EventNo++;
    }
}

internal record SensorEvent<T>(
    byte[] ValueTypes,
    T[] Values
);

internal record AggregatedValues(int Min, int Avg, int Max);
