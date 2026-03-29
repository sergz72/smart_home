package core

import (
    "bytes"
    "devices"
    "encoding/binary"
    "encoding/json"
    "fmt"
    "log"
    "net"
    "net/http"
    "periph.io/x/periph/host"
    "strings"
    "time"
)

func Init(iniFileName string, testExtTemp float64) {
    _, err := host.Init()
    if err != nil {
        log.Fatal(err)
    }
    err = loadConfiguration(iniFileName, testExtTemp)
    if err != nil {
        log.Fatal(err)
    }
}

func udpSend(key []byte, address string, command string) ([]byte, error) {
    data, err := AesEncode([]byte(command), key, false, func(length int) ([]byte, error) {
        if length < 8 {
            return nil, fmt.Errorf("nonce size must be >= 8")
        }
        millis := uint64(time.Now().UnixNano() / 1000000)
        bMillis := make([]byte, 8)
        binary.LittleEndian.PutUint64(bMillis, millis)
        randPart, err := GenerateRandomBytes(length - 6)
        if err != nil {
            return nil, err
        }
        return append(randPart, bMillis[0], bMillis[1], bMillis[2], bMillis[3], bMillis[4], bMillis[5]), nil
    })
    if err != nil {
        return nil, err
    }
    raddr, err := net.ResolveUDPAddr("udp", address)
    if err != nil {
        return nil, err
    }

    var conn *net.UDPConn
    conn, err = net.DialUDP("udp", nil, raddr)
    if err != nil {
        return nil, err
    }
    defer conn.Close()
    err = conn.SetReadBuffer(65507)
    if err != nil {
        return nil, err
    }
    _, err = conn.Write(data)
    if err != nil {
        return nil, err
    }
    p := make([]byte, 65507)
    conn.SetReadDeadline(time.Now().Add(time.Second))
    var n int
    n, err = conn.Read(p)
    if err != nil {
        return nil, err
    }
    return AesDecode(p[:n], key, true, nil)
}

func getSensorData() ([]devices.SensorData, error) {
    response, err := udpSend(config.smartHomeKey, config.SmartHomeAddress, "GET " + config.SmartHomeCommand)
    if err != nil {
        return nil, err
    }
    var responseObject []devices.SensorData
    err = json.Unmarshal(response, &responseObject)
    if err != nil {
        return nil, err
    }
    return responseObject, nil
}

func Loop() {
    pollInterval := time.Duration(config.PollInterval) * time.Second
    rfDevicePowerStatus := make(map[string]bool, len(config.RFDevices))
    var sensorData []devices.SensorData = nil
    for {
        start := time.Now()
        log.Println("Retrieving data from smart home service...")
        newSensorData, err := getSensorData()
        if err == nil {
            log.Println("Success.")
            sensorData = newSensorData
        } else {
            log.Printf("Failure: %v\n", err.Error())
        }
        if config.testExtTemp != -999 {
            for i := 0; i < len(sensorData); i++ {
                sensorData[i] = devices.SensorData{LocationName: sensorData[i].LocationName, Data: devices.DataMap{  Temp: config.testExtTemp }}
            }
        }
        log.Println("Starting device polling...")
        for _, d := range config.Devices {
            if !d.Active {
                continue
            }
            rfDevice := config.rFDeviceMap[d.RFDeviceName]
            _, ok := rfDevicePowerStatus[d.RFDeviceName]
            if !ok {
                err := rfDevice.PowerControl(true)
                if err != nil {
                    log.Printf("RF device %v power up error: %v\n", d.RFDeviceName, err.Error())
                    continue
                }
                rfDevicePowerStatus[d.RFDeviceName] = true
            }
            rfDevice.SetTXPower(d.TXPower)
            retryCount := config.RetryCount
            if retryCount <= 0 {
                retryCount = 1
            }
            var response devices.DeviceResponse
            retries := 0
            for retries < retryCount {
                response = d.handler.ReadData(sensorData, config.block, rfDevice, d.Address, d.Parameters)
                if response.Error == nil {
                    break
                }
                retries++
            }
            if response.Error == nil {
                log.Printf("Response from device(%v retries, tx power: %v) %v: %v\n", retries, d.TXPower, d.Name, response.ToString())
                RSSI, err := rfDevice.GetLastRSSI()
                if err != nil {
                    log.Printf("GetRSSI error: %v\n", err)
                }
                var LQI int
                LQI, err = rfDevice.GetLastLQI()
                if err != nil {
                    log.Printf("GetLQI error: %v\n", err)
                }
                log.Printf("RSSI: %v, LQI: %v, device RSSI: %v, device LQI: %v\n", RSSI, LQI, response.RSSI, response.LQI)
                sendSensorData(response, d)
            } else {
                log.Printf("Error reading data from device(tx power: %v) %v: %v\n", d.TXPower, d.Name, response.Error.Error())
            }
        }
        elapsed := time.Since(start)
        if elapsed < pollInterval {
            for name, _ := range rfDevicePowerStatus {
                config.rFDeviceMap[name].PowerControl(false)
            }
            rfDevicePowerStatus = make(map[string]bool, len(config.RFDevices))
            time.Sleep(pollInterval - elapsed)
        }
    }
}

func sendToSensorData(sensorName string, data string) {
    if len(sensorName) > 0 && len(data) > 0 {
        log.Printf("Sending sensor data for %v\n", sensorName)
        url := config.SensorDataURL + sensorName
        response, err := http.Post(url, "application/json", strings.NewReader(data))
        if err != nil {
            log.Printf("HTTP post error(URL: %v): %v", url, err.Error())
        } else {
            buf := new(bytes.Buffer)
            buf.ReadFrom(response.Body)
            log.Printf("HTTP post response: status: %v, body: %v\n", response.StatusCode, buf.String())
        }
    }
}

func sendSensorData(response devices.DeviceResponse, device *device) {
    if len(config.SensorDataURL) > 0 {
        sendToSensorData(device.EnvSensorName, response.ResponseEnv)
        sendToSensorData(device.EleSensorName, response.ResponseEle)
    }
}