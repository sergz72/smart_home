package core

import (
	"core/entities"
	"fmt"
	"os"
	"strconv"
	"sync"
	"time"
)

type DB struct {
	Locations map[int]entities.Location
	Sensors map[int]entities.Sensor
	SensorDataMap map[int]map[int][]entities.SensorData
	SensorDataAggregated map[int]map[int]entities.SensorData
	DataTypeMap map[string][]int
	DataToBeSaved map[int][]int
	mutex sync.RWMutex
}

func (a *DB) Load(config *configuration, now time.Time) error {
	var err error
	a.Locations, err = entities.ReadLocationsFromJson(config.DataFolder + string(os.PathSeparator) + "locations.json")
	if err != nil {
		return err
	}
	a.Sensors, err = entities.ReadSensorsFromJson(config.DataFolder + string(os.PathSeparator) + "sensors.json")
	if err != nil {
		return err
	}
	a.buildDataTypeMap()
	a.DataToBeSaved = make(map[int][]int)
	a.mutex = sync.RWMutex{}
	return a.ReadSensorDataFromJson(config.DataFolder, now, config.RawDataDays)
}

func (a *DB) buildDataTypeMap() {
	a.DataTypeMap = make(map[string][]int)
	for _, sensor := range a.Sensors {
		arr, ok := a.DataTypeMap[sensor.DataType]
		if ok {
			a.DataTypeMap[sensor.DataType] = append(arr, sensor.Id)
		} else {
			a.DataTypeMap[sensor.DataType] = []int{sensor.Id}
		}
	}
}

func (a *DB) ReadSensorDataFromJson(path string, now time.Time, rawDataDays int) error {
	datesPath := path + string(os.PathSeparator) + "dates"
	d, err := os.Open(datesPath)
	if err != nil {
		return err
	}
	fi, err := d.Readdir(-1)
	if err != nil {
		_ = d.Close()
		return err
	}
	_ = d.Close()
	a.SensorDataMap = make(map[int]map[int][]entities.SensorData)
	a.SensorDataAggregated = make(map[int]map[int]entities.SensorData)
	for _, file := range fi {
		if file.Mode().IsDir() {
			datePath := datesPath + "/" + file.Name()
			date, err := strconv.Atoi(file.Name())
			if err != nil {
				return err
			}
			sensorData, err := entities.ReadSensorDataFromJson(datePath)
			if err != nil {
				return err
			}
			days := int(now.Sub(buildDate(date)).Hours() / 24)
			if days <= rawDataDays {
				a.SensorDataMap[date] = sensorData
			}
			a.SensorDataAggregated[date] = aggregateSensorDataArray(sensorData)
		}
	}
	return nil
}

func (a *DB) aggregateLastDayData() {
	lastDay := toDate(time.Now().AddDate(0, 0, -1))
	d, ok := a.SensorDataMap[lastDay]
	if ok {
		a.mutex.RLock()
		a.SensorDataAggregated[lastDay] = aggregateSensorDataArray(d)
		a.mutex.RUnlock()
	}
}

func (a *DB) backupData(config *configuration, now time.Time) {
	a.mutex.Lock()
    for date, sensors := range a.DataToBeSaved {
    	var m []int
    	for _, sensorId := range sensors {
    		if !entities.WriteSensorDataToJson(config.DataFolder, date, sensorId, a.SensorDataMap[date][sensorId]) {
    			m = append(m, sensorId)
			}
		}
		if len(m) == 0 {
			delete(a.DataToBeSaved, date)
		} else {
			a.DataToBeSaved[date] = m
		}
	}
	a.deleteOldRawSensorData(config.RawDataDays, now)
	a.mutex.Unlock()
}

func (a *DB) deleteOldRawSensorData(rawDataDays int, now time.Time) {
	for date, _ := range a.SensorDataMap {
		days := int(now.Sub(buildDate(date)).Hours() / 24)
		//log.Printf("now = %v, date = %v, dats = %v\n", now, date, days)
		if days > rawDataDays {
			fmt.Printf("Deleting raw sensor data for %v...\n", date)
			delete(a.SensorDataMap, date)
		}
	}
}

func (a *DB) saveSensorData(sensorId int, m decodedMessage) error {
	if len(m.Error) > 0 {
		fmt.Printf("Sensor %v returned error status: %v\n", m.SensorName, m.Error)
		return nil
	}
    d, t := toDateTime(m.MessageTime)

    a.mutex.RLock()
	if a.addToSensorData(m, d, t, sensorId) {
		a.addToDataToBeSaved(d, sensorId)
	}
	a.mutex.RUnlock()

	return nil
}

func (a *DB) addToDataToBeSaved(date int, sensorId int) {
	data, ok := a.DataToBeSaved[date]
	if ok {
		found := false
		for _, id := range data {
			if id == sensorId {
				found = true
				break
			}
		}
		if !found {
			a.DataToBeSaved[date] = append(data, sensorId)
		}
	} else {
		a.DataToBeSaved[date] = []int{sensorId}
	}
}

func (a *DB) addToSensorData(m decodedMessage, date int, tim int, sensorId int) bool {
	v := entities.SensorData{
		EventTime: tim,
		Data:      m.Message,
	}
	d, ok := a.SensorDataMap[date]
	if !ok {
		d = make (map[int][]entities.SensorData)
		d[sensorId] = []entities.SensorData{v}
		a.SensorDataMap[date] = d
	} else {
		sd, ok := d[sensorId]
		if !ok {
			d[sensorId] = []entities.SensorData{v}
		} else {
			for _, sdata := range sd {
				if sdata.EventTime == tim {
					return false
				}
			}
			d[sensorId] = append(sd, v)
		}
	}

	return true
}

func buildDate(date int) time.Time {
	return time.Date(date / 10000, time.Month((date / 100) % 100), date % 100, 0, 0, 0, 0, time.UTC)
}

func buildTime(tim int) time.Time {
	return time.Date(0, 1, 0, tim / 10000, (tim / 100) % 100, tim % 100, 0, time.UTC)
}

func toDate(t time.Time) int {
	return t.Year() * 10000 + int(t.Month()) * 100 + t.Day()
}

func toDateTime(t time.Time) (int, int) {
	return toDate(t), t.Hour() * 10000 + t.Minute() * 100 + t.Second()
}

func fromTime(t time.Time) int64 {
	return int64(t.Year()) * 10000000000 + int64(t.Month()) * 100000000 + int64(t.Day()) * 1000000 + int64(t.Hour()) * 10000 +
		int64(t.Minute()) * 100 + int64(t.Second())
}

func nextDate(date1 int) int {
	t := time.Date(date1 / 10000, time.Month((date1 / 100) % 100), date1 % 100, 0, 0, 0, 0, time.UTC).
		AddDate(0, 0, 1)
	return toDate(t)
}

func addTimeToDate(date time.Time, tim time.Time) time.Time {
	return date.Add(time.Duration(tim.Hour()) * time.Hour + time.Duration(tim.Minute()) * time.Minute +
		            time.Duration(tim.Second()) * time.Second)
}