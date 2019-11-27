package widget

import (
    "core"
    "fmt"
    "golang.org/x/sys/unix"
    "math"
    "os"
    "time"
)

const I2cSlave = 0x0703

func Max44009() {
    deviceHandle, err := os.OpenFile(core.Config.Max44009.DeviceName, unix.O_RDWR, 0666)
    if err != nil {
        fmt.Printf("Device %v open error: %v\n", core.Config.Max44009.DeviceName, err.Error())
        return
    }
    defer deviceHandle.Close()
    err = unix.IoctlSetInt(int(deviceHandle.Fd()), I2cSlave, core.Config.Max44009.Address)
    if err != nil {
        fmt.Printf("Device %v ioctl error: %v\n", core.Config.Max44009.DeviceName, err.Error())
        return
    }
    data2 := make([]byte, 2)
    data2[0] = 0x03
    data2[1] = 0x00
    _, err = deviceHandle.Write(data2)
    if err != nil {
        fmt.Printf("Device %v config write error: %v\n", core.Config.Max44009.DeviceName, err.Error())
        return
    }
    data1 := make([]byte, 1)
    prevState := true
    for {
        data1[0] = 0x03
        _, err = deviceHandle.Write(data1)
        if err != nil {
            fmt.Printf("Device %v write error: %v\n", core.Config.Max44009.DeviceName, err.Error())
            return
        }
        _, err = deviceHandle.Read(data1)
        if err != nil {
            fmt.Printf("Device %v read error: %v\n", core.Config.Max44009.DeviceName, err.Error())
            return
        }
        msb := data1[0]
        data1[0] = 0x04
        _, err = deviceHandle.Write(data1)
        if err != nil {
            fmt.Printf("Device %v write error: %v\n", core.Config.Max44009.DeviceName, err.Error())
            return
        }
        _, err = deviceHandle.Read(data1)
        if err != nil {
            fmt.Printf("Device %v read error: %v\n", core.Config.Max44009.DeviceName, err.Error())
            return
        }
        lsb := data1[0]
        exponent := float64((msb & 0xF0) >> 4)
        mantissa := float64(((msb & 0x0F) << 4) | (lsb & 0x0F))
        luminance := math.Pow(2, exponent) * mantissa * 0.045
//        fmt.Printf("Ambient Light luminance : %v lux\n", luminance)
        if luminance >= core.Config.Max44009.Max && !prevState {
            core.Queue <- core.DisplayMessage{CommandType: 0, Command: core.DisplayCommand{ Command: "on" }}
            prevState = true
        } else if luminance <= core.Config.Max44009.Min && prevState {
            core.Queue <- core.DisplayMessage{CommandType: 0, Command: core.DisplayCommand{ Command: "off" }}
            prevState = false
        }
        time.Sleep(3 * time.Second)
    }
}
