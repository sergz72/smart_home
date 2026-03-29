package core

import (
	"fmt"
	"sort"
	"sync"
	"time"
)

type SensorDataMessage struct {
	messageTime time.Time
	sensorName  string
	message     string
}

type queue struct {
	config *configuration
	messageMap map[int64][]SensorDataMessage
	lastValues map[string]string
	mutex sync.Mutex
	lastFrom int64
}

func (q *queue) initQueue(config *configuration) {
	q.config = config
	q.messageMap = make(map[int64][]SensorDataMessage)
	q.lastValues = make(map[string]string)
}

func (q *queue) enQueue(message SensorDataMessage) {
	t := fromTime(message.messageTime, 0)
	q.mutex.Lock()
	q.lastValues[message.sensorName] = message.message
	m, ok := q.messageMap[t]
	if !ok {
		q.messageMap[t] = []SensorDataMessage{message}
	} else {
		q.messageMap[t] = append(m, message)
	}
	q.cleanQueue(message.messageTime)
	q.mutex.Unlock()
}

func (q *queue) cleanQueue(now time.Time) {
	minTime := fromTime(now, -q.config.QueueParameters.expirationTime)
	var keyArray []int64
	for k := range q.messageMap {
		if k < minTime {
			fmt.Printf("Expired by time: %v\n", k)
			delete(q.messageMap, k)
		} else {
			keyArray = append(keyArray, k)
		}
	}

	sort.Slice(keyArray, func(i, j int) bool { return keyArray[i] < keyArray[j] })

	idx := len(keyArray) - q.config.QueueParameters.Size - 1
	for idx >= 0 {
		k := keyArray[idx]
		fmt.Printf("Expired by size: %v\n", k)
		delete(q.messageMap, k)
		idx--
	}
}

func fromTime(t time.Time, diff int) int64 {
	if diff != 0 {
		t = t.Add(time.Duration(diff) * time.Second)
	}
	return int64(t.Year()) * 10000000000 + int64(t.Month()) * 100000000 + int64(t.Day()) * 1000000 + int64(t.Hour()) * 10000 +
		   int64(t.Minute()) * 100 + int64(t.Second())
}

func (q *queue) buildKeyArray(from int64) []int64 {
	var keyArray []int64
	for k := range q.messageMap {
		if k > from {
			keyArray = append(keyArray, k)
		}
	}

	sort.Slice(keyArray, func(i, j int) bool { return keyArray[i] < keyArray[j] })

    return keyArray
}

func (q *queue) deQueue(from int64) []SensorDataMessage {
	q.mutex.Lock()
	updateLastFrom := from == -1
	if updateLastFrom {
		from = q.lastFrom
	}
	var result []SensorDataMessage
	cnt := 0
	keyArray := q.buildKeyArray(from)
	for _, k := range keyArray {
		v := q.messageMap[k]
		result = append(result, v...)
		if updateLastFrom && q.lastFrom < k {
			q.lastFrom = k
		}
		cnt += len(v)
		if cnt >= q.config.QueueParameters.MaxResponseSize {
			break
		}
	}
	q.mutex.Unlock()
	return result
}
