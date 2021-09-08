package devices

import (
    "errors"
    "periph.io/x/periph/conn/i2c"
    "periph.io/x/periph/conn/i2c/i2creg"
    "periph.io/x/periph/conn/physic"
    "periph.io/x/periph/devices/bmxx80"
    "strconv"
)

type BsBmp struct {
    bus        i2c.BusCloser
    dev        *bmxx80.Dev
    tempOffset float64
}

func (b *BsBmp) Init(settings map[string]string) error {
    emulatedStr := settings["emulated"]
    emulated := emulatedStr == "true"

    tempOffsetStr, ok := settings["tempOffset"]
    if !ok {
        return errors.New("tempOffset is missing")
    }
    var err error
    b.tempOffset, err = strconv.ParseFloat(tempOffsetStr, 64)
    if err != nil {
        return err
    }

    if !emulated {
        i2cPortStr, ok := settings["i2cPort"]
        if !ok {
            return errors.New("i2cPort is missing")
        }
        // Open a handle to I²C bus
        b.bus, err = i2creg.Open(i2cPortStr)
        if err != nil {
            return err
        }

        // Open a handle to a bme280/bmp280 connected on the I²C bus using default settings
        b.dev, err = bmxx80.NewI2C(b.bus, 0x76, &bmxx80.DefaultOpts)
        if err != nil {
            return err
        }
    } else {
        b.bus = nil
        b.dev = nil
    }

    return nil
}

func (b *BsBmp) Close() {
    if b.dev != nil {
        _ = b.dev.Halt()
    }
    if b.bus != nil {
        _ = b.bus.Close()
    }
}

func (b *BsBmp) Sense(e *Env) error {
    if b.dev == nil { // emulated
        e.Temperature = 20.0 + b.tempOffset
        e.Humidity    = 40.0
        e.Pressure    = 1000.0
        e.HumidityPresent = true
        e.PressurePresent = true
        return nil
    }
    var pe physic.Env
    err := b.dev.Sense(&pe)
    if err == nil {
        e.Temperature = pe.Temperature.Celsius() + b.tempOffset
        e.Humidity = RelativeHumidityToFloat(pe.Humidity)
        e.Pressure = PressureToFloat(pe.Pressure)
        e.HumidityPresent = true
        e.PressurePresent = true
    }
    return err
}