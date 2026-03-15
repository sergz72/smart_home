using System.Text;

namespace SmartHomeServiceClientLibrary;

public record Sensor(int Id, string DataType, string Location, string LocationType)
{
    internal static List<Sensor> ParseResponse(MemoryStream response)
    {
        using var reader = new BinaryReader(response);
        var result = new List<Sensor>();
        var length = reader.ReadByte();
        while (length-- > 0)
        {
            var id = (int)reader.ReadByte();
            var dataType = System.Text.Encoding.UTF8.GetString(reader.ReadBytes(3));
            var locationLength = reader.ReadByte();
            var location = System.Text.Encoding.UTF8.GetString(reader.ReadBytes(locationLength));
            var locationType = System.Text.Encoding.UTF8.GetString(reader.ReadBytes(3));
            result.Add(new Sensor(id, dataType, location, locationType));
        }
        return result;
    }
}
        
public record SensorDataValues(int Time, Dictionary<string, int> Values)
{
    internal static SensorDataValues FromBinary(BinaryReader reader)
    {
        var map = new Dictionary<string, int>();
        var valuesCount = reader.ReadByte();
        var time = reader.ReadInt32();
        while (valuesCount-- > 0)
        {
            var key = Encoding.UTF8.GetString(reader.ReadBytes(4));
            var value = reader.ReadInt32();
            map[key] = value;
        }
        return new SensorDataValues(time, map);
    }
    
    public override string ToString()
    {
        var sb = new StringBuilder();
        sb.Append(Time);
        sb.Append(" {");
        var first = true;
        foreach (var (key, value) in Values)
        {
            if (first)
                first = false;
            else
                sb.Append(", ");
            sb.Append(key).Append('=').Append(value);
        }
        sb.Append('}');
        return sb.ToString();
    }
}

public record LastSensorData(int Date, SensorDataValues Values)
{
    internal static Dictionary<int, LastSensorData> ParseResponse(MemoryStream response)
    {
        using var reader = new BinaryReader(response);
        var length = reader.ReadByte();
        var result = new Dictionary<int, LastSensorData>();
        while (length-- > 0)
        {
            var sensorId = reader.ReadByte();
            var date = reader.ReadInt32();
            var values = SensorDataValues.FromBinary(reader);
            result[sensorId] = new LastSensorData(date, values);
        }
        return result;
    }
}

public enum TimeUnit {
    Day,
    Month,
    Year
}

public record DateOffset (int Offset, TimeUnit Unit);

public record SmartHomeQuery(
    short MaxPoints,
    string DataType,
    int? StartDate,
    DateOffset? StartDateOffset,
    DateOffset? Period)
{
    internal byte[] ToBinary()
    {
        var dataTypeBytes = System.Text.Encoding.UTF8.GetBytes(DataType);
        if (dataTypeBytes.Length != 3)
            throw new ArgumentException("wrong DataType length");
        var result = new byte[12];
        result[0] = 2;
        BitConverter.GetBytes(MaxPoints).CopyTo(result, 1);
        dataTypeBytes.CopyTo(result, 3);
        if (StartDate != null)
            BitConverter.GetBytes((int)StartDate).CopyTo(result, 6);
        else
        {
            result[6] = (byte)StartDateOffset!.Unit;
            result[7] = (byte)StartDateOffset.Offset;
        }
        if (Period != null)
        {
            result[10] = (byte)Period.Offset;
            result[11] = (byte)Period.Unit;
        }
        return result;
    }
}

public record AggregatedSensorDataValues(int Min, int Avg, int Max);

public record SensorData(
    int Date,
    List<SensorDataValues>? Values,
    Dictionary<string, AggregatedSensorDataValues>? Aggregated)
{
    internal static SensorData BuildFromAggregatedResponse(BinaryReader reader)
    {
        var date = reader.ReadInt32();
        var map = new Dictionary<string, AggregatedSensorDataValues>();
        var valuesCount = reader.ReadByte();
        while (valuesCount-- > 0) {
            var key = System.Text.Encoding.UTF8.GetString(reader.ReadBytes(4));
            var min = reader.ReadInt32();
            var avg = reader.ReadInt32();
            var max = reader.ReadInt32();
            map[key] = new AggregatedSensorDataValues(min, avg, max);
        }
        return new SensorData(date, null, map);
    }
    
    internal static SensorData BuildFromResponse(BinaryReader reader)
    {
        var date = reader.ReadInt32();
        var dataCount = reader.ReadInt32();
        var list = new List<SensorDataValues>();
        while (dataCount-- > 0) {
            list.Add(SensorDataValues.FromBinary(reader));
        }
        return new SensorData(date, list, null);
    }
}

public record SensorDataResponse(bool Aggregated, Dictionary<int, List<SensorData>> Data)
{
    internal static SensorDataResponse ParseResponse(MemoryStream response, bool aggregated, Func<BinaryReader, 
        SensorData> sensorDataParser)
    {
        using var reader = new BinaryReader(response);
        var data = new Dictionary<int, List<SensorData>>();
        while (response.Position < response.Length)
        {
            var sensorId = reader.ReadByte();
            var length = reader.ReadInt16();
            var list = new List<SensorData>();
            while (length-- > 0) {
                var sdata = sensorDataParser(reader);
                list.Add(sdata);
            }
            data[sensorId] = list;
        }
        return new SensorDataResponse(aggregated, data);
    }
}
