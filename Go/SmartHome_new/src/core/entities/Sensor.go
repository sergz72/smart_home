package entities

import (
	"encoding/json"
	"os"
)

type Sensor struct {
	Id            int
	Name          string
	DataType      string
	LocationId    int
	DeviceId      int
	DeviceSensors map[int]string
	Offsets       map[string]int
}

func ReadSensorsFromJson(path string) (map[int]Sensor, error) {
	var sensors []Sensor
	dat, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}
	if err := json.Unmarshal(dat, &sensors); err != nil {
		return nil, err
	}

	result := make(map[int]Sensor)
	for _, l := range sensors {
		result[l.Id] = l
	}

	return result, nil
}
