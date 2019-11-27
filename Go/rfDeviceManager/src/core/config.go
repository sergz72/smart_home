package core

import (
    "crypto/aes"
    "crypto/cipher"
    "devices"
    "encoding/json"
    "fmt"
    "io/ioutil"
)

type deviceHandler interface {
    ReadData(sensorData []devices.SensorData, block cipher.Block, device devices.RFDevice, address []byte,
             parameters devices.DeviceConfig) devices.DeviceResponse
}

type device struct {
    Name string
    Class string
    RFDeviceName string
    Address []byte
    TXPower int
    Parameters devices.DeviceConfig
    EnvSensorName string
    EleSensorName string
    Active bool
    handler deviceHandler
}

type rFDeviceConfig struct {
    ClassName string
    DeviceName string
    ConfigFileName string
}

type configuration struct {
    RFDevices []rFDeviceConfig
    PollInterval int
    RetryCount int
    KeyFileName string
    SmartHomeAddress string
    SmartHomeCommand string
    SmartHomeKeyFileName string
    SensorDataURL string
    Devices []*device
    rFDeviceMap map[string]devices.RFDevice
    smartHomeKey []byte
    block cipher.Block
    testExtTemp float64
}

var config configuration

func loadConfiguration(iniFileName string, testExtTemp float64) error {
    iniFileContents, err := ioutil.ReadFile(iniFileName)
    if err != nil {
        return err
    }
    err = json.Unmarshal(iniFileContents, &config)
    if err != nil {
        return err
    }

    var aesKey []byte
    aesKey, err = ioutil.ReadFile(config.KeyFileName)
    if err != nil {
        return err
    }
    config.block, err = aes.NewCipher(aesKey)
    if err != nil {
        return err
    }

    config.smartHomeKey, err = ioutil.ReadFile(config.SmartHomeKeyFileName)
    if err != nil {
        return err
    }

    rfDeviceConfigMap := make(map[string]rFDeviceConfig)
    for _, d := range config.RFDevices {
        rfDeviceConfigMap[d.DeviceName] = d
    }

    config.rFDeviceMap = make(map[string]devices.RFDevice, len(config.RFDevices))

    for _, d := range config.Devices {
        switch d.Class {
        case "TempHumiSensor":
            d.handler = &devices.TempHumiSensor{}
        default:
            return fmt.Errorf("unknown device class: %v", d.Class)
        }
        err = loadRfDevice(d.RFDeviceName, rfDeviceConfigMap)
        if err != nil {
            return err
        }
    }

    config.testExtTemp = testExtTemp

    return nil
}

func loadRfDevice(deviceName string, rfDeviceConfigMap map[string]rFDeviceConfig) error {
    _, ok := config.rFDeviceMap[deviceName]
    if !ok {
        var deviceConfig rFDeviceConfig
        deviceConfig, ok = rfDeviceConfigMap[deviceName]
        if !ok {
            return fmt.Errorf("unknown rf device: %v", deviceName)
        }
        var class devices.RFDevice
        var err error
        switch deviceConfig.ClassName {
        case "NRF24": class, err = NRF24Init(deviceConfig.ConfigFileName)
        case "CC1101": class, err = CC1101Init(deviceConfig.ConfigFileName)
        default: return fmt.Errorf("unknown rf device class: %v", deviceConfig.ClassName)
        }
        if err != nil {
            return err
        }
        config.rFDeviceMap[deviceName] = class
    }
    return nil
}
