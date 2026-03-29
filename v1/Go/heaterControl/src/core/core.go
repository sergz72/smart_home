package core

import (
    "errors"
    "fmt"
    "heaterControl/src/devices"
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

    err = config.sensor.Init(config.Sensor)
    if err != nil {
        return err
    }
    defer config.sensor.Close()

    pin := gpioreg.ByName(config.GpioName)
    if pin == nil {
        message := fmt.Sprintf("failed to find pin %v", config.GpioName)
        if config.FailOnUnknownPin {
            return errors.New(message)
        } else {
            fmt.Println(message)
        }
    } else {
        err = pin.Out(gpio.High)
        if err != nil {
            return err
        }
    }

    defer func() {
        if pin != nil {
            _ = pin.In(gpio.PullUp, gpio.NoEdge)
        }
    }()

    err = serverStart()
    if err != nil {
        return err
    }

    exitCh = make(chan error, 1)
    tempCh := make(chan float64, 1)
    envCh  := make(chan *devices.Env, 1)

    setupSignals()

    heaterHandlerInit(time.Now())

    go envLoop(tempCh, envCh)

    for {
        select {
        case err = <-exitCh:
            return err
        case temp := <-tempCh:
            heaterHandler(pin, temp, time.Now())
        case env := <- envCh:
            smartHomeHandler(env)
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
