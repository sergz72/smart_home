using System.Net;
using System.Net.Sockets;
using System.Security.Cryptography;
using System.Text;
using SmartHomeService;

namespace SmartHomeUserServer;

internal class Server
{
    private const long MaxTimeDifference = 60;
    
    private readonly byte[] _key;
    private readonly ISmartHomeService _service;
    private readonly UdpClient _client;
    private readonly ILoggerCreator _loggerCreator;
    private readonly Logger _logger;
    private readonly ServerConfiguration _configuration;
    
    private volatile bool _stop, _stopped;
    
    internal Server(ServerConfiguration configuration, ISmartHomeService service, ILoggerCreator loggerCreator)
    {
        _key = File.ReadAllBytes(configuration.KeyFileName);
        if (configuration.Port == 0)
            throw new Exception("Port must be specified");
        if (configuration.Name.Length == 0)
            throw new Exception("Name must be specified");
        _configuration = configuration;
        _loggerCreator = loggerCreator;
        _logger = loggerCreator.CreateLogger(configuration.Name);
        _service = service;
        _client = new UdpClient(configuration.Port);
    }
    
    internal void Start()
    {
        _logger.Info($"Starting server on port {_configuration.Port}");
        while (true)
        {
            IPEndPoint? ep = null;
            var data = _client.Receive(ref ep);
            if (_stop) break;
            Handle(data, ep);
        }
        _client.Close();
        _logger.Info($"{_configuration.Name} stopped");
        _stopped = true;
    }

    internal void Stop()
    {
        _stop = true;
        new UdpClient().Send([0], 1, new IPEndPoint(IPAddress.Loopback, _configuration.Port));
        while (!_stopped)
            Thread.Sleep(100);
    }

    private void Handle(byte[] data, IPEndPoint ep)
    {
        var logger = _loggerCreator.CreateLogger($"{_configuration.Name} {ep}");
        logger.Debug("New connection");
        byte[] decrypted;
        try
        {
            decrypted = Decrypt(data);
        }
        catch (Exception e)
        {
            logger.Error(e.Message);
            return;
        }

        byte[] response;
        try
        {
            response = Execute(decrypted, logger);
        }
        catch (Exception e)
        {
            logger.Error(e.Message);
            response = BuildErrorResponse(e.Message);
        }
        try
        {
            var compressed = Compressor.Compress(response);
            var encrypted = Encrypt(compressed);
            _client.Send(encrypted, encrypted.Length, ep);
        }
        catch (Exception e)
        {
            logger.Error(e.Message);
        }
    }

    private byte[] BuildErrorResponse(string message)
    {
        var bytes = Encoding.UTF8.GetBytes(message);
        var result = new byte[bytes.Length + 1];
        result[0] = 1;
        Array.Copy(bytes, 0, result, 1, bytes.Length);
        return result;
    }

    private static byte[] BuildOkResponse(byte[] data)
    {
        var result = new byte[data.Length + 1];
        result[0] = 0;
        Array.Copy(data, 0, result, 1, data.Length);
        return result;
    }

    private byte[] Encrypt(byte[] data)
    {
        var iv = RandomNumberGenerator.GetBytes(12);
        var cipher = new ChaCha20(_key, iv);
        var encrypted = cipher.Encrypt(data);
        var result = new byte[encrypted.Length + 12];
        iv.CopyTo(result, 0);
        encrypted.CopyTo(result, 12);
        return result;
    }
    
    private static void ValidateIv(byte[] iv)
    {
        var time = BitConverter.ToInt64(iv, 4);
        var now = ((DateTimeOffset)DateTime.UtcNow).ToUnixTimeSeconds();
        if ((now >= time && now - time > MaxTimeDifference) ||
            (now < time && time - now > MaxTimeDifference))
            throw new Exception("Invalid IV");
    }
    
    private byte[] Decrypt(byte[] data)
    {
        if (data.Length < 13)
            throw new Exception("Invalid request length");
        var iv = TransformIv(data[..12]);
        ValidateIv(iv);
        var cipher = new ChaCha20(_key, iv);
        return cipher.Encrypt(data[12..]);
    }

    private byte[] TransformIv(byte[] iv)
    {
        var iv3 = new byte[12];
        var randomPart = iv[..4];
        randomPart.CopyTo(iv3, 0);
        randomPart.CopyTo(iv3, 4);
        randomPart.CopyTo(iv3, 8);
        var cipher = new ChaCha20(_key, iv3);
        var transformed = cipher.Encrypt(iv[4..]);
        Array.Copy(transformed, 0, iv3, 4, 8);
        return iv3;
    }
    
    private byte[] Execute(byte[] data, Logger logger)
    {
        if (data.Length == 0)
            throw new Exception("Empty request");
        return data[0] switch
        {
            0 => GetLocations(data, logger),
            1 => GetLastData(data, logger),
            2 => GetSensorData(data, logger),
            _ => throw new Exception("Invalid request")
        };
    }

    private byte[] GetSensorData(byte[] data, Logger logger)
    {
        logger.Debug("Get sensor data request");
        var query = SmartHomeQuery.FromBinary(_service, data[1..]);
        logger.Debug(query.ToString());
        var result = _service.GetSensorData(query);
        return BuildOkResponse(result.ToBinary());
    }

    private byte[] GetLastData(byte[] data, Logger logger)
    {
        if (data.Length != 1)
            throw new Exception("GetLastData: Invalid data length");
        logger.Debug("Get last data request");
        return BuildOkResponse(_service.GetLastSensorData().ToBinary());
    }

    private byte[] GetLocations(byte[] data, Logger logger)
    {
        if (data.Length != 1)
            throw new Exception("GetLocations: Invalid data length");
        logger.Debug("Get locations request");
        return BuildOkResponse(_service.GetLocations().ToBinary());
    }
}