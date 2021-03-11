package core

import (
  "testing"
  "time"
)

func TestMessageDecoding(t *testing.T) {
  messages, err := unmarshalResponse([]byte("[{\"messageTime\": \"2020-01-02 15:04\", \"sensorName\": \"test_ee\", \"message\": {\"temp\": 0.1}}]"))
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
  if messages[0].MessageTime != time.Date(2020, 1, 2, 15, 4, 0, 0, time.UTC) {
    t.Errorf("wrong message time: %v", messages[0].MessageTime)
  }
  if messages[0].SensorName != "test_ee" {
    t.Errorf("wrong sensor name: %v", messages[0].SensorName)
  }
  m := messages[0].Message
  if  len(m.Values) != 1 {
    t.Errorf("wrong message: %v", messages[0].Message)
  }
  v, ok := m.Values["temp"]
  if !ok {
    t.Error("temp key must be present")
  }
  if v != 0.1 {
    t.Errorf("wrong temp value: %v", v)
  }
  if messages[0].Error != "" {
    t.Errorf("wrong error: %v", messages[0].Error)
  }
}

func TestErrorDecoding(t *testing.T) {
  messages, err := unmarshalResponse([]byte("[{\"messageTime\": \"2020-01-02 15:04\", \"sensorName\": \"test_ee\", \"message\": \"error\"}]"))
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
  if messages[0].MessageTime != time.Date(2020, 1, 2, 15, 4, 0, 0, time.UTC) {
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
