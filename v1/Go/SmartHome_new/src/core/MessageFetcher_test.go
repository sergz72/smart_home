package core

import (
    "bufio"
    "os"
    "smartHome/src/core/entities"
    "strings"
    "testing"
    "time"
)

func TestMessageDecoding(t *testing.T) {
    messages, err := unmarshalResponse([]byte("[{\"messageTime\": \"2020-01-02 15:04:59\", \"sensorName\": \"test_ee\", \"message\": {\"temp\": 0.1}}]"))
    if err != nil {
        t.Fatal(err)
    }
    if messages == nil {
        t.Fatal("nil response")
    }
    l := len(messages)
    if l != 1 {
        t.Fatalf("wrong messages length: %v", l)
    }
    if messages[0].MessageTime != time.Date(2020, 1, 2, 15, 4, 59, 0, time.UTC) {
        t.Errorf("wrong message time: %v", messages[0].MessageTime)
    }
    if messages[0].SensorName != "test_ee" {
        t.Errorf("wrong sensor name: %v", messages[0].SensorName)
    }
    m := messages[0].Message
    if len(m.Values) != 1 {
        t.Errorf("wrong message: %v", messages[0].Message)
    }
    v, ok := m.Values["temp"]
    if !ok {
        t.Error("temp key must be present")
    }
    if v != 10 {
        t.Errorf("wrong temp value: %v", v)
    }
    if messages[0].Error != "" {
        t.Errorf("wrong error: %v", messages[0].Error)
    }
}

func TestMessageDecoding2(t *testing.T) {
    messages, err := unmarshalResponse([]byte("[{\"messageTime\": \"2021-09-21 19:35:01\", \"sensorName\": \"zal_int\", \"message\": {\"humi\":63.70,\"temp\":23.70}},{\"messageTime\": \"2021-09-21 19:35:01\", \"sensorName\": \"zal_ext\", \"message\": {\"humi\":82.90,\"temp\":11.60}},{\"messageTime\": \"2021-09-21 19:38:57\", \"sensorName\": \"pump\", \"message\": {\"icc\":0.0,\"pres\":2.9}},{\"messageTime\": \"2021-09-21 19:39:08\", \"sensorName\": \"pump\", \"message\": {\"icc\":0.0,\"pres\":2.8}}]"))
    if err != nil {
        t.Fatal(err)
    }
    if messages == nil {
        t.Fatal("nil response")
    }
    l := len(messages)
    if l != 4 {
        t.Fatalf("wrong messages length: %v", l)
    }
    var db DB
    db.SensorDataMap = make(map[int]map[int][]entities.SensorData)
    db.DataToBeSaved = make(map[int][]int)
    for _, message := range messages {
        err = db.saveSensorData(1, message)
        if err != nil {
            t.Fatal(err)
        }
    }
    l = len(db.SensorDataMap)
    if l != 1 {
        t.Fatalf("wrong SensorDataMap length: %v", l)
    }
}

func TestErrorDecoding(t *testing.T) {
    messages, err := unmarshalResponse([]byte("[{\"messageTime\": \"2020-01-02 15:04:45\", \"sensorName\": \"test_ee\", \"message\": \"error\"}]"))
    if err != nil {
        t.Fatal(err)
    }
    if messages == nil {
        t.Fatal("nil response")
    }
    l := len(messages)
    if l != 1 {
        t.Fatalf("wrong messages length: %v", l)
    }
    if messages[0].MessageTime != time.Date(2020, 1, 2, 15, 4, 45, 0, time.UTC) {
        t.Errorf("wrong message time: %v", messages[0].MessageTime)
    }
    if messages[0].SensorName != "test_ee" {
        t.Errorf("wrong sensor name: %v", messages[0].SensorName)
    }
    m := messages[0].Message
    if len(m.Values) != 0 {
        t.Errorf("wrong message: %v", messages[0].Message)
    }
    if messages[0].Error != "error" {
        t.Errorf("wrong error: %v", messages[0].Error)
    }
}

func TestLogReplay(t *testing.T) {
    config := configuration{
        DataFolder:  "../../test_resources/db",
        RawDataDays: 10,
    }
    var db DB
    err := db.Load(&config, time.Date(2021, 2, 6, 0, 0, 0, 0, time.UTC))
    if err != nil {
        t.Fatal(err)
    }

    file, err := os.Open("../../test_resources/server.out")
    if err != nil {
        t.Fatal(err)
    }
    defer file.Close()

    scanner := bufio.NewScanner(file)
    //counter := 0
    for scanner.Scan() {
        text := scanner.Text()
        if strings.HasPrefix(text, "[") {
            //fmt.Println(text)
            messages, err := unmarshalResponse([]byte(text))
            if err != nil {
                t.Fatal(err)
            }

            for _, message := range messages {
                sensorId, err := getSensorId(&db, message.SensorName)
                if err != nil {
                    t.Fatal(err)
                }

                err = db.saveSensorData(sensorId, message)
                if err != nil {
                    t.Fatal(err)
                }
            }
            /*counter++
              if counter % 5 == 0 {
                time.Sleep(5 * time.Second)
              }*/
        }
    }
    config.DataFolder = os.TempDir() + "/db"
    _ = os.MkdirAll(config.DataFolder+"/dates", 0777)
    db.backupData(&config, time.Now())
}
