package core

import (
    "encoding/json"
    "errors"
    "io/ioutil"
)

type configuration struct {
    I2cPortName             string
    GpioName                string
    CheckIntervalMs         int
    SmartHomeInterval       int
    MinPressure             float64
    MaxPressure             float64
    SmartHomeWatUrl         string
    FailOnUnknownPin        bool
    PressureSensorIsPresent bool
    KeyFile                 string
    key                     []byte
    UdpPort                 int
}

var config configuration
var iniFileName string

func loadConfiguration(_iniFileName string) error {
    iniFileName = _iniFileName
    iniFileContents, err := ioutil.ReadFile(_iniFileName)
    if err != nil {
        return err
    }
    err = json.Unmarshal(iniFileContents, &config)
    if err != nil {
        return err
    }
    return validateConfig()
}

func validateConfig() error {
    if config.GpioName == "" {
        return errors.New("GpioName is missing or empty")
    }
    if config.CheckIntervalMs <= 0 {
        return errors.New("wrong CheckIntervalMs value")
    }
    if config.SmartHomeInterval * 1000 < config.CheckIntervalMs {
        return errors.New("wrong SmartHomeInterval value")
    }
    if config.MinPressure <= 0 {
        return errors.New("wrong MinPressure value")
    }
    if config.MaxPressure <= 0 {
        return errors.New("wrong MaxPressure value")
    }
    if config.SmartHomeWatUrl == "" {
        return errors.New("SmartHomeWatUrl is missing or empty")
    }
    if config.UdpPort <= 0 {
        return errors.New("wrong UdpPort value")
    }
    if config.KeyFile == "" {
        return errors.New("KeyFile is missing or empty")
    }
    var err error
    config.key, err = ioutil.ReadFile(config.KeyFile)
    return err
}
