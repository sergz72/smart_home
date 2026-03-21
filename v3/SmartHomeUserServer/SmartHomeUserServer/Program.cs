using System.Text.Json;
using SmartHomeService;
using SmartHomeUserServer;

using var configurationStream = File.OpenRead(args[0]);
var configurations = JsonSerializer.Deserialize<List<Configuration>>(configurationStream)
                     ?? throw new Exception("Invalid configuration file");

var servers = new List<Server>();
foreach (var configuration in configurations)
{
    var service = new RedisSmartHomeService(configuration.ServiceConfiguration);
    var server = new Server(configuration.ServerConfiguration, service, new ConsoleLoggerCreator(null));
    servers.Add(server);
}

var tasks = new List<Task>();
foreach (var server in servers)
    tasks.Add(Task.Run(() => server.Start()));

Console.CancelKeyPress += (sender, eventArgs) =>
{
    eventArgs.Cancel = true;
    foreach (var server in servers)
        server.Stop();
};

Task.WaitAll(tasks);

return;

internal record ServerConfiguration(string Name, ushort Port, string KeyFileName);

internal record Configuration(
    ServerConfiguration ServerConfiguration,
    RedisSmartHomeServiceConfiguration ServiceConfiguration);