package core

import (
	"core/entities"
	"testing"
	"time"
)

func TestAggregateResults(t *testing.T) {
	dataMap := make(map[string][]OutSensorData)
	var sensor1Data []OutSensorData
	var sensor2Data []OutSensorData
	for i := 0; i < 10; i++ {
		data1 := entities.PropertyMap{
			Values: make(map[string]float64),
			Stats:  nil,
		}
		data2 := entities.PropertyMap{
			Values: make(map[string]float64),
			Stats:  nil,
		}
		data1.Values["temp"] = 5.0 + float64(i)
		data1.Values["humi"] = 80.0 - float64(i)
		data1.Values["pres"] = 1020.0 - float64(i*2)
		data2.Values["vcc"] = 3300.0 + float64(i*20)
		data2.Values["vbat"] = 4200.0 - float64(i*20)

		sensor1Data = append(sensor1Data, OutSensorData{
			locationName: "loc1",
			locationType: "typ1",
			dataType:     "env",
			date:         time.Date(2020, time.January, 2, 0, 0, 0, 0, time.UTC),
			tim:          time.Date(0, 0, 0, 20, i * 5, 0, 0, time.UTC),
			data:         data1,
		})
		sensor2Data = append(sensor2Data, OutSensorData{
			locationName: "loc2",
			locationType: "typ2",
			dataType:     "env",
			date:         time.Date(2020, time.January, 2, 0, 0, 0, 0, time.UTC),
			tim:          time.Date(0, 0, 0, 20, i * 5 + 1, 0, 0, time.UTC),
			data:         data2,
		})
	}
	dataMap["sensor1"] = sensor1Data
	dataMap["sensor2"] = sensor2Data

	result := aggregateResults(dataMap, 3)
	if len(result) != 6 {
		t.Errorf("Wrong result: %v", result)
	}
}

func TestFromPeriod(t *testing.T) {
	d1, t1, d2, t2 := fromPeriod(20, time.Date(2021, 2, 2, 12, 55, 0, 0, time.UTC))
	if d1 != 20210201 {
		t.Errorf("Wrong d1: %v", d1)
	}
	if t1 != 165500 {
		t.Errorf("Wrong t1: %v", t1)
	}
	if d2 != 20210202 {
		t.Errorf("Wrong d2: %v", d2)
	}
	if t2 != 125500 {
		t.Errorf("Wrong t2: %v", t2)
	}
}

func TestFilterSensorData(t *testing.T) {
	config, err := loadConfiguration("../../test_resources/configuration.json")
	if err != nil {
		t.Errorf("loadConfiguration returned an error: %v", err.Error())
		return
	}
	var db DB
	now := time.Date(2021, 1, 8, 12, 0, 0, 0, time.UTC)
	err = db.Load(config, now)
	if err != nil {
		t.Errorf("db.Load returned an error: %v", err.Error())
		return
	}
	filterTest(t, &db, now, 20, 0, 0, "env", 5, "cabinet_int", 96)
	filterTest(t, &db, now, 20, 0, 0, "ele", 2, "garderob_ele", 8)
	filterTest(t, &db, now, 20, 0, 0, "all", 7, "cabinet_int", 96)
	filterTest(t, &db, now, 0, 20210106, 20210107, "all", 7, "cabinet_int", 2)

	filterTest(t, &db, time.Date(2021, 1, 8, 0, 0, 0, 0, time.UTC),
		0, 0, 0, "all", 7, "cabinet_int", 1)
}

func filterTest(t *testing.T, db *DB, now time.Time, period int, start int, end int, dataType string,
	            expectedNumberOfSensors int, sensorName string, expectedNumberOfResults int) {
	result := filterSensorData(db, period, start, end, dataType, now)
	l := len(result)
	if l != expectedNumberOfSensors {
		t.Errorf("Wrong result length: %v", l)
		return
	}
	d, ok := result[sensorName]
	if !ok {
		t.Errorf("%v sensor must be present", sensorName)
		return
	}
	l = len(d)
	if l != expectedNumberOfResults {
		t.Errorf("Wrong result length for %v sensor: %v", sensorName, l)
	}
}

func TestOutSensorDataToString(t *testing.T) {
	vals := make(map[string]float64)
	vals["humi"] = 100
	d := OutSensorData{
		locationName: "loc",
		locationType: "typ",
		dataType:     "dt",
		date:         time.Date(2021, time.February, 3, 0, 0, 0, 0, time.UTC),
		tim:          time.Date(0, 0, 0, 20, 45, 5, 0, time.UTC),
		data:         entities.PropertyMap{
			Values: vals,
			Stats:  nil,
		},
	}
	s := d.toString()
	if s != "{\"locationName\": \"loc\", \"locationType\": \"typ\", \"date\": \"2021-02-03 20:45\", \"dataType\": \"dt\", \"data\": {\"humi\":100}}" {
		t.Errorf("Wrong string value: %v", s)
	}
}