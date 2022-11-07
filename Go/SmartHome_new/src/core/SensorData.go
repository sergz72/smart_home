package core

import (
	"bytes"
	"encoding/json"
	"fmt"
	"net/url"
	"smartHome/src/core/entities"
	"strconv"
	"time"
)

func SensorDataHandler(server *Server, w *bytes.Buffer, req string) {
	values, err := url.ParseQuery(req)
	if err != nil {
		w.Write([]byte("400 Bad request: query parsing error"))
		return
	}
	dataType := values.Get("data_type")
	if len(dataType) == 0 {
		w.Write([]byte("400 Bad request: missing or empty datatype parameter"))
		return
	}
	maxPointsValue := values.Get("maxPoints")
	var maxPoints int
	if len(maxPointsValue) > 0 {
		maxPoints, err = strconv.Atoi(maxPointsValue)
		if err != nil || maxPoints <= 0 {
			w.Write([]byte(fmt.Sprintf("400 Bad request: invalid maxPoints parameter %v", maxPointsValue)))
			return
		}
	} else {
		maxPoints = 1000000
	}
	start := 0
	end := 0
	period := 0
	starts := values.Get("start")
	if len(starts) > 0 {
		ends := values.Get("end")
		if len(starts) != 8 || len(ends) != 8 {
			w.Write([]byte(fmt.Sprintf("400 Bad request: invalid start or end parameter %v %v", starts, ends)))
			return
		}
		start, err = strconv.Atoi(starts)
		if err != nil || start <= 0 {
			w.Write([]byte(fmt.Sprintf("400 Bad request: invalid start parameter %v", starts)))
			return
		}
		end, err = strconv.Atoi(ends)
		if err != nil || end <= 0 {
			w.Write([]byte(fmt.Sprintf("400 Bad request: invalid end parameter %v", ends)))
			return
		}
	} else {
		periods := values.Get("period")
		if len(periods) > 0 {
			period, err = strconv.Atoi(periods)
			if err != nil || period <= 0 {
				w.Write([]byte(fmt.Sprintf("400 Bad request: invalid period parameter: %v", periods)))
				return
			}
		}
	}

	server.db.mutex.Lock()
	resultMap := filterSensorData(server.db, period, start, end, dataType, time.Now(), server.config.TimeOffset)
	server.db.mutex.Unlock()
	results := aggregateResults(resultMap, maxPoints, server.config)
	data, err := json.Marshal(results)
	if err != nil {
		w.Write([]byte(fmt.Sprintf("500 json.Marshal error: %v", err)))
		return
	}
	w.Write(data)
}

// returns map sensor id -> [map date to OutSensorData]
func filterSensorData(db *DB, period int, start int, end int, dataType string, now time.Time, timeOffset int) map[int]*OutSensorData {
	returnLatest := period == 0 && start == 0 && end == 0
	if returnLatest {
		period = 1
	}
	if period != 0 && timeOffset < 0 {
		period -= timeOffset
	}
	resultMap := make(map[int]*OutSensorData)
	fromTime := 0
	endTime := 235959
	useAggregatedData := period == 0
	if !useAggregatedData {
		start, fromTime, end, endTime = fromPeriod(period, now)
	}
	all := dataType == "all"
	for start <= end {
		if useAggregatedData {
			data, ok := db.SensorDataAggregated[start]
			if ok {
				for k, v := range data {
					if all || isValidDataType(db, k, dataType) {
						addToResultMap(db, resultMap, start, k, v)
					}
				}
			}
		} else {
			data, ok := db.SensorDataMap[start]
			if ok {
				toTime := 235959
				if start == end {
					toTime = endTime
				}
				for k, v := range data {
					for _, d := range v {
						if d.EventTime >= fromTime && d.EventTime <= toTime &&
							(all || isValidDataType(db, k, dataType)) {
							addToResultMap(db, resultMap, start, k, d)
						}
					}
				}
			}
		}
		start = nextDate(start)
		fromTime = 0
	}
	if returnLatest {
		getLatestValues(resultMap)
	}
	return resultMap
}

func getLatestValues(resultMap map[int]*OutSensorData) {
	for _, v := range resultMap {
		v.timeData = getLatestValue(v.timeData)
	}
}

func getLatestValue(data map[int][]entities.SensorData) map[int][]entities.SensorData {
	var latest entities.SensorData
	var maxDate int
	for k := range data {
		if k > maxDate {
			maxDate = k
		}
	}
	maxTime := -1
	for _, e := range data[maxDate] {
		if e.EventTime > maxTime {
			maxTime = e.EventTime
			latest = e
		}
	}
	return map[int][]entities.SensorData{maxDate: {latest}}
}

func isValidDataType(db *DB, sensorId int, dataType string) bool {
	ids, ok := db.DataTypeMap[dataType]
	if ok {
		for _, id := range ids {
			if id == sensorId {
				return true
			}
		}
	}
	return false
}

// map sensorId -> OutSensorData
func addToResultMap(db *DB, resultMap map[int]*OutSensorData, date int, sensorId int, data entities.SensorData) {
	result, ok := resultMap[sensorId]
	if !ok {
		result = MakeOutSensorData(db, sensorId)
		resultMap[sensorId] = result
	}
	sd, ok := result.timeData[date]
	if ok {
		result.timeData[date] = append(sd, data)
	} else {
		result.timeData[date] = []entities.SensorData{data}
	}
}

func fromPeriod(period int, now time.Time) (int, int, int, int) {
	from := now.Add(-time.Hour * time.Duration(period))
	d1, t1 := toDateTime(now)
	d2, t2 := toDateTime(from)
	return d2, t2, d1, t1
}

type sensorStats struct {
	Min int
	Avg int
	Max int
	Cnt int
	Sum int
}

func aggregateSensorDataArray(data map[int][]entities.SensorData) map[int]entities.SensorData {
	result := make(map[int]entities.SensorData)
	for sensorId, dataArray := range data {
		statsMap := make(map[string]*sensorStats)
		for _, d := range dataArray {
			for k, v := range d.Data.Values {
				dv, ok := statsMap[k]
				if !ok {
					statsMap[k] = &sensorStats{
						Min: v,
						Avg: v,
						Max: v,
						Sum: v,
						Cnt: 1,
					}
				} else {
					if v < dv.Min {
						dv.Min = v
					}
					if v > dv.Max {
						dv.Max = v
					}
					dv.Avg += v
					dv.Sum += v
					dv.Cnt++
				}
			}
		}
		out := entities.SensorData{
			EventTime: 0,
			Data:      entities.PropertyMap{Stats: make(map[string]map[string]int)},
		}
		for prop, stats := range statsMap {
			outv := make(map[string]int)
			outv["Min"] = stats.Min
			outv["Max"] = stats.Max
			outv["Avg"] = stats.Avg / stats.Cnt
			outv["Cnt"] = stats.Cnt
			outv["Sum"] = stats.Sum
			out.Data.Stats[prop] = outv
		}
		result[sensorId] = out
	}
	return result
}
