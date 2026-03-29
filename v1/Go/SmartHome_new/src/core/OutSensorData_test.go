package core

import (
	"math/rand"
	"smartHome/src/core/entities"
	"testing"
	"time"
)

func TestOutSensorDataLength(t *testing.T) {
	sd := OutSensorData{timeData: map[int][]entities.SensorData{
		1: {{}, {}, {}},
		2: {{}, {}},
		3: {{}},
	}}
	if sd.length() != 6 {
		t.Fatal("wrong length")
	}
}

func TestOutSensorDataAggregateValues(t *testing.T) {
	r := rand.New(rand.NewSource(time.Now().UnixNano()))
	sd := OutSensorData{timeData: map[int][]entities.SensorData{
		1: makeTestSensorDataValues(r, 9),
		2: makeTestSensorDataValues(r, 10),
		3: makeTestSensorDataValues(r, 10),
		4: makeTestSensorDataValues(r, 1),
	}}
	aggregated := sd.aggregate(2, &configuration{
		TotalCalculation: map[string]int{"pwr": 12},
	})
	if len(aggregated.TimeData) != 3 {
		t.Fatal("wrong length")
	}
	checkTotal(t, sd.timeData, &aggregated)

	aggregated = sd.aggregate(1, &configuration{
		TotalCalculation: map[string]int{"pwr": 12},
	})
	if len(aggregated.TimeData) != 4 {
		t.Fatal("wrong length")
	}
	checkTotal(t, sd.timeData, &aggregated)
}

func TestOutSensorDataAggregateStats(t *testing.T) {
	r := rand.New(rand.NewSource(time.Now().UnixNano()))
	sd := OutSensorData{timeData: map[int][]entities.SensorData{
		1: makeTestSensorDataStats(r, 9),
		2: makeTestSensorDataStats(r, 10),
		3: makeTestSensorDataStats(r, 10),
		4: makeTestSensorDataStats(r, 1),
	}}
	aggregated := sd.aggregate(2, &configuration{
		TotalCalculation: map[string]int{"pwr": 12},
	})
	if len(aggregated.TimeData) != 3 {
		t.Fatal("wrong length")
	}
	checkTotal(t, sd.timeData, &aggregated)

	aggregated = sd.aggregate(1, &configuration{
		TotalCalculation: map[string]int{"pwr": 12},
	})
	if len(aggregated.TimeData) != 4 {
		t.Fatal("wrong length")
	}
	checkTotal(t, sd.timeData, &aggregated)
}

func checkTotal(t *testing.T, timeData map[int][]entities.SensorData, aggregated *OutSensorData) {
	expected := 0
	div := 1

	for _, data := range timeData {
		for _, sd := range data {
			if sd.Data.Values != nil {
				expected += sd.Data.Values["pwr"]
				div = 12
			} else {
				expected += sd.Data.Stats["pwr"]["Sum"]
				//fmt.Printf("%v %v\n", sd.Data.Stats["pwr"]["Sum"], expected)
			}
		}
	}

	if aggregated.Total != expected/div {
		t.Fatal("wrong total")
	}

	if div == 1 {
		expected = 0

		for _, data := range aggregated.TimeData {
			for _, sd := range data.Data {
				expected += sd.Data.Stats["pwr"]["Sum"]
			}
		}

		if aggregated.Total != expected {
			t.Fatal("wrong total")
		}
	}
}

func makeTestSensorDataValues(r *rand.Rand, n int) []entities.SensorData {
	var result []entities.SensorData

	eventTime := 0
	for n > 0 {
		result = append(result, makeTestSensorDataValue(r, eventTime))
		n--
		eventTime += 10
	}

	return result
}

func makeTestSensorDataStats(r *rand.Rand, n int) []entities.SensorData {
	var result []entities.SensorData

	eventTime := 0
	for n > 0 {
		result = append(result, makeTestSensorDataStat(r, eventTime))
		n--
		eventTime += 10
	}

	return result
}

func makeTestSensorDataValue(r *rand.Rand, eventTime int) entities.SensorData {
	return entities.SensorData{
		EventTime: eventTime,
		Data: entities.PropertyMap{
			Values: map[string]int{
				"humi": r.Intn(10000),
				"temp": r.Intn(2000) + 1800,
				"pwr":  r.Intn(10000),
			},
		},
	}
}

func makeTestSensorDataStat(r *rand.Rand, eventTime int) entities.SensorData {
	return entities.SensorData{
		EventTime: eventTime,
		Data: entities.PropertyMap{
			Stats: map[string]map[string]int{
				"humi": makeStats(r, 10000),
				"temp": makeStats(r, 2000),
				"pwr":  makeStats(r, 10000),
			},
		},
	}
}

func makeStats(r *rand.Rand, n int) map[string]int {
	min := r.Intn(n)
	return map[string]int{
		"Min": min,
		"Max": min + 1000,
		"Avg": r.Intn(n),
		"Sum": r.Intn(n),
	}
}
