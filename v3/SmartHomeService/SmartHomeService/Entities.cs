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

public record Location(string Name, string LocationType)
{
    public void Save(BinaryWriter writer)
    {
        LastSensorData.SaveString(writer, Name);
        LastSensorData.SaveFixedSizeString(writer, LocationType, 3, "Location type");
    }
}

internal record Sensor(string Name, string DataType, int LocationId, int? DeviceId,
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
    Locations GetLocations();
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
    
    internal LastSensorData(Dictionary<int, Dictionary<string, SensorDataItem>> data)
    {
        Data = data;
    }

    internal static void SaveFixedSizeString(BinaryWriter writer, string s, int expectedSize, string name)
    {
        var bytes = Encoding.UTF8.GetBytes(s);
        if (bytes.Length != expectedSize)
            throw new InvalidDataException($"{name} must be {expectedSize} bytes");
        writer.Write(bytes);
    }

    internal static void SaveString(BinaryWriter writer, string s)
    {
        var bytes = Encoding.UTF8.GetBytes(s);
        writer.Write((byte)bytes.Length);
        writer.Write(bytes);
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
                SaveFixedSizeString(writer, valueType, 4, "Value type");
                lastData.Save(writer);
            }           
        }
        return stream.ToArray();
    }
}

public sealed class SensorDataResult
{
    // map valueType -> sensor data list
    public readonly Dictionary<string, List<SensorData>> Data;
    
    internal SensorDataResult(Dictionary<string, List<SensorData>> data)
    {
        Data = data;
    }

    public byte[] ToBinary()
    {
        using var stream = new MemoryStream();
        using var writer = new BinaryWriter(stream);
        foreach (var (valueType, data) in Data)
        {
            LastSensorData.SaveFixedSizeString(writer, valueType, 4, "Value type");
            writer.Write((byte)data.Count);
            foreach (var sdata in data)
                sdata.Save(writer);
        }
        return stream.ToArray();
    }
}

public record LocationAndSensors(Location Location, int[] Sensors)
{
    public void Save(BinaryWriter writer)
    {
        Location.Save(writer);
        writer.Write((byte)Sensors.Length);
        foreach (var sensorId in Sensors)
            writer.Write((byte)sensorId);
    }
}

public sealed class Locations
{
    public readonly Dictionary<int, LocationAndSensors> LocationMap;
    public readonly string TimeZone;
    
    internal Locations(Dictionary<int, LocationAndSensors> locations, string timeZone)
    {
        TimeZone = timeZone;
        LocationMap = locations;
    }
    
    public byte[] ToBinary()
    {
        using var stream = new MemoryStream();
        using var writer = new BinaryWriter(stream);
        LastSensorData.SaveString(writer, TimeZone);
        foreach (var (locationId, location) in LocationMap)
        {
            writer.Write((byte)locationId);
            location.Save(writer);
        }
        return stream.ToArray();
    }
}