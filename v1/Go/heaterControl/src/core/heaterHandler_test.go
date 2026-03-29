package core

import (
    "periph.io/x/periph/conn/gpio"
    "periph.io/x/periph/conn/physic"
    "testing"
    "time"
)

type testPin struct{}
var testLevel gpio.Level

func (t testPin) String() string {
    return "testPin"
}

func (t testPin) Halt() error {
    return nil
}

func (t testPin) Name() string {
    return "testPin"
}

func (t testPin) Number() int {
    return 0
}

func (t testPin) Function() string {
    return ""
}

func (t testPin) In(_ gpio.Pull, _ gpio.Edge) error {
    return nil
}

func (t testPin) Read() gpio.Level {
    return gpio.Low
}

func (t testPin) WaitForEdge(_ time.Duration) bool {
    return false
}

func (t testPin) Pull() gpio.Pull {
    return gpio.PullUp
}

func (t testPin) DefaultPull() gpio.Pull {
    return gpio.PullUp
}

func (t testPin) Out(l gpio.Level) error {
    testLevel = l
    return nil
}

func (t testPin) PWM(_ gpio.Duty, _ physic.Frequency) error {
    return nil
}

func setup() {
    config.NightTemp = 18
    config.DayTemp = 21
    config.DayStartHour = 7
    config.DayStartMinute = 30
    config.DayEndHour = 20
    config.DayEndMinute = 59
    config.Exceptions = []exception{
        {month: time.September, Day: 10},
    }
    config.dayDaysOfWeek = []time.Weekday{time.Monday, time.Tuesday, time.Wednesday, time.Thursday, time.Friday}
}

func TestCheckDayTime(t *testing.T) {
    setup()
    if checkDayTime(time.Date(2022, time.January, 1, 6, 50, 0, 0, time.Local)) != config.NightTemp {
        t.Fatal("6.50 fail")
    }
    if checkDayTime(time.Date(2022, time.January, 1, 7, 29, 0, 0, time.Local)) != config.NightTemp {
        t.Fatal("7.29 fail")
    }
    if checkDayTime(time.Date(2022, time.January, 1, 7, 30, 0, 0, time.Local)) != config.DayTemp {
        t.Fatal("7.30 fail")
    }
    if checkDayTime(time.Date(2022, time.January, 1, 20, 59, 0, 0, time.Local)) != config.DayTemp {
        t.Fatal("20.59 fail")
    }
    if checkDayTime(time.Date(2022, time.January, 1, 21, 00, 0, 0, time.Local)) != config.NightTemp {
        t.Fatal("21.00 fail")
    }
}

func TestGetExpectedTemp(t *testing.T) {
    setup()
    //Thursday
    if getExpectedTemp(time.Date(2021, time.September, 9, 7, 00, 0, 0, time.Local)) != config.NightTemp {
        t.Fatal("Sep 10 11.00 fail")
    }
    if getExpectedTemp(time.Date(2021, time.September, 9, 11, 00, 0, 0, time.Local)) != config.DayTemp {
        t.Fatal("Sep 10 11.00 fail")
    }
    if getExpectedTemp(time.Date(2021, time.September, 9, 21, 00, 0, 0, time.Local)) != config.NightTemp {
        t.Fatal("Sep 10 11.00 fail")
    }
    //Friday, exception
    if getExpectedTemp(time.Date(2021, time.September, 10, 11, 00, 0, 0, time.Local)) != config.NightTemp {
        t.Fatal("Sep 10 11.00 fail")
    }
    //Saturday
    if getExpectedTemp(time.Date(2021, time.September, 11, 11, 00, 0, 0, time.Local)) != config.NightTemp {
        t.Fatal("Sep 11 11.00 fail")
    }
}

func TestHeaterHandler(t *testing.T) {
    setup()
    config.Delta = 0.5
    tim := time.Date(2021, time.September, 9, 7, 00, 0, 0, time.Local)
    heaterHandlerInit(tim)
    testLevel = pinState
    pin := testPin{}
    heaterHandler(pin, config.NightTemp - config.Delta, tim)
    if testLevel != gpio.Low {
        t.Fatal("below fail")
    }
    heaterHandler(pin, config.NightTemp, tim)
    if testLevel != gpio.Low {
        t.Fatal("eq fail")
    }
    heaterHandler(pin, config.NightTemp + config.Delta - 0.01, tim)
    if testLevel != gpio.Low {
        t.Fatal("edge fail")
    }
    heaterHandler(pin, config.NightTemp + config.Delta, tim)
    if testLevel != gpio.High {
        t.Fatal("above fail")
    }
    heaterHandler(pin, config.NightTemp, tim)
    if testLevel != gpio.High {
        t.Fatal("eq2 fail")
    }
    heaterHandler(pin, config.NightTemp - 0.01, tim)
    if testLevel != gpio.Low {
        t.Fatal("below2 fail")
    }
    heaterHandler(pin, config.NightTemp + config.Delta - 0.01, tim)
    if testLevel != gpio.Low {
        t.Fatal("edge2 fail")
    }
    heaterHandler(pin, config.NightTemp + config.Delta, tim)
    if testLevel != gpio.High {
        t.Fatal("above2 fail")
    }
}