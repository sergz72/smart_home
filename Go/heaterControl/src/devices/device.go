package devices

import (
    "periph.io/x/periph/conn/physic"
)

type Env struct {
    Temperature     float64
    Humidity        float64
    HumidityPresent bool
    Pressure        float64
    PressurePresent bool
}

type Device interface {
    Init(settings map[string]string) error
    Close()
    Sense(e *Env) error
}

func RelativeHumidityToFloat(v physic.RelativeHumidity) float64 {
    return float64(v) / float64(physic.PercentRH)
}

func PressureToFloat(v physic.Pressure) float64 {
    return float64(v) / float64(physic.Pascal) / 100
}
