package core

import (
    "time"
)

type line struct {
    y int
    lastDevice string
    lastMessage string
    lastColor int16
    p0BlockingTime time.Time
}

func (l *line) init(y int) {
    l.y = y
    l.lastDevice = ""
    l.lastMessage = ""
    l.lastColor = 0
    l.p0BlockingTime = time.Now().Add(-time.Second)
}

func (l *line) show(deviceName string, priority int, text string, color int16) error {
    if priority == 0 {
        if time.Now().Before(l.p0BlockingTime) {
            return nil
        }
    } else {
        l.p0BlockingTime = time.Now().Add(Config.holdTime)
    }

    if l.lastMessage == text && l.lastColor == color {
        return nil
    }

    if l.lastDevice != deviceName {
        err := l.clear()
        if err != nil {
            return err
        }
        l.lastDevice = deviceName
    }

    err := displayText(0, byte(l.y), color, byte(Config.FontSize), text)
    if err == nil {
        l.lastMessage = text
        l.lastColor = color
    }
    return err
}

func (l *line) clear() error {
    return fill(0, byte(l.y), byte(Config.FontWidth * Config.Width), byte(Config.FontSize), 0)
}
