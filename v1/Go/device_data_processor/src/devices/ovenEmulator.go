package devices

import "fmt"

type OvenEmulator struct {
    adcValue int
    heaterOn bool
    command []byte
}

func (o *OvenEmulator) Init() {
    o.adcValue = 750
    o.heaterOn = false
}

func (o *OvenEmulator) Read(buffer []byte) (int, error) {
    if len(o.command) == 1 {
        switch o.command[0] {
        case byte('r'):
            if o.heaterOn {
                if o.adcValue < 1023 {
                    o.adcValue = o.adcValue + 1
                }
            } else {
                if o.adcValue > 750 {
                    o.adcValue = o.adcValue - 1
                }
            }
            code := fmt.Sprintf("%04x", o.adcValue)
            buffer[0] = code[0]
            buffer[1] = code[1]
            buffer[2] = code[2]
            buffer[3] = code[3]
            return 4, nil
        case byte('e'):
            o.heaterOn = true
            buffer[0] = 'O'
            buffer[1] = 'k'
            return 2, nil
        case byte('d'):
            o.adcValue += 10
            o.heaterOn = false
            buffer[0] = 'O'
            buffer[1] = 'k'
            return 2, nil
        }
    }
    buffer[0] = 'E'
    buffer[1] = 'r'
    return 2, nil
}

func (o *OvenEmulator) Write(buffer []byte) (int, error) {
    o.command = buffer
    return len(buffer), nil
}
