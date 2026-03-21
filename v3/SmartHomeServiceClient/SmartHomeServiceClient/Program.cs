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
    port));

double responseTimeMs;

switch (query)
{
    case "locations":
        var locationsResponse = client.GetLocations(out responseTimeMs);
        PrintLocationsResponse(locationsResponse);
        break;
    case "last_data":
        var lastResponse = client.GetLastSensorData(out responseTimeMs);
        PrintLastResponse(lastResponse);
        break;
    default:
        Usage();
        return;
}

Console.WriteLine("Response time {0} ms.", responseTimeMs);

return;

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