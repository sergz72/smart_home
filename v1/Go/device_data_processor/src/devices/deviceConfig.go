package devices

type IronProfile struct {
    Title string
    HeaterID int
    Adder float64
    Multiplier float64
    DefaultTemperatureLow int
    DefaultTemperatureHigh int
    DualTemperatureEnable bool
    temperatureLow int
    temperatureHigh int
    temperature int
    active bool
}

type DeviceConfig struct {
    C int
    U1 float64
    R1 float64
    R0 float64
    A float64
    B float64
    IronProfiles []*IronProfile
}

