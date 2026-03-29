package devices

import (
    "bytes"
    "crypto/cipher"
    "encoding/binary"
    "fmt"
    "log"
    "time"
)

type TempHumiSensor struct {
    Timestamp int32
    Temperature int16
    BatteryVoltage uint16
    RSSI int16
    LQI byte
    Humidity byte
    Magic [4]byte
}

type tempHumiSensorOut struct {
    Timestamp int32
    ExtTemperatureW int16
    ExtTemperatureN int16
    Magic [8]byte
}

func (s *TempHumiSensor) ReadData(sensorData []SensorData, block cipher.Block, device RFDevice, address []byte,
                                  parameters DeviceConfig) DeviceResponse {
    var extTempN int16 = -999;
    var extTempW int16 = -999;
    if sensorData != nil {
        for _, item := range sensorData {
            item := item
            if item.LocationName == parameters.SensorDataKeyW {
                extTempW = int16(item.Data.Temp * 10)
            } else if item.LocationName == parameters.SensorDataKeyN {
                extTempN = int16(item.Data.Temp * 10)
            }
        }
    }
    if extTempN == -999 {
        log.Printf("error: no sensor data found for %v", parameters.SensorDataKeyN)
    }
    if extTempW == -999 {
        log.Printf("error: no sensor data found for %v", parameters.SensorDataKeyW)
    }
    err := device.SetNumberOfBytesToReceive(16)
    if err != nil {
        return DeviceResponse{ Error: err }
    }
    t := time.Now()
    _, offset := t.Zone();
    outData := tempHumiSensorOut{
        int32(t.Unix()) + int32(offset),
        extTempW,
        extTempN,
        [8]byte{ 0x55, 0xAA, 0x11, 0x22, 0x33, 0x44, 0x55, 0x77 },
    }
    //log.Printf("outData.Timestamp: %v\n", outData.Timestamp)
    var buffer bytes.Buffer
    binary.Write(&buffer, binary.LittleEndian, outData)
    encryptedData := make([]byte, 16)
    block.Encrypt(encryptedData, buffer.Bytes())
    //log.Printf("txData: %v\n", encryptedData)
    err = device.Transmit(address, encryptedData)
    if err != nil {
        return DeviceResponse{ Error: err }
    }
    time.Sleep(time.Duration(500) * time.Millisecond)
    data, err := device.Receive()
    if err != nil {
        return DeviceResponse{ Error: err }
    }
    if data == nil {
        return DeviceResponse{ Error: fmt.Errorf("no response")}
    }
    //log.Printf("rxData: %v\n", data)
    block.Decrypt(encryptedData, data)
    buffer.Truncate(0)
    buffer.Write(encryptedData)
    binary.Read(&buffer, binary.LittleEndian, s)
    if s.Timestamp != outData.Timestamp {
        log.Printf("s.Timestamp: %v, outData.Timestamp: %v\n", s.Timestamp, outData.Timestamp)
        return DeviceResponse{Error: fmt.Errorf("invalid timestamp")}
    }
    resultEnv := fmt.Sprintf("{\"temp\":%v.%v, \"humi\":%v}",
                             s.Temperature / 10, s.Temperature % 10, s.Humidity)
    resultEle := fmt.Sprintf("{\"vbat\":%v}", s.BatteryVoltage)
    return DeviceResponse{resultEnv, resultEle, int(s.RSSI), int(s.LQI), nil }
}