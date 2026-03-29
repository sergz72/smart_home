package core

import (
	"bytes"
	"database/sql"
	"encoding/json"
	"errors"
	"fmt"
	"log"
	"net/url"
	"strconv"
	"strings"
	"time"
)

type PropertyMap map[string]interface{}

func (p PropertyMap) toString() string {
	b, _ := json.Marshal(p)
	return string(b)
}

func (p *PropertyMap) Scan(src interface{}) error {
	source, ok := src.([]byte)
	if !ok {
		return errors.New("Type assertion .([]byte) failed.")
	}

	var i interface{}
	err := json.Unmarshal(source, &i)
	if err != nil {
		return err
	}

	*p, ok = i.(map[string]interface{})
	if !ok {
		return errors.New("Type assertion .(map[string]interface{}) failed.")
	}

	return nil
}

func SensorDataHandler(server *Server, w *bytes.Buffer, req string) {
	values, err := url.ParseQuery(req)
	if err != nil {
		w.Write([]byte("400 Bad request: query parsing error"))
		return
	}
	dataType := values.Get("data_type")
	where := ""
	tableName := "v_sensor_data"
	starts := values.Get("start")
	if len(starts) > 0 {
		ends := values.Get("end")
   	if len(ends) == 0 {
			w.Write([]byte("400 Bad request: end parameter is missing"))
    	return
   	}
   	where = fmt.Sprintf("event_date + event_time between to_timestamp('%s', 'YYYYMMDDHH24MI') and to_timestamp('%s', 'YYYYMMDDHH24MI')", starts, ends)
	} else {
		periods := values.Get("period")
		var period int
		if len(periods) > 0 {
		    period, err = strconv.Atoi(periods)
		    if err != nil {
					w.Write([]byte("400 Bad request: " + err.Error()))
		    	return
		    }
  			where = fmt.Sprintf("AGE(now(), event_date + event_time) <= INTERVAL '%v hour'", period)
		} else {
			tableName = "v_latest_sensor_data"
		}
	}

	if dataType != "all" {
		if len(where) > 0 {
			where = fmt.Sprintf(" WHERE data_type = '%s' AND %s", dataType, where)
		} else {
			where = fmt.Sprintf(" WHERE data_type = '%s'", dataType)
		}
	} else if len(where) > 0 {
		where = fmt.Sprintf(" WHERE %s", where)
	}

	var rows *sql.Rows
	server.mutex.Lock()
	rows, err = server.db.Query(fmt.Sprintf("SELECT location_name, location_type, event_date, event_time, data_type, data FROM %s%s", tableName, where))
	server.mutex.Unlock()
	if err == nil {
		var results []string
		for rows.Next() {
			var locationName string
			var locationType string
			var dataType string
			var date time.Time
			var time time.Time
			var data PropertyMap
			err = rows.Scan(&locationName, &locationType, &date, &time, &dataType, &data)
			if err != nil {
				break
			}
			time = time.AddDate(date.Year(), int(date.Month())-1, date.Day())
			result := fmt.Sprintf("{\"locationName\": \"%v\", \"locationType\": \"%v\", \"date\": \"%v\", \"dataType\": \"%v\", \"data\": %v}",
				locationName, locationType, time.Format("2006-01-02 15:04"), dataType, data.toString())
			//fmt.Println(result)
			results = append(results, result)
		}
		if err == nil {
			w.Write([]byte("["))
			w.Write([]byte(strings.Join(results, ",")))
			w.Write([]byte("]"))
			return
		}
	}
	log.Print(err)
	w.Write([]byte("500 Server error: " + err.Error()))
}
