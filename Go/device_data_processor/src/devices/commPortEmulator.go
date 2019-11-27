package devices

import "os"

type Emulator interface {
    Init()
    Read(buffer []byte) (int, error)
    Write(buffer []byte) (int, error)
}

type DeviceFile struct {
    File *os.File
    EmulatorHandler Emulator
}

func (d *DeviceFile) Read(buffer []byte) (int, error) {
    if d.File != nil {
        return d.File.Read(buffer)
    }
    return d.EmulatorHandler.Read(buffer)
}

func (d *DeviceFile) Write(buffer []byte) (int, error) {
    if d.File != nil {
        return d.File.Write(buffer)
    }
    return d.EmulatorHandler.Write(buffer)
}

func (d *DeviceFile) Close() error {
    if d.File != nil {
        return d.File.Close()
    }
    return nil
}