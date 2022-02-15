package devices

import (
    "periph.io/x/periph/conn/i2c"
    "periph.io/x/periph/conn/i2c/i2creg"
    "periph.io/x/periph/conn/physic"
    "periph.io/x/periph/experimental/devices/ads1x15"
)

type Ads1115 struct {
    bus  i2c.BusCloser
    pin0 ads1x15.PinADC
    pin2 ads1x15.PinADC
}

func (d *Ads1115) Init(I2cPortName string) error {
    if I2cPortName == "emulator" {
        d.bus  = nil
        d.pin0 = nil
        d.pin2 = nil
        return nil
    }

    var err error
    d.bus, err = i2creg.Open(I2cPortName)
    if err != nil {
        return err
    }

    adc, err := ads1x15.NewADS1115(d.bus, &ads1x15.DefaultOpts)
    if err != nil {
        _ = d.bus.Close()
        return err
    }

    d.pin0, err = adc.PinForChannel(ads1x15.Channel0, 3300*physic.MilliVolt, 128*physic.Hertz, ads1x15.BestQuality)
    if err != nil {
        _ = d.bus.Close()
        return err
    }

    d.pin2, err = adc.PinForChannel(ads1x15.Channel2, 3300*physic.MilliVolt, 128*physic.Hertz, ads1x15.BestQuality)
    if err != nil {
        _ = d.pin0.Halt()
        _ = d.bus.Close()
        return err
    }

    return nil
}

func (d *Ads1115) ReadCurrentChannel() (float64, error) {
    if d.pin0 == nil {
        return 0.8, nil
    }
    s, err := d.pin0.Read()
    if err != nil {
        return 0, err
    }
    return float64(s.V) / float64(physic.Volt), nil
}

func (d *Ads1115) ReadPressureChannel() (float64, error) {
    if d.pin2 == nil {
        return 0.5, nil
    }
    s, err := d.pin2.Read()
    if err != nil {
        return 0, err
    }
    return float64(s.V) / float64(physic.Volt), nil
}

func (d *Ads1115) Close() {
    if d.pin0 != nil {
        _ = d.pin0.Halt()
    }
    if d.pin2 != nil {
        _ = d.pin2.Halt()
    }
    if d.bus != nil {
        _ = d.bus.Close()
    }
}
