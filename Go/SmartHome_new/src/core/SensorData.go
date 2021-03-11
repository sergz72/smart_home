package core

import (
	"bytes"
	"core/entities"
	"fmt"
	"net/url"
	"strconv"
	"strings"
	"time"
)

type OutSensorData struct {
	locationName string
	locationType string
	dataType string
	date time.Time
	tim time.Time
	data entities.PropertyMap
}

func (d *OutSensorData) toString() string {
	tim := addTimeToDate(d.date, d.tim)
	return fmt.Sprintf("{\"locationName\": \"%v\", \"locationType\": \"%v\", \"date\": \"%v\", \"dataType\": \"%v\", \"data\": %v}",
		d.locationName, d.locationType, tim.Format("2006-01-02 15:04"), d.dataType, d.data.ToString())
}

func MakeOutSensorData(db *DB, d entities.SensorData, date int, sensorId int) OutSensorData {
	s := db.Sensors[sensorId]
	l := db.Locations[s.LocationId]
	return OutSensorData{
		locationName: l.Name,
		locationType: l.LocationType,
		dataType:     s.DataType,
		date:         buildDate(date),
		tim:          buildTime(d.EventTime),
		data:         d.Data,
	}
}

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
	resultMap := filterSensorData(server.db, period, start, end, dataType, time.Now())
	server.db.mutex.Unlock()
	results := aggregateResults(resultMap, maxPoints)
	w.Write([]byte("["))
	w.Write([]byte(strings.Join(results, ",")))
	w.Write([]byte("]"))
}

func filterSensorData(db *DB, period int, start int, end int, dataType string, now time.Time) map[string][]OutSensorData {
	returnLatest := period == 0 && start == 0 && end == 0
	if returnLatest {
		period = 1
	}
	resultMap := make(map[string][]OutSensorData)
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
		return getLatestValues(resultMap)
	}
	return resultMap
}

func getLatestValues(resultMap map[string][]OutSensorData) map[string][]OutSensorData {
	result := make(map[string][]OutSensorData)
	for k, v := range resultMap {
		result[k] = getLatestValue(v)
	}
	return result
}

func getLatestValue(data []OutSensorData) []OutSensorData {
	var latest *OutSensorData
	var dt time.Time
	for _, d := range data {
		if latest == nil {
			latest = &d
			dt = addTimeToDate(latest.date, latest.tim)
		} else {
			dt2 := addTimeToDate(d.date, d.tim)
			if dt2.After(dt) {
				latest = &d
			}
		}
	}
	if latest == nil {
		return []OutSensorData{}
	}
	return []OutSensorData{*latest}
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

func addToResultMap(db *DB, resultMap map[string][]OutSensorData, date int, sensorId int, data entities.SensorData) {
	sensorName := db.Sensors[sensorId].Name
	result, ok := resultMap[sensorName]
	if ok {
		resultMap[sensorName] = append(result, MakeOutSensorData(db, data, date, sensorId))
	} else {
		resultMap[sensorName] = []OutSensorData{MakeOutSensorData(db, data, date, sensorId)}
	}
}

func fromPeriod(period int, now time.Time) (int, int, int, int) {
	from := now.Add(-time.Hour * time.Duration(period))
	d1, t1 := toDateTime(now)
	d2, t2 := toDateTime(from)
	return d2, t2, d1, t1
}

func aggregateResults(resultMap map[string][]OutSensorData, maxPoints int) []string {
	var result []string

	for _, sensorData := range resultMap {
		compressionLevel := len(sensorData) / maxPoints + 1
		for i := 0; i < len(sensorData); i += compressionLevel {
			result = append(result, aggregateSensorData(sensorData, i, compressionLevel))
		}
	}
	return result
}

func aggregateSensorData(data []OutSensorData, from int, compressionLevel int) string {
	if from >= len(data) {
		return ""
	}
	var result = OutSensorData{
		locationName: data[from].locationName,
		locationType: data[from].locationType,
		dataType:     data[from].dataType,
		date:         data[from].date,
		tim:          data[from].tim,
		data:         data[from].data,
	}
	if result.data.Stats != nil {
		for _, v := range result.data.Stats {
			v["Cnt"] = 1
		}
	}
	if compressionLevel > 1 {
		i := from + 1
		compressionLevel--
		if result.data.Values != nil {
			for compressionLevel > 0 && i < len(data) {
				for k, v := range result.data.Values {
					v2, ok := data[i].data.Values[k]
					if ok {
						result.data.Values[k] = v + v2
					}
				}
				i++
				compressionLevel--
			}
			for k, v := range result.data.Values {
				result.data.Values[k] = v / float64(i - from)
			}
		} else {
			for compressionLevel > 0 && i < len(data) {
				for k, v := range result.data.Stats {
					v2, ok := data[i].data.Stats[k]
					if ok {
						for kk, vv := range v {
							vv2, ok := v2[kk]
							if ok {
								switch kk {
								case "Min":
									if vv2 < vv {
										v[kk] = vv2
									}
								case "Max":
									if vv2 > vv {
										v[kk] = vv2
									}
								case "Avg":
									v[kk] = vv2 + vv
								case "Cnt":
									v[kk] = vv + 1
								}
							}
						}
					}
				}
				i++
				compressionLevel--
			}
			for _, v := range result.data.Stats {
				v["Avg"] = v["Avg"] / float64(i - from)
			}
		}
	}
	return result.toString()
}

type sensorStats struct {
	Min float64
	Avg float64
	Max float64
	Cnt int
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
					dv.Cnt++
				}
			}
		}
		out := entities.SensorData{
			EventTime: 0,
			Data:      entities.PropertyMap{Stats: make(map[string]map[string]float64)},
		}
		for prop, stats := range statsMap {
			outv := make(map[string]float64)
			outv["Min"] = stats.Min
			outv["Max"] = stats.Max
			outv["Avg"] = stats.Avg / float64(stats.Cnt)
			outv["Cnt"] = float64(stats.Cnt)
			out.Data.Stats[prop] = outv
		}
		result[sensorId] = out
	}
	return result
}
