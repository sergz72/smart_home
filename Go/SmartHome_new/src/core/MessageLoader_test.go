package core

import (
	"crypto/aes"
	"encoding/binary"
	"smartHome/src/core/entities"
	"sync"
	"testing"
)

var testKey = []byte{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}

func TestSensorDataLoad16(t *testing.T) {
	server := Server{
		deviceKey: testKey,
		db: &DB{
			Sensors: map[int]entities.Sensor{
				1: {
					Id:            1,
					DeviceId:      1,
					DeviceSensors: map[int]string{0: "humi", 1: "temp"},
				},
			},
			SensorDataMap:   make(map[int]map[int][]entities.SensorData),
			DataToBeSaved:   make(map[int][]int),
			DeviceToSensors: map[int][]int{1: {1}},
			mutex:           sync.RWMutex{},
		},
		mutex: sync.Mutex{},
	}
	var buffer [16]byte
	binary.LittleEndian.PutUint16(buffer[4:], 1)  // deviceID
	binary.LittleEndian.PutUint32(buffer[6:], 10) // time
	buffer[10] = 0x18
	buffer[11] = 0xFC
	buffer[12] = 0xFF // -1000
	buffer[13] = 0xE8
	buffer[14] = 0x03
	buffer[15] = 0x00                                                          // 1000
	binary.LittleEndian.PutUint32(buffer[:], calculateCRC(buffer[:], testKey)) // crc
	block, err := aes.NewCipher(testKey)
	if err != nil {
		t.Fatal(err.Error())
	}
	block.Encrypt(buffer[:], buffer[:])

	if !tryLoadSensorData(&server, buffer[:], 0, false) {
		t.Fatal("failed to load sensor data")
	}

	if tryLoadSensorData(&server, buffer[:], 0, false) {
		t.Fatal("should fail to load sensor data")
	}
}

func TestSensorDataLoad32(t *testing.T) {
	server := Server{
		deviceKey: testKey,
		db: &DB{
			Sensors: map[int]entities.Sensor{
				1: {
					Id:            1,
					DeviceId:      1,
					DeviceSensors: map[int]string{0: "humi", 1: "temp", 2: "pres", 3: "vcc", 4: "vbat", 5: "vvv"},
				},
			},
			SensorDataMap:   make(map[int]map[int][]entities.SensorData),
			DataToBeSaved:   make(map[int][]int),
			DeviceToSensors: map[int][]int{1: {1}},
			mutex:           sync.RWMutex{},
		},
		mutex: sync.Mutex{},
	}
	var buffer [32]byte
	binary.LittleEndian.PutUint16(buffer[4:], 1)   // deviceID
	binary.LittleEndian.PutUint32(buffer[6:], 10)  // time
	binary.LittleEndian.PutUint32(buffer[16:], 10) // time
	buffer[10] = 0x18
	buffer[11] = 0xFC
	buffer[12] = 0xFF // -1000
	buffer[13] = 0xE8
	buffer[14] = 0x03
	buffer[15] = 0x00 // 1000
	buffer[20] = 0x18
	buffer[21] = 0xFC
	buffer[22] = 0xFF // -1000
	buffer[23] = 0xE8
	buffer[24] = 0x03
	buffer[25] = 0x00 // 1000
	buffer[26] = 0x18
	buffer[27] = 0xFC
	buffer[28] = 0xFF // -1000
	buffer[29] = 0xE8
	buffer[30] = 0x03
	buffer[31] = 0x00                                                          // 1000
	binary.LittleEndian.PutUint32(buffer[:], calculateCRC(buffer[:], testKey)) // crc
	block, err := aes.NewCipher(testKey)
	if err != nil {
		t.Fatal(err.Error())
	}
	block.Encrypt(buffer[:], buffer[:])
	block.Encrypt(buffer[16:], buffer[16:])

	lastDeviceTime[1] = 0
	if !tryLoadSensorData(&server, buffer[:], 0, false) {
		t.Fatal("failed to load sensor data")
	}

	if tryLoadSensorData(&server, buffer[:], 0, false) {
		t.Fatal("should fail to load sensor data")
	}
}

func TestCalculateCRC(t *testing.T) {
	bytes := make([]byte, 32)
	dkey := make([]byte, 16)
	for i := 0; i < 32; i++ {
		bytes[i] = byte(i)
	}
	for i := 0; i < 16; i++ {
		dkey[i] = byte(i)
	}
	crc := calculateCRC(bytes, dkey)
	if crc != 116093568 {
		t.Fatal("Incorrect CRC: ", crc)
	}
}
