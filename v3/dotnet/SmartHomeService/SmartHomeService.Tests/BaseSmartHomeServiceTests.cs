namespace SmartHomeService.Tests;

public class BaseSmartHomeServiceTests
{
    [Test]
    public void TestBuildBaseDateRange()
    {
        var service = new TestService("EST");
        var tz = TimeZoneInfo.FindSystemTimeZoneById("Eastern Standard Time");
        var query = new SmartHomeQuery(200, "env", null, new DateOffset(-1, TimeUnit.Day), null);
        var result = service.BuildBaseDateRange(query);
        var now = DateTime.UtcNow;
        Assert.That(result.StartDateTime.Hour, Is.EqualTo(now.Hour));
        query = new SmartHomeQuery(200, "env", new DateOnly(2026, 1, 1), null, new DateOffset(1, TimeUnit.Day));
        result = service.BuildBaseDateRange(query);
        Assert.That(result.StartDateTime.Hour, Is.EqualTo(5));
    }
}

internal class TestService : BaseSmartHomeService
{
    public TestService(string timeZone) : base(timeZone)
    {
    }

    public override LastSensorData GetLastSensorData()
    {
        throw new NotImplementedException();
    }

    public override SensorDataResult GetSensorData(SmartHomeQuery sensorDataQuery, out bool aggregated)
    {
        throw new NotImplementedException();
    }

    public override YearlySensorDataResult GetYearlySensorData()
    {
        throw new NotImplementedException();
    }
}