package core

import (
    "devices"
    "encoding/json"
    "golang.org/x/sys/unix"
    "io/ioutil"
    "log"
    "strings"
)

type deviceHandler interface {
    Connect(file devices.DeviceFile, config devices.DeviceConfig)
    Disconnect()
    GetFile() devices.DeviceFile
    CommandHandler(command []byte) []byte
}

type device struct {
    Name string
    Vid string
    Pid string
    Serial string
    Speed int
    Parameters devices.DeviceConfig
    Emulate bool
    baud uint32
    handler deviceHandler
    emulator devices.Emulator
    active bool
}

type configuration struct {
    Port int
    LogLevel string
    LogFileName string
    DisplayUrl string
    Devices []*device
}

var bauds = map[int]uint32{
    50:      unix.B50,
    75:      unix.B75,
    110:     unix.B110,
    134:     unix.B134,
    150:     unix.B150,
    200:     unix.B200,
    300:     unix.B300,
    600:     unix.B600,
    1200:    unix.B1200,
    1800:    unix.B1800,
    2400:    unix.B2400,
    4800:    unix.B4800,
    9600:    unix.B9600,
    19200:   unix.B19200,
    38400:   unix.B38400,
    57600:   unix.B57600,
    115200:  unix.B115200,
    230400:  unix.B230400,
    460800:  unix.B460800,
    500000:  unix.B500000,
    576000:  unix.B576000,
    921600:  unix.B921600,
    1000000: unix.B1000000,
    1152000: unix.B1152000,
    1500000: unix.B1500000,
    2000000: unix.B2000000,
    2500000: unix.B2500000,
    3000000: unix.B3000000,
    3500000: unix.B3500000,
    4000000: unix.B4000000,
}

var config configuration

func readFileAndSkipComments(fileName string) ([]byte, error) {
    fileContents, err := ioutil.ReadFile(fileName)
    if err != nil {
        return nil, err
    }
    lines := strings.Split(string(fileContents), "\n")
    var sb strings.Builder
    for _, line := range lines {
        if !strings.HasPrefix(line, "//") {
            _, err = sb.WriteString(line)
            if err != nil {
                return nil, err
            }
            _, err := sb.WriteString("\n")
            if err != nil {
                return nil, err
            }
        }
    }
    return []byte(sb.String()), nil
}

func loadConfiguration(iniFileName string) bool {
    iniFileContents, err := readFileAndSkipComments(iniFileName)
    if err != nil {
        log.Fatal(err)
    }
    err = json.Unmarshal(iniFileContents, &config)
    if err != nil {
        log.Fatal(err)
    }

    devices.DisplayUrl = config.DisplayUrl

    for _, d := range config.Devices {
        d.active = false
        switch d.Name {
        case "LCMeter":
            d.handler = &devices.LCMeter{}
            d.emulator = nil
        case "Generator_ad9833":
            d.handler = &devices.GeneratorAD9833{}
            d.emulator = nil
        case "Oven":
            d.handler = &devices.Oven{}
            if d.Emulate {
                d.emulator = &devices.OvenEmulator{}
                d.emulator.Init()
                forceConnect(d)
            } else {
                d.emulator = nil
            }
        case "DrillIronC":
            d.handler = &devices.DrillIronC{}
            if d.Emulate {
                d.emulator = &devices.DrillIronCEmulator{}
                d.emulator.Init()
                forceConnect(d)
            } else {
                d.emulator = nil
            }
        default:
            log.Panicf("Unknown device name: %v", d.Name)
        }
        baud, ok := bauds[d.Speed]
        if !ok {
            log.Panicf("Invalid comm port speed: %v", d.Speed)
        }
        d.baud = baud
    }

    return true
}

func findDevice(dp *deviceParameters) *device {
    for _, d := range config.Devices {
        if d.Serial == dp.serial && d.Pid == dp.productId && d.Vid == dp.vendorId {
            return d
        }
    }

    return nil
}

func findDeviceByName(deviceName string) *device {
    for _, d := range config.Devices {
        if d.Name == deviceName {
            return d
        }
    }

    return nil
}