package core

import (
    "fmt"
    "heaterControl/src/devices"
    "log"
    "net/http"
    "strings"
)

func smartHomeHandler(e *devices.Env) {
    //log.Printf("env = %v", e)
    request := buildRequest(e)
    log.Printf("request = %v", request)
    if !strings.HasPrefix(config.SmartHomeUrl, "test") {
        response, err := http.Post(config.SmartHomeUrl, "application/json", strings.NewReader(request))
        if err != nil {
            log.Printf("http post error: %v", err)
        } else {
            if response.StatusCode != http.StatusOK {
                log.Printf("non-ok response from smart home service: %v", response)
            }
        }
    }
}

func buildRequest(e *devices.Env) string {
    var sb strings.Builder
    sb.WriteString(fmt.Sprintf("{\"temp\":%v", e.Temperature))
    if e.HumidityPresent {
        sb.WriteString(fmt.Sprintf(",\"humi\":%v", e.Humidity))
    }
    if e.PressurePresent {
        sb.WriteString(fmt.Sprintf(",\"pres\":%v", e.Pressure))
    }
    sb.WriteString("}")
    return sb.String()
}

