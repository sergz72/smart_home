package entities

import (
	"encoding/json"
	"io/ioutil"
)

type Sensor struct {
	Id int
	Name string
	DataType string
	LocationId int
}

func ReadSensorsFromJson(path string) (map[int]Sensor, error) {
	var sensors []Sensor
	dat, err := ioutil.ReadFile(path)
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