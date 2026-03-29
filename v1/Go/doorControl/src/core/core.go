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
)

var (
	exitCh chan error
    doorCh chan bool
    lightCh chan int
)

func initPin(pinName string) (gpio.PinIO, error) {
    pin := gpioreg.ByName(pinName)
    if pin == nil {
        message := fmt.Sprintf("failed to find pin %v", pinName)
        if config.FailOnUnknownPin {
            return nil, errors.New(message)
        } else {
            fmt.Println(message)
        }
    } else {
        err := pin.Out(gpio.High)
        if err != nil {
            return nil, err
        }
    }
    return pin, nil
}

func Loop(iniFileName string) error {
    _, err := host.Init()
    if err != nil {
        log.Fatal(err)
    }
    err = loadConfiguration(iniFileName)
    if err != nil {
        return err
    }

    doorPin, err := initPin(config.DoorGpioName)
    if err != nil {
        return err
    }
    lightPin, err := initPin(config.LightGpioName)
    if err != nil {
        return err
    }

    exitCh = make(chan error, 1)
    doorCh = make(chan bool, 100)
    lightCh = make(chan int, 100)

    err = serverStart()
    if err != nil {
        return err
    }

    setupSignals()

    doorHandlerInit(doorPin)
    lightHandlerInit(lightPin)

    for {
        select {
        case err = <-exitCh:
            if doorPin != nil {
                _ = doorPin.In(gpio.PullUp, gpio.NoEdge)
            }
            if lightPin != nil {
                _ = lightPin.In(gpio.PullUp, gpio.NoEdge)
            }
            return err
        }
    }
}

func setupSignals() {
    c := make(chan os.Signal, 1)
    signal.Notify(c, syscall.SIGINT, syscall.SIGTERM, syscall.SIGQUIT)
    go func() {
        <-c
        exitCh <- errors.New("interrupt - exiting")
    }()
}
