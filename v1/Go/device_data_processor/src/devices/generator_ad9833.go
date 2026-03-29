package devices

import (
    "encoding/json"
    "errors"
    "fmt"
    "log"
    "strings"
    "sync"
    "time"
)

const (
    ad9833B28 = 0x2000
// ad9833_HLB     = 0x1000
// ad9833_FSELECT = 0x0800
// ad9833_PSELECT = 0x0400
// ad9833_RESET   = 0x0100
    ad9833Sleep1 = 0x0080
    ad9833Sleep12 = 0x0040
    ad9833Opbiten = 0x0020
    ad9833Div2 = 0x0008
    ad9833Mode = 0x0002
    ad9833FreqReg0 = 0x4000
// ad9833_FREQ_REG1 = 0x8000

    ad9833FMclk int64 = 25000000
    ad9833MaxFreq = 12500000

    ad9833BufferLength = 40
)

type GeneratorAD9833Command struct {
    Command string
    Frequency1 int
    Frequency2 int
    Step int
}

type GeneratorAD9833 struct {
    file DeviceFile
    stop bool
    mutex sync.Mutex
    buf []byte
    buf2 []byte
    statusWord int
    freqWord1 int
    freqWord2 int
    currentSetCommand GeneratorAD9833Command
    Levels []float64
    ErrorMessage string
    ErrorMode bool
}

func (d *GeneratorAD9833) Connect(file DeviceFile, parameters DeviceConfig) {
    d.file = file
    d.stop = false
    d.buf  = make([]byte, ad9833BufferLength)
    d.buf2 = make([]byte, ad9833BufferLength)
    d.freqWord1 = -1

    var currentSetCommand GeneratorAD9833Command
    var prevSetCommand GeneratorAD9833Command

    for !d.stop {
        if !d.ErrorMode {
            d.mutex.Lock()
            prevSetCommand = currentSetCommand
            currentSetCommand = d.currentSetCommand
            d.mutex.Unlock()
            if currentSetCommand.Frequency1 > 0 {
                if currentSetCommand.Frequency1 == currentSetCommand.Frequency2 {
                    if prevSetCommand.Frequency1 != prevSetCommand.Frequency2 ||
                        currentSetCommand.Frequency1 != prevSetCommand.Frequency1 {
                        err := d.freqSet(int64(currentSetCommand.Frequency1))
                        if err != nil {
                            d.mutex.Lock()
                            d.ErrorMessage = err.Error()
                            d.ErrorMode = true
                            d.mutex.Unlock()
                            currentSetCommand.Frequency1 = 0
                            continue
                        }
                    }
                    _, level, err := d.getInputLevelDb()
                    if err != nil {
                        d.mutex.Lock()
                        d.ErrorMessage = err.Error()
                        d.ErrorMode = true
                        d.mutex.Unlock()
                        currentSetCommand.Frequency1 = 0
                    } else {
                        levels := make([]float64, 1)
                        levels[0] = level
                        d.mutex.Lock()
                        d.Levels = levels
                        d.mutex.Unlock()
                    }
                } else if currentSetCommand.Frequency2 > currentSetCommand.Frequency1 && currentSetCommand.Step > 0 {
                    size := (currentSetCommand.Frequency2 - currentSetCommand.Frequency1) / currentSetCommand.Step + 1
                    levels := make([]float64, size)
                    freq := currentSetCommand.Frequency1
                    for i := 0; i < size; i++ {
                        if freq > ad9833MaxFreq {
                            break
                        }
                        err := d.freqSet(int64(freq))
                        if err != nil {
                            d.mutex.Lock()
                            d.ErrorMessage = err.Error()
                            d.ErrorMode = true
                            d.mutex.Unlock()
                            break
                        }
                        time.Sleep(5 * time.Millisecond)
                        _, level, err := d.getInputLevelDb()
                        if err != nil {
                            d.mutex.Lock()
                            d.ErrorMessage = err.Error()
                            d.ErrorMode = true
                            d.mutex.Unlock()
                            break
                        }
                        levels[i] = level
                        freq += currentSetCommand.Step
                    }
                    if !d.ErrorMode {
                        d.mutex.Lock()
                        d.Levels = levels
                        d.mutex.Unlock()
                    }
                }
            }
        }
        time.Sleep(300 * time.Millisecond)
    }
}

func (d *GeneratorAD9833) Disconnect() {
    d.stop = true
}

func (d *GeneratorAD9833) GetFile() DeviceFile {
    return d.file
}

func (d *GeneratorAD9833) CommandHandler(commandBody []byte) []byte {
    var command GeneratorAD9833Command
    err := json.Unmarshal(commandBody, &command)
    if err != nil {
        return []byte(err.Error())
    }
    switch command.Command {
    case "status":
        var statusBody []byte
        d.mutex.Lock()
        statusBody, err = json.Marshal(d)
        d.Levels = nil
        d.mutex.Unlock()
        if err != nil {
            return []byte(err.Error())
        }
        return statusBody
    case "sleep":
        err = d.sleep()
    case "sine":
        err = d.sine()
    case "square":
        err = d.square()
    case "squareDiv2":
        err = d.squareDiv2()
    case "triangular":
        err = d.triangular()
    case "set":
        d.mutex.Lock()
        d.currentSetCommand = command
        d.mutex.Unlock()
    default:
        return []byte("Unknown command.")
    }
    if err != nil {
        return []byte(err.Error())
    }
    return []byte("Ok")
}

func (d *GeneratorAD9833) freqSet(freq int64) error {
    code := freq * (1 << 28) / ad9833FMclk / 10
    codeLo := int(code & 0x3FFF)
    codeHi := int((code >> 14) & 0x3FFF)
    var err error = nil
    if codeHi != d.freqWord2 {
        d.statusWord |= ad9833B28
        err = d.write9833(d.statusWord, codeLo |ad9833FreqReg0, codeHi |ad9833FreqReg0)
    } else if codeLo != d.freqWord1 {
        d.statusWord &^= ad9833B28
        err = d.write9833(d.statusWord, codeLo |ad9833FreqReg0)
    }
    if err == nil {
        d.freqWord1 = codeLo
        d.freqWord2 = codeHi
    }
    return err
}

func (d *GeneratorAD9833) sleep() error {
    d.statusWord = ad9833Sleep1 | ad9833Sleep12
    return d.write9833(d.statusWord)
}

func (d *GeneratorAD9833) square() error {
    d.statusWord = ad9833Opbiten
    return d.write9833(d.statusWord)
}

func (d *GeneratorAD9833) squareDiv2() error {
    d.statusWord = ad9833Opbiten | ad9833Div2
    return d.write9833(d.statusWord)
}

func (d *GeneratorAD9833) sine() error {
    d.statusWord = 0
    return d.write9833(d.statusWord)
}

func (d *GeneratorAD9833) triangular() error {
    d.statusWord = ad9833Mode
    return d.write9833(d.statusWord)
}

// input_level_in_db = 85 * (level_in_mv - 500) / 2060 - 75
func (d *GeneratorAD9833) getInputLevelDb() (bool, float64, error) {
    n, err := d.command("r", 5)
    if err != nil {
        return false, 0, err
    }
    if n != 5 {
        return false, 0, d.wrongAnswerError(n)
    }
    stable := d.buf[0] == byte('S')
    code := 0
    for _, b := range d.buf[1:5] {
        code <<= 4
        if b < byte('0') {
            return false, 0, d.wrongAnswerError(n)
        }
        if b <= byte('9') {
            code += int(b - byte('0'))
            continue
        }
        if b < byte('A') || b > byte('F') {
            return false, 0, d.wrongAnswerError(n)
        }
        code += int(b - byte('A') + 10)
    }
    levelInMv := float64(code * 5 / 2)
    return stable, 85 * (levelInMv- 500) / 2060 - 75, nil
}

func (d *GeneratorAD9833) command(cmd string, expectedLength int) (int, error) {
    d.mutex.Lock()
    _, err := d.file.Write([]byte(cmd))
    if err != nil {
        d.mutex.Unlock()
        log.Println(err)
        return 0, err
    }
    cnt := 0
    for i := 0; i < 20; i++ {
        time.Sleep(5 * time.Millisecond)
        var n int
        n, err = d.file.Read(d.buf2)
        if err != nil {
            d.mutex.Unlock()
            log.Println(err)
            return 0, err
        }
        for j := 0; j < n; j++ {
            d.buf[cnt] = d.buf2[j]
            cnt++
        }
        if cnt >= expectedLength {
            break
        }
    }
    d.mutex.Unlock()
    return cnt, nil
}

func (d *GeneratorAD9833) wrongAnswerError(n int) error {
    errorMessage := fmt.Sprintf("Wrong answer from device: %v", string(d.buf[:n]))
    log.Println(errorMessage)
    return errors.New(errorMessage)
}

func (d *GeneratorAD9833) write9833(words ...int) error {
    var command strings.Builder
    command.WriteString("w")
    for _, w := range words {
        command.WriteString(fmt.Sprintf("%04X", w))
    }
    command.WriteString("\r")
    n, err := d.command(command.String(), 2)
    if err != nil {
        return err
    }
    if n != 2 || d.buf[0] != byte('O') || d.buf[1] != byte('k') {
        return d.wrongAnswerError(n)
    }
    return nil
}
