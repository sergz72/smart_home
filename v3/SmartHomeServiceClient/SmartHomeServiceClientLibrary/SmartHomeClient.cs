using NetworkServiceClientLibrary;
using ICSharpCode.SharpZipLib.BZip2;

namespace SmartHomeServiceClientLibrary;

public sealed class SmartHomeClient(NetworkServiceConfig config)
{
    private readonly NetworkService _service = new(config);

    private static MemoryStream Decompress(byte[] decrypted)
    {
        var outStream = new MemoryStream();
        BZip2.Decompress(new MemoryStream(decrypted), outStream, false);
        outStream.Position = 0;
        return outStream;
    }
    
    public SensorDataResponse GetSensorData(SmartHomeQuery query)
    {
        var response = SendAndDecompress(query.ToBinary());
        var aggregated = ValidateResponse(response, 1) == 1;
        return SensorDataResponse.ParseResponse(response, aggregated,
            aggregated ? SensorData.BuildFromAggregatedResponse : SensorData.BuildFromResponse);
    }
    
    public List<Sensor> GetSensors()
    {
        var response = SendAndDecompress([0,0]);
        ValidateResponse(response);
        return Sensor.ParseResponse(response);
    }

    private MemoryStream SendAndDecompress(byte[] request)
    {
        var response = _service.Send(request);
        return Decompress(response);
    }

    public Dictionary<int, LastSensorData> GetLastSensorData(byte days)
    {
        var response = SendAndDecompress(BuildLastSensorDataRequest(days));
        ValidateResponse(response);
        return LastSensorData.ParseResponse(response);
    }

    private static int ValidateResponse(MemoryStream response, int maxValidReponse = 0)
    {
        if (response.Length == 0)
            throw new IOException("Empty response");
        var code = response.ReadByte();
        if (code > maxValidReponse)
            throw new ResponseError(response.ToArray());
        return code;
    }
    
    private static byte[] BuildLastSensorDataRequest(byte days)
    {
        return [1, days];
    }
}