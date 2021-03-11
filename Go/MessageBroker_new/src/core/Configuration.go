package core

import (
    "encoding/json"
    "fmt"
    "io/ioutil"
    "strconv"
)

type queueParameters struct {
    Size int
    ExpirationTime string
    MaxResponseSize int
    expirationTime int
}

type configuration struct {
    KeyFileName string
    InPort int
    OutPort int
    QueueParameters queueParameters
}

func loadConfiguration(configFileName string) (*configuration, error) {
    iniFileContents, err := ioutil.ReadFile(configFileName)
    if err != nil {
        return nil, err
    }
    var config configuration
    err = json.Unmarshal(iniFileContents, &config)
    if err != nil {
        return nil, err
    }
    if len(config.KeyFileName) == 0 || config.InPort <= 0 || config.OutPort <= 0 || config.QueueParameters.Size <= 0 ||
       len(config.QueueParameters.ExpirationTime) < 2 {
        return nil, fmt.Errorf("invalid configuration")
    }
    expirationTime, err := strconv.Atoi(config.QueueParameters.ExpirationTime[:len(config.QueueParameters.ExpirationTime)  - 1])
    if err != nil {
        return nil, err
    }
    if expirationTime <= 0 {
        return nil, fmt.Errorf("invalid expiration time value")
    }
    switch config.QueueParameters.ExpirationTime[len(config.QueueParameters.ExpirationTime)  - 1] {
    case 'm':
        config.QueueParameters.expirationTime = expirationTime * 60
    case 'h':
        config.QueueParameters.expirationTime = expirationTime * 60 * 60
    case 'd':
        config.QueueParameters.expirationTime = expirationTime * 60 * 60 * 24
    }
    return &config, nil
}
