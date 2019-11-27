package core

import (
    "fmt"
    "golang.org/x/sys/unix"
    "net"
    "os"
    "strings"
    "time"
    "unsafe"
)

var deviceHandle *os.File
var deviceConnection net.Conn

const timeoutDuration = 2 * time.Second

func tcpDisplayInit(hostName string) error {
    var err error
    d := net.Dialer{Timeout: timeoutDuration}
    deviceConnection, err = d.Dial("tcp", hostName)
    return err
}

func deviceRead(data []byte) (int, error) {
    if deviceHandle != nil {
        return deviceHandle.Read(data)
    }
    deviceConnection.SetReadDeadline(time.Now().Add(timeoutDuration))
    return deviceConnection.Read(data)
}

func deviceWrite(data []byte) (int, error) {
    if deviceHandle != nil {
        return deviceHandle.Write(data)
    }
    deviceConnection.SetWriteDeadline(time.Now().Add(timeoutDuration))
    return deviceConnection.Write(data)
}

func readAnswer() error {
    var b [4]byte
    n, err := deviceRead(b[:])
    if err != nil {
        return err
    }
    if n != 4 {
        return fmt.Errorf("unexpected number of bytes read: %v", n)
    }
    message := string(b[:])
    if message != "done" {
        return fmt.Errorf("unexpected device answer: %s", message)
    }
    return nil
}

func oneByteCommand(title string, b byte) error {
    fmt.Println(title)
    _, err := deviceWrite([]byte{b,2})
    if err != nil {
        return err
    }
    return readAnswer()
}

func displayOff() error {
    return oneByteCommand("displayOn", 0)
}

func displayOn() error {
    return oneByteCommand("displayOn", 1)
}

func displayClear() error {
    return oneByteCommand("displayClear", 2)
}

// Discards data written to the port but not transmitted,
// or data received but not read
/*func flush() error {
    const TCFLSH = 0x540B
    _, _, errno := unix.Syscall(
        unix.SYS_IOCTL,
        uintptr(deviceHandle.Fd()),
        uintptr(TCFLSH),
        uintptr(unix.TCIOFLUSH),
    )

    if errno == 0 {
        return nil
    }
    return errno
}*/

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

func DisplayInit(deviceName string) error {
    var err error
    if strings.HasPrefix(deviceName, "tcp://") {
        deviceHandle = nil
        err = tcpDisplayInit(deviceName[6:])
        if err != nil {
            return err
        }
    } else {
        deviceConnection = nil
        deviceHandle, err = os.OpenFile(deviceName, unix.O_RDWR|unix.O_NOCTTY|unix.O_NONBLOCK, 0666)
        if err != nil {
            return err
        }

        //defer deviceHandle.Close()

        const rate uint32 = unix.B4000000
        const cflagToUse uint32 = unix.CREAD | unix.CLOCAL | unix.CS8 | rate

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
            return errno
        }

        if err = unix.SetNonblock(int(fd), false); err != nil {
            return err
        }
    }

    err = displayOn()
    if err != nil {
        return err
    }
    return displayClear()
}

func fill(x byte, y byte, width byte, height byte, color int16) error {
    b := make([]byte, 8)
    b[0] = 3
    b[1] = 8
    b[2] = byte(color & 0xFF)
    b[3] = byte((color >> 8) & 0xFF)
    b[4] = x
    b[5] = y
    b[6] = width
    b[7] = height
    _, err := deviceWrite(b)
    if err != nil {
        return err
    }
    return readAnswer()
}

func displayText(x byte, y byte, color int16, fontId byte, text string) error {
    l := len(text)
    if l == 0 {
        return fmt.Errorf("zero text length")
    }
    if color == 0 || color > 0xFFF {
        return fmt.Errorf("incorrect color: %v", color)
    }
    if l > 64 - 8 {
        return fmt.Errorf("string is too long")
    }
    //fmt.Printf("text %v,%v,%v,%v %s\n", x, y, color, fontId, text)
    ll := 8 + l
    b := make([]byte, ll)
    b[0] = 4
    b[1] = byte(ll)
    b[2] = byte(color & 0xFF)
    b[3] = byte((color >> 8) & 0xFF)
    b[4] = x
    b[5] = y
    b[6] = fontId
    b[7] = byte(l)
    i := 8
    for _, bb := range []byte(text) {
        b[i] = bb
        i += 1
    }
    _, err := deviceWrite(b)
    if err != nil {
        return err
    }
    return readAnswer()
}
