package entities

import (
	"encoding/json"
	"fmt"
	"io/ioutil"
	"os"
	"strconv"
	"strings"
)

type PropertyMap struct {
	Values map[string]float64
	Stats  map[string]map[string]float64
}

func (p PropertyMap) ToString() string {
	if p.Values != nil {
		v, _ := json.Marshal(p.Values)
		return string(v)
	}
	s, _ := json.Marshal(p.Stats)
	return string(s)
}

func (p *PropertyMap) UnmarshalJSON(data []byte) error {
	return json.Unmarshal(data, &p.Values)
}

func (p PropertyMap) MarshalJSON() ([]byte, error) {
	return json.Marshal(p.Values)
}

func ToPropertyMap(m map[string]interface{}) (PropertyMap, error) {
	pm := PropertyMap{Values: make(map[string]float64)}

	for k, v := range m {
		value, ok := v.(float64)
		if !ok {
			return PropertyMap{}, fmt.Errorf(".(float64) conversion failure")
		}
		pm.Values[k] = value
	}

	return pm, nil
}

type SensorData struct {
	EventTime int
	Data PropertyMap
}

func ReadSensorDataFromJson(path string) (map[int][]SensorData, error) {
	d, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	fi, err := d.Readdir(-1)
	if err != nil {
		_ = d.Close()
		return nil, err
	}
	_ = d.Close()
	result := make(map[int][]SensorData)
	for _, file := range fi {
		if strings.HasSuffix(file.Name(), ".json") {
			sensorId, err := strconv.Atoi(file.Name()[0 : len(file.Name())-5])
			if err != nil {
				return nil, err
			}
			sensorData, err := readSensorData(path + string(os.PathSeparator) + file.Name())
			if err != nil {
				return nil, err
			}
			result[sensorId] = sensorData
		}
	}

	return result, nil
}

func readSensorData(fileName string) ([]SensorData, error) {
	var sensorData []SensorData
	dat, err := ioutil.ReadFile(fileName)
	if err != nil {
		return nil, err
	}
	if err := json.Unmarshal(dat, &sensorData); err != nil {
		return nil, err
	}
	return sensorData, nil
}

func WriteSensorDataToJson(dataFolder string, date int, sensorId int, data []SensorData) bool {
	bytes, err := json.Marshal(data)
	if err != nil {
		fmt.Printf("json.Marshal error: %v\n", err.Error())
		return false
	}
	datePath := dataFolder + string(os.PathSeparator) + "dates" + string(os.PathSeparator) + strconv.Itoa(date)
	if _, err := os.Stat(datePath); os.IsNotExist(err) {
		err = os.Mkdir(datePath, 0755)
		if err != nil {
			fmt.Printf("Folder creation error: %v %v\n", datePath, err.Error())
			return false
		}
	}
	fileName := datePath + string(os.PathSeparator) + strconv.Itoa(sensorId) + ".json"
	bakFileName := fileName + ".bak"
	bakFileCreated := false
	if _, err := os.Stat(fileName); err == nil {
		_ = os.Remove(bakFileName)
		err = os.Rename(fileName, bakFileName)
		if err != nil {
			fmt.Printf("Backup file creation error: %v %v\n", fileName, err.Error())
			return false
		}
		bakFileCreated = true
	}
	err = ioutil.WriteFile(fileName, bytes, 0644)
	if err != nil {
		fmt.Printf("File creation error: %v %v\n", fileName, err.Error())
		return false
	}
	if bakFileCreated {
		_ = os.Remove(bakFileName)
	}
	return true
}
