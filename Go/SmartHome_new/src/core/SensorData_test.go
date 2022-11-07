package core

import (
	"smartHome/src/core/entities"
	"testing"
	"time"
)

func TestAggregateResults(t *testing.T) {
	var timeData1 []entities.SensorData
	var timeData2 []entities.SensorData
	for i := 0; i < 10; i++ {
		data1 := entities.PropertyMap{
			Values: make(map[string]int),
			Stats:  nil,
		}
		data2 := entities.PropertyMap{
			Values: make(map[string]int),
			Stats:  nil,
		}
		data1.Values["temp"] = 5 + i
		data1.Values["humi"] = 80 - i
		data1.Values["pres"] = 1020 - i*2
		timeData1 = append(timeData1, entities.SensorData{
			EventTime: i,
			Data:      data1,
		})
		data2.Values["vcc"] = 3300 + i*20
		data2.Values["vbat"] = 4200 - i*20
		timeData2 = append(timeData2, entities.SensorData{
			EventTime: i,
			Data:      data2,
		})
	}

	sensor1Data := OutSensorData{
		LocationName: "loc1",
		LocationType: "typ1",
		DataType:     "env",
		timeData:     map[int][]entities.SensorData{1: timeData1},
	}
	sensor2Data := OutSensorData{
		LocationName: "loc2",
		LocationType: "typ2",
		DataType:     "ele",
		timeData:     map[int][]entities.SensorData{1: timeData2},
	}

	dataMap := map[int]*OutSensorData{1: &sensor1Data, 2: &sensor2Data}

	result := aggregateResults(dataMap, 3, &configuration{})
	if len(result) != 2 {
		t.Fatal("Wrong result length")
	}
	if len(result[0].TimeData) != 1 {
		t.Fatal("Wrong result[0].TimeData length")
	}
	if len(result[1].TimeData) != 1 {
		t.Fatal("Wrong result[1].TimeData length")
	}
	if len(result[0].TimeData[0].Data) != 3 {
		t.Fatal("Wrong result[0].TimeData[0].Data length")
	}
	if len(result[1].TimeData[0].Data) != 3 {
		t.Fatal("Wrong result[1].TimeData[0].Data length")
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
	config, err := loadConfiguration("../../test_resources/testConfiguration.json")
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
	filterTest(t, &db, now, 20, 0, 0, "env", 5, 1, 96)
	filterTest(t, &db, now, 20, 0, 0, "ele", 2, 5, 8)
	filterTest(t, &db, now, 20, 0, 0, "all", 7, 1, 96)
	filterTest(t, &db, now, 0, 20210106, 20210107, "all", 7, 1, 2)

	filterTest(t, &db, time.Date(2021, 1, 8, 0, 0, 0, 0, time.UTC),
		0, 0, 0, "all", 7, 1, 1)
}

func filterTest(t *testing.T, db *DB, now time.Time, period int, start int, end int, dataType string,
	expectedNumberOfSensors int, sensorId int, expectedNumberOfResults int) {
	result := filterSensorData(db, period, start, end, dataType, now, 0)
	l := len(result)
	if l != expectedNumberOfSensors {
		t.Errorf("Wrong result length: %v", l)
		return
	}
	d, ok := result[sensorId]
	if !ok {
		t.Errorf("%v sensor must be present", sensorId)
		return
	}
	l = d.length()
	if l != expectedNumberOfResults {
		t.Errorf("Wrong result length for %v sensor: %v", sensorId, l)
	}
}
