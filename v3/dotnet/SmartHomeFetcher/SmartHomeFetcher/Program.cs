using System.Text.Json;
using SmartHomeFetcher;
using SmartHomeService;

try
{
    using var configurationStream = File.OpenRead(args[0]);
    var configuration = JsonSerializer.Deserialize<Configuration>(configurationStream)
                        ?? throw new Exception("Invalid configuration file");
    var service = configuration.BuildService();
    var dryRun = args.Length > 1 && args[1] == "--dry-run";
    if (dryRun)
        Console.WriteLine("Dry run mode.");
    var fetcher = new Fetcher(configuration, service);
    fetcher.FetchMessages(dryRun);
}
catch (Exception e)
{
    Console.WriteLine(e.Message);
}

return;

internal record Configuration(
    RedisSmartHomeServiceConfiguration? RedisServiceConfiguration,
    FileSmartHomeServiceConfiguration? FileServiceConfiguration,
    string KeyFileName, int Timeout, int Retries, Dictionary<string, string[]> Addresses)
    : ServiceConfiguration(RedisServiceConfiguration, FileServiceConfiguration);
