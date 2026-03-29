package core

import (
	"smartHome/src/core/entities"
	"testing"
	"time"
)

func TestDBLoad(t *testing.T) {
	config, err := loadConfiguration("../../test_resources/testConfiguration.json")
	if err != nil {
		t.Errorf("loadConfiguration returned an error: %v", err.Error())
		return
	}
	var db DB
	err = db.Load(config, time.Date(2021, 2, 6, 0, 0, 0, 0, time.UTC))
	if err != nil {
		t.Errorf("db.Load returned an error: %v", err.Error())
		return
	}
	testLocations(t, &db)
	testSensors(t, &db)
	testDataTypeMap(t, &db)
	testSensorDataMap(t, &db)
	testSensorDataAggregated(t, &db)
	testUpdateOffsets(t, &db)
}

func testSensorDataAggregated(t *testing.T, db *DB) {
	l := len(db.SensorDataAggregated)
	if l != 3 {
		t.Errorf("db.Load returned incorrect aggregated sensor data map length: %v", l)
		return
	}
	d, ok := db.SensorDataAggregated[20210107]
	if !ok {
		t.Error("aggregated sensor data map must contain key 20210107")
		return
	}
	l = len(d)
	if l != 7 {
		t.Errorf("db.Load returned incorrect aggregated sensor data map length for key 20210107: %v", l)
		return
	}
	sd, ok := d[1]
	if !ok {
		t.Error("aggregated sensor data map must contain key 1 for key 20210107")
		return
	}
	if sd.EventTime != 0 {
		t.Error("aggregated sensor data stats event time should be 0")
		return
	}
	if sd.Data.Values != nil {
		t.Error("aggregated sensor data stats values should be nil")
		return
	}
	l = len(sd.Data.Stats)
	if l != 3 {
		t.Errorf("db.Load returned incorrect aggregated sensor data stats length for key 20210107/1: %v", l)
		return
	}
}

func testSensorDataMap(t *testing.T, db *DB) {
	l := len(db.SensorDataMap)
	if l != 1 {
		t.Errorf("db.Load returned incorrect sensor data map length: %v", l)
		return
	}
	d, ok := db.SensorDataMap[20210107]
	if !ok {
		t.Error("sensor data map must contain key 20210107")
		return
	}
	l = len(d)
	if l != 7 {
		t.Errorf("db.Load returned incorrect sensor data map length for key 20210107: %v", l)
		return
	}
	sd, ok := d[1]
	if !ok {
		t.Error("sensor data map must contain key 1 for key 20210107")
		return
	}
	l = len(sd)
	if l != 288 {
		t.Errorf("db.Load returned incorrect sensor data map length for key 20210107/1: %v", l)
		return
	}
	var sdata *entities.SensorData
	for _, data := range sd {
		if data.EventTime == 1000 {
			sdata = &data
			break
		}
	}
	if sdata == nil {
		t.Error("sensor data map for key 20210107/1: time 1000 is missing")
		return
	}
	if sdata.Data.Stats != nil {
		t.Error("sensor data map for key 20210107/1: stats should be nil")
		return
	}
	l = len(sdata.Data.Values)
	if l != 3 {
		t.Errorf("sensor data map for key 20210107/1: wrong values map length: %v", l)
		return
	}
	checkData(t, sdata.Data.Values, "humi", 3760)
	checkData(t, sdata.Data.Values, "pres", 100300)
	checkData(t, sdata.Data.Values, "temp", 2020)
}

func checkData(t *testing.T, values map[string]int, dataName string, dataValue int) {
	v, ok := values[dataName]
	if !ok {
		t.Errorf("sensor data map for key 20210107/1: %v data is missing", dataName)
		return
	}
	if v != dataValue {
		t.Errorf("sensor data map for key 20210107/1: %v data is incorrect: %v", dataName, dataValue)
	}
}

func testLocations(t *testing.T, db *DB) {
	l := len(db.Locations)
	if l != 13 {
		t.Errorf("db.Load returned incorrect locations map length: %v", l)
		return
	}
	loc, ok := db.Locations[8]
	if !ok {
		t.Error("location with id 8 is missing")
		return
	}
	if loc.Name != "Комната Игоря" || loc.LocationType != "fl1" {
		t.Error("location with id 8 has wrong data")
	}
}

func testSensors(t *testing.T, db *DB) {
	l := len(db.Sensors)
	if l != 12 {
		t.Errorf("db.Load returned incorrect sensors map length: %v", l)
		return
	}
	s, ok := db.Sensors[3]
	if !ok {
		t.Error("sensor with id 3 is missing")
		return
	}
	if s.Name != "garderob_int" || s.DataType != "env" || s.LocationId != 3 {
		t.Error("sensor with id 3 has wrong data")
	}
}

func testDataTypeMap(t *testing.T, db *DB) {
	l := len(db.DataTypeMap)
	if l != 3 {
		t.Errorf("db.Load returned incorrect data type map length: %v", l)
		return
	}
	m, ok := db.DataTypeMap["env"]
	if !ok {
		t.Error("data type map has no 'env' key")
		return
	}
	if len(m) != 8 {
		t.Error("data type map has with 'env' key should have length=5")
		return
	}
	cnt := 0
	for _, id := range m {
		if id == 1 || id == 2 || id == 3 || id == 4 || id == 6 || id == 8 || id == 9 || id == 11 {
			cnt++
		}
	}
	if cnt != 8 {
		t.Error("data type map has with 'env' key contains wrong data")
	}
}

func testUpdateOffsets(t *testing.T, db *DB) {
	m := entities.PropertyMap{
		Values: map[string]int{"temp": 1100, "humi": 5000},
	}
	db.updateOffsets(1, m)
	if m.Values["temp"] != 1100 {
		t.Fatal("Wrong temp value")
	}
	if m.Values["humi"] != 5000 {
		t.Fatal("Wrong temp value")
	}
	sensor, ok := db.Sensors[1]
	if ok {
		sensor.Offsets = map[string]int{"temp": -100}
		db.Sensors[1] = sensor
	}
	db.updateOffsets(1, m)
	if m.Values["temp"] != 1000 {
		t.Fatal("Wrong corrected temp value")
	}
	if m.Values["humi"] != 5000 {
		t.Fatal("Wrong temp value")
	}
}
