package core

import (
    "log"
    "periph.io/x/periph/conn/gpio"
    "time"
)

func doorHandlerInit(pin gpio.PinIO) {
    go func() {
        for {
            select {
            case <-doorCh:
                if pin != nil {
                    _ = pin.Out(gpio.Low)
                }
                time.Sleep(time.Second)
                log.Println("door is open")
                if pin != nil {
                    _ = pin.Out(gpio.High)
                }
                time.Sleep(time.Second)
            }
        }
    }()

}

