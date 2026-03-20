using System.Text;
using ICSharpCode.SharpZipLib.BZip2;

namespace SmartHomeService;

public record SensorDataItem(long Timestamp, double Value)
{
    internal void Save(BinaryWriter writer)
    {
        writer.Write((int)(Timestamp / 1000));
        writer.Write((int)(Value * 100));
    }
}

public record AggregatedSensorData(List<SensorDataItem> Min, List<SensorDataItem> Avg, List<SensorDataItem> Max);
public record SensorData(int SensorId, List<SensorDataItem>? Raw, AggregatedSensorData? Aggregated)
{
    private static void WriteSensorDataItemList(BinaryWriter writer, List<SensorDataItem> list)
    {
        writer.Write((short)list.Count);
        foreach (var item in list)
            item.Save(writer);
    }
    
    internal void Save(BinaryWriter writer)
    {
        writer.Write((byte)SensorId);
        writer.Write(Raw != null ? (byte)0 : (byte)1);
        if (Raw != null)
            WriteSensorDataItemList(writer, Raw);
        else
        {
            WriteSensorDataItemList(writer, Aggregated!.Min);
            WriteSensorDataItemList(writer, Aggregated!.Avg);
            WriteSensorDataItemList(writer, Aggregated!.Max);
        }
    }
}

public record SensorDataItemWithDate(DateTime Dt, double Value)
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

public interface ISmartHomeService
{
    string GetLocationNameBySensorId(int sensorId);
    string GetLocationName(int locationId);
    bool IsExtSensor(int sensorId);
    DateTime GetDateTime(long timestamp);
    LastSensorData GetLastSensorData();
    SensorDataResult GetSensorData(SmartHomeQuery sensorDataQuery);
    double ResponseTimeMs { get; }
    SensorDataItemWithDate BuildSensorDataItemWithDate(SensorDataItem data);
    string GetValueType(string valueType);
}

public static class Compressor
{
    public static byte[] Compress(byte[] data)
    {
        var stream = new MemoryStream(data);
        using var outStream = new MemoryStream();
        BZip2.Compress(stream, outStream, true, 9);
        return outStream.ToArray();
    }
}

public sealed class LastSensorData
{
    // map location id to map valueType to sensor data
    public readonly Dictionary<int, Dictionary<string, SensorDataItem>> Data;
    
    public LastSensorData(Dictionary<int, Dictionary<string, SensorDataItem>> data)
    {
        Data = data;
    }

    internal static void SaveValueType(BinaryWriter writer, string valueType)
    {
        var valueTypeBytes = Encoding.UTF8.GetBytes(valueType);
        if (valueTypeBytes.Length != 4)
            throw new InvalidDataException("Value type must be 4 bytes");
        writer.Write(valueTypeBytes);
    }

    public byte[] ToBinary()
    {
        using var stream = new MemoryStream();
        using var writer = new BinaryWriter(stream);
        foreach (var (locationId, lastDataByLocation) in Data)
        {
            writer.Write((byte)locationId);
            writer.Write((byte)lastDataByLocation.Count);
            foreach (var (valueType, lastData) in lastDataByLocation)
            {
                SaveValueType(writer, valueType);
                lastData.Save(writer);
            }           
        }
        return stream.ToArray();
    }
}

public sealed class SensorDataResult
{
    public readonly Dictionary<string, List<SensorData>> Data;
    
    public SensorDataResult(Dictionary<string, List<SensorData>> data)
    {
        Data = data;
    }

    public byte[] ToBinary()
    {
        using var stream = new MemoryStream();
        using var writer = new BinaryWriter(stream);
        foreach (var (valueType, data) in Data)
        {
            LastSensorData.SaveValueType(writer, valueType);
            writer.Write((byte)data.Count);
            foreach (var sdata in data)
                sdata.Save(writer);
        }
        return stream.ToArray();
    }
}