using System.Text;

namespace BluetoothClient;

[System.Runtime.CompilerServices.InlineArray(8)]
public struct ValueArray
{
    private int _element0;
}

public record struct SensorEvent(long TimeAndSensorId, ValueArray ValuesAndSensorIds)
{
    public uint SensorId => (uint)(TimeAndSensorId & 0xFF);
    public long EventTimeMs => TimeAndSensorId >> 8;
    
    public override string ToString()
    {
        var dateTime = DateTimeOffset.FromUnixTimeMilliseconds(EventTimeMs);
        var sb = new StringBuilder();
        sb.Append("Timestamp: ").Append(EventTimeMs).Append(" Date: ")
            .Append(dateTime).Append(" SensorId: ").Append(SensorId);
        for (var i = 0; i < 8; i++)
        {
            var valueAndSensorId = ValuesAndSensorIds[i];
            if (valueAndSensorId == 0)
                break;
            sb.Append(" Value").Append(i).Append(": ").Append(GetValue(i)).Append(" ValueType").Append(i)
                .Append(": ").Append(GetSensorId(i));
        }
        return sb.ToString();
    }

    public int GetValue(int i)
    {
        var v = ValuesAndSensorIds[i] & 0xFFFFFF;
        if ((v & 0x800000) != 0)
            v = (int)(v | 0xFF000000);
        return v;
    }
    
    public uint GetSensorId(int i) => (uint)(ValuesAndSensorIds[i] >> 24);
}
