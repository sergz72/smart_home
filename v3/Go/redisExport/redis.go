package main

import (
	"context"
	"errors"
	"strconv"
	"strings"
	"time"

	"github.com/redis/go-redis/v9"
)

type sensorData struct {
	timestamp time.Time
	values    map[string]int
}

type sensorDataItem struct {
	timestamp time.Time
	value     int
}

type redisDatabase struct {
	rdb *redis.Client
	loc *time.Location
}

type redisExport struct {
	export map[int]map[string][]sensorDataItem
}

func (e *redisExport) transformToSensorData() map[int][]sensorData {
	result := make(map[int][]sensorData)
	for sensorId, data := range e.export {
		timeMap := make(map[time.Time]map[string]int)
		for valueType, values := range data {
			for _, value := range values {
				timeValue, ok := timeMap[value.timestamp]
				if !ok {
					timeValue = make(map[string]int)
					timeMap[value.timestamp] = timeValue
				}
				timeValue[valueType] = value.value
			}
		}
		var sd []sensorData
		for tm, values := range timeMap {
			sd = append(sd, sensorData{tm, values})
		}
		result[sensorId] = sd
	}
	return result
}

func (d *redisDatabase) exportData(timestamp int64) (redisExport, error) {
	ctx := context.Background()
	names, err := d.rdb.TSQueryIndex(ctx, []string{"type=sensor"}).Result()
	if err != nil {
		return redisExport{}, err
	}
	result := make(map[int]map[string][]sensorDataItem)
	for _, name := range names {
		parts := strings.Split(name, ":")
		sensorId, err := strconv.Atoi(parts[0])
		if err != nil {
			return redisExport{}, err
		}
		valueMap, ok := result[sensorId]
		if !ok {
			valueMap = make(map[string][]sensorDataItem)
			result[sensorId] = valueMap
		}
		valueType := parts[1]
		val, err := d.rdb.Do(ctx, "TS.RANGE", name, timestamp, "+").Result()
		if err != nil {
			return redisExport{}, err
		}
		rows, ok := val.([]interface{})
		if !ok {
			return redisExport{}, errors.New("invalid data")
		}
		var sd []sensorDataItem
		for _, row := range rows {
			r, ok := row.([]interface{})
			if !ok || len(r) != 2 {
				return redisExport{}, errors.New("invalid data")
			}
			ts, ok := r[0].(int64)
			if !ok {
				return redisExport{}, errors.New("invalid data")
			}
			v, ok := r[1].(float64)
			if !ok {
				return redisExport{}, errors.New("invalid data")
			}
			sd = append(sd, sensorDataItem{timestamp: time.UnixMilli(ts).In(d.loc), value: int(v * 100)})
		}
		valueMap[valueType] = sd
	}
	return redisExport{result}, nil
}

func buildRedisDatabase(parameters map[string]string, loc *time.Location) (redisDatabase, error) {
	redisHostName, ok := parameters["--redis_host_name"]
	if !ok {
		redisHostName = "127.0.0.1"
	}
	rdb := redis.NewClient(&redis.Options{
		Addr:     redisHostName,
		Password: "", // no password
		DB:       0,  // use default DB
		Protocol: 3,
	})
	return redisDatabase{rdb, loc}, nil
}
