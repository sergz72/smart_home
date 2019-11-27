package main

import (
	"database/sql"
	"fmt"
	"github.com/kataras/iris"
	"io/ioutil"
	"log"
	"strings"
	"time"
)

type SensorDataMessage struct {
	sensor_name string
	message string
}

var queue chan SensorDataMessage
var sensors = make(map[string]int, 0)

func SensorDataHandler(ctx iris.Context) {
	if ctx.Request().Body == nil {
		panic("unmarshal: empty body")
	}

	rawData, err := ioutil.ReadAll(ctx.Request().Body)
	if err != nil {
		panic(err)
	}
	
	data := string(rawData)

  queue <- SensorDataMessage{ctx.Params().Get("sensor_name"), data}
}

func getSensors(db *sql.DB) error {
	fmt.Println("Reading sensors table...")
	rows, err := db.Query("SELECT id, name FROM sensors")
	if err != nil {
		return err
	}
	defer rows.Close()
	sensors = make(map[string]int, 0)
	for rows.Next() {
		var id int
		var name string
		err = rows.Scan(&id, &name)
		if err != nil {
			return err
		}
		sensors[name] = id
	}

	return nil
}

func getSensorId(db *sql.DB, sensor_name string) (int, error) {
	var err error = nil
  sensor_id, ok := sensors[sensor_name]
  if !ok {
  	err = getSensors(db)
  	if err == nil {
			sensor_id, ok = sensors[sensor_name]
			if !ok {
				err = fmt.Errorf("Sensor not found: %s", sensor_name)
			}
		}
	}

	return sensor_id, err
}

func SensorDataProcessor(db *sql.DB) {
	queue = make(chan SensorDataMessage)

	stmt, err := db.Prepare("insert into sensor_data(sensor_id, event_date, event_time, data, error_message) values($1,$2,$3,$4,$5)")
	checkErr(err)
        
	for {
		sensorDataMessage := <-queue
		now := time.Now()
		date := time.Date(now.Year(), now.Month(), now.Day(), 0, 0, 0, 0, now.Location())
		sensor_id, iderr := getSensorId(db, sensorDataMessage.sensor_name)
		if iderr != nil {
			log.Print(iderr)
			continue
		}
		if (strings.HasPrefix(sensorDataMessage.message, "{")) {
			data := sensorDataMessage.message
			fmt.Printf("sensor_name: %s event_date %v event_time %v data %s\n",
				         sensorDataMessage.sensor_name, date, now, data)
			_, err := stmt.Exec(sensor_id, date, now, data, nil)
			if err != nil {
				log.Print(err)
	    }
		} else {
			errorMessage := sensorDataMessage.message
			fmt.Printf("sensor_name: %s event_date %v event_time %v error_message %s\n",
  				       sensorDataMessage.sensor_name, date, now, errorMessage)
			_, err := stmt.Exec(sensor_id, date, now, nil, errorMessage)
			if err != nil {
		    log.Print(err)
      }
		}
	}
}