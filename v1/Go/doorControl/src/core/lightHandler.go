package core

import (
    "log"
    "periph.io/x/periph/conn/gpio"
    "time"
)

var endTime int64 = 0
var pinLevel gpio.Level

func lightHandlerInit(pin gpio.PinIO) {
    pinLevel = gpio.High
    go func() {
       for duration := range lightCh {
           if duration > 0 {
               endTime = time.Now().Unix() + int64(duration)
           } else {
               endTime = 0
           }
       }
    }()
    go lightProc(pin)
}

func lightProc(pin gpio.PinIO) {
    for {
        t := time.Now().Unix()
        if t <= endTime {
            if pinLevel == gpio.High {
                log.Println("light is ON")
                pinLevel = gpio.Low
                if pin != nil {
                    _ = pin.Out(pinLevel)
                }
            }
        } else {
            if pinLevel == gpio.Low {
                log.Println("light is OFF")
                pinLevel = gpio.High
                if pin != nil {
                    _ = pin.Out(pinLevel)
                }
            }
        }
        time.Sleep(time.Second)
    }
}

