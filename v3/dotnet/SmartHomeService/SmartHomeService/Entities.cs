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

    public static SensorDataItem ParseResponse(BinaryReader reader)
    {
        var timestamp = (long)reader.ReadInt32() * 1000;
        var value = (double)reader.ReadInt32() / 100;
        return new SensorDataItem(timestamp, value);
    }
}

public record AggregatedSensorData(List<SensorDataItem> Min, List<SensorDataItem> Avg, List<SensorDataItem> Max)
{
    public static AggregatedSensorData ParseResponse(BinaryReader reader)
    {
        var min = SensorData.ParseSensorDataItemList(reader);
        var avg = SensorData.ParseSensorDataItemList(reader);
        var max = SensorData.ParseSensorDataItemList(reader);
        return new AggregatedSensorData(min, avg, max);
    }

    public void Save(BinaryWriter writer)
    {
        SensorData.WriteSensorDataItemList(writer, Min);
        SensorData.WriteSensorDataItemList(writer, Avg);
        SensorData.WriteSensorDataItemList(writer, Max);
    }
}

public record SensorData(int LocationId, List<SensorDataItem>? Raw, AggregatedSensorData? Aggregated)
{
    internal static void WriteSensorDataItemList(BinaryWriter writer, List<SensorDataItem> list)
    {
        writer.Write((short)list.Count);
        foreach (var item in list)
            item.Save(writer);
    }
    
    internal static List<SensorDataItem> ParseSensorDataItemList(BinaryReader reader)
    {
        var length = reader.ReadInt16();
        var list = new List<SensorDataItem>();
        while (length-- > 0)
            list.Add(SensorDataItem.ParseResponse(reader));
        return list;
    }
    
    internal void Save(BinaryWriter writer)
    {
        writer.Write((byte)LocationId);
        if (Raw != null)
            WriteSensorDataItemList(writer, Raw);
        else
            Aggregated!.Save(writer);
    }

    public static SensorData ParseResponse(BinaryReader reader, bool aggregated)
    {
        var locationId = (int)reader.ReadByte();
        if (aggregated)
            return new SensorData(locationId, null, AggregatedSensorData.ParseResponse(reader));
        return new SensorData(locationId, ParseSensorDataItemList(reader), null);
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
    internal DateTime CalculateDate(DateTime dateTime)
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
    DateOffset? Period)
{
    public static SmartHomeQuery FromBinary(ISmartHomeService service, byte[] data)
    {
        if (data.Length != 11)
            throw new Exception("SmartHomeQuery: Invalid data length");
        var maxPoints = BitConverter.ToInt16(data, 0);
        var dataType = Encoding.UTF8.GetString(data[2..5]);
        var dateOrOffset = BitConverter.ToInt32(data, 5);
        var periodValue = (int)data[9];
        var period = periodValue > 0 ? new DateOffset(periodValue, (TimeUnit)data[10]) : null;
        if (dateOrOffset < 0)
        {
            var offsetValue = dateOrOffset >> 8;
            var offsetUnit = (TimeUnit)(dateOrOffset & 0xFF);
            return new SmartHomeQuery(maxPoints, dataType, null, new DateOffset(offsetValue, offsetUnit), period);
        }
        return new SmartHomeQuery(maxPoints, dataType, service.BuildDate(dateOrOffset), null, period);
    }
    
    public byte[] ToBinary()
    {
        var dataTypeBytes = Encoding.UTF8.GetBytes(DataType);
        if (dataTypeBytes.Length != 3)
            throw new ArgumentException("wrong DataType length");
        var result = new byte[11];
        BitConverter.GetBytes(MaxPoints).CopyTo(result, 0);
        dataTypeBytes.CopyTo(result, 2);
        int date;
        if (StartDate != null)
            date = StartDate.Value.Year * 10000 + StartDate.Value.Month * 100 + StartDate.Value.Day;
        else
            date = (-StartDateOffset!.Offset << 8) | (int)StartDateOffset!.Unit;
        BitConverter.GetBytes(date).CopyTo(result, 5);
        if (Period != null)
        {
            result[9] = (byte)Period.Offset;
            result[10] = (byte)Period.Unit;
        }
        return result;
    }
}

public record Location(string Name, string LocationType)
{
    public void Save(BinaryWriter writer)
    {
        LastSensorData.SaveString(writer, Name);
        LastSensorData.SaveFixedSizeString(writer, LocationType, 3, "Location type");
    }

    public static Location ParseResponse(MemoryStream response)
    {
        var name = LastSensorData.LoadString(response);
        var locationType = LastSensorData.LoadFixedSizeString(response, 3);
        return new Location(name, locationType);
    }
}

public record Sensor(string Name, string DataType, int LocationId, int? DeviceId,
    Dictionary<int, string> DeviceSensors, Dictionary<string, double> Offsets, bool Enabled);

public interface ISmartHomeService
{
    string GetLocationName(int locationId);
    bool IsExtLocation(int locationId);
    DateTime GetDateTime(long timestamp);
    LastSensorData GetLastSensorData();
    SensorDataResult GetSensorData(SmartHomeQuery sensorDataQuery, out bool aggregated);
    double ResponseTimeMs { get; }
    SensorDataItemWithDate BuildSensorDataItemWithDate(SensorDataItem data);
    string GetValueType(string valueType);
    Locations GetLocations();
    DateTime BuildDate(int date);
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

public sealed class YearlySensorDataResult
{
    // map year to location id to map valueType to sensor data
    public readonly Dictionary<int, LastSensorData> Data;

    private YearlySensorDataResult(Dictionary<int, LastSensorData> data)
    {
        Data = data;
    }
    
    public byte[] ToBinary()
    {
        using var stream = new MemoryStream();
        using var writer = new BinaryWriter(stream);
        foreach (var kv in Data)
        {
            writer.Write((short)kv.Key);
            kv.Value.ToBinary(writer);
        }
        return stream.ToArray();
    }

    public static YearlySensorDataResult ParseResponse(MemoryStream response)
    {
        var data = new Dictionary<int, LastSensorData>();
        using var reader = new BinaryReader(response);
        while (response.Position < response.Length)
        {
            var year = (int)reader.ReadInt16();
            var sd = LastSensorData.ParseResponse(reader);
            data.Add(year, sd);
        }
        return new YearlySensorDataResult(data);
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

    internal static string LoadString(MemoryStream stream)
    {
        var length = stream.ReadByte();
        return LoadFixedSizeString(stream, length);
    }

    internal static string LoadFixedSizeString(MemoryStream stream, int length)
    {
        var bytes = new byte[length];
        stream.ReadExactly(bytes);
        return Encoding.UTF8.GetString(bytes);
    }

    internal static string LoadFixedSizeString(BinaryReader reader, int length)
    {
        return Encoding.UTF8.GetString(reader.ReadBytes(length));
    }

    public byte[] ToBinary()
    {
        using var stream = new MemoryStream();
        using var writer = new BinaryWriter(stream);
        ToBinary(writer);
        return stream.ToArray();
    }

    public void ToBinary(BinaryWriter writer)
    {
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
    }

    public static LastSensorData ParseResponse(MemoryStream response)
    {
        using var reader = new BinaryReader(response);
        return ParseResponse(reader);
    }

    public static LastSensorData ParseResponse(BinaryReader reader)
    {
        var result = new Dictionary<int, Dictionary<string, SensorDataItem>>();
        var count = reader.ReadByte();
        while (count-- > 0)
        {
            var locationId = (int)reader.ReadByte();
            var length = reader.ReadByte();
            var map = new Dictionary<string, SensorDataItem>();
            while (length-- > 0)
            {
                var valueType = LoadFixedSizeString(reader, 4);
                var lastData = SensorDataItem.ParseResponse(reader);
                map[valueType] = lastData;
            }
            result[locationId] = map;
        }
        return new LastSensorData(result);
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

    public byte[] ToBinary(bool aggregated)
    {
        using var stream = new MemoryStream();
        using var writer = new BinaryWriter(stream);
        writer.Write(aggregated ? (byte)1 : (byte)0);
        foreach (var (valueType, data) in Data)
        {
            LastSensorData.SaveFixedSizeString(writer, valueType, 4, "Value type");
            writer.Write((byte)data.Count);
            foreach (var sdata in data)
                sdata.Save(writer);
        }
        return stream.ToArray();
    }

    public static SensorDataResult ParseResponse(MemoryStream response)
    {
        using var reader = new BinaryReader(response);
        var result = new Dictionary<string, List<SensorData>>();
        var aggregated = response.ReadByte() != 0;
        while (response.Position < response.Length)
        {
            var valueType = LastSensorData.LoadFixedSizeString(reader, 4);
            var length = reader.ReadByte();
            var list = new List<SensorData>();
            while (length-- > 0)
                list.Add(SensorData.ParseResponse(reader, aggregated));
            result[valueType] = list;
        }
        return new SensorDataResult(result);
    }
}

public sealed class Locations
{
    public readonly Dictionary<int, Location> LocationMap;
    public readonly string TimeZone;
    
    internal Locations(Dictionary<int, Location> locations, string timeZone)
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

    public static Locations ParseResponse(MemoryStream response)
    {
        var timeZone = LastSensorData.LoadString(response);
        var locations = new Dictionary<int, Location>();
        while (response.Position < response.Length)
        {
            var locationId = response.ReadByte();
            var location = Location.ParseResponse(response);
            locations[locationId] = location;
        }

        return new Locations(locations, timeZone);
    }
}