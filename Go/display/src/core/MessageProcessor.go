package core

import (
    "fmt"
    "github.com/kataras/iris"
)

type TextLine struct {
    Color int16
    Text string
}

type ShowDataCommand struct {
    DeviceName string
    Messages []TextLine
}

type DisplayCommand struct {
    Command string
}

type DisplayMessage struct {
    CommandType int
    Command DisplayCommand
    Show ShowDataCommand
}

var Queue chan DisplayMessage
var lines []line
var isOn bool

func MessageProcessorInit() {
    Queue = make(chan DisplayMessage)
    lines = make([]line, Config.NumLines)
    y := 0
    l := 0
    for l < Config.NumLines {
        lines[l].init(y)
        y += Config.FontSize
        l++
    }
    isOn = true
}


func ShowDataHandler(ctx iris.Context) {
    var m DisplayMessage
    if err := ctx.ReadJSON(&m.Show); err != nil {
        ctx.StatusCode(iris.StatusBadRequest)
        ctx.WriteString(err.Error())
        return
    }
    m.CommandType = 1

    Queue <- m
}

func DisplayCommandHandler(ctx iris.Context) {
    var m DisplayMessage
    if err := ctx.ReadJSON(&m.Command); err != nil {
        ctx.StatusCode(iris.StatusBadRequest)
        ctx.WriteString(err.Error())
        return
    }
    m.CommandType = 0

    Queue <- m
}

func MessageProcessor() {
    for {
        message := <-Queue
        if message.CommandType == 0 {
            switch message.Command.Command {
            case "off":
                if !isOn {
                    continue
                }
                if displayOff() == nil {
                    isOn = false
                }
            case "on":
                if isOn {
                    continue
                }
                if displayOn() == nil {
                    isOn = true
                }
            case "clear":
                if !isOn {
                    continue
                }
                displayClear()
            }
        } else {
            if !isOn {
                continue
            }
            displayMessage(message.Show)
        }
    }
}

func displayMessage(command ShowDataCommand) {
    deviceConfiguration := GetDeviceConfiguration(command.DeviceName)
    if deviceConfiguration == nil {
        fmt.Printf("Unknown device: %v", command.DeviceName)
        return
    }
    numLines := len(command.Messages)
    if numLines != deviceConfiguration.NumLines {
        fmt.Printf("Wrong number of lines received from device %v: %v", command.DeviceName, numLines)
        return
    }
    lineNo := deviceConfiguration.LineStart
    messageNo := 0
    for numLines > 0 {
        message := command.Messages[messageNo]
        err := lines[lineNo].show(command.DeviceName, deviceConfiguration.Priority, message.Text, message.Color)
        if err != nil {
            fmt.Printf("Line.show error: %v", err.Error())
            return
        }
        numLines -= 1
        lineNo += 1
        messageNo += 1
    }
}