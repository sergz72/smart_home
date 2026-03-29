package devices

import (
    "fmt"
    "log"
    "strconv"
    "time"
)

type LCMeter struct {
    file DeviceFile
    stop bool
}

func (d *LCMeter) Connect(file DeviceFile, parameters DeviceConfig) {
    d.file = file
    d.stop = false
    buf := make([]byte, 40)
    for !d.stop {
        _, err := d.file.Write([]byte{'F'})
        time.Sleep(250 * time.Millisecond)
        if err != nil {
            log.Println(err)
            displayLCMeterError("e0001")
        } else {
            var n int
            n, err = d.file.Read(buf)
            if err != nil {
                log.Println(err)
                displayLCMeterError("e0002")
            } else {
                if n == 0 {
                    log.Println("LMeter: No answer from device")
                    displayLCMeterError("e0003")
                } else {
                    n, err = strconv.Atoi(string(buf[:n]))
                    if err != nil {
                        log.Println(err)
                        displayLCMeterError("e0004")
                    } else {
                        displayLCMeterResult(n, parameters.C)
                    }
                }
            }
        }
    }
}

func displayLCMeterResult(f int, c int) {
    F := float64(f) / 1000000.0
    L := 25530.0 / F / F / float64(c)
    var result string
    if L < 10 {
        result = fmt.Sprintf("%.2fu", L)
    } else if L < 100 {
        result = fmt.Sprintf("%.1fu", L)
    } else if L < 1000 {
        result = fmt.Sprintf("%4du", int(L))
    } else if L < 10000 {
        result = fmt.Sprintf("%.2fm", L / 1000)
    } else if L < 100000 {
        result = fmt.Sprintf("%.1fm", L / 1000)
    } else if L < 10000000 {
        result = fmt.Sprintf("%4dm", int(L / 1000))
    } else {
        result = "HI   "
    }
    postResult("LCMeter", []TextLine{{16, result}}) // Green
}

func displayLCMeterError(message string) {
    postResult("LCMeter", []TextLine{{256, message}}) // Red
}

func (d *LCMeter) Disconnect() {
    d.stop = true
}

func (d *LCMeter) GetFile() DeviceFile {
    return d.file
}

func (d *LCMeter) CommandHandler(command []byte) []byte {
    return []byte("Not supported")
}