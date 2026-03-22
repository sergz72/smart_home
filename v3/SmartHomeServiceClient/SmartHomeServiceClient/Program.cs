using System.Text;
using NetworkServiceClientLibrary;
using SmartHomeService;
using SmartHomeServiceClientLibrary;

if (args.Length != 4)
    Usage();

var keyBytes = File.ReadAllBytes(args[0]);
var hostName = args[1];
var port = ushort.Parse(args[2]);
var query = args[3];

var client = new SmartHomeClient(new NetworkServiceConfig(
    [],
    keyBytes,
    hostName,
    port,
    2000));

try
{
   ResponseStatistics statistics = new ResponseStatistics();

    switch (query)
    {
        case "locations":
            var locationsResponse = client.GetLocations(statistics);
            PrintLocationsResponse(locationsResponse);
            break;
        case "last_data":
            var lastResponse = client.GetLastSensorData(statistics);
            PrintLastResponse(lastResponse);
            break;
        default:
            var q = BuildSmartHomeQuery();
            var response = client.GetSensorData(q, statistics);
            PrintSensorDataResponse(response);
            break;
    }
    
    Console.WriteLine("Response time {0} ms.", statistics.ResponseTimeMs);
    Console.WriteLine("Response size {0}.", statistics.ResponseSize);
    Console.WriteLine("Decompressed size {0}.", statistics.DecompressedSize);
}
catch (Exception e)
{
    Console.WriteLine(e.Message);
    Environment.Exit(1);
}

return;

(string, string) BuildKeyValue(string parameter)
{
    var parts = parameter.Split("=");
    if (parts.Length != 2)
        throw new ArgumentException($"Invalid parameter {parameter}");
    return (parts[0], parts[1]);
}

(DateTime?, DateOffset?) ParseDate(string dateStr)
{
    if (dateStr.StartsWith('-'))
    {
        var period = ParsePeriod("offset", dateStr[1..]);
        return (null, period);
    }
    var date = int.Parse(dateStr);
    return (BuildDate(date), null);
}

DateTime BuildDate(int date)
{
    return new DateTime(date / 10000, (date / 100) % 100, date % 100);
}

DateOffset? ParsePeriod(string title, string? periodStr)
{
    if (periodStr == null)
        return null;
    var unit = periodStr.Last() switch
    {
        'd' => TimeUnit.Day,
        'm' => TimeUnit.Month,
        'y' => TimeUnit.Year,
        _ => throw new ArgumentException($"Invalid period {periodStr}")
    };
    var value = int.Parse(periodStr[..^1]);
    if (value <= 0)
        throw new ArgumentException($"Invalid {title} value {value}");
    return new DateOffset(value, unit);
}

SmartHomeQuery BuildSmartHomeQuery()
{
    var parameters = query
        .Split('&')
        .Select(BuildKeyValue)
        .ToDictionary(kv => kv.Item1, kv => kv.Item2);
    var dataType = parameters.GetValueOrDefault("dataType") ?? throw new ArgumentException("Missing dataType");
    var maxPoints = short.Parse(parameters.GetValueOrDefault("maxPoints", "0"));
    Console.WriteLine("maxPoints: {0}", maxPoints);
    Console.WriteLine("dataType: {0}", dataType);
    var (startDate, startDateOffset) =
        ParseDate(parameters.GetValueOrDefault("startDate") ?? throw new ArgumentException("Missing startDate"));
    if (startDate != null)
        Console.WriteLine("startDate: {0}", startDate);
    else
    {
        Console.WriteLine("startDateOffset.offset: {0}", startDateOffset!.Offset);
        Console.WriteLine("startDateOffset.unit: {0}", startDateOffset.Unit);
    }

    var period = ParsePeriod("period", parameters.GetValueOrDefault("period"));
    if (period != null) {
        Console.WriteLine("period.offset: {0}", period.Offset);
        Console.WriteLine("period.unit: {0}", period.Unit);
    }

    return new SmartHomeQuery(maxPoints, dataType, startDate, startDateOffset, period);
}

void PrintLocationsResponse(Locations locations)
{
    Console.WriteLine("Time zone: {0}", locations.TimeZone);
    foreach (var (locationId, location) in locations.LocationMap)
        Console.WriteLine("Location {0}: {1} {2}", locationId, location.Name, location.LocationType);   
}

void PrintLastResponse(LastSensorData lastData)
{
    foreach (var (locationId, data) in lastData.Data)
    {
        Console.WriteLine("Location {0}: {1}", locationId, LastDataToString(data));
    }
}

void PrintSensorDataResponse(SensorDataResult response)
{
    foreach (var (valueType, data) in response.Data)
    {
        Console.WriteLine("Value type {0}:", valueType);
        foreach (var sdata in data)
        {
            if (sdata.Raw != null)
                Console.WriteLine("Location {0}: raw data count {1}", sdata.LocationId, sdata.Raw.Count);
            else
                Console.WriteLine("Location {0}: aggregated data count {1}", sdata.LocationId, sdata.Aggregated!.Avg.Count);
        }
    }
}

string LastDataToString(Dictionary<string, SensorDataItem> data)
{
    var sb = new StringBuilder();
    foreach (var (valueType, sensorData) in data)
    {
        sb.Append(valueType);
        sb.Append('=');
        sb.Append(sensorData.Value);
        sb.Append("(");
        sb.Append(sensorData.Timestamp);
        sb.Append(") ");
    }
    return sb.ToString();
}

void Usage()
{
    Console.WriteLine("SmartHomeServiceClient keyFileName hostName port query");
    Environment.Exit(1);
}