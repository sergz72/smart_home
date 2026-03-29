package core

import (
    "testing"
    "time"
)

func TestQueue(t *testing.T) {
    config, err := loadConfiguration("../../test_resources/queueTestConfiguration.json")
    if err != nil {
        t.Fatal(err)
    }
    var q queue
    q.initQueue(config)
    tim := time.Now()
    q.enQueue(SensorDataMessage{tim.Add(-5 * time.Second), "sensor1", "{\"temp\":0.6}"})
    q.enQueue(SensorDataMessage{tim.Add(-4 * time.Second), "sensor2", "{\"temp\":0.8}"})
    q.enQueue(SensorDataMessage{tim.Add(-3 * time.Second), "sensor3", "{\"temp\":0.9}"})
    q.enQueue(SensorDataMessage{tim.Add(-2 * time.Second), "sensor4", "{\"temp\":0.1}"})
    q.enQueue(SensorDataMessage{tim.Add(-1 * time.Second), "sensor5", "{\"temp\":0.2}"})
    q.enQueue(SensorDataMessage{tim, "sensor6", "{\"temp\":0.5}"})
    l := 0
    for _, v := range q.messageMap {
        l += len(v)
    }
    if l != 5 {
        t.Errorf("Wrong message map length: %v", l)
    }

    result := q.deQueue(fromTime(tim, -3))
    l = len(result)
    if l != 3 {
        t.Errorf("Wrong result length: %v, expected: 3", l)
    }
    result = q.deQueue(fromTime(tim, -1))
    l = len(result)
    if l != 1 {
        t.Errorf("Wrong result length: %v, expected: 1", l)
    }

    result = q.deQueue(-1)
    l = len(result)
    if l != 3 {
        t.Errorf("Wrong result length: %v, expected: 3", l)
    }

    result = q.deQueue(-1)
    l = len(result)
    if l != 2 {
        t.Errorf("Wrong result length: %v, expected: 2", l)
    }
    q.enQueue(SensorDataMessage{tim.Add(121 * time.Second), "sensor5", "{\"temp\":0.2}"})
    result = q.deQueue(-1)
    l = len(result)
    if l != 1 {
        t.Errorf("Wrong result length: %v, expected: 1", l)
    }
}

