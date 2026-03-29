package core

import (
    "regexp"
    "strings"
)

const regexDisconnect = "USB disconnect"

type deviceParameters struct {
    vendorId string
    productId string
    serial string
    ttyName string
}

type logAnalyzer struct {
    patternLine *regexp.Regexp
    patternLine2 *regexp.Regexp
    patternVendor *regexp.Regexp
    patternSerial *regexp.Regexp
    patternAttached *regexp.Regexp
    patternAttached2 *regexp.Regexp
    devices map[string]*deviceParameters
}

func (l *logAnalyzer) init() {
    l.patternLine = regexp.MustCompile("\\w+\\s+\\d+ \\d+:\\d+:\\d+ [\\w\\-]+ kernel: \\[\\s*\\d+\\.\\d+\\] usb ([0-9\\-\\.]+): (.+)")
    l.patternLine2 = regexp.MustCompile("\\w+\\s+\\d+ \\d+:\\d+:\\d+ [\\w\\-]+ kernel: \\[\\s*\\d+\\.\\d+\\] cdc_acm ([0-9\\-\\.]+):[0-9\\.]+: (.+)")
    l.patternVendor = regexp.MustCompile("idVendor=([0-9a-f]+), idProduct=([0-9a-f]+)")
    l.patternSerial = regexp.MustCompile("SerialNumber: (.+)")
    l.patternAttached = regexp.MustCompile("attached to (ttyUSB\\d+)")
    l.patternAttached2 = regexp.MustCompile("(ttyACM\\d+): USB ACM device")
    l.devices = make(map[string]*deviceParameters)
}

func (l *logAnalyzer) processLine(line string) {
    //fmt.Println(line)
    m := l.patternLine.FindStringSubmatch(line)
    if len(m) == 3 {
        //fmt.Println("1")
        deviceId := m[1]
        message := m[2]
        l.processLine1(deviceId, message)
        return
    }
    m = l.patternLine2.FindStringSubmatch(line)
    if len(m) == 3 {
        //fmt.Println("2")
        deviceId := m[1]
        message := m[2]
        l.processLine2(deviceId, message)
        return
    }
}

func (l *logAnalyzer) processLine1(deviceId string, message string) {
    m := l.patternVendor.FindStringSubmatch(message)
    if len(m) == 3 {
        //fmt.Println("3")
        dp := l.getDevice(deviceId)
        dp.vendorId = m[1]
        dp.productId = m[2]
        return
    }

    m = l.patternSerial.FindStringSubmatch(message)
    if len(m) == 2 {
        //fmt.Println("4")
        dp := l.getDevice(deviceId)
        dp.serial = m[1]
        return
    }
    m = l.patternAttached.FindStringSubmatch(message)
    if len(m) == 2 {
        //fmt.Println("5")
        dp := l.getDevice(deviceId)
        dp.ttyName = m[1]
        portConnected(dp)
        return
    }
    if strings.Contains(message, regexDisconnect) {
        dp, ok := l.devices[deviceId]
        if ok {
            delete(l.devices, deviceId)
            portDisconnected(dp)
        }
    }
}

func (l *logAnalyzer) processLine2(deviceId string, message string) {
    m := l.patternAttached2.FindStringSubmatch(message)
    if len(m) == 2 {
        dp := l.getDevice(deviceId)
        dp.ttyName = m[1]
        portConnected(dp)
    }
}

func (l *logAnalyzer) getDevice(deviceId string) *deviceParameters {
    dp, ok := l.devices[deviceId]
    if !ok {
        dp = &deviceParameters{}
        l.devices[deviceId] = dp
    }
    return dp
}

func forceConnect(d *device) {
    dp := deviceParameters{ vendorId: d.Vid, productId: d.Pid, serial: d.Serial, ttyName: "ttyUSB0" }
    portConnected(&dp)
}