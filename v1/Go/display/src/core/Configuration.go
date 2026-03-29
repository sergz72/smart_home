package core

import (
    "encoding/json"
    "io/ioutil"
    "log"
    "time"
)

type Device struct {
    Name string
    Priority int
    LineStart int
    NumLines int
}

type Max44009Config struct {
    Min float64
    Max float64
    DeviceName string
    Address int
}

type Configuration struct {
    Port int
    NumLines int
    Width int
    FontSize int
    FontWidth int
    KeyFileName string
    SmartHomeAddress string
    SmartHomeCommand string
    Max44009 *Max44009Config
    HoldTime int
    holdTime time.Duration
    Devices []*Device
    devices map[string]*Device
}

var Config Configuration

func checkInt(name string, value int) {
    if value <= 0 {
        log.Fatalf("Invalid or Missing %v", name)
    }
}

func LoadConfiguration(iniFileName string) {
    iniFileContents, err := ioutil.ReadFile(iniFileName)
    if err != nil {
        log.Fatal(err)
    }
    err = json.Unmarshal(iniFileContents, &Config)
    if err != nil {
        log.Fatal(err)
    }


    checkInt("port number", Config.Port)
    checkInt("font size", Config.FontSize)
    checkInt("font width", Config.FontWidth)
    checkInt("number of display lines", Config.NumLines)
    checkInt("display width", Config.Width)
    checkInt("hold time", Config.HoldTime)
    if Config.Devices == nil {
        log.Fatal("Missing devices configurations")
    }
    if len(Config.Devices) == 0 {
        log.Fatal("Empty devices configurations")
    }
    Config.devices = make(map[string]*Device, len(Config.Devices))
    for _, device := range Config.Devices {
        if len(device.Name) == 0 {
            log.Fatalf("Missing device name")
        }
        checkInt("number of lines for device " + device.Name, device.NumLines)
        if device.LineStart < 0 || device.LineStart + device.NumLines > Config.NumLines {
            log.Fatalf("Invalid configuration for device %v", device.Name)
        }
        if device.Priority < 0 || device.Priority > 1 {
            log.Fatalf("Invalid priority for device %v", device.Name)
        }
        Config.devices[device.Name] = device
    }

    Config.holdTime = time.Duration(Config.HoldTime) * time.Second
}

func GetDeviceConfiguration(deviceName string) *Device {
    config, ok := Config.devices[deviceName]
    if !ok {
        return nil
    }
    return config
}