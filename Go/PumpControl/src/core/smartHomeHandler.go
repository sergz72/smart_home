package core

import (
    "fmt"
    "log"
    "net/http"
    "strings"
)

func smartHomeHandler(r adcResult) {
    request := buildRequest(r)
    log.Printf("request = %v", request)
    if !strings.HasPrefix(config.SmartHomeWatUrl, "test") {
        response, err := http.Post(config.SmartHomeWatUrl, "application/json", strings.NewReader(request))
        if err != nil {
            log.Printf("http post error: %v", err)
        } else {
            if response.StatusCode != http.StatusOK {
                log.Printf("non-ok response from smart home service: %v", response)
            }
        }
    }
}

func buildRequest(r adcResult) string {
    var sb strings.Builder
    sb.WriteString(fmt.Sprintf("{\"icc\":%.1f", r.current))
    if config.PressureSensorIsPresent {
        sb.WriteString(fmt.Sprintf(",\"pres\":%.1f", r.pres))
    }
    sb.WriteString("}")
    return sb.String()
}
