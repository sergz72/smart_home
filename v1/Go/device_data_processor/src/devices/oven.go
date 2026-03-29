package devices

import (
    "encoding/json"
    "fmt"
    "log"
    "math"
    "strconv"
    "sync"
    "time"
)

type OvenProgramItem struct {
    Temperature int
    Seconds int
}

func (i OvenProgramItem) String(timeLeft int) string {
    return fmt.Sprintf("%3d%2d", i.Temperature, timeLeft)
}

type Oven struct {
    file DeviceFile
    stop bool
    Temperature int
    program []OvenProgramItem
    ProgramStep int
    TimeLeft int
    HeaterOn bool
    StabMode bool
    ErrorMessage *string
    buf []byte
    mutex sync.Mutex
}

type OvenCommand struct {
    Command string
    Program []OvenProgramItem
}

func (d *Oven) Connect(file DeviceFile, parameters DeviceConfig) {
    d.file = file
    d.stop = false
    d.HeaterOn = false
    d.ProgramStep = -1
    d.StabMode = false
    d.ErrorMessage = nil
    d.buf = make([]byte, 40)

    for !d.stop {
        if d.ErrorMessage == nil {
            _, err := d.file.Write([]byte{'r'})
            if err != nil {
                log.Println(err)
                d.ovenError("e0001")
            } else {
                time.Sleep(100 * time.Millisecond)
                var n int
                n, err = d.file.Read(d.buf)
                if err != nil {
                    log.Println(err)
                    d.ovenError("e0002")
                } else {
                    if n != 4 {
                        log.Printf("Oven: Wrong answer from device: %v\n", string(d.buf[:n]))
                        d.ovenError("e0003")
                    } else {
                        adcValue, err := strconv.ParseInt(string(d.buf[:n]), 16, 16)
                        if err != nil {
                            log.Println(err)
                            d.ovenError("e0004")
                        } else {
                            d.calculateTemperature(adcValue, parameters)
                            d.displayResult()
                            d.programRunner()
                        }
                    }
                }
            }
        }
        time.Sleep(900 * time.Millisecond)
    }
}

func (d *Oven) programRunner() {
    d.mutex.Lock()
    if d.ProgramStep != -1 {
        if d.TimeLeft == 0 {
            d.ProgramStep += 1
            d.StabMode = false
            if d.ProgramStep >= len(d.program) {
                if d.HeaterOn {
                    d.off(true)
                }
                d.ProgramStep = -1
                d.mutex.Unlock()
                return
            }
            d.TimeLeft = d.program[d.ProgramStep].Seconds
        }
        if d.Temperature < d.program[d.ProgramStep].Temperature {
            if !d.HeaterOn {
                d.on()
            }
        } else {
            if d.HeaterOn {
                d.off(true)
            }
            d.StabMode = true
        }
        if d.StabMode {
            d.TimeLeft -= 1
        }
    }
    d.mutex.Unlock()
}

func (d *Oven) command(cmd byte, displayError bool) bool {
    _, err := d.file.Write([]byte{cmd})
    if err != nil {
        log.Println(err)
        if displayError {
            d.ovenError("e0005")
        }
        return false
    }
    time.Sleep(100 * time.Millisecond)
    var n int
    n, err = d.file.Read(d.buf)
    if err != nil {
        log.Println(err)
        if displayError {
            d.ovenError("e0006")
        }
        return false
    }
    if n != 2 {
        log.Printf("Oven: Wrong answer from device: %v\n", string(d.buf[:n]))
        if displayError {
            d.ovenError("e0007")
        }
        return false
    }
    if d.buf[0] != byte('O') || d.buf[1] != byte('k') {
        log.Printf("Oven: Wrong answer from device: %v\n", string(d.buf[:n]))
        if displayError {
            d.ovenError("e0008")
        }
        return false
    }
    return true
}

func (d *Oven) ovenError(message string) {
    postResult("Oven", []TextLine{{256, "Error"},{256, message}}) // Red
    if d.ProgramStep != -1 {
        if d.HeaterOn {
            d.off(false)
        }
        d.ProgramStep = -1
        d.ErrorMessage = &message
    }
}

func (d *Oven) off(displayError bool) bool {
    if d.command(byte('d'), displayError) {
        d.HeaterOn = false
        return true
    }
    return false
}

func (d *Oven) on() bool {
    if d.command(byte('e'), true) {
        d.HeaterOn = true
        return true
    }
    return false
}

func (d *Oven) displayResult() {
    temperatureString := fmt.Sprintf("%5d", d.Temperature)
    postResult("Oven", []TextLine{{272, d.getProgramString()},{16, temperatureString}}) // Yellow Green
}

func (d *Oven) getProgramString() string {
    if d.ProgramStep < 0 || d.ProgramStep >= len(d.program) {
        return "     "
    }
    return d.program[d.ProgramStep].String(d.TimeLeft)
}

func (d *Oven) calculateTemperature(adcValue int64, parameters DeviceConfig) {
  u := 2.56 * float64(adcValue) / 1024
  r := parameters.R1 * u / (parameters.U1 - u)
  d.Temperature = int((-parameters.R0*parameters.A +
            math.Sqrt(parameters.R0*parameters.R0*parameters.A*parameters.A-4*parameters.R0*parameters.B*(parameters.R0-r)))/
                      (2*parameters.R0*parameters.B))
}

func (d *Oven) Disconnect() {
    d.stop = true
}

func (d *Oven) GetFile() DeviceFile {
    return d.file
}

func (d *Oven) CommandHandler(commandBody []byte) []byte {
    var command OvenCommand
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
    case "start":
        if d.ProgramStep != -1 {
            return []byte("Program already started.")
        }
        if command.Program == nil {
            return []byte("Missing program body.")
        }
        if len(command.Program) == 0 {
            return []byte("Empty program body.")
        }
        d.program = command.Program
        d.TimeLeft = d.program[0].Seconds
        d.ProgramStep = 0
    case "stop":
        d.mutex.Lock()
        d.ProgramStep = -1
        d.mutex.Unlock()
        d.StabMode = false
        d.ErrorMessage = nil
        if d.HeaterOn {
            d.off(true)
            if d.ErrorMessage != nil {
                return []byte(*d.ErrorMessage)
            }
        }
    default:
        return []byte("Unknown command.")
    }
    return []byte("Ok")
}
