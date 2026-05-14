using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text.Json;
using Linux.Bluetooth;
using Linux.Bluetooth.Extensions;

namespace SmartHomeService;

internal enum DeviceValueType
{
    ValueTypeCo2 = 1,
    ValueTypeHumiInt = 2,
    ValueTypeIcc = 3,
    ValueTypeLuxInt = 4,
    ValueTypePres = 5,
    ValueTypePwr = 6,
    ValueTypeTempInt = 7,
    ValueTypeVbat = 8,
    ValueTypeVcc = 9,
    ValueTypeHumiExt = 10,
    ValueTypeLuxExt = 11,
    ValueTypeTempExt = 12,
    ValueTypeHumiExt2 = 13,
    ValueTypeLuxExt2 = 14,
    ValueTypeTempExt2 = 15
}

internal record BluetoothSmartHomeServiceConfiguration(
    string DeviceName = "",
    int Timeout = 5,
    string ServiceUuid = "",
    string CharacteristicUuid = "",
    string SensorsFile = "",
    string LocationsFile = "",
    string TimeZone = "",
    string ValueTypesFile = "",
    string SaveFileName = "");

internal record struct SensorEvent(long TimeAndSensorId, ValueArray<int> ValuesAndValueTypes)
{
    internal uint SensorId => (uint)(TimeAndSensorId & 0xFF);
    internal long EventTimeMs => TimeAndSensorId >> 8;

    internal int GetValue(int i)
    {
        var v = ValuesAndValueTypes[i] & 0xFFFFFF;
        if ((v & 0x800000) != 0)
            v = (int)(v | 0xFF000000);
        return v;
    }

    internal string GetValueType(int i)
    {
        var valueTypeId = (DeviceValueType)(ValuesAndValueTypes[i] >> 24);
        switch (valueTypeId)
        {
            case DeviceValueType.ValueTypeHumiExt:
            case DeviceValueType.ValueTypeHumiExt2:
                valueTypeId = DeviceValueType.ValueTypeHumiInt;
                break;
            case DeviceValueType.ValueTypeTempExt:
            case DeviceValueType.ValueTypeTempExt2:
                valueTypeId = DeviceValueType.ValueTypeTempInt;
                break;
            case DeviceValueType.ValueTypeLuxExt:
            case DeviceValueType.ValueTypeLuxExt2:
                valueTypeId = DeviceValueType.ValueTypeLuxInt;
                break;
        }
        return ValueTypes.ReverseMap[(int)valueTypeId];
    }

    public IEnumerable<SensorDataItemAndLocationId> GetSensorDataAndLocations(int locationId)
    {
        for (var idx = 0; idx < 8; idx++)
        {
            var valueAndValueType = ValuesAndValueTypes[idx];
            if (valueAndValueType == 0)
                break;
            var valueType = GetValueType(idx);
            var value = GetValue(idx);
            var di = new SensorDataItem(EventTimeMs, (double)value / 100);
            yield return new SensorDataItemAndLocationId(locationId, valueType, di);
        }
    }
}

internal record SensorDataItemAndLocationId(int LocationId, string ValueType, SensorDataItem Item);

public sealed class BluetoothSmartHomeService : BaseSmartHomeService
{
    private record DateRange(int MaxPoints, bool Aggregated, long StartTimestamp, long EndTimestamp);

    private readonly List<SensorEvent> _events;
    private readonly GattCharacteristic _characteristic;
    private readonly TimeSpan _timeout;
    private readonly string _saveFileName;

    private long _timestamp;

    internal static BluetoothSmartHomeServiceConfiguration ReadConfiguration(string configFileName)
    {
        using var configurationStream = File.OpenRead(configFileName);
        return JsonSerializer.Deserialize<BluetoothSmartHomeServiceConfiguration>(configurationStream)
               ?? throw new Exception("Invalid configuration file");
    }

    private BluetoothSmartHomeService(BluetoothSmartHomeServiceConfiguration smartHomeServiceConfiguration,
        GattCharacteristic characteristic) :
        base(smartHomeServiceConfiguration.SensorsFile, smartHomeServiceConfiguration.LocationsFile,
            smartHomeServiceConfiguration.TimeZone)
    {
        ValueTypes.Init(smartHomeServiceConfiguration.ValueTypesFile);
        _characteristic = characteristic;
        _timestamp = 0;
        _timeout = TimeSpan.FromSeconds(smartHomeServiceConfiguration.Timeout);
        _events = LoadEvents(smartHomeServiceConfiguration.SaveFileName);
        _timestamp = _events.Count != 0 ? _events[^1].EventTimeMs + 1 : 0;
        _saveFileName = smartHomeServiceConfiguration.SaveFileName;
    }

    private static List<SensorEvent> LoadEvents(string saveFileName)
    {
        if (!File.Exists(saveFileName)) 
            return [];
        var data = File.ReadAllBytes(saveFileName);
        return new List<SensorEvent>(MemoryMarshal.Cast<byte, SensorEvent>(data).ToArray());
    }

    internal static async Task<BluetoothSmartHomeService> BuildBluetoothSmartHomeServiceAsync(
        BluetoothSmartHomeServiceConfiguration smartHomeServiceConfiguration)
    {
        IAdapter1? adapter = (await BlueZManager.GetAdaptersAsync()).FirstOrDefault();
        if (adapter is null)
            throw new Exception("No Bluetooth adapters found");
        var devices = await adapter.GetDevicesAsync();
        foreach (var device in devices)
        {
            string name;
            try
            {
                name = await device.GetNameAsync();
            }
            catch (Exception ex)
            {
                name = "";
            }
            if (name == smartHomeServiceConfiguration.DeviceName)
            {
                var timeout = TimeSpan.FromSeconds(smartHomeServiceConfiguration.Timeout);
                await device.ConnectAsync();
                await device.WaitForPropertyValueAsync("Connected", value: true, timeout);
                await device.WaitForPropertyValueAsync("ServicesResolved", value: true, timeout);
                var service = await device.GetServiceAsync(smartHomeServiceConfiguration.ServiceUuid);
                var characteristic =
                    await service.GetCharacteristicAsync(smartHomeServiceConfiguration.CharacteristicUuid);
                return new BluetoothSmartHomeService(smartHomeServiceConfiguration, characteristic);
            }
        }

        throw new KeyNotFoundException($"Device {smartHomeServiceConfiguration.DeviceName} not found");
    }

    private async Task LoadNewData()
    {
        await _characteristic.WriteValueAsync(BitConverter.GetBytes(_timestamp), new Dictionary<string, object>());
        var value = await _characteristic.ReadValueAsync(_timeout);
        while (value.Length != 0)
        {
            var events = MemoryMarshal.Cast<byte, SensorEvent>(value).ToArray();
            AddEvents(events);
            value = await _characteristic.ReadValueAsync(_timeout);
        }
    }

    private void AddEvents(SensorEvent[] events)
    {
        _timestamp = events[^1].EventTimeMs + 1;
        _events.AddRange(events);
        var bytes = MemoryMarshal.AsBytes(events);
        File.AppendAllBytes(_saveFileName, bytes);
    }

    public override LastSensorData GetLastSensorData()
    {
        var stopwatch = new Stopwatch();
        stopwatch.Start();
        LoadNewData().Wait(_timeout);
        stopwatch.Stop();
        ResponseTimeMs = stopwatch.Elapsed.TotalMilliseconds;

        var data = new Dictionary<int, Dictionary<string, SensorDataItem>>();
        var sensors = Sensors
            .Where(s => s.Value.Enabled)
            .Select(s => s.Key)
            .ToHashSet();
        for (var i = _events.Count - 1; i >= 0; i--)
        {
            var ev = _events[i];
            var sid = ev.SensorId;
            if (!sensors.Contains(sid))
                continue;
            var locationId = Sensors[sid].LocationId;
            for (var idx = 0; idx < 8; idx++)
            {
                var valueAndValueType = ev.ValuesAndValueTypes[idx];
                if (valueAndValueType == 0)
                    break;
                var valueType = ev.GetValueType(idx);
                var value = ev.GetValue(idx);
                var di = new SensorDataItem(ev.EventTimeMs, (double)value / 100);
                if (data.TryGetValue(locationId, out var locationValues))
                    locationValues[valueType] = di;
                else
                    data[locationId] = new Dictionary<string, SensorDataItem> { { valueType, di } };
            }
            sensors.Remove(sid);
            if (sensors.Count == 0)
                break;
        }

        return new LastSensorData(data);
    }

    public override Dictionary<string, SensorTimestamp> GetSensorTimestamps()
    {
        throw new NotSupportedException();
    }

    public override void InsertMessages(List<SensorMessages> messages, Logger logger, bool dryRun)
    {
        throw new NotSupportedException();
    }

    private DateRange BuildDateRange(SmartHomeQuery query)
    {
        var range = BuildBaseDateRange(query);
        var aggregated = (range.EndDateTime - range.StartDateTime).TotalMinutes / 5 > range.MaxPoints;
        var startTimestamp = new DateTimeOffset(range.StartDateTime).ToUnixTimeMilliseconds();
        var endTimestamp = new DateTimeOffset(range.EndDateTime).ToUnixTimeMilliseconds();
        return new DateRange(range.MaxPoints, aggregated, startTimestamp, endTimestamp);
    }
    
    public override SensorDataResult GetSensorData(SmartHomeQuery sensorDataQuery, out bool aggregated)
    {
        var stopwatch = new Stopwatch();
        stopwatch.Start();
        LoadNewData().Wait(_timeout);
        stopwatch.Stop();
        ResponseTimeMs = stopwatch.Elapsed.TotalMilliseconds;

        var sensors = GetSensors(sensorDataQuery.DataType);
        if (sensors.Count == 0)
        {
            aggregated = false;
            return new SensorDataResult([]);
        }
        var dateRange = BuildDateRange(sensorDataQuery);
        aggregated = dateRange.Aggregated;
        var rawData = GetRawSensorData(sensors, dateRange);
        return dateRange.Aggregated ? GetAggregatedSensorData(rawData, dateRange.MaxPoints) : rawData;
    }

    private SensorDataResult GetAggregatedSensorData(SensorDataResult rawData, int maxPoints)
    {
        throw new NotImplementedException();
    }

    private SensorDataResult GetRawSensorData(HashSet<uint> sensors, DateRange dateRange)
    {
        var d = _events
            .Where(e => 
                e.EventTimeMs >= dateRange.StartTimestamp &&
                e.EventTimeMs <= dateRange.EndTimestamp &&
                sensors.Contains(e.SensorId))
            .SelectMany(e => e.GetSensorDataAndLocations(Sensors[e.SensorId].LocationId))
            .GroupBy(e => e.ValueType)
            .Select(group => (group.Key, CreateSensorDataList(group)))
            .ToDictionary();
        return new SensorDataResult(d); 
    }

    private static List<SensorData> CreateSensorDataList(IGrouping<string, SensorDataItemAndLocationId> group)
    {
        return group
            .GroupBy(item => item.LocationId)
            .Select(g => 
                new SensorData(g.Key, g.Select(item => item.Item).ToList(), null))
            .ToList();
    }

    public override YearlySensorDataResult GetYearlySensorData()
    {
        throw new NotSupportedException();
    }
}