using System.Runtime.InteropServices;
using BluetoothClient;
using Linux.Bluetooth;
using Linux.Bluetooth.Extensions;
using Tmds.DBus;

var timeout = TimeSpan.FromSeconds(5);
const string serviceUuid = "0000abf0-0000-1000-8000-00805f9b34fb";
const string characteristicUuid = "0000abf1-0000-1000-8000-00805f9b34fb";

if (args.Length != 2)
{
    Console.WriteLine("Usage: BluetoothClient <device name> timestamp");
    return 1;
}

var deviceName = args[0];
var timestamp = long.Parse(args[1]);

IAdapter1? adapter = (await BlueZManager.GetAdaptersAsync()).FirstOrDefault();
if (adapter is null)
{
    Console.WriteLine("No Bluetooth adapters found");
    return 1;
}

var devices = await adapter.GetDevicesAsync();
foreach (var device in devices)
{
    try
    {
        var name = await device.GetNameAsync();
        Console.WriteLine($"Device '{name}' found");
        if (name == deviceName)
        {
            await device.ConnectAsync();
            await device.WaitForPropertyValueAsync("Connected", value: true, timeout);
            await device.WaitForPropertyValueAsync("ServicesResolved", value: true, timeout);
            await ExecuteQuery(device);
            return 0;
        }
    }
    catch (DBusException e)
    {
        Console.WriteLine(e);
    }
}

Console.WriteLine($"Device '{deviceName}' not found");

return 1;

async Task ExecuteQuery(Device device)
{
    Console.WriteLine("Querying for data...");
    var service = await device.GetServiceAsync(serviceUuid);
    var characteristic = await service.GetCharacteristicAsync(characteristicUuid);
    await characteristic.WriteValueAsync(BitConverter.GetBytes(timestamp), new Dictionary<string, object>());
    Console.WriteLine("Reading data...");
    var value = await characteristic.ReadValueAsync(timeout);
    while (value.Length != 0)
    {
        var events = new List<SensorEvent>(MemoryMarshal.Cast<byte, SensorEvent>(value).ToArray());
        Console.WriteLine("Received events:");
        foreach (var ev in events)
            Console.WriteLine(ev);
        Console.WriteLine("Reading data...");
        value = await characteristic.ReadValueAsync(timeout);
    }
}
