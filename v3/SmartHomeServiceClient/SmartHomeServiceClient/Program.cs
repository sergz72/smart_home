using System.Diagnostics;
using NetworkServiceClientLibrary;
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

var stopWatch = new Stopwatch();

switch (query)
{
    case "sensors":
        stopWatch.Start();
        var sensorsResponse = client.GetSensors();
        stopWatch.Stop();
        PrintSensorsResponse(sensorsResponse);
        break;
    case string s when s.StartsWith("last_data_from="):
        var days = byte.Parse(s[15..]);
        stopWatch.Start();
        var lastResponse = client.GetLastSensorData(days);
        stopWatch.Stop();
        PrintLastResponse(lastResponse);
        break;
    default:
        Usage();
        break;
}
Console.WriteLine("Response time {0} us.", stopWatch.Elapsed.Microseconds);
return;

void PrintSensorsResponse(List<Sensor> sensors)
{
    foreach (var sensor in sensors)
        Console.WriteLine("Sensor {0}: {1} {2} {3}", sensor.Id, sensor.DataType, sensor.Location, sensor.LocationType);   
}

void PrintLastResponse(Dictionary<int, LastSensorData> lastData)
{
    foreach (var (sensorId, data) in lastData)
        Console.WriteLine("Sensor {0}: {1} {2}", sensorId, data.Date, data.Values);  
}

void Usage()
{
    Console.WriteLine("SmartHomeServiceClient keyFileName hostName port query");
    Environment.Exit(1);
}