package devices

import (
    "bytes"
    "encoding/json"
    "log"
    "net/http"
)

var DisplayUrl string

type TextLine struct {
    Color int
    Text string
}

type ShowDataCommand struct {
    DeviceName string
    Messages []TextLine
}

func postResult(deviceName string, messages []TextLine) {
    if len(DisplayUrl) == 0 {
        return
    }
    var sdc ShowDataCommand
    sdc.DeviceName = deviceName
    sdc.Messages = messages
    jsonBytes, err := json.Marshal(sdc)
    if err != nil {
        log.Println(err)
        return
    }
    _, err = http.Post(DisplayUrl, "application.json", bytes.NewBuffer(jsonBytes))
    if err != nil {
        log.Println(err)
    }
}
