package devices

import (
    "encoding/json"
    "fmt"
    "github.com/kataras/iris/core/errors"
    "log"
    "strconv"
    "sync"
    "time"
)

type DrillIronCommand struct {
    Command string
    Value int
    TemperatureLow int
    TemperatureHigh int
}

type DrillIronC struct {
    file DeviceFile
    stop bool
    profiles []*IronProfile
    buf []byte
    ButtonPressed bool
    HeaterTemperature []int64
    mutex sync.Mutex
}

func (d *DrillIronC) initProfiles() {
    for _, profile := range d.profiles {
        if profile.DefaultTemperatureLow == 0 {
            profile.DefaultTemperatureLow = 150
        }
        if profile.DefaultTemperatureHigh == 0 {
            if profile.DualTemperatureEnable {
                profile.DefaultTemperatureHigh = profile.DefaultTemperatureLow + 20
            } else {
                profile.DefaultTemperatureHigh = profile.DefaultTemperatureLow
            }
        }
    }
}

func (d *DrillIronC) Connect(file DeviceFile, parameters DeviceConfig) {
    d.file = file
    d.stop = false
    d.ButtonPressed = false
    d.profiles = parameters.IronProfiles
    d.initProfiles()
    d.buf = make([]byte, 40)
    d.HeaterTemperature = make([]int64, 3)

    for !d.stop {
        d.mutex.Lock()
        _, err := d.file.Write([]byte{'s'})
        if err != nil {
            d.mutex.Unlock()
            log.Println(err)
            d.drillIronError("e0001")
        } else {
            time.Sleep(100 * time.Millisecond)
            var n int
            n, err = d.file.Read(d.buf)
            d.mutex.Unlock()
            if err != nil {
                log.Println(err)
                d.drillIronError("e0002")
            } else {
                if n != 13 {
                    log.Printf("Wrong answer from device: %v\n", string(d.buf[:n]))
                    d.drillIronError("e0003")
                } else {
                    d.ButtonPressed = d.buf[12] != '0'
                    d.HeaterTemperature[0], err = strconv.ParseInt(string(d.buf[:4]), 16, 16)
                    if err != nil {
                        log.Println(err)
                        d.drillIronError("e0004")
                    } else {
                        d.HeaterTemperature[1], err = strconv.ParseInt(string(d.buf[4:8]), 16, 16)
                        if err != nil {
                            log.Println(err)
                            d.drillIronError("e0005")
                        } else {
                            d.HeaterTemperature[2], err = strconv.ParseInt(string(d.buf[8:12]), 16, 16)
                            if err != nil {
                                log.Println(err)
                                d.drillIronError("e0006")
                            }
                        }
                    }
                }
            }
            d.updateProfileTemperatures()
        }
        time.Sleep(900 * time.Millisecond)
    }
}

func (d *DrillIronC) Disconnect() {
    d.stop = true
}

func (d *DrillIronC) GetFile() DeviceFile {
    return d.file
}

func (d *DrillIronC) command(cmd string, displayError bool) error {
    d.mutex.Lock()
    _, err := d.file.Write([]byte(cmd))
    if err != nil {
        d.mutex.Unlock()
        log.Println(err)
        if displayError {
            d.drillIronError("e0007")
        }
        return err
    }
    time.Sleep(100 * time.Millisecond)
    var n int
    n, err = d.file.Read(d.buf)
    d.mutex.Unlock()
    if err != nil {
        log.Println(err)
        if displayError {
            d.drillIronError("e0008")
        }
        return err
    }
    if n != 1 || d.buf[0] != byte('K') {
        errorMessage := fmt.Sprintf("Wrong answer from device: %v", string(d.buf[:n]))
        log.Println(errorMessage)
        if displayError {
            d.drillIronError("e0009")
        }
        return errors.New(errorMessage)
    }
    return nil
}

func (d *DrillIronC) drillIronError(message string) {
    postResult("DrillIronC", []TextLine{{256, "Error"},{256, message}}) // Red
}

func (d *DrillIronC) drillCommandHandler(deviceCommand byte, value int) error {
    if value <= 0 || value > 100 {
        return errors.New("Incorrect command value.")
    }
    return d.command(fmt.Sprintf("%c%02d", deviceCommand, value), true)
}

func (d *DrillIronC) ironCommandHandler(heaterID int, value int) error {
    if heaterID <= 0 || heaterID > 3 {
        return errors.New("Incorrect heaterID value.")
    }
    if value < 0 || value > 4095 {
        return errors.New("Incorrect temperature value.")
    }
    return d.command(fmt.Sprintf("h%d%04d", heaterID, value), true)
}

func (d *DrillIronC) CommandHandler(commandBody []byte) []byte {
    var command DrillIronCommand
    err := json.Unmarshal(commandBody, &command)
    if err != nil {
        return []byte(err.Error())
    }
    switch command.Command {
    case "status":
        var statusBody []byte
        statusBody, err = json.Marshal(d)
        if err != nil {
            return []byte(err.Error())
        }
        return statusBody
    case "drillPower":
        err = d.drillCommandHandler('d', command.Value)
    case "drillRiseTime":
        err = d.drillCommandHandler('r', command.Value)
    case "heaterOff":
        err = d.ironCommandHandler(command.Value, 0)
        if (err == nil) {
            for _, profile := range d.profiles {
                if profile.HeaterID == command.Value {
                    profile.active = false
                }
            }
        }
    case "profileOn":
        if command.Value < 0 || command.Value >= len(d.profiles) {
            return []byte("Incorrect profile ID")
        }
        d.profiles[command.Value].temperatureLow = command.TemperatureLow
        d.profiles[command.Value].temperatureHigh = command.TemperatureHigh
        d.profiles[command.Value].temperature = 0
        d.profiles[command.Value].active = true
    case "profiles":
        var statusBody []byte
        statusBody, err = json.Marshal(d.profiles)
        if err != nil {
            return []byte(err.Error())
        }
        return statusBody
    default:
        return []byte("Unknown command.")
    }
    if err != nil {
        return []byte(err.Error())
    }
    return []byte("Ok")
}

func (d *DrillIronC) updateProfileTemperatures() {
    for _, profile := range d.profiles {
        if profile.active {
            requiredTemperature := profile.temperatureLow
            if d.ButtonPressed {
                requiredTemperature = profile.temperatureHigh
            }
            if profile.temperature != requiredTemperature {
                err := d.ironCommandHandler(profile.HeaterID, profile.calculateCode(requiredTemperature))
                if err == nil {
                    profile.temperature = requiredTemperature
                }
            }
        }
    }
}

func (p *IronProfile) calculateCode(temperature int) int {
    code := int(p.Multiplier * float64(temperature) + p.Adder)
    if code < 0 {
        return 0
    } else if code > 4095 {
        return 4095
    }
    return code
}