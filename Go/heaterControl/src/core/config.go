package core

import (
    "encoding/json"
    "errors"
    "heaterControl/src/devices"
    "io/ioutil"
    "time"
)

type exception struct {
    Month string
    month time.Month
    Day   int
}

type configuration struct {
    Sensor            map[string]string
    sensor            devices.Device
    GpioName          string
    CheckInterval     int
    SmartHomeInterval int
    Delta             float64
    NightTemp         float64
    DayTemp           float64
    DayStartHour      int
    DayStartMinute    int
    DayEndHour        int
    DayEndMinute      int
    DayDaysOfWeek     []string
    dayDaysOfWeek     []time.Weekday
    Exceptions        []exception
    KeyFile           string
    key               []byte
    UdpPort           int
    SmartHomeUrl      string
    FailOnUnknownPin  bool
}

var dayOfWeekMap = map[string]time.Weekday {
    "Mon": time.Monday,
    "Tue": time.Tuesday,
    "Wed": time.Wednesday,
    "Thu": time.Thursday,
    "Fri": time.Friday,
    "Sat": time.Saturday,
    "Sun": time.Sunday,
}

var monthMap = map[string]time.Month {
    "Jan": time.January,
    "Feb": time.February,
    "Mar": time.March,
    "Apr": time.April,
    "May": time.May,
    "Jun": time.June,
    "Jul": time.July,
    "Aug": time.August,
    "Sep": time.September,
    "Oct": time.October,
    "Nov": time.November,
    "Dec": time.December,
}

var config configuration
var iniFileName string

func loadConfiguration(_iniFileName string) error {
    iniFileName = _iniFileName
    iniFileContents, err := ioutil.ReadFile(_iniFileName)
    if err != nil {
        return err
    }
    err = json.Unmarshal(iniFileContents, &config)
    if err != nil {
        return err
    }
    return validateConfig()
}

func validateConfig() error {
    sensorName, ok := config.Sensor["Name"]
    if !ok {
        return errors.New("Sensor.Name is missing")
    }
    switch sensorName {
    case "BMXX80":
        config.sensor = &devices.BsBmp{}
    default:
        return errors.New("unknown sensor name")
    }
    if config.GpioName == "" {
        return errors.New("GpioName is missing or empty")
    }
    if config.CheckInterval <= 0 {
        return errors.New("wrong CheckInterval value")
    }
    if config.SmartHomeInterval < config.CheckInterval {
        return errors.New("wrong SmartHomeInterval value")
    }
    if config.Delta <= 0 {
        return errors.New("wrong Delta value")
    }
    if config.NightTemp <= 0 {
        return errors.New("wrong NightTemp value")
    }
    if config.DayTemp <= 0 {
        return errors.New("wrong DayTemp value")
    }
    if config.DayStartHour <= 0 || config.DayStartHour > 23 {
        return errors.New("wrong DayStartHour value")
    }
    if config.DayStartMinute < 0 || config.DayStartMinute > 59 {
        return errors.New("wrong DayStartMinute value")
    }
    if config.DayEndHour <= 0 || config.DayEndHour > 23 {
        return errors.New("wrong DayEndHour value")
    }
    if config.DayEndMinute < 0 || config.DayEndMinute > 59 {
        return errors.New("wrong DayEndMinute value")
    }
    if config.DayDaysOfWeek == nil || len(config.DayDaysOfWeek) == 0 {
        return errors.New("DayDaysOfWeek is missing or empty")
    }
    for _, dw := range config.DayDaysOfWeek {
        var wd time.Weekday
        found := false
        for k, v := range dayOfWeekMap {
            if dw == k {
                wd = v
                found = true
            }
        }
        if !found {
            return errors.New("wrong DayDaysOfWeek")
        }
        config.dayDaysOfWeek = append(config.dayDaysOfWeek, wd)
    }
    for i := 0; i < len(config.Exceptions); i++ {
        e := config.Exceptions[i]
        var m time.Month
        found := false
        for k, v := range monthMap {
            if e.Month == k {
                m = v
                found = true
            }
        }
        if !found {
            return errors.New("wrong Exceptions month")
        }
        if e.Day <= 0 || e.Day > 31 {
            return errors.New("wrong Exceptions day")
        }
        config.Exceptions[i].month = m
    }
    if config.UdpPort <= 0 {
        return errors.New("wrong UdpPort value")
    }
    if config.KeyFile == "" {
        return errors.New("KeyFile is missing or empty")
    }
    if config.SmartHomeUrl == "" {
        return errors.New("SmartHomeAddress is missing or empty")
    }
    var err error
    config.key, err = ioutil.ReadFile(config.KeyFile)
    return err
}
