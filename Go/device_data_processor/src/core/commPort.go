package core

import (
    "devices"
    "fmt"
    "golang.org/x/sys/unix"
    "log"
    "os"
    "time"
    "unsafe"
)

func portConnected(dp *deviceParameters) {
    fmt.Printf("Device connect: %v\n", dp)
    d := findDevice(dp)
    if d != nil {
        var file *os.File
        var err error
        if d.Emulate {
            err = nil
            file = nil
        } else {
            file, err = openCommPort(dp.ttyName, d.baud)
        }
        if err != nil {
            log.Fatal(err)
        } else {
            go d.handler.Connect(devices.DeviceFile{File: file, EmulatorHandler: d.emulator}, d.Parameters)
            d.active = true
            fmt.Println(d)
        }
    }
}

func portDisconnected(dp *deviceParameters) {
    fmt.Printf("Device disconnect: %v\n", dp)
    d := findDevice(dp)
    if d != nil {
        d.handler.Disconnect()
        d.active = false
        //d.handler.getFile().Close()
    }
}

// Converts the timeout values for Linux / POSIX systems
func posixTimeoutValues(readTimeout time.Duration) (vmin uint8, vtime uint8) {
    const MAXUINT8 = 1<<8 - 1 // 255
    // set blocking / non-blocking read
    var minBytesToRead uint8 = 1
    var readTimeoutInDeci int64
    if readTimeout > 0 {
        // EOF on zero read
        minBytesToRead = 0
        // convert timeout to deciseconds as expected by VTIME
        readTimeoutInDeci = (readTimeout.Nanoseconds() / 1e6 / 100)
        // capping the timeout
        if readTimeoutInDeci < 1 {
            // min possible timeout 1 Deciseconds (0.1s)
            readTimeoutInDeci = 1
        } else if readTimeoutInDeci > MAXUINT8 {
            // max possible timeout is 255 deciseconds (25.5s)
            readTimeoutInDeci = MAXUINT8
        }
    }
    return minBytesToRead, uint8(readTimeoutInDeci)
}

func openCommPort(ttyName string, rate uint32) (*os.File, error) {
    deviceHandle, err := os.OpenFile("/dev/" + ttyName,  unix.O_RDWR|unix.O_NOCTTY|unix.O_NONBLOCK, 0666)
    if err != nil {
        return nil, err
    }

    cflagToUse := unix.CREAD | unix.CLOCAL | unix.CS8 | rate

    fd := deviceHandle.Fd()
    vmin, vtime := posixTimeoutValues(time.Second)
    t := unix.Termios{
        Iflag:  unix.IGNPAR,
        Cflag:  cflagToUse,
        Ispeed: rate,
        Ospeed: rate,
    }
    t.Cc[unix.VMIN] = vmin
    t.Cc[unix.VTIME] = vtime

    if _, _, errno := unix.Syscall6(
        unix.SYS_IOCTL,
        uintptr(fd),
        uintptr(unix.TCSETS),
        uintptr(unsafe.Pointer(&t)),
        0,
        0,
        0,
    ); errno != 0 {
        return nil, errno
    }

    if err = unix.SetNonblock(int(fd), false); err != nil {
        return nil, err
    }

    return deviceHandle, nil
}

