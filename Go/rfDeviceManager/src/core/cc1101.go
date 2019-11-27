package core

import (
    "encoding/json"
    "fmt"
    "io/ioutil"
    "log"
    "periph.io/x/periph/conn/gpio"
    "periph.io/x/periph/conn/gpio/gpioreg"
    "periph.io/x/periph/conn/physic"
    "periph.io/x/periph/conn/spi"
    "periph.io/x/periph/conn/spi/spireg"
    "strings"
    "time"
)

type CC1101RxAttenuation byte
const (
    Cc1101RxAttenuation0db  CC1101RxAttenuation = 0
    Cc1101RxAttenuation6db  CC1101RxAttenuation = 0x10
    Cc1101RxAttenuation12db CC1101RxAttenuation = 0x20
    Cc1101RxAttenuation18db CC1101RxAttenuation = 0x30
)

func (v *CC1101RxAttenuation) UnmarshalJSON(b []byte) error {
    s := strings.Trim(string(b), "\"")
    switch s {
    case "0db":
        *v = Cc1101RxAttenuation0db
        return nil
    case "6db":
        *v = Cc1101RxAttenuation6db
        return nil
    case "12db":
        *v = Cc1101RxAttenuation12db
        return nil
    case "18db":
        *v = Cc1101RxAttenuation18db
        return nil
    default:
        return fmt.Errorf("unknown CC1101RxAttenuation value %v", s)
    }
}

type CC1101AddressCheckConfig byte
const (
    Cc1101NoAddressCheck           CC1101AddressCheckConfig = 0
    Cc1101AddressCheck             CC1101AddressCheckConfig = 1
    Cc1101AddressCheckAnd0BCast    CC1101AddressCheckConfig = 2
    Cc1101AddressCheckAnd0255BCast CC1101AddressCheckConfig = 3
)

func (v *CC1101AddressCheckConfig) UnmarshalJSON(b []byte) error {
    s := strings.Trim(string(b), "\"")
    switch s {
    case "NoAddressCheck":
        *v = Cc1101NoAddressCheck
        return nil
    case "AddressCheck":
        *v = Cc1101AddressCheck
        return nil
    case "AddressCheckAnd0BCast":
        *v = Cc1101AddressCheckAnd0BCast
        return nil
    case "AddressCheckAnd0255BCast":
        *v = Cc1101AddressCheckAnd0255BCast
        return nil
    default:
        return fmt.Errorf("unknown CC1101AddressCheckConfig value %v", s)
    }
}

type CC1101Pqt byte
const (
    Cc1101Pqt0 CC1101Pqt = 0
    Cc1101Pqt1 CC1101Pqt = 0x20
    Cc1101Pqt2 CC1101Pqt = 0x40
    Cc1101Pqt3 CC1101Pqt = 0x60
    Cc1101Pqt4 CC1101Pqt = 0x80
    Cc1101Pqt5 CC1101Pqt = 0xA0
    Cc1101Pqt6 CC1101Pqt = 0xC0
    Cc1101Pqt7 CC1101Pqt = 0xE0
)

func (v *CC1101Pqt) UnmarshalJSON(b []byte) error {
    s := strings.Trim(string(b), "\"")
    switch s {
    case "0":
        *v = Cc1101Pqt0
        return nil
    case "1":
        *v = Cc1101Pqt1
        return nil
    case "2":
        *v = Cc1101Pqt2
        return nil
    case "3":
        *v = Cc1101Pqt3
        return nil
    case "4":
        *v = Cc1101Pqt4
        return nil
    case "5":
        *v = Cc1101Pqt5
        return nil
    case "6":
        *v = Cc1101Pqt6
        return nil
    case "7":
        *v = Cc1101Pqt7
        return nil
    default:
        return fmt.Errorf("unknown CC1101Pqt value %v", s)
    }
}

type CC1101PacketLengthConfig byte
const (
    Cc1101PacketLengthFixed    CC1101PacketLengthConfig = 0
    Cc1101PacketLengthVariable CC1101PacketLengthConfig = 1
    Cc1101PacketLengthInfinite CC1101PacketLengthConfig = 2
)

func (v *CC1101PacketLengthConfig) UnmarshalJSON(b []byte) error {
    s := strings.Trim(string(b), "\"")
    switch s {
    case "PacketLengthFixed":
        *v = Cc1101PacketLengthFixed
        return nil
    case "PacketLengthVariable":
        *v = Cc1101PacketLengthVariable
        return nil
    case "PacketLengthInfinite":
        *v = Cc1101PacketLengthInfinite
        return nil
    default:
        return fmt.Errorf("unknown CC1101PacketLengthConfig value %v", s)
    }
}

type CC1101Mode byte
const (
    Cc1101ModeGFsk1200 CC1101Mode = 0
)

func (v *CC1101Mode) UnmarshalJSON(b []byte) error {
    s := strings.Trim(string(b), "\"")
    switch s {
    case "GFsk1200":
        *v = Cc1101ModeGFsk1200
        return nil
    default:
        return fmt.Errorf("unknown CC1101Mode value %v", s)
    }
}

type CC1101SyncMode byte
const (
    Cc1101SyncModeOff        CC1101SyncMode = 0
    Cc1101SyncMode1516       CC1101SyncMode = 1
    Cc1101SyncMode1616       CC1101SyncMode = 2
    Cc1101SyncMode3032       CC1101SyncMode = 3
    Cc1101SyncModeCSense     CC1101SyncMode = 4
    Cc1101SyncMode1516CSense CC1101SyncMode = 5
    Cc1101SyncMode1616CSense CC1101SyncMode = 6
    Cc1101SyncMode3032CSense CC1101SyncMode = 7
)

func (v *CC1101SyncMode) UnmarshalJSON(b []byte) error {
    s := strings.Trim(string(b), "\"")
    switch s {
    case "Off":
        *v = Cc1101SyncModeOff
        return nil
    case "1516":
        *v = Cc1101SyncMode1516
        return nil
    case "1616":
        *v = Cc1101SyncMode1616
        return nil
    case "3032":
        *v = Cc1101SyncMode3032
        return nil
    case "CSense":
        *v = Cc1101SyncModeCSense
        return nil
    case "1516CSense":
        *v = Cc1101SyncMode1516CSense
        return nil
    case "1616CSense":
        *v = Cc1101SyncMode1616CSense
        return nil
    case "3032CSense":
        *v = Cc1101SyncMode3032CSense
        return nil
    default:
        return fmt.Errorf("unknown CC1101SyncMode value %v", s)
    }
}

type CC1101PreambleConfig byte
const (
    Cc1101Preamble2  CC1101PreambleConfig = 0
    Cc1101Preamble3  CC1101PreambleConfig = 0x10
    Cc1101Preamble4  CC1101PreambleConfig = 0x20
    Cc1101Preamble6  CC1101PreambleConfig = 0x30
    Cc1101Preamble8  CC1101PreambleConfig = 0x40
    Cc1101Preamble12 CC1101PreambleConfig = 0x50
    Cc1101Preamble16 CC1101PreambleConfig = 0x60
    Cc1101Preamble24 CC1101PreambleConfig = 0x70
)

func (v *CC1101PreambleConfig) UnmarshalJSON(b []byte) error {
    s := strings.Trim(string(b), "\"")
    switch s {
    case "2":
        *v = Cc1101Preamble2
        return nil
    case "3":
        *v = Cc1101Preamble3
        return nil
    case "4":
        *v = Cc1101Preamble4
        return nil
    case "6":
        *v = Cc1101Preamble6
        return nil
    case "8":
        *v = Cc1101Preamble8
        return nil
    case "12":
        *v = Cc1101Preamble12
        return nil
    case "16":
        *v = Cc1101Preamble16
        return nil
    case "24":
        *v = Cc1101Preamble24
        return nil
    default:
        return fmt.Errorf("unknown CC1101PreambleConfig value %v", s)
    }
}

type CC1101RxTimeConfig byte
const (
    Cc1101RxTime0 CC1101RxTimeConfig = 0
    Cc1101RxTime1 CC1101RxTimeConfig = 1
    Cc1101RxTime2 CC1101RxTimeConfig = 2
    Cc1101RxTime3 CC1101RxTimeConfig = 3
    Cc1101RxTime4 CC1101RxTimeConfig = 4
    Cc1101RxTime5 CC1101RxTimeConfig = 5
    Cc1101RxTime6 CC1101RxTimeConfig = 6
    Cc1101RxTime7 CC1101RxTimeConfig = 7
)

func (v *CC1101RxTimeConfig) UnmarshalJSON(b []byte) error {
    s := strings.Trim(string(b), "\"")
    switch s {
    case "0":
        *v = Cc1101RxTime0
        return nil
    case "1":
        *v = Cc1101RxTime1
        return nil
    case "2":
        *v = Cc1101RxTime2
        return nil
    case "3":
        *v = Cc1101RxTime3
        return nil
    case "4":
        *v = Cc1101RxTime4
        return nil
    case "5":
        *v = Cc1101RxTime5
        return nil
    case "6":
        *v = Cc1101RxTime6
        return nil
    case "7":
        *v = Cc1101RxTime7
        return nil
    default:
        return fmt.Errorf("unknown CC1101RxTimeConfig value %v", s)
    }
}

type CC1101CcaMode byte
const (
    Cc1101CcaModeAlways     CC1101CcaMode = 0
    Cc1101CcaModeRSSI       CC1101CcaMode = 0x10
    Cc1101CcaModePacket     CC1101CcaMode = 0x20
    Cc1101CcaModeRSSIPacket CC1101CcaMode = 0x30
)

func (v *CC1101CcaMode) UnmarshalJSON(b []byte) error {
    s := strings.Trim(string(b), "\"")
    switch s {
    case "Always":
        *v = Cc1101CcaModeAlways
        return nil
    case "RSSI":
        *v = Cc1101CcaModeRSSI
        return nil
    case "Packet":
        *v = Cc1101CcaModePacket
        return nil
    case "RSSIPacket":
        *v = Cc1101CcaModeRSSIPacket
        return nil
    default:
        return fmt.Errorf("unknown CC1101CcaMode value %v", s)
    }
}

type CC1101RxOffMode byte
const (
    Cc1101RxOffModeIdle   CC1101RxOffMode = 0
    Cc1101RxOffModeFstxon CC1101RxOffMode = 4
    Cc1101RxOffModeTx     CC1101RxOffMode = 8
    Cc1101RxOffModeRx     CC1101RxOffMode = 0xC
)

func (v *CC1101RxOffMode) UnmarshalJSON(b []byte) error {
    s := strings.Trim(string(b), "\"")
    switch s {
    case "Idle":
        *v = Cc1101RxOffModeIdle
        return nil
    case "Fstxon":
        *v = Cc1101RxOffModeFstxon
        return nil
    case "Tx":
        *v = Cc1101RxOffModeTx
        return nil
    case "Rx":
        *v = Cc1101RxOffModeRx
        return nil
    default:
        return fmt.Errorf("unknown CC1101RxOffMode value %v", s)
    }
}

type CC1101TxOffMode byte
const (
    Cc1101TxOffModeIdle   CC1101TxOffMode = 0
    Cc1101TxOffModeFstxon CC1101TxOffMode = 1
    Cc1101TxOffModeTx     CC1101TxOffMode = 2
    Cc1101TxOffModeRx     CC1101TxOffMode = 3
)

func (v *CC1101TxOffMode) UnmarshalJSON(b []byte) error {
    s := strings.Trim(string(b), "\"")
    switch s {
    case "Idle":
        *v = Cc1101TxOffModeIdle
        return nil
    case "Fstxon":
        *v = Cc1101TxOffModeFstxon
        return nil
    case "Tx":
        *v = Cc1101TxOffModeTx
        return nil
    case "Rx":
        *v = Cc1101TxOffModeRx
        return nil
    default:
        return fmt.Errorf("unknown CC1101TxOffMode value %v", s)
    }
}

type CC1101AutoCal byte
const (
    Cc1101AutoCalNever    CC1101AutoCal = 0
    Cc1101AutoCalFromIdle CC1101AutoCal = 0x10
    Cc1101AutoCalToIdle   CC1101AutoCal = 0x20
    Cc1101AutoCal4time    CC1101AutoCal = 0x30
)

func (v *CC1101AutoCal) UnmarshalJSON(b []byte) error {
    s := strings.Trim(string(b), "\"")
    switch s {
    case "Never":
        *v = Cc1101AutoCalNever
        return nil
    case "FromIdle":
        *v = Cc1101AutoCalFromIdle
        return nil
    case "ToIdle":
        *v = Cc1101AutoCalToIdle
        return nil
    case "4Time":
        *v = Cc1101AutoCal4time
        return nil
    default:
        return fmt.Errorf("unknown CC1101AutoCal value %v", s)
    }
}

type CC1101PoTimeout byte
const (
    Cc1101PoTimeout2us   CC1101PoTimeout = 0
    Cc1101PoTimeout40us  CC1101PoTimeout = 4
    Cc1101PoTimeout150us CC1101PoTimeout = 8
    Cc1101PoTimeout600us CC1101PoTimeout = 0xC
)

func (v *CC1101PoTimeout) UnmarshalJSON(b []byte) error {
    s := strings.Trim(string(b), "\"")
    switch s {
    case "2us":
        *v = Cc1101PoTimeout2us
        return nil
    case "40us":
        *v = Cc1101PoTimeout40us
        return nil
    case "150us":
        *v = Cc1101PoTimeout150us
        return nil
    case "600us":
        *v = Cc1101PoTimeout600us
        return nil
    default:
        return fmt.Errorf("unknown CC1101PoTimeout value %v", s)
    }
}

var cc1101TxPower = [4][41]byte {
    {0x12,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x1C,0x1C,0x1C,0x1C,0x1C,0x34,0x34,0x34,0x34,0x34,0x51,0x51,0x51,0x51,0x51,0x51,0x51,0x51,0x51,0x51,0x85,0x85,0x85,0x85,0x85,0xCB,0xCB,0xC2,0xC2,0xC2},
    {0x12,0x0E,0x0E,0x0E,0x0E,0x0E,0x0E,0x0E,0x0E,0x0E,0x0E,0x1D,0x1D,0x1D,0x1D,0x1D,0x34,0x34,0x34,0x34,0x34,0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x84,0x84,0x84,0x84,0x84,0xC8,0xC8,0xC0,0xC0,0xC0},
    {0x03,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x1E,0x1E,0x1E,0x1E,0x1E,0x27,0x27,0x27,0x27,0x27,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x81,0x81,0x81,0x81,0x81,0xCB,0xCB,0xC2,0xC2,0xC2},
    {0x03,0x0E,0x0E,0x0E,0x0E,0x0E,0x0E,0x0E,0x0E,0x0E,0x0E,0x1E,0x1E,0x1E,0x1E,0x1E,0x27,0x27,0x27,0x27,0x27,0x8E,0x8E,0x8E,0x8E,0x8E,0x8E,0x8E,0x8E,0x8E,0x8E,0xCD,0xCD,0xCD,0xCD,0xCD,0xC7,0xC7,0xC0,0xC0,0xC0},
}

type CC1101Strobe byte
const (
    Cc1101StrobeSRes    CC1101Strobe = 0x30
    Cc1101StrobeSFstxon CC1101Strobe = 0x31
    Cc1101StrobeSRx     CC1101Strobe = 0x34
    Cc1101StrobeSTx     CC1101Strobe = 0x35
    Cc1101StrobeSIdle   CC1101Strobe = 0x36
    Cc1101StrobeSFrx    CC1101Strobe = 0x3A
    Cc1101StrobeSFtx    CC1101Strobe = 0x3B
    Cc1101StrobeSNop    CC1101Strobe = 0x3D
)

const cc1101IoCfg0  = 2
const cc1101WorCtrl = 0x20
const cc1101FsCal3  = 0x23
const cc1101Test2   = 0x2C

const cc1101PktLen = 6
const cc1101PktCtrl1 = 7

const cc1101PacketReceived = 7

const cc1101AppendStatus = 4
const cc1101CrcAutoFlush = 8

const cc1101WhiteData = 0x40
const cc1101CrcEn     = 4

const cc1101ManchesterEn = 8
const cc1101DemDcFiltOff = 0x80
const cc1101FecEn        = 0x80
const cc1101RxTimeRssi   = 0x10
const cc1101RxTimeQual   = 8
const cc1101PinCtrlEn    = 2
const cc1101XOscForceOn  = 1

const cc1101PaTable = 0x3E

const cc1101FOsc = 26000

const cc1101Timeout = 100

const cc1101PartNum = 0xF0
const cc1101Version = 0xF1
const cc1101Lqi     = 0xF3
const cc1101RSSI    = 0xF4
const cc1101TxBytes = 0xFA
const cc1101RxBytes = 0xFB

const Cc1101StatusRdy = 0x80
const Cc1101StatusFifoMask = 0x0F
const Cc1101ModeMask = 0x70
const Cc1101ModeIdle = 0
const Cc1101ModeRx = 0x10
const Cc1101ModeTx = 0x20
const Cc1101ModeFstxon = 0x30
const Cc1101ModeCalibrate = 0x40
const Cc1101ModeSettling = 0x50
const Cc1101ModeRxFifoUnderflow = 0x60
const Cc1101ModeTxFifoUnderflow = 0x70

const cc1101Fifo  = 0x3F

const Cc1101Read  = 0x80
const Cc1101Burst = 0x40

type CC1101 struct {
    DeviceName         string
    GD0GpioName        string
    GD2GpioName        string
    RxAttenuation      CC1101RxAttenuation
    Pqt                CC1101Pqt
    CrcAutoFlush       bool
    AddressCheck       CC1101AddressCheckConfig
    WhiteData          bool
    CrcEnabled         bool
    PacketLengthConfig CC1101PacketLengthConfig
    Address            byte
    Channel            byte
    FreqOffset         byte
    Freq               int // in KHz
    DemDcFiltOff       bool
    Mode               CC1101Mode
    ManchesterEn       bool
    SyncMode           CC1101SyncMode
    EnableFec          bool
    NumPreamble        CC1101PreambleConfig
    RxTimeRSSI         bool
    RxTimeQual         bool
    RxTime             CC1101RxTimeConfig
    CcaMode            CC1101CcaMode
    RxOffMode          CC1101RxOffMode
    TxOffMode          CC1101TxOffMode
    Autocal            CC1101AutoCal
    PoTimeout          CC1101PoTimeout
    EnablePinControl   bool
    ForceXOSCOn        bool
    port               spi.PortCloser
    conn               spi.Conn
    gd0Pin             gpio.PinIO
    gd2Pin             gpio.PinIO
    band               int
    lastRSSI           int
    lastLQI            int
}

type cc1101BaudRateAndModeParameters struct {
    adcRetention byte
    freqIf       byte
    mode         byte
    chanSpcE     byte
    chanSpcM     byte
    mDMCFG4      byte
    drateM       byte
    dEVIATN      byte
    fOCCFG       byte
    wORCTRL      byte
    fSCAL3       byte
    fSCAL2       byte
    fSCAL1       byte
    fSCAL0       byte
    tEST2        byte
    tEST1        byte
    tEST0        byte
}

var cc1101PredefinedBaudRateAndModeParameters = []cc1101BaudRateAndModeParameters {
    //Gfsk 1200
    cc1101BaudRateAndModeParameters{
        0x40,
        6,
        0x10,
        2,
        0xF8,
        0xF5,
        0x83,
        0x15,
        0x16,
        0xFB,
        0xE9,
        0x2A,
        0,
        0x1F,
        0x81,
        0x35,
        0x09,
    },
}

type timeoutError struct {
}

func (e *timeoutError) Error() string {
    return "timeout"
}

func getBand(freq int) (int, error) {
    if freq < 300000 ||
       (freq > 348000 && freq < 387000) ||
       (freq > 464000 && freq < 779000) ||
       freq > 928000 {
        return 0, fmt.Errorf("incorrect frequency")
    }

    if freq < 348000 {
        return 0, nil //315
    }
    if freq < 464000 {
        return 1, nil //433
    }
    if freq < 900000 {
        return 2, nil //868
    }
    return 3, nil //915
}

func CC1101Init(configFileName string) (*CC1101, error) {
    var device CC1101

    log.Printf("CC1101Init, config file name : %v\n", configFileName)

    configFileContents, err := ioutil.ReadFile(configFileName)
    if err != nil {
        return nil, err
    }
    err = json.Unmarshal(configFileContents, &device)
    if err != nil {
        return nil, err
    }

    device.band, err = getBand(device.Freq)
    if err != nil {
        return nil, err
    }

    err = device.open()
    if err != nil {
        return nil, err
    }
    _, err = device.Strobe(Cc1101StrobeSRes)
    if err != nil {
        return nil, err
    }
    time.Sleep(time.Millisecond)
    err = device.initRegisters()
    if err != nil {
        return nil, err
    }

    return &device, nil
}

func (device *CC1101) open() error {
    var err error
    device.port, err = spireg.Open(device.DeviceName)
    if err != nil {
        return err
    }
    device.conn, err = device.port.Connect(physic.MegaHertz, spi.Mode0, 8)
    if err != nil {
        device.port.Close()
        return err
    }
    device.gd0Pin = gpioreg.ByName(device.GD0GpioName)
    if device.gd0Pin == nil {
        return fmt.Errorf("failed to find %v", device.GD0GpioName)
    }
    device.gd2Pin = gpioreg.ByName(device.GD2GpioName)
    if device.gd2Pin == nil {
        return fmt.Errorf("failed to find %v", device.GD2GpioName)
    }
    err = device.gd0Pin.In(gpio.Float, gpio.NoEdge)
    if err != nil {
        device.port.Close()
        return err
    }
    err = device.gd2Pin.In(gpio.Float, gpio.NoEdge)
    if err != nil {
        device.port.Close()
        return err
    }
    return err
}

func (device *CC1101) Close() error {
    return device.port.Close()
}

func (device *CC1101) initRegisters() error {
    p := cc1101PredefinedBaudRateAndModeParameters[device.Mode]

    txdata := make([]byte, 3)
    rxdata := make([]byte, 3)
    // disable the clock output on GDO0
    txdata[0] = cc1101IoCfg0
    txdata[1] = cc1101PacketReceived
    txdata[2] = p.adcRetention | byte(device.RxAttenuation) | 7
    err := device.tx(false, txdata, rxdata)
    if err != nil {
        return err
    }

    freq := int(int64(device.Freq) * 65536 / cc1101FOsc)

    txdata = make([]byte, 20)
    rxdata = make([]byte, 20)
    txdata[0] = cc1101PktCtrl1
    v := byte(device.AddressCheck) | byte(device.Pqt) | cc1101AppendStatus
    if device.CrcAutoFlush {
        v |= cc1101CrcAutoFlush
    }
    txdata[1] = v
    v = byte(device.PacketLengthConfig)
    if device.WhiteData {
        v |= cc1101WhiteData
    }
    if device.CrcEnabled {
        v |= cc1101CrcEn
    }
    txdata[2] = v
    txdata[3] = device.Address
    txdata[4] = device.Channel
    txdata[5] = p.freqIf
    txdata[6] = device.FreqOffset
    txdata[7] = byte(freq >> 16)
    txdata[8] = byte((freq >> 8) & 0xFF)
    txdata[9] = byte(freq & 0xFF)
    txdata[10] = p.mDMCFG4
    txdata[11] = p.drateM
    v = byte(device.SyncMode)
    if device.DemDcFiltOff {
        v |= cc1101DemDcFiltOff
    }
    if device.ManchesterEn {
        v |= cc1101ManchesterEn
    }
    txdata[12] = v
    v = byte(device.NumPreamble) | p.chanSpcE
    if device.EnableFec {
        v |= cc1101FecEn
    }
    txdata[13] = v
    txdata[14] = p.chanSpcM
    txdata[15] = p.dEVIATN
    v = byte(device.RxTime)
    if device.RxTimeRSSI {
        v |= cc1101RxTimeRssi
    }
    if device.RxTimeQual {
        v |= cc1101RxTimeQual
    }
    txdata[16] = v
    txdata[17] = byte(device.CcaMode) | byte(device.RxOffMode) | byte(device.TxOffMode)
    v = byte(device.Autocal) | byte(device.PoTimeout)
    if device.EnablePinControl {
        v |= cc1101PinCtrlEn
    }
    if device.ForceXOSCOn {
        v |= cc1101XOscForceOn
    }
    txdata[18] = v
    txdata[19] = p.fOCCFG
    err = device.tx(false, txdata, rxdata)
    if err != nil {
        return err
    }

    txdata = make([]byte, 2)
    rxdata = make([]byte, 2)
    txdata[0] = cc1101WorCtrl
    txdata[1] = p.wORCTRL
    err = device.tx(false, txdata, rxdata)
    if err != nil {
        return err
    }

    txdata = make([]byte, 5)
    rxdata = make([]byte, 5)
    txdata[0] = cc1101FsCal3
    txdata[1] = p.fSCAL3
    txdata[2] = p.fSCAL2
    txdata[3] = p.fSCAL1
    txdata[4] = p.fSCAL0
    err = device.tx(false, txdata, rxdata)
    if err != nil {
        return err
    }

    txdata = make([]byte, 4)
    rxdata = make([]byte, 4)
    txdata[0] = cc1101Test2
    txdata[1] = p.tEST2
    txdata[2] = p.tEST1
    txdata[3] = p.tEST0
    return device.tx(false, txdata, rxdata)
}

func (device *CC1101) tx(read bool, w, r []byte) error {
    l := len(w)
    if l < 2 || l != len(r) {
        return fmt.Errorf("incorrect length")
    }
    t := cc1101Timeout
    for t != 0 && device.gd2Pin.Read() != gpio.Low {
        t--
        time.Sleep(time.Duration(10) * time.Microsecond)
    }
    if t == 0 {
        return &timeoutError{}
    }
    if l > 2 {
        w[0] |= Cc1101Burst
    }
    if read {
        w[0] |= Cc1101Read
    }
    //log.Printf("tx: %v\n", w)
    return device.conn.Tx(w, r)
}

func (device *CC1101) Strobe(strobe CC1101Strobe) (byte, error) {
    t := cc1101Timeout
    for t != 0 && device.gd2Pin.Read() != gpio.Low {
        t--
        time.Sleep(time.Duration(10) * time.Microsecond)
    }
    if t == 0 {
        return 0, &timeoutError{}
    }
    w := []byte{byte(strobe)}
    r := make([]byte, 1)
    //log.Printf("strobe: %v\n", w)
    err := device.conn.Tx(w, r)
    if err != nil {
        return 0, err
    }
    return r[0], nil
}

func (device *CC1101) convertTxPower(power int) byte {
    if power <= -30 {
        return cc1101TxPower[device.band][0]
    }
    if power >= 10 {
        return cc1101TxPower[device.band][40]
    }
    return cc1101TxPower[device.band][power + 30]
}

func (device *CC1101) SetTXPower(power int) error {
    txPower := device.convertTxPower(power)
    w := []byte{cc1101PaTable, txPower}
    r := make([]byte, 2)
    return device.tx(false, w, r)
}

func (device *CC1101) fifoWrite(data []byte) error {
    data = append([]byte{cc1101Fifo}, data...)
    r := make([]byte, len(data))
    return device.tx(false, data, r)
}

func (device *CC1101) fifoRead(length byte) ([]byte, error) {
    length++
    w := make([]byte, length)
    w[0] = cc1101Fifo|Cc1101Read
    r := make([]byte, length)
    err := device.tx(true, w, r)
    if err != nil {
        return nil, err
    }
    return r, nil
}

func (device *CC1101) GetRxBytes() (byte, error) {
    w := []byte{cc1101RxBytes, 0xFF}
    r := make([]byte, 2)
    err := device.tx(false, w, r)
    if err != nil {
        return 0, err
    }
    return r[1], nil
}

func (device *CC1101) PowerControl(powerUp bool) error {
    _, err := device.Strobe(Cc1101StrobeSIdle)
    return err
}

func (device *CC1101) Receive() ([]byte, error) {
    if device.gd0Pin.Read() != gpio.High {
        return nil, nil // no data
    }

    bytes, err := device.GetRxBytes()
    if err != nil {
        device.Strobe(Cc1101StrobeSFrx)
        return nil, err
    }

    if bytes <= 4 {
        device.Strobe(Cc1101StrobeSFrx)
        return nil, fmt.Errorf("wrong received packet length")
    }
    data, err := device.fifoRead(bytes)
    if err != nil {
        device.Strobe(Cc1101StrobeSFrx)
        return nil, err
    }
    l := len(data)
    device.lastRSSI = calculateRSSI(data[l - 2])
    device.lastLQI = int(data[l - 1])
    return data[2:l - 2], nil
}

func calculateRSSI(rssi byte) int {
    rssiInt := int(rssi)
    if rssiInt >= 128 {
        return (rssiInt - 256) / 2 - 74
    }
    return rssiInt / 2 - 74
}

func (device *CC1101) Transmit(address []byte, data []byte) error {
    if len(address) != 1 {
        return fmt.Errorf("incorrect address length")
    }
    data = append(address, data...)
    err := device.fifoWrite(data)
    if err != nil {
        return err
    }
    _, err = device.Strobe(Cc1101StrobeSTx)
    return err
}

func (device *CC1101) SetNumberOfBytesToReceive(numBytes byte) error {
    txData := make([]byte, 2)
    rxData := make([]byte, 2)
    // packet length
    txData[0] = cc1101PktLen
    txData[1] = numBytes + 1 // +1 for address byte
    return device.tx(false, txData, rxData)
}

func (device *CC1101) GetRSSI() (byte, error) {
    w := []byte{cc1101RSSI, 0xFF}
    r := make([]byte, 2)
    err := device.tx(false, w, r)
    if err != nil {
        return 0, err
    }
    return r[1], nil
}

func (device *CC1101) GetLQI() (byte, error) {
    w := []byte{cc1101Lqi, 0xFF}
    r := make([]byte, 2)
    err := device.tx(false, w, r)
    if err != nil {
        return 0, err
    }
    return r[1] & 0x7F, nil
}

func (device *CC1101) GetLastRSSI() (int, error) {
    return device.lastRSSI, nil
}

func (device *CC1101) GetLastLQI() (int, error) {
    return device.lastLQI, nil
}
