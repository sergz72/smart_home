package core

import (
    "log"
    "math"
    "pumpControl/src/devices"
    "time"
)

const (
    R1 = 3300.0
    R2 = 2200.0
)

type adcResult struct {
    pres    float64
    current float64
}

var (
	device devices.Ads1115
    prevResult adcResult
)

func adcInit() error {
    return device.Init(config.I2cPortName)
}

func adcClose() {
    device.Close()
}

func calculatePressure(adcValue float64) float64 {
    if adcValue < 0.5 {
        return 0
    }
    return math.Round((adcValue - 0.5) * 120.0 / 4.0) / 10.0
}

//1A = 0.06
//2A = 0.25
//3A = 0.44
// adcValue = 0.06 + I * 0.19 - 0.19 = I * 0.19 - 0.13
// I = (adcValue + 0.13) / 0.19
func calculateCurrent(adcValue float64) float64 {
    if adcValue < 0.1 { //noise
        return 0
    }
    i := (adcValue + 0.13) / 0.19
    return math.Round(i * 2) / 2
}

func adcLoop(adcCh chan adcResult, shCh chan adcResult, checkInterval time.Duration, smartHomeInterval time.Duration) {
    var t time.Duration = 0
    for {
        v, err := device.ReadCurrentChannel()
        if err != nil {
            log.Printf("ReadCurrentChannel error %v\n", err)
            continue
        }
        c := calculateCurrent(v)
        //log.Printf("CurrentChannel value %v\n", v)
        //log.Printf("CurrentChannel converted value %v\n", c)
        p := 0.0
        if config.PressureSensorIsPresent {
            v, err = device.ReadPressureChannel()
            if err != nil {
                log.Printf("ReadPressureChannel error %v\n", err)
                continue
            }
            v = v * (R1 + R2) / R1
            p = calculatePressure(v)
            //log.Printf("PressureChannel value %v\n", v)
            //log.Printf("PressureChannel converted value %v\n", p)
        }
        r := adcResult{
            pres:    p,
            current: c,
        }
        if config.PressureSensorIsPresent {
            adcCh <- r
        }
        if t >= smartHomeInterval {
            t = 0
            if prevResult.pres != r.pres || prevResult.current != r.current {
                prevResult = r
                shCh <- r
            }
        }
        time.Sleep(checkInterval)
        t += checkInterval
    }
}
