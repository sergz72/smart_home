package core

import (
    "errors"
    "fmt"
    "log"
    "os"
    "os/signal"
    "periph.io/x/periph/conn/gpio"
    "periph.io/x/periph/conn/gpio/gpioreg"
    "periph.io/x/periph/host"
    "syscall"
    "time"
)

var exitCh chan error

func Loop(iniFileName string) error {
    _, err := host.Init()
    if err != nil {
        log.Fatal(err)
    }
    err = loadConfiguration(iniFileName)
    if err != nil {
        return err
    }

    err = adcInit()
    if err != nil {
        return err
    }
    defer adcClose()

    pin := gpioreg.ByName(config.GpioName)
    if pin == nil {
        message := fmt.Sprintf("failed to find pin %v", config.GpioName)
        if config.FailOnUnknownPin {
            return errors.New(message)
        } else {
            fmt.Println(message)
        }
    } else {
        err = pin.Out(gpio.Low)
        if err != nil {
            return err
        }
    }

    defer func() {
        if pin != nil {
            _ = pin.In(gpio.PullDown, gpio.NoEdge)
        }
    }()

    err = serverStart()
    if err != nil {
        return err
    }

    exitCh = make(chan error, 1)
    adcCh  := make(chan adcResult, 1)
    shCh  := make(chan adcResult, 1)

    setupSignals()

    pumpHandlerInit()

    go adcLoop(adcCh, shCh, time.Duration(config.CheckIntervalMs) * time.Millisecond,
               time.Duration(config.SmartHomeInterval) * time.Second)

    for {
        select {
        case err = <-exitCh:
            return err
        case result := <-adcCh:
            pumpHandler(pin, result)
        case sh := <- shCh:
            smartHomeHandler(sh)
        }
    }
}

func setupSignals() {
    c := make(chan os.Signal, 1)
    signal.Notify(c, syscall.SIGHUP, syscall.SIGINT, syscall.SIGTERM, syscall.SIGQUIT)
    go func() {
        for s := range c {
            if s == syscall.SIGHUP {
                fmt.Println("SIGHUP - config reload")
                err := loadConfiguration(iniFileName)
                if err == nil {
                    fmt.Println("Config reloaded")
                    continue
                }
                panic(err)
            }
            exitCh <- errors.New("interrupt - exiting")
            break
        }
    }()
}
