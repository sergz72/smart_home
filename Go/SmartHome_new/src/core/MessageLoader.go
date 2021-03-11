package core

import (
	"core/entities"
	"encoding/json"
	"errors"
	"fmt"
	"log"
	"strconv"
	"strings"
	"time"
)

type MessageDate struct {
	time.Time
}

func (md *MessageDate) UnmarshalJSON(input []byte) error {
	strInput := string(input)
	strInput = strings.Trim(strInput, `"`)
	newTime, err := time.Parse("2006-01-02 15:04", strInput)
	if err != nil {
		return err
	}

	md.Time = newTime

	return nil
}

type message struct {
	MessageTime MessageDate
	SensorName string
	Message interface{}
}

type decodedMessage struct {
	MessageTime time.Time
	SensorName string
	Message entities.PropertyMap
	Error string
}

func unmarshalResponse(response []byte) ([]decodedMessage, error) {
	var messages []message

	err := json.Unmarshal(response, &messages)
	if err != nil {
		return nil, err
	}

	decodedMessages := make([]decodedMessage, 0, len(messages))

	for _, m := range messages {
		message, errorMessage, err := decodeMessage(m.Message)
		if err != nil {
			return nil, err
		}
		decodedMessages = append(decodedMessages, decodedMessage{m.MessageTime.Time, m.SensorName, message, errorMessage})
	}

	return decodedMessages, nil
}

func decodeMessage(m interface{}) (entities.PropertyMap, string, error) {
	switch m.(type) {
	case string: return entities.PropertyMap{}, m.(string), nil
	case map[string]interface{}:
		m, err := entities.ToPropertyMap(m.(map[string]interface{}))
		return m, "", err
	}
    return entities.PropertyMap{}, "", errors.New("unknown datatype")
}

func getSensorId(db *DB, sensorName string) (int, error) {
	for _, sensor := range db.Sensors {
		if sensor.Name == sensorName {
			return sensor.Id, nil
		}
	}
	return 0, fmt.Errorf("sensor not found: %s", sensorName)
}

func processAddress(server *Server, timeout time.Duration, address string, key []byte, isSensors bool,
	                responseProcessor func(*Server, string, []byte) []error) {
	var response []byte
	var err error
	retryCount := server.config.FetchConfiguration.Retries
	url := "GET /sensor_data/sensors"
	if !isSensors {
		server.mutex.Lock()
		url = "GET /sensor_data/all@" + strconv.FormatInt(server.fetcherTimestamp[address], 10)
		server.mutex.Unlock()
	}
	for {
		log.Printf("processAddress: URL = %v\n", url)
		response, err = udpSend(key, address, url, timeout)
		if err == nil {
			break
		}
		log.Println(err)
		if retryCount <= 0 {
			return
		}
		retryCount--
	}

	errorList := responseProcessor(server, address, response)
	for _, err := range errorList {
		log.Println(err)
	}
}

func processResponse(server *Server, address string, response []byte) []error {
	messages, err := unmarshalResponse(response)
	if err != nil {
		return []error{err}
	}

	var result []error
	for _, message := range messages {
		sensorId, err := getSensorId(server.db, message.SensorName)
		if err != nil {
			result = append(result, err)
			continue
		}

		err = server.db.saveSensorData(sensorId, message)
		if err == nil {
			f := fromTime(message.MessageTime)
			server.mutex.Lock()
			if f > server.fetcherTimestamp[address] {
				server.fetcherTimestamp[address] = f
			}
			server.mutex.Unlock()
		} else {
			result = append(result, err)
		}
	}
	return result
}

func processSensorsResponse(server *Server, address string, response []byte) []error {
	var sensors []string
	err := json.Unmarshal(response, &sensors)
	if err != nil {
		return []error{err}
	}

	var result []error
	var from int64 = 0
	server.db.mutex.Lock()
	for _, sensorName := range sensors {
		sensorId, err := getSensorId(server.db, sensorName)
		if err != nil {
			result = append(result, err)
			continue
		}

		for k, v := range server.db.SensorDataMap {
			data, ok := v[sensorId]
			if ok {
				for _, d := range data {
					f := int64(k) * 1000000 + int64(d.EventTime)
					if f > from {
						from = f
					}
				}
			}
		}
	}
	server.db.mutex.Unlock()

	if len(result) == 0 {
		server.mutex.Lock()
		log.Printf("address: %v, from = %v\n", address, from)
		server.fetcherTimestamp[address] = from
		server.mutex.Unlock()
	}

	return result
}

func fetcherInit(server *Server, key []byte) {
	cont := true
	for cont {
		cont = false
		for _, address := range server.config.FetchConfiguration.Addresses {
			server.mutex.Lock()
			_, ok := server.fetcherTimestamp[address]
			server.mutex.Unlock()
			if !ok {
				processAddress(server, server.config.FetchConfiguration.timeoutDuration, address, key, true, processSensorsResponse)
				cont = true
			}
		}
	}
}

func fetchData(server *Server, key []byte) {
	for _, address := range server.config.FetchConfiguration.Addresses {
		server.mutex.Lock()
		_, ok := server.fetcherTimestamp[address]
		server.mutex.Unlock()
		if ok {
			processAddress(server, server.config.FetchConfiguration.timeoutDuration, address, key, false, processResponse)
		}
	}
}

