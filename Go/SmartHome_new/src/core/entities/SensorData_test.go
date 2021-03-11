package entities

import (
	"encoding/json"
	"testing"
)

func TestMarshalSensorData(t *testing.T) {
	vals := make(map[string]float64)
	vals["humi"] = 100
	sd := []SensorData{{
		EventTime: 100102,
		Data:      PropertyMap{
			Values: vals,
			Stats:  nil,
		},
	}}
	data, err := json.Marshal(sd)
	if err != nil {
		t.Error(err)
		return
	}
	s := string(data)
	if s != "[{\"EventTime\":100102,\"Data\":{\"humi\":100}}]" {
		t.Errorf("invalid data: %v", s)
	}
}