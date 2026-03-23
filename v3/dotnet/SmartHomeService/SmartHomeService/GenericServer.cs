using System.Net;
using System.Net.Sockets;

namespace SmartHomeService;

public record ServerConfiguration(string Name, ushort Port, string KeyFileName);

public abstract class GenericServer
{
    protected readonly byte[] Key;
    protected readonly UdpClient Client;
    private readonly ILoggerCreator _loggerCreator;
    private readonly Logger _logger;
    private readonly ServerConfiguration _configuration;
    
    private volatile bool _stop;
    
    protected GenericServer(ServerConfiguration configuration, ILoggerCreator loggerCreator)
    {
        Key = File.ReadAllBytes(configuration.KeyFileName);
        if (configuration.Port == 0)
            throw new Exception("Port must be specified");
        if (configuration.Name.Length == 0)
            throw new Exception("Name must be specified");
        _configuration = configuration;
        _loggerCreator = loggerCreator;
        _logger = loggerCreator.CreateLogger(configuration.Name);
        Client = new UdpClient(configuration.Port);
    }
    
    public void Start()
    {
        _logger.Info($"Starting server on port {_configuration.Port}");
        while (true)
        {
            IPEndPoint? ep = null;
            var data = Client.Receive(ref ep);
            if (_stop) break;
            var logger = _loggerCreator.CreateLogger($"{_configuration.Name} {ep}");
            logger.Debug("New connection");
            Handle(data, ep, logger);
            logger.Debug("Connection closed");
        }
        Client.Close();
        _logger.Info("Server stopped");
    }

    public void Stop()
    {
        _stop = true;
        new UdpClient().Send([0], 1, new IPEndPoint(IPAddress.Loopback, _configuration.Port));
    }

    protected abstract void Handle(byte[] data, IPEndPoint ep, Logger logger);
}