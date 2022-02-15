package core

import (
    "log"
    "periph.io/x/periph/conn/gpio"
    "time"
)

var (
    pinState  gpio.Level
    pumpState pumpStatus
)

type pumpStatus struct {
    MinPressure  int
    MaxPressure  int
    LastPressure int
    PumpIsOn     bool
    ForcedOff    bool
    LastPumpOn   int64
    LastPumpOff  int64
}

func buildPumpStatus() *pumpStatus {
    return &pumpState
}

func pumpHandlerInit() {
    pinState = gpio.Low
    pumpState.MinPressure = int(config.MinPressure * 10)
    pumpState.MaxPressure = int(config.MaxPressure * 10)
}

func pumpIsOn() bool {
    return pinState == gpio.High
}

func pumpHandler(pin gpio.PinIO, r adcResult) {
    pumpState.LastPressure = int(r.pres * 10)
    if pumpIsOn() {
        if pumpState.ForcedOff || r.pres >= config.MaxPressure {
            pumpOff(r.pres, pin)
        }
    } else {
        if !pumpState.ForcedOff && r.pres <= config.MinPressure {
            pumpOn(r.pres, pin)
        }
    }
}

func pumpOn(pres float64, pin gpio.PinIO) {
    pinState = gpio.High
    pumpState.PumpIsOn = true
    pumpState.LastPumpOn = time.Now().Unix()
    if pin != nil {
        _ = pin.Out(pinState)
    }
    log.Printf("%v pump on", pres)
}

func pumpOff(pres float64, pin gpio.PinIO) {
    pinState = gpio.Low
    pumpState.PumpIsOn = false
    pumpState.LastPumpOff = time.Now().Unix()
    if pin != nil {
        _ = pin.Out(pinState)
    }
    log.Printf("%v pump off", pres)
}
