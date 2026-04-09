using System.Runtime.InteropServices;
using System.Text.Json;

namespace SmartHomeService;

public static class ValueTypes
{
    public static Dictionary<string, int> Map { get; private set; }= null!;
    public static Dictionary<int, string> ReverseMap { get; private set; }= null!;

    public static void Init(string fileName)
    {
        Map = File.ReadAllLines(fileName)
            .Select((line, idx) => (line, idx))
            .ToDictionary(lineAndIdx => lineAndIdx.line.Trim().PadRight(4), lineAndIdx => lineAndIdx.idx + 1);
        ReverseMap = Map.Select(kv => (kv.Value, kv.Key)).ToDictionary();
    }
}

[System.Runtime.CompilerServices.InlineArray(8)]
public record struct ValueTypeArray
{
    private byte _element0;
}

[System.Runtime.CompilerServices.InlineArray(8)]
public record struct ValueArray<T>
{
    private T _element0;
}

public record ValueTypeValue<T>(byte ValueType, T Value);

public readonly record struct SensorDataFileItem<T>(uint TimeAndSensorId, ValueTypeArray ValueTypes, ValueArray<T> Values)
{
    public SensorDataFileItem(uint timeAndSensorId, List<ValueTypeValue<T>> items): 
        this(timeAndSensorId, BuildValueTypeArray(items), BuildValueArray(items))
    {
    }

    private static ValueArray<T> BuildValueArray(List<ValueTypeValue<T>> items)
    {
        var arr = new ValueArray<T>();
        var idx = 0;
        foreach (var item in items)
            arr[idx++] = item.Value;
        return arr;
    }

    private static ValueTypeArray BuildValueTypeArray(List<ValueTypeValue<T>> items)
    {
        var arr = new ValueTypeArray();
        var idx = 0;
        foreach (var item in items)
            arr[idx++] = item.ValueType;
        return arr;
    }

    public uint SensorId => TimeAndSensorId & 0xFF;
    public uint EventTimeMs => (TimeAndSensorId >> 8) * 10;
}

public abstract class SensorEvents<T>
{
    public readonly List<SensorDataFileItem<T>> Items;

    protected SensorEvents()
    {
        Items = [];
    }
    
    protected SensorEvents(byte[] data)
    {
        Items = new List<SensorDataFileItem<T>>(MemoryMarshal.Cast<byte, SensorDataFileItem<T>>(data).ToArray());
    }
    
    public ReadOnlySpan<byte> ToBinary()
    {
        return MemoryMarshal.AsBytes(Items.ToArray());
    }
}

public sealed class RawSensorEvents: SensorEvents<int>
{
    public RawSensorEvents()
    {
    }

    public RawSensorEvents(byte[] data): base(data)
    {
    }
}

public sealed class AggregatedSensorEvents: SensorEvents<AggregatedValues>
{
    public AggregatedSensorEvents()
    {
        
    }

    public AggregatedSensorEvents(byte[] data): base(data)
    {
    }
}

public record struct AggregatedValues(int Min, int Avg, int Max)
{
    public AggregatedValues() : this(0, 0, 0)
    {
    }
}

public record FileSmartHomeServiceConfiguration(
    string BaseFolder, int KeyDivider, string SensorsFile, 
    string LocationsFile, string TimeZone, string ValueTypesFile);

public sealed class FileSmartHomeService: BaseSmartHomeService
{
    private readonly string _baseFolder;
    private readonly int _keyDivider;
    
    private static FileSmartHomeServiceConfiguration ReadConfiguration(string configFileName)
    {
        using var configurationStream = File.OpenRead(configFileName);
        return JsonSerializer.Deserialize<FileSmartHomeServiceConfiguration>(configurationStream)
               ?? throw new Exception("Invalid configuration file");
    }
    
    public FileSmartHomeService(string configFileName): this(ReadConfiguration(configFileName))
    {
    }

    public FileSmartHomeService(FileSmartHomeServiceConfiguration configuration) :
        base(configuration.SensorsFile, configuration.LocationsFile, configuration.TimeZone)
    {
        _baseFolder = configuration.BaseFolder;
        _keyDivider = configuration.KeyDivider;
        ValueTypes.Init(configuration.ValueTypesFile);
    }

    public override LastSensorData GetLastSensorData()
    {
        var fromDate = DateTime.Now.AddDays(-1);
        var from = fromDate.Year * 10000 + fromDate.Month * 100 + fromDate.Day;
        
        var data = new Dictionary<int, Dictionary<string, SensorDataItem>>();
        var sensors = Sensors
            .Where(s => s.Value.Enabled)
            .Select(s => s.Key)
            .ToHashSet();
        foreach (var fileData in ReadFiles(from, null, "", true))
        {
            var items = new RawSensorEvents(fileData.Data);
            if (items.Items.Count == 0)
                continue;
            for (var i = items.Items.Count - 1; i >= 0; i--)
            {
                var item = items.Items[i];
                var sid = item.SensorId;
                var locationId = Sensors[sid].LocationId;
                var idx = 0;
                foreach (var vt in item.ValueTypes)
                {
                    if (vt == 0)
                        break;
                    var valueType = ValueTypes.ReverseMap[vt];
                    var di = new SensorDataItem(BuildTimestamp(fileData.Key, item.EventTimeMs),
                        (double)item.Values[idx] / 100);
                    if (data.TryGetValue(locationId, out var locationValues))
                        locationValues[valueType] = di;
                    else
                        data[locationId] = new Dictionary<string, SensorDataItem> { { valueType, di } };
                    idx++;
                }
                sensors.Remove(sid);
                if (sensors.Count == 0)
                    break;
            }
            if (sensors.Count == 0)
                break;
        }
        return new LastSensorData(data);
    }

    private long BuildTimestamp(int date, uint timeMs)
    {
        var t = (int)(timeMs / 1000);
        var ms = (int)(timeMs % 1000);
        var hour = t / 3600;
        var minute = (t / 60) % 60;
        var second = t % 60;
        var dt = new DateTime(date / 10000, (date / 100) % 100, date % 100, hour, minute, second, ms, DateTimeKind.Unspecified);
        return new DateTimeOffset(TimeZoneInfo.ConvertTime(dt, TimeZone).ToUniversalTime()).ToUnixTimeMilliseconds();
    }

    public override SensorDataResult GetSensorData(SmartHomeQuery sensorDataQuery, out bool aggregated)
    {
        throw new NotImplementedException();
    }

    private IEnumerable<FileData> ReadFiles(int from, int? to, string extension, bool reverse)
    {
        var fromYear = from / _keyDivider;
        var toYear = to / _keyDivider;
        return Directory.EnumerateDirectories(_baseFolder)
            .Select(path => int.Parse(Path.GetFileName(path)))
            .Where(year => year >= fromYear && (toYear == null || year <= toYear))
            .SelectMany(year => Directory.EnumerateFiles(Path.Combine(_baseFolder, year.ToString())))
            .Where(name => Path.GetExtension(name) == extension)
            .Select(name => (int.Parse(Path.GetFileName(name)), name))
            .Where(pair => pair.Item1 >= from && (to == null || pair.Item1 <= to))
            .OrderBy(pair => reverse ? -pair.Item1 : pair.Item1)
            .Select(pair => new FileData(pair.Item1, File.ReadAllBytes(pair.name)));
    }
}

internal record FileData(int Key, byte[] Data);