using NetworkServiceClientLibrary;
using ICSharpCode.SharpZipLib.BZip2;
using SmartHomeService;

namespace SmartHomeServiceClientLibrary;

public sealed class SmartHomeClient(NetworkServiceConfig config)
{
    private readonly NetworkService _service = new(config, false);

    private static MemoryStream Decompress(byte[] decrypted)
    {
        var outStream = new MemoryStream();
        BZip2.Decompress(new MemoryStream(decrypted), outStream, false);
        outStream.Position = 0;
        return outStream;
    }
    
    public SensorDataResult GetSensorData(SmartHomeQuery query, out double responseTimeMs)
    {
        var response = SendAndDecompress(BuildSensorDataRequest(query), out responseTimeMs);
        ValidateResponse(response);
        return SensorDataResult.ParseResponse(response);
    }

    private static byte[] BuildSensorDataRequest(SmartHomeQuery query)
    {
        var data = query.ToBinary();
        var response = new byte[data.Length + 1];
        response[0] = 2;
        data.CopyTo(response, 1);
        return response;
    }

    public Locations GetLocations(out double responseTimeMs)
    {
        var response = SendAndDecompress([0], out responseTimeMs);
        ValidateResponse(response);
        return Locations.ParseResponse(response);
    }

    private MemoryStream SendAndDecompress(byte[] request, out double responseTimeMs)
    {
        var response = _service.Send(request, out responseTimeMs);
        return Decompress(response);
    }

    public LastSensorData GetLastSensorData(out double responseTimeMs)
    {
        var response = SendAndDecompress([1], out responseTimeMs);
        ValidateResponse(response);
        return LastSensorData.ParseResponse(response);
    }

    private static void ValidateResponse(MemoryStream response)
    {
        if (response.Length == 0)
            throw new IOException("Empty response");
        var code = response.ReadByte();
        if (code == 2)
            throw new ResponseError(response.ToArray());
        if (code != 0)
            throw new Exception($"Invalid response code {code}");
    }
}