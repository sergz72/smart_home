using System.Runtime.InteropServices;
using System.Text;
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
public struct ValueTypeArray
{
    private byte _element0;
    
    public bool Equals(ValueTypeArray other)
    {
        for (var i = 0; i < 8; i++)
        {
            if (this[i] != other[i])
                return false;
        }
        return true;
    }

    public override int GetHashCode()
    {
        return 0;
    }
}

[System.Runtime.CompilerServices.InlineArray(8)]
public struct ValueArray<T> : IEquatable<ValueArray<T>>
{
    private T _element0;

    public bool Equals(ValueArray<T> other)
    {
        for (var i = 0; i < 8; i++)
        {
            if (!this[i]!.Equals(other[i]))
                return false;
        }
        return true;
    }

    public override int GetHashCode()
    {
        return 0;
    }
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
    public uint EventTime => (TimeAndSensorId >> 8);
    
    public bool Equals(SensorDataFileItem<T> other)
    {
        return TimeAndSensorId == other.TimeAndSensorId && ValueTypes.Equals(other.ValueTypes) && Values.Equals(other.Values);
    }
    
    public override int GetHashCode()
    {
        return 0;
    }
    
    public override string ToString()
    {
        var sb = new StringBuilder();
        sb.Append("EventTime=").Append(TimeToString(EventTimeMs));
        sb.Append(" SensorId=").Append(SensorId);
        var idx = 0;
        foreach (var vt in ValueTypes)
        {
            if (vt == 0)
                break;
            sb.Append(' ').Append(SmartHomeService.ValueTypes.ReverseMap[vt]).Append('=').Append(Values[idx++]);
        }
        return sb.ToString();
    }

    internal static string TimeToString(uint timeMs)
    {
        var t = new TimeOnly(timeMs * TimeSpan.TicksPerMillisecond);
        return t.ToString("HH:mm:ss.fff");
    }
}

public abstract class SensorEvents<T>
{
    public readonly List<SensorDataFileItem<T>> Items;

    protected SensorEvents()
    {
        Items = [];
    }

    protected SensorEvents(List<SensorDataFileItem<T>> items)
    {
        Items = items;
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

    public RawSensorEvents(List<SensorDataFileItem<int>> items): base(items)
    {
    }
    
    public RawSensorEvents(byte[] data): base(data)
    {
    }
    
    internal static IEnumerable<(string, int, SensorDataItem)> ToSensorData(int date, SensorDataFileItem<int> item, int locationId, TimeZoneInfo timeZone)
    {
        var idx = 0;
        foreach (var vt in item.ValueTypes)
        {
            yield return (ValueTypes.ReverseMap[vt], locationId,
                new SensorDataItem(FileSmartHomeService.BuildTimestamp(date, item.EventTimeMs, timeZone), (double)item.Values[idx++] / 100));
        }
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

    internal AggregatedSensorEvents(Dictionary<uint, Dictionary<byte, AggregatedValuesL>> data): base(Convert(data))
    {
    }
    
    private static List<SensorDataFileItem<AggregatedValues>> Convert(Dictionary<uint, Dictionary<byte, AggregatedValuesL>> data)
    {
        return data
            .Select(kv => new SensorDataFileItem<AggregatedValues>(kv.Key,
                kv.Value.Select(v => new ValueTypeValue<AggregatedValues>(v.Key, v.Value.ToAggregatedValues())).ToList()))
            .ToList();
    }

    internal static IEnumerable<AggregatedFileSensorDataItem> ToSensorData(int date, SensorDataFileItem<AggregatedValues> item, int locationId, TimeZoneInfo timeZone)
    {
        var idx = 0;
        foreach (var vt in item.ValueTypes)
        {
            yield return (new AggregatedFileSensorDataItem(ValueTypes.ReverseMap[vt], locationId,
                FileSmartHomeService.BuildTimestamp(date, 0, timeZone), item.Values[idx++]));
        }
    }
}

internal record AggregatedFileSensorDataItem(string ValueType, int LocationId, SensorDataItem Min, SensorDataItem Avg, SensorDataItem Max)
{
    public AggregatedFileSensorDataItem(string valueType, int locationId, long timestamp, AggregatedValues itemValue):
        this(valueType, locationId, new SensorDataItem(timestamp + itemValue.Min.Time, (double)itemValue.Min.Value / 100),
            new SensorDataItem(timestamp, (double)itemValue.Avg / 100), 
            new SensorDataItem(timestamp + itemValue.Max.Time, (double)itemValue.Max.Value / 100))
    {
    }
}

public record struct AggregatedValue(uint Time, int Value)
{
    public override string ToString()
    {
        return SensorDataFileItem<int>.TimeToString(Time * 10) + "," + Value;
    }
}

public record struct AggregatedValues(AggregatedValue Min, int Avg, AggregatedValue Max)
{
    public override string ToString()
    {
        return "[min=" + Min + " avg=" + Avg + " max=" + Max + "]";
    }
}

internal class AggregatedValuesL
{
    private AggregatedValue _min;
    private AggregatedValue _max;
    private long _sum;
    private long _cnt;

    internal AggregatedValuesL(uint time, int value)
    {
        _min = new AggregatedValue(time, value);
        _max = new AggregatedValue(time, value);
        _sum = value;
        _cnt = 1;
    }

    public AggregatedValuesL(AggregatedValues values, uint offset)
    {
        _min = values.Min with { Time = values.Min.Time + offset };
        _max = values.Max with { Time = values.Max.Time + offset };
        _sum = values.Avg;
        _cnt = 1;
    }

    internal void ProcessValue(uint time, int value)
    {
        if (_min.Value > value)
            _min = new AggregatedValue(time, value);
        else if (_max.Value < value)
            _max = new AggregatedValue(time, value);
        _sum += value;
        _cnt++;
    }
    
    internal AggregatedValues ToAggregatedValues()
    {
        return new AggregatedValues(_min, (int)(_sum / _cnt), _max);
    }

    public void ProcessValue(AggregatedValues values, uint offset)
    {
        if (_min.Value > values.Min.Value)
            _min = values.Min with { Time = values.Min.Time + offset };
        else if (_max.Value < values.Max.Value)
            _max = values.Max with { Time = values.Max.Time + offset };
        _sum += values.Avg;
        _cnt++;
    }
}

public record FileSmartHomeServiceConfiguration(
    string BaseFolder, int KeyDivider, string SensorsFile, 
    string LocationsFile, string TimeZone, string ValueTypesFile);

public sealed class FileSmartHomeService: BaseSmartHomeService
{
    private record DateRange(int MaxPoints, bool Aggregated, int StartDate, uint StartTime, int EndDate, uint EndTime);

    public const string RawFileExtension = ".raw";
    public const string AggregatedFileExtension = ".aggregated";
    public const string YearlyFileExtension = ".yearly";
    
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
        foreach (var fileData in ReadFiles(_baseFolder, _keyDivider, from, null, RawFileExtension, true))
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
                    var di = new SensorDataItem(BuildTimestamp(fileData.Key, item.EventTimeMs, TimeZone),
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

    internal static long BuildTimestamp(int date, uint timeMs, TimeZoneInfo timeZone)
    {
        var t = (int)(timeMs / 1000);
        var ms = (int)(timeMs % 1000);
        var hour = t / 3600;
        var minute = (t / 60) % 60;
        var second = t % 60;
        var dt = new DateTime(date / 10000, (date / 100) % 100, date % 100, hour, minute, second, ms, DateTimeKind.Unspecified);
        return new DateTimeOffset(TimeZoneInfo.ConvertTime(dt, timeZone).ToUniversalTime()).ToUnixTimeMilliseconds();
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
        var data = ReadFiles(
            _baseFolder,
            _keyDivider,
            dateRange.StartDate,
            sensorDataQuery.Period == null ? null : dateRange.EndDate,
            aggregated ? AggregatedFileExtension : RawFileExtension,
            false);
        return dateRange.Aggregated ? GetAggregatedSensorData(sensors, dateRange.MaxPoints, data) : GetRawSensorData(sensors, dateRange, data); 
    }

    public override YearlySensorDataResult GetYearlySensorData()
    {
        throw new NotImplementedException();
    }

    private SensorDataResult GetRawSensorData(HashSet<uint> sensors, DateRange dateRange, IEnumerable<FileData> data)
    {
        var d = data.SelectMany(dt => new RawSensorEvents(dt.Data).Items.Select(item => (dt.Key, item)))
            .Where(di =>
                sensors.Contains(di.item.SensorId)
                && (di.Key > dateRange.StartDate
                 || (di.Key == dateRange.StartDate && di.item.EventTime >= dateRange.StartTime))
                && (di.Key < dateRange.EndDate
                    || (di.Key == dateRange.EndDate && di.item.EventTime <= dateRange.EndTime)))
            .SelectMany(di => RawSensorEvents.ToSensorData(di.Key, di.item, Sensors[di.item.SensorId].LocationId, TimeZone))
            // valueType, locationId, sensorDataItem
            .GroupBy(li => li.Item1)
            // grouped by valueType
            .Select(grp => (grp.Key, ToItemList(grp.GroupBy(item => item.Item2), dateRange.MaxPoints)))
            .ToDictionary();
        return new SensorDataResult(d); 
    }

    private static List<SensorData> ToItemList(IEnumerable<IGrouping<int, (string, int, SensorDataItem)>> items, int maxPoints)
    {
        return items.Select(item => new SensorData(item.Key, Aggregate(item.Select(i => i.Item3).ToList(), maxPoints), null)).ToList();
    }

    private static List<SensorDataItem> Aggregate(List<SensorDataItem> list, int maxPoints)
    {
        if (list.Count <= maxPoints)
            return list;
        var coef = maxPoints / list.Count + (maxPoints % list.Count == 0 ? 0 : 1);
        var result = new List<SensorDataItem>();
        for (var i = 0; i < list.Count; i += coef)
        {
            var t = list[i].Timestamp;
            var sum = 0.0;
            var cnt = 0;
            for (var k = 0; k < coef; k++)
            {
                var idx = i + k;
                if (idx >= list.Count)
                    break;
                sum += list[idx].Value;
                cnt++;
            }
            result.Add(new SensorDataItem(t, sum / cnt));
        }
        return result;
    }

    private static List<SensorData> ToItemList(IEnumerable<IGrouping<int, AggregatedFileSensorDataItem>> items, int maxPoints)
    {
        return items.Select(item => new SensorData(item.Key, null, 
            new AggregatedSensorData(
                Aggregate(item.Select(i => i.Min).ToList(), maxPoints).ToList(),
                Aggregate(item.Select(i => i.Avg).ToList(), maxPoints).ToList(),
                Aggregate(item.Select(i => i.Max).ToList(), maxPoints).ToList()
                ))).ToList();
    }
    
    private SensorDataResult GetAggregatedSensorData(HashSet<uint> sensors, int maxPoints, IEnumerable<FileData> data)
    {
        var d = data.SelectMany(dt => new AggregatedSensorEvents(dt.Data).Items.Select(item => (dt.Key, item)))
            .Where(di => sensors.Contains(di.item.SensorId))
            .SelectMany(di => AggregatedSensorEvents.ToSensorData(di.Key, di.item, Sensors[di.item.SensorId].LocationId, TimeZone))
            .GroupBy(vti => vti.ValueType)
            .Select(grp => (grp.Key, ToItemList(grp.GroupBy(item => item.LocationId), maxPoints)))
            .ToDictionary();
        return new SensorDataResult(d); 
    }
    
    private DateRange BuildDateRange(SmartHomeQuery query)
    {
        var range = BuildBaseDateRange(query);
        var aggregated = (range.EndDateTime - range.StartDateTime).TotalMinutes / 5 > range.MaxPoints;
        var (startDate, startTime) = SplitDate(range.StartDateTime);
        var (endDate, endTime) = SplitDate(range.EndDateTime);
        return new DateRange(range.MaxPoints, aggregated, startDate, startTime, endDate, endTime);
    }

    private static (int startDate, uint startTime) SplitDate(DateTime dateTime)
    {
        var date = dateTime.Year * 10000 + dateTime.Month * 100 + dateTime.Day;
        var time = dateTime.TimeOfDay.TotalMilliseconds / 10;
        return (date, (uint)time);
    }

    private static IEnumerable<FileData> ReadFiles(string baseFolder, int keyDivider, int from, int? to,
        string extension, bool reverse)
    {
        var fromYear = from / keyDivider;
        var toYear = to / keyDivider;
        return Directory.EnumerateDirectories(baseFolder)
            .Select(path => int.Parse(Path.GetFileName(path)))
            .Where(year => year >= fromYear && (toYear == null || year <= toYear))
            .SelectMany(year => Directory.EnumerateFiles(Path.Combine(baseFolder, year.ToString()), "*" + extension))
            .Select(name => (int.Parse(Path.GetFileNameWithoutExtension(name)), name))
            .Where(pair => pair.Item1 >= from && (to == null || pair.Item1 <= to))
            .OrderBy(pair => reverse ? -pair.Item1 : pair.Item1)
            .Select(pair => new FileData(pair.Item1, File.ReadAllBytes(pair.name)));
    }
    
    public static void AggregateRawData(int from, string baseFolder, int keyDivider)
    {
        Parallel.ForEach(ReadFiles(baseFolder, keyDivider, from, null,
            RawFileExtension, false), fileData =>
        {
            var items = new RawSensorEvents(fileData.Data);
            if (items.Items.Count == 0)
                return;
            var aggregated = AggregateRawData(items);
            var data = aggregated.ToBinary();
            var fileName = BuildFileName(fileData.Key, baseFolder, keyDivider, AggregatedFileExtension);
            File.WriteAllBytes(fileName, data);
        });
    }

    public static void AggregateYearly(int startYear, string baseFolder, int keyDivider)
    {
        var result = new Dictionary<int, Dictionary<uint, Dictionary<byte, AggregatedValuesL>>>();
        foreach (var fileData in ReadFiles(baseFolder, keyDivider, startYear * 10000 + 0101, null,
            AggregatedFileExtension, false))
        {
            var items = new AggregatedSensorEvents(fileData.Data);
            if (items.Items.Count == 0)
                continue;
            var itemYear = fileData.Key / keyDivider;
            if (!result.TryGetValue(itemYear, out var yearResult))
            {
                yearResult = new Dictionary<uint, Dictionary<byte, AggregatedValuesL>>();
                result[itemYear] = yearResult;
            }
            var offset = BuildOffset(fileData.Key);
            foreach (var item in items.Items)
            {
                if (!yearResult.TryGetValue(item.SensorId, out var sensorResult))
                {
                    sensorResult = new Dictionary<byte, AggregatedValuesL>();
                    yearResult[item.SensorId] = sensorResult;
                }
                var idx = 0;
                foreach (var vt in item.ValueTypes)
                {
                    if (vt == 0)
                        break;
                    var values = item.Values[idx++];
                    if (!sensorResult.TryGetValue(vt, out var vtResult))
                        sensorResult[vt] = new AggregatedValuesL(values, offset);
                    else
                        vtResult.ProcessValue(values, offset);
                }
            }
        }

        foreach (var kv in result)
        {
            var data = new AggregatedSensorEvents(kv.Value).ToBinary();
            var fileName = Path.Combine(baseFolder, kv.Key + YearlyFileExtension);
            File.WriteAllBytes(fileName, data);
        }
    }

    private static uint BuildOffset(int date)
    {
        var year = date / 10000;
        var month = (date / 100) % 100;
        var day = date % 100;
        var d1 = new DateTime(year, 1, 1);
        var d2 = new DateTime(year, month, day);
        return (uint)((d2 - d1).TotalMilliseconds / 10);
    }

    private static string BuildFileName(int key, string baseFolder, int keyDivider, string extension)
    {
        return Path.Combine(baseFolder, (key / keyDivider).ToString(), key.ToString() + extension);
    }
    
    private static AggregatedSensorEvents AggregateRawData(RawSensorEvents rawData)
    {
        var aggregated = new Dictionary<uint, Dictionary<byte, AggregatedValuesL>>();
        foreach (var e in rawData.Items)
        {
            if (!aggregated.TryGetValue(e.SensorId, out var sensorData))
            {
                sensorData = new Dictionary<byte, AggregatedValuesL>();
                aggregated[e.SensorId] = sensorData;
            }
            var idx = 0;
            foreach (var vt in e.ValueTypes)
            {
                if (vt == 0)
                    break;
                var value = e.Values[idx++];
                if (sensorData.TryGetValue(vt, out var aValue))
                    aValue.ProcessValue(e.TimeAndSensorId >> 8, value);
                else
                    sensorData[vt] = new AggregatedValuesL(e.TimeAndSensorId >> 8, value);
            }
        }
        return new AggregatedSensorEvents(aggregated);
    }
}

public record FileData(int Key, byte[] Data);