package devices

import "encoding/json"

type DeviceConfig struct {
    SensorDataKeyW string
    SensorDataKeyN string
}

type RFDevice interface {
    Transmit(address []byte, data []byte) error
    Receive() ([]byte, error)
    PowerControl(powerUp bool) error
    SetTXPower(txPower int) error
    SetNumberOfBytesToReceive(numBytes byte) error
    GetLastRSSI() (int, error)
    GetLastLQI() (int, error)
}

type DeviceResponse struct {
    ResponseEnv string
    ResponseEle string
    RSSI int
    LQI int
    Error error
}

func (r DeviceResponse) ToString() string {
    b, _ := json.Marshal(r)
    return string(b)
}

type DataMap struct {
    Temp float64
}

type SensorData struct {
    LocationName string
    Data DataMap
}
