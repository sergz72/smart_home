package main

import (
	"database/sql"
	"errors"
	"fmt"
	"time"

	_ "github.com/lib/pq"
)

type postgresDatabase struct {
	db  *sql.DB
	loc *time.Location
}

func buildPostgresDatabase(parameters map[string]string, loc *time.Location) (*postgresDatabase, error) {
	postgresHostName, ok := parameters["--postgres_host_name"]
	if !ok {
		postgresHostName = "127.0.0.1"
	}
	postgresDbName, ok := parameters["--postgres_db_name"]
	if !ok {
		return &postgresDatabase{}, errors.New("--postgres_db_name is required")
	}
	connectionString := fmt.Sprintf("host=%s user=postgres dbname=%s", postgresHostName, postgresDbName)
	db, err := sql.Open("postgres", connectionString)
	if err != nil {
		return nil, err
	}
	return &postgresDatabase{db, loc}, nil
}

func (d *postgresDatabase) getMaxTimestamp() (int64, error) {
	var dt int64
	err := d.db.QueryRow("select max(event_date::bigint * 1000000 + event_time) from sensor_events").Scan(&dt)
	if err != nil {
		return 0, err
	}
	date := int(dt / 1000000)
	tm := int(dt % 1000000)
	newTime := time.Date(date/10000, time.Month((date/100)%100), date%100, tm/10000,
		(tm/100)%100, tm%100, 0, d.loc)
	return newTime.UnixMilli() + 1000, nil
}

func (d *postgresDatabase) importData(rows redisExport) error {
	for sensorId, data := range rows.export {
		for valueType, items := range data {
			fmt.Printf("Sensor id %d value type %s item count %d\n", sensorId, valueType, len(items))
			for _, item := range items {
				date := item.timestamp.Year()*10000 + int(item.timestamp.Month())*100 + item.timestamp.Day()
				tm := item.timestamp.Hour()*10000 + item.timestamp.Minute()*100 + item.timestamp.Second()
				_, err := d.db.Exec("insert into sensor_events(sensor_id, event_date, event_time, value_type, value) values($1,$2,$3,$4,$5)",
					sensorId, date, tm, valueType, item.value)
				if err != nil {
					return err
				}
			}
		}
	}
	return nil
}
