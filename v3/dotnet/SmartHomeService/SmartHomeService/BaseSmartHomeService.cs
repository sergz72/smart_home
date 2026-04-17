using System.Text.Json;

namespace SmartHomeService;

public abstract class BaseSmartHomeService: ISmartHomeService
{
    protected static readonly Dictionary<string, string> ValueTypeMap = new Dictionary<string, string>
    {
        {"humi", "Humidity"},
        {"temp", "Temperature"},
        {"pres", "Pressure"},
        {"co2 ", "CO2"},
        {"lux ", "Luminosity"},
        {"pwr ", "Power"},
        {"icc ", "Current"},
        {"vcc ", "Voltage"}
    };
    
    protected readonly Dictionary<uint, Sensor> Sensors;
    protected readonly Dictionary<int, Location> Locations;
    protected readonly TimeZoneInfo TimeZone;
    protected readonly string TimeZoneName;

    public double ResponseTimeMs { get; protected set; }
    
    public string GetLocationName(int locationId)
    {
        return Locations[locationId].Name;
    }

    public int GetLocationId(uint sensorId)
    {
        return Sensors[sensorId].LocationId;
    }
    
    public bool IsExtLocation(int locationId)
    {
        return Locations[locationId].LocationType == "ext";
    }

    public Locations GetLocations()
    {
        return new Locations(Locations, TimeZoneName);
    }

    public long BuildTimestamp(int year, long timeMs)
    {
        var date = TimeZoneInfo.ConvertTimeToUtc(new DateTime(year, 1, 1, 0, 0, 0), TimeZone);
        var timestamp = new DateTimeOffset(date).ToUnixTimeMilliseconds();
        return timestamp + timeMs;
    }

    public DateTime GetDateTime(long timestamp)
    {
        return TimeZoneInfo
            .ConvertTimeFromUtc(DateTimeOffset.FromUnixTimeMilliseconds(timestamp).DateTime, TimeZone);
    }
    
    public abstract LastSensorData GetLastSensorData();

    public abstract SensorDataResult GetSensorData(SmartHomeQuery sensorDataQuery, out bool aggregated);

    public abstract YearlySensorDataResult GetYearlySensorData();
    
    public SensorDataItemWithDate BuildSensorDataItemWithDate(SensorDataItem data)
    {
        return new SensorDataItemWithDate(GetDateTime(data.Timestamp), data.Value);
    }

    public string GetValueType(string valueType)
    {
        return ValueTypeMap[valueType];
    }
    
    protected BaseSmartHomeService(string sensorsFile, string locationsFile, string timeZone)
    {
        TimeZone = TimeZoneInfo.FindSystemTimeZoneById(timeZone);
        TimeZoneName = timeZone;
        using var sensorsStream = File.OpenRead(sensorsFile);
        Sensors = JsonSerializer.Deserialize<Dictionary<uint, Sensor>>(sensorsStream)
                   ?? throw new Exception("Invalid sensors file");
        using var locationsStream = File.OpenRead(locationsFile);
        Locations = JsonSerializer.Deserialize<Dictionary<int, Location>>(locationsStream)
                     ?? throw new Exception("Invalid locations file");
    }

    protected BaseSmartHomeService(string timeZone)
    {
        TimeZone = TimeZoneInfo.FindSystemTimeZoneById(timeZone);
        TimeZoneName = timeZone;
        Sensors = [];
        Locations = [];
    }
    
    protected HashSet<uint> GetSensors(string dataType)
    {
        return Sensors
            .Where(s => s.Value.DataType == dataType)
            .Select(s => s.Key)
            .ToHashSet();
    }
    
    public BaseDateRange BuildBaseDateRange(SmartHomeQuery query)
    {
        var maxPoints = query.MaxPoints <= 0 ? 2000 : query.MaxPoints;
        var now = DateTime.UtcNow;
        var startDateTime = query.StartDate == null
            ? query.StartDateOffset!.CalculateDate(now)
            : TimeZoneInfo.ConvertTimeToUtc(new DateTime((DateOnly)query.StartDate, TimeOnly.MinValue), TimeZone);
        var endDateTime = query.Period?.CalculateDate(startDateTime) ?? now;
        return new BaseDateRange(maxPoints, startDateTime, endDateTime);
    }

    public record BaseDateRange(int MaxPoints, DateTime StartDateTime, DateTime EndDateTime);
}