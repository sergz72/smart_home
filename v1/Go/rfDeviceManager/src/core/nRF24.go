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

type NRF24 struct {
    DeviceName   string
    CEGpioName   string
    IRQGpioName  string
    TwoCRCBytes  bool
    RXAddress    []byte
    Channel      byte
    DataRate     NRF24DataRate
    TXPower      int
    port         spi.PortCloser
    conn         spi.Conn
    cePin        gpio.PinIO
    irqPin       gpio.PinIO
    config       byte
    addressWidth int
}

type NRF24Reg byte

const (
    NRF24RegConfig     NRF24Reg  = 0
    NRF24RegEnAA       NRF24Reg  = 1
    NRF24RegEnRXAddr   NRF24Reg  = 2
    NRF24RegSetupAW    NRF24Reg  = 3
    NRF24RegSetupRetr  NRF24Reg  = 4
    NRF24RegChannel    NRF24Reg  = 5
    NRF24RegRfSetup    NRF24Reg  = 6
    NRF24RegStatus     NRF24Reg  = 7
    NRF24RegObserveTX  NRF24Reg  = 8
    NRF24RegRPD        NRF24Reg  = 9
    NRF24RegAddrP0     NRF24Reg  = 0x0A
    NRF24RegAddrP1     NRF24Reg  = 0x0B
    NRF24RegAddrP2     NRF24Reg  = 0x0C
    NRF24RegAddrP3     NRF24Reg  = 0x0D
    NRF24RegAddrP4     NRF24Reg  = 0x0E
    NRF24RegAddrP5     NRF24Reg  = 0x0F
    NRF24RegTXAddr     NRF24Reg  = 0x10
    NRF24RegRXPWP0     NRF24Reg  = 0x11
    NRF24RegRXPWP1     NRF24Reg  = 0x12
    NRF24RegRXPWP2     NRF24Reg  = 0x13
    NRF24RegRXPWP3     NRF24Reg  = 0x14
    NRF24RegRXPWP4     NRF24Reg  = 0x15
    NRF24RegRXPWP5     NRF24Reg  = 0x16
    NRF24RegFIFOStatus NRF24Reg  = 0x17
    NRF24RegDynPD      NRF24Reg  = 0x1C
    NRF24RegFeature    NRF24Reg  = 0x1D

    nRF24RegRXPayload  NRF24Reg  = 0x61
    nRF24RegTXPayload  NRF24Reg  = 0xA0
    nRF24RegFlushTX    NRF24Reg  = 0xE1
    nRF24RegFlushRX    NRF24Reg  = 0xE2
    nRF24RegReuseTXPL  NRF24Reg  = 0xE3
    nRF24RegRXPLWidth  NRF24Reg  = 0x60
    nRF24RegWAckPayload NRF24Reg = 0xA8
    nRF24RegTXPLNoAck  NRF24Reg  = 0xB0
    nRF24RegNOP        NRF24Reg  = 0xFF
)

const nRF24RegWrite = 0x20

type NRF24DataRate byte

const (
    NRF24DataRate250K NRF24DataRate = 0x20
    NRF24DataRate1M   NRF24DataRate = 0
    NRF24DataRate2M   NRF24DataRate = 8
)

type NRF24RXPipe byte

const (
    NRF24RXPipe0 NRF24RXPipe = 0
    NRF24RXPipe1 NRF24RXPipe = 1
    NRF24RXPipe2 NRF24RXPipe = 2
    NRF24RXPipe3 NRF24RXPipe = 3
    NRF24RXPipe4 NRF24RXPipe = 4
    NRF24RXPipe5 NRF24RXPipe = 5
)

type NRF24Feature byte

const (
    NRF24FeatureNone   NRF24Feature = 0
    NRF24FeatureDPL    NRF24Feature = 4
    NRF24FeatureACK    NRF24Feature = 2
    NRF24FeatureDynAck NRF24Feature = 1
)

func (device *NRF24) open() error {
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
    device.cePin = gpioreg.ByName(device.CEGpioName)
    if device.cePin == nil {
        return fmt.Errorf("failed to find %v", device.CEGpioName)
    }
    device.irqPin = gpioreg.ByName(device.IRQGpioName)
    if device.irqPin == nil {
        return fmt.Errorf("failed to find %v", device.IRQGpioName)
    }
    err = device.irqPin.In(gpio.Float, gpio.FallingEdge)
    if err != nil {
        device.port.Close()
        return err
    }
    err = device.ClrCE()
    if err != nil {
        device.port.Close()
    }
    return err
}

func (device *NRF24) SetCE() error {
    return device.cePin.Out(gpio.High)
}

func (device *NRF24) ClrCE() error {
    return device.cePin.Out(gpio.Low)
}

func NRF24Init(configFileName string) (*NRF24, error) {
    var device NRF24

    log.Printf("NRF24Init, config file name : %v\n", configFileName)

    configFileContents, err := ioutil.ReadFile(configFileName)
    if err != nil {
        return nil, err
    }
    err = json.Unmarshal(configFileContents, &device)
    if err != nil {
        return nil, err
    }

    err = device.open()
    if err != nil {
        return nil, err
    }
    err = device.initRegisters()
    if err != nil {
        return nil, err
    }
    return &device, nil
}

func (device *NRF24) initConfigRegister() error {
    device.config = 0x38
    if device.TwoCRCBytes {
        device.config |= 4
    }
    _, err := device.WriteRegister(NRF24RegConfig, []byte{device.config})
    return err
}

func (device *NRF24) PowerControl(powerUp bool) error {
    if powerUp {
        device.config |= 2
    } else {
        device.config &= 0xFD
    }
    _, err := device.WriteRegister(NRF24RegConfig, []byte{device.config})
    if err == nil {
        if powerUp {
            time.Sleep(time.Duration(2) * time.Millisecond)
        } else {
            return device.ClrCE()
        }
    }
    return err
}

func (device *NRF24) SetAA(enAA byte) error {
    _, err := device.WriteRegister(NRF24RegEnAA, []byte{enAA & 0x3F})
    return err
}

func (device *NRF24) RX() error {
    device.config |= 1
    _, err := device.WriteRegister(NRF24RegConfig, []byte{device.config})
    if err != nil {
        return err
    }
    return device.SetCE()
}

func (device *NRF24) TX() error {
    device.config &= 0xFE
    _, err := device.WriteRegister(NRF24RegConfig, []byte{device.config})
    return err
}

func (device *NRF24) RXAddrControl(enRXAddr byte) error {
    _, err := device.WriteRegister(NRF24RegEnRXAddr, []byte{enRXAddr & 0x3F})
    return err
}

func (device *NRF24) setAddressWidth(width int) error {
    if width < 3 || width > 5 {
        return fmt.Errorf("incorrect address width")
    }
    _, err := device.WriteRegister(NRF24RegSetupAW, []byte{byte(width - 2)})
    return err
}

func (device *NRF24) SetRetransmit(retransmit byte) error {
    _, err := device.WriteRegister(NRF24RegSetupRetr, []byte{retransmit})
    return err
}

func (device *NRF24) SetChannel(channel byte) error {
    if channel > 125 {
        return fmt.Errorf("incorrect channel number")
    }
    _, err := device.WriteRegister(NRF24RegChannel, []byte{channel})
    return err
}

func getTXPowerByte(txPower int) byte {
    if txPower <= -18 {
        return 0
    }
    if txPower <= -12 {
        return 2
    }
    if txPower <= -6 {
        return 4
    }
    return 6
}

func (device *NRF24) SetDataRateAndTXPower(dataRate NRF24DataRate, txPower int) error {
    device.DataRate = dataRate
    txPowerByte := getTXPowerByte(txPower)
    _, err := device.WriteRegister(NRF24RegRfSetup, []byte{byte(dataRate)|txPowerByte})
    return err
}

func (device *NRF24) SetTXPower(txPower int) error {
    txPowerByte := getTXPowerByte(txPower)
    _, err := device.WriteRegister(NRF24RegRfSetup, []byte{byte(device.DataRate)|txPowerByte})
    return err
}

func (device *NRF24) ClearIRQFlags() error {
    _, err := device.WriteRegister(NRF24RegStatus, []byte{0x70})
    return err
}

func (device *NRF24) SetRXAddress(pipe NRF24RXPipe, address []byte) error {
    l := len(address)
    if pipe > NRF24RXPipe1 {
        if l != 1 {
            return fmt.Errorf("only 1 byte address is expected for pipes 2-5")
        }
    } else {
        if l != device.addressWidth {
            err := device.setAddressWidth(l)
            if err != nil {
                return err
            }
            device.addressWidth = l
        }
    }
    _, err := device.writeRegister((byte(NRF24RegAddrP0) + byte(pipe))|nRF24RegWrite, address)
    return err
}

func (device *NRF24) SetTXAddress(address []byte) error {
    if len(address) != device.addressWidth {
        return fmt.Errorf("incorrect address width")
    }
    _, err := device.WriteRegister(NRF24RegTXAddr, address)
    return err
}

func (device *NRF24) SetDynPD(value byte) error {
    _, err := device.WriteRegister(NRF24RegDynPD, []byte{value & 0x3F})
    return err
}

func (device *NRF24) SetFeature(value NRF24Feature) error {
    _, err := device.WriteRegister(NRF24RegFeature, []byte{byte(value)})
    return err
}

func (device *NRF24) SetRxPayloadLength(pipe NRF24RXPipe, value byte) error {
    if value > 32 {
        return fmt.Errorf("incorrect rx payload length")
    }
    _, err := device.writeRegister((byte(NRF24RegRXPWP0) + byte(pipe))|nRF24RegWrite, []byte{value})
    return err
}

func (device *NRF24) FlushTX() error {
    _, err := device.command(nRF24RegFlushTX)
    return err
}

func (device *NRF24) FlushRX() error {
    _, err := device.command(nRF24RegFlushRX)
    return err
}

func (device *NRF24) NOP() (byte, error) {
    return device.command(nRF24RegNOP)
}

func (device *NRF24) ReadRXFifo() ([]byte, error) {
    status, err := device.GetStatus()
    if err != nil {
        return nil, err
    }
    pipeNo := (status & 0x0E) >> 1
    if pipeNo > 5 {
        // no data
        return nil, nil
    }
    size, err := device.ReadRegister(NRF24Reg(byte(NRF24RegRXPWP0) + pipeNo), 1)
    if err != nil {
        return nil, err
    }
    if size[1] == 0 {
        // no data
        return nil, nil
    }
    data, err := device.ReadRegister(nRF24RegRXPayload, int(size[1]))
    if err != nil {
        return nil, err
    }
    return data[1:], nil
}

func (device *NRF24) WriteTXFifo(data []byte) error {
    _, err := device.WriteRegister(nRF24RegTXPayload, data)
    return err
}

func (device *NRF24) initRegisters() error {
    err := device.initConfigRegister()
    if err != nil {
        return err
    }
    // no AutoAck
    err = device.SetAA(0)
    if err != nil {
        return err
    }
    //only pipe 1
    err = device.RXAddrControl(2)
    if err != nil {
        return err
    }
    // no retransmit
    err = device.SetRetransmit(0)
    if err != nil {
        return err
    }
    err = device.SetChannel(device.Channel)
    if err != nil {
        return err
    }
    err = device.SetDataRateAndTXPower(device.DataRate, device.TXPower)
    if err != nil {
        return err
    }
    err = device.ClearIRQFlags()
    if err != nil {
        return err
    }
    err = device.SetRXAddress(NRF24RXPipe1, device.RXAddress)
    if err != nil {
        return err
    }
    err = device.SetRxPayloadLength(NRF24RXPipe0, 0)
    if err != nil {
        return err
    }
    err = device.SetRxPayloadLength(NRF24RXPipe1, 0)
    if err != nil {
        return err
    }
    err = device.SetRxPayloadLength(NRF24RXPipe2, 0)
    if err != nil {
        return err
    }
    err = device.SetRxPayloadLength(NRF24RXPipe3, 0)
    if err != nil {
        return err
    }
    err = device.SetRxPayloadLength(NRF24RXPipe4, 0)
    if err != nil {
        return err
    }
    err = device.SetRxPayloadLength(NRF24RXPipe5, 0)
    if err != nil {
        return err
    }
    err = device.SetDynPD(0)
    if err != nil {
        return err
    }
    err = device.SetFeature(NRF24FeatureNone)
    if err != nil {
        return err
    }
    err = device.FlushTX()
    if err != nil {
        return err
    }
    return device.FlushRX()
}

func (device *NRF24) Close() error {
    return device.port.Close()
}

func (device *NRF24) rw(txData []byte) ([]byte, error) {
    rxData := make([]byte, len(txData))
    err := device.conn.Tx(txData, rxData)
    return rxData, err
}

func (device *NRF24) ReadRegister(register NRF24Reg, dataBytes int) ([]byte, error) {
    txData := make([]byte, dataBytes + 1)
    txData[0] = byte(register)
    return device.rw(txData)
}

func (device *NRF24) writeRegister(register byte, data []byte) (byte, error) {
    data = append([]byte{register}, data...)
    //log.Printf("writeRegister: %v\n", data)
    rxData, err := device.rw(data)
    if err != nil {
        return 0, err
    }
    return rxData[0], nil
}

func (device *NRF24) command(command NRF24Reg) (byte, error) {
    rxData, err := device.rw([]byte{byte(command)})
    if err != nil {
        return 0, err
    }
    return rxData[0], nil
}

func (device *NRF24) WriteRegister(register NRF24Reg, data []byte) (byte, error) {
    return device.writeRegister(byte(register)|nRF24RegWrite, data)
}

func (device *NRF24) GetStatus() (byte, error) {
    data, err := device.ReadRegister(NRF24RegStatus, 1)
    if err != nil {
        return 0, err
    }
    return data[1], nil
}

func (device *NRF24) Transmit(address []byte, data []byte) error {
    err := device.SetTXAddress(address)
    if err != nil {
        return err
    }
    err = device.ClrCE()
    if err != nil {
        return err
    }
    err = device.TX()
    if err != nil {
        return err
    }
    err = device.WriteTXFifo(data)
    if err != nil {
        return err
    }
    err = device.SetCE()
    if err != nil {
        return err
    }
    defer func() {
        device.ClrCE()
        device.ClearIRQFlags()
        device.RX()
    }()

    counter := 200 // 20 ms timeout
    for {
        time.Sleep(100 * time.Microsecond)
        status, err := device.GetStatus()
        if err != nil {
            device.FlushTX()
            return err
        }
        if (status & 0x10) != 0 { //MAX_RT
            device.FlushTX()
            return fmt.Errorf("number of retransmits exceed")
        }
        if (status & 0x20) != 0 { //TX_DS
            break
        }
        counter--
        if counter == 0 {
            device.FlushTX()
            return fmt.Errorf("tx timeout")
        }
    }
    return nil
}

func (device *NRF24) SetNumberOfBytesToReceive(numBytes byte) error {
    return device.SetRxPayloadLength(NRF24RXPipe1, numBytes)
}

func (device *NRF24) Receive() ([]byte, error) {
    return device.ReadRXFifo()
}

func (device *NRF24) GetLastRSSI() (int, error) {
    return 0, nil
}

func (device *NRF24) GetLastLQI() (int, error) {
    return 0, nil
}

func (v *NRF24DataRate) UnmarshalJSON(b []byte) error {
    s := strings.Trim(string(b), "\"")
    switch s {
    case "NRF24DataRate250K":
        *v = NRF24DataRate250K;
        return nil
    case "NRF24DataRate1M":
        *v = NRF24DataRate1M;
        return nil
    case "NRF24DataRate2M":
        *v = NRF24DataRate2M;
        return nil
    default:
        return fmt.Errorf("unknown NRF24DataRate value %v", s)
    }
}
