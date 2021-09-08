package core

import (
    "log"
    "periph.io/x/periph/conn/gpio"
    "time"
)

var (
	expectedTemp float64
    setTemp      float64
    lastTemp     float64
    pinState     gpio.Level
)

type heaterStatus struct {
    SetTemp   int
    LastTemp  int
    HeaterOn  bool
    DayTemp   int
    NightTemp int
}

func buildHeaterStatus() *heaterStatus {
    status := heaterStatus{
        SetTemp:   int(setTemp * 10),
        LastTemp:  int(lastTemp * 10),
        HeaterOn:  heaterIsOn(),
        DayTemp:   int(config.DayTemp * 10),
        NightTemp: int(config.NightTemp * 10),
    }
    return &status
}

func heaterHandlerInit(t time.Time) {
    expectedTemp = getExpectedTemp(t)
    setTemp = expectedTemp
    pinState = gpio.High
}

func heaterIsOn() bool {
    return pinState == gpio.Low
}

func heaterHandler(pin gpio.PinIO, temp float64, t time.Time) {
    //log.Printf("heaterHandler %v", temp)
    lastTemp = temp
    oldTemp := expectedTemp
    expectedTemp = getExpectedTemp(t)
    if expectedTemp != oldTemp {
        setTemp = expectedTemp
    }
    if heaterIsOn() { // heater on
        if temp >= setTemp + config.Delta {
            heaterOff(temp, pin)
        }
    } else { // heater off
        if temp < setTemp {
            heaterOn(temp, pin)
        }
    }
}

func heaterOn(temp float64, pin gpio.PinIO) {
    pinState = gpio.Low
    if pin != nil {
        _ = pin.Out(pinState)
    }
    log.Printf("%v heater on", temp)
}

func heaterOff(temp float64, pin gpio.PinIO) {
    pinState = gpio.High
    if pin != nil {
        _ = pin.Out(pinState)
    }
    log.Printf("%v heater off", temp)
}

func getExpectedTemp(t time.Time) float64 {
    d  := t.Day()
    m  := t.Month()
    wd := t.Weekday()
    for _, e := range config.Exceptions {
        if d == e.Day && m == e.month {
            return config.NightTemp
        }
    }
    for _, dw := range config.dayDaysOfWeek {
        if wd == dw {
            return checkDayTime(t)
        }
    }
    return config.NightTemp
}

func checkDayTime(t time.Time) float64 {
    h := t.Hour()
    m := t.Minute()
    if h < config.DayStartHour || h > config.DayEndHour {
        return config.NightTemp
    }
    if h == config.DayStartHour {
        if m < config.DayStartMinute {
            return config.NightTemp
        }
    }
    if h == config.DayEndHour {
        if m > config.DayEndMinute {
            return config.NightTemp
        }
    }
    return config.DayTemp
}
