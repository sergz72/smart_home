package devices

import (
    "fmt"
    "log"
)

type DrillIronCEmulator struct {
    command []byte
    status []int
    buttonPressed bool
    counter int
}

func (d *DrillIronCEmulator) Init() {
    d.status = make([]int, 3)
    d.status[0] = 50
    d.status[1] = 500
    d.status[2] = 2000
    d.buttonPressed = false
    d.counter = 0
}

func (d *DrillIronCEmulator) Read(buffer []byte) (int, error) {
    switch len(d.command) {
    case 1:
        if d.command[0] == 's' {
            var p int
            if d.buttonPressed {
                p = 1
            } else {
                p = 0
            }
            status := fmt.Sprintf("%04d%04d%04d%d", d.status[0], d.status[1], d.status[2], p)
            statusBytes := []byte(status)
            copy(buffer, statusBytes)
            log.Println(status)
            return len(statusBytes), nil
        }
    case 3:
        if (d.command[0] == 'd' || d.command[0] == 'r') && (d.command[1] >= '0' && d.command[1] <= '9') &&
           (d.command[2] >= '0' && d.command[2] <= '9') && (d.command[1] != '0' || d.command[2] != '0') {
            log.Println(string(d.command))
            buffer[0] = 'K'
            return 1, nil
        }
    case 6:
        if d.command[0] == 'h' && (d.command[1] > '0' && d.command[1] < '4') &&
            (d.command[2] >= '0' && d.command[2] <= '4') && (d.command[3] >= '0' && d.command[3] <= '9') &&
            (d.command[4] >= '0' && d.command[4] <= '9') && (d.command[5] >= '0' && d.command[5] <= '9') {
            var value int
            value = int(d.command[5] - '0') * 1000
            value += int(d.command[4] - '0') * 100
            value += int(d.command[3] - '0') * 10
            value += int(d.command[2] - '0')
            if value <= 4095 {
                log.Println(string(d.command))
                buffer[0] = 'K'
                return 1, nil
            }
        }
    }
    log.Printf("Error: %v\n", string(d.command))
    buffer[0] = 'E'
    return 1, nil
}

func (d *DrillIronCEmulator) Write(buffer []byte) (int, error) {
    d.command = buffer
    d.counter += 1
    d.buttonPressed = (d.counter % 10) > 6
    return len(buffer), nil
}
