package core

import (
    "heaterControl/src/devices"
    "time"
)

func envLoop(tempCh chan float64, envCh chan *devices.Env) {
    duration := time.Duration(config.CheckInterval) * time.Second
    var e devices.Env
    totalDuration := 0
    for {
        err := config.sensor.Sense(&e)
        if err != nil {
            time.Sleep(time.Second)
            err = config.sensor.Sense(&e)
        }
        if err == nil {
            tempCh <- e.Temperature
            if totalDuration >= config.SmartHomeInterval {
                totalDuration = 0
                envCh <- &e
            }
        }
        time.Sleep(duration)
        totalDuration += config.CheckInterval
    }
}
