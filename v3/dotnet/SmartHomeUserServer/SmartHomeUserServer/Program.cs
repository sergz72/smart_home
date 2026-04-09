using System.Text.Json;
using SmartHomeService;
using SmartHomeUserServer;

using var configurationStream = File.OpenRead(args[0]);
var configurations = JsonSerializer.Deserialize<List<Configuration>>(configurationStream)
                     ?? throw new Exception("Invalid configuration file");

var servers = new List<Server>();
foreach (var configuration in configurations)
{
    var service = configuration.BuildService();
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

internal record Configuration(
    ServerConfiguration ServerConfiguration,
    RedisSmartHomeServiceConfiguration? RedisServiceConfiguration,
    FileSmartHomeServiceConfiguration? FileServiceConfiguration)
{
    public ISmartHomeService BuildService()
    {
        if (RedisServiceConfiguration != null && RedisServiceConfiguration.RedisConnectionString.Length != 0)
            return new RedisSmartHomeService(RedisServiceConfiguration);
        if (FileServiceConfiguration != null && FileServiceConfiguration.BaseFolder.Length != 0)
            return new FileSmartHomeService(FileServiceConfiguration);
        throw new InvalidDataException("No valid Smart Home service configuration provided");
    }
}