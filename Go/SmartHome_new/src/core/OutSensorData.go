package core

import (
	"smartHome/src/core/entities"
	"sort"
)

type SensorTimeData struct {
	Date int                      `json:"date"`
	Data []entities.OutSensorData `json:"data"`
}

func buildSensorTimeData(date int, data []entities.SensorData) SensorTimeData {
	var outData []entities.OutSensorData
	for _, d := range data {
		outData = append(outData, entities.OutSensorData{
			EventTime: d.EventTime,
			Data: entities.OutPropertyMap{
				Values: d.Data.Values,
				Stats:  d.Data.Stats,
			},
		})
	}
	return SensorTimeData{
		Date: date,
		Data: outData,
	}
}

type OutSensorData struct {
	LocationName string           `json:"locationName"`
	LocationType string           `json:"locationType"`
	DataType     string           `json:"dataType"`
	Total        int              `json:"total"`
	TimeData     []SensorTimeData `json:"timeData"`
	timeData     map[int][]entities.SensorData
}

func (sd *OutSensorData) length() int {
	l := 0
	for _, v := range sd.timeData {
		l += len(v)
	}
	return l
}

func (sd *OutSensorData) aggregate(compressionLevel int, config *configuration) OutSensorData {
	return OutSensorData{
		LocationName: sd.LocationName,
		LocationType: sd.LocationType,
		DataType:     sd.DataType,
		Total:        sd.calculateTotal(config.TotalCalculation),
		TimeData:     sd.buildTimeData(compressionLevel),
		timeData:     nil,
	}
}

func (sd *OutSensorData) calculateTotal(totalCalculation map[string]int) int {
	total := 0
	exists := false
	divider := 1

	// loop by date
	for _, v := range sd.timeData {
		// loop by sensorData
		for _, d := range v {
			if d.Data.Values != nil {
				for dataType, div := range totalCalculation {
					value, ok := d.Data.Values[dataType]
					if ok {
						exists = true
						total += value
						divider = div
					}
				}
			} else {
				for dataType /*, div*/ := range totalCalculation {
					value, ok := d.Data.Stats[dataType]
					if ok {
						exists = true
						total += value["Sum"]
						//fmt.Printf("%v %v\n", value["Sum"], total)
						//divider = div
					}
				}
			}
		}
	}

	if !exists {
		return -1
	}

	return total / divider
}

func (sd *OutSensorData) buildTimeData(compressionLevel int) []SensorTimeData {
	if sd.timeData == nil || len(sd.timeData) == 0 {
		return []SensorTimeData{}
	}

	var result []SensorTimeData

	if compressionLevel == 1 {
		for date, data := range sd.timeData {
			result = append(result, buildSensorTimeData(date, data))
		}
	} else {
		var keys []int
		for date := range sd.timeData {
			keys = append(keys, date)
		}
		sort.Ints(keys)
		currentDate := keys[0]
		currentTime := sd.timeData[currentDate][0].EventTime
		currentSensorTimeData := SensorTimeData{
			Date: currentDate,
		}
		var values []entities.PropertyMap
		for _, date := range keys {
			data := sd.timeData[date]
			for _, d := range data {
				if len(values) == compressionLevel {
					currentSensorTimeData.Data = append(currentSensorTimeData.Data, entities.OutSensorData{
						EventTime: currentTime,
						Data:      aggregateValues(values),
					})
					currentTime = d.EventTime
					values = []entities.PropertyMap{}
					if currentDate != date {
						result = append(result, currentSensorTimeData)
						currentDate = date
						currentSensorTimeData = SensorTimeData{
							Date: currentDate,
						}
					}
				}
				values = append(values, d.Data)
			}
		}
		if len(values) > 0 {
			currentSensorTimeData.Data = append(currentSensorTimeData.Data, entities.OutSensorData{
				EventTime: currentTime,
				Data:      aggregateValues(values),
			})
			result = append(result, currentSensorTimeData)
		}
	}

	return result
}

func aggregatePropertyMapValues(values []entities.PropertyMap) map[string]int {
	if values[0].Values == nil {
		return nil
	}

	result := make(map[string]int)

	for _, v := range values {
		for dataType, value := range v.Values {
			result[dataType] = result[dataType] + value
		}
	}

	l := len(values)
	for dataType, value := range result {
		result[dataType] = value / l
	}

	return result
}

func aggregatePropertyMapStats(values []entities.PropertyMap) map[string]map[string]int {
	if values[0].Stats == nil {
		return nil
	}

	result := make(map[string]map[string]int)

	for _, v := range values {
		for dataType, stats := range v.Stats {
			resultMap, ok := result[dataType]
			if ok {
				for param, value := range stats {
					v2, ok := resultMap[param]
					if ok {
						switch param {
						case "Min":
							if value < v2 {
								resultMap[param] = value
							}
						case "Max":
							if value > v2 {
								resultMap[param] = value
							}
						case "Avg":
							resultMap[param] = v2 + value
						case "Sum":
							resultMap[param] = v2 + value
						case "Cnt":
							resultMap[param] = v2 + value
						}
					} else {
						resultMap[param] = value
					}
				}
			} else {
				copyOfStats := make(map[string]int)
				for key, value := range stats {
					copyOfStats[key] = value
				}
				result[dataType] = copyOfStats
			}
		}
	}

	l := len(values)
	for _, stats := range result {
		stats["Avg"] = stats["Avg"] / l
	}

	return result
}

func aggregateValues(values []entities.PropertyMap) entities.OutPropertyMap {
	return entities.OutPropertyMap{
		Values: aggregatePropertyMapValues(values),
		Stats:  aggregatePropertyMapStats(values),
	}
}

func MakeOutSensorData(db *DB, sensorId int) *OutSensorData {
	s := db.Sensors[sensorId]
	l := db.Locations[s.LocationId]
	return &OutSensorData{
		LocationName: l.Name,
		LocationType: l.LocationType,
		DataType:     s.DataType,
		timeData:     make(map[int][]entities.SensorData),
	}
}

func aggregateResults(resultMap map[int]*OutSensorData, maxPoints int, config *configuration) []OutSensorData {
	result := []OutSensorData{}

	for _, sensorData := range resultMap {
		l := sensorData.length()
		compressionLevel := l/maxPoints + 1
		result = append(result, sensorData.aggregate(compressionLevel, config))
	}
	return result
}
