package core

import (
	"bytes"
	"crypto/aes"
	"encoding/binary"
	"log"
	"smartHome/src/core/entities"
	"time"
)

var deviceDataOffsets = []int{10, 13, 20, 23, 26, 29, 36, 39, 42, 45}

var lastDeviceTime map[int]uint32

func init() {
	lastDeviceTime = make(map[int]uint32)
}

func tryLoadSensorData(server *Server, data []byte, timeOffset int, useDeviceTime bool) bool {
	if server.deviceKey == nil {
		return false
	}
	l := len(data)
	if l != 16 && l != 32 && l != 48 {
		return false
	}

	block, err := aes.NewCipher(server.deviceKey)
	if err != nil {
		return false
	}
	for i := 0; i < l; i += len(server.deviceKey) {
		block.Decrypt(data[i:], data[i:])
	}
	deviceID := int(binary.LittleEndian.Uint16(data[4:]))
	sensors, ok := server.db.DeviceToSensors[deviceID]
	if !ok {
		return false
	}
	crc := binary.LittleEndian.Uint32(data)
	if crc != calculateCRC(data, server.deviceKey) {
		return false
	}
	eventTime := binary.LittleEndian.Uint32(data[6:])
	if l > 16 {
		eventTime2 := binary.LittleEndian.Uint32(data[16:])
		if eventTime != eventTime2 {
			return false
		}
	}
	if l > 32 {
		eventTime3 := binary.LittleEndian.Uint32(data[32:])
		if eventTime != eventTime3 {
			return false
		}
	}
	if eventTime <= lastDeviceTime[deviceID] {
		return false
	}
	messages := buildMessages(server.db, sensors, data, timeOffset, eventTime, useDeviceTime)
	if messages == nil {
		return false
	}
	lastDeviceTime[deviceID] = eventTime
	log.Printf("Received sensor message from device %v timestamp %v", deviceID, eventTime)
	for k, v := range messages {
		log.Printf("%v %v", k, v)
	}
	for sensorId, message := range messages {
		err := server.db.saveSensorData(sensorId, *message)
		if err != nil {
			log.Println(err.Error())
		}
	}
	return true
}

func buildMessages(db *DB, sensors []int, data []byte, timeOffset int, eventTime uint32, useDeviceTime bool) map[int]*decodedMessage {
	result := make(map[int]*decodedMessage)

	for deviceSensorId, offset := range deviceDataOffsets {
		realSensorId, dataName := db.GetSensor(sensors, deviceSensorId)
		if dataName == "" {
			break
		}
		b := []byte{data[offset], data[offset+1], data[offset+2], 0}
		if (b[2] & 0x80) != 0 {
			b[3] = 0xFF
		}
		var sensorData int32
		buffer := bytes.NewReader(b)
		err := binary.Read(buffer, binary.LittleEndian, &sensorData)
		if err != nil {
			log.Println(err.Error())
			return nil
		}
		if dataName == "pwr" {
			sensorData *= 100
		}
		m, ok := result[realSensorId]
		if ok {
			m.Message.Values[dataName] = int(sensorData)
		} else {
			var t time.Time
			if useDeviceTime {
				t = time.Unix(int64(eventTime), 0).Add(time.Duration(timeOffset) * time.Hour)
			} else {
				t = time.Now().Add(time.Duration(timeOffset) * time.Hour)
			}
			m = &decodedMessage{
				MessageTime: t,
				Message: entities.PropertyMap{
					Values: map[string]int{
						dataName: int(sensorData),
					},
				},
			}
			result[realSensorId] = m
		}
	}
	return result
}

func calculateCRC(bytes []byte, dkey []byte) uint32 {
	var crc uint32
	var key uint32
	for i := 0; i < len(dkey); i += 4 {
		key += binary.LittleEndian.Uint32(dkey[i:])
	}
	for i := 4; i < len(bytes); i += 4 {
		crc += binary.LittleEndian.Uint32(bytes[i:])
	}
	return crc * key
}
