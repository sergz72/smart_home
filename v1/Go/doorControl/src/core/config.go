package core

import (
    "encoding/json"
    "errors"
    "io/ioutil"
)

type configuration struct {
    LightGpioName    string
    DoorGpioName     string
    KeyFile          string
    key              []byte
    UdpPort          int
    FailOnUnknownPin bool
}

var config configuration

func loadConfiguration(_iniFileName string) error {
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
    if config.LightGpioName == "" {
        return errors.New("LightGpioName is missing or empty")
    }
    if config.DoorGpioName == "" {
        return errors.New("DoorGpioName is missing or empty")
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
