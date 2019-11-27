package widget

import (
    "core"
    "encoding/binary"
    "encoding/json"
    "fmt"
    "io/ioutil"
    "net"
    "time"
)

type DataMap struct {
    Temp float64
}
type SensorData struct {
    LocationName string
    Data DataMap
}

func udpSend(key []byte, address string, command string) ([]byte, error) {
    data, err := core.AesEncode([]byte(command), key, false, func(length int) ([]byte, error) {
        if length < 8 {
            return nil, fmt.Errorf("nonce size must be >= 8")
        }
        millis := uint64(time.Now().UnixNano() / 1000000)
        bMillis := make([]byte, 8)
        binary.LittleEndian.PutUint64(bMillis, millis)
        randPart, err := core.GenerateRandomBytes(length - 6)
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
    return core.AesDecode(p[:n], key, true, nil)
}

func Env() {
    key, err := ioutil.ReadFile(core.Config.KeyFileName)
    if err != nil {
        core.Queue <- core.DisplayMessage{CommandType: 1, Show: core.ShowDataCommand{DeviceName: "env",
            Messages: []core.TextLine{{Color: 256, Text: "errk"}}}}
        fmt.Print(err.Error())
        return
    }
    for {
        response, err := udpSend(key, core.Config.SmartHomeAddress, "GET " + core.Config.SmartHomeCommand)
        if err == nil {
            var responseObject []SensorData
            err = json.Unmarshal(response, &responseObject)
            if err == nil {
                var i *SensorData = nil
                for _, item := range responseObject {
                    if item.LocationName == "Северная сторона дома" {
                        i = &item
                    }
                }
                if i != nil {
                    core.Queue <- core.DisplayMessage{CommandType: 1, Show: core.ShowDataCommand{DeviceName: "env",
                                                      Messages: []core.TextLine{{Color: 257, Text: fmt.Sprintf("%5.1f", i.Data.Temp)}}}}
                } else {
                    core.Queue <- core.DisplayMessage{CommandType: 1, Show: core.ShowDataCommand{DeviceName: "env",
                                                      Messages: []core.TextLine{{Color: 256, Text: "err4"}}}}
                }
            } else {
                core.Queue <- core.DisplayMessage{CommandType: 1, Show: core.ShowDataCommand{DeviceName: "env",
                                                  Messages: []core.TextLine{{Color: 256, Text: "err3"}}}}
                fmt.Println(string(response))
                fmt.Println(err.Error())
            }
        } else {
            core.Queue <- core.DisplayMessage{CommandType: 1, Show: core.ShowDataCommand{DeviceName: "env",
                                              Messages: []core.TextLine{{Color: 256, Text: "err1"}}}}
            fmt.Print(err.Error())
        }
        time.Sleep(5 * time.Minute)
    }
}

